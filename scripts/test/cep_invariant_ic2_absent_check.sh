#!/usr/bin/env bash
# scripts/test/cep_invariant_ic2_absent_check.sh
#   —— `INV-CEP-09-IC2`（「多 VC 仲裁下 vc3 内 E 不被 D 饿死」）的**存在性反断言**载体
#
# per 判据文件 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §2.4（Q-4）
#     + §9.2 SF-3（grep 模式写死）
#
# ── 为什么是「反断言」而不是「正断言」────────────────────────────────────
#   `ADR-CEP-09` 自记「多 VC 仲裁属 Stage 4+」。在本仓建模它 = **提前锁定一个尚未
#   裁定的仲裁策略**（策略选择属架构决策，本仓无依据）⇒ 正确处置 = 把「当前不可判」
#   从**隐式**变成**显式且可机检**：
#     · 断言「仓内不存在多 VC 仲裁实现」为**真值**
#     · 一旦出现 ⇒ 本闸转红 ⇒ **强制重评**该 invariant（⛔ 不得继续沿用「无可断言对象」）
#
# ⚠️ **这不算「已建模」** —— 该 invariant 的语义**仍未达成**；本闸只钉住「未建模」这一事实。
#
# ⚠️ 模式词写死（AC-12）：`多 VC 仲裁` / `vc_arbitrat` / `VcArbitrat`。
#    ⛔ **不得**用 `Arbiter` 作模式词 —— 仓内 `ArbiterTLM2` / `ArbiterTLM4` 已占用该词
#    （`include/chstream_register.hh`）⇒ 用之即**永恒误红**。
#
# 用法:
#   bash scripts/test/cep_invariant_ic2_absent_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PATTERN='多 VC 仲裁\|vc_arbitrat\|VcArbitrat'
HITS="$(grep -rn "$PATTERN" include/ src/ test/ 2>/dev/null)"
N="$(printf '%s\n' "$HITS" | sed '/^[[:space:]]*$/d' | wc -l)"

if [ "$N" -ne 0 ]; then
  # INV-CARRIER: INV-CEP-09-IC2
  echo "  ✗ INV-CEP-09-IC2: 检测到多 VC 仲裁实现($N 处) ⇒ 该 invariant 必须重评"
  echo "    (⛔ 不得继续沿用「无可断言对象」的登记;须给出仲裁策略的 ADR 裁决)"
  printf '%s\n' "$HITS" | head -5
  exit 1
fi

echo "  ✓ INV-CEP-09-IC2: 仓内无多 VC 仲裁实现(存在性反断言成立 ⇒ 该 invariant 当前无可断言对象)"
exit 0
