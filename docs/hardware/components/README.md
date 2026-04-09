# Hardware Component Library

This folder is the hardware source-of-truth library for PCB capture.

## Active component set
- `esp32-s3/` — ESP32-S3 module references used for the Feather-class MCU baseline.
- `sa818/` — SA818-V radio module references and imported CAD assets.
- `sgtl5000/` — SGTL5000 codec references and CAD assets for the bench-proven audio path.
- `neo-m8n/` — current GPS baseline is the u-blox `NEO-M9N` breakout/module; this folder name is legacy and will be renamed only when the schematic tree is moved to a dedicated PCB project.
- `max17048/` — active fuel-gauge baseline for PCB capture.
- `mcp73831/` — active single-cell charger baseline for PCB capture.

## Archived legacy references
- `max17043/` — archived alternate; not part of the current PCB baseline.
- `wm8960/` — archived codec alternate; not part of the current PCB baseline.

## How to use this library
1. Start with each component folder's `sources.md` and `cad_assets.md`.
2. Use only the active component set above for the first KiCad schematic.
3. Treat archived folders as historical context only.
4. If a standard KiCad library part is preferred over a vendored local copy, record that explicitly in `cad_assets.md`.

## Important power-reference note
The battery and gauge reference for this project is the Adafruit ESP32-S3
Feather with `4MB Flash / 2MB PSRAM`, because that is the board used on the
bench and Adafruit's current guide for that board identifies `MAX17048` as the
fuel gauge.
