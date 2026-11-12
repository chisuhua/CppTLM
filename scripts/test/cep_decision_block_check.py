#!/usr/bin/env python3
"""scripts/test/cep_decision_block_check.py
   —— CEP 治理判据闸:**决策块计数快照(R-d)** + **残余登记双向对账(R-c)**

用途:把 `ADR-CEP-00` SU 2026-11-11 §⑤ 自认的两项「有登记、无牙齿」残留
     —— **R-c**(「一决策一文档化」残余登记 + 3 条触发条件**无机器检查**)
     —— **R-d**(决策块计数口径**三个互不相等的实现**)
     —— 落为**有牙齿的机器判据**。

判定口径(per 台账 Entry #359 D-1~D-5 + Entry #361 D-6~D-9 + 本 change 的 design.md):
  · **R-d 段** —— 22 份 ADR 的 §2 决策块计数,与**闸外快照**逐份比对。
  · **R-c 段** —— 双向对账:正向(磁盘集合)+ 反向(重述块)。

⚠️ **受控重复登记**(per design D-3 / Entry #361 ③ D-8 第 2 点):
   本文件的 §2 提取逻辑与 **C++** 版 `test/cep_adr_parser.hh:250` 的 `read_section`
   **构成受控重复**(Python 无法调用 C++ ⇒ 必有一份 Python 实现)。
   ⇒ 先例 = `test/cep_adr_parser.hh:169-173` 对 `no_inv_adr_files` 的双实现自登记。
   ⛔ **不改** C++ 版;两版口径必须一致(本文件为 **canonical 实现**,
      `ADR-CEP-00:431` 的「标题口径 4」= 本实现的**文档镜像**)。

⚠️ **期望值的唯一来源**(⛔ 禁自证):
   · R-c 的期望值 = **闸内源码常量**(D-5.1)
   · R-d 的期望值 = **闸外快照** `test/cep_decision_block_snapshot.json`(D-4)
   ⛔ 期望值**不得**从被检对象自身或 `ADR-CEP-NUMBER-MAP` 派生。

⚠️ **双根语义**(per design D-7):
   · **文档语料**(ADR 集合 / 重述块 / §2 计数)⇒ `--root`(影子树)
   · **被指物存在性**(仓内路径 / 台账 Entry / ADR 短名 glob / 符号定义式)
     ⇒ **真实仓根** `ROOT`(影子树不含台账与闸外文件 ⇒ 在影子根解析会恒红)
   · **快照读取** ⇒ **真实仓根**(快照是闸外的期望值载体,⛔ 不是语料)

用法:
  python3 scripts/test/cep_decision_block_check.py            # 校验(退出码 0 = 绿)
  python3 scripts/test/cep_decision_block_check.py --root DIR # 文档语料取 DIR(影子树)
  python3 scripts/test/cep_decision_block_check.py --list     # 只打印判据清单
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SNAPSHOT_REL = os.path.join("test", "cep_decision_block_snapshot.json")
LEDGER_REL = os.path.join(".rddf", "roadmap", "objectives", "obj-cep-platform.md")
SNAPSHOT_SCHEMA = "cep-decision-block-snapshot-v1"

# ── 红因签名(与 design.md D-8 一一对应;发布即冻结)──────────────────────────
SIG_COUNT_MISMATCH = "决策块计数快照偏离"
SIG_BAD_LEVEL = "决策块标题非 ### 级"
SIG_BAD_NUMBER = "决策块编号非法"
SIG_DANGER_FENCE = "围栏内含行首 ##"
SIG_SNAPSHOT_BAD = "快照文件非法"
SIG_MISSING_ADR = "三份 ADR 缺失"
SIG_SPLIT_ADR = "存在同号拆出件"
SIG_RESIDUAL_MISS = "重述块缺项"
SIG_RESIDUAL_FMT = "重述块格式不符"
SIG_TRIGGER_FMT = "触发条件表结构不符"
SIG_REF_UNRESOLVED = "引用解析缺失"

# ── D-3.1 四步口径(per D-1)─────────────────────────────────────────────────
SEC2_RE = re.compile(r"^## 2\b.*$", re.M)
NEXT_SEC_RE = re.compile(r"^##\s", re.M)
DECISION_RE = re.compile(r"^(#{2,6})\s*决策\s*(.*)$")
NUM_RE = re.compile(r"\s*(\d+(?:\.\d+)?)")
# D-3.2 块 ID 的终止符集(实测形态:「决策 1:」「决策 2(精简):」「决策 2.1」)
TERM_CHARS = set(" \t:：(（-—、,，。/")


def strip_fences(text):
    """D-3.1 ①:剥离**全文**的 ``` 围栏块(内容不可见)"""
    out = []
    in_fence = False
    for line in text.splitlines():
        if line.startswith("```"):
            in_fence = not in_fence
            continue
        if not in_fence:
            out.append(line)
    return "\n".join(out)


