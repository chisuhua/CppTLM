#!/usr/bin/env python3
"""scripts/test/agents_md_lint.py —— AGENTS.md 结构与体量守门

三项检查（全部可机器判定，退出码 0 = 全绿）：

  1. STRUCTURE 树路径存在性 —— 解析 AGENTS.md `## STRUCTURE` 的代码围栏,
     按缩进重建层级, 逐条 stat 路径。防止「树与仓库现实脱节」。
  2. 体量上限 —— AGENTS.md 字节数上限（防止 append-only 日志无限膨胀,
     AGENTS.md 注入每个 agent 上下文, 体量直接等于每会话 token 成本）。
  3. 单行长度上限 —— 防止把流水账压成超长单行（历史形态：单行 6609 字符）。

用法:
  python3 scripts/test/agents_md_lint.py                     # 根文件, 打印报告
  python3 scripts/test/agents_md_lint.py --strict             # 根文件, 有问题 rc=1
  python3 scripts/test/agents_md_lint.py --all --strict        # + 全部子目录 AGENTS.md
  python3 scripts/test/agents_md_lint.py --root <dir> --all     # 对拷贝根校验
  python3 scripts/test/agents_md_lint.py --selftest             # 6 个决定性变异(拷贝根)

子目录 AGENTS.md 推荐模板（6 段, 只写本目录特有内容）:
  1) H1 标题（目录 — 一句话职责）
  2) 状态行（版本 / 最后更新 —— ⛔ 不写过程日志）
  3) 职责（本目录管什么、边界在哪）
  4) 文件索引（**分组**列关键文件, ⛔ 不逐个罗列全部文件）
  5) 本目录铁律（只有这里才成立、违反会坏事的约束）
  6) 常见坑（易错点）+ 关联（上层索引 / 测试标签 / ADR）

作者: CppTLM Team
日期: 2026-11-05
"""

import glob
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROOT = REPO_ROOT          # 可被 --root 覆盖（--selftest 用拷贝根）
AGENTS = os.path.join(ROOT, "AGENTS.md")

MAX_BYTES = 65536        # 64 KiB
MAX_LINE_CHARS = 1000
MAX_SUB_BYTES = 10240     # 子目录 AGENTS.md 体量上限（10 KiB/份）

# ── 子目录 AGENTS.md 校验（--all，2026-11-19 新增）──────────────────────────
# 判据：① 体量 ② 单行长度 ③ **未标注的失效路径** ④ 根文件登记清单 == 磁盘清单
# ③ 的「标注」是**节级或行级**：整节在讲已归档/已迁出/跨仓/命名约定时，该节内的
#    路径不校验存在性（`docs/research/AGENTS.md` 的「跨仓指针」表即此形态）。
#    这是「有意引用」的合法出口，⛔ 不是消音器 —— 未标注的失效引用一律判红。
SKIP_DIRS = {".git", "build", "build-on", "external", ".venv", "venv",
             "node_modules", ".cache", "__pycache__"}
PATH_EXTS = {"hh", "h", "cc", "cpp", "py", "json", "md", "sh", "tsv", "yaml",
             "yml", "txt", "cfg", "in", "jsonc", "cmake", "toml"}
ANNOTATIONS = ("已归档", "归档", "已迁", "已删除", "ArchForge", "跨仓", "历史",
               "原路径", "eprecated", "已废弃", "待建", "占位", "不属本仓",
               "已清理", "如 ", "例如", "示意", "命名", "SystemC")
TICK = re.compile(r"`([^`\n]+)`")

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


def _ancestors(d):
    out = []
    while True:
        out.append(d)
        if d in ("", ".", ROOT):
            break
        nd = os.path.dirname(d)
        if nd == d:
            break
        d = nd
    return out


def _path_like(core):
    last = core.rstrip("/").split("/")[-1]
    if last.startswith("."):
        return False
    if core.endswith("/"):
        return True
    return "." in last and last.rsplit(".", 1)[1] in PATH_EXTS


def _build_name_index():
    """全仓文件名索引（排除 build/external 等）—— 裸文件名按「本仓存在同名文件」判定。"""
    names = set()
    for dp, dns, fns in os.walk(ROOT):
        dns[:] = [d for d in dns if d not in SKIP_DIRS]
        names.update(fns)
    return names


