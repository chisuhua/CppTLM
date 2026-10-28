#!/usr/bin/env bash
# scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh
#   —— 证明 `REGISTER_CHSTREAM_BIDIRECTIONAL` 的编译期锁(task 4.3 / 4.4)**有牙**
#
# 背景(per 判据纪律):「纯负向断言必须有正控」+「编译期锁不得只以『编译红』充当
#   ctest 判据」⇒ 判据必须有一个**可执行的反向 fixture**:破坏它 ⇒ 守卫**必须报红**。
#
# 2 个断言:
#   ① **正控**:N=2 ⇒ 必须**编译成功**(否则宏本身坏了)
#   ② **反向**:N=1 ⇒ 必须**编译失败**,且错误信息**精确命中**宏内的
#      `static_assert(n > 1)`(判据 CC-04 ③)—— 若失败原因是别的(例如类型不匹配),
#      判据就是「因错误的原因红」⇒ 本脚本判红。
#
# ⚠️ 本脚本**只读**仓内文件,**不**写任何文件 ⇒ 可安全进 ctest 并行运行。
#
# 用法:bash scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_chstream_bidirectional_macro_probe.cc"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_macro_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
# ⚠️ **必须用 C++23** —— 本探针 include `chstream_register.hh` ⇒ 拉进
#    `include/utils/module_group.hh`,它用了 `std::println` / `std::print`
#    (C++23 特性)。⇒ 用 C++17 编译会因**无关**原因失败(⇒ 正控假红)。
CXXSTD="${CXXSTD:--std=c++23}"
INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test"
          -I "$ROOT/external/json")
pass=0; fail=0

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); }

# ── ① 正控:N=2 ⇒ 必须编译成功 ────────────────────────────────────
if "$CXX" "$CXXSTD" -fsyntax-only -DPROBE_PORT_COUNT=2 "${INCLUDES[@]}" \
        "$PROBE_SRC" >"$TMP/ok.log" 2>&1; then
    ok "正控:N=2 编译成功(宏与 static_assert 均正常)"
else
    bad "正控:N=2 编译失败 —— $(grep -m2 'error:' "$TMP/ok.log" | tr '\n' ' ')"
    sed 's/^/      /' "$TMP/ok.log" | head -10
fi

# ── ② 反向:N=1 ⇒ 必须编译失败,且命中预期 static_assert ────────────
if "$CXX" "$CXXSTD" -fsyntax-only -DPROBE_PORT_COUNT=1 "${INCLUDES[@]}" \
        "$PROBE_SRC" >"$TMP/bad.log" 2>&1; then
    bad "反向:N=1 竟编译成功 —— static_assert 未生效?"
else
    ok "反向:N=1 编译失败(期望编译失败)"
    # 失败原因**必须**是宏内的 static_assert(判据 CC-04 ③)
    if grep -q "端口数 N 必须 > 1" "$TMP/bad.log"; then
        ok "反向:N=1 的失败原因**精确命中**预期的 static_assert"
        grep -m1 "端口数 N 必须 > 1" "$TMP/bad.log" | sed 's/^/      /'
    else
        bad "反向:N=1 编译失败,但失败原因**不是**预期的 static_assert ⇒ 判据「因错误的原因红」"
        grep -m3 'error:' "$TMP/bad.log" | sed 's/^/      /'
    fi
fi

echo ""
echo "══════════════════════════════════════"
echo "反向验证:${pass} 条通过 / ${fail} 条失败"
[ "$fail" -eq 0 ] || exit 1
