# Portable releases

A release bundles the Qt launcher, the Switch2RECOMP engine (recompiler, native NVN Vulkan runtime, VP9
decoding and shader translator), Python 3.14, GCC/G++ 16.2, .NET 10.0.12 and SPIRV-Tools.
Windows includes app-local MSVC runtime DLLs. Linux includes Qt, its XCB/WebP plugins,
SDL2, and the shared libraries discovered during packaging.

## Bundle layout

The bundle mirrors this repository, so the launcher finds everything the same way it does in a checkout:

```text
Switch2RECOMP-turbo-<platform>-x64/
  Switch2RECOMP-turbo.cmd | .sh   starts bin/launcher with --root third_party/Switch2RECOMP
  bin/launcher/                   Qt launcher and its Qt/MSVC runtime files
  src/recomp_cli.py               build entry point the launcher runs
  scripts/ docs/ tests/           launcher helpers and documentation
  python/ gcc/ dotnet/            bundled tools (the launcher's equivalent of a checkout's .venv)
  shader-tools/ lib/ (Linux)
  licenses/ sources/              third-party notices and corresponding source archives
  third_party/Switch2RECOMP/      the engine: recompiler, runtime sources, tools,
                                  bin/runtime_*, runtime/vp9/*/libvpx.a, tools/hactool (Windows)
  dependency-manifest.json build-info.json SHA256SUMS
```

## Run a release

- Windows x64: extract the entire ZIP into a writable folder, then open
  `Switch2RECOMP-turbo.cmd`.
- Linux x64: extract the tar.gz, then run `./Switch2RECOMP-turbo.sh`.
  The release baseline is Ubuntu 24.04 (glibc 2.39) or a compatible newer desktop.
  An X11 session or XWayland is required. Extract onto a filesystem that supports
  Unix permissions and symbolic links.

No Python, compiler, Qt, .NET, or Vulkan SDK installation is needed to build games.
The host still supplies its GPU driver, display server, fonts, desktop audio services,
and OS libraries such as glibc. The NVN texture decoder requires an x64 CPU with
AVX2, FMA, F16C, and POPCNT. A compatible Vulkan GPU/driver is needed for Vulkan games.

Keep the toolkit directory together. Its startup script selects that copy even if
another toolkit was used previously. Games launched from the library inherit the
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
   Checks required runtime headers, shader helpers, Qt plugins, licenses, source archives, and provenance.
   Rejects personal data, development venvs, and broken, absolute, or escaping symlinks.
2. Starts bundled Python/zstd, .NET, and SPIRV tools.
3. Compiles and runs a C++ program with bundled GCC.
4. Builds a synthetic NUP through the deployed Qt launcher.
5. Confirms the copied runtime matches the packaged binary, checks the Vulkan setting and NUP pointer,
   imports that build twice without duplicating the library entry, then imports the staged shader modules
   and starts the shader helper. The synthetic game is never launched.

Packaging requires linked VP9 and NVN telemetry markers in both native runtimes and Sony HID loader markers
on Windows. The clean workflow builds the OpenGL/NVN desktop runtime, its audio mixer, and VP9 decoder.
Optional Eden, OpenH264, Opus, and SPIRV-Cross/LSFG static libraries are not built by this workflow.

Both jobs must pass before publishing. Pushes to `main` and manual all-platform runs on `main`
create a release tagged `turbo-<run number>-<attempt>`, targeting the exact tested commit.
PRs and manual runs on other branches only upload workflow artifacts. Each release has a Windows ZIP,
Linux tar.gz, archive checksums, and a source archive of the launcher with the pinned engine included,
each with its own checksum. `build-info.json` records the launcher commit, the engine commit and its
source link, and whether a local packaging run had tracked or untracked changes in either repository.
The archive checks do not establish game compatibility or GPU rendering performance.

## Build locally

