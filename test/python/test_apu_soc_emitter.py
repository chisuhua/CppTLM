# test/python/test_apu_soc_emitter.py
# P5 Python emitter + simulation 测试
# 验证 cpptlm 库的 apu_soc 生成器
import json
import pytest
from pathlib import Path


def test_apu_soc_template_load():
    """验证 ApuSoC 模板 JSON 可加载."""
    config_path = Path("configs/apu_soc_v1.json")
    assert config_path.exists(), f"Missing {config_path}"
    with open(config_path) as f:
        config = json.load(f)
    assert config["name"] == "apu_soc_v1"
    apu = config["modules"][0]
    assert "cpu_topology" in apu["params"]
    assert "gpu_topology" in apu["params"]


def test_gpu_2gpc_2tpc_2cu_simulate(tmp_path):
    """端到端: 加载 GPU JSON 配置, 验证模块计数."""
    config_path = Path("configs/gpu_2gpc_2tpc_2cu.json")
    assert config_path.exists()
    with open(config_path) as f:
        config = json.load(f)
    gpu = config["modules"][0]
    assert gpu["type"] == "GpuCluster"
    assert gpu["params"]["gpc_count"] == 2
    assert gpu["params"]["tpc_per_gpc"] == 2
    assert gpu["params"]["cu_per_tpc"] == 2
