# Hardware block diagram

See: `assets/hardware_block_diagram.png`

Notes:
- SGTL5000 is the current codec baseline for MVP/Rev A planning.
- Include explicit I2S `MCLK` routing from ESP32-S3 to SGTL5000 `SYS_MCLK`.
- Use an I2S codec path for stable audio levels and simpler filtering.
- Keep RF module physically separated/shielded from digital sections.
- Provide test points for audio in/out, PTT, UART, and power rails.
