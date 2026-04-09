# GPS Sources

This folder name is legacy. The current bench and PCB baseline is the u-blox
`NEO-M9N` class module.

## Official datasheets
- NEO-M9N Data Sheet (u-blox PDF)
  - https://www.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf

## Official application notes / design guidance
- NEO-M9N product and integration landing page
  - https://www.u-blox.com/en/product/neo-m9n-module

## Notes
- Integration guidance is required for RF layout, ground clearance, antenna feed, and backup supply design.
- Rename this folder only when the dedicated KiCad PCB project is created; until then, treat `neo-m8n/` as the repo-local GPS reference bucket for the current `M9N` baseline.
