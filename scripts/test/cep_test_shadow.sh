#!/usr/bin/env bash
# scripts/test/cep_test_shadow.sh
#   —— CEP 闸族**共享影子树机制**(单一真源)
#
# 用法:
#   source scripts/test/cep_test_shadow.sh
#   cep_shadow_reset <shadow_dir> [extra_cp...]
#
#   <shadow_dir>   影子树根(会被 rm -rf 后重建)
#   extra_cp...    额外复制项(相对**真实仓根**),形如 test/cep_adr_structural_deferrals.json
#
# ⚠️ **共享契约**:本文件是 `cep_adr_structural_check.sh` 与 `cep_decision_block_check.sh`
#    的**唯一**影子树实现 ⇒ 任何改动**必须同步两闸**。
#    ⛔ 任一闸 SHALL NOT 自带影子树复制实现 —— 第二份实现即隐性分叉
#    (per 台账 Entry #361 ③ D-8:「复用」一词若不给具体机制,落地即成第二份 5 行实现)。
#
# ⚠️ **必复制集合 = `docs/adr`**(两闸共用的 ADR 语料)⇒ 单一真源,⛔ 不得各闸自定。
# ⚠️ **差异复制 = `[extra_cp...]`** 参数化:
#    · `cep_adr_structural_check.sh` 需 `test/cep_adr_structural_deferrals.json`
#    · `cep_decision_block_check.sh` **不需要**(⛔ 不得把 deferrals 复制进新闸的影子)
#
# ⚠️ **路径解析**:本 helper 以**自身位置**推导真实仓根 ⇒ 调用方的 CWD 无关。

SHADOW_HELPER_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd)"

# cep_shadow_reset <shadow_dir> [extra_cp...]
cep_shadow_reset() {
    if [[ $# -lt 1 ]]; then
        echo "cep_shadow_reset: 需要 <shadow_dir> 参数" >&2
        return 2
    fi
    local shadow="$1"; shift
    local extra
    rm -rf "$shadow"
    mkdir -p "$shadow/docs" "$shadow/test"
    # ── 必复制集合(两闸共用)────────────────────────────────────────────
    cp -r "$SHADOW_HELPER_ROOT/docs/adr" "$shadow/docs/adr"
    # ── 差异复制(各闸按需)──────────────────────────────────────────────
    for extra in "$@"; do
        mkdir -p "$shadow/$(dirname "$extra")"
        cp "$SHADOW_HELPER_ROOT/$extra" "$shadow/$extra"
    done
}
