# Implementation Backlog (Comprehensive)

Date: 2026-02-28
Related docs:
- `docs/01_product_brief.md`
- `docs/02_mvp_scope.md`
- `docs/03_system_architecture.md`
- `docs/05_ble_gatt_spec.md`
- `docs/06_firmware_architecture.md`
- `docs/08_test_plan.md`
- `docs/09_risks_and_mitigations.md`
- `docs/11_hf_ble_feasibility_study.md`

## 1. Purpose
This backlog is the execution plan for delivering:
- MVP APRS 2m Pocket TNC + Tracker
- MVP KISS TNC interoperability over BLE
- HF-capable variant with unified app architecture

## 2. Backlog Conventions
- Priority:
  - `P0` critical path to MVP or safety
  - `P1` high value, near-term
  - `P2` medium value, post-MVP
  - `P3` future/optional
- Status:
  - `todo`, `in_progress`, `blocked`, `done`
- Estimate:
  - `S` <= 2 days
  - `M` 3-5 days
  - `L` 1-2 weeks
  - `XL` > 2 weeks
- Dependencies: list of item IDs that must complete first.
- Done criteria: minimum acceptance condition for closing item.

## 3. Milestones
- `M0` Foundation and bench bring-up
- `M1` MVP feature complete
- `M2` MVP stabilization and field validation
- `M3` Ecosystem expansion and broader client compatibility
- `M4` HF variant discovery and beta
- `M5` HF productization decision

## 4. Program and Product Management

### PM-001 Project board and cadence
- Priority: `P0`
- Estimate: `S`
- Dependencies: none
- Done criteria: Kanban or sprint board created with this backlog imported; weekly review cadence defined.

### PM-002 Definition of Ready / Done
- Priority: `P0`
- Estimate: `S`
- Dependencies: PM-001
- Done criteria: team-agreed checklists for story readiness and completion.

### PM-003 Requirements traceability matrix
- Priority: `P1`
- Estimate: `M`
- Dependencies: PM-001
- Done criteria: matrix maps backlog IDs to MVP scope and acceptance criteria.

### PM-004 Release train plan
- Priority: `P1`
- Estimate: `S`
- Dependencies: PM-001
- Done criteria: release windows and freeze dates set for M0-M2.

### PM-005 Change control for protocol and hardware revisions
- Priority: `P1`
- Estimate: `S`
- Dependencies: PM-001
- Done criteria: versioning and compatibility policy published.

## 5. Hardware Stream (MVP Device)

### HW-001 Finalize component selection
- Priority: `P0`
- Estimate: `M`
- Dependencies: none
- Done criteria: approved BOM v0 with alternates for supply risk.

### HW-002 SA818 electrical validation
- Priority: `P0`
- Estimate: `M`
- Dependencies: HW-001
- Done criteria: confirmed supply limits, UART levels, PTT polarity, AF levels on actual module SKU.

### HW-003 ESP32-S3 pin map freeze
- Priority: `P0`
- Estimate: `S`
- Dependencies: HW-001
- Done criteria: pin assignment doc locked with boot strap constraints validated.

### HW-004 Power tree schematic
- Priority: `P0`
- Estimate: `M`
- Dependencies: HW-001
- Done criteria: MCP73831/2 charger path, MAX17048 fuel gauge path, battery, regulator, rail isolation, and test points in schematic.

### HW-005 RF and audio front-end schematic (SGTL5000 + SA818)
- Priority: `P0`
- Estimate: `L`
- Dependencies: HW-002
- Done criteria: AF coupling, attenuation options, explicit `I2S_MCLK` routing, RF path and connector with DNP flexibility.

### HW-006 ESD and connector protection design
- Priority: `P1`
- Estimate: `M`
- Dependencies: HW-004
- Done criteria: ESD parts on exposed IO and USB-C included and reviewed.

### HW-007 Prototype schematic review (EVT)
- Priority: `P0`
- Estimate: `S`
- Dependencies: HW-004, HW-005, HW-006
- Done criteria: formal review complete; action items closed.

### HW-008 PCB layout Rev A
- Priority: `P0`
- Estimate: `L`
- Dependencies: HW-007
- Done criteria: layout complete with RF keepout, power return strategy, analog separation.

