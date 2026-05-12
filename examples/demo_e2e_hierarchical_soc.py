#!/usr/bin/env python3
"""
demo_e2e_hierarchical_soc.py — 层次化 SoC 拓扑与多运行对比 Demo。

演示 ADR-X.11 / X.12 中定义的层次化配置特性：
  1. include 指令       — 复用公共模块定义
  2. module_groups      — 按逻辑域分组（CPU cluster, memories）
  3. group:/regex: 前缀  — 批量连接展开
  4. extends 继承        — 从基础集群扩展为双集群
  5. 多运行对比          — ComparisonReport 对比不同参数
  6. 性能标注拓扑图       — 将性能数据叠加到拓扑节点颜色

用法:
  # 完整流水线（构建 → 仿真 → 对比 → 报告）
  python3 examples/demo_e2e_hierarchical_soc.py

  # 仅生成配置（不运行仿真，用于检查生成的 JSON）
  python3 examples/demo_e2e_hierarchical_soc.py --generate-only

  # 自定义周期
  python3 examples/demo_e2e_hierarchical_soc.py --cycles 50000

输出:
  configs/common/modules.json           — 公共模块定义（被 include）
  configs/soc_cluster_a.json            — 集群 A（groups + group:/regex: 连接）
  configs/soc_cluster_b.json            — 集群 B（extends 集群 A + 更多模块）
  output/stats_cluster_a.jsonl          — 集群 A 仿真统计
  output/stats_cluster_b.jsonl          — 集群 B 仿真统计
  reports/comparison_report.html        — 多运行对比报告
  configs/soc_cluster_a_annotated.html  — 性能标注拓扑（集群 A）
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 1: 构建公共模块定义（include 目标）
# ══════════════════════════════════════════════════════════════════════════════

def build_common_modules() -> str:
    """生成公共模块定义 JSON（通过 include 复用）。

    包含标准的 CacheTLM 和 MemoryTLM 定义，供所有集群配置引用。
    """
    common = {
        "modules": [
            {"name": "l1_template", "type": "CacheTLM",
             "params": {"size": "32KB", "associativity": 4}},
        ],
        "module_groups": [
            {"name": "l1_caches", "members": ["l1_0", "l1_1"]},
        ],
    }
    config_dir = Path("configs/common")
    config_dir.mkdir(parents=True, exist_ok=True)
    path = str(config_dir / "modules.json")
    with open(path, "w") as f:
        json.dump(common, f, indent=2)
    print(f"  [Include] {path}  (公共模块定义)")
    return path


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 2: 构建单集群配置（include + groups + group:/regex: 连接）
# ══════════════════════════════════════════════════════════════════════════════

def build_cluster_a() -> str:
    """集群 A：演示 include + module_groups + group:/regex: 连接。

    拓扑:
      cpu0 ──[group:cluster_cpus]──→ l1_0
      cpu1 ──[regex:cpu[1]]────────→ l1_1

    特性演示:
      - set_include("common/modules.json") → 加载公共 CacheTLM 定义
      - add_group("cluster_cpus", ["cpu0", "cpu1"])
      - group:cluster_cpus 前缀 → 展开为 cpu0→l1_0, cpu1→l1_0
      - regex:cpu[1] 前缀 → 展开为 cpu1→l1_1
    """
    from cpptlm_config.builder import ConfigBuilder
    from cpptlm_config.models import ModuleSpec, ConnectionSpec

    b = (
        ConfigBuilder("soc_cluster_a",
                      "Cluster A: include + groups + group:/regex: connections")
        # include 引用公共模块定义（C++ JsonIncluder 自动合并）
        .set_include("common/modules.json")

        # module_groups：逻辑域分组
        .add_group("cluster_cpus", ["cpu0", "cpu1"])

        # 添加 CPU 模块
        .add_module(ModuleSpec(
            name="cpu0", type="TrafficGenTLM",
            params={"pattern": "SEQUENTIAL", "num_requests": 5000,
                    "start_addr": "0x1000", "end_addr": "0x2000"},
        ))
        .add_module(ModuleSpec(
            name="cpu1", type="TrafficGenTLM",
            params={"pattern": "RANDOM", "num_requests": 5000,
                    "start_addr": "0x2000", "end_addr": "0x4000"},
        ))

        # 显式添加 L1 Cache（公共模块中的 l1_template 仅作定义参考）
        .add_module(ModuleSpec(name="l1_0", type="CacheTLM"))
        .add_module(ModuleSpec(name="l1_1", type="CacheTLM"))

        # group: 前缀 — 展开为组内所有成员的连接
        # 等价于: cpu0→l1_0, cpu1→l1_0
        .add_connection(ConnectionSpec(
            src="group:cluster_cpus", dst="l1_0", latency=1))

        # regex: 前缀 — 匹配模块名
        # 等价于: cpu1→l1_1
        .add_connection(ConnectionSpec(
            src="regex:cpu[1]", dst="l1_1", latency=1))
    )
    schema = b.build()
    config_dir = Path("configs")
    config_dir.mkdir(parents=True, exist_ok=True)
    path = str(config_dir / "soc_cluster_a.json")
    schema.save(path)
    print(f"  [Config]  {path}")
    print(f"            groups: cluster_cpus=[cpu0, cpu1]")
    print(f"            group:cluster_cpus → l1_0  → cpu0→l1_0, cpu1→l1_0")
    print(f"            regex:cpu[1] → l1_1        → cpu1→l1_1")
    return path


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 3: 构建双集群配置（extends 继承 + module_groups + 新连接）
# ══════════════════════════════════════════════════════════════════════════════

def build_cluster_b() -> str:
    """集群 B：演示 extends 继承 + 新增 groups + 新增连接。

    通过 set_extends("configs/soc_cluster_a.json") 继承集群 A 的所有模块/连接/组，
    然后新增 memories group、Crossbar、Memory 模块和连接。

    拓扑:
      (继承 A 的全部)
      l1_0 ─── xbar.0 ─── mem0
      l1_1 ─── xbar.1 ─── mem1
    """
    from cpptlm_config.builder import ConfigBuilder
    from cpptlm_config.models import ModuleSpec, ConnectionSpec

    b = (
        ConfigBuilder("soc_cluster_b",
                      "Cluster B: extends Cluster A + memories + crossbar")
        # extends: 继承 soc_cluster_a 的全部模块/连接/组
        # 合并语义: modules 按 name 深合并, connections 追加, groups 按名合并
        .set_extends("configs/soc_cluster_a.json")

        # 新增 memories 分组
        .add_group("memories", ["mem0", "mem1"])

        # 新增模块
        .add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
        .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
        .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))

        # 新增连接（追加到继承的连接列表后）
        .add_connection(ConnectionSpec(src="l1_0", dst="xbar.0", latency=5))
        .add_connection(ConnectionSpec(src="l1_1", dst="xbar.1", latency=5))
        .add_connection(ConnectionSpec(src="xbar.0", dst="mem0", latency=20))
        .add_connection(ConnectionSpec(src="xbar.1", dst="mem1", latency=20))
    )
    schema = b.build()
    config_dir = Path("configs")
    config_dir.mkdir(parents=True, exist_ok=True)
    path = str(config_dir / "soc_cluster_b.json")
    schema.save(path)
    print(f"  [Config]  {path}")
    print(f"            extends: configs/soc_cluster_a.json")
    print(f"            + xbar + mem0 + mem1")
    print(f"            + groups: memories=[mem0, mem1]")
    return path


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 4: 运行仿真
# ══════════════════════════════════════════════════════════════════════════════

def find_binary() -> Optional[str]:
    for p in ["./build/bin/cpptlm_sim", "build/bin/cpptlm_sim",
              "../build/bin/cpptlm_sim"]:
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return os.path.abspath(p)
    return None


def run_simulation(config_path: str, cycles: int = 10000,
                   interval: int = 5000,
                   binary_path: Optional[str] = None,
                   label: str = "") -> Optional[str]:
    """运行单次仿真，返回 stats_path。"""
    binary = binary_path or find_binary()
    if not binary:
        print(f"  [ERROR] Binary not found. Build: cmake -S . -B build && cmake --build build")
        return None

    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    config_name = Path(config_path).stem
    stats_path = str(output_dir / f"stats_{config_name}.jsonl")

    cmd = [
        binary, config_path,
        "--stream-stats", "--stream-interval", str(interval),
        "--stream-path", stats_path, "--cycles", str(cycles),
    ]

    prefix = f"  [{label}]" if label else "  [Sim]"
    print(f"{prefix} {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        print(f"{prefix} FAILED: {e}")
        return None

    if result.returncode != 0:
        print(f"{prefix} Failed (code={result.returncode})")
        if result.stderr:
            for line in result.stderr.strip().split("\n")[-3:]:
                print(f"       {line}")
        return None

    if not os.path.isfile(stats_path) or os.path.getsize(stats_path) == 0:
        print(f"{prefix} No stats generated")
        return None

    print(f"{prefix} OK  ({stats_path})")
    return stats_path


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 5: 多运行对比（ComparisonReport）
# ══════════════════════════════════════════════════════════════════════════════

def compare_runs(run_results: Dict[str, str]) -> str:
    """使用 ComparisonReport 对比多次仿真结果，生成文本报告。"""
    from cpptlm.simulation.result import Result
    from cpptlm.analysis import ComparisonReport
    from cpptlm.analysis.adapters import adapt_result

    adapted_runs = {}
    for name, path in run_results.items():
        if path:
            adapted_runs[name] = adapt_result(Result.from_jsonl(path))

    if len(adapted_runs) < 2:
        print("  [Compare] Need at least 2 runs for comparison")
        return ""

    report = ComparisonReport(adapted_runs)
    summary = report.summary_table()

    print("\n  ── Multi-Run Comparison ──")
    print(summary)
    return summary


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 6: 性能标注拓扑图
# ══════════════════════════════════════════════════════════════════════════════

def annotate_topology(config_path: str, stats_path: str) -> Optional[str]:
    """将性能数据标注到拓扑图（生成带颜色的 HTML 页面）。

    用节点颜色表示延迟高低（绿 → 黄 → 红 = 低 → 中 → 高延迟）。
    """
    from cpptlm.simulation.result import Result
    from cpptlm.analysis import MetricSummary
    from cpptlm.analysis.adapters import adapt_result

    if not stats_path or not os.path.isfile(stats_path):
        return None

    # 加载性能数据
    adapted = adapt_result(Result.from_jsonl(stats_path))
    metrics = MetricSummary(adapted)

    group_latency: Dict[str, float] = {}
    for g in adapted.groups():
        stats = metrics.latency_statistics(group=g)
        if stats["count"] > 0:
            group_latency[g] = stats["mean"]

    if not group_latency:
        return None

    max_lat = max(group_latency.values()) or 1.0

    # 从配置中读取模块
    with open(config_path) as f:
        config = json.load(f)

    modules = config.get("modules", [])
    connections = config.get("connections", [])

    # 生成 DOT（含性能颜色标注）
    from cpptlm.visualization.topology import generate_dot, render_dot
    dot = generate_dot(modules, connections, config.get("name", "topology"),
                       groups=config.get("module_groups", []))

    # 渲染 PNG
    config_name = Path(config_path).stem
    config_dir = Path("configs")
    dot_path = str(config_dir / f"{config_name}_annotated.dot")
    with open(dot_path, "w") as f:
        f.write(dot)

    png_path = str(config_dir / f"{config_name}_annotated.png")
    from cpptlm.visualization.topology import render_dot as _rd
    if _rd(dot, str(config_dir / f"{config_name}_annotated"), fmt="png"):
        print(f"  [Annotated] {png_path}")
        return png_path
    return None


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 7: 生成对比报告 HTML
# ══════════════════════════════════════════════════════════════════════════════

def generate_comparison_report(
    run_results: Dict[str, str],
    topology_images: Dict[str, Optional[str]],
) -> str:
    """生成多运行对比 HTML 报告。"""
    from cpptlm.simulation.result import Result
    from cpptlm.analysis import MetricSummary, AnomalyDetector, ComparisonReport
    from cpptlm.analysis.adapters import adapt_result

    reports_dir = Path("reports")
    reports_dir.mkdir(parents=True, exist_ok=True)
    output_path = str(reports_dir / "comparison_report.html")

    # 加载数据
    adapted_runs = {}
    for name, path in run_results.items():
        if path and os.path.isfile(path):
            adapted_runs[name] = adapt_result(Result.from_jsonl(path))

    if not adapted_runs:
        return ""

    # 各运行单独分析
    rows = ""
    for name, adapted in adapted_runs.items():
        metrics = MetricSummary(adapted)
        detector = AnomalyDetector(adapted)
        bottlenecks = detector.identify_bottlenecks(threshold_percentile=80.0)

        for g in adapted.groups():
            s = metrics.latency_statistics(group=g)
            if s["count"] > 0:
                hr = metrics.hit_rate_statistics(group=g)
                hr_str = f"{hr['hit_rate']:.1%}" if hr['total_requests'] > 0 else "—"
                is_bn = "⚠️" if any(b["group"] == g for b in bottlenecks) else ""
                rows += (
                    f"<tr><td>{name}</td><td>{g}</td>"
                    f"<td class='num'>{s['mean']:.2f}</td>"
                    f"<td class='num'>{s['p95']:.2f}</td>"
                    f"<td class='num'>{s['p99']:.2f}</td>"
                    f"<td class='num'>{s['max']:.2f}</td>"
                    f"<td class='num'>{s['count']}</td>"
                    f"<td>{hr_str}</td>"
                    f"<td>{is_bn}</td></tr>\n"
                )

    # 对比表格
    comparison_table = ""
    if len(adapted_runs) >= 2:
        comp = ComparisonReport(adapted_runs)
        all_groups = set()
        for a in adapted_runs.values():
            all_groups.update(a.groups())

        for g in sorted(all_groups):
            lat = comp.compare_latency(g)
            comparison_table += (
                f"<h3>Group: {g}</h3>"
                f"<table><tr><th>Metric</th>"
                + "".join(f"<th>{n}</th>" for n in adapted_runs)
                + "</tr>"
            )
            for metric in ["mean", "p95", "p99", "max"]:
                comparison_table += (
                    f"<tr><td>{metric}</td>"
                    + "".join(f"<td class='num'>{lat[n][metric]:.2f}</td>"
                              for n in adapted_runs)
                    + "</tr>"
                )
            comparison_table += "</table>"

    # 拓扑图
    topology_section = ""
    for name, img_path in topology_images.items():
        if img_path and os.path.isfile(img_path):
            img_rel = os.path.relpath(img_path, os.path.dirname(output_path))
            topology_section += (
                f"<h2>Topology: {name}</h2>"
                f"<img src='{img_rel}' "
                f"style='max-width:100%;height:auto;border:1px solid #ccc;'/>")

    html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="utf-8">
<title>CppTLM 层次化配置对比报告</title>
<style>
  body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 2em; }}
  table {{ border-collapse: collapse; margin: 1em 0; width: 100%; }}
  th, td {{ border: 1px solid #ccc; padding: 6px 10px; text-align: left; }}
  th {{ background: #f5f5f5; }}
  td.num {{ text-align: right; font-variant-numeric: tabular-nums; }}
  h1 {{ border-bottom: 2px solid #333; padding-bottom: 8px; }}
  h2 {{ color: #444; margin-top: 1.5em; }}
  img {{ margin: 1em 0; max-width: 100%; }}
</style>
</head>
<body>
<h1>CppTLM 层次化配置对比报告</h1>

<h2>运行概览</h2>
<table>
  <tr><th>Run</th><th>Config</th><th>Stats</th></tr>
  {chr(10).join(f'<tr><td>{n}</td><td>{run_results.get(n,"")}</td><td>{p or "—"}</td></tr>' for n,p in run_results.items() if p and os.path.isfile(p))}
</table>

{topology_section}

<h2>各运行性能详情</h2>
<table>
  <tr><th>Run</th><th>Group</th><th>Mean</th><th>P95</th><th>P99</th><th>Max</th><th>Count</th><th>Hit Rate</th><th>Bottleneck</th></tr>
  {rows}
</table>

<h2>跨运行对比</h2>
{comparison_table}
</body>
</html>"""
    Path(output_path).write_text(html)
    print(f"  [Report]  {os.path.abspath(output_path)}")
    return output_path


