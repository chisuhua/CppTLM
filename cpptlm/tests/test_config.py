#!/usr/bin/env python3
"""cpptlm/tests/test_config.py — Config layer unit tests."""

import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))

from cpptlm.config import ConfigBuilder
from cpptlm.config.topologies import MeshTopology, RingTopology, CrossbarTopology


class TestConfigBuilder(unittest.TestCase):
    def test_builder_creation(self):
        builder = ConfigBuilder(name="test", description="test config")
        self.assertEqual(builder.name, "test")
        self.assertEqual(builder.description, "test config")

    def test_add_module(self):
        from cpptlm_config.models import ModuleSpec
        from cpptlm_config.types import ModuleType
        builder = ConfigBuilder(name="test")
        builder.add_module(ModuleSpec(name="r0", type=ModuleType.ROUTER_TLM))
        schema = builder.build()
        self.assertEqual(len(schema.modules), 1)
        self.assertEqual(schema.modules[0].name, "r0")

    def test_add_connection(self):
        from cpptlm_config.models import ConnectionSpec
        builder = ConfigBuilder(name="test")
        builder.add_connection(ConnectionSpec(src="r0", dst="r1", latency=1))
        schema = builder.build()
        self.assertEqual(len(schema.connections), 1)
        self.assertEqual(schema.connections[0].src, "r0")

    def test_set_extends(self):
        builder = ConfigBuilder(name="test")
        builder.set_extends("configs/base.json")
        schema = builder.build()
        self.assertEqual(schema.extends, "configs/base.json")

    def test_chainable_api(self):
        from cpptlm_config.models import ModuleSpec
        from cpptlm_config.types import ModuleType
        builder = ConfigBuilder(name="test")
        result = builder.add_module(ModuleSpec(name="r0", type=ModuleType.ROUTER_TLM))
        self.assertIs(result, builder)

    def test_set_include(self):
        builder = ConfigBuilder(name="test")
        builder.set_include("common/modules.json")
        schema = builder.build()
        self.assertEqual(schema.include, "common/modules.json")

    def test_add_group(self):
        builder = ConfigBuilder(name="test")
        builder.add_group("cpus", ["cpu0", "cpu1"])
        builder.add_group("memories", ["mem0"], exclude=["mem0"])
        schema = builder.build()
        self.assertEqual(len(schema.module_groups), 2)
        self.assertEqual(schema.module_groups[0].name, "cpus")
        self.assertEqual(schema.module_groups[0].members, ["cpu0", "cpu1"])
        self.assertEqual(len(schema.module_groups[1].exclude), 1)

    def test_group_connection_in_json(self):
        import json
        from cpptlm_config.models import ConnectionSpec
        builder = ConfigBuilder(name="test")
        builder.add_group("cpus", ["cpu0", "cpu1"])
        builder.add_connection(ConnectionSpec(src="group:cpus", dst="l1", latency=2))
        d = builder.build().to_json_dict()
        self.assertIn("module_groups", d)
        self.assertIn('group:', d["connections"][0]["src"])


class TestMeshTopology(unittest.TestCase):
    def test_mesh_creation(self):
        mesh = MeshTopology(rows=2, cols=2)
        self.assertEqual(mesh.rows, 2)
        self.assertEqual(mesh.cols, 2)

    def test_mesh_build(self):
        mesh = MeshTopology(rows=2, cols=2)
        builder = mesh.build()
        self.assertIsInstance(builder, ConfigBuilder)
        self.assertIn("mesh", builder.name)


class TestRingTopology(unittest.TestCase):
    def test_ring_creation(self):
        ring = RingTopology(nodes=4)
        self.assertEqual(ring.nodes, 4)

    def test_ring_build(self):
        ring = RingTopology(nodes=4)
        builder = ring.build()
        self.assertIsInstance(builder, ConfigBuilder)


class TestCrossbarTopology(unittest.TestCase):
    def test_crossbar_creation(self):
        xbar = CrossbarTopology(ports=4)
        self.assertEqual(xbar.ports, 4)

    def test_crossbar_build(self):
        xbar = CrossbarTopology(ports=4)
        builder = xbar.build()
        self.assertIsInstance(builder, ConfigBuilder)


if __name__ == "__main__":
    unittest.main()
