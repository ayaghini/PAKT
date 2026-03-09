# test_capability.py – pytest tests for capability.py (INT-001)
#
# No BLE adapter required.
#
# Run: pytest test_capability.py -v --tb=short

import json
import pytest
from unittest.mock import AsyncMock, MagicMock

from capability import DeviceCapabilities, CapabilityNegotiator, Feature


# ── Helpers ───────────────────────────────────────────────────────────────────

def _caps_json(**kw) -> str:
    defaults = {
        "fw_ver": "0.1.0",
        "hw_rev": "EVT-A",
        "protocol": 1,
        "features": list(Feature.MVP_REQUIRED | {Feature.GPS_ONBOARD}),
    }
    defaults.update(kw)
    return json.dumps(defaults)

def run(coro):
    import asyncio
    return asyncio.get_event_loop().run_until_complete(coro)


# ── DeviceCapabilities.parse ──────────────────────────────────────────────────

class TestParse:
    def test_parses_fw_ver(self):
        caps = DeviceCapabilities.parse(_caps_json(fw_ver="0.2.1"))
        assert caps.fw_ver == "0.2.1"

    def test_parses_protocol(self):
        caps = DeviceCapabilities.parse(_caps_json(protocol=1))
        assert caps.protocol == 1

    def test_parses_features_as_frozenset(self):
        caps = DeviceCapabilities.parse(_caps_json())
        assert isinstance(caps.features, frozenset)
        assert Feature.APRS_2M in caps.features

    def test_source_is_read(self):
        caps = DeviceCapabilities.parse(_caps_json())
        assert caps.source == "read"

    def test_invalid_json_returns_error_source(self):
        caps = DeviceCapabilities.parse("NOT JSON")
        assert caps.source == "error"

    def test_empty_features_list(self):
        caps = DeviceCapabilities.parse(_caps_json(features=[]))
        assert len(caps.features) == 0

    def test_extra_unknown_feature_preserved(self):
        caps = DeviceCapabilities.parse(_caps_json(features=["aprs_2m", "future_feature"]))
        assert "future_feature" in caps.features

    def test_raw_json_stored(self):
        raw = _caps_json()
        caps = DeviceCapabilities.parse(raw)
        assert caps.raw_json == raw


# ── DeviceCapabilities.assumed_mvp ────────────────────────────────────────────

class TestAssumedMvp:
    def test_source_is_assumed(self):
        caps = DeviceCapabilities.assumed_mvp()
        assert caps.source == "assumed"

    def test_protocol_is_1(self):
        assert DeviceCapabilities.assumed_mvp().protocol == 1

    def test_has_all_mvp_features(self):
        caps = DeviceCapabilities.assumed_mvp()
        for f in Feature.MVP_REQUIRED:
            assert caps.supports(f), f"Missing feature: {f}"


# ── supports / missing_mvp_features / is_compatible ──────────────────────────

class TestFeatureQueries:
    def test_supports_present_feature(self):
        caps = DeviceCapabilities.parse(_caps_json())
        assert caps.supports(Feature.APRS_2M)

    def test_supports_absent_feature_returns_false(self):
        caps = DeviceCapabilities.parse(_caps_json(features=["aprs_2m"]))
        assert not caps.supports(Feature.HF_AUDIO)

    def test_supports_all_true(self):
        caps = DeviceCapabilities.parse(_caps_json())
        assert caps.supports_all(Feature.APRS_2M, Feature.BLE_CHUNKING)

    def test_supports_all_false_if_one_missing(self):
        caps = DeviceCapabilities.parse(_caps_json(features=["aprs_2m"]))
        assert not caps.supports_all(Feature.APRS_2M, Feature.MSG_ACK)

    def test_missing_mvp_features_empty_when_all_present(self):
        caps = DeviceCapabilities.parse(_caps_json())
        assert caps.missing_mvp_features() == []

    def test_missing_mvp_features_lists_absent(self):
        caps = DeviceCapabilities.parse(_caps_json(features=["aprs_2m"]))
        missing = caps.missing_mvp_features()
        assert Feature.MSG_ACK in missing
        assert Feature.APRS_2M not in missing

    def test_is_compatible_full_mvp(self):
        caps = DeviceCapabilities.parse(_caps_json())
        assert caps.is_compatible()

    def test_is_compatible_false_wrong_protocol(self):
        caps = DeviceCapabilities.parse(_caps_json(protocol=99))
        assert not caps.is_compatible()

    def test_is_compatible_false_missing_feature(self):
        caps = DeviceCapabilities.parse(_caps_json(features=["aprs_2m"]))
        assert not caps.is_compatible()

    def test_summary_contains_fw_ver(self):
        caps = DeviceCapabilities.parse(_caps_json(fw_ver="1.2.3"))
        assert "1.2.3" in caps.summary()

    def test_summary_contains_assumed_tag(self):
        caps = DeviceCapabilities.assumed_mvp()
        assert "[assumed]" in caps.summary()


# ── CapabilityNegotiator ──────────────────────────────────────────────────────

class TestCapabilityNegotiator:
    def _make_client(self, payload: str):
        c = MagicMock()
        c.read_gatt_char = AsyncMock(return_value=payload.encode("utf-8"))
        return c

    def test_initial_caps_are_assumed(self):
        neg = CapabilityNegotiator()
        assert neg.caps.source == "assumed"

    def test_read_returns_parsed_caps(self):
        neg = CapabilityNegotiator()
        client = self._make_client(_caps_json())
        caps = run(neg.read(client))
        assert caps.source == "read"
        assert caps.protocol == 1

    def test_read_fires_on_caps_callback(self):
        received = []
        neg = CapabilityNegotiator(on_caps=received.append)
        client = self._make_client(_caps_json())
        run(neg.read(client))
        assert len(received) == 1
        assert received[0].source == "read"

    def test_read_falls_back_to_assumed_on_ble_error(self):
        neg = CapabilityNegotiator()
        c = MagicMock()
        c.read_gatt_char = AsyncMock(side_effect=Exception("char not found"))
        caps = run(neg.read(c))
        assert caps.source == "assumed"

    def test_read_falls_back_on_invalid_json(self):
        neg = CapabilityNegotiator()
        client = self._make_client("NOT JSON")
        caps = run(neg.read(client))
        # error source from parse, then reset to assumed in read()
        assert caps.source in ("assumed", "error")

    def test_reset_restores_assumed(self):
        neg = CapabilityNegotiator()
        client = self._make_client(_caps_json())
        run(neg.read(client))
        assert neg.caps.source == "read"
        neg.reset()
        assert neg.caps.source == "assumed"

    def test_caps_property_reflects_last_read(self):
        neg = CapabilityNegotiator()
        client = self._make_client(_caps_json(fw_ver="9.9.9"))
        run(neg.read(client))
        assert neg.caps.fw_ver == "9.9.9"
