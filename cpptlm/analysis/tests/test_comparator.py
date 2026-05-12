#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_comparator.py — ComparisonReport unit tests."""

import unittest
import tempfile
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.simulation.result import Result


class TestComparisonReport(unittest.TestCase):
    def test_creation_with_runs(self):
        """ComparisonReport can be created with named runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            path1 = f.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 20}}\n')
            path2 = f.name

        try:
            result1 = Result.from_jsonl(path1)
            result2 = Result.from_jsonl(path2)
            from cpptlm.analysis.comparator import ComparisonReport
            report = ComparisonReport({"run1": result1, "run2": result2})
            self.assertIsNotNone(report)
            self.assertEqual(len(report.runs), 2)
        finally:
            os.unlink(path1)
            os.unlink(path2)

    def test_compare_latency(self):
        """Compare latency statistics across runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 20}}\n')
            path1 = f.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 15}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 25}}\n')
            path2 = f.name

        try:
            result1 = Result.from_jsonl(path1)
            result2 = Result.from_jsonl(path2)
            from cpptlm.analysis.comparator import ComparisonReport
            report = ComparisonReport({"fast": result1, "slow": result2})
            comparison = report.compare_latency("system.cache")
            self.assertIn("fast", comparison)
            self.assertIn("slow", comparison)
            self.assertEqual(comparison["fast"]["mean"], 15.0)
            self.assertEqual(comparison["slow"]["mean"], 20.0)
        finally:
            os.unlink(path1)
            os.unlink(path2)

    def test_compare_throughput(self):
        """Compare throughput across runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {}}\n')
            path1 = f.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {}}\n')
            f.write('{"timestamp_ns": 1500, "simulation_cycle": 150, "group": "system.cache", "data": {}}\n')
            path2 = f.name

        try:
            result1 = Result.from_jsonl(path1)
            result2 = Result.from_jsonl(path2)
            from cpptlm.analysis.comparator import ComparisonReport
            report = ComparisonReport({"slow": result1, "fast": result2})
            comparison = report.compare_throughput("system.cache")
            self.assertIn("slow", comparison)
            self.assertIn("fast", comparison)
            self.assertGreater(comparison["fast"]["requests_per_sec"], comparison["slow"]["requests_per_sec"])
        finally:
            os.unlink(path1)
            os.unlink(path2)

    def test_summary_table(self):
        """Summary table provides text comparison of all runs."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 20}}\n')
            path1 = f.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 15}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 25}}\n')
            path2 = f.name

        try:
            result1 = Result.from_jsonl(path1)
            result2 = Result.from_jsonl(path2)
            from cpptlm.analysis.comparator import ComparisonReport
            report = ComparisonReport({"run1": result1, "run2": result2})
            table = report.summary_table()
            self.assertIsInstance(table, str)
            self.assertIn("run1", table)
            self.assertIn("run2", table)
            self.assertIn("mean", table)
        finally:
            os.unlink(path1)
            os.unlink(path2)

    def test_missing_run_handling(self):
        """Handle missing group data gracefully."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 10}}\n')
            path1 = f.name

        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "other.group", "data": {"latency": 20}}\n')
            path2 = f.name

        try:
            result1 = Result.from_jsonl(path1)
            result2 = Result.from_jsonl(path2)
            from cpptlm.analysis.comparator import ComparisonReport
            report = ComparisonReport({"run1": result1, "run2": result2})
            comparison = report.compare_latency("nonexistent")
            self.assertEqual(comparison["run1"]["mean"], 0.0)
            self.assertEqual(comparison["run2"]["mean"], 0.0)
        finally:
            os.unlink(path1)
            os.unlink(path2)


if __name__ == "__main__":
    unittest.main()