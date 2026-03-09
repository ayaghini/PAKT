// Unit tests for AX.25 codec (Step 3 / FW-008)

#include "doctest/doctest.h"
#include "pakt/Ax25.h"

#include <cstring>

// ── FCS ───────────────────────────────────────────────────────────────────────

TEST_SUITE("ax25::fcs")
{
    TEST_CASE("empty buffer does not crash")
    {
        // No undefined behaviour on zero-length input
        uint16_t result = pakt::ax25::fcs(nullptr, 0);
        // Specific value: CRC-CCITT of empty buffer = 0x1D0F (after init 0xFFFF XOR 0xFFFF)
        (void)result;
    }

    TEST_CASE("known-good FCS for ASCII 'A'")
    {
        // CRC-16/CCITT (reflected) of {0x41} = 0xB915
        uint8_t data[] = {0x41};
        CHECK(pakt::ax25::fcs(data, 1) == 0xB915u);
    }

    TEST_CASE("known-good FCS for {0x01, 0x02, 0x03}")
    {
        uint8_t data[] = {0x01, 0x02, 0x03};
        // Pre-computed expected value for this CRC variant
        uint16_t result = pakt::ax25::fcs(data, 3);
        // Verify round-trip: appending FCS bytes should give same FCS as just the data
        uint8_t with_fcs[5];
        memcpy(with_fcs, data, 3);
        with_fcs[3] = (uint8_t)(result & 0xFF);
        with_fcs[4] = (uint8_t)(result >> 8);
        // When we compute FCS over data+FCS, the result equals 0xF0B8 (CRC-CCITT residual).
        CHECK(pakt::ax25::fcs(with_fcs, 5) == 0xF0B8u);
    }

    TEST_CASE("FCS is deterministic")
    {
        uint8_t d[] = {0xDE, 0xAD, 0xBE, 0xEF};
        CHECK(pakt::ax25::fcs(d, 4) == pakt::ax25::fcs(d, 4));
    }

    TEST_CASE("FCS changes when data changes")
    {
        uint8_t a[] = {0x01};
        uint8_t b[] = {0x02};
        CHECK(pakt::ax25::fcs(a, 1) != pakt::ax25::fcs(b, 1));
    }
}

// ── Encode / Decode round-trip ────────────────────────────────────────────────

static pakt::ax25::Frame make_test_frame(const char *callsign, uint8_t ssid,
                                          const char *info)
{
    pakt::ax25::Frame f{};
    // Destination
    strncpy(f.addr[0].callsign, pakt::ax25::kTocall, pakt::ax25::kAddrCallsignMaxLen);
    f.addr[0].ssid = 0;

    // Source
    strncpy(f.addr[1].callsign, callsign, pakt::ax25::kAddrCallsignMaxLen);
    f.addr[1].callsign[pakt::ax25::kAddrCallsignMaxLen] = '\0';
    f.addr[1].ssid = ssid;

    f.addr_count = 2;
    f.control    = pakt::ax25::kControlUI;
    f.pid        = pakt::ax25::kPidNoLayer3;
    f.info_len   = strlen(info);
    memcpy(f.info, info, f.info_len);
    return f;
}

