#!/usr/bin/env bash
# scripts/test/cep_b3b2_d4_probe_check.sh
#   —— `B3b-2` 受限族的 **D-4 触发探测清单**校验闸
#      (per 台账 Entry #274 §④ E1 + metis gap 分析的 P0-1~P0-4 / G1~G5)
#
# 闸的两部分:
#   ① **真源校验**:探测清单的结构 / 覆盖 / 合规 / 防漂移(5 组判据)
#   ② **决定性变异**(取证):4 个影子副本 ⇒ 闸须按**固定签名**变红
#      —— 证明「判据有牙」而非「恒真」(per 本仓「决定性变异是闭合证据的必附项」纪律)
#
# ⚠️ 本闸**只**校验**协议清单的结构与合规边界**;
#    ⛔ **不**保证执行者未在其他渠道接触规范正文(那是 `incident` 登记 + 季度人工审计的责任)
#    ⛔ **不**改任何 DoD 计数(B3b-2 仍 0/4)
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

PROBE="test/cep_b3b2_d4_probe.json"
DEFERRALS="test/cep_b3b2_deferrals.json"
VALIDATE="scripts/test/cep_b3b2_d4_probe_validate.py"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-b3b2-d4-shadow}"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

# ── 真源 sha256(变异后须复原)────────────────────────────────────────
src_sha_before="$(sha256sum "$PROBE" | cut -d' ' -f1)"
def_sha_before="$(sha256sum "$DEFERRALS" | cut -d' ' -f1)"

mkdir -p "$SHADOW"

# ── ① 真源校验 ───────────────────────────────────────────────────────
echo "══ ① 真源校验 ══"
if out="$(python3 "$VALIDATE" "$PROBE" "$DEFERRALS" 2>&1)"; then
    ok "探测清单: $out"
else
    ng "探测清单未通过: $(echo "$out" | head -3 | tr '\n' ' ')"
fi

# ── ② 决定性变异 ①:注入正文类字段 ⇒ BODY-FIELD-FORBIDDEN ────────────
echo "══ ② 变异 ①:注入正文类字段(body_excerpt)══"
mut="$SHADOW/probe_body.json"
python3 - "$PROBE" "$mut" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding='utf-8'))
d['probes'][0]['body_excerpt'] = '§1.5 UALink Coherency ...'
json.dump(d, open(sys.argv[2], 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PY
if out="$(python3 "$VALIDATE" "$mut" "$DEFERRALS" 2>&1)"; then
    ng "变异①未变红(实际: $out)"
elif echo "$out" | grep -q 'BODY-FIELD-FORBIDDEN'; then
    ok "变异①按签名变红: $(echo "$out" | grep -m1 'BODY-FIELD-FORBIDDEN')"
else
    ng "变异①签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
fi

# ── ③ 决定性变异 ②:元数据位指向 .pdf 正文直链 ⇒ DOC-URL-IN-METADATA-SLOT ──
echo "══ ③ 变异 ②:元数据位换成 .pdf 直链 ══"
mut="$SHADOW/probe_pdf.json"
python3 - "$PROBE" "$mut" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding='utf-8'))
for pr in d['probes']:
    for s in pr.get('search_sources', []) or []:
        s['url'] = 'https://example.org/UALink200_Specification_v1.0.pdf'
        break
    break
json.dump(d, open(sys.argv[2], 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PY
if out="$(python3 "$VALIDATE" "$mut" "$DEFERRALS" 2>&1)"; then
    ng "变异②未变红(实际: $out)"
elif echo "$out" | grep -q 'DOC-URL-IN-METADATA-SLOT'; then
    ok "变异②按签名变红: $(echo "$out" | grep -m1 'DOC-URL-IN-METADATA-SLOT')"
else
    ng "变异②签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
fi

# ── ④ 决定性变异 ③:删一格覆盖 ⇒ COVERAGE-GAP ────────────────────────
echo "══ ④ 变异 ③:删一格覆盖(cxl_3_0_symmetric/internal_excerpt)══"
mut="$SHADOW/probe_gap.json"
python3 - "$PROBE" "$mut" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding='utf-8'))
d['probes'] = [p for p in d['probes']
                if not (p.get('family') == 'cxl_3_0_symmetric'
                        and p.get('path') == 'internal_excerpt')]
