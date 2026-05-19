"""
PDU Aggregator — polls /api/power from the C++ bridge every 30s,
computes per-outlet P=V×I, logs to SQLite via aiosqlite.
"""
from __future__ import annotations

import asyncio
import json
import os
import time
from pathlib import Path

import aiosqlite
import httpx

BRIDGE_URL = os.environ.get("BRIDGE_URL", "http://localhost:8080")
POLL_INTERVAL = 30.0
DB_PATH = Path(os.environ.get("PDU_DB", "pdu/pdu_metrics.db"))


async def init_db(db: aiosqlite.Connection) -> None:
    await db.executescript("""
        CREATE TABLE IF NOT EXISTS outlet_metrics (
            ts          INTEGER NOT NULL,
            outlet_id   INTEGER NOT NULL,
            voltage_v   REAL,
            current_a   REAL,
            watts       REAL,
            active      INTEGER,
            PRIMARY KEY (ts, outlet_id)
        );
        CREATE TABLE IF NOT EXISTS system_metrics (
            ts            INTEGER PRIMARY KEY,
            cpu_temp_c    REAL,
            mem_used_pct  INTEGER,
            thermal_state TEXT,
            total_watts   REAL
        );
    """)
    await db.commit()


async def run_once(db: aiosqlite.Connection) -> None:
    async with httpx.AsyncClient(timeout=5.0) as client:
        r = await client.get(f"{BRIDGE_URL}/api/power")
        r.raise_for_status()
        data = r.json()

    ts = int(time.time())
    outlets = data.get("outlets", [])
    total_watts = sum(o.get("watts", 0.0) for o in outlets if o.get("active"))

    # Log per-outlet metrics
    rows = [
        (ts, o["id"], o["voltage_v"], o["current_a"], o["watts"], int(o["active"]))
        for o in outlets
    ]
    await db.executemany(
        "INSERT OR REPLACE INTO outlet_metrics VALUES (?,?,?,?,?,?)", rows
    )

    # Log system metrics
    await db.execute(
        "INSERT OR REPLACE INTO system_metrics VALUES (?,?,?,?,?)",
        (ts, data.get("cpu_temp_c"), data.get("mem_used_pct"),
         data.get("thermal_state"), total_watts),
    )
    await db.commit()
    print(f"[pdu] ts={ts} temp={data.get('cpu_temp_c'):.1f}°C "
          f"total={total_watts:.1f}W thermal={data.get('thermal_state')}")


async def main() -> None:
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    async with aiosqlite.connect(DB_PATH) as db:
        await init_db(db)
        print(f"[pdu] aggregator started — polling {BRIDGE_URL}/api/power every {POLL_INTERVAL}s")
        while True:
            try:
                await run_once(db)
            except Exception as e:
                print(f"[pdu] poll error: {e}")
            await asyncio.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    asyncio.run(main())