Clone with `--recurse-submodules` (or run `git submodule update --init`). Release dependency preparation
needs Git and Python 3.14+ (including zstd support). Launcher builds need Qt 6.8.3 with `qtimageformats`
and `qtsvg`, and a native C/C++ build environment. Windows needs VS 2022 Build Tools; Linux needs CMake,
Ninja, GCC/G++, X11/OpenGL development headers, and `glslang-tools`. Windows runtime builds also need
glslang on PATH; the workflow builds glslang 15.1.0 from source with VS 2022 x64.
Linux dependency preparation also requires `dpkg-dev`, Make, GCC/G++, and NASM for the pinned VP9 build.

```text
python scripts/release-deps.py --dest build/release-deps
```

This downloads checksum-pinned portable dependencies into a separate directory.
Both bundles retain the libvpx 1.14.0 Ubuntu `1ubuntu2.3` source package and patches under `sources/`.
Windows verifies the engine's checked-in `runtime/vp9/win/libvpx.a` against its documented hash. Linux
applies the complete Ubuntu patch series with `dpkg-source`, then builds a static PIC, multithreaded VP9-only
decoder into the engine's `runtime/vp9/lin/libvpx.a`, recording configure options, compiler version and
binary hash. An existing different Linux archive is preserved and must be moved aside explicitly.
If an archive extraction was interrupted, remove only that incomplete dependency
subdirectory and rerun the command.

Windows, from PowerShell (install CMake 3.31.10 and Ninja 1.13.2 in the build interpreter first):

```powershell
$env:PATH = "$pwd\build\release-deps\gcc\Library\bin;$env:PATH"
python third_party/Switch2RECOMP/tools/build_nvn.py --jobs 8
python src/recomp_cli.py --build-runtime --target win64 --backend win32 --cc "$pwd\build\release-deps\gcc\Library\bin\x86_64-w64-mingw32-gcc.exe"
./scripts/build-launcher.ps1 -QtPrefix C:/Qt/6.8.3/msvc2022_64
python scripts/package-release.py --deps build/release-deps --launcher bin/launcher --qt C:/Qt/6.8.3/msvc2022_64
python scripts/smoke-release.py dist/Switch2RECOMP-turbo-windows-x64.zip
```

Linux:

```sh
export SWR_QT_PREFIX=/path/to/Qt/6.8.3/gcc_64
python3 third_party/Switch2RECOMP/tools/build_nvn.py --linux --jobs 8
python3 src/recomp_cli.py --build-runtime --target linux --backend x11 --cc gcc
sh scripts/build-launcher.sh
python3 scripts/package-release.py --deps build/release-deps --launcher bin/launcher-linux --qt "$SWR_QT_PREFIX"
python3 scripts/smoke-release.py dist/Switch2RECOMP-turbo-linux-x64.tar.gz
```

Packaging refuses to merge into an existing staging directory. Use another `--output`
directory for a new local run. `--work` selects a separate staging filesystem, useful
when working from a Windows-mounted drive under WSL. `--stage-only` skips compression.
Source files come from explicit directories and Git's tracked/unignored file lists of the
launcher checkout and of the engine submodule (listed separately, since `git ls-files` does not
descend into submodules); dependencies come only from the selected release-dependency directory.
The personal working tree is never recursively archived.

## Notices and source

The engine's `LICENSE.txt` and `NOTICE.txt` (SwitchRecomp by MZ Labs) ship under
`third_party/Switch2RECOMP/`. The engine's startup screen is disabled in the turbo branch.
Linux Python's matching full distribution supplies `licenses/python/` and `PYTHON.json` because the
stripped install-only archive omits them. The full archive download is checksum-pinned in
`dependency-manifest.json`; only its license texts and build metadata are added to the release.
See [THIRD_PARTY.md](../third_party/Switch2RECOMP/docs/THIRD_PARTY.md) for bundled components and source
locations. Update the download pins and source references together when upgrading.
