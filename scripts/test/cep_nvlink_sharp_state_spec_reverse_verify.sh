#!/usr/bin/env bash
# scripts/test/cep_nvlink_sharp_state_spec_reverse_verify.sh
#   —— 队列 #13 `cpptlm-cep-nvlink-sharp-state-spec` 的**反向验证**
#
# 行为(per delta spec NSS-01 ~ NSS-09 + tasks Phase 3.2 ~ 3.5):
#   ① **基线正控**:干净载体必须编译通过并通过探针(⇒ 探针本身有鉴别力);
#   ② 逐条**毒化**载体 ⇒ 判据**必须失败**;
#   ③ 失败原因 SHALL 命中**期望的判据 id**(不给「反正红了」放行)。
#
# ⚠️ **本组形态 = 「毒化 ⇒ 判据必红」**(鉴别力)⇒ 与队列 #11 修**误报**的那一组
#    (「毒化 ⇒ 判据仍绿」= 回归防护)**形态相反** ⇒ 两组都须登记,避免混用。
#
# ⚠️ **两类失败形态都支持**:
#   ① **编译期红** ⇒ 载体的 `static_assert` 触发 ⇒ 归因 = 编译日志里的 `<判据 id>:`
#   ② **运行期红** ⇒ 探针的 `PROBE:FAIL <id>` 输出
#
# 机制:每条 fixture 把**被毒化的载体**放进影子目录 ⇒ 编译探针时 `-I $SHADOW/include` **优先于**
#      `-I include` ⇒ 探针吃到的是**毒化副本**,而仓内文件**零改动**。
#
# 用法:bash scripts/test/cep_nvlink_sharp_state_spec_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_nvlink_sharp_state_spec_probe.cc"
CARRIER="include/tlm/cep/nvlink_sharp_state_spec.hh"
CXX="${CXX:-g++}"

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }
[ -f "$CARRIER" ]   || { echo "  ✗ 载体不存在: $CARRIER"; exit 1; }

# ── fixture 定义 ────────────────────────────────────────────────────────────
# 格式:id:::carrier-relative-path:::old:::new:::expected-id
#   · old / new = 该文件内**必须恰好命中 1 次**的替换对
#   · expected-id = 失败时**必须命中**的判据 id(编译期:static_assert 文案前缀;
#                  运行期:探针输出的 `PROBE:FAIL <id>`)
# ⚠️ 分隔符用 `:::`(多字符)—— 单字符 `|` 会被锚点里的含 `|` 文本截断
# ⚠️ `@NL@` = 换行(脚本替换,避免 bash 里硬换行把数组元素拆断)
FIXTURES=(
  # NSS-01a —— 给分类函数加 opcode 参数 ⇒ **签名锁**编译红
  "nss-01a-opcode-param:::${CARRIER}:::constexpr IngressDecision classify_at_fabric_ingress(bool region_hit,:::constexpr IngressDecision classify_at_fabric_ingress(std::uint32_t opcode, bool region_hit,:::CP-OPCODE-CLASSIFIER"
  # NSS-01b —— include 项目内头 ⇒ **运行期源扫描**红
  "nss-01b-project-include:::${CARRIER}:::#include <type_traits>:::#include <type_traits>@NL@#include \"tlm/cep/collective_plane.hh\":::CP-05"
  # NSS-02 —— R4 反向(未 commit ⇒ divert)⇒ 编译红
  "nss-02-epoch-reversed:::${CARRIER}:::return state == EpochState::Committed;:::return state == EpochState::Uncommitted;:::CP-EPOCH-SEMANTICS"
  # NSS-03 —— drain 失败仍返 Ok ⇒ 编译红
  "nss-03-drain-ignored:::${CARRIER}:::return drain_succeeded ? RegisterResult::Ok : RegisterResult::DrainFailed;:::return RegisterResult::Ok;:::CP-DRAIN-FAIL"
  # NSS-04 —— 注销后恒 Pass(抹掉泄露可观测性)⇒ 编译红
  "nss-04-destroy-always-pass:::${CARRIER}:::return region_still_registered ? IngressDecision::DiverT : IngressDecision::Pass;:::return IngressDecision::Pass;:::NSS-04-DESTROY-LEAK"
  # NSS-05 —— fail-stop 恒 false ⇒ 编译红
  "nss-05-fail-stop-off:::${CARRIER}:::return region_hit && epoch_committed && coherence_atomic_hit;:::return false;:::CP-DIVERT-STOP"
  # NSS-06 —— ordering C 反转比较 ⇒ 编译红
  "nss-06-ordering-c-reversed:::${CARRIER}:::return code_a < code_b;:::return code_a > code_b;:::NSS-06-ORDERING-MIS"
  # NSS-07 —— **填入具体 VC 编号**(delta spec 指定的形态)⇒ 运行期源扫描红
  #   ⚠️ 为什么不是「加第 2 个枚举值」:`kVcReservationStateCount` 是**登记锁**(手写常量与枚举
  #      手工同步)⇒ `enum { Unavailable = 0, Reserved = 1 }` **不会**触发计数断言(C++17 无法反射枚举基数)
  #      ⇒ 真正**结构性**的保证在「类型无 `vc_id` 字段」这一面(SFINAE + 源扫描)
  "nss-07-vc-id-field:::${CARRIER}:::enum class VcReservationState : std::uint8_t { Unavailable = 0 };:::enum class VcReservationState : std::uint8_t { Unavailable = 0 };@NL@struct VcReservationDetail { VcReservationState state; std::uint8_t vc_id; };:::CP-03"
  # NSS-08 —— 能力域加跨 fabric 取值 ⇒ 计数锁编译红
  "nss-08-scope-cross-fabric:::${CARRIER}:::enum class CollectiveScope : std::uint8_t { IntraNode = 0 };:::enum class CollectiveScope : std::uint8_t { IntraNode = 0, CrossFabric = 1 };:::NSS-08-SCOPE-OVERREACH"
  # NSS-09 —— 某格 ref 置空 ⇒ all_basis_valid 编译红
  "nss-09-basis-ref-null:::${CARRIER}:::{EvidenceKind::SelfDerived, false, SourceId::AdrContract, \"C-8\"}:::{EvidenceKind::SelfDerived, false, SourceId::AdrContract, nullptr}:::NSS-09-BASIS-MISSING"
)

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/nvs_rv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/external/json")

