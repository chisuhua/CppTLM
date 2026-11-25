#!/usr/bin/env bash
# scripts/test/cep_oswald_provenance_check.sh
# CEP × Gen 族 gap 分析的**论文侧溯源闸** —— 仓内可机检性守门
#
# 职责（判据逐条列，⛔ 「rc≠0」不构成红因；fixture 必须命中**自己的**红因签名）:
#   P-1  manifest schema 版本正确
#   P-2  extracts[] 逐条 sha256 == 仓内实际文件 sha256
#   P-3  claims[] 逐条：行区间存在 + must_contain 命中 + 锚点区间 sha256 匹配
#   P-4  claims 条数下界（防删条目 · 当前 24）
#   P-5  papers[] 已物化时 sha256 必须匹配；未物化 ⇒ **SKIP**（⛔ 不当绿）
#   P-6  paper-claims-index.md 的 claim id ↔ manifest **双向零 orphan**
#   P-7  抽取件存在且非空
#
# 背景: 论文原文存于**仓外**（mynotes 仓）⇒ 仓内可复核的是「抽取件 + 行级锚点」。
#       本闸即该可复核性的机器载体（per docs/research/oswald/AGENTS.md）。
#
# 作者: CppTLM Team / 日期: 2026-11-25
#
# 用法:
#   bash scripts/test/cep_oswald_provenance_check.sh              # 正常校验
#   bash scripts/test/cep_oswald_provenance_check.sh --root <dir>  # 对拷贝根校验
#   bash scripts/test/cep_oswald_provenance_check.sh --selftest    # 3 条反向 fixture
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

OSWALD_DIR="docs/research/oswald"
MANIFEST="$OSWALD_DIR/source-manifest.json"
INDEX_MD="$OSWALD_DIR/paper-claims-index.md"

RED=$'\033[1;31m'; GRN=$'\033[1;32m'; YEL=$'\033[1;33m'; DIM=$'\033[2m'; RST=$'\033[0m'
FAILED=0

pass() { echo "  ${GRN}[ OK ]${RST} $*"; }
fail() { echo "  ${RED}[FAIL]${RST} $*"; FAILED=1; }
skip() { echo "  ${YEL}[SKIP]${RST} $*"; }

