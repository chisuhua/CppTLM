#!/usr/bin/env bash
# scripts/cep/parity/run_parity_trace.sh
#   —— H7 对拍基准的**同源 trace 档**端到端执行入口
#
# 流程(两侧消费**同一条**访问流,per `docs/architecture/14-coherence-parity-baseline.md` §4.1):
#   ① 生成确定性 trace(CSV + gem5 protobuf)
#   ② gem5 侧:`PyTrafficGen.createTrace` 回放 ⇒ 产出 gem5 侧 JSON
#   ③ CppTLM 侧:`TrafficGenTLM`(TRACE 模式)+ `CacheTLM` ⇒ 产出 cpptlm 侧 JSON
#   ④ 比较器:P-1 / P-1b / P-4a / P-4b / P-2(观察值)⇒ verdict
#
# ⚠️ **口径边界(不得越界表述)**:
#   本档验证的是「**同源访问流下的口径一致性**」+「路径延迟常数」,
#   **不验证 H7**(本仓 stub cache 的现实性)⇒ **H7 仍未验证 · R7 维持 🟡 中**。
#   ⚠️ **P-2(命中率)只作观察登记**:本仓 `CacheTLM` 是「无限容量 + 精确地址 map」
#      (命中 = 该地址曾被访问过),gem5 是「容量内命中」⇒ 命中判据**结构性不同**
#      ⇒ 命中率**不可直接判**(per `14 号` §4.2)。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5/build/NULL/gem5.opt}"
TRACE_BASE="${PARITY_TRACE_BASE:-$OUT_DIR/trace}"
# ⚠️ **默认 1e5 条** —— `14 号` §4.1 规定「主对拍 **≥ 10⁵ 事务**」
#    (与 `verification_scope` 声明一致);统计置信度需要足够样本量。
NUM_ACCESSES="${PARITY_NUM_ACCESSES:-100000}"
WORKING_SET="${PARITY_WORKING_SET:-65536}"
SEED="${PARITY_SEED:-42}"
# trace 的 tick 间隔(默认 10000 = 10ns @1THz):
# ⚠️ 间隔过密 ⇒ 请求堆在 cache 的 MSHR(`mshrs=4`)上 ⇒ 事件循环看似卡死(实测踩过);
#    间隔过大 ⇒ trace 总时长 = 条数 × 间隔 逼近 `progress_check`(1e9 ticks)⇒ 撞上。
#    ⇒ 1e5 条 × 10000 = 1e9 ticks ⇒ `progress_check` 由探针按**实测条数**算(4 倍)。
TICK_INTERVAL="${PARITY_TICK_INTERVAL:-10000}"

log() { printf '[run_parity_trace] %s\n' "$*"; }
fail() { printf '[run_parity_trace] ✗ %s\n' "$*" >&2; exit 1; }

# ① gem5 就绪(缺席 ⇒ ctest skip 77)
if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5 二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

# ② 生成 trace(确定性)
log "生成 trace(seed=$SEED, 条数=$NUM_ACCESSES, 工作集=$WORKING_SET)..."
python3 "$ROOT/scripts/cep/parity/parity_trace_gen.py" \
    --out "$TRACE_BASE" --seed "$SEED" \
    --num-accesses "$NUM_ACCESSES" --working-set "$WORKING_SET" \
    --tick-interval "$TICK_INTERVAL" \
    || fail "trace 生成失败"

# ③ gem5 侧回放
log "gem5 侧回放(same-source trace)..."
PARITY_TRACE_PB="$TRACE_BASE.pb" \
PARITY_OUT="$OUT_DIR/gem5_trace_probe.json" \
PARITY_STATS="$OUT_DIR/gem5_trace_stats.txt" \
    "$GEM5_BIN" "--stats-file=$OUT_DIR/gem5_trace_stats.txt" \
    "$ROOT/scripts/cep/parity/gem5_trace_probe.py" \
    > "$OUT_DIR/gem5_trace_run.log" 2>&1 \
    || { tail -20 "$OUT_DIR/gem5_trace_run.log"; fail "gem5 回放失败"; }

# ④ CppTLM 侧回放(读**同一** CSV)
log "CppTLM 侧回放(同一 trace)..."
PARITY_TRACE_CSV="$TRACE_BASE.csv" \
PARITY_OUT="$OUT_DIR/cpptlm_probe.json" \
    "$ROOT/build/bin/cpptlm_tests" "[cep][parity]" \
    || fail "CppTLM 侧回放失败"

# ⑤ 比较
log "比较两侧 ..."
set +e
python3 "$ROOT/scripts/cep/parity/parity_compare.py" \
    --gem5 "$OUT_DIR/gem5_trace_probe.json" \
    --cpptlm "$OUT_DIR/cpptlm_probe.json" \
    --out "$OUT_DIR/verdict_trace.json"
rc=$?
set -e

# ⚠️ **已知限制检测(上游 gem5 缺陷,非本仓判据问题)**:
#    gem5 v24 的 `ProtoInputStream` 用 protobuf 的 `IstreamInputStream`(零拷贝路径)
#    **逐条消息新建** `io::CodedInputStream` ⇒ **文件较大时提前 EOF**
#    (实测:1000 条读到 966 / 20000 条读到 19735 / 100000 条读到 98874;
#     同一文件用 Python 的 protobuf 库可完整读出 ⇒ 文件本身没问题)。
#    ⇒ 文件同源路径在 gem5 v24 侧**不可靠** ⇒ 走**参数同源**路线
#      (`scripts/cep/parity/run_parity_linear.sh`)。
#    ⇒ 这里检测到截断 ⇒ 报「已知限制」并 **exit 77**(ctest 跳过)——
#      不让**上游缺陷**把门禁打红,但**信息保留**在日志与台账里。
_trunc=$(python3 -c "
import json
m = json.load(open('$OUT_DIR/trace.meta.json'))
g = json.load(open('$OUT_DIR/gem5_trace_probe.json'))
print(1 if g.get('accesses', 0) < m.get('num_accesses', 0) else 0)
" 2>/dev/null || echo 0)
if [[ "$_trunc" == "1" ]]; then
    log "⚠️ **已知限制**:gem5 v24 的 protobuf 流**提前 EOF**(上游缺陷)⇒"
    log "    gem5 读到 $(python3 -c "import json;print(json.load(open('$OUT_DIR/gem5_trace_probe.json')).get('accesses'))") 条 < trace $(python3 -c "import json;print(json.load(open('$OUT_DIR/trace.meta.json'))['num_accesses'])") 条"
    log "    ⇒ 文件同源路径**不可用**;请用 scripts/cep/parity/run_parity_linear.sh(参数同源)"
    log "产物:$OUT_DIR/{trace.csv,trace.pb,trace.meta.json,gem5_trace_probe.json,cpptlm_probe.json,verdict_trace.json}"
    exit 77
fi
log "产物:$OUT_DIR/{trace.csv,trace.pb,trace.meta.json,gem5_trace_probe.json,cpptlm_probe.json,verdict_trace.json}"
exit $rc
