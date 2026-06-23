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


# =============================================================================
# F5: 高级复合 cluster 工厂 (per docs/superpowers/plans/2026-06-20-future-work-roadmap.md F5)
# =============================================================================


def cpu_nested_cluster(
    num_cores: int = 2,
    l1_count: int = 2,
    l1_size: str = "16KB",
    l2_size: str = "256KB",
    cluster_id: str = "cpu0",
    name: str = "cpu",
) -> TopoLayer:
    """创建 2-level CPU cluster: CpuCluster 含 N cores + CacheCluster (L1xN + L2).

    返回 TopoLayer with:
      - modules: [{name=cluster_id, type=CpuCluster, params={num_cpus, cluster_id, ...}}]
      - sublayer cpu_nested_cache (CacheCluster with l1_count L1 + 1 L2)

    模块命名:
      - {cluster_id}_cpu0, {cluster_id}_cpu1, ..., {cluster_id}_cpu{N-1}
      - {cluster_id}.l2cache (CacheCluster sublayer)

    Args:
        num_cores: CPU 数量 (default 2)
        l1_count: 每 CacheCluster 的 L1 数量 (default 2)
        l1_size: L1 大小 (default "16KB")
        l2_size: L2 大小 (default "256KB")
        cluster_id: cluster_id 参数 (default "cpu0")
        name: 顶层模块名 (default "cpu")

    Returns:
        TopoLayer 含 2-level 嵌套结构
    """
    _check_reserved([name])
    l = TopoLayer(name=f"cpu_nested_{name}")

    # CpuCluster 顶层模块 + sublayers (cpu cores + cache cluster)
    l.add_module(
        name,
        "CpuCluster",
        num_cpus=num_cores,
        cluster_id=cluster_id,
    )
    # 2-level 嵌套通过 sublayer 实现 (CpuCluster 内部含 cpu cores + l2cache sublayer)
    inner = TopoLayer(name=f"{name}_inner")
    cpu_names = [f"{cluster_id}_cpu{i}" for i in range(num_cores)]
    for i, cpu_name in enumerate(cpu_names):
        inner.add_module(cpu_name, "CPUTLM")
    inner.add_sublayer(_cache_cluster_subcluster(
        sub_name=f"{cluster_id}_l2cache",
        l1_count=l1_count,
        l1_size=l1_size,
        l2_size=l2_size,
    ))
    # CpuCluster 内部 connections: cpu{i} → l2cache.l1_{i % l1_count}
    for i, cpu_name in enumerate(cpu_names):
        l1_target = f"{cluster_id}_l2cache.l1_{i % l1_count}"
        inner.add_connection(cpu_name, l1_target, latency=1)
    l.add_sublayer(inner)
    return l


def _cache_cluster_subcluster(
    sub_name: str,
    l1_count: int,
    l1_size: str,
    l2_size: str,
) -> TopoLayer:
    """内部 helper: 生成 CacheCluster sublayer (L1xN + L2)."""
    cl = TopoLayer(name=sub_name)
    cl.add_module(sub_name, "CacheCluster", l1_count=l1_count, l1_size=l1_size, l2_size=l2_size)
    return cl