def section2(text):
    """D-3.1 ②:取 §2 章范围(§2 标题之后 → 下一个 `^##\\s` 之前);无 §2 ⇒ None"""
    m = SEC2_RE.search(text)
    if not m:
        return None
    rest = text[m.end():]
    m2 = NEXT_SEC_RE.search(rest)
    return rest[: m2.start()] if m2 else rest


def analyze_section2(sec):
    """D-3.1 ③④ + D-3.2 + D-3.3①②:返回 (计数, [(签名, 细节), ...])"""
    count = 0
    reds = []
    for line in sec.splitlines():
        m = DECISION_RE.match(line)
        if not m:
            continue
        level, rest = m.group(1), m.group(2)
        if len(level) != 3:
            reds.append((SIG_BAD_LEVEL, f"标题层级 h{len(level)}: {line.strip()[:60]}"))
            continue
        mm = NUM_RE.match(rest)
        if not mm:
            reds.append((SIG_BAD_NUMBER, f"编号非 ASCII 十进制: {line.strip()[:60]}"))
            continue
        nxt = rest[mm.end(): mm.end() + 1]
        if nxt and nxt not in TERM_CHARS:
            reds.append(
                (SIG_BAD_NUMBER, f"编号非全匹配(后继字符 {nxt!r}): {line.strip()[:60]}")
            )
            continue
        count += 1
    return count, reds


def fence_aware_lines(text):
    """返回 (逐行是否在围栏内, 围栏块列表[(首行号, 末行号, 内容行)])"""
    in_fence = False
    fence_flags = []
    blocks = []
    start = 0
    content = []
    for i, line in enumerate(text.splitlines()):
        if line.startswith("```"):
            if in_fence:
                in_fence = False
                blocks.append((start, i, content))
                content = []
            else:
                in_fence = True
                start = i
            fence_flags.append(True)
            continue
        fence_flags.append(in_fence)
        if in_fence:
            content.append((i, line))
    if in_fence:  # 未闭合围栏
        blocks.append((start, len(fence_flags) - 1, content))
    return fence_flags, blocks


def section2_line_range(text):
    """围栏感知地定位 §2 行区间(围栏内的 `## ` **不**算章节边界)"""
    flags, _ = fence_aware_lines(text)
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if flags[i]:
            continue
        if re.match(r"^## 2\b", line):
            start = i
            break
    if start is None:
        return None
    for j in range(start + 1, len(lines)):
        if flags[j]:
            continue
        if re.match(r"^##\s", lines[j]):
            return (start, j)
    return (start, len(lines))


def danger_fences(text):
    """D-3.3 ③:§2 范围内、内容含行首 `## ` 的围栏块"""
    rng = section2_line_range(text)
    if rng is None:
        return []
    lo, hi = rng
    _, blocks = fence_aware_lines(text)
    hits = []
    for start, end, content in blocks:
        if end < lo or start > hi:
            continue
        for lineno, line in content:
            if re.match(r"^##\s", line):
                hits.append((lineno + 1, line.strip()[:60]))
                break
    return hits


