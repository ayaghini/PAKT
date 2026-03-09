# test_telemetry_app.py – pytest tests for telemetry.py and diagnostics.py (APP-004, APP-005)
#
# No BLE adapter required.
#
# Run: pytest test_telemetry_app.py -v --tb=short

import json
import pytest
from pathlib import Path

from telemetry import (
    DeviceStatus, GpsTelem, PowerTelem, SysTelem,
    parse_notify, _fmt_uptime,
)
from diagnostics import DiagnosticsStore


# ── Helpers ───────────────────────────────────────────────────────────────────

def _status(**kw) -> str:
    defaults = {"radio":"idle","bonded":False,"gps_fix":False,
                "pending_tx":0,"rx_queue":0,"uptime_s":0}
    defaults.update(kw)
    return json.dumps(defaults)

def _gps(**kw) -> str:
    defaults = {"lat":43.6,"lon":-79.4,"alt_m":76.0,"speed_kmh":0.0,
                "course":0.0,"sats":8,"fix":1,"ts":0}
    defaults.update(kw)
    return json.dumps(defaults)

def _power(**kw) -> str:
    defaults = {"batt_v":3.8,"batt_pct":80,"tx_dbm":30.0,"vswr":1.5,"temp_c":25.0}
    defaults.update(kw)
    return json.dumps(defaults)

def _sys(**kw) -> str:
    defaults = {"free_heap":120000,"min_heap":80000,"cpu_pct":10,
                "tx_pkts":5,"rx_pkts":12,"tx_errs":0,"rx_errs":1,"uptime_s":300}
    defaults.update(kw)
    return json.dumps(defaults)


# ── DeviceStatus.parse ────────────────────────────────────────────────────────

class TestDeviceStatusParse:
    def test_parses_idle(self):
        s = DeviceStatus.parse(_status(radio="idle"))
        assert s is not None
        assert s.radio == "idle"

    def test_parses_tx(self):
        s = DeviceStatus.parse(_status(radio="tx", bonded=True))
        assert s.radio == "tx"
        assert s.bonded is True

    def test_gps_fix_parsed(self):
        s = DeviceStatus.parse(_status(gps_fix=True))
        assert s.gps_fix is True

    def test_uptime_parsed(self):
        s = DeviceStatus.parse(_status(uptime_s=3601))
        assert s.uptime_s == 3601

    def test_invalid_json_returns_none(self):
        assert DeviceStatus.parse("NOT JSON") is None

    def test_summary_contains_radio(self):
        s = DeviceStatus.parse(_status(radio="rx"))
        assert "rx" in s.summary()


# ── GpsTelem.parse ────────────────────────────────────────────────────────────

class TestGpsTelemParse:
    def test_parses_coords(self):
        g = GpsTelem.parse(_gps(lat=43.6532, lon=-79.3832))
        assert pytest.approx(g.lat_deg, abs=1e-4) == 43.6532
        assert pytest.approx(g.lon_deg, abs=1e-4) == -79.3832

    def test_parses_sats_and_fix(self):
        g = GpsTelem.parse(_gps(sats=6, fix=1))
        assert g.sats == 6
        assert g.fix == 1

    def test_no_fix(self):
        g = GpsTelem.parse(_gps(fix=0, sats=0))
        assert "no-fix" in g.summary()

    def test_invalid_json_returns_none(self):
        assert GpsTelem.parse("{bad}") is None

    def test_summary_contains_lat(self):
        g = GpsTelem.parse(_gps(lat=43.6532))
        assert "43.65" in g.summary()


# ── PowerTelem.parse ──────────────────────────────────────────────────────────

class TestPowerTelemParse:
    def test_parses_batt(self):
        p = PowerTelem.parse(_power(batt_v=3.72, batt_pct=65))
        assert pytest.approx(p.batt_v, abs=0.001) == 3.72
        assert p.batt_pct == 65

    def test_vswr_zero_shows_na(self):
        p = PowerTelem.parse(_power(vswr=0.0))
        assert "n/a" in p.summary()

    def test_invalid_json_returns_none(self):
        assert PowerTelem.parse("") is None

    def test_summary_contains_temp(self):
        p = PowerTelem.parse(_power(temp_c=42.1))
        assert "42.1" in p.summary()


# ── SysTelem.parse ────────────────────────────────────────────────────────────

class TestSysTelemParse:
    def test_parses_heap(self):
        s = SysTelem.parse(_sys(free_heap=98304))
        assert s.free_heap == 98304

    def test_parses_errors(self):
        s = SysTelem.parse(_sys(tx_errs=2, rx_errs=5))
        assert s.tx_errs == 2
        assert s.rx_errs == 5

    def test_invalid_json_returns_none(self):
        assert SysTelem.parse("null") is None

    def test_summary_contains_cpu(self):
        s = SysTelem.parse(_sys(cpu_pct=42))
        assert "42%" in s.summary()


