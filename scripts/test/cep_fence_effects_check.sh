#!/usr/bin/env bash
# scripts/test/cep_fence_effects_check.sh
# 「fence 一致性效果」派生声明面 —— 契约闸
# per openspec/changes/cep-fence-consistency-effect-derivation/spec.md
#
# 职责（对应 improvement 的 AC）：
#   AC-1  推导函数存在性（**编译期** · 由 test_cep_fence_effects.cc 的 static_assert 承担）
#   AC-2  派生正确性 —— **独立 oracle**（测试内硬编码期望表）· ⚠️ 闸**不重算**派生值（⛔ 防自证）；
#         本闸承担 oracle 的**存在性护栏**（oracle 被删 ⇒ 必红 ⇒ 防判据退化为空）
#   AC-3  输入副本漂移：头文件副本 ⇄ protocol_instances.json 逐实例逐字段
#   AC-4  域外必红：跨族 / 未知标签 ⇒ 必红
#   AC-5  JSON 缺键必红（带签名 · ⛔ 不静默 SKIP）
#   AC-6  实例集双向零 orphan
#   AC-7  basis 非空 + id 唯一 + 诚实边界常量
#   AC-8  通用层零协议类名 / 零 include 协议头 + 与 G1 / G10 正交
#   AC-11 反向 fixture（2 条 · 各按自己的红因签名 · 跑在拷贝根 ⇒ 零工作树污染）
#
# 判据纪律：⛔ 「rc≠0」不构成红因；fixture 必须断言**红因签名命中**。
# 作者: CppTLM Team / 日期: 2026-11-24
#
# 用法:
#   bash scripts/test/cep_fence_effects_check.sh              # 正常校验
#   bash scripts/test/cep_fence_effects_check.sh --root <dir>  # 对拷贝根校验
#   bash scripts/test/cep_fence_effects_check.sh --selftest    # 2 条反向 fixture
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

HDR="include/tlm/cep/fence_effects.hh"
INST="include/tlm/cep/protocol_instances.json"
TEST_SRC="test/test_cep_fence_effects.cc"
SPEC="openspec/specs/cep-fence-consistency-effect-derivation/spec.md"

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

    # ── 解析声明表行（id / read / write / barrier）─────────────────────────
    ROWS=$(python3 - "$HDR" <<'PYEOF'
import re, sys
t = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r"== AC-7 scan begin: kFenceEffectCapabilities init ==(.*?)== AC-7 scan end: kFenceEffectCapabilities init ==", t, re.S)
if not m:
    sys.exit(0)
for row in re.finditer(r'\{"([a-z0-9_]+)",\s*"([^"]*)",\s*"([^"]*)",\s*(true|false)', m.group(1)):
    print("|".join([row.group(1), row.group(2), row.group(3), row.group(4)]))
