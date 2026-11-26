#!/usr/bin/env bash
# scripts/test/cep_ssp_check.sh
# SSP 声明面（批 1）—— 契约闸
# per openspec/changes/cep-ssp-declaration/proposal.md 的 A-1 ~ A-14
#    （来源 = ADR-CEP-24 §6 V-24-1~13 + CP-Q-1a 裁决 §8 的 8 条 AC）
#
# 本闸覆盖**静态 / 结构面**（runtime 面的逐格对账与 fail-closed 由
# `test/test_cep_ssp_declaration.cc` 的 `[cep][ssp]` 承担 —— ctest 已注册 `cep_ssp_declaration`）：
#   A-4   锚集封闭：kCoreAnchorCount == 14 且 kCoreAnchors 恰 14 行
#   A-6   14 个锚名**逐字**（防改名漂移 · 先例 kEventNames）
#   A-9   「无等价」占比可观测：ssps.json 中无 anchor 的私有事件占比 > 半数 ⇒ 红（触发锚集复审）
#   A-11  通用层（event_alphabet.hh / ssp_loader.hh）**零协议名**
#   A-12a 声明面**不表达** belief / token / atomic / 瞬态态
#   A-12b 措辞禁令：无「保证无死锁 / 保证无违例 / 保证兼容」
#   A-14  表达力表述须含「3/6」「2/6」，且**不得**出现「6/6」
#   D-1   JSON 与 schema 成对（AGENTS.md 铁律 3）
#   D-2   ssps.json 的每个 event 的 anchor ∈ 14 锚（独立于 C++ loader 的复述）
#   D-3   ssps.json 的 event **原子性**：不得含嵌套 `events` 键
#
# 判据纪律：⛔ 「rc≠0」不构成红因；fixture 必须断言**红因签名命中**。
# 作者: CppTLM Team / 日期: 2026-11-26
#
# 用法:
#   bash scripts/test/cep_ssp_check.sh              # 正常校验
#   bash scripts/test/cep_ssp_check.sh --root <dir>  # 对拷贝根校验
#   bash scripts/test/cep_ssp_check.sh --selftest     # 3 条反向 fixture（拷贝根 · 零工作树污染）
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

INC="include/tlm/cep"
ALPHA="$INC/event_alphabet.hh"
LOADER="$INC/ssp_loader.hh"
DATA="$INC/ssps.json"
SCHEMA="$INC/ssp.schema.json"
SPEC="openspec/specs/cep-ssp-declaration/spec.md"

PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); printf '  \033[0;32m✓\033[0m %s\n' "$*"; }
bad()  { FAIL=$((FAIL+1)); printf '  \033[0;31m✗\033[0m %s\n' "$*"; }
need(){ [ -f "$ROOT/$1" ] || { bad "缺文件: $1"; return 1; }; }

# 通用层零协议名（协议名清单逐字 · ⛔ 非开放集表述）
PROTO_NAMES='MESI|MOESI|CHI|CXL|TileLink|NVLink|UALink|MESIF'

