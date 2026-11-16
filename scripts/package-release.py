#!/usr/bin/env python3
# RecompLauncher - native game library and build launcher.
# The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
"""Stage an allowlisted, relocatable toolkit and archive it with a SHA-256 manifest.

The bundle mirrors the repository layout, so the launcher finds everything the same way it does in a checkout:
the launcher, src/recomp_cli.py and the shared tools (python, gcc, dotnet, shader-tools) at the root, and the
engine submodule with its runtime, VP9 archive and hactool under third_party/Switch2RECOMP."""
import argparse
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
NAME = 'RecompLauncher'   # archive, top-level folder and start-script name
ENGINE_DIR = Path('third_party/Switch2RECOMP')   # same relative path in the checkout and in the bundle
ENGINE = ROOT / ENGINE_DIR
LAUNCHER_DIRS = {'src', 'scripts', 'docs', 'tests', '.github'}
LAUNCHER_FILES = {'README.md', '.gitattributes', '.gitignore', '.gitmodules'}
ENGINE_DIRS = {'runtime', 'tools', 'tool', 'tests', 'docs'}
ENGINE_FILES = {'a64.py', 'brt.py', 'codegen.py', 'nup.py', 'nsp.py', 'nxloader.py', 'switchrecomp.py', 'tools_sym.py',
                'LICENSE.txt', 'NOTICE.txt', 'CHANGELOG.txt', 'README.md', 'README.txt', 'DEVELOPER_GUIDE.md',
                'REGRESSION.md', '.gitattributes', '.gitignore'}
JUNK = {'__pycache__', '_downloads', '.git', '.venv', 'venv', '.pytest_cache', 'shader_cache',
        'captures', 'logs', 'games', 'saves', 'savedata', 'firmware'}
PRIVATE_NAMES = {'library.json', 'python.path', 'performance.txt', 'threads.txt', 'prod.keys', 'title.keys'}
PRIVATE_SUFFIXES = {'.log', '.pyc', '.nup', '.nsp', '.xci', '.keys', '.nca', '.nso', '.nro'}


def excluded(path):
    return (any(part.lower() in JUNK for part in path.parts) or path.name.lower() in PRIVATE_NAMES
            or path.suffix.lower() in PRIVATE_SUFFIXES)


def runtime_name(platform):
    return 'runtime_win64_win32.exe' if platform == 'windows' else 'runtime_linux_x11'


def vp9_path(platform):
    return Path('runtime/vp9') / ('win' if platform == 'windows' else 'lin') / 'libvpx.a'


def required_files(platform):
    engine = ['switchrecomp.py', 'nup.py', 'nsp.py', 'runtime/rt.h', 'runtime/audio.h',
              'runtime/audio_limit.h', 'runtime/audio_queue.h', 'runtime/controller_sony.h',
              'runtime/controller_sony_win32.h', 'runtime/nvn/gfx/dmsh_abi.h',
              'runtime/nvn/vk/vk_perf.c', 'runtime/nvn/vk/vk_perf.h', 'tools/build_nvn.py',
              'tools/s2r_paths.py', 'tools/nxshader/s2r_paths.py', 'tools/nxshader/nxs_live.py',
              'tools/nxshader/nxs_cache.py', 'tools/nxshader/nxs_front.py', 'tools/nxshader/nxshader.py',
              'tools/nxshader/nxs_refl.py', 'tools/nxshader/nxs_spv.py', 'tools/nxshader/dmsh_abi.py',
              'tools/nxshader/dmsh_ampere.py', 'tools/nxshader/dmsh_meshstage.py',
              'tools/nxshader/xlate_bin/dmsh_xlate.dll', 'tools/nxshader/xlate_bin/dmsh_xlate.deps.json',
              'tools/nxshader/xlate_bin/dmsh_xlate.runtimeconfig.json',
              'tools/nxshader/xlate_bin/LICENSE-Ryujinx.txt', 'tools/nxshader/xlate_bin/LICENSE-nxnative.txt',
              'LICENSE.txt', 'NOTICE.txt', 'docs/THIRD_PARTY.md',
              'runtime/vp9/LICENSE-libvpx.txt', 'runtime/vp9/PATENTS-libvpx.txt',
              vp9_path(platform).as_posix(), 'bin/' + runtime_name(platform)]
    common = ['src/recomp_cli.py', 'docs/RELEASES.md',
              'dependency-manifest.json', 'dotnet/LICENSE.txt', 'dotnet/ThirdPartyNotices.txt',
              'sources/libvpx_1.14.0-1ubuntu2.3.dsc', 'sources/libvpx_1.14.0.orig.tar.xz',
              'sources/libvpx_1.14.0-1ubuntu2.3.debian.tar.xz']
    common += ['sources/' + module + '-everywhere-src-6.8.3.tar.xz' for module in ('qtbase', 'qtimageformats', 'qtsvg')]
    if platform == 'windows':
        engine += ['tools/hactool/hactool.exe', 'tools/hactool/LICENSE']
        common += [NAME + '.cmd', 'python/python.exe', 'python/LICENSE.txt',
                   'gcc/Library/bin/x86_64-w64-mingw32-gcc.exe', 'dotnet/dotnet.exe', 'scripts/_python.bat',
                   'bin/launcher/SwitchRecompLauncher.exe', 'bin/launcher/Qt6Core.dll',
                   'bin/launcher/Qt6Gui.dll', 'bin/launcher/Qt6Widgets.dll',
                   'bin/launcher/platforms/qwindows.dll', 'bin/launcher/platforms/qoffscreen.dll',
                   'bin/launcher/imageformats/qwebp.dll', 'bin/launcher/msvcp140.dll',
                   'bin/launcher/vcruntime140.dll', 'bin/launcher/vcruntime140_1.dll'] + [
                   'shader-tools/Library/bin/' + tool + '.exe' for tool in ('spirv-as', 'spirv-val', 'spirv-opt')]
    else:
        common += [NAME + '.sh', 'fix-gcc.sh', 'python/bin/python3', 'dotnet/dotnet', 'scripts/_env.sh',
                   'gcc/bin/x86_64-conda-linux-gnu-gcc',
                   'bin/launcher/SwitchRecompLauncher', 'bin/launcher/plugins/platforms/libqxcb.so',
                   'bin/launcher/plugins/platforms/libqoffscreen.so', 'bin/launcher/plugins/imageformats/libqwebp.so',
                   'lib/libSDL2-2.0.so.0', 'lib/libQt6Core.so.6', 'linux-libraries.json',
                   'licenses/python/PYTHON.json', 'licenses/python/licenses/LICENSE.cpython.txt'] + [
                   'shader-tools/bin/' + tool for tool in ('spirv-as', 'spirv-val', 'spirv-opt')]
    return common + [(ENGINE_DIR / name).as_posix() for name in engine]


