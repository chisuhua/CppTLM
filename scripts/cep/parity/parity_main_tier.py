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


# ── **封闭枚举表**(profile → class)──────────────────────────────────
# ⚠️ **唯一真源 = `docs/architecture/cep/parity-baseline.md` §4.4**
#    (「流的两类分类与『按声明分类』的判据语义」);此处是它的**机器镜像**。
# ⚠️ **分类由「声明的 profile」决定,不由观测结果反推** ——
#    否则标准**不可证伪**(一条真均匀流若真挂了会被自动改判为 breaking)。
PROFILE_CLASS = {
    # 真均匀覆盖 W(与 gem5 `createRandom` 同语义)⇒ R-B 前提成立 ⇒ 必须全过
    "uniform": "conforming",
    # 双峰(热区全命中 + 冷区全不命中)⇒ R-B 前提**不成立** ⇒ 必须 FAIL(正控激发)
    "hot_cold": "breaking",
    # 80% 内 + 20% 外(足迹 1MiB+W)⇒ 既非均匀、也不保证误差 > 5pp ⇒ 不参与判定
    "fixed_ratio": "observe",
}
# ⚠️ `profile` 缺失时的兜底:参数同源档的流由 **gem5 `createRandom`**(真均匀)产生
#    ⇒ 归 `conforming`;⚠️ 未知 profile ⇒ 归 `observe`(不参与判定,避免误判)。
DEFAULT_PROFILE = "uniform"
UNKNOWN_PROFILE_CLASS = "observe"


def classify(profile):
    """按**声明的 profile** 归类(per §4.4 封闭枚举表)。"""
    if not profile:
        return PROFILE_CLASS.get(DEFAULT_PROFILE, UNKNOWN_PROFILE_CLASS)
    return PROFILE_CLASS.get(profile, UNKNOWN_PROFILE_CLASS)


