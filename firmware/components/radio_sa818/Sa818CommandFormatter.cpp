// Sa818CommandFormatter.cpp
#include "pakt/Sa818CommandFormatter.h"
#include <cstdio>
#include <cstring>

namespace pakt {

size_t Sa818CommandFormatter::connect(char *buf, size_t len)
{
    static const char kCmd[] = "AT+DMOCONNECT\r\n";
    constexpr size_t  kLen   = sizeof(kCmd) - 1;   // exclude '\0'
    if (kLen >= len) return 0;
    memcpy(buf, kCmd, kLen);
    buf[kLen] = '\0';
    return kLen;
}

size_t Sa818CommandFormatter::set_group(char *buf, size_t len,
                                         uint32_t rx_hz, uint32_t tx_hz,
                                         uint8_t squelch, bool wide_band)
{
    // Frequency format: MHz integer + '.' + 4-digit fraction
    //   e.g. 144390000 Hz → "144.3900"
    const unsigned long tx_mhz  = static_cast<unsigned long>(tx_hz / 1000000u);
    const unsigned long tx_frac = static_cast<unsigned long>((tx_hz % 1000000u) / 100u);
    const unsigned long rx_mhz  = static_cast<unsigned long>(rx_hz / 1000000u);
    const unsigned long rx_frac = static_cast<unsigned long>((rx_hz % 1000000u) / 100u);

    int n = snprintf(buf, len,
                     "AT+DMOSETGROUP=%u,%lu.%04lu,%lu.%04lu,0000,%u,0000\r\n",
                     static_cast<unsigned>(wide_band ? 1 : 0),
                     tx_mhz, tx_frac,
                     rx_mhz, rx_frac,
                     static_cast<unsigned>(squelch));
    if (n <= 0 || static_cast<size_t>(n) >= len) return 0;
    return static_cast<size_t>(n);
}

} // namespace pakt
