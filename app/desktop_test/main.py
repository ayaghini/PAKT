#!/usr/bin/env python3
# main.py – PAKT Desktop BLE Test Tool (APP-000, APP-008)
#
# Interactive CLI for:
#   - Scanning and connecting to a PAKT APRS TNC over BLE
#   - Exercising all GATT paths (read, write, notify)
#   - Exporting a timestamped session log
#
# Usage:
#   pip install bleak
#   python main.py
#
# Pairing / bonding (APP-008):
#   On first connect, Windows will show a system pairing dialog.
#   If a write is rejected (AUTH_ERR), the device is connected but not bonded.
#   Resolution:
#     1. Open Windows Bluetooth settings.
#     2. Remove ("Forget") the PAKT-TNC device.
#     3. Disconnect and reconnect from this tool — pairing dialog will reappear.
#   The firmware requires LE Secure Connections with bonding before accepting
#   any writes to config, command, or TX request characteristics.

from __future__ import annotations

import asyncio
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

from config_store import ConfigStore
from message_tracker import MsgState
from pakt_client import PaktClient
from transport import State

# ── Session log ───────────────────────────────────────────────────────────────

_log_entries: list[str] = []


def _on_event(direction: str, name: str, payload: str) -> None:
    ts   = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
    line = f"{ts}  {direction:<14}  {name:<22}  {payload}"
    _log_entries.append(line)

    # Colour-code key event types for readability.
    prefix = {
        "AUTH_ERR":  "  [!] ",
        "ERROR":     "  [!] ",
        "RECONNECT": "  [~] ",
        "TRANSPORT": "  [~] ",
        "NOTIFY":    "  [N] ",
    }.get(direction, "      ")
    print(prefix + line)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _state_label(state: State) -> str:
    return {
        State.IDLE:         "disconnected",
        State.SCANNING:     "scanning…",
        State.CONNECTING:   "connecting…",
        State.CONNECTED:    "connected",
        State.RECONNECTING: "reconnecting…",
        State.ERROR:        "ERROR (reconnect failed)",
    }.get(state, state.name)


def _menu(client: PaktClient) -> None:
    print()
    print("── PAKT Desktop BLE Test Tool ─────────────────────────────")
    print(f"   Status: {_state_label(client.state)}")
    if client.is_connected:
        print(f"   MTU:    {client.mtu} bytes  "
              f"(payload/chunk: {max(1, client.mtu - 6)} B)")
        print()
        print("  [1]  Read device info (DIS)")
        print("  [2]  Read config")
        print("  [3]  Write config (JSON)")
        print("  [4]  Show cached config")
        print("  [5]  Diff staged vs cached config")
        print("  [6]  Send command (JSON)")
        print("  [7]  Send TX request (JSON)")
        print("  [8]  Listen for notifications (30 s)")
        print("  [9]  Show message queue")
        print("  [T]  Show telemetry snapshot")
        print("  [X]  Export diagnostics report")
        print("  [D]  Disconnect")
    else:
        print()
        print("  [S]  Scan for PAKT devices")
        print("  [C]  Connect by address")
        print("  [4]  Show cached config (offline)")
    print("  [E]  Export session log")
    print("  [Q]  Quit")
    print("───────────────────────────────────────────────────────────")


async def _pick_device(client: PaktClient) -> str | None:
    found = await client.scan(timeout=8.0)
    if not found:
        print("  No PAKT devices found.")
        return None
    print()
    for i, (name, addr) in enumerate(found):
        print(f"  [{i}]  {name}  ({addr})")
    raw = input("  Select [0]: ").strip() or "0"
    try:
        return found[int(raw)][1]
    except (ValueError, IndexError):
        print("  Invalid selection.")
        return None


def _export_log() -> None:
    if not _log_entries:
        print("  Nothing to export.")
        return
    ts   = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    path = Path(f"pakt_session_{ts}.log")
    path.write_text("\n".join(_log_entries) + "\n", encoding="utf-8")
    print(f"  Log exported → {path.resolve()}")


def _show_cached_config(store: ConfigStore) -> None:
    data = store.load()
    if data is None:
        print("  No cached config found.")
        return
    print(f"  Cached at:  {data.get('updated_utc', '?')}")
    print(f"  Device:     {data.get('device_address', '?')}")
    print(f"  Source:     {data.get('source', '?')}")
    print()
    print(json.dumps(data.get("config", {}), indent=4))


def _prompt_json(example: str, prompt: str = "JSON: ") -> str:
    print(f"  e.g. {example}")
    return input(f"  {prompt}").strip()


def _print_pairing_help() -> None:
    print()
    print("  ┌─ Pairing required ──────────────────────────────────────────────┐")
    print("  │ The device rejected the write — the BLE link is not bonded.     │")
    print("  │ To fix:                                                         │")
    print("  │  1. Open Windows Bluetooth settings.                            │")
    print("  │  2. Select PAKT-TNC → 'Remove device'.                         │")
    print("  │  3. Press [D] to disconnect, then [S] or [C] to reconnect.     │")
    print("  │     Windows will show the pairing dialog on reconnect.          │")
    print("  └─────────────────────────────────────────────────────────────────┘")
    print()


def _show_telemetry(client: PaktClient) -> None:
    ds = client.diagnostics
    print()
    print("  ── Telemetry snapshot ──────────────────────────────────")
    if ds.latest_status:
        print(f"  Status  : {ds.latest_status.summary()}")
    else:
        print("  Status  : (no samples yet)")
    if ds.latest_gps:
        print(f"  GPS     : {ds.latest_gps.summary()}")
    else:
        print("  GPS     : (no samples yet)")
    if ds.latest_power:
        print(f"  Power   : {ds.latest_power.summary()}")
    else:
        print("  Power   : (no samples yet)")
    if ds.latest_sys:
        print(f"  System  : {ds.latest_sys.summary()}")
    else:
        print("  System  : (no samples yet)")
    print(f"  RX frames received this session: {ds.rx_frame_count}")
    print()