run_checks() {
    local R="$1"

    echo "── 判据 1（A-11）通用层零协议名 ──"
    for f in "$ALPHA" "$LOADER"; do
        if [ -f "$R/$f" ]; then
            if grep -qE "$PROTO_NAMES" "$R/$f"; then bad "$f 出现协议名"; else ok "$f 无协议名"; fi
        else bad "缺 $f"; fi
    done

    echo "── 判据 2（A-4）锚集封闭 = 14 ──"
    if [ -f "$R/$ALPHA" ]; then
        if grep -qE 'kCoreAnchorCount = 14' "$R/$ALPHA" && \
           grep -qE 'static_assert\(kCoreAnchors.size\(\) == kCoreAnchorCount' "$R/$ALPHA"; then
            ok "kCoreAnchorCount = 14 + static_assert"
        else bad "锚集封闭常量缺失"; fi
        local nrows; nrows=$(grep -cE '^\s*\{CoreAnchor::' "$R/$ALPHA")
        if [ "$nrows" -eq 14 ]; then ok "kCoreAnchors 恰 14 行"; else bad "kCoreAnchors 行数 = $nrows (期望 14)"; fi
    else bad "缺 $ALPHA"; fi

    echo "── 判据 3（A-6）14 个锚名逐字 ──"
    if [ -f "$R/$ALPHA" ]; then
        local miss=0
        for name in ReadShared ReadUnique Write WriteNoAlloc Upgrade \
                    SnoopShared SnoopUnique SnoopInvalidate EvictDirty EvictClean \
                    Cap Grow Prune Report; do
            grep -q "\"$name\"" "$R/$ALPHA" || { bad "锚名缺: $name"; miss=1; }
        done
        [ "$miss" -eq 0 ] && ok "14 锚名逐字齐备"
    fi

    echo "── 判据 4（D-1）JSON 与 schema 成对 ──"
    need "$DATA" && need "$SCHEMA" && ok "ssps.json + ssp.schema.json 成对"

    echo "── 判据 5（D-2 / D-3 / A-9）数据面：anchor ⊆ 14 锚 · 原子性 · 无等价占比 ──"
    if [ -f "$R/$DATA" ]; then
        python3 - "$R/$DATA" "$R/$ALPHA" <<'PY' || return 1
import json, re, sys
data_path, alpha_path = sys.argv[1], sys.argv[2]
d = json.load(open(data_path))
alpha = open(alpha_path).read()
anchors = set(re.findall(r'\{CoreAnchor::\w+,\s*"([A-Za-z]+)"', alpha))
if len(anchors) != 14:
    print(f"  \033[0;31m✗\033[0m 从 alpha 解析出锚数 = {len(anchors)} (期望 14)"); sys.exit(1)
bad = 0
total = noanchor = 0
for s in d.get("ssps", []):
    for e in s.get("events", []):
        total += 1
        if "events" in e:
            print(f"  \033[0;31m✗\033[0m event '{e.get('name')}' 含嵌套 events（违反原子性 A-8）"); bad = 1
        a = e.get("anchor")
        if a is None:
            noanchor += 1
            if not e.get("flow") or not e.get("architect_approval"):
                print(f"  \033[0;31m✗\033[0m 私有事件 '{e.get('name')}' 缺 flow/architect_approval"); bad = 1
        elif a not in anchors:
            print(f"  \033[0;31m✗\033[0m event '{e.get('name')}' 的 anchor '{a}' 非 14 锚"); bad = 1
if bad: sys.exit(1)
print(f"  \033[0;32m✓\033[0m anchor ⊆ 14 锚 · 原子性 OK（events={total}）")
if total and noanchor * 2 > total:
    print(f"  \033[0;31m✗\033[0m 「无等价」占比 {noanchor}/{total} > 半数 ⇒ 触发锚集复审（A-9）"); sys.exit(1)
print(f"  \033[0;32m✓\033[0m 「无等价」占比 {noanchor}/{total} ≤ 半数")
PY
    else bad "缺 $DATA"; fi

    echo "── 判据 6（A-12a）声明面不表达 belief / token / atomic / 瞬态态 ──"
    if [ -f "$R/$SCHEMA" ]; then
        if grep -qiE 'belief|token|transient|atomic' "$R/$SCHEMA"; then
            bad "schema 出现禁表达字段"; else ok "schema 无 belief/token/transient/atomic"; fi
    else bad "缺 $SCHEMA"; fi

    echo "── 判据 7（A-12b）措辞禁令：保证无死锁 / 无违例 / 兼容 ──"
    local forbid=0
    for f in "$ALPHA" "$LOADER" "$SCHEMA" "$SPEC"; do
        [ -f "$R/$f" ] || continue
        if grep -qE '保证无死锁|保证无违例|保证兼容' "$R/$f"; then bad "$f 含禁表述"; forbid=1; fi
    done
    [ "$forbid" -eq 0 ] && ok "无「保证无…」类禁表述"

    echo "── 判据 8（A-14）表达力表述：须含 3/6 与 2/6，且不得 6/6 ──"
    if [ -f "$R/$SPEC" ]; then
        local s; s=$(cat "$R/$SPEC")
        if printf '%s' "$s" | grep -q '3/6'; then ok "spec 含 3/6"; else bad "spec 缺 3/6 表述"; fi
        if printf '%s' "$s" | grep -q '2/6'; then ok "spec 含 2/6"; else bad "spec 缺 2/6 表述"; fi
        if printf '%s' "$s" | grep -qE '表达力.{0,8}6/6|6/6.{0,8}表达力'; then
            bad "spec 出现「6/6 表达力」（禁）"; else ok "spec 无「6/6 表达力」误述"; fi
    else bad "缺 $SPEC"; fi

    echo "── 判据 9（A-10 复述）实例面 14 属性不动 ──"
    if grep -q 'REQUIRE(props.size() == 14)' "$R/test/test_cep_schema.cc" 2>/dev/null; then
        ok "test_cep_schema.cc 仍有 properties == 14 断言"
    else bad "实例面 14 属性断言缺失（V-24-1）"; fi
}

