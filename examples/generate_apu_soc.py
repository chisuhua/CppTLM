#!/usr/bin/env python3
"""
generate_apu_soc.py — Generate APU SoC configs from docs/soc_arch/specs/apu-soc-design.md.

Generates 3 phase-stage JSON configs reflecting the 6-phase APU rollout:

  --phase 7b  → configs/apu_soc_phase7b.json  (TrafficGen + Cache + Crossbar + Memory, v2.1.0-loadable)
  --phase 7f  → configs/apu_soc_full.json     (Full APU: 4 CU + mesh + TCC + CoherentXBar + HBM, ASPIRATIONAL)

Mirrors docs/soc_arch/specs/apu-soc-design.md §2.2 (final topology)
and §2.3 (per-stage reductions).

Usage:
  python3 examples/generate_apu_soc.py --phase 7b
  python3 examples/generate_apu_soc.py --phase 7f --num-cu 4 --gpu-mesh 2x2
  python3 examples/generate_apu_soc.py --all    # Generate all 3 phases

Phase 7.B uses only currently-registered TLM modules (TrafficGenTLM/CacheTLM/CrossbarTLM/MemoryTLM).
Phase 7.A 已随 `GPUTLM` 归档移除（per change cpptlm-config-type-registration-align T7.1）。
Phase 7.F references ComputeUnitTLM/TCCTLM/CoherentXBarTLM which are planned for
Phase 7.B/C/D implementation — the config documents the target architecture.
"""
import argparse
import json
import os
import sys


def build_apu_phase7b(num_cpu=2):
    """Phase 7.B: TrafficGen CPU + Cache hierarchy + Crossbar + Memory."""
    modules = [
        {"name": f"cpu_{i}", "type": "TrafficGenTLM",
         "params": {"pattern": "SEQUENTIAL" if i == 0 else "RANDOM",
                    "num_requests": 1000,
                    "start_addr": f"0x{0x1000 + i * 0x3000:x}",
                    "end_addr": f"0x{0x4000 + i * 0x4000:x}"}}
        for i in range(num_cpu)
    ]
    modules += [
        {"name": f"l1_cpu_{i}", "type": "CacheTLM"} for i in range(num_cpu)
    ]
    modules.append({"name": "l2_cpu_shared", "type": "CacheTLM"})
    modules.append({"name": "xbar", "type": "CrossbarTLM"})
    modules.append({
        "name": "hbm_memory", "type": "MemoryTLM",
        # ⚠️ 只留被消费的 `capacity_gb`（`read_latency` / `write_latency` 已态②）
        "params": {"capacity_gb": 16},
    })

    connections = []
    for i in range(num_cpu):
        connections.append({"src": f"cpu_{i}", "dst": f"l1_cpu_{i}", "latency": 1})
        connections.append({"src": f"l1_cpu_{i}", "dst": "l2_cpu_shared", "latency": 3})
    connections.append({"src": "l2_cpu_shared", "dst": "xbar.0", "latency": 5})
    connections.append({"src": "xbar", "dst": "hbm_memory", "latency": 100, "bandwidth": 200})

    return {
        "name": "APU SoC — Phase 7.B (CPU + Cache + Crossbar + Memory)",
        "description": f"Phase 7.B: {num_cpu} CPU TrafficGen + L1+L2 caches + "
                       "CrossbarTLM + MemoryTLM. Write-through simplified, bypass coherence. "
                       "Mirrors apu-soc-design.md §2.3 7.B.",
        "modules": modules,
        "connections": connections,
        "hierarchy": {
            "name": "apu_phase7b",
            "children": [
                {"name": "cpu_cluster", "children": [
                    *[{"name": f"cpu_{i}", "children": []} for i in range(num_cpu)],
                    *[{"name": f"l1_cpu_{i}", "children": []} for i in range(num_cpu)],
                    {"name": "l2_cpu_shared", "children": []},
                ]},
                {"name": "interconnect", "children": [{"name": "xbar"}]},
                {"name": "memory_cluster", "children": [{"name": "hbm_memory"}]},
            ],
        },
        "coherence_domains": [{
            "name": "apu_write_through",
            "members": [f"l1_cpu_{i}" for i in range(num_cpu)] + ["l2_cpu_shared"],
            "protocol": "WRITE_THROUGH_SIMPLIFIED",
        }],
    }


