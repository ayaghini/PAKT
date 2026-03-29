# BLE GATT Specification

This document defines the Bluetooth Low Energy Generic Attribute Profile (GATT)
for the APRS Pocket TNC.

Client note:
- MVP development started with the desktop BLE test app.
- The repo now also carries an iPhone-only SwiftUI app track under `app/ios/`.
- Protocol compatibility between desktop and iPhone clients is a requirement: new app-facing features should extend the existing contract rather than fork it.

## Services Overview

The device exposes:

1. **Device Information Service (Standard):** `0x180A`
2. **APRS Service (Custom):** configuration, commands, messaging, and status
3. **Device Telemetry Service (Custom):** runtime telemetry channels
4. **KISS Service (Custom):** raw KISS TNC frame transport for third-party APRS software

## Project Base UUID

All custom services and characteristics use the shared 128-bit base UUID.

- **Base UUID template:** `544E4332-8A48-4328-9844-3F5C00000000`
- **Rule:** insert the 16-bit value into bytes 12-13 of the UUID (the `0000` before the last group)
- **Example:** `0xA001` -> `544E4332-8A48-4328-9844-3F5CA0010000`

All UUID strings below are shown fully expanded.

---

## 1. Device Information Service

Standard BLE service.

| Characteristic    | UUID (16-bit) | Properties | Value (example)        |
| ----------------- | ------------- | ---------- | ---------------------- |
| Manufacturer Name | `0x2A29`      | Read       | "PAKT"                 |
| Model Number      | `0x2A24`      | Read       | "APRS-TNC-1"           |
| Firmware Revision | `0x2A26`      | Read       | "0.1.0"                |
| Software Revision | `0x2A28`      | Read       | "0.1.0" (desktop app)  |

---

## 2. APRS Service

**Service UUID:** `544E4332-8A48-4328-9844-3F5CA0000000`

| Characteristic        | UUID (16-bit) | Full UUID                               | Properties             | Max App Payload | Data Format  | Notes / canonical payload |
| --------------------- | ------------- | --------------------------------------- | ---------------------- | --------------- | ------------ | -------------------------- |
| **Device Config**     | `0xA001`      | `544E4332-8A48-4328-9844-3F5CA0010000` | Read, Write            | 256 B           | UTF-8 JSON   | `{"callsign":"W1AW","ssid":9}` |
| **Device Command**    | `0xA002`      | `544E4332-8A48-4328-9844-3F5CA0020000` | Write Without Response | 64 B            | UTF-8 JSON   | `{"cmd":"beacon_now"}` or `{"cmd":"radio_set","freq_hz":144390000}` |
| **Device Status**     | `0xA003`      | `544E4332-8A48-4328-9844-3F5CA0030000` | Notify                 | 128 B           | UTF-8 JSON   | `{"radio":"idle","bonded":true,"gps_fix":true,"pending_tx":0,"rx_queue":0,"uptime_s":3600}` |
| **Device Capabilities** | `0xA004`    | `544E4332-8A48-4328-9844-3F5CA0040000` | Read                   | 160 B           | UTF-8 JSON   | `{"fw_ver":"0.1.0","hw_rev":"EVT-A","protocol":1,"features":["aprs_2m","ble_chunking","telemetry","msg_ack","config_rw","gps_onboard","kiss_ble"]}` |
| **RX Packet Stream**  | `0xA010`      | `544E4332-8A48-4328-9844-3F5CA0100000` | Notify                 | 256 B           | UTF-8 String | APRS packet in TNC2 monitor format |
| **TX Request**        | `0xA011`      | `544E4332-8A48-4328-9844-3F5CA0110000` | Write                  | 256 B           | UTF-8 JSON   | `{"dest":"APRS","text":"Hello World","ssid":0}` |
| **TX Result**         | `0xA012`      | `544E4332-8A48-4328-9844-3F5CA0120000` | Notify                 | 64 B            | UTF-8 JSON   | `{"msg_id":"42","status":"tx|acked|timeout|cancelled|error"}` |

---

## 3. Device Telemetry Service

**Service UUID:** `544E4332-8A48-4328-9844-3F5CA0200000`