### HW-009 Design rule and SI/PI checks
- Priority: `P0`
- Estimate: `M`
- Dependencies: HW-008
- Done criteria: DRC/ERC clean and documented exceptions approved.

### HW-010 EVT build and bring-up fixtures
- Priority: `P0`
- Estimate: `M`
- Dependencies: HW-008
- Done criteria: 5-10 EVT units assembled; bring-up jig and checklists available.

### HW-011 EVT characterization
- Priority: `P0`
- Estimate: `L`
- Dependencies: HW-010
- Done criteria: measured rails, thermals, RF sensitivity proxy, audio dynamic range, current profiles.

### HW-012 Rev B updates from EVT
- Priority: `P1`
- Estimate: `L`
- Dependencies: HW-011
- Done criteria: Rev B schematic/PCB updates complete with issue closure report.

### HW-013 Pre-compliance planning
- Priority: `P2`
- Estimate: `M`
- Dependencies: HW-011
- Done criteria: EMC/regulatory test plan and lab options prepared.

## 6. Firmware Stream (MVP)

### FW-001 Repository and build system baseline
- Priority: `P0`
- Estimate: `S`
- Dependencies: none
- Done criteria: reproducible build in CI; pinned toolchain versions.

### FW-002 Hardware abstraction layer skeleton
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-001
- Done criteria: interfaces for `IAudioIO`, `IRadioControl`, `IStorage`, `IPacketLink` created and tested with mocks.

### FW-003 SA818 driver
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-002
- Done criteria: frequency, squelch, power and PTT commands verified on hardware.

### FW-004 I2S codec driver and clocking (SGTL5000 baseline)
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-002
- Done criteria: stable 8 kHz pipeline with no sustained underrun/overrun under test load; SGTL5000 `SYS_MCLK` configuration validated on bench.

### FW-005 GPS parser and fix management
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-002
- Done criteria: NMEA parsing and stale-fix handling implemented with unit tests.

### FW-006 AFSK demod pipeline
- Priority: `P0`
- Estimate: `XL`
- Dependencies: FW-004
- Done criteria: decodes known sample corpus at target accuracy in bench tests.

### FW-007 AFSK mod pipeline
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-004
- Done criteria: generated signal passes decode by reference receiver at target error rates.

### FW-008 AX.25 framing and CRC
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-006, FW-007
- Done criteria: standards-compliant frame encode/decode with unit tests.

### FW-009 APRS payload encode/decode helpers
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-008
- Done criteria: position and message frame helpers validated against known-good vectors.

### FW-010 TX scheduler and message ACK/retry FSM
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-009
- Done criteria: retry and timeout behavior validated in bench integration tests.

### FW-011 BLE GATT service implementation
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-002
- Done criteria: services/characteristics from `05_ble_gatt_spec.md` implemented and interoperable with test app.

### FW-012 BLE security hardening
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-011
- Done criteria: encrypted + bonded writes enforced; pairing window and bond reset behavior implemented.

### FW-013 Config storage and schema versioning
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-011
- Done criteria: persistent config survives reboot and schema migration tested.

### FW-014 Power management state machine
- Priority: `P1`
- Estimate: `L`
- Dependencies: FW-003, FW-011
- Done criteria: idle/sleep transitions reliable with wake behavior defined.

### FW-015 Telemetry and diagnostics endpoints
- Priority: `P1`
- Estimate: `M`
- Dependencies: FW-011
- Done criteria: telemetry fields stable and rate-limited; diagnostics include fault counters.

### FW-016 Watchdog and fault recovery policy
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-003, FW-004, FW-011
- Done criteria: brownout, stalled task and BLE fault recovery tested with no unsafe TX lockup.

### FW-017 Manufacturing test mode
- Priority: `P1`
- Estimate: `M`
- Dependencies: FW-011
- Done criteria: fixture can run pass/fail checks over serial or BLE.

### FW-018 KISS framing and BLE service implementation
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-010, FW-011, INT-001
- Done criteria: KISS framer/parser, KISS GATT service, shared TX scheduler path, and RX frame wrapping implemented with host tests.

