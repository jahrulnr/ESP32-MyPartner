#!/usr/bin/env bash
# Create a temporary monitor interface on the parent's current channel.
set -euo pipefail
parent=${1:-wlp9s0}
monitor=${2:-espnowmon}
project_dir=$(cd "$(dirname "$0")/../.." && pwd)
if [[ ${EUID} -ne 0 ]]; then
  echo 'Run this script with administrator privileges.' >&2
  exit 1
fi
if /usr/sbin/iw dev "$monitor" info >/dev/null 2>&1; then
  echo "Interface $monitor already exists; refusing to replace it." >&2
  exit 1
fi
/usr/sbin/iw dev "$parent" interface add "$monitor" type monitor
trap '/usr/sbin/iw dev "$monitor" del' EXIT
/usr/sbin/ip link set "$monitor" up
cd "$project_dir"
python3 -B -m tools.espnow_linux --interface "$monitor"
