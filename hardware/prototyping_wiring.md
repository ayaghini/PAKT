# Prototyping Wiring Guide

This document provides a text-based wiring guide for connecting the primary components to an ESP32-S3 development board for prototyping.

**Disclaimer:** This is a logical wiring guide. Always refer to the datasheets for your specific breakout boards for correct pin locations and voltage requirements. GPIO assignments can be changed in the firmware, but this provides a functional starting point.

## Core Components
- **MCU:** ESP32-S3 Development Kit
- **Audio Codec:** WM8960 breakout board
- **GPS Module:** U-blox NEO-M8N breakout board
- **Radio Module:** SA818 VHF Module
- **Fuel Gauge:** MAX17043 breakout board
- **Display:** 0.96" I2C OLED (SSD1306)
- **Indicators:** 3x LEDs (Green, Blue, Red)
- **Inputs:** 1x Momentary Push-Button, 1x Slide Switch for Power
- **Haptics:** 1x Pancake Vibration Motor
- **Charger:** MCP73831 based module or TP4056 module for prototyping
- **Power:** 3.7V LiPo Battery, 3.3V LDO Regulator

---

## 1. Power Distribution

This describes the flow of power through the system.

1. **USB 5V Input** -> **Charger Module (e.g., TP4056) `IN+`**
2. **Charger Module `B+` and `B-`** -> **LiPo Battery Terminals**
3. **LiPo Battery Positive Terminal** -> **Input of Power Slide Switch**
4. **Output of Power Slide Switch** -> **Input of power stage**
5. **Power stage outputs** -> dedicated rails for digital/analog loads

### Power rail requirements (important)
- Do not assume one shared 3.3V rail is adequate for all components during TX.
- SA818 can create current transients during transmit; isolate its supply path from ESP32/codec where practical.
- Size regulators and decoupling for TX peak current with margin.
- Keep star-ground or low-impedance ground return paths between SA818, codec analog section, and battery.
- Add local bulk and high-frequency decoupling near SA818 and ESP32.

**Recommended prototype split:**
- `3.3V_DIG`: ESP32-S3, GPS, OLED, MAX17043.
- `3.3V_AUD`: WM8960 analog supply pins (if breakout allows separation).
- `V_RADIO`: SA818 supply per module requirements (often direct battery or dedicated regulator).

---

## 2. I2C Bus (Inter-Integrated Circuit)

The I2C bus is shared by the Audio Codec, Fuel Gauge, and OLED Display.

- **Bus Master:** ESP32-S3
- **Devices:** WM8960, MAX17043, SSD1306 OLED

| ESP32-S3 Pin | Connects To                                       | Purpose   |
| ------------ | ------------------------------------------------- | --------- |
| **GPIO8**    | WM8960 `SDA`, MAX17043 `SDA`, OLED `SDA`         | I2C Data  |
| **GPIO9**    | WM8960 `SCL`, MAX17043 `SCL`, OLED `SCL`         | I2C Clock |
| `3.3V`       | `3.3V` on all three breakout boards              | Power     |
| `GND`        | `GND` on all three breakout boards               | Ground    |

---

## 3. I2S Bus (Audio Codec)

This bus handles the digital audio data between the ESP32-S3 and the WM8960.

| ESP32-S3 Pin | WM8960 Pin    | Purpose                        | Direction     |
| ------------ | ------------- | ------------------------------ | ------------- |
| **GPIO5**    | `BCLK`        | Bit Clock                      | ESP32 -> Codec|
| **GPIO6**    | `LRC` / `WS`  | Left/Right (Word Select) Clock| ESP32 -> Codec|
| **GPIO7**    | `DIN`         | Data In (TX Audio)             | ESP32 -> Codec|
| **GPIO10**   | `DOUT`        | Data Out (RX Audio)            | Codec -> ESP32|

---

## 4. UART Buses (Serial)

Two separate UART peripherals are used.

### UART 1: GPS Module (NEO-M8N)

| ESP32-S3 Pin | NEO-M8N Pin | Purpose         | Direction    |
| ------------ | ----------- | --------------- | ------------ |
| **GPIO17**   | `RXD`       | GPS Config (TX) | ESP32 -> GPS |
| **GPIO18**   | `TXD`       | NMEA Data (RX)  | GPS -> ESP32 |

