#!/usr/bin/env python3
"""cep_invariant_map_check.py —— B2「66 条 invariant 逐条有牙载体」的映射表生成 + 闸校验

per `ADR-CEP-06` 之外的对象:`objective §6.2 B2` + 台账 **Entry #232 / #233**
(`C1 裁决` = 规则 A 逐条 / 规则 B 有牙 / 规则 C 机器可查 + 附裁的 ⏭ 登记形态)

用法:
    python3 scripts/test/cep_invariant_map_check.py --generate   # 由 INV-CARRIER 标记生成映射表
    python3 scripts/test/cep_invariant_map_check.py --check      # 闸校验(退出码 0 = 绿)

映射表(`test/cep_invariant_assertion_map.json`)是**生成物**(由 `// INV-CARRIER: <id>` 刚性标记派生),
⛔ 不是手写口径源。⏭ 登记物(`test/cep_invariant_deferrals.json`)是**手写**的、但每条须有
台账锚(`ledger_entry`)且须被校验 ④ 解析到真实台账 Entry。

四项闸校验:
    ① ⏭ id ⊆ 66 集合
    ② reason / trigger_condition / ledger_entry 非空(反向 fixture: 置空 ⇒ 红)
    ③ 有牙载体 id ∪ 授权⏭ id == 66 集合,且交集为空
    ④ ledger_entry 须解析到**真实存在的台账 Entry**,且该 Entry 正文含该 id
    ⑤ 每个载体标记的**邻域**(±8 行)须含断言宏(C/C++)或非零退出式(shell)⇒ 「恒真占位」判红
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
MAP_PATH = ROOT / "test" / "cep_invariant_assertion_map.json"
DEFER_PATH = ROOT / "test" / "cep_invariant_deferrals.json"
LEDGER_PATH = ROOT / ".rddf" / "roadmap" / "objectives" / "obj-cep-platform.md"

# 66 个实际编号(67 编号位 − INV-CEP-04-IC4 永久跳空)—— 取自 canonical_invariants 的 expected_groups()
INV_RE = re.compile(r'"((?:INV|CEN)-[A-Za-z0-9\-]+)"')
CARRIER_RE = re.compile(r"INV-CARRIER:\s*((?:INV|CEN)-[A-Za-z0-9\-]+)")
ASSERT_MACROS = ("REQUIRE", "CHECK", "static_assert", "FAIL(", "SUCCEED(", "assert")
SHELL_NONZERO = re.compile(r"exit\s+[1-9]|return\s+[1-9]")
NEIGHBORHOOD = 8


def canonical_ids():
    src = (ROOT / "test" / "test_cep_canonical_invariants.cc").read_text(encoding="utf-8")
    ids = set()
    for m in INV_RE.finditer(src):
        ids.add(m.group(1))
    ids.discard("INV-CEP-04-IC4")  # 永久跳空
    return ids


def scan_carriers():
    """扫描 test/ 与 scripts/test/ 下的 INV-CARRIER 标记 ⇒ {id: [(file, line, kind)]}"""
    found = {}
    for base in ("test", "scripts/test"):
        for f in sorted((ROOT / base).rglob("*")):
            if not f.is_file() or f.suffix not in (".cc", ".hh", ".sh", ".py"):
                continue
            try:
                lines = f.read_text(encoding="utf-8", errors="ignore").splitlines()
            except OSError:
                continue
            for i, ln in enumerate(lines):
                for m in CARRIER_RE.finditer(ln):
                    found.setdefault(m.group(1), []).append(
                        (str(f.relative_to(ROOT)), i + 1, f.suffix))
    return found


def neighbor_ok(path, line, suffix):
    lines = (ROOT / path).read_text(encoding="utf-8", errors="ignore").splitlines()
    lo = max(0, line - 1 - NEIGHBORHOOD)
    hi = min(len(lines), line + NEIGHBORHOOD)
    window = "\n".join(lines[lo:hi])
    if suffix == ".py":
        return bool(re.search(r"sys\.exit\(|raise\s+\w+Error", window))
    if suffix == ".sh":
        return bool(SHELL_NONZERO.search(window)) or "exit " in window
    return any(m in window for m in ASSERT_MACROS)


def cmd_generate():
    carriers = scan_carriers()
    out = {
        "_generated_by": "scripts/test/cep_invariant_map_check.py --generate",
        "_note": "生成物 —— 勿手改;载体由 // INV-CARRIER: <id> 刚性标记派生(per 规则 C)",
        "carriers": {
            k: [{"file": f, "line": ln, "kind": kd} for (f, ln, kd) in v]
            for k, v in sorted(carriers.items())
        },
    }
    MAP_PATH.write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[cep-b4-map] 已生成映射表: {MAP_PATH.relative_to(ROOT)} "
          f"({len(carriers)} 个 id 有载体标记)")
    return 0


def cmd_check():
    problems = []
    ids66 = canonical_ids()

    # ── 映射表存在且是当前标记的派生物(防止手改)───────────────────────
    if not MAP_PATH.exists():
        problems.append(f"缺映射表 {MAP_PATH.relative_to(ROOT)}(请先 --generate)")
    carriers = scan_carriers()
    if MAP_PATH.exists():
        mp = json.loads(MAP_PATH.read_text(encoding="utf-8"))
        declared = set(mp.get("carriers", {}))
        actual = set(carriers)
        if declared != actual:
            only_m = sorted(declared - actual)
            only_a = sorted(actual - declared)
            problems.append(f"映射表与标记不一致(映射表已手改或标记已增删): "
                            f"仅映射表有={only_m} 仅标记有={only_a}")

    # ── ⑤ 邻域有牙校验 ────────────────────────────────────────────────
    for cid, entries in sorted(carriers.items()):
        for (f, ln, suf) in entries:
            if not neighbor_ok(f, ln, suf):
                problems.append(f"{cid} 的载体标记 {f}:{ln} 邻域(±{NEIGHBORHOOD} 行)"
                                f"未见断言宏 / 非零退出式 ⇒ 疑「恒真占位」")

    # ── deferrals ─────────────────────────────────────────────────────
    deferrals = {}
    if not DEFER_PATH.exists():
        problems.append(f"缺 ⏭ 登记物 {DEFER_PATH.relative_to(ROOT)}")
    else:
        deferrals = json.loads(DEFER_PATH.read_text(encoding="utf-8"))
        entries = deferrals.get("deferrals", [])
        ledger_text = LEDGER_PATH.read_text(encoding="utf-8") if LEDGER_PATH.exists() else ""
        for d in entries:
            did = d.get("id", "")
            disp = str(d.get("disposition", ""))
            if disp not in ("⏭-unmodelled", "⏭-pending"):
                problems.append(f"⏭ 条目 {did} 的 disposition 非法: '{disp}'"
                                f"(须为 ⏭-unmodelled / ⏭-pending)")
            # ① ⊆ 66
            if did not in ids66:
                problems.append(f"⏭ 条目 {did} 不在 66 集合内")
            # ② 三字段非空
            for k in ("reason", "trigger_condition", "ledger_entry"):
                if not str(d.get(k, "")).strip():
                    problems.append(f"⏭ 条目 {did} 的 {k} 为空")
            # ④ 台账锚可解析且正文含该 id
            le = str(d.get("ledger_entry", "")).strip()
            if le:
                if le not in ledger_text:
                    problems.append(f"⏭ 条目 {did} 的 ledger_entry '{le}' 在台账中不存在")
                elif did not in ledger_text:
                    problems.append(f"⏭ 条目 {did} 的 ledger_entry '{le}' 的台账正文未含该 id")
            # ③ 与有牙载体不得重叠
            if did in carriers:
                problems.append(f"⏭ 条目 {did} 同时有 INV-CARRIER 载体(交集必须为空)")

    # ── ③ 集合并集 == 66 ─────────────────────────────────────────────
    union = set(carriers) | set(deferrals.get("deferrals", []) and
                                [d.get("id", "") for d in deferrals.get("deferrals", [])])
    union = {u for u in union if u}
    missing = sorted(ids66 - union)
    extra = sorted(union - ids66)
    if missing:
        problems.append(f"以下 id 既无载体也无 ⏭ 登记(共 {len(missing)}): {missing}")
    if extra:
        problems.append(f"以下 id 不在 66 集合内(共 {len(extra)}): {extra}")

    n_carrier = len(carriers)
    entries = deferrals.get("deferrals", [])
    n_unmod = sum(1 for d in entries if d.get("disposition") == "⏭-unmodelled")
    n_pending = sum(1 for d in entries if d.get("disposition") == "⏭-pending")

    if problems:
        for p in problems:
            print(f"  ✗ {p}", file=sys.stderr)
        return 1
    print(f"  ✓ 映射表闸通过: 66/66 = {n_carrier} 项有牙载体 + {n_unmod} 项 ⏭-unmodelled "
          f"+ {n_pending} 项 ⏭-pending")
    if n_pending:
        print(f"  ⚠️ ⏭-pending = {n_pending} 项 ⇒ **B2 不得宣称达成**"
              f"(达成宣称要求 pending == 0;per 裁决 §5 登记格式)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--generate", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--for-achievement", action="store_true",
                    help="达成宣称模式:⏭-pending 必须为 0,否则红")
    args = ap.parse_args()
    if args.generate:
        return cmd_generate()
    rc = cmd_check()
    if rc == 0 and args.for_achievement:
        doc = json.loads(DEFER_PATH.read_text(encoding="utf-8"))
        pend = [d.get("id", "") for d in doc.get("deferrals", [])
                if d.get("disposition") == "⏭-pending"]
        if pend:
            print(f"  ✗ 达成宣称被拒:仍有 {len(pend)} 项 ⏭-pending(载体未逐条补齐)⇒ "
                  f"B2 **不得**登记 ✅;例:{pend[:5]}", file=sys.stderr)
            return 1
        print("  ✓ 达成宣称模式通过:⏭-pending == 0")
    return rc


if __name__ == "__main__":
    sys.exit(main())
