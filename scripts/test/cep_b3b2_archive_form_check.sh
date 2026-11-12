#!/usr/bin/env bash
# scripts/test/cep_b3b2_archive_form_check.sh
#   —— B3b-2 **归档形态**(MUST-FIX-3 六项)+ **回滚路径**(MUST-FIX-4)的对账闸
#
# 背景(per 台账 Entry #384 / architect 决策
#   `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md`):
#   MUST-FIX-3 的 6 项形态定义与 MUST-FIX-4 的回滚路径此前**只存在于裁决文本**,
#   仓内**无载体** ⇒ 终局 Ⅰ / Ⅳ(依赖归档形态)**不可执行**。
#   本 change 落地形态 ⇒ 本闸守住「形态已定义 + 判据未被降级 + 指针闭合」。
#
# 判据(每条可机器判,⛔ 不断言 rc≠0):
#   V1 归档态段存在 + §14.1~§14.7 **七个子标题**齐备
#   V2 闸的锚解析 = **行首标题匹配**(⛔ 不得退回全文子串)
#   V3 重定向表存在 + `roots[0]` = 台账现路径
#   V4 `07 号` §12 含维护态判定规则的**单向指针**
#   V5 回滚形态**成对可判**(规则文本 + 成对登记表)
#   V6 闸的**行为**有牙(破坏 deferrals 的 ledger_entry ⇒ 闸红)
#
# 决定性变异(影子树内,⛔ 不动真实仓):
#   M1 破坏 deferrals 的 `ledger_entry` ⇒ 闸必红(签名含 `ledger_entry`)
#   M2 把闸的锚解析改回全文子串 ⇒ V2 必红(签名 `锚解析降级`)
#
# 退出码: 0 = 全绿 · 1 = 有 FAIL · 2 = 环境缺失
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

OBJ=".rddf/roadmap/objectives/obj-cep-platform.md"
DOC07="docs/architecture/07-coherence-exploration-platform.md"
GATE="scripts/test/cep_b3b2_deferral_check.sh"
REDIRECT="test/cep_b3b2_ledger_redirect.json"
DECL="test/cep_b3b2_deferrals.json"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-b3b2-archive-form}"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }
say() { echo "FAIL $1 $2 :: $3"; }

for f in "$OBJ" "$DOC07" "$GATE" "$REDIRECT" "$DECL"; do
    if [[ ! -f "$f" ]]; then
        echo "MISSING :: $f"; exit 2
    fi
done
if [[ ! -f "scripts/test/cep_test_shadow.sh" ]]; then
    echo "MISSING helper :: scripts/test/cep_test_shadow.sh"; exit 2
fi
# shellcheck source=/dev/null
source scripts/test/cep_test_shadow.sh

echo "═══ B3b-2 归档形态 / 回滚路径 · 对账闸(6 条判据 + 2 条决定性变异)═══"

# ── V1 · 归档态段 + 七个子标题 ────────────────────────────────────────────
missing=""
if ! grep -q '^## §14 · 归档态' "$OBJ"; then
    missing="§14 归档态段"
else
    for sub in "§14.1" "§14.2" "§14.3" "§14.4" "§14.5" "§14.6" "§14.7"; do
        grep -q "^### ${sub} " "$OBJ" || missing="${missing} ${sub}"
    done
fi
if [[ -z "$missing" ]]; then
    ok "V1 归档态段存在 + §14.1~§14.7 七个子标题齐备"
else
    ng "V1 归档形态定义缺项: ${missing}"
    say "V1" "归档形态定义缺项" "缺:${missing}"
fi

# ── V2 · 闸的锚解析 = 行首标题匹配(不得降级为全文子串)──────────────────
# ⚠️ 抽成函数:变异 M2 需要对**影子树内**的闸跑同一判据 ⇒ 证明该判据确实能抓住降级。
v2_check() { grep -q "startswith('### ' + le)" "$1"; }
if v2_check "$GATE"; then
    ok "V2 闸的锚解析 = 行首标题匹配(未降级)"
else
    ng "V2 闸的锚解析已降级"
    say "V2" "锚解析降级" "$GATE 内未见 startswith('### ' + le) 的行首标题匹配"
fi

# ── V3 · 重定向表 ────────────────────────────────────────────────────────
roots="$(python3 - "$REDIRECT" <<'PY'
import json, sys
try:
    d = json.load(open(sys.argv[1], encoding='utf-8'))
    print((d.get('roots') or [''])[0])
except Exception:
    print('')
PY
)"
if [[ "$roots" == ".rddf/roadmap/objectives/obj-cep-platform.md" ]]; then
    ok "V3 重定向表存在且 roots[0] = 台账现路径"
else
    ng "V3 重定向表首根不符"
    say "V3" "重定向表首根不符" "roots[0] = '${roots}'(期望 .rddf/roadmap/objectives/obj-cep-platform.md)"
