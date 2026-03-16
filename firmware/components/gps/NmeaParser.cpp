// NmeaParser.cpp – NMEA-0183 GPS sentence parser (FW-005)
//
// Pure C++17 – no ESP-IDF / FreeRTOS dependencies.
// Intended for the gps_task (ESP32-S3 UART) and host unit tests.

#include "pakt/NmeaParser.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace pakt {

// ── Stream interface ──────────────────────────────────────────────────────────

bool NmeaParser::feed(uint8_t byte)
{
    const char c = static_cast<char>(byte);

    if (c == '$') {
        // Start of a new sentence – reset buffer regardless of prior state.
        buf_[0]     = '$';
        buf_pos_    = 1;
        in_sentence_ = true;
        return false;
    }

    if (!in_sentence_) return false;

    if (c == '\r' || c == '\n') {
        // End of sentence.
        buf_[buf_pos_] = '\0';
        in_sentence_   = false;
        if (buf_pos_ < 6) return false;  // Too short to be valid.
        return process(buf_);
    }

    if (buf_pos_ >= kMaxSentenceLen) {
        // Overlong sentence – discard.
        in_sentence_ = false;
        buf_pos_     = 0;
        return false;
    }

    buf_[buf_pos_++] = c;
    return false;
}

// ── Batch/test interface ──────────────────────────────────────────────────────

