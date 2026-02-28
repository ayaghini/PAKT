# BLE GATT Specification

This document defines the Bluetooth Low Energy Generic Attribute Profile (GATT) for the APRS Pocket TNC.

## Services Overview

The device will expose the following services:

1.  **Device Information Service (Standard):** `0x180A` - Provides standard manufacturer and device information.
2.  **APRS Service (Custom):** Provides the core functionality for configuration and data transfer.
3.  **Device Telemetry Service (Custom):** Provides real-time telemetry data from the device.

## Project Base UUID

All custom services and characteristics are defined using a shared 128-bit base UUID. The 16-bit UUIDs listed in the tables are substituted into the `xxxx` part of this base.

**Base UUID:** `544E-4332-8A48-4328-9844-3F5Cxxxx0000`

---

## 1. Device Information Service

This is a standard BLE service.

| Characteristic      | UUID (16-bit) | Properties | Value (Example)          |
| ------------------- | ------------- | ---------- | ------------------------ |
| Manufacturer Name   | `0x2A29`      | Read       | "PAKT"                   |
| Model Number        | `0x2A24`      | Read       | "APRS-TNC-1"             |
| Firmware Revision   | `0x2A26`      | Read       | "0.1.0"                  |
| Software Revision   | `0x2A28`      | Read       | "0.1.0" (Companion App)  |

---

## 2. APRS Service

**Service UUID:** `544E-4332-8A48-4328-9844-3F5C`**`A000`**`0000`

This service consolidates the configuration, command, and data transfer characteristics for the APRS functionality.

| Characteristic        | UUID (16-bit) | Properties                    | Max Size | Data Format         | Notes / MVP Payload                                                                                                       |
| --------------------- | ------------- | ----------------------------- | -------- | ------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **Device Config**     | `0xA001`      | Read, Write                   | 256 B    | UTF-8 JSON          | `{"callsign":"N0CALL", "ssid":7, "beacon_interval_s":300, "symbol_table":"/", "symbol_code":">", "comment":"Pocket TNC"}` |
| **Device Command**    | `0xA002`      | Write Without Response        | 64 B     | UTF-8 JSON          | `{"cmd":"beacon_now"}` or `{"cmd":"radio_set", "freq_hz":144390000}`                                                     |
| **Device Status**     | `0xA003`      | Notify                        | 128 B    | UTF-8 JSON          | `{"state":"idle", "gps_fix":1, "batt_pct":88, "tx_pending":0}` (Rate-limited stream)                                       |
| **RX Packet Stream**  | `0xA010`      | Notify                        | 256 B    | UTF-8 String        | A single, raw APRS packet in TNC2 monitor format, e.g., `N0CALL>APRS,WIDE1-1:!4903.50N/12310.00W>Comment`            |
| **TX Request**        | `0xA011`      | Write                         | 256 B    | UTF-8 JSON          | `{"to":"CQ", "text":"Hello World", "msg_id":"01"}`. `msg_id` is a client-generated ID to track responses.               |
| **TX Result**         | `0xA012`      | Notify                        | 64 B     | UTF-8 JSON          | `{"msg_id":"01", "status":"acked"}` or `{"msg_id":"02", "status":"error", "reason":"timeout"}`                        |

---

## 3. Device Telemetry Service

**Service UUID:** `544E-4332-8A48-4328-9844-3F5C`**`A020`**`0000`

This service provides detailed, real-time telemetry, intended for debugging or advanced diagnostics.

| Characteristic        | UUID (16-bit) | Properties | Max Size | Data Format         | Notes / MVP Payload                                                                |
| --------------------- | ------------- | ---------- | -------- | ------------------- | ---------------------------------------------------------------------------------- |
| **GPS Telemetry**     | `0xA021`      | Notify     | 128 B    | UTF-8 JSON          | `{"lat":49.035, "lon":-123.100, "alt_m":150.0, "speed_kmh":12.5, "course_deg":90}` |
| **Power Telemetry**   | `0xA022`      | Notify     | 64 B     | UTF-8 JSON          | `{"v":3.95, "pct":88, "charging":false}`                                           |
| **System Telemetry**  | `0xA023`      | Notify     | 64 B     | UTF-8 JSON          | `{"uptime_s":1800, "heap_free":50123}`                                             |

---

## Security Notes
- MVP: BLE pairing is sufficient. No bond will be required.
- A "press button to pair" mechanism should be considered for a production version to prevent unauthorized connections in public spaces.

