#!/usr/bin/env bash
# scripts/test/cep_flow_support_reverse_verify.sh
#   —— 证明 `flow_support` 值层判据(change `cpptlm-cep-flow-support-assertions`)**有牙**
#
# 背景(per 仓内铁律 2):「新断言**无持久反向 fixture** ⇒ 整用例被删时仓内无任何自动化会红」。
#   本脚本做**破坏 → 必须报红 → 恢复 → 必须报绿**的闭环,作为台账 Entry 的证据。
#
# 破坏对象(**两份副本,仓内文件零改动**):
#   · 声明源 `include/tlm/cep/protocol_instances.json` 的**副本**
#   · 期望表 `test/cep_flow_support_expected.json` 的**副本**
#   ⇒ 通过 env 覆盖(`CEP_FLOW_SUPPORT_DECL` / `CEP_FLOW_SUPPORT_EXPECTED`)指向副本
#   ⇒ 本脚本**只写自己的临时目录** ⇒ **可安全进 ctest 并行运行**
#     (对比:`cep_decl_reverse_verify.sh` 临时改写仓内 `fabric.json` ⇒ 刻意不注册 ctest)
#
# 失败模式纪律(per `D-η` ③ 的既有口径差异):红 ⇒ 必须**非零退出**;
#   ⛔ **不接受**「编译失败」冒充「判据失败」—— 本脚本不重编译,红只可能来自判据本身;
#   若二进制缺失 ⇒ 判**环境错误**(exit 2),⛔ 不计入「红」。
#
# 用法:bash scripts/test/cep_flow_support_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

BIN="build/bin/cpptlm_tests"
DECL="include/tlm/cep/protocol_instances.json"
EXPECTED="test/cep_flow_support_expected.json"
FILTER="[cep][flow-support]"

TMP="${TMPDIR:-/tmp/opencode}/cep_flow_support_rv.$$"
rm -rf "$TMP"; mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$BIN" ]; then
    echo "  ✗ 找不到测试二进制 $BIN —— 环境错误(⛔ 不计入「红」);请先 cmake --build build" >&2
    exit 2
fi

pass=0; fail=0
# run <label> <expect: RED|GREEN> <decl_path> <expected_path>
run() {
    local label="$1" expect="$2" dpath="$3" epath="$4" out rc
    out=$(CEP_FLOW_SUPPORT_DECL="$dpath" CEP_FLOW_SUPPORT_EXPECTED="$epath" \
          "$BIN" "$FILTER" -r compact 2>&1)
    rc=$?
    if [ "$expect" = "RED" ]; then
        if [ "$rc" -ne 0 ]; then
            echo "  ✓ [$label] 判红(退出码 $rc)"
            pass=$((pass+1))
        else
            echo "  ✗ [$label] 期望红但判绿 —— 判据**无牙**"
            echo "$out" | tail -5 | sed 's/^/      /'
            fail=$((fail+1))
        fi
    else
        if [ "$rc" -eq 0 ]; then
            echo "  ✓ [$label] 判绿"
            pass=$((pass+1))
        else
            echo "  ✗ [$label] 期望绿但判红(退出码 $rc)"
            echo "$out" | tail -8 | sed 's/^/      /'
            fail=$((fail+1))
        fi
    fi
}

# ── 副本准备 ────────────────────────────────────────────────────────────
cp "$DECL" "$TMP/decl_ok.json"
cp "$DECL" "$TMP/decl_bad.json"
cp "$EXPECTED" "$TMP/exp_ok.json"
cp "$EXPECTED" "$TMP/exp_bad.json"

echo "═══ 破坏 1:声明源副本 —— repo_owned 某实例的 1 个 flow_support 布尔取反 ═══"
python3 - "$TMP/decl_bad.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
ro = d["repo_owned_instance_ids"]
by = {i["id"]: i for i in d["instances"]}
hit = None
for rid in ro:
    for k, v in by[rid]["flow_support"].items():
        if v is False:
            by[rid]["flow_support"][k] = True
            hit = (rid, k)
            break
    if hit:
        break
assert hit, "没有任何 repo_owned 实例含 false 格可翻 —— 期望表与声明源已不可区分"
json.dump(d, open(p, "w"), ensure_ascii=False, indent=2)
print(f"      已毒化: {hit[0]} / {hit[1]} : false -> true")
PYEOF
[ $? -eq 0 ] || { echo "  ✗ 毒化脚本失败" >&2; exit 2; }

echo "═══ 破坏 2:期望表副本 —— **另一个**实例的 1 个 flow_support 布尔取反 ═══"
# ⚠️ **必须毒化与破坏 1 不同的实例**:两侧毒化**同一格** ⇒ 声明源与期望表重新一致 ⇒
#    判据**理应**判绿(那不是无牙,而是两侧同步漂移)⇒ 故「同时漂移」这条 fixture
#    必须让两侧指向**不同**的格,才能验证「双侧漂移 ⇒ 判红」。
python3 - "$TMP/exp_bad.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
eligible = []
for rid, inst in d["instances"].items():
    for k, v in inst["flow_support"].items():
        if v is False:
            eligible.append((rid, k))
            break
assert len(eligible) >= 2, f"期望表里可翻 false 格的实例不足 2 个({len(eligible)})⇒ 无法构造「双侧不同格」fixture"
rid, k = eligible[1]          # ⛔ 跳过第 1 个 ⇒ 与破坏 1 的实例不同
d["instances"][rid]["flow_support"][k] = True
json.dump(d, open(p, "w"), ensure_ascii=False, indent=2)
print(f"      已毒化: {rid} / {k} : false -> true(⛔ 与破坏 1 的实例不同)")
PYEOF
[ $? -eq 0 ] || { echo "  ✗ 毒化脚本失败" >&2; exit 2; }

echo ""
echo "═══ 判据闭环(4 条)═══"
run "未毒化(声明源副本 = 原样)"      GREEN "$TMP/decl_ok.json"  "$TMP/exp_ok.json"
run "声明源副本漂移 1 格"            RED   "$TMP/decl_bad.json" "$TMP/exp_ok.json"
run "期望表副本漂移 1 格"            RED   "$TMP/decl_ok.json"  "$TMP/exp_bad.json"
run "声明源 + 期望表同时漂移"       RED   "$TMP/decl_bad.json" "$TMP/exp_bad.json"

echo ""
echo "═══ 仓内零改动复核 ═══"
for f in "$DECL" "$EXPECTED"; do
    if git diff --quiet -- "$f" 2>/dev/null; then
        echo "  ✓ $f 未被改动"
        pass=$((pass+1))
    else
        echo "  ✗ $f 被改动 —— 本脚本必须只写临时目录"
        fail=$((fail+1))
    fi
done

echo ""
if [ "$fail" -eq 0 ]; then
    echo "✅ flow_support 值层判据反向验证全部通过($pass 项)"
    exit 0
fi
echo "❌ flow_support 值层判据反向验证失败:$fail 项红 / $pass 项绿"
exit 1