def load_snapshot():
    """D-4:读快照并校验其自身健康度 ⇒ (快照, [(签名, 细节), ...])"""
    reds = []
    path = os.path.join(ROOT, SNAPSHOT_REL)
    if not os.path.isfile(path):
        return None, [(SIG_SNAPSHOT_BAD, f"快照文件不存在: {SNAPSHOT_REL}")]
    try:
        with open(path, encoding="utf-8") as fh:
            snap = json.load(fh)
    except (OSError, ValueError) as exc:
        return None, [(SIG_SNAPSHOT_BAD, f"快照无法解析: {exc}")]
    if snap.get("schema") != SNAPSHOT_SCHEMA:
        reds.append((SIG_SNAPSHOT_BAD, f"schema 非 {SNAPSHOT_SCHEMA}: {snap.get('schema')!r}"))
    files = snap.get("files")
    if not isinstance(files, dict):
        return None, reds + [(SIG_SNAPSHOT_BAD, "files 字段缺失或非对象")]
    if snap.get("count") != len(files):
        reds.append(
            (SIG_SNAPSHOT_BAD, f"count({snap.get('count')}) != len(files)({len(files)})")
        )
    total = sum(v for v in files.values() if isinstance(v, int))
    if snap.get("total") != total:
        reds.append((SIG_SNAPSHOT_BAD, f"total({snap.get('total')}) != sum(files)({total})"))
    auth = str(snap.get("authorized_by", ""))
    m = re.match(r"^Entry #(\d+)$", auth)
    if not m:
        reds.append((SIG_SNAPSHOT_BAD, f"authorized_by 非 'Entry #N' 形态: {auth!r}"))
    else:
        ledger = os.path.join(ROOT, LEDGER_REL)
        found = False
        if os.path.isfile(ledger):
            with open(ledger, encoding="utf-8") as fh:
                found = re.search(rf"^### Entry #{m.group(1)}\b", fh.read(), re.M) is not None
        if not found:
            reds.append((SIG_SNAPSHOT_BAD, f"authorized_by 指向的台账 Entry 不存在: {auth}"))
    return snap, reds


def adr_files(corpus_root):
    adr_dir = os.path.join(corpus_root, "docs", "adr")
    if not os.path.isdir(adr_dir):
        return None
    return sorted(
        os.path.join(adr_dir, name)
        for name in os.listdir(adr_dir)
        if name.startswith("ADR-CEP-") and name.endswith(".md")
    )


# ── R-d 段 ────────────────────────────────────────────────────────────────
def check_rd(corpus_root):
    """R-d:快照逐份比对 + 两条显式红 + 围栏危险标记。

    职责边界:本段遍历**快照已登记的文件**(22 份)⇒ 语料里**新增**的 ADR 文件
    不在本段职责内(由 R-c 的「无同号拆出件」+ `cep_adr_structural_check.sh` 的
    分区完整性负责)⇒ 两段职责解耦,变异红因唯一。
    """
    results = []
    snap, reds = load_snapshot()
    results.extend(reds)
    if snap is None:
        return results
    files = snap["files"]
    for basename in sorted(files):
        path = os.path.join(corpus_root, "docs", "adr", basename)
        if not os.path.isfile(path):
            results.append(
                (
                    SIG_COUNT_MISMATCH,
                    f"{basename}: 文件缺失(快照期望 {files[basename]} 块)",
                )
            )
            continue
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        sec = section2(strip_fences(text))
        if sec is None:
            results.append(
                (SIG_COUNT_MISMATCH, f"{basename}: §2 章缺失(快照期望 {files[basename]} 块)")
            )
            continue
        count, file_reds = analyze_section2(sec)
        results.extend((sig, f"{basename}: {detail}") for sig, detail in file_reds)
        expected = files[basename]
        if count != expected:
            results.append(
                (
                    SIG_COUNT_MISMATCH,
                    f"{basename}: 期望 {expected} 块, 实得 {count} 块, 差值 {count - expected:+d}",
                )
            )
        for lineno, snippet in danger_fences(text):
            results.append(
                (SIG_DANGER_FENCE, f"{basename}:{lineno}: 围栏内容含行首 '## ': {snippet}")
            )
    return results


