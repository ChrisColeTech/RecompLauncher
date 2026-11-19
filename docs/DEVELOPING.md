# Developing nxrs

How to build the launcher from source, work on the engine submodule, and run the tests. For using the app, see
the [README](../README.md); for packaging release bundles, see [RELEASES.md](RELEASES.md).

## Repository layout

```
src/launcher/                   Qt 6 C++ launcher (this app)
src/recomp_cli.py               build CLI entry point; drives the engine
third_party/Switch2RECOMP/      engine submodule (turbo branch of the Switch2RECOMP-turbo fork)
third_party/gcc/                portable GCC 16.2 toolchain (not committed; see setup)
scripts/                        setup, build, run and release helpers
docs/                           guides (this file, launcher, build environment, releases, engine notes)
tests/                          launcher Python tests (workflow, release scripts, Python selection)
bin/launcher/                   deployed launcher (build output)
```

Launcher Python tests live in `tests/`; engine tests stay in `third_party/Switch2RECOMP/tests/`. The C++
test suites build with the launcher.

## The engine submodule

`third_party/Switch2RECOMP` is a git submodule pinned to a commit on the `turbo` branch of
[Switch2RECOMP-turbo](https://github.com/ChrisColeTech/Switch2RECOMP-turbo): upstream SwitchRecomp plus
the fork's engine enhancements. Clone with submodules:

```bat
git clone --recurse-submodules https://github.com/ChrisColeTech/nxrs.git
rem or, in an existing clone:
git submodule update --init
```

Engine changes are made and pushed in the fork, never only here. To pull upstream fixes and bump the pin:

```bat
rem in the fork checkout
git fetch upstream
git checkout main  && git merge --ff-only upstream/main && git push origin main
git checkout turbo && git merge main && git push origin turbo
rem here
git submodule update --remote third_party/Switch2RECOMP
rem rebuild + test, then commit the new pin
```

Merge (do not rebase) `turbo`: the pin is a commit id, and a rebase would orphan pinned commits.

## Building

Windows requirements: Visual Studio 2022 Build Tools with the MSVC x64 C++ toolset, Qt 6.8 MSVC
x64 (Core, Gui, Widgets, Test, plus the Qt Image Formats module), and Python 3.8+. CMake 3.31.10
and Ninja 1.13.2 are pinned into the project venv by setup. Full details:
[BUILD_ENVIRONMENT_GUIDE.md](BUILD_ENVIRONMENT_GUIDE.md).

```bat
scripts\setup.bat                 rem one-time: venv + pinned build tools + GCC check
scripts\build-runtime.bat         rem builds third_party\Switch2RECOMP\bin\runtime_win64_win32.exe
scripts\build-launcher.bat        rem builds, tests, and deploys Qt to bin\launcher
scripts\run-launcher.bat          rem start the launcher
```

Pass `-QtPrefix C:\Qt\6.8.3\msvc2022_64` to `build-launcher.bat` for a non-default Qt. Both
launcher CTest suites run automatically; full output lands in `logs\launcher_release.log`.

The app icon is generated: edit `scripts/make-icon.py` and run it (needs Pillow) to rewrite
`src/launcher/icons/app.ico` and `app.png`.

## Recompiling a game from the command line

```bat
scripts\build-from-nup.bat "D:\Games\Game [title][v0].nup" --outdir "D:\Games\Game-build" -o Game
"D:\Games\Game-build\Game.exe"
```

The output directory receives `Game.exe`, `Game_code.dll`, `Game.ini`, generated C in `Game_build/`,
and `Game_data/` with the ExeFS plus a `romfs.nup` pointer — the original package remains the only
RomFS copy and is read in place at runtime. NSP inputs need `prod.keys` installed first; use
`scripts\build-from-nsp.bat`.

## Testing

```bat
.venv\Scripts\python.exe third_party\Switch2RECOMP\tests\test_nup.py -v
.venv\Scripts\python.exe third_party\Switch2RECOMP\tests\test_nsp.py -v
.venv\Scripts\python.exe tests\test_python_selection.py -v
.venv\Scripts\python.exe tests\test_launcher_workflow.py -v
.venv\Scripts\python.exe tests\test_release.py -v
```

`test_launcher_workflow` exercises the deployed launcher end to end: it imports an existing native
build without compiling, and drives a real build of a synthetic, game-data-free NUP through the
actual `BuildManager`/`QProcess` path, asserting the ready-to-play library entry persists.
`test_release` covers the release packaging contracts.

## Releases

Every push to `main` runs `.github/workflows/release.yml`: both platforms build, package, and pass a relocated
smoke test. To publish a release, bump the version in `VERSION` (semantic versioning) and push: the workflow
publishes `v<VERSION>` once, with the commit changelog since the previous release as its notes.
Building and smoke-testing a bundle locally (including the WSL mirror of the Linux job) is in
[RELEASES.md](RELEASES.md).
