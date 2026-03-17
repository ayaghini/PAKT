// test_kiss_framer.cpp – Host unit tests for KissFramer (FW-018, INT-003)
//
// Covers:
//   - KissFramer::escape / unescape round-trips
//   - KissFramer::encode: output format, FEND delimiters, escaped bytes
//   - KissFramer::decode: strips FENDs, unescapes, validates command byte
//   - decode: error cases (malformed, oversize, non-port-0)
//   - encode/decode round-trip with known AX.25 vectors
//   - Return-from-KISS (0x0F) handling
//   - No-op extended commands
//   - Buffer size constraints and MVP frame limit

#include "doctest/doctest.h"
#include "pakt/KissFramer.h"

#include <cstring>
#include <vector>

using pakt::KissFramer;
using pakt::kKissFend;
using pakt::kKissFesc;
using pakt::kKissTfend;
using pakt::kKissTfesc;
using pakt::kKissMaxFrame;
using pakt::kKissCmdData;
using pakt::kKissCmdReturnFromKiss;

// ── escape ────────────────────────────────────────────────────────────────────

TEST_SUITE("KissFramer::escape")
{
    TEST_CASE("plain bytes pass through unchanged")
    {
        uint8_t in[]  = {0x01, 0x02, 0x7E, 0xFF};
        uint8_t out[8] = {};
        size_t n = KissFramer::escape(in, sizeof(in), out, sizeof(out));
        REQUIRE(n == 4);
        CHECK(memcmp(in, out, 4) == 0);
    }

    TEST_CASE("0xC0 (FEND) is escaped to 0xDB 0xDC")
    {
        uint8_t in[] = {0xC0};
        uint8_t out[4] = {};
        size_t n = KissFramer::escape(in, 1, out, sizeof(out));
        REQUIRE(n == 2);
        CHECK(out[0] == kKissFesc);
        CHECK(out[1] == kKissTfend);
    }

    TEST_CASE("0xDB (FESC) is escaped to 0xDB 0xDD")
    {
        uint8_t in[] = {0xDB};
        uint8_t out[4] = {};
        size_t n = KissFramer::escape(in, 1, out, sizeof(out));
        REQUIRE(n == 2);
        CHECK(out[0] == kKissFesc);
        CHECK(out[1] == kKissTfesc);
    }

    TEST_CASE("mixed bytes with special values")
    {
        uint8_t in[]  = {0x01, 0xC0, 0xDB, 0x02};
        uint8_t out[16] = {};
        size_t n = KissFramer::escape(in, sizeof(in), out, sizeof(out));
        // 0x01 → 1 byte, 0xC0 → 2 bytes, 0xDB → 2 bytes, 0x02 → 1 byte = 6 bytes
        REQUIRE(n == 6);
        CHECK(out[0] == 0x01);
        CHECK(out[1] == kKissFesc);
        CHECK(out[2] == kKissTfend);
        CHECK(out[3] == kKissFesc);
        CHECK(out[4] == kKissTfesc);
        CHECK(out[5] == 0x02);
    }

    TEST_CASE("returns 0 on buffer overflow")
    {
        uint8_t in[]  = {0xC0, 0xC0};
        uint8_t out[2] = {}; // too small for 4 bytes
        size_t n = KissFramer::escape(in, sizeof(in), out, sizeof(out));
        CHECK(n == 0);
    }

    TEST_CASE("null input returns 0")
    {
        uint8_t out[8] = {};
        CHECK(KissFramer::escape(nullptr, 4, out, sizeof(out)) == 0);
    }
}

// ── unescape ──────────────────────────────────────────────────────────────────