def memory_cluster_hierarchical(
    channels: int = 4,
    channel_size: str = "1GB",
    memory_type: str = "HBM",
    name: str = "mem",
) -> TopoLayer:
    """创建多通道 hierarchical Memory cluster: N MemoryTLM + Arbiter (per MemoryCluster pattern).

    返回 TopoLayer with:
      - modules: [{name=mem, type=MemoryCluster, params={channel_count, channel_size, memory_type}}]
      - sublayer 含 {channels} 个 MemoryTLM + 1 Arbiter

    模块命名 (在 sublayer 内):
      - mem_channel0, mem_channel1, ..., mem_channel{N-1}
      - mem_arbiter

    Args:
        channels: 通道数 (default 4)
        channel_size: 每通道大小 (default "1GB")
        memory_type: 内存类型 "DDR4" / "HBM" (default "HBM")
        name: MemoryCluster 模块名 (default "mem")

    Returns:
        TopoLayer with hierarchical MemoryCluster
    """
    _check_reserved([name])
    l = TopoLayer(name=f"memory_hier_{name}")

    # MemoryCluster 顶层模块
    l.add_module(
        name,
        "MemoryCluster",
        channel_count=channels,
        channel_size=channel_size,
        memory_type=memory_type,
    )
    # sublayer: N channels + arbiter
    inner = TopoLayer(name=f"{name}_inner")
    for i in range(channels):
        inner.add_module(
            f"{name}_channel{i}",
            "MemoryTLM",
            size=channel_size,
            mem_type=memory_type,
        )
    inner.add_module(f"{name}_arbiter", "ArbiterTLM", n_ports=channels)
    l.add_sublayer(inner)
    return l


def gpu_topology(
    gpc_count: int = 2,
    tpc_per_gpc: int = 2,
    cu_per_tpc: int = 2,
    cu_template: str = "configs/templates/compute_unit_v1.json",
    name: str = "gpu",
) -> TopoLayer:
    """创建 4-level GPU topology: GpuCluster → N*GpcCluster → M*TpcCluster → cu_count*ComputeCluster.

    返回 TopoLayer with:
      - modules: [{name=gpu, type=GpuCluster, params={gpc_count, tpc_per_gpc, cu_per_tpc, cu_template}}]
      - sublayer 含 {gpc_count} GpcCluster (each with {tpc_per_gpc} TpcCluster, each with {cu_per_tpc} ComputeCluster)

    Per F5 §5.3: GPU cu_template 透传 (P1.5 后需要支持 cu_template 透传 — 见 commit e8c2a97).

    Args:
        gpc_count: GPC 数量 (default 2)
        tpc_per_gpc: 每 GPC 的 TPC 数量 (default 2)
        cu_per_tpc: 每 TPC 的 CU 数量 (default 2)
        cu_template: Compute Unit 蓝图 JSON 路径 (per P1.5 cu_template 透传)
        name: GpuCluster 顶层模块名 (default "gpu")

    Returns:
        TopoLayer with 4-level GPU nesting
    """
    _check_reserved([name])
    l = TopoLayer(name=f"gpu_topo_{name}")

    # GpuCluster 顶层
    l.add_module(
        name,
        "GpuCluster",
        gpc_count=gpc_count,
        tpc_per_gpc=tpc_per_gpc,
        cu_per_tpc=cu_per_tpc,
        cu_template=cu_template,
    )
    # 4-level sublayer: gpu → N×gpc → M×tpc → cu_count×compute_grp
    inner = TopoLayer(name=f"{name}_inner")
    for gpc_idx in range(gpc_count):
        gpc_name = f"gpc{gpc_idx}"
        gpc = TopoLayer(name=gpc_name)
        gpc.add_module(gpc_name, "GpcCluster", gpc_id=gpc_idx, tpc_per_gpc=tpc_per_gpc,
                       cu_per_tpc=cu_per_tpc, cu_template=cu_template)
        for tpc_idx in range(tpc_per_gpc):
            tpc_name = f"{gpc_name}_tpc{tpc_idx}"
            tpc = TopoLayer(name=tpc_name)
            tpc.add_module(tpc_name, "TpcCluster", tpc_id=tpc_idx,
                           cu_per_tpc=cu_per_tpc, cu_template=cu_template)
            compute_grp = TopoLayer(name=f"{tpc_name}_compute_grp")
            compute_grp.add_module(
                f"{tpc_name}_compute_grp", "ComputeCluster",
                cu_template=cu_template, cu_count=cu_per_tpc,
            )
            tpc.add_sublayer(compute_grp)
            gpc.add_sublayer(tpc)
        inner.add_sublayer(gpc)
    l.add_sublayer(inner)
    return l
