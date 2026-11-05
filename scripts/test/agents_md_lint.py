#!/usr/bin/env python3
"""scripts/test/agents_md_lint.py —— AGENTS.md 结构与体量守门

三项检查（全部可机器判定，退出码 0 = 全绿）：

  1. STRUCTURE 树路径存在性 —— 解析 AGENTS.md `## STRUCTURE` 的代码围栏,
     按缩进重建层级, 逐条 stat 路径。防止「树与仓库现实脱节」。
  2. 体量上限 —— AGENTS.md 字节数上限（防止 append-only 日志无限膨胀,
     AGENTS.md 注入每个 agent 上下文, 体量直接等于每会话 token 成本）。
  3. 单行长度上限 —— 防止把流水账压成超长单行（历史形态：单行 6609 字符）。

用法:
  python3 scripts/test/agents_md_lint.py           # 打印报告
  python3 scripts/test/agents_md_lint.py --strict  # 有问题返回非零退出码

作者: CppTLM Team
日期: 2026-11-05
"""

import glob
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AGENTS = os.path.join(REPO_ROOT, "AGENTS.md")

MAX_BYTES = 65536        # 64 KiB
MAX_LINE_CHARS = 1000

# 树中带这些字符的 token 视为通配/占位, 走 glob 匹配
GLOB_CHARS = set("*?~")
# 无法机器判定的占位 token, 直接跳过
SKIP_CHARS = set("{}<>|()")

RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
BLUE = "\033[0;34m"
NC = "\033[0m"


def extract_structure_tree(text):
    """提取 '## STRUCTURE' 之后第一个代码围栏的内容。"""
    m = re.search(r"^## STRUCTURE[^\n]*\n(.*?)^```\n(.*?)^```", text, re.M | re.S)
    return m.group(2) if m else None


def resolve(token, prefix):
    return prefix + token


def check_path(path):
    """返回 None 表示通过；否则返回问题描述。"""
    if os.path.exists(path):
        return None
    # 通配符: 允许前缀匹配（tree_*.cc / cep_*.sh / *_protocol_engine.hh 之类）
    if any(c in os.path.basename(path) for c in GLOB_CHARS):
        if glob.glob(path + "*") or glob.glob(path):
            return None
    return f"路径不存在: {path}"


def expand_braces(token):
    """展开 {a,b} 组合 —— 树里用 `demo_x.{cc,py}` / `{a,b}_adapter.hh` 表达并列项。"""
    m = re.search(r"\{([^}]*)\}", token)
    if not m:
        return [token]
    out = []
    for alt in m.group(1).split(","):
        out.extend(expand_braces(token[:m.start()] + alt.strip() + token[m.end():]))
    return out


FILE_TOKEN_RE = re.compile(r"^[^/]*\.[A-Za-z0-9_]+$")


def collect_tree_paths(tree):
    """按缩进重建层级, 产出 (相对路径, 源码行号) 列表。"""
    results = []
    stack = []  # [(indent, name_with_slash)]
    for lineno, raw in enumerate(tree.split("\n"), start=1):
        stripped = raw.strip()
        if not stripped:
            continue
        indent = len(raw) - len(raw.lstrip())
        # 纯注释行: 不参与层级栈 (避免注释续行把前缀带歪)
        if stripped.startswith("#"):
            continue
        while stack and stack[-1][0] >= indent:
            stack.pop()
        prefix = "".join(name for _, name in stack)
        body = stripped.split("#", 1)[0].strip()
        if not body:
            continue
        for token in body.split():
            for expanded in expand_braces(token):
                if not expanded:
                    continue
                if any(c in expanded for c in SKIP_CHARS):
                    continue
                if expanded.endswith("/"):
                    # 同一行的多个目录项互为兄弟 —— 先弹出同缩进的旧项
                    while stack and stack[-1][0] >= indent:
                        stack.pop()
                    stack.append((indent, expanded))
                    results.append((resolve(expanded, prefix), lineno, True))
                elif FILE_TOKEN_RE.match(os.path.basename(expanded)):
                    # 只认「有真实扩展名」的 token（排除 `02-04` / `...` 这类说明性写法）
                    results.append((resolve(expanded, prefix), lineno, False))
    return results


def main():
    strict = "--strict" in sys.argv[1:]

    if not os.path.isfile(AGENTS):
        print(f"{RED}✗ AGENTS.md 不存在: {AGENTS}{NC}")
        return 1

    with open(AGENTS, encoding="utf-8") as f:
        text = f.read()

    problems = []

    # ---- 检查 1: STRUCTURE 树路径 ----
    tree = extract_structure_tree(text)
    if tree is None:
        problems.append("STRUCTURE 节缺少代码围栏（无法解析目录树）")
    else:
        seen = set()
        for path, lineno, is_dir in collect_tree_paths(tree):
            if (path, is_dir) in seen:
                continue
            seen.add((path, is_dir))
            err = check_path(path)
            if err:
                problems.append(f"STRUCTURE 树 L{lineno}: {err}")

    # ---- 检查 2: 体量上限 ----
    size = len(text.encode("utf-8"))
    if size > MAX_BYTES:
        problems.append(
            f"体量超限: {size} B > {MAX_BYTES} B "
            f"(超出 {size - MAX_BYTES} B) —— 过程日志应落 ADR `## Status Update` "
            f"或 openspec/changes/archive/, 不进 AGENTS.md"
        )

    # ---- 检查 3: 单行长度 ----
    for lineno, line in enumerate(text.split("\n"), start=1):
        if len(line) > MAX_LINE_CHARS:
            problems.append(
                f"单行过长: L{lineno} = {len(line)} 字符 > {MAX_LINE_CHARS} —— "
                f"拆分该行（历史形态: 单行 6609 字符的 KPI 流水账）"
            )

    # ---- 报告 ----
    print(f"{BLUE}AGENTS.md 守门检查{NC}")
    print(f"  文件: {os.path.relpath(AGENTS, REPO_ROOT)}  ({size} B / 上限 {MAX_BYTES} B)")
    print(f"  检查项: STRUCTURE 树路径 + 体量上限 + 单行长度上限")
    print("")

    if problems:
        print(f"{RED}✗ 发现 {len(problems)} 项问题:{NC}")
        for p in problems:
            print(f"  {RED}•{NC} {p}")
        print("")
        print(f"{YELLOW}修复建议:{NC}")
        print("  1. STRUCTURE 树: 改到真实路径, 或删掉该条目（树必须与仓库一致）")
        print("  2. 体量: 把过程日志移到 ADR `## Status Update` 段或 openspec/changes/archive/")
        print("  3. 单行: 拆成多条（每条一行事实）")
        print("")
        return 1 if strict else 0

    print(f"{GREEN}✅ AGENTS.md 结构 / 体量 / 行长全部合规{NC}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
