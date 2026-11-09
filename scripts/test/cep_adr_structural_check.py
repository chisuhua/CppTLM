#!/usr/bin/env python3
"""scripts/test/cep_adr_structural_check.py
   —— ADR **结构闸**(per oracle 2026-11-09 裁决 C-1 / 台账 Entry #307)

用途:把「ADR 正文结构缺口」从**暗**变**可见**,并**锁住**例外登记不被滥用。

判定口径(per `compliance_rule`):
  · 分母 = `decision_adrs` 列出的 **19 份决策 ADR**(22 份 `ADR-CEP-*` − NUMBER-MAP / 00 宪章 / 00A 锚点表)
  · 合规 = **顶层 `##` 章**含 `Verif|验证|验收判据` **且**位于首个 `## Status Update` **之前**
  · 不合规者必须落在两类例外之一:
      - `waivers`        —— **永久豁免**(已签发 + ADR 不可变 ⇒ 正文永远补不了)⇒ 带 `evidence_anchor` 行号锚点,
                           闸**校验锚点仍存活** ⇒ 豁免不得成为后门
      - `pending_signing`—— **待补**(起草中 ⇒ 正文尚可补章)⇒ 不是豁免,是待办登记

闸的 4 条「有牙」判定(任一不满足 ⇒ 非零退出):
  ① **分区完整性**:磁盘上的 `ADR-CEP-*.md` 必须 == `decision_adrs` ∪ `excluded`
     ⇒ 新增 ADR 文件若未显式归类 ⇒ 红(这是「暗变可见」的核心)
  ② **未登记即红**:不合规且既不在 `waivers` 也不在 `pending_signing` ⇒ 红
  ③ **豁免锚点存活**:`waivers[].evidence_anchor` 的每一行仍存在且仍含 Verification 标题 ⇒ 否则红
  ④ **登记表自身健康**:条目指向的文件存在 · `ledger_entry` 非空 · 名单无重复

另报(**不判红**,per oracle C-3:生命周期状态真源 = ADR 文件自身):
  · `status_drift`          —— NUMBER-MAP 与 ADR header 的**可判定**矛盾
  · `map_status_ambiguity`  —— NUMBER-MAP 状态列 ✅ **三义** ⇒ 不可判读 ⇒ 降级为镜像

用法:
  python3 scripts/test/cep_adr_structural_check.py            # 校验(退出码 0 = 绿)
  python3 scripts/test/cep_adr_structural_check.py --list     # 只打印清单
"""
import argparse
import fnmatch
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
REGISTRY = os.path.join(ROOT, "test", "cep_adr_structural_deferrals.json")
REGISTRY_REL = os.path.join("test", "cep_adr_structural_deferrals.json")

CHAPTER_RE = re.compile(r"^##\s+.*(Verif|验证|验收判据)")
NESTED_RE = re.compile(r"^#{3,4}\s+.*(Verif|验证|验收判据)")
SU_RE = re.compile(r"^##\s*(Status Update|状态更新)", re.M | re.I)


def fail(msg):
    print(f"  ❌ {msg}")
    return 1


def ok(msg):
    print(f"  ✅ {msg}")
    return 0


def info(msg):
    print(f"  · {msg}")
    return 0


def load_registry(path=None):
    with open(path or REGISTRY, encoding="utf-8") as fh:
        return json.load(fh)


def adr_paths(reg):
    d = os.path.join(ROOT, reg["adr_dir"])
    return sorted(
        os.path.join(d, n) for n in os.listdir(d)
        if fnmatch.fnmatch(n, reg["adr_glob"])
    )


def body_before_su(text):
    m = SU_RE.search(text)
    return text[: m.start()] if m else text


def has_compliant_chapter(text):
    for line in body_before_su(text).split("\n"):
        if CHAPTER_RE.match(line):
            return True
    return False


