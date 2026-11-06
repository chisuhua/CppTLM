#!/usr/bin/env bash
# scripts/test/cep_source_effect_check.sh
#   —— 来源类型判据的**决定性变异**取证
#      (change cpptlm-cep-basis-source-kind-and-cell-free + 契约对齐 change)
#
# 四态(期望签名钉死,⛔ 非「任意失败即算」):
#   ① clean          —— 真源            ⇒ JSON↔头文件一致 + 探针 rc=0 + "SRK clean"
#   ② dropped        —— 影子 JSON 删 cxl-d1 ⇒ 一致性检查**红**,签名 "missing in json: cxl-d1"
#   ③ downgraded     —— 影子头把 tilelink-c 改 PublicNonnormative
#                                        ⇒ 探针**红**,签名 "SRK-02 inconsistency"
#   ④ misclassified  —— 影子头把 cxl 的 transition_is_free 改判 `Unknown`
#                                        ⇒ 探针**红**,签名 "FREE-KIND mismatch"
#      (④ 覆盖「`CellKind::Free` 被 `Unknown` 吞掉」—— 消费端降级语义漂移的判据)
#
# 影子树机制照 `cep_tilelink_engine_failclosed.sh` 先例:改副本,真源零改动;
# 收尾断言真源 sha256 未变(3 个文件)。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

CXX="${CXX:-g++}"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-source-shadow}"
PROBE="scripts/test/cep_source_effect_check_probe.cc"
HEADER="include/tlm/cep/source_registry.hh"
JSON="include/tlm/cep/sources.json"
CXL_ADAPTER="include/tlm/cep/cxl_cache_adapter.hh"

mkdir -p "$SHADOW"
pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

sha_before_h="$(sha256sum "$HEADER" | cut -d' ' -f1)"
sha_before_j="$(sha256sum "$JSON" | cut -d' ' -f1)"
sha_before_a="$(sha256sum "$CXL_ADAPTER" | cut -d' ' -f1)"

# ── 影子树准备:复制整个 cep 头目录(影子优先于真源) ────────────────────────
prepare_shadow() {
    rm -rf "$SHADOW/include"
    mkdir -p "$SHADOW/include/tlm"
    cp -r include/tlm/cep "$SHADOW/include/tlm/cep"
}

# ── 一致性检查(JSON ↔ 头文件 id 集合) ─────────────────────────────────────
consistency() {
    local json_path="$1" header_path="$2"
    python3 - "$json_path" "$header_path" <<'PY'
import json, re, sys
json_path, header_path = sys.argv[1], sys.argv[2]
doc = json.load(open(json_path, encoding='utf-8'))
jids = [s['id'] for s in doc.get('sources', [])]
text = open(header_path, encoding='utf-8').read()
hids = re.findall(r'\{"([a-z0-9-]+)",\s*RefKind::', text)
missing = [i for i in hids if i not in jids]
extra = [i for i in jids if i not in hids]
if missing:
    print('missing in json: ' + ', '.join(missing)); sys.exit(1)
if extra:
    print('extra in json: ' + ', '.join(extra)); sys.exit(1)
if jids != hids:
    print('order mismatch: json=' + str(jids) + ' header=' + str(hids)); sys.exit(1)
print('consistent: %d sources' % len(jids))
PY
}

echo "══ ① clean ══"
if out="$(consistency "$JSON" "$HEADER" 2>&1)" && echo "$out" | grep -q '^consistent: '; then
    ok "一致性: $out"
else
    ng "一致性(clean): $out"
fi
if "$CXX" -std=c++23 -I include "$PROBE" -o "$SHADOW/probe_clean" 2>"$SHADOW/clean.err" \
    && "$SHADOW/probe_clean" >"$SHADOW/clean.out" 2>&1 \
    && grep -q '^SRK clean$' "$SHADOW/clean.out"; then
    ok "探针: SRK clean"
else
    ng "探针(clean): $(cat "$SHADOW/clean.out" "$SHADOW/clean.err" 2>/dev/null | head -3)"
fi

