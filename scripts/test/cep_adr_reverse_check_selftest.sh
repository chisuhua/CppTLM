#!/bin/bash
# scripts/test/cep_adr_reverse_check_selftest.sh
#   —— `cep_docs_sync_check.sh` 第 [13/13] 组（**ADR 清单反向完整性**，G-16）
#      的**决定性变异自测**。
#
# 用途（登记册 G-16）：`ADR_LIST` 是**正向**覆盖集（清单 → 磁盘：存在 + 含
#   「不变量/决策」节）⇒ ⛔ 无「磁盘 → 清单」的**反向**检查时，**新增 ADR 忘
#   登记** ⇒ 该 ADR **静默脱检**。本自测证明新增的反向判据**有牙**。
#
# ⚠️ **每条变异必须「按签名红」**（⛔「rc≠0」不构成红因）—— 断言**点名具体
#    文件名**，而非「计数 ≥ 1」。
#
# ⚠️ **影子模式，真实仓零改动**：
#   · 影子 ADR 目录 = 真仓 24 份 ADR 的 **symlink** + 1 份合成 ADR
#     （⛔ 真仓 `docs/adr/ADR-CEP-*.md` 属**冻结面** ⇒ 绝不在真仓增删文件）；
#   · 影子排除集 = 临时文件（仓内真实排除集文件**只读**）；
#   · 收尾**校验真实仓字节指纹前后相等**。
#
# ⚠️ 本自测**不注册 ctest**：它**复制并变异**文件（`cep_freeze_check.sh` /
#   `docs_adr_index_check.sh` 的自测**已注册**，但那两条只变异临时清单；
#   本条要在**影子 ADR 目录**上跑 ⇒ 成本与风险高于收益）⇒
#   与 `scripts/test/rddf_stage_gate.sh` 的 `--self-test` 同一纪律
#   （「开发期/评审期证据，不是回归闸」）。**回归闸本体 =**
#   `cep_docs_sync_check`（已注册 ctest，`--strict` ⇒ 反向判据不绿即 CI 红）。
#
# 用法：bash scripts/test/cep_adr_reverse_check_selftest.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

SHD="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_adr_reverse_selftest.XXXXXX")" || exit 2
trap 'rm -rf "$SHD"' EXIT

PASS=0
FAIL=0
ok()  { PASS=$((PASS+1)); printf '  ✓ %s\n' "$*"; }
bad() { FAIL=$((FAIL+1)); printf '  ✗ %s\n' "$*"; }

FP_BEFORE="$(find docs/adr scripts/test/cep_adr_exclude.txt -type f -exec sha256sum {} \; | sort | sha256sum | cut -d' ' -f1)"

printf '\n═══ G-16 · ADR 清单反向完整性 —— 决定性变异（影子模式，真实仓零改动）═══\n'

# ── 影子 ADR 目录：真仓 24 份 ADR 的 symlink + 1 份合成 ADR ────────────
mkdir -p "$SHD/adr"
for f in docs/adr/ADR-CEP-*.md; do ln -s "$ROOT/$f" "$SHD/adr/$(basename "$f")"; done
NEW="ADR-CEP-98-classified-but-unregistered.md"
printf '# 影子：已归类(OWNERS/NO_INV)但**未登记进 ADR_LIST** 的 ADR\n' > "$SHD/adr/$NEW"

run_gate() {  # $1 = ADR 目录, $2 = 排除集文件（空串 = 用仓内真实文件）
  local dir="$1" excl="$2"
  if [ -n "$excl" ]; then
    CEP_ADR_DIR="$dir" CEP_ADR_EXCLUDE_FILE="$excl" \
      bash scripts/test/cep_docs_sync_check.sh --strict 2>&1
  else
    CEP_ADR_DIR="$dir" bash scripts/test/cep_docs_sync_check.sh --strict 2>&1
  fi
}

# ── 变异①（**核心面**）：磁盘多一份未登记 ADR ⇒ 必须红并**点名该文件** ──
out="$(run_gate "$SHD/adr" "")"; rc=$?
if [ $rc -ne 0 ] \
   && printf '%s' "$out" | grep -qF "磁盘 ADR 未登记进 ADR_LIST/显式排除集（漏登记 ⇒ 静默脱检）:" \
   && printf '%s' "$out" | grep -qF "$NEW"; then
  ok "变异①：磁盘新增未登记 ADR ⇒ rc=$rc 且**点名 $NEW**（G-16 专属面）"
else
  bad "变异① 未按签名红（rc=$rc）"; printf '%s\n' "$out" | tail -8 | sed 's/^/      /'
fi

