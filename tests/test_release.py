"""Packaging contracts with disposable bundles; no downloads, runtime builds or games."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]                    # nxrs checkout
sys.path.insert(0, str(ROOT / 'third_party' / 'Switch2RECOMP'))  # engine submodule (switchrecomp)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


package = module('package_release', 'scripts/package-release.py')
smoke = module('smoke_release', 'scripts/smoke-release.py')
dependencies = module('release_dependencies', 'scripts/release-deps.py')
ENGINE = package.RELEASE_ENGINE      # engine/ inside a release
TOOLCHAIN = package.TOOLCHAIN        # toolchain/
LEGAL = package.LEGAL                # legal/


class ReleaseChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='release fixtures [spaces] ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def bundle(self, platform):
        root = self.root / platform; root.mkdir()
        for name in package.required_files(platform):
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'fixture')
        for name in ('legal/licenses/qt/qtbase', 'legal/licenses/qt/qtimageformats', 'legal/licenses/qt/qtsvg',
                     'toolchain/gcc/licenses', 'toolchain/shader-tools/licenses'):
            path = root / name; path.mkdir(parents=True, exist_ok=True); (path / 'LICENSE').write_text('fixture')
        runtime = root / ENGINE / 'bin' / package.runtime_name(platform)
        runtime.write_bytes(b'WebM Project VP9 Decoder\0dav1d %s decoding AV1\0Error parsing OBU data\0audio: opus decoder %uHz\0frame telemetry:\0hid.dll\0setupapi.dll')
        manifest = {'platform': platform, 'downloads': {'libvpx': {'sha256': '0' * 64, 'sources': {
            p.name: {'sha256': package.digest(p)} for p in (root / LEGAL / 'sources').glob('libvpx*')}},
            'dav1d': {'sha256': '0' * 64, 'sources': {
            p.name: {'sha256': package.digest(p)} for p in (root / LEGAL / 'sources').glob('dav1d*')}}}}
        (root / LEGAL / 'dependency-manifest.json').write_text(json.dumps(manifest))
        return root

    def sums(self, root):
        sums = root / LEGAL / 'SHA256SUMS'
        sums.write_text(''.join(package.digest(p) + '  ' + p.relative_to(root).as_posix() + '\n'
                                for p in sorted(root.rglob('*')) if p.is_file() and p != sums))

    def test_complete_bundles_and_missing_new_files(self):
        for platform in ('windows', 'linux'):
            root = self.bundle(platform)
            package.validate_contents(root, platform)
            for name in [(ENGINE / n).as_posix() for n in ('runtime/cpu.h', 'runtime/cpu_helpers.c', 'runtime/nvn/gfx/dmsh_abi.h',
                         'recomp_cli.py', 'nup.py', 'nca.py', 'zbic.py', 'dlc.py', 'tools/nxshader/s2r_paths.py',
                         'tools/nxshader/xlate_bin/dmsh_xlate.runtimeconfig.json', 'bin/' + package.runtime_name(platform))] + [
                         package.launcher_name(platform), 'legal/NOTICE.txt', 'legal/licenses/libvpx/LICENSE-libvpx.txt',
                         'legal/licenses/dav1d/LICENSE-dav1d.txt', 'legal/licenses/opus/LICENSE-opus.txt', 'legal/sources/dav1d-1.5.5.tar.xz',
                         'legal/sources/libvpx_1.14.0-1ubuntu2.3.debian.tar.xz', 'toolchain/dotnet/LICENSE.txt']:
                path = root / name; data = path.read_bytes(); path.unlink()
                with self.assertRaises(FileNotFoundError, msg=name): package.validate_contents(root, platform)
                path.write_bytes(data)

    def test_layout_rejects_sources_and_developer_clutter(self):
        # what the old repository-mirror bundle shipped: none of it belongs in a release
        clutter = {'windows': ['engine/runtime/gpu.c', 'engine/runtime/edenvk/deps/zstd/lib/zstd.h', 'engine/tests/test_nup.py',
                               'engine/docs/THIRD_PARTY.md', 'engine/tool/get_deps.py', 'engine/tools/s2r_paths.py',
                               'engine/tools/build_nvn.py', 'engine/runtime/vp9/win/libvpx.a', 'engine/tools/nxshader/run_split.py', 'engine/tools/nxshader/xlate_bin/dmsh_xlate',
                               'engine/LICENSE.txt', 'src/recomp_cli.py', 'scripts/_python.bat', '.gitignore', 'nxrs.cmd',
                               'vc_redist.x64.exe', 'third_party/Switch2RECOMP/switchrecomp.py', 'bin/launcher/nxrs.exe',
                               'docs/RELEASES.md', 'SwitchRecompLauncher.exe'],
                   'linux': ['engine/runtime/backend_x11.c', 'engine/tools/hactool/hactool.exe', 'nxrs.sh',
                             'scripts/_env.sh', 'gcc/bin/gcc', 'README.md', 'bin/launcher/nxrs']}
        for platform, names in clutter.items():
            root = self.bundle(platform); package.validate_contents(root, platform)
            for name in names:
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'clutter')
                with self.assertRaisesRegex(RuntimeError, 'Unexpected top-level|outside the release allowlist', msg=name):
                    package.validate_contents(root, platform)
                path.unlink()
                while path.parent != root and not list(path.parent.iterdir()):
                    path = path.parent; path.rmdir()
            package.validate_contents(root, platform)

    def test_runtime_requires_vp9_nvn_telemetry_and_sony(self):
        root = self.bundle('windows'); runtime = root / ENGINE / 'bin/runtime_win64_win32.exe'; data = runtime.read_bytes()
        for marker in (b'WebM Project VP9 Decoder', b'dav1d %s decoding AV1', b'Error parsing OBU data', b'audio: opus decoder %uHz', b'frame telemetry:', b'hid.dll', b'setupapi.dll'):
            runtime.write_bytes(data.replace(marker, b''))
            with self.assertRaisesRegex(RuntimeError, 'Native runtime feature'): package.validate_contents(root, 'windows')
        runtime.write_bytes(data)
        # the linked libvpx archive is a checkout build input: it must match the provenance the release records
        engine = self.root / 'checkout engine'; lib = engine / package.vp9_path('windows')
        lib.parent.mkdir(parents=True); lib.write_bytes(b'pinned archive')
        manifest = root / LEGAL / 'dependency-manifest.json'; record = json.loads(manifest.read_text())
        record['downloads']['libvpx']['sha256'] = package.digest(lib); manifest.write_text(json.dumps(record))
        package.verify_vp9_provenance(engine, manifest, 'windows')
        lib.write_bytes(b'wrong target library')
        with self.assertRaisesRegex(RuntimeError, 'provenance'): package.verify_vp9_provenance(engine, manifest, 'windows')
        # and the linked libdav1d archive (AV1), the same way; a wrong AV1 source in the bundle is refused too
        lib = engine / package.av1_path('windows'); lib.parent.mkdir(parents=True); lib.write_bytes(b'pinned archive')
        record['downloads']['dav1d']['sha256'] = package.digest(lib); manifest.write_text(json.dumps(record))
        package.verify_av1_provenance(engine, manifest, 'windows')
        lib.write_bytes(b'wrong target library')
        with self.assertRaisesRegex(RuntimeError, 'AV1 archive'): package.verify_av1_provenance(engine, manifest, 'windows')
        source = root / LEGAL / 'sources' / 'dav1d-1.5.5.tar.xz'; source.write_bytes(b'another archive')
        with self.assertRaisesRegex(RuntimeError, 'AV1 source checksum'): package.validate_contents(root, 'windows')

    def test_private_inputs_rejected(self):
        root = self.bundle('windows')
        for name in ('legal/library.json', 'engine/tools/nxshader/python.path', 'engine/runtime/prod.keys', 'engine/runtime/cache.NSP',
                     'toolchain/saves/private.dat', 'toolchain/.venv/Scripts/python.exe', 'logs/session.txt', 'shader_cache/cache.bin'):
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'private')
            with self.assertRaisesRegex(RuntimeError, 'Personal/generated'): package.validate_contents(root, 'windows')
            path.unlink()
            while path.parent != root and not list(path.parent.iterdir()):
                path = path.parent; path.rmdir()

    def test_manifest_rejects_extra_missing_changed_and_duplicate_files(self):
        root = self.bundle('linux'); self.sums(root); smoke.verify_manifest(root)
        manifest = root / LEGAL / 'SHA256SUMS'; original = manifest.read_text()
        extra = root / 'extra.txt'; extra.write_text('unlisted')
        with self.assertRaisesRegex(RuntimeError, 'exactly'): smoke.verify_manifest(root)
        extra.unlink()
        notice = root / LEGAL / 'NOTICE.txt'; notice.unlink()
        with self.assertRaises(FileNotFoundError): smoke.verify_manifest(root)
        notice.write_text('changed')
        with self.assertRaisesRegex(RuntimeError, 'Checksum'): smoke.verify_manifest(root)
        notice.write_text('fixture')
        manifest.write_text(original + original.splitlines()[0] + '\n')
        with self.assertRaisesRegex(RuntimeError, 'duplicate'): smoke.verify_manifest(root)

    def test_relocated_archives_and_outer_checksum(self):
        for platform in ('windows', 'linux'):
            root = self.bundle(platform); self.sums(root)
            archive = self.root / (platform + ('.zip' if platform == 'windows' else '.tar.gz'))
            if platform == 'windows':
                with zipfile.ZipFile(archive, 'w') as z:
                    for p in root.rglob('*'):
                        if p.is_file(): z.write(p, p.relative_to(root.parent))
            else:
                with tarfile.open(archive, 'w:gz') as tar: tar.add(root, arcname=root.name)
            checksum = Path(str(archive) + '.sha256'); checksum.write_text(package.digest(archive) + '  ' + archive.name + '\n')
            dest = self.root / ('relocated ' + platform); dest.mkdir()
            extracted = smoke.extract_archive(archive, dest)
            smoke.verify_manifest(extracted); package.validate_contents(extracted, platform)
            checksum.write_text('0' * 64 + '  ' + archive.name + '\n')
            with self.assertRaisesRegex(RuntimeError, 'Archive checksum'): smoke.extract_archive(archive, dest)

    def test_unsafe_archive_and_manifest_paths(self):
        for name in ('../escape', '/absolute', 'C:/outside', 'bundle/../../escape', 'bundle\\escape', './alias', 'a//b'):
            with self.assertRaisesRegex(RuntimeError, 'Unsafe'): smoke.safe_name(name)
        archive = self.root / 'bad.zip'
        with zipfile.ZipFile(archive, 'w') as z: z.writestr('../escape', 'bad')
        Path(str(archive) + '.sha256').write_text(package.digest(archive) + '  bad.zip\n')
        dest = self.root / 'extract'; dest.mkdir()
        with self.assertRaisesRegex(RuntimeError, 'Unsafe'): smoke.extract_archive(archive, dest)
        self.assertEqual(list(dest.iterdir()), [])

    @unittest.skipIf(os.name == 'nt', 'Unix symlink archive contract')
    def test_external_and_broken_symlinks_rejected(self):
        root = self.bundle('linux'); link = root / 'outside'
        for target in (self.root / 'missing', self.root, root / LEGAL / 'NOTICE.txt'):
            link.symlink_to(target)
            with self.assertRaisesRegex(RuntimeError, 'Non-relocatable'): package.validate_contents(root, 'linux')
            link.unlink()

    def test_gcc_install_rpath_stripped_from_specs(self):
        root = self.bundle('linux')
        specs = root / 'toolchain/gcc/lib/gcc/x86_64-conda-linux-gnu/16.2.0/specs'; specs.parent.mkdir(parents=True, exist_ok=True)
        specs.write_text('*link:\n%{T*}   %{!static:-rpath /home/runner/work/x [y]/gcc/lib}\n\n*lib:\n-lc\n')
        package.strip_gcc_rpath(root)
        self.assertEqual(specs.read_text(), '*link:\n%{T*}   \n\n*lib:\n-lc\n')
        with self.assertRaisesRegex(RuntimeError, 'specs layout'): package.strip_gcc_rpath(root)   # layout changed upstream

    @unittest.skipIf(os.name == 'nt', 'Unix symlink archive contract')
    def test_directory_symlinks_kept_relative_inside_the_stage(self):
        # the conda GCC sysroot: lib and usr/lib -> lib64; GCC searches <sysroot>/usr/lib/../lib64 for Scrt1.o
        root = self.bundle('linux'); sysroot = root / 'toolchain/gcc/x86_64-conda-linux-gnu/sysroot'
        (sysroot / 'usr/lib64').mkdir(parents=True); (sysroot / 'usr/lib64/Scrt1.o').write_bytes(b'crt')
        (sysroot / 'lib64').mkdir(); (root / 'toolchain/gcc/share/zoneinfo').mkdir(parents=True)
        (sysroot / 'usr/lib').symlink_to('lib64')                                  # relative, kept as stored
        (sysroot / 'lib').symlink_to(sysroot / 'lib64')                            # absolute inside: made relative
        (sysroot / 'usr/share').mkdir(); (sysroot / 'usr/share/zoneinfo').symlink_to('../../../../share/zoneinfo')
        package.dereference_symlinks(root)
        self.assertEqual(os.readlink(sysroot / 'usr/lib'), 'lib64')
        self.assertEqual(os.readlink(sysroot / 'lib'), 'lib64')
        self.assertEqual(os.readlink(sysroot / 'usr/share/zoneinfo'), '../../../../share/zoneinfo')
        self.assertEqual((sysroot / 'usr/lib/../lib64/Scrt1.o').read_bytes(), b'crt')
        package.validate_contents(root, 'linux')
        moved = self.root / 'relocated [x]'; root.rename(moved)                    # still resolves after moving
        self.assertEqual((moved / 'toolchain/gcc/x86_64-conda-linux-gnu/sysroot/usr/lib/Scrt1.o').read_bytes(), b'crt')
        outside = self.root / 'outside-dir'; outside.mkdir()
        (moved / 'toolchain/gcc/escape').symlink_to(outside)
        with self.assertRaisesRegex(RuntimeError, 'Directory symlink leaves'): package.dereference_symlinks(moved)

    @unittest.skipIf(os.name == 'nt', 'Unix symlink archive contract')
    def test_dereference_replaces_nonrelocatable_links_with_target_copies(self):
        root = self.bundle('linux')
        (root / 'toolchain/gcc/lib').mkdir(parents=True, exist_ok=True)
        real = root / 'toolchain/gcc/lib/libgomp.so.1.0.0'; real.write_bytes(b'gomp')
        (root / 'toolchain/gcc/lib/libgomp.so').symlink_to(real)             # relative, inside the stage
        (root / 'toolchain/gcc/lib/absolute.so').symlink_to(self.root / 'outside-target')
        (self.root / 'outside-target').write_bytes(b'absolute')
        (root / 'toolchain/gcc/lib/gcc-ghost.so').symlink_to(Path('/never-existed/lib/gcc-ghost.so.1.0.0'))  # toolchain-build path
        (root / 'toolchain/gcc/lib/gcc-ghost.so.1.0.0').write_bytes(b'ghost')
        package.dereference_symlinks(root)
        self.assertFalse((root / 'toolchain/gcc/lib/libgomp.so').is_symlink())
        self.assertEqual((root / 'toolchain/gcc/lib/libgomp.so').read_bytes(), b'gomp')
        self.assertFalse((root / 'toolchain/gcc/lib/absolute.so').is_symlink())
        self.assertEqual((root / 'toolchain/gcc/lib/absolute.so').read_bytes(), b'absolute')
        self.assertFalse((root / 'toolchain/gcc/lib/gcc-ghost.so').is_symlink())
        self.assertEqual((root / 'toolchain/gcc/lib/gcc-ghost.so').read_bytes(), b'ghost')
        package.validate_contents(root, 'linux')                   # the fixed tree now validates
        (root / 'toolchain/gcc/lib/libopenmp.so').symlink_to(Path('/never/libopenmp.so'))  # dev link, library not shipped
        package.dereference_symlinks(root)
        self.assertFalse((root / 'toolchain/gcc/lib/libopenmp.so').exists())  # dangling dev link removed
        package.validate_contents(root, 'linux')
        (root / 'toolchain/gcc/lib/suspicious.so').symlink_to(self.root / 'missing-dir/NOTICE.txt')  # basename IS shipped
        with self.assertRaisesRegex(RuntimeError, 'unreadable target'): package.dereference_symlinks(root)

    def test_engine_allowlist_takes_tracked_files_only(self):
        engine = self.root / 'engine checkout'
        wanted = [p.replace('*.', 'dmsh_xlate.').replace('*', 'Ryujinx') for p in package.ENGINE_FILES]
        unwanted = ['runtime/gpu.c', 'runtime/rt.h', 'runtime/edenvk/deps/zstd/lib/zstd.h', 'tests/test_nup.py', 'docs/README.md',
                    'tool/get_deps.py', 'tools/s2r_paths.py', 'tools/build_nvn.py', 'tools/nxshader/run_split.py',
                    'runtime/vp9/win/libvpx.a', 'brt.py', 'tools_sym.py', 'tools/nxshader/__pycache__/nxs_live.cpython-314.pyc',
                    'tools/nxshader/library.json', 'runtime/prod.keys', 'tools/nxshader/xlate_bin/dmsh_xlate']
        for name in wanted + unwanted + ['untracked.py']:
            p = engine / name; p.parent.mkdir(parents=True, exist_ok=True); p.touch()
        listing = lambda command, cwd: '\0'.join(wanted + unwanted).encode()   # what git ls-files reports as tracked
        with patch.object(package, 'ENGINE', engine), patch.object(package.subprocess, 'check_output', side_effect=listing):
            self.assertEqual({p.as_posix() for p in package.engine_files()}, set(wanted))
        (engine / 'nup.py').unlink(); wanted.remove('nup.py')
        with patch.object(package, 'ENGINE', engine), patch.object(package.subprocess, 'check_output', side_effect=listing):
            with self.assertRaisesRegex(FileNotFoundError, 'not tracked: nup.py'): list(package.engine_files())
        (engine / 'switchrecomp.py').unlink()
        with patch.object(package, 'ENGINE', engine):
            with self.assertRaisesRegex(FileNotFoundError, 'submodule'): list(package.engine_files())

    def nxrs(self, root, gnu=True, msvc=True):
        """The nxrs engine as stage_nxrs leaves it in a release."""
        names = ['nxrs-build.exe'] + (list(package.NXRS_MSVC) if msvc else []) + (list(package.NXRS_GNU) if gnu else [])
        for name in names:
            path = root / package.NXRS_ENGINE / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'fixture')
        (root / package.NXRS_ENGINE / 'engine.json').write_text(json.dumps({'contract': 2, 'engine': 'nxrs', 'version': 'abc'}))
        if gnu:
            clang = root / package.LLVM_MINGW / 'bin/clang.exe'; clang.parent.mkdir(parents=True, exist_ok=True); clang.write_bytes(b'clang')
        for name in ('LICENSE', 'llvm-mingw.txt'):
            path = root / LEGAL / 'licenses/nxrs-engine' / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text('fixture')

    def test_nxrs_engine_is_optional_and_complete_when_shipped(self):
        root = self.bundle('windows'); package.validate_contents(root, 'windows')            # a release without it (CI)
        self.nxrs(root); package.validate_contents(root, 'windows')
        for name in ['nxrs-engine/nxrs-build.exe', 'nxrs-engine/engine.json', 'nxrs-engine/gnu/libnxrt.a', 'nxrs-engine/nxfront.lib',
                     'toolchain/llvm-mingw/bin/clang.exe', 'legal/licenses/nxrs-engine/llvm-mingw.txt']:
            path = root / name; data = path.read_bytes(); path.unlink()
            with self.assertRaises(FileNotFoundError, msg=name): package.validate_contents(root, 'windows')
            path.write_bytes(data)
        for name in ('nxrs-engine/nxfront.lib', 'nxrs-engine/nxrt.lib'): (root / name).unlink()   # gnullvm only: fine
        package.validate_contents(root, 'windows')
        for name in ('nxrs-engine/switchrecomp.py', 'nxrs-engine/target/release/nxrs-build.pdb', 'nxrs-engine/gnu/libnxrt.rlib'):
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'clutter')
            with self.assertRaisesRegex(RuntimeError, 'nxrs engine file outside', msg=name): package.validate_contents(root, 'windows')
            path.unlink()
        (root / 'nxrs-engine/engine.json').write_text(json.dumps({'contract': 1, 'engine': 'nxrs'}))
        with self.assertRaisesRegex(RuntimeError, 'contract 2'): package.validate_contents(root, 'windows')
        linux = self.bundle('linux'); self.nxrs(linux)
        with self.assertRaisesRegex(RuntimeError, 'Unexpected top-level'): package.validate_contents(linux, 'linux')

    def test_nxrs_engine_staged_from_its_checkout(self):
        engine = self.root / 'nxrs engine'; target = engine / 'target/release'; gnu = engine / 'target/gnullvm'; llvm = engine / 'build/tools/llvm-mingw'
        for name in ['nxrs-build.exe', 'nxfront.lib', 'nxrt.lib', 'nxrs-build.pdb', 'nxrt-dev.exe']: (target / name).parent.mkdir(parents=True, exist_ok=True); (target / name).write_bytes(b'x')
        for name in ['libnxfront.a', 'libnxrt.a', 'libnxrt.rlib']: gnu.mkdir(parents=True, exist_ok=True); (gnu / name).write_bytes(b'x')
        for name in ['bin/clang.exe', 'bin/ld.lld.exe', 'bin/lldb.exe', 'bin/clangd.exe', 'x86_64-w64-mingw32/lib/libc++.a',
                     'i686-w64-mingw32/lib/libc++.a', 'aarch64-w64-mingw32/lib/x.a', 'python/bin/python.exe', 'include/stdio.h',
                     'lib/clang/21/lib/windows/libclang_rt.builtins-x86_64.a']:
            path = llvm / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'x')
        for name in ['LICENSES/llvm-mingw.txt', 'LICENSES/ryujinx.txt', 'build/tools/gnu-libs/libwindows.0.52.0.a']:
            path = engine / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'x')
        stage = self.root / 'stage'; answer = type('Answer', (), {'stdout': 'nxrs-contract 2\r\n'})()
        with patch.object(package.subprocess, 'run', return_value=answer), \
             patch.object(package, 'nxrs_version', return_value={'version': 'abc123', 'commit': 'abc123', 'local_changes': False}):
            package.stage_nxrs(stage, engine, target, gnu, llvm)
        shipped = {p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}
        self.assertEqual({n for n in shipped if n.startswith('nxrs-engine/')}, {'nxrs-engine/' + n for n in package.NXRS_FILES})
        self.assertEqual(json.loads((stage / 'nxrs-engine/engine.json').read_text()), {'contract': 2, 'engine': 'nxrs', 'version': 'abc123'})
        self.assertEqual({n for n in shipped if n.startswith('toolchain/')}, {'toolchain/llvm-mingw/' + n for n in (
            'bin/clang.exe', 'bin/ld.lld.exe', 'x86_64-w64-mingw32/lib/libc++.a', 'include/stdio.h', 'lib/clang/21/lib/windows/libclang_rt.builtins-x86_64.a')})
        self.assertTrue({'legal/licenses/nxrs-engine/llvm-mingw.txt', 'legal/licenses/nxrs-engine/ryujinx.txt', 'legal/licenses/nxrs-engine/LICENSE'} <= shipped)
        with patch.object(package.subprocess, 'run', return_value=type('Answer', (), {'stdout': 'usage'})()):
            with self.assertRaisesRegex(RuntimeError, 'contract'): package.stage_nxrs(self.root / 'stage2', engine, target, None, None)

    def test_launcher_folder_allowlist(self):
        deploy = self.root / 'deploy'
        for name in ('nxrs.exe', 'Qt6Core.dll', 'platforms/qwindows.dll', 'imageformats/qwebp.dll',
                     'vc_redist.x64.exe', 'SwitchRecompLauncher.exe', 'launcher.pdb', 'library.json'):
            p = deploy / name; p.parent.mkdir(parents=True, exist_ok=True); p.touch()
        self.assertEqual({p.as_posix() for p in package.launcher_files(deploy, 'windows')},
                         {'nxrs.exe', 'Qt6Core.dll', 'platforms/qwindows.dll', 'imageformats/qwebp.dll'})
        (deploy / 'nxrs').touch()
        self.assertEqual([p.as_posix() for p in package.launcher_files(deploy, 'linux')], ['nxrs'])
        (deploy / 'nxrs.exe').unlink()
        with self.assertRaises(FileNotFoundError): list(package.launcher_files(deploy, 'windows'))

    def test_cli_finds_engine_in_release_and_checkout(self):
        cli = module('recomp_cli', 'src/recomp_cli.py')
        self.assertEqual(Path(cli.engine_dir()), ROOT / 'third_party' / 'Switch2RECOMP')   # checkout: src/ -> third_party
        release = self.root / 'release' / 'engine'; release.mkdir(parents=True)
        for name in ('recomp_cli.py', 'switchrecomp.py'): (release / name).write_text((ROOT / 'src/recomp_cli.py').read_text())
        shipped = module('shipped_cli', release / 'recomp_cli.py')
        self.assertEqual(Path(shipped.engine_dir()), release)                                # release: beside itself

    def test_runtime_build_links_native_archive_and_vp9_on_both_platforms(self):
        import switchrecomp as compiler
        for win, backend, subdir, nvn in ((True, 'win32', 'win', 'nvn'), (False, 'x11', 'lin', 'nvn_lin')):
            rt = self.root / 'runtime'; vp9 = rt / 'vp9' / subdir / 'libvpx.a'
            vp9.parent.mkdir(parents=True, exist_ok=True); vp9.touch()
            av1 = rt / 'av1' / subdir / 'libdav1d.a'; av1.parent.mkdir(parents=True, exist_ok=True); av1.touch()
            native = self.root / 'build' / nvn / 'libnvn.a'; native.parent.mkdir(parents=True); native.touch()
            with patch.object(compiler, 'HERE', str(self.root)), patch.object(compiler, 'RT', str(rt)), \
                 patch.object(compiler, 'run_cc', return_value=None) as cc, patch.dict(os.environ, {}, clear=True):
                compiler.build_runtime('gcc', 'unused', win, backend, print)
            command = cc.call_args.args[0]
            for flag in ('-DSWR_HAVE_NVN', '-DSWR_HAVE_VP9', '-DSWR_HAVE_AV1', str(native), str(vp9), str(av1)):
                self.assertIn(flag, command)
            self.assertIn(str(rt / 'audio.c'), command)
            self.assertIn(str(rt / 'svc_hid.c'), command)

    def test_vp9_archives_verified_not_built_and_sources_recorded(self):
        dest = self.root / 'deps'; dest.mkdir()
        def fetch(url, sha, cache):
            path = Path(cache); path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'source fixture')
        archives = {}
        for platform, folder in (('windows', 'win'), ('linux', 'lin')):
            lib = self.root / 'runtime/vp9' / folder / 'libvpx.a'; lib.parent.mkdir(parents=True)
            lib.write_bytes(platform.encode()); archives[platform] = ('runtime/vp9/' + folder, package.digest(lib))
        with patch.object(dependencies, 'ENGINE', self.root), patch.object(dependencies, 'VPX_ARCHIVES', archives), \
             patch.object(dependencies.deps, 'fetch', side_effect=fetch), \
             patch.object(dependencies.subprocess, 'run', side_effect=AssertionError('VP9 must not be built')):
            for platform, (folder, sha) in archives.items():
                record = dependencies.prepare_vp9(dest, platform)
                self.assertEqual(record['sha256'], sha)
                self.assertEqual(record['build'], folder + '/README.md')
                self.assertEqual(set(record['sources']), set(dependencies.VPX_SOURCES))
                self.assertTrue(all(r['url'].startswith('https://archive.ubuntu.com/') for r in record['sources'].values()))
                lib = self.root / folder / 'libvpx.a'; data = lib.read_bytes()
                lib.write_bytes(b'rebuilt locally')
                with self.assertRaisesRegex(RuntimeError, 'documented source build'): dependencies.prepare_vp9(dest, platform)
                lib.unlink()
                with self.assertRaisesRegex(RuntimeError, 'documented source build'): dependencies.prepare_vp9(dest, platform)
                lib.write_bytes(data)
        for name in dependencies.VPX_SOURCES: self.assertTrue((dest / 'sources' / name).is_file())

    def test_committed_vp9_archives_match_pins(self):
        for folder, sha in dependencies.VPX_ARCHIVES.values():
            self.assertEqual(package.digest(dependencies.ENGINE / folder / 'libvpx.a'), sha, folder)

    def test_av1_archives_verified_not_built_and_source_recorded(self):
        dest = self.root / 'deps-av1'; dest.mkdir()
        def fetch(url, sha, cache):
            path = Path(cache); path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'source fixture')
        archives = {}
        for platform, folder in (('windows', 'win'), ('linux', 'lin')):
            lib = self.root / 'runtime/av1' / folder / 'libdav1d.a'; lib.parent.mkdir(parents=True)
            lib.write_bytes(platform.encode()); archives[platform] = ('runtime/av1/' + folder, package.digest(lib))
        with patch.object(dependencies, 'ENGINE', self.root), patch.object(dependencies, 'DAV1D_ARCHIVES', archives), \
             patch.object(dependencies.deps, 'fetch', side_effect=fetch), \
             patch.object(dependencies.subprocess, 'run', side_effect=AssertionError('dav1d must not be built')):
            for platform, (folder, sha) in archives.items():
                record = dependencies.prepare_av1(dest, platform)
                self.assertEqual(record['sha256'], sha)
                self.assertEqual(record['build'], 'runtime/av1/README.md')
                self.assertEqual(set(record['sources']), {package.DAV1D_SOURCE})
                self.assertTrue(all(r['url'].startswith('https://downloads.videolan.org/') for r in record['sources'].values()))
                lib = self.root / folder / 'libdav1d.a'; data = lib.read_bytes()
                lib.write_bytes(b'rebuilt locally')
                with self.assertRaisesRegex(RuntimeError, 'documented source build'): dependencies.prepare_av1(dest, platform)
                lib.unlink()
                with self.assertRaisesRegex(RuntimeError, 'documented source build'): dependencies.prepare_av1(dest, platform)
                lib.write_bytes(data)
        self.assertTrue((dest / 'sources' / package.DAV1D_SOURCE).is_file())

    def test_committed_av1_archives_match_pins_and_readme(self):
        readme = (dependencies.ENGINE / 'runtime/av1/README.md').read_text(encoding='utf-8')
        self.assertIn(dependencies.DAV1D_SOURCE[1], readme)
        self.assertEqual(dependencies.DAV1D_SOURCE[0].rsplit('/', 1)[1], package.DAV1D_SOURCE)
        for folder, sha in dependencies.DAV1D_ARCHIVES.values():
            self.assertEqual(package.digest(dependencies.ENGINE / folder / 'libdav1d.a'), sha, folder)
            self.assertIn(sha, readme)

if __name__ == '__main__': unittest.main()