def build_apu_full(num_cpu=2, gpu_mesh_size=2):
    """Phase 7.F: Full APU — CPU 侧 + GPU Mesh + CoherentXBar + HBM。

    ⚠️ 2026-11-18（change `cpptlm-config-type-registration-align` T1.4）：`ComputeUnitTLM`（×4）
    与 `TCCTLM`（×1）条目**已移除** —— 两者在 `include/` + `src/` **零类定义**（`ComputeUnitTLM`
    的历史实现已并入 `StreamingMultiprocessorTLM`；`TCCTLM` 从未实现），配置里保留它们只会让
    装配**静默丢弃**该模块。⛔ 「实现它们」路径明确排除（无规格来源，且与本仓已有 SM 分工冲突）。
    拓扑中 GPU 侧仍保留 2×2 mesh（`RouterTLM`），CU 与 TCC 的挂载点待 ArchForge 契约落地后重立项。
    """
    modules = []
    modules += [
        {"name": f"cpu_{i}", "type": "TrafficGenTLM",
         "params": {"pattern": "SEQUENTIAL" if i == 0 else "RANDOM",
                    "num_requests": 10000,
                    "start_addr": f"0x{0x1000 + i * 0x3000:x}",
                    "end_addr": f"0x{0x4000 + i * 0x4000:x}"}}
        for i in range(num_cpu)
    ]
    modules += [
        {"name": f"l1_cpu_{i}", "type": "CacheTLM",
         # ⚠️ `protocol` 已于 2026-11-18 落**态②**（`ADR-X.17` D-2 —— 协议语义不入 CacheTLM，
#     per ADR-CEP-15 的解冻边界）⇒ 生成器停止产出
         "params": {"size": "32KB", "associativity": 4}}
        for i in range(num_cpu)
    ]
    modules.append({"name": "l2_cpu_shared", "type": "CacheTLM",
                    "params": {"size": "256KB", "associativity": 8}})
    for y in range(gpu_mesh_size):
        for x in range(gpu_mesh_size):
            modules.append({
                "name": f"gpu_router_{x}_{y}", "type": "RouterTLM",
                "params": {"node_x": x, "node_y": y, "mesh_x": gpu_mesh_size, "mesh_y": gpu_mesh_size},
            })
    # ⚠️ `snoop_filter` / `domain_protocol` 已于 2026-11-18 落**态②**（`ADR-X.17` D-2）——
    #    `CoherentXBarTLM` 无 filter 概念（snoop 无条件广播）、无协议状态机 ⇒ 写了即装配失败
    #    ⇒ 生成器**停止产出**这两个键（per change cpptlm-module-param-effect-landing A2a）。
    modules.append({"name": "coherent_xbar", "type": "CoherentXBarTLM", "params": {}})
    # ⚠️ `read_latency` / `write_latency` / `type` 已于 2026-11-18 落**态②**（`ADR-X.17` D-2）——
    #    `MemoryTLM` 只消费 `capacity_gb`（其余键零消费面）⇒ 生成器**停止产出**它们
    #    （per change cpptlm-module-param-effect-landing 的 B1 批次）。
    modules.append({"name": "hbm_memory", "type": "MemoryTLM",
                    "params": {"capacity_gb": 16}})

    connections = []
    for i in range(num_cpu):
        connections.append({"src": f"cpu_{i}", "dst": f"l1_cpu_{i}", "latency": 1, "bandwidth": 100})
        connections.append({"src": f"l1_cpu_{i}", "dst": "l2_cpu_shared", "latency": 3, "bandwidth": 100})
    connections.append({"src": "l2_cpu_shared", "dst": "coherent_xbar.0", "latency": 5, "bandwidth": 100})

    for y in range(gpu_mesh_size):
        for x in range(gpu_mesh_size):
            if x + 1 < gpu_mesh_size:
                connections.append({"src": f"gpu_router_{x}_{y}.1", "dst": f"gpu_router_{x+1}_{y}.3",
                                    "latency": 1, "bandwidth": 100})
            if y + 1 < gpu_mesh_size:
                connections.append({"src": f"gpu_router_{x}_{y}.2", "dst": f"gpu_router_{x}_{y+1}.0",
                                    "latency": 1, "bandwidth": 100})
    connections.append({"src": "coherent_xbar", "dst": "hbm_memory", "latency": 100, "bandwidth": 400})

    return {
        "name": f"APU SoC — Phase 7.F Full ({num_cpu}CPU + {gpu_mesh_size}x{gpu_mesh_size} Mesh)",
        "description": f"Phase 7.F APU topology. Mirrors apu-soc-design.md §2.2. "
                       f"GPU 侧仅含 mesh（RouterTLM）+ CoherentXBar + HBM：ComputeUnitTLM / TCCTLM "
                       f"在 CppTLM **无实现**，其条目已移除（见 build_apu_full 的 docstring）。",
        "modules": modules,
        "connections": connections,
        "hierarchy": {
            "name": "apu_phase7f",
            "children": [
                {"name": "cpu_cluster", "children": [
                    *[{"name": f"cpu_{i}", "children": []} for i in range(num_cpu)],
                    *[{"name": f"l1_cpu_{i}", "children": []} for i in range(num_cpu)],
                    {"name": "l2_cpu_shared", "children": []},
                ]},
                {"name": "gpu_cluster", "children": [
                    {"name": "gpu_mesh", "children": [
                        *[{"name": f"gpu_router_{x}_{y}", "children": []}
                          for y in range(gpu_mesh_size) for x in range(gpu_mesh_size)],
                    ]},
                ]},
                {"name": "interconnect", "children": [{"name": "coherent_xbar"}]},
                {"name": "memory_cluster", "children": [{"name": "hbm_memory"}]},
            ],
        },
        "coherence_domains": [{
            "name": "apu_moesi_amd",
            "members": [f"l1_cpu_{i}" for i in range(num_cpu)] + ["l2_cpu_shared"],
            "protocol": "MOESI_AMD_6_STATE",
        }],
    }


