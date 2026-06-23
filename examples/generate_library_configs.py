#!/usr/bin/env python3
"""
generate_library_configs.py — F5 高级工厂函数 demo

演示 cpptlm.library 新增的 3 个工厂函数:
  - cpu_nested_cluster: 2-level CPU cluster (CpuCluster + CacheCluster L1xN+L2)
  - memory_cluster_hierarchical: 多通道 MemoryCluster (N channels + Arbiter)
  - gpu_topology: 4-level GPU (GpuCluster → N×GpcCluster → M×TpcCluster → cu×ComputeCluster)

通过 SoC fluent API 组合, 生成 C++ ModuleFactory 可加载的 JSON config.

Usage:
  python3 examples/generate_library_configs.py --demo cpu    → configs/library_cpu_demo.json
  python3 examples/generate_library_configs.py --demo memory → configs/library_memory_demo.json
  python3 examples/generate_library_configs.py --demo gpu    → configs/library_gpu_demo.json
  python3 examples/generate_library_configs.py --all         → All 3 demos
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm.library import (
    SoC,
    cpu_nested_cluster,
    memory_cluster_hierarchical,
    gpu_topology,
)


def build_cpu_demo():
    """2-level CPU cluster: 4 cores + CacheCluster (L1x2 + L2)."""
    soc = SoC("library_cpu_demo", description="F5 demo: cpu_nested_cluster (4 cores + L1x2 + L2)")
    soc.add_cluster(cpu_nested_cluster(num_cores=4, l1_count=2, l1_size="32KB", l2_size="1MB"))
    return soc


def build_memory_demo():
    """多通道 hierarchical Memory: 4 channels + Arbiter."""
    soc = SoC("library_memory_demo",
              description="F5 demo: memory_cluster_hierarchical (4 channels DDR4 + Arbiter)")
    soc.add_cluster(memory_cluster_hierarchical(channels=4, channel_size="2GB", memory_type="DDR4"))
    return soc


def build_gpu_demo():
    """4-level GPU topology: 2 GPC × 2 TPC × 2 CU = 8 CUs (cu_template 透传)."""
    soc = SoC("library_gpu_demo",
              description="F5 demo: gpu_topology (2×2×2 = 8 CUs, cu_template 透传到 GpuCluster/GpcCluster/TpcCluster)")
    soc.add_cluster(gpu_topology(gpc_count=2, tpc_per_gpc=2, cu_per_tpc=2,
                                 cu_template="configs/templates/compute_unit_v1.json"))
    return soc


DEMOS = {
    "cpu": build_cpu_demo,
    "memory": build_memory_demo,
    "gpu": build_gpu_demo,
}


def main():
    parser = argparse.ArgumentParser(description="F5 高级工厂函数 demo")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--demo", choices=list(DEMOS.keys()),
                       help="单个 demo: cpu / memory / gpu")
    group.add_argument("--all", action="store_true", help="生成所有 3 个 demo")
    parser.add_argument("--out-dir", default="configs",
                        help="输出目录 (default: configs)")
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)

    targets = list(DEMOS.keys()) if args.all else [args.demo]
    for name in targets:
        soc = DEMOS[name]()
        path = os.path.join(args.out_dir, f"library_{name}_demo.json")
        soc.save(path)
        print(f"  {name:6s} → {path}")


if __name__ == "__main__":
    main()