// Unit tests for AFSK modem (Step 3 / FW-006, FW-007)
//
// Key test: full pipeline round-trip.
//   1. Build an AX.25 frame with ax25::encode().
//   2. Modulate it to PCM samples with AfskModulator.
//   3. Feed samples to AfskDemodulator.
//   4. Verify the decoded raw bytes match the original.
//   5. Verify ax25::decode() on the recovered bytes gives the original frame.
//
// All tests run on the host CPU — no hardware required.

#include "doctest/doctest.h"

#include "pakt/AfskDemodulator.h"
#include "pakt/AfskModulator.h"
#include "pakt/Aprs.h"
#include "pakt/Ax25.h"

#include <cstring>
#include <vector>

static constexpr uint32_t kSampleRate = 8000;

// ── AfskModulator unit tests ──────────────────────────────────────────────────

TEST_SUITE("AfskModulator")
{
    TEST_CASE("modulate_frame returns non-zero sample count")
    {
        pakt::AfskModulator mod(kSampleRate);

        // Minimal AX.25 frame
        auto frame = pakt::aprs::make_ui_frame("N0CALL", 0);
        const char *info = "!4903.50N/12310.00W>test";
        frame.info_len = strlen(info);
        memcpy(frame.info, info, frame.info_len);

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        std::vector<int16_t> audio(16384, 0);
        size_t n = mod.modulate_frame(encoded, enc_len, audio.data(), audio.size());
        CHECK(n > 0);
        CHECK(n < audio.size()); // must not overflow
    }

    TEST_CASE("samples are within int16 range")
    {
        pakt::AfskModulator mod(kSampleRate);
        uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x01, 0x02, 0x00, 0x00};
        std::vector<int16_t> audio(8192, 0);
        size_t n = mod.modulate_frame(data, sizeof(data), audio.data(), audio.size());
        REQUIRE(n > 0);
        for (size_t i = 0; i < n; ++i) {
            CHECK(audio[i] >= -32767);
            CHECK(audio[i] <=  32767);
        }
    }

    TEST_CASE("reset clears state without crashing")
    {
        pakt::AfskModulator mod(kSampleRate);
        uint8_t data[] = {0x01, 0x02};
        std::vector<int16_t> audio(4096, 0);
        mod.modulate_frame(data, sizeof(data), audio.data(), audio.size());
        mod.reset(); // must not crash
        // After reset, a second modulation should also succeed
        size_t n = mod.modulate_frame(data, sizeof(data), audio.data(), audio.size());
        CHECK(n > 0);
    }

    TEST_CASE("returns 0 on null inputs")
    {
        pakt::AfskModulator mod(kSampleRate);
        std::vector<int16_t> audio(1024, 0);
        CHECK(mod.modulate_frame(nullptr, 10, audio.data(), audio.size()) == 0);
        uint8_t data[] = {0x01};
        CHECK(mod.modulate_frame(data, 1, nullptr, 1024) == 0);
    }
}

// ── Round-trip tests ──────────────────────────────────────────────────────────

// Helper: modulate a raw AX.25 encoded buffer and return recovered bytes.
static std::vector<uint8_t> modulate_demodulate(const uint8_t *data, size_t len)
{
    std::vector<uint8_t> recovered;

    pakt::AfskDemodulator demod(kSampleRate,
        [&](const uint8_t *frame_data, size_t frame_len) {
            recovered.assign(frame_data, frame_data + frame_len);
        });

    pakt::AfskModulator mod(kSampleRate);
    std::vector<int16_t> audio(32768, 0);
    size_t n = mod.modulate_frame(data, len, audio.data(), audio.size());
    REQUIRE(n > 0);

    // Feed in blocks to simulate streaming audio
    static constexpr size_t kBlockSize = 64;
    for (size_t offset = 0; offset < n; offset += kBlockSize) {
        size_t block = (offset + kBlockSize <= n) ? kBlockSize : (n - offset);
        demod.process(audio.data() + offset, block);
    }

    return recovered;
}