def _resolve(token, fpath, names):
    """目录 token（尾 `/`）按目录判定；含 `/` 的按路径判定；裸文件名按全仓同名文件判定。
    路径按「仓根 / 本文件所在目录 / 其各级祖先」依次尝试（子文件惯用相对写法）。"""
    import fnmatch
    is_dir = token.endswith("/")
    core = token.rstrip("/")
    if is_dir or "/" in core:
        for a in [ROOT] + _ancestors(os.path.dirname(fpath)):
            p = os.path.join(a, core)
            if any(c in core for c in "*?"):
                d2 = os.path.dirname(p) if not is_dir else p
                if os.path.isdir(d2) and any(fnmatch.fnmatch(x, os.path.basename(p))
                                             for x in os.listdir(d2)):
                    return True
            elif (os.path.isdir(p) if is_dir else os.path.exists(p)):
                return True
        return False
    return any(fnmatch.fnmatch(n, core) for n in names)


def check_sub_agents():
    """遍历仓内子目录 AGENTS.md（不含根、不含 external submodule），返回问题列表。"""
    problems = []
    names = _build_name_index()
    files = []
    for dp, dns, fns in os.walk(ROOT):
        dns[:] = [d for d in dns if d not in SKIP_DIRS]
        if "AGENTS.md" in fns:
            rel = os.path.relpath(os.path.join(dp, "AGENTS.md"), ROOT)
            if rel != "AGENTS.md":
                files.append(rel)
    files.sort()

    for rel in files:
        path = os.path.join(ROOT, rel)
        raw = open(path, encoding="utf-8", errors="replace").read()
        size = len(raw.encode("utf-8"))
        if size > MAX_SUB_BYTES:
            problems.append(f"{rel}: 体量 {size} B > {MAX_SUB_BYTES} B —— 只写本目录特有内容，通用规则留在根文件")
        section = ""
        for lineno, line in enumerate(raw.split("\n"), start=1):
            if len(line) > MAX_LINE_CHARS:
                problems.append(f"{rel} L{lineno}: 单行 {len(line)} 字符 > {MAX_LINE_CHARS}")
            if line.lstrip().startswith("#"):
                section = line
            exempt = any(a in line for a in ANNOTATIONS) or any(a in section for a in ANNOTATIONS)
            if exempt:
                continue
            for tok in TICK.findall(line):
                s = tok.strip()
                if not s or " " in s or any(c in s for c in "<>{}|()"):
                    continue
                if s.startswith(("http", "//", "$", "-", "*")):
                    continue
                s = re.sub(r":\d+(-\d+)?$", "", s)
                if not _path_like(s):
                    continue
                if not _resolve(s, path, names):
                    problems.append(
                        f"{rel} L{lineno}: 失效路径 `{s}`（若是有意引用，请在**该行或所属小节**注明 "
                        f"已归档 / 已迁 / 跨仓 / 命名约定 等标记）")

    # ④ 根文件登记清单 == 磁盘清单
    root = open(os.path.join(ROOT, "AGENTS.md"), encoding="utf-8").read()
    if "## ARCHIVES & HISTORY" in root:
        sec = root[root.index("## ARCHIVES & HISTORY"):]
        listed = set(re.findall(r"`([A-Za-z0-9_./-]+/AGENTS\.md)`", sec))
        actual = set(files)
        for x in sorted(listed - actual):
            problems.append(f"根文件 ARCHIVES & HISTORY 登记了不存在的 `{x}`")
        for x in sorted(actual - listed):
            problems.append(f"`{x}` 存在但未登记进根文件 ARCHIVES & HISTORY")
    else:
        problems.append("根文件缺 `## ARCHIVES & HISTORY` 节（无法对账子目录清单）")
    return problems, len(files)


def _run_cli(root):
    """以子进程方式跑自己（避免全局 ROOT 污染）。返回退出码。"""
    import subprocess
    return subprocess.run(
        [sys.executable, os.path.abspath(__file__), "--all", "--strict", "--root", root],
        capture_output=True, text=True).returncode


