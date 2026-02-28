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

Based on research, the following components are strong candidates for the project. The primary goal is to find a component that is available on a breakout board for easy prototyping.

### Primary Recommendation: WM8960

*   **Type:** Full stereo codec (ADC + DAC).
*   **Availability:** Excellent. Widely available on low-cost breakout boards, often marketed for the Raspberry Pi.
*   **Key Features:**
    *   **Programmable Gain Amplifier (PGA):** Allows software control over the input gain, which is essential for calibrating the RX level from the SA818.
    *   **Headphone Driver:** The integrated headphone driver can be used for monitoring or debugging the audio signals directly from the device.
    *   **Good Support:** Well-documented and supported by various communities.

### Alternative: TLV320AIC3204

*   **Type:** Full stereo codec (ADC + DAC).
*   **Availability:** Less common on hobbyist breakout boards, more suited for a custom PCB design.
*   **Key Features:**
    *   **Very Powerful:** A highly configurable and powerful codec from Texas Instruments.
    *   **Low Power:** Designed for portable audio applications.
    *   **Complexity:** Higher complexity in configuration compared to the WM8960, but offers more fine-grained control.

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

