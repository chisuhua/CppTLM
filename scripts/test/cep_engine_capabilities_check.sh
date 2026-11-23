#!/usr/bin/env bash
# scripts/test/cep_engine_capabilities_check.sh
# 协议引擎承载能力声明面 —— 契约闸
# per openspec/changes/cep-protocol-engine-capability-declaration/spec.md
#
# 职责（对应 proposal 的 AC）：
#   AC-1  通用层零协议名 / 零 include 协议头
#   AC-2  实例集双向零 orphan（kEngineCapabilities ↔ protocol_instances.json 的 id）
#   AC-5  basis 非空（粗检：区段内无空串）
#   AC-7  SKIP（解析 0 行）与 FAIL 可区分
#   AC-8  引擎头文件集反向存在性（恰 5 个已登记引擎头）
#   AC-9  单真源：struct / init 区段内不得出现 transient / in_flight 字段名
#   AC-3 / AC-4  probe TU 反向验证（编译失败 **且** 命中实例 id 字面量签名）
#
# 判据纪律：⛔ 「rc≠0」不构成红因；probe 必须断言**签名命中**。
# 作者: CppTLM Team / 日期: 2026-11-23
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HDR="$ROOT/include/tlm/cep/engine_capabilities.hh"
INST="$ROOT/include/tlm/cep/protocol_instances.json"
INC_DIR="$ROOT/include/tlm/cep"

FAILED=0
pass() { echo "  ✓ $*"; }
fail() { echo "  ✗ $*"; FAILED=$((FAILED + 1)); }

echo "=== CEP engine_capabilities 契约闸 ==="

# ── 前置：声明源不存在 ⇒ SKIP（可区分于 FAIL）──────────────────────────
if [ ! -f "$HDR" ]; then
    echo "[SKIP] $HDR 不存在（声明面尚未落地）"
    exit 0
fi

extract_region() { # <file> <marker-name>
    sed -n "/== AC-9 scan begin: $2 ==/,/== AC-9 scan end: $2 ==/p" "$1"
}

# ── 抽取声明表 id（grep **数据行**，⛔ 不 grep API / 类名）──────────────
DECL_IDS=$(extract_region "$HDR" "kEngineCapabilities init" \
    | grep -oE '\{"[a-z0-9_]+"' | sed -E 's/\{"//; s/"$//' | sort)

if [ -z "$DECL_IDS" ]; then
    echo "[SKIP] 从 kEngineCapabilities 初始化区解析出 0 行（自检未就绪）"
    exit 0
fi
pass "解析出 $(printf '%s\n' "$DECL_IDS" | wc -l | tr -d ' ') 行声明"

