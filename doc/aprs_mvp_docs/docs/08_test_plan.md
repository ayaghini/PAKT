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
- Pairing stability (Android, iOS)
- Config persistence after reboot
- Packet stream reliability under load
- Reconnect behavior

## 4) Field tests
- Mobile beaconing on foot/vehicle
- Battery endurance in real usage
- Thermal performance in pocket/bag

## Acceptance criteria (MVP)
- 95%+ beacon packets decode cleanly on a nearby receiver in controlled conditions
- RX can decode local APRS traffic without frequent false frames
- Messages show clear send/ack feedback in app
- BLE connection stays stable for 1 hour continuous RX stream
