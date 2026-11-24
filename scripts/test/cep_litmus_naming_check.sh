#!/usr/bin/env bash
# scripts/test/cep_litmus_naming_check.sh
#   —— 「litmus」命名与能力错配的**禁用表述闸**
#
# per change `cep-litmus-falsifier-naming-disambiguation`（阶段 1）
#     + `ADR-CEP-13` 决策 5 ②（平台不提供无违例保证 ⇒ 不得夸大能力）
#
# ── 闸做什么 ─────────────────────────────────────────────────────────
#   扫描 3 个文件；凡出现「memory-model litmus」/「herd7」/「RISC-V litmus」/
#   「内存模型 litmus」等**指涉他物**的表述时，**同一行**内必须带**否定标记**
#   （`不是` / `非` / `⛔` / `不`）⇒ 否则红，红因码 `litmus-scope-claim`。
#
#   ⛔ **否定词表不含「区别」** —— 「X 与内存模型 litmus 的区别是 Y」是**肯定句**
#      （断言两者有对应关系），不构成「本工装不是它」的否定（per metis P1-11）。
#   ⚠️ **「同一行」以 \n 为界**（⛔ 不按空行分段 —— 否则维护期一行换行即抖动）。
#      ⇒ 代价：**跨行的否定句会假红** —— ⛔ 接受（宁可假红不可假绿：假绿意味着
#      能力高估复发，正是本单元要治的病）⇒ 写消歧表述时**每行自洽**。
#   ⛔ **本闸只读** 冻结面 spec（`openspec/specs/cep-*/spec.md`）⇒ **零冻结面改动**。
#
# ── 为什么需要「同段否定」而不是「禁词」──────────────────────────────
#   论文侧的 herd7 litmus 是**正确**的讨论（`docs/research/` 内），全仓禁词会假红；
#   本仓工装自身则**不得**用它指涉自己 ⇒ 精确限定扫描集为「本工装表述面」的 3 处。
#
# ── 闸自身有牙（自测五路 · 对**影子副本**操作 ⇒ 仓内零改动）────────────
#   ① 「本工装是 memory-model litmus」        ⇒ 必红（`litmus-scope-claim`）
#   ② 「本工装**不是** memory-model litmus」  ⇒ 必须绿（否则闸不可用）
#   ③ 「X 与内存模型 litmus 的**区别是** Y」    ⇒ 必红（「区别」非否定词）
#   ④ probe TU：常量改 `true` ⇒ 编译失败**且**命中签名
#   ⑤ 扫描集缺一成员 ⇒ 必红（红因 `litmus-scan-set-missing`）
#
# ── ⛔ **本闸的文件名与红因码「必不改」（2026-11-24 · 改名裁定）────────────────────
#   本闸命名的是它的**管辖违例类**（「litmus 能力冒称」= `litmus-scope-claim`），
#   **不是**被守护工装的能力名 ⇒ 工装改名（`litmus_falsifier` ⇒ `compound_graph_falsifier`）
#   **不使本闸过期**：读者仍可能把复合图证伪工装错述为 memory-model litmus ⇒ 混淆风险不消失。
#   且红因码被阶段 1 归档 change（`2026-11-23-cep-litmus-falsifier-naming-disambiguation`）
#   的 AC 证据与验收报告**逐字引用** ⇒ 改名即断**审计链**。
#   ⇒ 保留：文件名 · 红因码 `litmus-scope-claim` · claim patterns · 否定词表 · SKIP 语义。
#   裁定记录：`.rddf/reviews/cep-litmus-falsifier-rename-adjudication-2026-11-24.md`
#
# 用法:
#   bash scripts/test/cep_litmus_naming_check.sh
#   BUILD_DIR=build bash scripts/test/cep_litmus_naming_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

# ── 扫描集（**相对路径** ⇒ 改名时只改这 3 个变量）────────────────────────
# ⚠️ `SCAN_ROOT` 可覆盖扫描集根：① 闸的「影子副本」自测用（构造缺成员的扫描集）
#    ② 与工件改名**解耦**（改名只改下面 3 个相对路径）。
SCAN_ROOT="${SCAN_ROOT:-$ROOT}"
HDR_REL="include/tlm/cep/compound_graph_falsifier.hh"
TESTF_REL="test/test_cep_compound_graph_falsification.cc"
SPEC_REL="openspec/specs/cep-compound-graph-falsification/spec.md"
HDR="$SCAN_ROOT/$HDR_REL"
TESTF="$SCAN_ROOT/$TESTF_REL"
SPEC="$SCAN_ROOT/$SPEC_REL"

