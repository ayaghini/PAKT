#pragma once
// NmeaParser.h – NMEA-0183 sentence parser for GPS fix data (FW-005)
//
// Parses $GPRMC / $GNRMC and $GPGGA / $GNGGA sentences to populate a GpsTelem
// record.  Designed for use on a single UART byte-stream (feed() method) or for
// direct sentence processing in unit tests (process() method).
//
// Stale-fix policy: call mark_stale() if no valid sentence is received within
// the application-defined timeout (e.g. 5 s).  The valid() flag is cleared.
//
// Thread safety: not thread-safe; intended for single-task (gps_task) use only.

#include "pakt/Telemetry.h"
#include <cstddef>
#include <cstdint>

namespace pakt {

class NmeaParser {
public:
    // NMEA spec: 82 characters max per sentence (including $ and CR LF).
    static constexpr size_t kMaxSentenceLen = 82;

    // Maximum number of comma-separated fields in one sentence.
    static constexpr size_t kMaxFields = 24;

    // ── Stream interface ──────────────────────────────────────────────────────

    // Feed a single raw byte from the GPS UART.
    // Returns true if a complete, checksum-valid sentence was processed and
    // the fix data was updated (sentence was GPRMC/GNRMC or GPGGA/GNGGA).
    bool feed(uint8_t byte);

    // ── Batch/test interface ──────────────────────────────────────────────────

    // Process a complete NMEA sentence string including the leading '$' and
    // trailing '*HH' checksum.  CR/LF are optional and are ignored.
    // Returns true if the sentence was recognised, checksum-valid, and the fix
    // data was updated.
    bool process(const char *sentence);

    // ── Fix state ─────────────────────────────────────────────────────────────

    // Current fix data.  Always safe to read; contains last-known values even
    // when valid() is false.
    const GpsTelem &fix() const { return fix_; }

    // Returns true only when a valid, non-stale GPS fix is available.
    bool valid() const { return valid_; }

    // Mark the fix as stale (call from application timer when no update arrives
    // within the expected period, e.g. 5 s for a 1 Hz GPS module).
    void mark_stale();

    // Reset all internal state and clear fix data.
    void reset();

private:
    // ── Byte-stream reassembly ────────────────────────────────────────────────

    char   buf_[kMaxSentenceLen + 1]{};
    size_t buf_pos_{0};
    bool   in_sentence_{false};

    // ── Fix state ─────────────────────────────────────────────────────────────

    GpsTelem fix_{};
    bool     valid_{false};

    // Last RMC date components retained across feed() calls to allow GGA
    // sentences to share the date when computing the Unix timestamp.
    uint8_t  rmc_day_{0};
    uint8_t  rmc_month_{0};
    uint16_t rmc_year_{0};  // 4-digit (2000+YY)

    // ── Sentence parsers (operate on field arrays) ────────────────────────────

    // Parse GPRMC / GNRMC.
    // Expected fields (index 0 = sentence type already stripped):
    //   [0]=hhmmss.ss [1]=status [2]=lat [3]=N/S [4]=lon [5]=E/W
    //   [6]=speed(kn) [7]=course [8]=DDMMYY ...
    bool parse_rmc(const char *fields[], size_t count);

    // Parse GPGGA / GNGGA.
    // Expected fields:
    //   [0]=hhmmss.ss [1]=lat [2]=N/S [3]=lon [4]=E/W [5]=fix_quality
    //   [6]=num_sats  [7]=hdop [8]=alt [9]=alt_unit ...
    bool parse_gga(const char *fields[], size_t count);

    // ── Static helpers ────────────────────────────────────────────────────────

    // Convert NMEA coordinate string "DDDMM.MMMM" to decimal degrees.
    // `hemi` should be 'N', 'S', 'E', or 'W'.
    static double nmea_to_deg(const char *coord, char hemi);

    // Parse time string "hhmmss.ss" into h/m/s components.
    static bool parse_time(const char *s, uint8_t &h, uint8_t &m, uint8_t &s_out);

    // Compute Unix timestamp (seconds since 1970-01-01 UTC).
    // Handles leap years (Gregorian).
    static int64_t make_timestamp(uint16_t year, uint8_t month, uint8_t day,
                                  uint8_t h, uint8_t m, uint8_t s);

    // Verify NMEA checksum '*HH' at end of sentence and return the body
    // (without leading '$' and without '*HH').
    // `out_body` receives a NUL-terminated copy of the inner sentence content.
    // Returns true if checksum matched, false otherwise.
    static bool verify_checksum(const char *sentence,
                                char *out_body, size_t out_size);

    // Split a mutable comma-separated string into an array of const char pointers.
    // Modifies `buf` in-place (replaces commas with NUL).
    // Returns the number of fields found (≤ max_fields).
    static size_t split_fields(char *buf,
                               const char *fields[], size_t max_fields);

    // Fast integer parse of up to `digits` characters, returns -1 on failure.
    static int parse_int_n(const char *s, size_t digits);
};

} // namespace pakt