# ── P-1/P-2/P-3/P-4/P-5/P-7：核心校验（python3） ───────────────────────────────
run_core() {
    python3 - "$OSWALD_DIR" <<'PY'
import hashlib, json, os, sys

oswald = sys.argv[1]
manifest_p = os.path.join(oswald, "source-manifest.json")
RED, GRN, YEL, DIM, RST = "\033[1;31m", "\033[1;32m", "\033[1;33m", "\033[2m", "\033[0m"
bad = 0

def sha256_of_range(path, a, b):
    lines = open(path, encoding="utf-8").read().split("\n")
    if a < 1 or b > len(lines) or b < a:
        return None
    return hashlib.sha256("\n".join(lines[a - 1:b]).encode()).hexdigest()

if not os.path.isfile(manifest_p):
    print(f"  {RED}[FAIL]{RST} 缺 manifest: {manifest_p}  (red: oswald-manifest-missing)")
    sys.exit(1)
m = json.load(open(manifest_p, encoding="utf-8"))

# P-1 schema
if m.get("schema") == "cep-oswald-source-manifest/v1":
    print(f"  {GRN}[ OK ]{RST} P-1 manifest schema = {m['schema']}")
else:
    print(f"  {RED}[FAIL]{RST} P-1 manifest schema 漂移: {m.get('schema')!r}  (red: oswald-schema-drift)")
    bad = 1

# P-7 抽取件存在且非空
for e in m.get("extracts", []):
    p = os.path.join(oswald, e["path"])
    if not os.path.isfile(p):
        print(f"  {RED}[FAIL]{RST} P-7 抽取件缺失: {e['path']}  (red: oswald-extract-missing)")
        bad = 1
    elif os.path.getsize(p) == 0:
        print(f"  {RED}[FAIL]{RST} P-7 抽取件为空: {e['path']}  (red: oswald-extract-empty)")
        bad = 1
if bad == 0:
    print(f"  {GRN}[ OK ]{RST} P-7 抽取件 {len(m.get('extracts', []))} 份全部存在且非空")

# P-2 抽取件 sha256
for e in m.get("extracts", []):
    p = os.path.join(oswald, e["path"])
    if not os.path.isfile(p):
        continue
    got = hashlib.sha256(open(p, "rb").read()).hexdigest()
    if got != e["sha256"]:
        print(f"  {RED}[FAIL]{RST} P-2 抽取件 sha256 漂移: {e['path']} 期望 {e['sha256'][:16]} 实得 {got[:16]}"
              f"  (red: oswald-extract-sha-drift)")
        bad = 1
if bad == 0:
    print(f"  {GRN}[ OK ]{RST} P-2 抽取件 sha256 全部匹配")

# P-4 claims 条数下界
claims = m.get("claims", [])
if len(claims) >= 24:
    print(f"  {GRN}[ OK ]{RST} P-4 claims 条数 {len(claims)} ≥ 24")
else:
    print(f"  {RED}[FAIL]{RST} P-4 claims 条数 {len(claims)} < 24（条目被删）  (red: oswald-claims-truncated)")
    bad = 1

# P-3 锚点逐条
for c in claims:
    p = os.path.join(oswald, c["file"])
    if not os.path.isfile(p):
        print(f"  {RED}[FAIL]{RST} P-3 锚点文件缺失: {c['id']} → {c['file']}  (red: oswald-anchor-missing)")
        bad = 1
        continue
    a, b = c["lines"]
    got = sha256_of_range(p, a, b)
    if got is None:
        print(f"  {RED}[FAIL]{RST} P-3 锚点行区间越界: {c['id']} L{a}-{b}  (red: oswald-anchor-out-of-range)")
        bad = 1
        continue
    if got != c["anchor_sha256"]:
        print(f"  {RED}[FAIL]{RST} P-3 锚点内容漂移: {c['id']} ({c['file']}:{a}-{b})  (red: oswald-anchor-drift)")
        bad = 1
        continue
    seg = "\n".join(open(p, encoding="utf-8").read().split("\n")[a - 1:b])
    if c["must_contain"] not in seg:
        print(f"  {RED}[FAIL]{RST} P-3 锚点关键字缺失: {c['id']} 缺 {c['must_contain']!r}  (red: oswald-anchor-keyword-missing)")
        bad = 1
if bad == 0:
    print(f"  {GRN}[ OK ]{RST} P-3 锚点 {len(claims)} 条全部命中（行区间 + 关键字 + sha256）")

# P-5 论文原文已物化 ⇒ sha256 必须匹配；未物化 ⇒ SKIP（⛔ 不当绿）
papers_dir = os.path.join(oswald, ".papers")
n_checked = n_skip = 0
for p in m.get("papers", []):
    dst = os.path.join(papers_dir, os.path.basename(p["materialized_path"]))
    if not os.path.isfile(dst):
        n_skip += 1
        continue
    got = hashlib.sha256(open(dst, "rb").read()).hexdigest()
    if got != p["sha256"]:
        print(f"  {RED}[FAIL]{RST} P-5 论文原文 sha256 漂移: {p['id']}  (red: oswald-paper-sha-drift)")
        bad = 1
    else:
        n_checked += 1
if n_checked:
    print(f"  {GRN}[ OK ]{RST} P-5 论文原文 {n_checked} 份已物化且 sha256 匹配")
else:
    print(f"  {YEL}[SKIP]{RST} P-5 论文原文未物化（.papers/ 为空）⇒ 原文同一性本机不可验"
          f"（⛔ 非绿；仓内可复核面 = 抽取件 + 锚点）")

sys.exit(1 if bad else 0)
PY
    return $?
}

# ── P-6：索引 ↔ manifest 双向零 orphan ───────────────────────────────────────
check_index_orphan() {
    local rc=0
    if [ ! -f "$INDEX_MD" ]; then
        fail "P-6 缺索引: $INDEX_MD  (red: oswald-index-missing)"; return 1
    fi
    local in_manifest in_index
    in_manifest="$(python3 -c "
import json;m=json.load(open('$MANIFEST',encoding='utf-8'))
print('\n'.join(c['id'] for c in m['claims']))")"
    in_index="$(grep -oE 'T-C[0-9]{2}|H-C[0-9]{2}' "$INDEX_MD" | sort -u)"
    local only_index only_manifest
    only_index="$(comm -13 <(echo "$in_manifest" | sort -u) <(echo "$in_index"))"
    only_manifest="$(comm -23 <(echo "$in_manifest" | sort -u) <(echo "$in_index"))"
    if [ -n "$only_index" ]; then
        fail "P-6 索引含 manifest 未登记的 claim id: $(echo $only_index)  (red: oswald-index-orphan)"; rc=1
    fi
    if [ -n "$only_manifest" ]; then
        fail "P-6 manifest 含索引未出现的 claim id: $(echo $only_manifest)  (red: oswald-claim-unindexed)"; rc=1
    fi
    [ $rc -eq 0 ] && pass "P-6 索引 ↔ manifest 双向零 orphan（$(echo "$in_index" | wc -l) 条）"
    return $rc
}

