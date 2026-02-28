# Component Selection Rationale

This document outlines the reasoning behind the selection of key components for the APRS Pocket TNC MVP. The primary goal is to select components that are not only fit-for-purpose but are also well-documented, available for prototyping, and have good community/library support.

## 1. I2S Audio Codec

The interface between the ESP32 (digital) and the SA818 radio module (analog) is critical for modem performance. An external codec is chosen over the ESP32's internal DAC/ADC for superior signal quality and noise immunity.

*   **Selected Component:** **WM8960**
*   **Alternatives:** TLV320AIC3204
*   **Rationale:**
    *   **Full Codec:** The WM8960 is a complete stereo codec, providing both ADC (for RX) and DAC (for TX) in a single package.
    *   **Availability:** It is widely available on low-cost breakout boards (often purple boards originally intended for the Raspberry Pi), making it ideal for rapid prototyping.
    *   **Features:** It includes a Programmable Gain Amplifier (PGA) on its inputs, which is critical for adjusting the RX audio level via software to handle varying signal strengths. It also includes a headphone driver which can be repurposed for debugging or even for direct audio output.
    *   **Documentation:** The WM8960 is well-documented and has existing driver support in many embedded environments.

## 2. GPS Module

Reliable and fast position acquisition is a core feature of the device.

*   **Selected Component:** **U-blox NEO-M8N**
*   **Alternatives:** U-blox NEO-6M, various MediaTek (MTK) modules.
*   **Rationale:**
    *   **Performance:** U-blox modules are widely regarded as the industry standard for performance and reliability.
    *   **Multi-Constellation Support:** The NEO-M8N can receive signals from multiple GNSS constellations simultaneously (GPS, GLONASS, Galileo, BeiDou). This significantly reduces Time-To-First-Fix (TTFF) and improves position accuracy, especially in challenging environments (urban canyons, etc.).
    *   **Interface:** It uses a standard UART interface for NMEA data, which is simple to connect to the ESP32.
    *   **Support:** U-blox modules have excellent library support in the Arduino/ESP32 ecosystem (e.g., TinyGPS++).

## 3. Power Management

The power subsystem is comprised of two main functions: charging the LiPo battery and monitoring its state.

### 3.1. Battery Charger

*   **Selected Component:** **MCP73831**
*   **Alternatives:** TP4056, BQ24075.
*   **Rationale:**
    *   **Simplicity and Reliability:** The MCP73831 is a simple, reliable single-cell LiPo charger IC in a small footprint. It is a significant step up from the ubiquitous TP4056 as it has better thermal management and is designed for standalone operation.
    *   **Prototyping:** While the TP4056 is common on modules, the MCP73831 is a better choice for a final PCB design. For initial prototyping, a pre-built TP4056 module is acceptable, but a custom board should use a higher-quality IC.

### 3.2. Battery Fuel Gauge

Simply measuring battery voltage is an inaccurate way to determine the state of charge. A dedicated fuel gauge IC is recommended.

*   **Selected Component:** **MAX17043**
*   **Alternatives:** LC709203F.
*   **Rationale:**
    *   **Accuracy:** The MAX17043 uses a sophisticated algorithm (`ModelGauge`) to track the battery's state of charge more accurately than simple voltage readings.
    *   **Simplicity:** It communicates over I2C and does not require a large, power-hungry sense resistor like some other fuel gauge methods.
    *   **Library Support:** It is well-supported by libraries from vendors like SparkFun and Adafruit, making firmware integration straightforward.

## 4. User Interface Components

These components provide at-a-glance status and basic interaction with the device without requiring the companion app.

### 4.1. Display

*   **Selected Component:** **0.96" I2C OLED Display (SSD1306)**
*   **Alternatives:** 1.3" I2C OLED (SH1106), various small TFT displays.
*   **Rationale:**
    *   **Low Power & High Contrast:** OLED technology is ideal for battery-powered devices, offering true blacks and excellent readability.
    *   **Simplicity:** The I2C interface requires only two GPIO pins (SDA/SCL), which can be shared with other I2C devices.
    *   **Excellent Support:** The SSD1306 driver is extremely common in the Arduino/ESP32 community, with robust libraries like `Adafruit_SSD1306` and `U8g2`.
    *   **Size:** The 0.96" size is a perfect fit for a "pocket" device.

### 4.2. Status LEDs

*   **Selected Component:** Standard 3mm or 5mm through-hole LEDs for prototyping; 0805 size for PCB.
*   **Rationale:** Simple, direct visual feedback is invaluable.
    *   **Power / Status (Green):** Indicates the device is on and operating normally.
    *   **RX Activity (Blue):** Flashes when a valid APRS packet is received.
    *   **TX Activity (Red):** Lights up when the device is transmitting.

### 4.3. Buttons

*   **Selected Component:** Momentary tactile push-buttons.
*   **Rationale:**
    *   **Pairing / Function Button:** A user-accessible button to initiate BLE pairing or to trigger a custom function (e.g., "Send Beacon Now").
    *   **Power Switch:** A simple slide switch is the most straightforward way to control power in a prototype. This is not a "soft" power button but a direct hardware switch between the battery and the regulator.

## 5. Miscellaneous Hardware

### 5.1. Antenna Connector

*   **Selected Component:** **SMA Female Connector (Panel Mount)**
*   **Alternatives:** u.FL connector.
*   **Rationale:**
    *   **Robustness:** For a portable device with a detachable antenna, an SMA connector is far more durable than a tiny u.FL connector, which is rated for very few connection cycles.
    *   **Compatibility:** SMA is a standard connector for many commercially available 2-meter VHF antennas.

### 5.2. Vibration Motor

*   **Selected Component:** Pancake-style vibration motor.
*   **Rationale:** Provides a silent, haptic notification for events like receiving a direct message, which is more discreet than a buzzer. Requires a simple transistor driver circuit to be controlled from a GPIO pin.
