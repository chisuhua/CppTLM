#!/usr/bin/env bash
# scripts/test/cep_index_position_check.sh
# **索引位置 + 完整性闸**(per 台账 Entry #295 的 F-08 残余裁定 + 新纪律;2026-11-12 三合一强化 per Entry #342)
#
# 纪律(逐字):「**索引永久置于文末**;此后**新增**段落 SHALL 插在索引**之前** ⇒ ⇒
#            **新增内容永不使既有 `path:line` 锚点位移**;索引**每次追加后重生成**。」
#
# 判据(全部可机器判;期望值来源 = **文件自身标题结构**,⛔ 不是索引自述 —— 防自证):
#   ① `docs/architecture/07-coherence-exploration-platform.md` 的 **§末 文档索引**段
#      必须是该文件的**最后一个** `## ` 级标题 ⇒ 即:**索引之后零个** `## ` 标题;
#   ② **三合一计数**:实际 `## ` 数 == 自述「N 个一级节」== 索引 A 数据行数
#      **且** 实际 `Status Update` 标题数 == 自述「M 则 SU」== 索引 B 数据行数;
#   ③ **行号正确性**:索引 A / 索引 B 的「行」列逐行 == 该标题在本文件中的**实测行号**
#      (标题必须存在 ⇒ 顺带检出「索引里有、正文里没有」的幽灵行);
#   ④ **标题覆盖**:索引 A 覆盖**全部** `## ` 标题(零遗漏 + 零重复);索引 B 覆盖全部 SU。
#
# ⚠️ **为什么需要它**:`07 号` 的索引头自述「由脚本生成」,而生成器**长期不在仓内**
#    ⇒ 索引靠**手工同步** ⇒ 实测腐化(自述 23 个一级节 vs 实际 28;索引 A 行号 **10/23 错**)。
#    生成器已于 2026-11-12 落仓(`scripts/cep/generate_doc_index.py`,per 台账 Entry #342);
#    本闸是它的**对手**:生成器负责产,本闸负责**发现漂移**(两者独立 ⇒ 任一方被绕过都会红)。
#    ⛔ 无本闸则「追加段落却忘重生成索引」与「把新段加到索引之后(破坏锚点稳定性)」都会**静默**发生。
#
# 用法:bash scripts/test/cep_index_position_check.sh
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

DOC="docs/architecture/07-coherence-exploration-platform.md"

if [[ ! -f "$DOC" ]]; then
  echo "✗ 未找到 $DOC" >&2
  exit 2
fi

python3 - "$DOC" <<'PY'
import re, sys

path = sys.argv[1]
lines = open(path, encoding="utf-8").read().splitlines()
text = "\n".join(lines)
problems = []


def fail(msg):
    problems.append(msg)
    print("✗ %s: %s" % (path, msg), file=sys.stderr)


def is_h2(l):
    return l.startswith("## ") and not l.startswith("### ")


# ── ① 索引段必须是最后一个 `## ` 级标题 ─────────────────────────────────────
idx_positions = [i for i, l in enumerate(lines) if is_h2(l) and "§末 文档索引" in l]
if not idx_positions:
    print(f"✗ {path}: 未找到「## §末 文档索引」标题", file=sys.stderr)
    sys.exit(1)
if len(idx_positions) > 1:
    fail("「§末 文档索引」标题出现 %d 次(应恰好 1 次)" % len(idx_positions))
idx = idx_positions[0]
after = [(i + 1, l) for i, l in enumerate(lines[idx + 1:], start=idx + 1) if is_h2(l)]
if after:
    for lineno, title in after[:5]:
        fail("%d: 索引之后仍有 `## ` 级标题 —— 违反「索引永久置于文末」纪律: %s" % (lineno, title[:60]))
    if len(after) > 5:
        fail("索引之后共 %d 个 `## ` 级标题(仅列前 5 条)" % len(after))

# ── ② 期望值的独立来源:文件自身的标题结构 ─────────────────────────────────
heads = [(i + 1, l[3:].strip()) for i, l in enumerate(lines) if is_h2(l)]
h2 = {}
dup = []
for lineno, title in heads:
    if title in h2:
        dup.append(title)
    else:
        h2[title] = lineno
if dup:
    fail("文件存在重复的 `## ` 标题 %d 个(行号无法唯一定位): %s" % (len(dup), dup[:3]))
sus = [(lineno, t) for lineno, t in heads if "Status Update" in t]

