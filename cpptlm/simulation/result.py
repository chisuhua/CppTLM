"""cpptlm/simulation/result.py — Result parser for JSONL output."""

from __future__ import annotations

import json
from pathlib import Path
from typing import List, Dict, Any, Optional


class Result:
    """Parse and analyze CppTLM simulation results from JSONL."""

    def __init__(self, jsonl_path: str):
        self.jsonl_path = jsonl_path
        self._records: List[Dict[str, Any]] = []
        self._load()

    def _load(self):
        with open(self.jsonl_path, "r") as f:
            for line in f:
                if line.strip():
                    self._records.append(json.loads(line))

    @classmethod
    def from_jsonl(cls, path: str) -> "Result":
        return cls(path)

    def records(self, group: Optional[str] = None) -> List[Dict[str, Any]]:
        if group is None:
            return self._records
        return [r for r in self._records if r.get("group") == group]

    def timestamps(self) -> List[int]:
        return [r["timestamp_ns"] for r in self._records]

    def groups(self) -> List[str]:
        return sorted(set(r["group"] for r in self._records))