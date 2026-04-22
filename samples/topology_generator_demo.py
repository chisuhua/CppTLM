#!/usr/bin/env python3
"""
topology_generator_demo.py — 拓扑生成器使用示例

演示如何使用 topology_generator.py 生成各种拓扑配置：
1. Mesh (2D 网格)
2. Ring (环形)
3. Bus (总线)
4. Hierarchical (层次树)
5. Crossbar (交叉开关)

生成的文件：
- JSON 配置文件 (用于 C++ 仿真)
- DOT 布局图 (Graphviz 可视化)
- 布局 JSON (节点坐标)
"""

import json
import os
import sys
import tempfile
import subprocess

# 添加 scripts 目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))

from topology_generator import TopologyGenerator


def demo_mesh():
    print("=" * 60)
    print("Demo 1: 4x4 Mesh NoC Topology")
    print("=" * 60)

    gen = TopologyGenerator("mesh_4x4")
    gen.generate_mesh(4, 4)

    print(f"  Nodes: {len(gen.graph.nodes)}")
    print(f"  Edges: {len(gen.graph.edges)}")

    # 导出配置
    config = gen.export_json_config()
    print(f"\n  JSON config keys: {list(config.keys())}")
    print(f"  Modules: {len(config['modules'])}")
    print(f"  Connections: {len(config['connections'])}")

    # 导出布局
    layout = gen.export_layout()
    print(f"\n  Layout nodes: {len(layout['nodes'])}")

    # 保存文件
    with tempfile.TemporaryDirectory() as tmpdir:
        json_path = os.path.join(tmpdir, "mesh_4x4.json")
        dot_path = os.path.join(tmpdir, "mesh_4x4.dot")
        layout_path = os.path.join(tmpdir, "mesh_4x4_layout.json")

        config = gen.export_json_config()
        with open(json_path, 'w') as f:
            json.dump(config, f, indent=2)

        gen.export_dot(dot_path)

        layout = gen.export_layout()
        with open(layout_path, 'w') as f:
            json.dump(layout, f, indent=2)

        print(f"\n  Files generated in {tmpdir}:")
        for f in os.listdir(tmpdir):
            print(f"    - {f}")

        # 尝试生成 PNG
        try:
            png_path = os.path.join(tmpdir, "mesh_4x4.png")
            subprocess.run(['dot', '-Tpng', dot_path, '-o', png_path],
                         capture_output=True, check=True)
            print(f"    - mesh_4x4.png (Graphviz)")
        except (subprocess.CalledProcessError, FileNotFoundError):
            print("    (Graphviz not available for PNG generation)")

    print()
    return config


def demo_ring():
    print("=" * 60)
    print("Demo 2: 8-node Ring Topology")
    print("=" * 60)

    gen = TopologyGenerator("ring_8")
    gen.generate_ring(8)

    print(f"  Nodes: {len(gen.graph.nodes)}")
    print(f"  Edges: {len(gen.graph.edges)}")

    config = gen.export_json_config()
    print(f"\n  Modules: {len(config['modules'])}")
    print(f"  Connections: {len(config['connections'])}")

    print()
    return config


def demo_bus():
    print("=" * 60)
    print("Demo 3: 4-node Shared Bus Topology")
    print("=" * 60)

    gen = TopologyGenerator("bus_4")
    gen.generate_bus(4)

    print(f"  Nodes: {len(gen.graph.nodes)}")
    print(f"  Edges: {len(gen.graph.edges)}")

    # Bus 有中心节点 + 4 个处理器节点
    node_types = {}
    for node, attrs in gen.graph.nodes(data=True):
        t = attrs.get('type', 'Unknown')
        node_types[t] = node_types.get(t, 0) + 1
    print(f"  Node types: {node_types}")

    print()
    return gen.export_json_config()


def demo_hierarchical():
    print("=" * 60)
    print("Demo 4: 3-level Hierarchical Tree")
    print("=" * 60)

    gen = TopologyGenerator("hier_3l")
    gen.generate_hierarchical(levels=3, factor=2)

    print(f"  Nodes: {len(gen.graph.nodes)}")
    print(f"  Edges: {len(gen.graph.edges)}")

    # 统计各层节点数
    level_counts = {}
    for node, attrs in gen.graph.nodes(data=True):
        t = attrs.get('type', 'Unknown')
        level_counts[t] = level_counts.get(t, 0) + 1
    print(f"  Node types: {level_counts}")

    print()
    return gen.export_json_config()


def demo_crossbar():
    print("=" * 60)
    print("Demo 5: 4x4 Crossbar Switch")
    print("=" * 60)

    gen = TopologyGenerator("xbar_4x4")
    gen.generate_crossbar(4, 4)

    print(f"  Nodes: {len(gen.graph.nodes)}")
    print(f"  Edges: {len(gen.graph.edges)}")

    # Crossbar: 1 arbiter + 4 inputs + 4 outputs
    node_types = {}
    for node, attrs in gen.graph.nodes(data=True):
        t = attrs.get('type', 'Unknown')
        node_types[t] = node_types.get(t, 0) + 1
    print(f"  Node types: {node_types}")

    print()
    return gen.export_json_config()


def demo_chain_calls():
    print("=" * 60)
    print("Demo 6: Chain Calls (Multiple Topologies)")
    print("=" * 60)

    gen = TopologyGenerator("multi")
    gen.generate_mesh(2, 2).generate_ring(4).generate_bus(2)

    print(f"  Total nodes: {len(gen.graph.nodes)}")
    print(f"  Total edges: {len(gen.graph.edges)}")

    config = gen.export_json_config()
    print(f"  Modules: {len(config['modules'])}")

    print()
    return config


def main():
    print("\n" + "=" * 60)
    print("CppTLM Topology Generator Demo")
    print("=" * 60 + "\n")

    # 运行所有演示
    configs = []
    configs.append(demo_mesh())
    configs.append(demo_ring())
    configs.append(demo_bus())
    configs.append(demo_hierarchical())
    configs.append(demo_crossbar())
    configs.append(demo_chain_calls())

    print("=" * 60)
    print("Summary")
    print("=" * 60)
    print(f"\nGenerated {len(configs)} topology configurations:")
    for i, cfg in enumerate(configs, 1):
        name = cfg.get('name', 'unnamed')
        modules = len(cfg.get('modules', []))
        connections = len(cfg.get('connections', []))
        print(f"  {i}. {name}: {modules} modules, {connections} connections")

    print("\n" + "=" * 60)
    print("Usage with C++ Simulator")
    print("=" * 60)
    print("""
# Generate a topology:
python3 scripts/topology_generator.py --type mesh --size 4x4 \\
    --output configs/my_mesh.json \\
    --dot output/my_mesh.dot

# Run simulation with the generated config:
./build/bin/cpptlm_sim configs/my_mesh.json --stream-stats

# Annotate stats onto the topology:
python3 scripts/stats_annotator.py \\
    --config configs/my_mesh.json \\
    --stats output/stats_stream.jsonl \\
    --dot output/my_mesh.dot \\
    --output report.html
""")


if __name__ == '__main__':
    main()