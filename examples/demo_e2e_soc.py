#!/usr/bin/env python3
"""
E2E SoC 性能建模与仿真 — 端到端参考实现（Template）。

本脚本是 CppTLM 的端到端性能建模模板。
用户应当拷贝并修改它来创建自已的 SoC 设计，而不是直接使用此文件。

流程:
  1. 构建拓扑  — 用 ConfigBuilder API 在 Python 中定义 SoC 结构
  2. 生成配置  — 序列化为 JSON 配置文件
  3. 运行仿真  — 调用 C++ 仿真器（cpptlm_sim），流式输出统计
  4. 实时监控  — 仿真运行中实时解析统计流，输出性能摘要
  5. 深度分析  — MetricSummary + AnomalyDetector
  6. 可视化报告 — 生成 HTML 性能报告

用法:
  # 单集群 SoC (4 CPU + 4 L1 + Crossbar + 2 Memory)
  python3 examples/demo_e2e_soc.py

  # 双集群（2 × soc_cluster + crossbar + memories）
  python3 examples/demo_e2e_soc.py --dual

  # 自定义仿真参数
  python3 examples/demo_e2e_soc.py --cycles 50000 --interval 10000

  # 自定义 JSON 配置（跳过拓扑构建步骤）
  python3 examples/demo_e2e_soc.py --config my_soc.json

  # 仅生成配置并退出（不运行仿真）
  python3 examples/demo_e2e_soc.py --generate-only

输出:
  configs/<name>.json       — 生成的 SoC 拓扑配置
  output/stats_<name>.jsonl — JSON Lines 格式的流式统计
  reports/<name>.html       — 仿真性能报告
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import List, Dict, Optional

# ── 确保包路径 ──────────────────────────────────────────────────────────────
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ══════════════════════════════════════════════════════════════════════════════
# 步骤 1: 构建 SoC 拓扑（用户自定义区域）
#
# 【模板说明】
# 以下是三个参考拓扑。用户应拷贝此脚本，
# 在 build_my_topology() 中定义自己的 SoC 结构。
# ══════════════════════════════════════════════════════════════════════════════

def build_single_cluster_soc() -> str:
    """构建单集群 SoC：4 核 + 4 L1 Cache + Crossbar + 2 Memory 控制器。

    这是最典型的 SoC 拓扑模板：
      CPU cluster → L1 caches → Crossbar → Memory controllers

    用户可以修改：
      - CPU 数量、访问模式（SEQUENTIAL/RANDOM/HOTSPOT/STRIDED）
      - L1 Cache 参数
      - Crossbar 端口数量
      - Memory 延迟
    """
    from cpptlm_config.builder import ConfigBuilder
    from cpptlm_config.models import ModuleSpec, ConnectionSpec

    b = (
        ConfigBuilder("single_cluster_soc",
                      "4 CPU cores + 4 L1 caches + Crossbar + 2 Memory controllers")
        .add_module(ModuleSpec(name="cpu0", type="TrafficGenTLM", params={
            "pattern": "SEQUENTIAL", "num_requests": 10000,
            "start_addr": "0x1000", "end_addr": "0x2000",
        }))
        .add_module(ModuleSpec(name="cpu1", type="TrafficGenTLM", params={
            "pattern": "RANDOM", "num_requests": 10000,
            "start_addr": "0x2000", "end_addr": "0x4000",
        }))
        .add_module(ModuleSpec(name="cpu2", type="TrafficGenTLM", params={
            "pattern": "HOTSPOT", "num_requests": 10000,
            "start_addr": "0x1000", "end_addr": "0x4000",
            "hotspot_addrs": ["0x1000", "0x2000", "0x3000"],
            "hotspot_weights": [50, 30, 20],
        }))
        .add_module(ModuleSpec(name="cpu3", type="TrafficGenTLM", params={
            "pattern": "STRIDED", "stride": 64, "num_requests": 10000,
            "start_addr": "0x1000", "end_addr": "0x8000",
        }))
        .add_module(ModuleSpec(name="l1_0", type="CacheTLM"))
        .add_module(ModuleSpec(name="l1_1", type="CacheTLM"))
        .add_module(ModuleSpec(name="l1_2", type="CacheTLM"))
        .add_module(ModuleSpec(name="l1_3", type="CacheTLM"))
        .add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
        .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
        .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="cpu0", dst="l1_0", latency=1))
        .add_connection(ConnectionSpec(src="cpu1", dst="l1_1", latency=1))
        .add_connection(ConnectionSpec(src="cpu2", dst="l1_2", latency=1))
        .add_connection(ConnectionSpec(src="cpu3", dst="l1_3", latency=1))
        .add_connection(ConnectionSpec(src="l1_0", dst="xbar.0", latency=5))
        .add_connection(ConnectionSpec(src="l1_1", dst="xbar.1", latency=5))
        .add_connection(ConnectionSpec(src="l1_2", dst="xbar.2", latency=5))
        .add_connection(ConnectionSpec(src="l1_3", dst="xbar.3", latency=5))
        .add_connection(ConnectionSpec(src="xbar.0", dst="mem0", latency=20))
        .add_connection(ConnectionSpec(src="xbar.1", dst="mem1", latency=20))
    )
    return _save_config(b.build(), "single_cluster_soc.json")


def build_dual_cluster_soc() -> str:
    """构建双集群 SoC：2 × 2 CPU 集群 + Crossbar + 2 Memory 控制器。

    通过 set_extends() 复用单集群配置，减少重复定义。
    """
    from cpptlm_config.builder import ConfigBuilder
    from cpptlm_config.models import ModuleSpec, ConnectionSpec

    b = (
        ConfigBuilder("dual_cluster_soc",
                      "Dual-cluster: 2×2 CPUs + Crossbar + 2 Memories")
        .set_extends("configs/single_cluster_soc.json")
        .add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
        .add_module(ModuleSpec(name="mem0", type="MemoryTLM"))
        .add_module(ModuleSpec(name="mem1", type="MemoryTLM"))
        .add_connection(ConnectionSpec(src="l1_0", dst="xbar.0", latency=5))
        .add_connection(ConnectionSpec(src="l1_1", dst="xbar.1", latency=5))
        .add_connection(ConnectionSpec(src="l1_2", dst="xbar.2", latency=5))
        .add_connection(ConnectionSpec(src="l1_3", dst="xbar.3", latency=5))
        .add_connection(ConnectionSpec(src="xbar.0", dst="mem0", latency=20))
        .add_connection(ConnectionSpec(src="xbar.1", dst="mem1", latency=20))
    )
    return _save_config(b.build(), "dual_cluster_soc.json")


# ══════════════════════════════════════════════════════════════════════════════
# 用户自定义拓扑 — 修改此函数来创建自己的 SoC 设计
# ══════════════════════════════════════════════════════════════════════════════

def build_my_topology() -> str:
    """【模板】用户自定义 SoC 拓扑。

    拷贝此脚本后，在此函数中定义自己的 SoC 结构：

        # 1. 创建 builder
        builder = ConfigBuilder("my_soc", "My custom SoC")

        # 2. 添加模块
        builder.add_module(ModuleSpec(name="cpu0", type="TrafficGenTLM", params={...}))
        builder.add_module(ModuleSpec(name="l1", type="CacheTLM"))
        builder.add_module(ModuleSpec(name="xbar", type="CrossbarTLM"))
        ...

        # 3. 定义连接
        builder.add_connection(ConnectionSpec(src="cpu0", dst="l1", latency=1))
        ...

    """
    return build_single_cluster_soc()


# ── 辅助：保存配置 ──────────────────────────────────────────────────────────

def _save_config(schema, filename: str) -> str:
    """序列化 ConfigSchema 为 JSON 文件并返回路径。"""
    config_dir = Path("configs")
    config_dir.mkdir(parents=True, exist_ok=True)
    path = str(config_dir / filename)
    schema.save(path)
    print(f"  [Config]  {path}")
    return path


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 2: 运行 C++ 仿真
# ══════════════════════════════════════════════════════════════════════════════

def find_binary() -> Optional[str]:
    """查找 cpptlm_sim 可执行文件。"""
    candidates = [
        "./build/bin/cpptlm_sim",
        "build/bin/cpptlm_sim",
        "../build/bin/cpptlm_sim",
    ]
    for p in candidates:
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return os.path.abspath(p)
    return None


def run_simulation(config_path: str, cycles: int = 10000,
                   interval: int = 5000, binary_path: Optional[str] = None) \
        -> Optional[str]:
    """运行 C++ 仿真，输出 JSONL 流式统计文件。

    返回 JSONL 文件路径，失败返回 None。
    """
    binary = binary_path or find_binary()
    if not binary:
        print("\n  [ERROR] cpptlm_sim binary not found.")
        print("  Build it with:")
        print("    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release")
        print("    cmake --build build -j$(nproc)")
        return None

    # 创建输出目录
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)

    config_name = Path(config_path).stem
    stats_path = str(output_dir / f"stats_{config_name}.jsonl")

    # 构造命令行
    cmd = [
        binary, config_path,
        "--stream-stats",
        "--stream-interval", str(interval),
        "--stream-path", stats_path,
        "--cycles", str(cycles),
    ]

    print(f"\n  [Simulation] {' '.join(cmd)}")
    print(f"  [Simulation] Cycles={cycles}, Interval={interval}")
    print(f"  [Simulation] Output: {stats_path}")

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=300,
        )
    except FileNotFoundError:
        print(f"  [ERROR] Binary not found: {binary}")
        return None
    except subprocess.TimeoutExpired:
        print(f"  [ERROR] Simulation timed out after 300s")
        return None

    if result.returncode != 0:
        print(f"  [ERROR] Simulation failed (code={result.returncode})")
        if result.stderr:
            for line in result.stderr.strip().split("\n"):
                print(f"    stderr: {line}")
        return None

    # 输出最后几行 stdout（仿真器有进度信息）
    for line in result.stdout.strip().split("\n"):
        print(f"    {line}")

    # 验证统计文件
    if not os.path.isfile(stats_path) or os.path.getsize(stats_path) == 0:
        print(f"  [ERROR] No stats output generated")
        return None

    print(f"  [Stats]   {stats_path} ({os.path.getsize(stats_path)} bytes)")
    return stats_path


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 3: 实时监控（仿真运行中实时解析统计流）
# ══════════════════════════════════════════════════════════════════════════════

def tail_stats(stats_path: str, poll_interval: float = 0.5) -> List[Dict]:
    """实时监控 JSONL 统计文件，返回所有已解析的记录。

    在仿真运行时的另一个线程中调用此函数。
    每次调用会增量读取新行并返回累积记录。
    """
    records = []
    seen_lines = set()

    def monitor():
        nonlocal records
        while not os.path.isfile(stats_path):
            time.sleep(0.1)

        with open(stats_path, "r") as f:
            while True:
                line = f.readline()
                if line and line.strip():
                    # 去重（文件可能被追加时读到部分行）
                    line_key = line.strip()
                    if line_key not in seen_lines:
                        seen_lines.add(line_key)
                        try:
                            record = json.loads(line.strip())
                            records.append(record)
                            _print_live_stats(record)
                        except json.JSONDecodeError:
                            pass  # 行未写完，跳过
                else:
                    # 检查仿真进程是否还在运行
                    time.sleep(poll_interval)

    t = threading.Thread(target=monitor, daemon=True)
    t.start()
    return records


def _print_live_stats(record: Dict):
    """打印单条统计记录的实时摘要。"""
    group = record.get("group", "?")
    cycle = record.get("simulation_cycle", 0)
    data = record.get("data", {})

    # 提取关键指标
    latency = data.get("latency", {})
    if isinstance(latency, dict):
        lat_str = f"avg={latency.get('avg', '?'):>8.2f}" if "avg" in latency else ""
    else:
        lat_str = f"val={latency:>8.2f}" if isinstance(latency, (int, float)) else ""

    hits = data.get("hits", data.get("flits_received", ""))
    misses = data.get("misses", data.get("flits_sent", ""))
    reqs = data.get("requests", data.get("flit_requests", ""))

    parts = [f"  [Live]  cycle={cycle:>6}  {group:<20}"]
    if lat_str:
        parts.append(lat_str)
    if isinstance(reqs, (int, float)):
        parts.append(f"req={reqs:>6}")
    if isinstance(hits, (int, float)):
        parts.append(f"hits={hits:>6}")
    if isinstance(misses, (int, float)):
        parts.append(f"miss={misses:>6}")

    print("  ".join(parts))


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 4: 深度分析
# ══════════════════════════════════════════════════════════════════════════════

def analyze(stats_path: str):
    """解析仿真结果，运行 MetricSummary 和 AnomalyDetector。"""
    from cpptlm.simulation.result import Result
    from cpptlm.analysis import MetricSummary, AnomalyDetector
    from cpptlm.analysis.adapters import adapt_result

    result = Result.from_jsonl(stats_path)
    adapted = adapt_result(result)

    metrics = MetricSummary(adapted)
    detector = AnomalyDetector(adapted)

    # ── 延迟统计 ──
    print("\n  ── Latency by Group ──")
    print(f"  {'Group':<25} {'Mean':>8} {'P95':>8} {'P99':>8} {'Count':>6}")
    print(f"  {'─'*25} {'─'*8} {'─'*8} {'─'*8} {'─'*6}")
    for group in adapted.groups():
        stats = metrics.latency_statistics(group=group)
        print(f"  {group:<25} {stats['mean']:>8.2f} {stats['p95']:>8.2f} "
              f"{stats['p99']:>8.2f} {stats['count']:>6}")

    # ── 吞吐量统计 ──
    print("\n  ── Throughput ──")
    for group in adapted.groups():
        thr = metrics.throughput_statistics(group=group)
        print(f"  {group:<25} {thr['requests_per_sec']:>10.1f} req/s "
              f"(total={thr['total_requests']})")

    # ── 命中率 ──
    print("\n  ── Cache Hit Rates ──")
    for group in adapted.groups():
        hr = metrics.hit_rate_statistics(group=group)
        if hr["total_requests"] > 0:
            print(f"  {group:<25} {hr['hit_rate']:>7.1%} "
                  f"(hits={hr['total_hits']}, misses={hr['total_misses']})")

    # ── 瓶颈检测 ──
    bottlenecks = detector.identify_bottlenecks(threshold_percentile=80.0)
    if bottlenecks:
        print("\n  ── Bottlenecks ──")
        for b in bottlenecks:
            print(f"  {b['group']:<25} latency={b['mean_latency']:>8.2f}  "
                  f"severity={b['severity']:<6}  (threshold={b['threshold']:.1f})")

    # ── 异常检测 ──
    print("\n  ── Anomaly Detection ──")
    for group in adapted.groups():
        outliers = detector.detect_outliers_zscore(group, threshold=2.5)
        if outliers:
            print(f"  {group:<25} {len(outliers)} outliers detected "
                  f"(z-score > 2.5)")
        else:
            print(f"  {group:<25} no anomalies")

    return adapted


# ══════════════════════════════════════════════════════════════════════════════
# 步骤 5: 生成报告
# ══════════════════════════════════════════════════════════════════════════════

def generate_report(stats_path: str,
                    topology_image: Optional[str] = None) -> str:
    """生成 HTML 性能报告（含拓扑图）。"""
    reports_dir = Path("reports")
    reports_dir.mkdir(parents=True, exist_ok=True)

    config_name = Path(stats_path).stem.replace("stats_", "")
    output_path = str(reports_dir / f"{config_name}.html")

    from cpptlm.visualization.report import ReportGenerator
    report_path = ReportGenerator(
        stats_path, topology_image=topology_image,
    ).generate(output_path=output_path)

    return report_path


# ══════════════════════════════════════════════════════════════════════════════
# 总入口: 端到端流水线
# ══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="CppTLM E2E SoC 性能建模与仿真 Demo",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python3 examples/demo_e2e_soc.py                      # 单集群 SoC
  python3 examples/demo_e2e_soc.py --dual               # 双集群 SoC
  python3 examples/demo_e2e_soc.py --cycles 50000       # 更多周期
  python3 examples/demo_e2e_soc.py --config my.json     # 自定义配置
  python3 examples/demo_e2e_soc.py --generate-only      # 仅生成配置
  python3 examples/demo_e2e_soc.py --dashboard          # 实时 Web Dashboard

将此脚本作为模板，修改 build_my_topology() 来创建自己的 SoC 设计。
        """,
    )
    parser.add_argument("--dual", action="store_true",
                        help="使用双集群 SoC 拓扑")
    parser.add_argument("--config", type=str, default=None,
                        help="直接使用现有 JSON 配置（跳过拓扑构建）")
    parser.add_argument("--cycles", type=int, default=10000,
                        help="仿真周期数 (default: 10000)")
    parser.add_argument("--interval", type=int, default=5000,
                        help="统计报告间隔 (default: 5000)")
    parser.add_argument("--generate-only", action="store_true",
                        help="仅生成 JSON 配置，不运行仿真")
    parser.add_argument("--binary", type=str, default=None,
                        help="cpptlm_sim 二进制路径 (默认自动查找)")
    parser.add_argument("--dashboard", action="store_true",
                        help="启动实时 Web Dashboard（http://localhost:<port>）")
    parser.add_argument("--dashboard-port", type=int, default=8080,
                        help="Dashboard 端口 (default: 8080)")

    args = parser.parse_args()

    print("=" * 60)
    print("  CppTLM — SoC Performance Modeling Pipeline")
    print("=" * 60)

    # ── Step 1: Build or use existing config ──
    print("\n  [Step 1/5] Building SoC topology...")

    if args.config:
        config_path = args.config
        if not os.path.isfile(config_path):
            print(f"  [ERROR] Config not found: {config_path}")
            sys.exit(1)
        print(f"  [Config]  {config_path} (user-provided)")
    else:
        if args.dual:
            config_path = build_dual_cluster_soc()
        else:
            config_path = build_single_cluster_soc()

    # ── Step 1.5: Visualize topology ──
    from cpptlm.visualization.topology import visualize_topology_from_config
    dot_path, img_path = visualize_topology_from_config(config_path)
    if img_path:
        print(f"  [Topology] {img_path}")
        print(f"  [Topology] DOT:  {dot_path}")
    elif dot_path:
        print(f"  [Topology] DOT:  {dot_path} (install graphviz for PNG)")

    if args.generate_only:
        print("\n  [Done] Configuration generated. Run simulation with:")
        print(f"    python3 examples/demo_e2e_soc.py --config {config_path}")
        sys.exit(0)

    # ── Step 2: Run simulation (with optional dashboard) ──
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    config_name = Path(config_path).stem
    stats_path = str(output_dir / f"stats_{config_name}.jsonl")

    if args.dashboard:
        # Dashboard 模式：后台运行仿真 + 前台实时 Web
        binary = find_binary() if not args.binary else args.binary
        if not binary:
            print("\n  [ERROR] cpptlm_sim binary not found. Build first.")
            sys.exit(1)

        cmd = [
            binary, config_path,
            "--stream-stats",
            "--stream-interval", str(args.interval),
            "--stream-path", stats_path,
            "--cycles", str(args.cycles),
        ]

        print(f"\n  [Step 2/5] Starting simulation in background...")
        print(f"  [Simulation] {' '.join(cmd)}")
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, bufsize=1,
        )

        # 启动 Dashboard
        from cpptlm.visualization.dashboard_server import DashboardServer
        server = DashboardServer(stats_path, port=args.dashboard_port)
        server.start()

        print(f"\n  [Step 3/5] Dashboard: {server.url}")
        print(f"  Press Ctrl+C to stop simulation and view final analysis\n")

        # 后台线程读取仿真日志
        def _print_log():
            if proc.stdout:
                for line in proc.stdout:
                    print(f"    {line}", end="")
        t = threading.Thread(target=_print_log, daemon=True)
        t.start()

        try:
            # 主线程等待仿真完成
            proc.wait()
        except KeyboardInterrupt:
            print("\n  [Dashboard] Stopping simulation...")
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

        server.stop()
        print(f"\n  [Stats]   {stats_path}")

        if proc.returncode != 0 and proc.returncode != -15:
            print(f"  [WARNING] Simulation returned code {proc.returncode}")

    else:
        # 普通模式：同步运行仿真
        print("\n  [Step 2/5] Running C++ simulation...")
        stats_path = run_simulation(
            config_path, cycles=args.cycles,
            interval=args.interval, binary_path=args.binary,
        )
        if stats_path is None:
            print("\n  [ABORTED] Simulation failed. See errors above.")
            sys.exit(1)

    # ── Step 4: Deep analysis ──
    print("\n  [Step 4/5] Analyzing simulation results...")
    analyze(stats_path)

    # ── Step 5: Generate report ──
    print("\n  [Step 5/5] Generating performance report...")
    report_path = generate_report(stats_path, topology_image=img_path)
    report_abspath = os.path.abspath(report_path)
    print(f"  [Report]  {report_abspath}")

    # ── Summary ──
    print("\n" + "=" * 60)
    print("  Pipeline Summary")
    print("=" * 60)
    print(f"  Config:   {os.path.abspath(config_path)}")
    print(f"  Stats:    {os.path.abspath(stats_path)}")
    print(f"  Report:   {report_abspath}")
    print()
    print("  Next steps:")
    print("    - Open the report in a browser")
    print(f"      file://{report_abspath}")
    print("    - Modify build_my_topology() for your own SoC")
    print("    - Tweak latency/bandwidth params to explore design space")
    if args.dashboard:
        print("    - Re-run with --dashboard for live visualization")
    print("=" * 60)


if __name__ == "__main__":
    main()
