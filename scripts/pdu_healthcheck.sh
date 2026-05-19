#!/usr/bin/env bash
# Verify /api/power responds within 2s — used by systemd WatchdogSec
set -euo pipefail
response=$(curl -sf --max-time 2 http://localhost:8080/api/power) || exit 1
echo "$response" | python3 -c "import sys,json; d=json.load(sys.stdin); sys.exit(0 if 'thermal_state' in d else 1)"
