# PAKT BLE Payload Contracts

Generated: 2026-03-15
Authority: single source of truth for all JSON wire formats exchanged over BLE.

This document defines the exact JSON schema for every JSON GATT characteristic read,
write, and notification payload. Firmware (`PayloadValidator`, `TxResultEncoder`,
`Telemetry.h`) and the desktop app (`pakt_client.py`, `message_tracker.py`)
must both conform to these schemas.

KISS-over-BLE is part of MVP, but it is a binary framing contract rather than a
JSON contract. The KISS source of truth is:

- `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`

---

## 1. Device Config (0xA001) — read/write, encrypted+bonded

### 1.1 Write (BLE → firmware)

```json
{
  "callsign": "W1AW",
  "ssid": 0
}
```

| Field       | Type   | Required | Constraints                          |
|-------------|--------|----------|--------------------------------------|
| `callsign`  | string | Yes      | 1–6 chars from `[A-Za-z0-9-]`        |
| `ssid`      | int    | No       | 0–15; defaults to 0 if absent        |

Validation is performed by `PayloadValidator::validate_config_payload()` in firmware.
Invalid writes return a BLE application error (write rejected).

### 1.2 Read (firmware → BLE)

Same schema as write. Firmware returns the currently stored callsign and SSID.
If no config has been written, returns `{"callsign":"","ssid":0}` as placeholder.

---

## 2. TX Request (0xA011) - write only, encrypted+bonded

Payloads larger than one BLE MTU are chunked using the BleChunker protocol
(3-byte header: `msg_id | chunk_idx | chunk_total`). The firmware reassembles
before passing the complete JSON to `PayloadValidator`.

```json
{
  "dest": "APRS",
  "text": "Hello, this is a beacon message",
  "ssid": 0
}
```

| Field  | Type   | Required | Constraints                            |
|--------|--------|----------|----------------------------------------|
| `dest` | string | Yes      | 1–6 chars from `[A-Za-z0-9-]`          |
| `text` | string | Yes      | 1–67 printable chars (APRS body limit) |
| `ssid` | int    | No       | 0–15; defaults to 0 if absent          |

Validation is performed by `PayloadValidator::validate_tx_request_payload()`.
Invalid writes are rejected; the firmware does not enqueue the message.

---

## 3. TX Result (0xA012) - notify only (firmware -> BLE)

Sent by firmware at two points in the TX lifecycle:

1. **Intermediate**: immediately before each transmission attempt (status `"tx"`).
2. **Terminal**: when the message reaches a final state.

```json
{
  "msg_id": "42",
  "status": "acked"
}
```

| Field    | Type   | Description                                          |
|----------|--------|------------------------------------------------------|
| `msg_id` | string | Numeric message ID assigned by TxScheduler (1–99999) |
| `status` | string | One of: `tx`, `acked`, `timeout`, `cancelled`, `error` |

### Status values

| Value       | Meaning                                                         |
|-------------|-----------------------------------------------------------------|
| `tx`        | Transmission attempt fired (intermediate; not terminal)          |
| `acked`     | Remote station sent a matching APRS ack (terminal)               |
| `timeout`   | Max retries (5) exhausted with no ack (terminal)                 |
| `cancelled` | Message was cancelled by the firmware (terminal)                 |
| `error`     | Transmit function failed (radio unavailable at time of TX) (terminal) |

`msg_id` in the notify matches the ID that will be assigned by the firmware;
the desktop app should correlate using the `_remap_placeholder()` mechanism in
`MessageTracker` until a firmware-assigned ID is confirmed on first `"tx"` event.

Encoding is performed by `TxResultEncoder::encode()` in firmware.

---

## 4. Device Status (0xA003) - notify only

```json
{
  "radio": "idle",
  "bonded": true,
  "gps_fix": true,
  "pending_tx": 0,
  "rx_queue": 0,
  "uptime_s": 3600
}
```

