# APRS 2m Pocket TNC + Tracker (SA818 + ESP32-S3 + GPS) - MVP Docs

Last updated: 2026-03-20

This folder contains the working documentation set for a standalone APRS
(1200 baud AFSK, AX.25) 2m device that exposes BLE interfaces to desktop and phone clients,
including MVP KISS TNC interoperability over BLE for third-party APRS software.

Companion repo-root operator docs live alongside this folder:
- `../dev_setup.md` — local toolchain, build, and test flow
- `../bench_bringup_checklist.md` — prototype bring-up sequence
- `../bench_measured_values_template.md` — bench measurement capture sheet

## Goal (MVP)
A pocket device that:
- Transmits periodic APRS position beacons on 144.390 MHz (NA), configurable by region
- Receives and decodes APRS packets and forwards them to BLE clients
- Sends APRS messages from BLE clients to RF with basic retry/ack handling
- Exposes a KISS-over-BLE profile so existing APRS software can use the device as a TNC
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
- `docs/15_interoperability_matrix.md`
- `docs/16_kiss_over_ble_spec.md`
- `docs/17_mvp_gap_analysis.md`
- `hardware/placeholder_bom.csv`
- `hardware/interfaces.md`
- `agent_bootstrap/README.md` (low-token onboarding pack for AI agents)

## Agent-first entrypoint
Start with:
1. `agent_bootstrap/README.md`
2. `agent_bootstrap/implementation_steps_mvp.md`
3. Remaining files in the strict bootstrap load order

The bootstrap pack includes:
- execution workflow
- implementation step sequence with required evidence
- QA gate criteria and failure handling
- step-to-source mapping for minimal-context document loading
- connected-device upload/debug/verify loop (`agent_bootstrap/device_loop.md`)
- rolling implementation log (`agent_bootstrap/audit.md`)
- gate status summary (`agent_bootstrap/gate_pass_matrix.md`)

## Current implementation snapshot
- Software: most MVP software paths are implemented, including native BLE, KISS-over-BLE, AX.25/APRS framing, AFSK modem, APRS TX scheduling, telemetry payloads, desktop BLE tooling, and KISS bridge support.
- Contracts: `payload_contracts.md` is the canonical JSON schema source for BLE payloads.
- Hardware: bench bring-up and RF/electrical validation remain the main gating steps.
- RF status: short APRS packet TX has now been bench-proven on a separate receiver; on-device APRS RX is still open.
- RX diagnosis status: current bench evidence shows the receive analog path is alive, instrumented, and now recordable via PSRAM-backed `16-bit` WAV export at both `8 kHz` and `16 kHz`. Scope captures and saved WAVs still disagree, so the current strongest suspicion is in the SGTL5000 / I2S / sample-capture path rather than gross analog line failure alone.
- Bench workflow status: blocking bench stages are now selectable through `firmware/main/bench_profile_config.h`, so prototype debug runs can be narrowed to only the needed stages.
- Interop: KISS-over-BLE is part of MVP and is software-complete enough for hardware validation; third-party client evidence is still pending.

## Documentation split
- `docs/aprs_mvp_docs/` is the canonical spec, architecture, protocol, and project-status tree.
- The sibling files under repo-root `docs/` are the practical operator/developer companion docs for setup and bench execution.
- If implementation-status wording disagrees, prefer `agent_bootstrap/gate_pass_matrix.md`, `agent_bootstrap/audit.md`, and `firmware/main/main.cpp`.

## Current hardware baseline
- Audio codec: `SGTL5000` (with explicit `I2S_MCLK`)
- RF module: `SA818`
- Battery charger: `MCP73831/2` class
- Battery fuel gauge: `MAX17048`

## Recommended reading by role
- New coding agent: start with `agent_bootstrap/README.md`
- Firmware bring-up: read `docs/06_firmware_architecture.md`, `docs/07_audio_i2s_codec_notes.md`, `agent_bootstrap/audit.md`, and `../bench_bringup_checklist.md`
- Host/app integration: read `docs/05_ble_gatt_spec.md`, `docs/16_kiss_over_ble_spec.md`, `payload_contracts.md`, and the desktop app sources under `app/desktop_test/`

## Current status at a glance
- MVP gate: open
- Strongest validated RF result so far: external APRS packet reception from the prototype TX path
- Most important missing RF result: on-device APRS RX decode
- Main next verification step: close the SGTL5000/I2S/sample-capture mismatch using the new `16 kHz` recorder and scope-guided comparison, then return to on-device APRS RX decode proof

## What this is / is not
- A practical starting point for hardware + firmware implementation
- Enough structure to onboard agents and execute implementation safely
- Not a complete electrical schematic or PCB layout yet
