#!/usr/bin/env python3
"""
demo_hierarchical_configs.py — 多层次 SoC 配置与仿真演示。

演示使用 ConfigBuilder 新 API 生成并运行多层次 JSON 配置：
  1. include_demo  — 演示 include 指令（复用 common/modules.json）
  2. soc_cluster  — 演示 add_group() + group:/regex: 连接
  3. dual_cluster  — 演示 set_extends() 配置继承

用法:
  python3 examples/demo_hierarchical_configs.py
  python3 examples/demo_hierarchical_configs.py --generate-only
"""

import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec


def build_include_demo():
    """使用 include 指令加载公共模块定义。"""
    return (
        ConfigBuilder("include_demo",
                      "Demonstrates JSON include directive")
        .set_include("common/modules.json")
        .add_module(ModuleSpec(
            name="cpu0", type="TrafficGenTLM",
            params={"pattern": "SEQUENTIAL", "num_requests": 5000,
                    "start_addr": "0x1000", "end_addr": "0x2000"},
        ))
        .add_connection(ConnectionSpec(src="cpu0", dst="l1_0", latency=1))
        .build()
    )


def build_soc_cluster():
    """使用 add_group() + group:/regex: 连接。"""
    return (
        ConfigBuilder("soc_cluster",
                      "CPU cluster with groups and regex connections")
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
        .add_connection(ConnectionSpec(src="group:cluster_cpus", dst="l1_0", latency=1))
        .add_connection(ConnectionSpec(src="regex:cpu[1]", dst="l1_1", latency=1))
        .build()
    )


def build_dual_cluster_soc():
    """使用 set_extends() 继承 soc_cluster 配置。"""
    return (
        ConfigBuilder("dual_cluster_soc",
                      "Dual-cluster extending soc_cluster")
        .set_extends("examples/demo_configs/soc_cluster.json")
        .add_group("memories", ["mem0", "mem1"])
        .add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
        .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
        .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="l1_0", dst="xbar.0", latency=5))
        .add_connection(ConnectionSpec(src="l1_1", dst="xbar.1", latency=5))
        .add_connection(ConnectionSpec(src="xbar.0", dst="mem0", latency=20))
        .add_connection(ConnectionSpec(src="xbar.1", dst="mem1", latency=20))
        .add_connection(ConnectionSpec(src="group:memories", dst="xbar.2", latency=2))
        .build()
    )


def run_simulation(config_path, cycles=10000, interval=5000):
    from cpptlm.simulation.runner import SimulationRunner
    import tempfile

    binary = "./build/bin/cpptlm_sim"
    if not os.path.exists(binary):
        return None

    stats_path = tempfile.mktemp(suffix=".jsonl")
    runner = SimulationRunner(binary_path=binary, config_path=config_path)
    runner.run_with_stats(stats_output=stats_path, interval=interval)
    result = runner.run(timeout=120)

    if result.returncode != 0 or not os.path.exists(stats_path):
        return None

    return stats_path


def analyze_and_report(config_name, config_path, stats_path):
    from cpptlm.simulation.result import Result
    from cpptlm.analysis import MetricSummary, AnomalyDetector
    from cpptlm.analysis.adapters import adapt_result

    result = Result.from_jsonl(stats_path)
    adapted = adapt_result(result)
    metrics = MetricSummary(adapted)
    detector = AnomalyDetector(adapted)

    print(f"\n  Config:    {config_name}")
    print(f"  Groups:    {', '.join(adapted.groups())}")

    has_data = False
    for group in adapted.groups():
        stats = metrics.latency_statistics(group=group)
        if stats["count"] > 0:
            has_data = True
            print(f"    {group:25s}  mean={stats['mean']:8.2f}  count={stats['count']}")

    if not has_data:
        print("  (simulation produced no traffic data)")

    bottlenecks = detector.identify_bottlenecks()
    if bottlenecks:
        print(f"  Bottleneck: {bottlenecks[0]['group']} ({bottlenecks[0]['severity']})")


def main():
    parser = argparse.ArgumentParser(description="Hierarchical SoC Config Demo")
    parser.add_argument("--generate-only", action="store_true",
                       help="Only generate JSON, don't run simulation")
    parser.add_argument("--cycles", type=int, default=10000)
    args = parser.parse_args()

    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    start = time.time()

    print("=" * 60)
    print("  CppTLM Hierarchical SoC Config Demo")
    print("=" * 60)

    configs = [
        ("include_demo",    build_include_demo(),     "include + base modules"),
        ("soc_cluster",     build_soc_cluster(),      "groups + regex: connections"),
        ("dual_cluster_soc", build_dual_cluster_soc(),"extends + multi-level groups"),
    ]

    saved_paths = []

    for name, schema, desc in configs:
        print(f"\n=== {name} ({desc}) ===")
        d = schema.to_json_dict()

        features = []
        if d.get("include"):
            features.append(f"include={d['include']}")
        if d.get("extends"):
            features.append(f"extends={d['extends']}")
        if d.get("module_groups"):
            groups_str = ", ".join(g["name"] for g in d["module_groups"])
            features.append(f"groups=[{groups_str}]")
        print(f"  Features:  {', '.join(features)}")
        print(f"  Modules:   {len(d['modules'])}  Connections: {len(d['connections'])}")

        # Highlight pattern connections
        for conn in d["connections"]:
            if "group:" in str(conn.get("src", "")) or "regex:" in str(conn.get("src", "")):
                print(f"    -> Pattern: {conn['src']} -> {conn['dst']}")

        path = f"examples/demo_configs/{name}.json"
        schema.save(path)
        saved_paths.append((name, path))

    if not args.generate_only:
        print("\n--- Running simulations ---")
        for name, path in saved_paths:
            stats_path = run_simulation(path, cycles=args.cycles)
            if stats_path:
                analyze_and_report(name, path, stats_path)
            else:
                print(f"\n  {name}: binary not found or simulation failed")

    elapsed = time.time() - start
    print(f"\n{'=' * 60}")
    print(f"  Done in {elapsed:.1f}s")
    print(f"  Configs saved to examples/demo_configs/")
    print("=" * 60)


if __name__ == "__main__":
    main()