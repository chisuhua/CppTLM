#!/usr/bin/env bash
# scripts/test/cep_index_position_check.sh
# **索引位置闸**(per 台账 Entry #295 的 F-08 残余裁定 + 新纪律)
#
# 纪律(逐字):「**索引永久置于文末**;此后**新增**段落 SHALL 插在索引**之前** ⇒ ⇒
#            **新增内容永不使既有 `path:line` 锚点位移**;索引**每次追加后重生成**。」
#
# 判据(可机器判):
#   ① `docs/architecture/07-coherence-exploration-platform.md` 的 **§末 文档索引**段
#      必须是该文件的**最后一个** `## ` 级标题 ⇒ 即:**索引之后零个** `## ` 标题;
#   ② 索引头部自述的计数(**一级节数 / Status Update 数**)必须与实际**逐个相等**
#      ⇒ 防止「追加了 SU 却没更新索引」;
#   ③ 索引 A / 索引 B 的**行数**必须与自述计数一致(一级节数 == 索引 A 行数;
#      SU 数 == 索引 B 行数)。
#
# ⚠️ **为什么需要它**:索引生成器**不在仓内**(登记为 backlog)⇒ 索引靠**手工同步**
#    ⇒ 无闸则「追加后忘更新索引」与「把新段加到索引之后(破坏锚点稳定性)」都会**静默**发生。
#
# 用法:bash scripts/test/cep_index_position_check.sh
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

DOC="docs/architecture/07-coherence-exploration-platform.md"
problems=0

if [[ ! -f "$DOC" ]]; then
  echo "✗ 未找到 $DOC" >&2
  exit 2
fi

python3 - "$DOC" <<'PY'
import re, sys

path = sys.argv[1]
lines = open(path, encoding="utf-8").read().splitlines()
problems = 0

# ① 索引段必须是最后一个 `## ` 级标题
idx_positions = [i for i, l in enumerate(lines) if l.startswith("## ") and "§末 文档索引" in l]
if not idx_positions:
    print(f"✗ {path}: 未找到「## §末 文档索引」标题", file=sys.stderr)
    sys.exit(1)
if len(idx_positions) > 1:
    print(f"✗ {path}: 「§末 文档索引」标题出现 {len(idx_positions)} 次(应恰好 1 次)", file=sys.stderr)
    problems += 1
idx = idx_positions[0]
after = [(i + 1, l) for i, l in enumerate(lines[idx + 1:], start=idx + 1) if l.startswith("## ")]
if after:
    for lineno, title in after[:5]:
        print(f"✗ {path}:{lineno}: 索引之后仍有 `## ` 级标题 —— 违反「索引永久置于文末」纪律: {title[:60]}",
              file=sys.stderr)
    problems += 1

# ② 头部自述计数 vs 实际
text = "\n".join(lines)
m = re.search(r"本文件已 \*\*(\d+) 个一级节 / (\d+) 则 Status Update\*\*", text)
if not m:
    print(f"✗ {path}: 未找到索引头部的「N 个一级节 / M 则 Status Update」自述行", file=sys.stderr)
    sys.exit(1)
claimed_sections, claimed_sus = int(m.group(1)), int(m.group(2))

# ③ 实际计数:一级节 = 索引 A 的行数;SU 数 = 索引 B 的行数
def table_rows_after(header_prefix, start):
    rows = 0
    in_table = False
    for l in lines[start:]:
        if l.startswith(header_prefix):
            in_table = True
            continue
        if in_table:
            if l.startswith("|"):
                if not l.startswith("|---") and not re.match(r"^\|\s*#\s*\|", l):
                    rows += 1
            elif l.strip() == "":
                continue
            else:
                break
    return rows

a_start = next((i for i, l in enumerate(lines) if l.startswith("### 索引 A")), None)
b_start = next((i for i, l in enumerate(lines) if l.startswith("### 索引 B")), None)
if a_start is None or b_start is None:
    print(f"✗ {path}: 未找到「### 索引 A」/「### 索引 B」小节", file=sys.stderr)
    sys.exit(1)

rows_a = table_rows_after("### 索引 A", a_start)
rows_b = table_rows_after("### 索引 B", b_start)
if rows_a != claimed_sections:
    print(f"✗ {path}: 索引 A 行数 {rows_a} ≠ 自述「{claimed_sections} 个一级节」", file=sys.stderr)
    problems += 1
if rows_b != claimed_sus:
    print(f"✗ {path}: 索引 B 行数 {rows_b} ≠ 自述「{claimed_sus} 则 Status Update」", file=sys.stderr)
    problems += 1

print(f"[index-position] 索引行 {idx + 1} · 一级节 {rows_a}/{claimed_sections} · SU {rows_b}/{claimed_sus} · 问题 {problems}")
sys.exit(1 if problems else 0)
PY
rc=$?
if [[ $rc -ne 0 ]]; then
  echo "✗ 索引位置闸未通过(见上)" >&2
  exit 1
fi

echo "✓ CEP 索引位置闸通过(索引在文末 + 计数自洽)"
exit 0