### FW-019 Firmware signing and OTA strategy (post-MVP)
- Priority: `P2`
- Estimate: `L`
- Dependencies: FW-013
- Done criteria: update package format and trust chain documented and prototyped.

## 7. Client App Stream (Desktop + Mobile)

### APP-000 Windows desktop BLE test harness
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-011
- Done criteria: desktop app can scan/pair/connect, read/write config, issue test commands, display status/RX/TX/telemetry, and export session logs.

### APP-001 App architecture baseline
- Priority: `P0`
- Estimate: `M`
- Dependencies: none
- Done criteria: layered architecture (`transport`, `mode services`, `ui`) scaffolded.

### APP-002 BLE transport client
- Priority: `P0`
- Estimate: `L`
- Dependencies: APP-000, APP-001, FW-011
- Done criteria: scan, pair, connect, MTU negotiation, reconnect flows implemented on Android and iOS.

### APP-003 Device configuration screen
- Priority: `P0`
- Estimate: `M`
- Dependencies: APP-002
- Done criteria: callsign/SSID/beacon/symbol/comment config read/write complete.

### APP-004 Live status dashboard
- Priority: `P0`
- Estimate: `M`
- Dependencies: APP-002
- Done criteria: batt/gps/state/queue status displayed with stale-data indicators.

### APP-005 RX packet monitor and log
- Priority: `P0`
- Estimate: `M`
- Dependencies: APP-002
- Done criteria: packet stream view with filtering and export.

### APP-006 Messaging workflow
- Priority: `P0`
- Estimate: `L`
- Dependencies: APP-002, FW-010
- Done criteria: send, pending, ack, timeout states visible and reliable.

### APP-007 Map view integration
- Priority: `P1`
- Estimate: `L`
- Dependencies: APP-004, APP-005
- Done criteria: device position and decoded stations list/map view functional.

### APP-008 Pairing UX hardening
- Priority: `P0`
- Estimate: `M`
- Dependencies: APP-002, FW-012
- Done criteria: deterministic flows for first pair, re-pair, bond reset, and access-denied states.

### APP-009 Offline-first behavior
- Priority: `P1`
- Estimate: `M`
- Dependencies: APP-004, APP-005
- Done criteria: recent telemetry and logs persist locally; app degrades gracefully when disconnected.

### APP-010 Session diagnostics and support bundle
- Priority: `P1`
- Estimate: `M`
- Dependencies: APP-005, FW-015
- Done criteria: user can export session logs and device diagnostics for support.

### APP-011 Accessibility and usability pass
- Priority: `P1`
- Estimate: `M`
- Dependencies: APP-003, APP-004, APP-006
- Done criteria: key screens meet baseline accessibility checks and task completion targets.

### APP-012 Beta instrumentation and crash analytics
- Priority: `P1`
- Estimate: `S`
- Dependencies: APP-001
- Done criteria: crash and performance telemetry enabled with privacy notice.

### APP-013 KISS compatibility harness
- Priority: `P0`
- Estimate: `M`
- Dependencies: APP-000, FW-018
- Done criteria: desktop-side KISS test or bridge utility can validate TX/RX against the device and record compatibility evidence.

## 8. Protocol and Interoperability Stream

### INT-001 Protocol capability negotiation
- Priority: `P1`
- Estimate: `M`
- Dependencies: FW-011, APP-002
- Done criteria: app and firmware negotiate supported features and versions.

### INT-002 Message framing for chunked payloads
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-011, APP-002
- Done criteria: chunk/reassembly with timeout and duplicate handling validated.

### INT-003 KISS-over-BLE profile
- Priority: `P0`
- Estimate: `L`
- Dependencies: INT-001
- Done criteria: spec finalized for MVP, UUIDs and security policy frozen, and reference implementation validated with at least one third-party client or bridge.

### INT-004 Third-party app compatibility adapter
- Priority: `P1`
- Estimate: `L`
- Dependencies: INT-003
- Done criteria: compatibility mode tested against target external app(s).

### INT-005 API and protocol docs publication pipeline
- Priority: `P1`
- Estimate: `S`
- Dependencies: INT-001
- Done criteria: versioned protocol docs with change log published in repo.

## 9. QA, Verification, and Validation Stream

