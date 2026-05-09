#!/usr/bin/env python3
"""cpptlm_config/validator.py — Topology validation with two-stage validation"""

from pydantic import BaseModel
from typing import List, Optional, Set, Dict
from collections import defaultdict, deque
import json
import re
import os
import glob as _glob


_PARAM_RULES_CACHE: Dict[str, Dict] = {}


def load_param_rules() -> Dict[str, Dict]:
    """Load all param_rules from configs/param_rules/*.json

    Returns:
        Dict mapping module_type string (e.g. "RouterTLM") -> rules dict.
        Handles both "INTEGER" and "INT" type strings (cross-phase compatibility).
    """
    if _PARAM_RULES_CACHE:
        return _PARAM_RULES_CACHE

    rules_dir = os.path.join(os.path.dirname(__file__), '..', 'configs', 'param_rules')
    for json_path in _glob.glob(os.path.join(rules_dir, '*.json')):
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            module_type = data.get('module_type', '')
            if module_type:
                _PARAM_RULES_CACHE[module_type.upper()] = data
        except (json.JSONDecodeError, OSError):
            pass
    return _PARAM_RULES_CACHE


class ValidationIssue(BaseModel):
    severity: str  # "error", "warning", "info"
    code: str      # "VALID-01", "PORT-01", etc.
    message: str
    suggestion: Optional[str] = None

class ValidationResult(BaseModel):
    is_valid: bool = True
    errors: List[ValidationIssue] = []
    warnings: List[ValidationIssue] = []

    def add_error(self, code: str, message: str, suggestion: Optional[str] = None):
        self.errors.append(ValidationIssue(
            severity="error", code=code, message=message, suggestion=suggestion
        ))
        self.is_valid = False

    def add_warning(self, code: str, message: str, suggestion: Optional[str] = None):
        self.warnings.append(ValidationIssue(
            severity="warning", code=code, message=message, suggestion=suggestion
        ))

