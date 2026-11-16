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

ROOT = Path(__file__).resolve().parents[1]                    # RecompLauncher checkout
sys.path.insert(0, str(ROOT / 'third_party' / 'Switch2RECOMP'))  # engine submodule (switchrecomp)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


package = module('package_release', 'scripts/package-release.py')
smoke = module('smoke_release', 'scripts/smoke-release.py')
dependencies = module('release_dependencies', 'scripts/release-deps.py')
ENGINE = package.ENGINE_DIR   # the engine's place inside a bundle


class ReleaseChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='release fixtures [spaces] ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def bundle(self, platform):
        root = self.root / platform; root.mkdir()
        for name in package.required_files(platform):
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'fixture')
        for name in ('licenses/qt/qtbase', 'licenses/qt/qtimageformats', 'licenses/qt/qtsvg', 'gcc/licenses', 'shader-tools/licenses'):
            path = root / name; path.mkdir(parents=True, exist_ok=True); (path / 'LICENSE').write_text('fixture')
        runtime = root / ENGINE / 'bin' / package.runtime_name(platform)
        runtime.write_bytes(b'WebM Project VP9 Decoder\0frame telemetry:\0hid.dll\0setupapi.dll')
        vpx = root / ENGINE / package.vp9_path(platform)
        manifest = {'platform': platform, 'downloads': {'libvpx': {'sha256': package.digest(vpx), 'sources': {
            p.name: {'sha256': package.digest(p)} for p in (root / 'sources').glob('libvpx*')}}}}
        (root / 'dependency-manifest.json').write_text(json.dumps(manifest))
        return root

    def sums(self, root):
        (root / 'SHA256SUMS').write_text(''.join(package.digest(p) + '  ' + p.relative_to(root).as_posix() + '\n'
            for p in sorted(root.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS'))

    def test_complete_bundles_and_missing_new_files(self):
        for platform in ('windows', 'linux'):
            root = self.bundle(platform)
            package.validate_contents(root, platform)
            for name in [(ENGINE / n).as_posix() for n in ('runtime/controller_sony_win32.h', 'runtime/nvn/vk/vk_perf.c',
                         'runtime/nvn/vk/vk_perf.h', 'tools/nxshader/s2r_paths.py',
                         'tools/nxshader/xlate_bin/dmsh_xlate.runtimeconfig.json', 'NOTICE.txt', 'bin/' + package.runtime_name(platform))] + [
                         'src/recomp_cli.py', 'docs/RELEASES.md', 'sources/libvpx_1.14.0-1ubuntu2.3.debian.tar.xz']:
                path = root / name; data = path.read_bytes(); path.unlink()
                with self.assertRaises(FileNotFoundError, msg=name): package.validate_contents(root, platform)
                path.write_bytes(data)

    def test_runtime_requires_vp9_nvn_telemetry_and_sony(self):
        root = self.bundle('windows'); runtime = root / ENGINE / 'bin/runtime_win64_win32.exe'; data = runtime.read_bytes()
        for marker in (b'WebM Project VP9 Decoder', b'frame telemetry:', b'hid.dll', b'setupapi.dll'):
            runtime.write_bytes(data.replace(marker, b''))
            with self.assertRaisesRegex(RuntimeError, 'Native runtime feature'): package.validate_contents(root, 'windows')
        runtime.write_bytes(data)
        (root / ENGINE / 'runtime/vp9/win/libvpx.a').write_bytes(b'wrong target library')
        with self.assertRaisesRegex(RuntimeError, 'provenance'): package.validate_contents(root, 'windows')

    def test_private_inputs_rejected(self):
        root = self.bundle('windows')
        for name in ('docs/library.json', 'tools/python.path', 'runtime/prod.keys', 'runtime/cache.NSP',
                     'tools/saves/private.dat', '.venv/Scripts/python.exe', 'logs/session.txt', 'shader_cache/cache.bin'):
            path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'private')
            with self.assertRaisesRegex(RuntimeError, 'Personal/generated'): package.validate_contents(root, 'windows')
            path.unlink()
            while path.parent != root and not list(path.parent.iterdir()):
                path = path.parent; path.rmdir()

    def test_manifest_rejects_extra_missing_changed_and_duplicate_files(self):
        root = self.bundle('linux'); self.sums(root); smoke.verify_manifest(root)
        manifest = root / 'SHA256SUMS'; original = manifest.read_text()
        extra = root / 'extra.txt'; extra.write_text('unlisted')
        with self.assertRaisesRegex(RuntimeError, 'exactly'): smoke.verify_manifest(root)
        extra.unlink()
        notice = root / ENGINE / 'NOTICE.txt'; notice.unlink()
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
        for target in (self.root / 'missing', self.root, root / ENGINE / 'NOTICE.txt'):
            link.symlink_to(target)
            with self.assertRaisesRegex(RuntimeError, 'Non-relocatable'): package.validate_contents(root, 'linux')
            link.unlink()

    def test_gcc_install_rpath_stripped_from_specs(self):
        root = self.bundle('linux')
        specs = root / 'gcc/lib/gcc/x86_64-conda-linux-gnu/16.2.0/specs'; specs.parent.mkdir(parents=True, exist_ok=True)
        specs.write_text('*link:\n%{T*}   %{!static:-rpath /home/runner/work/x [y]/gcc/lib}\n\n*lib:\n-lc\n')
        package.strip_gcc_rpath(root)
        self.assertEqual(specs.read_text(), '*link:\n%{T*}   \n\n*lib:\n-lc\n')
        with self.assertRaisesRegex(RuntimeError, 'specs layout'): package.strip_gcc_rpath(root)   # layout changed upstream

    @unittest.skipIf(os.name == 'nt', 'Unix symlink archive contract')
    def test_directory_symlinks_kept_relative_inside_the_stage(self):
        # the conda GCC sysroot: lib and usr/lib -> lib64; GCC searches <sysroot>/usr/lib/../lib64 for Scrt1.o
        root = self.bundle('linux'); sysroot = root / 'gcc/x86_64-conda-linux-gnu/sysroot'
        (sysroot / 'usr/lib64').mkdir(parents=True); (sysroot / 'usr/lib64/Scrt1.o').write_bytes(b'crt')
        (sysroot / 'lib64').mkdir(); (root / 'gcc/share/zoneinfo').mkdir(parents=True)
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
        self.assertEqual((moved / 'gcc/x86_64-conda-linux-gnu/sysroot/usr/lib/Scrt1.o').read_bytes(), b'crt')
        outside = self.root / 'outside-dir'; outside.mkdir()
        (moved / 'gcc/escape').symlink_to(outside)
        with self.assertRaisesRegex(RuntimeError, 'Directory symlink leaves'): package.dereference_symlinks(moved)

    @unittest.skipIf(os.name == 'nt', 'Unix symlink archive contract')
    def test_dereference_replaces_nonrelocatable_links_with_target_copies(self):
        root = self.bundle('linux')
        (root / 'gcc/lib').mkdir(parents=True, exist_ok=True)
        real = root / 'gcc/lib/libgomp.so.1.0.0'; real.write_bytes(b'gomp')
        (root / 'gcc/lib/libgomp.so').symlink_to(real)             # relative, inside the stage
        (root / 'gcc/lib/absolute.so').symlink_to(self.root / 'outside-target')
        (self.root / 'outside-target').write_bytes(b'absolute')
        (root / 'gcc/lib/gcc-ghost.so').symlink_to(Path('/never-existed/lib/gcc-ghost.so.1.0.0'))  # toolchain-build path
        (root / 'gcc/lib/gcc-ghost.so.1.0.0').write_bytes(b'ghost')
        package.dereference_symlinks(root)
        self.assertFalse((root / 'gcc/lib/libgomp.so').is_symlink())
        self.assertEqual((root / 'gcc/lib/libgomp.so').read_bytes(), b'gomp')
        self.assertFalse((root / 'gcc/lib/absolute.so').is_symlink())
        self.assertEqual((root / 'gcc/lib/absolute.so').read_bytes(), b'absolute')
        self.assertFalse((root / 'gcc/lib/gcc-ghost.so').is_symlink())
        self.assertEqual((root / 'gcc/lib/gcc-ghost.so').read_bytes(), b'ghost')
        package.validate_contents(root, 'linux')                   # the fixed tree now validates
        (root / 'gcc/lib/libopenmp.so').symlink_to(Path('/never/libopenmp.so'))  # dev link, library not shipped
        package.dereference_symlinks(root)
        self.assertFalse((root / 'gcc/lib/libopenmp.so').exists())  # dangling dev link removed
        package.validate_contents(root, 'linux')
        (root / 'gcc/lib/suspicious.so').symlink_to(self.root / 'missing-dir/NOTICE.txt')  # basename IS shipped
        with self.assertRaisesRegex(RuntimeError, 'unreadable target'): package.dereference_symlinks(root)

    def test_source_allowlist_covers_checkout_and_engine_submodule_but_not_personal_data(self):
        checkout = self.root / 'checkout'; engine = checkout / ENGINE
        launcher_names = ['src/recomp_cli.py', 'scripts/_python.bat', 'docs/RELEASES.md', 'third_party/Switch2RECOMP',
                          'bin/launcher/SwitchRecompLauncher.exe', 'library.json', 'logs/build.txt']
        engine_names = ['switchrecomp.py', 'runtime/controller_sony.h', 'runtime/nvn/vk/vk_perf.c', 'tools/nxshader/s2r_paths.py',
                        'scripts/build-from-nup.bat', 'tools/library.json', 'runtime/prod.keys', '.venv/Scripts/python.exe',
                        'games/Game.exe', 'performance.txt']
        for base, names in ((checkout, launcher_names[:3] + launcher_names[4:]), (engine, engine_names)):
            for name in names:
                p = base / name; p.parent.mkdir(parents=True, exist_ok=True); p.touch()
        def listing(command, cwd):   # git ls-files never descends into the submodule, it only lists its gitlink
            return '\0'.join(engine_names if Path(cwd) == engine else launcher_names).encode()
        with patch.object(package, 'ROOT', checkout), patch.object(package, 'ENGINE', engine), \
             patch.object(package.subprocess, 'check_output', side_effect=listing):
            self.assertEqual({p.as_posix() for p in package.source_files()},
                             set(launcher_names[:3]) | {(ENGINE / n).as_posix() for n in engine_names[:4]})
        (engine / 'switchrecomp.py').unlink()
        with patch.object(package, 'ROOT', checkout), patch.object(package, 'ENGINE', engine):
            with self.assertRaisesRegex(FileNotFoundError, 'submodule'): list(package.source_files())

    def test_runtime_build_links_native_archive_and_vp9_on_both_platforms(self):
        import switchrecomp as compiler
        for win, backend, subdir, nvn in ((True, 'win32', 'win', 'nvn'), (False, 'x11', 'lin', 'nvn_lin')):
            rt = self.root / 'runtime'; vp9 = rt / 'vp9' / subdir / 'libvpx.a'
            vp9.parent.mkdir(parents=True, exist_ok=True); vp9.touch()
            native = self.root / 'build' / nvn / 'libnvn.a'; native.parent.mkdir(parents=True); native.touch()
            with patch.object(compiler, 'HERE', str(self.root)), patch.object(compiler, 'RT', str(rt)), \
                 patch.object(compiler, 'run_cc', return_value=None) as cc, patch.dict(os.environ, {}, clear=True):
                compiler.build_runtime('gcc', 'unused', win, backend, print)
            command = cc.call_args.args[0]
            for flag in ('-DSWR_HAVE_NVN', '-DSWR_HAVE_VP9', str(native), str(vp9)):
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

if __name__ == '__main__': unittest.main()
