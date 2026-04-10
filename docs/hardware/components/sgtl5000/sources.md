# SGTL5000 Sources

## Datasheet / Product
- NXP SGTL5000 product page
  - https://www.nxp.com/products/audio-and-radio/audio-converters/ultra-low-power-audio-codec%3ASGTL5000
- NXP SGTL5000 datasheet (Rev. 7, 2022-01)
  - https://www.nxp.com/docs/en/data-sheet/SGTL5000.pdf

## Evaluation hardware
- NXP SGTL5000 evaluation kit
  - https://www.nxp.com/products/audio-and-radio/audio-converters/evaluation-kit-sgtl5000-low-power-stereo-codec%3AKITSGTL5000EVBE

## Notes
- SGTL5000 is selected as current project baseline codec for MVP and Rev A planning.
- Ensure `SYS_MCLK` planning is explicit in schematic and firmware clock configuration.
- Mono modem operation should use a single channel with deterministic routing and gain control.
- External `VDDD` is required for new designs (per NXP datasheet typical application note).
