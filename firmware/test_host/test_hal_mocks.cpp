// Unit tests for HAL mock implementations (Step 1 / FW-002)
//
// These tests verify that:
//   1. Each mock correctly implements its interface contract.
//   2. Safety invariants hold (e.g. PTT=off on failure, healthy=false before init).
// They also act as living documentation of the interface contracts.

#include "doctest/doctest.h"

#include "pakt/IAudioIO.h"
#include "pakt/IPacketLink.h"
#include "pakt/IRadioControl.h"
#include "pakt/IStorage.h"

#include "AudioIOMock.h"
#include "PacketLinkMock.h"
#include "RadioControlMock.h"

// ── AudioIOMock ───────────────────────────────────────────────────────────────

TEST_SUITE("AudioIOMock")
{
    TEST_CASE("unhealthy before init")
    {
        pakt::mock::AudioIOMock audio;
        CHECK_FALSE(audio.is_healthy());
    }

    TEST_CASE("init rejects unsupported sample rates")
    {
        pakt::mock::AudioIOMock audio;
        CHECK_FALSE(audio.init(44100));
        CHECK_FALSE(audio.init(22050));
        CHECK_FALSE(audio.init(48000));
        CHECK_FALSE(audio.is_healthy());
    }

    TEST_CASE("init accepts 8 kHz (primary APRS rate)")
    {
        pakt::mock::AudioIOMock audio;
        CHECK(audio.init(8000));
        CHECK(audio.is_healthy());
        CHECK(audio.sample_rate() == 8000u);
    }

    TEST_CASE("init accepts 16 kHz (secondary rate)")
    {
        pakt::mock::AudioIOMock audio;
        CHECK(audio.init(16000));
        CHECK(audio.is_healthy());
    }

    TEST_CASE("read returns 0 when no data is available")
    {
        pakt::mock::AudioIOMock audio;
        audio.init(8000);
        int16_t buf[64];
        CHECK(audio.read_samples(buf, 64) == 0);
    }

    TEST_CASE("read returns 0 when unhealthy")
    {
        pakt::mock::AudioIOMock audio; // not initialised
        int16_t buf[4];
        CHECK(audio.read_samples(buf, 4) == 0);
    }

    TEST_CASE("inject then read round-trip")
    {
        pakt::mock::AudioIOMock audio;
        audio.init(8000);
        int16_t in[4] = {100, 200, 300, 400};
        audio.inject_rx(in, 4);
        int16_t out[4] = {};
        CHECK(audio.read_samples(out, 4) == 4);
        CHECK(out[0] == 100);
        CHECK(out[1] == 200);
        CHECK(out[2] == 300);
        CHECK(out[3] == 400);
    }

    TEST_CASE("read is non-blocking: partial read when buf smaller than available")
    {
        pakt::mock::AudioIOMock audio;
        audio.init(8000);
        int16_t in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        audio.inject_rx(in, 8);
        int16_t out[4] = {};
        size_t n = audio.read_samples(out, 4);
        CHECK(n == 4);
        // Remaining 4 samples are still in the queue
        CHECK(audio.read_samples(out, 4) == 4);
    }

    TEST_CASE("write captures samples when healthy")
    {
        pakt::mock::AudioIOMock audio;
        audio.init(8000);
        int16_t samples[3] = {10, 20, 30};
        CHECK(audio.write_samples(samples, 3) == 3);
        REQUIRE(audio.captured_tx().size() == 3);
        CHECK(audio.captured_tx()[0] == 10);
        CHECK(audio.captured_tx()[2] == 30);
    }

    TEST_CASE("write returns 0 when unhealthy")
    {
        pakt::mock::AudioIOMock audio; // not initialised
        int16_t buf[1] = {0};
        CHECK(audio.write_samples(buf, 1) == 0);
    }

    TEST_CASE("reinit restores health")
    {
        pakt::mock::AudioIOMock audio; // not initialised → unhealthy
        CHECK_FALSE(audio.is_healthy());
        CHECK(audio.reinit());
        CHECK(audio.is_healthy());
    }

    TEST_CASE("clear_tx empties capture buffer")
    {
        pakt::mock::AudioIOMock audio;
        audio.init(8000);
        int16_t s[2] = {1, 2};
        audio.write_samples(s, 2);
        CHECK(audio.captured_tx().size() == 2);
        audio.clear_tx();
        CHECK(audio.captured_tx().empty());
    }
}