PYEOF
)
    if [ -z "$ROWS" ]; then
        fail "fence-table-parse-empty: 从 $HDR 解析出 0 行声明（自检未就绪）"
        return $FAILED
    fi
    ROW_N=$(printf '%s\n' "$ROWS" | wc -l | tr -d ' ')
    pass "解析出 $ROW_N 行声明"

    # ── AC-6：实例集双向零 orphan（⇄ JSON）───────────────────────────────
    JSON_ROWS=$(python3 - "$INST" <<'PYEOF'
import json, sys
try:
    d = json.load(open(sys.argv[1], encoding="utf-8"))
except Exception as exc:
    print("__ERROR__" + str(exc))
    sys.exit(0)
for i in d.get("instances", []):
    cl = i.get("consistency_labels")
    fs = i.get("flow_support")
    if not isinstance(cl, dict) or not isinstance(fs, dict) or "barrier" not in fs:
        print("__MISSING_KEY__" + str(i.get("id")))
        continue
    print("|".join([str(i.get("id")), str(cl.get("read")), str(cl.get("write")),
                    "true" if fs.get("barrier") else "false"]))
PYEOF
)
    if printf '%s' "$JSON_ROWS" | grep -q "__ERROR__"; then
        fail "fence-json-unreadable: 无法读取 $INST（声明源不可读）"
        return $FAILED
    fi
    if printf '%s' "$JSON_ROWS" | grep -q "__MISSING_KEY__"; then
        fail "fence-json-key-missing: $(printf '%s' "$JSON_ROWS" | grep '__MISSING_KEY__' | sed 's/^__MISSING_KEY__//' | tr '\n' ' ')（缺 consistency_labels / flow_support.barrier ⇒ 必红）"
    else
        pass "AC-5 JSON 键齐备（无缺键实例）"
    fi

    DECL_IDS=$(printf '%s\n' "$ROWS" | cut -d'|' -f1 | sort)
    JSON_IDS=$(printf '%s\n' "$JSON_ROWS" | grep -v '^__' | cut -d'|' -f1 | sort)
    MISSING=$(comm -23 <(printf '%s\n' "$JSON_IDS") <(printf '%s\n' "$DECL_IDS"))
    EXTRA=$(comm -13 <(printf '%s\n' "$JSON_IDS") <(printf '%s\n' "$DECL_IDS"))
    if [ -n "$MISSING" ]; then
        fail "fence-orphan-missing: $(echo "$MISSING" | tr '\n' ' ')"
    else
        pass "AC-6 正向：JSON 全部实例均已声明"
    fi
    if [ -n "$EXTRA" ]; then
        fail "fence-orphan-extra: $(echo "$EXTRA" | tr '\n' ' ')"
    else
        pass "AC-6 反向：无多余声明"
    fi

    # ── AC-3：输入副本漂移（头文件副本 ⇄ JSON 逐字段）────────────────────
    DRIFT=$(python3 - "$ROWS" "$INST" <<'PYEOF'
import json, sys
rows = {}
for line in sys.argv[1].splitlines():
    if not line.strip():
        continue
    parts = line.split("|")
    if len(parts) != 4:
        continue
    rows[parts[0]] = (parts[1], parts[2], parts[3])
d = json.load(open(sys.argv[2], encoding="utf-8"))
bad = []
for i in d.get("instances", []):
    rid = str(i.get("id"))
    if rid not in rows:
        continue
    cl = i.get("consistency_labels") or {}
    fs = i.get("flow_support") or {}
    want = (str(cl.get("read")), str(cl.get("write")), "true" if fs.get("barrier") else "false")
    if rows[rid] != want:
        bad.append(rid + " 副本=" + str(rows[rid]) + " JSON=" + str(want))
for b in bad:
    print(b)
PYEOF
)
    DRIFT_RC=$?
    if [ "$DRIFT_RC" -ne 0 ]; then
        # ⚠️ 检查器自身崩溃 ⇒ **必红**（⛔ 不得因「无输出」而误判为绿 —— 假绿纪律）
        fail "fence-copy-check-crash: 输入副本比对检查器异常退出（rc=$DRIFT_RC）⇒ 判据不可信"
    elif [ -n "$DRIFT" ]; then
        fail "fence-input-copy-drift: $(echo "$DRIFT" | tr '\n' ' ')"
        echo "$DRIFT" | head -3 | sed 's/^/      /'
    else
        pass "AC-3 输入副本与 JSON 逐字段一致"
    fi

    # ── AC-4：域外必红（输入层面：跨族 / 未知标签）────────────────────────
    DOMAIN=$(printf '%s\n' "$ROWS" | python3 -c "
import sys
def fam(x):
    if x == 'SC': return 'SC'
    if x == 'TSO': return 'TSO'
    if x.startswith('RC-'): return 'RC'
    if x == 'PLO': return 'PLO'
    return '?'
for line in sys.stdin.read().splitlines():
    if not line.strip():
        continue
    rid, rl, wl, _ = line.split('|')
    rf, wf = fam(rl), fam(wl)
    if rf == '?' or wf == '?':
        print(rid + ' 未知标签 ' + rl + '/' + wl)
    elif rf != wf:
        print(rid + ' 跨族组合 ' + rl + '/' + wl)
")
    DOMAIN_RC=$?
    if [ "$DOMAIN_RC" -ne 0 ]; then
        fail "fence-domain-check-crash: 域外检查器异常退出（rc=$DOMAIN_RC）⇒ 判据不可信"
    elif [ -n "$DOMAIN" ]; then
        fail "fence-derivation-out-of-domain: $(echo "$DOMAIN" | tr '\n' ' ')"
        echo "$DOMAIN" | head -3 | sed 's/^/      /'
    else
        pass "AC-4 全部行在推导域内"
    fi

    # ── AC-2：独立 oracle 的**存在性护栏**（⛔ 闸不重算派生值 ⇒ 不自证）────
    if [ ! -f "$TEST_SRC" ]; then
        fail "fence-oracle-missing: 判据测试 $TEST_SRC 不存在（独立 oracle 缺失 ⇒ 派生正确性无判据）"
    else
        ORACLE_N=$(grep -cE '\{"[a-z0-9_]+", *FenceEffect::' "$TEST_SRC" || true)
        if [ "$ORACLE_N" -ge "$ROW_N" ]; then
            pass "AC-2 独立 oracle 在位（测试内硬编码期望表 ${ORACLE_N} 行）"
        else
            fail "fence-oracle-missing: 测试内硬编码期望表只有 ${ORACLE_N} 行（应 ${ROW_N} 行）⇒ 派生正确性判据退化"
        fi
    fi

    # ── AC-7：basis 非空 + id 唯一 ──────────────────────────────────────
    if extract_region "$HDR" "kFenceEffectCapabilities init" | grep -qE '""'; then
        fail "fence-empty-basis: 初始化区段出现空串"
    else
        pass "AC-7 basis 非空（粗检）"
    fi
    DECL_N=$(printf '%s\n' "$DECL_IDS" | wc -l | tr -d ' ')
    DECL_U=$(printf '%s\n' "$DECL_IDS" | sort -u | wc -l | tr -d ' ')
    if [ "$DECL_N" != "$DECL_U" ]; then
        fail "fence-duplicate-id: 声明 id 重复（$DECL_N 行 / $DECL_U 唯一）"
    else
        pass "AC-7 id 唯一"
    fi

    # ── AC-7：诚实边界常量齐备 + 关键措辞 ────────────────────────────────
    MISSING_CONST=""
    for c in kFenceEffectIsDeclarationOnly \
             kFenceEffectIsNotCrossDomainGuarantee \
             kBarrierAbsenceMeansNoFenceEffect \
             kScModelMeansNoExtraFenceEffect \
             kRcEffectIsPaperModelBaseline \
             kFenceEffectGranularityIsPerInstance; do
        grep -qF "$c" "$HDR" || MISSING_CONST="$MISSING_CONST $c"
    done
    if [ -n "$MISSING_CONST" ]; then
        fail "fence-boundary-constant-missing:$MISSING_CONST"
    else
        pass "AC-7 诚实边界常量齐备（6 个）"
    fi
    if grep -qF "仅对**协议语义层**" "$HDR" || grep -qF "声明效果" "$HDR"; then
        pass "AC-7 关键措辞在位（声明效果 ⛔ 非引擎行为语义）"
    else
        fail "fence-boundary-wording-missing: 缺「声明效果」措辞"
    fi

    # ── AC-8：通用层零协议类名 / 零 include 协议头 + 与 G1 / G10 正交 ────
    PROTO_NAMES_RE='ChiRnfProtocolEngine|MesifProtocolEngine|MoesiGpuProtocolEngine|UalinkProtocolEngine|TileLinkProtocolEngine|CohIfNullProtocolEngine|NullProtocolEngine|ChiRnfAdapter|MesifAdapter|MoesiGpuAdapter|TileLinkCAdapter|CxlCacheAdapter|CxlDcohAdapter|NvlinkAdapter|UalinkAdapter'
    if grep -qE "$PROTO_NAMES_RE" "$HDR"; then
        fail "fence-generic-protocol-name: $(grep -oE "$PROTO_NAMES_RE" "$HDR" | sort -u | tr '\n' ' ')"
    else
        pass "AC-8 零协议类名"
    fi
    if grep -qE '#include[[:space:]]+"tlm/cep/[^"]*(protocol_engine|_adapter)\.hh"' "$HDR"; then
        fail "fence-generic-protocol-include: $(grep -oE '#include.*' "$HDR" | grep -E 'protocol_engine|_adapter')"
    else
        pass "AC-8 零 include 协议头"
    fi
    SCAN_TEXT=$(extract_region "$HDR" "FenceEffectCapability fields"; extract_region "$HDR" "kFenceEffectCapabilities init")
    if echo "$SCAN_TEXT" | grep -qiE 'egress|in_flight|hop_count|last_hops|latency_cycles'; then
        fail "fence-orthogonality-violation: 声明区段内出现 G1/G10 字段（egress/in_flight/hop/latency）"
    else
        pass "AC-8 与 G1 / G10 正交（无 egress/in_flight/hop/latency）"
    fi

    # ── AC-9/规则表：契约面（main spec）与实现同源可检 ──────────────────
    if [ ! -f "$SPEC" ]; then
        fail "fence-rule-table-missing: main spec $SPEC 不存在（推导规则表未随契约面登记）"
    else
        MISSING_RULE=""
        for r in "R1" "R2" "R3" "R4" "R5" "R6" "R7"; do
            grep -qF "$r" "$SPEC" || MISSING_RULE="$MISSING_RULE $r"
        done
        if [ -n "$MISSING_RULE" ]; then
            fail "fence-rule-table-missing: main spec 缺规则行:$MISSING_RULE"
        else
            pass "规则表 R1–R7 在 main spec 在位（契约面 ⇄ 实现同源）"
        fi
    fi

    return $FAILED
}

