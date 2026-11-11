#!/usr/bin/env bash
# scripts/test/cep_b3b2_deferral_check.sh
#   —— B3b-2(受限族 0/4)的**机器可查**登记校验
#      (change cpptlm-cep-dcoh-nvlink-refkind-migration 的配套登记面)
#
# 校验 4 条:
#   ① JSON 可解析 + 4 族齐全(cxl_2_0 / cxl_3_0_symmetric / nvlink / ualink)
#   ② 每条字段齐备(disposition / blocking_type / reason / trigger_condition / ledger_entry)
#   ③ 每条的 ledger_entry 在台账(obj-cep-platform.md)中**真实存在**
#   ④ 族名单与**外部真源** `obj-cep-platform.md §6.2` 的 **B3b-2 行**族枚举**一致**(防登记与真源漂移)
#
# ⚠️ 本闸只校验**登记的完整性**,⛔ **不**改 B3b-2 的计数(仍 0/4)——
#    登记「已按授权路径登记完毕」≠「已达成」(per `obj §6.2` B3b-2 行 与台账 Entry #264)。
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

# ④ 族名单 ↔ **外部真源** `obj-cep-platform.md §6.2` 的 B3b-2 行(防「脚本内硬编码名单 vs 真源」漂移)
#    ⚠️ 2026-11-12(per 台账 Entry #342):原 `grep -q <族名> "$DECL"` 段 grep 的是 **$DECL 自身**
#       ⇒ 恒真(真源侧零校验)⇒ 已删除;上方 python 段的集合判据保留作回归基线。
#    ⚠️ 为什么不是 `07 号` SU·六:收口批(Entry #339)已把 SU 正文改为**归宿指针** ⇒ 该段**不再含族枚举**;
#       现存真源 = `obj §6.2` 的 `- **B3b-2. …**` 行(4 族枚举 + 分母 4)。
out4="$(python3 - "$DECL" "$LEDGER" <<'PY'
import json, re, sys
decl_path, ledger_path = sys.argv[1], sys.argv[2]
# 族名 ↔ 真源 token 映射(真源 = `obj §6.2` 的 B3b-2 行,逐字)
TOKENS = {
    'cxl_2_0': 'CXL 2.0',
    'cxl_3_0_symmetric': 'CXL 3.0',
    'nvlink': 'NVLink',
    'ualink': 'UALink',
}
lines = open(ledger_path, encoding='utf-8').read().splitlines()
hits = [l for l in lines if re.match(r'^- \*\*B3b-2\.', l)]
if len(hits) != 1:
    print('真源定位失败: `obj §6.2` 的 B3b-2 行命中 %d 条(应恰好 1)' % len(hits))
    sys.exit(1)
src = hits[0]
errs = []
for fam in sorted(TOKENS):
    if TOKENS[fam] not in src:
        errs.append('真源缺族 token %r(%s)' % (TOKENS[fam], fam))
m = re.search(r'\*\*(\d+)/(\d+)\*\*', src)
if not m:
    errs.append('真源缺 `**N/M**` 计数')
elif int(m.group(2)) != len(TOKENS):
    errs.append('真源分母 %s ≠ 登记族数 %d' % (m.group(2), len(TOKENS)))
fams = sorted(r.get('family') for r in json.load(open(decl_path, encoding='utf-8')).get('deferrals', []))
if fams != sorted(TOKENS):
    errs.append('族集合 ≠ 真源映射: got %s expected %s' % (fams, sorted(TOKENS)))
if errs:
    print('; '.join(errs)); sys.exit(1)
print('4 族 == 真源 B3b-2 行(%s)' % ' / '.join(TOKENS[k] for k in sorted(TOKENS)))
PY
)"
if [[ $? -eq 0 ]]; then
    ok "真源交叉校验: $out4"
else
    ng "真源交叉校验: $out4"
fi

echo "────────────────────────────────────────"
if [[ "$fail" -eq 0 ]]; then
    echo "✅ cep_b3b2_deferral_check: $pass PASS / 0 FAIL"
    exit 0
fi
echo "❌ cep_b3b2_deferral_check: $fail FAIL"
exit 1
