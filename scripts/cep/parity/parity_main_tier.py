#!/usr/bin/env python3
"""scripts/cep/parity/parity_main_tier.py
   —— H7 主对拍档(**固定命中率档**)的分析器

⚠️ **口径(per `docs/architecture/14-coherence-parity-baseline.md` §4.2 的 2026-10-31 修订)**:
   H7 问的是「**带可配置固定命中率的简化模型**是否够准」,**不是**「简化模型改造后
   能否变成真实 cache」⇒ 本档**不改**本仓模型形态,而是:

     ① gem5 侧:跑**多条**访问流,每条**自然**产生一个实测命中率 `h_m` 与实测平均延迟;
     ② 本仓侧:**固定命中率参考模型** —— 预测平均延迟
        `L_model(h_0) = h_0 × 命中延迟 + (1 − h_0) × 未命中延迟`
        (命中/未命中延迟**从实现源码抽取**,per `cpptlm_params.py`;**不手抄**);
     ③ 判据:
        · **P-3 命中率对齐**:构造检查 —— 每条流能命中目标 `h` ± 5pp
          (证明「两段区间」确实能**连续调** `h`);
        · **P-2 H7 判定**:扫 `h_0` 网格 ⇒ 对**每条流**算
          `|L_model(h_0) − L_gem5| / L_gem5` ⇒ 取 **最大偏差**
          ⇒ **存在某个 `h_0` 使所有流的偏差 < 15%** ⇒ H7 在该 `h_0` 下成立。

   ⇒ **鉴别力来源**:各条流的 `h_m` **分散**(不瞄准)⇒ 若不存在统一 `h_0`,
     最小最大偏差会 > 15% ⇒ **H7 判不成立** ⇒ 这是**可证伪**的结论。

用法:
   python3 scripts/cep/parity/parity_main_tier.py --streams <streams.json> [--out verdict.json]

`streams.json` 形态(由 `run_parity_main.sh` 产出):
   [{ "name": ..., "hit_rate": h_m, "avg_latency_ns": L_gem5,
      "hits": ..., "misses": ..., "accesses": ... }, ...]
"""
import argparse
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import cpptlm_params  # noqa: E402

# `h_0` 网格:0.00 ~ 1.00,步长 0.05 ⇒ 21 个候选
H0_GRID = [round(i * 0.05, 2) for i in range(21)]


