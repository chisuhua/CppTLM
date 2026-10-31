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
    return {"hit_ns": 5.0, "miss_ns": 50.0, "accesses": 20000, "mode": "constant_tier"}


def base_gem5():
    return {"hit_latency_ns": 5.0, "miss_latency_ns": 50.0, "accesses": 20000,
            "mode": "constant_tier"}


def main():
    print("── 正控:两侧一致 ⇒ PASS ──")
    v = parity_compare.compare(base_cpptlm(), base_gem5(), TOL)
    check(v["passed"], "基线通过")
    check(len(v["checks"]) == 3, "常数档判据条数 = 3(P-1 / P-4a / P-4b)")

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

    print(f"\n反向验证:{PASS} 项通过 / {FAIL} 项失败")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
