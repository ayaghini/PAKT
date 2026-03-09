#pragma once

// IPacketLink – bidirectional AX.25 packet link (modem-side abstraction)
//
// Contract (from architecture_contracts.md §A):
//   - AX.25 frames in/out only; no BLE, framing, or chunking concerns here.
//   - Caller owns all frame memory; no heap allocation inside the interface.
//   - send() and recv() are non-blocking.

#include <cstddef>
#include <cstdint>

namespace pakt {

// A view into a caller-owned byte buffer holding one raw AX.25 frame.
// Flags (0x7E) are excluded; this is the payload between flags.
struct Ax25Frame {
    uint8_t *data;    // caller-owned buffer
    size_t   length;  // populated frame length in bytes
};

class IPacketLink
{
public:
    virtual ~IPacketLink() = default;

    // Queue a frame for transmission.
    // frame.data must remain valid until the frame has been sent.
    // Returns false if the TX queue is full.
    virtual bool send(const Ax25Frame &frame) = 0;

    // Poll for a received frame.
    // frame.data must point to a caller-supplied buffer of at least max_len bytes.
    // On success, frame.length is set to the decoded frame size.
    // Returns false if the RX queue is empty.
    virtual bool recv(Ax25Frame &frame, size_t max_len) = 0;

    // Number of frames waiting in the RX queue.
    virtual size_t rx_available() const = 0;

    // Number of free TX queue slots remaining.
    virtual size_t tx_free() const = 0;
};

} // namespace pakt
