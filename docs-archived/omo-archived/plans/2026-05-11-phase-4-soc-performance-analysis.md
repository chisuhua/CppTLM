# Phase 4: SoC Performance Analysis — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Python analysis module (`cpptlm.analysis`) that provides statistical performance summaries, multi-run comparisons, and anomaly detection for CppTLM SoC simulation results.

**Architecture:** Three independent analysis classes (`MetricSummary`, `ComparisonReport`, `AnomalyDetector`) operate on `Result` objects from Phase 2. Each class is self-contained and can be used standalone or composed into an end-to-end pipeline. The module integrates with existing `ReportGenerator` and `PerformanceDashboard` for HTML/report output.

**Tech Stack:** Python 3.10+, `unittest`, `numpy` (statistics), optional `scipy` (t-test), existing `cpptlm.simulation.Result`

---

## File Structure

| File | Status | Purpose |
|------|--------|---------|
| `cpptlm/analysis/__init__.py` | Create | Package exports |
| `cpptlm/analysis/metrics.py` | Create | `MetricSummary` — latency/throughput/hit-rate/queueing stats |
| `cpptlm/analysis/comparator.py` | Create | `ComparisonReport` — multi-run comparison + significance tests |
| `cpptlm/analysis/detector.py` | Create | `AnomalyDetector` — outlier detection + bottleneck identification |
| `cpptlm/analysis/tests/__init__.py` | Create | Test package init |
| `cpptlm/analysis/tests/conftest.py` | Create | Shared test fixtures (sys.path) |
| `cpptlm/analysis/tests/test_metrics.py` | Create | MetricSummary unit tests (TDD) |
| `cpptlm/analysis/tests/test_comparator.py` | Create | ComparisonReport unit tests (TDD) |
| `cpptlm/analysis/tests/test_detector.py` | Create | AnomalyDetector unit tests (TDD) |
| `cpptlm/__init__.py` | Modify | Add `MetricSummary`, `ComparisonReport`, `AnomalyDetector` to exports |

---

## Dependencies

Add to `pyproject.toml` under `[project.optional-dependencies]`:

```toml
analysis = [
    "numpy>=1.24.0",
    "scipy>=1.10.0",  # optional, for t-test
]
```

**Note:** numpy is already a common dependency. If not present, install with `pip install cpptlm[analysis]`.

---

## Pre-requisites (fix from Phase 2)

Before implementing Phase 4, fix `Result.groups()` to return sorted results for deterministic ordering:

**File:** `cpptlm/simulation/result.py` (line 36-37)

```python
def groups(self) -> List[str]:
    """Return sorted list of unique group names for deterministic order."""
    return sorted(set(r["group"] for r in self._records))
```

---

## Task 1: MetricSummary — Statistical Performance Metrics

**Files:**
- Create: `cpptlm/analysis/__init__.py`
- Create: `cpptlm/analysis/metrics.py`
- Create: `cpptlm/analysis/tests/__init__.py`
- Create: `cpptlm/analysis/tests/test_metrics.py`

**Parallel:** No (foundational, other tasks depend on it)

---

- [ ] **Step 1.1: Write failing test for MetricSummary creation**

```python
# cpptlm/analysis/tests/conftest.py
"""cpptlm/analysis/tests/conftest.py — Shared test fixtures."""

import sys
import os

# Ensure cpptlm package is importable (works for both installed and dev modes)
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
```

```python
# cpptlm/analysis/tests/test_metrics.py
#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_metrics.py — MetricSummary unit tests."""

import unittest
import tempfile
import os

# Import shared fixtures (handles sys.path)
from conftest import *  # noqa: F403, F401

from cpptlm.analysis.metrics import MetricSummary


class TestMetricSummary(unittest.TestCase):
    def test_creation_from_result(self):
        """MetricSummary can be created from a Result object."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 5}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            self.assertIsNotNone(metrics)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_creation_from_result -v`

**Expected:** `FAIL` — `ModuleNotFoundError: No module named 'cpptlm.analysis.metrics'`

---

- [ ] **Step 1.2: Create analysis package and minimal MetricSummary**

```python
# cpptlm/analysis/__init__.py
"""cpptlm.analysis — SoC performance analysis toolkit."""

from cpptlm.analysis.metrics import MetricSummary
from cpptlm.analysis.comparator import ComparisonReport
from cpptlm.analysis.detector import AnomalyDetector

__all__ = ["MetricSummary", "ComparisonReport", "AnomalyDetector"]
```

```python
# cpptlm/analysis/metrics.py
"""cpptlm/analysis/metrics.py — Statistical performance metrics."""

from __future__ import annotations

from typing import Dict, List, Any, Optional
import numpy as np

from cpptlm.simulation.result import Result


class MetricSummary:
    """Compute statistical summaries from simulation results.

    Supports latency, throughput, cache hit rate, and queueing delay metrics.
    """

    def __init__(self, result: Result):
        self.result = result
```

```python
# cpptlm/analysis/tests/__init__.py
"""cpptlm.analysis.tests — Analysis module test package."""
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_creation_from_result -v`

**Expected:** `PASS`

---

- [ ] **Step 1.3: Write failing test for latency_statistics**

Add to `test_metrics.py`:

```python
    def test_latency_statistics(self):
        """Compute latency mean, median, p95, p99, max."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 20}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "system.cache", "data": {"latency": 30}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.latency_statistics(group="system.cache")
            self.assertEqual(stats["mean"], 20.0)
            self.assertEqual(stats["median"], 20.0)
            self.assertEqual(stats["max"], 30)
            self.assertIn("p95", stats)
            self.assertIn("p99", stats)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_latency_statistics -v`

**Expected:** `FAIL` — `AttributeError: 'MetricSummary' object has no attribute 'latency_statistics'`

---

- [ ] **Step 1.4: Implement latency_statistics**

Add to `metrics.py`:

```python
    def latency_statistics(self, group: Optional[str] = None) -> Dict[str, float]:
        """Compute latency statistics for a given group.

        Args:
            group: Filter by group name (e.g., "system.cache"). If None, all records.

        Returns:
            Dict with keys: mean, median, p95, p99, max, min, count, std
        """
        records = self.result.records(group=group)
        latencies = [r["data"]["latency"] for r in records if "latency" in r.get("data", {})]

        if not latencies:
            return {"mean": 0.0, "median": 0.0, "p95": 0.0, "p99": 0.0, "max": 0.0, "min": 0.0, "count": 0, "std": 0.0}

        arr = np.array(latencies, dtype=np.float64)
        return {
            "mean": float(np.mean(arr)),
            "median": float(np.median(arr)),
            "p95": float(np.percentile(arr, 95)),
            "p99": float(np.percentile(arr, 99)),
            "max": int(np.max(arr)),
            "min": int(np.min(arr)),
            "count": len(latencies),
            "std": float(np.std(arr)),
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_latency_statistics -v`

