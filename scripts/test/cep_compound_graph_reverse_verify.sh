#!/usr/bin/env bash
# scripts/test/cep_compound_graph_reverse_verify.sh
#   —— 证明 `ADR-CEP-22` §Verification 的 **V-22-3 / V-22-5 / V-22-7 / V-22-10** 四条判据
#      **有牙**（反向 fixture = 毒化 ⇒ 必红 + 归因 id）
#
# 背景(per 仓内铁律 2):「新断言**无持久反向 fixture** ⇒ 整用例被删时仓内无任何自动化会红」。
#   本脚本做**破坏 → 必须报红 → 恢复 → 必须报绿**的闭环,作为台账 Entry 的证据。
#
# 四条毒化(**全部在影子树 / 临时目录,仓内文件零改动**):
#   ① **聚合毒化**(V-22-3) —— 影子树改 `compound_graph_falsifier.hh`:「**任一**候选成环即证伪」
#      ⇒ `co-two-candidates`(co1 成环 + co2 可行)判定应从 `not_falsified` **翻成** `falsified`
#      ⇒ 探针**运行期退出码 ≠ 0** ⇒ 判红
#   ② **公理面毒化**(V-22-5) —— 影子树改同头文件:**域内** `rf` 也进公理面
#      ⇒ `rf-intra-domain` 的公理面出现 `:rf` 边 ⇒ 探针判红
#   ③ **派生毒化**(V-22-7) —— 影子树改同头文件:**关掉** `rfe` 派生
#      ⇒ `xdomain-sb` 的跨域环**断** ⇒ 判定应从 `falsified` **翻成** `not_falsified` ⇒ 探针判红
#      （⛔ **不可**用「关掉 `fr`」作毒化 —— 该环**不依赖** `fr`，per `ADR-CEP-22` V-22-7 明文）
#   ④ **登记毒化**(V-22-10) —— 影子树把本 ADR 从 `decision_adrs` 移除
#      ⇒ `cep_adr_structural_check.sh --root` 必红（**未登记即红**）
#
# ⚠️ **⛔ 探针独立于 C++ 测试二进制**:探针是**自带的最小 TU** ⇒ 仓内用例被删时本 fixture 仍红。
# ⚠️ **失败模式纪律**(per 仓内铁律:编译失败不得冒充判据失败):
#   · 探针**编译失败** ⇒ **exit 2**(环境错误),⛔ 不计入「红」
#   · 探针**运行退出码 ≠ 0** ⇒ 判红(判据失败)
#   · 探针用**运行期判定** ⇒ 退出码即判决,⛔ 不会把「编译成功但没断言」误当绿
# ⚠️ **影子树路径**:**自建 `mktemp -d`**,⛔ 不复用任何固定路径(⇒ 并行 / 重跑安全)。
#
# 用法:bash scripts/test/cep_compound_graph_reverse_verify.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

FALSIFIER_HDR="include/tlm/cep/compound_graph_falsifier.hh"
TRACE_HDR="include/tlm/cep/memory_model_trace.hh"
REGISTRY="test/cep_adr_structural_deferrals.json"
STRUCT_GATE="scripts/test/cep_adr_structural_check.sh"
TRACES="test/cep_memory_model_traces.json"
INSTANCES="include/tlm/cep/protocol_instances.json"

TMP="$(mktemp -d "${TMPDIR:-/tmp/opencode}/cep-cgrv.XXXXXX")"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

pass=0; fail=0
ok()   { echo "  ✓ $1"; pass=$((pass+1)); }
bad()  { echo "  ✗ $1"; fail=$((fail+1)); }

# ── 仓内零改动的自证基线(⛔ 不用 `git diff --quiet`:脏工作区会假红)────────────
declare -A BEFORE
for f in "$FALSIFIER_HDR" "$TRACE_HDR" "$REGISTRY" "$TRACES" "$INSTANCES"; do
    BEFORE["$f"]="$(sha256sum "$f" | cut -d' ' -f1)"
done

