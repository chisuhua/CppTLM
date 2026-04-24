#!/usr/bin/env python3
"""
topology_generator.py — CppTLM 拓扑配置生成器

根据拓扑类型自动生成:
- JSON 配置文件 (用于 C++ 仿真)
- 布局 JSON (节点坐标)
- DOT 布局图 (Graphviz)

用法:
    python3 topology_generator.py --type mesh --size 4x4 -o configs/mesh.json
    python3 topology_generator.py --type ring --nodes 8 -o configs/ring.json
    python3 topology_generator.py --type bus --nodes 4 -o configs/bus.json
    python3 topology_generator.py --type hierarchical --levels 3 --factor 2 -o configs/hier.json

依赖:
    pip install networkx pydot

@author CppTLM Development Team
@date 2026-04-22
"""

import argparse
import json
import sys
from typing import Dict, List, Tuple, Optional, Any

try:
    import networkx as nx
except ImportError:
    print("Error: networkx is required. Install with: pip install networkx")
    sys.exit(1)

try:
    import pydot
except ImportError:
    print("Error: pydot is required. Install with: pip install pydot")
    sys.exit(1)


class TopologyGenerator:
    """拓扑生成器 - 根据类型生成模块连接图"""

    # =========================================================================
    # CppTLM 类型映射表
    # =========================================================================
    # 架构文档 (v2.2) 中的抽象类型 → CppTLM 注册表中的实际类型
    # 用于解决 "topology_generator 生成 MeshRouter/Processor，
    # 但注册表只有 Router/CPUSim" 的不匹配问题
    # 参考: docs/architecture/02-complex-topology-architecture.md Section 5.1

    CPPTLM_TYPE_MAP: Dict[str, str] = {
        # 终端类型 (Terminal Types)
        'Processor': 'CPUSim',           # 处理器核
        'Cache': 'CacheTLM',            # 缓存控制器
        'Memory': 'MemoryTLM',          # 内存控制器
        'Directory': 'DirectoryCtrl',  # 目录控制器 (如使用)
        'NetworkInterface': 'NICTLM',   # 网络接口 (Phase 7 NICTLM)

        # 网络类型 (Network Types)
        'Router': 'RouterTLM',          # 通用路由器 (Phase 7 RouterTLM)
        'MeshRouter': 'RouterTLM',      # Mesh 路由器 (Phase 7 RouterTLM 实现 XY 路由)
        'Bus': 'BusSim',               # 共享总线
        'Crossbar': 'CrossbarTLM',     # 交叉开关

        # 遗留类型 (Legacy - 保持兼容)
        'TrafficGen': 'TrafficGenTLM', # 流量生成器
        'Port': 'Port',                # 通用端口
    }

    def __init__(self, name: str = "topology", target: str = "cpptlm"):
        """
        初始化拓扑生成器

        Args:
            name: 拓扑名称
            target: 目标平台 ('cpptlm' | 'gem5')，默认 'cpptlm'
        """
        self.graph = nx.DiGraph()
        self.graph.name = name
        self.layout_coords: Dict[str, Tuple[float, float]] = {}
        self.target = target  # 'cpptlm' 或 'gem5'

    def _map_type(self, abstract_type: str) -> str:
        """
        将抽象类型映射到目标平台的实际类型

        Args:
            abstract_type: 架构文档中的抽象类型 (如 'MeshRouter')

        Returns:
            目标平台的具体类型 (如 'RouterTLM')
        """
        if self.target == "cpptlm":
            return self.CPPTLM_TYPE_MAP.get(abstract_type, abstract_type)
        else:
            # gem5 目标直接返回抽象类型
            return abstract_type

    # =========================================================================
    # 拓扑生成方法
    # =========================================================================

    def generate_mesh(self, rows: int, cols: int, router_type: str = "MeshRouter") -> 'TopologyGenerator':
        """
        生成 2D Mesh NoC 拓扑

        Args:
            rows: 行数
            cols: 列数
            router_type: Router 节点类型

        Returns:
            self (链式调用)

        拓扑结构:
            - 每个网格交点一个 Router
            - 水平和垂直连接
            - 每个 Router 连接一个 NI (NetworkInterface)
        """
        # 创建 Router 节点
        for r in range(rows):
            for c in range(cols):
                node_id = f"router_{r}_{c}"
                self.graph.add_node(node_id, type=router_type)
                # 默认网格布局: x = 列 * 100, y = -行 * 100 (向下)
                self.layout_coords[node_id] = (c * 100.0, -r * 100.0)

        # 创建网格连接 (上下左右)
        for r in range(rows):
            for c in range(cols):
                if c < cols - 1:
                    # 水平连接 (东)
                    self.graph.add_edge(
                        f"router_{r}_{c}", f"router_{r}_{c+1}",
                        latency=1, bandwidth=100
                    )
                if r < rows - 1:
                    # 垂直连接 (南)
                    self.graph.add_edge(
                        f"router_{r}_{c}", f"router_{r+1}_{c}",
                        latency=1, bandwidth=100
                    )

        # 添加 NI (Network Interface) 节点
        for r in range(rows):
            for c in range(cols):
                ni_id = f"ni_{r}_{c}"
                self.graph.add_node(ni_id, type="NetworkInterface")
                # NI 放置在 Router 附近 (右下方偏移)
                self.layout_coords[ni_id] = (c * 100.0 + 30.0, -r * 100.0 - 30.0)

                # NI 到 Router 的连接
                self.graph.add_edge(ni_id, f"router_{r}_{c}", latency=0)
                self.graph.add_edge(f"router_{r}_{c}", ni_id, latency=0)

        # 添加 Processor 节点 (每个 NI 背后一个)
        for r in range(rows):
            for c in range(cols):
                proc_id = f"proc_{r}_{c}"
                self.graph.add_node(proc_id, type="Processor")
                # Processor 放置在 NI 附近
                self.layout_coords[proc_id] = (c * 100.0 + 60.0, -r * 100.0 - 60.0)

                # Processor 到 NI 的连接
                self.graph.add_edge(proc_id, f"ni_{r}_{c}", latency=1)
                self.graph.add_edge(f"ni_{r}_{c}", proc_id, latency=1)

        return self

    def generate_ring(self, num_nodes: int, processor_type: str = "Processor") -> 'TopologyGenerator':
        """
        生成 Ring 总线拓扑

        Args:
            num_nodes: 节点数量
            processor_type: 节点类型

        Returns:
            self (链式调用)

        拓扑结构:
            - 所有节点围成环形
            - 每个节点连接左右邻居
        """
        import math

        for i in range(num_nodes):
            node_id = f"node_{i}"
            self.graph.add_node(node_id, type=processor_type)

            # 圆形布局
            radius = 150.0
            angle = 2 * math.pi * i / num_nodes
            x = radius * math.cos(angle)
            y = radius * math.sin(angle)
            self.layout_coords[node_id] = (x, y)

        # 创建环形连接 (双向)
        for i in range(num_nodes):
            next_i = (i + 1) % num_nodes
            self.graph.add_edge(
                f"node_{i}", f"node_{next_i}",
                latency=2, bandwidth=100
            )
            # 反向连接
            self.graph.add_edge(
                f"node_{next_i}", f"node_{i}",
                latency=2, bandwidth=100
            )

        return self

    def generate_bus(self, num_nodes: int, processor_type: str = "Processor") -> 'TopologyGenerator':
        """
        生成共享总线拓扑

        Args:
            num_nodes: 节点数量
            processor_type: 节点类型

        Returns:
            self (链式调用)

        拓扑结构:
            - 中心 Bus 节点
            - 所有 Processor 连接 Bus
        """
        # 创建总线节点
        self.graph.add_node("bus", type="Bus")
        self.layout_coords["bus"] = (250.0, 0.0)

        # 创建并连接处理器节点
        for i in range(num_nodes):
            node_id = f"node_{i}"
            self.graph.add_node(node_id, type=processor_type)

            # 水平排列
            spacing = 500.0 / (num_nodes + 1)
            self.layout_coords[node_id] = (50.0 + (i + 1) * spacing, 100.0)

            # 双向连接 (处理器 -> 总线 -> 处理器)
            self.graph.add_edge(node_id, "bus", latency=1, bandwidth=10)
            self.graph.add_edge("bus", node_id, latency=1, bandwidth=10)

        return self

    def generate_hierarchical(
        self,
        levels: int,
        factor: int,
        router_type: str = "Router",
        leaf_type: str = "Processor"
    ) -> 'TopologyGenerator':
        """
        生成多层级树状拓扑

        Args:
            levels: 层级数 (root 为 level 0)
            factor: 每个节点的分支数
            router_type: 中间节点类型
            leaf_type: 叶子节点类型

        Returns:
            self (链式调用)

        拓扑结构:
            - Level 0: 1 个根节点
            - Level 1..levels-1: Router 节点
            - Level levels: Processor 叶子节点
        """
        if levels < 1:
            return self

        # Level 0: 根节点
        self.graph.add_node("root", type=router_type)
        self.layout_coords["root"] = (250.0, 0.0)

        current_level_nodes = ["root"]

        for level in range(1, levels + 1):
            next_level_nodes = []
            y = -level * 100.0

            for i, parent in enumerate(current_level_nodes):
                # 计算父节点位置用于参考
                parent_x = self.layout_coords.get(parent, (250, 0))[0]

                for j in range(factor):
                    # 节点 ID
                    node_id = f"l{level}_n{i*factor + j}"

                    # 节点类型: 最后一级是 Processor，其他是 Router
                    node_type = leaf_type if level == levels else router_type
                    self.graph.add_node(node_id, type=node_type)

                    # 布局: 子节点在父节点下方，水平分散
                    num_siblings = len(current_level_nodes) * factor
                    spacing = 500.0 / (num_siblings + 1)
                    x = 50.0 + (i * factor + j + 1) * spacing

                    self.layout_coords[node_id] = (x, y)

                    # 双向连接
                    self.graph.add_edge(parent, node_id, latency=1, bandwidth=100)
                    self.graph.add_edge(node_id, parent, latency=1, bandwidth=100)

                    next_level_nodes.append(node_id)

            current_level_nodes = next_level_nodes

        return self

    def generate_crossbar(
        self,
        num_inputs: int,
        num_outputs: int,
        arbiter_type: str = "Arbiter"
    ) -> 'TopologyGenerator':
        """
        生成 Crossbar 开关拓扑

        Args:
            num_inputs: 输入端口数
            num_outputs: 输出端口数
            arbiter_type: 仲裁器类型

        Returns:
            self (链式调用)

        拓扑结构:
            - 每个输入连接 Arbiter
            - Arbiter 连接所有输出
            - 任意输入可路由到任意输出
        """
        # 添加 Arbiter
        self.graph.add_node("arbiter", type=arbiter_type)
        self.layout_coords["arbiter"] = (250.0, 0.0)

        # 添加输入端口 (左侧)
        for i in range(num_inputs):
            input_id = f"input_{i}"
            self.graph.add_node(input_id, type="Port")
            self.layout_coords[input_id] = (50.0, -50.0 + i * 50.0)
            self.graph.add_edge(input_id, "arbiter", latency=1, bandwidth=100)
            self.graph.add_edge("arbiter", input_id, latency=1, bandwidth=100)

        # 添加输出端口 (右侧)
        for j in range(num_outputs):
            output_id = f"output_{j}"
            self.graph.add_node(output_id, type="Port")
            self.layout_coords[output_id] = (450.0, -50.0 + j * 50.0)
            self.graph.add_edge("arbiter", output_id, latency=1, bandwidth=100)
            self.graph.add_edge(output_id, "arbiter", latency=1, bandwidth=100)

        return self

    # =========================================================================
    # 导入/导出方法
    # =========================================================================

    def export_json_config(self, use_mapping: bool = True) -> Dict:
        """
        导出为 CppTLM JSON 配置格式

        Args:
            use_mapping: 是否使用类型映射 (默认 True)
                         True:  'MeshRouter' → 'RouterTLM'
                         False: 保留原始类型名

        Returns:
            Dict: JSON 配置字典
        """
        modules = []
        for node, attrs in self.graph.nodes(data=True):
            node_type = attrs.get("type", "Unknown")
            if use_mapping and self.target == "cpptlm":
                node_type = self._map_type(node_type)

            modules.append({
                "name": node,
                "type": node_type
            })

        connections = []
        for src, dst, attrs in self.graph.edges(data=True):
            connections.append({
                "src": src,
                "dst": dst,
                "latency": attrs.get("latency", 1),
                "bandwidth": attrs.get("bandwidth", 100)
            })

        return {
            "name": self.graph.name or "generated_topology",
            "description": f"Auto-generated {self.graph.name} topology",
            "modules": modules,
            "connections": connections
        }

    def export_layout(self) -> Dict:
        """
        导出布局坐标

        Returns:
            Dict: 布局 JSON
        """
        return {
            "version": "1.0",
            "generator": "topology_generator.py",
            "nodes": {
                node: {"x": float(coords[0]), "y": float(coords[1])}
                for node, coords in self.layout_coords.items()
            }
        }

    def import_layout(self, layout_json: Dict) -> None:
        """
        导入布局坐标

        Args:
            layout_json: 布局 JSON 字典
        """
        for node, pos in layout_json.get("nodes", {}).items():
            if node in self.layout_coords:
                self.layout_coords[node] = (pos["x"], pos["y"])

    def apply_networkx_layout(self, algo: str = "kamada_kawai") -> None:
        """应用 NetworkX 自动布局算法（需要 numpy）"""
        layouts = {
            "spring": nx.spring_layout,
            "circular": nx.circular_layout,
            "kamada_kawai": nx.kamada_kawai_layout,
            "shell": nx.shell_layout,
            "spectral": nx.spectral_layout,
            "random": nx.random_layout,
        }

        if algo not in layouts:
            algo = "spring"

        try:
            pos = layouts[algo](self.graph)
            self.layout_coords = {node: tuple(v) for node, v in pos.items()}
        except Exception as e:
            print(f"Warning: NetworkX layout '{algo}' failed ({e.__class__.__name__}: {e})")
            print("       Using default layout from topology generation.")

    def export_dot(self, filepath: str, title: Optional[str] = None) -> None:
        """
        导出为 DOT 文件

        Args:
            filepath: 输出文件路径
            title: 图表标题
        """
        # 创建 pydot 图
        dot_graph = pydot.Dot(graph_type='digraph')
        dot_graph.set_rankdir('LR')
        dot_graph.set_label(f'"{title or self.graph.name}\"')
        dot_graph.set_nodesep('0.5')
        dot_graph.set_ranksep('0.75')

        # 节点样式映射
        type_styles = {
            "Processor": {"shape": "box", "style": "filled,bold", "fillcolor": "#E8F5E9", "color": "#4CAF50"},
            "Router": {"shape": "box", "style": "filled,bold", "fillcolor": "#E3F2FD", "color": "#2196F3"},
            "MeshRouter": {"shape": "box", "style": "filled,bold", "fillcolor": "#E3F2FD", "color": "#2196F3"},
            "NetworkInterface": {"shape": "box", "style": "filled", "fillcolor": "#FFF3E0", "color": "#FF9800"},
            "NI": {"shape": "box", "style": "filled", "fillcolor": "#FFF3E0", "color": "#FF9800"},
            "Bus": {"shape": "ellipse", "style": "filled", "fillcolor": "#FCE4EC", "color": "#E91E63"},
            "Arbiter": {"shape": "diamond", "style": "filled", "fillcolor": "#F3E5F5", "color": "#9C27B0"},
            "Port": {"shape": "box", "style": "filled", "fillcolor": "#ECEFF1", "color": "#607D8B"},
        }

        # 添加节点
        for node, attrs in self.graph.nodes(data=True):
            node_type = attrs.get("type", "Unknown")
            label = f"{node}\\n({node_type})"

            # 获取样式
            style = type_styles.get(node_type, {"shape": "box", "style": "filled"})
            fillcolor = style.get("fillcolor", "#FAFAFA")
            color = style.get("color", "#333333")
            shape = style.get("shape", "box")
            style_str = style.get("style", "filled")

            pydot_node = pydot.Node(
                node,
                label=label,
                shape=shape,
                style=style_str,
                fillcolor=fillcolor,
                color=color,
                fontsize=10
            )
            dot_graph.add_node(pydot_node)

            # 设置位置 (如果存在)
            if node in self.layout_coords:
                x, y = self.layout_coords[node]
                pydot_node.set("pos", f"{x},{y}")
                pydot_node.set("fixed_size", "true")

        # 添加边
        for src, dst, attrs in self.graph.edges(data=True):
            latency = attrs.get("latency", 1)
            bandwidth = attrs.get("bandwidth", 100)
            label = f"{latency}cy\\n{bandwidth}GB/s"

            pydot_edge = pydot.Edge(
                src, dst,
                label=label,
                fontsize=8,
                color="#666666",
                fontcolor="#666666"
            )
            dot_graph.add_edge(pydot_edge)

        # 写入文件（优先使用 to_string 避免依赖 graphviz 可执行文件）
        try:
            dot_graph.write(filepath, format='dot')
        except OSError:
            # graphviz 未安装时，手动写入 DOT 格式
            content = dot_graph.to_string()
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
        print(f"[OK] DOT layout saved to {filepath}")


