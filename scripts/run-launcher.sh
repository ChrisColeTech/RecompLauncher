#!/bin/sh
# nxrs - native game library and build launcher.
# The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
# Run the deployed Qt launcher from a developer checkout.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
launcher="$root/bin/launcher-linux/nxrs"
[ -f "$launcher" ] || launcher="$root/bin/launcher/nxrs"
if [ ! -x "$launcher" ] && [ ! -f "$launcher" ]; then
  echo "[RUN] Launcher not found: $launcher" >&2
  echo 'Build it first with: SWR_QT_PREFIX=... sh scripts/build-launcher.sh' >&2
  exit 1
fi
exec "$launcher" "$@"