| Field        | Type   | Description                                         |
|--------------|--------|-----------------------------------------------------|
| `radio`      | string | `"idle"`, `"tx"`, `"rx"`, `"error"`, `"unknown"`   |
| `bonded`     | bool   | True if a BLE bond is currently active              |
| `gps_fix`    | bool   | True if a current GPS fix is held by NmeaParser     |
| `pending_tx` | int    | Number of non-terminal messages in TxScheduler      |
| `rx_queue`   | int    | Number of decoded RX packets waiting to be read     |
| `uptime_s`   | int    | Seconds since firmware boot                         |

*Not yet implemented in firmware; schema reserved for Step 7.*
Desktop app `DeviceStatus.parse()` in `telemetry.py` uses these exact key names.

---

## 5. RX Packet (0xA010) - notify only

```json
{
  "from": "W1AW-9",
  "to": "APRS",
  "path": "WIDE1-1",
  "info": ">PAKT v0.1"
}
```

| Field  | Type   | Description                        |
|--------|--------|------------------------------------|
| `from` | string | Source callsign-SSID               |
| `to`   | string | Destination callsign               |
| `path` | string | Digipeater path string             |
| `info` | string | APRS information field (raw UTF-8) |

*Not yet implemented; schema reserved for Step 6 (RX pipeline).*

---

## 6. GPS Telemetry (0xA021) - notify only

```json
{
  "lat": 43.8130,
  "lon": -79.3943,
  "alt_m": 75.0,
  "speed_kmh": 11.1,
  "course": 54.7,
  "sats": 8,
  "fix": 1,
  "ts": 764426119
}
```

| Field       | Type  | Description                                           |
|-------------|-------|-------------------------------------------------------|
| `lat`       | float | Latitude in decimal degrees; negative = South         |
| `lon`       | float | Longitude in decimal degrees; negative = West         |
| `alt_m`     | float | Altitude in metres (MSL)                              |
| `speed_kmh` | float | Speed over ground in km/h                             |
| `course`    | float | Course over ground in degrees true (0–359.9)          |
| `sats`      | int   | Number of satellites used                             |
| `fix`       | int   | Fix quality: 0 = no fix, 1 = GPS, 2 = DGPS           |
| `ts`        | int   | Unix timestamp (seconds since 1970-01-01 UTC); 0 = unknown |

**Note:** the JSON key is `course` (not `course_deg`). The firmware struct field is
`GpsTelem::course_deg` but `GpsTelem::to_json()` serialises it as `"course"`.
Desktop app `GpsTelem.parse()` reads `d.get("course")`.

Produced by `GpsTelem::to_json()` in `Telemetry.cpp`.
Source data: `NmeaParser` (GPRMC + GPGGA).

---

## 7. Power Telemetry (0xA022) - notify only

```json
{
  "batt_v": 3.95,
  "batt_pct": 72,
  "tx_dbm": 30.0,
  "vswr": 1.3,
  "temp_c": 34.5
}
```

Desktop app `PowerTelem.parse()` in `telemetry.py` consumes this schema.

---

## 8. Device Capabilities (0xA004) - read only, no security restriction

```json
{"fw_ver":"0.1.0","hw_rev":"EVT-A","protocol":1,"features":["aprs_2m","ble_chunking","telemetry","msg_ack","config_rw","gps_onboard","kiss_ble"]}
```

Produced by `DeviceCapabilities::to_json()`.
The field set is defined by `DeviceCapabilities::mvp_defaults()`.

---

## 9. System Telemetry (0xA023) - notify only

```json
{
  "free_heap": 145000,
  "min_heap": 112000,
  "cpu_pct": 17,
  "tx_pkts": 42,
  "rx_pkts": 11,
  "tx_errs": 0,
  "rx_errs": 1,
  "uptime_s": 1800
}
```

Desktop app `SysTelem.parse()` in `telemetry.py` consumes this schema.

---

## Encoding rules (all payloads)

- Encoding: UTF-8, no BOM.
- Numeric JSON values: no surrounding quotes.
- Boolean JSON values: lowercase `true`/`false`.
- Floating-point: use sufficient precision (≥ 4 decimal places for lat/lon).
- No trailing commas.
- Extra unknown fields in write payloads are silently ignored by the validator.

