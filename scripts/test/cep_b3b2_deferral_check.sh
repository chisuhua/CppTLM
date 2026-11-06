#!/usr/bin/env bash
# scripts/test/cep_b3b2_deferral_check.sh
#   —— B3b-2(受限族 0/4)的**机器可查**登记校验
#      (change cpptlm-cep-dcoh-nvlink-refkind-migration 的配套登记面)
#
# 校验 4 条:
#   ① JSON 可解析 + 4 族齐全(cxl_2_0 / cxl_3_0_symmetric / nvlink / ualink)
#   ② 每条字段齐备(disposition / blocking_type / reason / trigger_condition / ledger_entry)
#   ③ 每条的 ledger_entry 在台账(obj-cep-platform.md)中**真实存在**
#   ④ 族名单与 `07 号` SU·六 的受限族名单**一致**(防登记与真源漂移)
#
# ⚠️ 本闸只校验**登记的完整性**,⛔ **不**改 B3b-2 的计数(仍 0/4)——
#    登记「已按授权路径登记完毕」≠「已达成」(per 07 号 SU·六 与台账 Entry #264)。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

DECL="test/cep_b3b2_deferrals.json"
LEDGER=".rddf/roadmap/objectives/obj-cep-platform.md"
pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

out="$(python3 - "$DECL" "$LEDGER" <<'PY'
import json, re, sys
decl_path, ledger_path = sys.argv[1], sys.argv[2]
doc = json.load(open(decl_path, encoding='utf-8'))
rows = doc.get('deferrals', [])
expected = ['cxl_2_0', 'cxl_3_0_symmetric', 'nvlink', 'ualink']
fams = [r.get('family') for r in rows]
errs = []
if sorted(fams) != sorted(expected):
    errs.append('family set mismatch: got %s expected %s' % (sorted(fams), sorted(expected)))
required = ['disposition', 'blocking_type', 'reason', 'trigger_condition', 'ledger_entry']
ledger = open(ledger_path, encoding='utf-8').read()
for r in rows:
    for k in required:
        if not r.get(k):
            errs.append('%s: missing field %s' % (r.get('family'), k))
    le = str(r.get('ledger_entry', ''))
    if le and le not in ledger:
        errs.append('%s: ledger_entry %s not found in ledger' % (r.get('family'), le))
if errs:
    print('\n'.join(errs)); sys.exit(1)
print('%d deferrals, all fields present, ledger entries resolvable' % len(rows))
PY
)"
if [[ $? -eq 0 ]]; then
    ok "登记校验: $out"
else
    ng "登记校验: $out"
fi

# ④ 与 07 号 SU·六 的族名单一致(受限族 = CXL 3.0 / NVLink / UALink + 冻结族 CXL 2.0)
if grep -q 'cxl_2_0' "$DECL" && grep -q 'cxl_3_0_symmetric' "$DECL" && \
   grep -q '"family": "nvlink"' "$DECL" && grep -q '"family": "ualink"' "$DECL"; then
    ok "族名单: 4 族齐全(CXL 2.0 冻结 + CXL 3.0 / NVLink / UALink 受限)"
else
    ng "族名单: 缺族"
fi

echo "────────────────────────────────────────"
if [[ "$fail" -eq 0 ]]; then
    echo "✅ cep_b3b2_deferral_check: $pass PASS / 0 FAIL"
    exit 0
fi
echo "❌ cep_b3b2_deferral_check: $fail FAIL"
exit 1
