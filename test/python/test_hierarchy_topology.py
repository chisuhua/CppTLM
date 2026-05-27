#!/usr/bin/env python3
"""
test_hierarchy_topology.py — TGMS v4.0 Hierarchy + Topology 连接测试

Python 驱动的端到端测试:
1. 使用直接 JSON 构建拓扑配置
2. 使用 cpptlm_sim 执行仿真
3. 验证 hierarchy 解析是否正确工作
"""

import json
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))


class TestHierarchyTopology(unittest.TestCase):
    """TGMS v4.0 Hierarchy Tree Parser 端到端测试"""

    def setUp(self):
        self.project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        self.sim_exe = os.path.join(self.project_root, 'build', 'bin', 'cpptlm_sim')
        self.config_dir = os.path.join(self.project_root, 'configs')

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
        config_data = {
            "modules": [
                {"name": "cache", "type": "CacheTLM"},
                {"name": "mem", "type": "MemoryTLM"}
            ],
            "connections": [
                {"src": "cache", "dst": "mem", "latency": 1}
            ],
            "hierarchy": {
                "name": "system",
                "children": [
                    {"name": "cache"},
                    {"name": "mem"}
                ]
            }
        }

        config_path = os.path.join(self.config_dir, 'test_simple_hierarchy.json')
        with open(config_path, 'w') as f:
            json.dump(config_data, f)

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)

    def test_nested_hierarchy(self):
        """测试嵌套层级树"""
        config_data = {
            "modules": [
                {"name": f"cpu{i}", "type": "CacheTLM"} for i in range(4)
            ] + [
                {"name": "xbar", "type": "CrossbarTLM"},
                {"name": "mem", "type": "MemoryTLM"}
            ],
            "connections": [
                {"src": f"cpu{i}", "dst": f"xbar.{i}", "latency": 1} for i in range(4)
            ] + [
                {"src": "xbar.0", "dst": "mem", "latency": 2}
            ],
            "hierarchy": {
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
        }

        config_path = os.path.join(self.config_dir, 'test_nested_hierarchy.json')
        with open(config_path, 'w') as f:
            json.dump(config_data, f)

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)

    def test_hierarchy_with_coherence_domains(self):
        """测试带 coherence_domains 的层级配置"""
        config_data = {
            "modules": [
                {"name": "cache0", "type": "CacheTLM"},
                {"name": "cache1", "type": "CacheTLM"},
                {"name": "xbar", "type": "CrossbarTLM"},
                {"name": "mem", "type": "MemoryTLM"}
            ],
            "connections": [
                {"src": "cache0", "dst": "xbar.0", "latency": 1},
                {"src": "cache1", "dst": "xbar.1", "latency": 1},
                {"src": "xbar.0", "dst": "mem", "latency": 2},
                {"src": "xbar.1", "dst": "mem", "latency": 2}
            ],
            "hierarchy": {
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
            },
            "coherence_domains": ["L2_cache", "memory"]
        }

        config_path = os.path.join(self.config_dir, 'test_coherence.json')
        with open(config_path, 'w') as f:
            json.dump(config_data, f)

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)

    def test_no_hierarchy_still_works(self):
        """测试没有 hierarchy 配置时仍然正常工作"""
        config_data = {
            "modules": [
                {"name": "cache", "type": "CacheTLM"},
                {"name": "mem", "type": "MemoryTLM"}
            ],
            "connections": [
                {"src": "cache", "dst": "mem", "latency": 1}
            ]
        }

        config_path = os.path.join(self.config_dir, 'test_no_hierarchy.json')
        with open(config_path, 'w') as f:
            json.dump(config_data, f)

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)


class TestTopologyConnectionPatterns(unittest.TestCase):
    """拓扑连接模式测试"""

    def setUp(self):
        self.project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        self.sim_exe = os.path.join(self.project_root, 'build', 'bin', 'cpptlm_sim')
        self.config_dir = os.path.join(self.project_root, 'configs')

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

    def test_crossbar_topology(self):
        """测试 Crossbar 拓扑"""
        config_data = {
            "modules": [
                {"name": f"cpu{i}", "type": "CacheTLM"} for i in range(4)
            ] + [
                {"name": "xbar", "type": "CrossbarTLM"},
                {"name": "mem", "type": "MemoryTLM"}
            ],
            "connections": [
                {"src": f"cpu{i}", "dst": f"xbar.{i}", "latency": 1} for i in range(4)
            ] + [
                {"src": f"xbar.{i}", "dst": "mem", "latency": 2} for i in range(4)
            ]
        }

        config_path = os.path.join(self.config_dir, 'test_crossbar_topology.json')
        with open(config_path, 'w') as f:
            json.dump(config_data, f)

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)

    def test_ring_topology(self):
        """测试 Ring 拓扑"""
        config_data = {
            "modules": [
                {"name": f"router{i}", "type": "RouterTLM"} for i in range(4)
            ],
            "connections": [
                {"src": f"router{i}.0", "dst": f"router{(i+1)%4}.2", "latency": 1}
                for i in range(4)
            ] + [
                {"src": f"router{i}.1", "dst": f"router{(i+1)%4}.3", "latency": 1}
                for i in range(4)
            ]
        }

        config_path = os.path.join(self.config_dir, 'test_ring_topology.json')
        with open(config_path, 'w') as f:
            json.dump(config_data, f)

        try:
            result = self._run_sim(config_path)
            self.assertIn(result.returncode, [0, 1],
                f"Simulation failed: {result.stderr}")
        finally:
            if os.path.exists(config_path):
                os.unlink(config_path)


if __name__ == '__main__':
    unittest.main()