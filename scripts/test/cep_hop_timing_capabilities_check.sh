#!/usr/bin/env bash
# scripts/test/cep_hop_timing_capabilities_check.sh
# 协议语义层的「hop / 时序模型」承载能力声明面 —— 契约闸
# per openspec/changes/cep-hop-timing-capability-declaration/spec.md
#
# 职责（对应 proposal 的 AC）：
#   AC-1  通用层零协议名 / 零 include 协议头
#   AC-2  实例集双向零 orphan（kHopTimingCapabilities ↔ protocol_instances.json 的 id）
#   AC-3  协议语义层 hop / 时序**恒 None**（现状锁；≠ None ⇒ 必红）
#   AC-4  引擎层 hop / 时序 API **反向存在性**（去注释扫描；出现 ⇒ 必红）
#   AC-6  载体 / 器件层 hop / 时延**存在性**（既有事实被删 ⇒ 必红）
#   AC-7  basis 非空 + id 唯一
#   AC-8  与 G1 正交（声明表区段内不得出现 egress / in_flight）
#   AC-9 / AC-10  诚实边界常�� + 关键措辞存在
#   AC-11 反向 fixture（3 条 · 每条须命中**自己的**红因签名）
#
# 判据纪律：⛔ 「rc≠0」不构成红因；fixture 必须断言**红因签名命中**。
# 作者: CppTLM Team / 日期: 2026-11-24
#
# 用法:
#   bash scripts/test/cep_hop_timing_capabilities_check.sh              # 正常校验
#   bash scripts/test/cep_hop_timing_capabilities_check.sh --root <dir>  # 对拷贝根校验
#   bash scripts/test/cep_hop_timing_capabilities_check.sh --selftest    # 3 条反向 fixture
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="$REPO_ROOT"
SELFTEST=0
while [ $# -gt 0 ]; do
    case "$1" in
        --root) ROOT="$2"; shift 2 ;;
        --selftest) SELFTEST=1; shift ;;
        *) echo "未知参数: $1" >&2; exit 2 ;;
    esac
done
cd "$ROOT" || exit 2

HDR="include/tlm/cep/hop_timing_capabilities.hh"
INST="include/tlm/cep/protocol_instances.json"
INC_DIR="include/tlm/cep"

FAILED=0
pass() { echo "  ✓ $*"; }
fail() { echo "  ✗ $*"; FAILED=$((FAILED + 1)); }

# ── 区段抽取（声明面自带 marker ⇒ 闸与头文件的边界是**显式**的）───────────
extract_region() { # <file> <marker-name>
    sed -n "/== AC-7 scan begin: $2 ==/,/== AC-7 scan end: $2 ==/p" "$1"
}

