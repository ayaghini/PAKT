// AX.25 UI-frame codec implementation
//
// FCS: CRC-16/CCITT with reflected polynomial 0x8408 (equivalent to 0x1021
// processed LSB-first, matching AX.25 bit transmission order).
// Transmitted FCS is inverted (~crc), appended LSB-first.

#include "pakt/Ax25.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace pakt::ax25 {

// ── FCS ───────────────────────────────────────────────────────────────────────

uint16_t fcs(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0x8408u) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFu;
}

// ── Address helpers ───────────────────────────────────────────────────────────

// Write a 7-byte AX.25 address field for one station.
// is_destination: sets C bit (bit 7) to 1 (command frame convention).
// is_last:        sets the HDLC extension bit (bit 0) to 1.
static void encode_address(uint8_t *out, const Address &addr,
                            bool is_destination, bool is_last)
{
    // Bytes 0–5: callsign padded to 6 chars with spaces, each byte shifted left by 1.
    for (int i = 0; i < 6; ++i) {
        char c = (addr.callsign[i] != '\0') ? (char)toupper((unsigned char)addr.callsign[i]) : ' ';
        out[i] = (uint8_t)((unsigned char)c << 1);
    }

    // Byte 6: [C/H][R][R][SSID3..0][ext]
    //   bit 7 (C/H): 1 for destination (command frame), or H (has-been-repeated) for digipeaters
    //   bits 6-5 (R): reserved, always 1
    //   bits 4-1: SSID value (0–15)
    //   bit 0 (ext): 1 if this is the last address field entry
    uint8_t ch_bit = (is_destination || addr.has_been_repeated) ? 0x80u : 0x00u;
    out[6] = ch_bit | 0x60u | ((addr.ssid & 0x0Fu) << 1) | (is_last ? 0x01u : 0x00u);
}

// Parse a 7-byte AX.25 address field.
// Returns true if this is the last address (extension bit set).
static bool decode_address(const uint8_t *in, Address &addr)
{
    // Bytes 0–5: shift right by 1 to recover ASCII; trim trailing spaces.
    int last_non_space = -1;
    for (int i = 0; i < 6; ++i) {
        char c = (char)(in[i] >> 1);
        addr.callsign[i] = c;
        if (c != ' ') last_non_space = i;
    }
    addr.callsign[last_non_space + 1] = '\0'; // null-terminate after last non-space

    uint8_t ssid_byte         = in[6];
    addr.ssid                 = (ssid_byte >> 1) & 0x0Fu;
    addr.has_been_repeated    = (ssid_byte & 0x80u) != 0;
    return (ssid_byte & 0x01u) != 0; // extension bit = last address
}

// ── encode ────────────────────────────────────────────────────────────────────

size_t encode(const Frame &frame, uint8_t *out, size_t out_max)
{
    if (frame.addr_count < 2 || frame.addr_count > kMaxAddresses) return 0;
    if (frame.info_len > kMaxInfoLen) return 0;

    // Space check: addr fields + ctrl + pid + info + fcs
    size_t needed = (size_t)frame.addr_count * 7u + 2u + frame.info_len + 2u;
    if (needed > out_max) return 0;

    size_t pos = 0;

    // Address field
    for (uint8_t i = 0; i < frame.addr_count; ++i) {
        bool is_dest = (i == 0);
        bool is_last = (i == frame.addr_count - 1u);
        encode_address(out + pos, frame.addr[i], is_dest, is_last);
        pos += 7;
    }

    // Control + PID
    out[pos++] = frame.control;
    out[pos++] = frame.pid;

    // Information field
    if (frame.info_len > 0) {
        memcpy(out + pos, frame.info, frame.info_len);
        pos += frame.info_len;
    }

    // FCS over everything written so far (addr + ctrl + pid + info)
    uint16_t checksum = fcs(out, pos);
    out[pos++] = (uint8_t)(checksum & 0xFFu);         // LSB first
    out[pos++] = (uint8_t)((checksum >> 8) & 0xFFu);  // MSB second

    return pos;
}

// ── decode ────────────────────────────────────────────────────────────────────

bool decode(const uint8_t *data, size_t len, Frame &out_frame)
{
    // Minimum: 2 addresses (14 bytes) + ctrl + pid + fcs = 18 bytes
    if (len < 18) return false;

    // Verify FCS: compare stored FCS against computed FCS over the payload.
    size_t payload_len = len - 2;
    uint16_t stored = (uint16_t)data[payload_len]
                    | ((uint16_t)data[payload_len + 1] << 8);
    if (fcs(data, payload_len) != stored) return false;

    // Parse address field
    size_t pos = 0;
    out_frame.addr_count = 0;
    bool last_addr = false;

    while (!last_addr && pos + 7 <= payload_len
           && out_frame.addr_count < kMaxAddresses) {
        last_addr = decode_address(data + pos,
                                   out_frame.addr[out_frame.addr_count++]);
        pos += 7;
    }

    if (out_frame.addr_count < 2 || !last_addr) return false;

    // Control + PID
    if (pos + 2 > payload_len) return false;
    out_frame.control = data[pos++];
    out_frame.pid     = data[pos++];

    // Information field (remaining bytes up to but not including FCS)
    out_frame.info_len = payload_len - pos;
    if (out_frame.info_len > kMaxInfoLen) return false;
    memcpy(out_frame.info, data + pos, out_frame.info_len);

    return true;
}

// ── to_tnc2 ───────────────────────────────────────────────────────────────────

size_t to_tnc2(const Frame &frame, char *buf, size_t buf_max)
{
    if (frame.addr_count < 2 || buf_max < 2) return 0;

    int n = 0;

    // Source (addr[1]) > Destination (addr[0])
    n += snprintf(buf + n, buf_max - (size_t)n, "%s", frame.addr[1].callsign);
    if (frame.addr[1].ssid > 0)
        n += snprintf(buf + n, buf_max - (size_t)n, "-%u", frame.addr[1].ssid);

    n += snprintf(buf + n, buf_max - (size_t)n, ">%s", frame.addr[0].callsign);
    if (frame.addr[0].ssid > 0)
        n += snprintf(buf + n, buf_max - (size_t)n, "-%u", frame.addr[0].ssid);

    // Digipeaters (addr[2..])
    for (uint8_t i = 2; i < frame.addr_count; ++i) {
        n += snprintf(buf + n, buf_max - (size_t)n, ",%s", frame.addr[i].callsign);
        if (frame.addr[i].ssid > 0)
            n += snprintf(buf + n, buf_max - (size_t)n, "-%u", frame.addr[i].ssid);
        if (frame.addr[i].has_been_repeated)
            n += snprintf(buf + n, buf_max - (size_t)n, "*");
    }

    // Info field
    n += snprintf(buf + n, buf_max - (size_t)n, ":%.*s",
                  (int)frame.info_len, (const char *)frame.info);

    return (n > 0) ? (size_t)n : 0;
}

} // namespace pakt::ax25
