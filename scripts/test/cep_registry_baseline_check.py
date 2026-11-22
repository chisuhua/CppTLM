#!/usr/bin/env python3
"""scripts/test/cep_registry_baseline_check.py
  —— 登记册 §0.1「活基线」的取值 == 实测 比对闸

per 登记册 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6.4 的 **W2-a**
（承载缺口 **G-2**「登记册基线 stale ⇒ 每次归档都重演一次」）。

── 为什么必须有本闸 ──────────────────────────────────────────────
`§0.1` 是**现行口径**的唯一落点，但它是一张**手写的表**：每次归档 / 新增 spec /
新增闸脚本都会让表里的数字变 stale，而 stale 的方向可能是「虚报已满」或「虚报未做」。
v4 → v5 的实测偏差已累计 **5 处**（闸族、CEP 测试、字符/字节量纲、归档份数、
force-add 前提）⇒ 靠「一次性刷新」不可能收敛（G-2 说的就是这个复发形态）。

⇒ 本闸把「**§0.1 里写的数**」与「**当场重算的数**」逐项锁死：
   改表里的数字 ⇒ 闸红；改仓库实际状态而不改表 ⇒ 闸红。两侧互为锁。

── 设计边界（刻意不做什么）────────────────────────────────────
1. ⛔ **不查 HEAD sha / 实测日期**：每次 commit 都会让它们漂移 ⇒ 会把闸变成
   噪声源（`§0.1` 标题里的 HEAD 是**人工刷新点**，职责归 planner 的阶段收口）。
2. ⛔ **不查需要外部 CLI 判定的行**：`openspec validate --specs --strict` 的
   WARNING 去重计数（Purpose 占位 / Requirement 正文 >500）不是「枚举」而是
   「一次 CLI 判定的产物」⇒ 纳入会让闸依赖 CLI 版本语义（§6.6 P-11 ③ 的忌）。
   ⚠️ 这不是遗漏：本闸只覆盖 **5 类可由文件系统枚举**的行，覆盖面在
   `COVERED_LABELS` 里显式列出，未覆盖的行在输出里点名（可审计）。
3. ⛔ **不做「宽容解析」**：行的文本形态本身是本闸的**契约** —— 换了形态
   ⇒ `STRUCTURE-ERROR` 而非静默跳过。否则「改一句文案」就能让判据失效。

用法：
    python3 scripts/test/cep_registry_baseline_check.py
    python3 scripts/test/cep_registry_baseline_check.py --registry <路径>   # 变异测试

退出码：0 = 全部一致 · 1 = 有不一致 · 2 = 结构/输入错误（缺文件、行缺失、形态不符）
"""

import argparse
import os
import re
import sys

RED = "\033[0;31m"
GRN = "\033[0;32m"
YLW = "\033[0;33m"
RST = "\033[0m"

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_REGISTRY = os.path.join(
    REPO_ROOT, ".rddf", "roadmap", "objectives", "obj-cep-platform-remaining.md"
)

# §0.1 只到 §0.2 之前（§0.2 是 2026-11-16 历史快照，逐字保留 ⇒ ⛔ 不参与比对）
SEC_START = "### §0.1"
SEC_END = "### §0.2"


def count_dirs(path):
    try:
        return sum(1 for e in os.scandir(path) if e.is_dir())
    except FileNotFoundError:
        return None


def count_glob(path, prefix, suffix):
    try:
        names = os.listdir(path)
    except FileNotFoundError:
        return None
    return sum(1 for n in names if n.startswith(prefix) and n.endswith(suffix))


def read_carrier_rows():
    """返回载体表 5 列数据行（跳过注释与空行/列数不符的行）。"""
    path = os.path.join(REPO_ROOT, "scripts", "test", "rddf_ac_carrier.tsv")
    rows = []
    try:
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                line = line.rstrip("\n")
                if not line or line.startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) != 5:
                    continue
                rows.append(parts)
    except FileNotFoundError:
        return None
    return rows


