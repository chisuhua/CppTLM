#!/usr/bin/env python3
"""
generate_hierarchy_tree.py — Generate configs/hierarchy_tree_3level.json.

Tests hierarchy tree parsing (include/core/topology_parser.hh:parse_hierarchy_tree).
Was previously tested only via inline JSON in test_tgms_v4_hierarchy_integration.cc.

Hierarchy format:
{
  "hierarchy": {
    "name": "system",
    "children": [
      {"name": "cluster_a", "children": [{"name": "sub_a0"}, ...]},
      ...
    ]
  },
  "coherence_domains": [...],
  "modules": [...],
  "connections": [...]
}

Usage:
  python3 examples/generate_hierarchy_tree.py
  python3 examples/generate_hierarchy_tree.py --clusters 4 --subclusters 2

Output:
  configs/hierarchy_tree_3level.json
"""
import argparse
import json
import os
import sys


def build_hierarchy(num_clusters=3, num_subclusters=2):
    """Build 3-level hierarchy tree: system → clusters → sub-clusters."""
    children = []
    for c in range(num_clusters):
        sub_children = [{"name": f"sub_{chr(ord('a') + c)}{i}", "children": []}
                        for i in range(num_subclusters)]
        children.append({"name": f"cluster_{chr(ord('a') + c)}", "children": sub_children})

    config = {
        "name": f"hierarchy_tree_{num_clusters}x{num_subclusters}",
        "description": f"{num_clusters}-level hierarchy tree with {num_clusters * num_subclusters} leaf nodes",
        "hierarchy": {
            "name": "system",
            "children": children,
        },
        "coherence_domains": [
            {
                "name": f"domain_{chr(ord('a') + c)}",
                "members": [f"sub_{chr(ord('a') + c)}{i}" for i in range(num_subclusters)],
                "protocol": "MESI",
            }
            for c in range(num_clusters)
        ],
        "modules": (
            [{"name": f"mem_{chr(ord('a') + c)}", "type": "MemoryTLM"}
             for c in range(num_clusters)] +
            [{"name": "xbar", "type": "CrossbarTLM"}]
        ),
        "connections": [
            {"src": f"mem_{chr(ord('a') + c)}", "dst": "xbar", "latency": 10}
            for c in range(num_clusters)
        ],
    }
    return json.dumps(config, indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description="Generate hierarchy tree config")
    parser.add_argument("--output", "-o", default="configs/hierarchy_tree_3level.json",
                        help="Output JSON path")
    parser.add_argument("--clusters", "-c", type=int, default=3,
                        help="Number of top-level clusters (default: 3)")
    parser.add_argument("--subclusters", "-s", type=int, default=2,
                        help="Number of sub-clusters per cluster (default: 2)")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w") as f:
        f.write(build_hierarchy(args.clusters, args.subclusters))

    print(f"✓ Wrote {args.output} ({args.clusters} clusters × {args.subclusters} sub-clusters)")


if __name__ == "__main__":
    main()
