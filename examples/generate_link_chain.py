#!/usr/bin/env python3
"""
generate_link_chain.py — Generate configs/link_tlm_chain.json via ConfigBuilder.

Tests LinkTLM module (registered in chstream_register.hh) — had no example config.

Usage:
  python3 examples/generate_link_chain.py
  python3 examples/generate_link_chain.py --output configs/link_tlm_chain.json

Output:
  configs/link_tlm_chain.json (CPU → LinkTLM → LinkTLM → Memory, 3-node chain)
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec


def build_link_chain_topology():
    """CPU → LinkTLM → LinkTLM → MemoryTLM (3-hop chain via 2 LinkTLM hops)."""
    b = (
        ConfigBuilder(
            "link_tlm_chain",
            "3-hop chain via LinkTLM — fills LinkTLM coverage gap",
        )
        .add_module(ModuleSpec(name="cpu0", type="CPUTLM"))
        .add_module(ModuleSpec(name="link0", type="LinkTLM"))
        .add_module(ModuleSpec(name="link1", type="LinkTLM"))
        .add_module(ModuleSpec(name="mem", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="cpu0", dst="link0", latency=2, bandwidth=100))
        .add_connection(ConnectionSpec(src="link0", dst="link1", latency=2, bandwidth=100))
        .add_connection(ConnectionSpec(src="link1", dst="mem", latency=5, bandwidth=100))
    )
    return json.dumps(b.build().to_json_dict(), indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description="Generate LinkTLM chain config")
    parser.add_argument("--output", "-o", default="configs/link_tlm_chain.json",
                        help="Output JSON path")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w") as f:
        f.write(build_link_chain_topology())

    print(f"✓ Wrote {args.output}")


if __name__ == "__main__":
    main()
