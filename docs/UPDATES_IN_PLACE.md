# Game Updates: Install, Choose, Rebuild

Status 2026-11-18: **built**. **Game → Install Updates…** opens a game's Updates window: add one or several update
packages, remove them again, and choose which version the game is built with. The engine side is
`third_party/Switch2RECOMP/docs/UPDATES.md`; a game and its update in one file is
[NSP_IN_PLACE.md](NSP_IN_PLACE.md) and the engine's `docs/COMBINED_NSP.md`.

## 1. What an update is here

On a console an update is installed beside the game and applied when the game starts. An emulator does the same
(section 6). nxrs recompiles the game's code ahead of time, and an update **replaces that code**, so:

- **Installing, removing or choosing another update needs the game built again.** The launcher never hides this:
  the card's button turns into **Rebuild** with the status **Update changed**, **Play** is unavailable until the
  build is done, and the Updates window says which version the current build is and which the next will be.
- **The update's data is not copied.** The built game reads its RomFS in place from the game's package and the
  update's package and composes them on every read. Both files have to stay where they are; nothing in them is
  ever written.
- **One update applies at a time**: the newest installed one, unless you choose another or none.

## 2. The Updates window

**Game → Install Updates…** for the selected game.

| Control | What it does |
|---|---|
| **Version to build** | `Newest installed update — 1.1.1 (v262144)` (the default), each installed update by version and file, or `The game without updates`. Choosing takes effect at the next build |
| the list | one row per installed package: file name, display version and metadata version, and its state: `applied` (the current build has it), `selected, build the game to apply it`, `in the current build, not selected`, `file not found` |
| **Add update…** | a picker for `*.nsp *.xci *.nca`, any number of files at once; dropping files on the window does the same. Each file is checked by the engine against the game's own package with your keys (version, and that it is this game's update). What is refused is shown under the list with its reason and does not join the list |
| **Remove** | takes the selected rows off the list (Ctrl or Shift select several; Delete and the context menu do the same). The files are not touched |
| **Build now** | enabled when the current build is not the chosen version: closes the window and starts the build |
| **Close** | every action is applied when it is clicked; nothing waits for Close |

Under the list a line says what is pending, for example: `The current build is the game as its package is. Build
the game again to get update 1.1.1 (v262144): an update replaces the game's code.`

A game that is running or building opens the window read-only. **Properties → Updates** shows the same list; an
entry removed there (right-click → Remove, or Delete) leaves on **OK**.

Without dialogs: `nxrs --select-title <game> --install-update <package>` / `--remove-update <package>`
(both repeatable), `--update-choice newest|none|<package>`, `--build` (build the selected game and exit),
`--show-updates` (open the window). Exit code 0 or 1.

## 3. What can be an update package

| File | |
|---|---|
| `.nsp` | an update package, or a combined package (game + update, also + DLC) of the same game: its update is used |
| `.xci` | a card image that holds the update |
| `.nca` | the update's Program NCA with its `.tik` beside it. No version information: it shows as "version unknown" and never counts as the newest |
| `.nsz`, `.xcz` | refused: a compressed file cannot be read in place while the game runs. Decompress it first (`nsz -D`) |

The game itself has to be an **NSP** package. A NUP already contains its update, and a game built from an extracted
folder has no package for the update's data to be composed over; the window says so when an update is added.
`prod.keys` is needed (**Keys → Install keys…**).

## 4. In the library and the build

`library.json`, per game:

```json
"updates": [{"path": "D:/Games/Game [UPD][v262144].nsp", "titleId": "01001f5010dfa000", "version": 262144, "display": "1.1.1"}],
"updateChoice": "",
"builtUpdate": "D:/Games/Game [UPD][v262144].nsp"
```

- `updateChoice`: `""` the newest installed, `"none"` the game without updates, or a package path.
- `builtUpdate`: what the last successful build applied (`""`, `"none"` or a path). A game counts as built only
  when this equals what is chosen now (`Game::hasBuild`, `Game::updatePending`); otherwise the card shows
  **Rebuild**.
