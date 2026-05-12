from __future__ import annotations

from typing import Dict, Optional

from cpptlm.simulation.result import Result
from cpptlm.analysis.metrics import MetricSummary


class ComparisonReport:
    def __init__(self, runs: Dict[str, Result]):
        self.runs = runs

    def compare_latency(self, group: str) -> Dict[str, Dict[str, float]]:
        result = {}
        for name, run in self.runs.items():
            summary = MetricSummary(run)
            result[name] = summary.latency_statistics(group=group)
        return result

    def compare_throughput(self, group: str) -> Dict[str, Dict[str, float]]:
        result = {}
        for name, run in self.runs.items():
            summary = MetricSummary(run)
            result[name] = summary.throughput_statistics(group=group)
        return result

    def summary_table(self) -> str:
        lines = []
        lines.append(f"{'Metric':<20} " + " ".join(f"{name:>15}" for name in self.runs.keys()))

        all_groups = set()
        for run in self.runs.values():
            all_groups.update(run.groups())

        for group in sorted(all_groups):
            lines.append(f"\n{group}:")
            lat = self.compare_latency(group)
            thr = self.compare_throughput(group)

            lines.append(f"{'  mean latency':<20} " + " ".join(f"{lat[n]['mean']:>15.2f}" for n in self.runs.keys()))
            lines.append(f"{'  p95 latency':<20} " + " ".join(f"{lat[n]['p95']:>15.2f}" for n in self.runs.keys()))
            lines.append(f"{'  p99 latency':<20} " + " ".join(f"{lat[n]['p99']:>15.2f}" for n in self.runs.keys()))
            lines.append(f"{'  throughput':<20} " + " ".join(f"{thr[n]['requests_per_sec']:>15.2f}" for n in self.runs.keys()))

        return "\n".join(lines)