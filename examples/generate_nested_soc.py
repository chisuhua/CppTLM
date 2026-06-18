#!/usr/bin/env python3
"""
examples/generate_nested_soc.py — SimModule 多层嵌套 JSON 配置生成示例 (v2.2 Stage 5)

功能:
    使用 cpptlm.library.SoC + cpu_l1_cluster() 等工厂函数
    构造 2 层 CpuCluster 嵌套结构 (顶层 CpuCluster 包含 4 CPUTLM + 1 CacheTLM + 1 MemoryTLM),
    序列化为 C++ ModuleFactory::instantiateAll() 可加载的 JSON 配置。
    同时演示 outputs/inputs 暴露端口的字段生成。

用法:
    python3 examples/generate_nested_soc.py
    → 产出 configs/example_emitter_nested.json

作者: CppTLM Team
日期: 2026-06-18
"""
from __future__ import annotations

import json
import os
import sys
from typing import Any, Dict, List, Optional

# ── 确保包路径 ──────────────────────────────────────────────────────────────
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm.topo.layer import TopoLayer
from cpptlm.topo.emitter import CxxCompatibleEmitter
from cpptlm.library import SoC, cpu_l1_cluster


def _layer_to_simmodule_entry(layer: TopoLayer, type_name: str,
                              params: Dict[str, Any],
                              outputs: Optional[List[Dict[str, str]]] = None,
                              inputs: Optional[List[Dict[str, str]]] = None) -> Dict[str, Any]:
    """将一个 TopoLayer 包装为 SimModule 类型的 module 条目。

    该函数把 TopoLayer 的 modules / connections 字典化为内联 `modules` / `connections`
    字段 (SimModule 嵌套 JSON 规范, see openspec/changes/json-nested-simmodule/specs/simmodule-nested)。
    """
    emitter = CxxCompatibleEmitter()
    inner = emitter.emit(layer)
    entry: Dict[str, Any] = {
        "name": layer.name,
        "type": type_name,
        "params": dict(params),
        "modules": inner.get("modules", []),
        "connections": inner.get("connections", []),
    }
    if outputs:
        entry["outputs"] = list(outputs)
    if inputs:
        entry["inputs"] = list(inputs)
    return entry


def build_nested_cpu_cluster() -> Dict[str, Any]:
    """构建 2 层 CpuCluster 嵌套 JSON 结构。

    结构:
        cluster0 (CpuCluster, num_cpus=4, cluster_id=outer)
        ├── cpu0..cpu3  (CPUTLM × 4)
        ├── cache       (CacheTLM)
        └── mem         (MemoryTLM)

    内部连接: cpu0..cpu3 -> cache -> mem
    暴露端口: cpu0.req_out -> cpu0_to_bus, cpu0.resp_in -> bus_to_cpu0
    """
    # ── 内部层: 用 cpu_l1_cluster 工厂快速创建 (2 cores + 1 L1) ──
    inner_cluster = cpu_l1_cluster(idx=0, n_cores=2, l1_size="32KB")
    # 改名匹配 OpenSpec 任务要求 (cluster0_cpuN / cluster0_l1 -> cpuN / cache)
    rename_map = {
        "cluster0_cpu0": "cpu0",
        "cluster0_cpu1": "cpu1",
        "cluster0_l1": "cache",
    }
    for m in inner_cluster.modules:
        if m.name in rename_map:
            m.name = rename_map[m.name]
    for c in inner_cluster.connections:
        if c.src in rename_map:
            c.src = rename_map[c.src]
        if c.dst in rename_map:
            c.dst = rename_map[c.dst]

    # ── 手动追加 cpu2/cpu3 + mem, 完整 4 cores + 1 cache + 1 mem ──
    inner_cluster.add_module("cpu2", "CPUTLM", pattern="SEQUENTIAL", start_addr="0x3000")
    inner_cluster.add_module("cpu3", "CPUTLM", pattern="SEQUENTIAL", start_addr="0x4000")
    inner_cluster.add_module("mem", "MemoryTLM", capacity_gb=4,
                              read_latency=50, write_latency=60)

    # ── 内部连接 (cpu0/cpu1 已有 cpu_l1_cluster 添加的 cpu->cache 连接, 只需补 cpu2/cpu3) ──
    for cpu in ("cpu2", "cpu3"):
        inner_cluster.add_connection(cpu, "cache", latency=1)
    inner_cluster.add_connection("cache", "mem", latency=10)

    # ── 包装为 CpuCluster 顶层 module 条目 ──
    cluster_entry = _layer_to_simmodule_entry(
        layer=inner_cluster,
        type_name="CpuCluster",
        params={"num_cpus": 4, "cluster_id": "outer"},
        outputs=[{"internal": "cpu0.req_out", "external": "cpu0_to_bus"}],
        inputs=[{"internal": "cpu0.resp_in", "external": "bus_to_cpu0"}],
    )

    return {
        "name": "example_emitter_nested",
        "description": (
            "2-level CpuCluster nesting: cluster0 (CpuCluster) with "
            "4 CPUTLM + 1 CacheTLM + 1 MemoryTLM, exposed via outputs/inputs. "
            "Generated via cpptlm.topo + cpptlm.library."
        ),
        "modules": [cluster_entry],
        "connections": [],
    }


def main() -> None:
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    config = build_nested_cpu_cluster()
    out_path = "configs/example_emitter_nested.json"
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
        f.write("\n")

    # ── 自检: 重新加载确认 JSON 可解析 ──
    with open(out_path) as f:
        loaded = json.load(f)
    assert "modules" in loaded, "top-level 'modules' field missing"
    assert any(m.get("type") == "CpuCluster" for m in loaded["modules"]), \
        "no CpuCluster module emitted"

    cluster = loaded["modules"][0]
    inner_names = sorted(m["name"] for m in cluster.get("modules", []))
    print(f"Generated nested SoC JSON: {out_path}")
    print(f"  top-level modules:    {len(loaded['modules'])}")
    print(f"  CpuCluster type:      {cluster['type']}")
    print(f"  CpuCluster params:    {cluster.get('params')}")
    print(f"  inner modules ({len(inner_names)}): {inner_names}")
    print(f"  inner connections:    {len(cluster.get('connections', []))}")
    print(f"  outputs:              {[o['external'] for o in cluster.get('outputs', [])]}")
    print(f"  inputs:               {[i['external'] for i in cluster.get('inputs', [])]}")


if __name__ == "__main__":
    main()
