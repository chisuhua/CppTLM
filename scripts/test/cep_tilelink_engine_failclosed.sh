#!/usr/bin/env bash
# scripts/test/cep_tilelink_engine_failclosed.sh
#   —— TileLink-C 运行时引擎的 **fail-closed 消费面 + 决定性变异**取证
#
# 机制(照队列 #14 `cep_ualink_inc_reverse_verify.sh` 影子树先例):
#   每次 fixture 把**被改动的副本**放进影子目录 ⇒ 编译探针时 `-I $SHADOW/include`
#   **优先于** `-I include` ⇒ 探针吃到的是**影子副本**,而**仓内真源零改动**;
#   收尾断言真源 sha256 **未变**(⇒ 「取证 + 复原」可机器验证)。
#
# 三态 fixture(期望**签名**钉死,⛔ 非「任意失败即算」):
#   ① clean   —— 影子树 = 真源副本      ⇒ 探针编译通过 + PASS direct(有事件)
#   ② pending —— 影子树把 `TLC-M-02` 行改 `basis=Pending` + `event_mask=0`
#                                             ⇒ 探针编译通过 + PASS pending(**零发射**)
#   ③ deleted —— 影子树删掉 `TLC-M-02` 行 + `kPlatformIntentCount` 11→10
#                                             ⇒ 编译**必须失败**,且失败签名命中
#                                                `tilelink_state_spec.hh` 的
#                                                T3 编译期锁(static_assert 文案含 "T3:")
#
# 用法:bash scripts/test/cep_tilelink_engine_failclosed.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE_SRC="scripts/test/cep_tilelink_engine_probe.cc"
SPEC_REL="include/tlm/cep/tilelink_state_spec.hh"
CXX="${CXX:-g++}"
SHADOW_ROOT="${SHADOW_ROOT:-/tmp/opencode/cep-tilelink-engine-shadow}"

pass=0
fail=0
fail_msgs=()

ok()  { echo "  ✓ $1"; pass=$((pass+1)); }
bad() { echo "  ✗ $1"; fail=$((fail+1)); fail_msgs+=("$1"); }

[ -f "$PROBE_SRC" ] || { echo "  ✗ 探针源不存在: $PROBE_SRC"; exit 1; }
[ -f "$SPEC_REL" ]  || { echo "  ✗ 缺 $SPEC_REL"; exit 1; }

REAL_SHA_BEFORE="$(sha256sum "$SPEC_REL" | awk '{print $1}')"

build_probe() {  # $1=影子根  $2=输出二进制
    # ⚠️ include 路径需覆盖 core 内部相对包含(`packet.hh` 在 include/core/)
    "$CXX" -std=c++17 -I "$1/include" -I include -I include/core -I external/json \
        "$PROBE_SRC" -o "$2" 2>"$2.log"
}

make_shadow() {   # $1=影子根
    rm -rf "$1"
    mkdir -p "$1/include/tlm/cep"
    cp "$SPEC_REL" "$1/include/tlm/cep/tilelink_state_spec.hh"
}

echo "════════ TileLink 引擎 fail-closed / 决定性变异取证 ════════"

# ── ① clean(正控)─────────────────────────────────────────────────────────
make_shadow "$SHADOW_ROOT/clean"
if build_probe "$SHADOW_ROOT/clean" "$SHADOW_ROOT/probe_clean"; then
    if out="$("$SHADOW_ROOT/probe_clean" 2>&1)"; then
        case "$out" in
            *"PASS direct"*) ok "① clean:探针编译通过 + 正控成立(Direct 行发 1 事件)" ;;
            *) bad "① clean:探针输出非预期 ⇒ $out" ;;
        esac
    else
        bad "① clean:探针运行失败 ⇒ $out"
    fi
else
    bad "① clean:探针编译失败 ⇒ $(head -3 "$SHADOW_ROOT/probe_clean.log")"
fi

# ── ② pending(fail-closed 消费面)────────────────────────────────────────
make_shadow "$SHADOW_ROOT/pending"
python3 - "$SHADOW_ROOT/pending/include/tlm/cep/tilelink_state_spec.hh" <<'PY'
import re, sys
p = sys.argv[1]
t = open(p, encoding='utf-8').read()
pat = re.compile(r'(\{PlatformIntent::Completion, "TLC-M-02", )kT3EventGrow(, )T3BasisKind::Direct(,)')
t2, n = pat.subn(r'\g<1>0\g<2>T3BasisKind::Pending\g<3>', t)
if n != 1:
    print("MUTATE_FAIL", n); sys.exit(1)
open(p, 'w', encoding='utf-8').write(t2)
PY
if [ $? -ne 0 ]; then
    bad "② pending:影子树毒化失败(行未唯一命中)"
else
    if build_probe "$SHADOW_ROOT/pending" "$SHADOW_ROOT/probe_pending"; then
        if out="$("$SHADOW_ROOT/probe_pending" 2>&1)"; then
            case "$out" in
                *"PASS pending"*) ok "② pending:探针编译通过 + **引擎发射出口零计数**(fail-closed 成立)" ;;
                *) bad "② pending:探针输出非预期 ⇒ $out" ;;
            esac
        else
            bad "② pending:探针运行失败 ⇒ $out"
        fi
    else
        bad "② pending:探针编译失败 ⇒ $(head -3 "$SHADOW_ROOT/probe_pending.log")"
    fi
fi

# ── ③ deleted(决定性变异:期望失败签名 = T3 编译期锁)─────────────────────
make_shadow "$SHADOW_ROOT/deleted"
python3 - "$SHADOW_ROOT/deleted/include/tlm/cep/tilelink_state_spec.hh" <<'PY'
import re, sys
p = sys.argv[1]
t = open(p, encoding='utf-8').read()
t, n1 = re.subn(r'kPlatformIntentCount = 11', 'kPlatformIntentCount = 10', t)
t, n2 = re.subn(r'\{PlatformIntent::Completion, "TLC-M-02".*?\},\n', '', t, flags=re.S)
if n1 != 1 or n2 != 1:
    print("MUTATE_FAIL", n1, n2); sys.exit(1)
open(p, 'w', encoding='utf-8').write(t)
PY
if [ $? -ne 0 ]; then
    bad "③ deleted:影子树毒化失败"
else
    if build_probe "$SHADOW_ROOT/deleted" "$SHADOW_ROOT/probe_deleted"; then
        bad "③ deleted:编译**居然通过** ⇒ 决定性变异判据无鉴别力"
    else
        if grep -q 'T3:' "$SHADOW_ROOT/probe_deleted.log"; then
            ok "③ deleted:编译红且**签名命中**(T3 编译期锁 static_assert)⇒ 按正确原因红"
        else
            bad "③ deleted:编译失败但**签名不匹配**(期望含 'T3:') ⇒ $(grep -m1 'error' "$SHADOW_ROOT/probe_deleted.log")"
        fi
    fi
fi

# ── ④ 真源未改动(取证 + 复原可机器验证)──────────────────────────────────
REAL_SHA_AFTER="$(sha256sum "$SPEC_REL" | awk '{print $1}')"
if [ "$REAL_SHA_BEFORE" = "$REAL_SHA_AFTER" ]; then
    ok "④ 真源未被改动(sha256 一致)⇒ 取证后无需复原"
else
    bad "④ 真源 sha256 变化 ⇒ 影子树机制失效"
fi

echo "────────────────────────────────────────────────────────"
echo "  通过 $pass / 失败 $fail"
if [ "$fail" -ne 0 ]; then
    printf '  失败项:%s\n' "${fail_msgs[@]}"
    exit 1
fi
exit 0
