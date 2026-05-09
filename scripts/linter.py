#!/usr/bin/env python3
"""linter.py — NoC JSON 配置检查器"""

import json
from typing import Dict, List, Any
from dataclasses import dataclass


@dataclass
class LintError:
    severity: str
    code: str
    message: str
    module: str = ""


class TopologyLinter:
    def __init__(self, config: Dict[str, Any]):
        self.config = config
        self.modules = config.get("modules", [])
        self.connections = config.get("connections", [])
        self.errors: List[LintError] = []

        self._module_names = {m.get("name") for m in self.modules}
        self._module_types = {m.get("name"): m.get("type", "") for m in self.modules}

    def lint(self) -> List[LintError]:
        self.errors = []
        self._check_modules()
        self._check_connections()
        self._check_isolated_modules()
        return self.errors

    def _check_modules(self):
        for i, mod in enumerate(self.modules):
            if "name" not in mod:
                self.errors.append(LintError("error", "E001", f"Module at index {i} missing 'name'"))
            if "type" not in mod:
                self.errors.append(LintError("error", "E002", f"Module '{mod.get('name', i)}' missing 'type'"))

    def _check_connections(self):
        seen = set()
        for i, conn in enumerate(self.connections):
            src = conn.get("src", "")
            dst = conn.get("dst", "")
            src_mod = src.split(".")[0] if "." in src else src
            dst_mod = dst.split(".")[0] if "." in dst else dst

            if not src or not dst:
                self.errors.append(LintError("error", "E003", f"Connection {i} missing src or dst"))
                continue

            if src_mod == dst_mod:
                self.errors.append(LintError("warning", "W001", f"Self-loop: {src} -> {dst}"))

            if src_mod not in self._module_names:
                self.errors.append(LintError("error", "E004", f"Connection source not found: {src_mod}"))

            if dst_mod not in self._module_names:
                self.errors.append(LintError("error", "E005", f"Connection destination not found: {dst_mod}"))

            pair = (src_mod, dst_mod)
            if pair in seen:
                self.errors.append(LintError("warning", "W002", f"Duplicate connection: {src} -> {dst}"))
            seen.add(pair)

    def _check_isolated_modules(self):
        connected = set()
        for conn in self.connections:
            src = conn.get("src", "").split(".")[0]
            dst = conn.get("dst", "").split(".")[0]
            connected.add(src)
            connected.add(dst)

        for mod in self.modules:
            name = mod.get("name", "")
            if name and name not in connected:
                self.errors.append(LintError("warning", "W003", f"Isolated module: {name}"))

    def has_errors(self) -> bool:
        return any(e.severity == "error" for e in self.errors)

    def report(self) -> str:
        lines = []
        for e in self.errors:
            prefix = "[ERROR]" if e.severity == "error" else "[WARN]"
            mod = f" ({e.module})" if e.module else ""
            lines.append(f"{prefix} {e.code}{mod}: {e.message}")
        return "\n".join(lines) if lines else "No issues found."


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Lint NoC topology config")
    parser.add_argument("config", help="JSON config file")
    args = parser.parse_args()

    with open(args.config, "r") as f:
        config = json.load(f)

    linter = TopologyLinter(config)
    errors = linter.lint()
    print(linter.report())

    if linter.has_errors():
        exit(1)


if __name__ == "__main__":
    main()