# Agent Update Audit

Date: 2026-03-09
Scope: review of recently added/updated firmware and desktop test app code, plus CI verification path.

## Refresh Review (new updates observed later on 2026-03-09)

### New findings (ordered by severity)

1. **Critical: host test binary has multiple `main()` definitions**
- Files:
  - `firmware/test_host/test_main.cpp:2`
  - `firmware/test_host/test_tx_scheduler.cpp:6`
  - `firmware/test_host/test_capability.cpp:3`
  - `firmware/test_host/test_telemetry.cpp:5`
- Evidence:
  - `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` is defined in 4 separate translation units.
- Impact:
  - Host tests should fail at link time (multiple `main` symbols).
- Recommendation:
  - Keep the macro only in `test_main.cpp`; remove it from the other test files.

2. **Critical: host tests reference a non-existent constant**
- File: `firmware/test_host/test_tx_scheduler.cpp`
- Evidence:
  - Uses `TxScheduler::kMaxMsgIdStr` in multiple places (for example line 53), but `kMaxMsgIdStr` is defined in `TxMessage.h` namespace scope, not as a `TxScheduler` member.
- Impact:
  - Host test compile failure.
- Recommendation:
  - Replace with `pakt::kMaxMsgIdStr` (or expose an equivalent constant from `TxScheduler`).

3. **High: desktop message state tracking cannot correlate TX results**
- Files:
  - `app/desktop_test/pakt_client.py:209`
  - `app/desktop_test/pakt_client.py:282`
  - `app/desktop_test/message_tracker.py:94`
- Evidence:
  - Sent messages are registered under local keys like `local:{self._msg_id}`.
  - TX result handling looks up by firmware `msg_id` from notify JSON.
  - No mapping step exists from local placeholder ID to firmware-assigned ID.
- Impact:
  - Message queue can remain `PENDING` even when firmware reports `acked`/`timeout`.
- Recommendation:
  - Use firmware-assigned `msg_id` from TX request/write response path, or implement first-TX-result remap from local placeholder to firmware ID.

## Findings (ordered by severity)

1. **Critical: duplicate function definitions in BLE server callbacks**
- File: `firmware/components/ble_services/BleServer.cpp`
- Evidence:
  - `on_config_chunk_complete` is defined at lines 181 and 408.
  - `on_tx_req_chunk_complete` is defined at lines 191 and 416.
- Impact:
  - This is a hard C++ compile/link blocker (redefinition in same translation unit).
  - Firmware build should fail until one definition per function remains.
- Recommendation:
  - Keep only one definition for each callback and remove placeholder bodies.

2. **High: CI app-tests job likely fails due missing `bleak` dependency**
- Files:
  - `.github/workflows/ci.yml:51-54`
  - `app/desktop_test/transport.py:23`
- Evidence:
  - CI installs only `pytest`.
  - `transport.py` imports `from bleak import BleakClient, BleakScanner` at module import time.
  - Tests import `transport` directly (`app/desktop_test/test_app.py:17`), so import fails before mocks are applied if `bleak` is absent.
- Impact:
  - `app-tests` job is fragile/broken in clean CI environments.
- Recommendation:
  - Either install `bleak` in CI, or move/import-guard bleak inside runtime methods and inject mocks cleanly for unit tests.

3. **Medium: slot eviction logic in BLE chunk reassembly appears incorrect**
- File: `firmware/components/ble_services/BleChunker.cpp:130-134`
- Evidence:
  - Oldest-slot selection compares `(s.start_ms - oldest->start_ms) > timeout_ms_`.
  - This does not reliably select the oldest active slot; it compares slot-to-slot delta against timeout, and unsigned wrap can mislead selection.
- Impact:
  - Under slot pressure, eviction can pick wrong slot, potentially dropping newer in-flight messages or causing non-deterministic behavior.
- Recommendation:
  - Select oldest by monotonic age (`now_ms - s.start_ms`), tracking max age among active slots.

4. **Low: text encoding artifacts in comments/log strings**
- Files: several under `app/desktop_test/*` and `firmware/components/ble_services/*`
- Evidence:
  - Rendered characters show mojibake-like sequences in terminal output (e.g., separators/arrows).
- Impact:
  - Primarily readability/maintainability issue; low runtime risk.
- Recommendation:
  - Normalize file encoding and replace decorative Unicode in source comments/CLI text with ASCII where possible.

## Verification Performed

## Static review
- Reviewed key updated files:
  - `app/desktop_test/main.py`
  - `app/desktop_test/pakt_client.py`
  - `app/desktop_test/transport.py`
  - `app/desktop_test/chunker.py`
  - `app/desktop_test/config_store.py`
  - `app/desktop_test/test_app.py`
  - `app/desktop_test/test_chunker.py`
  - `firmware/components/ble_services/BleServer.cpp`
  - `firmware/components/ble_services/BleChunker.cpp`
  - `.github/workflows/ci.yml`

## Runtime verification attempts
- `cmake` not available in current environment (`CommandNotFoundException`), so host C++ tests could not be built/run.
- `pytest` not available in current environment (`No module named pytest`), so Python tests could not be run.
- Attempt to install app requirements timed out/failed due local temp permission issue, so runtime verification remains blocked.
- Python syntax check succeeded for all files under `app/desktop_test` via `python -m py_compile`.

## Overall Assessment

The update introduces useful structure for desktop BLE testing and chunked transport, but there are multiple blocking correctness issues across firmware and tests (BLE callback redefinition, host-test build breakages, and TX-result tracking mismatch). These should be fixed before treating the branch as a stable baseline.