# ══════════════════════════════════════════════════════════════════════════
#  核心判据
# ══════════════════════════════════════════════════════════════════════════
run_checks() {
    FAILED=0

    # ── 前置：声明面不存在 ⇒ SKIP（可区分于 FAIL）─────────────────────────
    if [ ! -f "$HDR" ]; then
        echo "[SKIP] $HDR 不存在（声明面尚未落地）"
        return 0
    fi

    DECL_IDS=$(extract_region "$HDR" "kHopTimingCapabilities init" \
        | grep -oE '\{"[a-z0-9_]+"' | sed -E 's/\{"//; s/"$//' | sort)
    if [ -z "$DECL_IDS" ]; then
        echo "[SKIP] 从 kHopTimingCapabilities 初始化区解析出 0 行（自检未就绪）"
        return 0
    fi
    pass "解析出 $(printf '%s\n' "$DECL_IDS" | wc -l | tr -d ' ') 行声明"

    # ── AC-2：实例集双向零 orphan（↔ protocol_instances.json 的 id）─────────
    JSON_IDS=$(python3 -c "
import json
d=json.load(open('$INST'))
print('\n'.join(i['id'] for i in d['instances']))
" 2>/dev/null | sort)
    if [ -z "$JSON_IDS" ]; then
        fail "hop-timing-orphan-missing: 无法从 $INST 解析实例 id（声明源不可读）"
    else
        MISSING=$(comm -23 <(printf '%s\n' "$JSON_IDS") <(printf '%s\n' "$DECL_IDS"))
        EXTRA=$(comm -13 <(printf '%s\n' "$JSON_IDS") <(printf '%s\n' "$DECL_IDS"))
        if [ -n "$MISSING" ]; then
            fail "hop-timing-orphan-missing: $(echo "$MISSING" | tr '\n' ' ')"
        else
            pass "AC-2 正向：JSON 全部实例均已声明"
        fi
        if [ -n "$EXTRA" ]; then
            fail "hop-timing-orphan-extra: $(echo "$EXTRA" | tr '\n' ' ')"
        else
            pass "AC-2 反向：无多余声明"
        fi
    fi

    # ── AC-3：协议语义层 hop / 时序**恒 None**（现状锁）────────────────────
    if extract_region "$HDR" "kHopTimingCapabilities init" \
        | grep -qE 'HopModel::CounterOnly|TimingModel::ParameterizedDelay'; then
        fail "hop-timing-protocol-model-present: 协议语义层出现 hop / 时序建模声明 ⇒ 须走决策 + 更新声明"
        extract_region "$HDR" "kHopTimingCapabilities init" \
            | grep -nE 'HopModel::CounterOnly|TimingModel::ParameterizedDelay' | head -3 | sed 's/^/      /'
    else
        pass "AC-3 现状锁：协议语义层 hop / 时序 恒 None"
    fi

    # ── AC-7：basis 非空 + id 唯一 ────────────────────────────────────────
    if extract_region "$HDR" "kHopTimingCapabilities init" | grep -qE '""'; then
        fail "hop-timing-empty-basis: 初始化区段出现空串"
    else
        pass "AC-7 basis 非空（粗检）"
    fi
    DECL_N=$(printf '%s\n' "$DECL_IDS" | wc -l | tr -d ' ')
    DECL_UNIQ=$(printf '%s\n' "$DECL_IDS" | sort -u | wc -l | tr -d ' ')
    if [ "$DECL_N" != "$DECL_UNIQ" ]; then
        fail "hop-timing-duplicate-id: 声明 id 重复（$DECL_N 行 / $DECL_UNIQ 唯一）"
    else
        pass "AC-7 id 唯一"
    fi

    # ── AC-8：与 G1 正交（不重复 egress / in_flight 真源）──────────────────
    SCAN_TEXT=$(
        extract_region "$HDR" "HopTimingCapability fields"
        extract_region "$HDR" "kHopTimingCapabilities init"
    )
    if echo "$SCAN_TEXT" | grep -qiE 'egress|in_flight'; then
        fail "hop-timing-duplicate-source: 区段内出现 egress / in_flight 字样（与 G1 的真源重叠）"
        echo "$SCAN_TEXT" | grep -inE 'egress|in_flight' | head -3 | sed 's/^/      /'
    else
        pass "AC-8 与 G1 正交：struct / init 区段无 egress / in_flight"
    fi

    # ── AC-1：通用层零协议名 / 零 include 协议头 ───────────────────────────
    PROTO_NAMES_RE='ChiRnfProtocolEngine|MesifProtocolEngine|MoesiGpuProtocolEngine|UalinkProtocolEngine|TileLinkProtocolEngine|CohIfNullProtocolEngine|NullProtocolEngine|ChiRnfAdapter|MesifAdapter|MoesiGpuAdapter|TileLinkCAdapter|CxlCacheAdapter|CxlDcohAdapter|NvlinkAdapter|UalinkAdapter'
    if grep -qE "$PROTO_NAMES_RE" "$HDR"; then
        fail "hop-timing-generic-protocol-name: $(grep -oE "$PROTO_NAMES_RE" "$HDR" | sort -u | tr '\n' ' ')"
    else
        pass "AC-1 零协议类名"
    fi
    if grep -qE '#include[[:space:]]+"tlm/cep/[^"]*(protocol_engine|_adapter)\.hh"' "$HDR"; then
        fail "hop-timing-generic-protocol-include: $(grep -oE '#include.*' "$HDR" | grep -E 'protocol_engine|_adapter')"
    else
        pass "AC-1 零 include 协议头"
    fi

    # ── AC-9 / AC-10：诚实边界的机器可读常量 + 关键措辞 ───────────────────
    MISSING_CONST=""
    for c in kTickIsSteppingHookNotTimingModel \
             kHopTimingAbsenceIsProtocolSemanticsOnly \
             kProtocolSemanticsHasNoHopOrTimingModel; do
        grep -qF "$c" "$HDR" || MISSING_CONST="$MISSING_CONST $c"
    done
    if [ -n "$MISSING_CONST" ]; then
        fail "hop-timing-boundary-constant-missing:$MISSING_CONST"
    else
        pass "AC-9 / AC-10 诚实边界常量齐备"
    fi
    if grep -qF "仅对协议语义层" "$HDR" && grep -qF "步进" "$HDR"; then
        pass "AC-9 / AC-10 关键措辞在位（tick() ≠ 时序模型 · 仅对协议语义层成立）"
    else
        fail "hop-timing-boundary-wording-missing: 缺「仅对协议语义层」或「步进」措辞"
    fi

    # ── AC-4：引擎层 hop / 时序 API **反向存在性**（去注释扫描）───────────
    # ⚠️ `tick` **不在**模式词内 —— 它是**全局步进钩子**，⛔ 不是消息级时序模型。
    if ! python3 - "$INC_DIR" <<'PY'
import re, sys, glob, os
KEY = ("hop", "latency", "travel_time", "arrival_cycle", "completion_cycle")
inc = sys.argv[1]

def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    t = re.sub(r"//[^\n]*", " ", t)
    return t

bad = []
for path in sorted(glob.glob(os.path.join(inc, "*_protocol_engine.hh"))):
    text = strip_comments(open(path, encoding="utf-8").read())
    toks = set(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text))
    for tok in sorted(toks):
        if any(k in tok.lower() for k in KEY):
            bad.append((os.path.basename(path), tok))
if bad:
    print("  ✗ hop-timing-engine-api-detected:")
    for f, tok in bad[:10]:
        print(f"      {f}: {tok}")
    sys.exit(1)
sys.exit(0)
PY
    then
        fail "hop-timing-engine-api-detected: 协议引擎头出现 hop / 时序 API ⇒ 须走决策 + 更新声明"
    else
        pass "AC-4 引擎层零 hop / 时序 API（去注释扫描）"
    fi

    # ── AC-6：载体 / 器件层既有 hop / 时延事实**仍在** ────────────────────
    MISSING_FACT=""
    for pair in "cohif_nic.hh:hops" "cohif_nic.hh:last_recv_hops" \
                "cohif_responder.hh:completion_latency_cycles" \
                "stub_cache_core.hh:snoop_latency_cycles"; do
        f="${pair%%:*}"; tok="${pair##*:}"
        if [ ! -f "$INC_DIR/$f" ] || ! grep -qF "$tok" "$INC_DIR/$f"; then
            MISSING_FACT="$MISSING_FACT $f:$tok"
        fi
    done
    if [ -n "$MISSING_FACT" ]; then
        fail "hop-timing-carrier-fact-missing:$MISSING_FACT（载体 / 器件层既有事实被删 ⇒ 声明面失真）"
    else
        pass "AC-6 载体 / 器件层 hop / 时延事实齐备"
    fi

    return $FAILED
}

