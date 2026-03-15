# Agent Update Audit

Date: 2026-03-09
Scope: review of recently added/updated firmware and desktop test app code, plus CI verification path.

## Fix Log (2026-03-14)

All findings below have been resolved in the same session. Fixed-by references indicate the changed file.

| # | Severity | Status | Fixed in |
|---|----------|--------|----------|
| R1 | Critical | **FIXED** | `test_tx_scheduler.cpp`, `test_capability.cpp`, `test_telemetry.cpp` |
| R2 | Critical | **FIXED** | `test_tx_scheduler.cpp` |
| R3 | High     | **FIXED** | `message_tracker.py` |
| F1 | Critical | **FIXED** | `BleServer.cpp` |
| F2 | High     | **FIXED** | `.github/workflows/ci.yml` |
| F3 | Medium   | **FIXED** | `BleChunker.cpp` |
| F4 | Low      | deferred – cosmetic only | — |

---

## Fix Details (2026-03-14)

### R1 – Multiple `main()` definitions (Critical) — FIXED
- Removed `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` from `test_tx_scheduler.cpp`, `test_capability.cpp`, and `test_telemetry.cpp`.
- The macro now appears only in `test_main.cpp` (the designated entry-point TU).

### R2 – `TxScheduler::kMaxMsgIdStr` undefined (Critical) — FIXED
- `kMaxMsgIdStr` is defined at namespace scope (`pakt::kMaxMsgIdStr`) in `TxMessage.h`, not as a `TxScheduler` member.
- Replaced all six occurrences of `TxScheduler::kMaxMsgIdStr` with `kMaxMsgIdStr` in `test_tx_scheduler.cpp`; `using namespace pakt;` already in scope.

### R3 – TX result tracking cannot correlate firmware msg_id (High) — FIXED
- Added `MessageTracker._remap_placeholder(firmware_msg_id)` in `message_tracker.py`.
- When `on_tx_result` receives a `msg_id` not in `_messages`, it remaps the oldest pending `local:N` placeholder entry to the firmware-assigned ID, then processes normally.
- All subsequent `acked`/`timeout`/`error` notifications for that message are now resolved correctly.

### F1 – Duplicate function definitions in BleServer.cpp (Critical) — FIXED
- Removed the stub bodies of `on_config_chunk_complete` (old lines 181–189) and `on_tx_req_chunk_complete` (old lines 191–194) that appeared immediately after the chunker globals.
- The forward declarations at lines 175–176 are retained; the real implementations later in the file (`on_config_chunk_complete` forwarding to `handlers_.on_config_write`, `on_tx_req_chunk_complete` forwarding to `handlers_.on_tx_request`) are the sole definitions.

### F2 – CI missing `bleak` dependency (High) — FIXED
- Changed the `app-tests` CI step from `pip install pytest` to `pip install -r app/desktop_test/requirements.txt`.
- `requirements.txt` already declares `bleak>=0.22.0` and `pytest>=8.0.0`, so no new files were needed.

### F3 – Slot eviction wrong comparison in BleChunker (Medium) — FIXED
- Old: `if ((s.start_ms - oldest->start_ms) > timeout_ms_)` — compares slot-to-slot delta against timeout, not actual age.
- New: `if ((now_ms - s.start_ms) > (now_ms - oldest->start_ms))` — compares monotonic ages of each slot; uint32_t subtraction handles wrap correctly for ages < 2³¹ ms.

### F4 – Text encoding artifacts (Low) — deferred
- Cosmetic issue in comments and CLI log strings. No runtime impact. Deferred to a housekeeping pass.

---

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
