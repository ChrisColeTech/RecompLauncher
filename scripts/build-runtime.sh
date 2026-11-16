#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/scripts/_env.sh"
if [ -n "${SWR_CC:-}" ]; then cc=$SWR_CC
elif [ -x "$root/third_party/gcc/bin/x86_64-conda-linux-gnu-gcc" ]; then cc=$root/third_party/gcc/bin/x86_64-conda-linux-gnu-gcc
else cc=gcc
fi
"$python" "$root/src/recomp_cli.py" --build-runtime --target linux --backend x11 --cc "$cc" "$@"
printf '%s\n' '[RUNTIME] SUCCESS - third_party/Switch2RECOMP/bin/runtime_linux_x11'
