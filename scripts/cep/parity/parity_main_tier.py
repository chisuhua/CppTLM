#!/usr/bin/env python3
"""scripts/cep/parity/parity_main_tier.py
   —— H7 主对拍档(**固定命中率档**)的分析器 —— 口径 A(per `14 号` §4.4 的 2026-10-31 裁决)

⚠️ **口径(per `14 号` §4.4 / 台账 Entry #118)**:
   H7 问的是「**带可配置固定命中率的简化模型**是否够准」。本档:

     ① gem5 侧:多条访问流(扫工作集 W)⇒ 每条**自然**产生实测命中率 `h_m` 与实测平均延迟;
     ② 本仓侧:**固定命中率参考模型** —— 用**估算规则**从 workload 特征估 `h_0`,
        预测平均延迟 `L_model(h_0) = h_0×命中延迟 + (1 − h_0)×未命中延迟`
        (命中/未命中延迟**从实现源码抽取**,per `cpptlm_params.py`;**不手抄**);
     ③ 判据:
        · **P-3 估算误差**:`|h_估 − h_m| ≤ 5 个百分点`;
        · **P-2 H7 判定**:`|L_model(h_0) − L_gem5| / L_gem5 < 15%`。

   ⚠️ **`h_0` 绝不能用实测 `h_m`** —— 那样 P-2 恒过(偏差 < 5%)⇒ **无鉴别力**。
   ⇒ 规则**只**用 workload 特征(工作集大小 / cache 容量),**不**碰实测命中率。

用法:
   python3 scripts/cep/parity/parity_main_tier.py --streams <streams.json> [--out verdict.json]
"""
import argparse
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import cpptlm_params  # noqa: E402

HIT_RATE_TOL_PP = 5.0    # P-3 阈值(per `14 号` §4.2)
LATENCY_TOL = 0.15       # P-2 阈值(per `14 号` §4.4)


def estimate_h0_rB(working_set_bytes, cache_bytes):
    """**规则 R-B**:`h_0 = min(1, C/W)`(cache 容量 / 工作集)。

    ⚠️ **依据(实测校正,2026-10-31)**:均匀随机访问 W 个地址、cache 装 C 行 ⇒
    每次访问命中「该地址当前在 cache 里」的概率 ⇒ 稳态命中率 ≈ **C/W**
    (⚠️ **不是** `1 − C/W` —— 后者是「工作集装不进 cache 的比例」,不是命中率;
     早期版本记反 ⇒ 估出的 `h_0` 全错 ⇒ 实测误差 12~100pp)。
    ⇒ `W ≤ C` 时全部命中 ⇒ `min(1, C/W)` = 1 ✓。

    ⚠️ **该规则的适用范围(关键)**:它**只对「均匀随机」访问成立**。
    真实 workload 常带热点/非均匀 ⇒ 规则会失效。
    ⇒ 见 `P-2a 鉴别力自检`:workload 集若**只含**规则吻合的流 ⇒ 判据**无鉴别力**
    ⇒ **不可据此判定 H7**。
    """
    if cache_bytes <= 0:
        return None
    return min(1.0, float(cache_bytes) / float(working_set_bytes))


