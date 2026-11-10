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

派生视图(**不判红**,per 台账 Entry #321 / #322):
  · **编号 → 生命周期 / 文件存在性** —— 读各 ADR 的 header `> **状态**:` **现算现打**
    ⇒ **派生**而非存储 ⇒ 不可能腐化(先例:`cep_invariant_map_check.py`「映射表是生成物」)。
    ⚠️ **生命周期状态真源 = 各 ADR 文件自身**(header + Status Update 链,per oracle C-3);
    本视图**非权威**,且⛔ **不得被任何文档引用为状态依据**。
    口径落点 = `docs/adr/ADR-CEP-NUMBER-MAP.md` §2 决策 1 表上方。

用法:
  python3 scripts/test/cep_adr_structural_check.py            # 校验(退出码 0 = 绿)
  python3 scripts/test/cep_adr_structural_check.py --list     # 只打印清单
"""
import argparse
import fnmatch
import io
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


# ══════════════════════════════════════════════════════════════════════
# 派生视图:编号 → 生命周期 / 文件存在性(**纯导航 · 不判红**)
#   · 左表 = NUMBER-MAP §2 决策 1 的编号集合(⛔ 非磁盘清单 —— 否则 CEP-10 缺席)
#   · 生命周期抽取**锚定首个** `> **状态**:` 行
# ══════════════════════════════════════════════════════════════════════
NUMBER_MAP = os.path.join(ROOT, "docs", "adr", "ADR-CEP-NUMBER-MAP.md")
CEP_ID_RE = re.compile(r"^\| \*\*(CEP-[0-9A]+)\*\* \|")
STATUS_RE = re.compile(r"^>\s*\*\*状态\*\*:\s*(.+)$")


def number_map_ids():
    """从 NUMBER-MAP §2 决策 1 表读编号集合(⛔ 不依赖磁盘文件清单)"""
    if not os.path.exists(NUMBER_MAP):
        return []
    ids = []
    for line in io.open(NUMBER_MAP, encoding="utf-8"):
        m = CEP_ID_RE.match(line)
        if m and m.group(1) not in ids:
            ids.append(m.group(1))
    return ids


def adr_file_for(cep_id):
    """编号 → 对应 ADR 文件路径(⛔ None = 无文件 ⇒ 该编号未起草)"""
    d = os.path.join(ROOT, "docs", "adr")
    if not os.path.isdir(d):
        return None
    prefix = "ADR-" + cep_id + "-"
    for name in sorted(os.listdir(d)):
        if name.startswith(prefix) and name.endswith(".md"):
            return os.path.join(d, name)
    return None


def read_lifecycle_state(path):
    """读 header 的**首个** `> **状态**:` 行(锚定首个匹配 —— 某些 ADR 另有
    无 emoji 的状态行,如 ADR-CEP-17 ⇒ 不锚定会抽到散文行)

    返回 (生命周期类, 原始串);生命周期类 ∈ {已签发, 起草中, 未识别}
    """
    if not path:
        return ("未起草", "")
    for line in io.open(path, encoding="utf-8"):
        m = STATUS_RE.match(line)
        if not m:
            continue
        raw = m.group(1).strip()
        if "已签发" in raw:
            return ("已签发", raw)
        if "起草中" in raw:
            return ("起草中", raw)
        return ("未识别", raw)
    return ("未识别", "(无 `> **状态**:` 行)")


def print_derived_lifecycle_view():
    """打印派生视图(**纯导航 · 不判红**)"""
    ids = number_map_ids()
    print("\n── 派生视图:编号 → 生命周期 / 文件存在性 ──")
    print("  ⚠️ **派生自各 ADR header,非权威**;生命周期状态真源 = 各 ADR 自身"
          "(header + Status Update 链,per oracle C-3)")
    if not ids:
        print("  (NUMBER-MAP 未读到编号集合)")
        return
    parsed = 0
    unknown = []
    for cep_id in ids:
        path = adr_file_for(cep_id)
        if path is None:
            print(f"  · {cep_id:<8} 🟡 未起草(无 ADR 文件)")
            continue
        state, raw = read_lifecycle_state(path)
        if state == "未识别":
            unknown.append(cep_id)
        else:
            parsed += 1
        short = os.path.basename(path)
        print(f"  · {cep_id:<8} {state:<6} {short}  |  {raw}")
    total = len([i for i in ids if adr_file_for(i) is not None])
    print(f"  覆盖率:已解析 {parsed}/{total}" + (f" · 未识别: {', '.join(unknown)}" if unknown else ""))


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

    # ── ④b 归因 id 债务登记机检(per oracle 2026-11-09 must-fix #3)──────────
    #   「判据存在但机器载体无对应 id 条目」这类债务必须**登记在机器可查处**,
    #   且登记本身**可机检** ⇒ 否则它只是「纯承诺」(给缺口盖章)。
    print("\n[4b/4] 归因 id 债务登记(pending_invariant_ids)机检")
    ledger_path = os.path.join(ROOT, ".rddf", "roadmap", "objectives", "obj-cep-platform.md")
    ledger_text = ""
    if os.path.isfile(ledger_path):
        ledger_text = open(ledger_path, encoding="utf-8").read()
    id_debts = reg.get("pending_invariant_ids", [])
    if not id_debts:
        info("无归因 id 债务登记")
    for d in id_debts:
        miss = [k for k in ("adr", "chapter", "target_id", "scope", "trigger_condition", "owner", "ledger_entry")
                if not d.get(k)]
        if miss:
            errors += fail("id 债务条目字段缺失 %s: %s" % (d.get("adr"), ", ".join(miss)))
            continue
        apath = os.path.join(ROOT, reg["adr_dir"], d["adr"])
        if not os.path.isfile(apath):
            errors += fail("id 债务条目指向不存在的 ADR:%s" % d["adr"])
            continue
        atext = open(apath, encoding="utf-8").read()
        for chap in [c.strip() for c in str(d["chapter"]).split("/")]:
            if chap and chap not in atext:
                errors += fail("id 债务条目的章节锚点 %s §%s 在 ADR 内不存在" % (d["adr"], chap))
        if not re.fullmatch(r"INV-CEP-[0-9A-Z]+-ICx", str(d["target_id"])):
            errors += fail("id 债务条目的 target_id 命名空间不合规 %s(须形如 INV-CEP-NN-ICx)" % d["target_id"])
        if ledger_text and ("### %s" % d["ledger_entry"]) not in ledger_text:
            errors += fail("id 债务条目的台账凭据不存在:%s" % d["ledger_entry"])
    if id_debts:
        ok("归因 id 债务登记 %d 笔逐条机检通过(ADR 存在 + 章节锚点 + id 命名空间 + 台账 Entry)" % len(id_debts))

    # ── 报告(不判红) ───────────────────────────────────────────────
    if pending:
        print("\n── 待补清单(起草中 ADR 缺顶层 Verification 章 · 签发前必须补)──")
        for name, p in pending.items():
            print(f"  ⏳ {name}  [owner={p.get('owner')} batch={p.get('batch')}]")
    if waivers:
        print("\n── 永久豁免(ADR 不可变 ⇒ 正文无法补章)──")
        for name, w in waivers.items():
            print(f"  ⏭ {name}  anchors={w.get('evidence_anchor')}")
    print_derived_lifecycle_view()

    print("\n── 生命周期状态面 ──")
    print("  · NUMBER-MAP 状态列已归零(口径见 `ADR-CEP-NUMBER-MAP` §2 决策 1 表上方)")

    print("")
    if errors:
        print(f"❌ ADR 结构闸: {errors} 项失败")
        return 1
    print("✅ ADR 结构闸: 全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
