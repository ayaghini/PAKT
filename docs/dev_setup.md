# PAKT Developer Setup Guide

Covers: host unit tests, Python desktop tests, and ESP-IDF firmware build.
No hardware required for the first three sections.

This file is the practical setup companion to the canonical spec/status tree in
`docs/aprs_mvp_docs/`. For protocol or implementation-status questions, prefer:
- `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`
- `docs/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- `docs/aprs_mvp_docs/agent_bootstrap/audit.md`

Generated build directory note:
- `firmware/build_*` is the intended location for firmware and host-test build outputs.
- Repo-root `build_feather_s3/` is also an ESP-IDF-generated build tree for the same `firmware/` project, but it is a separate output directory rather than a second source tree.

---

## 1. Prerequisites

Important toolchain split:

- Firmware target (`firmware/`) is built through ESP-IDF using `idf.py`.
- Raw CMake is not the direct developer entrypoint for firmware builds.
- Raw CMake is used for the host-only test target in `firmware/test_host`.

### 1.1 Host unit tests (C++, CMake)

| Tool | Minimum version | Notes |
|------|-----------------|-------|
| CMake | 3.16 | FetchContent used for doctest |
| C++ compiler | GCC 10 / Clang 12 / MSVC 2019 (v16.11) | C++17 required |
| Ninja (optional) | any | faster builds |
| Git | any | doctest fetched via FetchContent |

### 1.2 Python desktop tests

| Tool | Minimum version |
|------|-----------------|
| Python | 3.10 |
| pip | any |

### 1.3 ESP-IDF firmware build (no hardware needed)

| Tool | Version | Notes |
|------|---------|-------|
| ESP-IDF | v5.3.2 | exact version used in CI; other v5.x may work |
| idf.py | bundled with ESP-IDF | |

Install ESP-IDF following the official guide for your OS. After install, `idf.py` must be on PATH or activated via `. $IDF_PATH/export.sh`.

---

## 2. Host unit tests

```bash
# From repo root:
cmake -S firmware/test_host -B build/test_host -DCMAKE_BUILD_TYPE=Release
cmake --build build/test_host
./build/test_host/pakt_tests --reporters=console --no-intro
```

On Windows (MSVC or MinGW):
```powershell
cmake -S firmware/test_host -B build/test_host -DCMAKE_BUILD_TYPE=Release
cmake --build build/test_host --config Release
.\build\test_host\Release\pakt_tests.exe --reporters=console --no-intro
```

Run a single test file's tests by filtering:
```bash
./build/test_host/pakt_tests -tc="GPRMC*" --reporters=console
```

Expected output: `[doctest] test cases: 200+ | 200+ passed | 0 failed`

---

## 3. Python desktop tests

```bash
cd app/desktop_test
python -m venv .venv
# Linux/macOS:
source .venv/bin/activate
# Windows:
.venv\Scripts\activate

pip install -r requirements.txt
pytest -v
```

Expected output: all tests pass. BLE transport tests use mocks (no hardware needed).

---

## 4. Firmware build (ESP-IDF, no hardware)

Use ESP-IDF's `idf.py` for firmware builds. Although ESP-IDF uses CMake under
the hood, developers and agents should treat `idf.py` as the supported command
surface for the firmware target.

```bash
# Activate ESP-IDF environment (adjust path to your install):
. ~/esp/esp-idf/export.sh        # Linux/macOS
# or:
%USERPROFILE%\esp\esp-idf\export.bat   # Windows CMD

cd firmware
idf.py set-target esp32s3
idf.py build
```

Build output: `firmware/build/pakt.bin`, `pakt.elf`, `pakt.map`.
No device connection is needed for a build-only check.

### 4.1 Bench/debug stage selection

The prototype firmware now supports build-time selection of which blocking bench
stages run at boot. Edit:

- `firmware/main/bench_profile_config.h`

This file controls:

- top-level benches: `audio_bench`, `sa818_bench`, `aprs_bench`
- APRS sub-stages: Stage 0 loopback, Stage A TX burst, Stage B RX gain sweep,
  PCM snapshot logging, and Stage C full RX recorder/export
- Stage C ADC gain step for targeted capture runs

Recommended workflow for focused debugging:

1. Edit `firmware/main/bench_profile_config.h`
2. Disable benches you do not want for this session
3. Rebuild with `idf.py build`
4. Flash and monitor the targeted run only

This is the preferred path for prototype debug work instead of commenting bench
code in `main.cpp`.

---

## 5. CI pipeline (reference)

The GitHub Actions CI runs three jobs on every push/PR to `main`:

| Job | Command | What it checks |
|-----|---------|----------------|
| `firmware-build` | `idf.py build` | Full ESP-IDF build, no warnings in touched modules |
| `host-tests` | CMake + CTest | 200+ C++ unit tests (ax25, aprs, modem, BLE chunker, TxScheduler, telemetry, GPS, payload validator, TX integration, PTT watchdog/PttController, config store, SA818 driver) |
| `app-tests` | `pytest` | Python tests for chunker, BLE transport FSM, config store, message tracker, telemetry parser |

See `.github/workflows/ci.yml` for full configuration.

---

## 6. Component dependency map

```
main
|- aprs_fsm         <- TxScheduler, TxResultEncoder, AprsTaskContext
|  \- payload_codec <- PayloadValidator, DeviceConfigStore (+ config_to_json)
|- safety_watchdog  <- PttWatchdog, PttController
|- radio_sa818      <- ISa818Transport, Sa818CommandFormatter, Sa818ResponseParser, Sa818Radio
|- ble_services     <- BleServer, BleChunker
|- gps              <- NmeaParser
|- telemetry        <- GpsTelem, BattTelem, SysTelem
|- capability       <- DeviceCapabilities
|- aprs             <- Aprs frame encoder/decoder
|  \- ax25         <- AX.25 framing
\- modem           <- AfskModulator, AfskDemodulator
```

`payload_codec`, `safety_watchdog`, and `radio_sa818` have no component dependencies (pure C++).
All others depend on `payload_codec` transitively through `aprs_fsm`.

---

## 7. Useful build targets

```bash
# Run only GPS parser tests:
./build/test_host/pakt_tests -tc="GPRMC*" -tc="GPGGA*" -tc="feed*"

# Run only payload validator tests (42 tests):
./build/test_host/pakt_tests -tc="config*" -tc="tx_request*"

# Run only TX integration tests (26 tests):
./build/test_host/pakt_tests -tc="TxResultEncoder*" -tc="AprsTaskContext*"

# Python: run only chunker tests:
pytest app/desktop_test/test_chunker.py -v

# Python: run all tests with verbose output:
pytest app/desktop_test/ -v

# Python: run only messaging tracker tests:
pytest app/desktop_test/test_messaging.py -v

# Run only PTT watchdog + PttController tests (21 tests):
./build/test_host/pakt_tests -tc="PttWatchdog*" -tc="PttController*" --reporters=console

# Run only config store tests (7 tests, includes config_to_json):
./build/test_host/pakt_tests -tc="DeviceConfigStore*" --reporters=console

# Run only SA818 driver tests (18 tests):
./build/test_host/pakt_tests -tc="Sa818*" --reporters=console
```
