#!/usr/bin/env python3
"""
generate_hierarchical_configs.py — 使用 ConfigBuilder 新 API 生成多层次 SoC 配置。

演示：
  - add_group() / set_include() / set_extends()
  - group: 和 regex: 前缀连接
  - include / extends 多文件复用

用法：
  python3 examples/generate_hierarchical_configs.py [--save]

输出：
  examples/demo_configs/soc_cluster.json         # 单集群配置（include + groups）
  examples/demo_configs/dual_cluster_soc.json    # 双集群配置（extends + groups）
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec


def build_soc_cluster():
    """单集群 SoC：2 CPU + L1 caches + groups + regex: connections.
    soc_cluster.json 直接运行时会自动解析 include 指令。
    Note: extends 链中 include 需内联（C++ 暂不支持跨文件上下文传播）"""
    b = (
        ConfigBuilder("soc_cluster", "Single CPU cluster with groups and regex connections")
        .add_group("cluster_cpus", ["cpu0", "cpu1"])
        .add_module(ModuleSpec(
            name="cpu0", type="TrafficGenTLM",
            params={"pattern": "SEQUENTIAL", "num_requests": 5000,
                    "start_addr": "0x1000", "end_addr": "0x2000"},
        ))
        .add_module(ModuleSpec(
            name="cpu1", type="TrafficGenTLM",
            params={"pattern": "RANDOM", "num_requests": 5000,
                    "start_addr": "0x2000", "end_addr": "0x4000"},
        ))
        .add_module(ModuleSpec(name="l1_0", type="CacheTLM"))
        .add_module(ModuleSpec(name="l1_1", type="CacheTLM"))
        # 使用 group: 前缀和 regex: 前缀进行连接
        .add_connection(ConnectionSpec(src="group:cluster_cpus", dst="l1_0", latency=1))
        .add_connection(ConnectionSpec(src="regex:cpu[1]", dst="l1_1", latency=1))
    )
    return b.build()


def build_dual_cluster_soc():
    """双集群 SoC：extends soc_cluster + crossbar + dual memories + groups.
    Note: 不使用 include（C++ JsonIncluder 不保留子目录上下文）"""
    b = (
        ConfigBuilder("dual_cluster_soc",
                      "Dual-cluster SoC extending common base config")
        .set_extends("examples/demo_configs/soc_cluster.json")
        .add_group("cluster0", ["cpu0", "l1_0"])
        .add_group("cluster1", ["cpu1", "l1_1"])
        .add_group("memories", ["mem0", "mem1"])
        .add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
        .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
        .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="l1_0", dst="xbar.0", latency=5))
        .add_connection(ConnectionSpec(src="l1_1", dst="xbar.1", latency=5))
        .add_connection(ConnectionSpec(src="xbar.0", dst="mem0", latency=20))
        .add_connection(ConnectionSpec(src="xbar.1", dst="mem1", latency=20))
        .add_connection(ConnectionSpec(src="group:memories", dst="xbar.2", latency=2))
    )
    return b.build()


def main():
    parser = argparse.ArgumentParser(description="Generate hierarchical SoC configs")
    parser.add_argument("--save", action="store_true", help="Save config files")
    args = parser.parse_args()

    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    print("=" * 60)
    print("  Hierarchical SoC Config Generator")
    print("=" * 60)

    print("\n--- soc_cluster (include + groups) ---")
    sc = build_soc_cluster()
    d_sc = sc.to_json_dict()
    print(json.dumps(d_sc, indent=2))
    print(f"\n  module_groups: {len(d_sc.get('module_groups', []))}")
    print(f"  include: {d_sc.get('include', '(none)')}")

    if args.save:
        path = "examples/demo_configs/soc_cluster.json"
        sc.save(path)
        print(f"  Saved: {path}")

    print("\n--- dual_cluster_soc (extends + groups) ---")
    dc = build_dual_cluster_soc()
    d_dc = dc.to_json_dict()
    print(json.dumps(d_dc, indent=2))
    print(f"\n  extends: {d_dc.get('extends', '(none)')}")

    if args.save:
        path = "examples/demo_configs/dual_cluster_soc.json"
        dc.save(path)
        print(f"  Saved: {path}")

    print("\nDone.")


if __name__ == "__main__":
    main()