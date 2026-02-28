# PAKT

APRS Pocket TNC project workspace.

## Current status
- Project is in design/spec phase.
- Primary implementation docs live under `doc/aprs_mvp_docs/`.
- Hardware prototyping notes live under `hardware/`.

## Start here
- `doc/aprs_mvp_docs/README.md`
- `doc/aprs_mvp_docs/docs/02_mvp_scope.md`
- `doc/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- `hardware/prototyping_wiring.md`

## Repo structure
- `doc/aprs_mvp_docs/docs/`: product, architecture, BLE, firmware, test, and risk docs
- `doc/aprs_mvp_docs/hardware/`: hardware interface docs and placeholder BOM
- `hardware/`: practical prototyping wiring and component rationale

## Notes
- BLE spec now defines explicit UUID mapping and MTU/chunking behavior.
- Security baseline requires encrypted + bonded writes and controlled pairing.
- Wiring guidance includes SA818 supply isolation and TX brownout checks.
