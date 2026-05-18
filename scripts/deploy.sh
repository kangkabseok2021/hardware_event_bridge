#!/usr/bin/env bash
set -euo pipefail
# Deploy updated binary with zero-downtime reload.

echo "[deploy] Building..."
cmake --build build -j"$(nproc)" --target hardware_event_bridge

echo "[deploy] Stopping service..."
systemctl stop hardware-event-bridge || true

echo "[deploy] Installing binary..."
install -Dm755 build/hardware_event_bridge /opt/hardware-event-bridge/hardware_event_bridge

echo "[deploy] Restarting service..."
systemctl start hardware-event-bridge
systemctl is-active --quiet hardware-event-bridge && echo "[deploy] OK" || echo "[deploy] FAILED"
