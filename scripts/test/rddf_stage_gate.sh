#!/usr/bin/env bash
# scripts/test/rddf_stage_gate.sh
# 阶段顺序门禁 —— per 册子 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6.5
#
# ⚠️ **引用转指（per Q-6d 裁决）**：原引用指向 §1.5 的 P-1~P-6 判据小节 ⇒ §1.5 整节已作废（三个框架治理 change
#    于 2026-11-18~19 全部归档）⇒ 旧引用悬空 ⇒ 改指 **§6.5（顺序机关节）**。
#
# 缺口来源（critic 2026-11-18）：「依赖图画得漂亮但**没有任何机关**」
#   —— 顺序原本只由文风保证 ⇒ 下一个 session 可以直接从 C2 T1 开工而不违反任何机制。
# 本脚本把「顺序」与「AC 可寻址」变成**可机械判定**的闸。
#
# ⚠️ **P-2a 的两个分支**（per Q-6b「不新建第三个登记面」）：
#   · `--phase A|B|C` ⇒ 旧框架治理三 change 的阶段顺序（**保留**，不回退）
#   · `--phase queue2` ⇒ **本计划队列 2**（登记册 §6.4）的 I-* 实施顺序
#     ⇒ 「下一 I-* 已被声明完成」= 载体表内该 I-* 的 state 列 == done
#       （⛔ 至少一行标 done 即视为已声明 ⇒ 提前开工必红）
#     ⇒ **完成态的机器形态 = 载体表第 5 列**（不另立状态文件 ⇒ 不新建第三个登记面）
#
# 用法：
#   bash scripts/test/rddf_stage_gate.sh              # 全量检查（P-1~P-6）
#   bash scripts/test/rddf_stage_gate.sh --phase A    # 额外断言「阶段 A 的前置已完成」
#   bash scripts/test/rddf_stage_gate.sh --phase queue2  # 额外断言「队列 2 的下一 I-* 未被声明完成」
#
# 退出码：0 = 全绿；1 = 有项失败（每项打印可定位的原因）
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT" || exit 2

CARRIER="scripts/test/rddf_ac_carrier.tsv"
RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[0;33m'; RST=$'\033[0m'

# ── 参数解析（--phase 接受 A / B / C / queue2）────────────────────────────
# ⚠️ `--phase` 只影响 P-2a 的「下一阶段」判定（见 P-2 段），不影响其余判据。
PHASE_ARG=""
SELF_TEST=0        # ⚠️ **必须在参数解析之前初始化**（否则解析时置 1 会被后面的初始化覆盖）
while [ $# -gt 0 ]; do
    case "$1" in
        --phase)
            PHASE_ARG="${2:-}"
            if [ -z "$PHASE_ARG" ]; then
                echo "  ✗ --phase 缺少取值（合法值：A / B / C / queue2）" >&2
                exit 2
            fi
            case "$PHASE_ARG" in
                A|B|C|queue2) ;;
                *)
                    echo "  ✗ --phase 非法取值: $PHASE_ARG（合法值：A / B / C / queue2）" >&2
                    exit 2
                    ;;
            esac
            shift 2
            ;;
        --self-test)
            SELF_TEST=1
            shift
            ;;
        *)
            echo "  ✗ 未知参数: $1（合法参数：--phase <A|B|C|queue2> / --self-test）" >&2
            exit 2
            ;;
    esac
done
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
#
# ⚠️ **「下一阶段」随阶段推进而变**：
#    · 显式 `--phase <A|B|C>` ⇒ 按该阶段查下一阶段;
#    · **无 `--phase`** ⇒ 从「哪个 change 仍在 `openspec/changes/`（未归档）」**推导当前阶段**：
#      三段的 change 依次为 A=`cpptlm-module-param-effect-landing`、
#      B=`cpptlm-config-type-registration-align`、C=`cpptlm-module-param-migrate-batch2`。
#      当前阶段 = **最早一个仍未归档**的那个;下一阶段 = 它之后那个（没有 ⇒ 无对象）。
#      ⛔ 硬编码 `NEXT_C=C2` 在 C2 **自己开工后**会把当前阶段的合法勾选判成「提前开工」
#      （实测：C2 的 T0 一勾选 P-2a 就红）—— 判据必须随阶段推进而移动。
_stage_of() {  # $1 = change 名 → 输出 A/B/C（未知 ⇒ 空）
    case "$1" in
        cpptlm-module-param-effect-landing)   echo A ;;
        cpptlm-config-type-registration-align) echo B ;;
        cpptlm-module-param-migrate-batch2)    echo C ;;
    esac
}
_next_after() {  # $1 = A/B/C → 输出下一阶段（末段 ⇒ 空）
    case "$1" in
        A) echo B ;;
        B) echo C ;;
        *) echo "" ;;
    esac
}
_change_of_stage() {  # $1 = A/B/C → change 名
    case "$1" in
        A) echo cpptlm-module-param-effect-landing ;;
        B) echo cpptlm-config-type-registration-align ;;
        C) echo cpptlm-module-param-migrate-batch2 ;;
    esac
}

