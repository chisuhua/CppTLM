#!/usr/bin/env python3
"""test_analyzer.py — analyzer.py 单元测试"""

import unittest
import sys
import os
import json

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts", "topology"))

from analyzer import TopologyAnalyzer


class TestAnalyzer(unittest.TestCase):

    def setUp(self):
        self.config = {
            "modules": [
                {"name": "nic0", "type": "NICTLM"},
                {"name": "router0", "type": "RouterTLM"},
                {"name": "router1", "type": "RouterTLM"},
                {"name": "mem0", "type": "MemoryTLM"},
            ],
            "connections": [
                {"src": "nic0", "dst": "router0.4", "latency": 1},
                {"src": "router0.1", "dst": "router1.3", "latency": 2},
                {"src": "router1.4", "dst": "mem0", "latency": 1},
            ]
        }
        self.analyzer = TopologyAnalyzer(self.config)

    def test_count_modules(self):
        self.assertEqual(self.analyzer.count_modules(), 4)

    def test_count_connections(self):
        self.assertEqual(self.analyzer.count_connections(), 3)

    def test_count_by_type(self):
        self.assertEqual(self.analyzer.count_by_type("RouterTLM"), 2)
        self.assertEqual(self.analyzer.count_by_type("NICTLM"), 1)

    def test_get_module_types(self):
        types = self.analyzer.get_module_types()
        self.assertIn("NICTLM", types)
        self.assertIn("RouterTLM", types)
        self.assertIn("MemoryTLM", types)

    def test_max_degree(self):
        self.assertEqual(self.analyzer.calculate_max_degree(), 2)

    def test_avg_latency(self):
        self.assertAlmostEqual(self.analyzer.calculate_avg_latency(), 4 / 3)

    def test_identify_bottlenecks(self):
        bottlenecks = self.analyzer.identify_bottlenecks(threshold_degree=2)
        self.assertIn("router0", bottlenecks)
        self.assertIn("router1", bottlenecks)

    def test_generate_report(self):
        report = self.analyzer.generate_report()
        self.assertEqual(report["summary"]["total_modules"], 4)
        self.assertEqual(report["summary"]["total_connections"], 3)
        self.assertIn("modules_by_type", report)
        self.assertIn("bottlenecks", report)


if __name__ == "__main__":
    unittest.main()