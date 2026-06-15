#!/usr/bin/env python3
"""
generate_apu_soc.py — Generate APU SoC configs from docs/soc_arch/specs/apu-soc-design.md.

Generates 3 phase-stage JSON configs reflecting the 6-phase APU rollout:

  --phase 7a  → configs/apu_soc_phase7a.json  (GPUTLM + MemoryTLM direct, v2.1.0-loadable)
  --phase 7b  → configs/apu_soc_phase7b.json  (TrafficGen + Cache + Crossbar + GPUTLM + Memory, v2.1.0-loadable)
  --phase 7f  → configs/apu_soc_full.json     (Full APU: 4 CU + mesh + TCC + CoherentXBar + HBM, ASPIRATIONAL)

Mirrors docs/soc_arch/specs/apu-soc-design.md §2.2 (final topology)
and §2.3 (per-stage reductions).

Usage:
  python3 examples/generate_apu_soc.py --phase 7b
  python3 examples/generate_apu_soc.py --phase 7f --num-cu 4 --gpu-mesh 2x2
  python3 examples/generate_apu_soc.py --all    # Generate all 3 phases

Phase 7.A/B use only currently-registered TLM modules (GPUTLM/CacheTLM/CrossbarTLM/MemoryTLM).
Phase 7.F references ComputeUnitTLM/TCCTLM/CoherentXBarTLM which are planned for
Phase 7.B/C/D implementation — the config documents the target architecture.
"""
import argparse
import json
import os
import sys


def build_apu_phase7a():
    """Phase 7.A: GPUTLM (ComputeUnit v0) + MemoryTLM direct (bypass cache/coherence)."""
    return {
        "name": "APU SoC — Phase 7.A (GPUTLM + MemoryTLM direct)",
        "description": "Phase 7.A: GPUTLM (ComputeUnitTLM v0 black-box) + MemoryTLM (HBM-like). "
                       "Bypass Cache/Crossbar/Coherence. Mirrors apu-soc-design.md §2.3 7.A.",
        "modules": [
            {"name": "gpu_unit_0", "type": "GPUTLM",
             "params": {"kernel_id": 0, "workgroup_size": 64,
                        "start_addr": "0x10000", "end_addr": "0x80000"}},
            {"name": "hbm_memory", "type": "MemoryTLM",
             "params": {"capacity_gb": 8, "read_latency": 100, "write_latency": 120}},
        ],
        "connections": [
            {"src": "gpu_unit_0", "dst": "hbm_memory", "latency": 100, "bandwidth": 200}
        ],
        "hierarchy": {
            "name": "apu_phase7a",
            "children": [
                {"name": "compute_cluster", "children": [{"name": "gpu_unit_0"}]},
                {"name": "memory_cluster", "children": [{"name": "hbm_memory"}]},
            ],
        },
        "coherence_domains": [
            {"name": "apu_phase7a_domain", "members": ["gpu_unit_0", "hbm_memory"], "protocol": "NONE"}
        ],
    }


def build_apu_phase7b(num_cpu=2):
    """Phase 7.B: TrafficGen CPU + Cache hierarchy + GPUTLM CU + Crossbar + Memory."""
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
    modules.append({
        "name": "compute_unit_0", "type": "GPUTLM",
        "params": {"kernel_id": 100, "workgroup_size": 64,
                   "start_addr": "0x10000", "end_addr": "0x40000"},
    })
    modules.append({"name": "xbar", "type": "CrossbarTLM"})
    modules.append({
        "name": "hbm_memory", "type": "MemoryTLM",
        "params": {"capacity_gb": 16, "read_latency": 100, "write_latency": 120},
    })

    connections = []
    for i in range(num_cpu):
        connections.append({"src": f"cpu_{i}", "dst": f"l1_cpu_{i}", "latency": 1})
        connections.append({"src": f"l1_cpu_{i}", "dst": "l2_cpu_shared", "latency": 3})
    connections.append({"src": "l2_cpu_shared", "dst": "xbar.0", "latency": 5})
    connections.append({"src": "compute_unit_0", "dst": "xbar.1", "latency": 5})
    connections.append({"src": "xbar", "dst": "hbm_memory", "latency": 100, "bandwidth": 200})

    return {
        "name": "APU SoC — Phase 7.B (CPU + Cache + GPUTLM + Crossbar + Memory)",
        "description": f"Phase 7.B: {num_cpu} CPU TrafficGen + L1+L2 caches + 1 GPUTLM CU + "
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
                {"name": "compute_cluster", "children": [{"name": "compute_unit_0"}]},
                {"name": "interconnect", "children": [{"name": "xbar"}]},
                {"name": "memory_cluster", "children": [{"name": "hbm_memory"}]},
            ],
        },
        "coherence_domains": [{
            "name": "apu_write_through",
            "members": [f"l1_cpu_{i}" for i in range(num_cpu)] + ["l2_cpu_shared", "compute_unit_0"],
            "protocol": "WRITE_THROUGH_SIMPLIFIED",
        }],
    }