# ── 探针源(运行期判定 ⇒ 退出码即判决)──────────────────────────────────────
cat > "$TMP/probe.cc" <<'PROBE_EOF'
// 最小探针:验证 `ADR-CEP-22` V-22-3 / V-22-5 / V-22-7 三条判据的**可观测行为**。
// ⛔ 独立于仓内 Catch2 用例 ⇒ 用例被删 / 被改时本 fixture 仍能判红。
#include "tlm/cep/compound_graph_falsifier.hh"
#include "tlm/cep/memory_model_trace.hh"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {
namespace cep = tlm::cep;
namespace mm = tlm::cep::mm;

nlohmann::json read_json(const std::string& path) {
    std::ifstream in(path);
    nlohmann::json j;
    if (!in) return j;
    in >> j;
    return j;
}

std::map<std::string, nlohmann::json> traces_of(const std::string& path) {
    std::map<std::string, nlohmann::json> out;
    const auto j = read_json(path);
    if (!j.is_object()) return out;
    const auto it = j.find("traces");
    if (it == j.end() || !it->is_array()) return out;
    for (const auto& t : *it) {
        if (t.is_object()) out[t.value("id", std::string())] = t;
    }
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: probe <agg|rf-in-axiom|rfe-off> <root>\n");
        return 2;
    }
    const std::string scen = argv[1];
    const std::string root = argv[2];

    const auto traces = traces_of(root + "/test/cep_memory_model_traces.json");
    const auto labels =
        mm::load_instance_labels(read_json(root + "/include/tlm/cep/protocol_instances.json"));

    auto run = [&](const std::string& id) {
        const auto it = traces.find(id);
        if (it == traces.end()) {
            std::fprintf(stderr, "probe: missing trace '%s'\n", id.c_str());
            std::exit(2);
        }
        mm::Trace t;
        mm::ParseError err;
        if (!mm::parse_trace(it->second, t, err)) {
            std::fprintf(stderr, "probe: parse failed (%s): %s\n", err.code.c_str(),
                         err.detail.c_str());
            std::exit(2);
        }
        return cep::evaluate_compound_graph_falsification(t, labels);
    };

    if (scen == "agg") {
        // V-22-3:聚合语义 = ∃ 可行 ⇒ 未证伪 / ∀ 成环 ⇒ 证伪
        //        ⛔ 「某候选成环 ⇒ 证伪」是假阳性
        const auto v = run("co-two-candidates");
        std::printf("verdict=%d candidates=%zu\n", static_cast<int>(v.verdict),
                    v.candidate_outcomes.size());
        if (v.verdict != cep::FalsifyVerdict::NotFalsified) return 1;
        if (!v.reason.empty()) return 1;
        return 0;
    }
    if (scen == "rf-in-axiom") {
        // V-22-5:公理面边类集合 == {ppo_com, rfe, fr, ws} ⛔ **不含**域内 `rf`
        const auto v = run("rf-intra-domain");
        for (const auto& e : v.axiom_edges) {
            // ⚠️ `:rf` 是 `:rfe` 的**前缀** ⇒ 须比对**结尾**
            if (e.size() >= 3 && e.compare(e.size() - 3, 3, ":rf") == 0) {
                std::printf("axiom面含域内 rf: %s\n", e.c_str());
                return 1;
            }
        }
        std::printf("axiom_edges=%zu (无 :rf)\n", v.axiom_edges.size());
        return 0;
    }
    if (scen == "rfe-off") {
        // V-22-7:金标准违例迹 ⇒ 跨域 `rfe` 拉环 ⇒ `falsified` + 见证
        const auto v = run("xdomain-sb");
        std::printf("verdict=%d cycle=%zu\n", static_cast<int>(v.verdict), v.cycle.size());
        if (v.verdict != cep::FalsifyVerdict::Falsified) return 1;
        if (v.cycle.empty()) return 1;
        return 0;
    }
    std::fprintf(stderr, "probe: unknown scenario '%s'\n", scen.c_str());
    return 2;
}
PROBE_EOF

