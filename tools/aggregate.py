#!/usr/bin/env python3
"""tools/aggregate.py — CEP sweep 聚合

per ADR-CEP-06 §4.1:把一个 sweep 目录下**多个 run** 的 metrics.json 聚合成
`sweep_summary.csv`(每行 = 1 个 run,每列 = 13 必纳 metric)。

纯标准库(零项目内依赖 / 零第三方依赖)。

⚠️ 本工具**不复制** `verification_mode` / `verification_scope`:
   它们是 run 级元数据(per ADR-CEP-06 §3),不在 13 必纳 metric 列中;
   复制进 CSV 会让「同一 run 的验证深度」出现第二个真源 ⇒ 只由原 metrics.json 承载。

用法:
    python3 tools/aggregate.py <sweep_dir> [-o <csv_path>]
    python3 tools/aggregate.py <sweep_dir> --check     # 只校验,不写
"""
import argparse
import csv
import json
import sys
from pathlib import Path

REQUIRED_METRICS = [
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
]


def discover_runs(sweep_dir: Path):
    """返回 [(run_id, metrics_path)];只认直接子目录里的 metrics.json。"""
    runs = []
    for child in sorted(sweep_dir.iterdir()):
        if not child.is_dir():
            continue
        m = child / "metrics.json"
        if m.is_file():
            runs.append((child.name, m))
    return runs


def load_metrics(path: Path):
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser(description="CEP sweep 聚合(13 必纳 metric → CSV)")
    ap.add_argument("sweep_dir", help="sweep 目录(其子目录各含 metrics.json)")
    ap.add_argument("-o", "--out", help="输出 CSV(默认 <sweep_dir>/sweep_summary.csv)")
    ap.add_argument("--check", action="store_true", help="只校验,不写文件")
    args = ap.parse_args()

    sweep_dir = Path(args.sweep_dir)
    if not sweep_dir.is_dir():
        print(f"✗ 不是目录: {sweep_dir}", file=sys.stderr)
        return 2

    runs = discover_runs(sweep_dir)
    if not runs:
        print(f"✗ 未找到任何 <run>/metrics.json: {sweep_dir}", file=sys.stderr)
        return 2

    rows = []
    missing = []
    for run_id, path in runs:
        metrics = load_metrics(path)
        absent = [k for k in REQUIRED_METRICS if k not in metrics]
        if absent:
            missing.append((run_id, absent))
            continue
        row = {"run_id": run_id}
        for k in REQUIRED_METRICS:
            row[k] = metrics[k]
        rows.append(row)

    if missing:
        for run_id, absent in missing:
            print(f"✗ {run_id} 缺 metric: {', '.join(absent)}", file=sys.stderr)
        return 1

    out = Path(args.out) if args.out else sweep_dir / "sweep_summary.csv"
    if args.check:
        print(f"✓ {len(rows)} 个 run 的 13 必纳 metric 齐备(未写文件)")
        return 0

    with out.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["run_id"] + REQUIRED_METRICS)
        writer.writeheader()
        writer.writerows(rows)

    print(f"✓ 已写 {out} ({len(rows)} 个 run)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
