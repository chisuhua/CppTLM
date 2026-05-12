#!/usr/bin/env python3
"""cpptlm/tests/test_visualization.py — Visualization unit tests."""

import unittest
import sys
import os
import tempfile
import json

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm.visualization.dashboard import PerformanceDashboard
from cpptlm.visualization.report import ReportGenerator


class TestPerformanceDashboard(unittest.TestCase):
    def test_dashboard_creation(self):
        dash = PerformanceDashboard("output.jsonl", port=8050)
        self.assertEqual(dash.data_path, "output.jsonl")
        self.assertEqual(dash.port, 8050)

    def test_dashboard_creation_default_port(self):
        dash = PerformanceDashboard("output.jsonl")
        self.assertEqual(dash.port, 8050)

    def test_plot_latency_returns_dict(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"latency": 5}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"latency": 10}}\n')
            path = f.name

        try:
            dash = PerformanceDashboard(path)
            result = dash.plot_latency("system.cache")
            self.assertIn("x", result)
            self.assertIn("y", result)
            self.assertEqual(result["x"], [100, 200])
            self.assertEqual(result["y"], [5, 10])
        finally:
            os.unlink(path)

    def test_plot_throughput_returns_dict(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 5}}\n')
            f.write('{"timestamp_ns": 2000, "simulation_cycle": 200, "group": "system.cache", "data": {"requests": 10}}\n')
            path = f.name

        try:
            dash = PerformanceDashboard(path)
            result = dash.plot_throughput("system.cache")
            self.assertIn("x", result)
            self.assertIn("y", result)
            self.assertEqual(result["x"], [100, 200])
            self.assertEqual(result["y"], [5, 10])
        finally:
            os.unlink(path)

    def test_plot_latency_missing_key(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {}}\n')
            path = f.name

        try:
            dash = PerformanceDashboard(path)
            result = dash.plot_latency("system.cache")
            self.assertEqual(result["y"], [0])
        finally:
            os.unlink(path)


class TestReportGenerator(unittest.TestCase):
    def test_report_creation(self):
        report = ReportGenerator("output.jsonl")
        self.assertEqual(report.result_path, "output.jsonl")

    def test_generate_creates_html_file(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 5}}\n')
            path = f.name

        try:
            report = ReportGenerator(path)
            output = report.generate("/tmp/test_report.html")
            self.assertTrue(os.path.exists(output))
            os.unlink(output)
        finally:
            os.unlink(path)

    def test_generate_returns_path(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.jsonl', delete=False) as f:
            f.write('{"timestamp_ns": 1000, "simulation_cycle": 100, "group": "system.cache", "data": {"requests": 5}}\n')
            path = f.name

        try:
            report = ReportGenerator(path)
            output = report.generate()
            self.assertTrue(os.path.exists(output))
            os.unlink(output)
        finally:
            os.unlink(path)


if __name__ == "__main__":
    unittest.main()