TEST_SUITE("KissFramer::unescape")
{
    TEST_CASE("plain bytes pass through unchanged")
    {
        uint8_t in[]  = {0x01, 0x02, 0x7E};
        uint8_t out[8] = {};
        int n = KissFramer::unescape(in, sizeof(in), out, sizeof(out));
        REQUIRE(n == 3);
        CHECK(memcmp(in, out, 3) == 0);
    }

    TEST_CASE("0xDB 0xDC unescapes to 0xC0")
    {
        uint8_t in[] = {kKissFesc, kKissTfend};
        uint8_t out[4] = {};
        int n = KissFramer::unescape(in, 2, out, sizeof(out));
        REQUIRE(n == 1);
        CHECK(out[0] == kKissFend);
    }

    TEST_CASE("0xDB 0xDD unescapes to 0xDB")
    {
        uint8_t in[] = {kKissFesc, kKissTfesc};
        uint8_t out[4] = {};
        int n = KissFramer::unescape(in, 2, out, sizeof(out));
        REQUIRE(n == 1);
        CHECK(out[0] == kKissFesc);
    }

    TEST_CASE("trailing FESC returns -1")
    {
        uint8_t in[] = {0x01, kKissFesc}; // no byte after FESC
        uint8_t out[8] = {};
        CHECK(KissFramer::unescape(in, sizeof(in), out, sizeof(out)) == -1);
    }

    TEST_CASE("unknown escape sequence returns -1")
    {
        uint8_t in[] = {kKissFesc, 0x01}; // 0x01 is not a valid TFEND/TFESC
        uint8_t out[8] = {};
        CHECK(KissFramer::unescape(in, sizeof(in), out, sizeof(out)) == -1);
    }

    TEST_CASE("FEND inside body returns -1")
    {
        uint8_t in[] = {0x01, kKissFend, 0x02}; // raw FEND in body is malformed
        uint8_t out[8] = {};
        CHECK(KissFramer::unescape(in, sizeof(in), out, sizeof(out)) == -1);
    }

    TEST_CASE("round-trip with all special bytes")
    {
        uint8_t original[] = {0xC0, 0xDB, 0x01, 0xC0};
        uint8_t escaped[16] = {};
        size_t esc_len = KissFramer::escape(original, sizeof(original), escaped, sizeof(escaped));
        REQUIRE(esc_len > 0);

        uint8_t recovered[16] = {};
        int rec_len = KissFramer::unescape(escaped, esc_len, recovered, sizeof(recovered));
        REQUIRE(rec_len == static_cast<int>(sizeof(original)));
        CHECK(memcmp(original, recovered, sizeof(original)) == 0);
    }
}

// ── encode ────────────────────────────────────────────────────────────────────

TEST_SUITE("KissFramer::encode")
{
    TEST_CASE("empty AX.25 frame produces FEND 0x00 FEND")
    {
        uint8_t ax25[] = {};
        uint8_t out[8] = {};
        size_t n = KissFramer::encode(ax25, 0, out, sizeof(out));
        REQUIRE(n == 3);
        CHECK(out[0] == kKissFend);
        CHECK(out[1] == kKissCmdData);
        CHECK(out[2] == kKissFend);
    }

    TEST_CASE("plain AX.25 bytes are sandwiched by FEND + 0x00 + FEND")
    {
        uint8_t ax25[] = {0x01, 0x02, 0x03};
        uint8_t out[16] = {};
        size_t n = KissFramer::encode(ax25, sizeof(ax25), out, sizeof(out));
        REQUIRE(n == 6); // FEND + 0x00 + 3 bytes + FEND
        CHECK(out[0] == kKissFend);
        CHECK(out[1] == kKissCmdData);
        CHECK(out[2] == 0x01);
        CHECK(out[3] == 0x02);
        CHECK(out[4] == 0x03);
        CHECK(out[5] == kKissFend);
    }

    TEST_CASE("0xC0 in AX.25 data is escaped")
    {
        uint8_t ax25[] = {0xC0};
        uint8_t out[16] = {};
        size_t n = KissFramer::encode(ax25, 1, out, sizeof(out));
        // FEND + 0x00 + 0xDB 0xDC + FEND = 5 bytes
        REQUIRE(n == 5);
        CHECK(out[0] == kKissFend);
        CHECK(out[1] == kKissCmdData);
        CHECK(out[2] == kKissFesc);
        CHECK(out[3] == kKissTfend);
        CHECK(out[4] == kKissFend);
    }

    TEST_CASE("0xDB in AX.25 data is escaped")
    {
        uint8_t ax25[] = {0xDB};
        uint8_t out[16] = {};
        size_t n = KissFramer::encode(ax25, 1, out, sizeof(out));
        REQUIRE(n == 5);
        CHECK(out[0] == kKissFend);
        CHECK(out[2] == kKissFesc);
        CHECK(out[3] == kKissTfesc);
    }

    TEST_CASE("returns 0 when output buffer is too small")
    {
        uint8_t ax25[] = {0x01, 0x02, 0x03};
        uint8_t out[4] = {}; // not enough room
        CHECK(KissFramer::encode(ax25, sizeof(ax25), out, sizeof(out)) == 0);
    }

    TEST_CASE("returns 0 when output buffer is less than 3 bytes")
    {
        uint8_t ax25[] = {};
        uint8_t out[2] = {};
        CHECK(KissFramer::encode(ax25, 0, out, sizeof(out)) == 0);
    }

    TEST_CASE("null ax25_data returns 0")
    {
        uint8_t out[16] = {};
        CHECK(KissFramer::encode(nullptr, 4, out, sizeof(out)) == 0);
    }
}

