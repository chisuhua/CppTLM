from __future__ import annotations

from typing import Optional, List, Dict, Any


class PerformanceDashboard:
    def __init__(self, data_path: str, port: int = 8050):
        self.data_path = data_path
        self.port = port
        self._data: Optional[Any] = None

    def load(self):
        from cpptlm.simulation.result import Result
        self._data = Result.from_jsonl(self.data_path)

    def plot_latency(self, group: str) -> Dict[str, Any]:
        if self._data is None:
            self.load()
        records = self._data.records(group)
        return {
            "x": [r["simulation_cycle"] for r in records],
            "y": [r["data"].get("latency", 0) for r in records]
        }

    def plot_throughput(self, group: str) -> Dict[str, Any]:
        if self._data is None:
            self.load()
        records = self._data.records(group)
        return {
            "x": [r["simulation_cycle"] for r in records],
            "y": [r["data"].get("requests", 0) for r in records]
        }

    def plot_metrics_summary(self, group: Optional[str] = None) -> Dict[str, Any]:
        from cpptlm.analysis import MetricSummary
        if self._data is None:
            self.load()
        metrics = MetricSummary(self._data)
        stats = metrics.latency_statistics(group=group)
        return {
            "mean": stats["mean"],
            "median": stats["median"],
            "p95": stats["p95"],
            "p99": stats["p99"],
            "max": stats["max"],
        }

    def plot_bottlenecks(self) -> Dict[str, Any]:
        from cpptlm.analysis import AnomalyDetector
        if self._data is None:
            self.load()
        detector = AnomalyDetector(self._data)
        bottlenecks = detector.identify_bottlenecks()
        return {
            "groups": [b["group"] for b in bottlenecks],
            "latencies": [b["mean_latency"] for b in bottlenecks],
            "severities": [b["severity"] for b in bottlenecks],
        }