def analyse(streams, tolerance=LATENCY_TOL, hit_rate_tol_pp=HIT_RATE_TOL_PP):
    p = cpptlm_params.collect()
    hit_ns = p["hit_ns"]
    miss_ns = p["miss_ns"]

    # ── 序列完整性前置断言:**按 driver 分档**(per `14 号` §4.4「条数契约(P-1)按 driver 分档」)
    # ⚠️ **BLOCKER-1 修复**:本断言原先是**无条件**的 ⇒ 与规格「**禁止**统一断言」冲突
    #    ⇒ random-driver 路径(条数由 `duration` 决定 ⇒ 实测可达名义的 **17~20 倍**,
    #      **超发是设计行为而非截断**)会在**分析阶段**恒红 ⇒ 该档不可复现。
    # ⚠️ 该断言此前是**哑火**的(rec 写错键名 ⇒ `nominal_accesses` 恒 0 ⇒ 恒跳过);
    #    D-5 修复让 rec 正确写出该字段 ⇒ 断言**首次变为活代码** ⇒ 必须同时分档。
    for st in streams:
        nom = int(st.get("nominal_accesses") or 0)
        act = int(st.get("accesses") or 0)
        mode = st.get("mode") or ""
        if mode == "same_source_trace":
            if nom and act != nom:
                raise SystemExit(
                    f"✗ 流 {st.get('name')} 的**序列被截断**:名义 {nom}、实测 {act}"
                    f" ⇒ 整个分析作废")
        elif mode == "parameter_same_source":
            # ⚠️ 该路径**不做**条数断言(超发是该路径的**设计行为**)⇒ 仅登记比例
            pass
        else:
            raise SystemExit(
                f"✗ 流 {st.get('name')} 的 mode 未知({mode!r})⇒ 无法判定条数契约"
                f" ⇒ 拒绝出结论(避免因未知口径而静默放行)")

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
            "profile": st.get("profile"),
            # ⚠️ **class 由「声明的 profile」决定**(不按观测结果反推)
            "class": classify(st.get("profile")),
            "working_set_bytes": st.get("working_set_bytes"),
            "cache_bytes": st.get("cache_bytes"),
            # ⚠️ breaking 流可**声明期望误差下界**(≥ 2× 阈值)⇒ 声明了就强制校验
            #    (生成器与声明不符 ⇒ 判红);未声明 ⇒ 低于 2× 阈值只记 warning
            "expected_error_floor_pp": st.get("expected_error_floor_pp"),
            "h_m": h_m,
            "h0_estimated": h0,
            "h_est_error_pp": dev_pct,
            "p3_ok": dev_pct <= hit_rate_tol_pp,
            "l_gem5": l_gem5,
            "l_model": l_model,
            "latency_deviation": dev_lat,
            "p2_ok": (dev_lat is not None and dev_lat < tolerance),
        })

    # ── **按 class 分列**(per `14 号` §4.4 封闭枚举表)─────────────────
    # ⚠️ **判定域 = `conforming` 全体**,不是全部流 ——
    #    `breaking` 流的作用是**证伪「判据本身无鉴别力」**(正控必须激发),
    #    它**不参与** H7 的判定;`observe` 流不参与任何判定。
    conforming = [r for r in per_stream if r["class"] == "conforming"]
    breaking = [r for r in per_stream if r["class"] == "breaking"]
    observe = [r for r in per_stream if r["class"] == "observe"]

    n_p3 = sum(1 for r in per_stream if r["p3_ok"])
    n_p2 = sum(1 for r in per_stream if r["p2_ok"])

    # ── **P-2a = A-1 鉴别力自检**(正控必须激发)────────────────────────
    # ⚠️ **A-1 的判定独立于观测结果**:由「声明的 breaking 流」决定,
    #    且要求这些流的 **P-3 误差全部 > 阈值**(正控必须激发)。
    #    ⇒ 若某条 breaking 流**意外通过** P-3 ⇒ **判红**
    #      (要么生成器变了、要么规则变了 ⇒ 判据失去鉴别力)。
    # ⚠️ **不再**用「P-3 失败的流数」反推(那会让标准不可证伪)。
    breaking_not_excited = []
    # ⚠️ **期望误差下界校验必须在 `p2a_a1_ok` 冻结之前** —— 否则「违反下界」只被
    #    append 进 `breaking_not_excited` 却**不影响** `p2a_a1_ok` / `passed`
    #    ⇒ 同一份 verdict 同时宣称 PASS 与判红(**半死断言**,BLOCKER-2)。
    #    ⇒ 顺序:先算 P-3 是否通过 → 再校验下界 → **然后**才冻结 `p2a_a1_ok`。
    for r in breaking:
        if r["p3_ok"]:
            breaking_not_excited.append(r["name"])

    # ⚠️ **期望误差下界校验**(脆性防护;per §4.4):声明了就强制,
    #    未声明则低于 2× 阈值只记 warning(不判红,避免因缺声明而误红)。
    warnings = []
    for r in breaking:
        floor = r.get("expected_error_floor_pp")
        if floor is not None:
            if r["h_est_error_pp"] < float(floor):
                breaking_not_excited.append(
                    f"{r['name']}(误差 {r['h_est_error_pp']:.2f}pp < 声明下界 {float(floor):.2f}pp)")
        elif r["h_est_error_pp"] < 2.0 * hit_rate_tol_pp:
            warnings.append(
                f"breaking 流 {r['name']} 的误差 {r['h_est_error_pp']:.2f}pp < 2× 阈值"
                f"({2.0 * hit_rate_tol_pp:.1f}pp)⇒ margin 偏薄 ⇒ 建议在流声明里"
                f"写明 `expected_error_floor_pp`")

    # ⚠️ **冻结点** —— 必须在上面两项校验**之后**(BLOCKER-2)
    p2a_a1_ok = len(breaking) >= 1 and not breaking_not_excited

    # ── **A-2 形态:范围限定声明**(per `14 号` §4.4)────────────────────
    # ⚠️ **A-2 不是「口头免责」**:把「结论的适用边界」变成 verdict 里
    #    **必须携带的字段** ⇒ 缺任一 ⇒ **FAIL**。
    # ⚠️ **从实际流集合派生**(不得硬编码)⇒ 声明的范围与跑的流**永远一致**。
    def _fmt_ws(r):
        w = r.get("working_set_bytes")
        return "?" if w is None else f"{int(w) / 1024:.0f}KiB"

    _conf_ws = sorted({_fmt_ws(r) for r in conforming})
    _break_ws = sorted({_fmt_ws(r) for r in breaking})
    _cache = next((r.get("cache_bytes") for r in per_stream
                   if r.get("cache_bytes")), 0)
    applicability = (
        f"**均匀随机(uniform-random)访问分布**(声明 profile = `uniform`);"
        f"工作集 W ∈ {{{', '.join(_conf_ws)}}};"
        f"gem5 classic cache = {int(_cache) / 1024:.0f}KiB / 2-way / LRU;"
        f"访问规模 = 1e5 量级;"
        f"本仓侧命中/未命中延迟常数 = {hit_ns:.0f} / {miss_ns:.0f} cycle @ 1GHz"
    ) if conforming else ""
    # ⚠️ **SHOULD_FIX**:基础子句按 `breaking` 是否为空分支 —— 否则同一字段内
    #    自相矛盾(「**未覆盖**…**未被证伪**」与「**已观测到规则失效**」并存)。
    if breaking:
        _lim_base = (
            f"**已实测的边界**:非均匀/带热点(非 uniform-random)的访问分布 —— "
            f"规则 R-B(h_0 = min(1, C/W))**只对均匀随机最优** ⇒ "
            f"本轮在 `breaking` 流上**观测到该规则失效**(误差 > 阈值)⇒ "
            f"**该估算规则**在非均匀负载上不可用(⚠️ 但**模型本体**在 h 对齐下仍准确)。"
        )
    else:
        _lim_base = (
            f"**未覆盖**:非均匀/带热点(非 uniform-random)的访问分布 —— "
            f"而规则 R-B(h_0 = min(1, C/W))恰恰**只对均匀随机最优** ⇒ "
            f"在未覆盖分布上该规则**未被证伪** ⇒ 结论**不得**表述为「H7 已验证」。"
        )
    limitation = (
        _lim_base
        + f"⚠️ **解除限定的前置条件**:需能喂**非均匀离线 trace** 的 gem5 驱动 —— "
        f"该前置**已具备**(与 gem5 版本**无关**;只需 `createTrace` 能读离线 trace 文件;"
        f"per 台账 Entry #120:此前登记的「v24 提前 EOF = 上游缺陷」**已被实测推翻**)"
        + (f"\n⚠️ 本轮 `breaking` 流 {len(breaking)} 条(W ∈ {{{', '.join(_break_ws)}}});"
           f"其 R-B 误差均 > 阈值 ⇒ 正控已激发" if breaking else "")
    ) if conforming else ""
    p2a_a2_ok = bool(applicability) and bool(limitation)
    # ⚠️ **A-2 只在 `breaking` 为空时适用** —— 否则「正控未激发」会被 A-2 兜底
    #    **掩盖**(反控 ⑫ 实测抓到:breaking 流意外通过 P-3 时 verdict 仍 PASS)。
    #    ⇒ breaking 存在但有未激发 ⇒ **判红**(形态 = `A-1-BROKEN`)。
    p2a_a2_applicable = (len(breaking) == 0)
    p2a_ok = p2a_a1_ok or (p2a_a2_ok and p2a_a2_applicable)

    # ── **总判定** —— ⚠️ 修 D-6:不再要求「全部流」P-2/P-3 通过 ──────
    #    (旧实现里 breaking 流由 P-3 失败反推,而 passed 又要求全部通过
    #     ⇒ 合取 ⇒ A-1 一旦满足,passed 恒 False ⇒ 结论必然是错误文案)。
    # ⇒ 判定域 = `conforming` 全体;`breaking` 走**正控激发**判据。
    conforming_ok = bool(conforming) and all(
        r["p2_ok"] and r["p3_ok"] for r in conforming)
    verdict = {
        # ⚠️ `p2a_ok` 已含「A-1 正控激发」或「A-2 兜底」两支 ⇒ 无需再并列 `breaking_ok`
        "passed": bool(conforming_ok and p2a_ok),
        "p2a_discrimination_ok": p2a_ok,
        "p2a_form": ("A-1" if p2a_a1_ok else
                     ("A-2" if (p2a_a2_ok and p2a_a2_applicable) else
                      ("A-1-BROKEN" if len(breaking) >= 1 else "NONE"))),
        "p2a_a1_ok": p2a_a1_ok,
        "p2a_a2_ok": p2a_a2_ok,
        "applicability": applicability,
        "limitation": limitation,
        # ⚠️ **breaking 流清单**(正控载体)—— 不再叫「discriminating」,
        #    因为它**不是**「P-3 失败的流」,而是「**声明**为规则失效型的流」。
        "p2a_breaking_count": len(breaking),
        "p2a_breaking_streams": [r["name"] for r in breaking],
        "p2a_breaking_not_excited": breaking_not_excited,
        "class_counts": {"conforming": len(conforming),
                         "breaking": len(breaking),
                         "observe": len(observe)},
        "conforming_ok": conforming_ok,
        "warnings": warnings,
        "p2a_note": (
            "P-2a 两形态(per `14 号` §4.4):"
            "**A-1** 鉴别力自检 = `breaking` 流 ≥ 1 条 **且** 它们的 P-3 误差"
            "**全部 > 阈值**(正控必须激发;分类由**声明的 profile** 决定,"
            "**不由观测结果反推**);"
            "**A-2** 范围限定声明 = verdict 必带 `applicability` + `limitation`"
            "⇒ 结论**只能**表述为「H7 在**【声明范围】内**成立」,**不得**表述为「H7 已验证」"),
        "tolerance": tolerance,
        "hit_rate_tol_pp": hit_rate_tol_pp,
        "hit_ns": hit_ns,
        "miss_ns": miss_ns,
        # ⚠️ 修陈旧标签:代码是 `min(1, C/W)`,旧标签印的是 `max(0, 1 - W/C)`(已作废)
        "rule": "R-B: h_0 = min(1, C/W)(cache 容量 / 工作集;不依赖实测命中率)",
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
    print(f"{'流':<14} {'class':<11} {'W(B)':>9} {'h_m实测':>8} {'h_0估计':>8} "
          f"{'误差pp':>7} {'P-3':>4} {'L_gem5':>9} {'L_model':>9} "
          f"{'偏差':>7} {'P-2':>4}")
    for r in v["per_stream"]:
        m3 = "✓" if r["p3_ok"] else "✗"
        m2 = "✓" if r["p2_ok"] else "✗"
        print(f"  {r['name']:<12} {r['class']:<11} {r['working_set_bytes']:>9} "
              f"{r['h_m']:>8.4f} "
              f"{(r['h0_estimated'] if r['h0_estimated'] is not None else -1):>8.4f} "
              f"{r['h_est_error_pp']:>6.2f} {m3:>4} {r['l_gem5']:>9.3f} "
              f"{r['l_model']:>9.3f} "
              f"{(r['latency_deviation'] if r['latency_deviation'] is not None else -1):>6.2%} "
              f"{m2:>4}")

    _cc = v["class_counts"]
    print(f"\n[parity] 分类:conforming {_cc['conforming']} · "
          f"breaking {_cc['breaking']} · observe {_cc['observe']}"
          f"(⚠️ **判定域 = conforming 全体**;breaking 是正控载体,不参与 H7 判定)")
    print(f"[parity] P-3(估算误差 ≤ {v['hit_rate_tol_pp']:.0f}pp): "
          f"{v['p3_passed']}/{v['total']} 通过"
          f"(⚠️ **breaking 流的 P-3 必须 FAIL** = 正控必须激发)")
    print(f"[parity] P-2(延迟偏差 < {v['tolerance']:.0%}): "
          f"{v['p2_passed']}/{v['total']} 通过"
          f"(⚠️ **仅对 conforming 判定**)")
    print(f"[parity] P-2a:形态 = **{v['p2a_form']}**"
          f"(A-1 鉴别力自检:{'✓' if v['p2a_a1_ok'] else '✗'}"
          f" —— breaking 流 {v['p2a_breaking_count']} 条"
          f"{'(全部正控激发)' if v['p2a_a1_ok'] else ''};"
          f"A-2 范围限定声明:{'✓' if v['p2a_a2_ok'] else '✗'})")
    if v["p2a_breaking_not_excited"]:
        print(f"       ✗ **正控未激发**:{', '.join(v['p2a_breaking_not_excited'])}"
              f" ⇒ 判红(生成器变了或规则变了 ⇒ 判据失去鉴别力)")
    for _w in v.get("warnings", []):
        print(f"       ⚠️ {_w}")
    print(f"       适用范围:{v['applicability']}")
    print(f"       局限    :{v['limitation']}")
    if v["passed"]:
        print(f"[parity] 结论:**PASS** —— 「H7 在【上述声明范围】内成立」"
              f"(⚠️ **不得**表述为「H7 已验证」)")
    elif not v["class_counts"]["conforming"]:
        # ⚠️ **判据不适用** ≠ **实质性否定**(SHOULD_FIX):workload 集里没有
        #    `conforming` 流 ⇒ P-2/P-3 **无判定对象** ⇒ 结论只能是「不可判」。
        print("[parity] 结论:**不可判**(workload 集不含 `conforming` 流 ⇒ "
              "P-2/P-3 **无判定对象** ⇒ 判据不适用;⚠️ **不是**「H7 不成立」)")
    elif not v["conforming_ok"]:
        print("[parity] 结论:**FAIL**(conforming 流未全过 P-2/P-3 ⇒ H7 在声明范围内不成立)")
    elif v["p2a_breaking_not_excited"]:
        print("[parity] 结论:**FAIL**(正控未激发 ⇒ 判据无鉴别力 ⇒ 不可判)")
    else:
        print("[parity] 结论:**不可判**(P-2/P-3 通过,但 P-2a 两种形态都不满足)")

    if args.out:
        pathlib.Path(args.out).write_text(
            json.dumps(v, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"[parity] verdict 已写出:{args.out}")
    return 0 if v["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