# ── P-2a · 队列 2（CEP 剩余任务计划）────────────────────────────────────
# ⚠️ **完成态的机器形态**（per Q-6b「不新建第三个登记面」+ 载体表第 5 列）：
#    `rddf_ac_carrier.tsv` 的第 5 列 `state ∈ {pending, done}`。
#    「下一 I-* 已被声明完成」= 载体表内该 I-* 的**所有**数据行的 state == done
#    （⛔ 至少一行标 done 即视为「已声明完成」⇒ 提前开工必红）。
#    ⛔ **不用行号**（队列会增删 ⇒ 行号位移）；⛔ **不用天数**（⛔ 不用「按天排序」判顺序）。
#
# 队列顺序（登记册 §6.4 队列 2 的实施行序）：
QUEUE2=(
  "I-0" "I-1" "I-9" "I-5" "I-2" "I-3" "I-4" "I-6a" "I-8" "I-10" "I-11" "I-12" "I-6" "I-7"
)
_queue2_done_count() {  # $1 = I-* 名 → 该 I-* 标 done 的数据行数
    awk -F'\t' -v q="$1" '!/^#/ && NF>=2 && $1==q && $5=="done" {n++} END{print n+0}' "$CARRIER"
}
_queue2_total_count() {  # $1 = I-* 名 → 该 I-* 的数据行总数
    awk -F'\t' -v q="$1" '!/^#/ && NF>=2 && $1==q {n++} END{print n+0}' "$CARRIER"
}
_queue2_next() {  # 找第一个「未全部 done」的 I-*（全部 done ⇒ 输出空）
    local q
    for q in "${QUEUE2[@]}"; do
        local tot done
        tot="$(_queue2_total_count "$q")"
        done="$(_queue2_done_count "$q")"
        # ⛔ 零数据行的 I-* 不算「已完成」（它还没被登记 ⇒ 视为未开工）
        if [ "$tot" -eq 0 ] || [ "$done" -lt "$tot" ]; then echo "$q"; return 0; fi
    done
    echo ""
}
# ⚠️ **「提前开工」的正确语义（per AC-2 变异实测修正）**：
#    ⛔ **不是**「下一项已 done」（那只是「已完成第一项」的正常状态）；
#    ✅ **是「跳序」** = 队列里**存在**一个 I-* 被声明 done，而它的**前驱**还没全部 done
#       ⇒ 后人先干完了 ⇒ 前置被跳过。
#    ⇒ 判据 = 遍历队列，对每个 I-*：若它已 done ∧ 其**前驱**未全部 done ⇒ 判红（签名含两者）。
_queue2_skip_violation() {  # 输出「<已done项> <未完成前驱>」；无 ⇒ 输出空
    local prev="" q tot done
    for q in "${QUEUE2[@]}"; do
        tot="$(_queue2_total_count "$q")"
        done="$(_queue2_done_count "$q")"
        if [ "$tot" -gt 0 ] && [ "$done" -eq "$tot" ]; then
            # 本项已全部 done ⇒ 检查它的前驱是否未全部 done（⛔ 队列首项无前驱 ⇒ 合法）
            if [ -n "$prev" ]; then
                local ptot pdone
                ptot="$(_queue2_total_count "$prev")"
                pdone="$(_queue2_done_count "$prev")"
                if [ "$ptot" -eq 0 ] || [ "$pdone" -lt "$ptot" ]; then
                    echo "$q $prev"
                    return 0
                fi
            fi
        fi
        prev="$q"
    done
    echo ""
}
if [ "$PHASE_ARG" = "queue2" ]; then
    # ── 队列 2 分支（CEP 剩余任务计划）──────────────────────────────────
    _viol="$(_queue2_skip_violation)"
    if [ -n "$_viol" ]; then
        _v_done="${_viol%% *}"
        _v_prev="${_viol##* }"
        fail "P-2a 队列 2 跳序：$_v_done 已声明完成，但前驱 $_v_prev 未完成 ⇒ 队列顺序被跳过"
    else
        NEXT_I="$(_queue2_next)"
        if [ -z "$NEXT_I" ]; then
            ok "P-2a 队列 2 全部 I-* 已声明完成 ⇒ 无下一项可提前开工"
        else
            ok "P-2a 队列 2 下一项 ($NEXT_I) ⇒ 无跳序 ⇒ 队列顺序未被跳过"
        fi
    fi
