#!/usr/bin/env python3
"""tools/ab_compare.py — CEP A/B 对比

per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/design.md D4:纯标准库,零项目内依赖。
输入两个实验目录(A / B),输出 delta_metrics.json + CDF overlay 的 HTML 片段。

用法:
    python3 tools/ab_compare.py <dir_a> <dir_b> [-o out_dir]
    python3 tools/ab_compare.py <dir_a> <dir_b> --check
"""
import argparse
import html
import json
import os
import sys

# 13 必纳 metric(per ADR-CEP-06 §2)—— 与 render_report / run_sweep 同源
REQUIRED_METRICS = [
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
]

NUMERIC_METRICS = [
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
]

# 「更好」方向(用于 delta 标注)。**两个集合都必须穷举** NUMERIC_METRICS:
# 落单的指标会被标成"方向未知",其 delta 仍会显示但**不带优劣标记** ——
# 这容易被误读成"没有差异",实际是"方向未定义"。
LOWER_IS_BETTER = {
    "total_cycles", "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls", "collective_stalls",
    "directory_misses", "deadlock_violations", "memory_violations",
}
# 越大越好的指标(吞吐类)
HIGHER_IS_BETTER = {
    "transactions_per_cycle",
}
# **中性**指标:只显示 delta,**不标优劣**。
# ⚠️ total_transactions 是**工作量**不是质量指标 —— 归入 HIGHER 会让
#    「跑得更久」得到 ✅「B 更优」,同时 total_cycles 被判 ⚠️「B 更差」→ 自相矛盾。
NEUTRAL_METRICS = {
    "total_transactions",
}


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


def _load_json(path, label):
    """读 JSON;文件缺失或**畸形**都走 sys.exit(优雅)而非未捕获 traceback。"""
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        sys.exit(f"[ab_compare] 缺 {label}: {path}")
    except json.JSONDecodeError as e:
        sys.exit(f"[ab_compare] {label} 畸形 JSON: {path} ({e})")
    except OSError as e:
        sys.exit(f"[ab_compare] 读 {label} 失败: {path} ({e})")


def load_metrics(exp_dir):
    p = os.path.join(exp_dir, "metrics.json")
    if not os.path.exists(p):
        sys.exit(f"[ab_compare] 缺 metrics.json: {p}")
    return _load_json(p, "metrics.json")


def load_cfg(exp_dir):
    p = os.path.join(exp_dir, "simulation_config.json")
    if not os.path.exists(p):
        return {}
    return _load_json(p, "simulation_config.json")


def check(a, b):
    problems = []
    for d in (a, b):
        if not os.path.isdir(d):
            problems.append(f"目录不存在: {d}")
        for name in ("simulation_config.json", "metrics.json"):
            if not os.path.exists(os.path.join(d, name)):
                problems.append(f"缺 {name}: {d}")
        cfg = load_cfg(d) if os.path.isdir(d) else {}
        if cfg:
            for k in ("verification_mode", "verification_scope"):
                if k not in cfg:
                    problems.append(f"{d}/simulation_config.json 缺 {k}")
        # 一致性闸门(ADR-CEP-06 IC3)—— 此前**只查键存在**,违例实验照样出报告
        mpath = os.path.join(d, "metrics.json")
        if os.path.isdir(d) and os.path.exists(mpath):
            try:
                with open(mpath, encoding="utf-8") as f:
                    m = json.load(f)
            except (json.JSONDecodeError, OSError) as e:
                problems.append(f"{d}/metrics.json 读取失败: {e}")
                m = None
            if m is not None:
                problems.extend(metric_type_problems(m))
                problems.extend(consistency_violations(m))
    if problems:
        for p in problems:
            print(f"  ✗ {p}")
        return 1
    print("  ✓ A/B 两侧 2 件套(metrics + config)齐备 + verification 声明完整 + 一致性闸门通过")
    return 0


