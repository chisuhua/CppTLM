#!/usr/bin/env bash
# scripts/test/cep_registry_stale_refs_check.sh
#   —— 登记册「旧口径残留串」的反向判据闸（§6.3.2 的机器化）
#
# per 登记册 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6.4 的 **W2-b**
# （承载缺口 **G-10**「剩余 grep 判据未接闸」）。
#
# ── 为什么需要本闸 ────────────────────────────────────────────────
# §6.3.2 列了 15 条「裁决已落地 ⇒ 旧口径串应 == 0」的反向判据，但 v5 时它们
# **只写在文档里、靠人跑**。历史教训：本轮三处传播缺口（D-b / D-c / D-d）**全部**
# 是「上游已改、下游漏改」⇒ 上游写「✅ 已回填」根本防不住。⇒ 判据必须**进闸**。
#
# ── 关键设计（三条，缺一则闸会「抓不到真问题」或「抓到自己」）──────
# ① **排除 §0.2 历史节**：它是 2026-11-16 口径快照、逐字保留 ⇒ 天然含旧计数
#    ⇒ 不排除就会**假红**（闸族 45 / 测试 73 两串都在那里）。
# ② **排除 §6.3.2 及之后**：该节是**判据的定义面**（旧串必须出现在那里才对）
#    ⇒ 不排除就会**抓到自己**。⚠️ 该排除同时覆盖尾部「v5 实测依据」块（历史快照）。
# ③ **MARK 词表排除「订正引述」行**：订正记录必然引用旧串（「旧表述为假」「初稿写…」）
#    ⇒ 只排 ①② 仍会把**订正记录本身**判红。词表固定在下（⛔ 不留在文档里靠人判，
#    per §6.6 P-12「反向判据必须带排除订正引述的机检」）。
#
# ── 边界（刻意不做）──────────────────────────────────────────────
# ⛔ 不校验「旧串出现在哪些文件的全部位置」—— 只校验 §6.3.2 点名的那几个文件
#    （判据的**域**是该表定义的，扩大域会让闸抓到的都是判据没要求的东西）。
# ⛔ 不替代 §6.6 的人工复核 —— 本闸只判「旧串是否回来」。
#
# 用法：
#   bash scripts/test/cep_registry_stale_refs_check.sh
#   bash scripts/test/cep_registry_stale_refs_check.sh --registry <路径>   # 变异测试
#
# 退出码：0 = 全部为 0 · 1 = 有残留 · 2 = 输入缺失

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[0;33m'; RST=$'\033[0m'

REGISTRY=".rddf/roadmap/objectives/obj-cep-platform-remaining.md"
while [ $# -gt 0 ]; do
    case "$1" in
        --registry) REGISTRY="${2:?--registry 需要路径}"; shift 2 ;;
        -h|--help) sed -n '1,40p' "$0"; exit 0 ;;
        *) echo -e "${RED}未知参数: $1${RST}"; exit 2 ;;
    esac
done

[ -f "$REGISTRY" ] || { echo -e "${RED}✗ 缺登记册: $REGISTRY${RST}"; exit 2; }

# ⚠️ **排除窗必须自身可验证**（否则「删掉标记行」就能让判据失效）：
#    BODY 范围依赖尾部快照块标记 `**v5 的实测依据`；该标记不存在 ⇒ 结构错误。
#    该块是 v4 字节口径的**历史快照**（其字节/字符误标属 **G-7 / W4-e** 的残余，
#    ⛔ 本闸不代裁、也不在它内部判红）⇒ 但**不许把它删掉来消音**。
if ! grep -q '^\*\*v5 的实测依据' "$REGISTRY"; then
    echo -e "${RED}✗ STRUCTURE-ERROR: 未找到尾部快照块标记 '**v5 的实测依据'${RST}"
    echo "    BODY 范围的排除窗依赖它（⛔ 不得靠删除标记来绕过判据）"
    exit 2
fi

# 订正标记词表（§6.3.3 的排除约定 · 固定在此，⛔ 不得散落在文档里靠人判）
MARK='已废|为假|旧表述|拍板时|初稿|订正|已移除|已统一|不可执行|白名单'

