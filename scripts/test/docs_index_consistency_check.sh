#!/usr/bin/env bash
# scripts/test/docs_index_consistency_check.sh
# **子目录索引不腐化闸** —— `docs/**/README.md` 的目录级索引
#
# 判据面(⚠️ 唯一真源 = `docs/README.md` §7「子目录索引登记契约」;⛔ 本闸不自创口径):
#   ① **覆盖**:目录 D 的 README 必须登记 D 的**顶层** `.md`(排除 README 自身)
#      —— 登记形态 = `](./name.md)` **或** `` `name.md` ``;⛔ 不递归(子目录由各自 README 自管)
#   ② **链接可达**:README 里所有**相对** `.md` 链接必须解析到真实文件
#   ③ **状态符号一致**(**仅 `docs/architecture`**):本体系号 == 索引符号
#      (符号集 ✅🟡📋⛔➡️🟢🔴;本体取首个 `状态|文档状态|Status` 标签后行尾/`|` 前的第一个符号;
#       索引取该行**最后一个非空 cell** 的首个符号;⛔ 显式豁免表当前 1 项 = `06-bis`)
#
# 适用范围(排除两项,理由见 §7.1):
#   · `docs/README.md`      —— 总入口(指向子目录 README,不逐文件登记)
#   · `docs/archive/**`     —— 归档目录 README 是**摘要**而非穷举
#   · ⛔ 判据③ 不覆盖其它目录:其索引「状态」列混入生命周期标记(⛔ supersede / ➡️ 已迁出)
#     与**时点快照**语义(如 `docs/implementation/2026-10-21-*` 的本体状态是写作时状态)⇒ 不可比
#
# 用法:
#   bash scripts/test/docs_index_consistency_check.sh              # 正常校验
#   bash scripts/test/docs_index_consistency_check.sh --root <dir> # 对拷贝根校验(供 --selftest)
#   bash scripts/test/docs_index_consistency_check.sh --selftest   # 5 个决定性变异(跑在拷贝根)
#   DOCS_IDX_NO_EXEMPT=1 bash ...                                  # 清空豁免表(证明豁免是必要的)
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="$REPO_ROOT"
SELFTEST=0
while [ $# -gt 0 ]; do
    case "$1" in
        --root) ROOT="$2"; shift 2 ;;
        --selftest) SELFTEST=1; shift ;;
        *) echo "未知参数: $1" >&2; exit 2 ;;
    esac
done
cd "$ROOT" || exit 2

pass() { echo -e "  \033[0;32m✓\033[0m $1"; }
fail() { echo -e "  \033[0;31m✗\033[0m $1"; }

