#!/usr/bin/env bash
# scripts/test/docs_adr_index_check.sh
# **ADR 索引不腐化闸** —— `docs/adr/README.md` 的「议题 / 状态 / 份数」三列
#
# 判据面(⚠️ 唯一真源 = `docs/adr/README.md` §7「抽取契约(判据面)」;⛔ 本闸不自创口径):
#   ① **逐系列计数**:磁盘 `ADR-*.md` 计数 == 索引表行数 == §1 统计表该行「份数」cell
#      == 合计 cell(⛔ 只查合计会漏检「加了行忘改两个 cell」)
#   ② **逐份状态**:每份 ADR 的索引「状态」== 按 §7.1~§7.2 对其本体的**确定性抽取**
#      (⚠️ 比的是「索引格 vs **同一抽取函数**的结果」⇒ 抽取函数唯一即判据唯一)
#   ③ **生成器 `--check`** rc=0(幂等 + 无漂移)
#
# ⚠️ **为什么需要它**:`docs/adr/README.md` 曾声称「机械抽取、零手抄」而生成器**不在仓内**
#    ⇒ 索引靠手工同步。同型失败已在 `07 号` 实测过一次(台账 Entry #342:5 个 `## ` 未列入仍绿、
#    行号 10/23 错)。⛔ 无本闸则「新增 ADR 忘登记」「ADR 状态改了索引没同步」都会**静默**发生。
#
# 用法:
#   bash scripts/test/docs_adr_index_check.sh              # 正常校验
#   bash scripts/test/docs_adr_index_check.sh --root <dir>  # 对拷贝根校验(供 --selftest)
#   bash scripts/test/docs_adr_index_check.sh --selftest    # 4 个决定性变异(跑在拷贝根,零工作树污染)
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

DOC="docs/adr/README.md"
GEN="scripts/docs/gen_adr_index.py"

pass() { echo -e "  \033[0;32m✓\033[0m $1"; }
fail() { echo -e "  \033[0;31m✗\033[0m $1"; }