def validate_contents(stage, platform):
    for path in stage.rglob('*'):
        relative = path.relative_to(stage)
        if excluded(relative): raise RuntimeError('Personal/generated release input: ' + str(relative))
        if path.is_symlink() and (os.path.isabs(os.readlink(path)) or not path.exists()
                                  or not path.resolve().is_relative_to(stage.resolve())):
            raise RuntimeError('Non-relocatable symlink: ' + str(relative))
    for name in required_files(platform):
        if not (stage / name).is_file() or not (stage / name).stat().st_size: raise FileNotFoundError(name)
    for name in ('licenses/qt/qtbase', 'licenses/qt/qtimageformats', 'licenses/qt/qtsvg', 'gcc/licenses', 'shader-tools/licenses'):
        if not any(p.is_file() for p in (stage / name).rglob('*')): raise FileNotFoundError(name)
    manifest = json.loads((stage / 'dependency-manifest.json').read_text())
    if manifest['platform'] != platform:
        raise RuntimeError('Dependency manifest platform mismatch')
    vpx = manifest['downloads']['libvpx']
    lib = stage / ENGINE_DIR / vp9_path(platform)
    if digest(lib) != vpx['sha256']: raise RuntimeError('VP9 archive does not match dependency provenance')
    for name, record in vpx['sources'].items():
        if digest(stage / 'sources' / name) != record['sha256']: raise RuntimeError('VP9 source checksum: ' + name)
    runtime = stage / ENGINE_DIR / 'bin' / runtime_name(platform)
    data = runtime.read_bytes()
    # these strings live in the linked decoder, telemetry and controller implementations, not build scripts
    markers = [b'WebM Project VP9 Decoder', b'frame telemetry:']
    if platform == 'windows': markers += [b'hid.dll', b'setupapi.dll']
    for marker in markers:
        if marker not in data: raise RuntimeError('Native runtime feature missing: ' + marker.decode())


def copy(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def tree(src, dst):
    if not src.is_dir(): raise FileNotFoundError(src)
    shutil.copytree(src, dst, dirs_exist_ok=True, symlinks=True,
                    ignore=shutil.ignore_patterns(*JUNK, '*.pyc', '*.pdb', '*.log'))


def source_files():
    """Allowlisted files of the launcher checkout, then of the engine submodule, as paths relative to ROOT.
    git ls-files never descends into a submodule (it lists only its gitlink), so the engine is listed on its own."""
    if not (ENGINE / 'switchrecomp.py').is_file():
        raise FileNotFoundError('Engine submodule is not checked out: git submodule update --init')
    for base, prefix, dirs, files in ((ROOT, Path(), LAUNCHER_DIRS, LAUNCHER_FILES),
                                      (ENGINE, ENGINE_DIR, ENGINE_DIRS, ENGINE_FILES)):
        paths = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=base).decode().split('\0')
        for name in sorted(set(paths)):
            p = Path(name)
            if not name or not (p.parts[0] in dirs or name in files): continue
            if excluded(p): continue
            if (base / p).is_file(): yield prefix / p


