#!/usr/bin/env bash
# scripts/test/cep_adr_structural_check.sh
#   —— ADR **结构闸**(per oracle 2026-11-09 裁决 C-1 / 台账 Entry #307 §③ T1)
#
# 用法:
#   bash scripts/test/cep_adr_structural_check.sh            # 校验 + 决定性变异(退出码 0 = 绿)
#   bash scripts/test/cep_adr_structural_check.sh --list     # 只打印清单
#
# 检查 4 组(判据见 `scripts/test/cep_adr_structural_check.py` 头注):
#   ① 分区完整性 —— 磁盘 `ADR-CEP-*.md` == `decision_adrs` ∪ `excluded`
#   ② 未登记即红 —— 不合规 ADR 必须落在 `waivers` / `pending_signing`
#   ③ 豁免锚点存活 —— 永久豁免的 `evidence_anchor` 必须仍指向 Verification 行(反后门)
#   ④ 登记表健康 —— reason / trigger_condition / owner / ledger_entry 齐备
#
# + **3 个决定性变异**(⛔ 在**影子树**上运行,⛔ 不动真实仓)—— 证明闸**有牙**:
#   变异① 出现**未归类**的 ADR 文件        ⇒ ① 必须红
#   变异② 豁免的 `evidence_anchor` 指向死行 ⇒ ③ 必须红(豁免不得成为后门)
#   变异③ 把一份不合规 ADR 的登记删掉        ⇒ ② 必须红(未登记即红)
#
# ⚠️ **本闸不改任何 ADR**:它只**登记 + 锁住**例外 ⇒ 缺口靠**补章**收敛(per 台账 T4 签发批次)
# ⚠️ **登记表** = `test/cep_adr_structural_deferrals.json`
#    (⛔ 与 `test/cep_b3b2_deferrals.json` —— B3b-2 的**冻结件** —— 无关,不得混用)
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

CHECK="scripts/test/cep_adr_structural_check.py"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-adr-structural}"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

if [[ "${1:-}" == "--list" ]]; then
    exec python3 "$CHECK" --list
fi

echo "══ ① 基线(无变异) ══"
out="$(python3 "$CHECK" 2>&1)"
rc=$?
if [[ $rc -eq 0 ]]; then
    ok "基线通过"
else
    ng "基线失败(rc=$rc)"
    echo "$out" | sed 's/^/    /'
fi

# ── 影子树准备(⛔ 只复制,不动真实仓)────────────────────────────────
shadow_reset() {
    rm -rf "$SHADOW"
    mkdir -p "$SHADOW/docs" "$SHADOW/test"
    cp -r docs/adr "$SHADOW/docs/adr"
    cp test/cep_adr_structural_deferrals.json "$SHADOW/test/"
}

echo ""
echo "══ ② 决定性变异 ══"

# 变异①:出现一份**未归类**的 ADR 文件 ⇒ ① 分区完整性必须红
shadow_reset
printf '# ADR-CEP-99: 变异体(未归类)\n' > "$SHADOW/docs/adr/ADR-CEP-99-mutation-unclassified.md"
out="$(python3 "$CHECK" --root "$SHADOW" 2>&1)"
rc=$?
if [[ $rc -ne 0 ]] && grep -q "未归类的 ADR 文件" <<< "$out"; then
    ok "变异①按签名变红:未归类的 ADR 文件"
else
    ng "变异①未按预期变红(rc=$rc)"
    echo "$out" | sed 's/^/    /'
fi

# 变异②:豁免的 evidence_anchor 指向**死行** ⇒ ③ 锚点存活必须红
shadow_reset
python3 - "$SHADOW/test/cep_adr_structural_deferrals.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
reg = json.load(open(p, encoding='utf-8'))
reg['waivers'][0]['evidence_anchor'] = [1]   # 第 1 行不是 Verification 标题 ⇒ 锚点失效
json.dump(reg, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PYEOF
out="$(python3 "$CHECK" --root "$SHADOW" 2>&1)"
rc=$?
if [[ $rc -ne 0 ]] && grep -q "豁免锚点已失效" <<< "$out"; then
    ok "变异②按签名变红:豁免锚点已失效"
else
    ng "变异②未按预期变红(rc=$rc)"
    echo "$out" | sed 's/^/    /'
fi

# 变异③:删掉一份不合规 ADR 的登记 ⇒ ② 未登记即红必须红
shadow_reset
python3 - "$SHADOW/test/cep_adr_structural_deferrals.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
reg = json.load(open(p, encoding='utf-8'))
reg['pending_signing'] = reg['pending_signing'][1:]   # 少登记一份 ⇒ 该 ADR 变成「未登记」
json.dump(reg, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
PYEOF
out="$(python3 "$CHECK" --root "$SHADOW" 2>&1)"
rc=$?
if [[ $rc -ne 0 ]] && grep -q "未登记例外" <<< "$out"; then
    ok "变异③按签名变红:不合规且未登记例外"
else
    ng "变异③未按预期变红(rc=$rc)"
    echo "$out" | sed 's/^/    /'
fi

echo ""
echo "────────────────────────────────────────"
echo "✅ ADR 结构闸: $pass 项通过 / $fail 项失败"
[[ $fail -eq 0 ]] || exit 1
exit 0