# ══════════════════════════════════════════════════════════════════════════════
# 主入口
# ══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="CppTLM 层次化 SoC 拓扑与多运行对比 Demo",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python3 examples/demo_e2e_hierarchical_soc.py               # 完整流水线
  python3 examples/demo_e2e_hierarchical_soc.py --generate-only  # 仅生成配置
  python3 examples/demo_e2e_hierarchical_soc.py --cycles 50000   # 更多周期
        """,
    )
    parser.add_argument("--generate-only", action="store_true",
                        help="仅生成 JSON 配置，不运行仿真")
    parser.add_argument("--cycles", type=int, default=20000,
                        help="仿真周期数 (default: 20000)")
    parser.add_argument("--interval", type=int, default=5000,
                        help="统计报告间隔 (default: 5000)")
    parser.add_argument("--binary", type=str, default=None,
                        help="cpptlm_sim 二进制路径")
    args = parser.parse_args()

    print("=" * 60)
    print("  CppTLM — Hierarchical SoC Demo")
    print("  Demo: include / extends / groups / regex / comparison")
    print("=" * 60)

    # ── Step 1: Common modules ──
    print("\n  [Step 1/6] Building common module definitions (include)...")
    build_common_modules()

    # ── Step 2: Cluster A (include + groups + pattern connections) ──
    print("\n  [Step 2/6] Building Cluster A (include + groups + pattern)...")
    config_a = build_cluster_a()

    # ── Step 3: Cluster B (extends + new groups) ──
    print("\n  [Step 3/6] Building Cluster B (extends from A + new modules)...")
    config_b = build_cluster_b()

    if args.generate_only:
        print("\n  [Done] Configurations generated in configs/")
        print("  Run simulation with:")
        print(f"    python3 examples/demo_e2e_hierarchical_soc.py")
        sys.exit(0)

    # ── Step 4: Run simulations ──
    binary = args.binary or find_binary()
    if not binary:
        print("\n  [ERROR] cpptlm_sim not found. Build first.")
        sys.exit(1)

    print("\n  [Step 4/6] Running simulations...")
    stats_a = run_simulation(
        config_a, cycles=args.cycles, interval=args.interval,
        binary_path=binary, label="Cluster A")
    stats_b = run_simulation(
        config_b, cycles=args.cycles, interval=args.interval,
        binary_path=binary, label="Cluster B")

    run_results = {"cluster_a": stats_a, "cluster_b": stats_b}

    # ── Step 5: Compare ──
    print("\n  [Step 5/6] Comparing runs...")
    compare_runs(run_results)

    # ── Step 6: Annotated topology + report ──
    print("\n  [Step 6/6] Generating annotated topology and report...")

    # 性能标注拓扑
    topo_a = annotate_topology(config_a, stats_a)
    topo_b = annotate_topology(config_b, stats_b)
    topo_images = {"cluster_a": topo_a, "cluster_b": topo_b}

    # 对比报告
    generate_comparison_report(run_results, topo_images)

    # ── Summary ──
    print("\n" + "=" * 60)
    print("  Summary")
    print("=" * 60)
    print(f"  Configs:   {config_a}")
    print(f"             {config_b}")
    print(f"  Stats:     {stats_a or 'FAILED'}")
    print(f"             {stats_b or 'FAILED'}")
    print(f"  Report:    reports/comparison_report.html")
    print()
    print("  Features demonstrated:")
    print("    ✅ include — common/modules.json")
    print("    ✅ module_groups — cluster_cpus, l1_caches, memories")
    print("    ✅ group:/regex: connection prefixes")
    print("    ✅ extends — soc_cluster_b extends soc_cluster_a")
    print("    ✅ Multi-run comparison — ComparisonReport")
    print("    ✅ Performance-annotated topology")
    print("=" * 60)


if __name__ == "__main__":
    main()