else
    # ── 旧规划（A/B/C 三阶段）分支 ──────────────────────────────────────
    CUR_STAGE="$PHASE_ARG"
    if [ -z "$CUR_STAGE" ]; then
        for _s in A B C; do
            _c="$(_change_of_stage "$_s")"
            if [ -d "openspec/changes/$_c" ]; then CUR_STAGE="$_s"; break; fi
        done
    fi

    if [ -z "$CUR_STAGE" ]; then
        ok "P-2a 三个阶段均已归档 ⇒ 无下一阶段可提前开工"
    else
        NEXT_STAGE="$(_next_after "$CUR_STAGE")"
        if [ -z "$NEXT_STAGE" ]; then
            ok "P-2a 当前阶段 ($CUR_STAGE) 已是序列末段 ⇒ 无下一阶段可提前开工"
        else
            NEXT_C="$(_change_of_stage "$NEXT_STAGE")"
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
        fi
    fi
fi

echo ""
# ⚠️ **自调用防护**(2026-11-21,per I-7a 落地时暴露):载体表的 `cmd` 行**可以**指向本
#    脚本自身(per I-7a 的 AC-13 行)⇒ P-3 会 `bash -c "$target $anchor"` 调起本脚本
#    ⇒ 内层又跑 P-3 ⇒ **无限递归**(实测 rc=124 超时 ⇒ 机关直接挂)。
# ⇒ 内层由环境变量 `RDDF_STAGE_GATE_INNER` 标记 ⇒ **整段跳过 P-3**
#    (外层正在执行同一条命令 ⇒ 内层重复跑 P-3 无意义;其余判据仍照跑)。
if [ -n "${RDDF_STAGE_GATE_INNER:-}" ]; then
    echo "═══ P-3 AC 可寻址（内层调用 ⇒ 跳过，避免自调用无限递归）═══"
    skip "P-3 内层跳过(外层正在执行同一条命令 ⇒ 防自调用递归)"
else
echo "═══ P-3 AC 可寻址（每个 AC 至少一条载体命中）═══"
if [ ! -f "$CARRIER" ]; then
    fail "P-3 载体表不存在: $CARRIER"
else
    declare -A ac_hit=()
    # ⚠️ **第 5 列 `state` 存在**（per Q-6b「不新建第三个登记面」⇒ 状态与载体同表）
    #    ⇒ 读取必须用 **5 变量**，否则 `anchor` 会被尾随的 state 污染
    #    （⛔ 回归记录：4 变量 read ⇒ anchor 变成 `'### D-1.2 类别 ①<TAB>pending'` ⇒ P-3 全红）
    while IFS=$'\t' read -r ac kind target anchor state; do
        case "${ac:-}" in ''|'#'*) continue;; esac
        # ⛔ state 合法性（第 5 列只允许两态；空 = 旧 4 列行 ⇒ 视为 pending）
        case "${state:-pending}" in
            pending|done) ;;
            *)
                fail "P-3 [$ac] state 非法: '${state:-}'（只允许 pending / done）"
                ;;
        esac
        if [ "$kind" = "TODO" ]; then
            # ⚠️ **TODO 行的时限判红**（per Q-6c 裁决：「TODO 行**必须**同时携带
            #    「升级时限 + 升级顺序」两个字段，且**最长存活 1 个工作日**（超过 ⇒ 判红）」）
            #    ⚠️ **理由**：闸对 TODO 原本是「跳过不计失败」⇒ 长期 TODO = 机关无牙
            #       （与「顺序只由文风保证」是同一种病）。
            #    ⚠️ **该时限是流程松紧取舍**（严格说属 user 域）⇒ architect 裁为**默认值**，
            #       **可被 user 修订** ⇒ ⛔ 不得表述为不可协商的技术约束（per 裁决 §8.3 L-2）。
            #    ⇒ 载体表格式：`TODO<TAB><说明><TAB>TODO!<升级时限 YYYY-MM-DD>|<升级顺序 N>`
            #       （第 4 列承载两个字段，用 `|` 分隔 ⇒ ⛔ 不新增第 6 列）
            _todo_meta="${anchor:-}"
            _todo_due=""
            _todo_seq=""
            case "$_todo_meta" in
                *"!"*)
                    _todo_due="$(printf '%s' "$_todo_meta" | sed -n 's/.*!\([0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}\).*/\1/p')"
                    _todo_seq="$(printf '%s' "$_todo_meta" | sed -n 's/.*|\([0-9][0-9]*\)$/\1/p')"
                    ;;
            esac
            if [ -z "$_todo_due" ] || [ -z "$_todo_seq" ]; then
                fail "P-3 [$ac] TODO 行**缺少「升级时限 + 升级顺序」**（格式: TODO!<YYYY-MM-DD>|<N>）⇒ 机关无牙（per Q-6c）"
            else
                _today="$(date +%Y-%m-%d)"
                # ⛔ 工作日口径：按「自然日差 > 2 天」近似 1 个工作日（周末不单独计算 ⇒ 偏保守）
                _days=$(( ( $(date -d "$_today" +%s) - $(date -d "$_todo_due" +%s) ) / 86400 ))
                if [ "$_days" -gt 2 ]; then
                    fail "P-3 [$ac] TODO 行已超升级时限（到期 $_todo_due · 顺序 $_todo_seq · 已过 $_days 天 > 1 工作日）⇒ 判红（per Q-6c）"
                else
                    skip "P-3 [$ac] 载体尚未产出（TODO · 到期 $_todo_due · 顺序 $_todo_seq · 剩余 $((2 - _days)) 天内须升级）"
                fi
            fi
            continue
        fi
        if [ "$kind" = "cmd" ]; then
            if [ "$target" = "./build/bin/cpptlm_tests" ] && [ "$anchor" = "--list-tests" ]; then
                if [ -x ./build/bin/cpptlm_tests ]; then ok "P-3 [$ac] cmd 可执行"; ac_hit["$ac"]=1; else fail "P-3 [$ac] cmd 不可执行: $target"; fi
            elif [ "$target" = "scripts/test/param_effect_inventory.py" ]; then
                if python3 scripts/test/param_effect_inventory.py --json >/dev/null 2>&1; then ok "P-3 [$ac] cmd rc=0"; ac_hit["$ac"]=1; else fail "P-3 [$ac] cmd 非 0: $target"; fi
            else
                # ⚠️ **自调用防护**:带 `RDDF_STAGE_GATE_INNER=1` ⇒ 内层跳过 P-3
                #    ⇒ `cmd` 行指向本脚本自身时**不会**无限递归(见 P-3 段首的说明)
                if RDDF_STAGE_GATE_INNER=1 bash -c "$target $anchor" >/dev/null 2>&1; then ok "P-3 [$ac] cmd rc=0"; ac_hit["$ac"]=1; else fail "P-3 [$ac] cmd 非 0: $target $anchor"; fi
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
fi   # ← 闭合「内层跳过 P-3」的 else 分支

