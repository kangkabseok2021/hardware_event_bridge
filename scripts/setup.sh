#!/usr/bin/env bash
set -euo pipefail

echo "[setup] Installing dependencies..."
apt-get update -qq
apt-get install -y --no-install-recommends \
    build-essential cmake git libsqlite3-dev

echo "[setup] Building..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_DEFAULT_CMP0135=NEW
cmake --build build -j"$(nproc)"

echo "[setup] Installing service..."
install -Dm755 build/hardware_event_bridge /opt/hardware-event-bridge/hardware_event_bridge
install -Dm644 systemd/hardware-event-bridge.service \
    /etc/systemd/system/hardware-event-bridge.service

id heb &>/dev/null || useradd -r -s /bin/false heb
mkdir -p /opt/hardware-event-bridge/data
chown heb /opt/hardware-event-bridge/data

systemctl daemon-reload
echo "[setup] Done. Run: systemctl enable --now hardware-event-bridge"
