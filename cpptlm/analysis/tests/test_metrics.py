#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_metrics.py — MetricSummary unit tests."""

import unittest
import tempfile
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.analysis.metrics import MetricSummary
from cpptlm.analysis.adapters import flatten_record, flatten_records, adapt_result


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
            self.assertEqual(stats["max"], 30.0)
            self.assertEqual(stats["min"], 10.0)
            self.assertIn("p95", stats)
            self.assertIn("p99", stats)
        finally:
            os.unlink(path)

    def test_latency_statistics_empty(self):
        """Empty data returns zeros with consistent float type."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.latency_statistics(group="nonexistent")
            self.assertEqual(stats["mean"], 0.0)
            self.assertEqual(stats["max"], 0.0)
            self.assertEqual(stats["min"], 0.0)
            self.assertIsInstance(stats["max"], float)
        finally:
            os.unlink(path)

    def test_throughput_statistics(self):
        """Compute requests/sec from timestamps."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "core0.l1", "data": {"latency": 5}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "core0.l1", "data": {"latency": 5}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "core0.l1", "data": {"latency": 5}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.throughput_statistics(group="core0.l1")
            self.assertIn("requests_per_sec", stats)
            self.assertGreater(stats["requests_per_sec"], 0)
        finally:
            os.unlink(path)

    def test_hit_rate_statistics(self):
        """Compute cache hit rates."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "l2.cache", "data": {"hits": 80, "misses": 20}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "l2.cache", "data": {"hits": 90, "misses": 10}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.hit_rate_statistics(group="l2.cache")
            self.assertAlmostEqual(stats["hit_rate"], 0.85, places=2)
            self.assertEqual(stats["total_hits"], 170)
            self.assertEqual(stats["total_misses"], 30)
        finally:
            os.unlink(path)

    def test_all_groups_summary(self):
        """Summary across all groups."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "group_a", "data": {"latency": 10}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "group_b", "data": {"latency": 20}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            stats = metrics.latency_statistics()
            self.assertEqual(stats["mean"], 15.0)
            self.assertEqual(stats["count"], 2)
        finally:
            os.unlink(path)


class TestCppStatsAdapter(unittest.TestCase):
    def test_flatten_nested_latency(self):
        """Distribution latency field is flattened to scalar."""
        record = {
            "data": {
                "latency": {"count": 5, "min": 2, "avg": 4.5, "max": 8, "stddev": 1.2},
                "hits": 100,
                "misses": 20,
            }
        }
        flat = flatten_record(record)
        self.assertEqual(flat["data"]["latency"], 4.5)
        self.assertEqual(flat["data"]["hits"], 100)

    def test_flatten_flat_records_preserved(self):
        """Flat fields are preserved as-is."""
        record = {"data": {"hits": 10, "misses": 5, "requests": 50}}
        flat = flatten_record(record)
        self.assertEqual(flat["data"]["hits"], 10)
        self.assertEqual(flat["data"]["requests"], 50)

    def test_flatten_empty_data(self):
        """Empty data returns empty."""
        record = {"data": {}}
        flat = flatten_record(record)
        self.assertEqual(flat["data"], {})

    def test_flatten_list(self):
        """Multiple records can be flattened at once."""
        records = [
            {"data": {"latency": {"avg": 2.0}}},
            {"data": {"latency": {"avg": 3.0}}},
        ]
        flat = flatten_records(records)
        self.assertEqual(len(flat), 2)
        self.assertEqual([r["data"]["latency"] for r in flat], [2.0, 3.0])

    def test_adapt_result_wrapper(self):
        """Adapted result maintains groups() and timestamps()."""
        import json
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write(json.dumps({
                "timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache",
                "data": {"latency": {"avg": 4.5}, "hits": 100},
            }) + "\n")
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            adapted = adapt_result(result)
            self.assertEqual(adapted.groups(), ["system.cache"])
            self.assertEqual(adapted.timestamps(), [1000])
            recs = adapted.records()
            self.assertEqual(recs[0]["data"]["latency"], 4.5)
        finally:
            os.unlink(path)