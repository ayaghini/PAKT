# Maintenance Guide And Source Map

This package is intended to stay maintainable as hardware and BLE behavior
change. Use this file to know where to update the source docs.

## Authoritative sources by topic

### Payload JSON contracts

Primary source:

- `docs/aprs_mvp_docs/payload_contracts.md`

Code that should stay aligned:

- `firmware/components/payload_codec/`
- `firmware/components/telemetry/`
- `firmware/components/capability/`
- `app/desktop_test/pakt_client.py`
- `app/desktop_test/telemetry.py`
- `app/desktop_test/capability.py`
- `app/desktop_test/message_tracker.py`

### BLE UUIDs and GATT layout

Primary sources:

- `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- `firmware/components/ble_services/include/pakt/BleUuids.h`
- `firmware/components/ble_services/BleServer.cpp`

### Actual implementation status

Primary sources:

- `firmware/main/main.cpp`
- `docs/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- `docs/aprs_mvp_docs/agent_bootstrap/audit.md`

### Hardware interfaces and assumptions

Primary sources:

- `docs/aprs_mvp_docs/hardware/interfaces.md`
- `hardware/prototyping_wiring.md`
- `hardware/prototype_breakout_wiring_plan.md`
- `docs/bench_bringup_checklist.md`

### Third-party interoperability direction

Primary sources:

- `docs/aprs_mvp_docs/docs/15_interoperability_matrix.md`
- `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`

## Known doc drift to watch

### Device Capabilities payload example drift

Current issue:

- `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md` shows an older simple example
  for Device Capabilities
- firmware and desktop app actually use:
  `{"fw_ver","hw_rev","protocol","features":[...]}`

Rule:

- when updating capability behavior, update both the GATT spec example and
  `payload_contracts.md`

### Spec versus implemented endpoint maturity

Current issue:

- some characteristics are fully specified, but the live firmware production
  path is still stubbed or hardware-gated

Rule:

- whenever an endpoint moves from reserved/stubbed to live, update:
  - this handoff package
  - `gate_pass_matrix.md`
  - `payload_contracts.md` if payload shape changed
  - `05_ble_gatt_spec.md` if behavior/property/security changed

## Update checklist when hardware or BLE changes

If UUIDs, services, properties, or security change:

1. update `firmware/components/ble_services/include/pakt/BleUuids.h`
2. update `firmware/components/ble_services/BleServer.cpp`
3. update `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
4. update this package's `02_native_ble_protocol.md`
5. update the desktop test app if host behavior changed

If payload fields change:

1. update `docs/aprs_mvp_docs/payload_contracts.md`
2. update firmware serializers/parsers/validators
3. update desktop app parsers
4. update/add tests
5. update this package's `02_native_ble_protocol.md`

If hardware wiring or internal buses change:

1. update `docs/aprs_mvp_docs/hardware/interfaces.md`
2. update wiring docs under `hardware/`
3. update `docs/bench_bringup_checklist.md`
4. update this package's `01_onboarding_brief.md` if the integration-relevant
   hardware model changed

If project maturity/status changes:

1. update `docs/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
2. update `docs/aprs_mvp_docs/agent_bootstrap/audit.md`
3. update this package's `03_current_software_and_app_status.md`

## Suggested handoff note for the next agent

Use native PAKT BLE as the richer device integration path and treat
KISS-over-BLE as an MVP interoperability path whose software stack is in place
but whose live hardware validation is still pending. Read capabilities first,
follow payload contracts, implement chunking, and treat hardware-validated
behavior separately from software-complete but still hardware-gated paths.