**Expected:** `PASS`

---

- [ ] **Step 1.5: Write failing test for throughput_statistics**

Add to `test_metrics.py`:

```python
    def test_throughput_statistics(self):
        """Compute throughput (requests per simulation cycle)."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 10}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"requests": 20}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.throughput_statistics(group="system.cache")
            self.assertEqual(stats["total_requests"], 30)
            self.assertEqual(stats["avg_requests_per_cycle"], 0.15)  # 30 / 200 cycles
            self.assertEqual(stats["peak_requests"], 20)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_throughput_statistics -v`

**Expected:** `FAIL` — `AttributeError: 'MetricSummary' object has no attribute 'throughput_statistics'`

---

- [ ] **Step 1.6: Implement throughput_statistics**

Add to `metrics.py`:

```python
    def throughput_statistics(self, group: Optional[str] = None) -> Dict[str, Any]:
        """Compute throughput statistics for a given group.

        Args:
            group: Filter by group name. If None, all records.

        Returns:
            Dict with keys: total_requests, avg_requests_per_cycle, peak_requests,
            cycles, bandwidth_utilization (placeholder 0.0)
        """
        records = self.result.records(group=group)
        requests = [r["data"]["requests"] for r in records if "requests" in r.get("data", {})]
        timestamps = [r["simulation_cycle"] for r in records]

        if not requests:
            return {"total_requests": 0, "avg_requests_per_cycle": 0.0, "peak_requests": 0, "cycles": 0, "bandwidth_utilization": 0.0}

        total = sum(requests)
        cycles = max(timestamps) if timestamps else 1
        return {
            "total_requests": total,
            "avg_requests_per_cycle": round(total / cycles, 6),
            "peak_requests": max(requests),
            "cycles": cycles,
            "bandwidth_utilization": 0.0,  # requires external bandwidth spec
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_throughput_statistics -v`

**Expected:** `PASS`

---

- [ ] **Step 1.7: Write failing test for hit_rate_statistics**

Add to `test_metrics.py`:

```python
    def test_hit_rate_statistics(self):
        """Compute L1/L2/LLC hit rates from hits and misses."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "l1cache", "data": {"hits": 80, "misses": 20}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "l2cache", "data": {"hits": 60, "misses": 40}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "llc", "data": {"hits": 30, "misses": 70}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            l1 = metrics.hit_rate_statistics(group="l1cache")
            l2 = metrics.hit_rate_statistics(group="l2cache")
            llc = metrics.hit_rate_statistics(group="llc")
            self.assertAlmostEqual(l1["hit_rate"], 0.8, places=2)
            self.assertAlmostEqual(l2["hit_rate"], 0.6, places=2)
            self.assertAlmostEqual(llc["hit_rate"], 0.3, places=2)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_hit_rate_statistics -v`

**Expected:** `FAIL` — `AttributeError: 'MetricSummary' object has no attribute 'hit_rate_statistics'`

---

- [ ] **Step 1.8: Implement hit_rate_statistics**

Add to `metrics.py`:

```python
    def hit_rate_statistics(self, group: Optional[str] = None) -> Dict[str, Any]:
        """Compute cache hit rate for a given group.

        Args:
            group: Filter by group name (e.g., "l1cache", "l2cache", "llc").

        Returns:
            Dict with keys: hit_rate (0.0-1.0), hits, misses, total, miss_rate
        """
        records = self.result.records(group=group)
        hits = sum(r["data"]["hits"] for r in records if "hits" in r.get("data", {}))
        misses = sum(r["data"]["misses"] for r in records if "misses" in r.get("data", {}))
        total = hits + misses

        if total == 0:
            return {"hit_rate": 0.0, "hits": 0, "misses": 0, "total": 0, "miss_rate": 0.0}

        return {
            "hit_rate": round(hits / total, 6),
            "hits": hits,
            "misses": misses,
            "total": total,
            "miss_rate": round(misses / total, 6),
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_hit_rate_statistics -v`

**Expected:** `PASS`

---

- [ ] **Step 1.9: Write failing test for queueing_delay_statistics**

Add to `test_metrics.py`:

```python
    def test_queueing_delay_statistics(self):
        """Compute queueing delay statistics."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"queueing_delay": 2}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"queueing_delay": 5}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "system.cache", "data": {"queueing_delay": 8}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.queueing_delay_statistics(group="system.cache")
            self.assertEqual(stats["mean"], 5.0)
            self.assertEqual(stats["max"], 8)
            self.assertEqual(stats["count"], 3)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_queueing_delay_statistics -v`

**Expected:** `FAIL` — `AttributeError: 'MetricSummary' object has no attribute 'queueing_delay_statistics'`

---

- [ ] **Step 1.10: Implement queueing_delay_statistics**

Add to `metrics.py`:

```python
    def queueing_delay_statistics(self, group: Optional[str] = None) -> Dict[str, float]:
        """Compute queueing delay statistics for a given group.

        Args:
            group: Filter by group name. If None, all records.

        Returns:
            Dict with keys: mean, median, p95, p99, max, min, count, std
        """
        records = self.result.records(group=group)
        delays = [r["data"]["queueing_delay"] for r in records if "queueing_delay" in r.get("data", {})]

        if not delays:
            return {"mean": 0.0, "median": 0.0, "p95": 0.0, "p99": 0.0, "max": 0.0, "min": 0.0, "count": 0, "std": 0.0}

        arr = np.array(delays, dtype=np.float64)
        return {
            "mean": float(np.mean(arr)),
            "median": float(np.median(arr)),
            "p95": float(np.percentile(arr, 95)),
            "p99": float(np.percentile(arr, 99)),
            "max": int(np.max(arr)),
            "min": int(np.min(arr)),
            "count": len(delays),
            "std": float(np.std(arr)),
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_queueing_delay_statistics -v`

**Expected:** `PASS`

---

- [ ] **Step 1.11: Write failing test for summary_all (convenience method)**

Add to `test_metrics.py`:

```python
    def test_summary_all(self):
        """summary_all returns a dict of all metric categories."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10, "requests": 5, "hits": 4, "misses": 1}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            summary = metrics.summary_all(group="system.cache")
            self.assertIn("latency", summary)
            self.assertIn("throughput", summary)
            self.assertIn("hit_rate", summary)
            self.assertEqual(summary["latency"]["mean"], 10.0)
            self.assertEqual(summary["hit_rate"]["hit_rate"], 0.8)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_summary_all -v`

