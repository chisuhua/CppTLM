#!/usr/bin/env bash
# scripts/test/cep_decision_block_check.sh
#   —— CEP 治理判据闸的**驱动壳**(基线正控 + 决定性变异 + 还原)
#
# 用法:
#   bash scripts/test/cep_decision_block_check.sh            # 基线 + 变异(退出码 0 = 绿)
#   bash scripts/test/cep_decision_block_check.sh --list     # 只打印判据清单
#
# 检查 2 段(判据见 `scripts/test/cep_decision_block_check.py` 头注 / design.md D-1):
#   [R-d] 22 份 ADR 的 §2 决策块计数 **逐份** == 快照(D-1 四步 + D-3.2 块 ID 全匹配)
#          + 显式红①(非 ### 级)+ 显式红②(编号非法)+ 围栏危险标记
#   [R-c] 双向对账:正向(磁盘集合 ⇄ 无同号拆出件)+ 反向(新 SU 的 `### ⑥` 重述块)
#
# + **决定性变异**(⛔ 在**影子树**上运行,⛔ 不动真实仓)—— 证明闸**有牙**:
#   R-d 变异① §2 内围栏内容含行首 '## '   ⇒ 必须按 `围栏内含行首 ##` 变红
#   R-d 变异② §2 内插入 `#### 决策 5`      ⇒ 必须按 `决策块标题非 ### 级` 变红
#   R-d 变异③ §2 内插入 `### 决策 Ⅻ`       ⇒ 必须按 `决策块编号非法` 变红
#   R-d 变异④ 删掉一个 `### 决策 N` 段     ⇒ 必须按 `决策块计数快照偏离` 变红
#   R-c 变异① 删重述块一行                  ⇒ 必须按 `重述块缺项` 变红
#   R-c 变异② 影子树造 `ADR-CEP-02a-*.md`   ⇒ 必须按 `存在同号拆出件` 变红
#                                          ⇒ ⛔ 且 `cep_adr_structural_check.sh` **同时红**(双闸各报各的签名)
#   R-c 变异③ 某行「判定方式」清空          ⇒ 必须按 `引用解析缺失` 变红
#
# ⚠️ **每条变异都必须**:基线正控绿 → 破坏 ⇒ **按正确原因红(红因有签名)** → 还原 ⇒ 绿
#    (⛔ 「rc≠0」不构成红因;⛔ 不以「文件消失」才红)
#
# ⚠️ **影子树机制已抽取**为共享 helper `scripts/test/cep_test_shadow.sh`
#    ⇒ 本闸与 `cep_adr_structural_check.sh` 共用**唯一一份**实现(per Entry #361 ③ D-8)。
#    本闸**不需要** `test/cep_adr_structural_deferrals.json` ⇒ ⛔ 不复制进影子。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

CHECK="scripts/test/cep_decision_block_check.py"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-decision-block}"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

source "$(dirname "${BASH_SOURCE[0]}")/cep_test_shadow.sh"
shadow_reset() {
    cep_shadow_reset "$SHADOW"
}

if [[ "${1:-}" == "--list" ]]; then
    exec python3 "$CHECK" --list
fi

# ── 影子树 ADR 路径 ──────────────────────────────────────────────────────
SH_ADR_01="$SHADOW/docs/adr/ADR-CEP-01-fabric-collective-plane.md"
SH_ADR_02="$SHADOW/docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md"

# ── 变异构造器(全部作用于影子树)────────────────────────────────────────
mutate_fence_danger() {   # R-d 变异①:围栏内容含行首 '## '
    python3 - "$1" <<'PYEOF'
import re, sys
p = sys.argv[1]
txt = open(p, encoding="utf-8").read()
m = re.search(r"^## 2\b.*$", txt, re.M)
assert m, "变异体:未找到 §2 标题"
ins = "\n\n```text\n## 伪代码示例(变异体)\n```\n"
open(p, "w", encoding="utf-8").write(txt[: m.end()] + ins + txt[m.end():])
PYEOF
}

