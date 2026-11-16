Portable Windows x64 and Linux x64 bundles of the SwitchRecomp Launcher with the
Switch2RECOMP-turbo engine, based on SwitchRecomp by MZ Labs.

Extract the complete archive and open `RecompLauncher.exe` (Windows) or
`RecompLauncher` (Linux) at the top of the extracted folder. Beside it, `engine/` holds the
recompiler and prebuilt native runtime, `toolchain/` the bundled Python, GCC/G++, .NET and
shader tools, and `legal/` the licenses, corresponding sources and checksums. No separate developer-tool installation
is required. Linux requires an Ubuntu 24.04-compatible desktop with X11/XWayland;
both platforms require a compatible GPU driver and AVX2-capable x64 CPU.

Each bundle passed a relocated synthetic-NUP build through the Qt launcher.
This checks packaging and compilation, not compatibility with every game.
Use your own game dumps. No games, keys, firmware, or personal library are included.

The release carries no source code beyond what a game build reads; the complete source of
the launcher and the pinned engine is `RecompLauncher-source.tar.gz` in this release.
See `docs/RELEASES.md` for requirements and `legal/THIRD_PARTY.md` in the bundle for licenses and source.
