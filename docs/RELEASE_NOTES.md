Portable Windows x64 and Linux x64 bundles of the SwitchRecomp Launcher with the
Switch2RECOMP-turbo engine, based on SwitchRecomp by MZ Labs.

Extract the complete archive and run `RecompLauncher.cmd` on Windows or
`./RecompLauncher.sh` on Linux. The bundle includes Python, GCC/G++, .NET,
Qt, the native runtime, and shader tools. No separate developer-tool installation
is required. Linux requires an Ubuntu 24.04-compatible desktop with X11/XWayland;
both platforms require a compatible GPU driver and AVX2-capable x64 CPU.

Each bundle passed a relocated synthetic-NUP build through the Qt launcher.
This checks packaging and compilation, not compatibility with every game.
Use your own game dumps. No games, keys, firmware, or personal library are included.

See `docs/RELEASES.md` for requirements and `third_party/Switch2RECOMP/docs/THIRD_PARTY.md`
for licenses and source.