run_checks() {
    python3 - <<'PY'
import glob, os, re, sys

SYM = re.compile(r"[✅🟡📋⛔➡🟢🔴]")
# 状态源行:优先「行首标签」(元数据行),否则退回任意位置的标签(如 `版本：x | 状态：y`)
LABEL_LINE = re.compile(r"^[\s>|*\-]*\*{0,2}\s*(?:文档状态|状态|Status)\s*\*{0,2}\s*[:：]", re.M)
LABEL = re.compile(r"(?:文档状态|状态|Status)\s*\*{0,2}\s*[:：]")
LINK = re.compile(r"\]\(([^)\s]+)\)")
TICK = re.compile(r"`([A-Za-z0-9_.\-]+\.md)`")

# §7.1 排除项
EXCLUDE = ("docs", "docs/archive")
# §7.4 判据③ 适用目录
STATUS_DIRS = {"docs/architecture"}
# §7.4 显式豁免表:(目录, 文件名) -> 理由
EXEMPT = {
    ("docs/architecture", "06-bis-coherence-as-network-extension.md"):
        "索引承载 supersede 生命周期标记(⛔ 已被 07 号取代)而本体只记修订版本(🟡)⇒ 两者语义不同层,非漂移",
}
if os.environ.get("DOCS_IDX_NO_EXEMPT") == "1":
    EXEMPT = {}

def body_symbol(text):
    """本体状态符号:状态源行(行首标签优先)后、行尾或 `|` 前的第一个符号。"""
    m = LABEL_LINE.search(text) or LABEL.search(text)
    if not m:
        return None
    seg = text[m.end():].split("\n")[0].split("|")[0]
    s = SYM.search(seg)
    return s.group(0) if s else None

def index_symbols(text):
    """{文件名: 索引状态符号} —— **按表头定位「状态」列**(⛔ 不猜最后一个 cell:
    说明列里的 ⚠️/⛔ 会污染)。表格以连续 `|` 行界定,`> |` 引用表格同样处理。"""
    rows = {}
    lines = [l.strip() for l in text.split("\n")]
    lines = [re.sub(r"^(>\s*)+", "", l) for l in lines]
    sep = re.compile(r"^\|[\s:\-|]+\|?$")
    col = None
    for i, s in enumerate(lines):
        if not s.startswith("|"):
            col = None
            continue
        if sep.match(s):
            continue
        cells = [c.strip() for c in s.split("|")]
        # 表头行 = 表格行且下一行为分隔行
        if i + 1 < len(lines) and sep.match(lines[i + 1]):
            hdr = [k for k, c in enumerate(cells) if "状态" in c]
            col = hdr[-1] if hdr else None
            continue
        lm = re.search(r"\[([A-Za-z0-9_.\-]+\.md)\]\(\./\1\)", s)
        if not lm:
            continue
        sym_cell = cells[col] if (col is not None and col < len(cells)) else " ".join(cells[1:])
        sm = SYM.search(sym_cell)
        rows.setdefault(lm.group(1), sm.group(0) if sm else None)
    return rows

def is_excluded(d):
    return d == EXCLUDE[0] or d == EXCLUDE[1] or d.startswith(EXCLUDE[1] + "/")

fails = []
checked_dirs = 0
missing_total = 0
broken_total = 0
sym_checked = 0

for rd in sorted(glob.glob("docs/**/README.md", recursive=True)):
    d = os.path.dirname(rd).replace(os.sep, "/")
    if is_excluded(d):
        continue
    disk = {os.path.basename(p) for p in glob.glob(os.path.join(d, "*.md"))
            if os.path.basename(p) != "README.md"}
    if not disk:
        continue
    checked_dirs += 1
    text = open(rd, encoding="utf-8", errors="replace").read()

    # ── 判据② 链接可达(docs/ 内部的相对链接;跨出 docs/ 的目标不属本判据面 —— 见 §7.5)──
    for l in LINK.findall(text):
        if l.startswith(("http://", "https://", "mailto:", "#")):
            continue
        p = os.path.normpath(os.path.join(d, l.split("#")[0]))
        if os.path.relpath(p, "docs").startswith(".."):
            continue   # 目标在 docs/ 之外(scripts/ · AGENTS.md · openspec/ …) ⇒ 由其它闸管
        if not os.path.exists(p):
            broken_total += 1
            fails.append(f"[判据②] {rd}: 链接不可达 {l}")

    # ── 判据① 覆盖 ──
    same = set()
    for l in LINK.findall(text):
        if l.startswith(("http://", "https://", "mailto:", "#")):
            continue
        p = os.path.normpath(os.path.join(d, l.split("#")[0]))
        if os.path.dirname(p) == os.path.normpath(d) and p.endswith(".md"):
            same.add(os.path.basename(p))
    reg = same | {x for x in TICK.findall(text) if x in disk}
    missing = sorted(disk - reg)
    if missing:
        missing_total += len(missing)
        for n in missing:
            fails.append(f"[判据①] {rd}: 未登记 {n}")

    # ── 判据③ 状态符号(仅 STATUS_DIRS)──
    if d in STATUS_DIRS:
        rows = index_symbols(text)
        for name in sorted(disk):
            if (d, name) in EXEMPT:
                continue
            if name not in rows:
                continue  # 缺行已由判据① 报
            idx_sym = rows[name]
            if idx_sym is None:
                continue  # 索引该行无符号 ⇒ 不属判据③ 覆盖面
            body_sym = body_symbol(open(os.path.join(d, name), encoding="utf-8",
                                        errors="replace").read())
            if body_sym is None:
                fails.append(f"[判据③] {rd}: {name} 本体状态行无符号(无法比对)")
                continue
            sym_checked += 1
            if body_sym != idx_sym:
                fails.append(f"[判据③] {rd}: {name} 本体 {body_sym} ≠ 索引 {idx_sym}")

if fails:
    for f in fails[:40]:
        print("  \033[0;31m✗\033[0m " + f)
    if len(fails) > 40:
        print(f"  … 另有 {len(fails) - 40} 条")
    print(f"  ✗ 合计:未登记 {missing_total} · 坏链接 {broken_total} · 其它 {len(fails) - missing_total - broken_total}")
    sys.exit(1)

print(f"  \033[0;32m✓\033[0m 判据① 覆盖:{checked_dirs} 个目录的顶层 .md 全部已登记")
print(f"  \033[0;32m✓\033[0m 判据② 链接可达:全部相对 .md 链接解析成功")
print(f"  \033[0;32m✓\033[0m 判据③ 状态符号一致:{sym_checked} 份已比对"
      f"(适用 {', '.join(sorted(STATUS_DIRS))};豁免 {len(EXEMPT)} 项)")
sys.exit(0)
PY
}

