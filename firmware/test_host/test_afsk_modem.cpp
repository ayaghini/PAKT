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

// ── TX buffer sizing ──────────────────────────────────────────────────────────
//
// Verify that a max-size AX.25 frame fits within kAfskMaxPcmSamples (25 600).
// This guards against the g_tx_pcm_buf in main.cpp being too small, which
// would cause AfskModulator::modulate_frame() to silently truncate output.

TEST_SUITE("AfskModulator TX buffer sizing")
{
    static constexpr size_t kAfskMaxPcmSamples = 25600; // must match main.cpp

    TEST_CASE("max-encoded AX.25 frame fits in 25 600 samples at 8 kHz")
    {
        // Worst-case AX.25 frame: kMaxEncodedLen bytes, all 0xFF (maximum bit stuffing).
        // 0xFF triggers a stuffed 0-bit after every 5 ones → up to 1.2× raw bit count.
        std::vector<uint8_t> worst(pakt::ax25::kMaxEncodedLen, 0xFF);

        pakt::AfskModulator mod(kSampleRate);
        std::vector<int16_t> big(kAfskMaxPcmSamples);
        size_t n = mod.modulate_frame(worst.data(), worst.size(),
                                      big.data(), big.size());

        // Must produce samples (non-zero) and must fit (no truncation).
        CHECK(n > 0);
        CHECK(n <= kAfskMaxPcmSamples);
        // Sanity: at least preamble-only samples.
        CHECK(n >= static_cast<size_t>(pakt::AfskModulator::kPreambleFlags * 8
                                       * kSampleRate / pakt::AfskModulator::kBaudRate));
    }

    TEST_CASE("zero-byte frame (degenerate) produces preamble+tail only")
    {
        // An empty data field is invalid, but modulate_frame should return 0
        // for a null pointer and non-zero for a valid-but-empty buffer.
        pakt::AfskModulator mod(kSampleRate);
        std::vector<int16_t> out(kAfskMaxPcmSamples);

        // Null pointer → must return 0.
        CHECK(mod.modulate_frame(nullptr, 0, out.data(), out.size()) == 0);

        // Empty but non-null payload: APRS packets always have at least FCS,
        // but test the boundary — modulate_frame returns 0 when data_len == 0.
        const uint8_t empty[] = {};
        CHECK(mod.modulate_frame(empty, 0, out.data(), out.size()) == 0);
    }

    TEST_CASE("output buffer exactly 1 sample too small returns 0")
    {
        // Compute how many samples a tiny frame needs, then pass a buffer
        // one sample smaller — modulate_frame should return 0 (no truncated output).
        const uint8_t tiny[] = {0xC0, 0xC0, 0xC0, 0xC0};  // 4 bytes, minimal stuffing
        pakt::AfskModulator mod(kSampleRate);

        // First, find the real sample count with a large buffer.
        std::vector<int16_t> big(kAfskMaxPcmSamples);
        size_t real_n = mod.modulate_frame(tiny, sizeof(tiny), big.data(), big.size());
        REQUIRE(real_n > 0);

        // Now pass a buffer one sample short.
        mod.reset();
        std::vector<int16_t> small(real_n - 1);
        size_t trunc_n = mod.modulate_frame(tiny, sizeof(tiny), small.data(), small.size());
        CHECK(trunc_n == 0);
    }

    TEST_CASE("exact-fit buffer returns sample count, not 0")
    {
        // Verify that a buffer of exactly the right size succeeds.
        // The previous post-hoc (pos == out_max) check incorrectly returned 0
        // for this case because it could not distinguish exact-fit from truncation.
        // The corrected implementation detects truncation inline (before-write check)
        // so exact-fit is unambiguously successful.
        const uint8_t tiny[] = {0xC0, 0xC0, 0xC0, 0xC0};
        pakt::AfskModulator mod(kSampleRate);

        std::vector<int16_t> big(kAfskMaxPcmSamples);
        size_t real_n = mod.modulate_frame(tiny, sizeof(tiny), big.data(), big.size());
        REQUIRE(real_n > 0);
        REQUIRE(real_n < kAfskMaxPcmSamples); // sanity: must not have already saturated

        // Exact-fit: buffer is exactly real_n samples.
        mod.reset();
        std::vector<int16_t> exact(real_n);
        size_t n = mod.modulate_frame(tiny, sizeof(tiny), exact.data(), exact.size());
        CHECK(n == real_n);
    }

    TEST_CASE("significantly undersized buffer returns 0")
    {
        // A buffer with only 10 samples cannot hold even the preamble —
        // truncation should be detected early and 0 returned.
        const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        pakt::AfskModulator mod(kSampleRate);
        std::vector<int16_t> tiny_buf(10);
        size_t n = mod.modulate_frame(data, sizeof(data), tiny_buf.data(), tiny_buf.size());
        CHECK(n == 0);
    }

    TEST_CASE("APRS encode_message + ax25::encode round-trip within buffer")
    {
        // Simulate the full RadioTxFn encoding path from aprs_task.
        const char *from_call = "W1AW";
        const char *to_call   = "K1XYZ";
        const char *text      = "Test APRS message via PAKT firmware";
        const char *msg_id    = "1";

        uint8_t info_buf[pakt::ax25::kMaxInfoLen];
        size_t  info_len = pakt::aprs::encode_message(
            to_call, 0, text, msg_id, info_buf, sizeof(info_buf));
        REQUIRE(info_len > 0);

        pakt::ax25::Frame frame = pakt::aprs::make_ui_frame(from_call, 0);
        std::memcpy(frame.info, info_buf, info_len);
        frame.info_len = info_len;

        uint8_t ax25_buf[pakt::ax25::kMaxEncodedLen];
        size_t  ax25_len = pakt::ax25::encode(frame, ax25_buf, sizeof(ax25_buf));
        REQUIRE(ax25_len > 0);

        // Modulate and verify sample count is within the TX buffer limit.
        pakt::AfskModulator mod(kSampleRate);
        std::vector<int16_t> out(kAfskMaxPcmSamples);
        size_t n = mod.modulate_frame(ax25_buf, ax25_len, out.data(), out.size());
        CHECK(n > 0);
        CHECK(n <= kAfskMaxPcmSamples);
    }
}
