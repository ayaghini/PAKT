// BleChunker implementation (INT-002)
// Pure C++ – no ESP-IDF or NimBLE dependencies.

#include "pakt/BleChunker.h"

#include <algorithm>
#include <cstring>

namespace pakt {

// ── Splitter ──────────────────────────────────────────────────────────────────

std::vector<std::vector<uint8_t>> BleChunker::split(
    const uint8_t *payload, size_t len,
    uint8_t msg_id, size_t chunk_payload_max)
{
    if (!payload || len == 0 || chunk_payload_max == 0) return {};

    size_t num_chunks = (len + chunk_payload_max - 1) / chunk_payload_max;
    if (num_chunks > kMaxChunks) return {};

    auto chunk_total = static_cast<uint8_t>(num_chunks);
    std::vector<std::vector<uint8_t>> result;
    result.reserve(num_chunks);

    for (size_t i = 0; i < num_chunks; ++i) {
        size_t offset      = i * chunk_payload_max;
        size_t chunk_len   = std::min(chunk_payload_max, len - offset);
        std::vector<uint8_t> chunk(kHeaderSize + chunk_len);
        chunk[0] = msg_id;
        chunk[1] = static_cast<uint8_t>(i);
        chunk[2] = chunk_total;
        memcpy(chunk.data() + kHeaderSize, payload + offset, chunk_len);
        result.push_back(std::move(chunk));
    }

    return result;
}

// ── Reassembler ───────────────────────────────────────────────────────────────

BleChunker::BleChunker(CompleteCallback cb, uint32_t timeout_ms)
    : cb_(std::move(cb)), timeout_ms_(timeout_ms)
{
}

bool BleChunker::feed(const uint8_t *chunk, size_t len, uint32_t now_ms)
{
    if (!chunk || len < kHeaderSize) return false;

    const uint8_t msg_id     = chunk[0];
    const uint8_t chunk_idx  = chunk[1];
    const uint8_t chunk_total = chunk[2];
    const uint8_t *payload   = chunk + kHeaderSize;
    const size_t   payload_len = len - kHeaderSize;

    if (chunk_total == 0)              return false;
    if (chunk_idx >= chunk_total)      return false;
    if (chunk_total > kMaxChunks)      return false;
    if (payload_len > kMaxChunkPayload) return false;

    // Expire stale slots before allocating a new one.
    tick(now_ms);

    Slot *slot = find_slot(msg_id);
    if (!slot) {
        slot = alloc_slot(msg_id, chunk_total, now_ms);
        if (!slot) return false; // all slots busy (shouldn't happen after tick)
    } else if (slot->chunk_total != chunk_total) {
        // Inconsistent chunk_total for this msg_id – discard.
        return false;
    }

    // Duplicate detection: ignore if already received.
    const uint64_t bit = uint64_t(1) << chunk_idx;
    if (slot->received_mask & bit) return true;

    memcpy(slot->payloads[chunk_idx], payload, payload_len);
    slot->payload_lens[chunk_idx] = static_cast<uint8_t>(payload_len);
    slot->received_mask |= bit;

    try_complete(*slot);
    return true;
}

void BleChunker::tick(uint32_t now_ms)
{
    for (auto &s : slots_) {
        if (s.active && (now_ms - s.start_ms) >= timeout_ms_) {
            s.active        = false;
            s.received_mask = 0;
        }
    }
}

void BleChunker::reset()
{
    for (auto &s : slots_) {
        s.active        = false;
        s.received_mask = 0;
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

BleChunker::Slot *BleChunker::find_slot(uint8_t msg_id)
{
    for (auto &s : slots_) {
        if (s.active && s.msg_id == msg_id) return &s;
    }
    return nullptr;
}

BleChunker::Slot *BleChunker::alloc_slot(
    uint8_t msg_id, uint8_t chunk_total, uint32_t now_ms)
{
    // Prefer an empty slot.
    for (auto &s : slots_) {
        if (!s.active) {
            s.msg_id        = msg_id;
            s.chunk_total   = chunk_total;
            s.received_mask = 0;
            s.start_ms      = now_ms;
            s.active        = true;
            memset(s.payload_lens, 0, sizeof(s.payload_lens));
            return &s;
        }
    }

    // All slots occupied (tick() did not free any); evict the oldest.
    // Compare monotonic ages: (now_ms - s.start_ms) > (now_ms - oldest->start_ms).
    // uint32_t subtraction handles wrap correctly as long as ages stay < 2^31.
    Slot *oldest = &slots_[0];
    for (auto &s : slots_) {
        if ((now_ms - s.start_ms) > (now_ms - oldest->start_ms)) oldest = &s;
    }
    oldest->msg_id        = msg_id;
    oldest->chunk_total   = chunk_total;
    oldest->received_mask = 0;
    oldest->start_ms      = now_ms;
    oldest->active        = true;
    memset(oldest->payload_lens, 0, sizeof(oldest->payload_lens));
    return oldest;
}

void BleChunker::try_complete(Slot &slot)
{
    // Build the expected mask for all chunks received.
    const uint64_t expected =
        (slot.chunk_total == 64) ? UINT64_MAX
                                 : (uint64_t(1) << slot.chunk_total) - 1;

    if ((slot.received_mask & expected) != expected) return;

    // All chunks arrived – assemble in order.
    std::vector<uint8_t> assembled;
    for (uint8_t i = 0; i < slot.chunk_total; ++i) {
        const uint8_t *p = slot.payloads[i];
        assembled.insert(assembled.end(), p, p + slot.payload_lens[i]);
    }

    // Free the slot before calling back (callback may feed new chunks).
    slot.active        = false;
    slot.received_mask = 0;

    if (cb_) cb_(assembled.data(), assembled.size());
}

} // namespace pakt
