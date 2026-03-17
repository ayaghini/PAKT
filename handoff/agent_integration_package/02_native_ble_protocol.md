# Native BLE Protocol And Interfaces

This is the current integration contract for third-party software.

## Protocol status

- `Native PAKT BLE`: current and intended for integration now
- `KISS-over-BLE`: MVP-required interoperability surface, specified but not yet implemented

## BLE naming and discovery

Expected advertised device name prefix:

- `PAKT`

Current firmware/device name used by the repo:

- `PAKT-TNC`

## Custom UUID base

Base UUID template:

- `544E4332-8A48-4328-9844-3F5C00000000`

Rule:

- insert the 16-bit value into the `0000` segment before the final `0000`

Example:

- `0xA001` -> `544E4332-8A48-4328-9844-3F5CA0010000`

## Services and characteristics

### Standard Device Information Service

- Service: `0x180A`
- Manufacturer Name: `0x2A29`
- Model Number: `0x2A24`
- Firmware Revision: `0x2A26`

### APRS Service

- Service UUID: `544E4332-8A48-4328-9844-3F5CA0000000`

Characteristics:

- Device Config
  - UUID: `544E4332-8A48-4328-9844-3F5CA0010000`
  - Properties: Read, Write
  - Security: encrypted + bonded required for write
- Device Command
  - UUID: `544E4332-8A48-4328-9844-3F5CA0020000`
  - Properties: Write Without Response
  - Security: encrypted + bonded required for write
  - Current implementation note: handler is present, but command behavior is
    still effectively stubbed in firmware
- Device Status
  - UUID: `544E4332-8A48-4328-9844-3F5CA0030000`
  - Properties: Notify
  - Current implementation note: schema exists, but live status production is
    not complete in firmware
- Device Capabilities
  - UUID: `544E4332-8A48-4328-9844-3F5CA0040000`
  - Properties: Read
  - Read this immediately after connect
- RX Packet Stream
  - UUID: `544E4332-8A48-4328-9844-3F5CA0100000`
  - Properties: Notify
  - Current implementation note: reserved in contract; full live RF RX path is
    hardware-gated
- TX Request
  - UUID: `544E4332-8A48-4328-9844-3F5CA0110000`
  - Properties: Write
  - Security: encrypted + bonded required for write
- TX Result
  - UUID: `544E4332-8A48-4328-9844-3F5CA0120000`
  - Properties: Notify

### Telemetry Service

- Service UUID: `544E4332-8A48-4328-9844-3F5CA0200000`

Characteristics:

- GPS Telemetry
  - UUID: `544E4332-8A48-4328-9844-3F5CA0210000`
  - Properties: Notify
- Power Telemetry
  - UUID: `544E4332-8A48-4328-9844-3F5CA0220000`
  - Properties: Notify
- System Telemetry
  - UUID: `544E4332-8A48-4328-9844-3F5CA0230000`
  - Properties: Notify

## Canonical payloads

Source of truth:

- `doc/aprs_mvp_docs/payload_contracts.md`

### Device Config

Write/read JSON:

```json
{"callsign":"W1AW","ssid":0}
```

Rules:

- `callsign`: 1-6 chars from `[A-Za-z0-9-]`
- `ssid`: optional, `0-15`

### TX Request

Write JSON:

```json
{"dest":"APRS","text":"Hello, this is a beacon message","ssid":0}
```

Rules:

- `dest`: 1-6 chars from `[A-Za-z0-9-]`
- `text`: 1-67 printable chars
- `ssid`: optional, `0-15`

### TX Result

Notify JSON:

```json
{"msg_id":"42","status":"acked"}
```

Known statuses:

- `tx`
- `acked`
- `timeout`
- `cancelled`
- `error`

### Device Status

Reserved/current schema:

```json
{"radio":"idle","bonded":true,"gps_fix":true,"pending_tx":0,"rx_queue":0,"uptime_s":3600}
```

### RX Packet

Reserved/current schema:

```json
{"from":"W1AW-9","to":"APRS","path":"WIDE1-1","info":">PAKT v0.1"}
```

### GPS Telemetry

```json
{"lat":43.8130,"lon":-79.3943,"alt_m":75.0,"speed_kmh":11.1,"course":54.7,"sats":8,"fix":1,"ts":764426119}
```

Important:

- the JSON key is `course`, not `course_deg`

### Power Telemetry

```json
{"batt_v":3.95,"batt_pct":72,"tx_dbm":30.0,"vswr":1.3,"temp_c":34.5}
```

### System Telemetry

```json
{"free_heap":145000,"min_heap":112000,"cpu_pct":17,"tx_pkts":42,"rx_pkts":11,"tx_errs":0,"rx_errs":1,"uptime_s":1800}
```

### Device Capabilities

Actual implemented schema:

```json
{"fw_ver":"0.1.0","hw_rev":"EVT-A","protocol":1,"features":["aprs_2m","ble_chunking","telemetry","msg_ack","config_rw","gps_onboard"]}
```

Important:

- this implemented schema is the one used by firmware and desktop app
- an older example in `05_ble_gatt_spec.md` shows a different shape
- for integration, use the implemented schema above

Feature names currently defined in code:

- `aprs_2m`
- `ble_chunking`
- `telemetry`
- `msg_ack`
- `config_rw`
- `gps_onboard`
- `hf_audio`

## Chunking protocol

Use chunking whenever payload length may exceed `negotiated_mtu - 3`.

Chunk header:

- byte 0: `msg_id`
- byte 1: `chunk_idx`
- byte 2: `chunk_total`

Effective chunk payload size:

- `mtu - 6`

Reason:

- ATT header uses 3 bytes
- chunk header uses 3 bytes

Requirements for the client:

- reassemble chunks by `msg_id`
- handle duplicates safely
- handle timeout/abandoned partial messages
- work at default MTU as well as larger negotiated MTU

## Security rules

Required for write-capable control endpoints:

- encrypted link
- bonded link

Applies to:

- Device Config writes
- Device Command writes
- TX Request writes

Practical client behavior:

- expect auth/encryption write failures before pairing
- pair through the OS BLE flow
- reconnect and retry after bonding if needed

## KISS-over-BLE

Current status:

- documented in `doc/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`
- now part of MVP scope
- not yet wired into current firmware/app
- should be treated as an implementation gap, not a future nice-to-have