def analyse(streams, tolerance=LATENCY_TOL, hit_rate_tol_pp=HIT_RATE_TOL_PP):
    p = cpptlm_params.collect()
    hit_ns = p["hit_ns"]
    miss_ns = p["miss_ns"]

    # ── 序列完整性前置断言:任何一条流被截断 ⇒ 整个分析作废 ──────────
    for st in streams:
        nom = int(st.get("nominal_accesses") or 0)
        act = int(st.get("accesses") or 0)
        if nom and act != nom:
            raise SystemExit(
                f"✗ 流 {st.get('name')} 的**序列被截断**:名义 {nom}、实测 {act}"
                f" ⇒ 整个分析作废")

    per_stream = []
    for st in streams:
        h_m = float(st["hit_rate"])
        l_gem5 = float(st["avg_latency_ns"])
        h0 = estimate_h0_rB(int(st.get("working_set_bytes", 0)),
                            int(st.get("cache_bytes", 0)))
        l_model = h0 * hit_ns + (1.0 - h0) * miss_ns
        dev_pct = abs(h0 - h_m) * 100.0
        dev_lat = abs(l_model - l_gem5) / l_gem5 if l_gem5 > 0 else None
        per_stream.append({
            "name": st["name"],
            "working_set_bytes": st.get("working_set_bytes"),
            "cache_bytes": st.get("cache_bytes"),
            "h_m": h_m,
            "h0_estimated": h0,
            "h_est_error_pp": dev_pct,
            "p3_ok": dev_pct <= hit_rate_tol_pp,
            "l_gem5": l_gem5,
            "l_model": l_model,
            "latency_deviation": dev_lat,
            "p2_ok": (dev_lat is not None and dev_lat < tolerance),
        })

    n_p3 = sum(1 for r in per_stream if r["p3_ok"])
    n_p2 = sum(1 for r in per_stream if r["p2_ok"])

    # ── **P-2a 鉴别力自检**(判据的**元要求**)──────────────────────────
    # ⚠️ 若 workload 集里**每条流**的估算误差都很小 ⇒ 说明「规则恰好对该分布最优」
    #    ⇒ 判据**分不清**「模型准」与「规则恰好蒙对」⇒ **无鉴别力** ⇒ 不可据此判 H7。
    # ⇒ 要求:workload 集**必须包含「规则失效型」流**(估算误差 > 5pp 的流 ≥ 1 条)。
    #    ⚠️ 本仓当前**做不到**:gem5 v24 的 CPU-less 驱动只有
    #       `createRandom`(均匀)与 `createLinear`(线性)⇒ **无非均匀确定性流**
    #       ⇒ 本自检**预期 FAIL** ⇒ 结论只能是「**H7 不可判**」,不是「H7 不成立」。
    discriminating = [r for r in per_stream if not r["p3_ok"]]
    p2a_ok = len(discriminating) >= 1
    verdict = {
        # ⚠️ **总判定 = P-2 ∧ P-3 ∧ P-2a** —— P-2a(鉴别力自检)是**元要求**:
        #    没它 ⇒ 「全过」可能只是「规则恰好对该分布最优」⇒ 假阳性。
        "passed": (bool(per_stream) and n_p2 == len(per_stream)
                   and n_p3 == len(per_stream) and p2a_ok),
        "p2a_discrimination_ok": p2a_ok,
        "p2a_discriminating_count": len(discriminating),
        "p2a_discriminating_streams": [r["name"] for r in discriminating],
        "p2a_note": (
            "鉴别力自检:workload 集须含「规则失效型」流(估算误差 > 5pp)。"
            "⚠️ 本仓当前**做不到** —— gem5 v24 的 CPU-less 驱动只有 "
            "`createRandom`(均匀)/`createLinear`(线性)⇒ **无非均匀确定性流**"
            "⇒ 本自检预期 FAIL ⇒ 结论只能是「**H7 不可判**」,**不是**「H7 不成立」"),
        "tolerance": tolerance,
        "hit_rate_tol_pp": hit_rate_tol_pp,
        "hit_ns": hit_ns,
        "miss_ns": miss_ns,
        "rule": "R-B: h_0 = max(0, 1 - W/C)(工作集/容量;不依赖实测命中率)",
        "per_stream": per_stream,
        "p3_passed": n_p3,
        "p2_passed": n_p2,
        "total": len(per_stream),
    }
    return verdict


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--streams", required=True,
                    help="多流结果 JSON(由 run_parity_main.sh 产出)")
    ap.add_argument("--out", default=None, help="verdict JSON 输出路径")
    args = ap.parse_args()

    streams = json.loads(pathlib.Path(args.streams).read_text(encoding="utf-8"))
    if not streams:
        raise SystemExit("✗ streams 为空")

    v = analyse(streams)

    print(f"── 估算规则:{v['rule']} ──")
    print(f"{'流':<14} {'W(B)':>9} {'h_m实测':>8} {'h_0估计':>8} "
          f"{'误差pp':>7} {'P-3':>4} {'L_gem5':>9} {'L_model':>9} "
          f"{'偏差':>7} {'P-2':>4}")
    for r in v["per_stream"]:
        m3 = "✓" if r["p3_ok"] else "✗"
        m2 = "✓" if r["p2_ok"] else "✗"
        print(f"  {r['name']:<12} {r['working_set_bytes']:>9} {r['h_m']:>8.4f} "
              f"{(r['h0_estimated'] if r['h0_estimated'] is not None else -1):>8.4f} "
              f"{r['h_est_error_pp']:>6.2f} {m3:>4} {r['l_gem5']:>9.3f} "
              f"{r['l_model']:>9.3f} "
              f"{(r['latency_deviation'] if r['latency_deviation'] is not None else -1):>6.2%} "
              f"{m2:>4}")

    print(f"\n[parity] P-3(估算误差 ≤ {v['hit_rate_tol_pp']:.0f}pp): "
          f"{v['p3_passed']}/{v['total']} 通过")
    print(f"[parity] P-2(延迟偏差 < {v['tolerance']:.0%}): "
          f"{v['p2_passed']}/{v['total']} 通过")
    print(f"[parity] P-2a(鉴别力自检:须含规则失效型流): "
          f"{'✓' if v['p2a_discrimination_ok'] else '✗'}"
          f" —— 规则失效型流 {v['p2a_discriminating_count']}/{v['total']} 条")
    if not v["p2a_discrimination_ok"]:
        print("       ⚠️ " + v["p2a_note"])
    if v["passed"]:
        print("[parity] 结论:**PASS**(H7 在口径 A 下成立)")
    elif v["p2a_discrimination_ok"]:
        print("[parity] 结论:**FAIL**(P-2 或 P-3 未达标 ⇒ H7 在口径 A 下不成立)")
    else:
        print("[parity] 结论:**不可判**(P-2/P-3 通过,但**鉴别力自检失败** ⇒ "
              "workload 集无「规则失效型」流 ⇒ 「全过」可能是假阳性)")

    if args.out:
        pathlib.Path(args.out).write_text(
            json.dumps(v, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"[parity] verdict 已写出:{args.out}")
    return 0 if v["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