def nested_anchor_lines(text):
    return [
        i for i, ln in enumerate(text.split("\n"), 1) if NESTED_RE.match(ln)
    ]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="只打印清单")
    ap.add_argument(
        "--root",
        default=None,
        help="仓根覆盖(供决定性变异在**影子树**上运行,⛔ 不动真实仓)",
    )
    ap.add_argument("--registry", default=None, help="登记表路径(默认 <root>/test/cep_adr_structural_deferrals.json)")
    args = ap.parse_args()

    global ROOT
    if args.root:
        ROOT = os.path.abspath(args.root)

    reg = load_registry(args.registry or os.path.join(ROOT, REGISTRY_REL))
    decision = list(reg.get("decision_adrs", []))
    excluded = {e["file"]: e for e in reg.get("excluded", [])}
    waivers = {w["adr"]: w for w in reg.get("waivers", [])}
    pending = {p["adr"]: p for p in reg.get("pending_signing", [])}

    disk = {os.path.basename(p): p for p in adr_paths(reg)}
    errors = 0
    print("══ ADR 结构闸(per oracle C-1 / Entry #307)══")

    # ── ① 分区完整性 ───────────────────────────────────────────────
    print("\n[1/4] 分区完整性(磁盘 == decision_adrs ∪ excluded)")
    listed = set(decision) | set(excluded)
    unclassified = sorted(set(disk) - listed)
    if unclassified:
        errors += fail(
            f"未归类的 ADR 文件 {len(unclassified)} 份(必须显式进 `decision_adrs` 或 `excluded`): "
            + ", ".join(unclassified)
        )
    else:
        ok(f"磁盘 {len(disk)} 份 ADR 全部已归类(决策 {len(decision)} + 排除 {len(excluded)})")

    missing = sorted(listed - set(disk))
    if missing:
        errors += fail("登记表指向的文件不存在:" + ", ".join(missing))
    else:
        ok("登记表指向的文件全部存在")

    dup = [f for f in decision if decision.count(f) > 1]
    if dup:
        errors += fail("decision_adrs 有重复条目:" + ", ".join(sorted(set(dup))))
    else:
        ok("decision_adrs 无重复条目")

    # ── ② 合规判定 + 未登记即红 ─────────────────────────────────────
    print("\n[2/4] 合规判定(顶层 Verification 章 · 首个 Status Update 之前)")
    compliant, noncompliant = [], []
    for name in decision:
        path = os.path.join(ROOT, reg["adr_dir"], name)
        if not os.path.isfile(path):
            continue
        text = open(path, encoding="utf-8").read()
        (compliant if has_compliant_chapter(text) else noncompliant).append(name)

    info(f"合规 {len(compliant)} 份:" + ", ".join(compliant))
    info(f"不合规 {len(noncompliant)} 份")
    unregistered = [n for n in noncompliant if n not in waivers and n not in pending]
    if unregistered:
        errors += fail(
            "不合规且未登记例外(必须进 `waivers` 或 `pending_signing`):" + ", ".join(unregistered)
        )
    else:
        ok(f"全部不合规 ADR 均已登记(豁免 {len(waivers)} + 待补 {len(pending)})")

    stale_waiver = [n for n in waivers if n in compliant]
    if stale_waiver:
        errors += fail("豁免条目对应的 ADR 已合规 ⇒ 豁免过期(必须移除):" + ", ".join(stale_waiver))
    stale_pending = [n for n in pending if n in compliant]
    if stale_pending:
        errors += fail("待补条目的 ADR 已合规 ⇒ 登记过期(必须移除):" + ", ".join(stale_pending))
    if not stale_waiver and not stale_pending:
        ok("无过期例外条目")

    # ── ③ 豁免锚点存活(反后门) ─────────────────────────────────────
    print("\n[3/4] 豁免锚点存活(永久豁免必须有活的判据锚点)")
    for name, w in waivers.items():
        path = os.path.join(ROOT, reg["adr_dir"], name)
        if not os.path.isfile(path):
            errors += fail(f"豁免条目指向不存在的文件:{name}")
            continue
        lines = open(path, encoding="utf-8").read().split("\n")
        anchors = w.get("evidence_anchor") or []
        if not anchors:
            errors += fail(f"豁免条目缺 `evidence_anchor` ⇒ 豁免不得成为后门:{name}")
            continue
        dead = [a for a in anchors if not (1 <= a <= len(lines)) or not NESTED_RE.match(lines[a - 1])]
        if dead:
            errors += fail(f"豁免锚点已失效 {name}: 行 {dead}(须指向仍含 Verification 标题的行)")
        else:
            ok(f"{name}: 锚点 {anchors} 均存活")
        if not w.get("reason") or not w.get("ledger_entry"):
            errors += fail(f"豁免条目字段不齐(reason / ledger_entry 必填):{name}")
    if not waivers:
        info("无永久豁免条目")

    # ── ④ 登记表健康 ───────────────────────────────────────────────
    print("\n[4/4] 登记表健康(reason / trigger_condition / owner / ledger_entry)")
    for name, p in pending.items():
        miss = [k for k in ("reason", "trigger_condition", "owner", "ledger_entry") if not p.get(k)]
        if miss:
            errors += fail(f"待补条目字段缺失 {name}: {', '.join(miss)}")
    if pending:
        ok(f"待补条目 {len(pending)} 份字段齐备")

    # ── 报告(不判红) ───────────────────────────────────────────────
    if pending:
        print("\n── 待补清单(起草中 ADR 缺顶层 Verification 章 · 签发前必须补)──")
        for name, p in pending.items():
            print(f"  ⏳ {name}  [owner={p.get('owner')} batch={p.get('batch')}]")
    if waivers:
        print("\n── 永久豁免(ADR 不可变 ⇒ 正文无法补章)──")
        for name, w in waivers.items():
            print(f"  ⏭ {name}  anchors={w.get('evidence_anchor')}")
    for key, label in (
        ("status_drift", "NUMBER-MAP ↔ ADR header 的可判定矛盾(修法在冻结集内 ⇒ 须授权)"),
        ("map_status_ambiguity", "NUMBER-MAP 状态列 ✅ 三义(不可判读 ⇒ 已降级为镜像)"),
    ):
        rows = reg.get(key, [])
        if rows:
            print(f"\n── {label} · {len(rows)} 条 ──")
            for r in rows:
                print(f"  · {r.get('adr') or r.get('cep_id')}: {r.get('map_state', '')}")

    print("")
    if errors:
        print(f"❌ ADR 结构闸: {errors} 项失败")
        return 1
    print("✅ ADR 结构闸: 全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
