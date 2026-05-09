#!/usr/bin/env python3
"""test_credit_flow.py — credit_flow.py 单元测试"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from credit_flow import calculate_vc_credits, calculate_mesh_credits, generate_credit_config


class TestCreditFlow(unittest.TestCase):

    def test_calculate_vc_credits_default(self):
        result = calculate_vc_credits(8, 1)
        self.assertEqual(result, 10)

    def test_calculate_vc_credits_high_latency(self):
        result = calculate_vc_credits(8, 3)
        self.assertEqual(result, 14)

    def test_calculate_vc_credits_small_buffer(self):
        result = calculate_vc_credits(4, 1)
        self.assertEqual(result, 6)

    def test_calculate_mesh_credits_2x2(self):
        result = calculate_mesh_credits(2, 2, 8, 1)
        self.assertIn("link_0_east", result)
        self.assertIn("link_0_north", result)
        self.assertIn("link_1_north", result)
        self.assertIn("link_2_east", result)
        self.assertEqual(len(result), 4)

    def test_calculate_mesh_credits_vc_count(self):
        result = calculate_mesh_credits(2, 2, 8, 1)
        for link_name, vc_map in result.items():
            self.assertEqual(len(vc_map), 4)
            for vc_id in range(4):
                self.assertIn(str(vc_id), vc_map)

    def test_generate_credit_config_structure(self):
        config = generate_credit_config("mesh", mesh_x=2, mesh_y=2)
        self.assertEqual(config["topology"], "mesh")
        self.assertEqual(config["dimensions"]["x"], 2)
        self.assertEqual(config["dimensions"]["y"], 2)
        self.assertIn("link_credits", config)

    def test_generate_credit_config_unsupported_topology(self):
        with self.assertRaises(ValueError):
            generate_credit_config("torus")


if __name__ == "__main__":
    unittest.main()