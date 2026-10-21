#!/usr/bin/env python3
"""tools/render_report.py — CEP 4 件套 → report.html

per openspec/changes/cpptlm-cep-umbrella/design.md D4:
  纯 Python 标准库,零项目内依赖;输入是 4 件套目录,输出是 report.html。

用法:
    python3 tools/render_report.py <exp_dir> [-o out.html]
    python3 tools/render_report.py <exp_dir> --check
"""
import argparse
import html
import json
import os
import sys

# 4 件套契约(per ADR-CEP-06 §2)的**方向性**:
#   输入 3 件套由 runner 产出 -> 本工具消费;输出 report.html 由本工具产出。
# ⚠️ 早期版本把 4 件套一律当作"输入"校验,导致 --check 要求自己产出的
#    report.html 存在 —— 自指矛盾(check 永远红)。现按方向区分。
INPUT_ARTIFACTS = ("simulation_config.json", "trace.h5", "metrics.json")
OUTPUT_ARTIFACT = "report.html"

REQUIRED_METRICS = [
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
]

VERIFICATION_NOTE = (
    "⚠️ 本结果仅在声明的 verification_scope 内有效;"
    "bounded_random / sampled 模式**不构成**「已证明无死锁 / 无违例」。"
)


def consistency_violations(metrics):
    """一致性闸门(per ADR-CEP-06 §3 IC3):deadlock_violations == 0 且
    memory_violations == 0,否则 = **一致性违例,实验 fail**。

    ⚠️ 此前本闸门只存在于文档里 —— 三个工具**只查 metric 键是否存在**、
       从不查**值**。实测 deadlock_violations=42 / memory_violations=17 的
       实验目录:render_report --check 退出码 0、run_sweep 计入成功、
       ab_compare 输出「⚠️ B 更差」的正常 A/B 结论(2026-10-21 代码复审 B2)。

    返回**违例描述列表**;空列表 = 合规。
    """
    problems = []
    dv = metrics.get("deadlock_violations")
    mv = metrics.get("memory_violations")
    if not isinstance(dv, int) or isinstance(dv, bool) or dv < 0:
        problems.append(f"deadlock_violations 非法或缺失: {dv!r}")
    elif dv > 0:
        problems.append(f"死锁违例 {dv} 次(deadlock_violations > 0)")

    if not isinstance(mv, int) or isinstance(mv, bool) or mv < 0:
        problems.append(f"memory_violations 非法或缺失: {mv!r}")
    elif mv > 0:
        problems.append(f"一致性违例 {mv} 次(memory_violations > 0)")

    # 自洽性:memory_violations 必须 == kinds.swmr + kinds.data_value
    kinds = metrics.get("memory_violation_kinds")
    if isinstance(kinds, dict):
        try:
            subtotal = int(kinds.get("swmr", 0)) + int(kinds.get("data_value", 0))
        except (TypeError, ValueError):
            subtotal = None
        if subtotal is not None and isinstance(mv, int) and not isinstance(mv, bool):
            if subtotal != mv:
                problems.append(
                    f"memory_violations({mv}) 与 memory_violation_kinds 合计({subtotal}) 不自洽")
    return problems


def metric_type_problems(metrics):
    """13 必纳 metric 的**类型 + 非负**校验(per experimental/metrics.schema.json)。

    ⚠️ 此前 schema 从未用于校验任何真实文件 —— "total_cycles": "abc" 也能过所有闸门。
    """
    problems = []
    for k in REQUIRED_METRICS:
        v = metrics.get(k)
        if not isinstance(v, (int, float)) or isinstance(v, bool):
            problems.append(f"metric {k} 非数值: {v!r}")
        elif v < 0:
            problems.append(f"metric {k} 为负: {v!r}")
    return problems


def load_metrics(exp_dir):
    """读 metrics.json;文件缺失或**畸形**都走 sys.exit(优雅)而非未捕获 traceback。"""
    path = os.path.join(exp_dir, "metrics.json")
    if not os.path.exists(path):
        sys.exit(f"[render_report] 缺 metrics.json: {path}")
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        sys.exit(f"[render_report] metrics.json 畸形 JSON: {path} ({e})")
    except OSError as e:
        sys.exit(f"[render_report] 读 metrics.json 失败: {path} ({e})")


def check(exp_dir):
    """契约检查模式:**输入 3 件套**齐备 + 13 必纳 metric + verification 声明。

    注意:report.html 是**本工具的产物**,不是输入 —— 故不作为 check 的失败条件,
    只作为状态报告(已渲染 / 未渲染)。完整实验目录(4 件套)的校验见
    `run_sweep.py --require-complete`。
    """
    problems = []
    for name in INPUT_ARTIFACTS:
        if not os.path.exists(os.path.join(exp_dir, name)):
            problems.append(f"输入件套缺: {name}")
    try:
        m = load_metrics(exp_dir)
    except SystemExit as e:
        problems.append(str(e))
        m = {}
    for k in REQUIRED_METRICS:
        if k not in m:
            problems.append(f"13 必纳 metric 缺: {k}")
    if m:
        problems.extend(metric_type_problems(m))
        # ⚠️ 一致性闸门(ADR-CEP-06 IC3)—— 此前**只查键存在**,违例实验照样通过
        problems.extend(consistency_violations(m))
    cfg_path = os.path.join(exp_dir, "simulation_config.json")
    cfg = {}
    if os.path.exists(cfg_path):
        with open(cfg_path, encoding="utf-8") as f:
            cfg = json.load(f)
        if "verification_mode" not in cfg:
            problems.append("simulation_config.json 缺 verification_mode")
        if "verification_scope" not in cfg:
            problems.append("simulation_config.json 缺 verification_scope")
    else:
        problems.append("缺 simulation_config.json")
    rendered = os.path.exists(os.path.join(exp_dir, OUTPUT_ARTIFACT))
    if problems:
        for p in problems:
            print(f"  ✗ {p}")
        return 1
    state = "已渲染" if rendered else "未渲染(本工具的产物)"
    print(f"  ✓ 输入 3 件套齐备 + 13 必纳 metric + verification 声明完整 + 一致性闸门通过")
    print(f"  · {OUTPUT_ARTIFACT}: {state}")
    return 0


