#!/usr/bin/env bash
# scripts/test/rddf_stage_gate.sh
# 阶段顺序门禁 —— per 册子 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §1.5.6
#
# 缺口来源（critic 2026-11-18）：「依赖图画得漂亮但**没有任何机关**」
#   —— 顺序原本只由文风保证 ⇒ 下一个 session 可以直接从 C2 T1 开工而不违反任何机制。
# 本脚本把「顺序」与「AC 可寻址」变成**可机械判定**的闸。
#
# 用法：
#   bash scripts/test/rddf_stage_gate.sh              # 全量检查（P-1~P-6）
#   bash scripts/test/rddf_stage_gate.sh --phase A    # 额外断言「阶段 A 的前置已完成」
#
# 退出码：0 = 全绿；1 = 有项失败（每项打印可定位的原因）
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT" || exit 2

CARRIER="scripts/test/rddf_ac_carrier.tsv"
RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[0;33m'; RST=$'\033[0m'
FAILED=0
SKIPPED=0

fail() { echo -e "${RED}  ✗ $1${RST}"; FAILED=$((FAILED + 1)); }
ok()   { echo -e "${GRN}  ✓ $1${RST}"; }
skip() { echo -e "${YLW}  ⚠ $1${RST}"; SKIPPED=$((SKIPPED + 1)); }

echo "═══ P-1 依赖边可验证（每条边带 file:line，锚点必须命中）═══"
# 格式：<说明> <TAB> <文件> <TAB> <锚点关键词>
# 依赖边清单逐条引自各 change 的 design.md / tasks.md（册子 §1.5.1）。
DEP_EDGES=$(cat <<'EOF'
C1.D-3 B3优先于C2.T1	openspec/changes/cpptlm-module-param-effect-landing/design.md	B3 优先于迁移批次 2 的 T1
C1.D-5 先给键确定效果再迁移	openspec/changes/cpptlm-module-param-effect-landing/design.md	先给键确定效果，再迁移类型
C2.T3.1 前置effect-landing	openspec/changes/cpptlm-module-param-migrate-batch2/tasks.md	B1（`TrafficGenTLM` / `MemoryTLM` / `GmmuTLM`）与 B3（`routing`）完成
R-5.D-4 T1先于T2	openspec/changes/cpptlm-config-type-registration-align/design.md	T1（对齐）必须先于 T2（上闸）
R-5.AC-08 blocked-by C2.T0.2	openspec/changes/cpptlm-config-type-registration-align/design.md	AC-08 标 `blocked-by`
EOF
)
while IFS=$'\t' read -r desc file anchor; do
    [ -z "${desc:-}" ] && continue
    if [ ! -f "$file" ]; then fail "P-1 [$desc] 载体文件不存在: $file"; continue; fi
    if grep -qF "$anchor" "$file"; then ok "P-1 [$desc] $file"; else fail "P-1 [$desc] 锚点未命中: $file ← '$anchor'"; fi
done <<< "$DEP_EDGES"

echo ""
echo "═══ P-2 顺序由机关保证（不得提前开工下一阶段）═══"
# P-2a：下一阶段的**实现面文件**不得出现在当前工作树变更集里
#       （防止「跳过阶段 A/B 直接开 C2 的实现」）
#       ⚠️ 判据**不**用「C2 的 change 目录」—— 改 C2 的 tasks/design 属**规划维护**，
#          不是「提前开工」⇒ 用目录判会误伤（本轮就误伤过一次）。
NEXT_CHANGE="cpptlm-module-param-migrate-batch2"
NEXT_IMPL_FILES="src/core/module_factory.cc include/core/sim_object.hh include/core/sim_module.hh src/core/module_factory_validate.cc"
CHANGED="$(git diff --name-only HEAD 2>/dev/null; git diff --name-only --cached 2>/dev/null)"
_hit=""
for f in $NEXT_IMPL_FILES; do
    if echo "$CHANGED" | grep -qx "$f"; then _hit="$_hit $f"; fi
done
if [ -n "$_hit" ]; then
    fail "P-2a 下一阶段 ($NEXT_CHANGE) 的实现面文件已在工作树变更集:$_hit ⇒ 阶段 A/B 尚未完成"
else
    ok "P-2a 下一阶段 ($NEXT_CHANGE) 的实现面未提前开工"
fi
# P-2b：--phase X 时断言该阶段的**前置 change**的 tasks.md 无未勾 task
PHASE=""
for a in "$@"; do [ "$a" = "--phase" ] && PHASE_NEXT=1; done
if [ "${PHASE_NEXT:-0}" = "1" ]; then
    _p=""
    while [ $# -gt 0 ]; do case "$1" in --phase) _p="$2"; shift 2;; *) shift;; esac; done
    case "$_p" in
        A) PREV="" ;;
        B) PREV="cpptlm-module-param-effect-landing" ;;
        C) PREV="cpptlm-config-type-registration-align" ;;
        *) fail "P-2b 未知阶段: $_p" ;;
    esac
    if [ -n "$PREV" ]; then
        _open="$(grep -c '^- \[ \]' "openspec/changes/$PREV/tasks.md" 2>/dev/null || echo 0)"
        if [ "$_open" -gt 0 ]; then fail "P-2b 阶段 $_p 的前置 $PREV 仍有 $_open 个未勾 task"; else ok "P-2b 阶段 $_p 的前置 $PREV 已完成"; fi
    else
        ok "P-2b 阶段 A 无前置"
    fi
fi

