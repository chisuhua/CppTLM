#!/usr/bin/env python3
# scripts/test/cep_dod_reading_check.py
#   —— C4「DoD 读法口径」判据闸(单一权威声明处 + 指针闭合)
#
# 用法:
#   python3 scripts/test/cep_dod_reading_check.py [--root DIR] [--list]
#     --root DIR   语料根(默认 = 由本文件位置推导的仓根)  ⇒ 影子树变异时指向影子根
#     --list       只打印判据清单
#
# 判据(见 openspec/changes/cpptlm-cep-dod-reading-consistency/design.md §D-5):
#   [S1] 旧文本(A3_OLD)**零命中**
#   [S2] 4 条整行锚(A1~A4):**整行全等** + 唯一承载文件 + 该文件内行次数 == 1
#   [S3] 缺失归因:A1 → `append-only 违规` / `新文本缺失`；A2 → `作用域子句缺失` / `新文本缺失`
#   [S4] A2 行的**作用域要素**(5 个 token 全含)
#   [S5] `07 号` 内**每一行**含禁令 token ⇒ 该行至少含 1 个作用域 token(否则 `绝对禁令复活`)
#   [S6] 新文本面(SU 块 + 手册行 + 台账 `[C4 · ...]` 行)**0 命中** 5 条禁表述
#   [S7] 指针:`## Status Update 2026-11-13` 在 `07 号` 恰 1 + 两处指针都含该 token + 取代指针存在
#
# 输出契约:`FAIL <check_id> <签名> :: <细节>`(⛔ `rc != 0` 不单独构成红因)
# 退出码:`0` 全绿 · `1` 有 FAIL · `2` 语料缺失(⛔ 不与 1 混用)
#
# ⚠️ **期望值载体不是语料**:本文件的常量写成 `NAME = "..."` ⇒ **整行不等于**锚文本
#    ⇒ 不构成第二命中。⛔ 不得改用「排除自身路径」实现(排除式会连带屏蔽真回归)。
# ⚠️ **单根语义**:本闸不解析被指物存在性(不查仓内路径 / 台账 Entry)⇒ 所有读取一律 `--root`。
#    与 `cep_decision_block_check.py` 的双根语义是**有意差异**(design §D-7)。
import argparse
import os
import sys

ROOT_DEFAULT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

DOC07 = "docs/architecture/07-coherence-exploration-platform.md"
GUIDE = "docs/guide/cep-usage.md"
OBJ = ".rddf/roadmap/objectives/obj-cep-platform.md"

# ── 4 条整行锚(单源 = design.md §D-2 / §D-8 的 fenced block;⛔ 手抄)─────────
A1 = '  - ⛔ **B3b-2 不得改写为「已达成」** —— 「DoD 降格后仍宣称达成」是 architect 2026-11-04 裁决**明确禁止**的失败模式(per `07 号` Status Update 2026-11-04 ①);B3b-1 的 ✅ **仅**指「规范可读且四要件全满足的 2 族」'
A2 = '- **作用域(唯一权威定义)**:⛔ 禁止的形态 = 「**不带 N/M 分解与逐族锚点的 ✅**」(即把 B3b-2 整体登记为达成而不给 N/M 分解与逐族机器可查锚点);**N = 0 时一律 ⏭,永不 ✅**;原「整行改写为已达成」读法仅在**三条件同时成立**时适用 —— N ≥ 1 且带**完整 N/M 分解 + 逐族锚点**的登记形态 ⛔ **不适用**该禁令。'
A3 = '3. ⛔ **不得**把 B3b-2 整行改写为「已达成」—— 「DoD 降格后仍宣称达成」是明确禁止的失败模式。📌 **本条作用域已重裁**(per 台账 Entry #373):作用域与例外**见 `07 号` 的 `Status Update 2026-11-13` ②**(⛔ 本行不复制作用域定义)。⚠️ 现行登记口径仍 = 口径 ②(挂起);口径 ④ **未 ratify**。⛔ **不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文**(per 台账 Entry #264 / Entry #284 的先例形态)'
A3_OLD = '3. ⛔ **不得**把 B3b-2 整行改写为「已达成」—— 「DoD 降格后仍宣称达成」是明确禁止的失败模式。⚠️ **本条作用域待重裁**(per 台账 Entry #332 ⑧ MUST-FIX-2 ①):重裁后禁的是「**不带 N/M 分解与逐族锚点的 ✅**」;**重裁落地前按现行绝对禁令执行** ⇒ ⛔ 重裁前**不得**以任何形式登记为达成。⛔ **本条最终口径以「口径 ④ 本体新 SU」(Entry #332 ⑪ 落点)为准**;新 SU 落地后本条**改为指针**,⛔ **不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文**(per 台账 Entry #264 / Entry #284 的先例形态)'
A4 = '- **丁否决的适用结果登记(per 台账 `Entry #332` ⑦;⛔ 不回改 `Entry #249` ⑤ 的五选项表)**:丁 = 「B2 形态:6/6 = 2 有引擎 + 4 授权⏭」,否决理由 = 「把外部许可条件计入 DoD 达成」;口径 ④(N ≥ 1 + M 授权⏭)与丁**同型** ⇒ 适用结果 = **丁的否决对口径 ④ 继续有效**,其约束形态已由本则 ② 收敛(N = 0 时一律 ⏭;N ≥ 1 须带完整 N/M 分解 + 逐族锚点);⛔ 本则**不**使口径 ④ 生效(仍受 ratify 门约束)。'

