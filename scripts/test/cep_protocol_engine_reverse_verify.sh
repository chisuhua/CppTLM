#!/usr/bin/env bash
# scripts/test/cep_protocol_engine_reverse_verify.sh
#   —— 证明 ProtocolEngine 端口面的契约判据(task 2.6 / 2.7)**有牙**的反向验证
#
# 背景(per 判据纪律):「纯负向断言必须有正控」+「编译期锁不得只以『编译红』充当
#   ctest 判据」⇒ 每条判据都要有一个**可执行的反向 fixture**:破坏它 ⇒ 对应守卫
#   **必须报红**。本脚本做**破坏 → 必须编译失败 → 干净版必须编译成功**的闭环。
#
# 2 类毒化(per `cpptlm-cep-cohif-carrier` task 2.6 / 2.7):
#   ① matrix_drop_one  —— 10 流覆盖矩阵**缺 1 流**(删 barrier 项)⇒ 覆盖完整性断言红
#   ② behavior_degrade —— barrier 的 immediate-complete **退化为 no-op** ⇒ 行为断言红
#
# ⚠️ 本脚本**只写自己的临时目录**,**不改**仓内任何文件 ⇒ 可安全进 ctest 并行运行。
#
# 用法:bash scripts/test/cep_protocol_engine_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

HDR="include/tlm/cep/protocol_engine_base.hh"
PROBE_SRC="scripts/test/cep_protocol_engine_compile_probe.cc"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_pe_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test"
          -I "$ROOT/external/json")
pass=0; fail=0

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); }

if [ ! -f "$HDR" ]; then
    echo "  ✗ 端口面头不存在: $HDR"; exit 1
fi

# ── 1. 构建探针(对着**干净**头)────────────────────────────────────
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/build.log" | head -20; exit 1
fi
ok "探针构建成功(干净头)"

# ── 2. 基线:干净头必须**编译成功**(正控)───────────────────────────
if "$CXX" -std=c++17 -fsyntax-only "${INCLUDES[@]}" "$PROBE_SRC" >"$TMP/base.log" 2>&1; then
    ok "基线:干净头编译期锁全绿"
else
    bad "基线:干净头编译期锁红 —— $(grep -m2 'error:' "$TMP/base.log" | tr '\n' ' ')"
fi

# ── 3. 毒化副本生成(各自独立目录 —— 共用会互相覆盖 ⇒ 判据假绿)──────
for kind in matrix_drop_one behavior_degrade; do
    mkdir -p "$TMP/shadow/$kind/tlm/cep"
    if ! "$TMP/probe" --poison "$kind" "$HDR" \
            "$TMP/shadow/$kind/tlm/cep/protocol_engine_base.hh" 2>"$TMP/poison_$kind.log"; then
        bad "毒化副本生成失败:$kind —— $(cat "$TMP/poison_$kind.log")"
    else
        ok "毒化副本生成成功:$kind"
    fi
done

# ── 4. 反向验证:每个毒化 ⇒ 编译**必须失败** ───────────────────────
run_compile_expect() {  # run_compile_expect <kind> <expect FAIL|OK>
    local kind="$1" expect="$2" log="$TMP/compile_$kind.log" rc color
    if "$CXX" -std=c++17 -fsyntax-only -I "$TMP/shadow/$kind" \
            "${INCLUDES[@]:1}" "$PROBE_SRC" >"$log" 2>&1; then rc=0; else rc=1; fi
    if [ $rc -eq 0 ]; then color=OK; else color=FAIL; fi
    if [ "$color" = "$expect" ]; then
        ok "$kind 编译 → $color(期望 $expect)"
    else
        bad "$kind 编译 → $color(期望 $expect) —— $(grep -m2 'error:' "$log" | tr '\n' ' ')"
    fi
}

echo ""
echo "═══ 反向 fixture ①:10 流覆盖矩阵缺 1 流(task 2.6)═══"
run_compile_expect matrix_drop_one FAIL

echo ""
echo "═══ 反向 fixture ②:某流语义退化为 no-op(task 2.7)═══"
run_compile_expect behavior_degrade FAIL

echo ""
echo "══════════════════════════════════════"
echo "反向验证:${pass} 条通过 / ${fail} 条失败"
[ "$fail" -eq 0 ] || exit 1
