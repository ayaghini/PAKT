# PAKT Agent Integration Package

This folder is the handoff package for an external engineering agent that needs
to add support for PAKT hardware into an existing APRS-capable software stack.

Read in this order:

1. `01_onboarding_brief.md`
2. `02_native_ble_protocol.md`
3. `03_current_software_and_app_status.md`
4. `04_maintenance_and_source_map.md`

What this package is for:

- onboard an agent quickly to the PAKT project
- describe the current hardware and software shape
- document the native BLE integration contract the external software should use
- separate implemented behavior from planned/future behavior
- show where to update docs later if hardware, BLE, or payloads change

Important guardrails:

- Treat native PAKT BLE and KISS-over-BLE as the two MVP integration surfaces.
- Treat `docs/aprs_mvp_docs/payload_contracts.md` as the payload source of truth.
- Treat `firmware/main/main.cpp` and `firmware/components/*` as the truth for
  what is actually implemented versus stubbed.
