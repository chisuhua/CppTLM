#!/usr/bin/env python3
# scripts/docs/gen_adr_index.py
#   —— `docs/adr/README.md`（真源 3 索引）的「议题 / 状态 / 份数」机械生成器
#
# 用法:
#   python3 scripts/docs/gen_adr_index.py --write     # 就地重写生成区间
#   python3 scripts/docs/gen_adr_index.py --check     # 已同步 ⇒ rc=0;漂移 ⇒ rc=1(CI / ctest 用)
#   python3 scripts/docs/gen_adr_index.py --json      # 导出抽取结果(供闸做判据②/①核对)
#   --doc <path>                                      # 目标文档(默认 docs/adr/README.md)
#
# ── 判据面(⚠️ 唯一真源 = `docs/adr/README.md` §7「抽取契约(判据面)」)──────────
#   ⛔ 本文件**只是 §7 的实现**,不得自创口径。改抽取规则 = 改判据面 ⇒ 须走授权
#      (同 `ADR-CEP-11` 协议版本锁定的判据面处置)。
#
#   ① 状态源行(顺序固定): `> **状态**: X` → 否则 `## Status` 之后首个非空行 → 皆无 ⇒ fail loud
#   ② 词表(封闭 9 项): 已签发 / 已实施 / 已确认 / Accepted / Proposed / 起草中 / 部分实施 / 提案 / 待确认
#      取词规则: 源行内扫描,取**最后一个**命中(时间顺序语义:后写者为准)
#      ⇒ 覆盖 ADR-CEP-20(`🟡 **Proposed**✅ **已签发**(...)` ⇒ `已签发`)
#   ③ 归一化: 词 → 规范短标签;**emoji 由词决定**,⛔ 不取源行 emoji
#      ⇒ 覆盖 ADR-DGPU-15(`✅ **Proposed**` ⇒ `🟢 Proposed`)
#   ④ 词表未命中 ⇒ **fail loud**(指名文件 + 原行);显式豁免表见 EXEMPT(当前 1 项)
#   ⑤ 议题 = H1 去 `ADR-<编号>:` 前缀(正则须**允许连字符**,否则 `ADR-CEP-00:` 剥不掉)
#   ⑥ 系列 = `ADR-CEP-*`→CEP / `ADR-DGPU-*`→DGPU / `ADR-X*`→X / 其余→其他
#
# ── 生成区间 ───────────────────────────────────────────────────────────────
#   `<!-- BEGIN ADR INDEX (generated) -->` 与 `<!-- END ADR INDEX (generated) -->` 之间。
#   ⛔ 区间外(口径说明 / 归档与取代 / 状态标签口径)一律手写不动。
#   ⚠️ **外来内容护栏**:区间内出现**不属生成块形态**的行 ⇒ `--write` **拒绝写盘**(rc=1 并打印行号),
#      `--check` 同口径 ⇒ 防「误置的正文被静默删除」(同 `generate_doc_index.py` 的 F-2 教训)。

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOC_DEFAULT = ROOT / "docs/adr/README.md"
ADR_DIR = ROOT / "docs/adr"

BEGIN = "<!-- BEGIN ADR INDEX (generated) -->"
END = "<!-- END ADR INDEX (generated) -->"

# §7.2 封闭词表（顺序 = 展示时的规范顺序）
VOCAB = ["已签发", "已实施", "已确认", "Accepted", "Proposed", "起草中", "部分实施", "提案", "待确认"]
# §7.2 同义词（提议 ≡ 提案；⛔ 不新增状态类，共用同一规范标签）
SYNONYM = {"提议": "提案"}
LABEL = {
    "已签发": "✅ 已签发",
    "已实施": "✅ 已实施",
    "已确认": "✅ 已确认",
    "Accepted": "✅ Accepted",
    "Proposed": "🟢 Proposed",
    "起草中": "🟡 起草中",
    "部分实施": "🟡 部分实施",
    "提案": "📋 提案",
    "待确认": "📋 待确认",
    "提议": "📋 提案",   # §7.2 同义词：与「提案」共用规范标签
}
# §7.2 显式豁免表（豁免须逐条写明理由；上限 1 项）
EXEMPT = {
    "ADR-X-SUMMARY.md": (
        "🟡 汇总（多阶段）",
        "汇总文档：其状态是多阶段进度叙述「Phase 3.1 已完成 / Phase 3.2 待实施 / Phase 3.3 部分落地」，"
        "不属决策状态词",
    ),
}

SERIES = [
    ("CEP", re.compile(r"^ADR-CEP-")),
    ("DGPU", re.compile(r"^ADR-DGPU-")),
    ("X", re.compile(r"^ADR-X")),
]
SERIES_ORDER = ["CEP", "DGPU", "X", "其他"]

