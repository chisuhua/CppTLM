from __future__ import annotations

from typing import Any, Dict, List


def flatten_record(record: Dict[str, Any]) -> Dict[str, Any]:
    """Flatten nested Distribution/Percentile fields from C++ StreamingReporter.

    C++ outputs Distribution fields as nested dicts:
      {"latency": {"count": 5, "min": 1, "avg": 2.5, "max": 5, "stddev": 1.2}}

    This extracts .avg as the scalar value:
      {"latency": 2.5}

    Flat fields (scalars, strings) are preserved as-is.
    """
    data = record.get("data", {})
    flat = {}
    for key, value in data.items():
        if isinstance(value, dict):
            if "avg" in value:
                flat[key] = value["avg"]
            elif "p50" in value:
                flat[key] = value.get("p95", value.get("avg", 0.0))
            else:
                flat[key] = value.get("count", 0)
        else:
            flat[key] = value
    return {**record, "data": flat}


def flatten_records(records: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Flatten all records in a list."""
    return [flatten_record(r) for r in records]


def adapt_result(result) -> Any:
    """Return a modified result with flattened data.

    Works with cpptlm.simulation.result.Result by wrapping records().
    """
    original_records = result.records()
    flat = flatten_records(original_records)

    class AdaptedResult:
        def __init__(self, wrapped):
            self._wrapped = wrapped
            self._flat = flat

        def records(self, group=None):
            if group is None:
                return self._flat
            return [r for r in self._flat if r.get("group") == group]

        def groups(self):
            return self._wrapped.groups()

        def timestamps(self):
            return self._wrapped.timestamps()

    return AdaptedResult(result)