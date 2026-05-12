#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_integration.py — E2E integration test for Phase 4."""

import unittest
import tempfile
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.simulation.result import Result
from cpptlm.analysis import MetricSummary, ComparisonReport, AnomalyDetector


class TestIntegration(unittest.TestCase):
    def test_single_cluster_soc(self):
        """E2E: Single cluster SoC (4 cores + shared L2 + LLC + memory)."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            for i in range(4):
                f.write(f'{{"timestamp_ns": {1000+i*100}, "simulation_cycle": {100+i*10}, "group": "core{i}.l1", "data": {{"latency": 2}}}}\n')
                f.write(f'{{"timestamp_ns": {2000+i*100}, "simulation_cycle": {200+i*10}, "group": "cluster{i}.l2", "data": {{"latency": 10}}}}\n')
            f.write('{"timestamp_ns": 5000, "simulation_cycle": 500, "group": "llc", "data": {"latency": 30}}\n')
            f.write('{"timestamp_ns": 6000, "simulation_cycle": 600, "group": "mem.ctrl0", "data": {"latency": 100}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            self.assertGreater(metrics.latency_statistics(group="mem.ctrl0")["mean"], 0)
            bottlenecks = AnomalyDetector(result).identify_bottlenecks()
            self.assertTrue(len(bottlenecks) > 0)
            self.assertEqual(bottlenecks[0]["group"], "mem.ctrl0")
        finally:
            os.unlink(path)

    def test_multi_cluster_soc(self):
        """E2E: Multi-cluster SoC (2 clusters x 4 cores + distributed LLC + 2 memory controllers)."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            for i in range(4):
                f.write(f'{{"timestamp_ns": {1000+i*50}, "simulation_cycle": {100+i*5}, "group": "cluster0.core{i}.l1", "data": {{"latency": 2}}}}\n')
                f.write(f'{{"timestamp_ns": {2000+i*50}, "simulation_cycle": {200+i*5}, "group": "cluster1.core{i}.l1", "data": {{"latency": 3}}}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "cluster0.l2", "data": {"latency": 12}}\n')
            f.write('{"timestamp_ns": 3100, "simulation_cycle": 310, "group": "cluster1.l2", "data": {"latency": 14}}\n')
            f.write('{"timestamp_ns": 4000, "simulation_cycle": 400, "group": "llc.slice0", "data": {"latency": 40}}\n')
            f.write('{"timestamp_ns": 4100, "simulation_cycle": 410, "group": "llc.slice1", "data": {"latency": 45}}\n')
            f.write('{"timestamp_ns": 5000, "simulation_cycle": 500, "group": "mem.ctrl0", "data": {"latency": 120}}\n')
            f.write('{"timestamp_ns": 5100, "simulation_cycle": 510, "group": "mem.ctrl1", "data": {"latency": 130}}\n')
            path = f.name

        try:
            result = Result.from_jsonl(path)
            metrics = MetricSummary(result)
            mem_ctrl_latencies = [
                metrics.latency_statistics(group="mem.ctrl0")["mean"],
                metrics.latency_statistics(group="mem.ctrl1")["mean"],
            ]
            self.assertGreater(sum(mem_ctrl_latencies), 0)
            bottlenecks = AnomalyDetector(result).identify_bottlenecks()
            self.assertTrue(len(bottlenecks) >= 1)
            self.assertEqual(bottlenecks[0]["group"], "mem.ctrl1")
        finally:
            os.unlink(path)


if __name__ == "__main__":
    unittest.main()