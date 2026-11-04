#!/usr/bin/env python3
"""tools/plot.py — CEP sweep 排名图

per ADR-CEP-06 §4.1:读 `sweep_summary.csv`,为每个数值 metric 生成一张排名条形图。

纯标准库:手写 SVG(不依赖 matplotlib —— 与 `run_sweep.py` 的「零项目内依赖」纪律一致)。

⚠️ 图只画 13 必纳 metric;`verification_mode` / `verification_scope` 是 run 级元数据,
   不进入图(否则会暗示「所有 run 验证深度相同」—— 见 ADR-CEP-06 §3 的越界表述禁令)。

用法:
    python3 tools/plot.py <sweep_summary.csv> [-o <out_dir>]
    python3 tools/plot.py <sweep_summary.csv> --list    # 只列出可画列
"""
import argparse
import csv
import sys
from pathlib import Path

# 不画这些列:标识列 / 非「越小越好」的派生量单独处理
SKIP_COLUMNS = {"run_id", "total_cycles", "total_transactions", "transactions_per_cycle"}

BAR_HEIGHT = 22
GAP = 6
LEFT = 220
CHART_WIDTH = 520


def load_rows(path: Path):
    with path.open(encoding="utf-8") as f:
        return list(csv.DictReader(f))


def numeric_columns(rows):
    if not rows:
        return []
    cols = []
    for key in rows[0]:
        if key in SKIP_COLUMNS:
            continue
        try:
            [float(r[key]) for r in rows]
        except (ValueError, KeyError):
            continue
        cols.append(key)
    return cols


def render_svg(metric: str, rows) -> str:
    pairs = sorted(((r["run_id"], float(r[metric])) for r in rows),
                   key=lambda kv: kv[1])
    max_v = max((v for _, v in pairs), default=0.0) or 1.0
    height = len(pairs) * (BAR_HEIGHT + GAP) + 60
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{LEFT + CHART_WIDTH + 80}" '
           f'height="{height}" role="img" aria-label="{metric} rank">']
    out.append(f'<text x="12" y="24" font-size="14" font-family="monospace">{metric} (升序)</text>')
    for i, (run_id, value) in enumerate(pairs):
        y = 40 + i * (BAR_HEIGHT + GAP)
        w = int((value / max_v) * CHART_WIDTH)
        out.append(f'<text x="12" y="{y + 16}" font-size="12" font-family="monospace">{run_id}</text>')
        out.append(f'<rect x="{LEFT}" y="{y}" width="{w}" height="{BAR_HEIGHT}" fill="#4a90d9" />')
        out.append(f'<text x="{LEFT + w + 6}" y="{y + 16}" font-size="12" '
                   f'font-family="monospace">{value:g}</text>')
    out.append("</svg>")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description="CEP sweep 排名图(纯标准库 SVG)")
    ap.add_argument("csv", help="sweep_summary.csv 路径")
    ap.add_argument("-o", "--out", help="输出目录(默认 CSV 同级 plots/)")
    ap.add_argument("--list", action="store_true", help="只列出可画的 metric 列")
    args = ap.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.is_file():
        print(f"✗ 找不到 CSV: {csv_path}", file=sys.stderr)
        return 2

    rows = load_rows(csv_path)
    if not rows:
        print("✗ CSV 无数据行", file=sys.stderr)
        return 2

    cols = numeric_columns(rows)
    if not cols:
        print("✗ 无可画的数值列", file=sys.stderr)
        return 2

    if args.list:
        for c in cols:
            print(c)
        return 0

    out_dir = Path(args.out) if args.out else csv_path.parent / "plots"
    out_dir.mkdir(parents=True, exist_ok=True)
    for metric in cols:
        (out_dir / f"{metric}_rank.svg").write_text(render_svg(metric, rows), encoding="utf-8")
    print(f"✓ 已写 {len(cols)} 张图到 {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
