"""cpptlm/library/interconnect.py — Interconnect cluster factories

包含: mesh_cluster, ring_cluster.
"""
from __future__ import annotations
from typing import List

from cpptlm.topo.layer import TopoLayer


RESERVED = {
    "groups", "modules", "connections", "hierarchy",
    "coherence_domains", "name", "description", "extends",
    "include", "plugin", "version", "metadata",
}


def _check_reserved(names: List[str]) -> None:
    for n in names:
        if n in RESERVED:
            raise ValueError(f"reserved module name '{n}'")


def mesh_cluster(rows: int, cols: int, name_prefix: str = "noc") -> TopoLayer:
    """创建 2D mesh: rows*cols RouterTLM, XY 路由连接.

    模块名: {prefix}_router_{x}_{y}, x∈[0,cols), y∈[0,rows)
    连接: 每个 router 向右 + 向下连 (north=0, east=1, south=2, west=3, local=4)
    """
    if rows < 1 or cols < 1:
        raise ValueError(f"mesh dimensions must be positive, got rows={rows} cols={cols}")
    l = TopoLayer(name=f"{name_prefix}_mesh")
    router_names: List[List[str]] = []
    for y in range(rows):
        row: List[str] = []
        for x in range(cols):
            n = f"{name_prefix}_router_{x}_{y}"
            row.append(n)
        router_names.append(row)
    flat_names = [n for row in router_names for n in row]
    _check_reserved(flat_names)
    for y in range(rows):
        for x in range(cols):
            n = router_names[y][x]
            l.add_module(n, "RouterTLM", node_x=x, node_y=y, mesh_x=cols, mesh_y=rows)
    for y in range(rows):
        for x in range(cols):
            cur = router_names[y][x]
            if x + 1 < cols:
                right = router_names[y][x + 1]
                l.add_connection(f"{cur}.1", f"{right}.3", latency=1)
            if y + 1 < rows:
                down = router_names[y + 1][x]
                l.add_connection(f"{cur}.2", f"{down}.0", latency=1)
    return l


def ring_cluster(n_nodes: int, name_prefix: str = "ring") -> TopoLayer:
    """创建 ring: n_nodes RouterTLM, 每节点向后继连 1 边.

    模块名: {prefix}_node_{i}, i∈[0, n_nodes)
    """
    if n_nodes < 2:
        raise ValueError(f"ring requires at least 2 nodes, got n_nodes={n_nodes}")
    l = TopoLayer(name=f"{name_prefix}_ring")
    node_names = [f"{name_prefix}_node_{i}" for i in range(n_nodes)]
    _check_reserved(node_names)
    for i, n in enumerate(node_names):
        l.add_module(n, "RouterTLM", node_id=i, mesh_x=n_nodes, mesh_y=1)
    for i, n in enumerate(node_names):
        nxt = node_names[(i + 1) % n_nodes]
        l.add_connection(n, nxt, latency=1)
    return l
