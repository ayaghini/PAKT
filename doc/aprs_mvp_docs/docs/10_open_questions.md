# Open questions

1. Default region(s) for MVP: NA 144.390 vs EU 144.800, and path presets per region.
2. Target enclosure constraints: pocket size, connectors, buttons, and antenna clearance.
3. Audio interface calibration details:
   - exact SA818 AF levels and impedance into SGTL5000 path
   - final AF_TX attenuation and AF_RX filtering values
4. Desired beacon formats and APRS symbol defaults.
5. Should we support Bluetooth LE audio/headsets for PTT? (likely V2)
6. Manufacturing strategy: assembled vs kit
7. Pairing UX details: dedicated pair button, pairing window timeout, and bond reset flow.
8. MAX17048 integration details:
   - alert pin usage vs polling-only fuel gauge reads in MVP firmware
9. Windows desktop app decisions:
   - desktop framework choice for BLE client (for example .NET/WinUI)
   - minimum test UI scope before phone app feature work continues
