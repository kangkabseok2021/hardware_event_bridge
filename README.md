# Hardware Event Bridge

A Linux C++17 daemon that bridges hardware peripherals to a networked system — reading sensor telemetry from simulated serial and I2C devices, dispatching events through an Observer pipeline, exposing real-time device state via a REST API, and logging asynchronously to SQLite. Runs as a hardened systemd service with a full CI/CD pipeline.

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
└─────────────────────────────────────────────────────────────────┘
```

---

## Design Patterns

### Observer (EventEngine)
Hardware drivers call `engine.emit(event)`. The engine dispatches to all registered listeners without coupling the drivers to the DeviceManager or SqliteLogger. Adding a new listener (e.g. Prometheus metrics) requires one `engine.subscribe(...)` call.

### Factory (DeviceFactory)
```cpp
DeviceFactory::create("serial", "serial-0", engine)
DeviceFactory::create("i2c",    "i2c-0",    engine)
```
New hardware types (USB, SPI) add a subclass + one `if` branch — the event loop and REST API are untouched.

### Dependency Injection
`main.cpp` is the only file that knows all subsystems. All others accept their dependencies by reference. This enables the test suite to inject stub engines and in-memory databases without mocking globals.

---

## Project Structure

```
hardware_event_bridge/
├── src/
│   ├── Event.h               Event type + DeviceId alias
│   ├── EventEngine.h         Observer dispatch (thread-safe)
│   ├── DeviceState.h         State enum + DeviceState struct
│   ├── DeviceManager.h/cpp   Registry (unordered_map + shared_mutex)
│   ├── SqliteLogger.h/cpp    Async writer thread + batch INSERT
│   ├── RestApi.h/cpp         httplib REST server (background thread)
│   ├── main.cpp              DI wiring — only file that knows everything
│   └── devices/
│       ├── IDevice.h         Abstract device interface
│       ├── SerialDevice.h    UART sensor simulation
│       ├── I2cDevice.h       I2C sensor simulation
│       └── DeviceFactory.h   Factory pattern
├── tests/
│   ├── test_event_engine.cpp   5 tests
│   ├── test_device_manager.cpp 6 tests
│   ├── test_sqlite_logger.cpp  4 tests
│   └── test_device_factory.cpp 4 tests
├── systemd/
│   └── hardware-event-bridge.service
├── scripts/
│   ├── setup.sh              Build + install service
│   └── deploy.sh             Zero-downtime binary swap
├── third_party/
│   ├── httplib.h             cpp-httplib (single header)
│   └── json.hpp              nlohmann/json (single header)
└── .github/workflows/ci.yml  Build → test → .deb package
```

---

## Quick Start

### Build

```bash
# Dependencies (Ubuntu/Debian)
sudo apt-get install -y build-essential cmake libsqlite3-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run

```bash
./build/hardware_event_bridge
# [bridge] Running — REST API on :8080
# [bridge] Endpoints: GET /devices  /devices/{id}  /events  /health
```

### Query the REST API

```bash
# All device states
curl http://localhost:8080/devices

# Single device
curl http://localhost:8080/devices/serial-0

# Recent events
curl "http://localhost:8080/events?limit=10"

# Health check
curl http://localhost:8080/health
```

**Example `/devices` response:**
```json
[
  {
    "id": "serial-0",
    "type": "serial",
    "status": "connected",
    "last_payload": "{\"temp_c\":22.41,\"voltage\":4.98}",
    "last_seen_ms": 1234567890
  },
  {
    "id": "i2c-0",
    "type": "i2c",
    "status": "connected",
    "last_payload": "{\"humidity_pct\":51.3,\"pressure_hpa\":1014.2}",
    "last_seen_ms": 1234567891
  }
]
```

---

## Testing

```bash
ctest --test-dir build --output-on-failure -V
```

**19/19 GoogleTests across 4 suites:**

| Suite | Tests | What's covered |
|---|---|---|
| `EventEngineTest` | 5 | Subscribe, multi-listener dispatch, listener count, payload preservation, concurrent emit from 10 threads |
| `DeviceManagerTest` | 6 | Register/get, unknown device returns nullopt, telemetry updates payload + status, error sets ERROR status, getAllDevices count, STATE_CHANGE connects |
| `SqliteLoggerTest` | 4 | Schema creation, log + retrieve (via writer thread), flush writes immediately, multiple events ordered |
| `DeviceFactoryTest` | 4 | Creates serial, creates I2C, unknown type throws `std::invalid_argument`, serial device emits events after start |

---

## systemd Service

**Install:**
```bash
sudo bash scripts/setup.sh
sudo systemctl enable --now hardware-event-bridge
```

**Unit file highlights:**

| Setting | Value | Why |
|---|---|---|
| `Type=simple` | — | Process doesn't fork |
| `Restart=on-failure` | — | Automatic recovery |
| `WatchdogSec=30s` | — | systemd kills and restarts if hung |
| `NoNewPrivileges=true` | — | Prevents privilege escalation |
| `ProtectSystem=strict` | — | Read-only filesystem except `ReadWritePaths` |
| `PrivateTmp=true` | — | Isolated /tmp namespace |

**Deploy updated binary:**
```bash
sudo bash scripts/deploy.sh
```

---

## CI/CD Pipeline

Two jobs on every push to `main`:

```
build-and-test
  ├── apt-get install libsqlite3-dev
  ├── cmake -B build -DCMAKE_BUILD_TYPE=Release
  ├── cmake --build build
  ├── ctest --output-on-failure   (19 tests)
  └── upload binary artifact

package (needs: build-and-test)
  ├── build release binary
  ├── create Debian package structure
  │     /opt/hardware-event-bridge/hardware_event_bridge
  │     /lib/systemd/system/hardware-event-bridge.service
  ├── dpkg-deb --build
  └── upload .deb artifact
```

---

## Hardware Integration

On real embedded hardware, swap the simulated drivers for real implementations:

| Simulated | Real implementation |
|---|---|
| `SerialDevice` (timer loop) | `termios2` — open `/dev/ttyACM0`, set baud rate, read in epoll loop |
| `I2cDevice` (timer loop) | `ioctl(fd, I2C_SLAVE, 0x76)` + `read()`/`write()` in epoll loop |
| Thread-per-device | Single `epoll_create1` fd monitoring all device fds + `timerfd` + `signalfd` |

Only `main.cpp` and the concrete device classes change — the EventEngine, DeviceManager, SqliteLogger, RestApi, and all 19 tests remain identical.
