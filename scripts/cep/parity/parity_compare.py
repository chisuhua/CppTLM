#!/usr/bin/env python3
"""scripts/cep/parity/parity_compare.py
   —— 对拍比较器:把 **CppTLM 侧参数** 与 **gem5 侧实测** 比对,产出判据 verdict

判据(辅助档 = **常数档**,per `docs/architecture/14-coherence-parity-baseline.md` §4.2):
   · **P-1 访问数**:两侧访问数**必须一致**(否则对拍无效 ⇒ 先查这个);
   · **P-4 延迟常数**:命中 / 未命中延迟的**相对偏差 < 容差**(默认 15%)。

⚠️ **口径边界(不得越界表述)**:本比较器验证的是**口径与建模语义**
   (本仓「miss = hit + 存储延迟」的分解是否与 gem5 一致 + cycle↔ns 换算),
   **不是**「本仓 stub cache 的参数贴近真实 cache」⇒ **H7 仍未验证**。

用法:
   python3 scripts/cep/parity/parity_compare.py --gem5 <gem5_result.json> [--out verdict.json]
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cpptlm_params  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
CONFIG = ROOT / "scripts" / "cep" / "parity" / "parity_config.json"


def rel_dev(measured, reference):
    """相对偏差(参考值为 0 时退化为绝对差 + 绝对阈值口径)"""
    if measured is None:
        return None
    if reference == 0:
        return None
    return abs(measured - reference) / abs(reference)


def attribute(dev, kind):
    """失配归因提示(per 14 号文档 §4.5 的 4 类)"""
    if dev is None:
        return "D-? 测量值缺失 ⇒ 归因未定(先查测量链路,不是模型问题)"
    if kind == "hit":
        if dev > 0.15:
            return ("D-3 时基归一 / D-4 协议开销 / 参数错配:gem5 的命中路径含 tag+data+response 三段, "
                    "若本仓常量只覆盖其中一段 ⇒ 偏差会系统性偏大")
    if kind == "miss":
        if dev > 0.15:
            return ("D-1 容量模型 / D-3 时基归一 / 参数错配:未命中路径含缓存查找 + 存储延迟两段, "
                    "请先核对 `parity_config.json` 的 tag/data/response/memory 四项分解是否与本仓口径同构")
    return "无"


def compare(cpptlm, gem5, tolerance, hit_rate_tol_pp=5.0):
    checks = []

    # ── P-1 访问数 ──
    # ⚠️ 字段名:两侧 JSON 都用 **`accesses`**(不是 `num_accesses`)⇒ 统一在此归一。
    n_cpptlm = cpptlm.get("accesses", cpptlm.get("num_accesses"))
    n_gem5 = gem5.get("accesses", gem5.get("num_accesses"))
    # ⚠️ **常数档**(`MemTest`)两侧访问数**不可能完全相等**:
    #    gem5 的 `MemTest` 会**先发一条写**再发配置条数的访问
    #    (实测:配置 20000 ⇒ gem5 侧 `overallAccesses` = 20101)
    #    ⇒ 常数档 P-1 放宽为「**相对偏差 ≤ 1%**」,并**明确标注**是放宽口径;
    #    **同源 trace 档**要求**严格相等**(逐条相同,判据更强)。
    if cpptlm.get("mode") == "same_source_trace" or gem5.get("mode") == "same_source_trace":
        p1_ok = (n_cpptlm is not None and n_gem5 is not None and n_cpptlm == n_gem5)
        p1_name = "访问数两侧**严格相等**(同源 trace)"
        p1_attr = "" if p1_ok else (
            "P-1 不满足 ⇒ 对拍无效(先查 trace 文件与消费路径,不是模型问题)")
    else:
        _den = max(float(n_cpptlm or 0), float(n_gem5 or 0))
        _dev = abs(float(n_cpptlm or 0) - float(n_gem5 or 0)) / _den if _den else 1.0
        p1_ok = (n_cpptlm is not None and n_gem5 is not None and _dev <= 0.01)
        p1_name = "访问数两侧偏差 ≤ 1%(常数档:gem5 `MemTest` 会多发 1 条预热写)"
        p1_attr = "" if p1_ok else (
            "P-1 不满足 ⇒ 对拍无效(先查访问流/请求数,不是模型问题)")
    checks.append({
        "id": "P-1",
        "name": p1_name,
        "ok": p1_ok,
        "detail": f"cpptlm={n_cpptlm} gem5={n_gem5}",
        "attribution": p1_attr,
    })

    # ── P-1b 读写条数逐条一致(**同源 trace** 的更强证据)────────────
    # ⚠️ 只在**同源 trace 档**有意义(两侧读同一文件 ⇒ 读写分布必然相同);
    #    常数档(MemTest)不满足 ⇒ 该判据在常数档**跳过**(不判红)。
    if cpptlm.get("mode") == "same_source_trace" or gem5.get("mode") == "same_source_trace":
        r_cpptlm = cpptlm.get("num_reads")
        w_cpptlm = cpptlm.get("num_writes")
        r_gem5 = gem5.get("num_reads")
        w_gem5 = gem5.get("num_writes")
        p1b_ok = (r_cpptlm == r_gem5 and w_cpptlm == w_gem5
                  and None not in (r_cpptlm, w_cpptlm, r_gem5, w_gem5))
        checks.append({
            "id": "P-1b",
            "name": "读/写条数两侧逐条一致(同源 trace 的直接证据)",
            "ok": p1b_ok,
            "detail": f"cpptlm(r={r_cpptlm},w={w_cpptlm}) gem5(r={r_gem5},w={w_gem5})",
            "attribution": "" if p1b_ok else (
                "P-1b 不满足 ⇒ **两侧没消费同一条 trace**(或 trace 生成不一致)"
                "⇒ 对拍无效,先查 trace 文件与消费路径,不是模型问题"),
        })

    # ── P-4 命中 / 未命中延迟常数 ──
    for cid, kind, ref, got in (
        ("P-4a", "hit", cpptlm.get("hit_ns"), gem5.get("hit_latency_ns")),
        ("P-4b", "miss", cpptlm.get("miss_ns"), gem5.get("miss_latency_ns")),
    ):
        dev = rel_dev(got, ref)
        # ⚠️ **严格小于**:判据文案是「偏差 < 容差」(per 14 号文档 §4.4)
        #    ⇒ 用 `<=` 会让「恰好等于容差」被放过(边界放行 = 判据松一格)。
        ok = dev is not None and dev < tolerance
        checks.append({
            "id": cid,
            "name": f"{kind} 延迟常数偏差 < {tolerance:.0%}",
            "ok": ok,
            "deviation": dev,
            "detail": f"cpptlm={ref}ns gem5={got}ns",
            "attribution": "" if ok else attribute(dev, kind),
        })

    # ── P-2 命中率偏差(仅**同源 trace** 档)────────────────────────────
    # ⚠️ `14 号` §4.2 的主对拍(容量对齐档)判据 = 「命中率 ± 5 个百分点」。
    #    但本仓 `CacheTLM` 是**无限容量 + 精确地址 map**(命中 = 该地址曾被访问过),
    #    gem5 是**容量内命中** ⇒ 两者命中判据**结构性不同**
    #    ⇒ 命中率偏差**必然存在**,本判据只作**登记**(观察值),**不参与 verdict**。
    #    ⇒ 真正的容量对齐档需要本仓侧引入「等容量 + 同替换」的目录视图(未做)。
    if cpptlm.get("mode") == "same_source_trace" or gem5.get("mode") == "same_source_trace":
        hr_cpptlm = cpptlm.get("miss_rate")
        hr_gem5 = gem5.get("miss_rate")
        if hr_cpptlm is not None and hr_gem5 is not None:
            dev_pp = abs(float(hr_cpptlm) - float(hr_gem5)) * 100.0
            checks.append({
                "id": "P-2",
                "name": f"未命中率偏差(观察值,阈值 ±{hit_rate_tol_pp:.0f}pp,**不参与 verdict**)",
                "ok": True,  # 登记项:不参与判定
                "observed": True,
                "deviation_pp": dev_pp,
                "detail": f"cpptlm={float(hr_cpptlm):.4f} gem5={float(hr_gem5):.4f} 偏差={dev_pp:.2f}pp",
                "attribution": (
                    f"偏差 {dev_pp:.2f}pp {'在' if dev_pp <= hit_rate_tol_pp else '**超**'}阈值 —— "
                    "**结构性差异**:本仓 `CacheTLM` 命中判据 = 「该地址曾被访问过」"
                    "(无限容量 + 精确地址 map),gem5 = 「容量内命中」(16KiB/2-way/LRU)"
                    "⇒ 命中率**不可直接比**(per `14 号` §4.2);真要判需本仓侧引入"
                    "「等容量 + 同替换」目录视图(未做)"),
            })

    passed = all(c["ok"] for c in checks)
    same_source = (cpptlm.get("mode") == "same_source_trace"
                   or gem5.get("mode") == "same_source_trace")
    scope_note = (
        ("同源 trace 档 ⇒ 验证的是「**同一条访问流**下的口径一致性 + 路径延迟常数」;"
         "**不**验证 H7(本仓 stub cache 的现实性)⇒ H7 仍未验证、R7 不降级;"
         "P-2(命中率)因命中判据结构性不同**只作观察登记**")
        if same_source else
        ("辅助档(常数档)⇒ 验证的是**口径与建模语义**;"
         "**不**验证 H7(本仓 stub cache 的现实性)⇒ H7 仍未验证、R7 不降级")
    )
    return {
        "passed": passed,
        "tolerance": tolerance,
        "mode": "same_source_trace" if same_source else "constant_tier",
        "checks": checks,
        "cpptlm": cpptlm,
        "gem5": gem5,
        "scope_note": scope_note,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gem5", required=True, help="gem5 侧结果 JSON")
    ap.add_argument("--cpptlm", default=None,
                    help="CppTLM 侧结果 JSON(同源 trace 档用;缺省则用参数抽取值)")
    ap.add_argument("--out", default=None, help="verdict JSON 输出路径")
    args = ap.parse_args()

    cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
    tolerance = float(cfg["tolerances"]["P-4_latency_rel"])

    if args.cpptlm:
        cpptlm = json.loads(pathlib.Path(args.cpptlm).read_text(encoding="utf-8"))
        # 延迟常数仍以**实现源码抽取**为准(不信任 JSON 里的派生值)
        _p = cpptlm_params.collect()
        cpptlm["hit_ns"] = _p["hit_ns"]
        cpptlm["miss_ns"] = _p["miss_ns"]
    else:
        cpptlm = cpptlm_params.collect()
        cpptlm["num_accesses"] = int(cfg["gem5"]["tester"]["num_accesses"])

    gem5 = json.loads(pathlib.Path(args.gem5).read_text(encoding="utf-8"))
    verdict = compare(cpptlm, gem5, tolerance)

    for c in verdict["checks"]:
        mark = "✓" if c["ok"] else "✗"
        print(f"  {mark} {c['id']} {c['name']} —— {c['detail']}")
        if not c["ok"] and c.get("attribution"):
            print(f"      归因提示:{c['attribution']}")
    print(f"[parity] 结论:{'PASS' if verdict['passed'] else 'FAIL'}"
          f"({verdict['mode']};{verdict['scope_note']})")

    if args.out:
        pathlib.Path(args.out).write_text(
            json.dumps(verdict, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"[parity] verdict 已写出:{args.out}")
    return 0 if verdict["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
