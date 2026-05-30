#!/usr/bin/env python3
"""cpptlm/topo/cli.py — CLI entry point for TopoOrchestrator"""

import argparse
import sys
import os
import json

from cpptlm.topo import (
    TopoOrchestrator, TopoLayer, TopoVariant, TopoVariantSet,
    TopoPatch, ConnectionSelector, ModuleSelector, PatchAction
)


def cmd_gen(args):
    orch = TopoOrchestrator(args.name)

    if args.noc == "mesh":
        noc = TopoLayer("noc")
        for i in range(args.clusters):
            for j in range(args.clusters):
                noc.add_module(f"router_{i}_{j}", "RouterTLM")
        orch.add_layer("noc", layers=[noc])

    cpu_cluster = TopoLayer("cpu_cluster")
    for i in range(args.cpus_per_cluster):
        cpu_cluster.add_module(f"cpu{i}", "CPUTLM")
    orch.add_layer("cpu_cluster", layers=[cpu_cluster])

    orch.add_layer("system", layers=[orch.get_layer("cpu_cluster")])

    output = args.output or f"{args.name}.json"
    orch.save(output)
    print(f"Generated: {output}")


def cmd_variant(args):
    orch = TopoOrchestrator("variant_orch")

    with open(args.base) as f:
        base_data = json.load(f)
    base_layer = TopoLayer.from_dict(base_data)
    orch.add_layer("base", layers=[base_layer])

    for var_name in args.variants:
        orch.add_variant(var_name, "base")

    os.makedirs(args.output_dir, exist_ok=True)
    for name, layer in orch.build_all_variants().items():
        out_path = os.path.join(args.output_dir, f"{name}.json")
        with open(out_path, "w") as f:
            json.dump(layer.to_dict(), f, indent=2)
        print(f"Variant {name}: {out_path}")


def cmd_patch(args):
    with open(args.input) as f:
        data = json.load(f)

    layer = TopoLayer.from_dict(data)

    selector: str = args.selector
    if "*" in selector or "?" in selector:
        sel = ConnectionSelector(selector)
    else:
        sel = ModuleSelector(selector)

    action = PatchAction(args.action)

    if args.action == "rewire":
        rewiring = {}
        if args.dst:
            rewiring["dst"] = args.dst
        if args.src:
            rewiring["src"] = args.src
        patch = TopoPatch(sel, action, rewiring=rewiring)
    else:
        patch = TopoPatch(sel, action)

    result = patch.apply_to(layer)

    with open(args.output, "w") as f:
        json.dump(result.to_dict(), f, indent=2)
    print(f"Patched: {args.output}")


def cmd_compare(args):
    base_layer = TopoLayer.from_dict(json.load(open(args.base)))
    variants = {}

    for var_file in args.variants:
        var_data = json.load(open(var_file))
        var_layer = TopoLayer.from_dict(var_data)
        variants[os.path.basename(var_file)] = var_layer

    print(f"{'Variant':<20} {'Modules':<10} {'Connections':<12} {'Depth':<6}")
    print("-" * 50)
    print(f"{'base':<20} {len(base_layer.modules):<10} {len(base_layer.connections):<12}")

    for name, layer in variants.items():
        depth = 0
        print(f"{name:<20} {len(layer.modules):<10} {len(layer.connections):<12} {depth}")


def main():
    parser = argparse.ArgumentParser(prog="cpptlm.topo.cli")
    sub = parser.add_subparsers(dest="command")

    p_gen = sub.add_parser("gen", help="Generate topology")
    p_gen.add_argument("--name", default="topo", help="Topology name")
    p_gen.add_argument("--output", help="Output JSON file")
    p_gen.add_argument("--noc", choices=["mesh"], help="NoC type")
    p_gen.add_argument("--clusters", type=int, default=2, help="Cluster count")
    p_gen.add_argument("--cpus-per-cluster", type=int, default=4, help="CPUs per cluster")

    p_var = sub.add_parser("variant", help="Generate variants")
    p_var.add_argument("--base", required=True, help="Base topology JSON")
    p_var.add_argument("--variants", nargs="+", required=True, help="Variant names")
    p_var.add_argument("--output-dir", default="variants", help="Output directory")

    p_patch = sub.add_parser("patch", help="Apply patch")
    p_patch.add_argument("--input", required=True, help="Input JSON")
    p_patch.add_argument("--output", required=True, help="Output JSON")
    p_patch.add_argument("--selector", required=True, help="Glob pattern")
    p_patch.add_argument("--action", required=True, choices=["add", "remove", "replace", "rewire"])
    p_patch.add_argument("--src", help="Rewire source")
    p_patch.add_argument("--dst", help="Rewire destination")

    p_cmp = sub.add_parser("compare", help="Compare variants")
    p_cmp.add_argument("--base", required=True, help="Base topology JSON")
    p_cmp.add_argument("--variants", nargs="+", required=True, help="Variant JSON files")

    args = parser.parse_args()

    if args.command == "gen":
        cmd_gen(args)
    elif args.command == "variant":
        cmd_variant(args)
    elif args.command == "patch":
        cmd_patch(args)
    elif args.command == "compare":
        cmd_compare(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()