// test_nmea_parser.cpp – Host unit tests for NmeaParser (FW-005)
//
// No hardware or RTOS required.  Uses doctest.
// Run: ./build/test_host/pakt_tests --reporters=console --no-intro

#include "doctest/doctest.h"
#include "pakt/NmeaParser.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace pakt;

// ── Test helpers ──────────────────────────────────────────────────────────────

// Build a valid NMEA sentence from a body string (without $ and *HH).
// Computes and appends the correct XOR checksum.
static std::string make_nmea(const char *body)
{
    uint8_t ck = 0;
    for (const char *p = body; *p; ++p) ck ^= static_cast<uint8_t>(*p);
    char buf[128];
    snprintf(buf, sizeof(buf), "$%s*%02X", body, static_cast<unsigned>(ck));
    return buf;
}

static bool approx(double a, double b, double tol = 1e-3) {
    return std::fabs(a - b) < tol;
}

// ── Standard test sentences (checksums computed by make_nmea) ─────────────────

// $GPRMC – Toronto area, heading NE at ~6 knots, 1994-03-23 12:35:19 UTC
static const std::string kRmcValid =
    make_nmea("GPRMC,123519,A,4348.780,N,07923.660,W,006.0,054.7,230394,000.0,W");

// $GPRMC void status (no fix)
static const std::string kRmcVoid =
    make_nmea("GPRMC,123519,V,0000.000,N,00000.000,E,000.0,000.0,010100,000.0,W");

// $GPGGA – same position, fix quality 1, 8 sats, altitude 75 m
static const std::string kGgaValid =
    make_nmea("GPGGA,123519,4348.780,N,07923.660,W,1,08,0.9,75.0,M,46.9,M,,");

// $GPGGA – fix quality 0 (no fix)
static const std::string kGgaNoFix =
    make_nmea("GPGGA,123519,0000.000,N,00000.000,E,0,00,0.0,0.0,M,0.0,M,,");

// $GNRMC – same data with different talker prefix
static const std::string kGnRmcValid =
    make_nmea("GNRMC,123519,A,4348.780,N,07923.660,W,006.0,054.7,230394,000.0,W");

// ── process(): checksum validation ───────────────────────────────────────────

TEST_CASE("process: valid GPRMC accepted") {
    NmeaParser p;
    CHECK(p.process(kRmcValid.c_str()));
    CHECK(p.valid());
}

TEST_CASE("process: bad checksum rejected") {
    NmeaParser p;
    // Corrupt last checksum digit.
    std::string bad = kRmcValid;
    bad.back() = (bad.back() == '0') ? '1' : '0';
    CHECK(!p.process(bad.c_str()));
    CHECK(!p.valid());
}

TEST_CASE("process: GPRMC void status clears valid flag") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    CHECK(p.valid());
    p.process(kRmcVoid.c_str());
    CHECK(!p.valid());
}

TEST_CASE("process: null sentence returns false") {
    NmeaParser p;
    CHECK(!p.process(nullptr));
}

TEST_CASE("process: missing leading dollar returns false") {
    NmeaParser p;
    // Strip the leading '$'.
    CHECK(!p.process(kRmcValid.c_str() + 1));
}

TEST_CASE("process: unrecognised sentence type returns false (no crash)") {
    NmeaParser p;
    std::string gsv = make_nmea("GPGSV,3,1,11,03,03,111,00,04,15,270,00*");
    // Even if checksum is off, should not crash.
    p.process(gsv.c_str());
    CHECK(!p.valid());
}

// ── GPRMC coordinate decoding ─────────────────────────────────────────────────

TEST_CASE("GPRMC: latitude decoded correctly") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    // 4348.780 N → 43 + 48.780/60 = 43.8130°
    CHECK(approx(p.fix().lat, 43.8130));
    CHECK(p.fix().lat > 0.0);  // Northern hemisphere.
}