# 范围定义（按「现行面」精确切窗；⛔ 不整段排除到 EOF —— 那会让 §6.4 的规则空转）
# ① BODY = 全文 − §0.2 历史节 − §6.3.2/§6.3.3（判据定义面）− 尾部「v5 实测依据」快照块
# ② S01  = 仅 §0.1（现行基线表）
# ③ S64  = 仅 §6.4（队列 0/1/2 + W1~W4 + 编号空间表）
# ④ S3   = 仅 §3（历史开放问题节 · `R-*` 编号空间）
scope_body() {
    sed -e '/^### §0\.2/,/^## §1 /d' \
        -e '/^#### §6\.3\.2/,/^### §6\.5 /d' \
        -e '/^\*\*v5 的实测依据/,$d' "$REGISTRY"
}
scope_01() { sed -n '/^### §0\.1/,/^### §0\.2/p' "$REGISTRY"; }
scope_64() { sed -n '/^#### 队列 0 /,/^### §6\.5 /p' "$REGISTRY"; }
scope_section3() { sed -n '/^## §3 /,/^## §4 /p' "$REGISTRY"; }

FAIL=0
PASS=0

report_pass() { PASS=$((PASS + 1)); printf "  ${GRN}✓${RST} %-46s %s\n" "$1" "$2"; }
report_fail() {
    FAIL=$((FAIL + 1))
    printf "  ${RED}✗${RST} %-46s %s\n" "$1" "$2"
    shift 2
    printf '      %s\n' "$@"
}

# chk <标签> <字面串> <SCOPE | SECTION3 | 文件路径>
chk() {
    local label="$1" needle="$2" target="$3" raw hits
    case "$target" in
        BODY)      raw="$(scope_body      | grep -F -- "$needle" || true)" ;;
        S01)       raw="$(scope_01        | grep -F -- "$needle" || true)" ;;
        S64)       raw="$(scope_64        | grep -F -- "$needle" || true)" ;;
        SECTION3)  raw="$(scope_section3  | grep -F -- "$needle" || true)" ;;
        *)         raw="$(grep -F -- "$needle" "$target" 2>/dev/null || true)" ;;
    esac
    if [ -z "$raw" ]; then report_pass "$label" "(0)"; return 0; fi
    hits="$(printf '%s\n' "$raw" | grep -vE "$MARK" || true)"
    if [ -z "$hits" ]; then
        report_pass "$label" "(0 · 余下均为订正引述)"
    else
        local n; n="$(printf '%s\n' "$hits" | grep -c . || true)"
        report_fail "$label" "残留 ${n} 行" "$hits"
    fi
}

# chk_re <标签> <正则> <SCOPE | SECTION3 | 文件路径>
chk_re() {
    local label="$1" pattern="$2" target="$3" raw hits
    case "$target" in
        BODY)      raw="$(scope_body      | grep -E -- "$pattern" || true)" ;;
        S01)       raw="$(scope_01        | grep -E -- "$pattern" || true)" ;;
        S64)       raw="$(scope_64        | grep -E -- "$pattern" || true)" ;;
        SECTION3)  raw="$(scope_section3  | grep -E -- "$pattern" || true)" ;;
        *)         raw="$(grep -E -- "$pattern" "$target" 2>/dev/null || true)" ;;
    esac
    if [ -z "$raw" ]; then report_pass "$label" "(0)"; return 0; fi
    hits="$(printf '%s\n' "$raw" | grep -vE "$MARK" || true)"
    if [ -z "$hits" ]; then
        report_pass "$label" "(0 · 余下均为订正引述)"
    else
        local n; n="$(printf '%s\n' "$hits" | grep -c . || true)"
        report_fail "$label" "残留 ${n} 行" "$hits"
    fi
}

echo -e "${YLW}[cep-registry-stale-refs] §6.3.2 旧口径残留串扫描（反向判据）${RST}"
echo "  登记册 = $REGISTRY"
echo "  范围     = §0.1..§6.4（排除 §0.2 历史节 · §6.3.2/§6.3.3 判据定义面 · 尾部快照块）"
echo -e "  ${YLW}ℹ${RST} 尾部「v5 实测依据」块按**历史快照**排除；其中字节/字符误标属 G-7 / W4-e 残余（⛔ 本闸不代裁）"

# ── Q-8 / Q-13 扩容 ──────────────────────────────────────────────
chk "Q-8/Q-13 · 乙层 5 份" '乙层 5 份' BODY
chk "Q-8/Q-13 · 乙层 5 份全补" '乙层 5 份全补' BODY
chk "Q-8/Q-13 · d1 单元" '乙层 5 份' .rddf/improvements/cep-d1-archive-verify-backfill.md
chk "Q-8/Q-13 · d1 单元（全补）" '乙层 5 份全补' .rddf/improvements/cep-d1-archive-verify-backfill.md

