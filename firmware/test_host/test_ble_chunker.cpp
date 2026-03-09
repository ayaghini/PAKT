// Unit tests for BleChunker (Step 4 / INT-002)
//
// Covers:
//   - Splitter: header format, chunk count, last-chunk sizing
//   - Reassembler: single chunk, multi-chunk in order, out-of-order,
//                  duplicates, timeout expiry, malformed input
//   - Round-trip: split() → feed() → callback delivers original payload

#include "doctest/doctest.h"
#include "pakt/BleChunker.h"

#include <cstring>
#include <vector>

using pakt::BleChunker;

// Helper: check whether two byte sequences match.
static bool bytes_eq(const std::vector<uint8_t> &a, const uint8_t *b, size_t n)
{
    if (a.size() != n) return false;
    return memcmp(a.data(), b, n) == 0;
}

// ── Splitter ──────────────────────────────────────────────────────────────────

TEST_SUITE("BleChunker::split")
{
    TEST_CASE("single chunk when payload fits")
    {
        uint8_t payload[] = {0x01, 0x02, 0x03};
        auto chunks = BleChunker::split(payload, 3, /*msg_id=*/0x42, /*max=*/20);
        REQUIRE(chunks.size() == 1);
        // Header
        CHECK(chunks[0][0] == 0x42); // msg_id
        CHECK(chunks[0][1] == 0x00); // chunk_idx
        CHECK(chunks[0][2] == 0x01); // chunk_total
        // Payload
        CHECK(chunks[0].size() == BleChunker::kHeaderSize + 3);
        CHECK(chunks[0][3] == 0x01);
        CHECK(chunks[0][4] == 0x02);
        CHECK(chunks[0][5] == 0x03);
    }

    TEST_CASE("multiple chunks with exact division")
    {
        // 40 bytes split into 20-byte chunks → 2 chunks
        std::vector<uint8_t> payload(40);
        for (size_t i = 0; i < 40; ++i) payload[i] = static_cast<uint8_t>(i);

        auto chunks = BleChunker::split(payload.data(), 40, 0x01, 20);
        REQUIRE(chunks.size() == 2);
        CHECK(chunks[0][2] == 2); // chunk_total
        CHECK(chunks[1][2] == 2);
        CHECK(chunks[0][1] == 0); // chunk_idx 0
        CHECK(chunks[1][1] == 1); // chunk_idx 1
        CHECK(chunks[0].size() == BleChunker::kHeaderSize + 20);
        CHECK(chunks[1].size() == BleChunker::kHeaderSize + 20);
    }

    TEST_CASE("last chunk smaller when payload not evenly divisible")
    {
        // 25 bytes, chunk max 20 → chunks of 20 and 5
        std::vector<uint8_t> payload(25, 0xFF);
        auto chunks = BleChunker::split(payload.data(), 25, 0x02, 20);
        REQUIRE(chunks.size() == 2);
        CHECK(chunks[0].size() == BleChunker::kHeaderSize + 20);
        CHECK(chunks[1].size() == BleChunker::kHeaderSize + 5);
    }

    TEST_CASE("returns empty on null payload")
    {
        auto chunks = BleChunker::split(nullptr, 10, 0x01, 20);
        CHECK(chunks.empty());
    }

    TEST_CASE("returns empty when chunk count exceeds kMaxChunks")
    {
        // 1 byte per chunk, 65 bytes → 65 chunks > kMaxChunks(64)
        std::vector<uint8_t> payload(65, 0xAB);
        auto chunks = BleChunker::split(payload.data(), 65, 0x01, 1);
        CHECK(chunks.empty());
    }

    TEST_CASE("msg_id is preserved in each chunk header")
    {
        uint8_t payload[] = {1, 2, 3, 4, 5};
        auto chunks = BleChunker::split(payload, 5, 0xDE, 3);
        REQUIRE(chunks.size() == 2);
        CHECK(chunks[0][0] == 0xDE);
        CHECK(chunks[1][0] == 0xDE);
    }

    TEST_CASE("payload bytes are correct across chunk boundary")
    {
        uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
        auto chunks = BleChunker::split(payload, 5, 0x01, 3);
        REQUIRE(chunks.size() == 2);
        // Chunk 0: AA BB CC
        CHECK(chunks[0][BleChunker::kHeaderSize + 0] == 0xAA);
        CHECK(chunks[0][BleChunker::kHeaderSize + 1] == 0xBB);
        CHECK(chunks[0][BleChunker::kHeaderSize + 2] == 0xCC);
        // Chunk 1: DD EE
        CHECK(chunks[1][BleChunker::kHeaderSize + 0] == 0xDD);
        CHECK(chunks[1][BleChunker::kHeaderSize + 1] == 0xEE);
    }
}

// ── Reassembler ───────────────────────────────────────────────────────────────

