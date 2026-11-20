#!/usr/bin/env bash
# scripts/test/openspec_version_check.sh
#   —— openspec CLI 的**版本锁定**闸（单一真源 = 仓根 `.openspec-cli-version`）
#
# per change `cpptlm-openspec-version-pinning`（登记册队列 2 的 **I-8**）
#
# ── 为什么需要这个闸（实测缺口）────────────────────────────────────
#   改动前：CI 的安装版本**硬编码**在 `.github/workflows/ci.yml`（`@1.14.3`），
#   仓内**无任何机器面**记录「本地该用哪个版本」⇒
#     ① 本地与 CI 版本漂移**无人发现**（直到 baseline 实测口径不一致才暴露）；
#     ② CLI 行为漂移会**静默改变**多个闸的语义（如 `cep_spec_strict_check.sh`
#        自记「工具 major 漂移会改变本闸语义 ⇒ 须走独立裁决（残余 R-6）」）。
#
# ── 判据（三条，缺一即红）───────────────────────────────────────────
#   ① 锁文件存在且内容是合法 semver；
#   ② CI 的安装步骤**从锁文件读**（⛔ 不在 workflow 里再写一份版本号）；
#   ③ 仓内 CLI 版本 == 锁值（CLI 缺席 ⇒ SKIP，属外部条件，⛔ 不判红）。
#
# 用法:
#   bash scripts/test/openspec_version_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

LOCK=".openspec-cli-version"
CI=".github/workflows/ci.yml"

if [ ! -f "$LOCK" ]; then
  echo "  ✗ 缺版本锁文件: $LOCK（单一真源缺失 ⇒ 版本无从判定）"
  exit 1
fi

LOCK_VER="$(tr -d '[:space:]' < "$LOCK")"
if ! printf '%s' "$LOCK_VER" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "  ✗ 锁文件内容不是合法 semver: '$LOCK_VER'"
  exit 1
fi

# ② CI 必须**从锁文件读**，不得内嵌另一份版本号
if ! grep -q 'cat \.openspec-cli-version' "$CI"; then
  echo "  ✗ CI 未从 $LOCK 读版本（⇒ 仓内存在第二份版本号 ⇒ 漂移无人发现）"
  exit 1
fi
# ⛔ 反向：workflow 里不得出现 `@<数字>` 形式的硬编码版本
if grep -nE '@fission-ai/openspec@[0-9]' "$CI" | grep -v '\${OS_VER}' | grep -q .; then
  echo "  ✗ CI 内仍存在硬编码的 openspec 版本号（应只经 \${OS_VER} 读取）:"
  grep -nE '@fission-ai/openspec@[0-9]' "$CI" | sed 's/^/    /'
  exit 1
fi

# ③ 仓内 CLI 版本 == 锁值（CLI 缺席 ⇒ SKIP）
if ! command -v openspec >/dev/null 2>&1; then
  echo "  · [SKIP] 本环境无 openspec CLI ⇒ 锁值与 CLI 的一致性无法在本仓校验（CI 侧由 ② 保证）"
  echo "  ✓ 锁值 ${LOCK_VER} 合法且 CI 从锁文件读取"
  exit 0
fi

CLI_VER="$(openspec --version 2>/dev/null | head -1 | tr -d '[:space:]')"
if [ "$CLI_VER" != "$LOCK_VER" ]; then
  echo "  ✗ openspec CLI 版本 ${CLI_VER:-<空>} != 锁值 ${LOCK_VER} ⇒ 版本漂移"
  echo "    (⛔ CLI 行为漂移会改变闸的语义 ⇒ 须升级锁值或改判据,不得放任)"
  exit 1
fi

echo "  ✓ openspec 版本锁定一致:锁值 = CI 安装版本 = 本机 CLI = ${LOCK_VER}"
exit 0
