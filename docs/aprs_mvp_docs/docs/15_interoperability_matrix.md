# PAKT APRS Pocket TNC — Interoperability Matrix (DOC-004)

Status: **DRAFT** — software readiness is high; hardware validation and field testing are still required before final publication.
Version: 0.2
Date: 2026-03-16

---

## 1. Scope

This document tracks tested and known-compatible client platforms, operating systems, and radio configurations for the PAKT TNC. All entries are classified by validation status.

Validation status codes:
- `verified` — tested on physical hardware, passes all relevant test cases
- `expected` — implementation complete; hardware test not yet performed
- `partial` — some test cases pass; known gaps documented
- `untested` — no validation performed yet
- `incompatible` — known to be incompatible; reason documented

---

## 2. Desktop client compatibility

### 2.1 Windows (native PAKT protocol)

| Platform | BLE stack | Python | Status | Notes |
|---|---|---|---|---|
| Windows 11 22H2+ | WinRT BLE | 3.10–3.13 | `expected` | bleak WinRT backend; pairing dialog confirmed in design |
| Windows 10 21H2+ | WinRT BLE | 3.10–3.13 | `expected` | Same as Win11; older WinRT BLE may have MTU negotiation quirks |
| Windows 10 <21H2 | WinRT BLE | 3.x | `untested` | Older WinRT BLE; may not support LE Secure Connections |

### 2.2 macOS (native PAKT protocol)

| Platform | BLE stack | Python | Status | Notes |
|---|---|---|---|---|
| macOS 13 Ventura+ | CoreBluetooth | 3.10+ | `untested` | bleak CoreBluetooth backend; requires user permission for BLE |
| macOS 12 Monterey | CoreBluetooth | 3.10+ | `untested` | Same as above |

### 2.3 Linux (native PAKT protocol)

| Platform | BLE stack | Python | Status | Notes |
|---|---|---|---|---|
| Ubuntu 22.04+ | BlueZ 5.64+ | 3.10+ | `untested` | bleak BlueZ backend; LE SC bonding via bluetoothctl |
| Raspberry Pi OS (Bookworm) | BlueZ 5.66+ | 3.11 | `untested` | Potential use as headless bridge node |

---

## 3. Mobile client compatibility

### 3.1 Android (native PAKT protocol)

| Platform | BLE version | Status | Notes |
|---|---|---|---|
| Android 12+ (API 31+) | BLE 5.x | `untested` | Target platform for phone app; LE SC required |
| Android 10–11 (API 29–30) | BLE 4.2+ | `untested` | LE SC supported; MTU negotiation behaviour varies by OEM |
| Android < 10 | BLE 4.x | `incompatible` | LE Secure Connections unreliable below Android 10 |

### 3.2 iOS (native PAKT protocol)

| Platform | BLE version | Status | Notes |
|---|---|---|---|
| iOS 16+ | BLE 5.x | `untested` | CoreBluetooth; LE SC with Just Works requires no entitlement |
| iOS 15 | BLE 5.x | `untested` | Same as iOS 16 |
| iOS < 15 | BLE 4.x | `untested` | LE SC available but not confirmed for this device profile |

---

## 4. KISS-over-BLE compatibility (MVP)

The KISS-over-BLE profile (INT-003, `docs/16_kiss_over_ble_spec.md`) is part of MVP and enables use with existing KISS-speaking APRS software either directly or via a thin bridge utility where needed.

| APRS Software | Platform | KISS mode | Status | Notes |
|---|---|---|---|---|
| APRSdroid | Android | KISS/TCP | `untested` | Bridge via Android BLE → local TCP KISS server |
| YAAC | Windows/macOS/Linux | KISS/serial | `untested` | Bridge via virtual COM port |
| Xastir | Linux | KISS/serial | `untested` | Bridge via virtual COM or named pipe |
| APRS-IS gateway scripts | Linux | KISS/serial | `untested` | Headless bridge via Raspberry Pi |
| Direwolf | Linux/Windows | KISS/serial | `untested` | Direwolf supports external TNC; bridge via serial |

---

## 5. Radio frequency configurations

The PAKT TNC uses the SA818 VHF module. Validated and known frequency configurations:

| Region | Frequency | Status | Notes |
|---|---|---|---|
| North America | 144.390 MHz | `expected` | Default firmware config |
| Europe | 144.800 MHz | `expected` | Set via `radio_set` command |
| Australia/NZ | 145.175 MHz | `untested` | Set via `radio_set` command |
| Japan | 144.640 MHz | `untested` | Set via `radio_set` command |
| SA818 VHF range | 134–174 MHz | `expected` | Any frequency in SA818 spec range configurable |

**Regulatory note:** The operator is responsible for operating on a permitted frequency and power level. The firmware does not enforce regulatory limits beyond what the SA818 hardware supports.

---

## 6. BLE MTU and chunking

The PAKT chunking protocol (INT-002) is designed to work at any negotiated MTU. Verified and expected configurations:

| MTU | Payload/chunk | Chunks for 256 B config | Status |
|---|---|---|---|
| 247 (BLE 5 optimal) | 241 B | 2 | `expected` |
| 185 | 179 B | 2 | `expected` |
| 23 (BLE 4 minimum) | 17 B | 16 | `expected` (host tests pass) |

Chunk reassembly with duplicate and out-of-order frames is validated in host unit tests (test_ble_chunker.cpp, test_chunker.py).

---

## 7. Known limitations and caveats

| Limitation | Affected platforms | Mitigation |
|---|---|---|
| Single BLE bond slot | All | Only one client can be bonded at a time; re-pair to switch clients |
| Just Works pairing (no PIN) | All | Attacker within BLE range during pairing window could intercept bond; pair in trusted environments |
| Windows "Remove device" required for bond reset | Windows | See pairing policy doc (`14_pairing_security_policy.md`) |
| LE SC not available on Android < 10 | Android | Minimum supported Android is 10 |
| BLE audio bridge latency (HF variant) | All | HF audio latency not measured; subject to Step 11 / HF discovery track findings |
| No OTA firmware update (MVP) | All | Firmware update requires USB-C cable and `idf.py flash` |
| KISS third-party validation not yet complete | All | Firmware/software path is implemented; validate with APRSdroid, YAAC, Xastir, or Direwolf during hardware bring-up |

---

## 8. Validation checklist (to complete before DOC-004 closes)

- [ ] Windows 11 connect/pair/write cycle confirmed
- [ ] Windows 10 connect/pair/write cycle confirmed
- [ ] AUTH_ERR + Remove device + re-pair confirmed on Windows
- [ ] Reconnect after link drop confirmed on Windows
- [ ] 1-hour BLE session stability confirmed (QA-004)
- [ ] Android test device paired and config write confirmed
- [ ] iOS test device paired and config write confirmed
- [ ] macOS test confirmed
- [ ] KISS TX validated with at least one reference third-party client or bridge
- [ ] KISS RX validated with at least one reference third-party client or bridge
- [ ] North America 144.390 MHz beacon decode confirmed by reference TNC
- [ ] Europe 144.800 MHz frequency set and confirmed
