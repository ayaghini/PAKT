# PAKT APRS Pocket TNC — Pairing and Security Policy (DOC-003)

Version: 0.1 (pre-hardware draft — hardware validation required before DOC-003 closes)
Date: 2026-03-09

---

## Security model overview

The PAKT TNC uses **Bluetooth LE Secure Connections (LE SC)** with mandatory bonding.

Key properties:

| Property | Value | Effect |
|---|---|---|
| Security mode | LE Secure Connections (`sm_sc = 1`) | Provides ECDH key agreement; resistant to passive eavesdropping |
| Bonding | Required (`sm_bonding = 1`) | Bond keys stored in NVS; reconnect without re-pairing |
| I/O capability | No Input / No Output (`BLE_SM_IO_CAP_NO_IO`) | Uses Just Works association; no PIN required |
| Write protection | Encrypted + bonded required | config, command, TX request characteristics reject writes from unbound clients |
| Read access | No encryption required | DIS, device status, telemetry are readable without bonding |

**Threat model summary:** An unpaired device within BLE range can read telemetry and device info, but cannot change the callsign, send commands, or transmit. The bonded client is the only client that can alter device behaviour.

---

## First-time pairing (Windows)

1. Open the PAKT desktop app (`python main.py`) and press `S` to scan.
2. Select the PAKT-TNC device.
3. Windows will display a **Bluetooth pairing request** dialog: *"Do you want to pair with PAKT-TNC?"*
4. Click **Yes** (or **Pair**). No PIN is shown or required — the device uses Just Works association.
5. After pairing, the app displays `CONNECT` and `SUBSCRIBE` log lines. The bond is now stored on both sides.
6. All write operations (config, command, TX) are now permitted.

> **Windows note:** On some Windows versions the pairing dialog appears in the system tray (bottom right) rather than a foreground window. If the write fails with AUTH_ERR immediately after connect, look for a pending notification in the tray.

---

## Subsequent connections

After the initial bond is established, the device will reconnect without showing a new pairing dialog.
The app's `BleTransport` FSM handles:
- Scanning → Connecting → Connected on first connect
- RECONNECTING state (up to 3 attempts, 1 s between) if the BLE link drops
- On reconnect success: notification handlers are automatically re-subscribed

---

## Write rejection (AUTH_ERR)

If a write is rejected and the app shows:

```
  [!] AUTH_ERR   <characteristic>   Write rejected: link not encrypted+bonded. ...
```

This means the BLE link is connected but not bonded. Causes:
- The bond was removed from the Windows side (Bluetooth settings → Remove device) but not cleared on the device.
- The device NVS was erased (factory reset) while the PC still has the old bond keys.
- A new PC is attempting to connect without pairing first.

**Resolution procedure:**

**From Windows:**
1. Open **Settings → Bluetooth & devices**.
2. Find **PAKT-TNC** in the device list.
3. Click **…** → **Remove device**.
4. In the desktop app, press `D` to disconnect (if connected), then `S` to scan and reconnect.
5. The Windows pairing dialog will appear again. Accept it.

**From the device (bond reset):**
If the Windows "Remove device" step is not sufficient (e.g. device still has stale keys):
1. Hold the device's **bond-reset button** for 3 seconds (LED will flash rapidly) — see hardware user guide.
2. This erases the NVS bond keys on the device side.
3. Then follow the "From Windows" procedure above to re-pair.

> **Note:** The bond-reset button procedure is a hardware feature not yet validated; update this document after first hardware bring-up.

---

## Bond management for multiple clients

The PAKT TNC stores **one bond** at a time in its NVS partition.
If a second client (e.g. a phone) bonds with the device, the existing bond (e.g. from the desktop) is replaced.

To use both a desktop and phone:
- Bond the phone first, then re-pair the desktop (phone bond is preserved if desktop re-pairs without erasing phone keys). This behaviour is pending hardware confirmation.
- Or use the desktop test tool exclusively during development and field testing.

---

## Security limits and known gaps

| Limitation | Impact | Mitigation |
|---|---|---|
| Just Works association (no PIN/passkey) | An attacker within range during the brief pairing window can intercept the bond | Keep the pairing window short (pair in a trusted location); re-pair if in doubt |
| Single bond slot | A second device bonding will evict the first | Use the bond-reset + re-pair procedure to restore access |
| No mutual authentication of app identity | The bonded PC could be any application | Physical possession of the bonded device is the trust anchor; acceptable for personal field equipment |
| NVS bond keys survive firmware update | After OTA, the bond is still valid (good UX) | Ensure firmware update path is trusted; a malicious firmware could read NVS keys |
| PTT safety under BLE fault | A software fault could leave PTT active during BLE disruption | FW-016 (watchdog + PTT safe-off) must pass G3 before field use; this is a P0 blocking risk |

---

## Regulatory and operational notes

- Configuring a callsign and transmitting constitutes operation under your amateur radio licence. You are responsible for all transmissions.
- The device will not transmit until a callsign is written (config write requires bonding — see above).
- Frequency and power settings are configurable via the `device_command` characteristic. Incorrect settings may violate band plans and regulations. Always confirm frequency before transmitting.
- The device is not type-accepted for commercial or non-amateur use.

---

## Document status

This document is a pre-hardware draft. The following items require validation on prototype hardware before this document can be marked `done` (DOC-003):

- [ ] Windows pairing dialog behaviour confirmed on Windows 10 and Windows 11
- [ ] AUTH_ERR flow confirmed: write rejection → remove device → re-pair → success
- [ ] Bond-reset button behaviour confirmed (hardware feature)
- [ ] Multi-client (phone + desktop) bond eviction behaviour confirmed
- [ ] PTT safe-off under BLE fault confirmed (G3 prerequisite)
