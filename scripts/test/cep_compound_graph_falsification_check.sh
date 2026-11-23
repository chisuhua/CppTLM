#!/usr/bin/env bash
# scripts/test/cep_compound_graph_falsification_check.sh
#   —— 复合一致性公理的 litmus **证伪工装**闸
#
# per change `cpptlm-cep-litmus-falsification-harness`（登记册队列 2 的 I-6a）
#     + `ADR-CEP-13` 决策 5 ②（实验只能证伪 · 平台不提供无违例保证）
#
# ── 闸做什么（两条臂）──────────────────────────────────────────────
#   臂① **正控**：跑真实语料 ⇒ 判定产物必须给出**真结果**
#        （当前 = 复合公理**不可证伪**，因为 `rfe`/`fr`/`ws` 三类边两仓均无定义）
#        ⇒ ⛔ 产物**不得**出现「已证明 / proved」。
#   臂② **反向 fixture**：喂一份**含跨域环**的语料 ⇒ 判定必须翻转为 `falsified = true`
#        ⇒ 证明工装**有牙**（不是恒绿占位）。
#
# ⚠️ 该闸**不判定公理成立**，只判定「证伪尝试**真的跑了**且判定形态诚实」。
#
# 用法:
#   bash scripts/test/cep_compound_graph_falsification_check.sh
#   BUILD_DIR=build bash scripts/test/cep_compound_graph_falsification_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-build}"
BIN="$BUILD_DIR/bin/cpptlm_tests"
CORPUS="test/cep_compound_graph_corpus.json"

if [ ! -x "$BIN" ]; then
  echo "  · [SKIP] 未找到测试二进制($BIN) —— litmus 判定需先构建(cpptlm_tests)"
  exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ── 臂① 正控：真实语料 ────────────────────────────────────────────────
CPPTLM_COMPOUND_GRAPH_CORPUS="$CORPUS" CPPTLM_COMPOUND_GRAPH_REPORT="$TMP/report.json" \
  "$BIN" "[cep][compound-graph][falsification][generic],[cep][compound-graph][falsification][repo-corpus]" -r compact >"$TMP/pos.out" 2>&1
if [ $? -ne 0 ]; then
  echo "  ✗ litmus 判据未全绿："
  tail -20 "$TMP/pos.out" | sed 's/^/    /'
  exit 1
fi

if [ ! -f "$TMP/report.json" ]; then
  echo "  ✗ 未产出判定产物($TMP/report.json) ⇒ 闸无判据"
  exit 1
fi

python3 - "$TMP/report.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
d = json.load(open(p, encoding="utf-8"))
problems = []
if d.get("reachable") is not False:
    problems.append(f"reachable 应为 false（当前边类无声明面 ⇒ 不可证伪），实为 {d.get('reachable')!r}")
if not str(d.get("unreachable_reason", "")).strip():
    problems.append("unreachable_reason 为空 ⇒ 不可达必须给出具体原因")
if d.get("falsified") is not False:
    problems.append(f"falsified 应为 false，实为 {d.get('falsified')!r}")
if not str(d.get("coverage_note", "")).strip():
    problems.append("coverage_note 为空 ⇒ 覆盖边界必须随判定一起读")
blob = json.dumps(d, ensure_ascii=False)
for bad in ("已证明", "proved", "PROVED"):
    if bad in blob:
        problems.append(f"判定产物出现「{bad}」⇒ ⛔ 实验只能证伪，不得表述为已证明")
if problems:
    for x in problems:
        print(f"  ✗ {x}")
    sys.exit(1)
print("  ✓ 正控:判定诚实（不可证伪 + 具体原因 + 覆盖边界 + 无「已证明」表述）")
PYEOF
if [ $? -ne 0 ]; then
  exit 1
fi

# ── 臂② 反向 fixture：含跨域环的语料 ⇒ 必须翻转成 falsified ────────────
cat >"$TMP/cyclic_corpus.json" <<'JSONEOF'
{
  "compound_edges": [
    {"instance": "mesif", "from_net": "compute_noc", "from_vc": 0,
     "to_net": "ualink_fabric", "to_vc": 0, "class": "rfe"},
    {"instance": "mesif", "from_net": "ualink_fabric", "from_vc": 0,
     "to_net": "compute_noc", "to_vc": 0, "class": "fr"}
  ]
}
JSONEOF

CPPTLM_COMPOUND_GRAPH_CORPUS="$TMP/cyclic_corpus.json" CPPTLM_COMPOUND_GRAPH_REPORT="$TMP/report2.json" \
  "$BIN" "[cep][compound-graph][falsification][generic]" -r compact >"$TMP/neg.out" 2>&1
if [ $? -ne 0 ]; then
  echo "  ✗ 反向 fixture:litmus 判据未全绿（⛔ 说明工装在有环时也会红 ⇒ 恒绿占位）"
  tail -20 "$TMP/neg.out" | sed 's/^/    /'
  exit 1
fi

python3 - "$TMP/report2.json" <<'PYEOF'
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
if d.get("falsified") is not True:
    print(f"  ✗ 反向 fixture:含跨域环的语料未被判为 falsified（实为 {d.get('falsified')!r}）"
          " ⇒ 证伪机械无鉴别力")
    sys.exit(1)
cycles = d.get("cycles") or []
if not cycles:
    print("  ✗ 反向 fixture:falsified 为真但无环文本 ⇒ 判定不自洽")
    sys.exit(1)
if "compute_noc" not in cycles[0] or "ualink_fabric" not in cycles[0]:
    print(f"  ✗ 反向 fixture:环文本未跨网络（{cycles[0]!r}）⇒ 不是跨域环")
    sys.exit(1)
print("  ✓ 反向 fixture:跨域环被证伪（判定翻转 ⇒ 工装有牙）")
PYEOF
if [ $? -ne 0 ]; then
  exit 1
fi

echo "  ✓ litmus 证伪工装闸通过（正控诚实 + 反向 fixture 有鉴别力）"
exit 0
