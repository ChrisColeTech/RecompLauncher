# RecompLauncher

[![Portable release](https://github.com/ChrisColeTech/RecompLauncher/actions/workflows/release.yml/badge.svg?branch=main)](https://github.com/ChrisColeTech/RecompLauncher/actions/workflows/release.yml)
[![Engine CI](https://github.com/ChrisColeTech/Switch2RECOMP-turbo/actions/workflows/engine-ci.yml/badge.svg?branch=turbo)](https://github.com/ChrisColeTech/Switch2RECOMP-turbo/actions/workflows/engine-ci.yml)
A game library for the [Switch2RECOMP](https://github.com/ChrisColeTech/Switch2RECOMP-turbo) static recompiler.
Add your own Switch game dumps, build them into native Windows or Linux games with one click, and play them
from the same library.

![Library](docs/images/launcher/library.jpg)

## Download and run

1. Download the zip (Windows) or tar.gz (Linux) from the latest
   [release](https://github.com/ChrisColeTech/RecompLauncher/releases/latest).
2. Extract the whole archive into a writable folder.
3. Run `RecompLauncher.exe` (Linux: `./RecompLauncher`).

Everything a build needs ships in the archive: Python, GCC, .NET and the shader tools. Nothing else to install.

## What you need

- **Your own game dumps**: `.nup` packages, `.nsp` packages, homebrew `.nro`, or an extracted folder with
  `exefs/main`. An `.nsp` can be a plain game, an encrypted Switch 1 game, or a **combined** package that holds the
  game with its update and DLC (`1G+1U`, `1G+1U+1D`): the update is applied while the game reads its data, nothing
  is extracted. No games, keys or firmware are included or downloaded.
- **`prod.keys`** for `.nsp` builds and for DLC, installed once with **Keys → Install keys…**. Update-only NSPs
  (without the game in the same file), tickets bound to one console and sparse or compressed packages are not
  supported ([details](docs/NSP_IN_PLACE.md#supported-packages)).
- **A 64-bit CPU with AVX2** and a GPU driver with Vulkan support. On Linux: Ubuntu 24.04 or newer, with X11 or
  XWayland.

## Add, build, play

1. **Add** — click **Add game** (Ctrl+O), or drag a package onto the window. Use the arrow next to it for an
   extracted folder or a game you already built.
2. **Build** — click **Build** on the card (Ctrl+B). The first build takes a while; progress shows on the card
   and the full output in the build panel.
3. **Play** — the card's button turns into **Play** (or double-click the card).

![Building](docs/images/launcher/building.jpg)

### DLC

Select the game, then **Game → Install DLC…** to open its DLC window:

- **Add DLC…** (or drop files on the window) installs one or several packages at once: `.nsp`, `.nsz`, `.xci`,
  `.xcz` or `.nca`. A package that holds the game, its update and its DLC together works too.
- **Remove** uninstalls the selected entries (select several with Ctrl or Shift). Your original files are never
  changed.

It needs `prod.keys` (**Keys → Install keys…**). The DLC is in effect the next time you play. A package that cannot
work, such as one whose ticket is tied to another console, is refused with the reason
([details](docs/DLC_IN_PLACE.md)).

### Updates

Select the game, then **Game → Install Updates…** to open its Updates window:

- **Add update…** (or drop files on the window) adds one or several update packages: `.nsp`, `.xci`, or an `.nca`
  with its ticket. The game itself has to be an `.nsp` package.
- **Version to build** chooses which one applies: the newest (the default), a particular one, or the game without
  updates. **Remove** takes the selected entries off the list.
- An update replaces the game's code, so the game has to be **built again** to apply a change: the card shows
  **Rebuild** until then, and **Build now** in the window starts it. The update's data is read from its file while
  you play: keep the file where it is ([details](docs/UPDATES_IN_PLACE.md)).

## Good to know

- **Keep your game files where they are.** Builds read the game's data straight from the original package, so
  moving or deleting it breaks the build.
- **Games run on their own.** Closing the launcher doesn't close a running game.
- **Cover art**: open a game's properties (the sliders button, or Alt+Enter) → **Artwork**. The selected game's
  art also fills the background.
- **Renderer**: Vulkan by default, OpenGL as an alternative, per game in properties → **Runtime**.
- **Controller buttons**: Settings → **Controller** chooses Switch (by position, the default; Circle = A on a
  PlayStation pad) or Xbox (by label, Cross = A); properties → **Runtime** can override it per game.
- **Settings** (bottom right) has the theme and accent color, the default build folder and worker count.
  The **Engine ready** badge next to it tells you if a build tool is missing.

| Shortcut | Action |
|---|---|
| Ctrl+O | Add game |
| Ctrl+B | Build selected game |
| Enter / double-click | Play |
| Alt+Enter | Properties |
| Ctrl+F | Search & Filter |
| Ctrl+L | Build output |
| F5 | Refresh library |

## Building from source

See [docs/DEVELOPING.md](docs/DEVELOPING.md). Release notes for every build are on the
[releases page](https://github.com/ChrisColeTech/RecompLauncher/releases).

## License

The engine is SwitchRecomp by MZ Labs, AGPL-3.0; its notices are in `legal/` in a release and in
[third_party/Switch2RECOMP](third_party/Switch2RECOMP) in the source.