def _export_diagnostics(client: PaktClient) -> None:
    ts   = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    path = Path(f"pakt_diagnostics_{ts}.json")
    client.diagnostics.export_json(path)
    print(f"  Diagnostics exported → {path.resolve()}")


def _show_message_queue(client: PaktClient) -> None:
    tracker = client.message_tracker
    msgs = tracker.recent(20)
    if not msgs:
        print("  No messages recorded this session.")
        return
    print()
    print(f"  {'ID':<8} {'DEST':<12} {'STATE':<12} {'TX':>3}  TEXT")
    print(f"  {'-'*8} {'-'*12} {'-'*12} {'-'*3}  {'-'*30}")
    for m in msgs:
        state_str = {
            MsgState.PENDING:   "pending",
            MsgState.ACKED:     "ACKED",
            MsgState.TIMEOUT:   "TIMEOUT",
            MsgState.ERROR:     "ERROR",
            MsgState.CANCELLED: "cancelled",
        }.get(m.state, m.state.name)
        print(f"  {m.msg_id:<8} {m.dest:<12} {state_str:<12} {m.tx_attempts:>3}  {m.text[:40]}")
    pending = tracker.pending()
    print(f"\n  {len(pending)} pending / {len(msgs)} total this session.")


# ── Main loop ─────────────────────────────────────────────────────────────────

async def _main() -> None:
    client = PaktClient(_on_event)
    store  = client.config_store

    # Show cached config on startup so the operator knows the last-known state.
    if store.exists():
        print("\n  ── Last cached config ──────────────────────────────")
        _show_cached_config(store)

    while True:
        _menu(client)
        choice = input("Choice: ").strip().upper()

        try:
            if choice == "Q":
                await client.disconnect()
                break

            elif choice == "E":
                _export_log()

            elif choice == "4":
                _show_cached_config(store)

            # ── Disconnected ──────────────────────────────────────────────────
            elif not client.is_connected:
                if choice == "S":
                    addr = await _pick_device(client)
                    if addr:
                        await client.connect(addr)
                elif choice == "C":
                    addr = input("  Address (e.g. AA:BB:CC:DD:EE:FF): ").strip()
                    if addr:
                        await client.connect(addr)

            # ── Connected ─────────────────────────────────────────────────────
            else:
                if choice == "D":
                    await client.disconnect()

                elif choice == "1":
                    info = await client.read_device_info()
                    for k, v in info.items():
                        print(f"  {k}: {v}")

                elif choice == "2":
                    raw = await client.read_config()
                    try:
                        print(json.dumps(json.loads(raw), indent=4))
                    except json.JSONDecodeError:
                        print(f"  {raw}")

                elif choice == "3":
                    # Show current cached config as a starting point.
                    cached_json = store.load_config_json()
                    if cached_json:
                        print("  Current cached config (edit and paste below):")
                        print(json.dumps(json.loads(cached_json), indent=4))
                    raw = _prompt_json(
                        '{"callsign":"N0CALL","ssid":7,"beacon_interval_s":300,'
                        '"symbol_table":"/","symbol_code":">","comment":"Pocket TNC"}',
                        "New config JSON: ",
                    )
                    if not raw:
                        continue
                    ok, err = ConfigStore.validate(raw)
                    if not ok:
                        print(f"  {err}")
                        continue
                    # Show diff if there is a cached version.
                    if cached_json:
                        diffs = ConfigStore.diff(cached_json, raw)
                        if diffs:
                            print("  Changes:")
                            for d in diffs:
                                print(d)
                        else:
                            print("  (no changes from cached config)")
                    await client.write_config(raw)

                elif choice == "5":
                    cached = store.load_config_json()
                    if not cached:
                        print("  No cached config to diff against.")
                        continue
                    staged = _prompt_json(
                        '{"callsign":"W1AW","ssid":0}',
                        "Staged JSON to diff: ",
                    )
                    if staged:
                        ok, err = ConfigStore.validate(staged)
                        if not ok:
                            print(f"  {err}")
                        else:
                            diffs = ConfigStore.diff(cached, staged)
                            if diffs:
                                for d in diffs:
                                    print(d)
                            else:
                                print("  (identical to cached config)")

                elif choice == "6":
                    raw = _prompt_json(
                        '{"cmd":"beacon_now"}  or  {"cmd":"radio_set","freq_hz":144390000}',
                        "Command JSON: ",
                    )
                    if raw:
                        await client.send_command(raw)

                elif choice == "7":
                    raw = _prompt_json(
                        '{"dest":"APRS","text":"Hello from PAKT","ssid":0}',
                        "TX Request JSON: ",
                    )
                    if raw:
                        await client.send_tx_request(raw)

                elif choice == "8":
                    print("  Listening for 30 s — notifications will appear above…")
                    await client.listen(30.0)

                elif choice == "9":
                    _show_message_queue(client)

                elif choice == "T":
                    _show_telemetry(client)

                elif choice == "X":
                    _export_diagnostics(client)

        except PermissionError:
            _print_pairing_help()
        except RuntimeError as exc:
            _on_event("ERROR", "cli", str(exc))
        except Exception as exc:  # noqa: BLE001
            msg = str(exc)
            _on_event("ERROR", "cli", msg)
            # Surface pairing guidance inline for auth errors.
            if "auth" in msg.lower() or "insufficient" in msg.lower():
                _print_pairing_help()


if __name__ == "__main__":
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    asyncio.run(_main())