def check_snapshot_totals(corpus_root):
    """AC-2 / AC-3 的机器证据:逐份计数合计"""
    snap, reds = load_snapshot()
    if snap is None:
        return None, reds
    total = 0
    per_file = {}
    for basename in sorted(snap["files"]):
        path = os.path.join(corpus_root, "docs", "adr", basename)
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        sec = section2(strip_fences(text))
        if sec is None:
            continue
        count, _ = analyze_section2(sec)
        per_file[basename] = count
        total += count
    return (per_file, total), reds


# ── R-c 段的闸内常量(per design D-5.1 —— 期望值的唯一来源)────────────────
CARRIER_ADR = "ADR-CEP-00-platform-charter-and-governance.md"
CANONICAL = [
    "ADR-CEP-01-fabric-collective-plane.md",
    "ADR-CEP-02-coherence-barrier-semantics-clarification.md",
    "ADR-CEP-03-fabric-collective-plane-internals.md",
]
RESIDUAL_TOKENS = ["ADR-CEP-01", "ADR-CEP-02", "ADR-CEP-03"]
SU_RESIDUAL_DATE = "2026-11-12"   # C3-C 新 SU(承载 `### ⑥` 重述块)
SU_TRIGGER_DATE = "2026-11-11"    # 既有 SU(承载 `### ②` 的 3 条触发条件表)
TOKEN_RE = re.compile(r"ADR-CEP-0[123](?![0-9])")
SPLIT_RE = re.compile(r"^ADR-CEP-0[123](?![0-9])")
PATH_RE = re.compile(r"^[\w./-]+\.(?:md|hh|cc|h|py|sh|json|tsv|yaml|yml|txt)$")
ENTRY_RE = re.compile(r"^Entry #(\d+)$")
ADR_SHORT_RE = re.compile(r"^ADR-CEP-[0-9A-Z]+$")
SYMBOL_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\(\)$")


def find_su_block(text, date_str, sub_heading):
    """定位 SU(标题含 date_str)与其下 `### <sub_heading>` 块 ⇒ (su_range, block_range)

    su_range   = (SU 标题行号, SU 结束行号)   —— SU 结束 = 下一个 `^## `
    block_range= (`###` 标题行号, 块结束行号) —— 块结束 = 下一个 `^#{2,3}\\s`
    """
    lines = text.splitlines()
    su_start = None
    for i, line in enumerate(lines):
        if line.startswith("## Status Update") and date_str in line:
            su_start = i
            break
    if su_start is None:
        return None, None
    su_end = len(lines)
    for j in range(su_start + 1, len(lines)):
        if re.match(r"^##\s", lines[j]):
            su_end = j
            break
    sub_re = re.compile(r"^###\s+" + re.escape(sub_heading))
    sub_start = None
    for j in range(su_start + 1, su_end):
        if sub_re.match(lines[j]):
            sub_start = j
            break
    if sub_start is None:
        return (su_start, su_end), None
    block_end = su_end
    for j in range(sub_start + 1, su_end):
        if re.match(r"^#{2,3}\s", lines[j]):
            block_end = j
            break
    return (su_start, su_end), (sub_start, block_end)


