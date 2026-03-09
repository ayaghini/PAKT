# APRS 2m Pocket TNC + Tracker (SA818 + ESP32-S3 + GPS) - MVP Docs

Date: 2026-02-27

This folder contains the starter documentation set for a standalone APRS
(1200 baud AFSK, AX.25) 2m device that exposes BLE interfaces to desktop and phone clients.

## Goal (MVP)
A pocket device that:
- Transmits periodic APRS position beacons on 144.390 MHz (NA), configurable by region
- Receives and decodes APRS packets and forwards them to BLE clients
- Sends APRS messages from BLE clients to RF with basic retry/ack handling
- Uses GPS for position/time
- Provides BLE configuration and live status
- Runs from battery and charges over USB-C
- Supports a Windows desktop BLE test app workflow before phone app rollout

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
- `docs/12_implementation_backlog.md`
- `hardware/placeholder_bom.csv`
- `hardware/interfaces.md`
- `agent_bootstrap/README.md` (low-token onboarding pack for AI agents)

## Agent-first entrypoint
Start with:
1. `agent_bootstrap/README.md`
2. `agent_bootstrap/agent_context.yaml`
3. Remaining files in the strict bootstrap load order

The bootstrap pack includes:
- execution workflow
- implementation step sequence with required evidence
- QA gate criteria and failure handling
- step-to-source mapping for minimal-context document loading
- connected-device upload/debug/verify loop (`agent_bootstrap/device_loop.md`)

## Current hardware baseline
- Audio codec: `SGTL5000` (with explicit `I2S_MCLK`)
- RF module: `SA818`
- Battery charger: `MCP73831/2` class
- Battery fuel gauge: `MAX17048`

## What this is / is not
- A practical starting point for hardware + firmware implementation
- Enough structure to start implementation tasks
- Not a complete electrical schematic or PCB layout yet

## Suggested next steps
1. Confirm target region defaults and APRS path presets.
2. Complete SGTL5000 + SA818 audio level calibration values for TX/RX.
3. Build/validate the devkit prototype path with Feather-aligned power telemetry (`MAX17048`).
4. Implement BLE GATT services and Windows desktop test app flows first:
   - set callsign/SSID, beacon interval, symbol, comment
   - show RX packets, status, and telemetry
   - send TX request and capture TX result
5. Add phone app UX flows after desktop harness is stable.
6. Complete TX beacon + RX decode loop, then message ACK/retry.