# ── AC-2：双向零 orphan（↔ protocol_instances.json 的 id）─────────────
JSON_IDS=$(python3 -c "
import json,sys
d=json.load(open('$INST'))
print('\n'.join(i['id'] for i in d['instances']))
" 2>/dev/null | sort)

if [ -z "$JSON_IDS" ]; then
    fail "无法从 $INST 解析实例 id（声明源不可读）"
else
    MISSING=$(comm -23 <(printf '%s\n' "$JSON_IDS") <(printf '%s\n' "$DECL_IDS"))
    EXTRA=$(comm -13 <(printf '%s\n' "$JSON_IDS") <(printf '%s\n' "$DECL_IDS"))
    if [ -n "$MISSING" ]; then
        fail "engine-cap-orphan-missing: $(echo "$MISSING" | tr '\n' ' ')"
    else
        pass "AC-2 正向：JSON 全部实例均已声明"
    fi
    if [ -n "$EXTRA" ]; then
        fail "engine-cap-orphan-extra: $(echo "$EXTRA" | tr '\n' ' ')"
    else
        pass "AC-2 反向：无多余声明"
    fi
fi

# ── AC-5（粗检）：basis 非空 ⇒ init 区段内不得出现空串 ─────────────────
if extract_region "$HDR" "kEngineCapabilities init" | grep -qE '""'; then
    fail "engine-cap-empty-basis: 初始化区段出现空串"
else
    pass "AC-5 粗检：初始化区段无空串"
fi

# ── AC-8：引擎头文件集反向存在性（恰 5 个已登记引擎头）────────────────
# ⚠️ 6 个 = 5 个**实例引擎** + 1 个 **CohIf Null 外壳**（`cohif_protocol_engine.hh`：
#    NullProtocolEngine<2> 的可注册形态，⛔ 不承载任何协议语义 ⇒ 无实例、无 EngineTypeTraits 特化）。
#    反向存在性的目的仍是「**出现新引擎即红**」⇒ 新增第 7 个文件必须同步更新本清单与声明面。
EXPECTED_ENGINE_HEADERS="chi_rnf_protocol_engine.hh cohif_protocol_engine.hh mesif_protocol_engine.hh moesi_gpu_protocol_engine.hh tilelink_protocol_engine.hh ualink_protocol_engine.hh"
ACTUAL_ENGINE_HEADERS=$(ls "$INC_DIR"/*_protocol_engine.hh 2>/dev/null | xargs -r -n1 basename | sort | tr '\n' ' ' | sed 's/ *$//')
EXPECTED_SORTED=$(printf '%s\n' $EXPECTED_ENGINE_HEADERS | sort | tr '\n' ' ' | sed 's/ *$//')
if [ "$ACTUAL_ENGINE_HEADERS" = "$EXPECTED_SORTED" ]; then
    pass "AC-8 引擎头文件集恰为已登记的 6 个（5 实例引擎 + 1 Null 外壳）"
else
    fail "engine-cap-new-engine-detected / engine-cap-engine-header-missing"
    echo "      实际: $ACTUAL_ENGINE_HEADERS"
    echo "      期望: $EXPECTED_SORTED"
fi

# ── AC-1：通用层零协议名 / 零 include 协议头 ────────────────────────────
PROTO_NAMES_RE='ChiRnfProtocolEngine|MesifProtocolEngine|MoesiGpuProtocolEngine|UalinkProtocolEngine|TileLinkProtocolEngine|ChiRnfAdapter|MesifAdapter|MoesiGpuAdapter|TileLinkCAdapter|CxlCacheAdapter|CxlDcohAdapter|NvlinkAdapter|UalinkAdapter'
if grep -qE "$PROTO_NAMES_RE" "$HDR"; then
    fail "generic-layer-protocol-name: $(grep -oE "$PROTO_NAMES_RE" "$HDR" | sort -u | tr '\n' ' ')"
else
    pass "AC-1 零协议类名"
fi
if grep -qE '#include[[:space:]]+"tlm/cep/[^"]*(protocol_engine|_adapter)\.hh"' "$HDR"; then
    fail "generic-layer-protocol-include: $(grep -oE '#include.*' "$HDR" | grep -E 'protocol_engine|_adapter')"
else
    pass "AC-1 零 include 协议头"
fi

# ── AC-9：struct / init 区段内不得出现 transient / in_flight 字段名 ────
SCAN_TEXT=$(
    extract_region "$HDR" "EngineCapability fields"
    extract_region "$HDR" "kEngineCapabilities init"
)
if echo "$SCAN_TEXT" | grep -qiE 'transient|in_flight'; then
    fail "engine-cap-duplicate-source: 区段内出现 transient / in_flight 字样"
    echo "$SCAN_TEXT" | grep -inE 'transient|in_flight' | head -3
else
    pass "AC-9 单真源：struct / init 区段无 transient / in_flight"
fi

# ── AC-3 / AC-4：probe TU 反向验证（编译失败 **且** 命中签名）───────────
CXX_BIN=${CXX:-g++}
STD_FLAG="-std=c++23"
if ! echo 'int main(){}' | "$CXX_BIN" "$STD_FLAG" -x c++ -fsyntax-only - >/dev/null 2>&1; then
    STD_FLAG="-std=c++20"
fi

TMP=$(mktemp -d "${TMPDIR:-/tmp}/cep_engcap.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/inc/tlm/cep"

probe_compile() { # <src> => 输出编译日志；返回编译 rc
    "$CXX_BIN" "$STD_FLAG" -fsyntax-only \
        -I"$TMP/inc" -I"$ROOT/include" -I"$ROOT/include/core" -I"$ROOT/external/json" \
        "$1" 2>&1
}

run_probe() { # <label> <src> <expect-signature> <expect-mode: pass|fail>
    local label=$1 src=$2 sig=$3 mode=$4 out rc
    out=$(probe_compile "$src")
    rc=$?
    if [ "$mode" = "pass" ]; then
        if [ $rc -eq 0 ]; then pass "probe[$label] 基线编译通过"; else
            fail "probe[$label] 基线本应编译通过却失败"; echo "$out" | head -6
        fi
        return
    fi
    # mode = fail
    if [ $rc -eq 0 ]; then
        fail "probe[$label] 本应编译失败但通过了（判据失去牙）"
    elif echo "$out" | grep -q "$sig"; then
        pass "probe[$label] 编译失败且命中签名「$sig」"
    else
        fail "probe[$label] 编译失败但未命中签名「$sig」"
        echo "$out" | head -6
    fi
}

# --- AC-3：chi_rnf 特化 Modeled → NotModeled ⇒ 必红且命中 chi_rnf ---
cat > "$TMP/ac3.cc" <<'EOF'
#include "tlm/cep/chi_rnf_protocol_engine.hh"
#include "tlm/cep/engine_capabilities.hh"
static_assert(tlm::cep::in_flight_declaration_matches<tlm::cep::ChiRnfProtocolEngine>(),
              "chi_rnf: EngineTypeTraits::in_flight 与 HasInFlightApi 不一致");
int main() { return 0; }
EOF
run_probe "AC-3-baseline" "$TMP/ac3.cc" "chi_rnf" pass
cp "$INC_DIR/chi_rnf_protocol_engine.hh" "$TMP/inc/tlm/cep/"
sed -i 's/in_flight = InFlightModeling::Modeled;/in_flight = InFlightModeling::NotModeled;/' \
    "$TMP/inc/tlm/cep/chi_rnf_protocol_engine.hh"
if grep -q "NotModeled" "$TMP/inc/tlm/cep/chi_rnf_protocol_engine.hh"; then
    run_probe "AC-3-mutated" "$TMP/ac3.cc" "chi_rnf" fail
else
    fail "AC-3 变异锚点未命中（判据失去牙）"
fi
rm -f "$TMP/inc/tlm/cep/chi_rnf_protocol_engine.hh"

# --- AC-4：mesif kTransientStateCount 0 → 2 ⇒ 必红且命中 mesif ---
cat > "$TMP/ac4.cc" <<'EOF'
#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/engine_capabilities.hh"
static_assert(tlm::cep::MesifAdapter::kTransientStateCount <=
                  tlm::cep::kMaxTransientStatesPerInstance,
              "mesif: kTransientStateCount 超出策略上限");
int main() { return 0; }
EOF
run_probe "AC-4-baseline" "$TMP/ac4.cc" "mesif" pass
cp "$INC_DIR/mesif_adapter.hh" "$TMP/inc/tlm/cep/"
sed -i 's/static constexpr std::size_t kTransientStateCount = 0;/static constexpr std::size_t kTransientStateCount = 2;/' \
    "$TMP/inc/tlm/cep/mesif_adapter.hh"
if grep -q "kTransientStateCount = 2;" "$TMP/inc/tlm/cep/mesif_adapter.hh"; then
    run_probe "AC-4-mutated" "$TMP/ac4.cc" "mesif" fail
else
    fail "AC-4 变异锚点未命中（判据失去牙）"
fi
rm -f "$TMP/inc/tlm/cep/mesif_adapter.hh"

# ── 汇总 ───────────────────────────────────────────────────────────────
echo "────────────────────────────────────────"
if [ "$FAILED" -eq 0 ]; then
    echo "[PASS] cep_engine_capabilities_check: 全部判据通过"
    exit 0
fi
echo "[FAIL] cep_engine_capabilities_check: $FAILED 项失败"
exit 1
