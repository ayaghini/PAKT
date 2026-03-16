# telemetry.py – APRS TNC telemetry parsers and display formatters (APP-004)
#
# Parses the four telemetry JSON payloads produced by the firmware and provides
# human-readable single-line summaries for the interactive CLI.
#
# Wire format (all characteristics, UTF-8 JSON):
#   device_status : {"radio":"idle","bonded":bool,"gps_fix":bool,
#                    "pending_tx":n,"rx_queue":n,"uptime_s":n}
#   gps_telem     : {"lat":f,"lon":f,"alt_m":f,"speed_kmh":f,"course":f,
#                    "sats":n,"fix":n,"ts":n}
#   power_telem   : {"batt_v":f,"batt_pct":n,"tx_dbm":f,"vswr":f,"temp_c":f}
#   system_telem  : {"free_heap":n,"min_heap":n,"cpu_pct":n,
#                    "tx_pkts":n,"rx_pkts":n,"tx_errs":n,"rx_errs":n,"uptime_s":n}

from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Optional


# ── Parsed dataclasses ────────────────────────────────────────────────────────

@dataclass
class DeviceStatus:
    radio:      str  = "unknown"
    bonded:     bool = False
    gps_fix:    bool = False
    pending_tx: int  = 0
    rx_queue:   int  = 0
    uptime_s:   int  = 0
    received_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))

    @classmethod
    def parse(cls, json_str: str) -> Optional["DeviceStatus"]:
        try:
            d = json.loads(json_str)
            return cls(
                radio      = str(d.get("radio", "unknown")),
                bonded     = bool(d.get("bonded", False)),
                gps_fix    = bool(d.get("gps_fix", False)),
                pending_tx = int(d.get("pending_tx", 0)),
                rx_queue   = int(d.get("rx_queue", 0)),
                uptime_s   = int(d.get("uptime_s", 0)),
            )
        except (json.JSONDecodeError, TypeError, ValueError):
            return None

    def summary(self) -> str:
        fix   = "fix" if self.gps_fix else "no-fix"
        bond  = "bonded" if self.bonded else "open"
        up    = _fmt_uptime(self.uptime_s)
        return (f"radio={self.radio:<5}  {bond:<6}  GPS:{fix:<6}  "
                f"pending_tx={self.pending_tx}  rx_q={self.rx_queue}  up={up}")


@dataclass
class GpsTelem:
    lat_deg:    float = 0.0
    lon_deg:    float = 0.0
    alt_m:      float = 0.0
    speed_kmh:  float = 0.0
    course_deg: float = 0.0
    sats:       int   = 0
    fix:        int   = 0
    timestamp_s: int  = 0
    received_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))

    @classmethod
    def parse(cls, json_str: str) -> Optional["GpsTelem"]:
        try:
            d = json.loads(json_str)
            return cls(
                lat_deg    = float(d.get("lat", 0.0)),
                lon_deg    = float(d.get("lon", 0.0)),
                alt_m      = float(d.get("alt_m", 0.0)),
                speed_kmh  = float(d.get("speed_kmh", 0.0)),
                course_deg = float(d.get("course", 0.0)),
                sats       = int(d.get("sats", 0)),
                fix        = int(d.get("fix", 0)),
                timestamp_s = int(d.get("ts", 0)),
            )
        except (json.JSONDecodeError, TypeError, ValueError):
            return None

    def summary(self) -> str:
        fix_str = ["no-fix", "GPS", "DGPS"][min(self.fix, 2)]
        return (f"{self.lat_deg:+.5f}°  {self.lon_deg:+.5f}°  "
                f"alt={self.alt_m:.0f}m  spd={self.speed_kmh:.1f}km/h  "
                f"crs={self.course_deg:.0f}°  sats={self.sats}  {fix_str}")


@dataclass
class PowerTelem:
    batt_v:    float = 0.0
    batt_pct:  int   = 0
    tx_dbm:    float = 0.0
    vswr:      float = 0.0
    temp_c:    float = 0.0
    received_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))

    @classmethod
    def parse(cls, json_str: str) -> Optional["PowerTelem"]:
        try:
            d = json.loads(json_str)
            return cls(
                batt_v   = float(d.get("batt_v", 0.0)),
                batt_pct = int(d.get("batt_pct", 0)),
                tx_dbm   = float(d.get("tx_dbm", 0.0)),
                vswr     = float(d.get("vswr", 0.0)),
                temp_c   = float(d.get("temp_c", 0.0)),
            )
        except (json.JSONDecodeError, TypeError, ValueError):
            return None

    def summary(self) -> str:
        vswr_str = f"{self.vswr:.2f}" if self.vswr > 0 else "n/a"
        return (f"batt={self.batt_v:.3f}V ({self.batt_pct}%)  "
                f"tx={self.tx_dbm:.1f}dBm  VSWR={vswr_str}  "
                f"temp={self.temp_c:.1f}°C")


@dataclass
class SysTelem:
    free_heap:  int = 0
    min_heap:   int = 0
    cpu_pct:    int = 0
    tx_pkts:    int = 0
    rx_pkts:    int = 0
    tx_errs:    int = 0
    rx_errs:    int = 0
    uptime_s:   int = 0
    received_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))

    @classmethod
    def parse(cls, json_str: str) -> Optional["SysTelem"]:
        try:
            d = json.loads(json_str)
            if not isinstance(d, dict):
                return None
            return cls(
                free_heap = int(d.get("free_heap", 0)),
                min_heap  = int(d.get("min_heap", 0)),
                cpu_pct   = int(d.get("cpu_pct", 0)),
                tx_pkts   = int(d.get("tx_pkts", 0)),
                rx_pkts   = int(d.get("rx_pkts", 0)),
                tx_errs   = int(d.get("tx_errs", 0)),
                rx_errs   = int(d.get("rx_errs", 0)),
                uptime_s  = int(d.get("uptime_s", 0)),
            )
        except (json.JSONDecodeError, TypeError, ValueError):
            return None

    def summary(self) -> str:
        up = _fmt_uptime(self.uptime_s)
        return (f"heap={self.free_heap//1024}KB (min {self.min_heap//1024}KB)  "
                f"cpu={self.cpu_pct}%  "
                f"tx={self.tx_pkts}({self.tx_errs}err)  "
                f"rx={self.rx_pkts}({self.rx_errs}err)  up={up}")


# ── Dispatcher ────────────────────────────────────────────────────────────────

# Maps characteristic name → parser
_PARSERS = {
    "device_status": DeviceStatus.parse,
    "gps_telem":     GpsTelem.parse,
    "power_telem":   PowerTelem.parse,
    "system_telem":  SysTelem.parse,
}

def parse_notify(name: str, json_str: str):
    """Parse a telemetry notification by characteristic name.

    Returns a typed dataclass instance or None if parsing fails or the name
    is not a known telemetry characteristic.
    """
    parser = _PARSERS.get(name)
    if parser is None:
        return None
    return parser(json_str)


# ── Internal ──────────────────────────────────────────────────────────────────

def _fmt_uptime(seconds: int) -> str:
    h, rem = divmod(seconds, 3600)
    m, s   = divmod(rem, 60)
    if h:
        return f"{h}h{m:02d}m{s:02d}s"
    if m:
        return f"{m}m{s:02d}s"
    return f"{s}s"
