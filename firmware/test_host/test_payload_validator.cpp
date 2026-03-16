// test_payload_validator.cpp – Host unit tests for PayloadValidator (P0)
//
// Tests inbound BLE JSON payload validation for config and tx_request writes.
// Run: ./build/test_host/pakt_tests --reporters=console --no-intro

#include "doctest/doctest.h"
#include "pakt/PayloadValidator.h"

#include <cstring>
#include <string>

using namespace pakt;

// Helper: call validate_config_payload with a string literal.
static bool cfg(const char *json, ConfigFields *out = nullptr)
{
    return PayloadValidator::validate_config_payload(
        reinterpret_cast<const uint8_t *>(json), strlen(json), out);
}

// Helper: call validate_tx_request_payload with a string literal.
static bool tx(const char *json, TxRequestFields *out = nullptr)
{
    return PayloadValidator::validate_tx_request_payload(
        reinterpret_cast<const uint8_t *>(json), strlen(json), out);
}

// ── Config: acceptance ────────────────────────────────────────────────────────

TEST_CASE("config: minimal valid payload (callsign only)") {
    CHECK(cfg("{\"callsign\":\"W1AW\"}"));
}

TEST_CASE("config: valid payload with ssid 0") {
    CHECK(cfg("{\"callsign\":\"W1AW\",\"ssid\":0}"));
}

TEST_CASE("config: valid payload with ssid 15") {
    CHECK(cfg("{\"callsign\":\"VE3XYZ\",\"ssid\":15}"));
}

TEST_CASE("config: single-char callsign is valid") {
    CHECK(cfg("{\"callsign\":\"A\"}"));
}

TEST_CASE("config: 6-char callsign is valid") {
    CHECK(cfg("{\"callsign\":\"VE3XYZ\"}"));
}

TEST_CASE("config: callsign with digit is valid") {
    CHECK(cfg("{\"callsign\":\"W1AW\"}"));
}

TEST_CASE("config: callsign with dash is valid") {
    CHECK(cfg("{\"callsign\":\"W1-AW\"}"));
}

TEST_CASE("config: out parameter filled correctly") {
    ConfigFields f;
    REQUIRE(cfg("{\"callsign\":\"W1AW\",\"ssid\":7}", &f));
    CHECK(strcmp(f.callsign, "W1AW") == 0);
    CHECK(f.ssid == 7);
}

TEST_CASE("config: out parameter ssid defaults to 0 when absent") {
    ConfigFields f;
    REQUIRE(cfg("{\"callsign\":\"W1AW\"}", &f));
    CHECK(f.ssid == 0);
}

// ── Config: rejection ─────────────────────────────────────────────────────────

TEST_CASE("config: missing callsign field") {
    CHECK(!cfg("{\"ssid\":0}"));
}

TEST_CASE("config: empty callsign string") {
    CHECK(!cfg("{\"callsign\":\"\"}"));
}

TEST_CASE("config: callsign too long (7 chars)") {
    CHECK(!cfg("{\"callsign\":\"ABCDEFG\"}"));
}

TEST_CASE("config: callsign with space") {
    CHECK(!cfg("{\"callsign\":\"W1 AW\"}"));
}

TEST_CASE("config: callsign with special char") {
    CHECK(!cfg("{\"callsign\":\"W1@AW\"}"));
}

TEST_CASE("config: ssid above 15") {
    CHECK(!cfg("{\"callsign\":\"W1AW\",\"ssid\":16}"));
}

TEST_CASE("config: ssid negative") {
    CHECK(!cfg("{\"callsign\":\"W1AW\",\"ssid\":-1}"));
}

TEST_CASE("config: null data pointer") {
    CHECK(!PayloadValidator::validate_config_payload(nullptr, 5));
}

TEST_CASE("config: zero length") {
    const uint8_t d[] = {'{', '}'};
    CHECK(!PayloadValidator::validate_config_payload(d, 0));
}

TEST_CASE("config: empty object (no callsign)") {
    CHECK(!cfg("{}"));
}

TEST_CASE("config: payload at kMaxJsonLen is rejected") {
    // Construct a string of exactly kMaxJsonLen bytes (trigger >= check).
    std::string big(PayloadValidator::kMaxJsonLen, 'x');
    CHECK(!PayloadValidator::validate_config_payload(
        reinterpret_cast<const uint8_t *>(big.c_str()), big.size()));
}

// ── TX request: acceptance ────────────────────────────────────────────────────

TEST_CASE("tx_request: minimal valid payload") {
    CHECK(tx("{\"dest\":\"APRS\",\"text\":\"Hello\"}"));
}

TEST_CASE("tx_request: valid payload with ssid") {
    CHECK(tx("{\"dest\":\"APRS\",\"text\":\"Hello\",\"ssid\":3}"));
}

