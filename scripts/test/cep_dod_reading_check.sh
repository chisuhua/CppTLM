#!/usr/bin/env bash
# scripts/test/cep_dod_reading_check.sh
#   —— C4「DoD 读法口径」判据闸的**驱动壳**(基线正控 + 10 条决定性变异 + 还原)
#
# 用法:
#   bash scripts/test/cep_dod_reading_check.sh            # 基线 + 变异(退出码 0 = 绿)
#   bash scripts/test/cep_dod_reading_check.sh --list     # 只打印判据清单
#
# 判据见 `scripts/test/cep_dod_reading_check.py` 头注 / design.md §D-5;签名表见 §D-4。
#
# **10 条变异**(design §D-6;⛔ 全部只作用于**影子树**,⛔ 不动真实仓):
#   M1  A2 复制到第 2 个文件            ⇒ 必须按 `新文本非唯一命中` 红(AC-7 ①)
#   M2  手册第 3 条还原为旧文            ⇒ 必须按 `旧文本仍存在` 红(AC-7 ②)
#   M3  删掉 SU ② 的作用域句(留禁令句)  ⇒ 必须按 `作用域子句缺失` 红(AC-7 ③)
#   M4  植入无作用域的绝对禁令           ⇒ 必须按 `绝对禁令复活` 红(AC-7 ④)
#   M5  A1 行**行内追加**(而非新增 bullet) ⇒ 必须按 `append-only 违规` 红(AC-7 ⑤)
#   M6~M10 植入 5 条禁表述各 1 条        ⇒ 必须按 `禁表述复活` 红(AC-8 ③)
#
# ⚠️ **每条变异都必须**:基线正控绿 → 破坏 ⇒ **按正确原因红(签名精确匹配)** → 还原 ⇒ 绿
#    (⛔ 「rc≠0」不构成红因;⛔ 不以「文件消失」才红)
#
# ⚠️ **影子树**复用共享 helper `scripts/test/cep_test_shadow.sh`(⛔ 自造复制逻辑);
#    本闸需要 3 份语料 ⇒ 由 `cep_shadow_reset` 的 `extra_cp...` 参数注入。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

CHECK="scripts/test/cep_dod_reading_check.py"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-dod-reading}"
LOG="${SHADOW_LOG:-/tmp/opencode/cep-dod-reading-last.log}"

DOC07="docs/architecture/07-coherence-exploration-platform.md"
GUIDE="docs/guide/cep-usage.md"
OBJ=".rddf/roadmap/objectives/obj-cep-platform.md"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

source "$(dirname "${BASH_SOURCE[0]}")/cep_test_shadow.sh"

if [[ "${1:-}" == "--list" ]]; then
    exec python3 "$CHECK" --list
fi

# ── 常量取自闸模块(⛔ 本壳不复写锚文本)──────────────────────────────────
const() { python3 -c "import sys; sys.path.insert(0,'scripts/test'); import cep_dod_reading_check as m; sys.stdout.write(getattr(m,'$1'))"; }
A1LINE="$(const A1)"
A2LINE="$(const A2)"
A3LINE="$(const A3)"
A3OLDLINE="$(const A3_OLD)"

echo "═══ C4 · DoD 读法口径判据闸(7 组判据 + 10 条决定性变异)═══"

# ── 0. 基线正控(真实仓)──────────────────────────────────────────────────
if python3 "$CHECK" >"$LOG" 2>&1; then
    ok "基线正控(真实仓)rc=0"
else
    ng "基线正控(真实仓)失败"
    sed 's/^/      /' "$LOG" | head -6
    echo "⇒ 基线不绿,变异无意义 ⇒ 中止"
    exit 1
fi

# ── 影子树 ───────────────────────────────────────────────────────────────
# 变异前后**真实仓语料指纹**(⛔ 不要求工作区干净 —— 实施期本来就有未提交改动)
fp_real() { sha256sum "$DOC07" "$GUIDE" "$OBJ" | awk '{print $1}' | tr '\n' ' '; }
FP_BEFORE="$(fp_real)"

reset_shadow() { cep_shadow_reset "$SHADOW" "$DOC07" "$GUIDE" "$OBJ"; }
run_shadow() { python3 "$CHECK" --root "$SHADOW" 2>&1; }

expect_red() {  # $1 = 变异名 · $2 = 期望签名
    local out rc
    out="$(run_shadow)"; rc=$?
    printf '%s\n' "$out" >"$LOG"
    if [[ $rc -eq 1 ]] && grep -qF "$2 :: " <<<"$out"; then
        ok "$1 ⇒ 红(签名「$2」)"
    else
        ng "$1 ⇒ 未按「$2」红(rc=$rc)"
        sed 's/^/      /' "$LOG" | head -4
    fi
}

