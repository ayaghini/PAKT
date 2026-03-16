// PayloadValidator.cpp – BLE write payload validation (P0: pre-hardware sprint)

#include "pakt/PayloadValidator.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace pakt {

// ── Internal JSON helpers ─────────────────────────────────────────────────────
//
// Minimal flat-object JSON scanner; no heap allocation.
// Assumes the input is a well-formed flat JSON object (no nested objects/arrays).
// Values inside strings that resemble keys are correctly skipped because the
// scanner tracks whether it is inside a string value.

// Skip over a JSON string starting at the opening '"'.
// Returns a pointer to the character after the closing '"', or nullptr on error.
static const char *skip_string(const char *p)
{
    if (!p || *p != '"') return nullptr;
    ++p;  // step past opening '"'
    while (*p) {
        if (*p == '\\') {
            ++p;  // skip the backslash
            if (!*p) return nullptr;  // truncated escape
            ++p;  // skip the escaped character
        } else if (*p == '"') {
            return p + 1;  // step past closing '"'
        } else {
            ++p;
        }
    }
    return nullptr;  // unterminated string
}

// Extract the content of a JSON string starting at the opening '"' into out[0..max_out-1].
// Returns true on success; out is always NUL-terminated on success.
// The string is truncated silently if longer than max_out-1.
static bool extract_string(const char *p, char *out, size_t max_out)
{
    if (!p || *p != '"' || !out || max_out == 0) return false;
    ++p;
    size_t n = 0;
    while (*p) {
        if (*p == '\\') {
            ++p;
            if (!*p) return false;
            if (n < max_out - 1) out[n++] = *p;
            ++p;
        } else if (*p == '"') {
            out[n] = '\0';
            return true;
        } else {
            if (n < max_out - 1) out[n++] = *p;
            ++p;
        }
    }
    return false;  // unterminated string
}

// Extract an integer value (no leading whitespace) into *out.
// Returns true on success.
static bool extract_int(const char *p, int *out)
{
    if (!p || !out) return false;
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }
    if (!isdigit(static_cast<unsigned char>(*p))) return false;
    int val = 0;
    while (isdigit(static_cast<unsigned char>(*p))) {
        val = val * 10 + (*p - '0');
        ++p;
    }
    *out = neg ? -val : val;
    return true;
}

// Find the JSON value for the given key in a flat JSON object string.
// Returns a pointer to the first non-whitespace character of the value,
// or nullptr if the key is not present or the JSON is malformed.
//
// This correctly skips over string values that contain the key name.
static const char *find_value(const char *json, const char *key)
{
    if (!json || !key) return nullptr;
    const size_t key_len = strlen(key);
    const char *p = json;

    while (*p) {
        // Skip whitespace and structural characters between values.
        while (*p && *p != '"') ++p;
        if (!*p) break;

        // p now points at '"' — this could be a key string.
        const char *key_start = p + 1;

        // Check if this quoted string matches our key exactly.
        if (strncmp(key_start, key, key_len) == 0 && key_start[key_len] == '"') {
            // Advance past the closing '"' of the key.
            p = key_start + key_len + 1;
            // Skip whitespace to ':'.
            while (*p == ' ' || *p == '\t') ++p;
            if (*p != ':') {
                // Not a key:value pair — might be a value string; skip and continue.
                continue;
            }
            ++p;  // skip ':'
            while (*p == ' ' || *p == '\t') ++p;
            return p;  // caller reads value from here
        }

        // Not our key: skip this string (could be a key or a string value).
        p = skip_string(p);
        if (!p) return nullptr;
    }
    return nullptr;
}

// ── Callsign validation ───────────────────────────────────────────────────────

static bool is_callsign_char(char c)
{
    return isalnum(static_cast<unsigned char>(c)) || c == '-';
}

static bool is_valid_callsign(const char *s)
{
    if (!s) return false;
    size_t len = strlen(s);
    if (len < 1 || len > 6) return false;
    for (size_t i = 0; i < len; ++i) {
        if (!is_callsign_char(s[i])) return false;
    }
    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool PayloadValidator::validate_config_payload(const uint8_t *data, size_t len,
                                               ConfigFields *out)
{
    if (!data || len == 0 || len >= kMaxJsonLen) return false;

    char json[kMaxJsonLen];
    memcpy(json, data, len);
    json[len] = '\0';

    // Required: "callsign" — 1-6 alphanumeric/dash chars.
    const char *v = find_value(json, "callsign");
    if (!v || *v != '"') return false;
    char callsign[8]{};
    if (!extract_string(v, callsign, sizeof(callsign))) return false;
    if (!is_valid_callsign(callsign)) return false;

    // Optional: "ssid" — integer 0-15.
    uint8_t ssid = 0;
    const char *sv = find_value(json, "ssid");
    if (sv) {
        int ssid_int = 0;
        if (!extract_int(sv, &ssid_int)) return false;
        if (ssid_int < 0 || ssid_int > 15) return false;
        ssid = static_cast<uint8_t>(ssid_int);
    }

    if (out) {
        strncpy(out->callsign, callsign, 6);
        out->callsign[6] = '\0';
        out->ssid = ssid;
    }
    return true;
}

bool PayloadValidator::validate_tx_request_payload(const uint8_t *data, size_t len,
                                                    TxRequestFields *out)
{
    if (!data || len == 0 || len >= kMaxJsonLen) return false;

    char json[kMaxJsonLen];
    memcpy(json, data, len);
    json[len] = '\0';

    // Required: "dest" — callsign rules.
    const char *dv = find_value(json, "dest");
    if (!dv || *dv != '"') return false;
    char dest[8]{};
    if (!extract_string(dv, dest, sizeof(dest))) return false;
    if (!is_valid_callsign(dest)) return false;

    // Required: "text" — 1-67 chars.
    const char *tv = find_value(json, "text");
    if (!tv || *tv != '"') return false;
    char text[69]{};
    if (!extract_string(tv, text, sizeof(text))) return false;
    size_t text_len = strlen(text);
    if (text_len < 1 || text_len > 67) return false;

    // Optional: "ssid" — integer 0-15.
    uint8_t ssid = 0;
    const char *sv = find_value(json, "ssid");
    if (sv) {
        int ssid_int = 0;
        if (!extract_int(sv, &ssid_int)) return false;
        if (ssid_int < 0 || ssid_int > 15) return false;
        ssid = static_cast<uint8_t>(ssid_int);
    }

    if (out) {
        strncpy(out->dest, dest, 6);
        out->dest[6] = '\0';
        out->ssid = ssid;
        strncpy(out->text, text, 67);
        out->text[67] = '\0';
    }
    return true;
}

} // namespace pakt
