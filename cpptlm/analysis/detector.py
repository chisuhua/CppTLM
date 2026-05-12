from __future__ import annotations

from typing import Dict, List, Any, Optional
import numpy as np

from cpptlm.simulation.result import Result
from cpptlm.analysis.metrics import MetricSummary


class AnomalyDetector:
    def __init__(self, result: Result):
        self.result = result

    def detect_outliers_zscore(self, group: str, threshold: float = 3.0) -> List[Dict]:
        records = self.result.records(group=group)
        latencies = [r["data"]["latency"] for r in records if "latency" in r.get("data", {})]

        if len(latencies) < 3:
            return []

        arr = np.array(latencies, dtype=np.float64)
        mean = np.mean(arr)
        std = np.std(arr)

        if std == 0:
            return []

        outliers = []
        for i, lat in enumerate(latencies):
            z = abs((lat - mean) / std)
            if z > threshold:
                outliers.append({
                    "index": i,
                    "value": float(lat),
                    "z_score": float(z),
                    "threshold": threshold,
                })

        return outliers

    def identify_bottlenecks(self, threshold_percentile: float = 95.0) -> List[Dict]:
        groups = self.result.groups()
        group_means = []

        for group in groups:
            stats = MetricSummary(self.result).latency_statistics(group=group)
            if stats["count"] > 0:
                group_means.append({"group": group, "mean_latency": stats["mean"]})

        if not group_means:
            return []

        means = [g["mean_latency"] for g in group_means]
        threshold = np.percentile(means, threshold_percentile)

        bottlenecks = []
        for gm in group_means:
            if gm["mean_latency"] >= threshold:
                severity = "high" if gm["mean_latency"] > np.percentile(means, 99) else "medium"
                bottlenecks.append({
                    "group": gm["group"],
                    "mean_latency": gm["mean_latency"],
                    "threshold": float(threshold),
                    "severity": severity,
                })

        bottlenecks.sort(key=lambda x: (x["mean_latency"], x["group"]), reverse=True)
        return bottlenecks

    def detect_anomalous_run(self, group: Optional[str] = None) -> Dict:
        records = self.result.records(group=group)
        latencies = [r["data"]["latency"] for r in records if "latency" in r.get("data", {})]

        if len(latencies) < 3:
            return {"anomalous": False, "reason": "insufficient_data"}

        arr = np.array(latencies, dtype=np.float64)
        mean = np.mean(arr)
        std = np.std(arr)

        if mean == 0:
            return {"anomalous": False, "reason": "zero_mean"}

        cv = std / mean

        return {
            "anomalous": cv > 1.0,
            "cv": float(cv),
            "reason": "high_variance" if cv > 1.0 else "normal",
        }