#!/usr/bin/env python3
"""credit_flow.py — Credit-based Flow Control 自动计算工具

功能：根据拓扑、缓冲区深度、链路延迟计算 NoC 中每个 VC 所需的 Credit 数量。
"""

import json
from typing import Dict


def calculate_vc_credits(buffer_depth: int, link_latency: int) -> int:
    """计算单个 VC 所需的 Credit 数量。"""
    return buffer_depth + link_latency * 2


def calculate_mesh_credits(mesh_x: int, mesh_y: int,
                           buffer_depth: int = 8,
                           link_latency: int = 1) -> Dict[str, Dict[str, int]]:
    """计算 2D Mesh 拓扑中所有链路的 Credit 配置。"""
    credits = {}
    vc_credits = calculate_vc_credits(buffer_depth, link_latency)

    for y in range(mesh_y):
        for x in range(mesh_x):
            node_id = y * mesh_x + x
            if x < mesh_x - 1:
                link_name = f"link_{node_id}_east"
                credits[link_name] = {str(vc): vc_credits for vc in range(4)}
            if y < mesh_y - 1:
                link_name = f"link_{node_id}_north"
                credits[link_name] = {str(vc): vc_credits for vc in range(4)}

    return credits


def generate_credit_config(topology_type: str, **kwargs) -> Dict:
    """生成完整的 Credit 配置文件。"""
    if topology_type == "mesh":
        mesh_x = kwargs.get("mesh_x", 2)
        mesh_y = kwargs.get("mesh_y", 2)
        buffer_depth = kwargs.get("buffer_depth", 8)
        link_latency = kwargs.get("link_latency", 1)
        return {
            "topology": "mesh",
            "dimensions": {"x": mesh_x, "y": mesh_y},
            "buffer_depth": buffer_depth,
            "link_latency": link_latency,
            "link_credits": calculate_mesh_credits(mesh_x, mesh_y, buffer_depth, link_latency)
        }
    else:
        raise ValueError(f"Unsupported topology: {topology_type}")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Calculate NoC credit configuration")
    parser.add_argument("--mesh-x", type=int, default=2, help="Mesh X dimension")
    parser.add_argument("--mesh-y", type=int, default=2, help="Mesh Y dimension")
    parser.add_argument("--buffer-depth", type=int, default=8, help="Buffer depth per VC")
    parser.add_argument("--link-latency", type=int, default=1, help="Link latency in cycles")
    parser.add_argument("--output", type=str, default="-", help="Output file (- for stdout)")
    args = parser.parse_args()

    config = generate_credit_config(
        "mesh",
        mesh_x=args.mesh_x,
        mesh_y=args.mesh_y,
        buffer_depth=args.buffer_depth,
        link_latency=args.link_latency
    )

    output = json.dumps(config, indent=2)
    if args.output == "-":
        print(output)
    else:
        with open(args.output, "w") as f:
            f.write(output)