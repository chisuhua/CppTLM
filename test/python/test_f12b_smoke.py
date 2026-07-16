#!/usr/bin/env python3
"""test_f12b_smoke.py — F12b-LD 基础烟雾测试 (G-F0)

验证:
  - CppTLM 以 --f12b-ld 启动时不崩溃
  - 不带 --f12b-ld 时零退化 (回归基线)
"""

import unittest
import subprocess
import sys
import os

CPPTLM_SIM = os.path.join(
    os.path.dirname(__file__), "..", "..", "build", "bin", "cpptlm_sim"
)

CONFIG_SMOKE = os.path.join(
    os.path.dirname(__file__), "..", "..", "configs", "vector_add_n1024.json"
)


class TestF12bSmoke(unittest.TestCase):
    """F12b-LD MemoryBridge 烟雾测试"""

    @classmethod
    def setUpClass(cls):
        if not os.path.isfile(CPPTLM_SIM):
            raise unittest.SkipTest(f"cpptlm_sim not built at {CPPTLM_SIM}")
        if not os.path.isfile(CONFIG_SMOKE):
            raise unittest.SkipTest(f"config not found at {CONFIG_SMOKE}")

    def test_f12b_default_off(self):
        """不带 --f12b-ld: cpptlm_sim 正常运行 (零退化)"""
        result = subprocess.run(
            [CPPTLM_SIM, CONFIG_SMOKE, "--cycles", "100"],
            capture_output=True, text=True, timeout=30,
        )
        self.assertIn("MemoryBridge disabled", result.stdout)
        self.assertEqual(result.returncode, 0)

    def test_f12b_enabled_no_crash(self):
        """带 --f12b-ld: MemoryBridge 初始化不崩溃"""
        result = subprocess.run(
            [CPPTLM_SIM, CONFIG_SMOKE, "--cycles", "100", "--f12b-ld"],
            capture_output=True, text=True, timeout=30,
        )
        self.assertIn("MemoryBridge enabled", result.stdout)
        self.assertEqual(result.returncode, 0)

    def test_f12b_requires_json_entries(self):
        """带 --f12b-ld 但 JSON 缺 kernel_launch: 报错而非崩溃"""
        # 使用不含 kernel_launch 的最小配置
        min_config = os.path.join(
            os.path.dirname(__file__), "..", "test_minimal_config.json"
        )
        with open(min_config, "w") as f:
            f.write('{"modules": [{"name":"mem","type":"MemoryTLM"}],'
                    '"connections": []}')
        try:
            result = subprocess.run(
                [CPPTLM_SIM, min_config, "--cycles", "10", "--f12b-ld"],
                capture_output=True, text=True, timeout=30,
            )
            self.assertNotEqual(result.returncode, 0)
        finally:
            if os.path.exists(min_config):
                os.remove(min_config)


if __name__ == "__main__":
    unittest.main()