### UART 2: Radio Module Control (SA818)

| ESP32-S3 Pin | SA818 Pin | Purpose      | Direction      |
| ------------ | --------- | ------------ | -------------- |
| **GPIO15**   | `RXD`     | SA818 RX Pin | ESP32 -> SA818 |
| **GPIO16**   | `TXD`     | SA818 TX Pin | SA818 -> ESP32 |

---

## 5. Analog and Control Lines

These are direct connections for audio and radio control.

### Audio Path (Codec <-> SA818)

This is an analog connection and should be made with shielded wire if possible.

| WM8960 Breakout Pin | SA818 Pin | Purpose             | Notes                                                      |
| ------------------- | --------- | ------------------- | ---------------------------------------------------------- |
| `LOUT` or `ROUT`    | `AF_IN`   | TX Audio from Codec | AC-coupling capacitor (for example 1-10uF) is required.   |
| `LIN` or `RIN`      | `AF_OUT`  | RX Audio to Codec   | AC-coupling capacitor (for example 1-10uF) is required.   |

### PTT Control (ESP32 -> SA818)

| ESP32-S3 Pin | SA818 Pin | Purpose               |
| ------------ | --------- | --------------------- |
| **GPIO11**   | `PTT`     | Push-to-Talk Control  |

---

## 6. UI and Indicator Connections

These connections are for LEDs, buttons, and haptics.

| ESP32-S3 Pin | Component               | Purpose                  | Wiring Note                                                                 |
| ------------ | ----------------------- | ------------------------ | --------------------------------------------------------------------------- |
| **GPIO38**   | Green LED (Anode)       | Power / Status Indicator | Cathode connects to GND via a current-limiting resistor (for example 330 ohm). |
| **GPIO39**   | Blue LED (Anode)        | RX Activity Indicator    | Cathode connects to GND via a current-limiting resistor (for example 330 ohm). |
| **GPIO40**   | Red LED (Anode)         | TX Activity Indicator    | Cathode connects to GND via a current-limiting resistor (for example 330 ohm). |
| **GPIO41**   | Momentary Button (Pin 1)| Pairing / Function Button| Pin 2 connects to GND. Enable internal pull-up resistor on GPIO.              |
| **GPIO42**   | Vibration Motor Driver  | Haptic Feedback          | Connect to the base of an NPN transistor (for example 2N2222) via about 1k ohm. |

**Vibration Motor Driver Note:** Do not connect the vibration motor directly to a GPIO pin. A simple NPN transistor circuit is required to provide sufficient current.

---

## 7. Final Assembly Notes

- **Antenna Connector:** The SA818 `ANT` pin should connect to the center pin of an SMA panel-mount connector. The ground shield should connect to the system ground plane.
- **Power Switch:** Place the slide switch after the battery and before the power stage input to control system power.
- **TX brownout check:** During prototype bring-up, verify SA818 transmit bursts do not reset ESP32 (scope rail sag during PTT).

---

## Summary Table for ESP32-S3 Connections

| ESP32-S3 GPIO | Primary Function | Connects to Component          |
| ------------- | ---------------- | ------------------------------ |
| 5             | I2S_BCLK         | WM8960                         |
| 6             | I2S_WS           | WM8960                         |
| 7             | I2S_DOUT         | WM8960                         |
| 8             | I2C_SDA          | WM8960, MAX17043, OLED Display |
| 9             | I2C_SCL          | WM8960, MAX17043, OLED Display |
| 10            | I2S_DIN          | WM8960                         |
| 11            | GPIO (PTT)       | SA818                          |
| 15            | UART2_TX         | SA818                          |
| 16            | UART2_RX         | SA818                          |
| 17            | UART1_TX         | NEO-M8N                        |
| 18            | UART1_RX         | NEO-M8N                        |
| 38            | GPIO (LED)       | Green LED (Status)             |
| 39            | GPIO (LED)       | Blue LED (RX)                  |
| 40            | GPIO (LED)       | Red LED (TX)                   |
| 41            | GPIO (Button)    | Pairing / Function Button      |
| 42            | GPIO (Haptics)   | Vibration Motor Driver         |
