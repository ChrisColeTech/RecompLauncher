#!/usr/bin/env python3
# RecompLauncher - native game library and build launcher.
"""Entry point for building games and the runtime with the vendored engine.

The vendored Switch2RECOMP project under third_party/ is used as a library; this
module is the only interface the launcher and the build scripts invoke.

Usage: python src/recomp_cli.py GAME.nup|.nsp|.nro|FOLDER [options]
       python src/recomp_cli.py --build-runtime --target win64 --backend win32 --cc gcc
All engine options (-o, --outdir, -O, --jobs, --runtime, --native, ...) pass through.
"""
import os
import sys


def main():
    engine = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                          os.pardir, "third_party", "Switch2RECOMP"))
    if not os.path.isfile(os.path.join(engine, "switchrecomp.py")):
        raise SystemExit("The vendored engine is missing: %s" % engine)
    sys.path.insert(0, engine)
    import switchrecomp
    return switchrecomp.main()


if __name__ == "__main__":
    sys.exit(main())
