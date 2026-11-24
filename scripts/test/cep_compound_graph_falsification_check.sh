#!/usr/bin/env bash
# scripts/test/cep_compound_graph_falsification_check.sh
#   —— 复合一致性公理的**操作级**证伪工装闸
#
# per change `cep-memory-model-operational-trace`（backlog `cep-memory-model-operational-trace`）
#     + `ADR-CEP-22`（✅ 已签发 v1.0 · 每域模型来源 + 域间边构造规则）
#     + `ADR-CEP-13` 决策 2（公理）· 决策 5 ②（实验只能证伪 · 平台不提供无违例保证）
#
# ── 闸做什么（六条臂）──────────────────────────────────────────────────────
#   臂① **正控**：跑真实操作迹语料 ⇒ 判定产物必须给出**真结果**
#        （金标准跨域违例迹 ⇒ `falsified` + 见证；companion 迹 ⇒ `not_falsified`）
#        ⇒ ⛔ 产物**不得**出现「已证明 / proved」。
#   臂② **取代臂**（旧语义「通道环 ⇒ falsified」⇒ **必须不**保留）：
#        喂**通道语料** ⇒ 判定必须为 `invalid` + 原因码 `channel-edge-not-op-edge`
#        ⇒ ⛔ 通道图 ≠ 操作图 ⇒ 混图 = **假绿**（per `ADR-CEP-22` 决策 4 / 缺口①）。
#   臂③ **派生边作为输入** ⇒ `invalid` + `derived-edge-as-input`
#        （`fr`/`ws` 是**派生边** ⇒ 语料直接给出 = 让语料断言结论）。
#   臂④ **未绑定域** ⇒ `invalid` + `unbound-domain`（⛔ 不得静默降级为「未证伪」）。
#   臂⑤ **截断** ⇒ `undecided` + `search-truncated`（⛔ 大迹不得静默变假阴性）。
#   臂⑥ **开工门**（AC-16）：`combination_contract` 在 schema 里恰好 **1** 处。
#
# ⚠️ 该闸**不判定公理成立**，只判定「证伪尝试**真的跑了**且判定形态诚实」。
#
# ── 旧→新语义映射表（AC-11 · **判据不得被静默删除**）────────────────────
#   | 旧臂（v1.0 及以前 · 通道图）                  | 新臂（操作图）                          | 理由 |
#   |---|---|---|---|
#   | 臂① 正控：仓内语料为空 ⇒ `reachable=false`   | 臂① 正控：金标准迹 ⇒ `falsified` + 见证 | 语料不再是空的；**不可证伪**的真结果改由「域模型未登记」等**具体原因**表达 |
#   | 臂② 通道环 ⇒ `falsified=true` + 环文本        | 臂② 通道语料 ⇒ `invalid` + `channel-edge-not-op-edge` | **范畴错误**：通道依赖图（顶点 = `(instance, network, vc)`）与公理要求的**操作图**不同型 ⇒ 通道环**不是**对复合公理的证伪（= 假绿）⇒ 通道环的真判据归 `channel_dependency_graph.hh`（`INV-CEP-09-IC1`）|
#   | （无）                                       | 臂③ 派生边作为输入 ⇒ `derived-edge-as-input` | 新增（`fr`/`ws` 的派生纪律） |
#   | （无）                                       | 臂④ 未绑定域 ⇒ `unbound-domain`            | 新增（域身份纪律） |
#   | （无）                                       | 臂⑤ 截断 ⇒ `undecided` + `search-truncated` | 新增（大迹不可判） |
#   | （无）                                       | 臂⑥ 开工门（schema 计数）                  | 新增（I-6 已落地的单命令判据） |
#
# 用法:
#   bash scripts/test/cep_compound_graph_falsification_check.sh
#   BUILD_DIR=build bash scripts/test/cep_compound_graph_falsification_check.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-build}"
BIN="$BUILD_DIR/bin/cpptlm_tests"
TRACES="test/cep_memory_model_traces.json"
CORPUS="test/cep_compound_graph_corpus.json"
SCHEMA="include/tlm/cep/protocol_instance.schema.json"

if [ ! -x "$BIN" ]; then
  echo "  · [SKIP] 未找到测试二进制($BIN) —— 判定需先构建(cpptlm_tests)"
  exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ── 臂①~⑤：跑判定面 ⇒ 逐臂核对判定产物 ───────────────────────────────────
CPPTLM_COMPOUND_GRAPH_TRACES="$TRACES" CPPTLM_COMPOUND_GRAPH_CORPUS="$CORPUS" \
CPPTLM_COMPOUND_GRAPH_REPORT="$TMP/report.json" \
  "$BIN" "[cep][compound-graph][falsification]" -r compact >"$TMP/run.out" 2>&1
if [ $? -ne 0 ]; then
  echo "  ✗ 判定面未全绿："
  tail -20 "$TMP/run.out" | sed 's/^/    /'
  exit 1
fi

if [ ! -f "$TMP/report.json" ]; then
  echo "  ✗ 未产出判定产物($TMP/report.json) ⇒ 闸无判据"
  exit 1
fi

python3 - "$TMP/report.json" <<'PYEOF'
import json, sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
if not isinstance(data, list):
    print("  ✗ 判定产物不是数组（trace_id → 判定的列表）")
    sys.exit(1)