def read_text(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


# ── R-c 段 ────────────────────────────────────────────────────────────────
def check_rc_forward(corpus_root):
    """D-5.2 正向:三份精确 basename 存在 + **无同号拆出件**(语料 = --root)"""
    reds = []
    adr_dir = os.path.join(corpus_root, "docs", "adr")
    if not os.path.isdir(adr_dir):
        return [(SIG_MISSING_ADR, f"语料缺少 docs/adr: {corpus_root}")]
    names = sorted(
        n for n in os.listdir(adr_dir) if n.startswith("ADR-CEP-") and n.endswith(".md")
    )
    for canon in CANONICAL:
        if canon not in names:
            reds.append((SIG_MISSING_ADR, f"{canon}: 语料内缺失"))
    for name in names:
        if SPLIT_RE.match(name) and name not in CANONICAL:
            reds.append((SIG_SPLIT_ADR, f"{name}: 同号拆出件(不属三份精确 basename)"))
    return reds


def check_rc_residual(corpus_root):
    """D-5.3 反向:新 SU 的 `### ⑥` 重述块(恰 3 bullet · 每行唯一 token · 三 token 各 ≥1)"""
    path = os.path.join(corpus_root, "docs", "adr", CARRIER_ADR)
    if not os.path.isfile(path):
        return [(SIG_RESIDUAL_FMT, f"{CARRIER_ADR}: 语料内缺失")]
    text = read_text(path)
    lines = text.splitlines()
    _, block = find_su_block(text, SU_RESIDUAL_DATE, "⑥")
    if block is None:
        return [
            (
                SIG_RESIDUAL_FMT,
                f"{CARRIER_ADR}: 定位不到 SU {SU_RESIDUAL_DATE} 的 '### ⑥' 重述块",
            )
        ]
    bs, be = block
    bullets = [l for l in lines[bs + 1: be] if l.startswith("- ")]
    reds = []
    if len(bullets) != 3:
        reds.append((SIG_RESIDUAL_FMT, f"重述块 bullet 数 = {len(bullets)}(期望恰 3)"))
    counts = {t: 0 for t in RESIDUAL_TOKENS}
    for bullet in bullets:
        found = TOKEN_RE.findall(bullet)
        if len(found) != 1:
            reds.append(
                (
                    SIG_RESIDUAL_FMT,
                    f"重述块 bullet 的完整 token 数 = {len(found)}(期望 1): {bullet.strip()[:56]}",
                )
            )
            continue
        counts[found[0]] = counts.get(found[0], 0) + 1
    for token in RESIDUAL_TOKENS:
        if counts.get(token, 0) < 1:
            reds.append((SIG_RESIDUAL_MISS, f"重述块缺完整编号 token: {token}"))
    return reds


def resolve_reference(span, cell):
    """D-6:4 类引用的可解析判定(⛔ 被指物存在性恒取**真实仓根**)"""
    if ADR_SHORT_RE.match(span):                                   # (c) ADR 短名
        import glob as _glob
        return bool(_glob.glob(os.path.join(ROOT, "docs", "adr", span + "-*.md")))
    m = ENTRY_RE.match(span)                                       # (b) 台账 Entry
    if m:
        ledger = os.path.join(ROOT, LEDGER_REL)
        if not os.path.isfile(ledger):
            return False
        return (
            re.search(rf"^### Entry #{m.group(1)}\b", read_text(ledger), re.M) is not None
        )
    if PATH_RE.match(span):                                        # (a) 仓内路径
        return os.path.exists(os.path.join(ROOT, span))
    if SYMBOL_RE.match(span):                                      # (d) 符号
        name = span[:-2]
        paths = [s for s in re.findall(r"`([^`]+)`", cell) if PATH_RE.match(s)]
        if not paths:
            return False
        target = os.path.join(ROOT, paths[-1])
        if not os.path.isfile(target):
            return False
        def_re = re.compile(
            r"^\s*[\w:<>,\s\*&~]*?\b" + re.escape(name) + r"\s*\([^)]*\)\s*(?:const\s*)?(?:\{|;|->)",
            re.M,
        )
        return def_re.search(read_text(target)) is not None
    return False


def check_rc_trigger(corpus_root):
    """D-5.4 触发条件:SU 2026-11-11 的 `### ②` 之下第一张表(表在**语料根**;引用解析取**真实仓根**)"""
    path = os.path.join(corpus_root, "docs", "adr", CARRIER_ADR)
    if not os.path.isfile(path):
        return [(SIG_TRIGGER_FMT, f"{CARRIER_ADR}: 语料内缺失")]
    text = read_text(path)
    lines = text.splitlines()
    _, block = find_su_block(text, SU_TRIGGER_DATE, "②")
    if block is None:
        return [
            (
                SIG_TRIGGER_FMT,
                f"{CARRIER_ADR}: 定位不到 SU {SU_TRIGGER_DATE} 的 '### ②' 触发条件段",
            )
        ]
    bs, be = block
    table = []
    started = False
    for line in lines[bs + 1: be]:
        if line.strip().startswith("|"):
            table.append(line)
            started = True
        elif started:
            break
    if not table:
        return [(SIG_TRIGGER_FMT, "'### ②' 之下无 Markdown 表")]
    header = [c.strip() for c in table[0].strip().strip("|").split("|")]
    col = next((i for i, h in enumerate(header) if "判定方式" in h), None)
    if col is None:
        return [(SIG_TRIGGER_FMT, f"表头无「判定方式」列: {header}")]
    rows = []
    for line in table[2:]:                       # 跳表头 + 分隔行
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) > col:
            rows.append(cells[col])
    if len(rows) != 3:
        return [(SIG_TRIGGER_FMT, f"触发条件数据行数 = {len(rows)}(期望恰 3)")]
    reds = []
    for idx, cell in enumerate(rows, 1):
        spans = re.findall(r"`([^`]+)`", cell)
        if not spans:
            reds.append((SIG_REF_UNRESOLVED, f"第 {idx} 行「判定方式」列无 inline code span"))
            continue
        if not any(resolve_reference(s, cell) for s in spans):
            reds.append(
                (SIG_REF_UNRESOLVED, f"第 {idx} 行「判定方式」列无可解析引用: {spans}")
            )
    return reds


