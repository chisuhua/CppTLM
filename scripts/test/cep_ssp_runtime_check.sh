#!/usr/bin/env bash
# scripts/test/cep_ssp_runtime_check.sh
# SSP **档 0 运行时 + 实验 4 件套** 契约闸（批 2 `cep-ssp-batch2` · ⑦⑧⑨⑩）
#
# 覆盖:
#   A-1  4 件套齐备（`simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`）
#   A-2  证据**声明档位**（`ADR-CEP-20` 决策 6：`verification_mode` + `verification_scope`）
#   A-3  ⛔ **档 0 禁冒充满档**：
#        · `simulation_mode` == `functional`（⛔ **不是** `timing` —— SSP 档 0 **零时序**）
#        · 13 项必纳 metric **全** `"not-collected"` + `_not_collected` **全 13 项**原因
#        · `_measured` 为**空**（本 run 零实测项 ⇒ 禁写裸 0 冒充实测）
#        · `verification_scope` **逐字**携带「零时序」+「不承诺语义」限定语
#   A-4  反向 fixture:**洞命中** ⇒ 该次实验判「不可判」⇒ **拒绝产出** ⇒ 本闸判红
#   A-5  预登记绑定:`hypothesis_id` == `H9` 且在 `experimental/preregistration.json`
#        **已登记**（`status` ≠ `pending`）⇒ 顺序锁成立（预登记先于产出）
#
# ⚠️ **本闸不覆盖** `ADR-CEP-14` C1~C10 在 SSP 协议上的实例化（需 SSP 驱动的
#   adapter / engine ⇒ 属批 ② 的 ⑧「装配 seam」后续项）—— ⛔ 本闸**不得**被表述为
#   「合同测试已在 SSP 协议上实例化」。
#
# 判据纪律:⛔ 「rc≠0」不构成红因;fixture 必须断言**红因签名命中**。
#
# 作者: CppTLM Team / 日期: 2026-11-27
#
# 用法:
#   bash scripts/test/cep_ssp_runtime_check.sh              # 正常校验
#   bash scripts/test/cep_ssp_runtime_check.sh --selftest    # 只跑反向 fixture(洞命中)
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT" || exit 2

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'
PASS=0; FAIL=0
ok()  { PASS=$((PASS+1)); printf '  %b✓%b %s\n' "$GREEN" "$NC" "$*"; }
bad() { FAIL=$((FAIL+1)); printf '  %b✗%b %s\n' "$RED" "$NC" "$*"; }

SELFTEST=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --selftest) SELFTEST=1; shift ;;
        -h|--help) sed -n '2,28p' "$0"; exit 0 ;;
        *) echo "未知参数: $1" >&2; exit 2 ;;
    esac
done

BIN="build/bin/cpptlm_tests"
if [[ ! -x "$BIN" ]]; then
    echo "✗ 未找到 $BIN,请先构建: ./build.sh" >&2
    exit 2
fi

RUN_DIR="$(mktemp -d)"
trap 'rm -rf "$RUN_DIR"' EXIT

echo "── A-1/A-2/A-3/A-5:SSP 档 0 run 的 4 件套 + 档位声明 ──────────────"
if ! bash scripts/test/cep_experiment_run.sh --ssp --out "$RUN_DIR/run"; then
    bad "SSP 档 0 run 产出失败(见上方输出)"
    exit 1
fi
RD="$RUN_DIR/run"

for f in simulation_config.json trace.h5 metrics.json report.html; do
    if [[ -s "$RD/$f" ]]; then ok "4 件套齐备: $f"; else bad "4 件套缺或空: $f"; fi
done

if python3 - "$RD" <<'PY'
import json, os, sys
rd = sys.argv[1]
m = json.load(open(os.path.join(rd, "metrics.json"), encoding="utf-8"))
c = json.load(open(os.path.join(rd, "simulation_config.json"), encoding="utf-8"))
REQ = ["total_cycles","total_transactions","transactions_per_cycle",
       "avg_latency_cycles","p50_latency_cycles","p95_latency_cycles",
       "p99_latency_cycles","max_latency_cycles","coherence_stalls",
       "collective_stalls","directory_misses","deadlock_violations",
       "memory_violations"]
problems = []
# A-2 档位声明
for obj, name in ((m, "metrics.json"), (c, "simulation_config.json")):
    if not obj.get("verification_mode"):
        problems.append(f"{name} 缺 verification_mode")
    if not obj.get("verification_scope"):
        problems.append(f"{name} 缺 verification_scope")
# A-3 档 0 禁冒充满档
if c.get("simulation_mode") != "functional":
    problems.append(f"simulation_mode 应为 functional(SSP 档 0 零时序),实得 {c.get('simulation_mode')!r}")
nc = m.get("_not_collected") or {}
for k in REQ:
    if m.get(k) != "not-collected":
        problems.append(f"metric {k} 应为 not-collected(档 0 不采),实得 {m.get(k)!r}")
    if k not in nc:
        problems.append(f"_not_collected 缺 {k} 的 per-metric 原因")
if m.get("_measured"):
    problems.append(f"_measured 应为空(本 run 零实测项),实得 {m.get('_measured')!r}")
scope = str(c.get("verification_scope", ""))
for kw in ("零时序", "不承诺语义"):
    if kw not in scope:
        problems.append(f"verification_scope 缺限定语 {kw!r}")
# A-5 预登记绑定
if c.get("hypothesis_id") != "H9":
    problems.append(f"hypothesis_id 应为 H9,实得 {c.get('hypothesis_id')!r}")
for p in problems:
    print(f"  \033[0;31m✗\033[0m {p}")
sys.exit(1 if problems else 0)
PY
then
    ok "A-2 档位声明 + A-3 档 0 禁冒充满档 + A-5 预登记绑定(H9)"
else
    bad "A-2/A-3/A-5 判据未过(见上方红因)"
fi

if [[ "$SELFTEST" -eq 1 ]]; then
    echo "── A-4 反向 fixture:洞命中 ⇒ 拒绝产出 ─────────────────────────"
    HOLE_DIR="$RUN_DIR/hole"
    mkdir -p "$HOLE_DIR"
    if CEP_SSP_HOLE=1 CEP_EXP_DIR="$HOLE_DIR" CEP_EXP_ID="ssp" \
        "$BIN" "[cep][ssp][emit]" -r compact >/dev/null 2>&1; then
        if [[ -e "$HOLE_DIR/metrics.json" || -e "$HOLE_DIR/simulation_config.json" ]]; then
            bad "反向 fixture 失败:洞命中却产出了文件(⛔ 不得留半套 4 件套)"
        else
            ok "反向 fixture 成立:洞命中 ⇒ 拒绝产出(该次实验判「不可判」)"
        fi
    else
        bad "反向 fixture 失败:洞命中 fixture 用例本身未通过"
    fi
fi

echo
if [[ $FAIL -eq 0 ]]; then
    echo -e "${GREEN}✓ SSP 运行时闸通过(${PASS} 项)${NC}"
    exit 0
fi
echo -e "${RED}✗ SSP 运行时闸失败(${FAIL} 项红 / ${PASS} 项绿)${NC}"
exit 1
