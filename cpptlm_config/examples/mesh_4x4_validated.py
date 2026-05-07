#!/usr/bin/env python3
"""cpptlm_config/examples/mesh_4x4_validated.py — 4x4 mesh with validation"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from cpptlm_config import ConfigBuilder, TopologyAdapter, TopologyValidator
except ImportError as e:
    print(f"Error: {e}")
    print("Install cpptlm_config: pip install -e cpptlm_config/")
    sys.exit(1)

def main():
    builder = TopologyAdapter.from_mesh(
        rows=4, cols=4,
        builder=ConfigBuilder(
            name="mesh_4x4_full",
            description="4x4 Mesh NoC with validation"
        )
    )

    config = builder.build()

    validator = TopologyValidator(config.to_json_dict())
    result = validator.validate()

    if not result.is_valid:
        print("Validation FAILED:")
        for issue in result.errors:
            print(f"  [{issue.code}] {issue.message}")
            if issue.suggestion:
                print(f"    Suggestion: {issue.suggestion}")
        sys.exit(1)

    for issue in result.warnings:
        print(f"  WARNING [{issue.code}]: {issue.message}")

    output_path = "configs/mesh_4x4_full.json"
    config.save(output_path)
    print(f"Generated and validated: {output_path}")

if __name__ == "__main__":
    main()