# Hardware Reference Library

This folder holds the hardware references that feed schematic capture and PCB
layout. The active baseline now follows the confirmed bench stack rather than
earlier option-screening documents.

## Active hardware baseline
- MCU: Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` as the bench-board baseline.
- Radio: `SA818-V`.
- Audio codec: `SGTL5000`.
- GPS: u-blox `NEO-M9N` breakout/module baseline.
- Fuel gauge: `MAX17048`.
- Charger: `MCP73831T-2ACI/OT` topology baseline.

## Reference-design anchor
For battery charging and battery gauging, the custom PCB should follow the
bench-board power behavior of the Adafruit ESP32-S3 Feather with `4MB Flash /
2MB PSRAM`, because that is the board used on the bench and it matches the
desired `MAX17048` fuel-gauge path.

## Folder structure
- `components/README.md` — active vs archived component folders.
- `components/<part>/sources.md` — OEM datasheets, application notes, and reference-design links.
- `components/<part>/cad_assets.md` — symbol, footprint, package, and model mapping for KiCad.
- `components/<part>/symbols/` — local KiCad symbol libraries when needed.
- `components/<part>/footprints/` — local KiCad footprint files when needed.
- `components/<part>/step/` — STEP models if they are stored in-repo.

## Source policy
- Prefer OEM product pages, datasheets, and application notes.
- Keep breakout-board reference links separate from semiconductor OEM links.
- Mark legacy or mirror references clearly.

Current asset status is tracked in:
- `components/CAD_ASSET_STATUS.md`
