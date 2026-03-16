# PAKT

APRS Pocket TNC project workspace.

## Current status
- Project is in active implementation and pre-hardware bring-up.
- Software foundations are largely in place (firmware components, host tests, desktop test app).
- Remaining high-risk items are hardware validation paths (SA818 electrical/audio, full APRS RF E2E, endurance gates).

## Start here
- `doc/aprs_mvp_docs/README.md`
- `doc/aprs_mvp_docs/agent_bootstrap/implementation_steps_mvp.md`
- `doc/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- `doc/aprs_mvp_docs/agent_bootstrap/audit.md`
- `doc/aprs_mvp_docs/docs/02_mvp_scope.md`
- `doc/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- `doc/aprs_mvp_docs/payload_contracts.md`
- `docs/dev_setup.md`
- `docs/bench_bringup_checklist.md`
- `hardware/prototyping_wiring.md`
- `hardware/prototype_breakout_wiring_plan.md`

## Repo structure
- `doc/aprs_mvp_docs/docs/`: product, architecture, BLE, firmware, test, and risk docs
- `doc/aprs_mvp_docs/hardware/`: hardware interface docs and placeholder BOM
- `doc/aprs_mvp_docs/agent_bootstrap/`: implementation sequencing, QA gates, audit log
- `hardware/`: practical prototyping wiring and component rationale
- `docs/`: developer setup and bench bring-up procedures

## Notes
- Treat `payload_contracts.md` as JSON wire-format source of truth.
- Keep `05_ble_gatt_spec.md` and quickstart examples aligned with payload contracts.
- Use `agent_bootstrap/audit.md` as the rolling implementation and risk ledger.
