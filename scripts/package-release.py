#!/usr/bin/env python3
# RecompLauncher - native game library and build launcher.
# The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
"""Stage an allowlisted, relocatable end-user release and archive it with a SHA-256 manifest.

Layout (the launcher finds engine/ and toolchain/ beside its own executable):
  RecompLauncher(.exe)  the Qt launcher with its Qt/MSVC DLLs and plugin folders (Linux: lib/, plugins/, qt.conf)
  engine/               only what a game build reads: the recompiler modules, recomp_cli.py, the prebuilt native
                        runtime, the runtime header/helper the generated C includes, the shader helper (+ hactool)
  toolchain/            python, gcc, dotnet, shader-tools
  legal/                engine LICENSE/NOTICE, third-party licenses, corresponding sources, provenance, SHA256SUMS
The complete source (launcher + engine at the pinned commit) is the separate source archive of each release."""
import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAME = 'RecompLauncher'   # archive, top-level folder and launcher executable name
ENGINE = ROOT / 'third_party/Switch2RECOMP'   # engine submodule in the checkout
RELEASE_ENGINE, TOOLCHAIN, LEGAL = Path('engine'), Path('toolchain'), Path('legal')
TOOLS = ('python', 'gcc', 'dotnet', 'shader-tools')
# Engine files a game build reads, as tracked in the submodule; everything else there builds the runtime or is
# developer tooling. switchrecomp.py imports nxloader + codegen; codegen imports a64; nxloader imports nup + nsp
# (packages, hactool) + zbic (Switch 2 ZBIC-compressed NSOs). Generated C is compiled with -I runtime and includes
# only cpu.h and cpu_helpers.c, which also key the object cache. copy_nxshader() stages tools/nxshader into every game, plus runtime/nvn/gfx/dmsh_abi.h.
ENGINE_FILES = ('switchrecomp.py', 'nxloader.py', 'codegen.py', 'a64.py', 'nup.py', 'nsp.py', 'zbic.py',
                'runtime/cpu.h', 'runtime/cpu_helpers.c', 'runtime/nvn/gfx/dmsh_abi.h',
                'tools/nxshader/nxs_live.py', 'tools/nxshader/nxs_front.py', 'tools/nxshader/nxs_cache.py',
                'tools/nxshader/nxshader.py', 'tools/nxshader/nxs_spv.py', 'tools/nxshader/nxs_refl.py',
                'tools/nxshader/dmsh_abi.py', 'tools/nxshader/dmsh_ampere.py', 'tools/nxshader/dmsh_meshstage.py',
                'tools/nxshader/s2r_paths.py',
                # the translator runs as 'dotnet dmsh_xlate.dll'; its extensionless macOS apphost is not shipped
                'tools/nxshader/xlate_bin/*.dll', 'tools/nxshader/xlate_bin/*.json', 'tools/nxshader/xlate_bin/LICENSE-*.txt')
# engine notices that move to legal/ (the runtime links libvpx statically, so its BSD notice ships with it)
ENGINE_LEGAL = {'LICENSE.txt': 'LICENSE.txt', 'NOTICE.txt': 'NOTICE.txt', 'docs/THIRD_PARTY.md': 'THIRD_PARTY.md',
                'runtime/vp9/LICENSE-libvpx.txt': 'licenses/libvpx/LICENSE-libvpx.txt',
                'runtime/vp9/PATENTS-libvpx.txt': 'licenses/libvpx/PATENTS-libvpx.txt'}
JUNK = {'__pycache__', '_downloads', '.git', '.venv', 'venv', '.pytest_cache', 'shader_cache',
        'captures', 'logs', 'games', 'saves', 'savedata', 'firmware'}
PRIVATE_NAMES = {'library.json', 'python.path', 'performance.txt', 'threads.txt', 'prod.keys', 'title.keys'}
PRIVATE_SUFFIXES = {'.log', '.pyc', '.nup', '.nsp', '.xci', '.keys', '.nca', '.nso', '.nro'}


