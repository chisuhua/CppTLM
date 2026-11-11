#!/usr/bin/env python3
# scripts/cep/generate_doc_index.py
# CEP 架构文档文末索引的**生成器**(per 台账 Entry #342 / change `cpptlm-cep-gate-strength-tooling` 的 R-a)
#
# 为什么存在:`07 号` 的索引头自述「本索引**由脚本生成**」,但生成器**长期不在仓内**
#   ⇒ 索引靠**手工同步** ⇒ 实测已腐化(自述 23 个一级节 vs 实际 28;索引 A 行号 10/23 错)。
#   ⛔ 本工具**不是**第三条治理机制 —— 它兑现的是 `07 号` §末索引**早已声明**的生成方式。
#
# 生成范围(**只**这三块;⛔ `### 索引 C` 不动 —— 它含人工语义「已迁出 / 用途」):
#   ① 索引头部的用途行(自述计数行)
#   ② `### 索引 A` · 一级节 = **全部** `## ` 标题 + **实测行号**
#   ③ `### 索引 B` · Status Update = **全部** `## … Status Update …` 标题 + 日期 / 台账 / 实测行号
#
# 用法:
#   python3 scripts/cep/generate_doc_index.py              # 打印重生成后的全文到 stdout(不写盘)
#   python3 scripts/cep/generate_doc_index.py --write      # 就地重写文末索引段
#   python3 scripts/cep/generate_doc_index.py --check      # 已同步 ⇒ rc=0;漂移 ⇒ rc=1(CI 用)
#
# ⚠️ **「外来内容」护栏**(2026-11-12,per 台账 Entry #343):「索引标题 → 索引 C」区间内
#    若存在**非生成块形态**的行 ⇒ **拒绝重写**(rc=1)并打印行号 ⇒ 防止 `--write`
#    把误置在区间内的正文**静默删除**(实测过的数据丢失形态)。处置 = ① 移到索引**之前**
#    · ② 或手工并入索引块。`--check` 与 `--write` **同口径**都拦。
#
# 作者: CppTLM Team / 日期: 2026-11-12
import argparse
import re
import sys
from pathlib import Path

DOC_DEFAULT = Path("docs/architecture/07-coherence-exploration-platform.md")
INDEX_TITLE_KEY = "§末 文档索引"
TOC_C_PREFIX = "### 索引 C"
SU_MARK = "Status Update"
DATE_RE = re.compile(r"(\d{4}-\d{2}-\d{2})")
LEDGER_RE = re.compile(r"台账 Entry #(\d+)")


def h2_headings(lines):
    """(0-based 行索引, 标题正文) —— 只认 `## `,⛔ 不含 `### `。"""
    return [
        (i, l[3:].strip())
        for i, l in enumerate(lines)
        if l.startswith("## ") and not l.startswith("### ")
    ]


def cell(s):
    if "|" in s:
        raise SystemExit("✗ 标题含 `|`,无法安全写入 Markdown 表格: %r" % s)
    return s


# ── 「外来内容」护栏(2026-11-12,per 台账 Entry #343 = verifier 报告 F-2)────────
# ⚠️ **为什么需要**:`--write` 重写的是「索引标题 → 索引 C」**整段区间** ⇒ 该区间内
#    任何**不属于生成块形态**的行都会被**静默删除**(实测:正文被删,索引却仍留一行
#    指向它 ⇒ 索引声称存在、正文没了)。⛔ 新增段落本就该插在索引**之前**
#    (per 索引纪律),落在区间内属**误置** ⇒ 本护栏**拒绝写盘**而不是默默吃掉。
#    白名单 = 生成块可能产生的行形态(空行 / 表格行 / 引用行 / `### ` 小节标题)。
#    双向自洽:生成块自身也必须落在白名单内(否则闸与生成器口径会分裂)。
def is_generatable(line):
    s = line.strip()
    if s == "":
        return True
    return s.startswith("|") or s.startswith(">") or s.startswith("### ")


def find_foreign(lines, idx_i, c_i):
    """区间内不属于生成块形态的行 ⇒ [(行号, 原文)]。"""
    return [(i + 1, lines[i]) for i in range(idx_i + 1, c_i) if not is_generatable(lines[i])]


def assert_block_generatable(block):
    bad = [l for l in block if not is_generatable(l)]
    if bad:
        raise SystemExit(
            "✗ 生成块含非白名单行形态(闸与生成器口径会分裂): %r" % bad[:3]
        )