if [ "$SELFTEST" -eq 1 ]; then
    echo "══ cep_oswald_provenance_check --selftest(3 变异,跑在拷贝根)══"
    BASE="$(mktemp -d)"
    trap 'rm -rf "$BASE"' EXIT
    # 拷贝根必须保持**仓内相对布局**（--root 指向仓根，闸按 docs/research/oswald/ 相对定位）
    for v in base m1 m2 m3; do
        mkdir -p "$BASE/$v/docs/research"
        cp -r "$OSWALD_DIR" "$BASE/$v/docs/research/oswald"
    done
    ok=0; total=0

    if bash "${BASH_SOURCE[0]}" --root "$BASE/base" >/dev/null 2>&1; then
        echo "  ${GRN}✓${RST} 基线:拷贝根 rc=0"; ok=$((ok+1))
    else
        echo "  ${RED}✗${RST} 基线:拷贝根应为绿"; exit 1
    fi
    total=$((total+1))

    # 变异 1: 篡改一条锚点内容 ⇒ 必须红 oswald-anchor-drift
    rm -rf "$BASE/m1/docs/research/oswald"; cp -r "$BASE/base/docs/research/oswald" "$BASE/m1/docs/research/oswald"
    python3 - "$BASE/m1/docs/research/oswald/extracts/thesis-text-extract.md" <<'PY'
import sys
p=sys.argv[1]; L=open(p,encoding="utf-8").read().split("\n")
L[7165]=L[7165].replace("acyclic(","acyclicX(")   # 破坏 Eq 8.3 的 acyclic( 行（第 7166 行）
open(p,"w",encoding="utf-8").write("\n".join(L))
PY
    out="$(bash "${BASH_SOURCE[0]}" --root "$BASE/m1" 2>&1)"
    total=$((total+1))
    if echo "$out" | grep -q "oswald-anchor-drift"; then
        echo "  ${GRN}✓${RST} 变异1 篡改锚点内容 ⇒ 红因 oswald-anchor-drift"; ok=$((ok+1))
    else
        echo "  ${RED}✗${RST} 变异1 期望红因 oswald-anchor-drift"; echo "$out" | sed 's/^/      /'
    fi

    # 变异 2: 篡改抽取件 ⇒ 必须红 oswald-extract-sha-drift
    rm -rf "$BASE/m2/docs/research/oswald"; cp -r "$BASE/base/docs/research/oswald" "$BASE/m2/docs/research/oswald"
    printf "\n<!-- tamper -->\n" >> "$BASE/m2/docs/research/oswald/extracts/ProtoGen_ISCA2018-draft.md"
    out="$(bash "${BASH_SOURCE[0]}" --root "$BASE/m2" 2>&1)"
    total=$((total+1))
    if echo "$out" | grep -q "oswald-extract-sha-drift"; then
        echo "  ${GRN}✓${RST} 变异2 篡改抽取件 ⇒ 红因 oswald-extract-sha-drift"; ok=$((ok+1))
    else
        echo "  ${RED}✗${RST} 变异2 期望红因 oswald-extract-sha-drift"; echo "$out" | sed 's/^/      /'
    fi

    # 变异 3: 删一条 claim ⇒ 必须红 oswald-claims-truncated
    rm -rf "$BASE/m3/docs/research/oswald"; cp -r "$BASE/base/docs/research/oswald" "$BASE/m3/docs/research/oswald"
    python3 - "$BASE/m3/docs/research/oswald/source-manifest.json" <<'PY'
import json,sys
p=sys.argv[1]; m=json.load(open(p,encoding="utf-8"))
m["claims"]=m["claims"][:5]
json.dump(m,open(p,"w",encoding="utf-8"),ensure_ascii=False,indent=1)
PY
    out="$(bash "${BASH_SOURCE[0]}" --root "$BASE/m3" 2>&1)"
    total=$((total+1))
    if echo "$out" | grep -q "oswald-claims-truncated"; then
        echo "  ${GRN}✓${RST} 变异3 删 claim 条目 ⇒ 红因 oswald-claims-truncated"; ok=$((ok+1))
    else
        echo "  ${RED}✗${RST} 变异3 期望红因 oswald-claims-truncated"; echo "$out" | sed 's/^/      /'
    fi

    echo
    if [ "$ok" -eq "$total" ]; then
        echo "${GRN}✅ selftest:${ok}/${total} 变异全部按**预期红因**变红${RST}"
        exit 0
    fi
    echo "${RED}❌ selftest 存在未按预期变红的判据${RST}"
    exit 1
fi

echo "══ CEP 论文溯源闸（oswald provenance）══"
run_core
core_rc=$?
[ $core_rc -ne 0 ] && FAILED=1
check_index_orphan || FAILED=1

echo
if [ "$FAILED" -eq 0 ]; then
    echo "${GRN}✅ 论文溯源闸全绿${RST}"
    exit 0
fi
echo "${RED}❌ 论文溯源闸存在红项${RST}"
exit 1