def excluded(path):
    return (any(part.lower() in JUNK for part in path.parts) or path.name.lower() in PRIVATE_NAMES
            or path.suffix.lower() in PRIVATE_SUFFIXES)


def runtime_name(platform):
    return 'runtime_win64_win32.exe' if platform == 'windows' else 'runtime_linux_x11'


def launcher_name(platform):
    return NAME + ('.exe' if platform == 'windows' else '')


def vp9_path(platform):
    return Path('runtime/vp9') / ('win' if platform == 'windows' else 'lin') / 'libvpx.a'


def engine_allowed(name, platform):
    """Whether engine/<name> (posix) belongs in a release: the allowlist, the CLI, the runtime and hactool."""
    extra = ['recomp_cli.py', 'bin/' + runtime_name(platform)]
    if platform == 'windows': extra += ['tools/hactool/hactool.exe', 'tools/hactool/LICENSE']
    return name in extra or any(fnmatch.fnmatchcase(name, pattern) for pattern in ENGINE_FILES)


def required_files(platform):
    engine = [n for n in ENGINE_FILES if '*' not in n] + [
        'recomp_cli.py', 'bin/' + runtime_name(platform), 'tools/nxshader/xlate_bin/dmsh_xlate.dll',
        'tools/nxshader/xlate_bin/dmsh_xlate.deps.json', 'tools/nxshader/xlate_bin/dmsh_xlate.runtimeconfig.json',
        'tools/nxshader/xlate_bin/LICENSE-Ryujinx.txt', 'tools/nxshader/xlate_bin/LICENSE-nxnative.txt']
    legal = [*ENGINE_LEGAL.values(), 'dependency-manifest.json',
             'sources/libvpx_1.14.0-1ubuntu2.3.dsc', 'sources/libvpx_1.14.0.orig.tar.xz',
             'sources/libvpx_1.14.0-1ubuntu2.3.debian.tar.xz']
    legal += ['sources/' + module + '-everywhere-src-6.8.3.tar.xz' for module in ('qtbase', 'qtimageformats', 'qtsvg')]
    toolchain = ['dotnet/LICENSE.txt', 'dotnet/ThirdPartyNotices.txt']
    if platform == 'windows':
        engine += ['tools/hactool/hactool.exe', 'tools/hactool/LICENSE']
        toolchain += ['python/python.exe', 'python/LICENSE.txt', 'gcc/Library/bin/x86_64-w64-mingw32-gcc.exe',
                      'dotnet/dotnet.exe'] + ['shader-tools/Library/bin/' + t + '.exe' for t in ('spirv-as', 'spirv-val', 'spirv-opt')]
        top = [launcher_name(platform), 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll',
               'platforms/qoffscreen.dll', 'imageformats/qwebp.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll']
    else:
        toolchain += ['python/bin/python3', 'dotnet/dotnet', 'gcc/bin/x86_64-conda-linux-gnu-gcc'] + [
                      'shader-tools/bin/' + t for t in ('spirv-as', 'spirv-val', 'spirv-opt')]
        legal += ['linux-libraries.json', 'licenses/python/PYTHON.json', 'licenses/python/licenses/LICENSE.cpython.txt']
        top = [launcher_name(platform), 'qt.conf', 'plugins/platforms/libqxcb.so', 'plugins/platforms/libqoffscreen.so',
               'plugins/imageformats/libqwebp.so', 'lib/libSDL2-2.0.so.0', 'lib/libQt6Core.so.6']
    return (top + [(RELEASE_ENGINE / n).as_posix() for n in engine] + [(TOOLCHAIN / n).as_posix() for n in toolchain]
            + [(LEGAL / n).as_posix() for n in legal])