// ── decode ────────────────────────────────────────────────────────────────────

TEST_SUITE("KissFramer::decode")
{
    TEST_CASE("basic data frame with FENDs")
    {
        uint8_t frame[] = {kKissFend, 0x00, 0x01, 0x02, 0x03, kKissFend};
        uint8_t ax25[16] = {};
        uint8_t cmd = 0xFF;
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25), &cmd);
        REQUIRE(n == 3);
        CHECK(cmd == kKissCmdData);
        CHECK(ax25[0] == 0x01);
        CHECK(ax25[1] == 0x02);
        CHECK(ax25[2] == 0x03);
    }

    TEST_CASE("frame without leading/trailing FENDs still decoded")
    {
        uint8_t frame[] = {0x00, 0x01, 0x02, 0x03};
        uint8_t ax25[16] = {};
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25));
        REQUIRE(n == 3);
        CHECK(ax25[0] == 0x01);
        CHECK(ax25[1] == 0x02);
        CHECK(ax25[2] == 0x03);
    }

    TEST_CASE("multiple leading FENDs are stripped")
    {
        uint8_t frame[] = {kKissFend, kKissFend, 0x00, 0xAA, kKissFend};
        uint8_t ax25[16] = {};
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25));
        REQUIRE(n == 1);
        CHECK(ax25[0] == 0xAA);
    }

    TEST_CASE("empty frame (only FENDs) returns -1")
    {
        uint8_t frame[] = {kKissFend, kKissFend};
        uint8_t ax25[16] = {};
        CHECK(KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25)) == -1);
    }

    TEST_CASE("zero-length input returns -1")
    {
        uint8_t frame[] = {};
        uint8_t ax25[16] = {};
        CHECK(KissFramer::decode(frame, 0, ax25, sizeof(ax25)) == -1);
    }

    TEST_CASE("escaped FEND in body is unescaped correctly")
    {
        // Frame body contains 0xDB 0xDC (escaped 0xC0)
        uint8_t frame[] = {kKissFend, 0x00, kKissFesc, kKissTfend, kKissFend};
        uint8_t ax25[16] = {};
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25));
        REQUIRE(n == 1);
        CHECK(ax25[0] == kKissFend);
    }

    TEST_CASE("escaped FESC in body is unescaped correctly")
    {
        uint8_t frame[] = {kKissFend, 0x00, kKissFesc, kKissTfesc, kKissFend};
        uint8_t ax25[16] = {};
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25));
        REQUIRE(n == 1);
        CHECK(ax25[0] == kKissFesc);
    }

    TEST_CASE("malformed escape sequence returns -1")
    {
        uint8_t frame[] = {0x00, kKissFesc, 0x01}; // 0x01 not valid after FESC
        uint8_t ax25[16] = {};
        CHECK(KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25)) == -1);
    }

    TEST_CASE("return-from-KISS (0x0F) returns 0 with cmd set")
    {
        uint8_t frame[] = {kKissFend, kKissCmdReturnFromKiss, kKissFend};
        uint8_t ax25[16] = {};
        uint8_t cmd = 0;
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25), &cmd);
        CHECK(n == 0);
        CHECK(cmd == kKissCmdReturnFromKiss);
    }

    TEST_CASE("extended command 0x01 returns 0 (no-op in MVP)")
    {
        uint8_t frame[] = {0x01, 0x00}; // TXDELAY command, port 0
        uint8_t ax25[16] = {};
        uint8_t cmd = 0;
        int n = KissFramer::decode(frame, sizeof(frame), ax25, sizeof(ax25), &cmd);
        CHECK(n == 0);
        CHECK(cmd == 0x01);
    }

    TEST_CASE("oversize frame returns -1")
    {
        // Construct a frame with body length > kKissMaxFrame
        std::vector<uint8_t> frame;
        frame.push_back(kKissFend);
        frame.push_back(0x00); // cmd
        for (size_t i = 0; i <= kKissMaxFrame; ++i) frame.push_back(0xAA);
        frame.push_back(kKissFend);

        std::vector<uint8_t> ax25(kKissMaxFrame + 4);
        int n = KissFramer::decode(frame.data(), frame.size(),
                                   ax25.data(), ax25.size());
        CHECK(n == -1);
    }

    TEST_CASE("null input returns -1")
    {
        uint8_t ax25[16] = {};
        CHECK(KissFramer::decode(nullptr, 4, ax25, sizeof(ax25)) == -1);
    }

    TEST_CASE("null output buffer returns -1")
    {
        uint8_t frame[] = {0x00, 0x01};
        CHECK(KissFramer::decode(frame, sizeof(frame), nullptr, 16) == -1);
    }
}

