// APRS packet helper implementation

#include "pakt/Aprs.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace pakt::aprs {

// ── Frame builder ─────────────────────────────────────────────────────────────

ax25::Frame make_ui_frame(const char *callsign, uint8_t ssid)
{
    ax25::Frame f{};
    f.control = ax25::kControlUI;
    f.pid     = ax25::kPidNoLayer3;

    // Destination: APRS tocall
    strncpy(f.addr[0].callsign, ax25::kTocall, ax25::kAddrCallsignMaxLen);
    f.addr[0].callsign[ax25::kAddrCallsignMaxLen] = '\0';
    f.addr[0].ssid              = 0;
    f.addr[0].has_been_repeated = false;

    // Source
    strncpy(f.addr[1].callsign, callsign, ax25::kAddrCallsignMaxLen);
    f.addr[1].callsign[ax25::kAddrCallsignMaxLen] = '\0';
    f.addr[1].ssid              = ssid;
    f.addr[1].has_been_repeated = false;

    f.addr_count = 2;
    f.info_len   = 0;
    return f;
}

// ── Position encoder ──────────────────────────────────────────────────────────

size_t encode_position(float lat_deg, float lon_deg,
                       char symbol_table, char symbol_code,
                       const char *comment,
                       uint8_t *info_out, size_t info_max)
{
    if (!info_out || info_max < 20) return 0;

    // Convert decimal degrees → DDMM.mm format
    float abs_lat = fabsf(lat_deg);
    int   lat_d   = (int)abs_lat;
    float lat_m   = (abs_lat - (float)lat_d) * 60.0f;
    char  lat_hem = (lat_deg >= 0.0f) ? 'N' : 'S';

    float abs_lon = fabsf(lon_deg);
    int   lon_d   = (int)abs_lon;
    float lon_m   = (abs_lon - (float)lon_d) * 60.0f;
    char  lon_hem = (lon_deg >= 0.0f) ? 'E' : 'W';

    int n = snprintf((char *)info_out, info_max,
                     "!%02d%05.2f%c%c%03d%05.2f%c%c%s",
                     lat_d, (double)lat_m, lat_hem, symbol_table,
                     lon_d, (double)lon_m, lon_hem, symbol_code,
                     comment ? comment : "");

    return (n > 0 && (size_t)n < info_max) ? (size_t)n : 0;
}

// ── Message encoder ───────────────────────────────────────────────────────────

size_t encode_message(const char *to_callsign, uint8_t to_ssid,
                      const char *text, const char *msg_id,
                      uint8_t *info_out, size_t info_max)
{
    if (!info_out || !to_callsign || !text || !msg_id) return 0;
    if (info_max < 15) return 0;

    // Build "CALLSSID" padded to exactly 9 chars
    char dest[10];
    if (to_ssid == 0) {
        snprintf(dest, sizeof(dest), "%-9s", to_callsign);
    } else {
        char with_ssid[10];
        snprintf(with_ssid, sizeof(with_ssid), "%s-%u", to_callsign, to_ssid);
        snprintf(dest, sizeof(dest), "%-9s", with_ssid);
    }

    // Format: ":DEST     :text{msgid}"
    int n = snprintf((char *)info_out, info_max, ":%9s:%s{%s", dest, text, msg_id);

    return (n > 0 && (size_t)n < info_max) ? (size_t)n : 0;
}

// ── Packet type ───────────────────────────────────────────────────────────────

char packet_type(const uint8_t *info, size_t info_len)
{
    if (!info || info_len == 0) return '\0';
    char c = (char)info[0];
    // Known APRS data type identifiers (APRS 1.01 table)
    switch (c) {
        case '!': case '=':           // position without/with messaging
        case '/': case '@':           // position with timestamp
        case '>':                     // status
        case ':':                     // message
        case ';':                     // object
        case ')':                     // item
        case 'T':                     // telemetry
        case '<': case '?': case '_': // capabilities, query, weather
            return c;
        default:
            return '\0';
    }
}

} // namespace pakt::aprs
