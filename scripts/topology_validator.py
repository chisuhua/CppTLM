#!/usr/bin/env python3
"""
CppTLM Topology Validator - Task 3.4
VALID-01: Module connectivity
VALID-02: BFS reachability
PORT-01: Router port directions
PORT-03: Bundle type compatibility
"""

import argparse
import json
import re
import sys
from collections import defaultdict, deque
from typing import Dict, List, Set, Tuple, Optional


class TopologyValidator:
    ROUTER_PORT_DIR = {0: 'NORTH', 1: 'EAST', 2: 'SOUTH', 3: 'WEST', 4: 'LOCAL'}

    def __init__(self, config_path: str, verbose: bool = False):
        self.config_path = config_path
        self.verbose = verbose
        self.config = self._load_config()
        self.errors: List[str] = []

    def _load_config(self) -> Dict:
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                content = f.read()
            # Strip // comments for JSON compatibility
            lines = [l for l in content.split('\n') if not l.strip().startswith('//')]
            return json.loads('\n'.join(lines))
        except FileNotFoundError:
            self.errors.append(f"File not found: {self.config_path}")
            return {}
        except json.JSONDecodeError as e:
            self.errors.append(f"Invalid JSON: {e}")
            return {}

    def _log(self, msg: str):
        if self.verbose:
            print(f"  {msg}")

    def validate_connectivity(self) -> bool:
        module_names = {m['name'] for m in self.config.get('modules', [])}
        undefined = set()

        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and src not in module_names:
                undefined.add(src)
            if dst and dst not in module_names:
                undefined.add(dst)

        if undefined:
            self.errors.append(f"VALID-01 FAIL: Undefined modules: {sorted(undefined)}")
            return False

        self._log("  VALID-01 PASS")
        return True

    def validate_reachability(self) -> bool:
        adj = defaultdict(list)
        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and dst:
                adj[src].append(dst)

        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}
        terminals = [n for n, t in module_types.items() if 'Processor' in t or 'NICTLM' in t]

        if len(terminals) < 2:
            return True

        start = terminals[0]
        visited = set()
        queue = deque([start])

        while queue:
            node = queue.popleft()
            if node in visited:
                continue
            visited.add(node)
            for neighbor in adj[node]:
                if neighbor not in visited:
                    queue.append(neighbor)

        unreachable = [t for t in terminals if t not in visited]
        if unreachable:
            self.errors.append(f"VALID-02 FAIL: Unreachable from {start}: {unreachable}")
            return False

        self._log("  VALID-02 PASS")
        return True

    def validate_port_directions(self) -> bool:
        def parse_router_coords(name: str) -> Optional[Tuple[int, int]]:
            m = re.match(r'router_(\d+)_(\d+)', name)
            return (int(m.group(1)), int(m.group(2))) if m else None

        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}
        errors = []

        for conn in self.config.get('connections', []):
            src, dst = conn.get('src', ''), conn.get('dst', '')
            src_mod, src_port = src.split('.')[0], src.split('.')[1] if '.' in src else None
            dst_mod, dst_port = dst.split('.')[0], dst.split('.')[1] if '.' in dst else None

            src_type = module_types.get(src_mod, '')
            dst_type = module_types.get(dst_mod, '')

            if 'Router' not in src_type or 'Router' not in dst_type:
                continue
            if not src_port or not dst_port:
                continue

            try:
                sp, dp = int(src_port), int(dst_port)
            except ValueError:
                continue

            src_coords = parse_router_coords(src_mod)
            dst_coords = parse_router_coords(dst_mod)
            if not src_coords or not dst_coords:
                continue

            sx, sy = src_coords
            dx, dy = dst_coords

            # Determine expected port directions based on relative position
            # Convention: src.port -> dst.port
            # Router ports: NORTH=0, EAST=1, SOUTH=2, WEST=3, LOCAL=4
            #
            # When dst is EAST of src (dx = sx + 1):
            #   src must use EAST(1), dst must use WEST(3)
            if dx == sx + 1 and dy == sy:
                expected = (1, 3)
            # When dst is WEST of src (dx = sx - 1):
            #   src must use WEST(3), dst must use EAST(1)
            elif dx == sx - 1 and dy == sy:
                expected = (3, 1)
            # When dst is SOUTH of src (dy = sy + 1):
            #   src must use SOUTH(2), dst must use NORTH(0)
            elif dx == sx and dy == sy + 1:
                expected = (2, 0)
            # When dst is NORTH of src (dy = sy - 1):
            #   src must use NORTH(0), dst must use SOUTH(2)
            elif dx == sx and dy == sy - 1:
                expected = (0, 2)

            if expected and (sp, dp) != expected:
                errors.append(
                    f"  {src_mod}.{sp}({self.ROUTER_PORT_DIR.get(sp, '?')}) -> "
                    f"{dst_mod}.{dp}({self.ROUTER_PORT_DIR.get(dp, '?')}) expected {expected}"
                )

        if errors:
            self.errors.append("PORT-01 FAIL:")
            self.errors.extend(errors)
            return False

        self._log("  PORT-01 PASS")
        return True

    def validate_bundle_types(self) -> bool:
        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}
        errors = []

        for conn in self.config.get('connections', []):
            src, dst = conn.get('src', ''), conn.get('dst', '')
            src_mod, src_port = src.split('.')[0], src.split('.')[1] if '.' in src else None
            dst_mod, dst_port = dst.split('.')[0], dst.split('.')[1] if '.' in dst else None

            src_type = module_types.get(src_mod, '')
            dst_type = module_types.get(dst_mod, '')

            if 'Router' in src_type and 'NetworkInterface' in dst_type:
                if src_port and dst_port:
                    try:
                        if not (int(src_port) == 4 and int(dst_port) == 1):
                            errors.append(f"  Router-Local(4) must connect to NI-Network(1)")
                    except ValueError:
                        pass

            if 'NetworkInterface' in src_type and 'Router' in dst_type:
                if src_port and dst_port:
                    try:
                        if not (int(src_port) == 1 and int(dst_port) == 4):
                            errors.append(f"  NI-Network(1) must connect to Router-Local(4)")
                    except ValueError:
                        pass

        if errors:
            self.errors.append("PORT-03 FAIL:")
            self.errors.extend(errors)
            return False

        self._log("  PORT-03 PASS")
        return True

    def validate(self) -> bool:
        print(f"\n[TopologyValidator] {self.config_path}")
        print("=" * 50)

        if not self.config:
            print("FAILED: Cannot load config")
            return False

        results = [
            ("VALID-01", self.validate_connectivity()),
            ("VALID-02", self.validate_reachability()),
            ("PORT-01", self.validate_port_directions()),
            ("PORT-03", self.validate_bundle_types()),
        ]

        all_passed = True
        for name, passed in results:
            print(f"  {'PASS' if passed else 'FAIL'}: {name}")
            if not passed:
                all_passed = False

        if self.errors:
            print("\nErrors:")
            for err in self.errors:
                print(f"  {err}")

        print("=" * 50)
        print("ALL VALIDATIONS PASSED" if all_passed else "VALIDATION FAILED")
        return all_passed


def main():
    parser = argparse.ArgumentParser(description='CppTLM Topology Validator')
    parser.add_argument('config', nargs='?', default='configs/mesh_2x2.json')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    validator = TopologyValidator(args.config, verbose=args.verbose)
    success = validator.validate()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()