# ── 三合一核心(判据 ①② + 生成器存在性)──────────────────────────────────────
run_checks() {
    local rc=0
    # [1/3] 判据① 逐系列计数 —— 期望值来源 = 磁盘 glob,⛔ 不取索引自述
    if ! python3 - "$DOC" <<'PY'
import re, sys, glob
from pathlib import Path
doc = Path(sys.argv[1])
if not doc.is_file():
    print("  ✗ 文档不存在:", doc); sys.exit(1)
text = doc.read_text(encoding="utf-8")
# 生成区间
b = text.find("<!-- BEGIN ADR INDEX (generated) -->")
e = text.find("<!-- END ADR INDEX (generated) -->")
if b < 0 or e < 0 or e < b:
    print("  ✗ 未找到生成区间标记(BEGIN/END)"); sys.exit(1)
region = text[b:e]

def series(n):
    if n.startswith("ADR-CEP-"): return "CEP"
    if n.startswith("ADR-DGPU-"): return "DGPU"
    if n.startswith("ADR-X"): return "X"
    return "其他"
ORDER = ["CEP", "DGPU", "X", "其他"]

on_disk = {}
for p in glob.glob("docs/adr/ADR-*.md"):
    s = series(Path(p).name)
    on_disk[s] = on_disk.get(s, 0) + 1

# §2~§5 各表行数
table_rows = {}
sec_re = re.compile(r"^## ([2-5])\. .*$", re.M)
heads = [(m.start(), m.group(1)) for m in sec_re.finditer(region)]
heads.append((len(region), "EOF"))
NUM2SER = {"2": "CEP", "3": "DGPU", "4": "X", "5": "其他"}
for i in range(len(heads) - 1):
    seg = region[heads[i][0]:heads[i + 1][0]]
    s = NUM2SER[heads[i][1]]
    table_rows[s] = len(re.findall(r"^\| \[ADR-[^\]]+\.md\]\(\./", seg, re.M))

# §1 统计表 cell(份数列)
cells = {}
total_cell = None
# §1 统计表的 5 行:4 个系列行(标签形态不一:「**CEP 系列**…」「**X 系列**…」「**其他**…」)
for m in re.finditer(r"^\| \*\*([^*]+)\*\*[^|]*\| (\d+) \|", region, re.M):
    k = m.group(1).strip()
    if k == "合计":
        continue
    if "CEP" in k:       cells["CEP"] = int(m.group(2))
    elif "DGPU" in k:    cells["DGPU"] = int(m.group(2))
    elif k.startswith("X"): cells["X"] = int(m.group(2))
    elif "其他" in k:    cells["其他"] = int(m.group(2))
tm = re.search(r"^\| \*\*合计\*\* \| \*\*(\d+)\*\* \|", region, re.M)
total_cell = int(tm.group(1)) if tm else None

ok = True
for s in ORDER:
    d = on_disk.get(s, 0); t = table_rows.get(s, 0)
    c = cells.get(s)
    if d != t or d != c:
        print(f"  ✗ 判据① [{s}]:磁盘={d} 索引表行={t} 统计表cell={c} ⇒ 应全部相等")
        ok = False
if total_cell != sum(on_disk.values()):
    print(f"  ✗ 判据① [合计]:合计cell={total_cell} 磁盘总数={sum(on_disk.values())}")
    ok = False
if ok:
    print(f"  ✓ 判据① 逐系列计数一致({len(ORDER)} 系列 / 合计 {sum(on_disk.values())})")
sys.exit(0 if ok else 1)
PY
    then rc=1; fi

    # [2/3] 判据② 逐份状态
    if ! python3 - "$DOC" "$GEN" <<'PY'
import json, re, subprocess, sys
from pathlib import Path
doc, gen = Path(sys.argv[1]), sys.argv[2]
try:
    out = subprocess.run([sys.executable, gen, "--json"], capture_output=True, text=True)
except Exception as ex:
    print("  ✗ 生成器无法执行:", ex); sys.exit(1)
if out.returncode != 0:
    print("  ✗ 生成器 §7 抽取失败(fail loud):")
    print((out.stderr or "").strip()[:600]); sys.exit(1)
exp = {r["file"]: r["status"] for r in json.loads(out.stdout)["rows"]}
text = doc.read_text(encoding="utf-8")
got = dict(re.findall(r"^\| \[(ADR-[^\]]+\.md)\]\(\./[^)]+\) \| [^|]+ \| ([^|]+) \|$", text, re.M))
got = {k: v.strip() for k, v in got.items()}
bad = [f for f in exp if got.get(f) != exp[f]]
missing = [f for f in exp if f not in got]
extra = [f for f in got if f not in exp]
for f in missing[:5]: print(f"  ✗ 判据② [{f}]:索引缺该行")
for f in extra[:5]:   print(f"  ✗ 判据② [{f}]:索引有该行但磁盘无")
for f in bad[:5]:     print(f"  ✗ 判据② [{f}]:索引={got[f]!r} ≠ 抽取={exp[f]!r}")
ok = not (bad or missing or extra)
if ok:
    print(f"  ✓ 判据② 逐份状态一致({len(exp)} 份)")
sys.exit(0 if ok else 1)
PY
    then rc=1; fi

    # [3/3] 判据③ 生成器 --check
    if python3 "$GEN" --check >/dev/null 2>&1; then
        pass "判据③ 生成器 --check rc=0（幂等 + 无漂移）"
    else
        fail "判据③ 生成器 --check 报漂移 ⇒ 跑 --write"
        rc=1
    fi
    return $rc
}

# ── --selftest:4 个决定性变异(拷贝根,零工作树污染)────────────────────────────
if [ "$SELFTEST" -eq 1 ]; then
    echo "══ docs_adr_index_check --selftest(4 变异,跑在拷贝根)══"
    BASE="$(mktemp -d)"
    trap 'rm -rf "$BASE"' EXIT
    copy_root() {
        local d="$1"; rm -rf "$d"; mkdir -p "$d/docs/adr" "$d/scripts/docs"
        cp docs/adr/ADR-*.md "$d/docs/adr/" 2>/dev/null || return 1
        cp docs/adr/README.md "$d/docs/adr/"
        cp "$GEN" "$d/scripts/docs/"
        cp scripts/docs/gen_adr_index.py "$d/scripts/docs/" 2>/dev/null || true
    }
    # 基线：拷贝根必须绿
    copy_root "$BASE/base"
    if ( cd "$BASE/base" && bash "$REPO_ROOT/scripts/test/docs_adr_index_check.sh" --root "$BASE/base" >/dev/null 2>&1 ); then
        pass "基线:拷贝根 rc=0"
    else
        fail "基线:拷贝根应为绿"; exit 1
    fi
    # 变异 1:改 1 份 ADR 状态 ⇒ 判据② 红
    copy_root "$BASE/m1"
    # ⚠️ 观测:契约是「取**最后一个**词表命中」⇒ 必须让变异成为**最后一个** token
    #    (前置插入「起草中」会被行内后出现的「已签发」覆盖 ⇒ 抽取不变 ⇒ 变异无效)
    python3 - "$BASE/m1/docs/adr/ADR-CEP-01-fabric-collective-plane.md" <<'PYMUT'
import re, sys
p = sys.argv[1]; lines = open(p, encoding="utf-8").read().split("\n")
for i, l in enumerate(lines):
    if re.match(r"^\s*>?\s*\*\*\s*(?:状态|Status)\s*\*\*\s*[:：]", l):
        lines[i] = l.rstrip() + " 🟡 起草中（变异追加）"
        break
