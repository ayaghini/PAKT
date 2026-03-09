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

## Codec Component Candidates

Current project baseline is SGTL5000.
All parts below are stereo ADC+DAC codecs; mono operation is supported by using one channel in firmware and routing.

### Primary recommendation (current): SGTL5000

- Type: full stereo codec (ADC + DAC), I2S, I2C control.
- Why it fits:
  - Development friendly with broad hobby/prototyping board availability.
  - Good match for 8 kHz/16 kHz narrowband modem use.
  - Sufficient gain/routing control for SA818 AF calibration in MVP scope.
- Important integration note:
  - Requires valid master clock input (`SYS_MCLK`); include explicit MCLK routing from ESP32-S3.
- Tradeoff:
  - Fewer advanced routing/processing features than higher-end codecs.

### Alternative A: TLV320AIC3204

- Type: full stereo codec (ADC + DAC), I2S, I2C control.
- Why it fits:
  - Very flexible gain/routing and strong documentation.
  - Good fallback for advanced calibration or tighter analog control needs.
- Tradeoff:
  - Higher configuration complexity.

### Alternative B: TLV320AIC3101 / TLV320AIC3104 family

- Type: full stereo codec family (ADC + DAC), I2S, I2C control.
- Why it fits:
  - Lower complexity path within TI ecosystem.
  - Good low-power options for battery operation.
- Tradeoff:
  - Feature set varies by exact part; verify routing/gain features before lock.

### Alternative C: ADAU1761

- Type: full stereo codec with integrated DSP features.
- Why it fits:
  - Very capable signal routing and processing.
  - Strong Analog Devices tooling and evaluation hardware.
- Tradeoff:
  - Highest integration complexity; best if advanced on-device audio shaping is needed.

### Recommendation by phase

- EVT/prototyping fastest path: SGTL5000 breakout + known ESP32 I2S driver path.
- Production-oriented path: SGTL5000 default, with AIC3204 fallback if calibration flexibility proves insufficient.

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
