# PDU Power Monitor — Smart PDU Simulator

An embedded Linux extension to the Hardware Event Bridge that simulates a smart Power Distribution Unit (PDU) for a server rack. Reads real CPU temperature and memory usage from Linux kernel interfaces, generates 8-outlet power telemetry with Gaussian noise, and manages a four-state thermal protection FSM. A Python asyncio aggregator polls the REST API every 30 s and logs per-outlet metrics (P = V × I) to SQLite.

---

## Architecture

```
Linux Kernel Interfaces
  /sys/class/thermal/thermal_zone0/temp   → CPU temperature (m°C)
  /proc/meminfo                           → MemTotal / MemAvailable
         │
┌────────▼───────────────────────────────────────────────────────┐
│  PowerMonitorDevice  (C++17)                                   │
│  8 simulated outlets: V=120V ± noise, I=sinusoidal + noise    │
│  ThermalFsm: NORMAL→WARN→CRITICAL→SHUTDOWN                    │
│  Emits TELEMETRY events every 500ms through EventEngine        │
└────────┬───────────────────────────────────────────────────────┘
         │ Observer pipeline → DeviceManager (last_payload)
┌────────▼───────────────────────────────────────────────────────┐
│  GET /api/power  (httplib REST)                                │
│  {cpu_temp_c, mem_used_pct, thermal_state, outlets[]}         │
└────────┬───────────────────────────────────────────────────────┘
         │ httpx every 30s
┌────────▼───────────────────────────────────────────────────────┐
│  pdu/aggregator.py  (Python asyncio)                           │
│  P = V × I per outlet → aiosqlite                             │
│  outlet_metrics + system_metrics tables                        │
└────────────────────────────────────────────────────────────────┘
```

---

## Thermal FSM

```
NORMAL (<60°C) → WARN (60–70°C) → CRITICAL (70–80°C) → SHUTDOWN (>80°C)
                      ↑                  ↑
                 hysteresis 3°C     hysteresis 3°C
                      ↓
                  SHUTDOWN latches — requires manual reset()
```

| State | Threshold | Action |
|---|---|---|
| NORMAL | < 60 °C | All 8 outlets active |
| WARN | ≥ 60 °C | Log alert event |
| CRITICAL | ≥ 70 °C | Shed outlets 6 + 7 (two highest-load) |
| SHUTDOWN | ≥ 80 °C | Latched — operator must call `reset()` |

Hysteresis of 3 °C prevents rapid oscillation at threshold boundaries.

---

## Outlet Simulation

Each outlet generates:

```
voltage_v = 120.0 + N(0, 0.5)          V (Gaussian noise)
current_a = 2.0 + 1.5·sin(0.1t + φᵢ) + N(0, 0.05)    A
watts     = voltage_v × current_a
```

`φᵢ = 0.8·i` staggers each outlet's load cycle — prevents all outlets peaking simultaneously, realistic for a server rack.

On non-Linux platforms (CI / macOS), `readCpuTemp()` and `readMemUsedPct()` return synthetic ramp values so the same code compiles and tests without hardware.

---

## REST API — `/api/power`

```json
{
  "cpu_temp_c": 55.3,
  "mem_used_pct": 42,
  "thermal_state": "NORMAL",
  "outlets": [
    {"id": 0, "voltage_v": 120.1, "current_a": 1.82, "watts": 218.6, "active": true},
    {"id": 1, "voltage_v": 119.9, "current_a": 2.31, "watts": 277.1, "active": true},
    ...
  ]
}
```

---

## Python Aggregator

```bash
uv sync
BRIDGE_URL=http://localhost:8080 uv run python pdu/aggregator.py
```

Polls `/api/power` every 30 s, computes total rack power (Σ watts across active outlets), and appends to SQLite:

```sql
CREATE TABLE outlet_metrics (
    ts        INTEGER NOT NULL,
    outlet_id INTEGER NOT NULL,
    voltage_v REAL, current_a REAL, watts REAL, active INTEGER,
    PRIMARY KEY (ts, outlet_id)
);
CREATE TABLE system_metrics (
    ts            INTEGER PRIMARY KEY,
    cpu_temp_c    REAL,
    mem_used_pct  INTEGER,
    thermal_state TEXT,
    total_watts   REAL
);
```

---

## systemd Service

```bash
# Install after the bridge service is running
sudo cp systemd/hardware-pdu.service /etc/systemd/system/
sudo systemctl enable --now hardware-pdu
```

The service starts `After=hardware-event-bridge.service` — the aggregator only polls once the bridge exposes `/api/power`.

Healthcheck: `scripts/pdu_healthcheck.sh` — used by monitoring systems or Docker `HEALTHCHECK`:
```bash
curl -sf --max-time 2 http://localhost:8080/api/power | \
  python3 -c "import sys,json; d=json.load(sys.stdin); sys.exit(0 if 'thermal_state' in d else 1)"
```

---

## Testing

### C++ — GoogleTest (15 tests)

```bash
ctest --test-dir build --output-on-failure -V
```

| Suite | n | Tests |
|---|---|---|
| `ThermalFsmTest` | 10 | StartsNormal, NormalToWarn, NormalToCritical, NormalToShutdown, WarnToCritical, WarnBackToNormal (hysteresis), CriticalBackToWarn (hysteresis), ShutdownLatches, ResetFromShutdown, StateNameStrings |
| `PowerMonitorTest` | 5 | EmitsTelemetry, PayloadContainsCpuTemp, OutletCountIs8, OutletWattsPositive, FactoryCreatesPowerMonitor |

### Python — pytest (4 tests)

```bash
uv run pytest pdu/tests/ -v
```

| Test | What it validates |
|---|---|
| `test_init_creates_tables` | Schema: outlet_metrics + system_metrics created |
| `test_run_once_inserts_outlets` | All 8 outlets written per poll |
| `test_run_once_inserts_system_metrics` | thermal_state + cpu_temp_c persisted |
| `test_total_watts_calculated` | Σ(V×I) correct to ±0.1 W |
