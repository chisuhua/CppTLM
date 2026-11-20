#!/usr/bin/env python3
"""cep_invariant_batch1_ledger_check.py —— invariant 批 1 的台账锚**可解析**闸

per 单元 `.rddf/improvements/cep-invariant-modeling-batch1.md` 的 AC-4：
    「新增 Entry **真实存在** 且其**正文含** 3 条 id」
    + 反 fixture：把 Entry 正文里的 id 改掉 ⇒ 判红 ⇒ 还原绿。

为什么要独立一个闸（而不是靠 `cep_invariant_map_check.py` 的判据 ④）：
    ④ 只校验**仍登记在 deferrals 里**的条目的 `ledger_entry` 锚。本批 3 条已**移出** deferrals
    （per 判据 ③「载体 ∩ ⏕ = ∅」）⇒ ④ 对它们**不再生效** ⇒ 若没有本闸，"台账里写着这 3 条
    已处置" 就只是**自述**、没有任何机器面。

判据（三条同时成立才绿）：
    ① 台账中存在某个 `### Entry #<n>` 段，其**正文同时含** 3 条 id
    ② 该段含载体落点（3 个载体文件路径各至少出现一次）
    ③ 该段**未**把三条中的任何一条表述为「已建模」（AC-1：三条终态不同质，⛔ 不得统一宣称）

用法：python3 scripts/test/cep_invariant_batch1_ledger_check.py
退出码：0 = 绿；1 = 红
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEDGER = ROOT / ".rddf" / "roadmap" / "objectives" / "obj-cep-platform.md"

REQUIRED_IDS = ("CEN-Ib8", "INV-CEP-08-IC1", "INV-CEP-09-IC2")
CARRIER_FILES = (
    "include/tlm/cep/mesif_protocol_engine.hh",
    "test/test_cep_invariant_cen_ib8.cc",
    "test/test_cep_ordering_point_checker.cc",
    "scripts/test/cep_invariant_ic2_absent_check.sh",
)


def split_entries(text: str):
    """按 `### Entry #<n>` 标题切段 ⇒ [(编号, 段正文)]"""
    out = []
    heads = list(re.finditer(r"^### Entry #(\d+)", text, re.M))
    for i, m in enumerate(heads):
        start = m.start()
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        out.append((m.group(1), text[start:end]))
    return out


def main() -> int:
    if not LEDGER.exists():
        print(f"  ✗ 缺台账: {LEDGER.relative_to(ROOT)}", file=sys.stderr)
        return 1
    text = LEDGER.read_text(encoding="utf-8")

    hits = [(n, body) for (n, body) in split_entries(text)
            if all(i in body for i in REQUIRED_IDS)]
    if not hits:
        print(f"  ✗ 台账中**没有**任何 Entry 同时含 {list(REQUIRED_IDS)} "
              f"⇒ 本批的台账锚不可解析", file=sys.stderr)
        return 1

    # 取**编号最大**的匹配段作本批锚（= 「新增 Entry」）；历史 Entry 提到这 3 条 id
    # 属登记溯源，不构成本批锚。
    num, body = max(hits, key=lambda h: int(h[0]))
    missing_carriers = [f for f in CARRIER_FILES if f not in body]
    if missing_carriers:
        print(f"  ✗ 含 {list(REQUIRED_IDS)} 的最新 Entry = #{num}，但它不含本批载体落点 "
              f"{missing_carriers} ⇒ 本批台账锚缺失（Entry 可能被改写/删除）", file=sys.stderr)
        return 1
    claims_modelled = [i for i in REQUIRED_IDS if re.search(
        re.escape(i) + r"[^\n]{0,24}已建模", body)]
    if claims_modelled:
        print(f"  ✗ Entry #{num} 把 {claims_modelled} 表述为「已建模」"
              f"(AC-1:三条终态不同质 ⇒ ⛔ 不得统一宣称)", file=sys.stderr)
        return 1
    good = [num]

    print(f"  ✓ invariant 批 1 的台账锚可解析: Entry #{'/#'.join(good)} 含 "
          f"{list(REQUIRED_IDS)} + {len(CARRIER_FILES)} 个载体落点，且无「已建模」误称")
    return 0


if __name__ == "__main__":
    sys.exit(main())
