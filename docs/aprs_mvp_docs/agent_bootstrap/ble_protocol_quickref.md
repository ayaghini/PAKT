# BLE Protocol Quick Reference For Agents

Purpose: give an agentic coding LLM the minimum BLE contract needed to work safely
without loading the full integration reference first.

Companion deep reference:
- [BLE API / Protocol Reference](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/docs/19_ble_integration_reference.md)

Use this file when:
- touching BLE services or characteristics
- updating desktop/iPhone clients
- adding commands, status fields, or telemetry
- validating protocol compatibility across firmware and apps

## What Must Stay Stable

Protocol revision:
- current native protocol revision: `1`

Do not break without an intentional protocol revision change:
- custom UUIDs
- JSON field names
- TX result status semantics
- native + KISS coexistence
- chunk header format: `msg_id | chunk_idx | chunk_total`

## BLE Surface Summary

Base UUID:
- `544E4332-8A48-4328-9844-3F5C00000000`

Custom services:
- APRS Service `0xA000`
- Telemetry Service `0xA020`
- KISS Service `0xA050`

Standard service:
- Device Information `0x180A`

## Characteristics You Need To Know

### APRS Service

| Name | ID | Type | Notes |
|---|---|---|---|
| Device Config | `0xA001` | Read/Write JSON | callsign + ssid |
| Device Command | `0xA002` | Write WNR JSON | `debug_stream`, `radio_set`, `beacon_now` |
| Device Status | `0xA003` | Notify JSON | dashboard/runtime state |
| Device Capabilities | `0xA004` | Read JSON | feature negotiation |
| RX Packet Stream | `0xA010` | Notify text | TNC2 monitor text, not JSON |
| TX Request | `0xA011` | Write JSON | native APRS send path |
| TX Result | `0xA012` | Notify JSON | `tx/acked/timeout/cancelled/error` |

### Telemetry Service

| Name | ID | Type | Notes |
|---|---|---|---|
| GPS Telemetry | `0xA021` | Notify JSON | `fix=0` is valid/alive |
| Power Telemetry | `0xA022` | Notify JSON | runtime power info |
| System Telemetry | `0xA023` | Notify JSON | counters/heap/uptime |
| Debug Stream | `0xA024` | Notify text | operator-facing debug, off by default |

### KISS Service

| Name | ID | Type | Notes |
|---|---|---|---|
| KISS RX | `0xA051` | Notify binary | chunked when needed |
| KISS TX | `0xA052` | Write w/ response binary | encrypted/bonded |

## Security Rules

Write-capable endpoints should be treated as protected:
- Device Config
- Device Command
- TX Request
- KISS TX

Current project intent:
- encrypted + bonded for production writes

When changing app/client behavior:
- preserve clear auth-failure handling
- do not silently downgrade protected writes

## Payload Rules

Canonical sources:
- JSON payloads: [payload_contracts.md](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/payload_contracts.md)
- KISS framing: [16_kiss_over_ble_spec.md](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md)
- full cross-platform view: [BLE API / Protocol Reference](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/docs/19_ble_integration_reference.md)

Important payload distinctions:
- `RX Packet Stream` = UTF-8 TNC2 text, not JSON
- `Debug Stream` = UTF-8 text lines, not JSON
- `KISS RX/TX` = binary KISS, not JSON

## Command Surface

Current `Device Command` families:

1. Debug stream

```json
{"cmd":"debug_stream","enabled":true}
```

2. Radio control

```json
{"cmd":"radio_set","rx_freq_hz":144390000,"tx_freq_hz":144390000,"squelch":1,"volume":4,"wide_band":true}
```

3. Beacon now

```json
{"cmd":"beacon_now"}
```

If you add or change command semantics:
- update firmware
- update desktop client
- update iPhone client
- update `payload_contracts.md`
- update [BLE API / Protocol Reference](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/docs/19_ble_integration_reference.md)

## Client Flow Agents Should Preserve

Expected client session:
1. connect
2. discover services
3. read `Device Capabilities`
4. read `Device Config`
5. subscribe `Device Status`, `RX Packet Stream`, `TX Result`, `GPS Telemetry`, `System Telemetry`
6. subscribe `Debug Stream` only when explicitly enabled
7. use `TX Request` for native APRS send
8. use `KISS TX/RX` only if `kiss_ble` capability exists

## Agent Checklist Before Merging BLE Changes

- UUIDs unchanged unless intentionally versioning protocol
- payload field names unchanged unless intentionally versioning protocol
- desktop client still aligned
- iPhone client still aligned
- `05_ble_gatt_spec.md` still aligned
- `payload_contracts.md` still aligned
- [BLE API / Protocol Reference](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/docs/19_ble_integration_reference.md) updated if behavior changed

## When To Open The Full Reference

Open the full doc immediately if you need:
- exact UUID strings
- exact field lists/examples
- chunking details
- sequence flow
- third-party conformance expectations

Deep reference:
- [BLE API / Protocol Reference](/Users/macmini4/Desktop/PAKT/docs/aprs_mvp_docs/docs/19_ble_integration_reference.md)