echo "══ SSP 声明面契约闸（cep_ssp_check）══"
run_checks "$ROOT"
echo "────────────────────────────────────────"
if [ "$FAIL" -eq 0 ]; then
    echo "[PASS] cep_ssp_check: 全部判据通过（$PASS 项）"
else
    echo "[FAIL] cep_ssp_check: $FAIL 项失败 / $PASS 项通过"
fi

# ── 反向 fixture（拷贝根 · 零工作树污染）────────────────────────────────────
if [ "$SELFTEST" -eq 1 ]; then
    echo; echo "══ --selftest：3 条决定性变异 ══"
    MUT=0
    mk() {
        local tmp; tmp=$(mktemp -d)
        mkdir -p "$tmp/include/tlm"
        cp -r "$REPO_ROOT/$INC" "$tmp/include/tlm/" 2>/dev/null
        mkdir -p "$tmp/openspec/specs/cep-ssp-declaration"
        cp "$REPO_ROOT/$SPEC" "$tmp/openspec/specs/cep-ssp-declaration/" 2>/dev/null
        mkdir -p "$tmp/test"
        cp "$REPO_ROOT/test/test_cep_schema.cc" "$tmp/test/" 2>/dev/null
        echo "$tmp"
    }

    # 变异①：通用层注入协议名 ⇒ 判据 1 红
    T=$(mk)
    sed -i '0,/^namespace tlm::cep::ssp {/s//\/\/ MESI leak\nnamespace tlm::cep::ssp {/' "$T/$ALPHA"
    OUT=$(bash "$0" --root "$T" 2>&1 || true)
    if printf '%s' "$OUT" | grep -q '出现协议名'; then
        echo "  ✓ 变异①按签名红（通用层协议名）"; else echo "  ✗ 变异①未红"; MUT=1; fi
    rm -rf "$T"

    # 变异②：锚集加一行 ⇒ 判据 2 红（行数 15）
    T=$(mk)
    sed -i 's|{CoreAnchor::Report, *"Report", *nullptr, *false},|{CoreAnchor::Report, "Report", nullptr, false},\n        {CoreAnchor::Cap, "Cap2", nullptr, false},|' "$T/$ALPHA"
    OUT=$(bash "$0" --root "$T" 2>&1 || true)
    if printf '%s' "$OUT" | grep -q 'kCoreAnchors 行数 = 15'; then
        echo "  ✓ 变异②按签名红（锚集 15 行）"; else echo "  ✗ 变异②未红"; MUT=1; fi
    rm -rf "$T"

    # 变异③：spec 注入「6/6 表达力」⇒ 判据 8 红
    T=$(mk)
    echo 'ni sha: 表达力 6/6' >> "$T/$SPEC"
    OUT=$(bash "$0" --root "$T" 2>&1 || true)
    if printf '%s' "$OUT" | grep -q '6/6 表达力'; then
        echo "  ✓ 变异③按签名红（6/6 误述）"; else echo "  ✗ 变异③未红"; MUT=1; fi
    rm -rf "$T"

    [ "$MUT" -eq 0 ] && { echo "  ⇒ selftest 全绿"; exit 0; } || exit 1
fi

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
