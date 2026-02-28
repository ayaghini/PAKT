# Hardware interfaces (draft)

## External
- USB-C: charging + optional debug/USB-serial
- Antenna: SMA (recommended for VHF)
- Optional: 3.5mm jack for debug audio (not required if BLE-only)
- Buttons:
  - Power / wake
  - “Pair” / “Beacon now” multifunction
- LEDs:
  - Charging
  - BLE status
  - TX indicator

## Internal signals
- ESP32-S3 UART -> SA818 UART (3.3V TTL)
- ESP32 GPIO -> PTT
- I2S -> codec
- Codec analog in/out -> SA818 AF in/out
- UART -> GPS NMEA (and PPS optional)
