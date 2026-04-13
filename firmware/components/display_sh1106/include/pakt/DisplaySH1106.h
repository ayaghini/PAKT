// DisplaySH1106.h – Lightweight SH1106 OLED driver for PAKT bench firmware.
//
// Designed for the 1.3" IIC v2.0 display module on the shared Feather I2C bus
// (GPIO3 SDA, GPIO4 SCL).  Sits alongside SGTL5000 (0x0A), MAX17048 (0x36),
// and u-blox M9N (0x42) on I2C_NUM_0 at 100 kHz.
//
// Bench-safe contract:
//   – init() probes the bus with i2c_master_probe(); if the display is absent
//     is_present() returns false and all subsequent calls are no-ops.
//   – flush() issues ESP-IDF I2C transactions; must not be called from an ISR.
//   – No dynamic allocation; the 1 KB framebuffer lives in BSS.
//   – No blocking waits inside flush(); uses a fixed 50 ms I2C timeout per
//     transaction.  The caller (display_task) owns the retry/skip logic.
//
// Character grid:
//   128 px wide / 64 px tall, 5×7 glyph + 1-px gap → 6×8 cell.
//   kCols = 21 characters per row, kRows = 8 rows.
//
// Usage:
//   DisplaySH1106 oled;
//   if (oled.init(g_shared_i2c_bus)) { ... }
//   oled.clear();
//   oled.draw_text(0, 0, "Hello");
//   oled.draw_hline(7);         // separator at pixel row 7
//   oled.flush();

#pragma once

#include "driver/i2c_master.h"
#include <cstddef>
#include <cstdint>

namespace pakt {

class DisplaySH1106 {
public:
    // SH1106 hardware constants.
    static constexpr uint8_t kDefaultAddr = 0x3C; // A0 pin low (most modules)
    static constexpr int     kWidth       = 128;
    static constexpr int     kHeight      = 64;
    static constexpr int     kPages       = kHeight / 8;  // 8
    static constexpr int     kColOffset   = 2;    // SH1106 internal 132-col map
    static constexpr int     kCharW       = 6;    // glyph (5px) + gap (1px)
    static constexpr int     kCharH       = 8;    // glyph (7px) + gap (1px)
    static constexpr int     kCols        = kWidth  / kCharW;  // 21
    static constexpr int     kRows        = kHeight / kCharH;  // 8

    // Probe for the display and send the initialization sequence.
    // Returns true if the device responds; false means the display is absent
    // and all other methods are no-ops (bench-safe).
    bool init(i2c_master_bus_handle_t bus, uint8_t addr = kDefaultAddr);

    bool is_present() const { return dev_ != nullptr; }

    // Fill framebuffer with zero (all pixels off).  Does not flush.
    void clear();

    // Draw a null-terminated ASCII string at character-grid position (col, row).
    // Clips at display edges.  Returns number of characters drawn.
    int draw_text(int col, int row, const char *text);

    // Turn on every pixel in horizontal pixel row y (0–63).
    void draw_hline(int y);

    // Draw a horizontal battery icon at pixel origin (px, py).
    // Icon footprint: 12 px wide × 7 px tall.
    //   pct 0–100 : fill level (left-to-right bar inside the battery shell).
    //   pct 255   : unknown – renders the shell empty (no fill bar).
    void draw_battery_icon(int px, int py, uint8_t pct);

    // Draw a satellite icon at pixel origin (px, py).
    // Icon footprint: 9 px wide × 7 px tall.
    //   fix = true  : solar panels are 2 px thick (fix acquired).
    //   fix = false : solar panels are 1 px thick (searching / no fix).
    // Pair with a satellite-count text drawn at px + 10 for a complete GPS widget.
    void draw_sat_icon(int px, int py, bool fix);

    // Transfer the in-memory framebuffer to the display over I2C.
    // Each of the 8 pages is written as two transactions (3-byte command +
    // 129-byte data).  Total ≈ 16 I2C transactions @ 100 kHz ≈ 25 ms.
    void flush();

private:
    i2c_master_dev_handle_t dev_         = nullptr;
    uint8_t                 fb_[kPages][kWidth] = {};  // 1 KB framebuffer

    // Low-level helpers.  Return false on I2C error (display then marked absent).
    bool send_cmd(uint8_t cmd);
    bool send_cmds(const uint8_t *buf, size_t n);
    bool send_data(const uint8_t *data, size_t len);

    // Set a single pixel in the framebuffer (clips silently).
    void set_pixel(int x, int y);

    // Rasterise a column-major bitmap into the framebuffer at pixel origin (px, py).
    // data: array of ncols bytes; each byte encodes a 7-pixel column (bit0 = top).
    void draw_col_bitmap(int px, int py, const uint8_t *data, int ncols);

    // Rasterise one character into the framebuffer at pixel origin (px, py).
    void draw_char(int px, int py, char ch);
};

} // namespace pakt