# ⛔ **扫描集精确限定 3 处** —— 不扩到全仓（`docs/research/` 讨论论文侧 herd7 litmus
#    是正确的 ⇒ 全仓禁词必假红）。见 stage-1 design.md §D-4。
SCAN_FILES=("$HDR" "$TESTF" "$SPEC")

# 否定词表（⛔ **不含「区别」** —— 「X 与内存模型 litmus 的区别是 Y」是肯定句）
NEG_PATTERNS=("不是" "非" "⛔" "不")

FAILED=0
pass() { echo "  ✓ $*"; }
fail() { echo "  ✗ $*"; FAILED=$((FAILED + 1)); }

echo "=== CEP litmus 命名消歧闸 ==="

# ── 扫描集**完整性**判定（2026-11-24 · 缺陷修复）────────────────────────────
# ⚠️ **历史缺陷**：扫描实现原为 `[ -f "$f" ] || continue` ⇒ 扫描集缺失 ⇒ **静默跳过**
#    ⇒ 闸**假绿**（实测：spec 路径改错 ⇒ 闸仍 RC=0「全部判据通过」）⇒ 契约面完全失检。
# ⚠️ 该缺陷由**改名直接触发**（spec 路径一变、不改扫描集 ⇒ 闸永久假绿）⇒ 必须同批修。
# ⇒ 修复口径：**HDR 在**（消歧主体已落地）而 TESTF / SPEC 任一缺失 ⇒ **FAIL**。
#    （HDR 缺失 ⇒ 整闸 SKIP 保持不变 —— 主体未落地是合法前置态。）
#
# ⚠️ **实现纪律**：本函数**只打印缺失成员**（空 = 完整），**不靠退出码传判**
#    —— 判据脚本的退出码在部分 shell 实现 / 包装环境下**不可靠**
#    ⇒ 判据的正确性**不得**依赖退出码语义。
scan_set_missing() { # <hdr> <testf> <spec> ⇒ 打印缺失成员（每行一个）；空 = 完整
    local h=$1 t=$2 s=$3
    local miss=()
    [ -f "$h" ] || miss+=("$h")
    [ -f "$t" ] || miss+=("$t")
    [ -f "$s" ] || miss+=("$s")
    if [ "${#miss[@]}" -gt 0 ]; then
        printf '%s\n' "${miss[@]}"
    fi
}

# ── 前置：被消歧主体不存在 ⇒ SKIP（可区分于 FAIL）────────────────────
if [ ! -f "$HDR" ]; then
    echo "[SKIP] $HDR 不存在（消歧声明面尚未落地）"
    exit 0
fi

MISS=$(scan_set_missing "$HDR" "$TESTF" "$SPEC")
if [ -n "$MISS" ]; then
    fail "litmus-scan-set-missing: 扫描集成员缺失（判据失去作用面）:"
    echo "$MISS" | sed 's/^/      /'
    echo "────────────────────────────────────────"
    echo "[FAIL] cep_litmus_naming_check: 1 项失败"
    exit 1
fi

# ── 扫描实现（可对任意文件集调用 ⇒ 自测复用同一份逻辑）──────────────
# $@ = 文件列表；命中且同段无否定标记 ⇒ 打印 `file:line:内容` 并置 `SCAN_HIT=1`
#
# ⚠️ **结果经全局变量 `SCAN_HIT` 传递，不用退出码** —— 同上：退出码不可靠。
# ⚠️ 实现注记：pattern 列表**不用** `case` + 反斜杠续行（续行在 pattern 位置有陷阱），
#    改用 `[[ ]]` + 变量展开，逐 pattern 单独判定。
CLAIM_PATTERNS=("memory-model litmus" "memory model litmus" "herd7" "RISC-V litmus" "RISC-V Litmus" "内存模型 litmus")

SCAN_HIT=0

scan_violations() {
    local f line content pat neg found_claim found_neg
    SCAN_HIT=0
    for f in "$@"; do
        [ -f "$f" ] || continue
        line=0
        while IFS= read -r content || [ -n "$content" ]; do
            line=$((line + 1))
            # ① 该行是否指涉「内存模型 litmus」（命中任一 pattern）
            found_claim=0
            for pat in "${CLAIM_PATTERNS[@]}"; do
                if [[ "$content" == *"$pat"* ]]; then found_claim=1; break; fi
            done
            [ "$found_claim" -eq 1 ] || continue
            # ② 同段是否带否定语境（⛔ 否定词表**不含「区别」** —— 见文件头）
            found_neg=0
            for neg in "${NEG_PATTERNS[@]}"; do
                if [[ "$content" == *"$neg"* ]]; then found_neg=1; break; fi
            done
            if [ "$found_neg" -eq 0 ]; then
                printf '%s:%s:%s\n' "$f" "$line" "$content"
                SCAN_HIT=1
            fi
        done < "$f"
    done
}

