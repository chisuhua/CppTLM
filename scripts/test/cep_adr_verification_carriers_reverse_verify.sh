#!/usr/bin/env bash
# scripts/test/cep_adr_verification_carriers_reverse_verify.sh
#   —— 证明 `ADR` Verification 判据载体(change `cpptlm-cep-adr-verification-carriers`)**有牙**
#
# 背景(per 仓内铁律 2):「新断言**无持久反向 fixture** ⇒ 整用例被删时仓内无任何自动化会红」。
#   本脚本做**破坏 → 必须报红 → 恢复 → 必须报绿**的闭环,作为台账 Entry 的证据。
#
# 破坏对象(**全部在影子树 / 临时目录,仓内文件零改动**):
#   · ① 宽度毒化  —— 影子树复制 `include/tlm/cep/sharer_tracker.hh` ⇒ 改 `kResponderMappedBits`
#                    4 → 8 ⇒ 编译**最小探针 TU** ⇒ 探针**运行期退出码**即判决
#   · ② 覆盖毒化  —— 影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 的一个 `### 决策 N` 段
#                    ⇒ 决策块闸(`--root` 影子树)必须红,且输出含文件名
#
# ⚠️ **⛔ 不毒化快照文件**:`scripts/test/cep_decision_block_check.py` 的 `load_snapshot()`
#    **固定读真实仓根**的 `ROOT/test/cep_decision_block_snapshot.json`;`--root` **只**重定向
#    **文档语料** ⇒ 快照毒化在不改仓内文件的前提下**不可实现**
#    (已签批契约 `openspec/specs/cep-invariant-gate/spec.md` 的「判据 SHALL 区分『文档语料』
#    与『被指物存在性』两个根」Scenario 明文如此)⇒ 覆盖证明只能走**影子树改 ADR**。
#
# ⚠️ **失败模式纪律**(per 仓内铁律:编译失败不得冒充判据失败):
#   · 探针**编译失败** ⇒ **exit 2**(环境错误),⛔ 不计入「红」
#   · 探针**运行退出码 ≠ 0** ⇒ 判红(判据失败)
#   · 探针用**运行期判定** ⇒ 退出码即判决,⛔ 不会把「编译成功但没断言」误当绿
#
# ⚠️ **影子树路径**:**自建 `mktemp -d`**,⛔ 不复用 wrapper 的固定 `/tmp/opencode/cep-decision-block`
#    (后者是**共享**路径 ⇒ 并行 / 重跑会互相覆盖)
#
# 用法:bash scripts/test/cep_adr_verification_carriers_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

BIN="build/bin/cpptlm_tests"
DECL_HDR="include/tlm/cep/sharer_tracker.hh"
SNAPSHOT="test/cep_decision_block_snapshot.json"
ADR20="docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md"
GATE="scripts/test/cep_decision_block_check.py"
# 新用例的**精确名** ⇒ 用例被删时 Catch2 报「无匹配」(rc=2)⇒ fixture 仍红(删除韧性)
CASE_NAME="CEP SharerTracker: responder 位宽常量与掩码数值锚"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep_avr.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

pass=0; fail=0

# ── 仓内零改动的自证基线(⛔ 不用 `git diff --quiet`:脏工作区会假红)─────────
declare -A BEFORE
for f in "$DECL_HDR" "$SNAPSHOT" "$ADR20"; do
    BEFORE["$f"]="$(sha256sum "$f" | cut -d' ' -f1)"
done

echo "═══ 基线(未毒化)═══"

# ① 判据本身绿(新用例精确名 ⇒ 用例被删 ⇒ rc=2 ⇒ 红)
if [ ! -x "$BIN" ]; then
    echo "  ✗ 找不到测试二进制 $BIN —— 环境错误(⛔ 不计入「红」);请先 cmake --build build" >&2
    exit 2
fi
out=$("$BIN" "$CASE_NAME" -r compact 2>&1); rc=$?
if [ "$rc" -eq 0 ]; then
    echo "  ✓ [基线] 新用例绿"
    pass=$((pass+1))
else
    echo "  ✗ [基线] 新用例应绿但红(退出码 $rc)"
    echo "$out" | tail -5 | sed 's/^/      /'
    fail=$((fail+1))
fi

# ② 决策块闸绿(真实仓)
#    ⚠️ 此处**仍显式传** `SHADOW_ROOT`(隔离本脚本自己的断言面)。
#    ⓘ 历史(2026-11-15):当时 wrapper 的默认影子路径是**固定**的
#    `/tmp/opencode/cep-decision-block` ⇒ 与 ctest 里并发跑的
#    `cep_decision_block_check` **共用同一目录** ⇒ 互相覆盖 ⇒ **假红**(单独跑绿、`-j4` 下红)。
#    该**默认值已改为 `mktemp -d`**(change `cpptlm-cep-gate-robustness` / 台账 Entry #436)
#    ⇒ 并发假红根因已从闸内消除;本行传值保留作**双保险**,不再是必需。
if SHADOW_ROOT="$TMP/shadow_wrapper" bash scripts/test/cep_decision_block_check.sh >/dev/null 2>&1; then
    echo "  ✓ [基线] 决策块闸绿"
    pass=$((pass+1))
else
    echo "  ✗ [基线] 决策块闸应绿但红"
    fail=$((fail+1))
fi

