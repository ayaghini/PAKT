# APRS 2m Pocket TNC + Tracker (SA818 + ESP32-S3 + GPS) — MVP Docs

Date: 2026-02-27

This folder contains a *starter* documentation set to kick off a repo for a standalone APRS (1200 baud AFSK, AX.25) 2‑meter device that connects to phones over BLE.

## Goal (MVP)
A pocket device that:
- Transmits periodic APRS position beacons on 144.390 MHz (NA) (configurable for region)
- Receives and decodes APRS packets and forwards them to the phone
- Sends APRS messages from phone → RF with basic retry/ack handling
- Uses GPS for position/time
- Provides BLE configuration + live status
- Runs from battery, charges over USB‑C

## Contents
- `docs/01_product_brief.md`
- `docs/02_mvp_scope.md`
- `docs/03_system_architecture.md`
- `docs/04_hardware_block_diagram.md` (+ `assets/hardware_block_diagram.png`)
- `docs/05_ble_gatt_spec.md`
- `docs/06_firmware_architecture.md`
- `docs/07_audio_i2s_codec_notes.md`
- `docs/08_test_plan.md`
- `docs/09_risks_and_mitigations.md`
- `docs/10_open_questions.md`
- `docs/11_hf_ble_feasibility_study.md`
- `hardware/placeholder_bom.csv`
- `hardware/interfaces.md`

## What this is / isn’t
- ✅ A practical starting point to design hardware + firmware
- ✅ Enough structure to start implementation tasks
- ❌ Not a complete electrical schematic or PCB layout (yet)

## Suggested next steps
1. Confirm target region(s) and default APRS frequency/path presets.
2. Choose an I2S audio codec and finalize audio levels/filters.
3. Build a breadboard / devkit prototype: ESP32‑S3 + codec + SA818 + GPS.
4. Implement BLE GATT services and a minimal phone app to:
   - set callsign/SSID, beacon interval, symbol, comment
   - show RX packets + map
   - send a message
5. Implement TX beacon + RX decode loop, then message ACK/retry.



