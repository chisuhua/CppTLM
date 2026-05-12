from __future__ import annotations

from typing import Dict, List, Any, Optional
import numpy as np

from cpptlm.simulation.result import Result


class MetricSummary:
    def __init__(self, result: Result):
        self.result = result

    @staticmethod
    def _extract_scalar(value: Any) -> float:
        """从 C++ 统计值中提取标量。

        C++ 的 Distribution 输出为嵌套 dict:
          {"avg": 2.5, "min": 1, ...}
        本方法提取 .avg；Scalar/Average 直接返回数值。
        """
        if isinstance(value, dict):
            return float(value.get("avg", value.get("p50", 0.0)))
        return float(value)

    def latency_statistics(self, group: Optional[str] = None) -> Dict[str, float]:
        records = self.result.records(group=group)
        latencies = []
        for r in records:
            if "latency" in r.get("data", {}):
                latencies.append(self._extract_scalar(r["data"]["latency"]))

        if not latencies:
            return {"mean": 0.0, "median": 0.0, "p95": 0.0, "p99": 0.0, "max": 0.0, "min": 0.0, "count": 0, "std": 0.0}

        arr = np.array(latencies, dtype=np.float64)
        return {
            "mean": float(np.mean(arr)),
            "median": float(np.median(arr)),
            "p95": float(np.percentile(arr, 95)),
            "p99": float(np.percentile(arr, 99)),
            "max": float(np.max(arr)),
            "min": float(np.min(arr)),
            "count": len(latencies),
            "std": float(np.std(arr)),
        }

    def throughput_statistics(self, group: Optional[str] = None) -> Dict[str, float]:
        records = self.result.records(group=group)
        timestamps = [r["timestamp_ns"] for r in records if "timestamp_ns" in r]

        if len(timestamps) < 2:
            return {"requests_per_sec": 0.0, "total_requests": len(timestamps)}

        time_span_ns = max(timestamps) - min(timestamps)
        if time_span_ns == 0:
            return {"requests_per_sec": 0.0, "total_requests": len(timestamps)}

        requests_per_sec = (len(timestamps) - 1) / (time_span_ns / 1e9)
        return {"requests_per_sec": float(requests_per_sec), "total_requests": len(timestamps)}

    def hit_rate_statistics(self, group: Optional[str] = None) -> Dict[str, float]:
        records = self.result.records(group=group)
        hits = sum(r["data"].get("hits", 0) for r in records if "data" in r)
        misses = sum(r["data"].get("misses", 0) for r in records if "data" in r)
        total = hits + misses

        if total == 0:
            return {"hit_rate": 0.0, "total_hits": 0, "total_misses": 0, "total_requests": 0}

        return {
            "hit_rate": float(hits / total),
            "total_hits": hits,
            "total_misses": misses,
            "total_requests": total,
        }