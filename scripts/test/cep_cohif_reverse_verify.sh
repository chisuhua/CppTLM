#!/usr/bin/env bash
# scripts/test/cep_cohif_reverse_verify.sh
#   —— 证明 `CohIfBundle` 的契约判据(CC-01 / CC-02)**有牙**的反向验证
#
# 背景(per 队列 #2 critic 7 轮提炼的判据纪律):
#   「纯负向断言必须有正控」+「编译期锁不得只以『编译红』充当 ctest 判据」
#   ⇒ 每条判据都要有一个**可执行的反向 fixture**:破坏它 ⇒ 对应守卫**必须报红**。
#   本脚本做**破坏 → 必须报红 → 干净版必须报绿**的闭环,并把结果作为台账 entry 的证据。
#
# 4 类毒化(per `cpptlm-cep-cohif-carrier` task 1.5 / 1.6 / 1.7):
#   ① flow_count_6      —— 第 6 个 `flow_id`      ⇒ **静态扫描**必红(CC-01 ①)
#   ② network_field    —— 混入网络层字段          ⇒ **静态扫描**必红(CC-01 ②)
#   ③ width_plus_one   —— 字段宽度 +1             ⇒ **编译**必失败(CC-02 ③)
#   ④ virtual_function —— 加虚函数                ⇒ **编译**必失败(CC-02 ②)
#
# ⚠️ 本脚本**只写自己的临时目录**,**不改**仓内任何文件 ⇒ 可安全进 ctest 并行运行
#    (与 `cep_decl_reverse_verify.sh` 不同:后者临时改写仓内文件,必须串行手动跑)。
#
# 用法:bash scripts/test/cep_cohif_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

HDR="include/bundles/cep_coherence_bundles_tlm.hh"
SCAN_PROBE_SRC="scripts/test/cep_cohif_scan_probe.cc"
COMPILE_PROBE_SRC="scripts/test/cep_cohif_compile_probe.cc"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_cohif_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
pass=0; fail=0

ok()   { echo "  ✓ $1"; pass=$((pass+1)); }
bad()  { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); }

# ── 0. 前置:被扫描的头必须存在 ──────────────────────────────────
if [ ! -f "$HDR" ]; then
    echo "  ✗ 载体头不存在: $HDR"; exit 1
fi

# ── 1. 构建两个探针 ────────────────────────────────────────────
if ! "$CXX" -std=c++17 -I "$ROOT/include" -I "$ROOT/test" \
        -o "$TMP/scan_probe" "$SCAN_PROBE_SRC" 2>"$TMP/scan_build.log"; then
    echo "  ✗ scan probe 构建失败:"; sed 's/^/      /' "$TMP/scan_build.log" | head -20; exit 1
fi
ok "scan probe 构建成功"

# ── 2. 基线:干净头 ⇒ 扫描绿 + 编译成功(正控)────────────────────
out="$("$TMP/scan_probe" "$HDR" 2>&1)"; rc=$?
if [ $rc -eq 0 ]; then ok "基线:干净头静态扫描绿"; else bad "基线:干净头静态扫描红 —— $out"; fi

if "$CXX" -std=c++17 -fsyntax-only -I "$ROOT/include" "$COMPILE_PROBE_SRC" \
        >"$TMP/compile_base.log" 2>&1; then
    ok "基线:干净头编译期锁全绿"
else
    bad "基线:干净头编译期锁红 —— $(sed 's/^/ /' "$TMP/compile_base.log" | head -6)"
fi

# ── 3. 毒化副本生成(4 种)──────────────────────────────────────
# ⚠️ 每个毒化**各自独立目录** —— 共用同一文件会互相覆盖(后一个毒化盖掉前一个 ⇒
#    扫描读到的是最后一次的毒化 ⇒ 判据「假绿」)。这是本脚本第一版的真实缺陷。
for kind in flow_count_6 network_field width_plus_one virtual_function; do
    mkdir -p "$TMP/shadow/$kind/bundles"
    if ! "$TMP/scan_probe" --poison "$kind" "$HDR" \
            "$TMP/shadow/$kind/bundles/cep_coherence_bundles_tlm.hh" \
            2>"$TMP/poison_$kind.log"; then
        bad "毒化副本生成失败:$kind —— $(cat "$TMP/poison_$kind.log")"
        continue
    fi
    ok "毒化副本生成成功:$kind"
done

# ── 4. 反向验证:每个毒化 ⇒ 对应守卫必须报红 ───────────────────
# 说明:
#   · `flow_count_6` / `network_field` 的守卫是**静态扫描**(域大小 / 字段集属**源码**属性,
#     编译期无从判定)⇒ 断言「扫描红」;编译结果**不约束**(width_plus_one 的
#     `sizeof` 断言会被网络层字段顺带打破 —— 那是副作用,不是契约)。
#   · `width_plus_one` / `virtual_function` 的守卫是**编译期锁**⇒ 断言「编译失败」。
run_scan_expect() {  # run_scan_expect <kind> <expect RED|GREEN>
    local kind="$1" expect="$2" out rc color
    out="$("$TMP/scan_probe" "$TMP/shadow/$kind/bundles/cep_coherence_bundles_tlm.hh" 2>&1)"; rc=$?
    if [ $rc -ne 0 ]; then color=RED; else color=GREEN; fi
    if [ "$color" = "$expect" ]; then
        ok "$kind 静态扫描 → $color(期望 $expect)"
    else
        bad "$kind 静态扫描 → $color(期望 $expect) —— $(echo "$out" | head -3 | tr '\n' ' ')"
    fi
}
run_compile_expect() {  # run_compile_expect <kind> <expect FAIL|OK>
    local kind="$1" expect="$2" log="$TMP/compile_$kind.log" rc color
    if "$CXX" -std=c++17 -fsyntax-only -I "$TMP/shadow/$kind" -I "$ROOT/include" \
            "$COMPILE_PROBE_SRC" >"$log" 2>&1; then rc=0; else rc=1; fi
    if [ $rc -eq 0 ]; then color=OK; else color=FAIL; fi
    if [ "$color" = "$expect" ]; then
        ok "$kind 编译 → $color(期望 $expect)"
    else
        bad "$kind 编译 → $color(期望 $expect) —— $(grep -m2 'error:' "$log" | tr '\n' ' ')"
    fi
}

echo ""
echo "═══ 反向 fixture ①:第 6 个 flow_id(CC-01 ①)═══"
run_scan_expect flow_count_6 RED

echo ""
echo "═══ 反向 fixture ②:混入网络层字段(CC-01 ②)═══"
run_scan_expect network_field RED

echo ""
echo "═══ 反向 fixture ③:字段宽度 +1(CC-02 ③)═══"
run_compile_expect width_plus_one FAIL

echo ""
echo "═══ 反向 fixture ④:加虚函数(CC-02 ②)═══"
run_compile_expect virtual_function FAIL

echo ""
echo "══════════════════════════════════════"
echo "反向验证:${pass} 条通过 / ${fail} 条失败"
[ "$fail" -eq 0 ] || exit 1