echo ""
echo "═══ P-3 AC 可寻址（每个 AC 至少一条载体命中）═══"
if [ ! -f "$CARRIER" ]; then
    fail "P-3 载体表不存在: $CARRIER"
else
    declare -A ac_hit=()
    while IFS=$'\t' read -r ac kind target anchor; do
        case "${ac:-}" in ''|'#'*) continue;; esac
        if [ "$kind" = "TODO" ]; then
            skip "P-3 [$ac] 载体尚未产出（TODO）"
            continue
        fi
        if [ "$kind" = "cmd" ]; then
            if [ "$target" = "./build/bin/cpptlm_tests" ] && [ "$anchor" = "--list-tests" ]; then
                if [ -x ./build/bin/cpptlm_tests ]; then ok "P-3 [$ac] cmd 可执行"; ac_hit["$ac"]=1; else fail "P-3 [$ac] cmd 不可执行: $target"; fi
            elif [ "$target" = "scripts/test/param_effect_inventory.py" ]; then
                if python3 scripts/test/param_effect_inventory.py --json >/dev/null 2>&1; then ok "P-3 [$ac] cmd rc=0"; ac_hit["$ac"]=1; else fail "P-3 [$ac] cmd 非 0: $target"; fi
            else
                if bash -c "$target $anchor" >/dev/null 2>&1; then ok "P-3 [$ac] cmd rc=0"; ac_hit["$ac"]=1; else fail "P-3 [$ac] cmd 非 0: $target $anchor"; fi
            fi
            continue
        fi
        if [ ! -f "$target" ]; then fail "P-3 [$ac] 载体文件不存在: $target"; continue; fi
        if grep -qF "$anchor" "$target"; then
            ok "P-3 [$ac] $target ← '$anchor'"
            ac_hit["$ac"]=1
        else
            fail "P-3 [$ac] 锚点未命中: $target ← '$anchor'"
        fi
    done < "$CARRIER"
    # 每个 AC 至少命中 1 条
    for ac in $(awk -F'\t' '!/^#/ && NF>=2 {print $1}' "$CARRIER" | sort -u); do
        [ "$ac" = "AC-01" ] && continue   # AC-01 的载体是 cmd（脚本口径），由上面的 cmd 分支判
        if [ -z "${ac_hit[$ac]:-}" ]; then
            # 该 AC 可能只登记了 TODO ⇒ 不算失败（上面已 skip）
            if grep -P "^$ac\tTODO\t" "$CARRIER" >/dev/null 2>&1; then :; else fail "P-3 [$ac] 没有任何载体命中"; fi
        fi
    done
fi

echo ""
echo "═══ P-4 跨阶段判别力不丢（A 阶段登记的判别测试不得被静默删除）═══"
EFF="test/test_param_effect_landing.cc"
if [ ! -f "$EFF" ]; then
    fail "P-4 判别测试文件不存在: $EFF"
else
    while IFS=$'\t' read -r ac kind target anchor; do
        [ "${kind:-}" = "test" ] || continue
        [ "$target" = "$EFF" ] || continue
        if grep -qF "$anchor" "$EFF"; then :; else fail "P-4 [$ac] 判别 case 已消失: '$anchor'"; fi
    done < "$CARRIER"
    ok "P-4 A 阶段登记的判别 case 全部在位"
fi

echo ""
echo "═══ P-5 KPI 回填不是「改数字」（活口径处数 + 一致性）═══"
LIVE_FILES=(
  "AGENTS.md"
  ".rddf/roadmap.md"
  ".rddf/roadmap/phases/phase-10.md"
  ".rddf/roadmap/objectives/obj-cep-platform.md"
  ".rddf/improvements/cpptlm-cep-umbrella.md"
  ".rddf/improvements/cpptlm-cep-nvlink-sharp.md"
)
_active="$(ls openspec/changes 2>/dev/null | grep -v '^archive$' | wc -l | tr -d ' ')"
_unsynced=0
for f in "${LIVE_FILES[@]}"; do
    if [ ! -f "$f" ]; then fail "P-5 活口径文件不存在: $f"; continue; fi
    if ! grep -qE "$_active/3" "$f"; then
        fail "P-5 [$f] 未见当前 active 值 $_active/3 ⇒ 活口径未同步"
        _unsynced=$((_unsynced + 1))
    fi
done
[ "$_unsynced" -eq 0 ] && ok "P-5 $_active 处活口径全部同步"

echo ""
echo "═══ P-6 风险可失败（硬编码下限不得残留）═══"
# 风险 9：R-5 的 AC-07 硬编码下限必须已改写为「实施日重测基线」
if grep -q "883336" openspec/changes/cpptlm-config-type-registration-align/proposal.md 2>/dev/null; then
    fail "P-6 R-5 proposal 仍写死 AC-07 下限 883336（应改为「实施日重测基线」）"
else
    ok "P-6 R-5 AC-07 下限已改写为重测基线"
fi
# 风险 2：C2 的 T8.1 基线过期
if grep -q "883191" openspec/changes/cpptlm-module-param-migrate-batch2/tasks.md 2>/dev/null; then
    fail "P-6 C2 tasks T8.1 仍写死基线 883191（须重测回填）"
else
    ok "P-6 C2 T8.1 基线已重测回填"
fi

echo ""
echo "════════════════════════════════════════════"
if [ "$FAILED" -gt 0 ]; then
    echo -e "${RED}阶段门禁失败：$FAILED 项失败 / $SKIPPED 项跳过${RST}"
    exit 1
fi
echo -e "${GRN}阶段门禁通过（$SKIPPED 项跳过）${RST}"
exit 0
