#!/usr/bin/env bash
# scripts/cep/parity/run_parity_main_trace.sh
#   —— H7 **主对拍档(文件同源)**端到端执行入口
#
# ⚠️ **与 `run_parity_main.sh`(参数同源)的区别**:
#   · 本脚本走**真正的文件同源**:两侧**读同一份 CSV**
#     (本仓侧读 `.csv`;gem5 侧回放由同一 CSV 生成的 `.pb`)⇒ **逐条相同**;
#   · `run_parity_main.sh` 是「参数同源」:两侧**各自独立**按同一组参数生成序列
#     ⇒ 靠**地址序列指纹**交叉验证。
#
# ⚠️ **为什么现在才有文件同源路径**:
#   gem5 **v24** 的 `ProtoInputStream` 有**提前 EOF** 缺陷 ⇒ `createTrace` 不可靠
#   (实测 1000 条读到 966)⇒ 文件同源不可用;
#   **gem5 v25 已重写 `src/proto/protoio.cc`**(改用 `io::FileInputStream` 非零拷贝 +
#   `ReadMessage`)⇒ **根因已消除** ⇒ 本路径可用。
#
# 流程:
#   ① 生成 **非均匀(双峰)** trace(⇒ 构成「**规则失效型**」流)
#      + 若干**均匀**流(对照)
#   ② gem5 侧 `createTrace` 回放(需 **gem5 v25**)
#   ③ CppTLM 侧读**同一** CSV
#   ④ 分析:用 **R-B 规则**估 `h_0` ⇒ **P-3**(估算误差)· **P-2**(延迟偏差)
#      ⇒ **P-2a 的 A-1 形态**(鉴别力自检:须含「规则失效型」流)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5-v25/build/NULL/gem5.opt}"
CFG_SHARED="$ROOT/scripts/cep/parity/parity_config.json"
CFG="$OUT_DIR/main_trace_config.json"
PROBE="$ROOT/scripts/cep/parity/gem5_trace_probe.py"
GEN="$ROOT/scripts/cep/parity/parity_trace_gen.py"
ANALYZE="$ROOT/scripts/cep/parity/parity_main_tier.py"
REC_TOOL="$ROOT/scripts/cep/parity/main_tier_stream_rec.py"

log() { printf '[run_parity_main_trace] %s\n' "$*"; }
fail() { printf '[run_parity_main_trace] ✗ %s\n' "$*" >&2; exit 1; }

if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5(v25)二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh --version v25.0.0.0)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

NUM_ACCESSES="${PARITY_NUM_ACCESSES:-100000}"
CACHE_BYTES=16384
BASE=0x100000

# ── 流定义:(profile, 名称, 额外参数)────────────────────────────────
# ⚠️ **必须含「规则失效型」流**(双峰分布)⇒ 否则 P-2a 的 **A-1 鉴别力自检**不满足
#    ⇒ 结论只能是 A-2(范围限定声明)⇒ H7 仍带限定。
#    ⇒ 这里**混合**:2 条均匀(对照)+ 2 条双峰(规则失效型)⇒ **A-1 可满足**。
STREAM_SPECS=(
    "uniform|W32768|32768|0"
    "uniform|W131072|131072|0"
    "hot_cold|HC_hot0.25|4096|0.25"
    "hot_cold|HC_hot0.75|4096|0.75"
)

STREAM_DIR="$OUT_DIR/main_trace_streams"
STREAMS_JSON="$OUT_DIR/main_trace_streams.json"
rm -rf "$STREAM_DIR" && mkdir -p "$STREAM_DIR"

for spec in "${STREAM_SPECS[@]}"; do
    IFS='|' read -r profile name ws hot_ratio <<< "$spec"
    log "流 $name:profile=$profile W=$ws hot_ratio=$hot_ratio ⇒ 生成 trace ..."
    cp "$CFG_SHARED" "$CFG"
    python3 "$GEN" --out "$OUT_DIR/trace_$name" --seed 42 \
        --num-accesses "$NUM_ACCESSES" --profile "$profile" \
        --working-set "$ws" --hot-bytes 4096 --cold-bytes 1048576 \
        --hot-ratio "$hot_ratio" >/dev/null || fail "流 $name 的 trace 生成失败"

    log "  gem5 侧回放(createTrace)..."
    PARITY_CONFIG="$CFG" \
    PARITY_TRACE_PB="$OUT_DIR/trace_$name.pb" \
    PARITY_TRACE_CSV="$OUT_DIR/trace_$name.csv" \
    PARITY_TRACE_META="$OUT_DIR/trace_$name.meta.json" \
    PARITY_OUT="$OUT_DIR/mt_${name}.json" \
    PARITY_STATS="$OUT_DIR/mt_${name}_stats.txt" \
        "$GEM5_BIN" "--stats-file=$OUT_DIR/mt_${name}_stats.txt" "$PROBE" \
        > "$OUT_DIR/mt_${name}_run.log" 2>&1 \
        || { tail -12 "$OUT_DIR/mt_${name}_run.log"; fail "流 $name 的 gem5 回放失败"; }
    python3 "$REC_TOOL" "$OUT_DIR/mt_${name}.json" "$name" "$STREAM_DIR" \
        || fail "流 $name 的结果提取失败"

    # 本仓侧:读**同一** CSV
    log "  CppTLM 侧回放(同一 CSV)..."
    PARITY_TRACE_CSV="$OUT_DIR/trace_$name.csv" \
    PARITY_VERIFICATION_MODE="$(python3 -c "
import json
print(json.load(open('$CFG'))['verification']['verification_mode'])
")" \
    PARITY_VERIFICATION_SCOPE="$(python3 -c "
import json
print(json.load(open('$CFG'))['verification']['verification_scope'])
")" \
    PARITY_OUT="$OUT_DIR/mt_${name}_cpptlm.json" \
        "$ROOT/build/bin/cpptlm_tests" "[cep][parity]" >/dev/null 2>&1 \
        || fail "流 $name 的 CppTLM 侧回放失败"
done

# 汇总(每条流取 gem5 侧结果 + 本仓侧指纹)
python3 "$ROOT/scripts/cep/parity/main_trace_collect.py" \
    "$STREAM_DIR" "$STREAMS_JSON" "$OUT_DIR" || fail "流结果汇总失败"

log "按 R-B 规则估算 h_0 ⇒ P-2 / P-3 / P-2a 判定 ..."
set +e
python3 "$ANALYZE" --streams "$STREAMS_JSON" --out "$OUT_DIR/verdict_main_trace.json"
rc=$?
set -e
log "产物:$STREAMS_JSON · $OUT_DIR/verdict_main_trace.json"
exit $rc