def render(exp_dir, out_path):
    m = load_metrics(exp_dir)
    # verification 声明的**唯一真源** = simulation_config.json(per ADR-CEP-06 §4)
    # ⚠️ 此前 check() 读 simulation_config、render() 却读 metrics.json →
    #    按 check 通过的目录渲染,报告里却显示「未声明」。
    cfg = {}
    cfg_path = os.path.join(exp_dir, "simulation_config.json")
    if os.path.exists(cfg_path):
        with open(cfg_path, encoding="utf-8") as f:
            cfg = json.load(f)
    violations = consistency_violations(m)
    rows = []
    for k in REQUIRED_METRICS:
        v = m.get(k, "—")
        extra = ""
        if k in ("deadlock_violations", "memory_violations"):
            kinds = m.get("memory_violation_kinds")
            if kinds:
                extra = " (" + ", ".join(f"{kk}={vv}" for kk, vv in sorted(kinds.items())) + ")"
        rows.append(f"<tr><td><code>{html.escape(k)}</code></td><td>{html.escape(str(v))}{extra}</td></tr>")
    for k, v in sorted(m.items()):
        if k in REQUIRED_METRICS or k in ("memory_violation_kinds",):
            continue
        if isinstance(v, (dict, list)):
            v = json.dumps(v, ensure_ascii=False)
        rows.append(f"<tr><td><code>{html.escape(k)}</code></td><td>{html.escape(str(v))}</td></tr>")

    mode = cfg.get("verification_mode") or m.get("verification_mode", "未声明")
    scope = cfg.get("verification_scope") or m.get("verification_scope", "未声明")
    if violations:
        violation_html = ("<div class=\"bad\"><strong>❌ 一致性违例(实验 fail)</strong><ul>" +
                          "".join(f"<li>{html.escape(v)}</li>" for v in violations) + "</ul></div>")
    else:
        violation_html = "<div class=\"ok\"><strong>✓ 一致性闸门通过</strong>(deadlock_violations == 0 且 memory_violations == 0)</div>"
    doc = f"""<!doctype html>
<html lang="zh"><head><meta charset="utf-8">
<title>CEP 实验报告 — {html.escape(os.path.basename(os.path.abspath(exp_dir)))}</title>
<style>
 body{{font-family:system-ui,sans-serif;margin:2rem;color:#222}}
 table{{border-collapse:collapse;margin:1rem 0}}
 td,th{{border:1px solid #ccc;padding:.35rem .7rem;text-align:left}}
 td:first-child{{font-family:ui-monospace,monospace}}
 .warn{{background:#fff4e5;border-left:4px solid #e8a33d;padding:.7rem 1rem}}
 .ok{{background:#eefaf0;border-left:4px solid #3a9;padding:.7rem 1rem}}
 .bad{{background:#ffecec;border-left:4px solid #c33;padding:.7rem 1rem}}
</style></head><body>
<h1>CEP 实验报告</h1>
<p><strong>实验目录</strong>: <code>{html.escape(os.path.abspath(exp_dir))}</code></p>
{violation_html}
<div class="warn"><strong>验证深度</strong>: {html.escape(mode)}<br>
<strong>验证范围</strong>: {html.escape(scope)}<br>{VERIFICATION_NOTE}</div>
<h2>指标</h2>
<table><tr><th>Metric</th><th>值</th></tr>{''.join(rows)}</table>
</body></html>
"""
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(doc)
    print(f"[render_report] 已生成 {out_path}")
    if violations:
        for v in violations:
            print(f"  ❌ {v}")
        print("[render_report] 实验 fail(一致性违例)")
        return 1
    return 0


def main():
    ap = argparse.ArgumentParser(description="CEP 4 件套 → report.html")
    ap.add_argument("exp_dir", help="实验目录(含 4 件套)")
    ap.add_argument("-o", "--out", help="输出 HTML(默认 <exp_dir>/report.html)")
    ap.add_argument("--check", action="store_true", help="只做契约检查,不生成")
    args = ap.parse_args()
    if not os.path.isdir(args.exp_dir):
        sys.exit(f"[render_report] 目录不存在: {args.exp_dir}")
    if args.check:
        return check(args.exp_dir)
    out = args.out or os.path.join(args.exp_dir, "report.html")
    return render(args.exp_dir, out)


if __name__ == "__main__":
    sys.exit(main())
