# PAKT Onboarding Brief

## What PAKT is

PAKT is a battery-powered APRS pocket TNC/tracker built around:

- `ESP32-S3`
- `SA818` VHF radio module
- `SGTL5000` audio codec
- GPS module over UART/NMEA
- BLE GATT interface for host software

The intended device behavior is:

- send APRS packets over RF
- receive APRS packets over RF
- expose config, commands, status, TX requests/results, and telemetry over BLE

## Integration goal

The external software already talks to other hardware and already provides APRS
functionality. The goal is to make that software accept PAKT hardware as a new
device/backend.

For the external agent, the practical target is:

1. connect to PAKT over BLE
2. read capabilities
3. read/write config
4. subscribe to notifications
5. send TX requests
6. consume TX results and telemetry
7. support KISS-over-BLE as part of MVP interoperability

## Recommended integration strategy

Use native PAKT BLE first for full control and telemetry, and treat KISS-over-BLE as the parallel MVP interoperability surface for existing APRS software.

Reason:

- it is the currently defined and implemented protocol path
- the desktop test app already uses it
- KISS-over-BLE is now part of MVP and the software path is implemented, but hardware validation and third-party interoperability evidence are still pending

If the external software architecture is KISS-centric, build a thin adapter
layer that maps:

- native PAKT `tx_request` to the software's outbound message model
- native PAKT `rx_packet` to the software's inbound APRS frame model
- native PAKT telemetry/status to the software's device health model

## Hardware model the agent should assume

Core internal interfaces:

- `ESP32-S3 UART <-> SA818 UART`
- `ESP32-S3 GPIO -> SA818 PTT`
- `ESP32-S3 I2S <-> SGTL5000`
- `ESP32-S3 UART <-> GPS`
- `ESP32-S3 I2C <-> MAX17048`

High-level data flow:

- Host app -> BLE -> ESP32-S3 -> APRS encode/TX pipeline -> SA818 -> RF
- RF -> SA818/audio path -> ESP32-S3 decode pipeline -> BLE -> Host app
- GPS -> ESP32-S3 -> telemetry/beaconing

## Current maturity snapshot

Software is ahead of hardware bring-up.

Implemented in repo:

- BLE GATT server and UUID layout
- payload validators and JSON serializers
- chunking/reassembly
- config storage
- TX scheduling and TX result handling
- capability negotiation
- desktop BLE test client
- host and app test suites in repo

Still hardware-gated or stubbed:

- real audio modem path on hardware
- real RF end-to-end APRS TX/RX validation
- GPS UART wiring in live firmware task
- battery/fuel-gauge live integration
- field validation and endurance validation

## Important integration truths

- Native BLE is the current integration contract.
- Write-capable endpoints require encrypted and bonded BLE links.
- Payload JSON contracts are authoritative in `payload_contracts.md`.
- Some docs are draft-level; the package below points out where implementation
  is ahead of or different from older examples.

## First tasks for the external agent

1. Implement BLE discovery/connect for device names beginning with `PAKT`.
2. Read Device Capabilities first and gate advanced behavior on it.
3. Implement native reads/writes/notifies for the characteristics in
   `02_native_ble_protocol.md`.
4. Reuse the chunking protocol for all payloads that may exceed MTU.
5. Plan for KISS support in parallel with native BLE because it is now part of MVP.
