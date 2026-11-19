#!/bin/sh
# nxrs - native game library and build launcher.
# The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.

set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${SWR_QT_PREFIX:?Set SWR_QT_PREFIX to the Qt 6.8.3 gcc_64 directory}"
build="$root/build/launcher-linux"
deploy="${SWR_LAUNCHER_DEPLOY:-$root/bin/launcher-linux}"
cmake -S "$root/src/launcher" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$SWR_QT_PREFIX" -DCMAKE_INSTALL_PREFIX="$deploy"
cmake --build "$build" --parallel "${SWR_JOBS:-8}"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure
cmake --install "$build"