echo ""
echo "═══ 基线:干净载体必须编译并通过探针(正控)═══"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/base_build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/base_build.log" | head -20; exit 1
fi
if ! "$TMP/probe" "$CARRIER" >"$TMP/base.out" 2>&1; then
    echo "  ✗ **干净载体未通过探针** ⇒ 探针本身有问题:"; sed 's/^/      /' "$TMP/base.out" | head -20; exit 1
fi
ok "基线:干净载体编译通过 + 探针全绿($(grep -c 'PROBE:OK' "$TMP/base.out") 处)"

echo ""
echo "═══ 逐条毒化 ⇒ 判据必须失败(且失败原因命中期望判据)═══"
for fx in "${FIXTURES[@]}"; do
    id="${fx%%:::*}"; _r="${fx#*:::}"
    file="${_r%%:::*}"; _r="${_r#*:::}"
    old="${_r%%:::*}"; _r="${_r#*:::}"
    new="${_r%%:::*}"; expected="${_r#*:::}"

    SHADOW="$TMP/shadow_$id"
    rm -rf "$SHADOW"
    # ⚠️ **整目录**复制 `include/tlm/cep/` ⇒ 影子里的其它头齐备(探针只吃被毒化的那一个)
    mkdir -p "$SHADOW/include/tlm"
    cp -r "$ROOT/include/tlm/cep" "$SHADOW/include/tlm/cep"

    # 毒化影子里的载体(仓内文件**零改动**)
    python3 - "$SHADOW/$file" "$old" "$new" <<'PY'
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
old = old.replace('@NL@', chr(10))
new = new.replace('@NL@', chr(10))
text = open(path, encoding='utf-8').read()
n = text.count(old)
if n != 1:
    print(f"ANCHOR-HIT:{n}")
    sys.exit(3)
open(path, 'w', encoding='utf-8').write(text.replace(old, new, 1))
PY
    if [ $? -eq 3 ]; then
        bad "$id —— 毒化锚点**未恰好命中 1 次** ⇒ fixture 本身失效"
        continue
    fi

    SHADOW_CARRIER="$SHADOW/$file"
    # 编译探针(影子优先)
    if ! "$CXX" -std=c++17 -I "$SHADOW/include" "${INCLUDES[@]}" -o "$TMP/p" "$PROBE_SRC" \
            2>"$TMP/b.log"; then
        # ① **编译期红**:归因 = 编译日志里的 static_assert 文案
        if grep -q "$expected" "$TMP/b.log"; then
            ok "$id ⇒ **编译期红**且归因命中($expected)"
        else
            bad "$id —— 编译失败但**归因未命中**期望判据($expected)"
            grep -E "error:|static assertion" "$TMP/b.log" | sed 's/^/      /' | head -4
        fi
        continue
    fi

    # ② **运行期红**
    if "$TMP/p" "$SHADOW_CARRIER" >"$TMP/o.txt" 2>&1; then
        bad "$id —— 毒化后探针**仍绿** ⇒ 判据无鉴别力"
        continue
    fi
    if ! grep -q "PROBE:FAIL $expected" "$TMP/o.txt"; then
        bad "$id —— 探针红了但**归因未命中**期望判据($expected)"
        sed 's/^/      /' "$TMP/o.txt" | head -4
        continue
    fi
    ok "$id ⇒ 探针红且归因命中($expected)"
done

echo ""
if [ "$fail" -gt 0 ]; then
    echo "✗ 反向验证失败:$fail 条(共 $((pass + fail)) 条)"
    for m in "${fail_msgs[@]}"; do echo "    · $m"; done
    exit 1
fi
echo "✓ 反向验证全部通过:$pass 条"
