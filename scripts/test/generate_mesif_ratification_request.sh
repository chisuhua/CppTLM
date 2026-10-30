#!/usr/bin/env bash
# scripts/test/generate_mesif_ratification_request.sh
#   —— 生成 / 校验「MESIF 实现层跨仓追认请求清单」
#
# 功能:从**机器可读载体**生成 `docs/implementation/2026-10-30-cep-mesif-adapter-ratification-request.md`
#       (先例:`generate_mesif_recognition_checklist.sh` —— 规约层的同类先例)。
#       `--check` 模式:只比对不写(提交前自检 / CI 闸)。
#
# ⚠️ 为什么必须有生成器:清单若手写,载体改了清单不会跟着改 ⇒「清单与载体一致」会退化成纸面
#    ⇒ 由 `test_cep_mesif_ratification_request.cc` 逐条核对。
#
# 用法:
#   bash scripts/test/generate_mesif_ratification_request.sh          # 生成
#   bash scripts/test/generate_mesif_ratification_request.sh --check   # 只比对(rc≠0 ⇒ 不一致)
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_mesif_ratification_probe.cc"
OUT="docs/implementation/2026-10-30-cep-mesif-adapter-ratification-request.md"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_rat_req.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test"
          -I "$ROOT/external/json")

[ -f "$PROBE_SRC" ] || { echo "✗ 探针源不存在:$PROBE_SRC"; exit 1; }

echo "── 编译探针 ──"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/build.log"; then
    echo "✗ 探针构建失败:"; sed 's/^/    /' "$TMP/build.log" | head -20; exit 1
fi
echo "  ✓ 探针构建成功"

if [ "${1:-}" = "--check" ]; then
    echo "── 校验(只比对)──"
    "$TMP/probe" "$ROOT" "$OUT" --check
    exit $?
fi

echo "── 生成清单 ──"
"$TMP/probe" "$ROOT" "$OUT"
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "✗ 生成失败"; exit "$rc"
fi
echo "  ✓ 清单已生成:$OUT"
