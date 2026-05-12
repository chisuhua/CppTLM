"""cpptlm/visualization/topology.py — 拓扑结构可视化（Graphviz DOT 生成与渲染）。"""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Union


def generate_dot(
    modules: List[Dict],
    connections: List[Dict],
    graph_name: str = "SoCTopology",
    groups: Optional[List[Dict]] = None,
) -> str:
    """根据模块和连接列表生成 Graphviz DOT 字符串。

    Args:
        modules: [{"name": "...", "type": "..."}, ...]
        connections: [{"src": "...", "dst": "...", "latency": N}, ...]
        graph_name: 图名称
        groups: [{"name": "...", "members": [...]}, ...] 用于 subgraph 着色

    Returns:
        DOT 格式字符串
    """
    lines = []
    lines.append(f"digraph {graph_name} {{")
    lines.append("  rankdir=LR;")
    lines.append("  splines=ortho;")
    lines.append("  node [shape=box, style=filled, fillcolor=lightyellow];")
    lines.append("  edge [fontsize=10];")
    lines.append("")

    # ── 模块节点 ──
    module_map: Dict[str, Dict] = {m["name"]: m for m in modules}

    # 按类型着色
    type_colors = {
        "TrafficGenTLM": "lightblue",
        "CacheTLM": "lightgreen",
        "CrossbarTLM": "lightsalmon",
        "MemoryTLM": "plum1",
        "RouterTLM": "lightsalmon",
    }
    default_color = "lightyellow"

    for m in modules:
        name = m["name"]
        mtype = m.get("type", "?")
        color = type_colors.get(mtype, default_color)
        label = f"{name}\\n({mtype})"
        lines.append(f'  "{name}" [label="{label}", fillcolor={color}];')

    lines.append("")

    # ── subgraph 分组着色（仅当 group 中有 ≥2 成员在同一组）──
    if groups:
        group_members: Dict[str, List[str]] = {}
        for g in groups:
            gname = g.get("name", "")
            members = g.get("members", [])
            existing = [m for m in members if m in module_map]
            if len(existing) >= 2:
                group_members[gname] = existing

        for gname, members in group_members.items():
            lines.append(f"  subgraph cluster_{gname} {{")
            lines.append(f'    label="{gname}";')
            lines.append("    style=dashed;")
            lines.append("    color=gray;")
            for m in members:
                lines.append(f'    "{m}";')
            lines.append("  }")
            lines.append("")

    # ── 连接边 ──
    for c in connections:
        src = c["src"]
        dst = c["dst"]
        latency = c.get("latency", "")
        label = f"  [lat={latency}]" if latency else ""
        lines.append(f'  "{src}" -> "{dst}"{label};')

    lines.append("}")
    return "\n".join(lines)


def render_dot(
    dot_string: str,
    output_path: str,
    fmt: str = "png",
) -> bool:
    """将 DOT 字符串渲染为图片（需要系统安装 graphviz 'dot' 命令）。

    Args:
        dot_string: DOT 图定义
        output_path: 输出文件路径（不含扩展名，扩展名由 fmt 决定）
        fmt: 输出格式 (png, svg, pdf)

    Returns:
        渲染成功返回 True
    """
    full_path = f"{output_path}.{fmt}"

    try:
        result = subprocess.run(
            ["dot", f"-T{fmt}", f"-o{full_path}"],
            input=dot_string,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode != 0:
            print(f"  [Warning] dot render failed: {result.stderr.strip()}")
            return False
        return True
    except FileNotFoundError:
        print("  [Warning] 'dot' not found. Install graphviz: apt install graphviz")
        return False
    except subprocess.TimeoutExpired:
        print("  [Warning] dot render timed out")
        return False


def visualize_topology_from_config(
    config_path: str,
    output_dir: str = "configs",
    fmt: str = "png",
) -> Tuple[Optional[str], Optional[str]]:
    """从 JSON 配置文件生成拓扑可视化。

    Args:
        config_path: JSON 配置路径
        output_dir: 输出目录
        fmt: 图片格式

    Returns:
        (dot_path, image_path) — 可能为 None
    """
    with open(config_path) as f:
        data = json.load(f)

    modules = data.get("modules", [])
    connections = data.get("connections", [])
    groups = data.get("module_groups", [])
    name = data.get("name", "topology")

    return _write_visualization(modules, connections, groups, name, output_dir, fmt)


def visualize_topology_from_schema(
    schema,
    output_dir: str = "configs",
    fmt: str = "png",
) -> Tuple[Optional[str], Optional[str]]:
    """从 ConfigSchema 对象生成拓扑可视化。

    Args:
        schema: ConfigSchema 实例（含 .model_dump()）
        output_dir: 输出目录
        fmt: 图片格式

    Returns:
        (dot_path, image_path)
    """
    raw = schema.model_dump()
    modules = raw.get("modules", [])
    connections = raw.get("connections", [])
    groups = raw.get("module_groups", [])
    name = raw.get("name", "topology")

    return _write_visualization(modules, connections, groups, name, output_dir, fmt)


def _write_visualization(
    modules: List[Dict],
    connections: List[Dict],
    groups: List[Dict],
    name: str,
    output_dir: str,
    fmt: str,
) -> Tuple[Optional[str], Optional[str]]:
    """内部：写入 DOT 和图片。"""
    dot = generate_dot(modules, connections, name, groups)

    os.makedirs(output_dir, exist_ok=True)

    dot_path = os.path.join(output_dir, f"{name}.dot")
    with open(dot_path, "w") as f:
        f.write(dot)

    image_path = None
    if render_dot(dot, os.path.join(output_dir, name), fmt=fmt):
        image_path = os.path.join(output_dir, f"{name}.{fmt}")

    return dot_path, image_path