mutate_bad_level() {       # R-d 变异②:非 ### 级的决策标题
    python3 - "$1" <<'PYEOF'
import re, sys
p = sys.argv[1]
txt = open(p, encoding="utf-8").read()
m = re.search(r"^## 2\b.*$", txt, re.M)
assert m, "变异体:未找到 §2 标题"
ins = "\n\n#### 决策 5:变异体(非 ### 级)\n"
open(p, "w", encoding="utf-8").write(txt[: m.end()] + ins + txt[m.end():])
PYEOF
}

mutate_bad_number() {      # R-d 变异③:非 ASCII 十进制编号
    python3 - "$1" <<'PYEOF'
import re, sys
p = sys.argv[1]
txt = open(p, encoding="utf-8").read()
m = re.search(r"^## 2\b.*$", txt, re.M)
assert m, "变异体:未找到 §2 标题"
ins = "\n\n### 决策 Ⅻ:变异体(非 ASCII 十进制)\n"
open(p, "w", encoding="utf-8").write(txt[: m.end()] + ins + txt[m.end():])
PYEOF
}

mutate_drop_decision() {   # R-d 变异④:删掉一个 `### 决策 N` 段
    python3 - "$1" <<'PYEOF'
import re, sys
p = sys.argv[1]
txt = open(p, encoding="utf-8").read()
pat = re.compile(r"^### 决策 3:.*?(?=^### |^## )", re.M | re.S)
new, n = pat.subn("", txt)
assert n == 1, f"变异体:预期删掉 1 段,实得 {n}"
open(p, "w", encoding="utf-8").write(new)
PYEOF
}

mutate_drop_residual() {  # R-c 变异①:删掉重述块的一行(反向缺项)
    python3 - "$1" <<'PYEOF'
import sys
p = sys.argv[1]
lines = open(p, encoding="utf-8").read().splitlines(keepends=True)
out, n = [], 0
for line in lines:
    if line.startswith("- `ADR-CEP-02`"):
        n += 1
        continue
    out.append(line)
assert n == 1, f"变异体:预期删掉 1 行重述 bullet,实得 {n}"
open(p, "w", encoding="utf-8").write("".join(out))
PYEOF
}

mutate_blank_trigger() {   # R-c 变异③:把某行「判定方式」清空(引用解析缺失)
    python3 - "$1" <<'PYEOF'
import sys
p = sys.argv[1]
lines = open(p, encoding="utf-8").read().splitlines(keepends=True)
n = 0
for i, line in enumerate(lines):
    if line.startswith("| **T1** |"):
        parts = line.rstrip("\n").split("|")
        assert len(parts) >= 5, f"变异体:表格列数异常 {len(parts)}"
        parts[3] = " "
        lines[i] = "|".join(parts) + "\n"
        n += 1
assert n == 1, f"变异体:预期命中 1 行 T1,实得 {n}"
open(p, "w", encoding="utf-8").write("".join(lines))
PYEOF
}

# run_shadow <期望签名> <描述>:跑影子树闸 ⇒ 绿则失败;红则须命中签名
run_shadow_expect_red() {
    local expect="$1" label="$2" out rc
    out="$(python3 "$CHECK" --root "$SHADOW" 2>&1)"
    rc=$?
    if [[ $rc -ne 0 ]] && grep -q "$expect" <<< "$out"; then
        ok "$label 按签名变红:$expect"
    else
        ng "$label 未按预期变红(rc=$rc)"
        echo "$out" | sed 's/^/    /'
    fi
}

run_shadow_expect_green() {
    local label="$1" out rc
    out="$(python3 "$CHECK" --root "$SHADOW" 2>&1)"
    rc=$?
    if [[ $rc -eq 0 ]]; then
        ok "$label 还原后绿"
    else
        ng "$label 还原后仍红(rc=$rc)"
        echo "$out" | sed 's/^/    /'
    fi
}