by_id = {d.get("trace_id", ""): d for d in data}
problems = []


def need(cond, msg):
    if not cond:
        problems.append(msg)


# ── 臂① 正控 ──────────────────────────────────────────────────────────────
sb = by_id.get("xdomain-sb")
need(sb is not None, "产物缺 `xdomain-sb`（金标准违例迹）")
if sb:
    need(sb.get("verdict") == "falsified",
         f"金标准违例迹应判 falsified，实为 {sb.get('verdict')!r} ⇒ 跨域 `rfe` 拉环未被检出")
    need(bool(sb.get("cycle")), "金标准违例迹判 falsified 但**无见证** ⇒ 判定不自洽")
    need(not sb.get("reason"), f"金标准违例迹不应带原因码（实为 {sb.get('reason')!r}）")

comp = by_id.get("xdomain-sb-companion")
need(comp is not None, "产物缺 `xdomain-sb-companion`（companion 迹）")
if comp:
    need(comp.get("verdict") == "not_falsified",
         f"companion 迹应判 not_falsified，实为 {comp.get('verdict')!r}")
    need(not comp.get("cycle"), "companion 迹不应有环")

# 覆盖边界随判定一起读（⚠️ 恒不为空）+ ⛔ 无「已证明」
for tid, d in by_id.items():
    need(str(d.get("coverage_note", "")).strip(),
         f"产物 `{tid}` 的 coverage_note 为空 ⇒ 覆盖边界必须随判定一起读")
blob = json.dumps(data, ensure_ascii=False)
for bad in ("已证明", "proved", "PROVED"):
    need(bad not in blob, f"判定产物出现「{bad}」⇒ ⛔ 实验只能证伪，不得表述为已证明")

# ── 臂② 取代臂（旧语义「通道环 ⇒ falsified」**不得**保留）─────────────────
ch = by_id.get("synthetic:channel-corpus")
need(ch is not None, "产物缺 `synthetic:channel-corpus`（通道语料的反 fixture）")
if ch:
    need(ch.get("verdict") == "invalid",
         f"通道语料必须判 invalid（⛔ 旧语义「通道环 ⇒ falsified」已**取代**），实为 {ch.get('verdict')!r}")
    need(ch.get("reason") == "channel-edge-not-op-edge",
         f"通道语料的原因码应为 channel-edge-not-op-edge，实为 {ch.get('reason')!r}")

# ── 臂③ 派生边作为输入 ───────────────────────────────────────────────────
dv = by_id.get("synthetic:derived-edge-as-input")
need(dv is not None, "产物缺 `synthetic:derived-edge-as-input`")
if dv:
    need(dv.get("verdict") == "invalid", "派生边作为输入必须判 invalid")
    need(dv.get("reason") == "derived-edge-as-input",
         f"派生边的原因码应为 derived-edge-as-input，实为 {dv.get('reason')!r}")

# ── 臂④ 未绑定域 ─────────────────────────────────────────────────────────
ub = by_id.get("synthetic:unbound-domain")
need(ub is not None, "产物缺 `synthetic:unbound-domain`")
if ub:
    need(ub.get("verdict") == "invalid", "未绑定域必须判 invalid")
    need(ub.get("reason") == "unbound-domain",
         f"未绑定域的原因码应为 unbound-domain，实为 {ub.get('reason')!r}")

# ── 臂⑤ 截断 ⇒ 不可判 ───────────────────────────────────────────────────
st = by_id.get("synthetic:search-truncated")
need(st is not None, "产物缺 `synthetic:search-truncated`")
if st:
    need(st.get("verdict") == "undecided",
         f"截断必须判 undecided（⛔ 不得静默变假阴性），实为 {st.get('verdict')!r}")
    need(st.get("reason") == "search-truncated",
         f"截断的原因码应为 search-truncated，实为 {st.get('reason')!r}")
    need(st.get("search_truncated") is True, "截断产物的 `search_truncated` 应为 true")

if problems:
    for x in problems:
        print(f"  ✗ {x}")
    sys.exit(1)
print("  ✓ 正控:判定诚实（金标准违例迹 ⇒ falsified + 见证 · companion ⇒ 未证伪 · 无「已证明」表述）")
print("  ✓ 取代臂:通道语料 ⇒ invalid + channel-edge-not-op-edge（旧语义「通道环 ⇒ falsified」已取代）")
print("  ✓ 派生边 / 未绑定域 / 截断 ⇒ 各自的原因码与判定面正确")
PYEOF
if [ $? -ne 0 ]; then
  exit 1
fi

# ── 臂⑥ 开工门（AC-16）：`combination_contract` 在 schema 里恰好 1 处 ────────
COUNT=$(grep -c '"combination_contract"' "$SCHEMA")
if [ "$COUNT" != "1" ]; then
  echo "  ✗ 开工门失败:`combination_contract` 在 $SCHEMA 里出现 $COUNT 次（应恰好 1）"
  exit 1
fi
echo "  ✓ 开工门:combination_contract 在 schema 里恰好 1 处（I-6 已落地）"

echo "  ✓ 操作级证伪工装闸通过（正控诚实 + 取代臂 + 三条反 fixture + 开工门）"
exit 0