**Expected:** `FAIL` — `AttributeError: 'MetricSummary' object has no attribute 'summary_all'`

---

- [ ] **Step 1.12: Implement summary_all**

Add to `metrics.py`:

```python
    def summary_all(self, group: Optional[str] = None) -> Dict[str, Any]:
        """Return a combined summary of all metric categories.

        Args:
            group: Filter by group name. If None, all records.

        Returns:
            Dict with keys: latency, throughput, hit_rate, queueing_delay
        """
        return {
            "latency": self.latency_statistics(group),
            "throughput": self.throughput_statistics(group),
            "hit_rate": self.hit_rate_statistics(group),
            "queueing_delay": self.queueing_delay_statistics(group),
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics.TestMetricSummary.test_summary_all -v`

**Expected:** `PASS`

---

- [ ] **Step 1.13: Run all MetricSummary tests**

**Run:** `python -m unittest cpptlm.analysis.tests.test_metrics -v`

**Expected:** All 6 tests PASS

---

- [ ] **Step 1.14: Commit**

```bash
git add cpptlm/analysis/ cpptlm/analysis/tests/
git commit -m "feat(analysis): add MetricSummary with latency/throughput/hit-rate/queueing stats"
```

---

## Task 2: ComparisonReport — Multi-Run Comparison

**Files:**
- Create: `cpptlm/analysis/comparator.py`
- Create: `cpptlm/analysis/tests/test_comparator.py`

**Parallel:** Can run in parallel with Task 3 (detector), but depends on Task 1 (MetricSummary)

---

- [ ] **Step 2.1: Write failing test for ComparisonReport creation**

```python
# cpptlm/analysis/tests/conftest.py
"""cpptlm/analysis/tests/conftest.py — Shared test fixtures."""

import sys
import os

# Ensure cpptlm package is importable (works for both installed and dev modes)
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
```

```python
# cpptlm/analysis/tests/test_comparator.py
#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_comparator.py — ComparisonReport unit tests."""

import unittest
import tempfile
import os

# Import shared fixtures (handles sys.path)
from conftest import *  # noqa: F403, F401

from cpptlm.analysis.comparator import ComparisonReport


class TestComparisonReport(unittest.TestCase):
    def test_creation_with_runs(self):
        """ComparisonReport can be created with named runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            report = ComparisonReport()
            report.add_run("single_cluster", result)
            self.assertEqual(len(report.runs), 1)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_creation_with_runs -v`

**Expected:** `FAIL` — `ModuleNotFoundError: No module named 'cpptlm.analysis.comparator'`

---

- [ ] **Step 2.2: Implement minimal ComparisonReport**

```python
# cpptlm/analysis/comparator.py
"""cpptlm/analysis/comparator.py — Multi-run comparison and significance testing."""

from __future__ import annotations

from typing import Dict, List, Any, Optional
import numpy as np

from cpptlm.simulation.result import Result
from cpptlm.analysis.metrics import MetricSummary


class ComparisonReport:
    """Compare multiple simulation runs and compute statistical differences.

    Usage::

        report = ComparisonReport()
        report.add_run("single_cluster", result1)
        report.add_run("multi_cluster", result2)
        comparison = report.compare_latency(group="system.cache")
    """

    def __init__(self):
        self.runs: Dict[str, Result] = {}

    def add_run(self, name: str, result: Result) -> "ComparisonReport":
        """Add a named simulation run."""
        self.runs[name] = result
        return self
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_creation_with_runs -v`

**Expected:** `PASS`

---

- [ ] **Step 2.3: Write failing test for compare_latency**

Add to `test_comparator.py`:

```python
    def test_compare_latency(self):
        """Compare latency across two runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f1:
            f1.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            f1.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 20}}\n')
            path1 = f1.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f2:
            f2.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 30}}\n')
            f2.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 40}}\n')
            path2 = f2.name

        try:
            from cpptlm.simulation.result import Result
            r1 = Result.from_jsonl(path1)
            r2 = Result.from_jsonl(path2)
            report = ComparisonReport()
            report.add_run("run_a", r1).add_run("run_b", r2)
            comp = report.compare_latency(group="system.cache")
            self.assertIn("run_a", comp)
            self.assertIn("run_b", comp)
            self.assertEqual(comp["run_a"]["mean"], 15.0)
            self.assertEqual(comp["run_b"]["mean"], 35.0)
        finally:
            os.unlink(path1)
            os.unlink(path2)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_compare_latency -v`

**Expected:** `FAIL` — `AttributeError: 'ComparisonReport' object has no attribute 'compare_latency'`

---

- [ ] **Step 2.4: Implement compare_latency**

Add to `comparator.py`:

```python
    def compare_latency(self, group: Optional[str] = None) -> Dict[str, Dict[str, Any]]:
        """Compare latency statistics across all runs.

        Args:
            group: Filter by group name.

        Returns:
            Dict mapping run name -> latency statistics dict
        """
        return {
            name: MetricSummary(result).latency_statistics(group)
            for name, result in self.runs.items()
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_compare_latency -v`

**Expected:** `PASS`

---

- [ ] **Step 2.5: Write failing test for compare_throughput**

Add to `test_comparator.py`:

```python
    def test_compare_throughput(self):
        """Compare throughput across two runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f1:
            f1.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 10}}\n')
            path1 = f1.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f2:
            f2.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 20}}\n')
            path2 = f2.name

        try:
            from cpptlm.simulation.result import Result
            r1 = Result.from_jsonl(path1)
            r2 = Result.from_jsonl(path2)
            report = ComparisonReport()
            report.add_run("run_a", r1).add_run("run_b", r2)
            comp = report.compare_throughput(group="system.cache")
            self.assertEqual(comp["run_a"]["total_requests"], 10)
            self.assertEqual(comp["run_b"]["total_requests"], 20)
        finally:
            os.unlink(path1)
            os.unlink(path2)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_compare_throughput -v`

**Expected:** `FAIL` — `AttributeError: 'ComparisonReport' object has no attribute 'compare_throughput'`

---

- [ ] **Step 2.6: Implement compare_throughput, compare_hit_rate, compare_queueing_delay**

Add to `comparator.py`:

```python
    def compare_throughput(self, group: Optional[str] = None) -> Dict[str, Dict[str, Any]]:
        """Compare throughput statistics across all runs."""
        return {
            name: MetricSummary(result).throughput_statistics(group)
            for name, result in self.runs.items()
        }

    def compare_hit_rate(self, group: Optional[str] = None) -> Dict[str, Dict[str, Any]]:
        """Compare hit rate statistics across all runs."""
        return {
            name: MetricSummary(result).hit_rate_statistics(group)
            for name, result in self.runs.items()
        }

    def compare_queueing_delay(self, group: Optional[str] = None) -> Dict[str, Dict[str, Any]]:
        """Compare queueing delay statistics across all runs."""
        return {
            name: MetricSummary(result).queueing_delay_statistics(group)
            for name, result in self.runs.items()
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_compare_throughput -v`

**Expected:** `PASS`

---

- [ ] **Step 2.7: Write failing test for statistical significance (t-test)**

Add to `test_comparator.py`:

```python
    def test_significance_test(self):
        """Run t-test between two runs if scipy is available."""
        try:
            import scipy.stats
        except ImportError:
            self.skipTest("scipy not installed")

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f1:
            for latency in [10, 11, 10, 12, 11]:
                f1.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}\n')
            path1 = f1.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f2:
            for latency in [30, 31, 30, 32, 31]:
                f2.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}\n')
            path2 = f2.name

        try:
            from cpptlm.simulation.result import Result
            r1 = Result.from_jsonl(path1)
            r2 = Result.from_jsonl(path2)
            report = ComparisonReport()
            report.add_run("run_a", r1).add_run("run_b", r2)
            sig = report.significance_test("latency", group="system.cache")
            self.assertIn("p_value", sig)
            self.assertIn("significant", sig)
            self.assertTrue(sig["significant"])  # should be highly significant
        finally:
            os.unlink(path1)
            os.unlink(path2)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_significance_test -v`

**Expected:** `FAIL` — `AttributeError: 'ComparisonReport' object has no attribute 'significance_test'`

---

- [ ] **Step 2.8: Implement significance_test**

Add to `comparator.py`:

```python
    def significance_test(self, metric: str, group: Optional[str] = None, alpha: float = 0.05) -> Dict[str, Any]:
        """Perform independent t-test between the first two runs.

        Args:
            metric: Metric name — "latency", "throughput", "hit_rate", "queueing_delay"
            group: Filter by group name.
            alpha: Significance threshold (default 0.05).

        Returns:
            Dict with keys: p_value, significant (bool), statistic, metric, group
        """
        try:
            from scipy import stats
        except ImportError:
            return {"p_value": None, "significant": None, "statistic": None, "metric": metric, "group": group, "error": "scipy not installed"}

        if len(self.runs) < 2:
            return {"p_value": None, "significant": None, "statistic": None, "metric": metric, "group": group, "error": "need at least 2 runs"}

        run_names = list(self.runs.keys())
        result_a = self.runs[run_names[0]]
        result_b = self.runs[run_names[1]]

        records_a = result_a.records(group=group)
        records_b = result_b.records(group=group)

        # Map metric to data key
        key_map = {
            "latency": "latency",
            "throughput": "requests",
            "hit_rate": "hits",
            "queueing_delay": "queueing_delay",
        }
        key = key_map.get(metric, metric)

        data_a = [r["data"][key] for r in records_a if key in r.get("data", {})]
        data_b = [r["data"][key] for r in records_b if key in r.get("data", {})]

        if not data_a or not data_b:
            return {"p_value": None, "significant": None, "statistic": None, "metric": metric, "group": group, "error": "insufficient data"}

        statistic, p_value = stats.ttest_ind(data_a, data_b)
        return {
            "p_value": float(p_value),
            "significant": bool(p_value < alpha),
            "statistic": float(statistic),
            "metric": metric,
            "group": group,
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_significance_test -v`

**Expected:** `PASS` (if scipy installed; SKIP otherwise)

---

- [ ] **Step 2.9: Write failing test for generate_report (JSON output)**

Add to `test_comparator.py`:

```python
    def test_generate_report(self):
        """Generate a full comparison report dict."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f1:
            f1.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            path1 = f1.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f2:
            f2.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 30}}\n')
            path2 = f2.name

        try:
            from cpptlm.simulation.result import Result
            r1 = Result.from_jsonl(path1)
            r2 = Result.from_jsonl(path2)
            report = ComparisonReport()
            report.add_run("run_a", r1).add_run("run_b", r2)
            output = report.generate_report(group="system.cache")
            self.assertIn("latency", output)
            self.assertIn("run_a", output["latency"])
            self.assertIn("run_b", output["latency"])
        finally:
            os.unlink(path1)
            os.unlink(path2)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_generate_report -v`

**Expected:** `FAIL` — `AttributeError: 'ComparisonReport' object has no attribute 'generate_report'`

---

- [ ] **Step 2.10: Implement generate_report**

Add to `comparator.py`:

```python
    def generate_report(self, group: Optional[str] = None) -> Dict[str, Any]:
        """Generate a comprehensive comparison report.

        Args:
            group: Filter by group name.

        Returns:
            Dict with keys: latency, throughput, hit_rate, queueing_delay, significance
        """
        return {
            "latency": self.compare_latency(group),
            "throughput": self.compare_throughput(group),
            "hit_rate": self.compare_hit_rate(group),
            "queueing_delay": self.compare_queueing_delay(group),
            "significance": self.significance_test("latency", group),
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator.TestComparisonReport.test_generate_report -v`

**Expected:** `PASS`

---

- [ ] **Step 2.11: Run all ComparisonReport tests**

**Run:** `python -m unittest cpptlm.analysis.tests.test_comparator -v`

**Expected:** All 5 tests PASS

---

- [ ] **Step 2.12: Commit**

```bash
git add cpptlm/analysis/comparator.py cpptlm/analysis/tests/test_comparator.py
git commit -m "feat(analysis): add ComparisonReport for multi-run performance comparison"
```

---

## Task 3: AnomalyDetector — Outlier Detection and Bottleneck Identification

**Files:**
- Create: `cpptlm/analysis/detector.py`
- Create: `cpptlm/analysis/tests/test_detector.py`

**Parallel:** Can run in parallel with Task 2 (comparator), but depends on Task 1 (MetricSummary)

---

- [ ] **Step 3.1: Write failing test for AnomalyDetector creation**

```python
# cpptlm/analysis/tests/test_detector.py
#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_detector.py — AnomalyDetector unit tests."""

import unittest
import tempfile
import os

# Import shared fixtures (handles sys.path)
from conftest import *  # noqa: F403, F401

from cpptlm.analysis.detector import AnomalyDetector


class TestAnomalyDetector(unittest.TestCase):
    def test_creation_from_result(self):
        """AnomalyDetector can be created from a Result object."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            self.assertIsNotNone(detector)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_creation_from_result -v`