# ══════════════════════════════════════════════════════════════════════════
#  --selftest：2 条决定性变异（跑在拷贝根 · 零工作树污染）
# ══════════════════════════════════════════════════════════════════════════
if [ "$SELFTEST" -eq 1 ]; then
    echo "══ cep_fence_effects_check --selftest(2 变异,跑在拷贝根)══"
    BASE="$(mktemp -d "${TMPDIR:-/tmp/opencode}/fence_selftest.XXXXXX")"
    trap 'rm -rf "$BASE"' EXIT

    copy_root() { # <dst>
        local d="$1"; rm -rf "$d"; mkdir -p "$d/scripts/test" "$d/include/tlm/cep" "$d/test" "$d/openspec/specs/cep-fence-consistency-effect-derivation"
        cp "$REPO_ROOT/scripts/test/cep_fence_effects_check.sh" "$d/scripts/test/"
        cp "$REPO_ROOT/include/tlm/cep/fence_effects.hh" "$d/include/tlm/cep/"
        cp "$REPO_ROOT/include/tlm/cep/protocol_instances.json" "$d/include/tlm/cep/"
        cp "$REPO_ROOT/test/test_cep_fence_effects.cc" "$d/test/"
        cp "$REPO_ROOT/openspec/specs/cep-fence-consistency-effect-derivation/spec.md" \
           "$d/openspec/specs/cep-fence-consistency-effect-derivation/" 2>/dev/null || true
    }

    # 基线：拷贝根必须绿
    copy_root "$BASE/base"
    if ( cd "$BASE/base" && bash "$REPO_ROOT/scripts/test/cep_fence_effects_check.sh" \
            --root "$BASE/base" >/dev/null 2>&1 ); then
        pass "基线:拷贝根 rc=0"
    else
        fail "基线:拷贝根应为绿"; exit 1
    fi

    # 变异 1：注入域外标签组合 ⇒ AC-4 红（fence-derivation-out-of-domain）
    copy_root "$BASE/m1"
    python3 - "$BASE/m1/include/tlm/cep/fence_effects.hh" <<'PYEOF'
