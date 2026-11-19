# Portable releases

A release bundles the Qt launcher, the Switch2RECOMP engine (recompiler, native NVN Vulkan runtime, VP9
decoding and shader translator), Python 3.14, GCC/G++ 16.2, .NET 10.0.12 and SPIRV-Tools.
Windows includes app-local MSVC runtime DLLs. Linux includes Qt, its XCB/WebP plugins,
SDL2, and the shared libraries discovered during packaging.

## Bundle layout

A release is laid out for the person running it, not as a copy of this repository:

```text
nxrs-<platform>-x64/
  nxrs.exe | nxrs   the launcher: double-click it
    Windows: + Qt and app-local MSVC DLLs, platforms/ imageformats/ styles/ ... (windeployqt output)
    Linux:   + lib/ (Qt, SDL2, ICU/OpenSSL and other non-glibc libraries), plugins/, qt.conf
  engine/       only what a game build reads:
                  switchrecomp.py nxloader.py codegen.py a64.py nup.py nsp.py dlc.py   the recompiler
                  recomp_cli.py                                                 the build entry point the launcher runs
                  bin/runtime_win64_win32.exe | bin/runtime_linux_x11           the prebuilt native runtime
                  runtime/cpu.h runtime/cpu_helpers.c                           included by the generated C
                  runtime/nvn/gfx/dmsh_abi.h tools/nxshader/ (+ xlate_bin/)     staged into every game for Vulkan
                  tools/hactool/ (Windows)                                      NSP header inspection
  toolchain/    python/ gcc/ dotnet/ shader-tools/
  legal/        LICENSE.txt NOTICE.txt THIRD_PARTY.md (engine), licenses/ (third party, incl. libvpx),
                sources/ (Qt and libvpx source archives), dependency-manifest.json, build-info.json, SHA256SUMS
```

The launcher looks for `engine/` and `toolchain/` beside its own executable first, then for a source checkout
(`third_party/Switch2RECOMP` with `.venv`, `third_party/gcc` and `bin/launcher` at the project root).
`legal/SHA256SUMS` lists every other file relative to the release root: `sha256sum -c legal/SHA256SUMS` from there.

A release carries no source beyond what a game build reads: no runtime C sources (the runtime ships prebuilt),
engine tests, docs or developer tools, no launcher sources or scripts, and no `vc_redist.x64.exe` (the MSVC DLLs
are app-local). The complete source is the `nxrs-source.tar.gz` published with every release.
Packaging enforces this: `engine/` holds only allowlisted files tracked at the pinned engine commit, and the
release root holds only the launcher, its Qt runtime and the three folders above.

On Linux the launcher binary carries `DT_RPATH=$ORIGIN/lib`, so it starts without a wrapper script or
`LD_LIBRARY_PATH`; the Qt plugins find `lib/` through Qt's own `$ORIGIN/../../lib`. The launcher then puts `lib/`,
`toolchain/gcc/lib` and `toolchain/shader-tools/lib` on the library path of the builds and games it starts.

## Run a release

- Windows x64: extract the entire ZIP into a writable folder, then open `nxrs.exe`.
- Linux x64: extract the tar.gz, then run `./nxrs` (or open it from the file manager).
  The release baseline is Ubuntu 24.04 (glibc 2.39) or a compatible newer desktop.
  An X11 session or XWayland is required. Extract onto a filesystem that supports
  Unix permissions and symbolic links.

No Python, compiler, Qt, .NET, or Vulkan SDK installation is needed to build games.
The host still supplies its GPU driver, display server, fonts, desktop audio services,
and OS libraries such as glibc. The NVN texture decoder requires an x64 CPU with
AVX2, FMA, F16C, and POPCNT. A compatible Vulkan GPU/driver is needed for Vulkan games.

Keep the release folder together. A launcher started from it uses that folder's engine even if
another one was used previously (unless one was chosen explicitly in File > Settings > Engine). Games launched from the library inherit the
current bundled Python, .NET, shader tools, and Linux library paths. Launch through
the library after moving the toolkit; standalone game executables can retain an old
Python marker until rebuilt. Moving the toolkit does not move the original NUPs.

