"""Tests for the PDU Python aggregator."""
import pytest
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

import aiosqlite

PDU_RESPONSE = {
    "cpu_temp_c": 55.3,
    "mem_used_pct": 42,
    "thermal_state": "NORMAL",
    "outlets": [
        {"id": i, "voltage_v": 120.1, "current_a": 1.5,
         "watts": 180.15, "active": True}
        for i in range(8)
    ],
}


def _make_mock_client(response_data):
    """Build an AsyncClient mock that returns response_data from GET."""
    mock_resp = MagicMock()           # sync MagicMock — json() is not a coroutine
    mock_resp.json.return_value = response_data
    mock_resp.raise_for_status = MagicMock()

    mock_get = AsyncMock(return_value=mock_resp)
    mock_session = MagicMock()
    mock_session.get = mock_get

    mock_client = MagicMock()
    mock_client.__aenter__ = AsyncMock(return_value=mock_session)
    mock_client.__aexit__ = AsyncMock(return_value=False)
    return mock_client


@pytest.fixture
async def db(tmp_path):
    from pdu.aggregator import init_db
    async with aiosqlite.connect(tmp_path / "test.db") as conn:
        await init_db(conn)
        yield conn


@pytest.mark.asyncio
async def test_init_creates_tables(tmp_path):
    from pdu.aggregator import init_db
    async with aiosqlite.connect(tmp_path / "test.db") as conn:
        await init_db(conn)
        async with conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table'"
        ) as cur:
            tables = {row[0] async for row in cur}
    assert "outlet_metrics" in tables
    assert "system_metrics" in tables


@pytest.mark.asyncio
async def test_run_once_inserts_outlets(db):
    from pdu.aggregator import run_once
    with patch("httpx.AsyncClient", return_value=_make_mock_client(PDU_RESPONSE)):
        await run_once(db)

    async with db.execute("SELECT COUNT(*) FROM outlet_metrics") as cur:
        row = await cur.fetchone()
    assert row[0] == 8


@pytest.mark.asyncio
async def test_run_once_inserts_system_metrics(db):
    from pdu.aggregator import run_once
    with patch("httpx.AsyncClient", return_value=_make_mock_client(PDU_RESPONSE)):
        await run_once(db)

    async with db.execute("SELECT thermal_state, cpu_temp_c FROM system_metrics") as cur:
        row = await cur.fetchone()
    assert row[0] == "NORMAL"
    assert abs(row[1] - 55.3) < 0.01


@pytest.mark.asyncio
async def test_total_watts_calculated(db):
    from pdu.aggregator import run_once
    with patch("httpx.AsyncClient", return_value=_make_mock_client(PDU_RESPONSE)):
        await run_once(db)

    async with db.execute("SELECT total_watts FROM system_metrics") as cur:
        row = await cur.fetchone()
    expected = 8 * 180.15
    assert abs(row[0] - expected) < 0.1
