# MVP scope

## Must-have (MVP)
### Radio/APRS
- TX: APRS position beacon (configurable interval)
- RX: APRS packet decode + forward to phone
- TX: APRS message send with basic retry logic
- Basic framing:
  - AX.25 UI frames
  - APRS payloads (position, message)
- PTT control and channel config via SA818 UART

### GPS
- Parse NMEA, maintain last fix + speed/course + UTC time
- Expose GPS data to phone over BLE
- Fail-safe when GPS unavailable (e.g., stale fix indicator)

### BLE
- Provisioning:
  - Callsign/SSID
  - TX frequency preset (region default)
  - Beacon interval
  - Symbol, comment/status text
- Data:
  - RX packets stream
  - TX request queue + TX status
  - Device telemetry (battery %, temp if available, RSSI proxy if available)

### Power
- Li-ion/LiPo battery with USB-C charging
- Battery voltage/percentage measurement
- Low power mode when idle

### UX (phone app)
- Connect/disconnect to device
- Configuration screen (callsign, interval, symbol/comment)
- Map view with:
  - current device position
  - decoded stations list
- Messaging view:
  - send message to callsign
  - show ack/timeouts

## Should-have (post-MVP)
- Smart beaconing
- KISS TNC over BLE so third-party apps can use the device
- Store-and-forward mailbox mode
- Firmware update over BLE/Wi‑Fi
- Digipeater / iGate mode (requires more policy + operational considerations)

## Won’t-have (initial)
- Multi-band
- DMR/other digital voice
- High power RF amplifier