# ── parse_notify dispatcher ───────────────────────────────────────────────────

class TestParseNotify:
    def test_device_status(self):
        obj = parse_notify("device_status", _status())
        assert isinstance(obj, DeviceStatus)

    def test_gps_telem(self):
        assert isinstance(parse_notify("gps_telem", _gps()), GpsTelem)

    def test_power_telem(self):
        assert isinstance(parse_notify("power_telem", _power()), PowerTelem)

    def test_system_telem(self):
        assert isinstance(parse_notify("system_telem", _sys()), SysTelem)

    def test_unknown_name_returns_none(self):
        assert parse_notify("rx_packet", "{}") is None

    def test_bad_json_returns_none(self):
        assert parse_notify("device_status", "!!") is None


# ── _fmt_uptime ───────────────────────────────────────────────────────────────

class TestFmtUptime:
    def test_seconds_only(self):
        assert _fmt_uptime(45) == "45s"

    def test_minutes_and_seconds(self):
        assert _fmt_uptime(125) == "2m05s"

    def test_hours(self):
        assert _fmt_uptime(3661) == "1h01m01s"

    def test_zero(self):
        assert _fmt_uptime(0) == "0s"


# ── DiagnosticsStore ──────────────────────────────────────────────────────────

class TestDiagnosticsStore:
    def _store_with_samples(self):
        ds = DiagnosticsStore()
        ds.ingest("device_status", DeviceStatus.parse(_status(radio="tx", bonded=True, uptime_s=300)))
        ds.ingest("gps_telem",     GpsTelem.parse(_gps(lat=43.6, lon=-79.4, sats=8, fix=1)))
        ds.ingest("power_telem",   PowerTelem.parse(_power(batt_v=3.8, batt_pct=80, temp_c=30.0)))
        ds.ingest("system_telem",  SysTelem.parse(_sys(free_heap=100000, cpu_pct=15)))
        return ds

    def test_latest_status(self):
        ds = self._store_with_samples()
        assert ds.latest_status is not None
        assert ds.latest_status.radio == "tx"

    def test_latest_gps(self):
        ds = self._store_with_samples()
        assert ds.latest_gps is not None
        assert pytest.approx(ds.latest_gps.lat_deg, abs=0.01) == 43.6

    def test_latest_power(self):
        ds = self._store_with_samples()
        assert ds.latest_power.batt_pct == 80

    def test_latest_sys(self):
        ds = self._store_with_samples()
        assert ds.latest_sys.cpu_pct == 15

    def test_export_dict_has_required_keys(self):
        ds = self._store_with_samples()
        report = ds.export_dict()
        assert "session_start_utc" in report
        assert "export_utc"        in report
        assert "duration_s"        in report

    def test_export_dict_power_stats(self):
        ds = DiagnosticsStore()
        for v in [3.7, 3.8, 3.9]:
            ds.ingest("power_telem", PowerTelem.parse(_power(batt_v=v, batt_pct=70)))
        r = ds.export_dict()["power"]
        assert r["batt_v_min"] == pytest.approx(3.7, abs=0.001)
        assert r["batt_v_max"] == pytest.approx(3.9, abs=0.001)
        assert r["batt_v_avg"] == pytest.approx(3.8, abs=0.001)

    def test_export_dict_sys_heap_min(self):
        ds = DiagnosticsStore()
        for h in [90000, 80000, 100000]:
            ds.ingest("system_telem", SysTelem.parse(_sys(free_heap=h)))
        r = ds.export_dict()["system"]
        assert r["heap_min_bytes"] == 80000
        assert r["heap_max_bytes"] == 100000

    def test_export_dict_rx_frames(self):
        ds = DiagnosticsStore()
        ds.add_rx_frame("W1AW>APRS:!4000.00N/07400.00W>Hello")
        r = ds.export_dict()
        assert len(r["rx_frames"]) == 1

    def test_export_json_writes_valid_json(self, tmp_path):
        ds = self._store_with_samples()
        path = tmp_path / "diag.json"
        ds.export_json(path)
        loaded = json.loads(path.read_text(encoding="utf-8"))
        assert "session_start_utc" in loaded

    def test_empty_store_export(self):
        ds = DiagnosticsStore()
        report = ds.export_dict()
        assert report["duration_s"] >= 0
        assert "device_status" not in report
        assert "gps" not in report

    def test_ingest_ignores_non_telemetry(self):
        ds = DiagnosticsStore()
        ds.ingest("rx_packet", "some text")   # should not raise
        assert ds.latest_status is None

    def test_max_samples_bound(self):
        ds = DiagnosticsStore()
        for i in range(DiagnosticsStore.kMaxSamples + 50):
            ds.ingest("power_telem", PowerTelem.parse(_power(batt_pct=i % 100)))
        assert len(ds._power) == DiagnosticsStore.kMaxSamples