Development checkouts prefer `SWR_PYTHON`, then `.venv`, then `venv`, before the bundled interpreter.
Release archives exclude development venvs, so a normal release uses its bundled Python. A valid explicit
`SWR_PYTHON` overrides either layout. No launcher/build step creates a venv or downloads Python.

The launcher saves the personal library in the OS application-data directory. Games,
keys, firmware, saves, artwork, logs, captures, and shader caches are not release inputs.

## Automated builds

`.github/workflows/release.yml` runs on pushes and pull requests to `main`, and can
also be started manually for one or both platforms. It checks out the engine submodule at the pinned commit.
Windows and Linux build independently. Each job runs the engine's Python tests, the launcher's packaging and
interpreter-selection tests, and the Qt tests, packages its bundle, then extracts it to a path containing
spaces and brackets, outside the checkout. With a minimal tool search path, the smoke test:

1. Verifies the archive checksum and complete per-file SHA-256 manifest, including unlisted-file detection.
   Checks the release layout and engine allowlist, required runtime headers, shader helpers, Qt plugins, licenses,
   source archives, and provenance (including that the checkout's libvpx archive the runtime links is the pinned one).
   Rejects personal data, development venvs, and broken, absolute, or escaping symlinks.
2. On Linux, checks that the launcher and its xcb plugin resolve every library, Qt from the bundle, with no
   `LD_LIBRARY_PATH`.
3. Starts bundled Python/zstd, .NET, SPIRV tools and (Windows) hactool.
4. Compiles, links and runs a C++ program with bundled GCC, whose sysroot must resolve inside the bundle.
5. Builds a synthetic NUP through the release launcher started with no `--root` and only the system `PATH`, so it
   must discover `engine/` and `toolchain/` itself; the build must run on the bundled Python, and every
   `#include` of the generated C must resolve inside `engine/runtime`.
6. Confirms the copied runtime matches the packaged binary, checks the Vulkan setting and NUP pointer,
   imports that build twice without duplicating the library entry, then imports the staged shader modules
   and starts the shader helper. The synthetic game is never launched.

Packaging requires linked VP9, AV1 (dav1d) and NVN telemetry markers in both native runtimes and Sony HID loader markers
on Windows. The clean workflow builds the OpenGL/NVN desktop runtime and its audio mixer, and links the committed VP9 decoder.
Optional Eden, OpenH264, Opus, and SPIRV-Cross/LSFG static libraries are not built by this workflow.
Packaging replaces file symlinks with copies of their targets but keeps directory symlinks that stay inside
the bundle as relative links: GCC finds `Scrt1.o` through its sysroot's `usr/lib -> lib64`. The bundled GCC
needs no fixup after extraction, because it derives its sysroot from its own location. Packaging only removes the
`-rpath` to the packaging machine that conda's specs add to every link.

Both jobs must pass before publishing. Releases are versioned from the `VERSION` file at the repository root
(`MAJOR.MINOR.PATCH`, optionally `-suffix` for a prerelease); the launcher's About/`--version` reads the same file at
build time. When a push to `main` (or a manual all-platform run on `main`) carries a `VERSION` that has no `v<VERSION>`
tag yet, the release job publishes release `v<VERSION>`, targeting the exact tested commit. Pushes that don't change
`VERSION` still build and test both platforms but publish nothing. To release: bump `VERSION` in a commit and push.
PRs and manual runs on other branches only upload workflow artifacts. The `launcher-<run>-<attempt>` releases predate
versioning. Each release has a Windows ZIP,
Linux tar.gz, archive checksums, and a source archive of the launcher with the pinned engine included,
each with its own checksum. `build-info.json` records the version, the launcher commit, the engine commit and its
source link, and whether a local packaging run had tracked or untracked changes in either repository.
The archive checks do not establish game compatibility or GPU rendering performance.

## Build locally

Clone with `--recurse-submodules` (or run `git submodule update --init`). Release dependency preparation
needs Git and Python 3.14+ (including zstd support). Launcher builds need Qt 6.8.3 with `qtimageformats`
and `qtsvg`, and a native C/C++ build environment. Windows needs VS 2022 Build Tools; Linux needs CMake,
Ninja, GCC/G++, X11/OpenGL development headers, and `glslang-tools`. Windows runtime builds also need
glslang on PATH; the workflow builds glslang 15.1.0 from source with VS 2022 x64.

```text
python scripts/release-deps.py --dest build/release-deps
```

This downloads checksum-pinned portable dependencies into a separate directory.
Both bundles retain the libvpx 1.14.0 Ubuntu `1ubuntu2.3` source package and patches under `legal/sources/`,
and dav1d 1.5.5's unmodified release archive (`dav1d-1.5.5.tar.xz`), with its license in `legal/licenses/dav1d/`.
The engine commits both AV1-decoder archives the same way, `runtime/av1/win/libdav1d.a` and
`runtime/av1/lin/libdav1d.a`, built by `runtime/av1/build_dav1d.py` as `runtime/av1/README.md` documents; each is
verified against its pinned SHA-256 (`DAV1D_ARCHIVES` in `scripts/release-deps.py`).
The engine commits both VP9-decoder archives, `runtime/vp9/win/libvpx.a` and `runtime/vp9/lin/libvpx.a`,
built from that source as documented in the README beside each one; nothing is compiled here. Each is verified
against its pinned SHA-256, which the dependency manifest records with the source hashes and the README path.
If an archive extraction was interrupted, remove only that incomplete dependency
subdirectory and rerun the command.

Windows, from PowerShell (install CMake 3.31.10 and Ninja 1.13.2 in the build interpreter first):

```powershell
$env:PATH = "$pwd\build\release-deps\gcc\Library\bin;$env:PATH"
python third_party/Switch2RECOMP/tools/build_nvn.py --jobs 8
python src/recomp_cli.py --build-runtime --target win64 --backend win32 --cc "$pwd\build\release-deps\gcc\Library\bin\x86_64-w64-mingw32-gcc.exe"
./scripts/build-launcher.ps1 -QtPrefix C:/Qt/6.8.3/msvc2022_64
python scripts/package-release.py --deps build/release-deps --launcher bin/launcher --qt C:/Qt/6.8.3/msvc2022_64
python scripts/smoke-release.py dist/nxrs-windows-x64.zip
```

Linux:

```sh
export SWR_QT_PREFIX=/path/to/Qt/6.8.3/gcc_64
python3 third_party/Switch2RECOMP/tools/build_nvn.py --linux --jobs 8
python3 src/recomp_cli.py --build-runtime --target linux --backend x11 --cc gcc
sh scripts/build-launcher.sh
python3 scripts/package-release.py --deps build/release-deps --launcher bin/launcher-linux --qt "$SWR_QT_PREFIX"
python3 scripts/smoke-release.py dist/nxrs-linux-x64.tar.gz
```

Packaging refuses to merge into an existing staging directory. Use another `--output`
directory for a new local run. `--work` selects a separate staging filesystem, useful
when working from a Windows-mounted drive under WSL. `--stage-only` skips compression.
Engine files come from an explicit allowlist (`ENGINE_FILES` in `scripts/package-release.py`) matched against
the engine submodule's tracked files; the launcher comes from the deployed launcher folder (its executable, DLLs
and Qt plugin folders only); dependencies come only from the selected release-dependency directory.
The personal working tree is never recursively archived.

## Notices and source

The engine's `LICENSE.txt`, `NOTICE.txt` (SwitchRecomp by MZ Labs) and `THIRD_PARTY.md` ship under `legal/`. The engine's startup screen is disabled in the turbo branch.
Linux Python's matching full distribution supplies `legal/licenses/python/` and `PYTHON.json` because the
stripped install-only archive omits them. The full archive download is checksum-pinned in
`dependency-manifest.json`; only its license texts and build metadata are added to the release.
See [THIRD_PARTY.md](../third_party/Switch2RECOMP/docs/THIRD_PARTY.md) for bundled components and source
locations. Update the download pins and source references together when upgrading.