# compile_probe <include 前缀> ⇒ 成功 0 / 失败 2(环境错误)
compile_probe() {
    local inc="$1"
    if g++ -std=c++17 -O0 -I"$inc/include" -I"$ROOT/include" -I"$ROOT/external/json" \
             -o "$TMP/probe" "$TMP/probe.cc" >"$TMP/probe_build.log" 2>&1; then
        return 0
    fi
    return 2
}

# run_probe <scenario> ⇒ 0 = 判据**成立** / 1 = 判据**被破坏**(红) / 2 = 环境错误
run_probe() {
    "$TMP/probe" "$1" "$ROOT" >"$TMP/probe_run.log" 2>&1
    local rc=$?
    if [ "$rc" -eq 2 ]; then
        echo "      探针环境错误(退出码 2):" >&2
        sed 's/^/        /' "$TMP/probe_run.log" >&2
        return 2
    fi
    return "$rc"
}

# poison_header <影子树头文件> <python 替换脚本文件>
poison_header() {
    local shadow_hdr="$1" pyfile="$2"
    mkdir -p "$(dirname "$shadow_hdr")"
    cp "$FALSIFIER_HDR" "$shadow_hdr"
    python3 "$pyfile" "$shadow_hdr"
}

echo "═══ 基线(未毒化)═══"
mkdir -p "$TMP/base/include/tlm/cep"
cp "$FALSIFIER_HDR" "$TMP/base/include/tlm/cep/compound_graph_falsifier.hh"
cp "$TRACE_HDR" "$TMP/base/include/tlm/cep/memory_model_trace.hh"

if ! compile_probe "$TMP/base"; then
    echo "  ✗ 探针编译失败 —— 环境错误(exit 2);⛔ 不计入「红」" >&2
    sed 's/^/      /' "$TMP/probe_build.log" | head -20 >&2
    exit 2
fi
for scen in agg rf-in-axiom rfe-off; do
    if run_probe "$scen"; then
        ok "[基线] 探针场景 '$scen' 绿"
    else
        rc=$?
        if [ "$rc" -eq 2 ]; then exit 2; fi
        bad "[基线] 探针场景 '$scen' 应绿但红"
        sed 's/^/      /' "$TMP/probe_run.log"
    fi
done

# ── 毒化 ①:聚合毒化(V-22-3)——「任一候选成环即证伪」────────────────────
echo ""
echo "═══ 毒化 ①:聚合 —— 影子树改「任一候选成环即证伪」(V-22-3) ═══"
cat > "$TMP/poison_agg.py" <<'PY_EOF'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
old = """        if (cycle_ids.empty()) {  // 记录**第一个**成环候选作见证
            for (const auto idx : cyc) cycle_ids.push_back(trace.ops[idx].id);
            co_order = current_co_id;
        }
"""
new = """        if (cycle_ids.empty()) {  // 记录**第一个**成环候选作见证
            for (const auto idx : cyc) cycle_ids.push_back(trace.ops[idx].id);
            co_order = current_co_id;
            // ⛔⛔⛔ 毒化(影子树):**任一**候选成环即证伪(ADR-CEP-22 V-22-3 反向 fixture)
            v.verdict = FalsifyVerdict::Falsified;
            v.cycle = cycle_ids;
            v.co_order = co_order;
            return v;
        }
"""
assert s.count(old) == 1, "毒化锚点不唯一/不存在 —— 头文件形态已变"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("      已毒化(影子树):任一候选成环即证伪")
PY_EOF
if ! poison_header "$TMP/agg/include/tlm/cep/compound_graph_falsifier.hh" "$TMP/poison_agg.py"; then
    echo "  ✗ 毒化脚本失败(环境错误)" >&2; exit 2
fi
cp "$TRACE_HDR" "$TMP/agg/include/tlm/cep/memory_model_trace.hh"
if ! compile_probe "$TMP/agg"; then
    echo "  ✗ 探针编译失败 —— 环境错误(exit 2);⛔ 不计入「红」" >&2
    sed 's/^/      /' "$TMP/probe_build.log" | head -20 >&2
    exit 2
