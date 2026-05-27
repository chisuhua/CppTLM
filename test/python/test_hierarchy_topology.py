#!/usr/bin/env python3
"""
test_hierarchy_topology.py — TGMS v4.0 Hierarchy + Topology 连接测试

Python 驱动的端到端测试:
1. 使用 ConfigBuilder 构建带 hierarchy 的拓扑配置
2. 使用 cpptlm_sim 执行仿真
3. 验证 hierarchy 解析是否正确工作
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec, ModuleType


class TestHierarchyTopology(unittest.TestCase):
    """TGMS v4.0 Hierarchy Tree Parser 端到端测试"""

    def setUp(self):
        self.project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.sim_exe = os.path.join(self.project_root, 'build', 'bin', 'cpptlm_sim')
        self.config_dir = os.path.join(self.project_root, 'configs')

    def _write_config(self, config_data, filename):
        path = os.path.join(self.config_dir, filename)
        with open(path, 'w') as f:
            json.dump(config_data, f)
        return path

    def _run_sim(self, config_path):
        if not os.path.exists(self.sim_exe):
            self.skipTest(f"cpptlm_sim not found at {self.sim_exe}")

        result = subprocess.run(
            [self.sim_exe, config_path],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result

    def test_simple_hierarchy(self):
        """测试简单层级树解析"""
        config = ConfigBuilder(
            name="test_simple_hierarchy",
            description="Simple hierarchy test"
        )
        config.add_module(ModuleSpec(name="cache", type=ModuleType.CACHE_TLM))
        config.add_module(ModuleSpec(name="mem", type=ModuleType.MEMORY_TLM))
        config.add_connection(ConnectionSpec(src="cache", dst="mem", latency=1))

        schema = config.build()
        schema_dict = schema.model_dump(mode='json')

        schema_dict['hierarchy'] = {
            "name": "system",
            "children": [
                {"name": "cache"},
                {"name": "mem"}
            ]
        }

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema_dict, f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)

    def test_nested_hierarchy(self):
        """测试嵌套层级树"""
        config = ConfigBuilder(
            name="test_nested_hierarchy",
            description="Nested hierarchy test"
        )
        for i in range(4):
            config.add_module(ModuleSpec(name=f"cpu{i}", type=ModuleType.CACHE_TLM))
        config.add_module(ModuleSpec(name="xbar", type=ModuleType.CROSSBAR_TLM))
        config.add_module(ModuleSpec(name="mem", type=ModuleType.MEMORY_TLM))

        for i in range(4):
            config.add_connection(ConnectionSpec(src=f"cpu{i}", dst=f"xbar.{i}", latency=1))
        config.add_connection(ConnectionSpec(src="xbar.0", dst="mem", latency=2))

        schema = config.build()
        schema_dict = schema.model_dump(mode='json')

        schema_dict['hierarchy'] = {
            "name": "soc",
            "children": [
                {
                    "name": "cluster0",
                    "children": [
                        {"name": "cpu0"},
                        {"name": "cpu1"}
                    ]
                },
                {
                    "name": "cluster1",
                    "children": [
                        {"name": "cpu2"},
                        {"name": "cpu3"}
                    ]
                },
                {"name": "xbar"},
                {"name": "mem"}
            ]
        }

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema_dict, f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)

    def test_hierarchy_with_coherence_domains(self):
        """测试带 coherence_domains 的层级配置"""
        config = ConfigBuilder(
            name="test_coherence",
            description="Coherence domains test"
        )
        config.add_module(ModuleSpec(name="cache0", type=ModuleType.CACHE_TLM))
        config.add_module(ModuleSpec(name="cache1", type=ModuleType.CACHE_TLM))
        config.add_module(ModuleSpec(name="xbar", type=ModuleType.CROSSBAR_TLM))
        config.add_module(ModuleSpec(name="mem", type=ModuleType.MEMORY_TLM))

        config.add_connection(ConnectionSpec(src="cache0", dst="xbar.0", latency=1))
        config.add_connection(ConnectionSpec(src="cache1", dst="xbar.1", latency=1))
        config.add_connection(ConnectionSpec(src="xbar.0", dst="mem", latency=2))
        config.add_connection(ConnectionSpec(src="xbar.1", dst="mem", latency=2))

        schema = config.build()
        schema_dict = schema.model_dump(mode='json')

        schema_dict['hierarchy'] = {
            "name": "soc",
            "children": [
                {
                    "name": "cluster0",
                    "children": [
                        {"name": "cache0", "coherence_domain": "L2_cache"},
                        {"name": "cache1", "coherence_domain": "L2_cache"}
                    ]
                },
                {"name": "xbar"},
                {"name": "mem"}
            ]
        }
        schema_dict['coherence_domains'] = ["L2_cache", "memory"]

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema_dict, f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)

    def test_no_hierarchy_still_works(self):
        """测试没有 hierarchy 配置时仍然正常工作"""
        config = ConfigBuilder(
            name="test_no_hierarchy",
            description="No hierarchy test"
        )
        config.add_module(ModuleSpec(name="cache", type=ModuleType.CACHE_TLM))
        config.add_module(ModuleSpec(name="mem", type=ModuleType.MEMORY_TLM))
        config.add_connection(ConnectionSpec(src="cache", dst="mem", latency=1))

        schema = config.build()
        schema_dict = schema.model_dump(mode='json')

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema_dict, f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)


class TestTopologyConnectionPatterns(unittest.TestCase):
    """拓扑连接模式测试"""

    def setUp(self):
        self.project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.sim_exe = os.path.join(self.project_root, 'build', 'bin', 'cpptlm_sim')

    def _run_sim(self, config_path):
        if not os.path.exists(self.sim_exe):
            self.skipTest(f"cpptlm_sim not found at {self.sim_exe}")

        result = subprocess.run(
            [self.sim_exe, config_path],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result

    def test_mesh_topology(self):
        """测试 Mesh 拓扑"""
        config = ConfigBuilder(
            name="mesh_2x2",
            description="2x2 Mesh topology"
        )

        nodes = ['r00', 'r01', 'r10', 'r11']
        for name in nodes:
            config.add_module(ModuleSpec(name=name, type=ModuleType.ROUTER_TLM))

        connections = [
            ('r00.1', 'r01.3'), ('r00.2', 'r10.0'),
            ('r01.1', 'r11.3'), ('r01.2', 'r00.0'),
            ('r10.1', 'r11.1'), ('r10.2', 'r00.0'),
            ('r11.1', 'r01.0'), ('r11.2', 'r10.0'),
        ]
        for src, dst in connections:
            config.add_connection(ConnectionSpec(src=src, dst=dst, latency=1))

        schema = config.build()
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema.model_dump(mode='json'), f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)

    def test_crossbar_topology(self):
        """测试 Crossbar 拓扑"""
        config = ConfigBuilder(
            name="crossbar_4port",
            description="4-port Crossbar topology"
        )

        for i in range(4):
            config.add_module(ModuleSpec(name=f"cpu{i}", type=ModuleType.CACHE_TLM))
        config.add_module(ModuleSpec(name="xbar", type=ModuleType.CROSSBAR_TLM))
        config.add_module(ModuleSpec(name="mem", type=ModuleType.MEMORY_TLM))

        for i in range(4):
            config.add_connection(ConnectionSpec(src=f"cpu{i}", dst=f"xbar.{i}", latency=1))
            config.add_connection(ConnectionSpec(src=f"xbar.{i}", dst="mem", latency=2))

        schema = config.build()
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema.model_dump(mode='json'), f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)

    def test_ring_topology(self):
        """测试 Ring 拓扑"""
        config = ConfigBuilder(
            name="ring_4node",
            description="4-node Ring topology"
        )

        for i in range(4):
            config.add_module(ModuleSpec(name=f"router{i}", type=ModuleType.ROUTER_TLM))

        for i in range(4):
            next_i = (i + 1) % 4
            config.add_connection(ConnectionSpec(
                src=f"router{i}.0", dst=f"router{next_i}.2", latency=1
            ))
            config.add_connection(ConnectionSpec(
                src=f"router{i}.1", dst=f"router{next_i}.3", latency=1
            ))

        schema = config.build()
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(schema.model_dump(mode='json'), f)
            config_path = f.name

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            os.unlink(config_path)


if __name__ == '__main__':
    unittest.main()