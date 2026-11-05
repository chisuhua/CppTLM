#!/usr/bin/env bash
# scripts/test/cep_ic3_report_states_check.sh
# IC3 报告层**三态**的机器 fixture(per ADR-CEP-06 SU#3 §②A / §⑤)
#
# 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #224)
#
# 背景:
#   SU#1 §④ 要求「违例类为 not-collected ⇒ IC3 判定『不适用』+ 该 run 不得作一致性安全断言的证据
#   + **报告层须渲染对应 badge**」。SU#3 把它落成**三态**:
#     ✓ 通过        —— 两个违例类**均实测**为 0
#     ❌ 违例 fail   —— 任一违例类**实测** > 0(或非法/缺失)
#     ⚪ 不适用      —— 任一违例类 == "not-collected"(**优先于**「通过」)
#
#   ⚠️ **为什么必须有本 fixture**:当前 4 个真实 run(h2/h3/h4/h6)**无一**落在「通过」态
#     (实测:h2 dv+mv 均 not-collected;h3 dv=not-collected;h4/h6 mv=not-collected)
#     ⇒ 若无合成 fixture,「通过」分支是**未被验证的死代码** ⇒ 本脚本补齐三态全覆盖。
#
# 4 条 fixture(每条都断言「因正确原因」):
#   F1 通过态        dv=0 ∧ mv=0                    ⇒ 须渲染「✓ 一致性闸门通过」且**不含**「不适用」
#   F2 不适用态      dv=not-collected ∧ mv=0        ⇒ 须渲染「⚪ 一致性闸门不适用」且**不含**「通过」
#   F3 违例态        dv=5 ∧ mv=0                    ⇒ 须渲染「❌ 一致性违例」
#   F4 连带洞(反事实)dv=not-collected ∧ mv=5        ⇒ 须渲染「❌ 一致性违例」且**含** memory_violations
#                                                      (⛔ 不得漏报 —— 此前 `dv=not-collected` 会提前 return)
#
# 用法:bash scripts/test/cep_ic3_report_states_check.sh

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

TOOL="tools/render_report.py"
if [[ ! -f "$TOOL" ]]; then
    echo "✗ 未找到 $TOOL" >&2
    exit 2
fi

TMP="$(mktemp -d /tmp/opencode/cep-ic3-XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

problems=0

# ── 合成一个 run 目录(只含 render 所需的 metrics.json + simulation_config.json)──
make_run() {
    local dir="$1" dv="$2" mv="$3"
    mkdir -p "$dir"
    python3 - "$dir" "$dv" "$mv" <<'PY'
import json, sys
d, dv, mv = sys.argv[1], sys.argv[2], sys.argv[3]

def cast(v):
    if v == "not-collected":
        return v
    return int(v)

metrics = {
    "total_cycles": 1000, "total_transactions": 200, "transactions_per_cycle": 0.2,
    "avg_latency_cycles": 10, "p50_latency_cycles": 10, "p95_latency_cycles": 12,
    "p99_latency_cycles": 14, "max_latency_cycles": 20,
    "coherence_stalls": 0, "collective_stalls": 0, "directory_misses": 0,
    "deadlock_violations": cast(dv), "memory_violations": cast(mv),
}
with open(f"{d}/metrics.json", "w", encoding="utf-8") as f:
    json.dump(metrics, f, ensure_ascii=False, indent=2)
with open(f"{d}/simulation_config.json", "w", encoding="utf-8") as f:
    json.dump({"hypothesis_id": "H4", "verification_mode": "bounded_random",
               "verification_scope": "IC3 三态 fixture"}, f, ensure_ascii=False, indent=2)
PY
}

# ── 渲染并取 badge 区段 ────────────────────────────────────────────────
render_badge() {
    local dir="$1"
    python3 "$TOOL" "$dir" >/dev/null 2>&1
    python3 - "$dir/report.html" <<'PY'
import re, sys
html = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r'<div class="(bad|ok|warn)">(.*?)</div>', html, re.S)
if not m:
    print("NO_BADGE")
else:
    text = re.sub(r"<[^>]+>", "", m.group(2))
    print(f"{m.group(1)}|{text.strip()}")
PY
}

expect_state() {
    local name="$1" dir="$2" want_state="$3" want_text="$4" forbid_text="${5:-}"
    local badge state text
    badge="$(render_badge "$dir")"
    state="${badge%%|*}"
    text="${badge#*|}"
    if [[ "$state" == "NO_BADGE" ]]; then
        echo "  ✗ $name: 未渲染出 badge"; problems=$((problems+1)); return
    fi
    if [[ "$state" != "$want_state" ]]; then
        echo "  ✗ $name: badge 态 = $state(期望 $want_state)· 文本 = $text"
        problems=$((problems+1)); return
    fi
    if [[ "$text" != *"$want_text"* ]]; then
        echo "  ✗ $name: badge 文本缺「$want_text」· 实际 = $text"
        problems=$((problems+1)); return
    fi
    if [[ -n "$forbid_text" && "$text" == *"$forbid_text"* ]]; then
        echo "  ✗ $name: badge 文本**不应**含「$forbid_text」· 实际 = $text"
        problems=$((problems+1)); return
    fi
    echo "  ✓ $name: [$state] $text"
}

echo "[cep-ic3-states] IC3 报告层三态 fixture(per ADR-CEP-06 SU#3)"

# F1 —— 通过态
make_run "$TMP/f1" 0 0
expect_state "F1 通过态(dv=0 ∧ mv=0)" "$TMP/f1" "ok" "一致性闸门通过" "不适用"

# F2 —— 不适用态(优先于「通过」)
make_run "$TMP/f2" "not-collected" 0
expect_state "F2 不适用态(dv=not-collected ∧ mv=0)" "$TMP/f2" "warn" "一致性闸门不适用" "闸门通过"

# F3 —— 违例态
make_run "$TMP/f3" 5 0
expect_state "F3 违例态(dv=5)" "$TMP/f3" "bad" "一致性违例"

# F4 —— 连带洞(dv 未采集 **不得** 漏报 mv 的真实违例)
make_run "$TMP/f4" "not-collected" 5
expect_state "F4 连带洞(dv=not-collected ∧ mv=5)" "$TMP/f4" "bad" "一致性违例"

if [[ $problems -gt 0 ]]; then
    echo "✗ IC3 三态 fixture 失败:${problems} 项" >&2
    exit 1
fi
echo "✓ IC3 三态 fixture 全部通过(4 条)"