m = re.search(r"本文件已 \*\*(\d+) 个一级节 / (\d+) 则 Status Update\*\*", text)
if not m:
    print(f"✗ {path}: 未找到索引头部的「N 个一级节 / M 则 Status Update」自述行", file=sys.stderr)
    sys.exit(1)
claimed_sections, claimed_sus = int(m.group(1)), int(m.group(2))

if len(heads) != claimed_sections:
    fail("实际 `## ` 数 %d ≠ 自述「%d 个一级节」" % (len(heads), claimed_sections))
if len(sus) != claimed_sus:
    fail("实际 Status Update 数 %d ≠ 自述「%d 则 Status Update」" % (len(sus), claimed_sus))

# ── ③ 索引 A / B 表格解析 ──────────────────────────────────────────────────
def section_body(start_prefix):
    start = next((i for i, l in enumerate(lines) if l.startswith(start_prefix)), None)
    if start is None:
        return None
    body = []
    for l in lines[start + 1:]:
        if l.startswith("### ") or l.startswith("## "):
            break
        body.append(l)
    return body


def parse_rows(prefix, tag, ncols):
    body = section_body(prefix)
    if body is None:
        fail("未找到 `%s` 小节" % prefix)
        return []
    rows = []
    for l in body:
        if not l.startswith("|"):
            continue
        if set(l.replace("|", "").replace(" ", "")) <= set("-:"):
            continue  # 表格分隔行
        cells = [c.strip() for c in l.strip().strip("|").split("|")]
        if cells[0] == "#":
            continue  # 表头行
        if len(cells) != ncols:
            fail("%s: 列数 %d ≠ %d —— %r" % (tag, len(cells), ncols, l[:60]))
            continue
        if not cells[0].isdigit():
            fail("%s: 首列非序号(%r)—— %r" % (tag, cells[0], l[:60]))
            continue
        rows.append(cells)
    return rows


rows_a = parse_rows("### 索引 A", "索引 A", 3)          # # | 章节 | 行
rows_b = parse_rows("### 索引 B", "索引 B", 5)          # # | 日期 | 主题 | 台账 | 行

if len(rows_a) != claimed_sections:
    fail("索引 A 数据行数 %d ≠ 自述「%d 个一级节」" % (len(rows_a), claimed_sections))
if len(rows_b) != claimed_sus:
    fail("索引 B 数据行数 %d ≠ 自述「%d 则 Status Update」" % (len(rows_b), claimed_sus))

# ── ④ 行号逐行校验 + 标题覆盖 ─────────────────────────────────────────────
def check_rows(rows, tag, title_idx, line_idx):
    listed = []
    for cells in rows:
        no, title, claim = cells[0], cells[title_idx], cells[line_idx]
        listed.append(title)
        if not claim.isdigit():
            fail("%s 第 %s 行:「行」列非数字(%r)" % (tag, no, claim))
            continue
        actual = h2.get(title)
        if actual is None:
            fail("%s 第 %s 行:标题在文件中不存在(%r)" % (tag, no, title[:60]))
        elif int(claim) != actual:
            fail("%s 第 %s 行:行号 %s ≠ 实际 %d(%r)" % (tag, no, claim, actual, title[:60]))
    return listed


listed_a = check_rows(rows_a, "索引 A", 1, 2)
listed_b = check_rows(rows_b, "索引 B", 2, 4)

for title in h2:
    if title not in listed_a:
        fail("索引 A 遗漏 `## ` 标题: %r" % title[:60])
for lineno, title in sus:
    if title not in listed_b:
        fail("索引 B 遗漏 Status Update: %r" % title[:60])
if len(set(listed_a)) != len(listed_a):
    fail("索引 A 存在重复标题 %d 处" % (len(listed_a) - len(set(listed_a))))
if len(set(listed_b)) != len(listed_b):
    fail("索引 B 存在重复标题 %d 处" % (len(listed_b) - len(set(listed_b))))

print(
    "[index-position] 索引行 %d · 一级节 A=%d/自述%d/实际%d · SU B=%d/自述%d/实际%d · 问题 %d"
    % (idx + 1, len(rows_a), claimed_sections, len(heads), len(rows_b), claimed_sus, len(sus), len(problems))
)
sys.exit(1 if problems else 0)
PY
rc=$?
if [[ $rc -ne 0 ]]; then
  echo "✗ 索引位置闸未通过(见上)" >&2
  exit 1
fi

echo "✓ CEP 索引闸通过(文末 + 三合一计数 + 行号逐行 + 标题覆盖)"
exit 0
