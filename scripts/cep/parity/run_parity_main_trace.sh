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
# ⚠️ **文件同源路径为何可用**(⚠️ **2026-10-31 更正**,per 台账 Entry #120):
#   此前本节写的是「gem5 **v24** 的 `ProtoInputStream` 有**提前 EOF** 缺陷 ⇒ 文件同源
#   不可用;**gem5 v25 已重写 `src/proto/protoio.cc`** ⇒ 根因已消除」——
#   **该结论已被实测推翻**(真根因 = 对拍探针的 tick 预算不足,与 gem5 版本无关;
#   **上游原版 v25 零补丁**即可读满 1e5 条)。
#   ⇒ 文件同源路径**与 gem5 版本解耦**(只需 `createTrace` 能读 trace 文件)。
#
# 流程:
#   ① 生成 trace:①「**真均匀**」流(⇒ `conforming` 类,R-B 前提成立)
#      + ②「**双峰**」流(⇒ `breaking` 类 = **正控**,R-B 前提不成立)
#      ⇒ 二者的 class 归类见 `14 号` §4.4 的**封闭枚举表**
#   ② gem5 侧 `createTrace` 回放
#   ③ CppTLM 侧读**同一** CSV
#   ④ 分析:按 **class 分列**判据 —— `conforming` 全过 P-2/P-3(= H7 判定);
#      `breaking` 的 P-3 **必须 FAIL**(正控必须激发)⇒ P-2a 的 **A-1 形态**
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

# ⚠️ **gem5 二进制路径的唯一真源** = `parity_config.json` 的
#    `gem5.versions.default`(dir + variant)⇒ 避免脚本各自硬编码版本目录
#    (此前 4 个脚本硬编码 `external/gem5/...` = v24 路径 ⇒ v24 build 删除后
#     **静默跳过** e2e ⇒ 门禁失守)。
GEM5_BIN="${GEM5_BIN:-$ROOT/$(python3 -c "
import json
v = json.load(open('$ROOT/scripts/cep/parity/parity_config.json'))['gem5']['versions']['default']
print(v['dir'] + '/build/NULL/gem5.opt')
")}"
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
# ⚠️ **流名不得含 `.`** —— `parity_trace_gen.py` 用 `Path(out).with_suffix(".csv")`
#    推导产物路径 ⇒ 流名里的 `.25` 会被**当作扩展名替换掉**
#    (`trace_HC_hot0.25` ⇒ 实际产出 `trace_HC_hot0.csv`)⇒ 回放时找不到文件 ⇒ 崩。
#    ⇒ 用 `_` 分隔: `HC_hot0p25`。
# ⚠️ **流集合设计**(per oracle 建议 + `14 号` §4.4):
#   · `uniform`(真均匀 ⇒ `conforming`):必须含 **W = C = 16KiB 的边界点**
#     (该点每组恰好 2 块 = 相联度 ⇒ 完美装下 ⇒ `h_m ≈ 1` = C/W ⇒ R-B 的**最强单点**),
#     外加 W=32KiB(=2C)与 W=128KiB(=8C)两个远离边界的点;
#   · `hot_cold`(双峰 ⇒ `breaking` = 正控):2 条不同 hot_ratio
#     (避免单点运气;当前 margin = 阈值的 3~14 倍)。
STREAM_SPECS=(
    "uniform|W16384|16384|0"
    "uniform|W32768|32768|0"
    "uniform|W131072|131072|0"
    "hot_cold|HC_hot0p25|4096|0.25"
    "hot_cold|HC_hot0p75|4096|0.75"
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