def expect_specs(_unused):
    specs = os.path.join(REPO_ROOT, "openspec", "specs")
    total = count_dirs(specs)
    if total is None:
        return None
    cep = count_glob(specs, "cep-", "")
    return [total, cep, total - cep]


def expect_gates(_unused):
    base = os.path.join(REPO_ROOT, "scripts", "test")
    sh = count_glob(base, "cep_", ".sh")
    py = count_glob(base, "cep_", ".py")
    if sh is None or py is None:
        return None
    return [sh, py, sh + py]


def expect_tests(_unused):
    base = os.path.join(REPO_ROOT, "test")
    n = count_glob(base, "test_cep_", ".cc")
    return None if n is None else [n]


def expect_openspec(_unused):
    changes = os.path.join(REPO_ROOT, "openspec", "changes")
    archive = os.path.join(changes, "archive")
    try:
        entries = os.listdir(changes)
    except FileNotFoundError:
        return None
    active = sum(
        1 for e in entries if e != "archive" and os.path.isdir(os.path.join(changes, e))
    )
    arch_dirs = count_dirs(archive)
    if arch_dirs is None:
        return None
    cep_arch = 0
    for e in os.scandir(archive):
        if e.is_dir() and "cep" in e.name:
            cep_arch += 1
    return [active, arch_dirs, cep_arch]


def expect_carrier(_unused):
    rows = read_carrier_rows()
    if rows is None:
        return None
    i_rows = [r for r in rows if r[0].startswith("I-")]
    w_rows = [r for r in rows if r[0].startswith("W-")]
    g_rows = [r for r in rows if r[0].startswith("G-")]
    ac_rows = [r for r in rows if r[0].startswith("AC-")]
    return [len(rows), len(i_rows), len(w_rows), len(g_rows), len(ac_rows)]


# 每项：(行首列标签, 从「实测」格抽数的正则, 期望值函数, 各数含义)
# ⚠️ 正则是**形态契约**：§0.1 的行改了形态 ⇒ STRUCTURE-ERROR（⛔ 不静默跳过）
CHECKS = [
    (
        "全仓主 spec",
        r"\*\*(\d+)\*\*\s*份（`cep-\*`\s*\*\*(\d+)\*\*\s*\+\s*非\s*`cep-\*`\s*\*\*(\d+)\*\*）",
        expect_specs,
        ["spec 总数", "cep-* 数", "非 cep-* 数"],
    ),
    (
        "闸族",
        r"\*\*(\d+)\*\*\s*个\s*`cep_\*\.sh`\s*\*\*\+\s*(\d+)\*\*\s*个\s*`cep_\*\.py`\s*=\s*\*\*(\d+)\*\*",
        expect_gates,
        ["cep_*.sh 数", "cep_*.py 数", "闸族合计"],
    ),
    (
        "CEP 测试",
        r"\*\*(\d+)\*\*\s*个\s*`test_cep_\*\.cc`",
        expect_tests,
        ["test_cep_*.cc 数"],
    ),
    (
        "OpenSpec",
        r"active\s*\*\*(\d+)\*\*.*归档\s*\*\*change 目录数\*\*\s*\*\*(\d+)\*\*（CEP\s*\*\*(\d+)\*\*）",
        expect_openspec,
        ["active 数", "归档 change 目录数", "CEP 归档数"],
    ),
    (
        "顺序机关载体表",
        r"\*\*(\d+)\*\*\s*个数据行.*?\*\*(\d+)\*\*\s*个\s*`I-\*`.*?"
        r"\*\*(\d+)\*\*\s*个\s*`W-\*`.*?\*\*(\d+)\*\*\s*个\s*`G-\*`.*?"
        r"\*\*(\d+)\*\*\s*个旧框架治理",
        expect_carrier,
        ["数据行总数", "I-* 行数", "W-* 行数", "G-* 行数", "AC-* 行数"],
    ),
]

COVERED_LABELS = [c[0] for c in CHECKS]