fi
if run_probe agg; then
    bad "[聚合毒化] 期望红但绿 —— 判据**无牙**(任一成环即证伪未被检出)"
else
    rc=$?
    if [ "$rc" -eq 2 ]; then exit 2; fi
    ok "[聚合毒化] 判红(探针退出码 $rc)—— 聚合语义判据**有牙**"
fi

# ── 毒化 ②:公理面毒化(V-22-5)—— 域内 `rf` 也进公理面 ───────────────────────
echo ""
echo "═══ 毒化 ②:公理面 —— 影子树让域内 rf 进公理面(V-22-5) ═══"
cat > "$TMP/poison_rf.py" <<'PY_EOF'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
old = """        // ⑥.2 `rfe`（跨域读自）
        for (const auto& e : rf_edges) {
            if (!e.cross_domain) continue;
            edges.push_back({e.w, e.r, "rfe"});
        }
"""
new = """        // ⑥.2 `rfe`（跨域读自）
        for (const auto& e : rf_edges) {
            // ⛔⛔⛔ 毒化(影子树):**域内** `rf` 也进公理面(ADR-CEP-22 V-22-5 反向 fixture)
            edges.push_back({e.w, e.r, e.cross_domain ? "rfe" : "rf"});
        }
"""
assert s.count(old) == 1, "毒化锚点不唯一/不存在 —— 头文件形态已变"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("      已毒化(影子树):域内 rf 进公理面")
PY_EOF
if ! poison_header "$TMP/rf/include/tlm/cep/compound_graph_falsifier.hh" "$TMP/poison_rf.py"; then
    echo "  ✗ 毒化脚本失败(环境错误)" >&2; exit 2
fi
cp "$TRACE_HDR" "$TMP/rf/include/tlm/cep/memory_model_trace.hh"
if ! compile_probe "$TMP/rf"; then
    echo "  ✗ 探针编译失败 —— 环境错误(exit 2);⛔ 不计入「红」" >&2
    sed 's/^/      /' "$TMP/probe_build.log" | head -20 >&2
    exit 2
fi
if run_probe rf-in-axiom; then
    bad "[公理面毒化] 期望红但绿 —— 判据**无牙**(域内 rf 进公理面未被检出)"
else
    rc=$?
    if [ "$rc" -eq 2 ]; then exit 2; fi
    ok "[公理面毒化] 判红(探针退出码 $rc)—— 公理面边类判据**有牙**"
fi

# ── 毒化 ③:派生毒化(V-22-7)—— 关掉 `rfe` 派生 ─────────────────────────────
echo ""
echo "═══ 毒化 ③:派生 —— 影子树关掉 rfe 派生(V-22-7) ═══"
cat > "$TMP/poison_rfe.py" <<'PY_EOF'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
old = """        // ⑥.2 `rfe`（跨域读自）
        for (const auto& e : rf_edges) {
            if (!e.cross_domain) continue;
            edges.push_back({e.w, e.r, "rfe"});
        }
"""
new = """        // ⑥.2 `rfe`（跨域读自）
        for (const auto& e : rf_edges) {
            if (!e.cross_domain) continue;
            // ⛔⛔⛔ 毒化(影子树):**关掉** `rfe` 派生(ADR-CEP-22 V-22-7 反向 fixture)
            //    ⛔ 不可用「关掉 `fr`」作毒化 —— 该环**不依赖** `fr`(V-22-7 明文)
        }
"""
assert s.count(old) == 1, "毒化锚点不唯一/不存在 —— 头文件形态已变"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("      已毒化(影子树):关掉 rfe 派生")
PY_EOF
if ! poison_header "$TMP/rfe/include/tlm/cep/compound_graph_falsifier.hh" "$TMP/poison_rfe.py"; then
    echo "  ✗ 毒化脚本失败(环境错误)" >&2; exit 2