def check_layout(stage, platform):
    """Nothing beyond the release layout: the launcher and its Qt runtime at the top, engine/ limited to the
    allowlist (no runtime sources, tests, docs or developer tools), and the three fixed folders."""
    plugin = '.dll' if platform == 'windows' else '.so'
    for path in stage.iterdir():
        name = path.name
        if path.is_dir():
            if name in (RELEASE_ENGINE.name, TOOLCHAIN.name, LEGAL.name): continue
            if platform == 'linux' and name == 'lib': continue   # bundled Qt/system shared libraries
            # Qt plugin folders: windeployqt's platforms, imageformats, styles, ...; Linux plugins/<kind>
            if all(p.is_dir() or plugin in p.name for p in path.rglob('*')): continue
            raise RuntimeError('Unexpected top-level folder in the release: ' + name)
        if platform == 'windows' and (name == launcher_name(platform) or name.lower().endswith('.dll')): continue
        if platform == 'linux' and name in (launcher_name(platform), 'qt.conf'): continue
        raise RuntimeError('Unexpected top-level file in the release: ' + name)
    for path in (stage / RELEASE_ENGINE).rglob('*'):
        name = path.relative_to(stage / RELEASE_ENGINE).as_posix()
        if path.is_file() and not engine_allowed(name, platform):
            raise RuntimeError('Engine file outside the release allowlist: ' + name)


def validate_contents(stage, platform):
    for path in stage.rglob('*'):
        relative = path.relative_to(stage)
        if excluded(relative): raise RuntimeError('Personal/generated release input: ' + str(relative))
        if path.is_symlink() and (os.path.isabs(os.readlink(path)) or not path.exists()
                                  or not path.resolve().is_relative_to(stage.resolve())):
            raise RuntimeError('Non-relocatable symlink: ' + str(relative))
    for name in required_files(platform):
        if not (stage / name).is_file() or not (stage / name).stat().st_size: raise FileNotFoundError(name)
    for name in ('legal/licenses/qt/qtbase', 'legal/licenses/qt/qtimageformats', 'legal/licenses/qt/qtsvg',
                 'toolchain/gcc/licenses', 'toolchain/shader-tools/licenses'):
        if not any(p.is_file() for p in (stage / name).rglob('*')): raise FileNotFoundError(name)
    check_layout(stage, platform)
    manifest = json.loads((stage / LEGAL / 'dependency-manifest.json').read_text())
    if manifest['platform'] != platform:
        raise RuntimeError('Dependency manifest platform mismatch')
    for name, record in manifest['downloads']['libvpx']['sources'].items():
        if digest(stage / LEGAL / 'sources' / name) != record['sha256']: raise RuntimeError('VP9 source checksum: ' + name)
    data = (stage / RELEASE_ENGINE / 'bin' / runtime_name(platform)).read_bytes()
    # these strings live in the linked decoder, telemetry and controller implementations, not build scripts
    markers = [b'WebM Project VP9 Decoder', b'frame telemetry:']
    if platform == 'windows': markers += [b'hid.dll', b'setupapi.dll']
    for marker in markers:
        if marker not in data: raise RuntimeError('Native runtime feature missing: ' + marker.decode())


def verify_vp9_provenance(engine, manifest_path, platform):
    """The runtime statically links the engine's committed libvpx archive; it must be the one whose source the
    release ships (legal/sources). The archive is a build input, so it is checked in the checkout, not shipped."""
    manifest = json.loads(Path(manifest_path).read_text())
    if digest(Path(engine) / vp9_path(platform)) != manifest['downloads']['libvpx']['sha256']:
        raise RuntimeError('VP9 archive does not match dependency provenance')


