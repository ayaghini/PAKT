# Hardware Reference Library

This folder holds the hardware references that feed schematic capture and PCB
layout. The active baseline now follows the confirmed bench stack rather than
earlier option-screening documents.

## Active hardware baseline
- MCU: `ESP32-S3-WROOM-1` (PCB antenna).
- Radio: `SA818S`.
- Audio codec: `SGTL5000XNLA3`.
- GPS layout baseline: u-blox `NEO-M8N/M8M` compatible footprint (`ublox_NEO`), with module choice finalized later.
- Fuel gauge: `MAX17048G+T10`.
- Charger: `MCP73831T-2ACI/OT`.
- GPS backup battery: `Panasonic BR-1225A/FAN` (non-rechargeable primary cell).

## Reference-design anchor
For battery charging and battery gauging, the custom PCB should follow the
bench-board power behavior of the Adafruit ESP32-S3 Feather baseline, because
that was used for bench validation and aligns with the `MAX17048` gauge path.

## Folder structure
- `components/README.md` - active vs archived component folders.
- `components/<part>/sources.md` - OEM datasheets, application notes, and reference-design links.
- `components/<part>/cad_assets.md` - symbol, footprint, package, and model mapping for KiCad.
- `components/<part>/symbols/` - local KiCad symbol libraries when needed.
- `components/<part>/footprints/` - local KiCad footprint files when needed.
- `components/<part>/step/` - STEP models if they are stored in-repo.

## Optional-components review flow
- Current plan: `hardware/PCB/rev01/reviews/HARDWARE_OPTIONAL_COMPONENT_PLAN.md`
- Completed reviews:
  - `hardware/PCB/rev01/reviews/GPS_OPTIONAL_COMPONENT_REVIEW.md`
  - `hardware/PCB/rev01/reviews/SA818_OPTIONAL_COMPONENT_REVIEW.md`

## Source policy
- Prefer OEM product pages, datasheets, and application notes.
- Keep breakout-board reference links separate from semiconductor OEM links.
- Mark legacy or mirror references clearly.

Current asset status is tracked in:
- `components/CAD_ASSET_STATUS.md`