def compare(a, b, out_dir):
    ma, mb = load_metrics(a), load_metrics(b)
    cfg_a, cfg_b = load_cfg(a), load_cfg(b)

    # S9:A/B 两侧 verification 声明**必须一致**,否则结论不可比
    ver_mismatch = []
    for k in ("verification_mode", "verification_scope"):
        va, vb = cfg_a.get(k, ""), cfg_b.get(k, "")
        if va != vb:
            ver_mismatch.append(f"{k}: A={va!r} vs B={vb!r}")

    deltas = {}
    unknown_direction = []
    for k in NUMERIC_METRICS:
        va, vb = ma.get(k), mb.get(k)
        if isinstance(va, (int, float)) and isinstance(vb, (int, float)):
            d = vb - va
            pct = (d / va * 100.0) if va else None
            better = None
            if d != 0:
                if k in NEUTRAL_METRICS:
                    pass    # 中性指标:只显示 delta,不标优劣
                elif ver_mismatch:
                    better = None   # 两侧验证声明不一致 → 结论不可比,不标优劣
                elif k in LOWER_IS_BETTER:
                    better = d < 0
                elif k in HIGHER_IS_BETTER:
                    better = d > 0
                else:
                    # 方向未定义:保持 None,并在报告里显式标注,避免被误读为"无差异"
                    unknown_direction.append(k)
            deltas[k] = {"a": va, "b": vb, "delta": d,
                         "delta_pct": pct, "b_is_better": better}
        else:
            deltas[k] = {"a": va, "b": vb, "delta": None,
                         "delta_pct": None, "b_is_better": None}

    payload = {
        "a_dir": os.path.abspath(a),
        "b_dir": os.path.abspath(b),
        "verification": {
            "a": {k: cfg_a.get(k, "") for k in ("verification_mode", "verification_scope")},
            "b": {k: cfg_b.get(k, "") for k in ("verification_mode", "verification_scope")},
        },
        "deltas": deltas,
        "unknown_direction": unknown_direction,
        "verification_mismatch": ver_mismatch,
        "caveat": ("⚠️ A/B 结论仅在各自身声明的 verification_scope 内有效;"
                   "bounded_random / sampled **不构成**「已证明无死锁 / 无违例」。"),
    }
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "delta_metrics.json"), "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2)

    rows = []
    for k in NUMERIC_METRICS:
        d = deltas[k]
        if d["delta"] is None:
            continue
        pct = "—" if d["delta_pct"] is None else f"{d['delta_pct']:+.1f}%"
        mark = ""
        if d["b_is_better"] is True:
            mark = " ✅ B 更优"
        elif d["b_is_better"] is False:
            mark = " ⚠️ B 更差"
        elif k in NEUTRAL_METRICS:
            mark = " (中性指标,不标优劣)"
        elif d["delta"] != 0 and k in unknown_direction:
            mark = " (方向未定义)"
        rows.append(f"<tr><td><code>{k}</code></td><td>{d['a']}</td><td>{d['b']}</td>"
                    f"<td>{d['delta']:+}</td><td>{pct}</td><td>{mark}</td></tr>")
    # ⚠️ 局部变量名 html 会**遮蔽** html 模块 → 改名 html_doc
    #    (此前此处 NameError: name 'html' is not defined)
    html_doc = f"""<div class="ab-compare">
<h2>A/B 对比</h2>
{"<div class=\"bad\"><strong>⚠️ A/B 两侧 verification 声明不一致 —— 结论不可比,不标优劣</strong><ul>" + "".join("<li>" + html.escape(x) + "</li>" for x in ver_mismatch) + "</ul></div>" if ver_mismatch else ""}
<table><tr><th>Metric</th><th>A</th><th>B</th><th>Δ</th><th>Δ%</th><th></th></tr>{''.join(rows)}</table>
<p class="caveat">{payload['caveat']}</p>
{"<p>⚠️ 方向未定义的指标: " + html.escape(", ".join(unknown_direction)) + "</p>" if unknown_direction else ""}
</div>"""
    with open(os.path.join(out_dir, "ab_compare.html"), "w", encoding="utf-8") as f:
        f.write(html_doc)
    print(f"[ab_compare] 已写 {out_dir}/delta_metrics.json + ab_compare.html")
    return 0


def main():
    ap = argparse.ArgumentParser(description="CEP A/B 对比")
    ap.add_argument("dir_a")
    ap.add_argument("dir_b")
    ap.add_argument("-o", "--out", default="ab_compare_out")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    if args.check:
        return check(args.dir_a, args.dir_b)
    return compare(args.dir_a, args.dir_b, args.out)


if __name__ == "__main__":
    sys.exit(main())