### QA-001 Unit test baseline (firmware)
- Priority: `P0`
- Estimate: `M`
- Dependencies: FW-002
- Done criteria: unit tests for parsers/protocol/core logic in CI.

### QA-002 Hardware-in-the-loop smoke suite
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-003, FW-004, FW-011
- Done criteria: automated smoke suite runs on bench rig and reports pass/fail.

### QA-003 RF functional test procedures
- Priority: `P0`
- Estimate: `M`
- Dependencies: HW-010, FW-007, FW-008
- Done criteria: procedure for beacon decode, RX decode, messaging validated and repeatable.

### QA-004 BLE endurance and reconnect matrix
- Priority: `P0`
- Estimate: `M`
- Dependencies: APP-000, APP-002, FW-011
- Done criteria: 1-hour continuous RX and reconnect scenarios pass on desktop harness and iOS/Android matrix.

### QA-005 Field test protocol
- Priority: `P1`
- Estimate: `M`
- Dependencies: APP-007, QA-003
- Done criteria: hiking/vehicle field scripts and data capture templates ready.

### QA-006 Regression suite for each firmware release
- Priority: `P0`
- Estimate: `M`
- Dependencies: QA-001, QA-002
- Done criteria: release gate defined and enforced in CI/CD.

### QA-007 Reliability KPI dashboard
- Priority: `P1`
- Estimate: `M`
- Dependencies: APP-012, QA-004
- Done criteria: dashboard tracks crash-free sessions, BLE drop rate, decode success proxies.

## 10. Documentation and Support Stream

### DOC-001 Setup and quickstart guide
- Priority: `P0`
- Estimate: `S`
- Dependencies: APP-003
- Done criteria: first-time setup guide validated by new user test.

### DOC-002 Radio basics troubleshooting guide
- Priority: `P1`
- Estimate: `M`
- Dependencies: QA-003
- Done criteria: common RF/path/antenna failure modes documented with checks.

### DOC-003 Pairing and security policy doc
- Priority: `P0`
- Estimate: `S`
- Dependencies: FW-012
- Done criteria: clear user/admin pairing and bond reset instructions published.

### DOC-004 Interoperability matrix
- Priority: `P1`
- Estimate: `M`
- Dependencies: INT-003, QA-004
- Done criteria: supported radios, phones, OS versions, and known caveats listed.

### DOC-006 MVP gap analysis and implementation order
- Priority: `P0`
- Estimate: `S`
- Dependencies: PM-003
- Done criteria: current firmware/protocol gaps and implementation order published for agents and maintainers.

### DOC-005 Developer contribution guide
- Priority: `P2`
- Estimate: `S`
- Dependencies: PM-002
- Done criteria: contribution workflow and coding/test standards published.

## 11. HF Variant Stream (Unified Architecture)

### HF-001 HF requirements and supported mode shortlist
- Priority: `P0`
- Estimate: `M`
- Dependencies: PM-003
- Done criteria: first supported workflows and radios selected (narrow matrix).

### HF-002 Radio profile abstraction (firmware)
- Priority: `P0`
- Estimate: `L`
- Dependencies: FW-002, HF-001
- Done criteria: profile model covers connector map, PTT method, CAT parameters, AF gain presets.

### HF-003 Radio profile manager (app)
- Priority: `P0`
- Estimate: `L`
- Dependencies: APP-001, HF-001
- Done criteria: user can select/import profile and run validation wizard.

### HF-004 CAT bridge implementation
- Priority: `P1`
- Estimate: `L`
- Dependencies: HF-002
- Done criteria: at least one HF radio CAT control path functional end-to-end.

### HF-005 PTT method support matrix
- Priority: `P1`
- Estimate: `M`
- Dependencies: HF-002
- Done criteria: GPIO/serial/CAT PTT methods supported with safety checks.

### HF-006 Audio calibration workflow
- Priority: `P0`
- Estimate: `M`
- Dependencies: HF-002, HF-003
- Done criteria: guided AF TX/RX level calibration completes with pass/fail indicators.

### HF-007 BLE audio bridge spike
- Priority: `P0`
- Estimate: `XL`
- Dependencies: HF-002, APP-002
- Done criteria: measured latency/jitter/power results on target phones with conclusion report.