# ── 探针源(运行期判定 ⇒ 退出码即判决)──────────────────────────────────
#    ⚠️ **不**编译仓内测试二进制 ⇒ 探针**独立于** C++ 用例 ⇒ 用例被删时本 fixture 仍红。
cat > "$TMP/probe.cc" <<'PROBE_EOF'
#include "tlm/cep/sharer_tracker.hh"
#include <cstdio>
int main() {
    const bool ok = (tlm::cep::kResponderMappedBits == 4u) &&
                    (tlm::cep::kMappedResponderMask == 0xFu);
    std::printf("kResponderMappedBits=%zu mask=0x%llx\n",
                tlm::cep::kResponderMappedBits,
                (unsigned long long)tlm::cep::kMappedResponderMask);
    return ok ? 0 : 1;
}
PROBE_EOF

# compile_probe <include 前缀> ⇒ 成功 0 / 失败 2(环境错误)
compile_probe() {
    local inc="$1"
    if g++ -std=c++17 -I"$inc/include" -I"$ROOT/include" \
             -o "$TMP/probe" "$TMP/probe.cc" >"$TMP/probe_build.log" 2>&1; then
        return 0
    fi
    return 2
}

echo ""
echo "═══ 破坏 ①:宽度毒化 —— 影子树改 kResponderMappedBits 4 → 8 ═══"
mkdir -p "$TMP/shadow/include/tlm/cep"
cp "$DECL_HDR" "$TMP/shadow/include/tlm/cep/sharer_tracker.hh"
python3 - "$TMP/shadow/include/tlm/cep/sharer_tracker.hh" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = "inline constexpr std::size_t kResponderMappedBits = 4;"
assert s.count(old) == 1, "毒化锚点不唯一/不存在 —— 头文件形态已变"
open(p, 'w').write(s.replace(old, "inline constexpr std::size_t kResponderMappedBits = 8;"))
print("      已毒化(影子树): kResponderMappedBits 4 -> 8")
PYEOF
if [ $? -ne 0 ]; then
    echo "  ✗ 毒化脚本失败(环境错误)" >&2; exit 2
fi

compile_probe "$TMP/shadow"
build_rc=$?
if [ "$build_rc" -eq 2 ]; then
    echo "  ✗ 探针编译失败 —— 环境错误(exit 2);⛔ 不计入「红」" >&2
    sed 's/^/      /' "$TMP/probe_build.log" | head -10 >&2
    exit 2
fi
"$TMP/probe"; probe_rc=$?
if [ "$probe_rc" -ne 0 ]; then
    echo "  ✓ [宽度毒化] 判红(探针退出码 $probe_rc)—— 判据**有牙**"
    pass=$((pass+1))
else
    echo "  ✗ [宽度毒化] 期望红但判绿 —— 判据**无牙**"
    fail=$((fail+1))
fi

echo ""
echo "═══ 破坏 ②:覆盖毒化 —— 影子树删 ADR-CEP-20 的一个 ### 决策 N 段 ═══"
mkdir -p "$TMP/shadow/docs"
cp -r docs/adr "$TMP/shadow/docs/adr"
python3 - "$TMP/shadow/docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md" <<'PYEOF'
import re, sys
p = sys.argv[1]
s = open(p).read()
# 删 §2 章内最后一个 `### 决策 N` 段(到下一个 `^## ` 或 §2 章末为止)
m = re.search(r'(?m)^### 决策 \d+.*$', s)
assert m, "ADR-CEP-20 无 `### 决策 N` 段 —— 形态已变"
end = re.search(r'(?m)^## ', s[m.end():])
stop = m.end() + (end.start() if end else len(s) - m.end())
s2 = s[:m.start()] + s[stop:]
assert s2 != s, "删除失败"
open(p, 'w').write(s2)
print(f"      已毒化(影子树): 删掉 `{m.group(0)[:40]}`")
PYEOF
if [ $? -ne 0 ]; then
    echo "  ✗ 毒化脚本失败(环境错误)" >&2; exit 2
fi

out=$(python3 "$GATE" --root "$TMP/shadow" 2>&1); gate_rc=$?
if [ "$gate_rc" -ne 0 ] && echo "$out" | grep -q '决策块计数快照偏离' \
   && echo "$out" | grep -q 'ADR-CEP-20'; then
    echo "  ✓ [覆盖毒化] 判红(退出码 $gate_rc)+ 归因含「决策块计数快照偏离」与「ADR-CEP-20」"
    pass=$((pass+1))
else
    echo "  ✗ [覆盖毒化] 期望红但绿(退出码 $gate_rc),或归因缺失"
    echo "$out" | tail -8 | sed 's/^/      /'
    fail=$((fail+1))
fi

echo ""
echo "═══ 恢复态复核(影子树丢弃 ⇒ 真实仓不受影响)═══"
if SHADOW_ROOT="$TMP/shadow_wrapper" bash scripts/test/cep_decision_block_check.sh >/dev/null 2>&1; then
    echo "  ✓ [恢复] 决策块闸绿"
    pass=$((pass+1))
else
    echo "  ✗ [恢复] 决策块闸应绿但红"
    fail=$((fail+1))
fi

echo ""
echo "═══ 仓内零改动复核(sha256sum 前后比对)═══"
for f in "$DECL_HDR" "$SNAPSHOT" "$ADR20"; do
    after="$(sha256sum "$f" | cut -d' ' -f1)"
    if [ "$after" = "${BEFORE[$f]}" ]; then
        echo "  ✓ $f 未被改动"
        pass=$((pass+1))
    else
        echo "  ✗ $f 被改动 —— 本脚本必须只写临时目录"
        fail=$((fail+1))
    fi
done

echo ""
if [ "$fail" -eq 0 ]; then
    echo "✅ ADR Verification 判据载体反向验证全部通过($pass 项)"
    exit 0
fi
echo "❌ ADR Verification 判据载体反向验证失败:$fail 项红 / $pass 项绿"
exit 1