- The build gets `--update <package>` or `--no-update` (`BuildManager::arguments`). The engine keeps the generated C
  of each version in a folder of its own, so going back to a version that was built before only links.
- Adding a package runs `switchrecomp.py <game's package> --update-info <packages>` (`UpdateProbe` in
  `buildmanager.cpp`); its lines go to the build console.

## 5. When something is missing

- **The update's file was moved or its drive is offline**: Build refuses with the file's name; a game already built
  with it stops at start with `the update package ... is missing` in its log instead of running with wrong data.
  Put the file back, or remove the update in the window and rebuild.
- **The keys were removed**: as for any encrypted package, the game says which key file it needs.

- **Hints.** The addresses a running game reports for the next build are kept per version of the program
  (`<name>.hints.for`, `<name>.hints.<id>`); before this, a base version built with an update's hints stopped at
  start.

## 6. Verified (2026-11-18)

Real library, real launcher code path (the window's actions through `--install-update`, `--remove-update`,
`--update-choice` and `--build`), game runs with the harness under the game lock and a memory watchdog. The owner's
dumps hold no game with a separate update, so Pokémon Legends: Arceus' combined `(1G+1U)` package was split into a
game package and an update package in the evidence folder (members copied byte for byte; the original was only
read). Evidence: `D:/Projects/Switch2RECOMP-main/build/evidence/updates/`.

| Step | In the launcher | The game |
|---|---|---|
| the game alone, imported | status bar `v1.0.0` | `title: version 1.0.0`, RomFS 6,104.2 MB, opening with Arceus (`upd-1-base-vk`) |
| a wrong package (Mario Kart 8 Deluxe's combined NSP) | refused: `this update is for 0100152000022000, not for this game`, exit 1, list unchanged | |
| **Add update…** with the 1.1.1 package | list `... — 1.1.1 (v262144) — selected, build the game to apply it`, pending line, card **Rebuild** (`launcher/updates-window-pending.png`, `main-rebuild.png`) | |
| **Rebuild** | 561 C units, 35 min; then `applied` (`updates-window-applied.png`, `properties-updates.png`) | `title: version 1.1.1`, `the update of its own package applied over the game`, 6,123.9 MB, opening to "What is thy name?" (`upd-2-updated`) |
| Version to build: none, then newest | pending, then not pending; no build needed | |
| **Remove**, **Rebuild** | list empty, card **Rebuild**; the build links only: 107 s | `title: version 1.0.0`, 6,104.2 MB, opening to "Select your appearance" (`mem-base`) |
| **Add update…** again, **Rebuild** | about 2 min | `title: version 1.1.1` again, "Select your appearance" (`mem-update`, `upd-returns`) |

Memory: the game's private bytes are 5.15 GB and flat over 234 s for both versions; a launcher-driven rebuild peaks
at 5.6 GB for its whole process tree.

Not verified: the window's own buttons were not clicked (its GUI tests cover them; the real runs used the same
functions through the command-line options), XCI and bare-NCA updates on real data (synthetic tests only), and a
game with several real updates (the choice was exercised with one update and "none").

## 7. Eglantine (yuzu), for comparison

Eglantine lists every installed update of a game by version, newest first, as checkboxes in the game's add-ons list,
at most one ticked; the first one not disabled applies, so the newest by default (`patch_manager.cpp`,
`configure_per_game_addons.cpp`). At start it swaps in the update's ExeFS (`PatchExeFS`) and reads the RomFS through
the update NCA layered over the base NCA: the AES-CTR-EX storage under the indirect (relocation) storage
(`PatchRomFS`, `content_archive.cpp`, `NcaFileSystemDriver`). "Version to build" is the same choice; the ExeFS swap
is the rebuild; the layered read is done by the built game's runtime.