# 双闸并验:同一影子树下,新闸与 ADR 结构闸**各报各的签名**(⛔ 不得互相掩盖)
#   · 新闸      = `cep_decision_block_check.py --root <shadow>`
#   · 结构闸    = `cep_adr_structural_check.py --root <shadow> --registry <真实仓的登记表>`
#   ⚠️ 登记表**不复制**进新闸的影子(per D-2)⇒ 结构闸用 `--registry` 显式指真实仓文件。
run_structural_expect_red() {
    local expect="$1" label="$2" out rc
    out="$(python3 scripts/test/cep_adr_structural_check.py --root "$SHADOW" \
            --registry test/cep_adr_structural_deferrals.json 2>&1)"
    rc=$?
    if [[ $rc -ne 0 ]] && grep -q "$expect" <<< "$out"; then
        ok "$label 结构闸按签名变红:$expect"
    else
        ng "$label 结构闸未按预期变红(rc=$rc)"
        echo "$out" | sed 's/^/    /'
    fi
}

echo "══ ① 基线(无变异) ══"
out="$(python3 "$CHECK" 2>&1)"
rc=$?
if [[ $rc -eq 0 ]]; then
    ok "基线通过"
    echo "$out" | sed 's/^/    /'
else
    ng "基线失败(rc=$rc)"
    echo "$out" | sed 's/^/    /'
fi

echo ""
echo "══ ② R-d 决定性变异 ══"

# 变异①:§2 内围栏内容含行首 '## ' ⇒ 显式红(围栏危险标记)
shadow_reset
mutate_fence_danger "$SH_ADR_01"
run_shadow_expect_red "围栏内含行首 ##" "R-d 变异①"
shadow_reset
run_shadow_expect_green "R-d 变异①"

# 变异②:非 `###` 级的决策标题 ⇒ 显式红①
shadow_reset
mutate_bad_level "$SH_ADR_01"
run_shadow_expect_red "决策块标题非 ### 级" "R-d 变异②"
shadow_reset
run_shadow_expect_green "R-d 变异②"

# 变异③:非 ASCII 十进制编号 ⇒ 显式红②
shadow_reset
mutate_bad_number "$SH_ADR_01"
run_shadow_expect_red "决策块编号非法" "R-d 变异③"
shadow_reset
run_shadow_expect_green "R-d 变异③"

# 变异④:删掉一个 `### 决策 N` 段 ⇒ 计数快照偏离
shadow_reset
mutate_drop_decision "$SH_ADR_02"
run_shadow_expect_red "决策块计数快照偏离" "R-d 变异④"
shadow_reset
run_shadow_expect_green "R-d 变异④"

echo ""
echo "══ ③ R-c 决定性变异 ══"

# 变异①:删掉重述块的一行 ⇒ 反向对账缺项
shadow_reset
mutate_drop_residual "$SHADOW/docs/adr/ADR-CEP-00-platform-charter-and-governance.md"
run_shadow_expect_red "重述块缺项" "R-c 变异①"
shadow_reset
run_shadow_expect_green "R-c 变异①"

# 变异②:影子树造同号拆出件 ⇒ 正向「无拆出件」不成立
#   ⚠️ 双闸并验:ADR 结构闸的 ① 分区完整性**也必须红**(两条闸各报各的签名)
shadow_reset
printf '# ADR-CEP-02a: 变异体(同号拆出件)\n' > "$SHADOW/docs/adr/ADR-CEP-02a-mutation-split.md"
run_shadow_expect_red "存在同号拆出件" "R-c 变异②"
run_structural_expect_red "未归类的 ADR 文件" "R-c 变异②"
shadow_reset
run_shadow_expect_green "R-c 变异②"

# 变异③:某行「判定方式」清空 ⇒ 引用解析缺失
shadow_reset
mutate_blank_trigger "$SHADOW/docs/adr/ADR-CEP-00-platform-charter-and-governance.md"
run_shadow_expect_red "引用解析缺失" "R-c 变异③"
shadow_reset
run_shadow_expect_green "R-c 变异③"

echo ""
echo "════════════════════════════════════════"
echo "✅ CEP 治理判据闸: $pass 项通过 / $fail 项失败"
[[ $fail -eq 0 ]] || exit 1
exit 0
