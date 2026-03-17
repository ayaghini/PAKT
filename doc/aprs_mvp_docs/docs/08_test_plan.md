# Test plan (MVP)

## 1) Bench tests
### Power
- Charge current, thermal behavior
- Battery voltage measurement accuracy
- Current draw in:
  - idle BLE connected
  - RX active
  - TX beacon every 60s
  - sleep mode

### SA818 control
- UART command success rates
- Frequency set and stability
- PTT timing

### Audio path
- Verify codec sample rate accuracy
- Measure TX audio level at SA818 input
- Verify RX audio not clipping

## 2) RF functional tests
### TX APRS beacon
- Decode with known-good receiver + decoder
- Confirm symbol, comment, position format

### RX decode
- Receive packets from local digipeaters / test transmitter
- Verify decode rate at varying signal levels

### Messaging
- Send message to a station that will ACK
- Verify retry and timeout behavior

## 3) BLE + app tests
- Windows desktop app:
  - Connect/pair/reconnect stability
  - Config read/write parity with firmware schema
  - RX stream, TX request/result, telemetry decode validation
  - KISS RX/TX chunking, frame integrity, and coexistence with native BLE endpoints
  - Session log export and timestamp consistency
- Pairing stability (Android, iOS)
- Config persistence after reboot
- Packet stream reliability under load
- Reconnect behavior
- KISS client compatibility:
  - At least one reference KISS client or bridge can send a frame to PAKT
  - At least one reference KISS client or bridge receives a frame from PAKT
  - Oversize or malformed KISS frames are rejected safely without crashing native BLE flows

## 4) Field tests
- Mobile beaconing on foot/vehicle
- Battery endurance in real usage
- Thermal performance in pocket/bag
- Third-party KISS software interoperability in a realistic operator workflow

## Acceptance criteria (MVP)
- 95%+ beacon packets decode cleanly on a nearby receiver in controlled conditions
- RX can decode local APRS traffic without frequent false frames
- Messages show clear send/ack feedback in app
- BLE connection stays stable for 1 hour continuous RX stream
- KISS-over-BLE works with at least one reference third-party APRS client or compatibility bridge