def selftest():
    """6 个决定性变异（拷贝根，零工作树污染）——证明三判据 + 标注出口都有牙。"""
    import shutil
    import tempfile

    base = tempfile.mkdtemp()
    try:
        def build(d):
            os.makedirs(os.path.join(d, "sub"))
            open(os.path.join(d, "sub", "x.hh"), "w").close()
            open(os.path.join(d, "AGENTS.md"), "w", encoding="utf-8").write(
                "# root\n\n## STRUCTURE (x)\n\n```\nsub/\n```\n\n"
                "## ARCHIVES & HISTORY\n\n- `sub/AGENTS.md`\n")
            open(os.path.join(d, "sub", "AGENTS.md"), "w", encoding="utf-8").write(
                "# sub\n\n见 `x.hh`\n")

        def case(name, mutate, want):
            d = os.path.join(base, name)
            build(d)
            mutate(d)
            got = 0 if _run_cli(d) == 0 else 1
            ok = (got == want)
            print(f"  {'\033[0;32m✓\033[0m' if ok else '\033[0;31m✗\033[0m'} "
                  f"{name}: 期望 {'绿' if want == 0 else '红'}，实得 {'绿' if got == 0 else '红'}")
            return ok

        print("══ agents_md_lint --selftest（6 变异，拷贝根）══")
        results = []

        results.append(case("基线", lambda d: None, 0))

        def m1(d):  # 新增未登记的子文件 ⇒ ④ 红
            open(os.path.join(d, "sub2_unused"), "w").close()
            os.makedirs(os.path.join(d, "c"))
            open(os.path.join(d, "c", "AGENTS.md"), "w").write("# c\n")
        results.append(case("变异1 未登记的子目录 AGENTS.md", m1, 1))

        def m2(d):  # 登记了不存在的 ⇒ ④ 红
            p = os.path.join(d, "AGENTS.md")
            t = open(p, encoding="utf-8").read().replace("- `sub/AGENTS.md`\n", "- `ghost/AGENTS.md`\n")
            open(p, "w", encoding="utf-8").write(t)
        results.append(case("变异2 登记幽灵条目", m2, 1))

        def m3(d):  # 未标注失效路径 ⇒ ③ 红
            p = os.path.join(d, "sub", "AGENTS.md")
            open(p, "a", encoding="utf-8").write("\n见 `missing/file.hh`\n")
        results.append(case("变异3 未标注失效路径", m3, 1))

        def m4(d):  # 同一条失效路径但带标注 ⇒ 绿（证明标注是合法出口，不是全放行）
            p = os.path.join(d, "sub", "AGENTS.md")
            open(p, "a", encoding="utf-8").write("\n见 `missing/file.hh`（已归档）\n")
        results.append(case("变异4 标注后的失效路径（应放行）", m4, 0))

        def m5(d):  # 撑过 10 KiB ⇒ ① 红
            p = os.path.join(d, "sub", "AGENTS.md")
            open(p, "a", encoding="utf-8").write("\n" + ("填充行\n" * 1200))
        results.append(case("变异5 子文件体量超 10 KiB", m5, 1))

        def m6(d):  # 单行超长 ⇒ ② 红
            p = os.path.join(d, "sub", "AGENTS.md")
            open(p, "a", encoding="utf-8").write("x" * 1200 + "\n")
        results.append(case("变异6 单行超长", m6, 1))

        ok = all(results)
        print(f"\033[0;32m✅ selftest:{sum(results)}/{len(results)} 变异符合预期\033[0m"
              if ok else "\033[0;31m❌ selftest 存在未按预期变红的判据\033[0m")
        return 0 if ok else 1
    finally:
        shutil.rmtree(base, ignore_errors=True)


def main():
    global ROOT, AGENTS
    argv = sys.argv[1:]
    strict = "--strict" in argv
    all_mode = "--all" in argv
    if "--root" in argv:
        ROOT = os.path.abspath(argv[argv.index("--root") + 1])
        AGENTS = os.path.join(ROOT, "AGENTS.md")
    # ⚠️ STRUCTURE 树的路径检查用相对路径 ⇒ 必须把 cwd 固定到 ROOT
    #    (否则从 build/ 或拷贝根运行时全部误报「路径不存在」)
    os.chdir(ROOT)
    if "--selftest" in argv:
        return selftest()

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

    # ---- 子目录 AGENTS.md（--all）----
    sub_count = 0
    if all_mode:
        sub_problems, sub_count = check_sub_agents()
        problems.extend(sub_problems)

    # ---- 报告 ----
    print(f"{BLUE}AGENTS.md 守门检查{NC}")
    print(f"  文件: {os.path.relpath(AGENTS, REPO_ROOT)}  ({size} B / 上限 {MAX_BYTES} B)")
    print(f"  检查项: STRUCTURE 树路径 + 体量上限 + 单行长度上限"
          + (f" + 子目录 AGENTS.md（{sub_count} 份：体量/行长/失效引用/登记对账）" if all_mode else ""))
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
        if all_mode:
            print("  4. 子目录 AGENTS.md: 失效引用要么改成真实路径, 要么在该行/小节注明"
                  " 已归档 / 已迁 / 跨仓 / 命名约定 等标记")
        print("")
        return 1 if strict else 0

    print(f"{GREEN}✅ AGENTS.md 结构 / 体量 / 行长全部合规{NC}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
