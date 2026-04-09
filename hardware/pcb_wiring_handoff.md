# PCB Wiring Handoff

This is the schematic-entry wiring guide for the first KiCad pass.

## 1. Power architecture

### Main flow
1. USB-C `VBUS` feeds the charger input.
2. Charger output and battery connector meet at `VBAT_RAW`.
3. `VBAT_RAW` feeds:
   - the fuel gauge `MAX17048`
   - the main system power path / regulator input
   - the radio power branch
4. Main regulation produces `V_SYS_3V3`.
5. `V_AUD_3V3` is derived from `V_SYS_3V3` through filtering for the codec analog domain.

### Required power nets
- `VBUS`
- `VBAT_RAW`
- `V_SYS_3V3`
- `V_AUD_3V3`
- `V_RADIO`
- `GND`

### Battery section rules
- Follow the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` bench-board battery behavior.
- Keep charger, battery connector, fuel gauge, and battery bulk capacitor physically close.
- Keep the SA818 high-current return away from codec analog return routing.

## 2. ESP32-S3 pin map to preserve

| GPIO | Net | Destination |
|---|---|---|
| GPIO3 | `I2C_SDA` | SGTL5000, MAX17048, GPS |
| GPIO4 | `I2C_SCL` | SGTL5000, MAX17048, GPS |
| GPIO8 | `I2S_BCLK` | SGTL5000 |
| GPIO15 | `I2S_WS` | SGTL5000 |
| GPIO12 | `I2S_DOUT` | SGTL5000 |
| GPIO10 | `I2S_DIN` | SGTL5000 |
| GPIO14 | `I2S_MCLK` | SGTL5000 |
| GPIO13 | `SA818_RX_CTRL` | SA818 RXD |
| GPIO9 | `SA818_TX_STAT` | SA818 TXD |
| GPIO11 | `SA818_PTT` | SA818 PTT |
| GPIO17 | `GPS_RX_CTRL` | GPS RXD fallback |
| GPIO18 | `GPS_TX_NMEA` | GPS TXD fallback |

## 3. Shared I2C bus

### Bus members
- `SGTL5000 @ 0x0A`
- `MAX17048 @ 0x36`
- `u-blox M9N @ 0x42`

### Rules
- Only one effective pull-up set on the bus.
- Keep the bus short and away from the SA818 RF area.
- Preserve shared-I2C GPS as the default path.

## 4. I2S audio bus

### Signals
- `I2S_BCLK`
- `I2S_WS`
- `I2S_DOUT`
- `I2S_DIN`
- `I2S_MCLK`

### Rules
- `I2S_MCLK` is mandatory for the SGTL5000.
- Keep the I2S group compact and referenced to a continuous ground plane.
- If edge quality becomes poor, reserve `22 Ohm` to `47 Ohm` series resistor footprints near the MCU.

## 5. Codec-to-radio analog path

### TX path
- `SGTL5000 LINE_OUT_L`
- AC-coupling capacitor
- DNP attenuation network
- `SA818 AF_IN`

### RX path
- `SA818 AF_OUT`
- AC-coupling capacitor
- optional DNP RC filter
- `SGTL5000 LINE_IN_L`

### Rules
- Keep analog routing short and away from RF and switching noise.
- Add accessible test points for `AF_TX_COUPLED` and `AF_RX_COUPLED`.
- Preserve the left-channel path that was proven on the bench.

## 6. SA818 interface

### Digital control
- `SA818_RX_CTRL`
- `SA818_TX_STAT`
- `SA818_PTT`

### Rules
- `SA818_PTT` is active low in the current bench baseline.
- Keep a DNP transistor stage available if direct GPIO drive needs conditioning.
- Add local `100 nF`, `10 uF`, and extra bulk capacitance at the SA818 supply entry.

## 7. GPS interface

### Default path
- Shared `I2C_SDA` / `I2C_SCL`

### Fallback path
- `GPS_TX_NMEA`
- `GPS_RX_CTRL`
- optional `GPS_PPS`

### Rules
- Keep GPS physically separated from the SA818 RF and power-burst area.
- Preserve UART fallback pads if space permits.

## 8. Bring-up-critical notes
1. Start `I2S/MCLK` before SGTL5000 I2C configuration.
2. Confirm `AT+DMOCONNECT` on the radio path before RF testing.
3. Measure TX deviation before locking the final `AF_TX` attenuation values.
4. Confirm no brownout during repeated TX bursts before PCB release.