### HF-008 High-rate transport channel prototype
- Priority: `P1`
- Estimate: `L`
- Dependencies: HF-007
- Done criteria: binary-framed channel prototype with observed reliability metrics.

### HF-009 HF beta feature flag and guardrails
- Priority: `P1`
- Estimate: `M`
- Dependencies: HF-003, HF-004
- Done criteria: feature-gated release with explicit beta warnings and diagnostics enabled.

### HF-010 HF field beta program
- Priority: `P1`
- Estimate: `L`
- Dependencies: HF-009
- Done criteria: beta cohort runs scripted tests; issues triaged with severity and reproducibility.

### HF-011 Go/no-go review for production HF audio bridge
- Priority: `P0`
- Estimate: `S`
- Dependencies: HF-007, HF-010
- Done criteria: decision record signed with objective thresholds and rationale.

### HF-012 HF productization backlog split
- Priority: `P1`
- Estimate: `M`
- Dependencies: HF-011
- Done criteria: approved plan for either production path or de-scope path.

## 12. Security and Compliance Stream

### SEC-001 Threat model and abuse cases
- Priority: `P1`
- Estimate: `M`
- Dependencies: FW-012
- Done criteria: documented threat model with mitigations for unauthorized TX/config tampering.

### SEC-002 Secure key and bond handling tests
- Priority: `P1`
- Estimate: `M`
- Dependencies: FW-012
- Done criteria: test coverage for bond reset, pairing windows, and replay resistance.

### SEC-003 Release artifact integrity checks
- Priority: `P2`
- Estimate: `S`
- Dependencies: FW-019
- Done criteria: signed build artifacts with verification in release process.

### SEC-004 Regional configuration safeguards
- Priority: `P1`
- Estimate: `S`
- Dependencies: FW-013
- Done criteria: region presets and user confirmation flow prevent accidental wrong-frequency defaults.

## 13. Initial Sprint Proposal (First 4-6 Weeks)

### Sprint A (Foundation)
- PM-001, PM-002
- HW-001, HW-002, HW-003, HW-004
- FW-001, FW-002, FW-003
- APP-001
- QA-001

### Sprint B (Core data paths)
- HW-005, HW-006, HW-007
- FW-004, FW-005, FW-011, FW-013
- APP-000, APP-002, APP-003, APP-004
- INT-002
- QA-002

### Sprint C (MVP feature complete candidate)
- HW-008, HW-010
- FW-006, FW-007, FW-008, FW-009, FW-010, FW-012, FW-016
- APP-005, APP-006, APP-008
- QA-003, QA-004
- DOC-001, DOC-003

### Sprint D (MVP interop closure)
- FW-015, FW-018
- APP-013
- INT-001, INT-003, INT-004
- DOC-004, DOC-006

## 14. MVP Release Gates (M1 -> M2)
- Gate G1: Functional completeness
  - All `P0` MVP items done or explicitly waived with mitigation.
  - Native BLE and KISS-over-BLE both function against their reference clients.
- Gate G2: Reliability
  - BLE 1-hour continuous RX stability target met on test matrix.
  - Controlled-condition beacon decode success meets acceptance target from test plan.
- Gate G3: Safety and security
  - Bonded/encrypted write protections validated.
  - No unsafe TX failure mode in fault-injection tests.
- Gate G4: Field readiness
  - Field test protocol completed with documented issues and fixes.

## 15. Post-MVP Priority Stack
1. `P1` stabilization and diagnostics (FW-015, APP-010, QA-007)
2. Broader ecosystem expansion beyond first MVP KISS target
3. HF discovery and narrow beta (HF-001 through HF-011)
4. OTA/signing and deeper security hardening (FW-019, SEC-003)

## 16. Open Planning Decisions
- Select Windows desktop app stack for BLE test harness (for example .NET/WinUI or Electron/WebBluetooth fallback).
- Select primary mobile stack and define ownership boundaries between firmware and app teams.
- Confirm exact target phone OS versions for support matrix.
- Confirm first three HF radios for profile certification.
- Define objective thresholds for HF audio bridge viability (latency, jitter, drop rate, battery impact).
