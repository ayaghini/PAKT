# MAX17048 Sources

## OEM references
- Analog Devices product page
  - https://www.analog.com/en/products/max17048.html
- Analog Devices datasheet PDF
  - https://www.analog.com/media/en/technical-documentation/data-sheets/max17048-max17049.pdf

## OEM application / design guidance
- Analog Devices technical article: Characterizing a Battery for Use with a Fuel Gauge
  - https://www.analog.com/en/resources/technical-articles/battery-gauge--maxim-integrated.html

## Bench-board reference
- Adafruit Learn guide: Adafruit ESP32-S3 Feather
  - https://learn.adafruit.com/adafruit-esp32-s3-feather/overview
- Adafruit Learn downloads page
  - https://learn.adafruit.com/adafruit-esp32-s3-feather/downloads

## Project note
- `MAX17048` is the locked fuel-gauge baseline for the custom PCB.
- The bench board is the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM`.
- Adafruit's current public Learn guide for that board identifies `MAX17048` as the battery gauge.
- Datasheet capture notes for schematic:
  - `VDD` bypass capacitor `0.1 uF` to GND is required.
  - `QSTRT` should be tied to GND when unused.
  - `ALRT`/`SDA`/`SCL` require system pullups when used.
