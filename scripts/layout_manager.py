#!/usr/bin/env python3
"""
layout_manager.py — CppTLM 布局坐标管理器

功能:
  - 布局坐标的导入/导出 (JSON)
  - DOT 属性注入（固定节点位置）
  - 从布局生成简化的 DOT

用法:
    python3 layout_manager.py --load layout.json --save layout.json
    python3 layout_manager.py --load layout.json --set router_0_0 100 200
    python3 layout_manager.py --dot-in topology.dot --dot-out topology_with_pos.dot

@author CppTLM Development Team
@date 2026-04-22
"""

import argparse
import json
import re
import sys
from typing import Dict, Tuple, Optional, Any


class LayoutManager:
    """布局坐标管理器"""

    def __init__(self, layout_file: str = None):
        self.layouts: Dict[str, Dict[str, float]] = {"nodes": {}}
        if layout_file:
            self.load(layout_file)

    def load(self, filepath: str) -> None:
        """从 JSON 文件加载布局"""
        with open(filepath, 'r', encoding='utf-8') as f:
            self.layouts = json.load(f)
        print(f"[OK] Loaded {len(self.layouts.get('nodes', {}))} node positions from {filepath}")

    def save(self, filepath: str) -> None:
        """保存布局到 JSON 文件"""
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self.layouts, f, indent=2, ensure_ascii=False)
        print(f"[OK] Saved layout to {filepath}")

    def set_position(self, node: str, x: float, y: float) -> None:
        """设置节点位置"""
        if "nodes" not in self.layouts:
            self.layouts["nodes"] = {}
        self.layouts["nodes"][node] = {"x": x, "y": y}

    def get_position(self, node: str) -> Tuple[float, float]:
        """获取节点位置"""
        pos = self.layouts.get("nodes", {}).get(node, {})
        return pos.get("x", 0), pos.get("y", 0)

    def list_nodes(self) -> list:
        """列出所有节点"""
        return list(self.layouts.get("nodes", {}).keys())

    def to_dot_attributes(self) -> Dict[str, Dict]:
        """生成 DOT 的 pos 属性（用于 Graphviz 固定布局）"""
        attrs = {}
        for node, pos in self.layouts.get("nodes", {}).items():
            attrs[node] = {"pos": f'{pos["x"]},{pos["y"]}'}
        return attrs

    def export_dot_with_positions(self, dot_input: str, dot_output: str) -> None:
        """
        读取 DOT 文件，注入位置属性，输出新 DOT

        Args:
            dot_input: 输入 DOT 文件
            dot_output: 输出 DOT 文件（带位置）
        """
        with open(dot_input, 'r', encoding='utf-8') as f:
            content = f.read()

        attrs = self.to_dot_attributes()

        # 正则替换节点定义，注入 pos 属性
        for node, attr in attrs.items():
            # 匹配模式: node_name [ 或 node_name[ 后面跟属性
            # 例如: "router_0_0" [label="router_0_0\n(MeshRouter)"
            # 变为: "router_0_0" [pos="0,0", label="router_0_0\n(MesheshRouter)"
            pattern = rf'("{re.escape(node)}"\s*\[[^\]]*)(\])'
            replacement = f'"{node}" [pos="{attr["pos"]}", \\1\\2'
            new_content = re.sub(pattern, replacement, content)
            if new_content != content:
                content = new_content
            else:
                # 尝试更宽松的匹配
                pattern2 = rf'({re.escape(node)})\s*\['
                replacement2 = f'"{node}" [pos="{attr["pos"]}", '
                content = re.sub(pattern2, replacement2, content)

        with open(dot_output, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[OK] DOT with positions saved to {dot_output}")

    def generate_simple_dot(self, output_path: str, title: str = "Layout") -> None:
        """
        从布局坐标生成简化的 DOT 文件

        Args:
            output_path: 输出 DOT 文件路径
            title: 图表标题
        """
        lines = [
            'digraph layout {',
            f'    label="{title}";',
            '    rankdir=LR;',
            '    node [shape=box, style="filled,rounded"];',
            '    edge [color="#666666"];',
            ''
        ]

        # 添加节点
        for node, pos in self.layouts.get("nodes", {}).items():
            x, y = pos["x"], pos["y"]
            lines.append(f'    "{node}" [label="{node}", pos="{x},{y}"];')

        lines.append('}')
        lines.append('')

        with open(output_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        print(f"[OK] Simple DOT saved to {output_path}")

    def scale(self, scale_x: float = 1.0, scale_y: float = 1.0) -> None:
        """缩放布局坐标"""
        for node, pos in self.layouts.get("nodes", {}).items():
            pos["x"] = pos["x"] * scale_x
            pos["y"] = pos["y"] * scale_y

    def translate(self, dx: float = 0.0, dy: float = 0.0) -> None:
        """平移布局坐标"""
        for node, pos in self.layouts.get("nodes", {}).items():
            pos["x"] = pos["x"] + dx
            pos["y"] = pos["y"] + dy

    def auto_arrange_grid(self, cols: int = None) -> None:
        """
        将节点自动排列成网格

        Args:
            cols: 列数，默认为自动计算
        """
        nodes = self.list_nodes()
        if not nodes:
            return

        if cols is None:
            cols = int(len(nodes) ** 0.5)

        for i, node in enumerate(sorted(nodes)):
            row = i // cols
            col = i % cols
            self.set_position(node, col * 100.0, -row * 100.0)


def main():
    parser = argparse.ArgumentParser(
        description="CppTLM Layout Manager - 布局坐标导入/导出工具"
    )

    # 加载/保存
    parser.add_argument("--load", "-l", help="加载布局 JSON 文件")
    parser.add_argument("--save", "-s", help="保存布局 JSON 文件")

    # 节点操作
    parser.add_argument("--set", nargs=3, metavar=("NODE", "X", "Y"),
                       help="设置节点位置，例: --set router_0_0 100 200")
    parser.add_argument("--get", metavar="NODE",
                       help="获取节点位置")
    parser.add_argument("--list", action="store_true",
                       help="列出所有节点")
    parser.add_argument("--auto-arrange", metavar="COLS", type=int,
                       help="自动排列成网格（指定列数）")

    # 缩放/平移
    parser.add_argument("--scale", nargs=2, type=float, metavar=("SX", "SY"),
                       help="缩放坐标，例: --scale 1.5 1.5")
    parser.add_argument("--translate", nargs=2, type=float, metavar=("DX", "DY"),
                       help="平移坐标，例: --translate 50 -50")

    # DOT 转换
    parser.add_argument("--dot-in", help="输入 DOT 文件（注入位置）")
    parser.add_argument("--dot-out", help="输出 DOT 文件（带位置）")
    parser.add_argument("--generate-dot", help="从布局生成简化 DOT")

    args = parser.parse_args()

    # 创建管理器
    manager = LayoutManager(args.load) if args.load else LayoutManager()

    # 设置节点位置
    if args.set:
        node, x, y = args.set
        manager.set_position(node, float(x), float(y))
        print(f"[OK] Set {node} position to ({x}, {y})")

    # 获取节点位置
    if args.get:
        x, y = manager.get_position(args.get)
        print(f"{args.get}: ({x}, {y})")

    # 列出所有节点
    if args.list:
        for node in manager.list_nodes():
            x, y = manager.get_position(node)
            print(f"  {node}: ({x}, {y})")

    # 自动排列
    if args.auto_arrange is not None:
        manager.auto_arrange_grid(args.auto_arrange)
        print(f"[OK] Auto-arranged {len(manager.list_nodes())} nodes into grid with {args.auto_arrange} columns")

    # 缩放
    if args.scale:
        sx, sy = args.scale
        manager.scale(sx, sy)
        print(f"[OK] Scaled by ({sx}, {sy})")

    # 平移
    if args.translate:
        dx, dy = args.translate
        manager.translate(dx, dy)
        print(f"[OK] Translated by ({dx}, {dy})")

    # DOT 转换
    if args.dot_in and args.dot_out:
        manager.export_dot_with_positions(args.dot_in, args.dot_out)

    # 生成简化 DOT
    if args.generate_dot:
        manager.generate_simple_dot(args.generate_dot)

    # 保存
    if args.save:
        manager.save(args.save)


if __name__ == "__main__":
    main()