ANCHORS = [("A1", A1, OBJ), ("A2", A2, DOC07), ("A3", A3, GUIDE), ("A4", A4, DOC07)]

SCAN_DIRS = ["docs", ".rddf/roadmap", "test", "scripts"]
SCAN_EXTS = {".md", ".py", ".sh", ".json", ".cc", ".hh", ".txt"}

# ── 签名表(design §D-4 · 9 条)─────────────────────────────────────────────
SIG_OLD = "旧文本仍存在"
SIG_DUP = "新文本非唯一命中"
SIG_MISS = "新文本缺失"
SIG_SCOPE = "作用域子句缺失"
SIG_BAN = "绝对禁令复活"
SIG_APPEND = "append-only 违规"
SIG_PHRASE = "禁表述复活"
SIG_PTR = "指针目标缺失"
SIG_REPL = "取代指针缺失"

# ── S4 作用域要素 / S5 禁令与作用域 token ───────────────────────────────────
SCOPE_ELEMS = ["不带 N/M 分解与逐族锚点", "N = 0", "⏭", "整行改写", "不适用"]
BAN_TOKENS_07 = ["不得改写为「已达成」", "整行改写为已达成"]
SCOPE_TOKENS = ["N/M 分解", "逐族锚点", "不适用"]

# ── S6 5 条禁表述(字面;⛔ 语义分类)─────────────────────────────────────────
BANNED_PHRASES = [
    ("禁表述 1", ["全 ⏭ 计入达成"]),
    ("禁表述 2", ["✅(⏭)"]),
    ("禁表述 3", ["提前进入 Stage 4+"]),
    ("禁表述 4", ["口径 ④ 已生效", "口径 ④ 已 ratify"]),
    ("禁表述 5", ["Stage 4+ 已开 ⇒ 可归档"]),
]

SU_HEADING_PREFIX = "## Status Update 2026-11-13"
C4_MARK = "[C4 · MUST-FIX-"


def _read(path):
    with open(path, encoding="utf-8", errors="ignore") as f:
        return f.read().split("\n")


def corpus(root):
    """返回 [(relpath, lineno, line)] —— 语料 = docs/ + .rddf/roadmap/ + test/ + scripts/"""
    out = []
    for d in SCAN_DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, _dirnames, filenames in os.walk(base):
            for fn in sorted(filenames):
                if os.path.splitext(fn)[1].lower() not in SCAN_EXTS:
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root)
                for i, line in enumerate(_read(full), 1):
                    out.append((rel, i, line))
    return out