echo "══ ② dropped(影子 JSON 删 cxl-d1)══"
python3 - "$JSON" "$SHADOW/sources_dropped.json" <<'PY'
import json, sys
src, dst = sys.argv[1], sys.argv[2]
d = json.load(open(src, encoding='utf-8'))
d['sources'] = [s for s in d['sources'] if s['id'] != 'cxl-d1']
json.dump(d, open(dst, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PY
if out="$(consistency "$SHADOW/sources_dropped.json" "$HEADER" 2>&1)"; then
    ng "dropped 未变红(实际: $out)"
elif echo "$out" | grep -q 'missing in json: cxl-d1'; then
    ok "dropped 按签名变红: $out"
else
    ng "dropped 签名不符: $out"
fi

echo "══ ③ downgraded(影子头 tilelink-c → PublicNonnormative)══"
prepare_shadow
sed -i 's/{"tilelink-c", RefKind::SpecClause,/{"tilelink-c", RefKind::PublicNonnormative,/' \
    "$SHADOW/include/tlm/cep/source_registry.hh"
if ! grep -q '{"tilelink-c", RefKind::PublicNonnormative,' "$SHADOW/include/tlm/cep/source_registry.hh"; then
    ng "downgraded 变异未生效(影子头未改)"
elif "$CXX" -std=c++23 -I "$SHADOW/include" -I include "$PROBE" -o "$SHADOW/probe_dg" 2>"$SHADOW/dg.err"; then
    if "$SHADOW/probe_dg" >"$SHADOW/dg.out" 2>&1; then
        ng "downgraded 未变红(探针 rc=0)"
    elif grep -q 'SRK-02 inconsistency' "$SHADOW/dg.out"; then
        ok "downgraded 按签名变红: $(head -1 "$SHADOW/dg.out")"
    else
        ng "downgraded 签名不符: $(head -3 "$SHADOW/dg.out")"
    fi
else
    ng "downgraded 编译失败(签名不符): $(head -2 "$SHADOW/dg.err")"
fi

echo "══ ④ misclassified(影子头 transition_is_free → 判 Unknown)══"
prepare_shadow
sed -i 's/return CxlStateSpec::transition(s, e).kind == CellKind::Free;/return CxlStateSpec::transition(s, e).kind == CellKind::Unknown;/' \
    "$SHADOW/include/tlm/cep/cxl_cache_adapter.hh"
if ! grep -q 'kind == CellKind::Unknown;' "$SHADOW/include/tlm/cep/cxl_cache_adapter.hh"; then
    ng "misclassified 变异未生效(影子头未改)"
elif "$CXX" -std=c++23 -I "$SHADOW/include" -I include "$PROBE" -o "$SHADOW/probe_mc" 2>"$SHADOW/mc.err"; then
    if "$SHADOW/probe_mc" >"$SHADOW/mc.out" 2>&1; then
        ng "misclassified 未变红(探针 rc=0)"
    elif grep -q 'FREE-KIND mismatch' "$SHADOW/mc.out"; then
        ok "misclassified 按签名变红: $(head -1 "$SHADOW/mc.out")"
    else
        ng "misclassified 签名不符: $(head -3 "$SHADOW/mc.out")"
    fi
else
    ng "misclassified 编译失败(签名不符): $(head -2 "$SHADOW/mc.err")"
fi

echo "══ 真源复原校验 ══"
sha_after_h="$(sha256sum "$HEADER" | cut -d' ' -f1)"
sha_after_j="$(sha256sum "$JSON" | cut -d' ' -f1)"
sha_after_a="$(sha256sum "$CXL_ADAPTER" | cut -d' ' -f1)"
[[ "$sha_before_h" == "$sha_after_h" ]] && ok "header sha256 未变" || ng "header 被改动!"
[[ "$sha_before_j" == "$sha_after_j" ]] && ok "json sha256 未变" || ng "json 被改动!"
[[ "$sha_before_a" == "$sha_after_a" ]] && ok "cxl adapter sha256 未变" || ng "cxl adapter 被改动!"

echo "────────────────────────────────────────"
if [[ "$fail" -eq 0 ]]; then
    echo "✅ cep_source_effect_check: $pass PASS / 0 FAIL"
    exit 0
fi
echo "❌ cep_source_effect_check: $fail FAIL"
exit 1
