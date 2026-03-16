#pragma once
// Sa818CommandFormatter – static AT command builder for the SA818-V module
//
// Pure C++, no ESP-IDF dependencies, host-testable.
//
// All methods write into a caller-supplied buffer and return the number of
// bytes written (excluding the null terminator), or 0 on buffer overflow.
//
// SA818 AT command reference:
//   Handshake : AT+DMOCONNECT\r\n
//   Set group : AT+DMOSETGROUP=BW,TXF,RXF,0000,SQ,0000\r\n
//     BW : 0 = 12.5 kHz narrow, 1 = 25 kHz wide
//     TXF/RXF : frequency in MHz with 4 decimal places, e.g. "144.3900"
//     SQ  : squelch 0–8

#include <cstddef>
#include <cstdint>

namespace pakt {

class Sa818CommandFormatter
{
public:
    // AT+DMOCONNECT\r\n  (18 bytes including CRLF)
    static size_t connect(char *buf, size_t len);

    // AT+DMOSETGROUP=BW,TXF,RXF,0000,SQ,0000\r\n
    //   rx_hz / tx_hz : frequency in Hz; valid VHF range 134 000 000–174 000 000
    //   squelch       : 0–8
    //   wide_band     : true → BW=1 (25 kHz), false → BW=0 (12.5 kHz)
    static size_t set_group(char *buf, size_t len,
                             uint32_t rx_hz, uint32_t tx_hz,
                             uint8_t squelch, bool wide_band);
};

} // namespace pakt
