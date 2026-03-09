// test_telemetry.cpp – Host unit tests for telemetry JSON serialisers (FW-015)
//
// No hardware or RTOS required.  Uses doctest.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "pakt/Telemetry.h"

#include <cstring>
#include <string>
#include <string_view>

using namespace pakt;

// ── Helper ────────────────────────────────────────────────────────────────────

static bool contains(const char *buf, const char *substr) {
    return std::strstr(buf, substr) != nullptr;
}

// ── DeviceStatus ──────────────────────────────────────────────────────────────

TEST_CASE("DeviceStatus: idle state serialises correctly") {
    char buf[kTelemetryJsonMaxLen];
    DeviceStatus s{RadioState::IDLE, false, false, 0, 0, 0};
    size_t n = s.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"radio\":\"idle\""));
    CHECK(contains(buf, "\"bonded\":false"));
    CHECK(contains(buf, "\"gps_fix\":false"));
}

TEST_CASE("DeviceStatus: TX state serialises correctly") {
    char buf[kTelemetryJsonMaxLen];
    DeviceStatus s{RadioState::TX, true, true, 2, 1, 120};
    size_t n = s.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"radio\":\"tx\""));
    CHECK(contains(buf, "\"bonded\":true"));
    CHECK(contains(buf, "\"gps_fix\":true"));
    CHECK(contains(buf, "\"pending_tx\":2"));
    CHECK(contains(buf, "\"uptime_s\":120"));
}

TEST_CASE("DeviceStatus: RX state name") {
    char buf[kTelemetryJsonMaxLen];
    DeviceStatus s{RadioState::RX, false, false, 0, 0, 0};
    s.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"radio\":\"rx\""));
}

TEST_CASE("DeviceStatus: ERROR state name") {
    char buf[kTelemetryJsonMaxLen];
    DeviceStatus s{RadioState::ERROR, false, false, 0, 0, 0};
    s.to_json(buf, sizeof(buf));
    CHECK(contains(buf, "\"radio\":\"error\""));
}

TEST_CASE("DeviceStatus: returns 0 on buffer too small") {
    char buf[5];
    DeviceStatus s{RadioState::IDLE, false, false, 0, 0, 0};
    CHECK(s.to_json(buf, sizeof(buf)) == 0);
}

TEST_CASE("DeviceStatus: output is valid-looking JSON (starts '{' ends '}'") {
    char buf[kTelemetryJsonMaxLen];
    DeviceStatus s{RadioState::IDLE, false, false, 0, 0, 0};
    size_t n = s.to_json(buf, sizeof(buf));
    CHECK(n > 2);
    CHECK(buf[0] == '{');
    CHECK(buf[n - 1] == '}');
}

// ── GpsTelem ──────────────────────────────────────────────────────────────────

TEST_CASE("GpsTelem: fields serialise correctly") {
    char buf[kTelemetryJsonMaxLen];
    GpsTelem g{43.6532, -79.3832, 76.5f, 0.0f, 0.0f, 8, 1, 1234567};
    size_t n = g.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"lat\":43.653200"));
    CHECK(contains(buf, "\"lon\":-79.383200"));
    CHECK(contains(buf, "\"sats\":8"));
    CHECK(contains(buf, "\"fix\":1"));
    CHECK(contains(buf, "\"ts\":1234567"));
}

TEST_CASE("GpsTelem: zero fix quality") {
    char buf[kTelemetryJsonMaxLen];
    GpsTelem g{0.0, 0.0, 0.0f, 0.0f, 0.0f, 0, 0, 0};
    size_t n = g.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"fix\":0"));
}

TEST_CASE("GpsTelem: returns 0 on buffer too small") {
    char buf[10];
    GpsTelem g{0.0, 0.0, 0.0f, 0.0f, 0.0f, 0, 0, 0};
    CHECK(g.to_json(buf, sizeof(buf)) == 0);
}

// ── PowerTelem ────────────────────────────────────────────────────────────────

TEST_CASE("PowerTelem: fields serialise correctly") {
    char buf[kTelemetryJsonMaxLen];
    PowerTelem p{3.82f, 72, 30.0f, 1.5f, 35.2f};
    size_t n = p.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"batt_pct\":72"));
    CHECK(contains(buf, "\"tx_dbm\":30.0"));
    CHECK(contains(buf, "\"vswr\":1.50"));
}

TEST_CASE("PowerTelem: returns 0 on buffer too small") {
    char buf[5];
    PowerTelem p{3.7f, 50, 10.0f, 1.0f, 25.0f};
    CHECK(p.to_json(buf, sizeof(buf)) == 0);
}

// ── SysTelem ─────────────────────────────────────────────────────────────────

TEST_CASE("SysTelem: fields serialise correctly") {
    char buf[kTelemetryJsonMaxLen];
    SysTelem s{120000, 80000, 15, 42, 100, 0, 3, 3600};
    size_t n = s.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(contains(buf, "\"free_heap\":120000"));
    CHECK(contains(buf, "\"cpu_pct\":15"));
    CHECK(contains(buf, "\"tx_pkts\":42"));
    CHECK(contains(buf, "\"rx_pkts\":100"));
    CHECK(contains(buf, "\"rx_errs\":3"));
    CHECK(contains(buf, "\"uptime_s\":3600"));
}

TEST_CASE("SysTelem: returns 0 on buffer too small") {
    char buf[5];
    SysTelem s{0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(s.to_json(buf, sizeof(buf)) == 0);
}

TEST_CASE("SysTelem: output fits in kTelemetryJsonMaxLen") {
    char buf[kTelemetryJsonMaxLen];
    SysTelem s{999999, 999999, 100, 9999, 9999, 9999, 9999, 999999};
    size_t n = s.to_json(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(n < kTelemetryJsonMaxLen);
}