STATUS_INLINE = re.compile(r"^\s*[>\|]?\s*[-*]?\s*\**\s*(?:状态|Status)\s*\**\s*[:：]\s*(.+)$", re.M)
STATUS_SECTION = re.compile(r"^##\s+Status\s*$", re.M)
H1 = re.compile(r"^#\s+(.+)$", re.M)
ISSUE_PREFIX = re.compile(r"^ADR-[A-Za-z0-9.-]+:\s*")


def series_of(name):
    for tag, rx in SERIES:
        if rx.match(name):
            return tag
    return "其他"


def status_source(text):
    """§7.1:返回 (原始状态行, 出处) —— 皆无 ⇒ None。"""
    m = STATUS_INLINE.search(text)
    if m:
        return m.group(1).strip(), "inline"
    m = STATUS_SECTION.search(text)
    if m:
        rest = text[m.end():].split("\n")
        for line in rest:
            if line.strip():
                return line.strip(), "section"
    return None


def normalize(raw):
    """§7.2~7.3:取最后一个词表命中 ⇒ 规范短标签。未命中 ⇒ None(fail loud 由调用方负责)。"""
    hits = []
    for i, w in enumerate(VOCAB + list(SYNONYM)):
        for m in re.finditer(re.escape(w), raw):
            hits.append((m.start(), i, w))
    if not hits:
        return None
    hits.sort()
    return LABEL[hits[-1][2]]


def issue_of(text):
    """§7.3:H1 去掉 `ADR-<编号>:` 前缀。"""
    m = H1.search(text)
    if not m:
        return None
    return ISSUE_PREFIX.sub("", m.group(1).strip()).strip()


def collect():
    """返回 (rows, errors)。rows = [{file, series, issue, status}]"""
    rows, errors = [], []
    files = sorted(p for p in ADR_DIR.glob("ADR-*.md"))
    if not files:
        errors.append(f"{ADR_DIR} 下无 `ADR-*.md` ⇒ 无法生成")
        return rows, errors
    for p in files:
        name = p.name
        text = p.read_text(encoding="utf-8", errors="replace")
        src = status_source(text)
        if src is None:
            errors.append(f"{name}: §7.1 两条形态皆未命中(无 `> **状态**:` 也无 `## Status` 章)")
            continue
        raw, _how = src
        if name in EXEMPT:
            status = EXEMPT[name][0]
        else:
            status = normalize(raw)
            if status is None:
                errors.append(
                    f"{name}: §7.2 词表未命中且不在豁免表 ⇒ fail loud;原行 = {raw!r}"
                )
                continue
        issue = issue_of(text)
        if not issue:
            errors.append(f"{name}: 无 H1 ⇒ 无法抽取议题")
            continue
        if "|" in issue:
            errors.append(f"{name}: 议题含 `|` ⇒ 无法安全写入 Markdown 表格:{issue!r}")
            continue
        rows.append(dict(file=name, series=series_of(name), issue=issue, status=status))
    return rows, errors


def build_block(rows):
    """生成区间正文(不含 BEGIN/END 行)。"""
    by = {s: [] for s in SERIES_ORDER}
    for r in rows:
        by[r["series"]].append(r)
    out = [""]
    # §1 全量统计
    out.append("## 1. 全量统计")
    out.append("")
    out.append("| 分组 | 份数 | 状态分布 |")
    out.append("|------|------|---------|")
    out.append("| **CEP 系列**（Coherence Exploration Platform） | %d | %s |"
               % (len(by["CEP"]), dist(by["CEP"])))
    out.append("| **DGPU 系列**（dGPU 应用架构） | %d | %s |"
               % (len(by["DGPU"]), dist(by["DGPU"])))
    out.append("| **X 系列**（框架基础：事务 / 错误 / 复位 / 参数 / 端口） | %d | %s |"
               % (len(by["X"]), dist(by["X"])))
    out.append("| **其他**（INC / LIB / METRIC / NV / TEMPLATE） | %d | %s |"
               % (len(by["其他"]), dist(by["其他"])))
    out.append("| **合计** | **%d** | — |" % len(rows))
    out.append("")
    out.append("> **口径说明**：`ADR-CEP-NUMBER-MAP.md` 是**编号预分配表**（非决策），计入 CEP 份数；")
    out.append("> `ADR-P1-TEMPLATE.md` 是**模板**（非决策），计入其他份数。")
    out.append("> 本表由 `scripts/docs/gen_adr_index.py` **机械生成**（判据面 = 本文件 §7）；"
               "`AGENTS.md` 的「23 份 ADR（= 24 个 `ADR-CEP-*` 文件 − `NUMBER-MAP`）」是**不同口径**，"
               "两者均正确。")
    out.append("")
    # §2~§5 四张系列表
    titles = {
        "CEP": "## 2. CEP 系列（Coherence Exploration Platform）",
        "DGPU": "## 3. DGPU 系列（dGPU 应用架构）",
        "X": "## 4. X 系列（框架基础）",
        "其他": "## 5. 其他",
    }
    for s in SERIES_ORDER:
        out.append("---")
        out.append("")
        out.append(titles[s])
        if s == "CEP":
            out.append("")
            out.append("> 治理宪章 = [`ADR-CEP-00-platform-charter-and-governance.md`](./ADR-CEP-00-platform-charter-and-governance.md)；")
            out.append("> **真源章节锚点表** = [`ADR-CEP-00A-real-source-anchor-map.md`](./ADR-CEP-00A-real-source-anchor-map.md)")
            out.append("> （**引用任何 CEP 章节前必查该表**，禁凭记忆写节号）。")
        if s == "DGPU":
            out.append("")
            out.append("> 对应三视角架构文档：`docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/architecture.md`（真源 2）。")
        out.append("")
        out.append("| ADR | 议题 | 状态 |")
        out.append("|-----|------|------|")
        for r in by[s]:
            out.append("| [%s](./%s) | %s | %s |" % (r["file"], r["file"], r["issue"], r["status"]))
        out.append("")
    return out