**Expected:** `FAIL` — `ModuleNotFoundError: No module named 'cpptlm.analysis.detector'`

---

- [ ] **Step 3.2: Implement minimal AnomalyDetector**

```python
# cpptlm/analysis/detector.py
"""cpptlm/analysis/detector.py — Anomaly detection and bottleneck identification."""

from __future__ import annotations

from typing import Dict, List, Any, Optional
import numpy as np

from cpptlm.simulation.result import Result
from cpptlm.analysis.metrics import MetricSummary


class AnomalyDetector:
    """Detect anomalous performance and identify system bottlenecks.

    Uses statistical methods (IQR, z-score) to flag outliers in simulation data.
    """

    def __init__(self, result: Result):
        self.result = result
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_creation_from_result -v`

**Expected:** `PASS`

---

- [ ] **Step 3.3: Write failing test for detect_latency_outliers (IQR method)**

Add to `test_detector.py`:

```python
    def test_detect_latency_outliers_iqr(self):
        """Detect outliers using IQR method."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            # Normal latencies: 10-14, Outlier: 100
            for latency in [10, 11, 12, 13, 14, 100]:
                f.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            outliers = detector.detect_latency_outliers(group="system.cache", method="iqr")
            self.assertEqual(len(outliers), 1)
            self.assertEqual(outliers[0]["latency"], 100)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_detect_latency_outliers_iqr -v`

**Expected:** `FAIL` — `AttributeError: 'AnomalyDetector' object has no attribute 'detect_latency_outliers'`

---

- [ ] **Step 3.4: Implement detect_latency_outliers**

Add to `detector.py`:

```python
    def detect_latency_outliers(
        self,
        group: Optional[str] = None,
        method: str = "iqr",
        threshold: float = 3.0
    ) -> List[Dict[str, Any]]:
        """Detect latency outliers in simulation records.

        Args:
            group: Filter by group name.
            method: "iqr" or "zscore".
            threshold: For zscore, number of standard deviations (default 3.0).

        Returns:
            List of outlier records with original data + outlier flag.
        """
        records = self.result.records(group=group)
        latencies = []
        valid_records = []

        for r in records:
            if "latency" in r.get("data", {}):
                latencies.append(r["data"]["latency"])
                valid_records.append(r)

        if len(latencies) < 4:
            return []

        arr = np.array(latencies, dtype=np.float64)
        outliers = []

        if method == "iqr":
            q1 = np.percentile(arr, 25)
            q3 = np.percentile(arr, 75)
            iqr = q3 - q1
            lower = q1 - 1.5 * iqr
            upper = q3 + 1.5 * iqr
            for rec, val in zip(valid_records, latencies):
                if val < lower or val > upper:
                    outlier = dict(rec)
                    outlier["outlier"] = True
                    outlier["outlier_value"] = val
                    outliers.append(outlier)
        elif method == "zscore":
            mean = np.mean(arr)
            std = np.std(arr)
            if std == 0:
                return []
            for rec, val in zip(valid_records, latencies):
                z = abs(val - mean) / std
                if z > threshold:
                    outlier = dict(rec)
                    outlier["outlier"] = True
                    outlier["z_score"] = round(z, 4)
                    outliers.append(outlier)

        return outliers
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_detect_latency_outliers_iqr -v`

**Expected:** `PASS`

---

- [ ] **Step 3.5: Write failing test for detect_latency_outliers (zscore method)**

Add to `test_detector.py`:

```python
    def test_detect_latency_outliers_zscore(self):
        """Detect outliers using z-score method."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            for latency in [10, 10, 10, 10, 10, 100]:
                f.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            outliers = detector.detect_latency_outliers(group="system.cache", method="zscore", threshold=2.0)
            self.assertEqual(len(outliers), 1)
            self.assertEqual(outliers[0]["latency"], 100)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_detect_latency_outliers_zscore -v`

**Expected:** `PASS` (implementation already done in Step 3.4)

---

- [ ] **Step 3.6: Write failing test for identify_bottlenecks**

Add to `test_detector.py`:

```python
    def test_identify_bottlenecks(self):
        """Identify groups with anomalous latency as bottlenecks."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            # cache: normal latency
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            # memory: very high latency -> bottleneck
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.memory", "data": {"latency": 500}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            bottlenecks = detector.identify_bottlenecks(threshold_percentile=95)
            self.assertEqual(len(bottlenecks), 1)
            self.assertEqual(bottlenecks[0]["group"], "system.memory")
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_identify_bottlenecks -v`

**Expected:** `FAIL` — `AttributeError: 'AnomalyDetector' object has no attribute 'identify_bottlenecks'`

---

- [ ] **Step 3.7: Implement identify_bottlenecks**

Add to `detector.py`:

```python
    def identify_bottlenecks(self, threshold_percentile: float = 95.0) -> List[Dict[str, Any]]:
        """Identify groups with anomalously high latency as bottlenecks.

        Compares mean latency across all groups. Flags groups whose mean latency
        exceeds the specified percentile of all group means.

        Args:
            threshold_percentile: Percentile threshold (0-100). Groups above this
                percentile are flagged as bottlenecks.

        Returns:
            List of bottleneck dicts with keys: group, mean_latency, percentile, severity
        """
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
                    "threshold": threshold,
                    "severity": severity,
                })

        # Sort by mean_latency descending, then by group name (stable sort for reproducibility)
        bottlenecks.sort(key=lambda x: (x["mean_latency"], x["group"]), reverse=True)
        return bottlenecks
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_identify_bottlenecks -v`

**Expected:** `PASS`

---

- [ ] **Step 3.8: Write failing test for detect_anomalous_run**

Add to `test_detector.py`:

```python
    def test_detect_anomalous_run(self):
        """Flag a run as anomalous if latency variance is extremely high."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            for latency in [10, 10, 10, 10, 500]:
                f.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            flag = detector.detect_anomalous_run(group="system.cache")
            self.assertTrue(flag["anomalous"])
            self.assertIn("reason", flag)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_detect_anomalous_run -v`

**Expected:** `FAIL` — `AttributeError: 'AnomalyDetector' object has no attribute 'detect_anomalous_run'`

---

- [ ] **Step 3.9: Implement detect_anomalous_run**

Add to `detector.py`:

```python
    def detect_anomalous_run(self, group: Optional[str] = None) -> Dict[str, Any]:
        """Determine if the entire run is anomalous based on variance and outlier ratio.

        Args:
            group: Filter by group name.

        Returns:
            Dict with keys: anomalous (bool), outlier_ratio, coefficient_of_variation, reason
        """
        records = self.result.records(group=group)
        latencies = [r["data"]["latency"] for r in records if "latency" in r.get("data", {})]

        if len(latencies) < 4:
            return {"anomalous": False, "outlier_ratio": 0.0, "coefficient_of_variation": 0.0, "reason": "insufficient data"}

        arr = np.array(latencies, dtype=np.float64)
        mean = np.mean(arr)
        std = np.std(arr)
        cv = std / mean if mean != 0 else 0.0

        outliers = self.detect_latency_outliers(group=group, method="iqr")
        outlier_ratio = len(outliers) / len(latencies)

        reasons = []
        if cv > 1.0:
            reasons.append(f"high coefficient of variation ({cv:.2f})")
        if outlier_ratio > 0.1:
            reasons.append(f"high outlier ratio ({outlier_ratio:.2%})")

        return {
            "anomalous": len(reasons) > 0,
            "outlier_ratio": round(outlier_ratio, 4),
            "coefficient_of_variation": round(cv, 4),
            "reason": "; ".join(reasons) if reasons else "normal",
        }
```

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector.TestAnomalyDetector.test_detect_anomalous_run -v`

**Expected:** `PASS`

---

- [ ] **Step 3.10: Run all AnomalyDetector tests**

**Run:** `python -m unittest cpptlm.analysis.tests.test_detector -v`

**Expected:** All 5 tests PASS

---

- [ ] **Step 3.11: Commit**

```bash
git add cpptlm/analysis/detector.py cpptlm/analysis/tests/test_detector.py
git commit -m "feat(analysis): add AnomalyDetector with outlier detection and bottleneck identification"
```

---

## Task 4: Integration — Update Package Exports and E2E Test

**Files:**
- Modify: `cpptlm/__init__.py`
- Create: `cpptlm/analysis/tests/test_integration.py`

**Parallel:** No (depends on Tasks 1-3)

---

- [ ] **Step 4.1: Update cpptlm/__init__.py exports**

```python
# cpptlm/__init__.py
"""cpptlm — CppTLM Python Library

Chip design generation, topology visualization, and performance visualization
for the CppTLM simulation framework.

Usage::

    from cpptlm.config import ConfigBuilder, MeshTopology
    from cpptlm.analysis import MetricSummary, ComparisonReport, AnomalyDetector

    # Generate mesh topology
    mesh = MeshTopology(rows=4, cols=4)
    config = mesh.build()

    # Run simulation and analyze results
    from cpptlm.simulation import SimulationRunner
    runner = SimulationRunner(config_path="config.json")
    result = runner.run_with_stats("output.jsonl")

    # Analyze performance
    metrics = MetricSummary(result)
    print(metrics.latency_statistics())
"""

__version__ = "0.1.0"

from cpptlm.config import ConfigBuilder
from cpptlm.simulation import SimulationRunner
from cpptlm.visualization import PerformanceDashboard
from cpptlm.analysis import MetricSummary, ComparisonReport, AnomalyDetector

__all__ = [
    "ConfigBuilder",
    "SimulationRunner",
    "PerformanceDashboard",
    "MetricSummary",
    "ComparisonReport",
    "AnomalyDetector",
]
```

---

- [ ] **Step 4.2: Write integration test (full E2E workflow)**

```python
# cpptlm/analysis/tests/test_integration.py
#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_integration.py — E2E integration test for Phase 4."""

import unittest
import tempfile
import os

# Import shared fixtures (handles sys.path)
from conftest import *  # noqa: F403, F401

from cpptlm.simulation.result import Result
from cpptlm.analysis import MetricSummary, ComparisonReport, AnomalyDetector


class TestPhase4Integration(unittest.TestCase):
    def _make_jsonl(self, records):
        """Helper to create a temp JSONL file from a list of dicts."""
        f = tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False)
        for rec in records:
            f.write(json.dumps(rec) + '\n')
        f.close()
        return f.name

    def test_e2e_single_cluster_soc(self):
        """End-to-end: single cluster 4-core SoC with L1/L2/LLC/Memory hierarchy."""
        records = [
            # L1 cache (per-core, low latency)
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "core0.l1", "data": {"latency": 2, "requests": 100, "hits": 95, "misses": 5}},
            {"timestamp_ns": 2000, "simulation_cycle": 200, "group": "core0.l1", "data": {"latency": 1, "requests": 120, "hits": 115, "misses": 5}},
            {"timestamp_ns": 3000, "simulation_cycle": 300, "group": "core1.l1", "data": {"latency": 2, "requests": 90, "hits": 85, "misses": 5}},
            # L2 cache (shared cluster)
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "cluster0.l2", "data": {"latency": 12, "requests": 20, "hits": 15, "misses": 5}},
            {"timestamp_ns": 2000, "simulation_cycle": 200, "group": "cluster0.l2", "data": {"latency": 10, "requests": 25, "hits": 18, "misses": 7}},
            # LLC (shared)
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "llc", "data": {"latency": 40, "requests": 10, "hits": 6, "misses": 4}},
            {"timestamp_ns": 2000, "simulation_cycle": 200, "group": "llc", "data": {"latency": 35, "requests": 12, "hits": 7, "misses": 5}},
            # Memory (DDR)
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "memory", "data": {"latency": 150, "requests": 4, "hits": 0, "misses": 4}},
            {"timestamp_ns": 2000, "simulation_cycle": 200, "group": "memory", "data": {"latency": 180, "requests": 5, "hits": 0, "misses": 5}},
        ]

        path = self._make_jsonl(records)
        try:
            result = Result.from_jsonl(path)

            # 1. MetricSummary for each hierarchy level
            metrics = MetricSummary(result)
            l1_stats = metrics.hit_rate_statistics(group="core0.l1")
            self.assertAlmostEqual(l1_stats["hit_rate"], 0.95, places=2)

            l2_stats = metrics.latency_statistics(group="cluster0.l2")
            self.assertEqual(l2_stats["mean"], 11.0)

            mem_stats = metrics.latency_statistics(group="memory")
            self.assertEqual(mem_stats["mean"], 165.0)

            # 2. AnomalyDetector
            detector = AnomalyDetector(result)
            bottlenecks = detector.identify_bottlenecks()
            self.assertEqual(bottlenecks[0]["group"], "memory")  # memory has highest latency

            # 3. ComparisonReport (simulate comparing two configs)
            report = ComparisonReport()
            report.add_run("baseline", result)
            report.add_run("optimized", result)  # same data for test
            comp = report.compare_latency()
            self.assertIn("baseline", comp)
            self.assertIn("optimized", comp)

        finally:
            os.unlink(path)

    def test_e2e_multi_cluster_soc(self):
        """End-to-end: multi-cluster SoC with distributed LLC."""
        records = [
            # Cluster 0
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "cluster0.l2", "data": {"latency": 10, "requests": 50, "hits": 40, "misses": 10}},
            {"timestamp_ns": 2000, "simulation_cycle": 200, "group": "cluster0.l2", "data": {"latency": 12, "requests": 55, "hits": 42, "misses": 13}},
            # Cluster 1
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "cluster1.l2", "data": {"latency": 11, "requests": 48, "hits": 38, "misses": 10}},
            {"timestamp_ns": 2000, "simulation_cycle": 200, "group": "cluster1.l2", "data": {"latency": 13, "requests": 52, "hits": 40, "misses": 12}},
            # Distributed LLC slices
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "llc.slice0", "data": {"latency": 35, "requests": 20, "hits": 12, "misses": 8}},
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "llc.slice1", "data": {"latency": 38, "requests": 18, "hits": 10, "misses": 8}},
            # Memory controllers
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "mem.ctrl0", "data": {"latency": 120, "requests": 8, "hits": 0, "misses": 8}},
            {"timestamp_ns": 1000, "simulation_cycle": 100, "group": "mem.ctrl1", "data": {"latency": 130, "requests": 6, "hits": 0, "misses": 6}},
        ]

        path = self._make_jsonl(records)
        try:
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)

            # Compare cluster hit rates
            c0_hit = metrics.hit_rate_statistics(group="cluster0.l2")
            c1_hit = metrics.hit_rate_statistics(group="cluster1.l2")
            self.assertAlmostEqual(c0_hit["hit_rate"], 0.78, places=2)
            self.assertAlmostEqual(c1_hit["hit_rate"], 0.77, places=2)

            # Bottleneck detection across distributed system
            detector = AnomalyDetector(result)
            bottlenecks = detector.identify_bottlenecks()
            self.assertEqual(bottlenecks[0]["group"], "mem.ctrl1")  # highest latency

        finally:
            os.unlink(path)


