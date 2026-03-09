# KISS-over-BLE Profile — Draft Specification (INT-003)

Status: **DRAFT** — not yet implemented. Target: post-MVP (M3).
Version: 0.1
Date: 2026-03-09

---

## 1. Purpose

This document defines the KISS-over-BLE profile: a transport-layer mapping of the KISS TNC protocol [TAPR KISS] onto the PAKT BLE GATT service. The goal is to enable existing APRS software (e.g. APRSdroid, YAAC, Xastir) that already speaks KISS to communicate with the PAKT TNC without modification, using a thin BLE bridge app on the phone or desktop.

This is a **post-MVP** feature. The MVP native PAKT protocol (Steps 0–10) takes priority. The KISS profile will be added only after the MVP BLE service is stable and field-validated.

---

## 2. Background: KISS Protocol

KISS (Keep It Simple Stupid) is a simple framing protocol defined for TNC communication over serial (RS-232/USB). Relevant properties:

- Frames delimited by `0xC0` (FEND) bytes
- Frame type byte: `0x00` = data frame, `0x0F` = return-from-KISS
- Special byte escaping: `0xC0 → 0xDB 0xDC`, `0xDB → 0xDB 0xDD`
- No ACK, no retransmit — KISS is a raw byte pipe

The PAKT BLE GATT service already provides structured message semantics (chunking, ack, retry). The KISS profile adds a raw-frame pipe alongside the structured service for compatibility with existing tools.

---

## 3. KISS-over-BLE Design

### 3.1 Transport approach

Rather than repurposing the existing APRS service characteristics, the KISS profile adds a new **KISS Service** with two characteristics:

| Characteristic | UUID | Properties | Direction | Description |
|---|---|---|---|---|
| KISS RX | `544E4332-8A48-4328-9844-3F5CA0500000` | Notify | Device → App | KISS frames decoded from RF, delivered to client |
| KISS TX | `544E4332-8A48-4328-9844-3F5CA0510000` | Write (with response) | App → Device | KISS frames to transmit over RF |

### 3.2 Frame transport

KISS frames MAY exceed a single BLE notify payload. The INT-002 chunking protocol (`[msg_id:1][chunk_idx:1][chunk_total:1][payload...]`) is used for both KISS RX and KISS TX characteristics, identical to the existing APRS service chunking.

- `mtu - 6` bytes of KISS frame data per chunk (ATT header 3 B + chunk header 3 B)
- Maximum KISS frame size: 64 chunks × (mtu - 6) bytes ≈ 15 KB at MTU 247

### 3.3 KISS service UUID

```
KISS Service:  544E4332-8A48-4328-9844-3F5CA0500000
KISS RX:       544E4332-8A48-4328-9844-3F5CA0500000  (notify)
KISS TX:       544E4332-8A48-4328-9844-3F5CA0510000  (write w/ response)
```

### 3.4 KISS frame handling

**TX path (App → Device → RF):**
1. App sends chunked KISS frame to KISS TX characteristic.
2. Firmware reassembles the frame via BleChunker.
3. Firmware strips FEND delimiters, un-escapes, extracts AX.25 frame.
4. Firmware passes AX.25 frame to the existing TX scheduler (TxScheduler).
5. Firmware does NOT send KISS-level ACK — KISS is a raw pipe.

**RX path (RF → Device → App):**
1. Firmware decodes APRS frame from modem.
2. Firmware wraps AX.25 frame in a KISS data frame (FEND `0x00` <data> FEND).
3. Firmware sends chunked KISS frame via KISS RX notify.
4. App reassembles and delivers to KISS-speaking software.

### 3.5 Return-from-KISS (`0x0F`)

If the app sends a frame with type byte `0x0F`, the device terminates KISS mode:
- Stops sending KISS RX notifies.
- Reverts to native PAKT telemetry/status notifies.
- Returns a KISS `0x0F` acknowledge frame (one-shot, no chunking needed).

### 3.6 Multi-client arbitration

KISS mode and native PAKT mode operate simultaneously. The device sends both KISS RX notifies and native `rx_packet` notifies for every decoded frame. TX requests from the native `tx_request` characteristic and KISS TX characteristic are merged into the same TxScheduler queue. KISS TX frames do not receive tx_result notifies — callers must implement their own retry logic if needed.

---

## 4. Capability negotiation

Before using the KISS profile, the client MUST check the Device Capabilities characteristic:

```python
caps = await client.read_capabilities()
if not caps.supports("kiss_ble"):
    # Fall back to native PAKT protocol
```

Firmware that supports the KISS profile will advertise `"kiss_ble"` in the features list. Firmware without the KISS profile will not have the KISS Service in its GATT table; attempting to subscribe to KISS RX will raise a characteristic-not-found error.

---

## 5. Reference implementation plan

The reference implementation will consist of:

1. **Firmware**: `firmware/components/kiss/` — KissFramer (encode/decode), KISS GATT service added to BleServer, TX path integration into TxScheduler.
2. **Desktop bridge app**: `app/desktop_test/kiss_bridge.py` — accepts serial KISS connection (virtual COM or named pipe) and bridges to BLE KISS characteristics.
3. **Test vectors**: known KISS-encoded AX.25 frames with expected BLE chunk sequences.

Implementation is deferred to M3 (Interop expansion milestone).

---

## 6. Open questions

1. **Latency impact**: Does adding KISS RX notify alongside native `rx_packet` notify double the BLE TX load? Measure at Step 11 / M3.
2. **Authentication**: Should KISS TX also require bonded+encrypted write? (Recommendation: yes — same policy as native `tx_request`.)
3. **Serial bridge COM port**: Windows virtual COM vs. named pipe for bridging to legacy KISS clients. Decision deferred to M3.
4. **KISS extended frame types** (0x01–0x06 port commands): out of scope for PAKT hardware; return `0x00` no-op response if received.

---

## 7. References

- TAPR KISS TNC specification: https://www.ax25.net/kiss.aspx (external)
- PAKT BLE GATT specification: `doc/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- INT-002 chunking specification: `doc/aprs_mvp_docs/docs/05_ble_gatt_spec.md` (chunk framing section)
