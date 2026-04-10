# GPS Backup Battery Sources

## Selected battery (rev01 default)
- Panasonic `BR-1225A/FAN` (3 V, 48 mAh, tab-mount primary lithium coin cell)
  - Product listing:
    - https://www.digikey.com/en/products/detail/panasonic-energy/BR-1225A-FAN/447501
  - Datasheet page:
    - https://www.digikey.com/htmldatasheets/production/3718077/0/0/1/br-1225a-fan.html

## GPS backup-domain references
- u-blox NEO-M9N Integration Manual (backup battery and minimal design guidance)
  - https://content.u-blox.com/sites/default/files/NEO-M9N_Integrationmanual_UBX-19014286.pdf
- u-blox NEO-M9N datasheet
  - https://www.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf

## Notes
- `BR-1225A/FAN` is non-rechargeable. Do not implement any charge path into the battery node.
- Keep isolation diode + DNP fallback link strategy in schematic so `V_BCKP` source can be switched without PCB respin.
