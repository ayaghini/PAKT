// sa818_bench_test.h — Staged bring-up test for the SA818-V VHF radio module
//
// ── Wiring baseline (verified) ───────────────────────────────────────────────
//   ESP GPIO13  →  SA818 RXD  (UART TX from ESP perspective)
//   SA818 TXD   →  ESP GPIO9  (UART RX from ESP perspective)
//   ESP GPIO11  →  SA818 PTT  (active-low: LOW = TX, HIGH = RX/idle)
//   SA818 AF_OUT → SGTL5000 LINE_IN L  (via AC coupling cap)
//   SGTL5000 LINE_OUT L → SA818 AF_IN  (via AC coupling cap + attenuation pad)
//
// ── Test stages ──────────────────────────────────────────────────────────────
//   1. UART handshake  – AT+DMOCONNECT, raw byte log, retry up to 3×
//   2. Version query   – AT+DMOVERQ (informational, not required to pass)
//   3. Frequency config – 144.390 MHz simplex, 25 kHz BW, squelch 1
//   4. PTT toggle      – 500 ms assert / deassert; GPIO state logged
//   5. RX audio        – squelch open, volume max; operator transmits nearby
//   6. TX audio        – brief 1 kHz tone via LINE_OUT → SA818 AF_IN
//                        waits up to `tx_wait_ms` for I2S TX to become ready
//
// ── Usage ────────────────────────────────────────────────────────────────────
//   Called from radio_task AFTER UART driver is installed and transport is
//   constructed, BEFORE the normal radio.init() / set_freq() sequence.
//   Blocking; returns when all stages are done (or timed out).
//
//   To skip: remove the run_sa818_bench() call in radio_task.

#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2s_std.h"
#include "pakt/ISa818Transport.h"
#include <cstdint>

namespace pakt::bench {

// Run the full SA818 staged bench test.
//
//   transport      – configured UART transport (radio_task creates this before calling)
//   ptt_gpio       – active-low PTT GPIO (HIGH = off, LOW = TX asserted)
//   uart_port      – used only for log display (communication is via transport)
//   p_i2s_tx       – pointer to the global I2S TX handle; bench polls until
//                    non-null for Stage 6. Pass nullptr to skip TX audio stage.
//   sample_rate_hz – I2S sample rate (must match audio init; typically 8000)
//   tx_wait_ms     – max milliseconds to wait for I2S TX to become available
void run_sa818_bench(pakt::ISa818Transport &transport,
                     gpio_num_t             ptt_gpio,
                     uart_port_t            uart_port,
                     const volatile void   *p_i2s_tx,
                     uint32_t               sample_rate_hz,
                     uint32_t               tx_wait_ms = 120000);

} // namespace pakt::bench
