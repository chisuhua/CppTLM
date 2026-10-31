#!/usr/bin/env bash
# scripts/cep/parity/run_parity_main.sh
#   —— H7 **主对拍档(固定命中率档)**端到端执行入口
#
# 流程:
#   ① **多流扫描**:对若干组 (hot 段遍数 R, hot 段行数 H, cold 段行数 C)
#      分别跑一次 gem5 ⇒ 每条流**自然**产生一个实测命中率 `h_m` 与实测平均延迟;
#      (⚠️ **不瞄准**某个 `h` —— 瞄准会让「统一 h_0」必然成立 ⇒ 判据失去鉴别力)
#   ② 汇总各流结果 ⇒ `main_tier_streams.json`
#   ③ 分析器 `parity_main_tier.py`:扫 `h_0` 网格 ⇒ 算各流的延迟偏差 ⇒
#      **存在统一 `h_0` 使所有流偏差 < 15%** ⇒ H7 判定
#
# ⚠️ **口径边界**:
#   · 本档验的是「**固定命中率参考模型**能否在多流上保持 <15% 的延迟偏差」
#     ⇒ 这是 **H7 的直接判定**;
#   · 但它**不能**被表述为「stub cache 的现实性已验证」——
#     简化模型与真实 cache 的**结构性差异**(read-no-allocate / 容量 / 替换)仍在;
#   · ⇒ 结论必须配合 `14 号` §4.5 的归因一起读。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5/build/NULL/gem5.opt}"
# ⚠️ **用临时 config 副本** —— 主对拍要按流改段配置(segments);
#    若直接改共享 `parity_config.json` ⇒ **污染**后续运行
#    (实测踩过:主对拍跑完后,参数同源档 `cep_parity_linear_e2e` 因 config 里
#     残留了 `segments` 而变红)。
CFG_SHARED="$ROOT/scripts/cep/parity/parity_config.json"
CFG="$OUT_DIR/main_tier_config.json"
PROBE="$ROOT/scripts/cep/parity/gem5_linear_probe.py"
ANALYZE="$ROOT/scripts/cep/parity/parity_main_tier.py"
CFG_TOOL="$ROOT/scripts/cep/parity/main_tier_stream_cfg.py"
REC_TOOL="$ROOT/scripts/cep/parity/main_tier_stream_rec.py"

log() { printf '[run_parity_main] %s\n' "$*"; }
fail() { printf '[run_parity_main] ✗ %s\n' "$*" >&2; exit 1; }

if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5 二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

# ── 流定义(R / H / C)─────────────────────────────────────────────────
# ⚠️ gem5 侧 cache = 16KiB / 2-way / LRU(per `parity_config.json` 的 `gem5.cache`)
#    ⇒ **hot 段必须 ≤ 16KiB**(否则第 2 遍也不命中);
#    ⇒ **cold 段必须 > 16KiB**(否则也命中)。
#    ⇒ h_m ≈ (R−1)×H / (R×H + C) ⇒ 随 R / H / C 变化 ⇒ **自然分散**。
CACHE_BYTES=16384
LINE=64
HOT_LINES_MAX=$(( CACHE_BYTES / LINE ))   # 256

STREAM_SPECS=(
    "1 256 4096"    # R=1 ⇒ 纯 cold ⇒ h_m ≈ 0
    "2 256 1024"    # h_m ≈ 256/(512+1024) = 0.167
    "3 128 512"     # h_m ≈ 2*128/(384+512) = 0.286
    "4 64 256"      # h_m = 3*64/(256+256) = 0.375
    "6 32 128"      # h_m ≈ 5*32/(192+128) = 0.5
    "10 16 64"      # h_m ≈ 9*16/(160+64) = 0.643
    "20 8 32"       # h_m ≈ 19*8/(160+32) = 0.792
)

BASE=0x100000
STREAM_DIR="$OUT_DIR/main_streams"
STREAMS_JSON="$OUT_DIR/main_tier_streams.json"
rm -rf "$STREAM_DIR" && mkdir -p "$STREAM_DIR"
# 从共享 config 复制一份到临时副本(每条流改的是副本)
cp "$CFG_SHARED" "$CFG"

for spec in "${STREAM_SPECS[@]}"; do
    read -r R H C <<< "$spec"
    name="R${R}_H${H}_C${C}"
    log "流 $name:R=$R H=$H(行) C=$C(行) ⇒ 跑 gem5 ..."
    python3 "$CFG_TOOL" "$CFG" "$BASE" "$H" "$C" "$R" || fail "流 $name 的段配置生成失败"
    # ⚠️ **必须传 `PARITY_CONFIG`** —— 探针默认读**脚本同目录**的
    #    `parity_config.json`(= 共享文件)⇒ 会绕过本脚本的临时副本
    #    ⇒ 实测踩过:副本里的 `segments` 没被探针读到 ⇒ 所有流退化成单区间
    #      ⇒ `h_m` 全变成 0.9974。
    PARITY_CONFIG="$CFG" \
    PARITY_OUT="$OUT_DIR/main_${name}.json" \
    PARITY_STATS="$OUT_DIR/main_${name}_stats.txt" \
        "$GEM5_BIN" "--stats-file=$OUT_DIR/main_${name}_stats.txt" "$PROBE" \
        > "$OUT_DIR/main_${name}_run.log" 2>&1 \
        || { tail -15 "$OUT_DIR/main_${name}_run.log"; fail "流 $name 的 gem5 回放失败"; }
    python3 "$REC_TOOL" "$OUT_DIR/main_${name}.json" "$name" "$STREAM_DIR" \
        || fail "流 $name 的结果提取失败"
done

# 汇总成**数组** JSON(⚠️ 早期版本字符串拼接 ⇒ 非法 JSON「Extra data」)
python3 "$ROOT/scripts/cep/parity/main_tier_collect.py" "$STREAM_DIR" "$STREAMS_JSON" \
    || fail "流结果汇总失败"

# ── 分析(扫 h_0 网格)────────────────────────────────────────────────
log "扫 h_0 网格 ⇒ H7 判定 ..."
set +e
python3 "$ANALYZE" --streams "$STREAMS_JSON" --out "$OUT_DIR/verdict_main.json"
rc=$?
set -e
log "产物:$STREAMS_JSON · $OUT_DIR/verdict_main.json"
exit $rc
