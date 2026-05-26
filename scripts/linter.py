#!/usr/bin/env python3
"""scripts/linter.py — Legacy linter for backward compatibility

Provides the original TopologyLinter API that was merged into TopologyValidator.
New code should use cpptlm_config.validator.TopologyValidator directly.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cpptlm_config.validator import TopologyValidator


class LintIssue:
    def __init__(self, severity, code, message, suggestion=None):
        self.severity = severity
        self.code = code
        self.message = message
        self.suggestion = suggestion


class TopologyLinter:

    def __init__(self, config: dict):
        self.config = config
        self._issues = []

    def lint(self):
        self._issues = []
        module_names = set()
        for m in self.config.get('modules', []):
            name = m.get('name', '')
            mod_type = m.get('type', '')
            if not name:
                self._issues.append(LintIssue('error', 'E001', "Module missing 'name' field", None))
            if not mod_type:
                self._issues.append(LintIssue('error', 'E002', f"Module '{name}' missing 'type' field", None))
            if name:
                module_names.add(name)

        undefined = set()
        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            if src and src not in module_names:
                undefined.add(src)
            if dst and dst not in module_names:
                undefined.add(dst)
        for mod in undefined:
            self._issues.append(LintIssue('error', 'E005', f"Undefined module '{mod}' referenced in connections", None))

        seen = set()
        for conn in self.config.get('connections', []):
            src = conn.get('src', '')
            dst = conn.get('dst', '')
            src_mod = src.split('.')[0] if '.' in src else src
            dst_mod = dst.split('.')[0] if '.' in dst else dst
            if src_mod == dst_mod:
                self._issues.append(LintIssue('warning', 'W001', f"Self-loop: {src} -> {dst}", None))
            pair = (src_mod, dst_mod)
            if pair in seen:
                self._issues.append(LintIssue('warning', 'W002', f"Duplicate connection: {src} -> {dst}", None))
            seen.add(pair)

        connected = set()
        for conn in self.config.get('connections', []):
            src = conn.get('src', '').split('.')[0]
            dst = conn.get('dst', '').split('.')[0]
            connected.add(src)
            connected.add(dst)

        for mod in self.config.get('modules', []):
            name = mod.get('name', '')
            if name and name not in connected:
                self._issues.append(LintIssue('warning', 'W003', f"Isolated module: {name}", None))

        return self._issues

    def has_errors(self) -> bool:
        return any(i.severity == 'error' for i in self._issues)

    @property
    def errors(self):
        return [i for i in self._issues if i.severity == 'error']

    @property
    def warnings(self):
        return [i for i in self._issues if i.severity == 'warning']