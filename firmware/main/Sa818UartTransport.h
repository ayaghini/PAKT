#pragma once
// Sa818UartTransport – ESP-IDF UART concrete ISa818Transport
//
// Wraps ESP-IDF uart_write_bytes / uart_read_bytes behind the ISa818Transport
// interface so Sa818Radio can be tested without ESP-IDF.
//
// Prerequisites (performed by radio_task before constructing this object):
//   uart_driver_install(port, rx_buf, tx_buf, 0, NULL, 0)
//   uart_param_config(port, &uart_config)   // 9600 8N1
//   uart_set_pin(port, TX_PIN, RX_PIN, ...)
//
// ESP-IDF only: do NOT include in the host test build.

#include "pakt/ISa818Transport.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace pakt {

class Sa818UartTransport final : public ISa818Transport
{
public:
    explicit Sa818UartTransport(uart_port_t port = UART_NUM_1)
        : port_(port) {}

    bool write(const char *data, size_t len) override
    {
        int sent = uart_write_bytes(port_, data, static_cast<int>(len));
        return sent == static_cast<int>(len);
    }

    size_t read(char *buf, size_t len, uint32_t timeout_ms) override
    {
        int n = uart_read_bytes(port_,
                                reinterpret_cast<uint8_t *>(buf),
                                static_cast<uint32_t>(len),
                                pdMS_TO_TICKS(timeout_ms));
        return (n > 0) ? static_cast<size_t>(n) : 0u;
    }

private:
    uart_port_t port_;
};

} // namespace pakt
