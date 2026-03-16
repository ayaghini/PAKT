#pragma once
// ISa818Transport – injectable UART abstraction for SA818 AT commands
//
// This interface exists solely to make Sa818Radio host-testable without
// ESP-IDF UART drivers.  The concrete implementation (Sa818UartTransport)
// lives in firmware/main/ and is NOT compiled in the host test build.

#include <cstddef>
#include <cstdint>

namespace pakt {

class ISa818Transport
{
public:
    virtual ~ISa818Transport() = default;

    // Write len bytes. Returns true on success.
    virtual bool write(const char *data, size_t len) = 0;

    // Read up to len bytes, waiting at most timeout_ms milliseconds.
    // Returns the number of bytes actually placed in buf (0 = timeout/error).
    virtual size_t read(char *buf, size_t len, uint32_t timeout_ms) = 0;
};

} // namespace pakt
