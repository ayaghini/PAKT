# MVP scope

## Must-have (MVP)
### Radio/APRS
- TX: APRS position beacon (configurable interval)
- RX: APRS packet decode and forward to phone
- TX: APRS message send with basic retry logic
- Basic framing:
  - AX.25 UI frames
  - APRS payloads (position, message)
- PTT control and channel config via SA818 UART

### GPS
- Parse NMEA, maintain last fix + speed/course + UTC time
- Expose GPS data to phone over BLE
- Fail-safe when GPS unavailable (for example stale-fix indicator)

### BLE
- Provisioning:
  - Callsign/SSID
  - TX frequency preset (region default)
  - Beacon interval
  - Symbol, comment/status text
- Data:
  - RX packet stream
  - TX request queue + TX status
  - Device telemetry (battery %, temp if available, RSSI proxy if available)
- Security baseline:
  - Encrypted connection for all writes
  - Bonded link required for config/command/TX endpoints
  - Physical user action required to enter pairing mode

### Power
- Li-ion/LiPo battery with USB-C charging via MCP73831/2 class charger
- Battery voltage/percentage measurement via MAX17048 fuel gauge
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
- Firmware update over BLE/Wi-Fi
- Digipeater / iGate mode (requires more policy + operational considerations)

## Won't-have (initial)
- Multi-band
- DMR/other digital voice
- High power RF amplifier

> ### Feedback
>
> This is a well-defined and realistic MVP scope. The clear separation between `Must-have` and `Should-have` is essential for staying on track.
>
> A few thoughts on specific items:
>
> *   **APRS Message Retry Logic:** It would be beneficial to define what "basic retry logic" entails. For example, is the device responsible for waiting for a specific APRS-level ACK packet (`ack` message type), or will it be a simpler time-based re-transmission? Defining this helps scope the firmware complexity.
> *   **GPS Fail-safe:** This is critical. In addition to a "stale-fix indicator," consider explicitly exposing the GPS fix status (e.g., No Fix, 2D, 3D) and the number of satellites in view via the BLE service. This gives the phone app richer contextual information to display to the user.
> *   **BLE Security:** The requirement for a physical user action to enter pairing mode is an excellent security measure that is often overlooked. This significantly hardens the device against unauthorized pairing attempts.
> *   **Firmware Updates:** While OTA updates are correctly scoped as `Should-have`, consider including a robust wired update method (e.g., via the USB-C port) as a `Must-have` for development. This provides a crucial recovery path if a bad firmware flash bricks the BLE stack, which can be a lifesaver.
> *   **Phone App UX:** For the map's "decoded stations list," consider adding a `Should-have` feature for time-based filtering (e.g., "show stations heard in the last 30 minutes"). In dense APRS areas, the map can become cluttered quickly, and this helps maintain usability.
