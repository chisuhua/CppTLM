#!/usr/bin/env python3
"""scripts/topology_validator.py — CppTLM Topology Validator (wrapper)

This file is now a lightweight wrapper around cpptlm_config.validator.TopologyValidator.
New validation logic should be added to cpptlm_config/validator.py.

Usage:
    python3 scripts/topology_validator.py configs/mesh_2x2.json
    python3 scripts/topology_validator.py configs/mesh_2x2.json -v
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cpptlm_config.validator import TopologyValidator

def main():
    import argparse
    parser = argparse.ArgumentParser(description='CppTLM Topology Validator')
    parser.add_argument('config', nargs='?', default='configs/mesh_2x2.json')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    validator = TopologyValidator.from_json_file(args.config)
    result = validator.validate()

    print(f"\n[TopologyValidator] {args.config}")
    print("=" * 50)

    all_passed = result.is_valid
    for issue in result.errors:
        print(f"  FAIL: [{issue.code}] {issue.message}")
        if issue.suggestion:
            print(f"    Suggestion: {issue.suggestion}")
    for issue in result.warnings:
        print(f"  WARN: [{issue.code}] {issue.message}")

    if all_passed:
        print("  ALL VALIDATIONS PASSED")
    else:
        print("  VALIDATION FAILED")

    print("=" * 50)
    sys.exit(0 if all_passed else 1)

if __name__ == "__main__":
    main()