def copy(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def tree(src, dst):
    if not src.is_dir(): raise FileNotFoundError(src)
    shutil.copytree(src, dst, dirs_exist_ok=True, symlinks=True,
                    ignore=shutil.ignore_patterns(*JUNK, '*.pyc', '*.pdb', '*.log'))


def launcher_files(launcher, platform):
    """The deployed launcher folder, allowlisted: the executable plus its DLLs and Qt plugin folders (Windows), or
    the executable alone (Linux: Qt libraries and plugins are collected from the Qt prefix). windeployqt also drops
    vc_redist.x64.exe there; the app-local MSVC DLLs make it redundant, so it is not shipped."""
    exe = launcher / launcher_name(platform)
    if not exe.is_file(): raise FileNotFoundError(exe)
    yield exe.relative_to(launcher)
    if platform != 'windows': return
    for path in sorted(launcher.rglob('*.dll')):
        relative = path.relative_to(launcher)
        if len(relative.parts) <= 2 and not excluded(relative): yield relative


def engine_files():
    """Allowlisted engine files as tracked at the pinned submodule commit, relative to the engine."""
    if not (ENGINE / 'switchrecomp.py').is_file():
        raise FileNotFoundError('Engine submodule is not checked out: git submodule update --init')
    tracked = subprocess.check_output(['git', 'ls-files', '-z', '--cached'], cwd=ENGINE).decode().split('\0')
    names = sorted(n for n in set(tracked)
                   if n and any(fnmatch.fnmatchcase(n, p) for p in ENGINE_FILES) and not excluded(Path(n)))
    for pattern in ENGINE_FILES:
        if not any(fnmatch.fnmatchcase(n, pattern) for n in names): raise FileNotFoundError('Engine file not tracked: ' + pattern)
    for name in names:
        if (ENGINE / name).is_file(): yield Path(name)


def git(repo, *args, optional=False):
    return subprocess.check_output(['git', *args], cwd=repo, text=True,
                                   stderr=subprocess.DEVNULL if optional else None).strip()


def provenance(platform):
    """Both commits: the launcher checkout and the engine submodule pinned in it."""
    dirty = any(git(repo, 'status', '--porcelain', '--untracked-files=normal') for repo in (ROOT, ENGINE))
    engine_commit = git(ENGINE, 'rev-parse', 'HEAD')
    info = {'commit': git(ROOT, 'rev-parse', 'HEAD'), 'engine_commit': engine_commit,
            'platform': platform, 'local_changes': dirty}
    try: engine_url = git(ROOT, 'config', '-f', '.gitmodules', 'submodule.%s.url' % ENGINE.relative_to(ROOT).as_posix(), optional=True)
    except subprocess.CalledProcessError: engine_url = ''
    if engine_url: info['engine_source'] = engine_url.removesuffix('.git') + '/tree/' + engine_commit
    try: info['source'] = git(ROOT, 'remote', 'get-url', 'origin', optional=True).removesuffix('.git') + '/tree/' + info['commit']
    except subprocess.CalledProcessError: pass
    return info


def linux_libraries(stage, qt):
    """Copy the Qt plugins to plugins/ and every non-glibc shared library the launcher, runtime and bundled tools
    need into lib/ (the launcher's DT_RPATH is $ORIGIN/lib; the launcher puts lib/ on its children's LD_LIBRARY_PATH)."""
    plugins = stage / 'plugins'
    for name in ('platforms/libqxcb.so', 'platforms/libqoffscreen.so', 'imageformats/libqwebp.so',
                 'imageformats/libqjpeg.so', 'imageformats/libqsvg.so', 'xcbglintegrations/libqxcb-glx-integration.so'):
        copy(qt / 'plugins' / name, plugins / name)
    tools = stage / TOOLCHAIN
    env = dict(os.environ, LD_LIBRARY_PATH=os.pathsep.join(map(str, [qt / 'lib', stage / 'lib', tools / 'gcc/lib'])))
    queue = [stage / launcher_name('linux'), stage / RELEASE_ENGINE / 'bin' / runtime_name('linux'),
             tools / 'python/bin/python3', tools / 'dotnet/dotnet', *plugins.rglob('*.so'),
             *stage.glob('lib/*.so*'), *[tools / 'shader-tools/bin' / tool for tool in ('spirv-as', 'spirv-val', 'spirv-opt')]]
    # .NET loads ICU/OpenSSL dynamically; SDL loads audio/display backends from the host desktop
    for pattern in ('libicu*.so.*', 'libssl.so.*', 'libcrypto.so.*'):
        queue.extend(Path('/usr/lib/x86_64-linux-gnu').glob(pattern))
    seen = set()
    system = re.compile(r'^(ld-linux|lib(c|m|pthread|dl|rt|resolv|util|nss_[^.]+)\.so)')
    records = {}
    while queue:
        binary = queue.pop()
        if str(binary) in seen: continue
        seen.add(str(binary))
        if binary.parent == Path('/usr/lib/x86_64-linux-gnu') and not system.match(binary.name):
            copy(binary, stage / 'lib' / binary.name)
            records.setdefault(binary.name, str(binary))
        output = subprocess.run(['ldd', str(binary)], env=env, capture_output=True, text=True, check=True).stdout
        if 'not found' in output: raise RuntimeError('Missing Linux dependency: ' + output)
        for name, path in re.findall(r'^\s*(\S+) => (/\S+)', output, re.M):
            if system.match(name): continue
            dst = stage / 'lib' / name
            if not dst.exists(): copy(Path(path), dst)
            records.setdefault(name, path)
            queue.append(Path(path))
    (stage / LEGAL / 'linux-libraries.json').write_text(json.dumps(records, indent=2) + '\n')
    # preserve the distribution copyright notices for copied system libraries
    for path in set(records.values()):
        result = subprocess.run(['dpkg-query', '-S', path], capture_output=True, text=True)
        if result.returncode: continue
        package = result.stdout.split(':', 1)[0]
        notice = Path('/usr/share/doc') / package / 'copyright'
        if notice.is_file(): copy(notice, stage / LEGAL / 'licenses/linux' / (package + '.copyright'))


def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()


def strip_gcc_rpath(stage):
    """conda's GCC specs add -rpath <install dir>/lib to every link, recording the packaging machine's path in each
    binary the bundle builds. Nothing needs it: the game code library links only libc/libm and the launcher puts
    toolchain/gcc/lib on LD_LIBRARY_PATH, so drop it. Nothing else needs patching on relocation: GCC derives its sysroot from its own location (spaces
    included) and never reads the clang-style x86_64-conda-linux-gnu.cfg."""
    for specs in stage.glob('toolchain/gcc/lib/gcc/x86_64-conda-linux-gnu/*/specs'):
        text = specs.read_text()
        stripped = re.sub(r'%\{!static:-rpath [^}]*\}', '', text)
        if stripped == text: raise RuntimeError('Unexpected GCC specs layout, no install rpath: ' + str(specs))
        specs.write_text(stripped)


def dereference_symlinks(stage):
    """Toolchain archives carry absolute symlinks (e.g. GCC's libgomp.so recorded against the toolchain's
    original build directory, which never exists on the packaging machine); a relocated bundle cannot follow
    them, so replace every file link with a copy of its target — falling back to the versioned sibling carrying the
    same basename (standard SONAME layout). Links whose library is not shipped anywhere in the stage (e.g. the
    dangling OpenMP dev link, unused by this build) are removed; anything else unresolvable is an error.
    Directory links (the GCC sysroot's lib and usr/lib -> lib64, which GCC searches as usr/lib/../lib64) stay
    links: kept as stored when relative, rewritten relative when absolute, provided they resolve inside the stage."""
    names = {p.name for p in stage.rglob('*') if p.is_file()}
    links = [p for p in stage.rglob('*') if p.is_symlink()]
    directories = {p: p.resolve() for p in links if p.is_dir()}   # before any link below is replaced
    for path in links:
        target = Path(os.readlink(path))
        if path in directories:
            if not directories[path].is_relative_to(stage.resolve()):
                raise RuntimeError('Directory symlink leaves the release: ' + str(path.relative_to(stage)))
            if target.is_absolute():
                path.unlink(); path.symlink_to(os.path.relpath(directories[path], path.parent.resolve()), True)
            continue
        candidates = [target] if target.is_absolute() else [Path(os.path.normpath(path.parent / target))]
        candidates.append(path.parent / target.name)
        resolved = next((candidate for candidate in candidates if candidate.is_file()), None)
        if resolved is not None:
            path.unlink(); shutil.copy2(resolved, path)
        elif target.name not in names:
            print('Removing dangling development symlink (library not shipped): ' + str(path.relative_to(stage)))
            path.unlink()
        else:
            raise RuntimeError('Non-relocatable symlink with unreadable target: ' + str(path.relative_to(stage)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--deps', type=Path, required=True)
    ap.add_argument('--launcher', type=Path, required=True)
    ap.add_argument('--qt', type=Path, required=True)
    ap.add_argument('--output', type=Path, default=ROOT / 'dist')
    ap.add_argument('--work', type=Path, help='Optional staging directory on a faster filesystem')
    ap.add_argument('--name', default=NAME)
    ap.add_argument('--stage-only', action='store_true')
    args = ap.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9._-]+', args.name): ap.error('Invalid archive name')
    platform = 'windows' if os.name == 'nt' else 'linux'
    args.output = args.output.resolve(); args.deps = args.deps.resolve(); args.qt = args.qt.resolve()
    args.launcher = args.launcher.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    verify_vp9_provenance(ENGINE, args.deps / 'dependency-manifest.json', platform)
    stage = (args.work.resolve() if args.work else args.output) / (args.name + '-' + platform + '-x64')
    # never merge a release with stale or unrelated files
    stage.mkdir(parents=True, exist_ok=False)
    # the launcher at the top, as deployed (Windows: with its Qt/MSVC DLLs and plugin folders)
    for name in launcher_files(args.launcher, platform): copy(args.launcher / name, stage / name)
    # engine/: the allowlisted engine files, the build CLI, the prebuilt runtime (+ hactool on Windows)
    engine = stage / RELEASE_ENGINE
    for name in engine_files(): copy(ENGINE / name, engine / name)
    copy(ROOT / 'src/recomp_cli.py', engine / 'recomp_cli.py')
    copy(ENGINE / 'bin' / runtime_name(platform), engine / 'bin' / runtime_name(platform))
    if platform == 'windows':   # nsp.py looks for engine/tools/hactool; the binary is a pinned download, its LICENSE tracked
        copy(args.deps / 'tools/hactool/hactool.exe', engine / 'tools/hactool/hactool.exe')
        copy(ENGINE / 'tools/hactool/LICENSE', engine / 'tools/hactool/LICENSE')
    # toolchain/: the bundled interpreters and compilers
    for name in TOOLS: tree(args.deps / name, stage / TOOLCHAIN / name)
    # legal/: notices, corresponding sources and provenance
    legal = stage / LEGAL
    for source, name in ENGINE_LEGAL.items(): copy(ENGINE / source, legal / name)
    tree(args.deps / 'licenses', legal / 'licenses')
    tree(args.deps / 'sources', legal / 'sources')
    copy(args.deps / 'dependency-manifest.json', legal / 'dependency-manifest.json')
    if platform == 'linux':
        tree(args.deps / 'lib', stage / 'lib')
        linux_libraries(stage, args.qt)
        (stage / 'qt.conf').write_text('[Paths]\nPlugins=plugins\n')
        strip_gcc_rpath(stage)
    dereference_symlinks(stage)
    validate_contents(stage, platform)
    (legal / 'build-info.json').write_text(json.dumps(provenance(platform), indent=2) + '\n')
    sums = legal / 'SHA256SUMS'   # paths relative to the release root: cd <release> && sha256sum -c legal/SHA256SUMS
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    sums.write_text(''.join(digest(p) + '  ' + p.relative_to(stage).as_posix() + '\n' for p in files))
    if args.stage_only:
        print(stage); return
    if platform == 'windows':
        archive = args.output / (stage.name + '.zip')
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for p in sorted(stage.rglob('*')):
                if p.is_file(): z.write(p, p.relative_to(stage.parent))
    else:
        archive = args.output / (stage.name + '.tar.gz')
        with tarfile.open(archive, 'w:gz') as tar: tar.add(stage, arcname=stage.name)
    Path(str(archive) + '.sha256').write_text(digest(archive) + '  ' + archive.name + '\n')
    print(archive)


if __name__ == '__main__': main()