def build_apu_full(num_cpu=2, num_cu=4, gpu_mesh_size=2):
    """Phase 7.F: Full APU — ComputeUnitTLM × N + GPU Mesh + TCC + CoherentXBar + HBM.
    NOTE: ComputeUnitTLM/TCCTLM/CoherentXBarTLM are NOT yet registered (Phase 7.B/C/D).
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
         "params": {"size": "32KB", "associativity": 4, "protocol": "MOESI_AMD"}}
        for i in range(num_cpu)
    ]
    modules.append({"name": "l2_cpu_shared", "type": "CacheTLM",
                    "params": {"size": "256KB", "associativity": 8, "protocol": "MOESI_AMD"}})
    modules += [
        {"name": f"compute_unit_{i}", "type": "ComputeUnitTLM",
         "params": {"kernel_id": i, "workgroup_size": 64}}
        for i in range(num_cu)
    ]
    for y in range(gpu_mesh_size):
        for x in range(gpu_mesh_size):
            modules.append({
                "name": f"gpu_router_{x}_{y}", "type": "RouterTLM",
                "params": {"node_x": x, "node_y": y, "mesh_x": gpu_mesh_size, "mesh_y": gpu_mesh_size},
            })
    modules.append({"name": "tcc", "type": "TCCTLM",
                    "params": {"size": "1MB", "coalescing": True}})
    modules.append({"name": "coherent_xbar", "type": "CoherentXBarTLM",
                    "params": {"snoop_filter": True, "domain_protocol": "MOESI_AMD"}})
    modules.append({"name": "hbm_memory", "type": "MemoryTLM",
                    "params": {"capacity_gb": 16, "read_latency": 100,
                               "write_latency": 120, "type": "HBM2"}})

    connections = []
    for i in range(num_cpu):
        connections.append({"src": f"cpu_{i}", "dst": f"l1_cpu_{i}", "latency": 1, "bandwidth": 100})
        connections.append({"src": f"l1_cpu_{i}", "dst": "l2_cpu_shared", "latency": 3, "bandwidth": 100})
    connections.append({"src": "l2_cpu_shared", "dst": "coherent_xbar.0", "latency": 5, "bandwidth": 100})

    for i in range(num_cu):
        cu_x = i % gpu_mesh_size
        cu_y = i // gpu_mesh_size
        connections.append({
            "src": f"compute_unit_{i}", "dst": f"gpu_router_{cu_x}_{cu_y}.4",
            "latency": 1, "bandwidth": 100,
        })

    for y in range(gpu_mesh_size):
        for x in range(gpu_mesh_size):
            if x + 1 < gpu_mesh_size:
                connections.append({"src": f"gpu_router_{x}_{y}.1", "dst": f"gpu_router_{x+1}_{y}.3",
                                    "latency": 1, "bandwidth": 100})
            if y + 1 < gpu_mesh_size:
                connections.append({"src": f"gpu_router_{x}_{y}.2", "dst": f"gpu_router_{x}_{y+1}.0",
                                    "latency": 1, "bandwidth": 100})
    connections.append({"src": "tcc", "dst": "coherent_xbar.1", "latency": 5, "bandwidth": 200})
    connections.append({"src": "coherent_xbar", "dst": "hbm_memory", "latency": 100, "bandwidth": 400})

    return {
        "name": f"APU SoC — Phase 7.F Full ({num_cpu}CPU + {num_cu}CU + {gpu_mesh_size}x{gpu_mesh_size} Mesh)",
        "description": f"Phase 7.F final APU topology. Mirrors apu-soc-design.md §2.2. "
                       f"NOTE: ComputeUnitTLM/CoherentXBarTLM/TCCTLM not yet registered "
                       f"(planned Phase 7.B/C/D).",
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
                    *[{"name": f"compute_unit_{i}", "children": []} for i in range(num_cu)],
                    {"name": "tcc", "children": []},
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
            "members": [f"l1_cpu_{i}" for i in range(num_cpu)] + ["l2_cpu_shared", "tcc"],
            "protocol": "MOESI_AMD_6_STATE",
        }],
    }


def main():
    parser = argparse.ArgumentParser(description="Generate APU SoC topology configs")
    parser.add_argument("--phase", choices=["7a", "7b", "7f"], default=None,
                        help="APU phase to generate (default: --all)")
    parser.add_argument("--all", action="store_true", help="Generate all 3 phases")
    parser.add_argument("--output-dir", "-o", default="configs",
                        help="Output directory (default: configs)")
    parser.add_argument("--num-cpu", type=int, default=2,
                        help="Number of CPU TrafficGen (default: 2)")
    parser.add_argument("--num-cu", type=int, default=4,
                        help="Number of ComputeUnit (Phase 7.F only, default: 4)")
    parser.add_argument("--gpu-mesh", type=int, default=2,
                        help="GPU mesh dimension N (NxN, default: 2)")
    args = parser.parse_args()

    if not args.all and not args.phase:
        args.all = True

    targets = []
    if args.all:
        targets = [("7a", build_apu_phase7a, "apu_soc_phase7a.json"),
                   ("7b", lambda: build_apu_phase7b(args.num_cpu), "apu_soc_phase7b.json"),
                   ("7f", lambda: build_apu_full(args.num_cpu, args.num_cu, args.gpu_mesh), "apu_soc_full.json")]
    else:
        if args.phase == "7a":
            targets = [("7a", build_apu_phase7a, "apu_soc_phase7a.json")]
        elif args.phase == "7b":
            targets = [("7b", lambda: build_apu_phase7b(args.num_cpu), "apu_soc_phase7b.json")]
        elif args.phase == "7f":
            targets = [("7f", lambda: build_apu_full(args.num_cpu, args.num_cu, args.gpu_mesh),
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
