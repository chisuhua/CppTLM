#!/usr/bin/env python3
"""analyzer.py — NoC 拓扑分析器"""

import json
from typing import Dict, List, Any
from pathlib import Path


class TopologyAnalyzer:
    def __init__(self, config: Dict[str, Any]):
        self.config = config
        self.modules = config.get("modules", [])
        self.connections = config.get("connections", [])

    @classmethod
    def from_json_file(cls, path: str):
        with open(path, "r") as f:
            return cls(json.load(f))

    def count_modules(self) -> int:
        return len(self.modules)

    def count_connections(self) -> int:
        return len(self.connections)

    def count_by_type(self, type_name: str) -> int:
        return sum(1 for m in self.modules if m.get("type") == type_name)

    def get_module_types(self) -> List[str]:
        return sorted(set(m.get("type", "unknown") for m in self.modules))

    def calculate_max_degree(self) -> int:
        degree = {}
        for conn in self.connections:
            src = conn.get("src", "").split(".")[0]
            dst = conn.get("dst", "").split(".")[0]
            degree[src] = degree.get(src, 0) + 1
            degree[dst] = degree.get(dst, 0) + 1
        return max(degree.values()) if degree else 0

    def calculate_avg_latency(self) -> float:
        if not self.connections:
            return 0.0
        total = sum(conn.get("latency", 0) for conn in self.connections)
        return total / len(self.connections)

    def identify_bottlenecks(self, threshold_degree: int = 4) -> List[str]:
        degree = {}
        for conn in self.connections:
            src = conn.get("src", "").split(".")[0]
            dst = conn.get("dst", "").split(".")[0]
            degree[src] = degree.get(src, 0) + 1
            degree[dst] = degree.get(dst, 0) + 1
        return [node for node, deg in degree.items() if deg >= threshold_degree]

    def generate_report(self) -> Dict[str, Any]:
        return {
            "summary": {
                "total_modules": self.count_modules(),
                "total_connections": self.count_connections(),
                "module_types": self.get_module_types(),
                "max_degree": self.calculate_max_degree(),
                "avg_latency": self.calculate_avg_latency(),
            },
            "modules_by_type": {
                t: self.count_by_type(t) for t in self.get_module_types()
            },
            "bottlenecks": self.identify_bottlenecks(),
        }


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Analyze NoC topology")
    parser.add_argument("config", help="JSON config file")
    parser.add_argument("--output", "-o", default="-", help="Output file")
    args = parser.parse_args()

    analyzer = TopologyAnalyzer.from_json_file(args.config)
    report = analyzer.generate_report()

    output = json.dumps(report, indent=2)
    if args.output == "-":
        print(output)
    else:
        with open(args.output, "w") as f:
            f.write(output)


if __name__ == "__main__":
    main()