TEST_CASE("GPRMC: longitude decoded correctly (West = negative)") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    // 07923.660 W → -(79 + 23.660/60) = -79.3943°
    CHECK(approx(p.fix().lon, -79.3944));
    CHECK(p.fix().lon < 0.0);
}

TEST_CASE("GPRMC: South latitude is negative") {
    NmeaParser p;
    std::string s = make_nmea("GPRMC,000000,A,3300.000,S,07000.000,W,000.0,000.0,010100,000.0,W");
    p.process(s.c_str());
    CHECK(p.fix().lat < 0.0);
    CHECK(approx(p.fix().lat, -33.0));
}

TEST_CASE("GPRMC: speed converted from knots to m/s") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    // 6.0 knots × 0.5144 = 3.0864 m/s
    CHECK(approx(static_cast<double>(p.fix().speed_mps), 6.0 * 0.5144, 0.01));
}

TEST_CASE("GPRMC: course decoded") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    CHECK(approx(p.fix().course_deg, 54.7f, 0.1));
}

// ── GPRMC timestamp ───────────────────────────────────────────────────────────

TEST_CASE("GPRMC: unix timestamp for 1994-03-23 12:35:19 UTC") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    // Verified: 8847 days * 86400 + 12*3600 + 35*60 + 19 = 764426119
    CHECK(p.fix().ts == 764426119LL);
}

TEST_CASE("GPRMC: timestamp is 0 before any valid fix") {
    NmeaParser p;
    CHECK(p.fix().ts == 0);
}

TEST_CASE("GPRMC: unix timestamp for 2000-01-01 00:00:00 UTC") {
    NmeaParser p;
    std::string s = make_nmea("GPRMC,000000,A,0000.000,N,00000.000,E,000.0,000.0,010100,000.0,E");
    p.process(s.c_str());
    CHECK(p.fix().ts == 946684800LL);
}

// ── GPGGA ─────────────────────────────────────────────────────────────────────

TEST_CASE("GPGGA: valid fix accepted") {
    NmeaParser p;
    CHECK(p.process(kGgaValid.c_str()));
    CHECK(p.valid());
}

TEST_CASE("GPGGA: fix quality 0 clears valid") {
    NmeaParser p;
    p.process(kGgaValid.c_str());
    CHECK(p.valid());
    p.process(kGgaNoFix.c_str());
    CHECK(!p.valid());
}

TEST_CASE("GPGGA: altitude decoded") {
    NmeaParser p;
    p.process(kGgaValid.c_str());
    CHECK(approx(p.fix().alt_m, 75.0f, 0.5f));
}

TEST_CASE("GPGGA: satellite count decoded") {
    NmeaParser p;
    p.process(kGgaValid.c_str());
    CHECK(p.fix().sats == 8);
}

TEST_CASE("GPGGA: fix quality stored (clamped to 2)") {
    NmeaParser p;
    p.process(kGgaValid.c_str());
    CHECK(p.fix().fix == 1);
}

TEST_CASE("GPGGA: DGPS fix quality 2 stored as 2") {
    NmeaParser p;
    std::string s = make_nmea("GPGGA,123519,4348.780,N,07923.660,W,2,08,0.9,75.0,M,46.9,M,,");
    p.process(s.c_str());
    CHECK(p.fix().fix == 2);
}

TEST_CASE("GPGGA: uses RMC date for timestamp when available") {
    NmeaParser p;
    p.process(kRmcValid.c_str());    // Sets RMC date.
    p.process(kGgaValid.c_str());    // Should share that date.
    CHECK(p.fix().ts == 764426119LL);
}

// ── GNRMC talker prefix ───────────────────────────────────────────────────────

TEST_CASE("GNRMC: accepted same as GPRMC") {
    NmeaParser p;
    CHECK(p.process(kGnRmcValid.c_str()));
    CHECK(p.valid());
    CHECK(approx(p.fix().lat, 43.8130));
}

// ── mark_stale / reset ────────────────────────────────────────────────────────