def main():
    parser = argparse.ArgumentParser(description="Generate APU SoC topology configs")
    parser.add_argument("--phase", choices=["7b", "7f"], default=None,
                        help="APU phase to generate (default: --all)")
    parser.add_argument("--all", action="store_true", help="Generate all 3 phases")
    parser.add_argument("--output-dir", "-o", default="configs",
                        help="Output directory (default: configs)")
    parser.add_argument("--num-cpu", type=int, default=2,
                        help="Number of CPU TrafficGen (default: 2)")
    parser.add_argument("--gpu-mesh", type=int, default=2,
                        help="GPU mesh dimension N (NxN, default: 2)")
    args = parser.parse_args()

    if not args.all and not args.phase:
        args.all = True

    targets = []
    if args.all:
        targets = [("7b", lambda: build_apu_phase7b(args.num_cpu), "apu_soc_phase7b.json"),
                   ("7f", lambda: build_apu_full(args.num_cpu, args.gpu_mesh), "apu_soc_full.json")]
    else:
        if args.phase == "7b":
            targets = [("7b", lambda: build_apu_phase7b(args.num_cpu), "apu_soc_phase7b.json")]
        elif args.phase == "7f":
            targets = [("7f", lambda: build_apu_full(args.num_cpu, args.gpu_mesh),
                        "apu_soc_full.json")]

    os.makedirs(args.output_dir, exist_ok=True)
    for phase, builder, filename in targets:
        config = builder() if not callable(builder) else builder()
        # If lambda, call it
        if callable(builder):
            config = builder()
        path = os.path.join(args.output_dir, filename)
        with open(path, "w") as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        n_mods = len(config["modules"])
        n_conns = len(config["connections"])
        print(f"✓ Phase {phase} → {path} ({n_mods} modules, {n_conns} connections)")


if __name__ == "__main__":
    main()