def dist(items):
    from collections import Counter
    c = Counter(i["status"] for i in items)
    if not c:
        return "—"
    # 规范顺序：词表顺序在前，豁免标签在后
    order = [LABEL[w] for w in VOCAB] + [v[0] for v in EXEMPT.values()]
    keys = [k for k in order if k in c] + sorted(k for k in c if k not in order)
    return " · ".join("%s %d" % (k, c[k]) for k in keys)


def is_generatable(line):
    """生成块可能产生的行形态(白名单)。⛔ 双向自洽:生成块自身也须落在白名单内。"""
    s = line.strip()
    if s == "":
        return True
    if s.startswith(("|", ">", "---")):
        return True
    if s.startswith("## 1. ") or s.startswith("## 2. ") or s.startswith("## 3. "):
        return True
    if s.startswith("## 4. ") or s.startswith("## 5. "):
        return True
    return False


def locate(lines):
    """返回 (begin_idx, end_idx) 行索引；缺失 ⇒ None。"""
    b = e = None
    for i, l in enumerate(lines):
        if l.strip() == BEGIN:
            b = i
        elif l.strip() == END:
            e = i
    if b is None or e is None or e < b:
        return None
    return b, e


def main(argv):
    ap = argparse.ArgumentParser(description="docs/adr/README.md 索引生成器(判据面 = 该文件 §7)")
    ap.add_argument("--doc", default=str(DOC_DEFAULT))
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--write", action="store_true", help="就地重写生成区间")
    g.add_argument("--check", action="store_true", help="已同步 ⇒ rc=0;漂移 ⇒ rc=1")
    g.add_argument("--json", action="store_true", help="导出抽取结果")
    args = ap.parse_args(argv)

    doc = Path(args.doc)
    if not doc.is_file():
        print(f"✗ 目标文档不存在: {doc}", file=sys.stderr)
        return 2

    rows, errors = collect()
    if errors:
        print("✗ §7 抽取失败(fail loud —— 闸与生成器口径要求「词表外不得静默跳过」):", file=sys.stderr)
        for e in errors:
            print("   · " + e, file=sys.stderr)
        return 1
    rows.sort(key=lambda r: (SERIES_ORDER.index(r["series"]), r["file"]))

    block = build_block(rows)
    bad_block = [l for l in block if not is_generatable(l)]
    if bad_block:
        print("✗ 生成块含非白名单行形态(闸与生成器口径会分裂): %r" % bad_block[:3], file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(dict(total=len(rows), rows=rows), ensure_ascii=False, indent=2))
        return 0

    lines = doc.read_text(encoding="utf-8").split("\n")
    loc = locate(lines)
    if loc is None:
        print(f"✗ 未找到生成区间标记({BEGIN} / {END})", file=sys.stderr)
        print("   · 一次性插入方法:在 §1 标题前放 BEGIN 行、§5 表末后放 END 行", file=sys.stderr)
        return 2
    b, e = loc

    foreign = [
        (i + 1, lines[i]) for i in range(b + 1, e)
        if not is_generatable(lines[i])
    ]
    if foreign:
        print("✗ 生成区间内出现非生成块形态的行 ⇒ **拒绝写盘**(防静默删除正文):", file=sys.stderr)
        for ln, txt in foreign[:10]:
            print("   L%d: %s" % (ln, txt.strip()[:100]), file=sys.stderr)
        print("   · 处置:把该内容移到区间**之外**(per 本文件 §7.5 的生成区间约定)", file=sys.stderr)
        return 1

    new_lines = lines[: b + 1] + block + lines[e:]
    new_text = "\n".join(new_lines)

    if args.check:
        cur = doc.read_text(encoding="utf-8")
        if cur == new_text:
            print("✓ 已同步(--check rc=0)")
            return 0
        print("✗ 漂移:文档与生成器输出不一致 ⇒ 跑 --write", file=sys.stderr)
        return 1

    doc.write_text(new_text, encoding="utf-8")
    print(f"✓ 已重写生成区间:{len(rows)} 份 ADR")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