TEST_CASE("tx_request: text at maximum length (67 chars)") {
    std::string payload = "{\"dest\":\"APRS\",\"text\":\"";
    payload += std::string(67, 'A');
    payload += "\"}";
    CHECK(tx(payload.c_str()));
}

TEST_CASE("tx_request: out parameter filled correctly") {
    TxRequestFields f;
    REQUIRE(tx("{\"dest\":\"W1AW\",\"text\":\"Hi there\",\"ssid\":2}", &f));
    CHECK(strcmp(f.dest, "W1AW") == 0);
    CHECK(strcmp(f.text, "Hi there") == 0);
    CHECK(f.ssid == 2);
}

TEST_CASE("tx_request: out parameter ssid defaults to 0") {
    TxRequestFields f;
    REQUIRE(tx("{\"dest\":\"W1AW\",\"text\":\"Hello\"}", &f));
    CHECK(f.ssid == 0);
}

// ── TX request: rejection ─────────────────────────────────────────────────────

TEST_CASE("tx_request: missing dest") {
    CHECK(!tx("{\"text\":\"Hello\"}"));
}

TEST_CASE("tx_request: missing text") {
    CHECK(!tx("{\"dest\":\"APRS\"}"));
}

TEST_CASE("tx_request: empty text") {
    CHECK(!tx("{\"dest\":\"APRS\",\"text\":\"\"}"));
}

TEST_CASE("tx_request: text too long (68 chars)") {
    std::string payload = "{\"dest\":\"APRS\",\"text\":\"";
    payload += std::string(68, 'A');
    payload += "\"}";
    CHECK(!tx(payload.c_str()));
}

TEST_CASE("tx_request: invalid dest callsign (space)") {
    CHECK(!tx("{\"dest\":\"AP RS\",\"text\":\"Hello\"}"));
}

TEST_CASE("tx_request: invalid dest callsign (empty)") {
    CHECK(!tx("{\"dest\":\"\",\"text\":\"Hello\"}"));
}

TEST_CASE("tx_request: dest too long (7 chars)") {
    CHECK(!tx("{\"dest\":\"ABCDEFG\",\"text\":\"Hello\"}"));
}

TEST_CASE("tx_request: ssid out of range") {
    CHECK(!tx("{\"dest\":\"APRS\",\"text\":\"Hi\",\"ssid\":16}"));
}

TEST_CASE("tx_request: null data") {
    CHECK(!PayloadValidator::validate_tx_request_payload(nullptr, 5));
}

TEST_CASE("tx_request: empty object") {
    CHECK(!tx("{}"));
}

// ── Key-name collision: text value containing a key-like substring ────────────

TEST_CASE("tx_request: text value containing 'dest' does not confuse parser") {
    // 'dest' appears in the text value; real 'dest' key should still be found.
    TxRequestFields f;
    REQUIRE(tx("{\"dest\":\"W1AW\",\"text\":\"my dest is far\"}", &f));
    CHECK(strcmp(f.dest, "W1AW") == 0);
    CHECK(strcmp(f.text, "my dest is far") == 0);
}

// ── Edge cases: whitespace, field order, escaped strings ──────────────────────

TEST_CASE("config: extra whitespace around colon and value") {
    // Spaces between key, colon, and value must be tolerated.
    CHECK(cfg("{ \"callsign\" : \"W1AW\" }"));
}

TEST_CASE("config: ssid field before callsign (field order independence)") {
    ConfigFields f;
    REQUIRE(cfg("{\"ssid\":3,\"callsign\":\"W1AW\"}", &f));
    CHECK(strcmp(f.callsign, "W1AW") == 0);
    CHECK(f.ssid == 3);
}

TEST_CASE("tx_request: text with JSON escaped quote is valid") {
    // JSON: {"dest":"APRS","text":"Say \"Hi\""}  →  text = Say "Hi"  (8 chars)
    TxRequestFields f;
    REQUIRE(tx(R"({"dest":"APRS","text":"Say \"Hi\""})", &f));
    CHECK(strcmp(f.text, "Say \"Hi\"") == 0);
}

TEST_CASE("tx_request: text with JSON escaped backslash is valid") {
    // JSON: {"dest":"APRS","text":"C:\\path"}  →  text = C:\path  (7 chars)
    TxRequestFields f;
    REQUIRE(tx(R"({"dest":"APRS","text":"C:\\path"})", &f));
    CHECK(strcmp(f.text, "C:\\path") == 0);
}

TEST_CASE("tx_request: key name appearing as string value is not confused with key") {
    // "callsign" appears as a VALUE before the real "dest" key.
    // The parser must reject it because the colon check fails (value is followed by ',').
    TxRequestFields f;
    REQUIRE(tx(R"({"fake":"dest","dest":"APRS","text":"Hi"})", &f));
    CHECK(strcmp(f.dest, "APRS") == 0);
}

TEST_CASE("tx_request: unterminated text string is rejected") {
    CHECK(!tx("{\"dest\":\"APRS\",\"text\":\"unterminated"));
}
