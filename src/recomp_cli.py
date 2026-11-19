#!/usr/bin/env python3
# nxrs - native game library and build launcher.
"""Entry point for building games and the runtime with the vendored engine.

The vendored Switch2RECOMP project is used as a library; this module is the only
interface the launcher and the build scripts invoke. In a checkout it lives in src/
and imports third_party/Switch2RECOMP; a release ships it inside engine/, beside the engine.

Usage: python src/recomp_cli.py GAME.nup|.nsp|.nro|FOLDER [options]
       python src/recomp_cli.py --build-runtime --target win64 --backend win32 --cc gcc
All engine options (-o, --outdir, -O, --jobs, --runtime, --native, ...) pass through.
"""
import os
import sys


def engine_dir():
    """The engine beside this file (a release ships it in engine/), else the checkout's vendored submodule."""
    here = os.path.dirname(os.path.abspath(__file__))
    if os.path.isfile(os.path.join(here, "switchrecomp.py")):
        return here
    return os.path.abspath(os.path.join(here, os.pardir, "third_party", "Switch2RECOMP"))


def main():
    engine = engine_dir()
    if not os.path.isfile(os.path.join(engine, "switchrecomp.py")):
        raise SystemExit("The vendored engine is missing: %s" % engine)
    sys.path.insert(0, engine)
    import switchrecomp
    return switchrecomp.main()


if __name__ == "__main__":
    sys.exit(main())