// ── encode → decode round-trip ────────────────────────────────────────────────

TEST_SUITE("KissFramer encode/decode round-trip")
{
    TEST_CASE("round-trip: simple AX.25 bytes")
    {
        uint8_t original[] = {0x82, 0x84, 0x86, 0x40, 0x40, 0x40, 0xE0,
                              0xA6, 0x60, 0x9A, 0x40, 0x40, 0x40, 0x61,
                              0x03, 0xF0, 0x21, 0x31, 0x32, 0x33};

        uint8_t encoded[512];
        size_t enc_len = KissFramer::encode(original, sizeof(original),
                                            encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        uint8_t recovered[512];
        int rec_len = KissFramer::decode(encoded, enc_len,
                                         recovered, sizeof(recovered));
        REQUIRE(rec_len == static_cast<int>(sizeof(original)));
        CHECK(memcmp(original, recovered, sizeof(original)) == 0);
    }

    TEST_CASE("round-trip: frame containing all special byte values")
    {
        uint8_t original[] = {0xC0, 0xDB, 0xC0, 0xDB, 0x01};
        uint8_t encoded[64];
        size_t enc_len = KissFramer::encode(original, sizeof(original),
                                            encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        uint8_t recovered[64];
        int rec_len = KissFramer::decode(encoded, enc_len,
                                         recovered, sizeof(recovered));
        REQUIRE(rec_len == static_cast<int>(sizeof(original)));
        CHECK(memcmp(original, recovered, sizeof(original)) == 0);
    }

    TEST_CASE("round-trip: empty AX.25 payload")
    {
        uint8_t encoded[16];
        size_t enc_len = KissFramer::encode(nullptr, 0, encoded, sizeof(encoded));
        // encode with null/0-len should produce FEND 0x00 FEND (3 bytes)
        // But encode returns 0 for null input; use empty array instead
        uint8_t empty[] = {};
        enc_len = KissFramer::encode(empty, 0, encoded, sizeof(encoded));
        REQUIRE(enc_len == 3);

        uint8_t recovered[16];
        int rec_len = KissFramer::decode(encoded, enc_len, recovered, sizeof(recovered));
        CHECK(rec_len == 0); // empty frame body → 0 AX.25 bytes
    }

    TEST_CASE("round-trip: max MVP frame size (330 bytes)")
    {
        std::vector<uint8_t> original(kKissMaxFrame, 0x42);
        std::vector<uint8_t> encoded(kKissMaxFrame * 2 + 4);
        size_t enc_len = KissFramer::encode(original.data(), original.size(),
                                            encoded.data(), encoded.size());
        REQUIRE(enc_len > 0);

        std::vector<uint8_t> recovered(kKissMaxFrame + 4);
        int rec_len = KissFramer::decode(encoded.data(), enc_len,
                                         recovered.data(), recovered.size());
        REQUIRE(rec_len == static_cast<int>(kKissMaxFrame));
        CHECK(memcmp(original.data(), recovered.data(), kKissMaxFrame) == 0);
    }
}