json.dump(d, open(sys.argv[2], 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PY
if out="$(python3 "$VALIDATE" "$mut" "$DEFERRALS" 2>&1)"; then
    ng "变异③未变红(实际: $out)"
elif echo "$out" | grep -q 'COVERAGE-GAP: cxl_3_0_symmetric/internal_excerpt'; then
    ok "变异③按签名变红: $(echo "$out" | grep -m1 'COVERAGE-GAP')"
else
    ng "变异③签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
fi

# ── ⑤ 决定性变异 ④:判据改成主观措辞 ⇒ SUBJECTIVE-CRITERION ─────────
echo "══ ⑤ 变异 ④:判据改成主观措辞 ══"
mut="$SHADOW/probe_subj.json"
python3 - "$PROBE" "$mut" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding='utf-8'))
d['probes'][0]['must_all'][0]['value'] = '确认可读即可'
json.dump(d, open(sys.argv[2], 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PY
if out="$(python3 "$VALIDATE" "$mut" "$DEFERRALS" 2>&1)"; then
    ng "变异④未变红(实际: $out)"
elif echo "$out" | grep -q 'SUBJECTIVE-CRITERION'; then
    ok "变异④按签名变红: $(echo "$out" | grep -m1 'SUBJECTIVE-CRITERION')"
else
    ng "变异④签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
fi

# ── ⑥ 决定性变异 ⑤:deferral trigger 漂移 ⇒ DEFERRRAL-TRIGGER-DRIFT ──
echo "══ ⑥ 变异 ⑤:deferral trigger 哈希漂移 ══"
mut="$SHADOW/probe_drift.json"
python3 - "$PROBE" "$mut" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding='utf-8'))
d['probes'][0]['deferral_trigger_sha256'] = '0' * 64
json.dump(d, open(sys.argv[2], 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PY
if out="$(python3 "$VALIDATE" "$mut" "$DEFERRALS" 2>&1)"; then
    ng "变异⑤未变红(实际: $out)"
elif echo "$out" | grep -q 'DEFERRRAL-TRIGGER-DRIFT'; then
    ok "变异⑤按签名变红: $(echo "$out" | grep -m1 'DEFERRRAL-TRIGGER-DRIFT')"
else
    ng "变异⑤签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
fi

# ── ⑦ 真源复原校验(变异只碰影子副本)────────────────────────────────
echo "══ ⑦ 真源复原校验 ══"
src_sha_after="$(sha256sum "$PROBE" | cut -d' ' -f1)"
def_sha_after="$(sha256sum "$DEFERRALS" | cut -d' ' -f1)"
if [[ "$src_sha_before" == "$src_sha_after" ]]; then
    ok "探测清单真源 sha256 未变"
else
    ng "探测清单真源被改(sha256 变化)"
fi
if [[ "$def_sha_before" == "$def_sha_after" ]]; then
    ok "deferrals 真源 sha256 未变(⛔ 本闸不改计数)"
else
    ng "deferrals 真源被改(⛔ 违规)"
fi

# ── ⑧ 计数纪律:新增文件不得出现「已达成」形态的 B3b-2 计数 ────────────
echo "══ ⑧ 计数纪律 ══"
if grep -nE 'B3b-2[^0-9]{0,20}(达成|✅)|[1-4]/4[^0-9]' "$PROBE" "$SHADOW"/*.json 2>/dev/null \
   | grep -v '0/4' | head -3 | grep -q .; then
    ng "探测清单出现「B3b-2 已达成」形态的计数"
else
    ok "探测清单无「B3b-2 已达成」形态的计数(B3b-2 仍 0/4)"
fi

echo "────────────────────────────────────────"
if [[ "$fail" -eq 0 ]]; then
    echo "✅ cep_b3b2_d4_probe_check: $pass PASS / 0 FAIL"
    exit 0
fi
echo "❌ cep_b3b2_d4_probe_check: $fail FAIL"
exit 1