def git(repo, *args, optional=False):
    return subprocess.check_output(['git', *args], cwd=repo, text=True,
                                   stderr=subprocess.DEVNULL if optional else None).strip()


def provenance(platform):
    """Both commits: the launcher checkout and the engine submodule pinned in it."""
    dirty = any(git(repo, 'status', '--porcelain', '--untracked-files=normal') for repo in (ROOT, ENGINE))
    engine_commit = git(ENGINE, 'rev-parse', 'HEAD')
    info = {'commit': git(ROOT, 'rev-parse', 'HEAD'), 'engine_commit': engine_commit,
            'platform': platform, 'local_changes': dirty}
    try: engine_url = git(ROOT, 'config', '-f', '.gitmodules', 'submodule.%s.url' % ENGINE_DIR.as_posix(), optional=True)
    except subprocess.CalledProcessError: engine_url = ''
    if engine_url: info['engine_source'] = engine_url.removesuffix('.git') + '/tree/' + engine_commit
    try: info['source'] = git(ROOT, 'remote', 'get-url', 'origin', optional=True).removesuffix('.git') + '/tree/' + info['commit']
    except subprocess.CalledProcessError: pass
    return info


def linux_libraries(stage, qt):
    plugins = stage / 'bin/launcher/plugins'
    for name in ('platforms/libqxcb.so', 'platforms/libqoffscreen.so', 'imageformats/libqwebp.so',
                 'imageformats/libqjpeg.so', 'imageformats/libqsvg.so', 'xcbglintegrations/libqxcb-glx-integration.so'):
        copy(qt / 'plugins' / name, plugins / name)
    env = dict(os.environ, LD_LIBRARY_PATH=os.pathsep.join(map(str, [qt / 'lib', stage / 'lib', stage / 'gcc/lib'])))
    queue = [stage / 'bin/launcher/SwitchRecompLauncher', stage / ENGINE_DIR / 'bin/runtime_linux_x11',
             stage / 'python/bin/python3', stage / 'dotnet/dotnet', *plugins.rglob('*.so'),
             *stage.glob('lib/*.so*'), *[stage / 'shader-tools/bin' / tool for tool in ('spirv-as', 'spirv-val', 'spirv-opt')]]
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
    (stage / 'linux-libraries.json').write_text(json.dumps(records, indent=2) + '\n')
    # preserve the distribution copyright notices for copied system libraries
    for path in set(records.values()):
        result = subprocess.run(['dpkg-query', '-S', path], capture_output=True, text=True)
        if result.returncode: continue
        package = result.stdout.split(':', 1)[0]
        notice = Path('/usr/share/doc') / package / 'copyright'
        if notice.is_file(): copy(notice, stage / 'licenses/linux' / (package + '.copyright'))


def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()


GCC_FIXUP = '''#!/bin/sh
# The bundled conda GCC bakes absolute paths in while the release is packaged (its sysroot, an rpath inside
# specs and the linker scripts) and, being GCC, it cannot run from a directory containing spaces. Repoint the
# baked paths at a space-free alias of this copy and wrap the drivers so every run resolves the alias as
# --sysroot. Safe to run repeatedly; recreates the alias if /tmp was cleared.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
gcc="$root/gcc"
if [ ! -d "$gcc" ]; then exit 0; fi
home=/tmp/switchrecomp-toolchains
mkdir -p "$home" 2>/dev/null || true
hash=$(printf '%s\\n' "$gcc" | cksum | cut -d' ' -f1)
alias_name="$home/$hash"
if [ ! -e "$alias_name" ]; then ln -s "$gcc" "$alias_name" 2>/dev/null || true; fi
cfg="$gcc/bin/x86_64-conda-linux-gnu.cfg"
have=''
if [ -f "$cfg" ]; then
    have=$(sed -n 's|^--sysroot=\\(.*\\)/x86_64-conda-linux-gnu/sysroot$|\\1|p' "$cfg")
    : > "$cfg"
fi
if [ -n "$have" ] && [ "$have" != "$alias_name" ]; then
    esc=$(printf '%s\\n' "$have" | sed 's/[][\\\\*^$|]/\\\\&/g')
    rep=$(printf '%s\\n' "$alias_name" | sed 's/[&\\\\|]/\\\\&/g')
    for f in "$gcc"/lib/gcc/x86_64-conda-linux-gnu/*/specs "$gcc"/x86_64-conda-linux-gnu/lib/ldscripts/*; do
        if [ -f "$f" ]; then sed -i "s|$esc|$rep|g" "$f"; fi
    done
fi
for name in gcc g++ c++ cpp; do
    driver="$gcc/bin/x86_64-conda-linux-gnu-$name"
    if [ -f "$driver" ] && [ ! -f "$driver.bin" ]; then
        mv "$driver" "$driver.bin"
        printf '#!/bin/sh\\nset -eu\\ngcc=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\\nhome=/tmp/switchrecomp-toolchains\\nmkdir -p "$home" 2>/dev/null || true\\na="$home/%s"\\nif [ ! -e "$a" ]; then ln -s "$gcc" "$a" 2>/dev/null || true; fi\\nif [ -e "$a" ]; then exec "$a/bin/x86_64-conda-linux-gnu-%s.bin" --sysroot="$a/x86_64-conda-linux-gnu/sysroot" "$@"; fi\\nexec "$gcc/bin/x86_64-conda-linux-gnu-%s.bin" --sysroot="$gcc/x86_64-conda-linux-gnu/sysroot" "$@"\\n' \\
            "$hash" "$name" "$name" > "$driver"
        chmod +x "$driver" "$driver.bin"
    fi
done
'''


