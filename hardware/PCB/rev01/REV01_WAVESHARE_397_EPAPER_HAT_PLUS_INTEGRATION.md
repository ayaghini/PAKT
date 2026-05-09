# REV01 Waveshare 3.97inch e-Paper HAT+ Integration

## Sources
- Product: https://www.waveshare.com/3.97inch-e-paper-hat-plus.htm?sku=31063
- Manual: https://www.waveshare.com/wiki/3.97inch_e-Paper_HAT%2B_Manual
- Schematic PDF: https://files.waveshare.com/wiki/3.97inch_e-Paper_HAT%2B_G/3.97inch_e-Paper_HAT%2B.pdf

## Required wiring (HAT 40-pin to ESP32)

| HAT physical pin | HAT net/signal | Connect to Rev01 |
|---:|---|---|
| 1 | 3V3 | `+3.3V` |
| 2 | 5V | `+5V` (optional; do not use if running full 3.3V domain only) |
| 6 | GND | `GND` |
| 11 | RST | `EPD_RST` -> ESP32 IO36 |
| 12 | PWR | `EPD_PWR_EN` -> ESP32 IO38 |
| 18 | BUSY | `EPD_BUSY` -> ESP32 IO37 |
| 19 | DIN (MOSI) | `EPD_DIN` -> ESP32 IO14 |
| 22 | DC | `EPD_DC` -> ESP32 IO35 |
| 23 | SCLK | `EPD_SCLK` -> ESP32 IO16 |
| 24 | CS (CE0) | `EPD_CS` -> ESP32 IO21 |
| 25 | GND | `GND` |

Recommended extra grounds for return-path quality: also tie any of 9/14/20/30/34/39 to GND plane.

## Suggested schematic symbol + footprint

- Symbol: `Connector_Generic:Conn_02x20_Odd_Even`
- Footprint: `Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical`

### Suggested purchasable connector MPNs
- `Sullins PPTC202LFBN-RC` (2x20 female, THT, 2.54mm)
- Any equivalent 2x20 stackable female header, 2.54mm pitch, long-tail style if you need passthrough/holder behavior.

### Buy links
- Sullins `PPTC202LFBN-RC` (Digi-Key): https://www.digikey.com/en/products/detail/sullins-connector-solutions/PPTC202LFBN-RC/810164
- Waveshare display product page: https://www.waveshare.com/3.97inch-e-paper-hat-plus.htm?sku=31063

## Mechanical dimensions for PCB placement

Confirmed from Waveshare manual:
- Driver board size: `99.50 mm x 60.00 mm`
- Display size: `86.40 mm x 51.84 mm`

Mounting-hole data status:
- The provided manual/schematic do not include explicit hole XY coordinates for this HAT+ board.
- If you need exact hole placement for direct screw alignment, measure from a physical board or request vendor CAD/GERBER.
- Conservative default for placeholder footprints:
  - Hole drill: `3.2 mm` (M3 clearance)
  - Courtyard/keepout diameter: `6.0 mm` minimum

## Firmware pin defines (Rev01 proposal)

```c
#define EPD_DIN_PIN   14
#define EPD_SCLK_PIN  16
#define EPD_CS_PIN    21
#define EPD_DC_PIN    35
#define EPD_RST_PIN   36
#define EPD_BUSY_PIN  37
#define EPD_PWR_PIN   38
```