# ── Q-8 成本估时 ─────────────────────────────────────────────────
chk "Q-8 · 成本估时 ≈3~4 d" '≈3~4 d' BODY
chk "Q-8 · 成本估时（d1 单元）" '≈3~4 d' .rddf/improvements/cep-d1-archive-verify-backfill.md

# ── D-1 口径 ────────────────────────────────────────────────────
chk "D-1 · 43 份" '43 份' BODY
chk "D-1 · 甲层 35 份" '甲层 35 份' BODY
chk "D-1 · 甲层 35 份（d1 单元）" '甲层 35 份' .rddf/improvements/cep-d1-archive-verify-backfill.md

# ── D-1 理由 ────────────────────────────────────────────────────
chk "D-1 · 最早 verify 报告 = 2026-11-12" '最早 verify 报告 = 2026-11-12' BODY
chk "D-1 · 无该实践" '无该实践' BODY
chk "D-1 · 最早 verify 报告（d1 单元）" '最早 verify 报告 = 2026-11-12' \
    .rddf/improvements/cep-d1-archive-verify-backfill.md

# ── §0.1 数字 ───────────────────────────────────────────────────
chk '§0.1 · **40**（50 CEP 归档' '**40**（50 CEP 归档' S01

# ── Q-9 三分 ────────────────────────────────────────────────────
chk "Q-9 · A 类 4 项" 'A 类 4 项' BODY
chk "Q-9 · B 类 3 项" 'B 类 3 项' BODY
for f in .rddf/improvements/cep-doc-hygiene-kpi-and-impl-state.md \
         .rddf/improvements/cep-registry-stale-cleanup.md; do
    chk "Q-9 · A 类 4 项（$(basename "$f")）" 'A 类 4 项' "$f"
    chk "Q-9 · B 类 3 项（$(basename "$f")）" 'B 类 3 项' "$f"
done

# ── Q-10 状态 ───────────────────────────────────────────────────
chk "Q-10 · 待 user 决策" '待 user 决策' .rddf/improvements/cep-archforge-ci-failclosed.md

# ── 编号空间（§3 表体首列）──────────────────────────────────────
chk_re "编号空间 · §3 的 **Q-1**~**Q-7**" '\*\*Q-[1-7]\*\*' SECTION3

# ── 队列落地 ────────────────────────────────────────────────────
chk "队列落地 · I-0~I-9" 'I-0~I-9' S64

# ── v5 增补的计数口径 ───────────────────────────────────────────
chk 'v5 · 闸族 **45** 个 `cep_*.sh`' '**45** 个 `cep_*.sh`' S01
chk 'v5 · 测试 **73** 个 `test_cep_*.cc`' '**73** 个 `test_cep_*.cc`' S01
chk 'v5 · 归档 **164**（CEP **50**）' '**164**（CEP **50**）' S01
chk 'v5 · spec **137** 份（`cep-*` **46**' '**137** 份（`cep-*` **46**' S01

# ── v5 · S3-6 字节口径（5 条）────────────────────────────────────
for s in '3091 字符' '3116 字符' '3015 字符' '2921 字符' '2761 字符'; do
    chk "v5 · 字节误标为字符（$s）" "$s" BODY
done

# ── v5 · Q-3 落地态（结构判据）──────────────────────────────────
if scope_64 | grep -F '**Q-3**' | grep -F '未落地' | grep -vE "$MARK" >/dev/null 2>&1; then
    report_fail "v5 · Q-3 标「未落地」" "残留" \
        "$(scope_64 | grep -F '**Q-3**' | grep -F '未落地' | grep -vE "$MARK")"
else
    report_pass "v5 · Q-3 标「未落地」" "(0)"
fi

# ── 队列 1 状态不得漂移（Q-1~Q-7 / Q-11 的行必须含「已裁」）────
for q in 1 2 3 4 5 6 7 11; do
    n="$(scope_64 | grep -E "^\| \*\*Q-${q}\*\* " | grep -c '已裁' || true)"
    if [ "${n:-0}" -ge 1 ]; then
        report_pass "队列 1 · Q-${q} 行含「已裁」" "(1)"
    else
        report_fail "队列 1 · Q-${q} 行含「已裁」" "0 命中 ⇒ 误标待裁" \
            "$(scope_64 | grep -E "^\| \*\*Q-${q}\*\* " || true)"
    fi
done

echo
if [ "$FAIL" -gt 0 ]; then
    echo -e "${RED}旧口径残留：${FAIL} 项红 / ${PASS} 项绿${RST}"
    exit 1
fi
echo -e "${GRN}旧口径残留：${PASS} 项全部为 0（订正引述已按 MARK 词表排除）${RST}"
exit 0
