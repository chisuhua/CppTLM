#!/usr/bin/env python3
"""test_stress_config_to_params.py — 验证 4 个迁移 JSON 中 `config` → `params` 字段迁移

迁移后 4 个 JSON 文件 (`configs/stress_full_system.json`, `configs/stress_strided.json`,
`configs/stress_hotspot.json`, `examples/demo_configs/single_cluster_soc.json`) 中
所有 TrafficGenTLM 的参数应位于 `params` 字段, C++ 端通过 `set_config()` 实际加载.

本测试不依赖 cpptlm Python 库, 直接用 stdlib `json` 读取 fixture 并断言字段值,
避免循环依赖 (这些 JSON 已被 C++ 端 ModuleFactory 验证通过).
"""
import json
import os
import sys
import unittest

# 项目根目录 (本文件位于 test/python/, 上溯 2 级)
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def load_config(rel_path):
    """从项目根目录加载 JSON fixture."""
    full_path = os.path.join(PROJECT_ROOT, rel_path)
    with open(full_path) as f:
        return json.load(f)


def find_module(config, name):
    """在 modules 数组中按 name 查找模块 entry."""
    for mod in config.get('modules', []):
        if mod.get('name') == name:
            return mod
    return None


class TestStressFullSystem(unittest.TestCase):
    """configs/stress_full_system.json: 4 个 TrafficGenTLM (cpu0-cpu3)"""

    def setUp(self):
        self.config = load_config('configs/stress_full_system.json')

    def test_cpu0_num_requests(self):
        """cpu0 的 params.num_requests 应为 10000 (用户配置值)."""
        cpu0 = find_module(self.config, 'cpu0')
        self.assertIsNotNone(cpu0, "cpu0 module not found")
        self.assertIn('params', cpu0, "cpu0 missing 'params' field")
        self.assertNotIn('config', cpu0, "cpu0 still has legacy 'config' field")
        self.assertEqual(cpu0['params'].get('num_requests'), 10000)

    def test_cpu1_pattern_random(self):
        """cpu1 pattern 应该是 RANDOM."""
        cpu1 = find_module(self.config, 'cpu1')
        self.assertIsNotNone(cpu1)
        self.assertEqual(cpu1['params'].get('pattern'), 'RANDOM')

    def test_cpu2_hotspot(self):
        """cpu2 hotspot 配置应含 3 个地址 + 3 个权重."""
        cpu2 = find_module(self.config, 'cpu2')
        self.assertIsNotNone(cpu2)
        params = cpu2.get('params', {})
        self.assertEqual(params.get('pattern'), 'HOTSPOT')
        self.assertEqual(len(params.get('hotspot_addrs', [])), 3)
        self.assertEqual(len(params.get('hotspot_weights', [])), 3)

    def test_cpu3_stride(self):
        """cpu3 stride 应该是 64."""
        cpu3 = find_module(self.config, 'cpu3')
        self.assertIsNotNone(cpu3)
        self.assertEqual(cpu3['params'].get('stride'), 64)


class TestStressStrided(unittest.TestCase):
    """configs/stress_strided.json: 1 个 TrafficGenTLM (traffic_gen_stride)"""

    def setUp(self):
        self.config = load_config('configs/stress_strided.json')

    def test_strided_stride(self):
        """traffic_gen_stride 的 params.stride 应为 64."""
        tg = find_module(self.config, 'traffic_gen_stride')
        self.assertIsNotNone(tg, "traffic_gen_stride module not found")
        self.assertIn('params', tg)
        self.assertNotIn('config', tg, "Module still has legacy 'config' field")
        self.assertEqual(tg['params'].get('pattern'), 'STRIDED')
        self.assertEqual(tg['params'].get('stride'), 64)
        self.assertEqual(tg['params'].get('num_requests'), 10000)


class TestStressHotspot(unittest.TestCase):
    """configs/stress_hotspot.json: 1 个 TrafficGenTLM (traffic_gen_hot)"""

    def setUp(self):
        self.config = load_config('configs/stress_hotspot.json')

    def test_hotspot_addrs(self):
        """traffic_gen_hot 的 params.hotspot_addrs 应含 3 个地址."""
        tg = find_module(self.config, 'traffic_gen_hot')
        self.assertIsNotNone(tg, "traffic_gen_hot module not found")
        self.assertIn('params', tg)
        self.assertNotIn('config', tg, "Module still has legacy 'config' field")
        params = tg.get('params', {})
        self.assertEqual(params.get('pattern'), 'HOTSPOT')
        addrs = params.get('hotspot_addrs', [])
        self.assertEqual(len(addrs), 3)
        # 验证具体地址
        for expected in ['0x1000', '0x2000', '0x3000']:
            self.assertIn(expected, addrs)


class TestSingleClusterSoc(unittest.TestCase):
    """examples/demo_configs/single_cluster_soc.json: 4 个 cpu module (cpu0-cpu3)"""

    def setUp(self):
        self.config = load_config('examples/demo_configs/single_cluster_soc.json')

    def test_all_cpus_have_params(self):
        """4 个 cpu module 都应有 params.pattern 字段."""
        for cpu_name in ['cpu0', 'cpu1', 'cpu2', 'cpu3']:
            with self.subTest(module=cpu_name):
                cpu = find_module(self.config, cpu_name)
                self.assertIsNotNone(cpu, f"{cpu_name} not found in modules")
                self.assertIn('params', cpu,
                              f"{cpu_name} missing 'params' field")
                self.assertNotIn('config', cpu,
                                 f"{cpu_name} still has legacy 'config' field")
                self.assertIn('pattern', cpu['params'],
                              f"{cpu_name}.params missing 'pattern'")

    def test_cpu0_pattern_sequential(self):
        """cpu0 pattern 应该是 SEQUENTIAL."""
        cpu0 = find_module(self.config, 'cpu0')
        self.assertEqual(cpu0['params'].get('pattern'), 'SEQUENTIAL')

    def test_cpu3_pattern_strided(self):
        """cpu3 pattern 应该是 STRIDED."""
        cpu3 = find_module(self.config, 'cpu3')
        self.assertEqual(cpu3['params'].get('pattern'), 'STRIDED')


class TestNoLegacyConfigField(unittest.TestCase):
    """回归测试: 所有迁移后 JSON 的 TrafficGenTLM 不应再有 'config' 字段 (dict form)."""

    FIXTURES = [
        ('configs/stress_full_system.json', ['cpu0', 'cpu1', 'cpu2', 'cpu3']),
        ('configs/stress_strided.json', ['traffic_gen_stride']),
        ('configs/stress_hotspot.json', ['traffic_gen_hot']),
        ('examples/demo_configs/single_cluster_soc.json', ['cpu0', 'cpu1', 'cpu2', 'cpu3']),
    ]

    def test_no_legacy_config_field_in_tg_modules(self):
        """所有迁移 JSON 中的指定 module 都不应有 'config' 字段."""
        for rel_path, module_names in self.FIXTURES:
            config = load_config(rel_path)
            for name in module_names:
                with self.subTest(file=rel_path, module=name):
                    mod = find_module(config, name)
                    self.assertIsNotNone(mod)
                    self.assertNotIn('config', mod,
                                     f"{rel_path}::{name} still has 'config' field")
                    self.assertIn('params', mod,
                                  f"{rel_path}::{name} missing 'params' field")


if __name__ == '__main__':
    unittest.main()
