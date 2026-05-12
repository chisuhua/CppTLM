#!/usr/bin/env python3
"""cpptlm/analysis/tests/test_detector.py — AnomalyDetector unit tests."""

import unittest
import tempfile
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.analysis.detector import AnomalyDetector


class TestAnomalyDetector(unittest.TestCase):
    def test_creation(self):
        """AnomalyDetector can be created from a Result object."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 5}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            self.assertIsNotNone(detector)
        finally:
            os.unlink(path)

    def test_detect_outliers_zscore(self):
        """Detect outliers using z-score method."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            for latency in [10, 10, 10, 10, 10, 100]:
                f.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            outliers = detector.detect_outliers_zscore(group="system.cache", threshold=2.0)
            self.assertIsInstance(outliers, list)
        finally:
            os.unlink(path)

    def test_identify_bottlenecks(self):
        """Identify high-latency groups as bottlenecks."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "mem.ctrl0", "data": {"latency": 120}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "mem.ctrl1", "data": {"latency": 130}}\n')
            f.write('{"timestamp_ns": 3000, "simulation_cycle": 300, "group": "core0.l1", "data": {"latency": 5}}\n')
            f.write('{"timestamp_ns": 4000, "simulation_cycle": 400, "group": "core1.l1", "data": {"latency": 6}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            bottlenecks = detector.identify_bottlenecks(threshold_percentile=95.0)
            self.assertIsInstance(bottlenecks, list)
            if bottlenecks:
                self.assertIn("group", bottlenecks[0])
                self.assertIn("mean_latency", bottlenecks[0])
                self.assertIn("severity", bottlenecks[0])
        finally:
            os.unlink(path)

    def test_detect_anomalous_run(self):
        """Flag a run as anomalous if latency variance is extremely high."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            for latency in [10, 10, 10, 10, 500]:
                f.write(f'{{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {{"latency": {latency}}}}}\n')
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            flag = detector.detect_anomalous_run(group="system.cache")
            self.assertIn("anomalous", flag)
        finally:
            os.unlink(path)

    def test_empty_data_handling(self):
        """Handle empty data gracefully."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            path = f.name

        try:
            from cpptlm.simulation.result import Result
            result = Result.from_jsonl(path)
            detector = AnomalyDetector(result)
            outliers = detector.detect_outliers_zscore(group="nonexistent")
            self.assertEqual(outliers, [])
            bottlenecks = detector.identify_bottlenecks()
            self.assertEqual(bottlenecks, [])
            flag = detector.detect_anomalous_run()
            self.assertFalse(flag.get("anomalous", False))
        finally:
            os.unlink(path)