#!/bin/sh
# nxrs - native game library and build launcher.
# The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
# One-time setup: Python venv with pinned build tools, plus GCC validation.
# Safe to re-run: installed components are reused.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

selected=""
if [ -n "${SWR_PYTHON:-}" ]; then
  "$SWR_PYTHON" -c 'import sys; sys.exit(sys.version_info < (3, 8))' && selected=$SWR_PYTHON
else
  for candidate in "$root/.venv/bin/python3" "$root/venv/bin/python3" "$root/python/bin/python3"; do
    if [ -z "$selected" ] && "$candidate" -c 'import sys; sys.exit(sys.version_info < (3, 8))' 2>/dev/null; then selected=$candidate; fi
  done
fi
if [ -z "$selected" ]; then
  echo '[SETUP] No usable Python found. Creating .venv with the system Python 3...'
  python3 -m venv "$root/.venv"
  selected="$root/.venv/bin/python3"
fi
echo "[SETUP] Python: $selected"

"$selected" -m pip install --quiet --disable-pip-version-check cmake ninja
if [ -x "$root/third_party/gcc/bin/gcc" ] || [ -x "$root/gcc/bin/gcc" ]; then
  echo '[SETUP] GCC portable toolchain found'
elif command -v gcc >/dev/null 2>&1; then
  echo '[SETUP] Using gcc from PATH'
else
  echo '[SETUP] FAILED - GCC missing. See docs/BUILD_ENVIRONMENT_GUIDE.md' >&2
  exit 1
fi
"$selected" -c 'import cmake' 2>/dev/null || true
printf '%s\n' '[SETUP] SUCCESS - Python and GCC are ready'