echo ""
echo "═══ P-4 跨阶段判别力不丢（A 阶段登记的判别测试不得被静默删除）═══"
EFF="test/test_param_effect_landing.cc"
if [ ! -f "$EFF" ]; then
    fail "P-4 判别测试文件不存在: $EFF"
else
    while IFS=$'\t' read -r ac kind target anchor state; do
        [ "${kind:-}" = "test" ] || continue
        [ "$target" = "$EFF" ] || continue
        if grep -qF "$anchor" "$EFF"; then :; else fail "P-4 [$ac] 判别 case 已消失: '$anchor'"; fi
    done < "$CARRIER"
    ok "P-4 A 阶段登记的判别 case 全部在位"
fi

echo ""
echo "═══ P-5 KPI 回填不是「改数字」（活口径处数 + 一致性）═══"
# ⚠️ **锚定形态 per Q-6a 裁决（措辞锚定 + 命中数计数）**：
#   ⛔ **不用行号**（活口径行本身在被反复编辑 ⇒ 行号会位移）
#   ⛔ **不用宽子串**（台账流水里同形措辞会造成假阳；见 obj 的 3 处 vs 粗体精确锚 1 处）
#   ✅ **联合命中语义**（不是「先计数、再逐行取值」两步）：
#      「对每个文件 —— **存在至少一处**命中该文件的**精确锚**
#        ∧ **该锚所在行内**的 `N/3` == 实测 `active/3`
#        ∧ **该精确锚的命中处数 == 期望处数**」
#      ⇒ 「台账流水里的同形措辞」**既不计入处数、也不参与取值判定** ⇒ 无歧义。
#
# ⚠️ **通用纪律**：任何锚点子串在落盘前必须跑 `grep -c` 复验（锚点期望处数写死在表里）。
# ⚠️ **`LIVE_FILES` 不增补**（Q-6a 裁决：6 文件即全集）；**新增活口径载体 ⇒ 必须同步进本表**。
#
# 格式：<文件路径> <TAB> <精确锚> <TAB> <期望命中处数>
LIVE_ANCHORS=(
  "AGENTS.md|实测基线|1"
  ".rddf/roadmap.md|当前真实 baseline = |1"
  ".rddf/roadmap/phases/phase-10.md|当前真实 baseline = |1"
  ".rddf/roadmap/objectives/obj-cep-platform.md|**当前 Proposed 并发 = |1"
  ".rddf/improvements/cpptlm-cep-umbrella.md|当前真实 baseline = |1"
  ".rddf/improvements/cpptlm-cep-nvlink-sharp.md|当前实测 **|2"
)
LIVE_FILES=(
  "AGENTS.md"
  ".rddf/roadmap.md"
  ".rddf/roadmap/phases/phase-10.md"
  ".rddf/roadmap/objectives/obj-cep-platform.md"
  ".rddf/improvements/cpptlm-cep-umbrella.md"
  ".rddf/improvements/cpptlm-cep-nvlink-sharp.md"
)
# ── 活口径**行**行长判据的受测文件（per 单元 `cep-doc-hygiene-kpi-and-impl-state` AC-1：
#    「断言载体须可机测（**并入既有闸**），⛔ 不新建闸脚本类型」）
# ⚠️ **只含 I-2 的 4 份 In-Scope 文档** —— `AGENTS.md` 与 `obj-cep-platform.md`
#    **不在**该单元范围 ⇒ 天然免疫（per 该单元 AC-2 的「范围对账」注：
#    `obj-cep-platform.md` 的锚点行本身 2963 字符 ⇒ 若纳入本判据会立即判红且与范围矛盾）。
# ⚠️ 上限 1000 与 `scripts/test/agents_md_lint.py` 的既有单行阈同阈。
KPI_LINE_MAX_FILES=(
  ".rddf/roadmap.md"
  ".rddf/roadmap/phases/phase-10.md"
  ".rddf/improvements/cpptlm-cep-umbrella.md"
  ".rddf/improvements/cpptlm-cep-nvlink-sharp.md"
)
KPI_LINE_MAX=1000
_active="$(ls openspec/changes 2>/dev/null | grep -v '^archive$' | wc -l | tr -d ' ')"
_unsynced=0
_total_anchor_hits=0
_expected_anchor_hits=0
for _entry in "${LIVE_ANCHORS[@]}"; do
    _f="${_entry%%|*}"
    _rest="${_entry#*|}"
    _anchor="${_rest%%|*}"
    _exp="${_rest##*|}"
    if [ ! -f "$_f" ]; then
        fail "P-5 活口径文件不存在: $_f"
        _unsynced=$((_unsynced + 1))
        continue
    fi
    # ① 该精确锚的命中处数 == 期望处数（⛔ 少锚 ⇒ 未来漂移抓不到）
    _hits="$(grep -cF "$_anchor" "$_f" || true)"
    [ -z "${_hits:-}" ] && _hits=0
    _total_anchor_hits=$((_total_anchor_hits + _hits))
    _expected_anchor_hits=$((_expected_anchor_hits + _exp))
    if [ "$_hits" -ne "$_exp" ]; then
        fail "P-5 [$_f] 精确锚命中处数 $_hits != 期望 $_exp（锚='$_anchor'）⇒ 锚点表与磁盘漂移"
        _unsynced=$((_unsynced + 1))
        continue
    fi
    # ② 该锚所在行内，取值 N/3 == 实测 active/3（⛔ 不是「文件内存在 N/3 子串」）
    _bad=0
    _found=0
    while IFS= read -r _line; do
        [ -z "$_line" ] && continue
        _found=$((_found + 1))
        # ① 行长判据（per I-2 的 AC-1）—— 活口径行超长 ⇒ KPI 行堆积（锚点句被流水淹没）
        #    ⚠️ `${#_line}` 在 UTF-8 locale 下取**字符数**（与 `agents_md_lint.py` 的单行阈同口径）
        for _lf in "${KPI_LINE_MAX_FILES[@]}"; do
            if [ "$_f" = "$_lf" ] && [ "${#_line}" -gt "$KPI_LINE_MAX" ]; then
                fail "P-5 [$_f] 活口径行长度 ${#_line} > $KPI_LINE_MAX ⇒ KPI 行堆积（per I-2 的 AC-1;锚='$_anchor'）"
                _bad=$((_bad + 1))
            fi
        done
        _val="$(printf '%s' "$_line" | grep -oE '[0-9]+[[:space:]]*/[[:space:]]*3' | head -1 | grep -oE '[0-9]+' | head -1)"
        if [ -z "${_val:-}" ]; then
            fail "P-5 [$_f] 锚所在行内未找到 N/3 取值（锚='$_anchor'）"
            _bad=$((_bad + 1))
        elif [ "$_val" != "$_active" ]; then
            fail "P-5 [$_f] 活口径取值 $_val/3 != 实测 $_active/3 ⇒ 活口径未同步（锚='$_anchor'）"
            _bad=$((_bad + 1))
        fi
    done < <(grep -F "$_anchor" "$_f" || true)
    if [ "$_found" -eq 0 ]; then
        fail "P-5 [$_f] 精确锚未命中（锚='$_anchor'）"
        _bad=$((_bad + 1))
    fi
    if [ "$_bad" -gt 0 ]; then _unsynced=$((_unsynced + 1)); fi