import re, sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
m = re.search(r'\{"mesif", "RC-acquire", "RC-release", true,', t)
assert m, "变异 1 锚点未命中"
t = t.replace('{"mesif", "RC-acquire", "RC-release", true,',
              '{"mesif", "RC-acquire", "XYZ", true,', 1)
open(p, "w", encoding="utf-8").write(t)
PYEOF
    if [ $? -ne 0 ]; then
        fail "变异 1 前置失败:锚点未命中（⛔ 不得当作通过）"; exit 1
    fi
    OUT=$( cd "$BASE/m1" && bash "$REPO_ROOT/scripts/test/cep_fence_effects_check.sh" \
            --root "$BASE/m1" 2>&1 )
    if printf '%s' "$OUT" | grep -qF "fence-derivation-out-of-domain"; then
        pass "变异 1:注入域外标签组合 ⇒ 红（fence-derivation-out-of-domain）"
    else
        fail "变异 1(域外标签)应以 fence-derivation-out-of-domain 红,实为:$(printf '%s' "$OUT" | tail -3 | tr '\n' ' ')"
        exit 1
    fi

    # 变异 2：改 JSON 的标签而头文件副本不动 ⇒ AC-3 红（fence-input-copy-drift）
    copy_root "$BASE/m2"
    python3 - "$BASE/m2/include/tlm/cep/protocol_instances.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
d = json.load(open(p, encoding="utf-8"))
hit = False
for i in d.get("instances", []):
    if i.get("id") == "mesif":
        i["consistency_labels"]["read"] = "SC"
        hit = True
assert hit, "变异 2 锚点未命中"
json.dump(d, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
PYEOF
    if [ $? -ne 0 ]; then
        fail "变异 2 前置失败:锚点未命中（⛔ 不得当作通过）"; exit 1
    fi
    OUT=$( cd "$BASE/m2" && bash "$REPO_ROOT/scripts/test/cep_fence_effects_check.sh" \
            --root "$BASE/m2" 2>&1 )
    if printf '%s' "$OUT" | grep -qF "fence-input-copy-drift"; then
        pass "变异 2:改 JSON 标签而副本不动 ⇒ 红（fence-input-copy-drift）"
    else
        fail "变异 2(副本漂移)应以 fence-input-copy-drift 红,实为:$(printf '%s' "$OUT" | tail -3 | tr '\n' ' ')"
        exit 1
    fi

    echo "────────────────────────────────────────"
    echo "✅ selftest:2/2 变异全部按**预期红因**变红"
    exit 0
fi

echo "══ CEP fence_effects 契约闸 ══"
run_checks
echo "────────────────────────────────────────"
if [ "$FAILED" -eq 0 ]; then
    echo "[PASS] cep_fence_effects_check: 全部判据通过"
    exit 0
fi
echo "[FAIL] cep_fence_effects_check: $FAILED 项失败"
exit 1
