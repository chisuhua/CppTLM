#!/usr/bin/env python3
"""cpptlm_config/examples/mesh_2x2.py — Simple 2x2 mesh example"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from cpptlm_config import ConfigBuilder, TopologyAdapter
except ImportError as e:
    print(f"Error: {e}")
    print("Install cpptlm_config: pip install -e cpptlm_config/")
    sys.exit(1)

def main():
    builder = TopologyAdapter.from_mesh(
        rows=2, cols=2,
        builder=ConfigBuilder(
            name="mesh_2x2",
            description="2x2 Mesh NoC with TLM modules"
        )
    )

    config = builder.build()
    output_path = "configs/mesh_2x2.json"
    config.save(output_path)
    print(f"Generated: {output_path}")

if __name__ == "__main__":
    main()