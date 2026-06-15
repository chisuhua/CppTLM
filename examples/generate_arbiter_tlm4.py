#!/usr/bin/env python3
"""
generate_arbiter_tlm4.py — Generate configs/arbiter_tlm4_test.json via ConfigBuilder.

Tests ArbiterTLM4 (ArbiterTLM<4>) — 4-port arbiter registered in chstream_register.hh
but previously had no example config.

Usage:
  python3 examples/generate_arbiter_tlm4.py
  python3 examples/generate_arbiter_tlm4.py --output configs/arbiter_tlm4_test.json

Output:
  configs/arbiter_tlm4_test.json (4 CPU → ArbiterTLM4 → 2 Memory controllers)
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config.builder import ConfigBuilder
from cpptlm_config.models import ModuleSpec, ConnectionSpec


def build_arbiter_tlm4_topology():
    """4 CPU → ArbiterTLM4 (4-port) → 2 MemoryTLM (fan-out from arbiter)."""
    b = (
        ConfigBuilder(
            "arbiter_tlm4_test",
            "4 CPU → ArbiterTLM4 (4-port) → 2 Memory — fills ArbiterTLM<4> coverage gap",
        )
        .add_module(ModuleSpec(name="cpu0", type="CPUTLM"))
        .add_module(ModuleSpec(name="cpu1", type="CPUTLM"))
        .add_module(ModuleSpec(name="cpu2", type="CPUTLM"))
        .add_module(ModuleSpec(name="cpu3", type="CPUTLM"))
        .add_module(ModuleSpec(name="arb4", type="ArbiterTLM4"))
        .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
        .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="cpu0", dst="arb4.0", latency=1))
        .add_connection(ConnectionSpec(src="cpu1", dst="arb4.1", latency=1))
        .add_connection(ConnectionSpec(src="cpu2", dst="arb4.2", latency=1))
        .add_connection(ConnectionSpec(src="cpu3", dst="arb4.3", latency=1))
        .add_connection(ConnectionSpec(src="arb4.0", dst="mem0", latency=50, bandwidth=200))
        .add_connection(ConnectionSpec(src="arb4.1", dst="mem1", latency=50, bandwidth=200))
    )
    return json.dumps(b.build().to_json_dict(), indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description="Generate ArbiterTLM4 config")
    parser.add_argument("--output", "-o", default="configs/arbiter_tlm4_test.json",
                        help="Output JSON path (default: configs/arbiter_tlm4_test.json)")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w") as f:
        f.write(build_arbiter_tlm4_topology())

    print(f"✓ Wrote {args.output}")


if __name__ == "__main__":
    main()
