// test_capability.cpp – Host unit tests for DeviceCapabilities (INT-001)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "pakt/DeviceCapabilities.h"

#include <cstring>
#include <string>

using namespace pakt;

static bool contains(const char *buf, const char *substr) {
    return std::strstr(buf, substr) != nullptr;
}

// ── mvp_defaults ──────────────────────────────────────────────────────────────

TEST_CASE("mvp_defaults: protocol is 1") {
    auto caps = DeviceCapabilities::mvp_defaults();
    CHECK(caps.protocol == 1);
}

TEST_CASE("mvp_defaults: fw_ver is not empty") {
    auto caps = DeviceCapabilities::mvp_defaults();
    CHECK(caps.fw_ver != nullptr);
    CHECK(caps.fw_ver[0] != '\0');
}

TEST_CASE("mvp_defaults: has all expected MVP features") {
    auto caps = DeviceCapabilities::mvp_defaults();
    CHECK(caps.has(Feature::APRS_2M));
    CHECK(caps.has(Feature::BLE_CHUNKING));
    CHECK(caps.has(Feature::TELEMETRY));
    CHECK(caps.has(Feature::MSG_ACK));
    CHECK(caps.has(Feature::CONFIG_RW));
    CHECK(caps.has(Feature::GPS_ONBOARD));
}

TEST_CASE("mvp_defaults: does not advertise HF_AUDIO") {
    auto caps = DeviceCapabilities::mvp_defaults();
    CHECK(!caps.has(Feature::HF_AUDIO));
}

// ── to_json ───────────────────────────────────────────────────────────────────

TEST_CASE("to_json: produces valid-looking JSON envelope") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    size_t n = caps.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(buf[0] == '{');
    CHECK(buf[n - 1] == '}');
}

TEST_CASE("to_json: contains fw_ver field") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"fw_ver\":\"0.1.0\""));
}

TEST_CASE("to_json: contains hw_rev field") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"hw_rev\":\"EVT-A\""));
}

TEST_CASE("to_json: contains protocol field") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"protocol\":1"));
}

TEST_CASE("to_json: features array contains aprs_2m") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"aprs_2m\""));
}

TEST_CASE("to_json: features array contains ble_chunking") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"ble_chunking\""));
}

TEST_CASE("to_json: features array does not contain hf_audio for MVP") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.to_json(buf, sizeof(buf));
    CHECK(!contains(buf, "\"hf_audio\""));
}

TEST_CASE("to_json: HF_AUDIO appears when feature bit set") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.features |= static_cast<uint32_t>(Feature::HF_AUDIO);
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"hf_audio\""));
}

TEST_CASE("to_json: returns 0 on buffer too small") {
    char buf[5];
    auto caps = DeviceCapabilities::mvp_defaults();
    CHECK(caps.to_json(buf, sizeof(buf)) == 0);
}

TEST_CASE("to_json: empty feature mask produces empty array") {
    char buf[kCapJsonMaxLen];
    DeviceCapabilities caps{"0.1.0", "EVT-A", 1, 0};
    caps.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"features\":[]"));
}

TEST_CASE("to_json: null fw_ver handled without crash") {
    char buf[kCapJsonMaxLen];
    DeviceCapabilities caps{nullptr, "EVT-A", 1, 0};
    size_t n = caps.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"fw_ver\":\"\""));
}

TEST_CASE("to_json: output fits in kCapJsonMaxLen") {
    char buf[kCapJsonMaxLen];
    auto caps = DeviceCapabilities::mvp_defaults();
    caps.features = UINT32_MAX;  // all features
    size_t n = caps.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(n < kCapJsonMaxLen);
}

// ── has() ─────────────────────────────────────────────────────────────────────

TEST_CASE("has: returns false for absent feature") {
    DeviceCapabilities caps{"0.1.0", "EVT-A", 1, 0};
    CHECK(!caps.has(Feature::APRS_2M));
}

TEST_CASE("has: returns true for present feature") {
    DeviceCapabilities caps{"0.1.0", "EVT-A", 1,
                            static_cast<uint32_t>(Feature::APRS_2M)};
    CHECK(caps.has(Feature::APRS_2M));
}