# =============================================================================
# 命令行入口
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="CppTLM Topology Generator - 生成 JSON 配置 + DOT 布局图",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 生成 4x4 Mesh 拓扑
  python3 topology_generator.py --type mesh --size 4x4 -o configs/mesh.json

  # 生成 8 节点 Ring 拓扑
  python3 topology_generator.py --type ring --nodes 8 -o configs/ring.json

  # 生成层次拓扑
  python3 topology_generator.py --type hierarchical --levels 3 --factor 2 -o configs/hier.json

  # 生成 Crossbar
  python3 topology_generator.py --type crossbar --inputs 4 --outputs 4 -o configs/xbar.json
        """
    )

    # 拓扑类型
    parser.add_argument(
        "--type", "-t",
        required=True,
        choices=["mesh", "ring", "bus", "hierarchical", "crossbar"],
        help="拓扑类型"
    )

    # Mesh 参数
    parser.add_argument(
        "--size", "-s",
        default="4x4",
        help="Mesh 尺寸 (格式: WxH, 例: 4x4) (用于 mesh 类型)"
    )

    # Ring/Bus 参数
    parser.add_argument(
        "--nodes", "-n",
        type=int,
        default=8,
        help="节点数量 (用于 ring/bus 类型)"
    )

    # Hierarchical 参数
    parser.add_argument(
        "--levels", "-l",
        type=int,
        default=3,
        help="层级数 (用于 hierarchical 类型)"
    )
    parser.add_argument(
        "--factor", "-f",
        type=int,
        default=2,
        help="分支因子 (用于 hierarchical 类型)"
    )

    # Crossbar 参数
    parser.add_argument(
        "--inputs", "-i",
        type=int,
        default=4,
        help="输入端口数 (用于 crossbar 类型)"
    )
    parser.add_argument(
        "--outputs", "-o",
        type=int,
        default=4,
        help="输出端口数 (用于 crossbar 类型)"
    )

    # 输出文件
    parser.add_argument(
        "--output", "-out",
        required=True,
        help="输出 JSON 配置文件"
    )
    parser.add_argument(
        "--layout",
        help="输出布局 JSON 文件 (可选)"
    )
    parser.add_argument(
        "--dot",
        help="输出 DOT 文件 (可选)"
    )

    # 布局选项
    parser.add_argument(
        "--layout-algo", "-la",
        default="kamada_kawai",
        choices=["spring", "circular", "kamada_kawai", "shell", "spectral", "random"],
        help="布局算法 (默认: kamada_kawai)"
    )

    # 名称
    parser.add_argument(
        "--name",
        default=None,
        help="拓扑名称 (默认: {type}_topology)"
    )

    # 目标平台
    parser.add_argument(
        "--target",
        default="cpptlm",
        choices=["cpptlm", "gem5"],
        help="目标平台类型 (默认: cpptlm)"
    )

    # 禁用类型映射
    parser.add_argument(
        "--no-mapping",
        action="store_true",
        help="禁用类型映射，保留抽象类型名 (MeshRouter/Processor 等)"
    )

    args = parser.parse_args()

    # 创建生成器
    name = args.name or f"{args.type}_topology"
    gen = TopologyGenerator(name=name, target=args.target)

    # 根据类型生成拓扑
    if args.type == "mesh":
        rows, cols = map(int, args.size.split('x'))
        gen.generate_mesh(rows, cols)
        print(f"[OK] Generated {rows}x{cols} Mesh topology")
        print(f"     - {rows * cols} routers")
        print(f"     - {rows * cols} NIs")
        print(f"     - {rows * cols} processors")

    elif args.type == "ring":
        gen.generate_ring(args.nodes)
        print(f"[OK] Generated Ring topology with {args.nodes} nodes")

    elif args.type == "bus":
        gen.generate_bus(args.nodes)
        print(f"[OK] Generated Bus topology with {args.nodes} nodes")

    elif args.type == "hierarchical":
        gen.generate_hierarchical(args.levels, args.factor)
        print(f"[OK] Generated Hierarchical topology")
        print(f"     - Levels: {args.levels}")
        print(f"     - Factor: {args.factor}")

    elif args.type == "crossbar":
        gen.generate_crossbar(args.inputs, args.outputs)
        print(f"[OK] Generated Crossbar topology")
        print(f"     - Inputs: {args.inputs}")
        print(f"     - Outputs: {args.outputs}")

    # 应用布局算法
    if args.layout_algo:
        gen.apply_networkx_layout(args.layout_algo)
        print(f"[OK] Applied '{args.layout_algo}' layout algorithm")

    # 导出配置
    config = gen.export_json_config(use_mapping=not args.no_mapping)
    output_path = args.output
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
    print(f"[OK] Config saved to {output_path}")

    # 导出布局
    if args.layout:
        layout = gen.export_layout()
        with open(args.layout, 'w', encoding='utf-8') as f:
            json.dump(layout, f, indent=2)
        print(f"[OK] Layout saved to {args.layout}")

    # 导出 DOT
    if args.dot:
        gen.export_dot(args.dot)
        print(f"[OK] DOT saved to {args.dot}")

    print(f"\n[Summary]")
    print(f"  Nodes: {gen.graph.number_of_nodes()}")
    print(f"  Edges: {gen.graph.number_of_edges()}")


if __name__ == "__main__":
    main()