bool NmeaParser::process(const char *sentence)
{
    if (!sentence || sentence[0] != '$') return false;

    // Verify checksum and extract body ("GPRMC,...") without $ or *HH.
    char body[kMaxSentenceLen + 1];
    if (!verify_checksum(sentence, body, sizeof(body))) return false;

    // Split into fields.
    const char *fields[kMaxFields];
    char work[kMaxSentenceLen + 1];
    strncpy(work, body, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    size_t nf = split_fields(work, fields, kMaxFields);
    if (nf < 2) return false;

    // Dispatch by sentence type (skip leading talker prefix: GP, GN, GL, GA).
    const char *type = fields[0];
    size_t type_len  = strlen(type);

    // Normalise: compare last 3 chars so GPRMC, GNRMC etc. all match.
    if (type_len >= 3) {
        const char *suffix = type + type_len - 3;
        if (strcmp(suffix, "RMC") == 0) {
            return parse_rmc(fields + 1, nf - 1);
        }
        if (strcmp(suffix, "GGA") == 0) {
            return parse_gga(fields + 1, nf - 1);
        }
    }

    return false;  // Unrecognised sentence type.
}

// ── State helpers ─────────────────────────────────────────────────────────────

void NmeaParser::mark_stale()
{
    valid_ = false;
}

void NmeaParser::reset()
{
    memset(buf_, 0, sizeof(buf_));
    buf_pos_     = 0;
    in_sentence_ = false;
    fix_         = GpsTelem{};
    valid_       = false;
    rmc_day_     = 0;
    rmc_month_   = 0;
    rmc_year_    = 0;
}

// ── Sentence parsers ──────────────────────────────────────────────────────────

bool NmeaParser::parse_rmc(const char *fields[], size_t count)
{
    // Need at least 9 fields: time status lat N/S lon E/W speed course date
    if (count < 9) return false;

    const char *time_str   = fields[0];  // hhmmss.ss
    const char *status     = fields[1];  // A = active, V = void
    const char *lat_str    = fields[2];
    const char *ns         = fields[3];
    const char *lon_str    = fields[4];
    const char *ew         = fields[5];
    const char *speed_str  = fields[6];  // knots
    const char *course_str = fields[7];
    const char *date_str   = fields[8];  // DDMMYY

    // Status V = invalid/void; clear valid flag.
    if (!status || status[0] != 'A') {
        valid_ = false;
        return false;
    }

    // Parse date (DDMMYY).
    if (!date_str || strlen(date_str) < 6) return false;
    int dd = parse_int_n(date_str + 0, 2);
    int mm = parse_int_n(date_str + 2, 2);
    int yy = parse_int_n(date_str + 4, 2);
    if (dd < 0 || mm < 0 || yy < 0) return false;
    rmc_day_   = static_cast<uint8_t>(dd);
    rmc_month_ = static_cast<uint8_t>(mm);
    rmc_year_  = static_cast<uint16_t>(2000 + yy);

    // Parse time.
    uint8_t h = 0, m = 0, s = 0;
    if (!parse_time(time_str, h, m, s)) return false;

    fix_.lat_deg    = nmea_to_deg(lat_str, ns && ns[0] ? ns[0] : 'N');
    fix_.lon_deg    = nmea_to_deg(lon_str, ew && ew[0] ? ew[0] : 'E');
    fix_.speed_kmh  = static_cast<float>(atof(speed_str) * 1.852);   // knots → km/h
    fix_.course_deg = static_cast<float>(atof(course_str));
    fix_.timestamp_s = static_cast<uint32_t>(make_timestamp(rmc_year_, rmc_month_, rmc_day_, h, m, s));

    valid_ = true;
    return true;
}

bool NmeaParser::parse_gga(const char *fields[], size_t count)
{
    // Need at least 10 fields: time lat N/S lon E/W fix_q sats hdop alt alt_unit
    if (count < 10) return false;

    const char *time_str = fields[0];
    const char *lat_str  = fields[1];
    const char *ns       = fields[2];
    const char *lon_str  = fields[3];
    const char *ew       = fields[4];
    const char *fix_q    = fields[5];
    const char *sats_str = fields[6];
    // fields[7] = hdop (unused)
    const char *alt_str  = fields[8];

    int fq = parse_int_n(fix_q, strlen(fix_q));
    if (fq <= 0) {
        valid_ = false;
        return false;  // No fix.
    }

    // Parse time (update timestamp if we already have an RMC date).
    uint8_t h = 0, m = 0, s = 0;
    parse_time(time_str, h, m, s);

    fix_.lat_deg     = nmea_to_deg(lat_str, ns && ns[0] ? ns[0] : 'N');
    fix_.lon_deg     = nmea_to_deg(lon_str, ew && ew[0] ? ew[0] : 'E');
    fix_.fix_quality = static_cast<uint8_t>(fq > 2 ? 2 : fq);
    fix_.sats_used   = static_cast<uint8_t>(atoi(sats_str));
    fix_.alt_m       = static_cast<float>(atof(alt_str));

    if (rmc_year_ > 0) {
        fix_.timestamp_s = static_cast<uint32_t>(make_timestamp(rmc_year_, rmc_month_, rmc_day_, h, m, s));
    }

    valid_ = (fq > 0);
    return true;
}

// ── Static helpers ────────────────────────────────────────────────────────────

double NmeaParser::nmea_to_deg(const char *coord, char hemi)
{
    if (!coord || coord[0] == '\0') return 0.0;

    // Format: DDDMM.MMMM or DDMM.MMMM
    // Find the decimal point; minutes start 2 chars before it.
    const char *dot = strchr(coord, '.');
    if (!dot || dot < coord + 2) return 0.0;

    // The integer part before the last two digits before '.' is degrees.
    size_t int_len = static_cast<size_t>(dot - coord);
    if (int_len < 2) return 0.0;

    size_t deg_chars = int_len - 2;
    char deg_buf[8]{};
    if (deg_chars >= sizeof(deg_buf)) return 0.0;
    memcpy(deg_buf, coord, deg_chars);
    deg_buf[deg_chars] = '\0';

    double degrees = atof(deg_buf);
    double minutes = atof(coord + deg_chars);
    double result  = degrees + minutes / 60.0;

    if (hemi == 'S' || hemi == 's' || hemi == 'W' || hemi == 'w') {
        result = -result;
    }
    return result;
}

bool NmeaParser::parse_time(const char *s, uint8_t &h, uint8_t &m, uint8_t &s_out)
{
    if (!s || strlen(s) < 6) return false;
    int ih = parse_int_n(s + 0, 2);
    int im = parse_int_n(s + 2, 2);
    int is = parse_int_n(s + 4, 2);
    if (ih < 0 || im < 0 || is < 0) return false;
    h     = static_cast<uint8_t>(ih);
    m     = static_cast<uint8_t>(im);
    s_out = static_cast<uint8_t>(is);
    return true;
}

int64_t NmeaParser::make_timestamp(uint16_t year, uint8_t month, uint8_t day,
                                    uint8_t h, uint8_t m, uint8_t s)
{
    // Days in each month (non-leap).
    static const int days_in_month[] = {
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    auto is_leap = [](int y) {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    };

    // Days since Unix epoch (1970-01-01).
    int64_t days = 0;
    for (int y = 1970; y < year; ++y) {
        days += is_leap(y) ? 366 : 365;
    }
    for (int mo = 1; mo < month; ++mo) {
        days += days_in_month[mo];
        if (mo == 2 && is_leap(year)) days += 1;
    }
    days += day - 1;

    return days * 86400LL + h * 3600LL + m * 60LL + s;
}

bool NmeaParser::verify_checksum(const char *sentence,
                                  char *out_body, size_t out_size)
{
    if (!sentence || sentence[0] != '$') return false;

    // Find '*' that precedes the two-hex checksum digits.
    const char *star = strrchr(sentence, '*');
    if (!star || strlen(star) < 3) return false;

    // Compute XOR of all bytes between '$' and '*' (exclusive).
    uint8_t calc = 0;
    for (const char *p = sentence + 1; p < star; ++p) {
        calc ^= static_cast<uint8_t>(*p);
    }

    // Parse the hex checksum after '*'.
    char hex[3] = {star[1], star[2], '\0'};
    char *end   = nullptr;
    unsigned long given = strtoul(hex, &end, 16);
    if (end != hex + 2) return false;

    if (calc != static_cast<uint8_t>(given)) return false;

    // Copy body (between '$' and '*') into out_body.
    size_t body_len = static_cast<size_t>(star - sentence - 1);
    if (body_len >= out_size) return false;
    memcpy(out_body, sentence + 1, body_len);
    out_body[body_len] = '\0';
    return true;
}

size_t NmeaParser::split_fields(char *buf,
                                 const char *fields[], size_t max_fields)
{
    size_t count = 0;
    fields[count++] = buf;

    for (char *p = buf; *p != '\0' && count < max_fields; ++p) {
        if (*p == ',') {
            *p = '\0';
            fields[count++] = p + 1;
        }
    }
    return count;
}

int NmeaParser::parse_int_n(const char *s, size_t digits)
{
    if (!s || digits == 0) return -1;
    int result = 0;
    for (size_t i = 0; i < digits; ++i) {
        if (!isdigit(static_cast<unsigned char>(s[i]))) return -1;
        result = result * 10 + (s[i] - '0');
    }
    return result;
}

} // namespace pakt
