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


def compare(cpptlm, gem5, tolerance):
    checks = []

    # ── P-1 访问数 ──
    n_cpptlm = cpptlm.get("num_accesses")
    n_gem5 = gem5.get("num_accesses")
    p1_ok = (n_cpptlm is not None and n_gem5 is not None and n_cpptlm == n_gem5)
    checks.append({
        "id": "P-1",
        "name": "访问数两侧一致",
        "ok": p1_ok,
        "detail": f"cpptlm={n_cpptlm} gem5={n_gem5}",
        "attribution": "" if p1_ok else "P-1 不满足 ⇒ 对拍无效(先查访问流/请求数,不是模型问题)",
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

    passed = all(c["ok"] for c in checks)
    return {
        "passed": passed,
        "tolerance": tolerance,
        "checks": checks,
        "cpptlm": cpptlm,
        "gem5": gem5,
        "scope_note": ("辅助档(常数档)⇒ 验证的是**口径与建模语义**;"
                       "**不**验证 H7(本仓 stub cache 的现实性)⇒ H7 仍未验证、R7 不降级"),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gem5", required=True, help="gem5 侧结果 JSON")
    ap.add_argument("--out", default=None, help="verdict JSON 输出路径")
    args = ap.parse_args()

    cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
    tolerance = float(cfg["tolerances"]["P-4_latency_rel"])

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
          f"(辅助档;{verdict['scope_note']})")

    if args.out:
        pathlib.Path(args.out).write_text(
            json.dumps(verdict, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"[parity] verdict 已写出:{args.out}")
    return 0 if verdict["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