# ══════════════════════════════════════════════════════════════════════════
#  --selftest：3 条决定性变异（跑在拷贝根 · 零工作树污染）
# ══════════════════════════════════════════════════════════════════════════
if [ "$SELFTEST" -eq 1 ]; then
    echo "══ cep_hop_timing_capabilities_check --selftest(3 变异,跑在拷贝根)══"
    BASE="$(mktemp -d)"
    trap 'rm -rf "$BASE"' EXIT

    copy_root() { # <dst>
        local d="$1"; rm -rf "$d"; mkdir -p "$d/include/tlm/cep"
        cp "$ROOT/include/tlm/cep/hop_timing_capabilities.hh" "$d/include/tlm/cep/"
        cp "$ROOT/include/tlm/cep/protocol_instances.json" "$d/include/tlm/cep/"
        cp "$ROOT"/include/tlm/cep/*_protocol_engine.hh "$d/include/tlm/cep/" 2>/dev/null || true
        cp "$ROOT/include/tlm/cep/cohif_nic.hh" "$d/include/tlm/cep/" 2>/dev/null || true
        cp "$ROOT/include/tlm/cep/cohif_responder.hh" "$d/include/tlm/cep/" 2>/dev/null || true
        cp "$ROOT/include/tlm/cep/stub_cache_core.hh" "$d/include/tlm/cep/" 2>/dev/null || true
    }

    # 基线：拷贝根必须绿
    copy_root "$BASE/base"
    if ( cd "$BASE/base" && bash "$REPO_ROOT/scripts/test/cep_hop_timing_capabilities_check.sh" \
            --root "$BASE/base" >/dev/null 2>&1 ); then
        pass "基线:拷贝根 rc=0"
    else
        fail "基线:拷贝根应为绿"; exit 1
    fi

    # 变异 1：声明表加一行 ⇒ AC-2 反向红（hop-timing-orphan-extra）
    copy_root "$BASE/m1"
    python3 - "$BASE/m1/include/tlm/cep/hop_timing_capabilities.hh" <<'PY'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
anchor = '        {"nvlink_4", HopModel::None, TimingModel::None,'
m = re.search(r'^\s*\{\"nvlink_4\".*?\},?\n', t, flags=re.M | re.S)
assert m, "变异 1 锚点未命中"
extra = '        {"mutant_extra", HopModel::None, TimingModel::None, "变异注入行"},\n'
t = t[:m.start()] + extra + t[m.start():]
open(p, "w", encoding="utf-8").write(t)
PY
    if [ $? -ne 0 ]; then
        fail "变异 1 前置失败:锚点未命中（⛔ 不得当作通过）"; exit 1
    fi
    OUT=$( cd "$BASE/m1" && bash "$REPO_ROOT/scripts/test/cep_hop_timing_capabilities_check.sh" \
            --root "$BASE/m1" 2>&1 )
    if printf '%s' "$OUT" | grep -qF "hop-timing-orphan-extra"; then
        pass "变异 1:声明表加一行 ⇒ 红（hop-timing-orphan-extra）"
    else
        fail "变异 1(加声明行)应以 hop-timing-orphan-extra 红,实为:$(printf '%s' "$OUT" | tail -3 | tr '\n' ' ')"
        exit 1
    fi

    # 变异 2：引擎头注入 hop API ⇒ AC-4 红（hop-timing-engine-api-detected）
    copy_root "$BASE/m2"
    printf '\n// 变异注入（反向 fixture）\ninline int last_hops() { return 0; }\n' \
        >> "$BASE/m2/include/tlm/cep/chi_rnf_protocol_engine.hh"
    OUT=$( cd "$BASE/m2" && bash "$REPO_ROOT/scripts/test/cep_hop_timing_capabilities_check.sh" \
            --root "$BASE/m2" 2>&1 )
    if printf '%s' "$OUT" | grep -qF "hop-timing-engine-api-detected"; then
        pass "变异 2:引擎头注入 hop API ⇒ 红（hop-timing-engine-api-detected）"
    else
        fail "变异 2(引擎注入 hop API)应以 hop-timing-engine-api-detected 红,实为:$(printf '%s' "$OUT" | tail -3 | tr '\n' ' ')"
        exit 1
    fi

    # 变异 3：声明 protocol_hop ≠ None ⇒ AC-3 红（hop-timing-protocol-model-present）
    copy_root "$BASE/m3"
    python3 - "$BASE/m3/include/tlm/cep/hop_timing_capabilities.hh" <<'PY'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
t2 = re.sub(r'(\{"mesif", )HopModel::None', r'\1HopModel::CounterOnly', t, count=1)
assert t2 != t, "变异 3 锚点未命中"
open(p, "w", encoding="utf-8").write(t2)
PY
    if [ $? -ne 0 ]; then
        fail "变异 3 前置失败:锚点未命中（⛔ 不得当作通过）"; exit 1
    fi
    OUT=$( cd "$BASE/m3" && bash "$REPO_ROOT/scripts/test/cep_hop_timing_capabilities_check.sh" \
            --root "$BASE/m3" 2>&1 )
    if printf '%s' "$OUT" | grep -qF "hop-timing-protocol-model-present"; then
        pass "变异 3:声明 protocol_hop ≠ None ⇒ 红（hop-timing-protocol-model-present）"
    else
        fail "变异 3(声明非 None)应以 hop-timing-protocol-model-present 红,实为:$(printf '%s' "$OUT" | tail -3 | tr '\n' ' ')"
        exit 1
    fi

    echo "────────────────────────────────────────"
    echo "✅ selftest:3/3 变异全部按**预期红因**变红"
    exit 0
fi

echo "══ CEP hop_timing_capabilities 契约闸 ══"
run_checks
echo "────────────────────────────────────────"
if [ "$FAILED" -eq 0 ]; then
    echo "[PASS] cep_hop_timing_capabilities_check: 全部判据通过"
    exit 0
fi
echo "[FAIL] cep_hop_timing_capabilities_check: $FAILED 项失败"
exit 1
