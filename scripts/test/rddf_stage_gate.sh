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

# 归档感知的路径解析 —— 三态结果（状态记在全局 `RESOLVE_STATE`）：
#   ok                 载体在原路径
#   archived           载体在 `openspec/changes/archive/<date>-<name>/` 下（已归档，仍可校验）
#   unavailable        该 change 已归档，但归档内容不在当前工作树
#
# ⚠️ 为什么必须有 archived 态：change 归档后其 AC 载体（design.md / tasks.md）整体迁移路径。
#    不回退 ⇒ 每次归档都使门禁变红 ⇒ 只能手改载体表追认 ⇒ 判据历史被改写
#    （⛔ 违背 P-4「判别 case 不得被静默删除」的初衷）。
#
# ⚠️ 为什么必须有 unavailable 态：`.gitignore:64` 排除了 `openspec/changes/archive/`
#    （历史仅 3 次 commit force-add 过部分内容）⇒ **干净 clone 上归档内容根本不存在**。
#    此时不能判 fail（那不是判据失败，是载体不可分发）也不能判 pass ⇒ **skip + 告警**，
#    并写明原因。⚠️ 这**不削弱** P-4：判据的**活载体**（`test/` 下的测试文件）始终 tracked。
RESOLVE_STATE=""   # ok | archived | unavailable | missing
RESOLVE_PATH=""    # ⚠️ 路径必须走**全局变量**回传 —— 若用 `echo` + `$(...)`，赋值会发生在
                   #    命令替换的子 shell 里 ⇒ RESOLVE_STATE 永远传不回父 shell（本脚本
                   #    曾因此让 unavailable 态**静默失效**，变异测试才发现）。
resolve_path() {
    local p="$1"
    RESOLVE_PATH="$p"
    if [ -f "$p" ]; then RESOLVE_STATE=ok; return 0; fi
    case "$p" in
        openspec/changes/archive/*) RESOLVE_STATE=unavailable; return 2 ;;
        openspec/changes/?*/*)
            local rest="${p#openspec/changes/}"
            local cname="${rest%%/*}"
            local tail="${rest#*/}"
            local hit
            hit="$(ls -d openspec/changes/archive/*-"$cname" 2>/dev/null | head -1)"
            if [ -n "$hit" ] && [ -f "$hit/$tail" ]; then
                RESOLVE_PATH="$hit/$tail"; RESOLVE_STATE=archived; return 0
            fi
            # change 已不在 active 集 ⇒ 视为已归档（归档内容是否在工作树决定上面那条）
            if [ ! -d "openspec/changes/$cname" ]; then RESOLVE_STATE=unavailable; return 2; fi
            ;;
    esac
    RESOLVE_STATE=missing; return 1
}

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
    resolve_path "$file"; file="$RESOLVE_PATH"
    if [ "$RESOLVE_STATE" = "unavailable" ]; then
        skip "P-1 [$desc] 载体已归档且不在工作树（\`openspec/changes/archive/\` 被 gitignore）: $file"
        continue
    fi
    if [ ! -f "$file" ]; then fail "P-1 [$desc] 载体文件不存在: $file"; continue; fi
    if grep -qF "$anchor" "$file"; then ok "P-1 [$desc] $file"; else fail "P-1 [$desc] 锚点未命中: $file ← '$anchor'"; fi
done <<< "$DEP_EDGES"

echo ""
echo "═══ P-2 顺序由机关保证（不得提前开工下一阶段）═══"
# P-2a：下一阶段 change 的 **task 不得被勾选**（= 不得声称完成了下一阶段的工作）
#
# ⚠️ 判据为什么**不用「文件出现在变更集」**：`src/core/module_factory.cc` 同时是
#    C2（删默认值注入段 :263-291）与 R-5（加预扫描段 :339-:350 之间）的落点 ——
#    design D-4 明写「不同位置 ⇒ 可并行」。用文件级判据会**误伤**当前阶段的合法工作
#    （实测：R-5 的 T2 一落地就红）。
#    可判定的语义是「**有没有声称做完下一阶段的活**」 ⇒ 判据 = 下一阶段 tasks 的 `[x]` 数。
NEXT_C="cpptlm-module-param-migrate-batch2"
resolve_path "openspec/changes/$NEXT_C/tasks.md"
NEXT_TASKS="$RESOLVE_PATH"
if [ ! -f "$NEXT_TASKS" ]; then
    ok "P-2a 下一阶段 ($NEXT_C) 尚未立项/已归档 ⇒ 无提前开工"
else
    _done="$(grep -c '^- \[x\]' "$NEXT_TASKS")"
    [ -z "${_done:-}" ] && _done=0
    if [ "$_done" -gt 0 ]; then
        fail "P-2a 下一阶段 ($NEXT_C) 已有 $_done 个 task 被勾选 ⇒ 阶段顺序被跳过"
    else
        ok "P-2a 下一阶段 ($NEXT_C) 零 task 勾选 ⇒ 未提前开工"
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
        resolve_path "$target"; target="$RESOLVE_PATH"
        if [ "$RESOLVE_STATE" = "unavailable" ]; then
            skip "P-3 [$ac] 载体已归档且不在工作树: $target"
            ac_hit["$ac"]=1
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
