# PAKT APRS Pocket TNC — Setup and Quickstart Guide (DOC-001)

Version: 0.1 (pre-hardware draft — validated by new-user test required before DOC-001 closes)
Date: 2026-03-09

---

## What you need

| Item | Notes |
|---|---|
| PAKT APRS Pocket TNC device | Charged via USB-C before first use |
| Windows 10/11 PC with Bluetooth 4.2+ | Bluetooth must be enabled |
| Python 3.10+ | Comes with Windows 11; or install from python.org |
| Amateur radio licence | Required to transmit on 144.390 MHz APRS |
| Valid callsign and SSID | e.g. `W1AW`, SSID `9` for a portable tracker |

---

## Step 1 — Install the desktop test app

```
pip install bleak
```

Clone or download the PAKT repository, then navigate to the desktop test app:

```
cd app/desktop_test
python main.py
```

The app starts in disconnected state and displays the main menu.

---

## Step 2 — Power on the device

Connect the PAKT TNC to USB-C power (or use a charged LiPo battery).
The device will:
1. Boot the ESP32-S3 (< 2 s).
2. Start advertising as **PAKT-TNC** over BLE.
3. Begin listening for APRS packets on 144.390 MHz (default).

The device is ready when the blue LED blinks slowly (advertising mode).

---

## Step 3 — Scan and connect

In the desktop app main menu:

```
  [S]  Scan for PAKT devices
```

Press `S` then Enter. The app scans for 8 seconds and lists found devices:

```
  [0]  PAKT-TNC  (AA:BB:CC:DD:EE:FF)
Select [0]:
```

Press Enter to select device 0 (or type the index number).

---

## Step 4 — Pair the device (first use only)

On **first connection**, Windows will show a Bluetooth pairing dialog.
Accept the pairing request. The device uses LE Secure Connections — no PIN is required.

After pairing, the app displays:

```
      <timestamp>  CONNECT         AA:BB:CC:DD:EE:FF      mtu=247
      <timestamp>  SUBSCRIBE       device_status          ok
      ...
```

You are now connected and bonded.

> **If you do NOT see a pairing dialog** and writes are rejected with `[!] AUTH_ERR`:
> See [Pairing and Security Policy](14_pairing_security_policy.md) for the bond-reset procedure.

---

## Step 5 — Configure your callsign

Press `3` (Write config) and enter your configuration as JSON:

```json
{
  "callsign": "W1AW",
  "ssid": 9,
  "beacon_interval_s": 300,
  "symbol_table": "/",
  "symbol_code": ">",
  "comment": "Pocket TNC"
}
```

The app will show a diff of any changes vs. the cached config, then write to the device.
The device stores the config in NVS flash (survives reboot).

**Field meanings:**

| Key | Description | Example |
|---|---|---|
| `callsign` | Your amateur callsign (no SSID) | `"W1AW"` |
| `ssid` | APRS SSID (0–15); 9 = portable | `9` |
| `beacon_interval_s` | Auto-beacon interval in seconds; 0 = off | `300` |
| `symbol_table` | APRS symbol table char (`/` = primary, `\` = alternate) | `"/"` |
| `symbol_code` | APRS symbol code char | `">"` (car) |
| `comment` | Free-text beacon comment (≤ 43 chars) | `"Pocket TNC"` |

---

## Step 6 — Read device info and status

Press `1` to read Device Information Service (firmware version, model, manufacturer).

Press `8` (Listen 30 s) to see live notifications:
- `device_status` — radio state, GPS fix, BLE bond state, pending TX count
- `gps_telem` — lat/lon/alt/speed/course/sats
- `power_telem` — battery voltage and percentage, TX power, VSWR, temperature
- `system_telem` — free heap, CPU load, packet counts, uptime

Press `T` at any time to see the latest telemetry snapshot without waiting for a notify.

---

## Step 7 — Send a test message

Press `7` (Send TX request) and enter:

```json
{"to": "CQ", "text": "Hello from PAKT", "msg_id": "01"}
```

The device will attempt to transmit the APRS message.
Watch for `tx_result` notifications — the device sends one for each transmission attempt
and a final `acked` or `timeout` result.

Press `9` to view the message queue with per-message state and attempt count.

---

## Step 8 — Export logs and diagnostics

Press `E` to export the full timestamped session log to `pakt_session_<timestamp>.log`.

Press `X` to export the structured diagnostics report (telemetry stats, packet counts,
GPS track, power history) to `pakt_diagnostics_<timestamp>.json`.

---

## Step 9 — Disconnect

Press `D` to disconnect cleanly. Press `Q` to quit.

The last-read config is cached locally in `pakt_config_cache.json` and shown on the
next startup so you always know the last-known device state.

---

## Frequency and regulatory notes

- Default frequency: **144.390 MHz** (North America APRS)
- Europe: 144.800 MHz — change via `radio_set` command (Step 7, command `{"cmd":"radio_set","freq_hz":144800000}`)
- **Transmitting requires a valid amateur radio licence** for the frequency and region in use.
- The device will not transmit until a valid callsign is configured (Step 5).

---

## Troubleshooting quick reference

| Symptom | Check |
|---|---|
| Device not found in scan | Ensure BLE is enabled on PC; device is powered and advertising (blue LED blinking) |
| Write rejected with AUTH_ERR | Bond is missing — see [Pairing and Security Policy](14_pairing_security_policy.md) |
| No GPS fix (`gps_fix: false`) | Move to outdoor location with clear sky view; allow 1–2 min for cold fix |
| No notifications arriving | Press `8` to subscribe; check that `SUBSCRIBE` lines appeared after connect |
| Config lost after reboot | Ensure write completed with no AUTH_ERR; check NVS flash is not erased |
| TX request stays in `pending` | Verify callsign is set; check radio state in `device_status` notify |
