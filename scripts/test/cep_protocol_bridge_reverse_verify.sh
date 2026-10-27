#!/usr/bin/env bash
# scripts/test/cep_protocol_bridge_reverse_verify.sh
#   —— 证明 ProtocolBridge 契约判据(task 3.8 / 3.9)**有牙**的反向验证
#
# 背景(per 判据纪律):「纯负向断言必须有正控」+「编译期锁不得只以『编译红』充当
#   ctest 判据」⇒ 每条判据都要有一个**可执行的反向 fixture**:破坏它 ⇒ 对应守卫
#   **必须报红**。本脚本做**破坏 → 必须运行期判红 → 干净版必须全绿**的闭环。
#
# 2 类毒化(per `cpptlm-cep-cohif-carrier` task 3.9):
#   ① perm_to_category —— 把 `perm` 映射到 `flit_category` ⇒ 字段映射判据必红
#   ② swap_addr_data   —— 把 `address` 与 `data` 对调 ⇒ 内容层 round-trip 必红
#
# ⚠️ Phase 3 的判据是**运行期**语义 ⇒ 毒化副本必须**编得过**但**判据红**;
#    若毒化导致编译失败,判据就是「因错误的原因红」⇒ 反向验证无效。
#    本脚本对每个毒化都断言「编译成功」+「运行期判红」两件事。
#
# ⚠️ 本脚本**只写自己的临时目录**,**不改**仓内任何文件 ⇒ 可安全进 ctest 并行运行。
#
# 用法:bash scripts/test/cep_protocol_bridge_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

HDR="include/tlm/cep/cohif_noc_bridge.hh"
PROBE_SRC="scripts/test/cep_protocol_bridge_probe.cc"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_pb_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test"
          -I "$ROOT/external/json")
pass=0; fail=0

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); }

if [ ! -f "$HDR" ]; then
    echo "  ✗ 桥头不存在: $HDR"; exit 1
fi

# ── 1. 构建探针(对着**干净**头)────────────────────────────────────
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/build.log" | head -20; exit 1
fi
ok "探针构建成功(干净头)"

# ── 2. 正控:干净实现 ⇒ 对应判据**必须全绿**(rc=0)──────────────────
for kind in perm_to_category swap_addr_data; do
    "$TMP/probe" --verify "$kind" >"$TMP/clean_$kind.log" 2>&1
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "正控:$kind 干净实现 ⇒ 判据全绿"
    else
        bad "正控:$kind 干净实现竟报红(rc=$rc) —— $(cat "$TMP/clean_$kind.log")"
    fi
done

# ── 3. 毒化副本生成(各自独立目录 —— 共用会互相覆盖 ⇒ 判据假绿)──────
for kind in perm_to_category swap_addr_data; do
    mkdir -p "$TMP/shadow/$kind/tlm/cep"
    if ! "$TMP/probe" --poison "$kind" "$HDR" \
            "$TMP/shadow/$kind/tlm/cep/cohif_noc_bridge.hh" 2>"$TMP/poison_$kind.log"; then
        bad "毒化副本生成失败:$kind —— $(cat "$TMP/poison_$kind.log")"
    else
        ok "毒化副本生成成功:$kind"
    fi
done

# ── 4. 反向验证:每个毒化 ⇒ **编得过** 且 **运行期判红** ─────────────
run_expect_red() {  # run_expect_red <kind>
    local kind="$1" log="$TMP/compile_$kind.log" rc
    # 4a. 编译**必须成功**(毒化是运行期语义 ⇒ 编译失败 = 判据「因错误的原因红」)
    # ⚠️ shadow 目录**放最前**(include 路径按序查找)⇒ 毒化头优先命中;
    #    `$ROOT/include` 仍保留 —— 否则 `tlm/cep/...` 这种带前缀的路径找不到。
    if ! "$CXX" -std=c++17 -I "$TMP/shadow/$kind" "${INCLUDES[@]}" \
            -o "$TMP/probe_$kind" "$PROBE_SRC" >"$log" 2>&1; then
        bad "$kind 编译失败 —— 判据会「因错误的原因红」(rc=编译错误)"
        sed 's/^/      /' "$log" | head -8
        return
    fi
    ok "$kind 编译成功(毒化是运行期语义 ⇒ 必须编得过)"
    # 4b. 运行**必须判红**
    "$TMP/probe_$kind" --verify "$kind" >"$TMP/run_$kind.log" 2>&1
    rc=$?
    if [ $rc -eq 1 ]; then
        ok "$kind 运行 ⇒ 判红(期望判红)"
        sed 's/^/      /' "$TMP/run_$kind.log" | head -4
    else
        bad "$kind 运行 ⇒ rc=$rc(期望 1 = 判红) —— $(cat "$TMP/run_$kind.log")"
    fi
}

echo ""
echo "═══ 反向 fixture ①:perm → flit_category(task 3.9)═══"
run_expect_red perm_to_category

echo ""
echo "═══ 反向 fixture ②:address ↔ data 对调(task 3.9)═══"
run_expect_red swap_addr_data

echo ""
echo "══════════════════════════════════════"
echo "反向验证:${pass} 条通过 / ${fail} 条失败"
[ "$fail" -eq 0 ] || exit 1