TEST_SUITE("ax25 encode/decode")
{
    TEST_CASE("basic round-trip: encode then decode recovers the frame")
    {
        auto original = make_test_frame("N0CALL", 7, "!4903.50N/12310.00W>Test");

        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(original, buf, sizeof(buf));
        REQUIRE(len > 0);

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(buf, len, decoded));

        CHECK(decoded.addr_count == 2);
        CHECK(strcmp(decoded.addr[0].callsign, pakt::ax25::kTocall) == 0);
        CHECK(strcmp(decoded.addr[1].callsign, "N0CALL") == 0);
        CHECK(decoded.addr[1].ssid == 7);
        CHECK(decoded.control == pakt::ax25::kControlUI);
        CHECK(decoded.pid == pakt::ax25::kPidNoLayer3);
        CHECK(decoded.info_len == original.info_len);
        CHECK(memcmp(decoded.info, original.info, original.info_len) == 0);
    }

    TEST_CASE("callsign with no SSID round-trips correctly")
    {
        auto f = make_test_frame("VK2XYZ", 0, "!3351.00S/15112.00E>hi");
        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(f, buf, sizeof(buf));
        REQUIRE(len > 0);

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(buf, len, decoded));
        CHECK(strcmp(decoded.addr[1].callsign, "VK2XYZ") == 0);
        CHECK(decoded.addr[1].ssid == 0);
    }

    TEST_CASE("encode returns 0 when buffer is too small")
    {
        auto f = make_test_frame("N0CALL", 0, "!4903.50N/12310.00W>x");
        uint8_t tiny[5];
        CHECK(pakt::ax25::encode(f, tiny, sizeof(tiny)) == 0);
    }

    TEST_CASE("encode returns 0 for frame with fewer than 2 addresses")
    {
        pakt::ax25::Frame f{};
        f.addr_count = 1;
        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        CHECK(pakt::ax25::encode(f, buf, sizeof(buf)) == 0);
    }

    TEST_CASE("decode rejects data with corrupted FCS")
    {
        auto f = make_test_frame("N0CALL", 0, "hello");
        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(f, buf, sizeof(buf));
        REQUIRE(len > 0);

        // Corrupt the last byte (part of FCS)
        buf[len - 1] ^= 0xFF;

        pakt::ax25::Frame decoded{};
        CHECK_FALSE(pakt::ax25::decode(buf, len, decoded));
    }

    TEST_CASE("decode rejects truncated data")
    {
        auto f = make_test_frame("W1AW", 0, "hi");
        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(f, buf, sizeof(buf));
        REQUIRE(len > 4);

        pakt::ax25::Frame decoded{};
        // Remove the last 4 bytes: frame is now too short / FCS is wrong
        CHECK_FALSE(pakt::ax25::decode(buf, len - 4, decoded));
    }

    TEST_CASE("frame with digipeater round-trips correctly")
    {
        pakt::ax25::Frame f{};
        strncpy(f.addr[0].callsign, "APZPKT", 6);
        strncpy(f.addr[1].callsign, "N0CALL", 6);
        f.addr[1].ssid = 7;
        strncpy(f.addr[2].callsign, "WIDE1", 5);
        f.addr[2].ssid = 1;
        f.addr_count = 3;
        f.control = pakt::ax25::kControlUI;
        f.pid     = pakt::ax25::kPidNoLayer3;
        const char *info = "!1234.56N/07654.32W>via wide";
        f.info_len = strlen(info);
        memcpy(f.info, info, f.info_len);

        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(f, buf, sizeof(buf));
        REQUIRE(len > 0);

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(buf, len, decoded));
        CHECK(decoded.addr_count == 3);
        CHECK(strcmp(decoded.addr[2].callsign, "WIDE1") == 0);
        CHECK(decoded.addr[2].ssid == 1);
    }

    TEST_CASE("FCS self-consistency: encode appends correct FCS")
    {
        auto f = make_test_frame("KG5PRT", 3, "!5100.00N/00100.00W>beacon");
        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(f, buf, sizeof(buf));
        REQUIRE(len >= 2);

        // The FCS over the entire output (including the appended FCS bytes)
        // must equal 0xF0B8 for this CRC variant.
        CHECK(pakt::ax25::fcs(buf, len) == 0xF0B8u);
    }

    TEST_CASE("empty info field is valid")
    {
        auto f = make_test_frame("K0AA", 0, "");
        f.info_len = 0;
        uint8_t buf[pakt::ax25::kMaxEncodedLen];
        size_t len = pakt::ax25::encode(f, buf, sizeof(buf));
        REQUIRE(len > 0);
        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(buf, len, decoded));
        CHECK(decoded.info_len == 0);
    }
}

// ── TNC2 formatting ───────────────────────────────────────────────────────────

TEST_SUITE("ax25::to_tnc2")
{
    TEST_CASE("basic frame formats correctly")
    {
        auto f = make_test_frame("N0CALL", 7, "!4903.50N/12310.00W>test");
        char buf[512];
        size_t n = pakt::ax25::to_tnc2(f, buf, sizeof(buf));
        REQUIRE(n > 0);
        // Should start with source callsign
        CHECK(strncmp(buf, "N0CALL-7>", 9) == 0);
        // Should contain destination
        CHECK(strstr(buf, "APZPKT") != nullptr);
        // Should contain info separator
        CHECK(strstr(buf, ":!") != nullptr);
    }

    TEST_CASE("no SSID omits the dash-ssid suffix")
    {
        auto f = make_test_frame("W1AW", 0, ">status");
        char buf[256];
        pakt::ax25::to_tnc2(f, buf, sizeof(buf));
        // "W1AW>" not "W1AW-0>"
        CHECK(strncmp(buf, "W1AW>", 5) == 0);
    }

    TEST_CASE("returns 0 for frame with fewer than 2 addresses")
    {
        pakt::ax25::Frame f{};
        f.addr_count = 1;
        char buf[64];
        CHECK(pakt::ax25::to_tnc2(f, buf, sizeof(buf)) == 0);
    }
}
