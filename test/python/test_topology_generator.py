#!/usr/bin/env python3
"""test_topology_generator.py — topology_generator.py 单元测试"""

import unittest
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

try:
    import networkx as nx
    import pydot
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False


@unittest.skipUnless(HAS_DEPS, "networkx/pydot not installed")
class TestTopologyGenerator(unittest.TestCase):

    def setUp(self):
        from topology_generator import TopologyGenerator
        self.generator = TopologyGenerator

    def test_mesh_2x2_nodes(self):
        gen = self.generator("test_mesh")
        gen.generate_mesh(2, 2)
        self.assertEqual(len(gen.graph.nodes), 12)
        self.assertEqual(len(gen.graph.edges), 20)

    def test_mesh_3x3_nodes(self):
        gen = self.generator("test_mesh")
        gen.generate_mesh(3, 3)
        self.assertEqual(len(gen.graph.nodes), 27)
        self.assertEqual(len(gen.graph.edges), 48)

    def test_ring_4_nodes(self):
        gen = self.generator("test_ring")
        gen.generate_ring(4)
        self.assertEqual(len(gen.graph.nodes), 4)
        self.assertEqual(len(gen.graph.edges), 8)

    def test_ring_8_nodes(self):
        gen = self.generator("test_ring")
        gen.generate_ring(8)
        self.assertEqual(len(gen.graph.nodes), 8)
        self.assertEqual(len(gen.graph.edges), 16)

    def test_bus_4_nodes(self):
        gen = self.generator("test_bus")
        gen.generate_bus(4)
        self.assertEqual(len(gen.graph.nodes), 5)
        self.assertEqual(len(gen.graph.edges), 8)

    def test_bus_8_nodes(self):
        gen = self.generator("test_bus")
        gen.generate_bus(8)
        self.assertEqual(len(gen.graph.nodes), 9)
        self.assertEqual(len(gen.graph.edges), 16)

    def test_hierarchical_2_levels(self):
        gen = self.generator("test_hier")
        gen.generate_hierarchical(levels=2, factor=2)
        self.assertGreater(len(gen.graph.nodes), 0)
        self.assertGreater(len(gen.graph.edges), 0)

    def test_hierarchical_3_levels(self):
        gen = self.generator("test_hier")
        gen.generate_hierarchical(levels=3, factor=2)
        self.assertGreater(len(gen.graph.nodes), 7)

    def test_crossbar_4x4(self):
        gen = self.generator("test_xbar")
        gen.generate_crossbar(4, 4)
        self.assertEqual(len(gen.graph.nodes), 9)
        self.assertEqual(len(gen.graph.edges), 16)

    def test_crossbar_8x4(self):
        gen = self.generator("test_xbar")
        gen.generate_crossbar(8, 4)
        self.assertEqual(len(gen.graph.nodes), 13)
        self.assertEqual(len(gen.graph.edges), 24)

    def test_layout_coordinates_mesh(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        self.assertEqual(len(gen.layout_coords), 12)

    def test_layout_coordinates_ring(self):
        gen = self.generator("test")
        gen.generate_ring(4)
        self.assertEqual(len(gen.layout_coords), 4)

    def test_layout_coordinates_bus(self):
        gen = self.generator("test")
        gen.generate_bus(4)
        self.assertEqual(len(gen.layout_coords), 5)

    def test_export_json_config_mesh(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        config = gen.export_json_config()
        self.assertIn("modules", config)
        self.assertIn("connections", config)
        self.assertIn("name", config)
        self.assertEqual(len(config["modules"]), 12)

    def test_export_json_config_ring(self):
        gen = self.generator("test")
        gen.generate_ring(4)
        config = gen.export_json_config()
        self.assertIn("modules", config)
        self.assertIn("connections", config)

    def test_export_json_config_bus(self):
        gen = self.generator("test")
        gen.generate_bus(4)
        config = gen.export_json_config()
        self.assertIn("modules", config)

    def test_export_json_config_hierarchical(self):
        gen = self.generator("test")
        gen.generate_hierarchical(levels=2, factor=2)
        config = gen.export_json_config()
        self.assertIn("modules", config)

    def test_export_json_config_crossbar(self):
        gen = self.generator("test")
        gen.generate_crossbar(4, 4)
        config = gen.export_json_config()
        self.assertIn("modules", config)
        self.assertIn("connections", config)

    def test_export_layout_mesh(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        layout = gen.export_layout()
        self.assertIn("nodes", layout)
        self.assertIn("version", layout)
        self.assertEqual(len(layout["nodes"]), 12)

    def test_export_layout_ring(self):
        gen = self.generator("test")
        gen.generate_ring(4)
        layout = gen.export_layout()
        self.assertIn("nodes", layout)
        self.assertEqual(len(layout["nodes"]), 4)

    def test_import_layout(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        original_coords = dict(gen.layout_coords)
        layout = gen.export_layout()
        gen2 = self.generator("test2")
        gen2.generate_mesh(2, 2)
        gen2.import_layout(layout)
        self.assertEqual(gen2.layout_coords, original_coords)

    def test_save_json_file(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            tmp_path = f.name
        try:
            config = gen.export_json_config()
            with open(tmp_path, 'w') as f:
                json.dump(config, f)
            with open(tmp_path, 'r') as f:
                data = json.load(f)
            self.assertIn("modules", data)
        finally:
            os.unlink(tmp_path)

    def test_save_dot_file(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.dot', delete=False) as f:
            tmp_path = f.name
        try:
            gen.export_dot(tmp_path)
            with open(tmp_path, 'r') as f:
                content = f.read()
            self.assertIn("digraph", content)
        finally:
            os.unlink(tmp_path)

    def test_save_layout_json_file(self):
        gen = self.generator("test")
        gen.generate_mesh(2, 2)
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            tmp_path = f.name
        try:
            layout = gen.export_layout()
            with open(tmp_path, 'w') as f:
                json.dump(layout, f)
            with open(tmp_path, 'r') as f:
                data = json.load(f)
            self.assertIn("nodes", data)
        finally:
            os.unlink(tmp_path)

    def test_chain_calls(self):
        gen = self.generator("test")
        result = (gen
            .generate_mesh(2, 2)
            .generate_ring(4)
            .generate_bus(2))
        self.assertGreater(len(result.graph.nodes), 12)

    def test_graph_name(self):
        gen = self.generator("my_topology")
        self.assertEqual(gen.graph.name, "my_topology")


if __name__ == '__main__':
    unittest.main()