# ── 臂① 正控：真实扫描集零违规 ───────────────────────────────────────
echo "--- 臂① 正控：真实扫描集（3 文件） ---"
VIOL=$(scan_violations "${SCAN_FILES[@]}")
if [ -n "$VIOL" ]; then
    fail "litmus-scope-claim: 以下行指涉内存模型 litmus 但同段无否定语境:"
    echo "$VIOL" | sed 's/^/      /'
else
    pass "臂① 正控：扫描集内无「无否定语境的内存模型 litmus 表述」"
fi

# ── 臂② 反向 fixture（对**影子副本** ⇒ 仓内零改动）──────────────────
echo "--- 臂② 反向 fixture（影子副本） ---"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/cep_litmus_naming.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
SHADOW="$TMP/shadow"
mkdir -p "$SHADOW"

# ① 肯定句 ⇒ 必须红
printf '%s\n' "本工装是 memory-model litmus 工装。" > "$SHADOW/f1.txt"
scan_violations "$SHADOW/f1.txt" >/dev/null
if [ "$SCAN_HIT" -eq 1 ]; then
    pass "臂②① 肯定句「本工装是 memory-model litmus」⇒ 必红（已红）"
else
    fail "臂②① 肯定句未被抓到（判据失去牙）"
fi

# ② 否定句 ⇒ 必须绿
printf '%s\n' "本工装不是 memory-model litmus 工装。" > "$SHADOW/f2.txt"
scan_violations "$SHADOW/f2.txt" >/dev/null
if [ "$SCAN_HIT" -eq 1 ]; then
    fail "臂②② 否定句「本工装不是 memory-model litmus」被误报（闸不可用）"
else
    pass "臂②② 否定句「本工装不是 memory-model litmus」⇒ 必须绿（已绿）"
fi

# ③ 「与…的区别是…」= 肯定句 ⇒ 必须红（「区别」非否定词）
printf '%s\n' "X 与内存模型 litmus 的区别是 Y。" > "$SHADOW/f3.txt"
scan_violations "$SHADOW/f3.txt" >/dev/null
if [ "$SCAN_HIT" -eq 1 ]; then
    pass "臂②③ 肯定句「X 与内存模型 litmus 的区别是 Y」⇒ 必红（已红）"
else
    fail "臂②③ 「区别是」肯定句未被抓到（否定词表误含「区别」）"
fi

# ④ 扫描集缺一成员 ⇒ 完整性判定必须识别（缺陷修复的实证）
#    ⚠️ **同进程函数调用**（⛔ **不递归调用本脚本** ⇒ 结构上不可能无限递归）
#    手法：影子根按**同一相对路径**放 hdr/testf，**故意不放** spec
#    ⇒ 完整性判定必须报出缺失成员 ⇒ 若报不出 ⇒ 缺陷未修复。
SHADOW_ROOT="$TMP/shadowroot"
mkdir -p "$SHADOW_ROOT/$(dirname "$HDR_REL")" "$SHADOW_ROOT/$(dirname "$TESTF_REL")"
cp "$HDR" "$SHADOW_ROOT/$HDR_REL"
cp "$TESTF" "$SHADOW_ROOT/$TESTF_REL"
OUT4=$(scan_set_missing "$SHADOW_ROOT/$HDR_REL" "$SHADOW_ROOT/$TESTF_REL" "$SHADOW_ROOT/$SPEC_REL")
if [ -z "$OUT4" ]; then
    fail "臂②④ 扫描集缺一成员 ⇒ 必须被识别（缺陷未修复：仍假绿）"
elif echo "$OUT4" | grep -qF "$SPEC_REL"; then
    pass "臂②④ 扫描集缺一成员 ⇒ 必红（已红，红因码 litmus-scan-set-missing）"
else
    fail "臂②④ 扫描集缺一成员 ⇒ 被识别但未点名缺失的 spec 成员"
    echo "$OUT4" | sed 's/^/      /'
fi

