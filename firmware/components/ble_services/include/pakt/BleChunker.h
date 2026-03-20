// BleChunker – BLE chunked payload splitter and reassembler (INT-002)
//
// Wire format per chunk: [msg_id:1][chunk_idx:1][chunk_total:1][payload_bytes...]
//   msg_id      : identifies a logical message (wraps at 255)
//   chunk_idx   : 0-based index of this chunk (0 = first)
//   chunk_total : total number of chunks for this msg_id
//
// A single-chunk message uses chunk_idx=0, chunk_total=1.
//
// This class is pure C++ (no ESP-IDF or NimBLE dependencies) and runs on the
// host for unit testing as well as on the device.

#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace pakt {

class BleChunker {
public:
    static constexpr size_t   kHeaderSize       = 3;   // msg_id + chunk_idx + chunk_total
    static constexpr size_t   kMaxChunks        = 64;  // max chunks per logical message
    static constexpr size_t   kMaxChunkPayload  = 252; // max bytes of payload per chunk
    static constexpr uint32_t kDefaultTimeoutMs = 5000;

    // Called when a complete message has been reassembled.
    using CompleteCallback = std::function<void(const uint8_t *data, size_t len)>;

    // ── Splitter (static, stateless) ─────────────────────────────────────────

    // Split `payload` (len bytes) into wire chunks, each with at most
    // `chunk_payload_max` bytes of payload (not counting the 3-byte header).
    // Returns a vector of complete wire frames (header + payload slice).
    // Returns an empty vector if any input is invalid or chunk count > kMaxChunks.
    static std::vector<std::vector<uint8_t>> split(
        const uint8_t *payload, size_t len,
        uint8_t msg_id, size_t chunk_payload_max);

    // ── Reassembler ──────────────────────────────────────────────────────────

    explicit BleChunker(CompleteCallback cb,
                        uint32_t timeout_ms = kDefaultTimeoutMs);

    // Feed one raw wire chunk (header + payload).
    // `now_ms` is a monotonic timestamp in milliseconds (caller-supplied).
    // Returns false if the chunk is malformed (too short, out-of-range index,
    // chunk_total=0, or chunk_total inconsistency with earlier chunks).
    bool feed(const uint8_t *chunk, size_t len, uint32_t now_ms);

    // Expire in-progress messages that started more than timeout_ms ago.
    // Call this periodically from a timer or task loop.
    void tick(uint32_t now_ms);

    // Discard all in-progress messages.
    void reset();

private:
    struct Slot {
        uint8_t  msg_id;
        uint8_t  chunk_total;
        uint64_t received_mask; // bit i set when chunk i has been received
        uint32_t start_ms;
        bool     active;
        uint8_t  payloads[kMaxChunks][kMaxChunkPayload];
        uint8_t  payload_lens[kMaxChunks];
    };

    // Bench firmware only needs one in-flight message per characteristic.
    // Keeping this at 1 avoids burning large amounts of internal DRAM.
    static constexpr size_t kSlots = 1;

    Slot     slots_[kSlots]{};
    CompleteCallback cb_;
    uint32_t timeout_ms_;

    Slot *find_slot(uint8_t msg_id);
    Slot *alloc_slot(uint8_t msg_id, uint8_t chunk_total, uint32_t now_ms);
    void  try_complete(Slot &slot);
};

} // namespace pakt