TEST_SUITE("AFSK modem round-trip")
{
    TEST_CASE("minimal frame survives modulate/demodulate with correct FCS")
    {
        auto frame = pakt::aprs::make_ui_frame("N0CALL", 0);
        const char *info = "!4903.50N/12310.00W>beacon";
        frame.info_len = strlen(info);
        memcpy(frame.info, info, frame.info_len);

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        auto recovered = modulate_demodulate(encoded, enc_len);
        REQUIRE(recovered.size() == enc_len);

        // Verify byte-for-byte match
        CHECK(memcmp(recovered.data(), encoded, enc_len) == 0);
    }

    TEST_CASE("decoded bytes parse successfully through ax25::decode")
    {
        auto frame = pakt::aprs::make_ui_frame("VE3XYZ", 9);
        const char *info = "!4500.00N/07500.00W>VE3 test station";
        frame.info_len = strlen(info);
        memcpy(frame.info, info, frame.info_len);

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        auto recovered = modulate_demodulate(encoded, enc_len);
        REQUIRE_FALSE(recovered.empty());

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(recovered.data(), recovered.size(), decoded));

        CHECK(strcmp(decoded.addr[1].callsign, "VE3XYZ") == 0);
        CHECK(decoded.addr[1].ssid == 9);
        CHECK(decoded.info_len == frame.info_len);
        CHECK(memcmp(decoded.info, frame.info, frame.info_len) == 0);
    }

    TEST_CASE("SSID 15 survives round-trip")
    {
        auto frame = pakt::aprs::make_ui_frame("W1AW", 15);
        const char *info = ">Status with SSID 15";
        frame.info_len = strlen(info);
        memcpy(frame.info, info, frame.info_len);

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        auto recovered = modulate_demodulate(encoded, enc_len);
        REQUIRE_FALSE(recovered.empty());

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(recovered.data(), recovered.size(), decoded));
        CHECK(decoded.addr[1].ssid == 15);
    }

    TEST_CASE("message frame survives round-trip")
    {
        auto frame = pakt::aprs::make_ui_frame("N0CALL", 7);
        size_t n = pakt::aprs::encode_message(
            "W1AW", 0, "Hello from PAKT", "042",
            frame.info, pakt::ax25::kMaxInfoLen);
        REQUIRE(n > 0);
        frame.info_len = n;

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        auto recovered = modulate_demodulate(encoded, enc_len);
        REQUIRE_FALSE(recovered.empty());

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(recovered.data(), recovered.size(), decoded));
        CHECK((char)decoded.info[0] == pakt::aprs::kTypeMessage);
        CHECK(strstr((const char *)decoded.info, "Hello from PAKT") != nullptr);
    }

    TEST_CASE("frame with all-ones byte (max bit stuffing) survives round-trip")
    {
        // 0xFF = 11111111 triggers bit stuffing every 5 bits — good stress test
        auto frame = pakt::aprs::make_ui_frame("K1TTT", 0);
        // Put a 0xFF byte inside the info field (padded around printable chars)
        const uint8_t info[] = "!ones:\xFF:end";
        frame.info_len = sizeof(info) - 1;
        memcpy(frame.info, info, frame.info_len);

        uint8_t encoded[pakt::ax25::kMaxEncodedLen];
        size_t enc_len = pakt::ax25::encode(frame, encoded, sizeof(encoded));
        REQUIRE(enc_len > 0);

        auto recovered = modulate_demodulate(encoded, enc_len);
        REQUIRE_FALSE(recovered.empty());

        pakt::ax25::Frame decoded{};
        REQUIRE(pakt::ax25::decode(recovered.data(), recovered.size(), decoded));
        CHECK(decoded.info_len == frame.info_len);
        CHECK(memcmp(decoded.info, frame.info, frame.info_len) == 0);
    }
}