def read_registry_section(path):
    """抽 §0.1 节正文（不含 heading）。返回 (lines, error)。"""
    try:
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
    except FileNotFoundError:
        return None, f"登记册不存在: {path}"
    lines = text.split("\n")
    start = None
    for i, line in enumerate(lines):
        if line.startswith(SEC_START):
            start = i + 1
            break
    if start is None:
        return None, f"未找到 `{SEC_START}` 节（§0.1 结构变更 ⇒ 本闸需同步）"
    end = len(lines)
    for i in range(start, len(lines)):
        if lines[i].startswith(SEC_END):
            end = i
            break
    return lines[start:end], None


def table_rows(section_lines):
    """抽出 section 内的表格数据行（跳过表头与分隔行）。"""
    rows = []
    for line in section_lines:
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 2:
            continue
        if set(cells[0]) <= set("-: "):
            continue
        if cells[0] == "项":
            continue
        rows.append(cells)
    return rows


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--registry", default=DEFAULT_REGISTRY, help="登记册路径（变异测试用）")
    args = ap.parse_args()

    print(f"{YLW}[cep-registry-baseline] §0.1 活基线：文档取值 == 实测 比对{RST}")
    print(f"  登记册 = {args.registry}")

    section, err = read_registry_section(args.registry)
    if err:
        print(f"{RED}✗ STRUCTURE-ERROR: {err}{RST}")
        return 2

    rows = table_rows(section)
    by_label = {}
    for cells in rows:
        by_label[cells[0]] = cells

    failures = 0
    structure_errors = 0
    for label, pattern, expect_fn, names in CHECKS:
        cells = by_label.get(label)
        if cells is None:
            print(f"{RED}✗ STRUCTURE-ERROR [{label}] §0.1 缺该行（行标签是本闸的契约）{RST}")
            structure_errors += 1
            continue
        cell = cells[1]
        m = re.search(pattern, cell)
        if not m:
            print(
                f"{RED}✗ STRUCTURE-ERROR [{label}] 「实测」格形态不符本闸契约{RST}\n"
                f"    实际: {cell[:160]}\n"
                f"    期望正则: {pattern}"
            )
            structure_errors += 1
            continue
        documented = [int(g) for g in m.groups()]
        actual = expect_fn(None)
        if actual is None:
            print(f"{RED}✗ [{label}] 无法重算（源目录缺失）{RST}")
            failures += 1
            continue
        if len(documented) != len(actual):
            print(
                f"{RED}✗ STRUCTURE-ERROR [{label}] 抽到 {len(documented)} 个数，"
                f"期望 {len(actual)} 个{RST}"
            )
            structure_errors += 1
            continue
        diffs = [
            (names[i], documented[i], actual[i])
            for i in range(len(actual))
            if documented[i] != actual[i]
        ]
        if diffs:
            for name, doc, act in diffs:
                print(f"{RED}✗ [{label}] {name}: 文档 **{doc}** ≠ 实测 {act}{RST}")
            failures += 1
        else:
            shown = " · ".join(f"{names[i]}={actual[i]}" for i in range(len(actual)))
            print(f"{GRN}✓ [{label}] {shown}{RST}")

    # 未覆盖行点名（可审计；⛔ 不是失败）
    uncovered = [c[0] for c in rows if c[0] not in COVERED_LABELS]
    if uncovered:
        print(f"{YLW}ℹ 本闸未覆盖的 §0.1 行（需外部 CLI 或非枚举口径）: {', '.join(uncovered)}{RST}")

    print()
    # ⚠️ 形态错误与「取值不一致」**分开报**：前者退出 2（输入/契约问题），
    #    后者退出 1（真的漂移）。⛔ 不合并 —— 否则 CI 无法区分「该改表」与「该修契约」。
    if structure_errors:
        print(f"{RED}STRUCTURE-ERROR：{structure_errors} 项（§0.1 的行形态不符本闸契约）{RST}")
        return 2
    if failures:
        print(f"{RED}基线比对失败：{failures} 项{RST}")
        return 1
    print(f"{GRN}基线比对通过：{len(CHECKS)} 项文档取值 == 实测{RST}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
