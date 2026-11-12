#!/usr/bin/env bash
# scripts/test/cep_iag_gate_reconcile_check.sh
#   —— IAG 族(实例级装配闸)**执行臂**的对账闸 + 决定性变异
#
# 背景(per 台账 Entry #379 / architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md`):
#   IAG 族(`[IAG-01]`~`[IAG-08]`)此前**只被测试侧调用** ⇒ 装配路径**不拦** ⇒ 「只测不拦」。
#   本 change 补上**两臂**:① 装配路径臂(`src/core/module_factory.cc`)② 离线 CLI 臂
#   (`examples/cep_instance_assembly_gate.cc`)⇒ 两臂**共用同一份**判据实现与加载规则。
#
# 本闸的判据(每条可机器判,⛔ 不断言 rc≠0):
#   C1 装配路径臂存在(调用 `run_instance_assembly_gate`)
#   C2 离线 CLI 臂存在(调用 `run_instance_assembly_gate`)
#   C3 两臂**共用加载器**(`load_instance_assembly_inputs`)⇒ 禁双实现
#   C4 **判据本体零 diff**(`instance_assembly_gate.hh` 未被改口径)
#   C5 离线 CLI 已注册(`add_executable` + `add_test`)
#   C6 `validate_topology` 的 **IAG 第二段**在命令面内,且 CEP config 清单 == 磁盘实数
#   C7 装配路径臂位于**模块创建之前**(行序早于 hierarchy 解析 ⇒ 失败时零副作用)
#
# 决定性变异(影子树 + 临时 config,⛔ 不动真实仓):
#   M1 破坏 config(bridge → 未声明实例)⇒ CLI 必红且**按签名** `[IAG-02]`
#   M2 摘掉 `validate_topology` 的 IAG 段 ⇒ C6 必红
#
# 退出码: 0 = 全绿 · 1 = 有 FAIL · 2 = 环境缺失(CLI 未编译 / 影子树 helper 缺失)
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-build}"
CLI="$BUILD_DIR/bin/examples/cep_instance_assembly_gate"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-iag-gate-reconcile}"

MF="src/core/module_factory.cc"
CLI_SRC="examples/cep_instance_assembly_gate.cc"
LOADER="include/tlm/cep/instance_assembly_loader.hh"
GATE_HH="include/tlm/cep/instance_assembly_gate.hh"
CMK_SCRIPTS="scripts/CMakeLists.txt"
CMK_EXAMPLES="examples/CMakeLists.txt"

pass=0
fail=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }
say() { echo "FAIL $1 $2 :: $3"; }

if [[ ! -x "$CLI" ]]; then
    echo "MISSING CLI :: $CLI(未编译 —— 请先 cmake --build $BUILD_DIR)"; exit 2
fi
if [[ ! -f "scripts/test/cep_test_shadow.sh" ]]; then
    echo "MISSING helper :: scripts/test/cep_test_shadow.sh"; exit 2
fi
# shellcheck source=/dev/null
source scripts/test/cep_test_shadow.sh

echo "═══ IAG 执行臂 · 对账闸(7 条判据 + 2 条决定性变异)═══"

# 真实仓 6 个文件的**字节指纹**(变异前后比对)
fp_real() { sha256sum "$MF" "$CLI_SRC" "$LOADER" "$GATE_HH" "$CMK_SCRIPTS" "$CMK_EXAMPLES" 2>/dev/null | awk '{print $1}' | tr '\n' ' '; }
FP_BEFORE="$(fp_real)"

# ── C1 / C2 / C3 · 两臂存在且共用实现与加载器 ────────────────────────────
for pair in "C1|$MF|装配路径臂" "C2|$CLI_SRC|离线 CLI 臂"; do
    cid="${pair%%|*}"; rest="${pair#*|}"; f="${rest%%|*}"; label="${rest#*|}"
    if grep -q 'run_instance_assembly_gate' "$f" 2>/dev/null; then
        ok "$cid $label 调用「run_instance_assembly_gate」"
    else
        ng "$cid $label 未调用 `run_instance_assembly_gate`"
        say "$cid" "执行臂缺失" "$label($f)内无 run_instance_assembly_gate 调用"
    fi
