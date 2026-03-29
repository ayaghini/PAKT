# PAKT BLE Payload Contracts

Generated: 2026-03-15
Authority: single source of truth for all JSON wire formats exchanged over BLE.

This document defines the exact JSON schema for every JSON GATT characteristic read,
write, and notification payload. Firmware (`PayloadValidator`, `TxResultEncoder`,
`Telemetry.h`) and the client apps (`pakt_client.py`, `message_tracker.py`, iPhone BLE models)
must all conform to these schemas.

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

## 2a. Device Command (0xA002) - write only, encrypted+bonded

The command channel carries small one-shot JSON payloads. Each payload must include
`"cmd"` and may include additional fields depending on the command family.

### 2a.1 Debug stream control

```json
{
  "cmd": "debug_stream",
  "enabled": true
}
```

| Field     | Type   | Required | Constraints |
|-----------|--------|----------|-------------|
| `cmd`     | string | Yes      | Must be `"debug_stream"` |
| `enabled` | bool   | Yes      | `true` enables BLE debug notify; `false` disables it |

### 2a.2 Radio control

```json
{
  "cmd": "radio_set",
  "rx_freq_hz": 144390000,
  "tx_freq_hz": 144390000,
  "squelch": 2,
  "volume": 4,
  "wide_band": true
}
```

At least one supported radio field must be present.

| Field        | Type   | Required | Constraints |
|--------------|--------|----------|-------------|
| `cmd`        | string | Yes      | Must be `"radio_set"` |
| `freq_hz`    | int    | No       | Positive Hz value; sets both RX and TX |
| `rx_freq_hz` | int    | No       | Positive Hz value |
| `tx_freq_hz` | int    | No       | Positive Hz value |
| `squelch`    | int    | No       | `0–8` |
| `volume`     | int    | No       | `1–8` |
| `wide_band`  | bool   | No       | `true` wide, `false` narrow |

### 2a.3 One-shot beacon

```json
{
  "cmd": "beacon_now"
}
```

| Field   | Type   | Required | Constraints |
|---------|--------|----------|-------------|
| `cmd`   | string | Yes      | Must be `"beacon_now"` |

Malformed payloads or unsupported `cmd` values are rejected and must not change device state.

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
  "encrypted": true,
  "gps_fix": true,
  "pending_tx": 0,
  "rx_queue": 0,
  "rx_freq_hz": 144390000,
  "tx_freq_hz": 144390000,
  "squelch": 1,
  "volume": 4,
  "wide_band": true,
  "debug_enabled": false,
  "uptime_s": 3600
}
```

| Field        | Type   | Description                                         |
|--------------|--------|-----------------------------------------------------|
| `radio`      | string | `"idle"`, `"tx"`, `"rx"`, `"error"`, `"unknown"`   |
| `bonded`     | bool   | True if a BLE bond is currently active              |
| `encrypted`  | bool   | True if the current BLE link is encrypted           |
| `gps_fix`    | bool   | True if a current GPS fix is held by NmeaParser     |
| `pending_tx` | int    | Number of non-terminal messages in TxScheduler      |
| `rx_queue`   | int    | Number of decoded RX packets waiting to be read     |
| `rx_freq_hz` | int    | Current SA818 RX frequency in Hz                    |
| `tx_freq_hz` | int    | Current SA818 TX frequency in Hz                    |
| `squelch`    | int    | Current SA818 squelch setting                       |
| `volume`     | int    | Current SA818 volume setting                        |
| `wide_band`  | bool   | Current SA818 bandwidth mode                        |
| `debug_enabled` | bool | True if the BLE debug stream is currently enabled  |
| `uptime_s`   | int    | Seconds since firmware boot                         |

Desktop app `DeviceStatus.parse()` in `telemetry.py` uses these exact key names.

---

## 5. RX Packet (0xA010) - notify only

Canonical payload is a UTF-8 TNC2 monitor string, not JSON.

Example:

```text
W1AW-9>APRS,WIDE1-1:>PAKT v0.1
```

Clients should treat the whole payload as one line of human-readable APRS monitor text.

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

## 7a. Debug Stream (0xA024) - notify only

Debug stream payloads are UTF-8 text lines, not JSON.

Example:

```text
[radio] radio_set rx=144390000 tx=144390000 squelch=1 volume=4 wide=true
```

Rules:
- stream is disabled by default
- stream is enabled with `{"cmd":"debug_stream","enabled":true}`
- each notify payload is one operator-facing line
- clients should display it as append-only session log text

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