| Characteristic       | UUID (16-bit) | Full UUID                               | Properties | Max App Payload | Data Format | Notes / canonical payload |
| -------------------- | ------------- | --------------------------------------- | ---------- | --------------- | ----------- | -------------------------- |
| **GPS Telemetry**    | `0xA021`      | `544E4332-8A48-4328-9844-3F5CA0210000` | Notify     | 128 B           | UTF-8 JSON  | `{"lat":49.035,"lon":-123.100,"alt_m":150.0,"speed_kmh":12.5,"course":90.0,"sats":8,"fix":1,"ts":764426119}` |
| **Power Telemetry**  | `0xA022`      | `544E4332-8A48-4328-9844-3F5CA0220000` | Notify     | 96 B            | UTF-8 JSON  | `{"batt_v":3.95,"batt_pct":88,"tx_dbm":30.0,"vswr":1.2,"temp_c":34.5}` |
| **System Telemetry** | `0xA023`      | `544E4332-8A48-4328-9844-3F5CA0230000` | Notify     | 96 B            | UTF-8 JSON  | `{"free_heap":120000,"min_heap":95000,"cpu_pct":22,"tx_pkts":5,"rx_pkts":3,"tx_errs":0,"rx_errs":0,"uptime_s":1800}` |
| **Debug Stream**     | `0xA024`      | `544E4332-8A48-4328-9844-3F5CA0240000` | Notify     | 180 B logical   | UTF-8 text  | Runtime debug lines, emitted only when enabled by `Device Command` |

Debug stream rules:

- Disabled by default.
- Enabled and disabled using `{"cmd":"debug_stream","enabled":true|false}` on `Device Command`.
- Intended for operator-facing runtime diagnostics in the desktop and iPhone apps.
- Firmware should emit scoped, human-readable lines only for selected categories; it must not blindly mirror all ESP logs onto BLE.

---

## 4. KISS Service

**Service UUID:** `544E4332-8A48-4328-9844-3F5CA0500000`

| Characteristic | UUID (16-bit) | Full UUID                               | Properties           | Max App Payload | Data Format | Notes |
| -------------- | ------------- | --------------------------------------- | -------------------- | --------------- | ----------- | ----- |
| **KISS RX**    | `0xA051`      | `544E4332-8A48-4328-9844-3F5CA0510000` | Notify               | 330 B logical   | Binary KISS | Device -> client, chunked when needed |
| **KISS TX**    | `0xA052`      | `544E4332-8A48-4328-9844-3F5CA0520000` | Write With Response  | 330 B logical   | Binary KISS | Client -> device, encrypted + bonded |

KISS MVP rules:

- Port 0 only.
- Data frame type `0x00` required for actual AX.25 transfer.
- `0x0F` return-from-KISS may be accepted and echoed for compatibility, but the device is not modal; native PAKT BLE and KISS service may coexist.
- Extended KISS port/parameter commands (`0x01`-`0x06`) are out of scope for MVP and may be ignored safely.
- MVP maximum logical KISS frame size is 330 bytes after reassembly; oversize frames must be dropped and counted as TX/RX errors.

## Canonical Schema Source

For JSON field-level contracts, use:
- `docs/aprs_mvp_docs/payload_contracts.md`

If this file and payload contracts diverge, payload contracts are authoritative.

For KISS binary framing and service behavior, use:
- `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`

---

## Security Notes

- Pairing alone is not sufficient for write-capable control endpoints.
- Require LE Secure Connections with bonding for production firmware.
- Restrict write access (`Device Config`, `Device Command`, `TX Request`, `KISS TX`) to encrypted + bonded links.
- Require a physical user action (pair button or boot-time pairing window) before accepting new bonds.
- Enforce an application-level allowlist for privileged commands (for example `radio_set`).

## Device Command notes

The `Device Command` characteristic remains the single command channel for one-shot actions and radio/debug control.

Current command families:

1. `{"cmd":"beacon_now"}`
- immediate APRS position beacon attempt using current config and latest GPS snapshot

2. `{"cmd":"debug_stream","enabled":true|false}`
- toggles the dedicated debug notify stream on `0xA024`

3. `{"cmd":"radio_set", ...}`
- supported fields are optional and may be combined in one write:
  - `freq_hz`
  - `rx_freq_hz`
  - `tx_freq_hz`
  - `squelch`
  - `volume`
  - `wide_band`
- unsupported or malformed payloads are rejected

## Device Status notes

`Device Status` is now a live dashboard-oriented notify, not a reserved placeholder.

Current status payload includes at least:
- `radio`
- `bonded`
- `encrypted`
- `gps_fix`
- `pending_tx`
- `rx_queue`
- `rx_freq_hz`
- `tx_freq_hz`
- `squelch`
- `volume`
- `wide_band`
- `debug_enabled`
- `uptime_s`

## Transport and MTU Notes

- Values in "Max App Payload" are logical payload sizes, not guaranteed single-PDU BLE payloads.
- Implement explicit chunking for any payload that can exceed `(negotiated_mtu - 3)` bytes.
- Required protocol fields for chunked payloads:
  - `msg_id`: unique id for a logical message.
  - `chunk_idx`: zero-based chunk index.
  - `chunk_total`: number of chunks.
- RX and TX endpoints must support reassembly timeout and duplicate-chunk handling.
- Clients should request a higher MTU, but firmware must still work at default MTU.
- KISS RX and KISS TX use the same chunk header and timeout rules as the native JSON endpoints.
