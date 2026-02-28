# Hardware interfaces (schematic handoff)

## External interfaces
- USB-C: charging and optional debug/USB-serial.
- Antenna: SMA connector, 50-ohm path from SA818 RF output.
- Optional 3.5mm debug audio jack (development only).
- Buttons:
  - Power switch (hardware battery path)
  - Function button (pair and beacon actions)
- Indicators:
  - Charging status
  - BLE status
  - TX and RX status

## Internal electrical interfaces
- ESP32-S3 UART <-> SA818 UART (3.3V TTL).
- ESP32-S3 GPIO -> SA818 PTT.
- ESP32-S3 I2S <-> WM8960 codec.
- WM8960 analog out -> SA818 AF_IN (AC-coupled).
- SA818 AF_OUT -> WM8960 analog in (AC-coupled).
- ESP32-S3 UART <-> GPS NMEA.
- Optional GPS PPS -> ESP32 interrupt-capable GPIO.

## Required named nets
- Power: `VBAT_RAW`, `V_RADIO`, `V_SYS_3V3`, `V_AUD_3V3`, `GND`.
- Radio control: `SA818_PTT`, `SA818_RX_CTRL`, `SA818_TX_STAT`.
- Audio: `AF_TX_COUPLED`, `AF_RX_COUPLED`.
- GPS: `GPS_TX_NMEA`, `GPS_RX_CTRL`, optional `GPS_PPS`.
- Shared buses: `I2C_SDA`, `I2C_SCL`, `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`.
- UI: `LED_STATUS_G`, `LED_RX_B`, `LED_TX_R`, `BTN_FUNC_N`, `HAPTIC_DRV`.
