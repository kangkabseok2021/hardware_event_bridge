# Hardware Event Bridge

A Linux C++17 daemon that bridges hardware peripherals (serial, I2C) to a networked system. Events from hardware drivers flow through an Observer pipeline, update a thread-safe device registry, and are logged asynchronously to SQLite. A REST API exposes real-time device state. Runs as a hardened systemd service.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Hardware Layer (simulated)                                     │
│  SerialDevice (/dev/ttyACM0)   I2cDevice (/dev/i2c-1 addr 0x76)│
│  temp + voltage @ 200ms        humidity + pressure @ 500ms      │
└──────────────────────┬──────────────────────────────────────────┘
                       │  emit(Event)
┌──────────────────────▼──────────────────────────────────────────┐
│  EventEngine  — Observer pattern                                │
│  subscribers: DeviceManager::onEvent, SqliteLogger::logEvent    │
└──────────────┬───────────────────────────────┬──────────────────┘
               │                               │
┌──────────────▼──────────┐     ┌──────────────▼──────────────────┐
│  DeviceManager          │     │  SqliteLogger                   │
│  unordered_map          │     │  lock-free queue → writer thread │
│  shared_mutex reads     │     │  batch INSERT every 100ms        │
│  O(1) lookup            │     │  events.db (SQLite WAL)          │
└──────────────┬──────────┘     └─────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────────┐
│  RestApi  — httplib                                             │
│  GET /devices   GET /devices/{id}   GET /events   GET /health   │
│  GET /api/power  (PDU telemetry)                                │
└─────────────────────────────────────────────────────────────────┘
```

---

## Design Patterns

### Observer (EventEngine)
Hardware drivers call `engine.emit(event)`. All registered listeners (DeviceManager, SqliteLogger, future Prometheus exporter) receive events without coupling.

### Factory (DeviceFactory)
```cpp
DeviceFactory::create("serial",        "serial-0",       engine)
DeviceFactory::create("i2c",           "i2c-0",          engine)
DeviceFactory::create("power_monitor", "power-monitor-0", engine)
```
Adding a new hardware type: one subclass + one `if` line in the factory.

### Dependency Injection
`main.cpp` is the only file that knows all subsystems. Every other class accepts its dependencies by reference — fully testable without mocking globals.

---

## Build & Run

```bash
# Dependencies (Ubuntu)
sudo apt-get install -y build-essential cmake libsqlite3-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/hardware_event_bridge
```

### REST API

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/devices` | All device states |
| `GET` | `/devices/{id}` | Single device |
| `GET` | `/events?limit=N` | Recent event log |
| `GET` | `/api/power` | PDU telemetry (outlets + thermal FSM) |
| `GET` | `/health` | Liveness probe |

**Example `/devices` response:**
```json
[
  {"id": "serial-0", "type": "serial", "status": "connected",
   "last_payload": "{\"temp_c\":22.4,\"voltage\":4.98}", "last_seen_ms": 1234567890}
]
```

---

## systemd Service

```bash
sudo bash scripts/setup.sh
sudo systemctl enable --now hardware-event-bridge
```

| Setting | Value |
|---|---|
| `Restart=on-failure` | Auto-recovery |
| `WatchdogSec=30s` | systemd kills + restarts if hung |
| `NoNewPrivileges=true` | Prevent privilege escalation |
| `ProtectSystem=strict` | Read-only filesystem |

---

## Testing (19 GoogleTests)

```bash
ctest --test-dir build --output-on-failure -V
```

| Suite | Tests |
|---|---|
| `EventEngineTest` | Subscribe, multi-listener, listener count, payload, concurrent emit |
| `DeviceManagerTest` | Register/get, unknown device nullopt, telemetry updates, error status, getAllDevices, STATE_CHANGE |
| `SqliteLoggerTest` | Schema creation, log+retrieve, flush, multiple events ordered |
| `DeviceFactoryTest` | Serial, I2C, unknown type throws, serial emits on start |
