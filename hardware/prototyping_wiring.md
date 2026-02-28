# Prototyping Wiring Guide (Schematic-Ready Draft)

This document defines a wiring baseline that can be transferred into schematic capture and first-pass PCB layout.

## Scope and assumptions
- Target architecture: ESP32-S3 + SA818 + WM8960 + NEO-M8N + MAX17043.
- Logic domain is 3.3V unless a module explicitly requires otherwise.
- SA818 supply must be validated against the exact module datasheet before PCB release.
- Final design must pass bench validation items in Section 10 before fabrication release.

## 1. Power Tree

### Functional flow
1. USB-C 5V input -> Li-ion charger input.
2. Charger BAT output -> single-cell Li-ion battery.
3. Battery rail -> power switch/load switch.
4. Switched battery rail feeds:
   - `V_RADIO` (SA818 path)
   - `V_SYS_3V3` regulator path (digital and codec domains)

### Required rails
- `VBAT_RAW`: battery node from charger/battery connector.
- `V_SYS_3V3`: primary regulated 3.3V digital rail.
- `V_AUD_3V3`: filtered codec analog rail (L or ferrite bead from `V_SYS_3V3`).
- `V_RADIO`: SA818 supply rail, isolated from MCU rail return noise.

### Power integrity requirements
- Place local bulk capacitance near SA818 supply entry.
- Place 100nF decoupling at every IC power pin, shortest loop to local ground.
- Add at least one bulk capacitor near ESP32-S3 module supply entry.
- Keep SA818 TX current return out of codec analog ground path.
- Include brownout test point on `V_SYS_3V3` and `V_RADIO`.

## 2. Digital Interfaces

### I2C shared bus
| ESP32-S3 Pin | Net        | Device Pins                           |
|--------------|------------|----------------------------------------|
| GPIO8        | I2C_SDA    | WM8960 SDA, MAX17043 SDA, SSD1306 SDA |
| GPIO9        | I2C_SCL    | WM8960 SCL, MAX17043 SCL, SSD1306 SCL |

I2C requirements:
- One pull-up set only per bus (typically 2.2k to 4.7k to 3.3V).
- Confirm no duplicate strong pull-ups on all breakouts in prototype.
- Keep bus short and route away from antenna feed and SA818 RF section.

### I2S bus (ESP32 master)
| ESP32-S3 Pin | Net      | WM8960 Pin |
|--------------|----------|------------|
| GPIO5        | I2S_BCLK | BCLK       |
| GPIO6        | I2S_WS   | LRC/WS     |
| GPIO7        | I2S_DOUT | DIN        |
| GPIO10       | I2S_DIN  | DOUT       |

I2S requirements:
- Keep traces short and length-matched where practical.
- Route over continuous ground reference.
- Avoid running parallel to RF or high-current TX supply routes.

### UART links
| Interface | ESP32-S3 Pin | Net           | Remote Pin |
|-----------|---------------|---------------|------------|
| GPS TX    | GPIO17        | GPS_RX_CTRL   | NEO RXD    |
| GPS RX    | GPIO18        | GPS_TX_NMEA   | NEO TXD    |
| SA818 TX  | GPIO15        | SA818_RX_CTRL | SA818 RXD  |
| SA818 RX  | GPIO16        | SA818_TX_STAT | SA818 TXD  |

Optional but recommended:
- Route GPS PPS to spare ESP32 interrupt-capable GPIO.

## 3. Control and UI Signals

| ESP32-S3 Pin | Net           | Destination            | Notes |
|--------------|---------------|------------------------|-------|
| GPIO11       | SA818_PTT     | SA818 PTT              | Verify active polarity from datasheet; add pull resistor and optional transistor stage footprint. |
| GPIO38       | LED_STATUS_G  | Green LED anode        | Series resistor required. |
| GPIO39       | LED_RX_B      | Blue LED anode         | Series resistor required. |
| GPIO40       | LED_TX_R      | Red LED anode          | Series resistor required. |
| GPIO41       | BTN_FUNC_N    | Function button to GND | Use internal or external pull-up; add debounce in firmware. |
| GPIO42       | HAPTIC_DRV    | NPN/MOSFET gate/base   | Use transistor driver + flyback diode if motor type requires it. |

## 4. Analog Audio Interface (WM8960 <-> SA818)

| Source         | Destination | Net            | Required conditioning |
|----------------|-------------|----------------|-----------------------|
| WM8960 DAC out | SA818 AF_IN | AF_TX_COUPLED  | AC coupling capacitor + optional trim attenuation network footprint. |
| SA818 AF_OUT   | WM8960 ADC  | AF_RX_COUPLED  | AC coupling capacitor + optional RC low-pass footprint. |

Analog design rules:
- Keep AF paths short, away from antenna and switching regulators.
- Use a dedicated analog ground island tied at a single low-impedance point.
- Provide footprints for optional gain trim (resistor divider/pot footprint during EVT).
- Add test pads for AF_TX_COUPLED and AF_RX_COUPLED.

## 5. RF and External Connectors

- SA818 ANT pin -> 50-ohm controlled path -> SMA connector center pin.
- Maintain solid ground around antenna connector and SA818 RF section.
- Add ESD protection on USB and user-exposed lines.
- Keep high-speed digital lines out of RF keep-out region.

## 6. Battery and Charging

- Prefer MCP73831-class charger for final PCB.
- If using USB-C receptacle, include CC resistors for sink configuration.
- Add battery connector reverse-polarity protection strategy.
- Provide charger status output to LED or MCU GPIO if available.

## 7. Suggested Supporting Circuit Footprints

Add these as DNP-capable where uncertain in EVT:
- PTT level-shift/transistor stage footprint.
- AF_TX attenuation resistor ladder footprint.
- AF_RX RC filter footprint.
- Ferrite bead between `V_SYS_3V3` and `V_AUD_3V3`.
- Extra bulk capacitor pads on `V_RADIO`.
- Optional series resistors (22 to 47 ohm) on I2S lines for edge control.

## 8. Pin Reservation and Bring-Up Notes

- Keep boot-critical ESP32 pins out of external pull conflicts.
- Avoid assigning strapping-sensitive pins to external circuits that can force boot states.
- Keep one spare GPIO for recovery/diagnostics.
- Add SW/serial debug header access for early firmware bring-up.

## 9. Schematic Handoff Netlist Summary

Core named nets to preserve in schematic:
- Power: `VBAT_RAW`, `V_RADIO`, `V_SYS_3V3`, `V_AUD_3V3`, `GND`.
- Audio: `AF_TX_COUPLED`, `AF_RX_COUPLED`.
- Radio control: `SA818_PTT`, `SA818_RX_CTRL`, `SA818_TX_STAT`.
- GPS: `GPS_TX_NMEA`, `GPS_RX_CTRL`, optional `GPS_PPS`.
- I2C: `I2C_SDA`, `I2C_SCL`.
- I2S: `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`.
- UI: `LED_STATUS_G`, `LED_RX_B`, `LED_TX_R`, `BTN_FUNC_N`, `HAPTIC_DRV`.

## 10. Pre-PCB Validation Checklist

Before committing to PCB rev A:
1. Confirm SA818 supply voltage/current limits on exact purchased module.
2. Confirm SA818 PTT polarity and UART electrical levels.
3. Measure required AF_TX level for proper deviation (target packet decode quality).
4. Measure AF_RX level/noise floor into codec path.
5. Verify no ESP32 resets during repeated TX bursts.
6. Verify GPS lock and UART stability while TX/RX active.
7. Verify I2C bus rise time with all attached peripherals.
8. Verify BLE operation while RX audio pipeline is active.