# ── 主流程 ────────────────────────────────────────────────────────────────
def run(corpus_root, list_only=False):
    if list_only:
        print("CEP 治理判据闸(cep_decision_block_check)判据清单:")
        print("  [R-d] 22 份 ADR 的 §2 决策块计数 == 快照逐份相等(D-1 四步 + D-3.2 块 ID 全匹配)")
        print("  [R-d] 显式红①:决策标题层级 ≠ ###")
        print("  [R-d] 显式红②:决策编号非 ASCII 十进制 / 非全匹配")
        print("  [R-d] 围栏危险标记:§2 内围栏内容含行首 '## '")
        print("  [R-d] 快照健康度:schema / count / total / authorized_by(台账 Entry 必须存在)")
        print("  [R-c] 正向:三份精确 basename 存在 + 无同号拆出件")
        print("  [R-c] 反向:新 SU 的 '### ⑥' 重述块(恰 3 bullet · 每行唯一 token)")
        print("  [R-c] 触发条件:SU '### ②' 之下第一张表(恰 3 数据行 · 4 类引用可解析)")
        return 0

    reds = check_rd(corpus_root)
    evidence, ev_reds = check_snapshot_totals(corpus_root)
    reds.extend(ev_reds)
    reds.extend(check_rc_forward(corpus_root))
    reds.extend(check_rc_residual(corpus_root))
    reds.extend(check_rc_trigger(corpus_root))

    if evidence is not None:
        per_file, total = evidence
        print(f"  · 决策块计数合计 = {total}(逐份 {len(per_file)} 份)")
    if reds:
        for sig, detail in reds:
            print(f"  ❌ {sig}: {detail}")
        print(f"  ❌ CEP 治理判据闸: {len(reds)} 项判红")
        return 1
    print("  ✅ CEP 治理判据闸:全部判据通过")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="CEP 治理判据闸(R-d 决策块计数 / R-c 双向对账)")
    parser.add_argument(
        "--root",
        default=ROOT,
        help="文档语料根(影子树);⛔ 被指物存在性 / 台账 / 快照恒取真实仓根",
    )
    parser.add_argument("--list", action="store_true", help="只打印判据清单")
    args = parser.parse_args(argv)
    corpus_root = os.path.abspath(args.root)
    if not os.path.isdir(os.path.join(corpus_root, "docs", "adr")):
        print(f"  ❌ 文档语料根缺少 docs/adr: {corpus_root}")
        return 2
    return run(corpus_root, list_only=args.list)


if __name__ == "__main__":
    sys.exit(main())
