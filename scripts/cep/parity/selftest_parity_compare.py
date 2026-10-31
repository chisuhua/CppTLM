#!/usr/bin/env python3
"""scripts/cep/parity/selftest_parity_compare.py
   —— 对拍比较器**自身**的反向验证(不需要 gem5 ⇒ 可进 ctest)

判据纪律:
   ① 每条判据都有**可执行反向 fixture** ⇒ 比较器逻辑坏掉时必红;
   ② 纯负向断言必带正控 ⇒ 「永远 FAIL」与「永远 PASS」都不可分辨;
   ③ fixture 状态变迁后必须**重新基线化** ⇒ 合成 fixture 的参数集中在下方常量区。

用法:python3 scripts/cep/parity/selftest_parity_compare.py
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import parity_compare  # noqa: E402

TOL = 0.15
PASS = 0
FAIL = 0


def check(ok, name):
    global PASS, FAIL
    if ok:
        PASS += 1
        print(f"  ✓ {name}")
    else:
        FAIL += 1
        print(f"  ✗ {name} —— **反向验证失败**")


def base_cpptlm():
    # ⚠️ 字段名与两侧 JSON 一致 = **`accesses`**(比较器里做了归一)
    return {"hit_ns": 5.0, "miss_ns": 50.0, "accesses": 20000, "mode": "constant_tier",
            # ⚠️ **P-5** 字段(per `ADR-CEP-06 §3`):缺失 ⇒ P-5 FAIL ⇒ 整个 verdict 红
            "verification_mode": "bounded_random",
            "verification_scope": "20000 accesses; deterministic linear sweep"}


def base_gem5():
    return {"hit_latency_ns": 5.0, "miss_latency_ns": 50.0, "accesses": 20000,
            "mode": "constant_tier",
            "verification_mode": "bounded_random",
            "verification_scope": "20000 accesses; deterministic linear sweep"}


def main():
    print("── 正控:两侧一致 ⇒ PASS ──")
    v = parity_compare.compare(base_cpptlm(), base_gem5(), TOL)
    check(v["passed"], "基线通过")
    # ⚠️ 常数档的判据集合(per `14 号` §4.4 的「档位分列」):
    #    P-1 · P-2(N/A,主对拍档)· P-3(N/A,主对拍档)· P-4a · P-4b · P-5
    #    ⇒ 6 条(P-1b / P-1c 只在同源/参数同源档出现)。
    check(len(v["checks"]) == 6,
          f"常数档判据条数 = 6(实测 {len(v['checks'])})")
    _ids = [c["id"] for c in v["checks"]]
    check(_ids == ["P-1", "P-4a", "P-4b", "P-3", "P-2", "P-5"],
          f"常数档判据 id 集合正确(实测 {_ids})")

    print("── 反控 ①:gem5 命中延迟偏 30% ⇒ 必须 FAIL ──")
    g = base_gem5()
    g["hit_latency_ns"] = 6.5
    v = parity_compare.compare(base_cpptlm(), g, TOL)
    check(not v["passed"], "命中偏差超容差 ⇒ FAIL")
    p4a = [c for c in v["checks"] if c["id"] == "P-4a"][0]
    check(p4a["deviation"] is not None and abs(p4a["deviation"] - 0.3) < 1e-9,
          "偏差值算对(0.3)")
    check(bool(p4a["attribution"]), "失败判据必须带归因提示(不得静默红)")

    print("── 反控 ②:未命中延迟偏 20% ⇒ 必须 FAIL ──")
    g = base_gem5()
    g["miss_latency_ns"] = 60.0
    v = parity_compare.compare(base_cpptlm(), g, TOL)
    check(not v["passed"], "未命中偏差超容差 ⇒ FAIL")

    print("── 反控 ③:访问数明显不一致(>1%)⇒ P-1 必须 FAIL ──")
    # ⚠️ 常数档 P-1 的口径是「偏差 ≤ 1%」(gem5 `MemTest` 会多发 1 条预热写)
    #    ⇒ 反控 fixture 必须用**明显**超阈值的差异,否则 fixture 本身不具鉴别力。
    g = base_gem5()
    g["accesses"] = 15000  # 25% 差异 ⇒ 必红
    v = parity_compare.compare(base_cpptlm(), g, TOL)
    p1 = [c for c in v["checks"] if c["id"] == "P-1"][0]
    check(not p1["ok"], "P-1 偏差 > 1% ⇒ FAIL")
    check(bool(p1["attribution"]), "P-1 失败必须带归因提示")

    print("── 反控 ③b:同源 trace 档访问数差 1 条 ⇒ 必须 FAIL(严格相等)──")
    c = base_cpptlm()
    c["mode"] = "same_source_trace"
    g = base_gem5()
    g["mode"] = "same_source_trace"
    g["accesses"] = 19999  # 差 1 条 ⇒ 同源档必须红
    v = parity_compare.compare(c, g, TOL)
    p1 = [c for c in v["checks"] if c["id"] == "P-1"][0]
    check(not p1["ok"], "同源档访问数差 1 条 ⇒ FAIL(严格相等)")

    print("── 反控 ③c:同源 trace 档读写条数不一致 ⇒ P-1b 必须 FAIL ──")
    c = base_cpptlm()
    c["mode"] = "same_source_trace"
    c["num_reads"] = 793
    c["num_writes"] = 207
    g = base_gem5()
    g["mode"] = "same_source_trace"
    g["num_reads"] = 792  # 差 1 条读
    g["num_writes"] = 207
    v = parity_compare.compare(c, g, TOL)
    p1b = [c for c in v["checks"] if c["id"] == "P-1b"][0]
    check(not p1b["ok"], "同源档读写条数不一致 ⇒ P-1b FAIL")
    check(bool(p1b["attribution"]), "P-1b 失败必须带归因提示")

    print("── 反控 ④:测量值缺失 ⇒ 不得**静默 PASS** ──")
    g = base_gem5()
    g["hit_latency_ns"] = None
    v = parity_compare.compare(base_cpptlm(), g, TOL)
    check(not v["passed"], "缺测量值 ⇒ FAIL(不得当作 0 偏差)")

    print("── 反控 ⑤:容差收紧到 0 ⇒ 基线也必须 FAIL(证明判据有鉴别力)──")
    v = parity_compare.compare(base_cpptlm(), base_gem5(), 0.0)
    check(not v["passed"], "容差 = 0 时零偏差仍 FAIL ⇒ 说明 '<' 是严格小于(与口径一致)")

    print("── 反控 ⑥:缺 `verification_mode` ⇒ P-5 必须 FAIL ──")
    c = base_cpptlm()
    del c["verification_mode"]
    v = parity_compare.compare(c, base_gem5(), TOL)
    p5 = [c for c in v["checks"] if c["id"] == "P-5"][0]
    check(not p5["ok"], "缺 verification_mode ⇒ P-5 FAIL")
    check(bool(p5["attribution"]), "P-5 失败必须带归因提示")
    check(not v["passed"], "缺 verification_mode ⇒ 整个 verdict 必须 FAIL")

    print("── 反控 ⑦:`verification_mode` 不在枚举 ⇒ P-5 必须 FAIL ──")
    c = base_cpptlm()
    c["verification_mode"] = "whatever"
    v = parity_compare.compare(c, base_gem5(), TOL)
    p5 = [c for c in v["checks"] if c["id"] == "P-5"][0]
    check(not p5["ok"], "verification_mode 越界 ⇒ P-5 FAIL")

    print("── 反控 ⑧:两侧 `verification_scope` 不一致 ⇒ P-5 必须 FAIL ──")
    c = base_cpptlm()
    g = base_gem5()
    g["verification_scope"] = "different scope"
    v = parity_compare.compare(c, g, TOL)
    p5 = [c for c in v["checks"] if c["id"] == "P-5"][0]
    check(not p5["ok"], "scope 不一致 ⇒ P-5 FAIL")

    print("── 反控 ⑨:主对拍档的 P-2a(A-2 形态)必须携带范围声明 ──")
    # ⚠️ 「范围限定声明」若是**口头免责** ⇒ 就没有鉴别力 ⇒ 必须是**可判字段**
    #    (`applicability` + `limitation` 齐备 ⇒ 缺一即 FAIL)。
    import parity_main_tier as pmt
    streams = [
        {"name": "S1", "hit_rate": 0.5, "avg_latency_ns": 26.0,
         "hits": 50000, "misses": 50000, "accesses": 100000,
         "nominal_accesses": 100000,
         # ⚠️ **mode 必须声明**(per `14 号` §4.4「条数契约按 driver 分档」)
         #    ⇒ 未知 mode ⇒ 分析器**拒判**(不得静默放行)
         "mode": "same_source_trace",
         "profile": "uniform",
         "working_set_bytes": 32768, "cache_bytes": 16384},
    ]
    v = pmt.analyse(streams)
    check(v["p2a_form"] == "A-2",
          f"A-1 不满足时形态应为 A-2(实测 {v['p2a_form']})")
    check(bool(v.get("applicability")), "A-2 必须携带 `applicability`(适用范围)")
    check(bool(v.get("limitation")), "A-2 必须携带 `limitation`(局限 + 解除条件)")
    check(v["passed"], "A-2 形态下 P-2/P-3 达标 ⇒ 判定通过")

    # ── **反控 ⑩ ~ ⑬**:按 class 分列的判据(per `14 号` §4.4 封闭枚举表)────
    # ⚠️ 这 4 条是 **D-6**(verdict 聚合自相矛盾)的**反向 fixture**:
    #    旧实现里「breaking 流」由 P-3 失败反推、而 `passed` 又要求全部通过
    #    ⇒ 合取 ⇒ A-1 一旦满足,`passed` 恒 False ⇒ 这些场景**无法被正确判定**。
    import parity_main_tier as pmt

    def _stream(name, profile, hit_rate, lat, ws, cache=16384,
                mode="same_source_trace", nominal=100000, actual=100000):
        return {"name": name, "profile": profile, "hit_rate": hit_rate,
                "avg_latency_ns": lat, "hits": int(hit_rate * 100000),
                "misses": int((1 - hit_rate) * 100000), "accesses": actual,
                "nominal_accesses": nominal,
                "mode": mode,
                "working_set_bytes": ws, "cache_bytes": cache}

    # `hot_cold` 的有效足迹 = hot + cold = 1052672 ⇒ R-B 的 h_0 = 16384/1052672
    _BREAK_WS = 4096 + 1048576
    _CONF_OK = _stream("C1", "uniform", 0.5, 26.0, 32768)
    _BREAK_OK = _stream("B1", "hot_cold", 0.17, 40.6, _BREAK_WS)

    print("── 反控 ⑩:conforming 流 P-2 挂 ⇒ 必须 FAIL ──")
    _c = _stream("C1", "uniform", 0.5, 10.0, 32768)   # l_model=27.5 ⇒ 偏差 175%
    v = pmt.analyse([_c, _BREAK_OK])
    check(not v["passed"], "conforming 流 P-2 挂 ⇒ FAIL")
    check(not v["conforming_ok"], "conforming_ok 必须为 False")

    print("── 反控 ⑪:零 breaking 流 ⇒ 形态必须是 A-2 ──")
    v = pmt.analyse([_CONF_OK])
    check(v["p2a_form"] == "A-2",
          f"零 breaking 流 ⇒ A-2(实测 {v['p2a_form']})")
    check(v["p2a_breaking_count"] == 0, "breaking 计数必须为 0")
    check(v["passed"], "A-2 形态下 conforming 达标 ⇒ 判定通过")

    print("── 反控 ⑫:breaking 流**意外通过** P-3 ⇒ 必须 FAIL(正控未激发)──")
    _b_excited = _stream("B1", "hot_cold", 1.0, 5.0, 16384)  # h_0=1.0 ⇒ 误差 0
    v = pmt.analyse([_CONF_OK, _b_excited])
    check(not v["passed"], "正控未激发 ⇒ FAIL")
    check(bool(v["p2a_breaking_not_excited"]),
          "必须列出未激发的 breaking 流(不得静默放行)")
    check(v["p2a_form"] != "A-1", "正控未激发 ⇒ 不得判为 A-1")

    print("── 反控 ⑬:conforming 流 P-3 挂 ⇒ FAIL 且**不得**被计入 breaking ──")
    _c_p3 = _stream("C1", "uniform", 0.5, 26.0, 131072)  # h_0=0.125 ⇒ 误差 37.5pp
    v = pmt.analyse([_c_p3, _BREAK_OK])
    check(not v["passed"], "conforming 流 P-3 挂 ⇒ FAIL")
    check(v["p2a_breaking_streams"] == ["B1"],
          f"conforming 的失败**不得**被计入 breaking(实测 {v['p2a_breaking_streams']})")
    check(v["p2a_a1_ok"], "A-1 仍成立(breaking 正控已激发)⇒ 失败原因是 conforming")

    print("── 反控 ⑭:A-1 形态下 conforming 全过 + breaking 正控激发 ⇒ PASS ──")
    v = pmt.analyse([_CONF_OK, _BREAK_OK])
    check(v["p2a_form"] == "A-1",
          f"含 breaking 且正控激发 ⇒ A-1(实测 {v['p2a_form']})")
    check(v["conforming_ok"], "conforming 全过 ⇒ conforming_ok")
    check(v["passed"], "⚠️ **D-6 的正解**:A-1 形态下 verdict 必须能 PASS"
                       "(旧实现恒 False)")

    print("── 反控 ⑮:random-driver 路径 nominal≠actual ⇒ **不得**炸 ──")
    # ⚠️ 该路径的条数由 gem5 `duration` 决定 ⇒ 实测可达名义的 17~20 倍
    #    ⇒ **超发是设计行为,不是截断** ⇒ 分析器的前置断言**必须按 driver 分档**
    #    (per `14 号` §4.4「条数契约按 driver 分档」)。
    _rnd = dict(_CONF_OK)
    _rnd.update({"name": "R1", "mode": "parameter_same_source",
                 "nominal_accesses": 100000, "accesses": 1999982})
    try:
        v = pmt.analyse([_rnd])
        check(True, "random-driver 路径 nominal≠actual ⇒ 不炸(断言已分档)")
    except SystemExit as e:
        check(False, f"random-driver 路径被条数断言误杀:{e}")

    print("── 反控 ⑯:unknown mode ⇒ 必须拒判(不得静默放行)──")
    _unk = dict(_CONF_OK)
    _unk.update({"name": "U1", "mode": "whatever"})
    try:
        pmt.analyse([_unk])
        check(False, "unknown mode ⇒ 必须 SystemExit")
    except SystemExit:
        check(True, "unknown mode ⇒ SystemExit(拒判)")

    print("── 反控 ⑰:breaking 流违反**声明的期望误差下界** ⇒ 必须判红 ──")
    # ⚠️ BLOCKER-2:floor 校验若在 `p2a_a1_ok` 冻结**之后** ⇒ 只被记录、不影响判定
    #    ⇒ 同一份 verdict 同时宣称 PASS 与判红 ⇒ 本反控守住「冻结前校验」。
    _b_floor = _stream("B1", "hot_cold", 0.7389, 16.226, _BREAK_WS)
    _b_floor["expected_error_floor_pp"] = 80.0   # 高于实测 72.34pp
    v = pmt.analyse([_CONF_OK, _b_floor])
    check(not v["passed"], "违反声明的误差下界 ⇒ passed 必须为 False")
    check(v["p2a_form"] != "A-1",
          f"违反下界 ⇒ 不得判为 A-1(实测 {v['p2a_form']})")
    check(bool(v["p2a_breaking_not_excited"]),
          "必须列出违反下界的 breaking 流(不得静默)")

    print(f"\n反向验证:{PASS} 项通过 / {FAIL} 项失败")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