class TopologyValidator:
    """Topology validator with VALID-01/02 and PORT-01/03 checks"""

    ROUTER_PORT_DIR = {0: 'NORTH', 1: 'EAST', 2: 'SOUTH', 3: 'WEST', 4: 'LOCAL'}

    def __init__(self, config_data: Dict):
        self.config = config_data
        self.result = ValidationResult()

    def validate_connectivity(self) -> "TopologyValidator":
        """VALID-01: All connected modules must be defined"""
        module_names = {m['name'] for m in self.config.get('modules', [])}
        undefined: Set[str] = set()

        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and src not in module_names:
                undefined.add(src)
            if dst and dst not in module_names:
                undefined.add(dst)

        if undefined:
            self.result.add_error(
                code="VALID-01",
                message=f"Undefined modules in connections: {sorted(undefined)}",
                suggestion="Add these modules to the modules list or fix connection src/dst"
            )
        return self

    def validate_reachability(self) -> "TopologyValidator":
        """VALID-02: BFS reachability from one terminal to all others"""
        adj = defaultdict(list)
        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and dst:
                adj[src].append(dst)

        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}
        terminals = [n for n, t in module_types.items()
                     if 'Processor' in t or 'NICTLM' in t or 'CPU' in t]

        if len(terminals) < 2:
            return self

        start = terminals[0]
        visited: Set[str] = set()
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
            self.result.add_error(
                code="VALID-02",
                message=f"Unreachable terminals from {start}: {unreachable}",
                suggestion="Check connectivity graph"
            )
        return self

    def validate_port_directions(self) -> "TopologyValidator":
        """PORT-01: Router port directions match physical layout"""
        def parse_router_coords(name: str):
            m = re.match(r'router_(\d+)_(\d+)', name)
            return (int(m.group(1)), int(m.group(2))) if m else None

        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}

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
            expected: Optional[tuple] = None

            if dx == sx + 1 and dy == sy:
                expected = (1, 3)
            elif dx == sx - 1 and dy == sy:
                expected = (3, 1)
            elif dx == sx and dy == sy + 1:
                expected = (2, 0)
            elif dx == sx and dy == sy - 1:
                expected = (0, 2)

            if expected and (sp, dp) != expected:
                self.result.add_warning(
                    code="PORT-01",
                    message=f"{src_mod}.{sp}({self.ROUTER_PORT_DIR.get(sp, '?')}) -> "
                            f"{dst_mod}.{dp}({self.ROUTER_PORT_DIR.get(dp, '?')}): "
                            f"expected port pair {expected}",
                    suggestion=f"For {src_mod}->{dst_mod}, use port pair {expected}"
                )
        return self

    def validate_bundle_types(self) -> "TopologyValidator":
        """PORT-03: Router-Local(4) must connect to NI-Network(1)"""
        module_types = {m['name']: m.get('type', '') for m in self.config.get('modules', [])}

        for conn in self.config.get('connections', []):
            src, dst = conn.get('src', ''), conn.get('dst', '')
            src_mod, src_port = src.split('.')[0], src.split('.')[1] if '.' in src else None
            dst_mod, dst_port = dst.split('.')[0], dst.split('.')[1] if '.' in dst else None

            src_type = module_types.get(src_mod, '')
            dst_type = module_types.get(dst_mod, '')

            if 'Router' in src_type and 'NICTLM' in dst_type:
                if src_port and dst_port:
                    try:
                        if not (int(src_port) == 4 and int(dst_port) == 1):
                            self.result.add_error(
                                code="PORT-03",
                                message=f"Router-Local(4) must connect to NI-Network(1), "
                                        f"got {src_mod}.{src_port} -> {dst_mod}.{dst_port}",
                                suggestion=f"Use port 4 on Router and port 1 on NICTLM"
                            )
                    except ValueError:
                        pass

            if 'NICTLM' in src_type and 'Router' in dst_type:
                if src_port and dst_port:
                    try:
                        if not (int(src_port) == 1 and int(dst_port) == 4):
                            self.result.add_error(
                                code="PORT-03",
                                message=f"NI-Network(1) must connect to Router-Local(4), "
                                        f"got {src_mod}.{src_port} -> {dst_mod}.{dst_port}",
                                suggestion=f"Use port 1 on NICTLM and port 4 on Router"
                            )
                    except ValueError:
                        pass
        return self

    def validate_required_params(self) -> "TopologyValidator":
        """PARAM-01: All required parameters must be present for each module"""
        rules = load_param_rules()
        for mod in self.config.get('modules', []):
            mod_type = mod.get('type', '')
            mod_name = mod.get('name', '')
            mod_rules = rules.get(mod_type.upper(), {})
            for param_name, rule in mod_rules.get('rules', {}).items():
                if rule.get('required', False):
                    params = mod.get('params', {})
                    if param_name not in params or params[param_name] is None:
                        self.result.add_error(
                            code="PARAM-01",
                            message=f"Module '{mod_name}' ({mod_type}) missing required parameter: {param_name}",
                            suggestion=f"Add '{param_name}' to params dict"
                        )
        return self

    def validate_param_ranges(self) -> "TopologyValidator":
        """PARAM-02: Numeric parameters must be within defined min/max ranges"""
        rules = load_param_rules()
        for mod in self.config.get('modules', []):
            mod_type = mod.get('type', '')
            mod_name = mod.get('name', '')
            mod_rules = rules.get(mod_type.upper(), {})
            params = mod.get('params', {})
            for param_name, rule in mod_rules.get('rules', {}).items():
                if param_name not in params:
                    continue
                value = params[param_name]
                ptype = rule.get('type', '').upper()
                if ptype in ('INTEGER', 'INT'):
                    if 'min_value' in rule and value < rule['min_value']:
                        self.result.add_error(
                            code="PARAM-02",
                            message=f"Module '{mod_name}': {param_name}={value} is below minimum {rule['min_value']}",
                            suggestion=f"Set {param_name} to at least {rule['min_value']}"
                        )
                    if 'max_value' in rule and value > rule['max_value']:
                        self.result.add_error(
                            code="PARAM-02",
                            message=f"Module '{mod_name}': {param_name}={value} exceeds maximum {rule['max_value']}",
                            suggestion=f"Set {param_name} to at most {rule['max_value']}"
                        )
        return self

    def validate(self) -> ValidationResult:
        """Run all validation checks"""
        self.validate_connectivity()
        self.validate_reachability()
        self.validate_port_directions()
        self.validate_bundle_types()
        self.validate_required_params()
        self.validate_param_ranges()
        return self.result

    @staticmethod
    def from_json_file(path: str) -> "TopologyValidator":
        """Load config from JSON file (supports // comments)"""
        with open(path, 'r', encoding='utf-8') as f:
            content = f.read()
        lines = [l for l in content.split('\n') if not l.strip().startswith('//')]
        data = json.loads('\n'.join(lines))
        return TopologyValidator(data)