# ── 变异②：同一影子目录 + 把该 ADR 显式排除（带理由）⇒ 必须绿 ──
cat > "$SHD/excl_ok.txt" <<EOF
# 影子排除集：合法排除（带理由 + 磁盘存在 + 不在 ADR_LIST）
$NEW|理由：影子变异 —— 显式排除，故不判红
EOF
out="$(run_gate "$SHD/adr" "$SHD/excl_ok.txt")"; rc=$?
if [ $rc -eq 0 ] && printf '%s' "$out" | grep -qF "全部被 ADR_LIST ∪ 显式排除集覆盖"; then
  ok "变异②：合法显式排除 ⇒ rc=0 且报「全部被覆盖」（排除通道真实可用，非死代码）"
else
  bad "变异② 异常（rc=$rc）"; printf '%s\n' "$out" | tail -6 | sed 's/^/      /'
fi

# ── 变异③：排除项**缺理由** ⇒ 必须红并点名该文件 ──
cat > "$SHD/excl_noreason.txt" <<EOF
# 影子排除集：缺理由
$NEW|
EOF
out="$(run_gate "$SHD/adr" "$SHD/excl_noreason.txt")"; rc=$?
if [ $rc -ne 0 ] && printf '%s' "$out" | grep -qF "排除项缺理由: $NEW"; then
  ok "变异③：排除项缺理由 ⇒ rc=$rc 且**点名该文件**"
else
  bad "变异③ 未按签名红（rc=$rc）"; printf '%s\n' "$out" | tail -6 | sed 's/^/      /'
fi

# ── 变异④：排除项指向**磁盘上不存在**的文件 ⇒ 必须红并点名该文件 ──
cat > "$SHD/excl_ghost.txt" <<'EOF'
# 影子排除集：指向不存在的文件（预先挖洞）
ADR-CEP-97-ghost.md|理由：影子变异 —— 磁盘上不存在
EOF
out="$(run_gate "$SHD/adr" "$SHD/excl_ghost.txt")"; rc=$?
if [ $rc -ne 0 ] && printf '%s' "$out" | grep -qF "排除项在磁盘上不存在: ADR-CEP-97-ghost.md"; then
  ok "变异④：排除项指向磁盘不存在的文件 ⇒ rc=$rc 且**点名该文件**"
else
  bad "变异④ 未按签名红（rc=$rc）"; printf '%s\n' "$out" | tail -6 | sed 's/^/      /'
fi

# ── 变异⑤：排除项其实**已在 ADR_LIST 内** ⇒ 必须红并点名该文件 ──
cat > "$SHD/excl_dup.txt" <<'EOF'
# 影子排除集：与 ADR_LIST 重复
ADR-CEP-21-sharer-bitmap-expansion.md|理由：影子变异 —— 与 ADR_LIST 重复
EOF
out="$(run_gate "$SHD/adr" "$SHD/excl_dup.txt")"; rc=$?
if [ $rc -ne 0 ] && printf '%s' "$out" | grep -qF "排除项已在 ADR_LIST 内(排除多余): ADR-CEP-21-sharer-bitmap-expansion.md"; then
  ok "变异⑤：排除项与 ADR_LIST 重复 ⇒ rc=$rc 且**点名该文件**"
else
  bad "变异⑤ 未按签名红（rc=$rc）"; printf '%s\n' "$out" | tail -6 | sed 's/^/      /'
fi

# ── 变异⑥（反向对照）：真实 ADR 目录 + 真实排除集 ⇒ 必须绿（无假红）──
out="$(run_gate "docs/adr" "")"; rc=$?
if [ $rc -eq 0 ] && printf '%s' "$out" | grep -qF "全部被 ADR_LIST ∪ 显式排除集覆盖"; then
  ok "变异⑥（反向对照）：真实仓基线态 ⇒ rc=0 且报「全部被覆盖」（无假红）"
else
  bad "变异⑥ 基线态异常（rc=$rc）"; printf '%s\n' "$out" | tail -6 | sed 's/^/      /'
fi

# ── 真实仓指纹核对（变异全程零污染）────────────────────────────────
FP_AFTER="$(find docs/adr scripts/test/cep_adr_exclude.txt -type f -exec sha256sum {} \; | sort | sha256sum | cut -d' ' -f1)"
if [ "$FP_BEFORE" = "$FP_AFTER" ]; then
  ok "真实仓字节指纹前后相等（${FP_BEFORE:0:16}…）⇒ 变异零污染"
else
  bad "真实仓指纹变了：before=$FP_BEFORE after=$FP_AFTER"
fi

printf '\n═══ 结果：%d 通过 / %d 失败 ═══\n' "$PASS" "$FAIL"
[ $FAIL -eq 0 ] || exit 1
exit 0
