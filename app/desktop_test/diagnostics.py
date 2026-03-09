# diagnostics.py – Session diagnostics store and exporter (APP-005)
#
# Collects parsed telemetry samples during a session, computes running
# statistics, and exports a structured JSON diagnostics artifact.
#
# Usage:
#   store = DiagnosticsStore()
#   store.ingest("power_telem", power_obj)
#   report = store.export_dict()
#   store.export_json(path)

from __future__ import annotations

import json
from collections import deque
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

from telemetry import DeviceStatus, GpsTelem, PowerTelem, SysTelem


class DiagnosticsStore:
    """Accumulate telemetry samples and produce a session diagnostics report.

    Keeps the last kMaxSamples entries per channel to bound memory use.
    Statistics (min/max/avg) are computed lazily at export time.
    """

    kMaxSamples = 300   # ~5 minutes at 1 Hz per channel

    def __init__(self) -> None:
        self._status: deque[DeviceStatus] = deque(maxlen=self.kMaxSamples)
        self._gps:    deque[GpsTelem]     = deque(maxlen=self.kMaxSamples)
        self._power:  deque[PowerTelem]   = deque(maxlen=self.kMaxSamples)
        self._sys:    deque[SysTelem]     = deque(maxlen=self.kMaxSamples)
        self._rx_frames: list[str]        = []   # decoded APRS frames (raw text)
        self._session_start = datetime.now(timezone.utc)

    # ── Ingestion ─────────────────────────────────────────────────────────────

    def ingest(self, name: str, obj: Any) -> None:
        """Accept a parsed telemetry object and store it."""
        if isinstance(obj, DeviceStatus):
            self._status.append(obj)
        elif isinstance(obj, GpsTelem):
            self._gps.append(obj)
        elif isinstance(obj, PowerTelem):
            self._power.append(obj)
        elif isinstance(obj, SysTelem):
            self._sys.append(obj)

    def add_rx_frame(self, frame_text: str) -> None:
        """Record a decoded APRS RX frame string."""
        self._rx_frames.append(frame_text)

    # ── Inspection ────────────────────────────────────────────────────────────

    @property
    def latest_status(self) -> Optional[DeviceStatus]:
        return self._status[-1] if self._status else None

    @property
    def latest_gps(self) -> Optional[GpsTelem]:
        return self._gps[-1] if self._gps else None

    @property
    def latest_power(self) -> Optional[PowerTelem]:
        return self._power[-1] if self._power else None

    @property
    def latest_sys(self) -> Optional[SysTelem]:
        return self._sys[-1] if self._sys else None

    @property
    def rx_frame_count(self) -> int:
        return len(self._rx_frames)

    # ── Export ────────────────────────────────────────────────────────────────

    def export_dict(self) -> dict:
        """Build the full diagnostics report as a Python dict."""
        now = datetime.now(timezone.utc)
        duration_s = int((now - self._session_start).total_seconds())

        report: dict = {
            "session_start_utc": self._session_start.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z",
            "export_utc":        now.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z",
            "duration_s":        duration_s,
            "rx_frames":         list(self._rx_frames),
        }

        # ── device_status summary ─────────────────────────────────────────────
        if self._status:
            last = self._status[-1]
            report["device_status"] = {
                "samples":    len(self._status),
                "last_radio": last.radio,
                "last_bonded": last.bonded,
                "last_gps_fix": last.gps_fix,
                "last_uptime_s": last.uptime_s,
            }

        # ── gps summary ───────────────────────────────────────────────────────
        if self._gps:
            lats  = [g.lat_deg    for g in self._gps if g.fix > 0]
            lons  = [g.lon_deg    for g in self._gps if g.fix > 0]
            alts  = [g.alt_m      for g in self._gps if g.fix > 0]
            last  = self._gps[-1]
            report["gps"] = {
                "samples":   len(self._gps),
                "fix_count": len(lats),
                "last_lat":  last.lat_deg,
                "last_lon":  last.lon_deg,
                "last_alt_m": last.alt_m,
                "last_sats": last.sats,
            }
            if lats:
                report["gps"]["lat_min"] = min(lats)
                report["gps"]["lat_max"] = max(lats)
                report["gps"]["alt_min_m"] = min(alts)
                report["gps"]["alt_max_m"] = max(alts)

        # ── power summary ─────────────────────────────────────────────────────
        if self._power:
            volts  = [p.batt_v   for p in self._power]
            pcts   = [p.batt_pct for p in self._power]
            temps  = [p.temp_c   for p in self._power]
            last   = self._power[-1]
            report["power"] = {
                "samples":      len(self._power),
                "batt_v_min":   round(min(volts), 3),
                "batt_v_max":   round(max(volts), 3),
                "batt_v_avg":   round(sum(volts) / len(volts), 3),
                "batt_pct_min": min(pcts),
                "batt_pct_max": max(pcts),
                "temp_c_min":   round(min(temps), 1),
                "temp_c_max":   round(max(temps), 1),
                "last_batt_v":  last.batt_v,
                "last_batt_pct": last.batt_pct,
                "last_tx_dbm":  last.tx_dbm,
                "last_vswr":    last.vswr,
            }

        # ── system summary ────────────────────────────────────────────────────
        if self._sys:
            heaps = [s.free_heap  for s in self._sys]
            cpus  = [s.cpu_pct    for s in self._sys]
            last  = self._sys[-1]
            report["system"] = {
                "samples":        len(self._sys),
                "heap_min_bytes": min(heaps),
                "heap_max_bytes": max(heaps),
                "cpu_pct_avg":    round(sum(cpus) / len(cpus), 1),
                "cpu_pct_max":    max(cpus),
                "last_tx_pkts":   last.tx_pkts,
                "last_rx_pkts":   last.rx_pkts,
                "last_tx_errs":   last.tx_errs,
                "last_rx_errs":   last.rx_errs,
                "last_uptime_s":  last.uptime_s,
                "last_min_heap":  last.min_heap,
            }

        return report

    def export_json(self, path: Path) -> None:
        """Write the diagnostics report to a JSON file."""
        path.write_text(json.dumps(self.export_dict(), indent=2) + "\n",
                        encoding="utf-8")
