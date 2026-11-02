#!/usr/bin/env bash
# scripts/test/cep_cxl_dcoh_state_spec_reverse_verify.sh
#   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric-state-spec` 的**反向验证**
#
# 行为(per delta spec CDS-01 ~ CDS-07 + tasks Phase 3.2):
#   ① **基线正控**:干净载体必须编译并通过探针(⇒ 探针本身有鉴别力);
#   ② 逐条**毒化**载体 ⇒ 判据**必须失败**;
#   ③ 失败原因 SHALL 命中**期望的判据 id**(不给「反正红了」放行)。
#
# ⚠️ **本组形态 = 「毒化 ⇒ 判据必红」**(鉴别力)⇒ 与队列 #11 修**误报**的那一组
#    (「毒化 ⇒ 判据仍绿」= 回归防护)**形态相反** ⇒ 两组都须登记,避免混用。
#
# ⚠️ **两类失败形态都支持**:
#   ① **编译期红** ⇒ 载体的 `static_assert` 触发 ⇒ 归因 = 编译日志里的 `CDS-0x:`
#   ② **运行期红** ⇒ 探针的 `PROBE:FAIL <id>` 输出
#
# 机制:每条 fixture 把**被毒化的载体**放进影子目录 ⇒ 编译探针时 `-I $SHADOW/include` **优先于**
#      `-I include` ⇒ 探针吃到的是**毒化副本**,而仓内文件**零改动**。
#
# 用法:bash scripts/test/cep_cxl_dcoh_state_spec_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_cxl_dcoh_state_spec_probe.cc"
CARRIER="include/tlm/cep/cxl_dcoh_state_spec.hh"
CXX="${CXX:-g++}"

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1 —— **反向验证失败**"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }

# ── fixture 定义 ───────────────────────────────────────────────────────────
# 格式:id:::carrier-relative-path:::old:::new:::expected-id
#   · old / new = 该文件内**必须恰好命中 1 次**的替换对
#   · expected-id = 失败时**必须命中**的判据 id(编译期:static_assert 文案前缀;
#                  运行期:探针输出的 `PROBE:FAIL <id>`)
# ⚠️ 分隔符用 `:::`(多字符)—— 单字符 `|` 会被锚点里的 `|=` 截断
FIXTURES=(
  "merge-state-space:::include/tlm/cep/cxl_dcoh_state_spec.hh:::            enum class DcohViewState : std::uint8_t { I = 0, S = 1, A = 2 };:::            using DcohViewState = DevCacheState;:::CDS-01"
  "snoop-set-emptied:::include/tlm/cep/cxl_dcoh_state_spec.hh:::            inline constexpr std::array<CxlMemChannel, 1> kDeviceInitiatedSnoopSet{@NL@                {CxlMemChannel::S2M_BISnp}};:::            inline constexpr std::array<CxlMemChannel, 0> kDeviceInitiatedSnoopSet{};:::CDS-03"
  "predicate-ignored:::include/tlm/cep/cxl_dcoh_state_spec.hh:::                return c.device_originated_bi_snoop && c.device_side_sf_or_directory &&@NL@                       c.spans_multiple_independent_hosts && c.p2p_without_host_cpu &&@NL@                       c.snooped_agent_is_caching_host;:::                return c.device_originated_bi_snoop;:::CDS-05"
  "pending-dropped:::include/tlm/cep/cxl_dcoh_state_spec.hh:::            inline constexpr std::array<Basis, kHomeDirStateCount> kBasisHomeDirState{@NL@                {{EvidenceKind::PublicDirect, true, SourceId::S4, \"§1 / S4\"},:::            inline constexpr std::array<Basis, kHomeDirStateCount> kBasisHomeDirState{@NL@                {{EvidenceKind::PublicDirect, false, SourceId::S4, \"§1 / S4\"},:::CDS-06"
)

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/tl_cxl_dcoh.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

INCLUDES=(-I "$ROOT/include" -I "$ROOT/include/bundles" -I "$ROOT/include/core" -I "$ROOT/external/json")

echo ""
echo "═══ 基线:干净载体必须编译并通过探针(正控)═══"
if ! "$CXX" -std=c++17 "${INCLUDES[@]}" -o "$TMP/probe" "$PROBE_SRC" 2>"$TMP/base_build.log"; then
    echo "  ✗ 探针构建失败:"; sed 's/^/      /' "$TMP/base_build.log" | head -20; exit 1
fi
if ! "$TMP/probe" >"$TMP/base.out" 2>&1; then
    echo "  ✗ **干净载体未通过探针** ⇒ 探针本身有问题:"; sed 's/^/      /' "$TMP/base.out" | head -20; exit 1
fi
ok "基线:干净载体编译通过 + 探针全绿"

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

    # 编译探针(影子优先)
    if ! "$CXX" -std=c++17 -I "$SHADOW/include" "${INCLUDES[@]}" -o "$TMP/p" "$PROBE_SRC" 2>"$TMP/b.log"; then
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
    if "$TMP/p" >"$TMP/o.txt" 2>&1; then
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