// ── RadioControlMock ──────────────────────────────────────────────────────────

TEST_SUITE("RadioControlMock")
{
    TEST_CASE("PTT is off by default (safe state)")
    {
        pakt::mock::RadioControlMock radio;
        CHECK_FALSE(radio.is_transmitting());
    }

    TEST_CASE("ptt(true) before init is rejected and PTT stays off")
    {
        pakt::mock::RadioControlMock radio;
        CHECK_FALSE(radio.ptt(true));
        CHECK_FALSE(radio.is_transmitting());
    }

    TEST_CASE("init succeeds and PTT remains off")
    {
        pakt::mock::RadioControlMock radio;
        CHECK(radio.init());
        CHECK(radio.is_initialized());
        CHECK_FALSE(radio.is_transmitting());
    }

    TEST_CASE("ptt on/off cycle after init")
    {
        pakt::mock::RadioControlMock radio;
        radio.init();
        CHECK(radio.ptt(true));
        CHECK(radio.is_transmitting());
        CHECK(radio.ptt(false));
        CHECK_FALSE(radio.is_transmitting());
    }

    TEST_CASE("ptt(false) is always safe to call")
    {
        pakt::mock::RadioControlMock radio;
        // Before init
        CHECK_FALSE(radio.ptt(false));   // returns false but doesn't assert
        CHECK_FALSE(radio.is_transmitting());
        // After init
        radio.init();
        CHECK(radio.ptt(false));
        CHECK_FALSE(radio.is_transmitting());
    }

    TEST_CASE("set_freq is idempotent after init")
    {
        pakt::mock::RadioControlMock radio;
        radio.init();
        CHECK(radio.set_freq(144390000u, 144390000u));
        CHECK(radio.rx_freq() == 144390000u);
        CHECK(radio.tx_freq() == 144390000u);
        // Calling again with same value must succeed
        CHECK(radio.set_freq(144390000u, 144390000u));
        CHECK(radio.rx_freq() == 144390000u);
    }

    TEST_CASE("set_freq before init returns false")
    {
        pakt::mock::RadioControlMock radio;
        CHECK_FALSE(radio.set_freq(144390000u, 144390000u));
    }

    TEST_CASE("EU APRS frequency is accepted")
    {
        pakt::mock::RadioControlMock radio;
        radio.init();
        CHECK(radio.set_freq(144800000u, 144800000u));
        CHECK(radio.rx_freq() == 144800000u);
    }

    TEST_CASE("set_squelch records value")
    {
        pakt::mock::RadioControlMock radio;
        radio.init();
        CHECK(radio.set_squelch(3));
        CHECK(radio.squelch() == 3);
    }

    TEST_CASE("set_power records value")
    {
        pakt::mock::RadioControlMock radio;
        radio.init();
        CHECK(radio.set_power(pakt::RadioPower::High));
        CHECK(radio.power() == pakt::RadioPower::High);
    }
}

// ── PacketLinkMock ────────────────────────────────────────────────────────────

