#!/usr/bin/env python3
"""
generate_vc_priorities.py — Generate configs/vc_priorities_mesh.json.

Tests vc_priorities feature on RouterTLM connections — VC scheduling/priority
had no config example.

Usage:
  python3 examples/generate_vc_priorities.py
  python3 examples/generate_vc_priorities.py --mesh-size 4 --output configs/vc_priorities_mesh.json

Output:
  configs/vc_priorities_mesh.json (2×2 mesh with per-link VC priorities)
"""
import argparse
import json
import os
import sys


def build_vc_priorities_mesh(mesh_size=2):
    """Build NxN mesh with per-link vc_priorities (alternating high/low)."""
    modules = []
    for y in range(mesh_size):
        for x in range(mesh_size):
            modules.append({
                "name": f"router_{x}_{y}",
                "type": "RouterTLM",
                "params": {"node_x": x, "node_y": y, "mesh_x": mesh_size, "mesh_y": mesh_size},
            })

    connections = []
    for y in range(mesh_size):
        for x in range(mesh_size):
            # East-West link (port 1=East, port 3=West in router convention)
            if x + 1 < mesh_size:
                connections.append({
                    "src": f"router_{x}_{y}.1",
                    "dst": f"router_{x + 1}_{y}.3",
                    "latency": 1,
                    "bandwidth": 100,
                    "vc_priorities": [0, 1] if (x + y) % 2 == 0 else [1, 0],
                })
            # North-South link (port 0=North, port 2=South)
            if y + 1 < mesh_size:
                connections.append({
                    "src": f"router_{x}_{y}.2",
                    "dst": f"router_{x}_{y + 1}.0",
                    "latency": 1,
                    "bandwidth": 100,
                    "vc_priorities": [0, 1] if (x + y) % 2 == 0 else [1, 0],
                })

    config = {
        "name": f"vc_priorities_mesh_{mesh_size}x{mesh_size}",
        "description": f"{mesh_size}x{mesh_size} mesh with alternating per-link VC priorities (fills vc_priorities coverage gap)",
        "modules": modules,
        "connections": connections,
    }
    return json.dumps(config, indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description="Generate VC priorities mesh config")
    parser.add_argument("--output", "-o", default="configs/vc_priorities_mesh.json",
                        help="Output JSON path")
    parser.add_argument("--mesh-size", "-s", type=int, default=2,
                        help="Mesh dimension (default: 2 → 2x2)")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w") as f:
        f.write(build_vc_priorities_mesh(args.mesh_size))

    n_routers = args.mesh_size ** 2
    print(f"✓ Wrote {args.output} ({n_routers} routers, {args.mesh_size}x{args.mesh_size} mesh with VC priorities)")


if __name__ == "__main__":
    main()
