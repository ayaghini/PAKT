# BLE GATT Specification

This document defines the Bluetooth Low Energy Generic Attribute Profile (GATT) for the APRS Pocket TNC.

## Services Overview

The device will expose the following services:

1. **Device Information Service (Standard):** `0x180A` - Provides standard manufacturer and device information.
2. **APRS Service (Custom):** Provides the core functionality for configuration and data transfer.
3. **Device Telemetry Service (Custom):** Provides real-time telemetry data from the device.

## Project Base UUID

All custom services and characteristics are defined using a shared 128-bit base UUID.
Use this canonical form:

- **Base UUID Template:** `544E4332-8A48-4328-9844-3F5C00000000`
- **Rule:** Insert the 16-bit value into bytes 12-13 of the UUID (the `0000` before the last group).
- **Example:** `0xA001` => `544E4332-8A48-4328-9844-3F5CA0010000`

All UUID strings in this document are shown fully expanded to avoid ambiguity.

---

## 1. Device Information Service

This is a standard BLE service.

| Characteristic    | UUID (16-bit) | Properties | Value (Example)         |
| ----------------- | ------------- | ---------- | ----------------------- |
| Manufacturer Name | `0x2A29`      | Read       | "PAKT"                  |
| Model Number      | `0x2A24`      | Read       | "APRS-TNC-1"            |
| Firmware Revision | `0x2A26`      | Read       | "0.1.0"                 |
| Software Revision | `0x2A28`      | Read       | "0.1.0" (Companion App) |

---

## 2. APRS Service

**Service UUID:** `544E4332-8A48-4328-9844-3F5CA0000000`

This service consolidates the configuration, command, and data transfer characteristics for the APRS functionality.

| Characteristic      | UUID (16-bit) | Full UUID                               | Properties             | Max App Payload | Data Format  | Notes / MVP Payload                                                                                                     |
| ------------------- | ------------- | --------------------------------------- | ---------------------- | --------------- | ------------ | ----------------------------------------------------------------------------------------------------------------------- |
| **Device Config**   | `0xA001`      | `544E4332-8A48-4328-9844-3F5CA0010000` | Read, Write            | 256 B           | UTF-8 JSON   | `{"callsign":"N0CALL","ssid":7,"beacon_interval_s":300,"symbol_table":"/","symbol_code":">","comment":"Pocket TNC"}` |
| **Device Command**  | `0xA002`      | `544E4332-8A48-4328-9844-3F5CA0020000` | Write Without Response | 64 B            | UTF-8 JSON   | `{"cmd":"beacon_now"}` or `{"cmd":"radio_set","freq_hz":144390000}`                                          |
| **Device Status**   | `0xA003`      | `544E4332-8A48-4328-9844-3F5CA0030000` | Notify                 | 128 B           | UTF-8 JSON   | `{"state":"idle","gps_fix":1,"batt_pct":88,"tx_pending":0}` (rate-limited)                                   |
| **RX Packet Stream**| `0xA010`      | `544E4332-8A48-4328-9844-3F5CA0100000` | Notify                 | 256 B           | UTF-8 String | Single APRS packet in TNC2 monitor format, e.g. `N0CALL>APRS,WIDE1-1:!4903.50N/12310.00W>Comment`                    |
| **TX Request**      | `0xA011`      | `544E4332-8A48-4328-9844-3F5CA0110000` | Write                  | 256 B           | UTF-8 JSON   | `{"to":"CQ","text":"Hello World","msg_id":"01"}`. `msg_id` is client-generated for tracking.               |
| **TX Result**       | `0xA012`      | `544E4332-8A48-4328-9844-3F5CA0120000` | Notify                 | 64 B            | UTF-8 JSON   | `{"msg_id":"01","status":"acked"}` or `{"msg_id":"02","status":"error","reason":"timeout"}`        |

---

## 3. Device Telemetry Service

**Service UUID:** `544E4332-8A48-4328-9844-3F5CA0200000`

This service provides detailed, real-time telemetry, intended for debugging or advanced diagnostics.

| Characteristic       | UUID (16-bit) | Full UUID                               | Properties | Max App Payload | Data Format | Notes / MVP Payload                                                             |
| -------------------- | ------------- | --------------------------------------- | ---------- | --------------- | ----------- | ------------------------------------------------------------------------------- |
| **GPS Telemetry**    | `0xA021`      | `544E4332-8A48-4328-9844-3F5CA0210000` | Notify     | 128 B           | UTF-8 JSON  | `{"lat":49.035,"lon":-123.100,"alt_m":150.0,"speed_kmh":12.5,"course_deg":90}` |
| **Power Telemetry**  | `0xA022`      | `544E4332-8A48-4328-9844-3F5CA0220000` | Notify     | 64 B            | UTF-8 JSON  | `{"v":3.95,"pct":88,"charging":false}`                                     |
| **System Telemetry** | `0xA023`      | `544E4332-8A48-4328-9844-3F5CA0230000` | Notify     | 64 B            | UTF-8 JSON  | `{"uptime_s":1800,"heap_free":50123}`                                        |

---

## Security Notes
- Pairing alone is not sufficient for write-capable control endpoints.
- Require LE Secure Connections with bonding for production firmware.
- Restrict write access (`Device Config`, `Device Command`, `TX Request`) to encrypted + bonded links.
- Require a physical user action (pair button or boot-time pairing window) before accepting new bonds.
- Enforce an application-level allowlist for privileged commands (for example `radio_set`).

## Transport and MTU Notes
- Values in "Max App Payload" are logical payload sizes, not guaranteed single-PDU BLE payloads.
- Implement explicit chunking for any payload that can exceed `(negotiated_mtu - 3)` bytes.
- Required protocol fields for chunked payloads:
  - `msg_id`: unique id for a logical message.
  - `chunk_idx`: zero-based chunk index.
  - `chunk_total`: number of chunks.
- RX and TX endpoints must support reassembly timeout and duplicate-chunk handling.
- Clients should request a higher MTU, but firmware must still work at default MTU.
