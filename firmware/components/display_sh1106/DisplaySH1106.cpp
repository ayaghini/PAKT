// DisplaySH1106.cpp – SH1106 OLED driver implementation.
//
// I2C protocol:
//   Control byte 0x00  → all following bytes in this transaction are commands.
//   Control byte 0x40  → all following bytes in this transaction are pixel data.
//
// Page layout (SH1106 default page-addressing mode):
//   8 pages, each 8 pixels tall.  Page byte: bit0 = topmost pixel, bit7 = bottom.
//   Column 0 of the display maps to SH1106 internal column 2 (kColOffset = 2).
//
// Font encoding (kFont5x7):
//   5 bytes per glyph, each byte is a column of 7 pixels (bit0=top, bit6=bottom,
//   bit7 always 0).  Glyphs cover ASCII 0x20 (' ') through 0x7E ('~').
//   Stored in DROM (flash read-only data section) to save RAM.

#include "pakt/DisplaySH1106.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "sh1106";

// ── Embedded 5×7 font (ASCII 0x20–0x7E) ──────────────────────────────────────
// Each entry: 5 column bytes (bit0 = top pixel, bit6 = bottom, bit7 = 0).
// Source: classic Arduino/Adafruit GFX glcdfont – widely used, public-domain.
static const uint8_t kFont5x7[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, // 0x20 ' '
    { 0x00, 0x00, 0x5F, 0x00, 0x00 }, // 0x21 '!'
    { 0x00, 0x07, 0x00, 0x07, 0x00 }, // 0x22 '"'
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, // 0x23 '#'
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, // 0x24 '$'
    { 0x23, 0x13, 0x08, 0x64, 0x62 }, // 0x25 '%'
    { 0x36, 0x49, 0x55, 0x22, 0x50 }, // 0x26 '&'
    { 0x00, 0x05, 0x03, 0x00, 0x00 }, // 0x27 '\''
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, // 0x28 '('
    { 0x00, 0x41, 0x22, 0x1C, 0x00 }, // 0x29 ')'
    { 0x14, 0x08, 0x3E, 0x08, 0x14 }, // 0x2A '*'
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, // 0x2B '+'
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, // 0x2C ','
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, // 0x2D '-'
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, // 0x2E '.'
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, // 0x2F '/'
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, // 0x30 '0'
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, // 0x31 '1'
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, // 0x32 '2'
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, // 0x33 '3'
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, // 0x34 '4'
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, // 0x35 '5'
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // 0x36 '6'
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, // 0x37 '7'
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, // 0x38 '8'
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, // 0x39 '9'
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, // 0x3A ':'
    { 0x00, 0x56, 0x36, 0x00, 0x00 }, // 0x3B ';'
    { 0x08, 0x14, 0x22, 0x41, 0x00 }, // 0x3C '<'
    { 0x14, 0x14, 0x14, 0x14, 0x14 }, // 0x3D '='
    { 0x00, 0x41, 0x22, 0x14, 0x08 }, // 0x3E '>'
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, // 0x3F '?'
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, // 0x40 '@'
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, // 0x41 'A'
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, // 0x42 'B'
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, // 0x43 'C'
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, // 0x44 'D'
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, // 0x45 'E'
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, // 0x46 'F'
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, // 0x47 'G'
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, // 0x48 'H'
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, // 0x49 'I'
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, // 0x4A 'J'
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, // 0x4B 'K'
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, // 0x4C 'L'
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, // 0x4D 'M'
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, // 0x4E 'N'
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, // 0x4F 'O'
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, // 0x50 'P'
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, // 0x51 'Q'
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, // 0x52 'R'
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, // 0x53 'S'
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, // 0x54 'T'
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, // 0x55 'U'
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, // 0x56 'V'
    { 0x3F, 0x40, 0x38, 0x40, 0x3F }, // 0x57 'W'
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, // 0x58 'X'
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, // 0x59 'Y'
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, // 0x5A 'Z'
    { 0x00, 0x7F, 0x41, 0x41, 0x00 }, // 0x5B '['
    { 0x02, 0x04, 0x08, 0x10, 0x20 }, // 0x5C '\'
    { 0x00, 0x41, 0x41, 0x7F, 0x00 }, // 0x5D ']'
    { 0x04, 0x02, 0x01, 0x02, 0x04 }, // 0x5E '^'
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, // 0x5F '_'
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, // 0x60 '`'
    { 0x20, 0x54, 0x54, 0x54, 0x78 }, // 0x61 'a'
    { 0x7F, 0x48, 0x44, 0x44, 0x38 }, // 0x62 'b'
    { 0x38, 0x44, 0x44, 0x44, 0x20 }, // 0x63 'c'
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, // 0x64 'd'
    { 0x38, 0x54, 0x54, 0x54, 0x18 }, // 0x65 'e'
    { 0x08, 0x7E, 0x09, 0x01, 0x02 }, // 0x66 'f'
    { 0x0C, 0x52, 0x52, 0x52, 0x3E }, // 0x67 'g'
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, // 0x68 'h'
    { 0x00, 0x44, 0x7D, 0x40, 0x00 }, // 0x69 'i'
    { 0x20, 0x40, 0x44, 0x3D, 0x00 }, // 0x6A 'j'
    { 0x7F, 0x10, 0x28, 0x44, 0x00 }, // 0x6B 'k'
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, // 0x6C 'l'
    { 0x7C, 0x04, 0x18, 0x04, 0x78 }, // 0x6D 'm'
    { 0x7C, 0x08, 0x04, 0x04, 0x78 }, // 0x6E 'n'
    { 0x38, 0x44, 0x44, 0x44, 0x38 }, // 0x6F 'o'
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, // 0x70 'p'
    { 0x08, 0x14, 0x14, 0x18, 0x7C }, // 0x71 'q'
    { 0x7C, 0x08, 0x04, 0x04, 0x08 }, // 0x72 'r'
    { 0x48, 0x54, 0x54, 0x54, 0x20 }, // 0x73 's'
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, // 0x74 't'
    { 0x3C, 0x40, 0x40, 0x20, 0x7C }, // 0x75 'u'
    { 0x1C, 0x20, 0x40, 0x20, 0x1C }, // 0x76 'v'
    { 0x3C, 0x40, 0x30, 0x40, 0x3C }, // 0x77 'w'
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, // 0x78 'x'
    { 0x0C, 0x50, 0x50, 0x50, 0x3C }, // 0x79 'y'
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }, // 0x7A 'z'
    { 0x00, 0x08, 0x36, 0x41, 0x00 }, // 0x7B '{'
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, // 0x7C '|'
    { 0x00, 0x41, 0x36, 0x08, 0x00 }, // 0x7D '}'
    { 0x10, 0x08, 0x08, 0x10, 0x08 }, // 0x7E '~'
};

