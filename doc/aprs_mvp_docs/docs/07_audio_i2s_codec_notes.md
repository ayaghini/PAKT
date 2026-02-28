# Audio + I2S codec notes

## Why I2S codec
- Cleaner ADC/DAC levels than ESP32 ADC/PWM
- Stable sample rate and dynamic range
- Easier to implement consistent TX deviation control

## Codec requirements (functional)
- I2S master/slave compatible with ESP32-S3
- Mono in/out is sufficient
- Support 8 kHz or 16 kHz sample rates
- Simple analog interface (line/mic levels)
- Low power

## Analog front-end considerations
- SA818 AF output level and impedance need conditioning into codec ADC.
- SA818 AF input expects appropriate level to achieve correct deviation:
  - Provide programmable gain (digital scaling + analog trim if needed)
- Add DC blocking, anti-alias filters, and optional limiter.

## AFSK modem
- 1200 baud Bell 202 AFSK:
  - Mark: 1200 Hz, Space: 2200 Hz
- Encoder:
  - NRZI, bit-stuffing, flags (0x7E)
- Decoder:
  - bandpass / Goertzel or PLL approach
  - symbol timing recovery
  - HDLC framing + CRC

## Calibration plan (MVP)
- Provide a "TX CAL" mode:
  - send test tone / known packet pattern
- Provide "RX CAL" mode:
  - measure noise floor and auto-set gain
- Expose `mic_gain` and `rx_gain` in BLE config.