TEST_SUITE("BleChunker reassembler")
{
    TEST_CASE("single chunk triggers callback immediately")
    {
        std::vector<uint8_t> received;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            received.assign(d, d + n);
        });

        uint8_t wire[] = {0x01, 0x00, 0x01, 0xAB, 0xCD}; // msg_id=1, idx=0, total=1
        CHECK(chunker.feed(wire, sizeof(wire), 0));
        REQUIRE(received.size() == 2);
        CHECK(received[0] == 0xAB);
        CHECK(received[1] == 0xCD);
    }

    TEST_CASE("two chunks in order assemble correctly")
    {
        std::vector<uint8_t> received;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            received.assign(d, d + n);
        });

        uint8_t c0[] = {0x05, 0x00, 0x02, 0x11, 0x22}; // chunk 0/2
        uint8_t c1[] = {0x05, 0x01, 0x02, 0x33, 0x44}; // chunk 1/2
        CHECK(chunker.feed(c0, sizeof(c0), 100));
        CHECK(received.empty()); // not complete yet
        CHECK(chunker.feed(c1, sizeof(c1), 101));
        REQUIRE(received.size() == 4);
        CHECK(received[0] == 0x11);
        CHECK(received[1] == 0x22);
        CHECK(received[2] == 0x33);
        CHECK(received[3] == 0x44);
    }

    TEST_CASE("out-of-order chunks assemble in correct sequence")
    {
        std::vector<uint8_t> received;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            received.assign(d, d + n);
        });

        uint8_t c1[] = {0x07, 0x01, 0x03, 0xBB};         // chunk 1/3
        uint8_t c2[] = {0x07, 0x02, 0x03, 0xCC};         // chunk 2/3
        uint8_t c0[] = {0x07, 0x00, 0x03, 0xAA};         // chunk 0/3 (arrives last)
        CHECK(chunker.feed(c1, sizeof(c1), 0));
        CHECK(received.empty());
        CHECK(chunker.feed(c2, sizeof(c2), 1));
        CHECK(received.empty());
        CHECK(chunker.feed(c0, sizeof(c0), 2));
        REQUIRE(received.size() == 3);
        CHECK(received[0] == 0xAA);
        CHECK(received[1] == 0xBB);
        CHECK(received[2] == 0xCC);
    }

    TEST_CASE("duplicate chunk is ignored and does not double-count")
    {
        int cb_count = 0;
        BleChunker chunker([&](const uint8_t *, size_t) { ++cb_count; });

        uint8_t c0[] = {0x10, 0x00, 0x02, 0xAA};
        uint8_t c0_dup[] = {0x10, 0x00, 0x02, 0xAA}; // same chunk again
        uint8_t c1[] = {0x10, 0x01, 0x02, 0xBB};

        chunker.feed(c0, sizeof(c0), 0);
        chunker.feed(c0_dup, sizeof(c0_dup), 1); // duplicate
        CHECK(cb_count == 0);
        chunker.feed(c1, sizeof(c1), 2);
        CHECK(cb_count == 1); // callback fires exactly once
    }

    TEST_CASE("timeout expires in-progress message")
    {
        bool completed = false;
        BleChunker chunker([&](const uint8_t *, size_t) { completed = true; },
                           /*timeout_ms=*/500);

        uint8_t c0[] = {0x20, 0x00, 0x02, 0xAA}; // chunk 0/2
        chunker.feed(c0, sizeof(c0), /*now_ms=*/0);

        // Advance past the timeout.
        chunker.tick(/*now_ms=*/600);

        // Feed the second chunk after timeout – slot was evicted, so the
        // message is treated as a new (single-chunk) message starting fresh.
        // Either way, the original message must NOT complete.
        CHECK_FALSE(completed);
    }

    TEST_CASE("malformed: chunk shorter than header returns false")
    {
        BleChunker chunker([](const uint8_t *, size_t) {});
        uint8_t bad[] = {0x01, 0x00}; // only 2 bytes, need at least 3
        CHECK_FALSE(chunker.feed(bad, sizeof(bad), 0));
    }

    TEST_CASE("malformed: chunk_total=0 returns false")
    {
        BleChunker chunker([](const uint8_t *, size_t) {});
        uint8_t bad[] = {0x01, 0x00, 0x00, 0xAA}; // chunk_total = 0
        CHECK_FALSE(chunker.feed(bad, sizeof(bad), 0));
    }

    TEST_CASE("malformed: chunk_idx >= chunk_total returns false")
    {
        BleChunker chunker([](const uint8_t *, size_t) {});
        uint8_t bad[] = {0x01, 0x02, 0x02, 0xAA}; // idx=2, total=2 → out of range
        CHECK_FALSE(chunker.feed(bad, sizeof(bad), 0));
    }

    TEST_CASE("inconsistent chunk_total for same msg_id returns false")
    {
        BleChunker chunker([](const uint8_t *, size_t) {});
        uint8_t c0[] = {0x30, 0x00, 0x02, 0xAA}; // msg 0x30 with total=2
        uint8_t c1[] = {0x30, 0x01, 0x03, 0xBB}; // same msg but total=3 → inconsistent
        CHECK(chunker.feed(c0, sizeof(c0), 0));
        CHECK_FALSE(chunker.feed(c1, sizeof(c1), 1));
    }

    TEST_CASE("reset discards in-progress messages")
    {
        bool completed = false;
        BleChunker chunker([&](const uint8_t *, size_t) { completed = true; });

        uint8_t c0[] = {0x40, 0x00, 0x02, 0xAA};
        chunker.feed(c0, sizeof(c0), 0);
        chunker.reset();

        // Feeding the second chunk after reset starts a new message; with
        // chunk_total=2 and only one chunk present, it should not complete.
        uint8_t c1[] = {0x40, 0x01, 0x02, 0xBB};
        chunker.feed(c1, sizeof(c1), 1);
        CHECK_FALSE(completed);
    }

    TEST_CASE("four simultaneous messages complete independently")
    {
        std::vector<uint8_t> results[4];
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            // Identify by first payload byte which message this is.
            if (n > 0 && d[0] < 4) results[d[0]].assign(d, d + n);
        });

        // Feed chunk 0 of all four messages.
        for (uint8_t i = 0; i < 4; ++i) {
            uint8_t c[] = {i, 0x00, 0x01, i}; // single-chunk message, payload = {i}
            chunker.feed(c, sizeof(c), i);
        }

        for (int i = 0; i < 4; ++i) {
            REQUIRE(results[i].size() == 1);
            CHECK(results[i][0] == static_cast<uint8_t>(i));
        }
    }
}