done

if grep -q 'load_instance_assembly_inputs' "$MF" && grep -q 'load_instance_assembly_inputs' "$CLI_SRC"; then
    ok "C3 两臂共用加载器「load_instance_assembly_inputs」"
else
    ng "C3 两臂未共用加载器"
    say "C3" "双实现" "装配臂/CLI 臂未同时使用 load_instance_assembly_inputs"
fi

# ── C4 · 判据本体零 diff ────────────────────────────────────────────────
if git diff --quiet -- "$GATE_HH" 2>/dev/null; then
    ok "C4 判据本体「instance_assembly_gate.hh」零 diff"
else
    ng "C4 判据本体被改动"
    say "C4" "口径漂移" "$GATE_HH 有 diff(本 change 只补执行臂,不改判据口径)"
fi

# ── C5 · CLI 注册 ───────────────────────────────────────────────────────
if grep -q 'add_executable(cep_instance_assembly_gate' "$CMK_EXAMPLES" &&
   grep -q 'add_test(NAME cep_instance_assembly_gate' "$CMK_EXAMPLES"; then
    ok "C5 离线 CLI 已注册(add_executable + add_test)"
else
    ng "C5 离线 CLI 未注册"
    say "C5" "注册缺失" "$CMK_EXAMPLES 内缺 add_executable/add_test(cep_instance_assembly_gate)"
fi

# ── C6 · validate_topology 的 IAG 第二段 + CEP config 清单 ──────────────
check_c6() {
    local f="$1"
    # ① 组合 target 的命令面必须含 IAG 段
    python3 - "$f" <<'PY'
import re, sys
t = open(sys.argv[1], encoding='utf-8').read()
m = re.search(r'add_custom_target\(\s*validate_topology(.*?)\n\s*\)', t, re.S)
if not m:
    print("NO_TARGET"); sys.exit(1)
body = m.group(1)
if '${CPPTLM_CEP_VALIDATION_COMMANDS}' not in body:
    print("NO_IAG_SEGMENT"); sys.exit(1)
cm = re.search(r'set\(CPPTLM_CEP_VALIDATION_CONFIGS(.*?)\)', t, re.S)
if not cm:
    print("NO_CONFIG_LIST"); sys.exit(1)
n = len(re.findall(r'configs/cep_[A-Za-z0-9_]+\.json', cm.group(1)))
print(f"COUNT={n}")
PY
}
c6_out="$(check_c6 "$CMK_SCRIPTS")"
disk_n=$(ls -1 configs/cep_*.json | wc -l)
if [[ "$c6_out" == COUNT=* ]]; then
    declared="${c6_out#COUNT=}"
    if [[ "$declared" == "$disk_n" ]]; then
        ok "C6 IAG 第二段在命令面内 + CEP config 清单($declared) == 磁盘实数($disk_n)"
    else
        ng "C6 CEP config 清单与磁盘不符"
        say "C6" "登记缺口" "清单声明 $declared 份,磁盘实数 $disk_n 份"
    fi
else
    ng "C6 validate_topology 缺 IAG 段"
    say "C6" "挂点失效" "检查结果: $c6_out"
fi

# ── C7 · 装配路径臂位于模块创建之前 ────────────────────────────────────
arm_line=$(grep -n 'run_instance_assembly_gate' "$MF" | head -1 | cut -d: -f1)
create_line=$(grep -n '^    // 2\. 创建所有模块实例' "$MF" | head -1 | cut -d: -f1)
if [[ -n "${arm_line:-}" && -n "${create_line:-}" && "$arm_line" -lt "$create_line" ]]; then
    ok "C7 装配路径臂(行 $arm_line)早于模块创建段(行 $create_line)⇒ 失败时零副作用"
else
    ng "C7 装配路径臂位置不早于模块创建"
    say "C7" "位置错误" "臂行号=${arm_line:-?} vs 模块创建段=${create_line:-?}"