static constexpr int kFontFirst = 0x20;
static constexpr int kFontLast  = 0x7E;

namespace pakt {

// ── Low-level I2C helpers ─────────────────────────────────────────────────────

bool DisplaySH1106::send_cmds(const uint8_t *buf, size_t n)
{
    // Build: [0x00 (cmd mode), cmd0, cmd1, ..., cmdN-1]
    uint8_t tx[16 + 1]; // max 16 commands at once; all callers stay within this
    if (n == 0 || n > 16) return false;
    tx[0] = 0x00; // control byte: D/C#=0 (command), Co=0
    std::memcpy(tx + 1, buf, n);
    const esp_err_t err = i2c_master_transmit(dev_, tx, n + 1, 50);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cmd tx error: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool DisplaySH1106::send_cmd(uint8_t cmd)
{
    return send_cmds(&cmd, 1);
}

bool DisplaySH1106::send_data(const uint8_t *data, size_t len)
{
    // SH1106 data packet: [0x40 (data mode), pixel0, pixel1, ...]
    // Maximum one page = 128 bytes; allocate accordingly.
    if (len == 0 || len > kWidth) return false;
    uint8_t tx[kWidth + 1];
    tx[0] = 0x40; // control byte: D/C#=1 (data), Co=0
    std::memcpy(tx + 1, data, len);
    const esp_err_t err = i2c_master_transmit(dev_, tx, len + 1, 50);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "data tx error: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool DisplaySH1106::init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (!bus) return false;

    // Probe before committing a device handle.
    const esp_err_t probe = i2c_master_probe(bus, addr, 30);
    if (probe != ESP_OK) {
        ESP_LOGI(TAG, "SH1106 not found at 0x%02X (probe: %s) – display disabled",
                 addr, esp_err_to_name(probe));
        return false;
    }

    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address  = addr;
    cfg.scl_speed_hz    = 100000;
    if (i2c_master_bus_add_device(bus, &cfg, &dev_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SH1106 device handle");
        dev_ = nullptr;
        return false;
    }

    // SH1106 initialisation sequence.
    // Comments reference SH1106 datasheet command tables.
    const uint8_t init_seq[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Set display clock: divide=1, Fosc=8
        0xA8, 0x3F, // Set multiplex ratio to 63 (64 rows)
        0xD3, 0x00, // Set display offset to 0
        0x40,       // Set start line to 0 (0x40 | 0)
        0xAD, 0x8B, // DC-DC control: internal charge pump ON
        0xA1,       // Set segment remap: SEG127 = col 0 (mirror for typical modules)
        0xC8,       // Set COM scan direction: remapped (bottom-to-top)
        0xDA, 0x12, // Set COM pins: alternative, no left/right remap
        0x81, 0xCF, // Set contrast to 207
        0xD9, 0xF1, // Set pre-charge: phase1=1, phase2=15
        0xDB, 0x40, // Set VCOMH deselect level: 1.0×Vcc
        0xA4,       // Resume from entire display on (follow RAM contents)
        0xA6,       // Set normal display (not inverted)
        0xAF,       // Display ON
    };

    // Send as batches of ≤16 commands.
    size_t sent = 0;
    while (sent < sizeof(init_seq)) {
        const size_t batch = (sizeof(init_seq) - sent > 16) ? 16
                                                             : sizeof(init_seq) - sent;
        if (!send_cmds(init_seq + sent, batch)) {
            ESP_LOGE(TAG, "Init sequence failed at byte %u", static_cast<unsigned>(sent));
            i2c_master_bus_rm_device(dev_);
            dev_ = nullptr;
            return false;
        }
        sent += batch;
    }

    clear();
    flush();
    ESP_LOGI(TAG, "SH1106 OLED ready at 0x%02X", addr);
    return true;
}

void DisplaySH1106::clear()
{
    std::memset(fb_, 0, sizeof(fb_));
}

void DisplaySH1106::draw_char(int px, int py, char ch)
{
    // Clamp to printable range; replace unknowns with '?'.
    if (ch < kFontFirst || ch > kFontLast) ch = '?';
    const uint8_t *glyph = kFont5x7[static_cast<uint8_t>(ch) - kFontFirst];

    for (int col = 0; col < 5; ++col) {
        const int x = px + col;
        if (x < 0 || x >= kWidth) continue;
        const uint8_t bits = glyph[col]; // bit0 = top pixel, bit6 = bottom
        for (int bit = 0; bit < 7; ++bit) {
            const int y = py + bit;
            if (y < 0 || y >= kHeight) continue;
            if (bits & (1u << bit)) {
                const int page = y / 8;
                const int pbit = y % 8;
                fb_[page][x] |= static_cast<uint8_t>(1u << pbit);
            }
        }
    }
    // Column 5 (gap) is left as zero; cleared by clear() each frame.
}

int DisplaySH1106::draw_text(int col, int row, const char *text)
{
    if (!text) return 0;
    int drawn = 0;
    int c = col;
    while (*text && c < kCols) {
        draw_char(c * kCharW, row * kCharH, *text);
        ++c;
        ++text;
        ++drawn;
    }
    return drawn;
}

void DisplaySH1106::draw_hline(int y)
{
    if (y < 0 || y >= kHeight) return;
    const int     page = y / 8;
    const uint8_t mask = static_cast<uint8_t>(1u << (y % 8));
    for (int x = 0; x < kWidth; ++x) {
        fb_[page][x] |= mask;
    }
}

void DisplaySH1106::flush()
{
    if (!dev_) return;

    for (int page = 0; page < kPages; ++page) {
        // Set page address, column address (with SH1106 2-column hardware offset).
        const uint8_t page_cmds[3] = {
            static_cast<uint8_t>(0xB0 | page),              // Set page start address
            static_cast<uint8_t>(0x00 | (kColOffset & 0x0F)), // Column low nibble
            static_cast<uint8_t>(0x10 | (kColOffset >> 4)),   // Column high nibble
        };
        if (!send_cmds(page_cmds, 3)) return; // abort flush on bus error

        // Write 128 bytes of pixel data for this page.
        if (!send_data(fb_[page], kWidth)) return;
    }
}

// ── Icon helpers ──────────────────────────────────────────────────────────────

void DisplaySH1106::set_pixel(int x, int y)
{
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    fb_[y / 8][x] |= static_cast<uint8_t>(1u << (y % 8));
}

void DisplaySH1106::draw_col_bitmap(int px, int py,
                                    const uint8_t *data, int ncols)
{
    for (int c = 0; c < ncols; ++c) {
        const uint8_t bits = data[c];
        for (int b = 0; b < 7; ++b) {
            if (bits & (1u << b)) {
                set_pixel(px + c, py + b);
            }
        }
    }
}

void DisplaySH1106::draw_battery_icon(int px, int py, uint8_t pct)
{
    // Horizontal battery: 12 px wide × 7 px tall.
    //
    // Physical layout (. = off, # = on, F = on when charged):
    //
    //   col: 0  1  2  3  4  5  6  7  8  9  10 11
    //   r0:  #  #  #  #  #  #  #  #  #  #  .  .   ← top border
    //   r1:  #  F  F  F  F  F  F  F  F  #  #  .   ← body + fill + nub
    //   r2:  #  F  F  F  F  F  F  F  F  #  #  .
    //   r3:  #  F  F  F  F  F  F  F  F  #  #  .
    //   r4:  #  F  F  F  F  F  F  F  F  #  #  .
    //   r5:  #  #  #  #  #  #  #  #  #  #  .  .   ← bottom border
    //   r6:  .  .  .  .  .  .  .  .  .  .  .  .   ← gap
    //
    // Col 0, 9 = side borders (0x3F: rows 0-5).
    // Cols 1-8 = fill area: 0x3F (filled) or 0x21 (top+bottom border only).
    // Cols 10-11 = nub: rows 1-4 only (0x1E).
    //
    // fill_cells: number of the 8 interior columns that are filled.

    int fill = (pct == 255) ? 0
             : static_cast<int>((8u * static_cast<unsigned>(pct) + 99u) / 100u);
    if (fill > 8) fill = 8;

    uint8_t cols[12];
    cols[0] = 0x3F;                      // left border
    for (int i = 0; i < 8; ++i) {
        cols[1 + i] = (i < fill) ? 0x3F  // filled column
                                 : 0x21; // border-only column (rows 0 + 5)
    }
    cols[9]  = 0x3F;                     // right border
    cols[10] = 0x1E;                     // nub left  (rows 1-4 = bits 1-4)
    cols[11] = 0x1E;                     // nub right

    draw_col_bitmap(px, py, cols, 12);
}

void DisplaySH1106::draw_sat_icon(int px, int py, bool fix)
{
    // Satellite icon: 9 px wide × 7 px tall.
    // Encoding: column bytes, bit0 = top pixel (row 0).
    //
    // The body is 3 px wide (cols 3-5), centred on a vertical antenna that
    // runs through col 4 (rows 0 and 5).  Solar panels flank the body on
    // each side; their thickness indicates fix status:
    //
    //   No fix (thin panels – 1 px):
    //   col: 0  1  2  3  4  5  6  7  8
    //   r0:  .  .  .  .  #  .  .  .  .   ← antenna tip
    //   r1:  .  #  .  #  #  #  .  #  .   ← panel outer + body top
    //   r2:  .  #  .  #  #  #  .  #  .   ← panel + body
    //   r3:  .  #  .  #  #  #  .  #  .   ← panel + body
    //   r4:  .  .  .  #  #  #  .  .  .   ← body bottom
    //   r5:  .  .  .  .  #  .  .  .  .   ← antenna lower
    //   r6:  .  .  .  .  .  .  .  .  .   ← gap
    //
    //   Fix (thick panels – 2 px + connector):
    //   col: 0  1  2  3  4  5  6  7  8
    //   r0:  .  .  .  .  #  .  .  .  .   ← antenna tip
    //   r1:  #  #  .  #  #  #  .  #  #   ← panels + body top
    //   r2:  #  #  #  #  #  #  #  #  #   ← full panels + connectors + body
    //   r3:  #  #  .  #  #  #  .  #  #   ← panels + body
    //   r4:  .  .  .  #  #  #  .  .  .   ← body bottom
    //   r5:  .  .  .  .  #  .  .  .  .   ← antenna lower
    //   r6:  .  .  .  .  .  .  .  .  .

    // No fix: thin single-pixel-wide panels on the outer column only.
    static const uint8_t kNoFix[9] = {
        0x00, // col0: empty (panel gap)
        0x0E, // col1: r1,r2,r3         (outer panel column)
        0x00, // col2: empty (gap between panel and body)
        0x1E, // col3: r1,r2,r3,r4      (body left side)
        0x3F, // col4: r0,r1,r2,r3,r4,r5 (body centre + antenna)
        0x1E, // col5: r1,r2,r3,r4      (body right side)
        0x00, // col6: empty (gap between body and panel)
        0x0E, // col7: r1,r2,r3         (outer panel column)
        0x00, // col8: empty (panel gap)
    };
    // Fix: thick two-pixel-wide panels with a connector row at r2.
    static const uint8_t kFix[9] = {
        0x0E, // col0: r1,r2,r3         (inner panel column)
        0x0E, // col1: r1,r2,r3         (outer panel column)
        0x04, // col2: r2               (connector at mid-row)
        0x1E, // col3: r1,r2,r3,r4      (body left)
        0x3F, // col4: r0,r1,r2,r3,r4,r5 (body centre + antenna)
        0x1E, // col5: r1,r2,r3,r4      (body right)
        0x04, // col6: r2               (connector at mid-row)
        0x0E, // col7: r1,r2,r3         (outer panel column)
        0x0E, // col8: r1,r2,r3         (inner panel column)
    };

    draw_col_bitmap(px, py, fix ? kFix : kNoFix, 9);
}

} // namespace pakt
