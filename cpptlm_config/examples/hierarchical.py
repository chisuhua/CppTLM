#!/usr/bin/env python3
"""cpptlm_config/examples/hierarchical.py — Inherit base config example"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from cpptlm_config import ConfigBuilder, ModuleSpec, ModuleType
except ImportError as e:
    print(f"Error: {e}")
    print("Install cpptlm_config: pip install -e cpptlm_config/")
    sys.exit(1)

def main():
    builder = ConfigBuilder(
        name="mesh_4x4_high_freq",
        description="4x4 Mesh with higher CPU frequency"
    )
    builder.set_extends("configs/mesh_4x4_full.json")

    builder.add_module(ModuleSpec(
        name="cpu_0_0",
        type=ModuleType.CPU_TLM,
        params={"frequency": 2000}
    ))

    config = builder.build()
    output_path = "configs/mesh_4x4_high_freq.json"
    config.save(output_path)
    print(f"Generated override config: {output_path}")

if __name__ == "__main__":
    main()