TEST_SUITE("PacketLinkMock")
{
    TEST_CASE("recv returns false when queue is empty")
    {
        pakt::mock::PacketLinkMock link;
        uint8_t buf[256];
        pakt::Ax25Frame f{buf, 0};
        CHECK_FALSE(link.recv(f, 256));
        CHECK(link.rx_available() == 0);
    }

    TEST_CASE("inject_rx then recv round-trip")
    {
        pakt::mock::PacketLinkMock link;
        uint8_t payload[] = {0xAA, 0xBB, 0xCC};
        link.inject_rx(payload, 3);
        CHECK(link.rx_available() == 1);

        uint8_t out[16];
        pakt::Ax25Frame f{out, 0};
        CHECK(link.recv(f, 16));
        CHECK(f.length == 3);
        CHECK(out[0] == 0xAA);
        CHECK(out[1] == 0xBB);
        CHECK(out[2] == 0xCC);
        CHECK(link.rx_available() == 0);
    }

    TEST_CASE("recv truncates to max_len")
    {
        pakt::mock::PacketLinkMock link;
        uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        link.inject_rx(payload, 8);

        uint8_t out[4];
        pakt::Ax25Frame f{out, 0};
        CHECK(link.recv(f, 4));
        CHECK(f.length == 4);
    }

    TEST_CASE("send queues frame and tx_free decreases")
    {
        pakt::mock::PacketLinkMock link;
        size_t initial_free = link.tx_free();
        CHECK(initial_free == pakt::mock::PacketLinkMock::kMaxQueueDepth);

        uint8_t payload[] = {0x01, 0x02};
        pakt::Ax25Frame f{payload, 2};
        CHECK(link.send(f));
        CHECK(link.tx_free() == initial_free - 1);
        CHECK(link.has_tx());
    }

    TEST_CASE("send returns false when TX queue is full")
    {
        pakt::mock::PacketLinkMock link;
        uint8_t payload[] = {0x01};
        pakt::Ax25Frame f{payload, 1};
        for (size_t i = 0; i < pakt::mock::PacketLinkMock::kMaxQueueDepth; ++i) {
            CHECK(link.send(f));
        }
        CHECK_FALSE(link.send(f));
        CHECK(link.tx_free() == 0);
    }

    TEST_CASE("pop_tx returns sent frame bytes")
    {
        pakt::mock::PacketLinkMock link;
        uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
        pakt::Ax25Frame f{payload, 4};
        link.send(f);
        auto popped = link.pop_tx();
        REQUIRE(popped.size() == 4);
        CHECK(popped[0] == 0xDE);
        CHECK(popped[3] == 0xEF);
    }

    TEST_CASE("multiple frames queue independently")
    {
        pakt::mock::PacketLinkMock link;
        uint8_t a[] = {0x01};
        uint8_t b[] = {0x02};
        link.inject_rx(a, 1);
        link.inject_rx(b, 1);
        CHECK(link.rx_available() == 2);

        uint8_t out[4];
        pakt::Ax25Frame f{out, 0};
        CHECK(link.recv(f, 4));
        CHECK(out[0] == 0x01);
        CHECK(link.recv(f, 4));
        CHECK(out[0] == 0x02);
    }
}

// ── IStorage – default_config() contract ─────────────────────────────────────

TEST_SUITE("DeviceConfig")
{
    TEST_CASE("default_config has correct schema version")
    {
        auto cfg = pakt::default_config();
        CHECK(cfg.schema_version == pakt::kConfigSchemaVersion);
    }

    TEST_CASE("default_config has no pre-set callsign (must be user-supplied)")
    {
        auto cfg = pakt::default_config();
        CHECK(cfg.callsign[0] == '\0');
    }

    TEST_CASE("default_config has no pre-set APRS frequency (requires explicit region selection)")
    {
        auto cfg = pakt::default_config();
        CHECK(cfg.aprs_rx_freq_hz == 0u);
        CHECK(cfg.aprs_tx_freq_hz == 0u);
    }

    TEST_CASE("default_config has beaconing disabled")
    {
        auto cfg = pakt::default_config();
        CHECK(cfg.beacon_interval_s == 0u);
    }

    TEST_CASE("default_config uses primary symbol table")
    {
        auto cfg = pakt::default_config();
        CHECK(cfg.aprs_symbol_table == '/');
    }
}