fi
cp "$TRACE_HDR" "$TMP/rfe/include/tlm/cep/memory_model_trace.hh"
if ! compile_probe "$TMP/rfe"; then
    echo "  ✗ 探针编译失败 —— 环境错误(exit 2);⛔ 不计入「红」" >&2
    sed 's/^/      /' "$TMP/probe_build.log" | head -20 >&2
    exit 2
fi
if run_probe rfe-off; then
    bad "[派生毒化] 期望红但绿 —— 判据**无牙**(关掉 rfe 派生未被检出)"
else
    rc=$?
    if [ "$rc" -eq 2 ]; then exit 2; fi
    ok "[派生毒化] 判红(探针退出码 $rc)—— 跨域拉环判据**有牙**"
fi

# ── 毒化 ④:登记毒化(V-22-10)—— 把本 ADR 从 `decision_adrs` 移除 ───────────
echo ""
echo "═══ 毒化 ④:登记 —— 影子树把 ADR-CEP-22 移出 decision_adrs(V-22-10) ═══"
mkdir -p "$TMP/reg/test"
python3 - "$REGISTRY" "$TMP/reg/$REGISTRY" <<'PY_EOF'
import json, sys
src, dst = sys.argv[1], sys.argv[2]
reg = json.load(open(src, encoding="utf-8"))
before = len(reg.get("decision_adrs", []))
reg["decision_adrs"] = [a for a in reg.get("decision_adrs", [])
                        if "ADR-CEP-22" not in a]
after = len(reg.get("decision_adrs", []))
assert after == before - 1, f"毒化未命中(decision_adrs 未减 1):{before} -> {after}"
json.dump(reg, open(dst, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
print(f"      已毒化(影子树):decision_adrs {before} -> {after}(移除 ADR-CEP-22)")
PY_EOF
if [ $? -ne 0 ]; then
    echo "  ✗ 毒化脚本失败(环境错误)" >&2; exit 2
fi
out=$(python3 scripts/test/cep_adr_structural_check.py --root "$ROOT" --registry "$TMP/reg/$REGISTRY" 2>&1); gate_rc=$?
if [ "$gate_rc" -ne 0 ]; then
    ok "[登记毒化] 判红(退出码 $gate_rc)—— 登记三件套判据**有牙**"
    echo "$out" | grep -E "✗|未归类|未登记" | head -3 | sed 's/^/      /'
else
    bad "[登记毒化] 期望红但绿 —— 判据**无牙**(移出 decision_adrs 未被检出)"
    echo "$out" | tail -8 | sed 's/^/      /'
fi

# ── 恢复态复核(影子树丢弃 ⇒ 真实仓不受影响)──────────────────────────────
echo ""
echo "═══ 恢复态复核 ═══"
if ! compile_probe "$TMP/base"; then
    echo "  ✗ 探针编译失败 —— 环境错误(exit 2)" >&2
    sed 's/^/      /' "$TMP/probe_build.log" | head -20 >&2
    exit 2
fi
for scen in agg rf-in-axiom rfe-off; do
    if run_probe "$scen"; then
        ok "[恢复] 探针场景 '$scen' 绿"
    else
        rc=$?
        if [ "$rc" -eq 2 ]; then exit 2; fi
        bad "[恢复] 探针场景 '$scen' 应绿但红"
    fi
done

echo ""
echo "═══ 仓内零改动复核(sha256sum 前后比对)═══"
for f in "$FALSIFIER_HDR" "$TRACE_HDR" "$REGISTRY" "$TRACES" "$INSTANCES"; do
    after="$(sha256sum "$f" | cut -d' ' -f1)"
    if [ "$after" = "${BEFORE[$f]}" ]; then
        ok "$f 未被改动"
    else
        bad "$f 被改动 —— 本脚本必须只写临时目录"
    fi
done

echo ""
if [ "$fail" -eq 0 ]; then
    echo "✅ 复合图判据反向验证全部通过($pass 项)"
    exit 0
fi
echo "❌ 复合图判据反向验证失败:$fail 项红 / $pass 项绿"
exit 1
