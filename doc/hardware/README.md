# Hardware Reference Library

This folder organizes datasheet and application-note sources for the project's major hardware components.

Note: direct PDF download from this shell environment is blocked by network policy, so this library currently stores curated source links and retrieval instructions.

## Components
- `components/esp32-s3`
- `components/sa818`
- `components/sgtl5000` (current codec baseline)
- `components/wm8960` (legacy reference only)
- `components/neo-m8n`
- `components/max17048` (current fuel-gauge baseline)
- `components/max17043` (legacy alternate)
- `components/mcp73831`

## Source policy
- Prefer official manufacturer URLs.
- If official PDF cannot be resolved, include a mirror and flag it as non-authoritative.

## CAD asset structure
Each component folder now includes:
- `cad_assets.md` for symbol/footprint/STEP tracking and verification notes.
- `symbols/` for KiCad symbol libraries (`.kicad_sym`).
- `footprints/` for KiCad footprint libraries (`.pretty` / `.kicad_mod`).
- `step/` for 3D models (`.step` / `.stp`).

Current import status and exact/provisional mapping is tracked in:
- `components/CAD_ASSET_STATUS.md`