expect_green() {  # $1 = 说明
    local out rc
    out="$(run_shadow)"; rc=$?
    if [[ $rc -eq 0 ]]; then
        ok "$1"
    else
        ng "$1(rc=$rc)"
        printf '%s\n' "$out" | head -4 | sed 's/^/      /'
    fi
}

plant_in_su() {  # $1 = 要植入 SU 块首行的文本
    python3 - "$SHADOW/$DOC07" "$1" <<'PY'
import sys
p, phrase = sys.argv[1], sys.argv[2]
lines = open(p, encoding="utf-8").read().split("\n")
idx = next(i for i, l in enumerate(lines) if l.startswith("## Status Update 2026-11-13"))
lines.insert(idx + 1, "- " + phrase)
open(p, "w", encoding="utf-8").write("\n".join(lines))
PY
}

# ── M1:A2 复制到第 2 个文件 ─────────────────────────────────────────────
reset_shadow
printf '%s\n' "$A2LINE" >"$SHADOW/test/c4_dup.md"
expect_red "M1 复制 A2 到第 2 文件" "新文本非唯一命中"
reset_shadow; expect_green "M1 还原后绿"

# ── M2:手册还原为旧文 ───────────────────────────────────────────────────
reset_shadow
python3 - "$SHADOW/$GUIDE" "$A3LINE" "$A3OLDLINE" <<'PY'
import sys
p, new, old = sys.argv[1], sys.argv[2], sys.argv[3]
lines = open(p, encoding="utf-8").read().split("\n")
hits = [i for i, l in enumerate(lines) if l == new]
assert len(hits) == 1, f"手册新文命中 {len(hits)} 次"
lines[hits[0]] = old
open(p, "w", encoding="utf-8").write("\n".join(lines))
PY
expect_red "M2 手册还原旧文" "旧文本仍存在"
reset_shadow; expect_green "M2 还原后绿"

# ── M3:删掉 SU ② 的作用域句(保留禁令句)──────────────────────────────────
reset_shadow
python3 - "$SHADOW/$DOC07" "$A2LINE" <<'PY'
import sys
p, a2 = sys.argv[1], sys.argv[2]
lines = open(p, encoding="utf-8").read().split("\n")
hits = [i for i, l in enumerate(lines) if l == a2]
assert len(hits) == 1, f"A2 命中 {len(hits)} 次"
del lines[hits[0]]
open(p, "w", encoding="utf-8").write("\n".join(lines))
PY
expect_red "M3 删作用域句(留禁令句)" "作用域子句缺失"
reset_shadow; expect_green "M3 还原后绿"

# ── M4:植入无作用域的绝对禁令 ───────────────────────────────────────────
reset_shadow
plant_in_su "⛔ **B3b-2 不得改写为「已达成」**"
expect_red "M4 植入无作用域绝对禁令" "绝对禁令复活"
reset_shadow; expect_green "M4 还原后绿"

# ── M5:A1 行内追加(而非新增 bullet)─────────────────────────────────────
reset_shadow
python3 - "$SHADOW/$OBJ" "$A1LINE" <<'PY'
import sys
p, a1 = sys.argv[1], sys.argv[2]
lines = open(p, encoding="utf-8").read().split("\n")
hits = [i for i, l in enumerate(lines) if l == a1]
assert len(hits) == 1, f"A1 命中 {len(hits)} 次"
lines[hits[0]] = a1 + " (追加)"
open(p, "w", encoding="utf-8").write("\n".join(lines))
PY
expect_red "M5 A1 行内追加" "append-only 违规"
reset_shadow; expect_green "M5 还原后绿"

# ── M6~M10:5 条禁表述各 1 条反例(AC-8 ③)────────────────────────────────
for phrase in '全 ⏭ 计入达成' '✅(⏭)' '提前进入 Stage 4+ 已成立' '口径 ④ 已生效' 'Stage 4+ 已开 ⇒ 可归档'; do
    reset_shadow
    plant_in_su "$phrase"
    expect_red "M「${phrase}」植入" "禁表述复活"
    reset_shadow
    expect_green "M「${phrase}」还原后绿"
done

# ── 真实仓零改动(字节指纹比对)────────────────────────────────────────────
if [[ "$(fp_real)" == "$FP_BEFORE" ]]; then
    ok "真实仓 3 份语料字节不变(变异只在影子树)"
else
    ng "真实仓语料被改动(影子树隔离失效)"
fi

echo "───────────────────────────────────────────────"
echo "  通过 $pass · 失败 $fail"
[[ $fail -eq 0 ]] || exit 1
echo "✅ cep_dod_reading_check 全绿(7 组判据 + 10 条变异)"