if __name__ == "__main__":
    unittest.main()
```

---

- [ ] **Step 4.3: Run integration tests**

**Run:** `python -m unittest cpptlm.analysis.tests.test_integration -v`

**Expected:** All 2 tests PASS

---

- [ ] **Step 4.4: Run full analysis test suite**

**Run:** `python -m unittest discover -s cpptlm/analysis/tests -v`

**Expected:** All tests PASS (6 + 5 + 5 + 2 = 18 tests)

---

- [ ] **Step 4.5: Commit**

```bash
git add cpptlm/__init__.py cpptlm/analysis/tests/test_integration.py
git commit -m "feat(analysis): integrate MetricSummary, ComparisonReport, AnomalyDetector into cpptlm package"
```

---

## Task 5: Integration with Existing Visualization and Reporting

**Files:**
- Modify: `cpptlm/visualization/report.py`
- Modify: `cpptlm/visualization/dashboard.py`

**Parallel:** No (depends on Tasks 1-4)

---

- [ ] **Step 5.1: Update ReportGenerator to include analysis summary**

```python
# cpptlm/visualization/report.py
"""cpptlm/visualization/report.py — HTML report generator with Phase 4 analysis."""

from __future__ import annotations

from pathlib import Path
from typing import Optional


class ReportGenerator:
    """Generate HTML reports from simulation results with performance analysis."""

    def __init__(self, result_path: str):
        self.result_path = result_path

    def generate(self, output_path: str = "cpptlm_report.html"):
        from cpptlm.simulation.result import Result
        from cpptlm.analysis.metrics import MetricSummary
        from cpptlm.analysis.detector import AnomalyDetector

        data = Result.from_jsonl(self.result_path)
        metrics = MetricSummary(data)
        detector = AnomalyDetector(data)

        groups = data.groups()
        group_summaries = []
        for group in groups:
            lat = metrics.latency_statistics(group=group)
            thr = metrics.throughput_statistics(group=group)
            hr = metrics.hit_rate_statistics(group=group)
            group_summaries.append(
                f"""<tr>
                    <td>{group}</td>
                    <td>{lat['mean']:.1f}</td>
                    <td>{lat['p95']:.1f}</td>
                    <td>{thr['total_requests']}</td>
                    <td>{hr['hit_rate']:.2%}</td>
                </tr>"""
            )

        bottlenecks = detector.identify_bottlenecks()
        bottleneck_rows = []
        for b in bottlenecks[:5]:
            bottleneck_rows.append(
                f"""<tr>
                    <td>{b['group']}</td>
                    <td>{b['mean_latency']:.1f}</td>
                    <td>{b['severity']}</td>
                </tr>"""
            )

        html = f"""<!DOCTYPE html>
<html>
<head><title>CppTLM Simulation Report</title></head>
<body>
<h1>CppTLM Simulation Report</h1>
<p>Groups: {", ".join(groups)}</p>

<h2>Performance Summary by Group</h2>
<table border="1">
<tr><th>Group</th><th>Latency (mean)</th><th>Latency (p95)</th><th>Requests</th><th>Hit Rate</th></tr>
{chr(10).join(group_summaries)}
</table>

<h2>Top Bottlenecks</h2>
<table border="1">
<tr><th>Group</th><th>Mean Latency</th><th>Severity</th></tr>
{chr(10).join(bottleneck_rows)}
</table>
</body>
</html>"""

        Path(output_path).write_text(html)
        return output_path
```

---

- [ ] **Step 5.2: Add analysis methods to PerformanceDashboard**

Add to `cpptlm/visualization/dashboard.py` (after existing plot methods):

```python
    def plot_latency_distribution(self, group: str = "system.cache") -> Dict[str, Any]:
        """Return histogram data for latency distribution."""
        from cpptlm.analysis.metrics import MetricSummary
        metrics = MetricSummary(self.result)
        stats = metrics.latency_statistics(group=group)
        return {
            "group": group,
            "mean": stats["mean"],
            "median": stats["median"],
            "p95": stats["p95"],
            "p99": stats["p99"],
            "max": stats["max"],
        }

    def plot_bottleneck_summary(self) -> Dict[str, Any]:
        """Return bottleneck data for dashboard display."""
        from cpptlm.analysis.detector import AnomalyDetector
        detector = AnomalyDetector(self.result)
        bottlenecks = detector.identify_bottlenecks()
        return {
            "bottlenecks": bottlenecks,
            "count": len(bottlenecks),
        }
