"""cpptlm/library — Reusable SoC cluster factory library

预置 cluster 模板 (CPU+L1, Memory, Crossbar, Mesh, Ring) 和 SoC 编排器.
所有 cluster 基于 TopoLayer, 通过 CxxCompatibleEmitter 序列化为 C++ JSON.
"""
from cpptlm.library.standard import (
    cpu_l1_cluster,
    memory_cluster,
    crossbar_cluster,
    cpu_nested_cluster,
    memory_cluster_hierarchical,
    gpu_topology,
)
from cpptlm.library.interconnect import (
    mesh_cluster,
    ring_cluster,
)
from cpptlm.library.soc import SoC

__all__ = [
    "cpu_l1_cluster",
    "memory_cluster",
    "crossbar_cluster",
    "cpu_nested_cluster",
    "memory_cluster_hierarchical",
    "gpu_topology",
    "mesh_cluster",
    "ring_cluster",
    "SoC",
]
