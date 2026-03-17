# Current Software And App Status

This document summarizes how relevant the existing app and firmware are for a
new external integration effort.

## Short answer

The existing desktop app is highly relevant as a protocol reference and test
harness, but it is not the final end-user integration layer. It should be used
as the best working example of how to talk to the device over native BLE.

## Desktop app status

Location:

- `app/desktop_test/`

What it already does:

- scans for PAKT BLE devices
- connects and reconnects
- reads device info and config
- writes config
- sends commands
- sends TX requests
- subscribes to notifications
- reassembles chunked BLE payloads
- parses telemetry and capability payloads
- tracks TX message states
- exports logs and diagnostics

Why it matters for the external agent:

- it is the clearest implementation example of the native PAKT BLE contract
- it already models capability negotiation, chunking, auth error handling, and
  message lifecycle tracking
- its code is easier to reuse conceptually than the firmware for host-side work

Most relevant files:

- `app/desktop_test/pakt_client.py`
- `app/desktop_test/transport.py`
- `app/desktop_test/chunker.py`
- `app/desktop_test/capability.py`
- `app/desktop_test/telemetry.py`
- `app/desktop_test/message_tracker.py`
- `app/desktop_test/main.py`

## Firmware status

Location:

- `firmware/`

What appears software-complete or near-complete:

- BLE UUID layout and GATT table
- BLE chunker
- payload validators
- device config storage
- capability advertisement
- TX scheduler and TX result encoding
- APRS/AX.25/modem support code
- SA818 command formatting/parsing
- GPS parser
- PTT watchdog/safe-off logic

What is still stubbed or hardware-gated in `firmware/main/main.cpp`:

- `audio_task`: stub
- `gps_task`: UART read placeholder
- `power_task`: stub
- `aprs_task`: TX path still uses a success-returning stub instead of a full
  on-hardware RF/audio path
- `Device Command` characteristic handler currently accepts writes but does not
  provide mature command execution behavior

Meaning for the external agent:

- the protocol shape is real enough to integrate against
- not every endpoint is proven on real hardware yet
- the safest target is config, capabilities, TX request/result handling, and
  telemetry/status as they become live

## Relevance assessment for external software integration

### Strongly relevant now

- BLE discovery/connect flow
- capability negotiation
- config read/write
- chunking/reassembly logic
- TX request/TX result lifecycle
- telemetry parsing
- auth/bonding behavior

### Relevant soon but not fully proven on hardware

- RX packet stream
- live device status stream
- end-to-end APRS RF send/receive behavior
- GPS live telemetry from real UART-fed fixes
- power telemetry from live hardware

### MVP target but not yet implemented

- KISS-over-BLE service
- KISS framing/bridge validation

### Not yet a current integration target

- phone app workflows
- OTA update flow

## Test and validation status visible in repo

Test inventory found in repo:

- Python tests in `app/desktop_test`: `25`
- C++ host tests in `firmware/test_host`: `207`
- Total test cases found by source scan: `232`

Repo documentation claims CI coverage for:

- firmware build
- C++ host tests
- Python app tests

Current local verification limits in this workspace:

- `pytest` is not installed here, so Python tests were not executed locally
- `cmake` is not installed here, so C++ host tests were not built or run locally

Because of that, this status package relies on:

- direct code inspection
- repo documentation
- test inventory present in source

## Best practical advice for the external agent

Treat the existing desktop app as the reference client, not as the final
product. Mirror its protocol behavior in the external software's hardware
adapter layer, and keep the adapter narrow so native BLE support and the MVP
KISS-over-BLE path can both be added cleanly.
