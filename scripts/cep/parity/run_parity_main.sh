#!/usr/bin/env bash
# scripts/cep/parity/run_parity_main.sh
#   —— H7 **主对拍档(固定命中率档)**端到端执行入口
#
# 流程(per `14 号` §4.4 的 2026-10-31 裁决 —— 口径 A):
#   ① **多流扫描**:用 gem5 的 `createRandom`(均匀随机)扫**不同工作集大小 W**
#      ⇒ 每条流**自然**产生一个实测命中率 `h_m` 与实测平均延迟;
#      (⚠️ **不瞄准**某个 `h` —— 瞄准会让「统一 h_0」必然成立 ⇒ 判据失去鉴别力)
#   ② 汇总各流结果 ⇒ `main_tier_streams.json`
#   ③ 分析器 `parity_main_tier.py`:用**估算规则 R-B**(`h_0 = max(0, 1 − W/C)`)
#      估 `h_0` ⇒ 与 gem5 实测比:
#      · **P-3**:`|h_估 − h_m| ≤ 5 个百分点`(估算误差)
#      · **P-2**:`|L_model(h_0) − L_gem5| / L_gem5 < 15%`(H7 判定)
#
# ⚠️ **口径边界**:本档验的是「**固定命中率参考模型**在**可估算**的命中率下能否保持
#   <15% 的延迟偏差」⇒ 是 **H7 的直接判定**;但它**不能**被表述为
#   「stub cache 的现实性已验证」(结构性差异仍在)。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5/build/NULL/gem5.opt}"
CFG_SHARED="$ROOT/scripts/cep/parity/parity_config.json"
CFG="$OUT_DIR/main_tier_config.json"
PROBE="$ROOT/scripts/cep/parity/gem5_linear_probe.py"
ANALYZE="$ROOT/scripts/cep/parity/parity_main_tier.py"
REC_TOOL="$ROOT/scripts/cep/parity/main_tier_stream_rec.py"

log() { printf '[run_parity_main] %s\n' "$*"; }
fail() { printf '[run_parity_main] ✗ %s\n' "$*" >&2; exit 1; }

if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5 二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

# ── 流定义:扫**工作集大小 W**(byte)────────────────────────────────
# ⚠️ gem5 侧 cache = 16KiB / 2-way / LRU ⇒ W ≫ C ⇒ 随机访问下命中率 ≈ 1 − C/W
#    ⇒ 扫 W ⇒ 得到**连续分散**的 `h_m`。
#    ⇒ ⚠️ **必须包含「W 远小于 C」**(命中率 ~1)与「W ≫ C」(命中率 ~0)两个极端,
#      以及中间若干点 ⇒ 否则「跨流泛化」无鉴别力。
CACHE_BYTES=16384
BASE=0x100000
STREAM_SPECS=(
    "4096"       # W = 4KiB   ⇒ h_m ≈ 0.75
    "8192"       # W = 8KiB   ⇒ h_m ≈ 0.5
    "16384"      # W = 16KiB  ⇒ h_m ≈ 0
    "32768"      # W = 32KiB  ⇒ h_m ≈ 0
    "131072"     # W = 128KiB ⇒ h_m ≈ 0
)

STREAM_DIR="$OUT_DIR/main_streams"
STREAMS_JSON="$OUT_DIR/main_tier_streams.json"
rm -rf "$STREAM_DIR" && mkdir -p "$STREAM_DIR"

for spec in "${STREAM_SPECS[@]}"; do
    W="$spec"
    name="W${W}"
    ok=0
    for attempt in $(seq 1 5); do
        log "流 $name:W=$W B ⇒ 跑 gem5(第 $attempt 次,预算自适应) ..."
        cp "$CFG_SHARED" "$CFG"
        # 写该流的配置(random driver + 该工作集 + 名义条数)
        python3 "$ROOT/scripts/cep/parity/main_tier_stream_cfg.py" \
            "$CFG" "$BASE" "$W" "$CACHE_BYTES" || fail "流 $name 的配置生成失败"
        PARITY_CONFIG="$CFG" \
        PARITY_OUT="$OUT_DIR/main_${name}.json" \
        PARITY_STATS="$OUT_DIR/main_${name}_stats.txt" \
            "$GEM5_BIN" "--stats-file=$OUT_DIR/main_${name}_stats.txt" "$PROBE" \
            > "$OUT_DIR/main_${name}_run.log" 2>&1 \
            || { tail -8 "$OUT_DIR/main_${name}_run.log"; fail "流 $name 的 gem5 回放失败"; }
        # 提取器自带**序列完整性硬断言** ⇒ 截断时非零退出 ⇒ 加倍预算重试
        if python3 "$REC_TOOL" "$OUT_DIR/main_${name}.json" "$name" "$STREAM_DIR"; then
            ok=1
            break
        fi
        log "  ⚠️ 流 $name 序列被截断 ⇒ tick 预算加倍后重跑 ..."
        python3 "$ROOT/scripts/cep/parity/main_tier_budget.py" "$CFG"
    done
    [[ $ok -eq 1 ]] || fail "流 $name 重试 5 次后序列仍被截断 ⇒ 拒绝出结论"
done

# 汇总成**数组** JSON
python3 "$ROOT/scripts/cep/parity/main_tier_collect.py" "$STREAM_DIR" "$STREAMS_JSON" \
    || fail "流结果汇总失败"

# ── 分析(R-B 估算规则 + P-2/P-3 判据)──────────────────────────────
log "按 R-B 规则估算 h_0 ⇒ P-2 / P-3 判定 ..."
set +e
python3 "$ANALYZE" --streams "$STREAMS_JSON" --out "$OUT_DIR/verdict_main.json"
rc=$?
set -e
log "产物:$STREAMS_JSON · $OUT_DIR/verdict_main.json"
exit $rc