def require(path, name):
    if not os.path.isfile(path):
        print(f"MISSING {name} :: {path}", flush=True)
        sys.exit(2)


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--root", default=ROOT_DEFAULT)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.list:
        print("S1 旧文本零命中 / S2 四锚整行唯一 / S3 缺失归因 / S4 作用域要素 / "
              "S5 绝对禁令复活 / S6 五条禁表述 / S7 指针目标与取代指针")
        return 0

    root = os.path.abspath(args.root)
    f07 = os.path.join(root, DOC07)
    fguide = os.path.join(root, GUIDE)
    fobj = os.path.join(root, OBJ)
    require(f07, "07 号")
    require(fguide, "手册")
    require(fobj, "台账")

    lines07 = _read(f07)
    lines_guide = _read(fguide)
    data = corpus(root)
    fails = []

    def fail(cid, sig, detail):
        fails.append(f"FAIL {cid} {sig} :: {detail}")

    # ── S1:旧文本零命中 ────────────────────────────────────────────────
    old_hits = [(p, n) for p, n, l in data if l == A3_OLD]
    if old_hits:
        fail("S1", SIG_OLD, "手册旧文仍存在: " + ", ".join(f"{p}:{n}" for p, n in old_hits[:5]))

    # ── S2:四锚整行唯一(缺 A2 时按 S3 归因)────────────────────────────
    missing = []
    for name, text, expect in ANCHORS:
        hits = [(p, n) for p, n, l in data if l == text]
        files = sorted({p for p, _ in hits})
        if not hits:
            missing.append(name)
            continue
        if len(files) > 1:
            fail("S2", SIG_DUP, f"{name} 命中 {len(files)} 个文件: {files}")
            continue
        if hits[0][0] != expect:
            fail("S2", SIG_DUP, f"{name} 承载文件 = {hits[0][0]},期望 {expect}")
            continue
        if len(hits) != 1:
            fail("S2", SIG_DUP, f"{name} 在 {hits[0][0]} 内命中 {len(hits)} 次(须恰 1)")

    # ── S3:缺失归因 ────────────────────────────────────────────────────
    def obj_has_prefix():
        return any(l.startswith("  - ⛔ **B3b-2 不得改写为") for l in _read(fobj))

    def doc07_has_ban():
        return any(any(t in l for t in BAN_TOKENS_07) for l in lines07)

    for name in missing:
        if name == "A1":
            if obj_has_prefix():
                fail("S3", SIG_APPEND, "A1 整行已变(疑似行内追加)⇒ 既有行必须逐字保留、只许新增子 bullet")
            else:
                fail("S3", SIG_MISS, "A1 在语料内 0 命中")
        elif name == "A2":
            if doc07_has_ban():
                fail("S3", SIG_SCOPE, "A2(作用域句)缺失,而 07 号 内禁令句仍在 ⇒ 作用域子句被删")
            else:
                fail("S3", SIG_MISS, "A2 在语料内 0 命中")
        else:
            fail("S3", SIG_MISS, f"{name} 在语料内 0 命中")

    # ── S4:作用域要素 ──────────────────────────────────────────────────
    if "A2" not in missing:
        lack = [t for t in SCOPE_ELEMS if t not in A2]
        if lack:
            fail("S4", SIG_SCOPE, "A2 缺要素: " + ", ".join(lack))

    # ── S5:绝对禁令不复活 ──────────────────────────────────────────────
    for i, l in enumerate(lines07, 1):
        if any(t in l for t in BAN_TOKENS_07) and not any(t in l for t in SCOPE_TOKENS):
            fail("S5", SIG_BAN, f"07 号:{i} 含禁令 token 但无作用域 token")

    # ── S6:禁表述(新文本面)───────────────────────────────────────────
    su_start = [i for i, l in enumerate(lines07) if l.startswith(SU_HEADING_PREFIX)]
    scope_lines = []
    if su_start:
        i = su_start[0]
        while i < len(lines07) and not (i > su_start[0] and lines07[i].startswith("## ")):
            scope_lines.append(("07号-SU", i + 1, lines07[i]))
            i += 1
    for i, l in enumerate(lines_guide, 1):
        if l == A3:
            scope_lines.append(("手册", i, l))
    for i, l in enumerate(_read(fobj), 1):
        if C4_MARK in l:
            scope_lines.append(("台账", i, l))
    for tag, idx, line in scope_lines:
        for label, phrases in BANNED_PHRASES:
            for ph in phrases:
                if ph in line:
                    fail("S6", SIG_PHRASE, f"{tag}:{idx} 命中 {label} 「{ph}」")

    # ── S7:指针目标 + 取代指针 ─────────────────────────────────────────
    heads = [l for l in lines07 if l.startswith(SU_HEADING_PREFIX)]
    if len(heads) != 1:
        fail("S7", SIG_PTR, f"07 号 的 `{SU_HEADING_PREFIX}` 标题命中 {len(heads)} 次(须恰 1)")
    if SU_HEADING_PREFIX.replace("## ", "") not in A3:
        fail("S7", SIG_PTR, "手册 A3 行不含新 SU 指针 token")
    c4 = [l for l in _read(fobj) if C4_MARK in l]
    if not any(C4_MARK + "2]" in l or C4_MARK + "MUST-FIX-2]" in l for l in c4):
        fail("S7", SIG_REPL, "台账无 `[C4 · MUST-FIX-2]` 取代指针行")
    elif not any("Status Update 2026-11-13" in l for l in c4 if "MUST-FIX-2" in l):
        fail("S7", SIG_PTR, "台账 `[C4 · MUST-FIX-2]` 行不含新 SU 指针 token")

    if fails:
        for f in fails:
            print(f)
        return 1
    print("OK cep_dod_reading_check: 7 组判据全绿(S1~S7)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
