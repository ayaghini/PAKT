# Hardware interfaces (schematic handoff)

## External interfaces
- USB-C: charging and optional native USB/debug.
- Antenna: SMA or equivalent `50 ohm` path from SA818 RF output.
- Single-cell Li-ion battery connector.
- Buttons:
  - Reset / boot on the MCU support side
  - Function button on the product side if retained
- Indicators:
  - Charging status
  - BLE / status
  - TX and RX status

## Internal electrical interfaces
- ESP32-S3 UART <-> SA818 UART (`3.3V` TTL)
- ESP32-S3 GPIO -> SA818 PTT
- ESP32-S3 I2S <-> SGTL5000 codec
- ESP32-S3 MCLK output -> SGTL5000 `SYS_MCLK`
- SGTL5000 analog out -> SA818 `AF_IN` (AC-coupled)
- SA818 `AF_OUT` -> SGTL5000 analog in (AC-coupled)
- ESP32-S3 shared I2C <-> SGTL5000
- ESP32-S3 shared I2C <-> MAX17048
- ESP32-S3 shared I2C <-> GPS (default path)
- ESP32-S3 UART <-> GPS NMEA (fallback path)
- Optional GPS PPS -> ESP32 interrupt-capable GPIO
- MCP73831 `STAT` output -> charge LED and optional observation node

## Battery-section reference rule
- The charger and fuel-gauge topology should follow the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM`, because that is the confirmed bench board and Adafruit identifies `MAX17048` on that variant.

## Required named nets
- Power: `VBUS`, `VBAT_RAW`, `V_RADIO`, `V_SYS_3V3`, `V_AUD_3V3`, `GND`
- Radio control: `SA818_PTT`, `SA818_RX_CTRL`, `SA818_TX_STAT`
- Audio: `AF_TX_COUPLED`, `AF_RX_COUPLED`
- GPS: `GPS_TX_NMEA`, `GPS_RX_CTRL`, optional `GPS_PPS`
- Shared buses: `I2C_SDA`, `I2C_SCL`, `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`, `I2S_MCLK`
