# MCP73831 Sources

## OEM references
- Microchip product page
  - https://www.microchip.com/en-us/product/MCP73831
- Microchip datasheet landing page
  - https://www.microchip.com/en-us/product/MCP73831#document-table

## OEM application notes
- AN1149: Battery Charger System Design for a Single-Cell Li-Ion Battery
  - https://www.microchip.com/en-us/application-notes/an1149
- AN947: Power Management in Portable Applications: Charging Lithium-Ion/Lithium-Polymer Batteries
  - https://www.microchip.com/en-us/application-notes/an947

## Bench-board reference
- Adafruit Learn guide: Adafruit ESP32-S3 Feather
  - https://learn.adafruit.com/adafruit-esp32-s3-feather/overview

## Project notes
- The project baseline is `MCP73831T-2ACI/OT`.
- Use the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` as the bench-board reference when reviewing the charger section.
- Keep `5.1 kOhm` as the starting `PROG` resistor assumption unless the battery charge-current target changes during sourcing review.
- Pull the exact datasheet PDF revision from the Microchip product page before fab release.