def build_block(heads, sus):
    first_date = None
    for _, title in sus:
        m = DATE_RE.search(title)
        if m:
            first_date = m.group(1)
            break
    out = [""]
    out.append(
        "> **用途**:本文件已 **%d 个一级节 / %d 则 Status Update** —— **全部** `## ` 级标题与**全部** "
        "Status Update 逐条列于下表(索引 A / 索引 B 由 `scripts/cep/generate_doc_index.py` **全量机械生成**,"
        "含**实测行号列**)⇒ 故设本索引(以**零影响**既有 `path:line` 锚点 —— ArchForge 对齐清单按行号锚定)"
        % (len(heads), len(sus))
    )
    out.append("")
    out.append(
        "> ⚠️ **新纪律(per 台账 Entry #295,逐字)**:本索引**永久置于文末**;此后**新增段落 SHALL 插在本索引之前** "
        "⇒ **新增内容永不使既有 `path:line` 锚点位移**;⚠️ **最新 SU 在本索引之前**(⛔ 勿在本文末找最新段)。"
    )
    out.append("")
    out.append(
        "> ⚠️ **本索引由脚本生成(零手抄)⇒ 每次追加后重生成**;生成器 = `scripts/cep/generate_doc_index.py`"
        "(2026-11-12 落仓,per 台账 Entry #342)⇒ `--write` 就地重写 / `--check` 判漂移。"
    )
    out.append("")
    out.append(
        "> ⚠️ **2026-11-12 起**:索引 A / 索引 B **全量列出**(⛔ 不再有「另有 N 个未列入」的 `## ` 标题);"
        "自述数 == 索引行数 == 文件**实际** `## ` / SU 数(`cep_index_position_check.sh` 三合一判据 + 行号逐行判据)。"
        "⚠️ **待人工判断的导航标注不在索引 A / B**(索引 A/B 是机械视图)⇒ 见索引 C 与 `§11` / `§12` 的正文说明。"
    )
    out.append("")
    out.append("### 索引 A · 一级节(%d)" % len(heads))
    out.append("")
    out.append("| # | 章节 | 行 |")
    out.append("|---|---|---|")
    for n, (i, title) in enumerate(heads, 1):
        out.append("| %d | %s | %d |" % (n, cell(title), i + 1))
    out.append("")
    out.append(
        "### 索引 B · Status Update(%d%s)"
        % (len(sus), (" · %s 起" % first_date) if first_date else "")
    )
    out.append("")
    out.append("| # | 日期 | 主题 | 台账 | 行 |")
    out.append("|---|---|---|---|---|")
    for n, (i, title) in enumerate(sus, 1):
        dm = DATE_RE.search(title)
        lm = LEDGER_RE.search(title)
        out.append(
            "| %d | %s | %s | %s | %d |"
            % (
                n,
                dm.group(1) if dm else "—",
                cell(title),
                ("Entry #%s" % lm.group(1)) if lm else "—",
                i + 1,
            )
        )
    out.append("")
    return out


def locate(lines):
    """返回 (索引标题行索引, 索引 C 行索引);不满足则报错退出。"""
    heads = h2_headings(lines)
    idx = [i for i, t in heads if INDEX_TITLE_KEY in t]
    if len(idx) != 1:
        raise SystemExit(
            "✗ 期望恰好 1 个含「%s」的 `## ` 标题,实得 %d 个" % (INDEX_TITLE_KEY, len(idx))
        )
    idx_i = idx[0]
    c_i = next(
        (i for i in range(idx_i + 1, len(lines)) if lines[i].startswith(TOC_C_PREFIX)), None
    )
    if c_i is None:
        raise SystemExit("✗ 未找到 `%s` 小节(索引 C 必须存在且位于索引 A/B 之后)" % TOC_C_PREFIX)
    return idx_i, c_i


def render(lines):
    idx_i, c_i = locate(lines)
    heads = h2_headings(lines)
    sus = [(i, t) for i, t in heads if SU_MARK in t]
    block = build_block(heads, sus)
    assert_block_generatable(block)
    new_lines = lines[: idx_i + 1] + block + lines[c_i:]
    return "\n".join(new_lines)


def main(argv):
    ap = argparse.ArgumentParser(description="CEP 架构文档文末索引生成器(索引 A/B 全量)")
    ap.add_argument("--doc", default=str(DOC_DEFAULT), help="目标文档(默认 %s)" % DOC_DEFAULT)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--write", action="store_true", help="就地重写索引段")
    g.add_argument("--check", action="store_true", help="已同步 ⇒ rc=0;漂移 ⇒ rc=1")
    args = ap.parse_args(argv)

    path = Path(args.doc)
    if not path.is_file():
        print("✗ 未找到 %s" % path, file=sys.stderr)
        return 2
    text = path.read_text(encoding="utf-8")
    lines = text.split("\n")
    idx_i, c_i = locate(lines)

    # ── 护栏:区间内的外来内容 ⇒ 拒绝(写盘会静默删除它)──────────────────────
    foreign = find_foreign(lines, idx_i, c_i)
    if foreign:
        print(
            "✗ %s:「## §末 文档索引」与「### 索引 C」之间存在 **%d 行外来内容** ⇒ 拒绝重写"
            % (path, len(foreign)),
            file=sys.stderr,
        )
        for lineno, raw in foreign[:10]:
            print("    %d: %s" % (lineno, raw.strip()[:90]), file=sys.stderr)
        print(
            "  · `--write` 会**静默删除**该区间的一切非生成内容 ⇒ 必须先处置。",
            file=sys.stderr,
        )
        print(
            "  · 处置:① 把该段移到索引**之前**(per 索引纪律)· ② 或手工并入索引块。",
            file=sys.stderr,
        )
        return 1

    new = render(lines)

    if args.check:
        if new == text:
            print("[index-gen] ✓ %s 索引段已同步" % path)
            return 0
        print("[index-gen] ✗ %s 索引段已漂移 ⇒ 跑 `--write` 重生成" % path, file=sys.stderr)
        return 1
    if args.write:
        if new == text:
            print("[index-gen] ✓ 无变化(幂等):%s" % path)
            return 0
        path.write_text(new, encoding="utf-8")
        print("[index-gen] ✅ 已重写索引段:%s" % path)
        return 0
    sys.stdout.write(new)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