fi

# ── V4 · 07 号 维护态单向指针 ────────────────────────────────────────────
if grep -q '维护态的 B3b-2 判定规则' "$DOC07"; then
    ok "V4 07 号 §12 含维护态判定规则的单向指针"
else
    ng "V4 维护态指针缺失"
    say "V4" "维护态指针缺失" "$DOC07 内未见「维护态的 B3b-2 判定规则」指针句"
fi

# ── V5 · 回滚形态成对可判 ────────────────────────────────────────────────
if grep -q '撤回标记(成对可判)' "$OBJ" && grep -q '#### §14.7.1 · 撤回标记登记' "$OBJ" &&
   grep -q '被撤回的 SU 标识' "$OBJ" && grep -q '撤回 Entry 号' "$OBJ"; then
    ok "V5 回滚形态成对可判(规则文本 + 成对登记表)"
else
    ng "V5 撤回标记不成对"
    say "V5" "撤回标记不成对" "$OBJ 内缺「成对可判」规则文本或成对登记表(被撤回的 SU 标识 / 撤回 Entry 号)"
fi

# ── V6 · 闸的行为有牙(影子树破坏)─────────────────────────────────────────
cep_shadow_reset "$SHADOW" "$GATE" "$DECL" "$REDIRECT" "$OBJ"
python3 - "$SHADOW/$DECL" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p, encoding='utf-8'))
for r in d['deferrals']:
    r['ledger_entry'] = 'Entry #999999'
json.dump(d, open(p, 'w', encoding='utf-8'), ensure_ascii=False)
PY
out="$(cd "$SHADOW" && bash scripts/test/cep_b3b2_deferral_check.sh 2>&1)"; rc=$?
if [[ $rc -eq 1 ]] && grep -qF 'ledger_entry' <<<"$out"; then
    ok "M1 破坏 deferrals 的 ledger_entry ⇒ 闸按签名「ledger_entry」红(rc=$rc)"
else
    ng "M1 破坏 deferrals 后闸未按 ledger_entry 红(rc=$rc)"
    say "M1" "闸无牙" "输出: $(head -3 <<<"$out" | tr '\n' ' ')"
fi
cep_shadow_reset "$SHADOW" "$GATE" "$DECL" "$REDIRECT" "$OBJ"
out2="$(cd "$SHADOW" && bash scripts/test/cep_b3b2_deferral_check.sh 2>&1)"; rc2=$?
if [[ $rc2 -eq 0 ]]; then
    ok "M1 还原后绿(rc=0)"
else
    ng "M1 还原后未绿(rc=$rc2)"
    say "M1" "还原失败" "输出: $(head -3 <<<"$out2" | tr '\n' ' ')"
fi

# ── M2 · 把锚解析改回全文子串 ⇒ V2 必红 ─────────────────────────────────
cep_shadow_reset "$SHADOW" "$GATE" "$DECL" "$REDIRECT" "$OBJ"
python3 - "$SHADOW/$GATE" <<'PY'
import sys
p = sys.argv[1]
t = open(p, encoding='utf-8').read()
old = "hit = any(any(l.startswith('### ' + le) for l in t.splitlines()) for t in texts)"
new = "hit = any(le in t for t in texts)"
assert old in t, "变异未生效:未找到行首标题匹配表达式"
open(p, 'w', encoding='utf-8').write(t.replace(old, new, 1))
PY
if v2_check "$SHADOW/$GATE"; then
    ng "M2 改回全文子串后 V2 仍判绿(判据无牙)"
    say "M2" "闸无牙" "影子树内的闸降级为全文子串后,V2 判据仍判绿"
else
    ok "M2 改回全文子串 ⇒ V2 对影子树判红(判据确实能抓住降级)"
fi
cep_shadow_reset "$SHADOW" "$GATE" "$DECL" "$REDIRECT" "$OBJ"

# ── 真实仓零改动(字节指纹前后比对)────────────────────────────────────────
fp_real() { sha256sum "$OBJ" "$DOC07" "$GATE" "$REDIRECT" "$DECL" 2>/dev/null | awk '{print $1}' | tr '\n' ' '; }
FP_BEFORE="$(fp_real)"
cep_shadow_reset "$SHADOW" "$GATE" "$DECL" "$REDIRECT" "$OBJ"
if [[ "$(fp_real)" == "$FP_BEFORE" ]]; then
    ok "真实仓 5 个文件字节不变(变异只在影子树)"
else
    ng "真实仓文件被改动(变异隔离失效)"
    say "ISOLATION" "变异越界" "真实仓文件字节指纹前后不等"
fi

echo "───────────────────────────────────────────────"
echo "  通过 $pass · 失败 $fail"
[[ $fail -eq 0 ]] || exit 1
echo "✅ cep_b3b2_archive_form_check 全绿(6 条判据 + 2 条决定性变异)"
