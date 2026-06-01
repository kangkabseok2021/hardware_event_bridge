# Hardware Event Bridge, PDU Power Monitor & PKV Claims Engine

Three C++ systems in one repository — sharing an httplib REST pattern, CMake FetchContent, and GitHub Actions CI.

| Project | Description | Docs |
|---|---|---|
| **Hardware Event Bridge** | C++17 Linux daemon bridging serial/I2C sensors to a REST API via an Observer + Factory pipeline | [docs/event-bridge.md](docs/event-bridge.md) |
| **PDU Power Monitor** | ThermalFsm + 8-outlet PDU simulator reading `/sys`+`/proc`; Python asyncio aggregator logs P=V×I to SQLite | [docs/pdu.md](docs/pdu.md) |
| **PKV Claims Engine** | C++20 private-health-insurance claims backend — pure `ClaimValidator` + `ReimbursementCalculator`, libpqxx 7.9 repositories, cpp-httplib REST layer, Docker Compose + PostgreSQL 16 | [pkv_claims_engine/](pkv_claims_engine/) |

---

## Repository Layout

```
hardware_event_bridge/
├── src/
│   ├── EventEngine.h/cpp      Observer pattern — thread-safe emit/subscribe
│   ├── DeviceManager.h/cpp    O(1) device registry (unordered_map + shared_mutex)
│   ├── SqliteLogger.h/cpp     Async batch-INSERT writer thread
│   ├── RestApi.h/cpp          httplib REST server (background thread)
│   ├── main.cpp               DI wiring — registers all devices + starts services
│   └── devices/
│       ├── IDevice.h          Abstract device interface
│       ├── SerialDevice.h     UART sensor simulation (temp + voltage)
│       ├── I2cDevice.h        I2C sensor simulation (humidity + pressure)
│       ├── ThermalFsm.h       4-state thermal FSM (NORMAL→WARN→CRITICAL→SHUTDOWN)
│       ├── PowerMonitorDevice.h 8-outlet PDU + /sys//proc reader
│       └── DeviceFactory.h    Factory pattern — one line per device type
├── pdu/
│   ├── aggregator.py          Python asyncio poller → aiosqlite (P=V×I per outlet)
│   └── tests/                 4 pytest tests
├── tests/                     34 GoogleTests (19 bridge + 10 ThermalFsm + 5 PDU)
├── systemd/
│   ├── hardware-event-bridge.service
│   └── hardware-pdu.service
├── scripts/
│   ├── setup.sh               Build + install bridge service
│   ├── deploy.sh              Zero-downtime binary swap
│   └── pdu_healthcheck.sh     curl /api/power + JSON validation
├── docs/
│   ├── event-bridge.md        Bridge system documentation
│   └── pdu.md                 PDU system documentation
└── .github/workflows/ci.yml   C++ build+test + Python pytest
```

---

## Quick Start

```bash
# Build C++ (both bridge + PDU device)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run bridge daemon (serves serial, I2C, and PDU devices)
./build/hardware_event_bridge
# → REST API on :8080

# Run PDU Python aggregator (separate terminal)
uv sync
uv run python pdu/aggregator.py
```

---

## CI

| Job | Tests |
|---|---|
| `cpp-build-test` | cmake build + 34 GoogleTests |
| `python-tests` | 4 pytest (PDU aggregator) |