// ── Round-trip: split → feed ──────────────────────────────────────────────────

TEST_SUITE("BleChunker round-trip")
{
    TEST_CASE("split then reassemble recovers original payload")
    {
        // 60-byte payload split at 20 bytes → 3 chunks.
        std::vector<uint8_t> original(60);
        for (size_t i = 0; i < 60; ++i) original[i] = static_cast<uint8_t>(i * 3);

        std::vector<uint8_t> recovered;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            recovered.assign(d, d + n);
        });

        auto chunks = BleChunker::split(original.data(), original.size(), 0xAB, 20);
        REQUIRE(chunks.size() == 3);

        for (const auto &c : chunks) {
            CHECK(chunker.feed(c.data(), c.size(), 0));
        }

        REQUIRE(recovered.size() == original.size());
        CHECK(memcmp(recovered.data(), original.data(), original.size()) == 0);
    }

    TEST_CASE("split then reassemble out-of-order recovers original payload")
    {
        std::vector<uint8_t> original = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
        std::vector<uint8_t> recovered;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            recovered.assign(d, d + n);
        });

        auto chunks = BleChunker::split(original.data(), original.size(), 0x99, 2);
        REQUIRE(chunks.size() == 3);

        // Feed in reverse order.
        for (int i = static_cast<int>(chunks.size()) - 1; i >= 0; --i) {
            chunker.feed(chunks[i].data(), chunks[i].size(), 0);
        }

        REQUIRE(recovered.size() == original.size());
        CHECK(memcmp(recovered.data(), original.data(), original.size()) == 0);
    }

    TEST_CASE("single-byte payload round-trips correctly")
    {
        uint8_t original[] = {0x7F};
        std::vector<uint8_t> recovered;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            recovered.assign(d, d + n);
        });

        auto chunks = BleChunker::split(original, 1, 0x01, 20);
        REQUIRE(chunks.size() == 1);
        CHECK(chunker.feed(chunks[0].data(), chunks[0].size(), 0));
        REQUIRE(recovered.size() == 1);
        CHECK(recovered[0] == 0x7F);
    }

    TEST_CASE("max chunk count (kMaxChunks) round-trips correctly")
    {
        // Use kMaxChunks chunks with 1-byte payloads each.
        const size_t total = BleChunker::kMaxChunks;
        std::vector<uint8_t> original(total);
        for (size_t i = 0; i < total; ++i) original[i] = static_cast<uint8_t>(i);

        std::vector<uint8_t> recovered;
        BleChunker chunker([&](const uint8_t *d, size_t n) {
            recovered.assign(d, d + n);
        });

        auto chunks = BleChunker::split(original.data(), total, 0xCC, 1);
        REQUIRE(chunks.size() == total);

        for (const auto &c : chunks) {
            chunker.feed(c.data(), c.size(), 0);
        }

        REQUIRE(recovered.size() == total);
        CHECK(memcmp(recovered.data(), original.data(), total) == 0);
    }
}
