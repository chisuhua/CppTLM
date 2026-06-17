#!/usr/bin/env python3
"""generate_via_emitter.py — End-to-end SoC generation via CxxCompatibleEmitter

演示:
  - cpptlm.library 工厂 (cpu_l1_cluster, memory_cluster, crossbar_cluster)
  - SoC fluent API (add_cluster, add_module, connect_group, save)
  - Python tag → C++ groups dict 展开
  - cluster 嵌套 → hierarchy 树
  - 自动 layout 坐标
  - 占位符 placement 默认值

用法:
  python3 examples/generate_via_emitter.py
  → 产出 configs/example_emitter_soc.json
"""
import os
import sys
import json

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm.library import (
    SoC, cpu_l1_cluster, memory_cluster, crossbar_cluster,
)


def build_dual_cluster_soc() -> SoC:
    """构建 2-cluster SoC: 2× (2 cores + 1 L1) → crossbar → 2 memories."""
    soc = SoC(
        "example_emitter_soc",
        description="2 clusters (2 cores + 1 L1 each) + crossbar + 2 memories, "
                    "generated via cpptlm.topo + cpptlm.library",
    )

    soc.add_cluster(
        cpu_l1_cluster(idx=0, n_cores=2)
        .layout_grid(dx=3, dy=1, x_offset=0, y_offset=0)
    ).tag("compute")

    soc.add_cluster(
        cpu_l1_cluster(idx=1, n_cores=2)
        .layout_grid(dx=3, dy=1, x_offset=300, y_offset=0)
    ).tag("compute")

    soc.add_cluster(
        memory_cluster(name="mem", n_banks=2).layout_grid(dx=2, dy=1, x_offset=100, y_offset=200)
    ).tag("mem")
    soc.add_module("xbar", "CrossbarTLM", port_count=4)
    soc._root.metadata["xbar_layout"] = {"x": 250, "y": 100}

    soc.connect("cluster0_l1", "xbar.0", latency=5)
    soc.connect("cluster1_l1", "xbar.1", latency=5)
    soc.connect_group("compute", "xbar.2", latency=10)
    soc.connect_group("mem", "xbar.3", latency=3)

    return soc


def main():
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    soc = build_dual_cluster_soc()
    out_path = "configs/example_emitter_soc.json"
    soc.save(out_path)

    with open(out_path) as f:
        data = json.load(f)

    print(f"Generated: {out_path}")
    print(f"  modules:     {len(data['modules'])}")
    print(f"  connections: {len(data['connections'])}")
    print(f"  groups:      {sorted(data.get('groups', {}).keys())}")
    print(f"  hierarchy:   {len(data.get('hierarchy', {}).get('children', []))} sublayers")
    if 'hierarchy' in data:
        for child in data['hierarchy']['children']:
            print(f"    - {child['name']}")


if __name__ == "__main__":
    main()