done
# ③ 总处数守恒：6 文件 / 7 处（nvlink-sharp.md 2 处）
if [ "$_total_anchor_hits" -ne "$_expected_anchor_hits" ]; then
    fail "P-5 活口径锚定总处数 $_total_anchor_hits != 期望 $_expected_anchor_hits"
    _unsynced=$((_unsynced + 1))
elif [ "$_total_anchor_hits" -ne 7 ]; then
    fail "P-5 活口径锚定总处数 $_total_anchor_hits != 7（6 文件 / 7 处；nvlink-sharp.md 2 处）"
    _unsynced=$((_unsynced + 1))
fi
[ "$_unsynced" -eq 0 ] && ok "P-5 $_active 处活口径全部同步（锚定 $_total_anchor_hits 处，逐处取值 == 实测）"

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

# ── --self-test：决定性变异自测段 ─────────────────────────────────────────
# ⚠️ **为什么要有这一段**（per 草稿 What Changes #6）：AC-2 / AC-3 的「决定性变异」
#    必须**可由一条命令复现**，否则「判据有鉴别力」只停留在口述 ⇒ 机关无牙。
# ⚠️ **不注册 ctest**（⛔ 理由：自测段会**复制并变异**文件 ⇒ 放 CI 里每次都跑一遍
#    成本与风险都不划算；它是**开发期/评审期**的证据，不是回归闸）。
#    ⛔ **真实仓字节指纹前后必须相等**（变异只在影子副本内发生）。
if [ "$SELF_TEST" = "1" ]; then
    echo ""
    echo "═══ SELF-TEST 决定性变异自测（影子副本内，真实仓零改动）═══"
    _shadow="$(mktemp -d "${TMPDIR:-/tmp}/rddf_stage_gate_selftest.XXXXXX")" || exit 2
    trap 'rm -rf "$_shadow"' EXIT
    # 影子骨架：闸 + 载体表 + 登记册 + 6 个活口径文件
    mkdir -p "$_shadow/scripts/test" "$_shadow/.rddf/roadmap/phases" "$_shadow/.rddf/roadmap/objectives" "$_shadow/.rddf/improvements"
    cp "$CARRIER" "$_shadow/scripts/test/" 2>/dev/null
    cp "$0" "$_shadow/scripts/test/rddf_stage_gate.sh" 2>/dev/null
    cp .rddf/roadmap/objectives/obj-cep-platform-remaining.md "$_shadow/.rddf/roadmap/objectives/" 2>/dev/null
    # ⚠️ **台账本体也必须拷入影子**（它是第 4 个活口径文件 ⇒ P-5 要读它的锚点行）
    cp .rddf/roadmap/objectives/obj-cep-platform.md "$_shadow/.rddf/roadmap/objectives/" 2>/dev/null
    cp .rddf/roadmap.md "$_shadow/.rddf/" 2>/dev/null
    cp .rddf/roadmap/phases/phase-10.md "$_shadow/.rddf/roadmap/phases/" 2>/dev/null
    cp AGENTS.md "$_shadow/" 2>/dev/null
    cp .rddf/improvements/cpptlm-cep-umbrella.md "$_shadow/.rddf/improvements/" 2>/dev/null
    cp .rddf/improvements/cpptlm-cep-nvlink-sharp.md "$_shadow/.rddf/improvements/" 2>/dev/null
    # 真实仓字节指纹（变异前）
    _fp_before="$(for f in AGENTS.md .rddf/roadmap.md .rddf/roadmap/phases/phase-10.md \
                     .rddf/roadmap/objectives/obj-cep-platform.md \
                     .rddf/improvements/cpptlm-cep-umbrella.md \
                     .rddf/improvements/cpptlm-cep-nvlink-sharp.md "$CARRIER"; do
                   [ -f "$f" ] && md5sum "$f"; done | md5sum | cut -d" " -f1)"
    _self_fail=0

    # ⚠️ **影子环境完整性前置检查**（否则红因会误导）：
    #    若 6 个活口径文件未全部拷入影子 ⇒ P-5 会报「活口径文件不存在」而非「取值不符」
    #    ⇒ 变异① 的命中计数会是 0/6 ⇒ ⛔ **不能**把它误报成「判据仍可被平凡满足」。
    _missing_live=0
    for _lf in AGENTS.md .rddf/roadmap.md .rddf/roadmap/phases/phase-10.md \
                 .rddf/roadmap/objectives/obj-cep-platform.md \
                 .rddf/improvements/cpptlm-cep-umbrella.md \
                 .rddf/improvements/cpptlm-cep-nvlink-sharp.md; do
        [ -f "$_shadow/$_lf" ] || { fail "SELF-TEST 环境不完整：影子缺活口径文件 $_lf（⛔ 非判据缺陷）"; _missing_live=1; }
    done
    if [ "$_missing_live" -gt 0 ]; then
        echo -e "${RED}SELF-TEST 失败：影子环境不完整（${_missing_live} 个活口径文件缺失）${RST}"
        rm -rf "$_shadow"
        exit 1
    fi

    # ── 变异 ①：P-5 判据鉴别力（7 处活口径全改 3/3 ⇒ 必须判红 6/6）────────
    python3 - "$_shadow" <<'PYEOF'
import re, sys
root = sys.argv[1]
targets = [("AGENTS.md","实测基线"), (".rddf/roadmap.md","当前真实 baseline = "),
           (".rddf/roadmap/phases/phase-10.md","当前真实 baseline = "),
           (".rddf/roadmap/objectives/obj-cep-platform.md","**当前 Proposed 并发 = "),
           (".rddf/improvements/cpptlm-cep-umbrella.md","当前真实 baseline = "),
           (".rddf/improvements/cpptlm-cep-nvlink-sharp.md","当前实测 **")]
n = 0
for f, a in targets:
    p = f"{root}/{f}"
    try: lines = open(p, encoding="utf-8").read().split("\n")
    except OSError: continue
    ch = False
    for i, l in enumerate(lines):
        if a in l:
            new = re.sub(r"(\d+)(\s*/\s*3)", r"3\2", l, count=1)
            if new != l: lines[i] = new; n += 1; ch = True
    if ch: open(p, "w", encoding="utf-8").write("\n".join(lines))
print(n)
PYEOF
    _mut1="$(cd "$_shadow" && bash scripts/test/rddf_stage_gate.sh 2>&1 | grep -c 'P-5 \[.*\] 活口径取值 3/3 != 实测 0/3')"
    if [ "${_mut1:-0}" -ge 6 ]; then
        ok "SELF-TEST 变异① P-5 判红 $_mut1/6 处（7 处锚定行全改 3/3）⇒ 判据有鉴别力"
    else
        fail "SELF-TEST 变异① P-5 仅判红 ${_mut1:-0}/6（期望 ≥6）⇒ 判据仍可被平凡满足"
        _self_fail=1
    fi

    # ── 变异 ②：队列 2 跳序（**动态**取「当前下一项」的后继标 done ⇒ 必须判红）──
    # ⚠️ **必须动态**（⛔ 不可硬编码 I-0/I-1）：本单元完成后 I-0 的 state 变 `done`
    #    ⇒ 「当前下一项」随之推进 ⇒ 硬编码的变异会退化成「合法顺序」⇒ 自测失效。
    rm -rf "$_shadow"; mkdir -p "$_shadow/scripts/test" "$_shadow/.rddf/roadmap/objectives"
    cp "$CARRIER" "$_shadow/scripts/test/"; cp "$0" "$_shadow/scripts/test/rddf_stage_gate.sh"
    cp .rddf/roadmap/objectives/obj-cep-platform-remaining.md "$_shadow/.rddf/roadmap/objectives/" 2>/dev/null
    _mut2_pair="$(python3 - "$_shadow" <<'PYEOF'
import sys
root = sys.argv[1]
QUEUE2 = ["I-0","I-1","I-9","I-5","I-2","I-3","I-4","I-6a","I-8","I-10","I-11","I-12","I-6","I-7"]
p = f"{root}/scripts/test/rddf_ac_carrier.tsv"
lines = open(p, encoding="utf-8").read().split("\n")
def state(q):
    for l in lines:
        if l.startswith(q + "\t"):
            return l.split("\t")[4] if len(l.split("\t")) > 4 else ""
    return ""
def total(q):
    return sum(1 for l in lines if l.startswith(q + "\t"))
# 当前下一项 = 第一个「未全部 done」的
nxt = None
for q in QUEUE2:
    if total(q) == 0 or state(q) != "done":
        nxt = q; break
# 跳序目标 = 它的**后继**（后继 done 而前驱未 done ⇒ 跳序）
if nxt is not None:
    i = QUEUE2.index(nxt)
    def mark(q, st):
        for k, l in enumerate(lines):
            if l.startswith(q + "\t"):
                parts = l.split("\t"); parts[4] = st; lines[k] = "\t".join(parts)
    if i + 1 < len(QUEUE2):
        tgt = QUEUE2[i+1]
        mark(tgt, "done")
        open(p, "w", encoding="utf-8").write("\n".join(lines))
        print(f"{nxt} {tgt}")
    elif i > 0:
        # ⚠️ **队尾特例(2026-11-21 实测 I-6 完成后暴露)**:`nxt` 已是**最后一项**
        #    ⇒ 无「后继」可标 done ⇒ 原实现什么也不改 ⇒ `_mut2_pair` 为空 ⇒ 自测误报
        #    「P-2a 无鉴别力」。⇒ 反向构造:把 `nxt` 标 done **且**把它的前驱改回 pending
        #    ⇒ P-2a 仍看到「q done 而前驱未 done」⇒ 判据可被变异驱动(⛔ 判据本身不动)。
        prevq = QUEUE2[i-1]
        mark(nxt, "done"); mark(prevq, "pending")
        open(p, "w", encoding="utf-8").write("\n".join(lines))
        print(f"{nxt} {prevq}")
PYEOF
    )"
    _mut2="$(cd "$_shadow" && bash scripts/test/rddf_stage_gate.sh --phase queue2 2>&1 | grep -c 'P-2a 队列 2 跳序')"
    if [ "${_mut2:-0}" -ge 1 ]; then
        ok "SELF-TEST 变异② 队列 2 跳序被判红（${_mut2_pair:-?} 标 done 而前驱未 done）⇒ 顺序由机关保证"
    else
        fail "SELF-TEST 变异② 队列 2 跳序未判红 ⇒ P-2a 无鉴别力"
        _self_fail=1
    fi

    # ── 变异 ③：合法顺序不得误红（**动态**取「当前下一项」标 done）──────────
    # ⚠️ **必须动态**（同上）：硬编码 I-0 ⇒ 本单元完成后 I-0 已 done ⇒ 该变异变成
    #    「把 I-1 标 done 而 I-0 已 done」= 合法顺序 ⇒ 测不到「合法不误红」的真实场景。
    rm -rf "$_shadow"; mkdir -p "$_shadow/scripts/test" "$_shadow/.rddf/roadmap/objectives"
    cp "$CARRIER" "$_shadow/scripts/test/"; cp "$0" "$_shadow/scripts/test/rddf_stage_gate.sh"
    cp .rddf/roadmap/objectives/obj-cep-platform-remaining.md "$_shadow/.rddf/roadmap/objectives/" 2>/dev/null
    _mut3_next="$(python3 - "$_shadow" <<'PYEOF'
import sys
root = sys.argv[1]
QUEUE2 = ["I-0","I-1","I-9","I-5","I-2","I-3","I-4","I-6a","I-8","I-10","I-11","I-12","I-6","I-7"]
p = f"{root}/scripts/test/rddf_ac_carrier.tsv"
lines = open(p, encoding="utf-8").read().split("\n")
def state(q):
    for l in lines:
        if l.startswith(q + "\t"):
            return l.split("\t")[4] if len(l.split("\t")) > 4 else ""
    return ""
def total(q):
    return sum(1 for l in lines if l.startswith(q + "\t"))
nxt = None
for q in QUEUE2:
    if total(q) == 0 or state(q) != "done":
        nxt = q; break
if nxt is not None:
    for k, l in enumerate(lines):
        if l.startswith(nxt + "\t"):
            parts = l.split("\t"); parts[4] = "done"; lines[k] = "\t".join(parts)
    open(p, "w", encoding="utf-8").write("\n".join(lines))
    print(nxt)
PYEOF
    )"
    _mut3="$(cd "$_shadow" && bash scripts/test/rddf_stage_gate.sh --phase queue2 2>&1 | grep -c 'P-2a 队列 2 跳序')"
    if [ "${_mut3:-0}" -eq 0 ]; then
        ok "SELF-TEST 变异③ 合法顺序（${_mut3_next:-?} 置 done 且前驱已全 done）未误判跳序 ⇒ 判据无假阳"
    else
        fail "SELF-TEST 变异③ 合法顺序被误判跳序 ⇒ 判据有假阳"
        _self_fail=1
    fi

    # ── 真实仓字节指纹（变异后）必须相等 ─────────────────────────────────
    _fp_after="$(for f in AGENTS.md .rddf/roadmap.md .rddf/roadmap/phases/phase-10.md \
                    .rddf/roadmap/objectives/obj-cep-platform.md \
                    .rddf/improvements/cpptlm-cep-umbrella.md \
                    .rddf/improvements/cpptlm-cep-nvlink-sharp.md "$CARRIER"; do
                  [ -f "$f" ] && md5sum "$f"; done | md5sum | cut -d" " -f1)"
    if [ "$_fp_before" = "$_fp_after" ]; then
        ok "SELF-TEST 真实仓字节指纹前后相等（$_fp_before）⇒ 变异未污染真实仓"
    else
        fail "SELF-TEST 真实仓字节指纹变化（before=$_fp_before after=$_fp_after）⇒ 影子副本泄漏"
        _self_fail=1
    fi

    rm -rf "$_shadow"
    echo ""
    if [ "$_self_fail" -gt 0 ]; then
        echo -e "${RED}SELF-TEST 失败：$_self_fail 项${RST}"
        exit 1
    fi
    echo -e "${GRN}SELF-TEST 全部通过（3 项决定性变异 + 字节指纹）${RST}"
    exit 0
fi

echo ""
echo "════════════════════════════════════════════"
if [ "$FAILED" -gt 0 ]; then
    echo -e "${RED}阶段门禁失败：$FAILED 项失败 / $SKIPPED 项跳过${RST}"
    exit 1
fi
echo -e "${GRN}阶段门禁通过（$SKIPPED 项跳过）${RST}"
exit 0