#!/usr/bin/env python3
"""test_path_tracer.py — path_tracer.py 单元测试"""

import unittest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts", "topology"))

from path_tracer import PathTracer


class TestPathTracer(unittest.TestCase):

    def setUp(self):
        self.tracer = PathTracer(2, 2)

    def test_node_to_coord(self):
        self.assertEqual(self.tracer.node_to_coord(0), (0, 0))
        self.assertEqual(self.tracer.node_to_coord(1), (1, 0))
        self.assertEqual(self.tracer.node_to_coord(2), (0, 1))
        self.assertEqual(self.tracer.node_to_coord(3), (1, 1))

    def test_coord_to_node(self):
        self.assertEqual(self.tracer.coord_to_node(0, 0), 0)
        self.assertEqual(self.tracer.coord_to_node(1, 0), 1)
        self.assertEqual(self.tracer.coord_to_node(0, 1), 2)
        self.assertEqual(self.tracer.coord_to_node(1, 1), 3)

    def test_trace_xy_same_node(self):
        hops = self.tracer.trace_xy(0, 0)
        self.assertEqual(len(hops), 0)

    def test_trace_xy_east(self):
        hops = self.tracer.trace_xy(0, 1)
        self.assertEqual(len(hops), 1)
        self.assertEqual(hops[0].direction, "EAST")

    def test_trace_xy_north(self):
        hops = self.tracer.trace_xy(0, 2)
        self.assertEqual(len(hops), 1)
        self.assertEqual(hops[0].direction, "NORTH")

    def test_trace_xy_diagonal(self):
        hops = self.tracer.trace_xy(0, 3)
        self.assertEqual(len(hops), 2)
        self.assertEqual(hops[0].direction, "EAST")
        self.assertEqual(hops[1].direction, "NORTH")

    def test_trace_xy_west(self):
        hops = self.tracer.trace_xy(1, 0)
        self.assertEqual(len(hops), 1)
        self.assertEqual(hops[0].direction, "WEST")

    def test_total_latency(self):
        self.assertEqual(self.tracer.total_latency(0, 3, 1), 14)


if __name__ == "__main__":
    unittest.main()