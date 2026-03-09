// Unit tests for APRS packet helpers (Step 3 / FW-009)

#include "doctest/doctest.h"
#include "pakt/Aprs.h"
#include "pakt/Ax25.h"

#include <cstring>

// ── make_ui_frame ─────────────────────────────────────────────────────────────

TEST_SUITE("aprs::make_ui_frame")
{
    TEST_CASE("frame has correct destination and source")
    {
        auto f = pakt::aprs::make_ui_frame("N0CALL", 7);
        CHECK(strcmp(f.addr[0].callsign, pakt::ax25::kTocall) == 0);
        CHECK(strcmp(f.addr[1].callsign, "N0CALL") == 0);
        CHECK(f.addr[1].ssid == 7);
        CHECK(f.addr_count == 2);
    }

    TEST_CASE("frame has correct control and PID for APRS UI")
    {
        auto f = pakt::aprs::make_ui_frame("VE3XYZ", 0);
        CHECK(f.control == pakt::ax25::kControlUI);
        CHECK(f.pid == pakt::ax25::kPidNoLayer3);
    }

    TEST_CASE("frame info field starts empty")
    {
        auto f = pakt::aprs::make_ui_frame("K1ABC", 0);
        CHECK(f.info_len == 0);
    }
}

// ── encode_position ───────────────────────────────────────────────────────────

TEST_SUITE("aprs::encode_position")
{
    TEST_CASE("North/East position encodes correctly")
    {
        uint8_t info[128];
        size_t n = pakt::aprs::encode_position(
            49.058333f,   // 49°03.50'N
            -123.166667f, // 123°10.00'W
            '/', '>',
            "Pocket TNC",
            info, sizeof(info));
        REQUIRE(n > 0);

        // First character must be '!'
        CHECK((char)info[0] == '!');
        // Should contain N and W hemisphere chars
        const char *s = (const char *)info;
        CHECK(strchr(s, 'N') != nullptr);
        CHECK(strchr(s, 'W') != nullptr);
        // Symbol table / code
        CHECK(strchr(s, '/') != nullptr);
        CHECK(strchr(s, '>') != nullptr);
        // Comment
        CHECK(strstr(s, "Pocket TNC") != nullptr);
    }

    TEST_CASE("South/West position uses correct hemispheres")
    {
        uint8_t info[128];
        size_t n = pakt::aprs::encode_position(
            -33.866667f,  // South
            151.2f,       // East
            '/', '[',
            nullptr,
            info, sizeof(info));
        REQUIRE(n > 0);
        const char *s = (const char *)info;
        CHECK(strchr(s, 'S') != nullptr);
        CHECK(strchr(s, 'E') != nullptr);
    }

    TEST_CASE("null comment is handled without crash")
    {
        uint8_t info[128];
        size_t n = pakt::aprs::encode_position(
            0.0f, 0.0f, '/', '.', nullptr, info, sizeof(info));
        CHECK(n > 0);
    }

    TEST_CASE("returns 0 when output buffer is too small")
    {
        uint8_t tiny[5];
        size_t n = pakt::aprs::encode_position(
            49.0f, -123.0f, '/', '>', "comment", tiny, sizeof(tiny));
        CHECK(n == 0);
    }

    TEST_CASE("encodes into a valid AX.25 frame that round-trips")
    {
        auto frame = pakt::aprs::make_ui_frame("N0CALL", 0);
        size_t n = pakt::aprs::encode_position(
            48.858333f, 2.294444f, '/', '>', "Paris test",
            frame.info, pakt::ax25::kMaxInfoLen);
        REQUIRE(n > 0);
        frame.info_len = n;

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(encoded, enc_len, decoded));
        CHECK((char)decoded.info[0] == pakt::aprs::kTypePosition);
    }
}

// ── encode_message ────────────────────────────────────────────────────────────

TEST_SUITE("aprs::encode_message")
{
    TEST_CASE("basic message format is correct")
    {
        uint8_t info[128];
        size_t n = pakt::aprs::encode_message(
            "W1AW", 0, "Hello World", "001", info, sizeof(info));
        REQUIRE(n > 0);

        const char *s = (const char *)info;
        // Starts with ':'
        CHECK(s[0] == ':');
        // Contains message text
        CHECK(strstr(s, "Hello World") != nullptr);
        // Contains message ID with '{'
        CHECK(strstr(s, "{001}") != nullptr || strstr(s, "{001") != nullptr);
    }

    TEST_CASE("destination callsign is padded to 9 chars")
    {
        uint8_t info[128];
        pakt::aprs::encode_message("CQ", 0, "test", "1", info, sizeof(info));
        // Field between first ':' and second ':' must be exactly 9 chars
        const char *s = (const char *)info;
        REQUIRE(s[0] == ':');
        const char *second_colon = strchr(s + 1, ':');
        REQUIRE(second_colon != nullptr);
        CHECK(second_colon - (s + 1) == 9);
    }

    TEST_CASE("SSID 0 destination has no -0 suffix")
    {
        uint8_t info[128];
        pakt::aprs::encode_message("N0CALL", 0, "hi", "5", info, sizeof(info));
        const char *s = (const char *)info;
        // "N0CALL   :" — no "-0" in the destination field
        CHECK(strstr(s, "-0") == nullptr);
    }

    TEST_CASE("non-zero SSID destination includes SSID")
    {
        uint8_t info[128];
        pakt::aprs::encode_message("N0CALL", 3, "hi", "5", info, sizeof(info));
        const char *s = (const char *)info;
        CHECK(strstr(s, "-3") != nullptr);
    }

    TEST_CASE("returns 0 when buffer is too small")
    {
        uint8_t tiny[5];
        size_t n = pakt::aprs::encode_message("X", 0, "y", "1", tiny, sizeof(tiny));
        CHECK(n == 0);
    }

    TEST_CASE("message encodes into a valid AX.25 frame that round-trips")
    {
        auto frame = pakt::aprs::make_ui_frame("N0CALL", 7);
        size_t n = pakt::aprs::encode_message(
            "W1AW", 0, "Testing 1 2 3", "042",
            frame.info, pakt::ax25::kMaxInfoLen);
        REQUIRE(n > 0);
        frame.info_len = n;

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(encoded, enc_len, decoded));
        CHECK((char)decoded.info[0] == pakt::aprs::kTypeMessage);
        CHECK(strstr((const char *)decoded.info, "Testing 1 2 3") != nullptr);
    }
}

// ── packet_type ───────────────────────────────────────────────────────────────

TEST_SUITE("aprs::packet_type")
{
    TEST_CASE("identifies position packet '!'")
    {
        const uint8_t info[] = "!4903.50N/12310.00W>test";
        CHECK(pakt::aprs::packet_type(info, sizeof(info) - 1) == '!');
    }

    TEST_CASE("identifies message packet ':'")
    {
        const uint8_t info[] = ":W1AW     :hello{01}";
        CHECK(pakt::aprs::packet_type(info, sizeof(info) - 1) == ':');
    }

    TEST_CASE("returns '\\0' for unknown type")
    {
        const uint8_t info[] = "Xsomething";
        CHECK(pakt::aprs::packet_type(info, sizeof(info) - 1) == '\0');
    }

    TEST_CASE("returns '\\0' for empty info field")
    {
        CHECK(pakt::aprs::packet_type(nullptr, 0) == '\0');
    }

    TEST_CASE("identifies position-with-timestamp '/'")
    {
        const uint8_t info[] = "/092345z4903.50N/12310.00W>test";
        CHECK(pakt::aprs::packet_type(info, sizeof(info) - 1) == '/');
    }
}