TEST_CASE("mark_stale: clears valid flag") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    CHECK(p.valid());
    p.mark_stale();
    CHECK(!p.valid());
}

TEST_CASE("mark_stale: fix data readable after stale") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    p.mark_stale();
    CHECK(approx(p.fix().lat, 43.8130));  // Coordinates still available.
}

TEST_CASE("reset: clears valid flag and fix data") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    p.reset();
    CHECK(!p.valid());
    CHECK(p.fix().lat  == 0.0);
    CHECK(p.fix().lon  == 0.0);
    CHECK(p.fix().ts   == 0);
    CHECK(p.fix().sats == 0);
}

TEST_CASE("reset: parser accepts valid sentence after reset") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    p.reset();
    CHECK(!p.valid());
    p.process(kRmcValid.c_str());
    CHECK(p.valid());
}

// ── feed(): byte-stream interface ────────────────────────────────────────────

TEST_CASE("feed: complete sentence triggers parse") {
    NmeaParser p;
    for (char c : kRmcValid) p.feed(static_cast<uint8_t>(c));
    p.feed('\r');
    p.feed('\n');
    CHECK(p.valid());
}

TEST_CASE("feed: partial sentence does not trigger parse") {
    NmeaParser p;
    const char *partial = "$GPRMC,123";
    while (*partial) p.feed(static_cast<uint8_t>(*partial++));
    CHECK(!p.valid());
}

TEST_CASE("feed: sentence without leading dollar is ignored") {
    NmeaParser p;
    // Feed the sentence without the '$'.
    const char *no_dollar = kRmcValid.c_str() + 1;
    while (*no_dollar) p.feed(static_cast<uint8_t>(*no_dollar++));
    p.feed('\n');
    CHECK(!p.valid());
}

TEST_CASE("feed: overlong sentence discarded without crash") {
    NmeaParser p;
    p.feed('$');
    for (size_t i = 0; i < 200; ++i) p.feed('X');
    p.feed('\n');
    CHECK(!p.valid());
}

TEST_CASE("feed: second valid sentence updates fix") {
    NmeaParser p;

    for (char c : kRmcValid) p.feed(static_cast<uint8_t>(c));
    p.feed('\r'); p.feed('\n');
    CHECK(p.valid());
    double lat1 = p.fix().lat;

    // Feed the GGA sentence (also valid).
    for (char c : kGgaValid) p.feed(static_cast<uint8_t>(c));
    p.feed('\r'); p.feed('\n');
    CHECK(p.valid());
    CHECK(approx(p.fix().lat, lat1));  // Same position in both sentences.
}

TEST_CASE("feed: new-sentence dollar resets buffer mid-stream") {
    NmeaParser p;
    // Start a sentence, then interrupt with a new one.
    const char *interrupted = "$GARBAGE_STUFF";
    while (*interrupted) p.feed(static_cast<uint8_t>(*interrupted++));
    // Now feed the real sentence (starting with $).
    for (char c : kRmcValid) p.feed(static_cast<uint8_t>(c));
    p.feed('\r'); p.feed('\n');
    CHECK(p.valid());
}

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST_CASE("process: empty sentence returns false") {
    NmeaParser p;
    CHECK(!p.process(""));
    CHECK(!p.process("$"));
    CHECK(!p.process("$*00"));
}

TEST_CASE("process: GPRMC missing fields returns false") {
    NmeaParser p;
    // Only 4 fields – not enough.
    std::string s = make_nmea("GPRMC,123519,A,4348.780");
    CHECK(!p.process(s.c_str()));
}

TEST_CASE("process: subsequent void after valid leaves fix data intact") {
    NmeaParser p;
    p.process(kRmcValid.c_str());
    double lat = p.fix().lat;
    p.process(kRmcVoid.c_str());
    CHECK(!p.valid());
    // Coordinates from last valid fix still accessible.
    CHECK(p.fix().lat == lat);
}
