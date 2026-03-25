// Sa818Radio.cpp
#include "pakt/Sa818Radio.h"
#include "pakt/Sa818CommandFormatter.h"
#include "pakt/Sa818ResponseParser.h"
#include <cstdio>
#include <cstring>

namespace pakt {

static constexpr size_t kCmdBufLen  = 64;
static constexpr size_t kRespBufLen = 32;

Sa818Radio::Sa818Radio(ISa818Transport &transport, PttGpioFn ptt_fn)
    : transport_(transport), ptt_fn_(std::move(ptt_fn))
{}

void Sa818Radio::force_ptt_off()
{
    transmitting_ = false;
    if (ptt_fn_) ptt_fn_(false);
}

bool Sa818Radio::exchange(const char *cmd, size_t cmd_len,
                           char *resp_buf, size_t resp_len,
                           uint32_t timeout_ms)
{
    if (!transport_.write(cmd, cmd_len)) {
        force_ptt_off();
        return false;
    }
    size_t n = transport_.read(resp_buf, resp_len - 1, timeout_ms);
    if (n == 0) {
        force_ptt_off();
        return false;
    }
    resp_buf[n] = '\0';
    return true;
}

bool Sa818Radio::init()
{
    // PTT always off on entry, regardless of prior state.
    force_ptt_off();

    char cmd[kCmdBufLen];
    char resp[kRespBufLen];

    size_t n = Sa818CommandFormatter::connect(cmd, sizeof(cmd));
    if (n == 0) return false;

    if (!exchange(cmd, n, resp, sizeof(resp))) {
        error_ = true;
        return false;
    }
    if (Sa818ResponseParser::parse_connect(resp) != Sa818ResponseParser::Result::Ok) {
        force_ptt_off();
        error_ = true;
        return false;
    }

    initialized_ = true;
    return true;
}

bool Sa818Radio::set_freq(uint32_t rx_hz, uint32_t tx_hz)
{
    if (error_) return false;

    // Idempotent: skip UART round-trip when values have not changed.
    if (initialized_ && rx_hz == rx_hz_ && tx_hz == tx_hz_) return true;

    char cmd[kCmdBufLen];
    char resp[kRespBufLen];

    size_t n = Sa818CommandFormatter::set_group(
        cmd, sizeof(cmd), rx_hz, tx_hz, squelch_, /*wide_band=*/true);
    if (n == 0) return false;

    if (!exchange(cmd, n, resp, sizeof(resp))) {
        error_ = true;
        return false;
    }
    if (Sa818ResponseParser::parse_set_group(resp) != Sa818ResponseParser::Result::Ok) {
        force_ptt_off();
        error_ = true;
        return false;
    }

    rx_hz_ = rx_hz;
    tx_hz_ = tx_hz;
    return true;
}

bool Sa818Radio::set_squelch(uint8_t level)
{
    squelch_ = level;
    // If already initialized and a frequency is configured, apply immediately.
    // Squelch is part of AT+DMOSETGROUP so a re-send is required; the caller
    // cannot rely on the next set_freq() call because of the idempotency check.
    if (!initialized_ || rx_hz_ == 0) return true;
    char cmd[kCmdBufLen];
    char resp[kRespBufLen];
    const size_t n = Sa818CommandFormatter::set_group(
        cmd, sizeof(cmd), rx_hz_, tx_hz_, squelch_, /*wide_band=*/true);
    if (n == 0) return false;
    if (!exchange(cmd, n, resp, sizeof(resp))) return false;
    return Sa818ResponseParser::parse_set_group(resp) == Sa818ResponseParser::Result::Ok;
}

bool Sa818Radio::set_volume(uint8_t level)
{
    if (error_) return false;
    char cmd[kCmdBufLen];
    char resp[kRespBufLen];
    const int n = snprintf(cmd, sizeof(cmd), "AT+DMOSETVOLUME=%u\r\n",
                           static_cast<unsigned>(level));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(cmd)) return false;
    if (!exchange(cmd, static_cast<size_t>(n), resp, sizeof(resp))) return false;
    const char *colon = strchr(resp, ':');
    return colon && colon[1] == '0';
}

bool Sa818Radio::set_power(RadioPower power)
{
    power_ = power;
    return true;   // cached; SA818 power control applied on next group set
}

bool Sa818Radio::ptt(bool on)
{
    // ptt(false) must succeed even in error state to ensure safe de-assertion.
    if (on && error_) return false;
    transmitting_ = on;
    if (ptt_fn_) ptt_fn_(on);
    return true;
}

} // namespace pakt