fi

# ── M1 · 决定性变异(行为级):破坏 config ⇒ CLI 按 [IAG-02] 红 ───────────
BROKEN="$(mktemp -t iag_broken_XXXXXX.json)"
python3 - "$BROKEN" <<'PY'
import json, sys
c = json.load(open('configs/cep_chi_rnf_pe_minimal.json', encoding='utf-8'))
c['cep']['bridges'][0]['protocol_instance'] = 'not_declared_xyz'
json.dump(c, open(sys.argv[1], 'w', encoding='utf-8'), ensure_ascii=False)
PY
out="$("$CLI" "$BROKEN" 2>&1)"; rc=$?
if [[ $rc -eq 1 ]] && grep -qF '[IAG-02]' <<<"$out"; then
    ok "M1 破坏 config ⇒ CLI 按签名「[IAG-02]」红(rc=$rc)"
else
    ng "M1 破坏 config 未按 [IAG-02] 红(rc=$rc)"
    say "M1" "闸无牙" "输出: $(head -3 <<<"$out" | tr '\n' ' ')"
fi
out2="$("$CLI" configs/cep_chi_rnf_pe_minimal.json 2>&1)"; rc2=$?
if [[ $rc2 -eq 0 ]]; then
    ok "M1 还原后绿(rc=0)"
else
    ng "M1 还原后未绿(rc=$rc2)"
    say "M1" "还原失败" "输出: $(head -3 <<<"$out2" | tr '\n' ' ')"
fi
rm -f "$BROKEN"

# ── M2 · 决定性变异(源码级 · 影子树):摘掉 IAG 段 ⇒ C6 红 ───────────────
cep_shadow_reset "$SHADOW" "$CMK_SCRIPTS" "$CMK_EXAMPLES" "$MF" "$CLI_SRC" "$LOADER" "$GATE_HH"
python3 - "$SHADOW/$CMK_SCRIPTS" <<'PY'
import re, sys
p = sys.argv[1]
t = open(p, encoding='utf-8').read()
t2 = t.replace('        ${CPPTLM_CEP_VALIDATION_COMMANDS}\n', '')
assert t2 != t, "变异未生效:未找到 IAG 段行"
open(p, 'w', encoding='utf-8').write(t2)
PY
m2_out="$(check_c6 "$SHADOW/$CMK_SCRIPTS")"
if [[ "$m2_out" == "NO_IAG_SEGMENT" ]]; then
    ok "M2 摘掉 IAG 段 ⇒ 对账判据红(NO_IAG_SEGMENT)"
else
    ng "M2 摘掉 IAG 段后对账判据未红"
    say "M2" "闸无牙" "检查结果: $m2_out"
fi
cep_shadow_reset "$SHADOW" "$CMK_SCRIPTS" "$CMK_EXAMPLES" "$MF" "$CLI_SRC" "$LOADER" "$GATE_HH"
m2_out2="$(check_c6 "$SHADOW/$CMK_SCRIPTS")"
if [[ "$m2_out2" == COUNT=* ]]; then
    ok "M2 还原后绿"
else
    ng "M2 还原后未绿"
    say "M2" "还原失败" "检查结果: $m2_out2"
fi

# ── 真实仓零改动(**字节指纹前后比对**)────────────────────────────────────
# ⚠️ 不得用「工作区必须干净」作判据 —— 实施期本就有未提交改动 ⇒ 改用指纹前后比对。
if [[ "$(fp_real)" == "$FP_BEFORE" ]]; then
    ok "真实仓 6 个文件字节不变(变异只在影子树 / 临时文件)"
else
    ng "真实仓文件被改动(变异隔离失效)"
    say "ISOLATION" "变异越界" "真实仓文件字节指纹前后不等"
fi

echo "───────────────────────────────────────────────"
echo "  通过 $pass · 失败 $fail"
[[ $fail -eq 0 ]] || exit 1
echo "✅ cep_iag_gate_reconcile_check 全绿(7 条判据 + 2 条决定性变异)"
