# System architecture (high level)

## Major subsystems
1. RF subsystem
   - SA818 VHF module
   - Antenna, matching, filtering, shielding

2. Audio modem subsystem
   - I2S audio codec (ADC/DAC)
   - Analog interface to SA818 AF in/out
   - DSP / modem running on ESP32-S3:
     - 1200 baud AFSK demod (Bell 202)
     - AX.25 framing/CRC
     - APRS encode/decode helpers

3. Positioning subsystem
   - GPS module (UART NMEA, optional PPS)

4. Connectivity subsystem
   - BLE GATT services
   - Optional Wi‑Fi (later) for updates / gateway

5. Power subsystem
   - Li‑ion/LiPo charger
   - Battery gauge / ADC measurement
   - Regulators (3.3V digital, SA818 supply as required)
   - Power switching (RF off in sleep)

## Data flow
- GPS → ESP32 → APRS encoder → Audio DAC → SA818 → RF
- RF → SA818 audio → Codec ADC → ESP32 demod → packet stream → BLE → Phone
- Phone → BLE → ESP32 → APRS message encoder → TX