def analyse(streams, tolerance=0.15, h0_grid=None):
    """对多条流 × `h_0` 网格算偏差,给出 H7 判定。"""
    grid = h0_grid if h0_grid is not None else H0_GRID
    p = cpptlm_params.collect()
    hit_ns = p["hit_ns"]
    miss_ns = p["miss_ns"]

    rows = []
    for h0 in grid:
        l_model = h0 * hit_ns + (1.0 - h0) * miss_ns
        devs = []
        for s in streams:
            l_gem5 = float(s["avg_latency_ns"])
            if l_gem5 <= 0:
                continue
            dev = abs(l_model - l_gem5) / l_gem5
            devs.append((s["name"], dev))
        if not devs:
            continue
        max_dev = max(d for _, d in devs)
        rows.append({
            "h0": h0,
            "model_avg_latency_ns": l_model,
            "max_deviation": max_dev,
            "per_stream": {n: d for n, d in devs},
            "ok": max_dev < tolerance,
        })

    best = min(rows, key=lambda r: r["max_deviation"]) if rows else None

    # ── **对齐对照**(登记项,**不参与 verdict**)─────────────────────────
    # ⚠️ 排除「模型本身不准」这个**替代解释**:
    #    若 `h_0 = h_m`(用**实测**命中率当配置值)⇒ 偏差应只来自
    #    「未命中延迟常数 50ns vs gem5 实测 48.997ns」的 ~2% ⇒ 偏差应 < 3%。
    #    ⇒ 若对齐后偏差也很小 ⇒ **模型准确**,失败原因是「`h_0` 的选取规则」
    #      (而非「简化模型不够准」)⇒ 这是**归因**的关键区分。
    aligned = []
    for st in streams:
        h_m = float(st.get("hit_rate", 0.0))
        l_gem5 = float(st["avg_latency_ns"])
        l_model = h_m * hit_ns + (1.0 - h_m) * miss_ns
        dev = abs(l_model - l_gem5) / l_gem5 if l_gem5 > 0 else None
        aligned.append({
            "name": st["name"],
            "h_m": h_m,
            "l_gem5": l_gem5,
            "l_model_at_h_m": l_model,
            "deviation": dev,
        })
    aligned_max = max((a["deviation"] for a in aligned if a["deviation"] is not None),
                       default=None)
    verdict = {
        "passed": bool(best and best["ok"]),
        "tolerance": tolerance,
        "hit_ns": hit_ns,
        "miss_ns": miss_ns,
        "h0_grid": grid,
        "streams": [
            {"name": s["name"], "hit_rate": s.get("hit_rate"),
             "avg_latency_ns": s.get("avg_latency_ns"),
             "hits": s.get("hits"), "misses": s.get("misses"),
             "accesses": s.get("accesses")}
            for s in streams
        ],
        "rows": rows,
        "best": best,
        "aligned_control": {
            "note": ("登记项:把 h_0 设成**实测** h_m ⇒ 偏差应只来自未命中延迟常数"
                     "(50ns vs gem5 实测)⇒ 用来区分「模型不准」与「h_0 选取不当」"),
            "per_stream": aligned,
            "max_deviation": aligned_max,
            "model_is_accurate": (aligned_max is not None and aligned_max < 0.05),
        },
    }
    return verdict


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--streams", required=True, help="多流结果 JSON(由 run_parity_main.sh 产出)")
    ap.add_argument("--out", default=None, help="verdict JSON 输出路径")
    args = ap.parse_args()

    streams = json.loads(pathlib.Path(args.streams).read_text(encoding="utf-8"))
    if not streams:
        raise SystemExit("✗ streams 为空")

    v = analyse(streams)

    print("── 各条流的实测命中率 / 平均延迟 ──")
    for s in v["streams"]:
        print(f"  {s['name']:<28} h_m={s['hit_rate']:.4f}  L_gem5={s['avg_latency_ns']:.3f}ns")
    print(f"\n── 参考模型:命中 {v['hit_ns']}ns / 未命中 {v['miss_ns']}ns;"
          f"容差 {v['tolerance']:.0%} ──")
    print(f"  {'h_0':>6} {'L_model':>10} {'最大偏差':>10}  判定")
    for r in v["rows"]:
        mark = "✓" if r["ok"] else "✗"
        print(f"  {r['h0']:>6.2f} {r['model_avg_latency_ns']:>10.3f} "
              f"{r['max_deviation']:>9.2%}  {mark}")
    if v["best"]:
        b = v["best"]
        print(f"\n[parity] 最优 h_0 = {b['h0']:.2f} · 最大偏差 = {b['max_deviation']:.2%} "
              f"⇒ {'**PASS**(存在统一 h_0 使所有流偏差 < 阈值)' if v['passed'] else '**FAIL**(无统一 h_0 ⇒ H7 在该口径下不成立)'}")

    ac = v["aligned_control"]
    if ac["max_deviation"] is not None:
        print(f"\n── 对齐对照(h_0 = 实测 h_m;**登记项**,不参与 verdict)──")
        for a in ac["per_stream"]:
            print(f"  {a['name']:<28} h_m={a['h_m']:.4f} "
                  f"L_model={a['l_model_at_h_m']:.3f}ns L_gem5={a['l_gem5']:.3f}ns "
                  f"偏差={a['deviation']:.2%}")
        print(f"  ⇒ 对齐后最大偏差 = {ac['max_deviation']:.2%} ⇒ "
              f"**{'模型本身准确' if ac['model_is_accurate'] else '模型本身也不准'}**"
              f"(⇒ 归因指向「h_0 选取规则」而非「简化模型不准」"
              f"{'' if ac['model_is_accurate'] else '⚠️'})")

    if args.out:
        pathlib.Path(args.out).write_text(
            json.dumps(v, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"[parity] verdict 已写出:{args.out}")
    return 0 if v["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
