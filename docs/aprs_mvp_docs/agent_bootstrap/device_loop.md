# Device Loop (Connected ESP32-S3)

Use this when a real board is connected over USB and the agent is expected to:
- build
- flash/upload
- monitor logs
- run bench verification checks

## 1) Preconditions
- Board connected over USB data cable.
- Correct driver installed (USB CDC/serial visible in OS).
- Exactly one expected target board connected, or explicit port selected.
- Firmware toolchain installed for your stack:
  - ESP-IDF (`idf.py`)
  - PlatformIO (`pio`)
  - Arduino CLI (`arduino-cli`)

## 2) Select command profile
Choose one profile and keep it stable for the session.

### Profile A: ESP-IDF
- Build: `idf.py build`
- Flash: `idf.py -p <PORT> flash`
- Monitor: `idf.py -p <PORT> monitor`
- Flash+monitor: `idf.py -p <PORT> flash monitor`

### Profile B: PlatformIO
- Build: `pio run`
- Upload: `pio run -t upload --upload-port <PORT>`
- Monitor: `pio device monitor --port <PORT> --baud <BAUD>`
- Upload+monitor: `pio run -t upload --upload-port <PORT> && pio device monitor --port <PORT> --baud <BAUD>`

### Profile C: Arduino CLI
- Compile: `arduino-cli compile --fqbn <FQBN> <SKETCH_OR_PROJECT>`
- Upload: `arduino-cli upload -p <PORT> --fqbn <FQBN> <SKETCH_OR_PROJECT>`
- Monitor: `arduino-cli monitor -p <PORT> -c baudrate=<BAUD>`

## 3) Port discovery and lock

Windows (PowerShell):
```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
```

Linux:
```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

macOS:
```bash
ls /dev/cu.* 2>/dev/null
```

Rules:
- If multiple ports match, do not guess. Set `<PORT>` explicitly.
- Record selected port in step evidence.

## 4) Agent execution loop (connected hardware)
1. Build firmware.
2. Flash firmware.
3. Open monitor and capture boot + app startup logs.
4. Run one focused verification scenario for the active step.
5. If failure:
   - capture error signature
   - classify (build/flash/boot/runtime/protocol)
   - apply smallest fix
   - repeat loop
6. Store summary evidence before closing step.

## 5) Minimum verification checks after each flash
- Board boots without reset loop.
- App reaches expected startup state.
- `PTT` default state is safe (`off`).
- BLE advertising is visible (confirm with phone or BLE scanner tool).
- No new critical runtime errors in logs for touched modules.

## 6) Troubleshooting quick map
- `Port not found`:
  - reconnect cable
  - verify data-capable USB cable
  - re-run port discovery
- `Permission denied / busy port`:
  - close serial monitors
  - ensure one uploader/monitor process only
- `Flash timeout / sync failed`:
  - lower upload speed
  - hold BOOT if needed during reset/flash
- `Boot loop after flash`:
  - capture first boot log lines
  - revert recent low-level init changes (clocking/pin config/power init)
  - if NVS corruption suspected: add `--erase-flash` flag before reflashing (`idf.py -p <PORT> erase-flash flash` or `pio run -t erase --upload-port <PORT>` then re-upload)
- `Runtime unstable with BLE/audio`:
  - reduce notify rate
  - inspect task starvation and buffer underruns

## 7) Evidence format for connected runs
- Selected profile: `idf.py` / `pio` / `arduino-cli`
- Port and baud:
- Build command + result:
- Flash command + result:
- Monitor excerpt (startup + key checks):
- Verification scenario executed:
- Outcome (pass/fail) + residual risks:
