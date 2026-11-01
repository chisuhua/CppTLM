#!/usr/bin/env bash
# scripts/test/cep_mesif_adapter_reverse_verify.sh
#   —— 证明 MESIF **实现层**判据(队列 #9 `cpptlm-cep-mesif-adapter` 的
#      MA-01 ~ MA-05 / MA-08 / HD-01 ~ HD-05)**有牙**的反向验证
#
# ── 与既有先例的口径差异(显式说明,per design `D-η` ③)────────────────
# `cep_mesif_state_spec_reverse_verify.sh` 把「编译失败」与「运行期谓词失败」
# **合并计数**(「两者都算被抓到」)⇒ 「**因正确原因**红」没有机器区分。
# 本脚本**不沿用**该口径:
#   · 每条 fixture 声明**期望失败模式**:`compile-lock` 或 `runtime-predicate`;
#   · `compile-lock` ⇒ 必须在编译日志里命中**指定的 `static_assert` 文案**;
#   · `runtime-predicate` ⇒ 必须在探针输出里命中**指定的判据 id**(且编译必须成功);
#   · 模式不符(该红编译期却红在运行期 / 反之)⇒ **直接判失败**,不给「反正红了」放行。
#
# ── 两层判据 id(归因不模糊)────────────────────────────────────────
# 探针按层打印 `FAIL [spec|impl|decl] <id>`。规约载体带 14 条编译期锁 ⇒ 毒化规约载体
# **通常**在编译期先红,`[impl]` 层不会运行 —— 这是**层序**而非漏判,脚本对 `compile-lock`
# 型 fixture 会显式记录「impl 层未运行」。
#
# ⚠️ 本脚本**只写自己的临时目录**,**不改**仓内任何文件 ⇒ 可安全进 ctest 并行运行。
#
# 用法:bash scripts/test/cep_mesif_adapter_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_mesif_adapter_probe.cc"
CEP_DIR="include/tlm/cep"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_mesif_adapter_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

CXX="${CXX:-g++}"
INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/test"
          -I "$ROOT/external/json")
pass=0; fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }

# ── fixture 定义 ───────────────────────────────────────────────────
# 格式:kind|file|old|new|mode|expected
#   mode = runtime-predicate  ⇒ expected = 判据 id 列表(空格分隔,形如 `impl:MA-01-ZERO-COPY`)
#   mode = compile-lock       ⇒ expected = `static_assert` 文案片段
FIXTURES=(
  # ── 实现层:adapter 载体 ───────────────────────────────────────────
  "adapter-forged-table|include/tlm/cep/mesif_adapter.hh|        /// 映射轴 = **链路事务视角**|        static constexpr std::uint8_t kTable[5][6] = {};  /// 映射轴 = **链路事务视角**|runtime-predicate|impl:MA-01-ZERO-COPY"
  "adapter-mapping-missing-row|include/tlm/cep/mesif_adapter.hh|            {\"query\", bundles::NoCFlitBundle::CATEGORY_REQUEST},|            {\"queryx\", bundles::NoCFlitBundle::CATEGORY_REQUEST},|runtime-predicate|impl:MA-03-IDENTITY"
  "adapter-mapping-wrong-category|include/tlm/cep/mesif_adapter.hh|            {\"snoop_resp\", bundles::NoCFlitBundle::CATEGORY_RESPONSE},|            {\"snoop_resp\", bundles::NoCFlitBundle::CATEGORY_REQUEST},|runtime-predicate|impl:MA-03-IDENTITY"
  "adapter-identity-flag-lie|include/tlm/cep/mesif_adapter.hh|        static constexpr bool kMappingIsIdentity = true;|        static constexpr bool kMappingIsIdentity = false;|runtime-predicate|impl:MA-03-IDENTITY-REASON"
  "adapter-transient-declared|include/tlm/cep/mesif_adapter.hh|        static constexpr std::size_t kTransientStateCount = 0;|        static constexpr std::size_t kTransientStateCount = 1;|runtime-predicate|impl:MA-02-TRANSIENT"
  # ── 实现层:tracker 载体(第 6 种实现)──────────────────────────────
  "tracker-caps-drift|include/tlm/cep/sharer_tracker.hh|    SharerTrackerCaps caps = kTrackerTraits[5].caps;|    SharerTrackerCaps caps = kTrackerTraits[0].caps;|runtime-predicate|impl:HD-01-CAPS"
  "tracker-aggregation-degraded|include/tlm/cep/sharer_tracker.hh|        for (std::size_t l = 0; l <= level; ++l) {|        for (std::size_t l = 0; l <= kRootLevel; ++l) {|runtime-predicate|impl:HD-02-AGGREGATION"
  "tracker-decision-topic-empty|include/tlm/cep/sharer_tracker.hh|{\"B-1\", \"层级数与层级语义(L0 最细 → L2 最粗)\",|{\"B-1\", \"\",|runtime-predicate|impl:HD-04-DECISIONS"
  "tracker-decision-not-pending|include/tlm/cep/sharer_tracker.hh|         \" + ADR-CEP-04 §13 的「层级深度是实现参数,不是能力属性」\", true},|         \" + ADR-CEP-04 §13 的「层级深度是实现参数,不是能力属性」\", false},|runtime-predicate|impl:HD-04-DECISIONS"
  "tracker-F-gap-dropped|include/tlm/cep/sharer_tracker.hh|    static constexpr bool kForwardPointRoleRepresentable = false;|    static constexpr bool kForwardPointRoleRepresentable = true;|runtime-predicate|impl:HD-05-GAP"
  # ── 规约载体(编译期锁;实现层不会运行)────────────────────────────
  "spec-rank-regression|include/tlm/cep/mesif_state_spec.hh|                Perm::WRITEBACK, // M|                Perm::READ, // M|compile-lock|MESIF 秩口径被破坏"
  "spec-write-rule|include/tlm/cep/mesif_state_spec.hh|{1, 2, 1, 2, 1, 0}, // S :|{1, 2, 2, 2, 1, 0}, // S :|compile-lock|MESIF 写规则被破坏"
  # ── 声明层 ────────────────────────────────────────────────────────
  "decl-upgrade-row-missing|perm_upgrades.json|||runtime-predicate|decl:MA-04-UPGRADE-ROW"
  "decl-current-state-lie|perm_upgrades.json|**4 个实例全部具备该能力**|`mesif` 无该能力|runtime-predicate|decl:MA-04-CURRENT-STATE"
  "decl-status-bogus|protocol_instances.json|||runtime-predicate|decl:MA-05-STATUS-DOMAIN"
  "decl-vc-out-of-range|protocol_instances.json|          \"query\": 2,|          \"query\": 9,|runtime-predicate|decl:MA-08-VC-BOUND"
)

echo ""
echo "═══ 基线:干净载体必须编译并通过探针(正控)═══"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/base_build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/base_build.log" | head -20; exit 1
fi
ok "探针构建成功(干净载体)"

BASE_LOG="$TMP/base_run.log"
if "$TMP/probe" "$ROOT" >"$BASE_LOG" 2>&1; then
    ok "基线:干净载体全部判据通过($(tail -1 "$BASE_LOG"))"
else
    bad "基线:干净载体判据红 —— $(grep -c '^FAIL' "$BASE_LOG") 条失败"
    sed 's/^/      /' "$BASE_LOG" | head -10
fi

# ── 单条 fixture ────────────────────────────────────────────────────
run_fixture() {
    local kind="$1" file="$2" old="$3" new="$4" mode="$5" expected="$6"
    local work="$TMP/$kind"
    local mrc=0
    local adapter_override=""
    mkdir -p "$work"
    local json_dir="$ROOT/$CEP_DIR"
    local extra_inc=()

    case "$file" in
        *.json)
            # 声明源毒化:把 4 个 JSON 复制到影子目录,只改其中一份
            mkdir -p "$work/json"
            local f
            for f in protocol_instances.json protocol_instance.schema.json perm_upgrades.json fabric.json; do
                cp "$ROOT/$CEP_DIR/$f" "$work/json/$f" || { bad "$kind 声明源复制失败"; return; }
            done
            json_dir="$work/json"
            if [ "$kind" = "decl-upgrade-row-missing" ]; then
                python3 - "$work/json/perm_upgrades.json" <<'PY' 2>"$work/mut.log"
import json, sys
p = sys.argv[1]
d = json.load(open(p, encoding='utf-8'))
before = len(d['perm_upgrade']['silent_upgrades'])
d['perm_upgrade']['silent_upgrades'] = [u for u in d['perm_upgrade']['silent_upgrades']
                                        if u.get('instance_id') != 'mesif']
after = len(d['perm_upgrade']['silent_upgrades'])
if before - after != 1:
    print(f"移除行数 {before - after}(必须恰好 1)—— 毒化是空操作")
    sys.exit(3)
open(p, 'w', encoding='utf-8').write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")
PY
                mrc=$?
            elif [ "$kind" = "decl-status-bogus" ]; then
                # ⚠️ 必须**结构化**改:`"status"` 的值会随实现进度**翻转**(planned → implemented)
                #    ⇒ 字符串锚点会随状态变迁**失效**(本脚本在 status 翻转后实测即红了一次)。
                #    这正是判据纪律第 4 条的形态:状态变迁 ⇒ fixture 必须重新基线化,
                #    且脚本要**响亮报红**(而不是静默跳过)。
                python3 - "$work/json/protocol_instances.json" <<'PY' 2>"$work/mut.log"
import json, sys
p = sys.argv[1]
d = json.load(open(p, encoding='utf-8'))
hit = 0
for inst in d['instances']:
    if inst.get('id') != 'mesif':
        continue
    inst['status'] = 'bogus'          # 越域值:schema enum 只有 planned/implemented/excluded
    hit += 1
if hit != 1:
    print(f"mesif 命中 {hit} 次(必须恰好 1)—— 毒化是空操作")
    sys.exit(3)
open(p, 'w', encoding='utf-8').write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")
PY
                mrc=$?
            elif [ "$kind" = "decl-vc-out-of-range" ]; then
                # ⚠️ 必须**结构化**改 JSON:`"query": 2` 在 `moesi_gpu` 与 `mesif` 里
                #    各出现一次 ⇒ 字符串替换不唯一(会被 anchor 唯一性检查挡下)
                python3 - "$work/json/protocol_instances.json" <<'PY' 2>"$work/mut.log"
import json, sys
p = sys.argv[1]
d = json.load(open(p, encoding='utf-8'))
hit = 0
for inst in d['instances']:
    if inst.get('id') != 'mesif':
        continue
    inst['vc_binding']['ualink_fabric']['query'] = 9   # fabric 的 vc_count = 4 ⇒ 越界
    hit += 1
if hit != 1:
    print(f"mesif 命中 {hit} 次(必须恰好 1)—— 毒化是空操作")
    sys.exit(3)
open(p, 'w', encoding='utf-8').write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")
PY
                mrc=$?
            else
                python3 - "$work/json/$file" "$old" "$new" <<'PY' 2>"$work/mut.log"
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(path, encoding='utf-8').read()
n = s.count(old)
if n != 1:
    print(f"旧串命中 {n} 次(必须恰好 1 次)—— 毒化是空操作或定位不唯一")
    sys.exit(3)
open(path, 'w', encoding='utf-8').write(s.replace(old, new, 1))
PY
                mrc=$?
            fi
            ;;
        *)
            # 头文件毒化:影子 include 目录优先于真实 include
            local shadow="$work/shadow"
            local rel="${file#include/}"          # `#include "tlm/cep/x.hh"` ⇒ 影子根下同名相对路径
            mkdir -p "$shadow/$(dirname "$rel")"
            cp "$ROOT/$file" "$shadow/$rel" || { bad "$kind 影子副本失败"; return; }
            python3 - "$shadow/$rel" "$old" "$new" <<'PY' 2>"$work/mut.log"
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(path, encoding='utf-8').read()
n = s.count(old)
if n != 1:
    print(f"旧串命中 {n} 次(必须恰好 1 次)—— 毒化是空操作或定位不唯一")
    sys.exit(3)
open(path, 'w', encoding='utf-8').write(s.replace(old, new, 1))
PY
            mrc=$?
            extra_inc=(-I "$shadow")
            adapter_override="$shadow/$rel"
            ;;
    esac
    # ⚠️ 必须用 `mrc` 而不是 `$?`:分支末尾的数组赋值会把 `$?` 重置为 0
    #    ⇒ 毒化失败会被静默吞掉(本脚本首版即踩此坑:anchor 不匹配 ⇒ 变异是空操作
    #    ⇒ 却被误判成「判据无牙」)。
    if [ "$mrc" -ne 0 ]; then
        bad "$kind 毒化失败(anchor 不唯一 / 命中 0 次) —— $(head -2 "$work/mut.log" | tr '\n' ' ')"
        return
    fi

    local blog="$work/build.log" rlog="$work/run.log"
    if ! "$CXX" -std=c++17 "${extra_inc[@]}" "${INCLUDES[@]}" -o "$work/probe" "$PROBE_SRC" \
            >"$blog" 2>&1; then
        # 编译期红:模式必须是 compile-lock,且文案要命中
        if [ "$mode" != "compile-lock" ]; then
            bad "$kind ⇒ 红在了**编译期**,但期望的是 runtime-predicate(归因错误)"
            sed 's/^/      /' "$blog" | grep -E "error" | head -3
            return
        fi
        if grep -qF -- "$expected" "$blog"; then
            ok "$kind ⇒ compile-lock 命中「${expected}」(impl 层未运行:编译期锁先红)"
        else
            bad "$kind ⇒ 编译期红了,但**未命中**期望文案「${expected}」"
            sed 's/^/      /' "$blog" | grep -E "error" | head -3
        fi
        return
    fi

    if [ "$mode" = "compile-lock" ]; then
        bad "$kind ⇒ 期望 compile-lock,但**编译通过**了 ⇒ 编译期锁失效"
        return
    fi

    "$work/probe" "$ROOT" "$json_dir" ${adapter_override:+"$adapter_override"} >"$rlog" 2>&1
    local prc=$?
    if [ $prc -eq 0 ]; then
        bad "$kind ⇒ 毒化后探针**仍然全绿** ⇒ 判据无牙"
        return
    fi

    # 「因正确原因红」的机器判定:逐个期望 id 必须出现在失败明细里
    local all_hit=1 id
    for id in $expected; do
        local layer="${id%%:*}" cid="${id#*:}"
        if grep -qF "FAIL [${layer}] ${cid}" "$rlog"; then
            : # 命中
        else
            all_hit=0
            bad "$kind ⇒ 未命中期望判据「${layer}:${cid}」"
        fi
    done
    if [ $all_hit -eq 1 ]; then
        # 归因要**完整可读**:把探针报出的全部 id 一并打印(两层分开)
        local detected
        detected="$(grep -oE 'FAIL \[[a-z]+\] [A-Za-z0-9-]+' "$rlog" | sed 's/FAIL //' | sort -u | tr '\n' ' ')"
        ok "$kind ⇒ 命中 [${expected}](探针实际报红:${detected})"
    fi
}

for fx in "${FIXTURES[@]}"; do
    IFS='|' read -r kind file old new mode expected <<<"$fx"
    echo ""
    echo "═══ 毒化:${kind}(${mode})═══"
    executed=$(( ${executed:-0} + 1 ))
    run_fixture "$kind" "$file" "$old" "$new" "$mode" "$expected"
done

# ── 空转防护:fixture 数必须与声明一致 ────────────────────────────────
echo ""
echo "══════════════════════════════════════"
if [ "${executed:-0}" -ne "${#FIXTURES[@]}" ]; then
    bad "已执行 fixture 数 ${executed:-0} ≠ 声明数 ${#FIXTURES[@]}(有 fixture 被**静默跳过**)"
fi
echo "反向验证:${pass} 项通过 / ${fail} 项失败(共 ${#FIXTURES[@]} 条毒化 + 基线 2 项检查)"
if [ "$fail" -ne 0 ]; then
    echo ""
    echo "失败明细:"
    for m in "${fail_msgs[@]}"; do echo "  - $m"; done
    exit 1
fi
