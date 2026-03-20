# KISS-over-BLE Profile — MVP Specification (INT-003)

Status: **MVP REQUIRED** — specification frozen for implementation.
Version: 0.2
Date: 2026-03-16

---

## 1. Purpose

This document defines the KISS-over-BLE profile: a transport-layer mapping of
the KISS TNC protocol [TAPR KISS] onto the PAKT BLE GATT service. The goal is
to enable existing APRS software (for example APRSdroid, YAAC, and Xastir) to
communicate with the PAKT TNC in MVP, either directly or through a thin bridge
utility where needed.

KISS is now an MVP requirement because third-party APRS software compatibility
is a core project goal. Native PAKT BLE remains the richer control/telemetry
protocol, but KISS-over-BLE must coexist with it in the same MVP release.

---

## 2. Background: KISS Protocol

KISS (Keep It Simple Stupid) is a simple framing protocol defined for TNC
communication over serial (RS-232/USB). Relevant properties:

- Frames delimited by `0xC0` (FEND) bytes
- Frame type byte: `0x00` = data frame, `0x0F` = return-from-KISS
- Special byte escaping: `0xC0 -> 0xDB 0xDC`, `0xDB -> 0xDB 0xDD`
- No ACK, no retransmit — KISS is a raw byte pipe

The PAKT BLE GATT service already provides structured message semantics
(chunking, ack, retry). The KISS profile adds a raw-frame pipe alongside the
structured service for compatibility with existing tools.

---

## 3. KISS-over-BLE Design

### 3.1 Transport approach

Rather than repurposing the existing APRS service characteristics, the KISS
profile adds a new **KISS Service** with two characteristics:

| Characteristic | UUID | Properties | Direction | Description |
|---|---|---|---|---|
| KISS RX | `544E4332-8A48-4328-9844-3F5CA0510000` | Notify | Device -> App | KISS frames decoded from RF, delivered to client |
| KISS TX | `544E4332-8A48-4328-9844-3F5CA0520000` | Write (with response) | App -> Device | KISS frames to transmit over RF |

### 3.2 Frame transport

KISS frames MAY exceed a single BLE ATT payload. The INT-002 chunking protocol
(`[msg_id:1][chunk_idx:1][chunk_total:1][payload...]`) is used for both KISS RX
and KISS TX characteristics, identical to the existing APRS service chunking.

- `mtu - 6` bytes of KISS frame data per chunk
- MVP maximum logical KISS frame size after reassembly: `330` bytes
- Oversize frames are dropped and counted as protocol errors

### 3.3 KISS service UUIDs

```
KISS Service:  544E4332-8A48-4328-9844-3F5CA0500000
KISS RX:       544E4332-8A48-4328-9844-3F5CA0510000  (notify)
KISS TX:       544E4332-8A48-4328-9844-3F5CA0520000  (write w/ response)
```

### 3.4 KISS frame handling

**TX path (App -> Device -> RF):**
1. App sends chunked KISS frame to KISS TX characteristic.
2. Firmware reassembles the frame via BleChunker.
3. Firmware strips FEND delimiters, un-escapes, and parses the KISS command byte.
4. Only port `0` data frames (`0x00`) are forwarded as AX.25 frames to the
   existing TX scheduler (`TxScheduler`).
5. Firmware does NOT send KISS-level ACK for data frames; KISS remains a raw pipe.
6. Invalid or oversize KISS frames are dropped and counted in error telemetry.

**RX path (RF -> Device -> App):**
1. Firmware decodes APRS/AX.25 frame from modem.
2. Firmware wraps the AX.25 frame in a KISS data frame (FEND `0x00` <data> FEND).
3. Firmware sends chunked KISS frame via KISS RX notify.
4. App reassembles and delivers to KISS-speaking software.

### 3.5 Return-from-KISS (`0x0F`)

If the app sends a frame with type byte `0x0F`, the device may echo a one-shot
`0x0F` compatibility response, but it does **not** switch modes. The device is
non-modal in MVP: native PAKT BLE and KISS-over-BLE may both remain active.

### 3.6 Multi-client arbitration

KISS mode and native PAKT mode operate simultaneously. The device sends both
KISS RX notifies and native `rx_packet` notifies for every decoded frame. TX
requests from the native `tx_request` characteristic and KISS TX characteristic
are merged into the same `TxScheduler` queue. KISS TX frames do not receive
`tx_result` notifies; KISS callers must implement their own retry logic if
needed.

---

## 4. Capability negotiation

Before using the KISS profile, the client MUST check the Device Capabilities
characteristic:

```python
caps = await client.read_capabilities()
if not caps.supports("kiss_ble"):
    # Fall back to native PAKT protocol
```

Firmware that supports the KISS profile will advertise `"kiss_ble"` in the
features list. Firmware without the KISS profile will not have the KISS Service
in its GATT table; attempting to subscribe to KISS RX will raise a
characteristic-not-found error.

---

## 5. MVP implementation scope

The MVP implementation consists of:

1. **Firmware**: `firmware/components/kiss/` — KissFramer (encode/decode), KISS
   GATT service added to BleServer, TX path integration into TxScheduler.
2. **Desktop bridge app**: `app/desktop_test/kiss_bridge.py` — accepts serial
   KISS connection (virtual COM or named pipe) and bridges to BLE KISS
   characteristics.
3. **Test vectors**: known KISS-encoded AX.25 frames with expected BLE chunk
   sequences.

---

## 6. MVP acceptance

The KISS profile is MVP-complete when all of the following are true:

1. KISS Service is present in the GATT table and capability record advertises `kiss_ble`.
2. A chunked KISS TX data frame reaches the shared TX scheduler and produces RF
   output through the normal APRS path.
3. A decoded RF frame appears on both native `rx_packet` and KISS RX.
4. Oversize or malformed KISS frames are dropped safely with no crash or stuck-TX behavior.
5. At least one third-party KISS client or bridge exchanges frames successfully
   with the device.

---

## 7. Protocol decisions frozen for MVP

1. KISS TX requires the same encrypted + bonded write policy as native `tx_request`.
2. Only port `0` is supported.
3. KISS extended commands (`0x01`-`0x06`) are ignored or treated as no-op in MVP.
4. Device operation is non-modal; native PAKT BLE and KISS-over-BLE may coexist.
5. The MVP logical frame size limit is 330 bytes after reassembly.

---

## 8. References

- TAPR KISS TNC specification: https://www.ax25.net/kiss.aspx (external)
- PAKT BLE GATT specification: `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- INT-002 chunking specification: `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md` (chunk framing section)
