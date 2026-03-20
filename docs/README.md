# Documentation Index

`docs/` is the single top-level documentation home for this repo.

## Project Snapshot
- Current phase: bench prototype verification
- Proven today:
  - codec/radio path boots and bench audio stages run
  - APRS packet TX has been received externally in a supervised setup
  - RX analog/audio path is active, instrumented, and can now be recorded to a PSRAM-backed WAV export
- Still open:
  - on-device APRS RX decode from a trusted Bell 202 source
  - SA818 TX deviation measurement
  - BLE safety/security hardware validation
  - final RX margin closure with the new `16-bit` recorder path

## Main Sections
- `aprs_mvp_docs/` — canonical product, protocol, architecture, QA, and status docs
- `dev_setup.md` — local toolchain, build, and test flow
- `bench_bringup_checklist.md` — bench execution order for the prototype
- `bench_measured_values_template.md` — measurement capture sheet
- `hardware/` — hardware reference library, source links, and CAD asset tracking
- `../firmware/main/bench_profile_config.h` — build-time bench stage selection for targeted debug sessions

## Recommended Entry Points
- Repo orientation: `aprs_mvp_docs/README.md`
- Current implementation status: `aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- Rolling verification record: `aprs_mvp_docs/agent_bootstrap/audit.md`
- Bench work: `bench_bringup_checklist.md`
- Local setup: `dev_setup.md`

## Structure Notes
- `aprs_mvp_docs/` is the canonical spec and project-status tree.
- `hardware/` is the reference-library tree for components and assets.
- Repo-root `hardware/` is different: it contains practical wiring and prototype-planning notes rather than the source-library material.