def dereference_symlinks(stage):
    """Toolchain archives carry absolute symlinks (e.g. GCC's libgomp.so recorded against the toolchain's
    original build directory, which never exists on the packaging machine); a relocated bundle cannot follow
    them, so replace every link with a copy of its target — falling back to the versioned sibling carrying the
    same basename (standard SONAME layout). Links whose library is not shipped anywhere in the stage (e.g. the
    dangling OpenMP dev link, unused by this build) are removed; anything else unresolvable is an error."""
    names = {p.name for p in stage.rglob('*') if p.is_file()}
    for path in [p for p in stage.rglob('*') if p.is_symlink()]:
        target = Path(os.readlink(path))
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
    ap = argparse.ArgumentParser(description=__doc__)
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
    args.output.mkdir(parents=True, exist_ok=True)
    stage = (args.work.resolve() if args.work else args.output) / (args.name + '-' + platform + '-x64')
    # never merge a release with stale or unrelated files
    stage.mkdir(parents=True, exist_ok=False)
    for path in source_files(): copy(ROOT / path, stage / path)
    for name in ('python', 'gcc', 'dotnet', 'shader-tools'): tree(args.deps / name, stage / name)
    if platform == 'windows': tree(args.deps / 'tools/hactool', stage / ENGINE_DIR / 'tools/hactool')
    copy(args.deps / 'dependency-manifest.json', stage / 'dependency-manifest.json')
    tree(args.launcher, stage / 'bin/launcher')
    runtime = Path('bin') / runtime_name(platform)
    copy(ENGINE / runtime, stage / ENGINE_DIR / runtime)
    # Linux VP9 is generated by release-deps and ignored by Git; never silently drop it.
    copy(ENGINE / vp9_path(platform), stage / ENGINE_DIR / vp9_path(platform))
    tree(args.deps / 'licenses', stage / 'licenses')
    tree(args.deps / 'sources', stage / 'sources')
    if platform == 'windows':
        (stage / (NAME + '.cmd')).write_text('@echo off\n"%~dp0bin\\launcher\\SwitchRecompLauncher.exe" --root "%~dp0third_party\\Switch2RECOMP" %*\n', newline='\r\n')
    else:
        tree(args.deps / 'lib', stage / 'lib')
        linux_libraries(stage, args.qt)
        (stage / 'bin/launcher/qt.conf').write_text('[Paths]\nPlugins=plugins\n')
        wrapper = stage / (NAME + '.sh')
        wrapper.write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                           'if [ -x "$root/fix-gcc.sh" ]; then "$root/fix-gcc.sh"; fi\n'
                           'export LD_LIBRARY_PATH="$root/lib:$root/gcc/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
                           'export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"\n'
                           'exec "$root/bin/launcher/SwitchRecompLauncher" --root "$root/third_party/Switch2RECOMP" "$@"\n')
        wrapper.chmod(0o755)
        fixup = stage / 'fix-gcc.sh'; fixup.write_text(GCC_FIXUP); fixup.chmod(0o755)
    dereference_symlinks(stage)
    validate_contents(stage, platform)
    (stage / 'build-info.json').write_text(json.dumps(provenance(platform), indent=2) + '\n')
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    (stage / 'SHA256SUMS').write_text(''.join(digest(p) + '  ' + p.relative_to(stage).as_posix() + '\n' for p in files))
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
