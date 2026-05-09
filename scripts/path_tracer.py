#!/usr/bin/env python3
"""path_tracer.py — NoC 路径追踪器"""

from typing import List, Tuple
from dataclasses import dataclass


@dataclass
class Hop:
    from_node: str
    to_node: str
    direction: str
    latency: int = 1


class PathTracer:
    def __init__(self, mesh_x: int, mesh_y: int):
        self.mesh_x = mesh_x
        self.mesh_y = mesh_y

    def node_to_coord(self, node_id: int) -> Tuple[int, int]:
        return (node_id % self.mesh_x, node_id // self.mesh_x)

    def coord_to_node(self, x: int, y: int) -> int:
        return y * self.mesh_x + x

    def trace_xy(self, src: int, dst: int) -> List[Hop]:
        hops = []
        sx, sy = self.node_to_coord(src)
        dx, dy = self.node_to_coord(dst)
        cx, cy = sx, sy
        current = src

        while cx < dx:
            next_node = self.coord_to_node(cx + 1, cy)
            hops.append(Hop(str(current), str(next_node), "EAST"))
            cx += 1
            current = next_node
        while cx > dx:
            next_node = self.coord_to_node(cx - 1, cy)
            hops.append(Hop(str(current), str(next_node), "WEST"))
            cx -= 1
            current = next_node

        while cy < dy:
            next_node = self.coord_to_node(cx, cy + 1)
            hops.append(Hop(str(current), str(next_node), "NORTH"))
            cy += 1
            current = next_node
        while cy > dy:
            next_node = self.coord_to_node(cx, cy - 1)
            hops.append(Hop(str(current), str(next_node), "SOUTH"))
            cy -= 1
            current = next_node

        return hops

    def total_latency(self, src: int, dst: int, link_latency: int = 1) -> int:
        hops = self.trace_xy(src, dst)
        router_latency = 6
        return len(hops) * (link_latency + router_latency)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Trace NoC path")
    parser.add_argument("--mesh-x", type=int, required=True)
    parser.add_argument("--mesh-y", type=int, required=True)
    parser.add_argument("--src", type=int, required=True)
    parser.add_argument("--dst", type=int, required=True)
    args = parser.parse_args()

    tracer = PathTracer(args.mesh_x, args.mesh_y)
    hops = tracer.trace_xy(args.src, args.dst)

    print(f"Path from {args.src} to {args.dst}:")
    for hop in hops:
        print(f"  {hop.from_node} --[{hop.direction}]--> {hop.to_node}")
    print(f"Total hops: {len(hops)}")
    print(f"Estimated latency: {tracer.total_latency(args.src, args.dst)} cycles")


if __name__ == "__main__":
    main()