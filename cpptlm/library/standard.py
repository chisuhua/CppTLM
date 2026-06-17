"""cpptlm/library/standard.py — Standard cluster factory functions

包含: cpu_l1_cluster, memory_cluster, crossbar_cluster.
所有工厂返回 TopoLayer 实例, 模块名自动带 cluster 前缀.
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


def cpu_l1_cluster(idx: int, n_cores: int = 2, l1_size: str = "32KB") -> TopoLayer:
    """创建 CPU+L1 cluster: n_cores 个 CPUTLM + 1 CacheTLM, 各自 core→L1 连接.

    模块名: {prefix}_cpu0, {prefix}_cpu1, ..., {prefix}_l1
    前缀: cluster{idx}
    """
    prefix = f"cluster{idx}"
    l = TopoLayer(name=prefix)
    cpu_names = [f"{prefix}_cpu{i}" for i in range(n_cores)]
    l1_name = f"{prefix}_l1"
    _check_reserved(cpu_names + [l1_name])
    for i, n in enumerate(cpu_names):
        l.add_module(n, "CPUTLM", pattern="SEQUENTIAL", start_addr=f"0x{0x1000 + i * 0x1000:x}")
    l.add_module(l1_name, "CacheTLM", size=l1_size)
    for n in cpu_names:
        l.add_connection(n, l1_name, latency=1)
    return l


def memory_cluster(name: str, n_banks: int = 1) -> TopoLayer:
    """创建 Memory cluster: n_banks 个 MemoryTLM.

    模块名: {name}_mem0, {name}_mem1, ...
    """
    bank_names = [f"{name}_mem{i}" for i in range(n_banks)]
    _check_reserved(bank_names)
    l = TopoLayer(name=name)
    for n in bank_names:
        l.add_module(n, "MemoryTLM", capacity_gb=4, read_latency=50, write_latency=60)
    return l


def crossbar_cluster(n_ports: int = 4, name: str = "xbar") -> TopoLayer:
    """创建 Crossbar cluster: 1 个 CrossbarTLM with metadata.port_count."""
    _check_reserved([name])
    l = TopoLayer(name=f"xbar_wrap_{name}")
    l.metadata["xbar_name"] = name
    l.metadata["port_count"] = n_ports
    l.add_module(name, "CrossbarTLM")
    return l
