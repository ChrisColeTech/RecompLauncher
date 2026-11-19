# nxrs Turbo launcher

It is a static recompiler and desktop build/library utility, not an emulator product. Add your own
game dumps, build them, then launch the finished games from the same library. No per-game desktop shortcuts
are needed. The library is saved automatically and is restored the next time you open the launcher.

Start with the [README walkthrough](../README.md#add-build-play). No games, keys or firmware are included.

## Start it

For all-in-one releases, open `nxrs.exe` (Windows) or
`./nxrs` (Linux) at the top of the extracted folder. Download bundles from [the latest Turbo release](https://github.com/ChrisColeTech/nxrs/releases/latest).
The following path is for a Windows developer checkout:

```powershell
& ".\bin\launcher\nxrs.exe"
```

The entire `bin/launcher/` directory contains the Qt DLLs and plugins needed by the frontend. Do not move
only the EXE. The engine is found by walking up from the launcher's directory until a
`third_party/Switch2RECOMP/switchrecomp.py` (or a flat `switchrecomp.py`) is found, or it can be chosen under
**File → Settings → Engine**.

## Add, build, play

1. **Add game:** select a `.nup`, `.nsp` (also one that holds the game with its update and DLC), or `.nro`, or drop it into the library. The dropdown also accepts extracted
   game folders and existing native builds.
2. In the add dialog, choose the display name under **General** and build location under **Build**, then click
   **Add game**. Later use the card's sliders button or **Alt+Enter** for Properties; save edits with **OK**.
3. Click **Build** on the game's card. Live output appears in a dock that can be resized, hidden, or undocked. Progress stays
   in the status bar even when the dock is hidden.
4. After a successful build, the card's button changes to **Play**.
5. Click **Play**, double-click the cover/list row, or press Enter on the selection. Subsequent sessions use
   the existing build; launching does not recompile it.

### DLC (add-on content)

Select the game and choose **Game → Install DLC…**. The game's **DLC window** opens: one row per installed package
with the title ids and versions installed from it, and three buttons.

- **Add DLC…** opens a picker for `*.nsp *.nsz *.xci *.xcz *.nca`. Any number of files can be chosen at once (or
  dropped on the window), and an NSP that holds the game, its update and its DLC together is fine (only the add-on
  titles are taken). The launcher checks each file's type and signature, adds it to the game's DLC list
  (`Game.dlc` in `library.json`) and has the engine install it into the build's data folder
  (`switchrecomp.py --sync-dlc … --datadir <build>/<name>_data`); the engine's output goes to the build console.
  What the engine refuses (a ticket tied to another console, keys without the content's generation, DLC of another
  game, a package without DLC, a title that another package already provides) is shown under the list with its
  reason and leaves the list again. `prod.keys` is required (**Keys → Install keys**).
- **Remove** uninstalls the selected rows (Ctrl or Shift select several; Delete and the context menu do the same):
  they leave the list and the build's data folder right away. The original files are never changed or deleted.
- **Close** closes the window; nothing waits for it, every action is applied when it is clicked.

Also:

- A game that is not built yet keeps the list; the DLC is installed when its build finishes.
- **Play** checks that the data folder holds exactly the listed DLC and installs or removes what differs first.
- **Properties → DLC** shows the same list; an entry removed there (right-click → **Remove**, or Delete) is
  uninstalled on **OK**.
- `nxrs --select-title <game> --install-dlc <package>` and `--remove-dlc <package>` (both repeatable) do
  the window's Add and Remove without any dialog; the exit code is 0 or 1.
- DLC installed from the command line (`switchrecomp.py --install-dlc`) shows up in the list by itself.
- On Linux the engine needs `hactool` on the PATH for DLC, as for NSP builds.

Design, formats and what was verified: [DLC_IN_PLACE.md](DLC_IN_PLACE.md).

### Updates

**Game → Install Updates…** opens the game's **Updates window**, the DLC window's twin: **Add update…** (several
files at once, or a drop) for `*.nsp *.xci *.nca`, **Remove** for the selected rows, **Version to build** (the newest
installed update, a particular one, or the game without updates) and **Build now**. An update replaces the game's
code, so every change there needs the game built again: the card's button becomes **Rebuild** with the status
**Update changed** and **Play** waits for the build. The update's data is read in place from its package.
**Properties → Updates** shows the same list. Everything else: [UPDATES_IN_PLACE.md](UPDATES_IN_PLACE.md).

To add a game you already compiled, choose **File → Import build…** and select its EXE. The corresponding
`_code.dll` and `_data` folder must be next to it. On Linux select the extensionless executable with its
matching `_code.so`. NUP sources are recovered from `_data/romfs.nup` when present.

The original NUP must remain accessible while playing. Changing the source in Properties requires a rebuild.
The same applies to direct NSP builds. Before building an NSP, choose **Keys > Install keys** and select your legally
dumped `prod.keys`. The launcher copies it to its private application-data folder, not the engine or game build.
Removing a library entry does not delete its game, source, saves, or caches. Cancelled/failed builds are kept
marked as needing a build rather than treating partially overwritten artifacts as ready.

### Supported inputs and keys

The package picker explicitly offers `*.nup *.nsp *.nro`. The launcher checks a `.nsp` for PFS0 magic and labels it
**DIRECT NSP**. This is an import check, not full package validation. The recompiler supports application NSPs with
plaintext or AES-CTR encrypted ExeFS and RomFS, and combined packages that hold the game and its update (the update's
BKTR patch is applied in place; a package's DLC joins the game's DLC list on the first build). Update-only packages,
tickets bound to one console and sparse/compressed layouts are refused with the reason. The built game's version is
shown in the selection line and in **Properties → General**. See
[NSP_IN_PLACE.md](NSP_IN_PLACE.md#supported-packages) for hactool/key requirements and
[NUP_IN_PLACE.md](NUP_IN_PLACE.md) for indexed NUP restrictions. Renaming another format does not convert it.

**Add extracted folder…** accepts a folder containing `main`, `exefs/main`, or `ExeFS/main`; keep its corresponding
game data available. **Import build…** accepts native artifacts rather than source packages.

The key installer checks for a header-key name and copies the selected file atomically to
`keys/prod.keys` inside the launcher's application-data directory. It does not verify that all key values are
correct. **Keys installed** confirms a copy, not successful decryption. The launcher passes the installed path
as `SWR_PROD_KEYS` to builds/games; key contents are not copied into build outputs. Missing keys block NSP builds
with **NSP builds need prod.keys. Use Keys > Install keys first.**

## Desktop layout

- Native Windows title bar and window controls.
- File, Game, Keys, View, and Help menus.
- The command strip contains **Add game** and **Search & Filter**.
- Compact artwork cards with the title in the footer. Only unbuilt/problem entries show a corner badge.
- Each card has its own **Play / Build / Stop / Cancel build** button and a Properties button.
- The sortable list fills the central area edge-to-edge. Its context menu provides per-game actions
  (play, build, properties, folders, log, remove).
- **Search & Filter** contains search, state/source filters, sorting, Gallery/List selection, and the card-size slider.
- Active filters are counted on that command and in the status bar; **Clear filters** resets the view.
- Compact status bar with the game count, build progress on the left, selection details, and an engine
  health badge — click the badge to see what is missing and jump to Settings.
- **Settings** is at the bottom right of the status bar and remains available through the File menu.
- Dockable build output, opened by **View → View console** or **Ctrl+L**.

There is no dashboard or permanent inspector. **Properties** holds General, Build, Runtime, Artwork, DLC, and Updates tabs.
Global tool paths, default build storage, worker count, and backdrop visibility are in **File → Settings**.

**Settings → Appearance** offers Dark/Light and Lime, Mint, Violet, Rose, or Cyan accents. Changes preview
immediately; OK saves them and Cancel restores the previous appearance. Focus/selection uses surface changes
instead of outline borders. Each theme uses appropriate foreground colors rather than simply inverting the
dark palette. Tests check primary/secondary text and accent-button text contrast for all ten combinations;
this is not a claim of a complete accessibility audit.

### Cover art and backgrounds

Open **Properties → Artwork** to choose a cover image and a separate background image. The selected game's
background is dimmed behind the gallery; if none is specified, its cover is used. Turn this off from
**View → Show selected game background** or Appearance settings. Images stay at their selected paths.

An unbranded gamepad placeholder appears when no image is assigned. Existing game icons are used when available.
PNG, JPEG, WebP, and other supported Qt formats can be used. The Qt Image Formats module, including `qwebp.dll`,
is required and deployed by the build script. Images that cannot be decoded produce a clear error when assigning
them. Artwork is cached by path/size/modification time; saving Properties or pressing F5 also invalidates the cache.
Preview/demo entries are not saved and do not contain bundled games or downloadable artwork.

## Build and runtime options

**Properties → Build** contains the output folder, compiler workers (0 inherits the global default), and
optimization level. O2 is the default. The output filename is fixed for an existing entry so renaming a
display title does not silently rename native modules and save directories.

**Properties → Runtime** selects Vulkan/native NVN or OpenGL. The renderer value is written to the game INI
on the next launch without discarding other settings. Vulkan needs a runtime built with NVN, deployed shader
tools, Python, and .NET 10. The launcher checks the latter tools and reports missing setup instead of silently
launching a Vulkan game with no shader translator.

**Settings → Controller** sets the default controller button layout, the engine's `button_layout` INI key:
**Switch** (by position like the Switch pad: right = A, bottom = B; on a PlayStation pad Circle = A, Cross = B) or
**Xbox** (by the printed Xbox letters: Cross = A, Square = X). **Properties → Runtime → Controller layout** overrides
it per game ("Use default" inherits). The launcher writes the effective value into the game INI after a build, on
every Play, and right away when either setting changes for an already-built game (a running game applies it live).
Per-button remapping (`button_map`) stays INI-only; see the engine's docs/CONTROLLERS_WIN32.md.

The frontend uses the toolkit's prebuilt runtime and calls the CLI entry `src/recomp_cli.py` (which imports the
vendored engine) through `QProcess` with an argument list. Spaces, brackets, and shell metacharacters in paths are
not passed through a shell. Game processes are
detached, so closing the launcher does not terminate them. Windows running-state detection matches the full EXE
path, including games started outside the launcher. **Cancel build** affects only that card's compilation;
**Stop** requests a normal close of that card's game, after rechecking the process path. It does not force-kill
games. Properties remain available during a run/build for artwork and display-name edits; source/build/runtime
controls are locked until that operation finishes.

Controller, audio, graphics, and vibration support belong to the underlying runtime. The fork includes Windows
Sony HID input and classic-rumble/player-light code; physical output checks remain pending. See
[CONTROLLERS_WIN32.md](CONTROLLERS_WIN32.md). Queued stereo audio and smooth slowdown are available, while some
Switch 2 audio/video paths remain incomplete. The performance target is 60 FPS, not a guarantee: the documented
integrated race measured about 58.4 presentations/s with stutters, not stable minimum 60 FPS. See
[the performance investigation](MKW_PERFORMANCE_INVESTIGATION.md).

## Library storage and keyboard shortcuts

### Existing compiled games

Builds created from the command line are not automatically library entries. Use **File > Import build…** and
select the game's executable. Each build needs the executable, matching `_code.dll` / `_code.so`, and matching
`_data` directory.
The importer reads `romfs.nsp` / `romfs.nup` to associate the source and reads the renderer from the game's INI.
It does not compile, copy, or launch the game. An unbuilt entry for that source receives the build paths while
keeping its title, artwork and build preferences. Separate completed builds can coexist; repeated imports of
the same executable do not duplicate it.

For example, import `build/mkw-nsp-1.8/Mario Kart World 180.exe`. It appears as **Mario Kart World 180**,
format **DIRECT NSP**, with **Play** when the NSP referenced by `_data/romfs.nsp` is available. The original
NSP must stay at that path because RomFS is read from it in place. Select the card's **Play** button or double-click
the card. Direct launch uses that same EXE; from PowerShell, set the interpreter for older runtime copies first:

```powershell
$env:SWR_PYTHON = (Resolve-Path '.\.venv\Scripts\python.exe').Path
Start-Process -FilePath '.\build\mkw-nsp-1.8\Mario Kart World 180.exe' -WorkingDirectory '.\build\mkw-nsp-1.8'
```

The current Qt launcher passes its selected Python to the game automatically, including older runtime copies.
Direct-launch interpreter discovery changes in
`third_party/Switch2RECOMP/runtime/nvn_hook.c` require a later runtime rebuild/deployment.

The default library is `library.json` under Qt's local application-data directory for
`nxrs/nxrs` (normally `%LOCALAPPDATA%\nxrs\nxrs`; the folder of the earlier name,
`%LOCALAPPDATA%\RecompLauncher\RecompLauncher`, or the older `SwitchRecompLauncher`, is copied over once on first
start and left in place). It stores paths,
display metadata, per-game build options, view preferences, and window/dock layout. Writes are atomic.
An unreadable/corrupt library is not silently overwritten.

| Shortcut | Action |
|---|---|
| Ctrl+O | Add NUP/NSP/NRO |
| Ctrl+B | Build selected game |
| Ctrl+F | Open Search & Filter and focus title search |
| Esc | Close the search/filter popout |
| Ctrl+L | View console |
| Alt+Enter | Game Properties |
| Enter / double-click | Play ready game; open build options if unbuilt |
| F5 | Refresh library and tools |

## Build the launcher

Windows requirements: Visual Studio 2022 C++ x64 tools, Qt 6 MSVC x64 (Core, Gui, Widgets, Test, and the Qt Image Formats
module), and Python.
The script discovers Visual Studio with `vswhere`, builds, runs the core/UI tests, and deploys Qt beside the EXE.
Python selection is: explicit `SWR_PYTHON`, `.venv`, `venv`, portable `python`, then PATH. CMake 3.31.10 and
Ninja 1.13.2 must already be installed in that interpreter. A missing dependency is reported; the build does not
create a venv or download packages. The launcher uses the same interpreter order and exports its selection as
`SWR_PYTHON` to builds and games. Settings > Engine shows the selected absolute Python path.

```powershell
# From the repository root:
& ".\scripts\build-launcher.bat"
# Explicit Qt installation:
& ".\scripts\build-launcher.bat" -QtPrefix "C:\Qt\6.8.3\msvc2022_64"
```

Output: `bin/launcher/nxrs.exe`; full build/test/deploy output: `logs/launcher_release.log`.

Linux uses Qt 6.8.3 `gcc_64` with Widgets, Test and Image Formats, CMake, Ninja and a native C++ toolchain:

```sh
export SWR_QT_PREFIX=/path/to/Qt/6.8.3/gcc_64
sh scripts/build-launcher.sh
./bin/launcher-linux/nxrs
```

Both frontend builds need an existing toolkit runtime to compile games. See
[BUILD_ENVIRONMENT_GUIDE.md](BUILD_ENVIRONMENT_GUIDE.md) or use a complete portable release.

## Verification

```powershell
& ".\.venv\Scripts\python.exe" third_party\Switch2RECOMP\tests\test_python_selection.py -v
& ".\.venv\Scripts\python.exe" third_party\Switch2RECOMP\tests\test_launcher_workflow.py LauncherWorkflow.test_import_existing_build_without_compiling
# Separate compilation smoke test, when runtime/build work is not in progress:
& ".\.venv\Scripts\python.exe" third_party\Switch2RECOMP\tests\test_launcher_workflow.py LauncherWorkflow.test_build_saves_ready_library_entry
```

The workflow test uses a synthetic NUP (no copyrighted game data), a disposable library, and paths containing
spaces, brackets, and `&`. It exercises the deployed launcher, actual GCC compilation and linking, shader-tool
staging, and persistence of the successful ready-to-play entry. It does not run the synthetic guest code.

Core Qt tests cover library round trips/corruption protection, argument boundaries, artifact readiness,
per-game build settings, preservation of unrelated game INI settings, combined filters, and theme contrast.
UI tests cover filter clearing and keyboard dismissal. Core tests also check per-card action states,
running-EXE identification, equivalent Windows path spellings, NSP classification, imported NSP builds and key-path
environment propagation. The documentation fixture below adds actual Window/dialog interaction checks.

## Screenshot capture

The README images in [`docs/images/launcher/`](images/launcher/) are captures of the running launcher's
window (Win32 `PrintWindow`) with a real library: `library.jpg` is the gallery, `building.jpg` a real game
build caught while it was generating C. The launcher also has its own `--screenshot <png>` flag, which grabs
the window (or the active dialog/popout) with `QWidget::grab()` and exits; `--demo` seeds a non-persistent
preview library, and `--show-filters`/`--show-properties`/`--show-console`, `--preview-theme` and `--list-view`
open panes or switch appearance for scripted captures.

Verification for these docs (November 15, 2026):

| Check | Result |
| --- | --- |
| `scripts\setup.bat` | venv with pinned CMake 3.31.10/Ninja 1.13.2 and `backports.zstd`; GCC 16.2 found in `third_party\gcc` |
| `scripts\build-runtime.bat` | Built `third_party\Switch2RECOMP\bin\runtime_win64_win32.exe` |
| `scripts\build-launcher.bat` | Build/deploy; both CTest suites passed |
| `.venv\Scripts\python.exe third_party\Switch2RECOMP\tests\test_nup.py -v` | Six synthetic parser tests passed, including raw/zstd reads and corruption rejection |
| `.venv\Scripts\python.exe third_party\Switch2RECOMP\tests\test_nsp.py -v` | Three synthetic parser tests passed: ExeFS/RomFS, invalid PFS0, BKTR rejection |
| `.venv\Scripts\python.exe third_party\Switch2RECOMP\tests\test_python_selection.py -v` | Passed (POSIX shell selection skipped on Windows) |
| `test_launcher_workflow.py` | Import-without-compiling and full synthetic build through the deployed launcher passed |
| CLI `scripts\build-from-nup.bat` + game run | Output contract produced; runtime booted (audio, in-place RomFS, OpenGL), guest module executed |

## Release integration

The launcher is a frontend, not a replacement for the toolkit. A Windows release should contain this deployed
Qt folder together with the toolkit's `switchrecomp.py` and its Python modules (`nup.py`, `nsp.py`, `nca.py`, `dlc.py`,
`nxloader.py`, `a64.py`, `codegen.py`), the built `runtime_win64_win32.exe`, shader tools and their ABI
header/licenses, scripts/docs, and the engine's LICENSE.txt/NOTICE.txt. Portable Python/GCC may be installed
by `scripts/setup.bat`; .NET 10 is required for the live Vulkan shader translator. Game builds stage shader
tools without requiring the development-only `build/nvn/libnvn.a` to exist on the player's machine.

Do not package personal `library.json` files, games, saves, extracted data, shader caches, captures, or logs.
This native Qt launcher is `nxrs.exe`.