open(p, "w", encoding="utf-8").write("\n".join(lines))
PYMUT
    if ( cd "$BASE/m1" && bash "$REPO_ROOT/scripts/test/docs_adr_index_check.sh" --root "$BASE/m1" >/dev/null 2>&1 ); then
        fail "变异 1(改 ADR 状态)应为红,实为绿 ⇒ 判据② 无牙"; exit 1
    else pass "变异 1:改 ADR 状态 ⇒ 红"; fi
    # 变异 2:加 1 份未登记 ADR ⇒ 判据① 红
    copy_root "$BASE/m2"
    sed 's/ADR-TEST-01/X/' "$BASE/m2/docs/adr/ADR-DGPU-01-callback-worker-replaces-detached-threads.md" > "$BASE/m2/docs/adr/ADR-DGPU-99-mutation-test.md"
    sed -i '1s/^# .*/# ADR-DGPU-99: mutation probe/' "$BASE/m2/docs/adr/ADR-DGPU-99-mutation-test.md"
    if ( cd "$BASE/m2" && bash "$REPO_ROOT/scripts/test/docs_adr_index_check.sh" --root "$BASE/m2" >/dev/null 2>&1 ); then
        fail "变异 2(加未登记 ADR)应为红,实为绿 ⇒ 判据① 无牙"; exit 1
    else pass "变异 2:加未登记 ADR ⇒ 红"; fi
    # 变异 3:删 1 行索引 ⇒ 判据① 红
    copy_root "$BASE/m3"
    python3 - "$BASE/m3/docs/adr/README.md" <<'PY'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
t = re.sub(r"^\| \[ADR-DGPU-01-[^\n]*\n", "", t, count=1, flags=re.M)
open(p, "w", encoding="utf-8").write(t)
PY
    if ( cd "$BASE/m3" && bash "$REPO_ROOT/scripts/test/docs_adr_index_check.sh" --root "$BASE/m3" >/dev/null 2>&1 ); then
        fail "变异 3(删索引行)应为红,实为绿 ⇒ 判据① 无牙"; exit 1
    else pass "变异 3:删 1 行索引 ⇒ 红"; fi
    # 变异 4:改合计 cell 不改表 ⇒ 判据① 红
    # ⚠️ **不得写死合计数**（2026-11-24 实测事故：ADR 落盘把合计从 63 改成 64 ⇒
    #    硬编码的 `replace` **失配** ⇒ 变异退化为**空操作** ⇒ 文件未被改 ⇒
    #    判据仍绿 ⇒ selftest 报「变异 4 应为红,实为绿 ⇒ 判据① 无牙」——
    #    ⛔ 这类「变异失效」会被误读成**判据无牙**，实为**变异自身失效**）。
    # ⇒ 修法：**读当前值并 +1**（与实际份数无关）；且**形态未命中 ⇒ 显式失败**
    #   （⛔ 不得静默跳过 ⇒ 否则退化为「空变异必绿」）。
    copy_root "$BASE/m4"
    python3 - "$BASE/m4/docs/adr/README.md" <<'PY'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
m = re.search(r"^\|\s*\*\*合计\*\*\s*\|\s*\*\*(\d+)\*\*\s*\|", t, flags=re.M)
if not m:
    print("变异 4 前置失败:未命中 README.md 的「合计」cell 形态 ⇒ 变异会退化为空操作", file=sys.stderr)
    sys.exit(3)
t = t[:m.start(1)] + str(int(m.group(1)) + 1) + t[m.end(1):]
open(p, "w", encoding="utf-8").write(t)
PY
    if [ $? -ne 0 ]; then
        fail "变异 4 前置失败:合计 cell 形态未命中 ⇒ 变异退化为空操作(⛔ 不得当作通过)"; exit 1
    fi
    if ( cd "$BASE/m4" && bash "$REPO_ROOT/scripts/test/docs_adr_index_check.sh" --root "$BASE/m4" >/dev/null 2>&1 ); then
        fail "变异 4(改合计 cell)应为红,实为绿 ⇒ 判据① 无牙"; exit 1
    else pass "变异 4:改合计 cell 不改表 ⇒ 红"; fi
    echo -e "\033[0;32m✅ selftest:4/4 变异全部变红(4 个判据面均有牙)\033[0m"
    exit 0
fi

echo "══ docs_adr_index_check（判据面 = docs/adr/README.md §7）══"
if [ ! -f "$GEN" ]; then fail "生成器缺失: $GEN"; exit 1; fi
if run_checks; then
    echo -e "\033[0;32m✅ ADR 索引不腐化闸通过\033[0m"
    exit 0
else
    echo -e "\033[0;31m❌ ADR 索引不腐化闸失败\033[0m"
    exit 1
fi