```

---

- [ ] **Step 5.3: Write test for ReportGenerator with analysis**

Add to `cpptlm/tests/test_visualization.py`:

```python
    def test_report_generator_includes_analysis(self):
        """ReportGenerator now includes performance analysis tables."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10, "requests": 5, "hits": 4, "misses": 1}}\n')
            path = f.name

        try:
            from cpptlm.visualization.report import ReportGenerator
            report = ReportGenerator(path)
            output = report.generate(output_path="test_report.html")
            content = Path(output).read_text()
            self.assertIn("Performance Summary by Group", content)
            self.assertIn("Top Bottlenecks", content)
            self.assertIn("system.cache", content)
            os.unlink(output)
        finally:
            os.unlink(path)
```

**Run:** `python -m unittest cpptlm.tests.test_visualization.TestReportGenerator.test_report_generator_includes_analysis -v`

**Expected:** `PASS`

---

- [ ] **Step 5.4: Commit**

```bash
git add cpptlm/visualization/report.py cpptlm/visualization/dashboard.py cpptlm/tests/test_visualization.py
git commit -m "feat(visualization): integrate Phase 4 analysis into ReportGenerator and PerformanceDashboard"
```

---

## Task 6: Final Validation and Documentation

**Files:**
- Create: `cpptlm/analysis/README.md`

**Parallel:** No (depends on all previous tasks)

---

- [ ] **Step 6.1: Create analysis module README**

```markdown
# cpptlm.analysis — SoC Performance Analysis

Phase 4 of the CppTLM Python library. Provides statistical analysis,
multi-run comparison, and anomaly detection for cycle-accurate NoC simulations.

## Classes

### MetricSummary

Statistical summaries from a single simulation run.

```python
from cpptlm.simulation.result import Result
from cpptlm.analysis import MetricSummary

result = Result.from_jsonl("output.jsonl")
metrics = MetricSummary(result)

# Latency statistics
lat = metrics.latency_statistics(group="system.cache")
print(f"Mean: {lat['mean']:.1f}, P95: {lat['p95']:.1f}, P99: {lat['p99']:.1f}")

# Throughput
thr = metrics.throughput_statistics(group="system.cache")
print(f"Total requests: {thr['total_requests']}")

# Cache hit rates
hr = metrics.hit_rate_statistics(group="l1cache")
print(f"Hit rate: {hr['hit_rate']:.2%}")

# Queueing delay
qd = metrics.queueing_delay_statistics(group="system.cache")
print(f"Mean delay: {qd['mean']:.1f}")

# All metrics at once
summary = metrics.summary_all(group="system.cache")
```

### ComparisonReport

Compare multiple simulation runs (e.g., single-cluster vs multi-cluster).

```python
from cpptlm.analysis import ComparisonReport

report = ComparisonReport()
report.add_run("single_cluster", result1)
report.add_run("multi_cluster", result2)

# Compare latency
comp = report.compare_latency(group="system.cache")

# Full report with significance testing
full = report.generate_report(group="system.cache")
```

### AnomalyDetector

Detect outliers and identify bottlenecks.

```python
from cpptlm.analysis import AnomalyDetector

detector = AnomalyDetector(result)

# Find outlier records
outliers = detector.detect_latency_outliers(group="system.cache", method="iqr")

# Identify bottleneck groups
bottlenecks = detector.identify_bottlenecks()

# Check if entire run is anomalous
flag = detector.detect_anomalous_run(group="system.cache")
```

## Integration with Visualization

```python
from cpptlm.visualization import ReportGenerator

# HTML report now includes analysis tables
gen = ReportGenerator("output.jsonl")
gen.generate("report.html")
```

## SoC Memory Hierarchy Support

The analysis module is designed for CPU SoC simulations with:
- L1 Cache (per-core, ~1-2 cycles)
- L2 Cache (cluster, ~10 cycles)
- LLC (shared, ~30-50 cycles)
- DDR Memory (~100-200 cycles)

Group naming convention in JSONL:
- `core{i}.l1` — L1 cache for core i
- `cluster{j}.l2` — L2 cache for cluster j
- `llc` or `llc.slice{k}` — Last level cache
- `memory` or `mem.ctrl{m}` — Memory controller
```

---

- [ ] **Step 6.2: Run full Python test suite**

**Run:**
```bash
python -m unittest discover -s cpptlm/tests -v
python -m unittest discover -s cpptlm/analysis/tests -v
```

**Expected:** All tests PASS (existing + new)

---

- [ ] **Step 6.3: Final commit**

```bash
git add cpptlm/analysis/README.md
git commit -m "docs(analysis): add README for Phase 4 SoC performance analysis module"
```

---

## Work Breakdown Summary

```
Task 1: MetricSummary ─────┐
                           ├──→ Task 4: Integration & E2E ──→ Task 5: Visualization ──→ Task 6: Docs
Task 2: ComparisonReport ──┤      (depends on 1-3)
                           │
Task 3: AnomalyDetector ───┘

Parallel opportunities:
- Task 2 and Task 3 can be developed in parallel (both depend only on Task 1)
- Task 4, 5, 6 are sequential
```

## Test Summary

| Test File | Tests | Coverage |
|-----------|-------|----------|
| `test_metrics.py` | 6 | MetricSummary creation, latency, throughput, hit_rate, queueing_delay, summary_all |
| `test_comparator.py` | 5 | ComparisonReport creation, compare_latency/throughput, significance_test, generate_report |
| `test_detector.py` | 5 | AnomalyDetector creation, IQR/zscore outliers, bottleneck identification, anomalous run |
| `test_integration.py` | 2 | E2E single-cluster SoC, E2E multi-cluster SoC |
| **Total** | **18** | |

## Integration Points

| Phase 4 Component | Integrates With | How |
|-------------------|-----------------|-----|
| `MetricSummary` | `Result` (Phase 2) | Operates on `Result.records()` |
| `ComparisonReport` | `Result` (Phase 2) | Accepts multiple `Result` objects |
| `AnomalyDetector` | `Result` (Phase 2) | Operates on `Result.records()` |
| `ReportGenerator` (updated) | `MetricSummary` + `AnomalyDetector` | Embeds analysis tables in HTML |
| `PerformanceDashboard` (updated) | `MetricSummary` + `AnomalyDetector` | New plot methods for analysis |

## Notes

- **scipy is optional**: If not installed, `significance_test` returns an error dict. All other functionality works without scipy.
- **numpy is required**: Used for percentile, mean, std calculations.
- **Group naming**: The analysis assumes JSONL records use `"group"` field for module identification (e.g., `"system.cache"`, `"core0.l1"`).
- **Performance**: All analysis methods are O(n) where n is record count. Suitable for 10K-1M record files.
- **No C++ changes**: This is a pure Python Phase. No modifications to C++ simulator needed.
