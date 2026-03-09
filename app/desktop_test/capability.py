# capability.py – Device capability negotiation client (INT-001)
#
# Reads and parses the Device Capabilities characteristic (UUID_DEV_CAPS)
# on connect, then exposes feature flags to the rest of the app so operations
# can degrade gracefully when connecting to older firmware.
#
# Wire format: read-only UTF-8 JSON, no chunking required.
# Example: {"fw_ver":"0.1.0","hw_rev":"EVT-A","protocol":1,
#            "features":["aprs_2m","ble_chunking","telemetry","msg_ack","config_rw"]}
#
# Protocol compatibility:
#   If the characteristic is absent (older firmware), the client falls back to
#   ASSUMED_CAPABILITIES (protocol=1, all MVP features) with a logged warning.
#
# Thread safety: not thread-safe; call from asyncio task only.

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Optional


# ── Known feature names (mirrors firmware Feature enum) ───────────────────────

class Feature:
    APRS_2M      = "aprs_2m"
    BLE_CHUNKING = "ble_chunking"
    TELEMETRY    = "telemetry"
    MSG_ACK      = "msg_ack"
    CONFIG_RW    = "config_rw"
    GPS_ONBOARD  = "gps_onboard"
    HF_AUDIO     = "hf_audio"

    # All features expected from an MVP device
    MVP_REQUIRED = frozenset([APRS_2M, BLE_CHUNKING, TELEMETRY, MSG_ACK, CONFIG_RW])


# ── Parsed capability record ──────────────────────────────────────────────────

@dataclass
class DeviceCapabilities:
    fw_ver:   str       = "unknown"
    hw_rev:   str       = "unknown"
    protocol: int       = 0
    features: frozenset = field(default_factory=frozenset)
    raw_json: str       = ""
    source:   str       = "read"    # "read" | "assumed" | "error"

    # ── Factory methods ───────────────────────────────────────────────────────

    @classmethod
    def parse(cls, json_str: str) -> "DeviceCapabilities":
        """Parse a capability JSON string from the device.

        Returns a valid DeviceCapabilities on success, or an instance with
        source="error" if parsing fails.
        """
        try:
            d = json.loads(json_str)
            features = frozenset(str(f) for f in d.get("features", []))
            return cls(
                fw_ver   = str(d.get("fw_ver",  "unknown")),
                hw_rev   = str(d.get("hw_rev",  "unknown")),
                protocol = int(d.get("protocol", 0)),
                features = features,
                raw_json = json_str,
                source   = "read",
            )
        except (json.JSONDecodeError, TypeError, ValueError):
            return cls(source="error", raw_json=json_str)

    @classmethod
    def assumed_mvp(cls) -> "DeviceCapabilities":
        """Fallback capability record used when the characteristic is absent.

        Assumes protocol=1 + all MVP features so the client can operate
        against firmware that predates this characteristic.
        """
        return cls(
            fw_ver   = "unknown",
            hw_rev   = "unknown",
            protocol = 1,
            features = Feature.MVP_REQUIRED | frozenset([Feature.GPS_ONBOARD]),
            raw_json = "",
            source   = "assumed",
        )

    # ── Feature queries ───────────────────────────────────────────────────────

    def supports(self, feature: str) -> bool:
        """Return True if the device advertises the named feature."""
        return feature in self.features

    def supports_all(self, *features: str) -> bool:
        return all(f in self.features for f in features)

    def missing_mvp_features(self) -> list[str]:
        """Return MVP feature names not advertised by this device."""
        return [f for f in Feature.MVP_REQUIRED if f not in self.features]

    def is_compatible(self) -> bool:
        """True if protocol == 1 and all MVP features present."""
        return self.protocol == 1 and not self.missing_mvp_features()

    def summary(self) -> str:
        source_tag = f"[{self.source}]" if self.source != "read" else ""
        feat_str = ", ".join(sorted(self.features)) if self.features else "(none)"
        return (f"protocol={self.protocol}  fw={self.fw_ver}  hw={self.hw_rev}"
                f"  features=[{feat_str}]{source_tag}")


# ── Negotiator ────────────────────────────────────────────────────────────────

class CapabilityNegotiator:
    """Manages capability read on connect and exposes the result.

    Usage::

        neg = CapabilityNegotiator(on_caps=my_callback)
        # After BLE connect:
        caps = await neg.read(bleak_client)
        if not caps.supports(Feature.MSG_ACK):
            print("This firmware does not support message ACK")
    """

    # UUID of the Device Capabilities characteristic (canonical form)
    UUID = "544e4332-8a48-4328-9844-3f5ca0040000"

    def __init__(self, on_caps=None) -> None:
        self._caps: DeviceCapabilities = DeviceCapabilities.assumed_mvp()
        self._on_caps = on_caps

    @property
    def caps(self) -> DeviceCapabilities:
        return self._caps

    async def read(self, client) -> DeviceCapabilities:
        """Read and parse the Device Capabilities characteristic.

        Falls back to assumed_mvp() if the characteristic is absent or
        returns invalid JSON, and logs a warning via on_caps callback.
        """
        try:
            raw_bytes = await client.read_gatt_char(self.UUID)
            json_str  = raw_bytes.decode("utf-8", errors="replace")
            caps      = DeviceCapabilities.parse(json_str)
            if caps.source == "error":
                caps = DeviceCapabilities.assumed_mvp()
                caps.source = "error"
        except Exception:
            caps = DeviceCapabilities.assumed_mvp()

        self._caps = caps
        if self._on_caps:
            self._on_caps(caps)
        return caps

    def reset(self) -> None:
        """Reset to assumed MVP caps (called on disconnect)."""
        self._caps = DeviceCapabilities.assumed_mvp()
