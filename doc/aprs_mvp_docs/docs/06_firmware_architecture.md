# Firmware architecture (draft)

## RTOS model
ESP32-S3 FreeRTOS tasks suggested:
1. `radio_task`
   - SA818 UART config
   - PTT control
   - TX scheduling
2. `audio_task`
   - I2S read/write (codec)
   - AFSK demod/encode pipeline
3. `aprs_task`
   - AX.25 encode/decode
   - APRS parsing/building (position/message)
   - Message retry/ack state machine
4. `gps_task`
   - NMEA parse
   - Fix aging, smoothing, time
5. `ble_task`
   - GATT services, notify throttling
   - Config storage
6. `power_task`
   - battery measurement
   - sleep policy, wake sources

## Key interfaces
- `IAudioIO` (codec abstraction): `read_samples()`, `write_samples()`
- `IRadioControl`: `set_freq()`, `set_squelch()`, `ptt(on/off)`
- `IPacketLink`: send/receive AX.25 frames
- `IStorage`: config + logs

## Data structures
- Ring buffer for audio samples
- Packet queue:
  - RX decoded frames -> BLE notify
  - TX requests -> scheduler

## Timing
- Use precise sample rate (e.g., 8 kHz) for AFSK modem.
- Ensure TX preamble flags and bit-stuffing per AX.25.
- Rate-limit BLE notifications to avoid starving modem.
