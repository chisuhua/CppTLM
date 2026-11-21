#!/usr/bin/env python3
"""cep_invariant_batch2_ledger_check.py —— invariant 批 2 的台账锚**可解析**闸

per 单元 `.rddf/improvements/cep-invariant-modeling-batch2.md` 的 AC-1④：
    「台账中存在某个 `### Entry #<n>` 段，其**正文同时含** `INV-CEP-13-IC1` 与
      `INV-CEP-13-IC3`，**且**含本单元的载体落点，**且**该段**未**把二者表述为
      「已建模 / 可判」」
    + 反 fixture：把 Entry 正文里的任一 id 改掉 ⇒ 判红 ⇒ 还原绿。

为什么要独立一个闸（而不是靠 `cep_invariant_map_check.py` 的判据 ④）：
    ④ 只校验**仍登记在 deferrals 里**的条目的 `ledger_entry` 锚。本批 2 条已**移出**
    deferrals（per 判据 ③「载体 ∩ ⏭ = ∅」）⇒ ④ 对它们**不再生效** ⇒ 若没有本闸，
    「台账里写着这 2 条已处置」就只是**自述**、没有任何机器面。
    （批 1 已因此另立闸 `cep_invariant_batch1_ledger_check.py` —— 本闸照其范式。）

⚠️ **不得**用「台账全文含该 id」作判据 —— 台账现存 **13 / 9 处**含这两个 id
    ⇒ 全文判据**恒绿**（无鉴别力）。⇒ 必须按 **Entry 段**切分并取**编号最大**的匹配段。

用法：python3 scripts/test/cep_invariant_batch2_ledger_check.py
退出码：0 = 绿；1 = 红
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEDGER = ROOT / ".rddf" / "roadmap" / "objectives" / "obj-cep-platform.md"

REQUIRED_IDS = ("INV-CEP-13-IC1", "INV-CEP-13-IC3")
CARRIER_FILES = (
    "include/tlm/cep/protocol_instance.schema.json",
    "test/test_cep_combination_contract.cc",
    "test/cep_combination_contract_expected.json",
)
# ⛔ 「已建模 / 可判」是**夸大**表述：AC-2 明令「66/66」只能读作「66 条全部有载体」，
#     ⛔ 不得读作「全部可判 / 全部达成」⇒ 台账 Entry 不得这样宣称这 2 条。
# ⚠️ 两处负向限定(实测踩到的坑)：
#     ① `(?<!不)` —— 「不可判的声明项」是**反向**用法,不算夸大;
#     ② `(?!定性)` —— 「公理的**可判定性**仍受限于…」是**合法技术术语**(谈的是公理的
#        可判定性受限,⛔ 不是宣称「66 条全部可判」)⇒ 不算夸大。
FORBIDDEN_PATTERNS = (
    (r"已建模", "「已建模」"),
    (r"(?<!不)可判(?!定性)", "「可判」"),
    (r"全部达成", "「全部达成」"),
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

    # 取**编号最大**的匹配段作本批锚（= 「新增 Entry」）；历史 Entry 提到这 2 条 id
    # 属登记溯源，不构成本批锚。
    num, body = max(hits, key=lambda h: int(h[0]))
    missing_carriers = [f for f in CARRIER_FILES if f not in body]
    if missing_carriers:
        print(f"  ✗ 含 {list(REQUIRED_IDS)} 的最新 Entry = #{num}，但它不含本批载体落点 "
              f"{missing_carriers} ⇒ 本批台账锚缺失（Entry 可能被改写/删除）", file=sys.stderr)
        return 1
    claims = []
    for pat, label in FORBIDDEN_PATTERNS:
        for m in re.finditer(pat, body):
            line_no = body[:m.start()].count("\n") + 1
            claims.append(f"{label} @段内第 {line_no} 行")
    if claims:
        print(f"  ✗ Entry #{num} 含夸大表述 {claims}"
              f"（AC-2：只能是「有载体」，⛔ 不得读作「全部可判 / 全部达成」）", file=sys.stderr)
        return 1

    print(f"  ✓ invariant 批 2 的台账锚可解析: Entry #{num} 含 "
          f"{list(REQUIRED_IDS)} + {len(CARRIER_FILES)} 个载体落点，且无夸大表述")
    return 0


if __name__ == "__main__":
    sys.exit(main())
