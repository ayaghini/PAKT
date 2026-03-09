# config_store.py – Local device config persistence (APP-003)
#
# Saves the last-read or last-written device config to a JSON file so that:
#   - The operator can inspect the current config offline.
#   - Config edits can be staged and reviewed before writing to the device.
#   - The last-known config is visible at tool startup without a BLE read.
#
# File format:
#   {
#     "updated_utc": "2026-03-08T10:23:45.678+00:00",
#     "device_address": "AA:BB:CC:DD:EE:FF",
#     "source": "read" | "write",
#     "config": { ...parsed config object... }
#   }

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

DEFAULT_PATH = Path("pakt_config_cache.json")


class ConfigStore:
    """Read/write device config to a local JSON cache file."""

    def __init__(self, path: Path = DEFAULT_PATH) -> None:
        self._path = path

    # ── Persistence ───────────────────────────────────────────────────────────

    def save(
        self,
        config_json: str,
        device_address: str = "",
        source: str = "read",
    ) -> None:
        """Persist *config_json* (a UTF-8 JSON string) to the cache file.

        Parameters
        ----------
        config_json:
            Raw JSON string as returned by the device or typed by the user.
        device_address:
            BLE address of the device this config came from (informational).
        source:
            "read"  – config was read from the device.
            "write" – config was written to the device and accepted.
        """
        entry: dict[str, Any] = {
            "updated_utc":    datetime.now(timezone.utc).isoformat(),
            "device_address": device_address,
            "source":         source,
            "config":         json.loads(config_json),
        }
        self._path.write_text(json.dumps(entry, indent=2), encoding="utf-8")

    def load(self) -> dict[str, Any] | None:
        """Load and return the full cache entry, or None if absent/corrupt."""
        if not self._path.exists():
            return None
        try:
            return json.loads(self._path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return None

    def load_config(self) -> dict[str, Any] | None:
        """Return just the config object from the cache, or None."""
        data = self.load()
        return data.get("config") if data else None

    def load_config_json(self) -> str | None:
        """Return the cached config as a JSON string, or None."""
        cfg = self.load_config()
        return json.dumps(cfg) if cfg is not None else None

    def exists(self) -> bool:
        return self._path.exists()

    # ── Validation helpers ────────────────────────────────────────────────────

    @staticmethod
    def validate(config_json: str) -> tuple[bool, str]:
        """Basic JSON parse check.

        Returns (True, "") on success or (False, error_message) on failure.
        Callers should validate further (e.g. required keys) before writing.
        """
        try:
            json.loads(config_json)
            return True, ""
        except json.JSONDecodeError as exc:
            return False, f"Invalid JSON: {exc}"

    @staticmethod
    def diff(old_json: str, new_json: str) -> list[str]:
        """Return a list of human-readable change lines between two config JSON strings.

        Lines use the format:  "  key: old_value → new_value"
        Only top-level keys are compared.  Returns [] if both are identical.
        """
        try:
            old = json.loads(old_json)
            new = json.loads(new_json)
        except json.JSONDecodeError:
            return ["(cannot diff: one or both strings are not valid JSON)"]

        changes: list[str] = []
        all_keys = sorted(set(old) | set(new))
        for key in all_keys:
            oval = old.get(key, "<absent>")
            nval = new.get(key, "<absent>")
            if oval != nval:
                changes.append(f"  {key}: {oval!r} → {nval!r}")
        return changes