# ── 臂③ probe TU 反 fixture：常量改 true ⇒ 编译失败且命中签名 ─────────
echo "--- 臂③ probe TU 反 fixture（常量改 true ⇒ 编译失败且命中签名） ---"
CXX_BIN=${CXX:-g++}
STD_FLAG="-std=c++23"
if ! echo 'int main(){}' | "$CXX_BIN" "$STD_FLAG" -x c++ -fsyntax-only - >/dev/null 2>&1; then
    STD_FLAG="-std=c++20"
fi

# 探针：消费两个常量（⛔ include 路径由 $HDR_REL 派生 ⇒ 改名时自动跟随）
cat > "$TMP/probe.cc" <<EOF
#include "tlm/cep/$(basename "$HDR_REL")"
static_assert(tlm::cep::kFalsifierScope[0] != '\0',
              "litmus-scope: kFalsifierScope 必须非空");
static_assert(!tlm::cep::kFalsifierIsMemoryModelLitmus,
              "litmus-scope: 复合图证伪工装不是内存模型 litmus");
int main() { return 0; }
EOF

probe_compile() { # <src> => 输出编译日志；返回编译 rc
    "$CXX_BIN" "$STD_FLAG" -fsyntax-only \
        -I"$ROOT/include" -I"$ROOT/include/core" -I"$ROOT/external/json" \
        "$1" 2>&1
}

# 基线：必须编译通过
OUT=$(probe_compile "$TMP/probe.cc")
RC=$?
if [ $RC -eq 0 ]; then
    pass "臂③ 基线：probe TU 编译通过"
else
    fail "臂③ 基线：probe TU 本应编译通过却失败"
    echo "$OUT" | head -8 | sed 's/^/      /'
fi

# 变异：把常量改 true ⇒ 必须编译失败 **且** 命中签名
MUT_INC="$TMP/inc"
mkdir -p "$MUT_INC/tlm/cep"
cp "$HDR" "$MUT_INC/tlm/cep/$(basename "$HDR_REL")"
# 变异锚点：只改**本工装自己声明**的那一行（⛔ 不改仓内头文件 ⇒ 仓内零改动）
sed -i 's/inline constexpr bool kFalsifierIsMemoryModelLitmus = false;/inline constexpr bool kFalsifierIsMemoryModelLitmus = true;/' \
    "$MUT_INC/tlm/cep/$(basename "$HDR_REL")"
if grep -q "kFalsifierIsMemoryModelLitmus = true;" "$MUT_INC/tlm/cep/$(basename "$HDR_REL")"; then
    OUT2=$("$CXX_BIN" "$STD_FLAG" -fsyntax-only \
        -I"$MUT_INC" -I"$ROOT/include" -I"$ROOT/include/core" -I"$ROOT/external/json" \
        "$TMP/probe.cc" 2>&1)
    RC2=$?
    if [ $RC2 -eq 0 ]; then
        fail "臂③ 变异：本应编译失败但通过了（判据失去牙）"
    elif echo "$OUT2" | grep -q "litmus-scope"; then
        pass "臂③ 变异：编译失败且命中签名「litmus-scope」"
    else
        fail "臂③ 变异：编译失败但未命中签名「litmus-scope」"
        echo "$OUT2" | head -8 | sed 's/^/      /'
    fi
else
    fail "臂③ 变异锚点未命中（判据失去牙）"
fi

# ── 臂④ 头文件诚实边界三要素共现（作用域标记 ∧ 判据对象 ∧ 否定标记）──
# ⚠️ **2026-11-24 操作级改型**：判据对象由「通道顶点形态 `(instance, network, vc)`」
#    改为「**操作级**作用域标记」⇒ 工装的图 = 操作图（per `ADR-CEP-22` 决策 4/6）。
echo "--- 臂④ 头文件诚实边界三要素共现 ---"
SCOPE_KEY='kFalsifierScope = "operation-level-composite-graph"'
if grep -qF "$SCOPE_KEY" "$HDR" && grep -qF '环' "$HDR" && grep -qE '不是|非|⛔|不' "$HDR"; then
    pass "臂④ 头文件含作用域标记 'operation-level-composite-graph' ∧ 判据对象「环」 ∧ 否定标记"
else
    fail "litmus-scope-boundary: 头文件边界段缺三要素（作用域标记 / 判据对象 / 否定语境）"
fi

# ── 汇总 ───────────────────────────────────────────────────────────────
echo "────────────────────────────────────────"
if [ "$FAILED" -eq 0 ]; then
    echo "[PASS] cep_litmus_naming_check: 全部判据通过"
    exit 0
fi
echo "[FAIL] cep_litmus_naming_check: $FAILED 项失败"
exit 1
