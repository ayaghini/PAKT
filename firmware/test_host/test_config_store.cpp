// test_config_store.cpp – Host unit tests for DeviceConfigStore
//
// Covers: in-memory update, storage backend persistence, persist-failure path,
// load-without-backend behaviour, and config_to_json read-after-write path.
//
// Run: ./build/test_host/pakt_tests --reporters=console --no-intro

#include "doctest/doctest.h"
#include "pakt/DeviceConfigStore.h"
#include "pakt/IStorage.h"

#include <cstring>
#include <string>

using namespace pakt;

// ── StorageMock ───────────────────────────────────────────────────────────────

struct StorageMock : IStorage {
    bool         save_returns = true;
    bool         load_returns = false;
    int          save_calls   = 0;
    DeviceConfig last_saved{};

    bool load(DeviceConfig &cfg) override
    {
        if (load_returns) { cfg = last_saved; return true; }
        return false;
    }

    bool save(const DeviceConfig &cfg) override
    {
        ++save_calls;
        last_saved = cfg;
        return save_returns;
    }

    bool erase() override { return true; }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("DeviceConfigStore: apply updates in-memory config (no storage)") {
    DeviceConfigStore store;

    ConfigFields f{};
    strncpy(f.callsign, "W1AW", sizeof(f.callsign) - 1);
    f.ssid = 7;

    CHECK(store.apply(f));
    CHECK(strcmp(store.config().callsign, "W1AW") == 0);
    CHECK(store.config().ssid == 7);
}

TEST_CASE("DeviceConfigStore: apply with storage backend calls save once") {
    StorageMock sm;
    DeviceConfigStore store(&sm);

    ConfigFields f{};
    strncpy(f.callsign, "KD9ABC", sizeof(f.callsign) - 1);
    f.ssid = 3;

    CHECK(store.apply(f));
    CHECK(sm.save_calls == 1);
    CHECK(strcmp(sm.last_saved.callsign, "KD9ABC") == 0);
    CHECK(sm.last_saved.ssid == 3);
}

TEST_CASE("DeviceConfigStore: storage save failure returns false, in-memory still updated") {
    StorageMock sm;
    sm.save_returns = false;
    DeviceConfigStore store(&sm);

    ConfigFields f{};
    strncpy(f.callsign, "N0CALL", sizeof(f.callsign) - 1);
    f.ssid = 0;

    CHECK_FALSE(store.apply(f));
    CHECK(sm.save_calls == 1);
    // In-memory state is updated even when persist fails
    CHECK(strcmp(store.config().callsign, "N0CALL") == 0);
}

TEST_CASE("DeviceConfigStore: load without storage returns false and retains defaults") {
    DeviceConfigStore store;
    CHECK_FALSE(store.load());
    // Callsign remains unset (empty string = "must be set by user")
    CHECK(store.config().callsign[0] == '\0');
    CHECK(store.config().ssid == 0);
}

TEST_CASE("DeviceConfigStore: config_to_json reflects applied callsign and ssid") {
    DeviceConfigStore store;

    ConfigFields f{};
    strncpy(f.callsign, "W1AW", sizeof(f.callsign) - 1);
    f.ssid = 7;
    store.apply(f);

    char buf[64];
    size_t n = DeviceConfigStore::config_to_json(store.config(), buf, sizeof(buf));
    REQUIRE(n > 0);
    CHECK(std::string(buf, n) == "{\"callsign\":\"W1AW\",\"ssid\":7}");
}

TEST_CASE("DeviceConfigStore: config_to_json default config has empty callsign and ssid 0") {
    DeviceConfigStore store;   // no apply() called

    char buf[64];
    size_t n = DeviceConfigStore::config_to_json(store.config(), buf, sizeof(buf));
    REQUIRE(n > 0);
    CHECK(std::string(buf, n) == "{\"callsign\":\"\",\"ssid\":0}");
}

TEST_CASE("DeviceConfigStore: config_to_json returns 0 on buffer too small") {
    DeviceConfigStore store;
    char buf[5];   // too small for any valid JSON
    CHECK(DeviceConfigStore::config_to_json(store.config(), buf, sizeof(buf)) == 0);
}