# ── --selftest:5 个决定性变异(拷贝根,零工作树污染)────────────────────────────
if [ "$SELFTEST" -eq 1 ]; then
    echo "══ docs_index_consistency_check --selftest(5 变异,跑在拷贝根)══"
    BASE="$(mktemp -d)"
    trap 'rm -rf "$BASE"' EXIT
    copy_root() {
        local d="$1"; rm -rf "$d"; mkdir -p "$d"
        cp -a docs "$d/docs"
    }
    run_on() { ( cd "$1" && bash "$REPO_ROOT/scripts/test/docs_index_consistency_check.sh" --root "$1" ); }

    copy_root "$BASE/base"
    if run_on "$BASE/base" >/dev/null 2>&1; then pass "基线:拷贝根 rc=0"; else
        fail "基线:拷贝根应为绿"; run_on "$BASE/base" | tail -12; exit 1; fi

    # 变异 1:删 1 行登记 ⇒ 判据① 红
    copy_root "$BASE/m1"
    python3 - "$BASE/m1/docs/architecture/README.md" <<'PY'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
t2 = re.sub(r"^\| \[09-topology-visualization-pipeline\.md\][^\n]*\n", "", t, count=1, flags=re.M)
assert t2 != t, "变异 1 未生效"
open(p, "w", encoding="utf-8").write(t2)
PY
    if run_on "$BASE/m1" >/dev/null 2>&1; then
        fail "变异 1(删 1 行登记)应为红,实为绿 ⇒ 判据① 无牙"; exit 1
    else pass "变异 1:删 1 行登记 ⇒ 红"; fi

    # 变异 2:加 1 份未登记 .md ⇒ 判据① 红
    copy_root "$BASE/m2"
    printf '# probe\n' > "$BASE/m2/docs/guide/unregistered-probe.md"
    if run_on "$BASE/m2" >/dev/null 2>&1; then
        fail "变异 2(加未登记 .md)应为红,实为绿 ⇒ 判据① 无牙"; exit 1
    else pass "变异 2:加未登记 .md ⇒ 红"; fi

    # 变异 3:改本体状态符号 ⇒ 判据③ 红
    copy_root "$BASE/m3"
    python3 - "$BASE/m3/docs/architecture/09-topology-visualization-pipeline.md" <<'PY'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
t2 = re.sub(r"^(\*\*状态\*\*:\s*)🟡", r"\1✅", t, count=1, flags=re.M)
assert t2 != t, "变异 3 未生效"
open(p, "w", encoding="utf-8").write(t2)
PY
    if run_on "$BASE/m3" >/dev/null 2>&1; then
        fail "变异 3(改本体系号)应为红,实为绿 ⇒ 判据③ 无牙"; exit 1
    else pass "变异 3:改本体状态符号 ⇒ 红"; fi

    # 变异 4:断 1 条同目录链接 ⇒ 判据② 红
    copy_root "$BASE/m4"
    python3 - "$BASE/m4/docs/designs/dgpu-soc/README.md" <<'PY'
import sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
t2 = t.replace("(./timing-mode.md)", "(./nope-does-not-exist.md)", 1)
assert t2 != t, "变异 4 未生效"
open(p, "w", encoding="utf-8").write(t2)
PY
    if run_on "$BASE/m4" >/dev/null 2>&1; then
        fail "变异 4(断同目录链接)应为红,实为绿 ⇒ 判据② 无牙"; exit 1
    else pass "变异 4:断同目录链接 ⇒ 红"; fi

    # 变异 5:清空豁免表 ⇒ 必红(证明豁免是**必要的**,不是消音器)
    copy_root "$BASE/m5"
    if ( cd "$BASE/m5" && DOCS_IDX_NO_EXEMPT=1 bash "$REPO_ROOT/scripts/test/docs_index_consistency_check.sh" --root "$BASE/m5" >/dev/null 2>&1 ); then
        fail "变异 5(清空豁免表)应为红,实为绿 ⇒ 豁免项可能已不必要(应删掉该豁免)"; exit 1
    else pass "变异 5:清空豁免表 ⇒ 红(豁免确有必要)"; fi

    echo -e "\033[0;32m✅ selftest:5/5 变异全部变红(3 个判据面均有牙,豁免必要性已证)\033[0m"
    exit 0
fi

echo "══ docs_index_consistency_check（判据面 = docs/README.md §7）══"
if run_checks; then
    echo -e "\033[0;32m✅ 子目录索引一致性闸通过\033[0m"
    exit 0
else
    echo -e "\033[0;31m❌ 子目录索引一致性闸失败\033[0m"
    exit 1
fi
