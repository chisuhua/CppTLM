#!/usr/bin/env bash
# scripts/cep/parity/run_parity.sh
#   —— H7 对拍基准的**端到端执行入口**(gem5 探针 → 比较器 → verdict)
#
# 流程:
#   ① `build_gem5.sh --check` ⇒ gem5 二进制就绪(否则提示先构建)
#   ② 跑 gem5 探针(产出 gem5 侧 JSON + stats 文本)
#   ③ 跑比较器(产出 verdict JSON + 人读结论)
#
# 产物(默认落在 `$PARITY_OUT_DIR`,可用环境变量改):
#   gem5_probe.json      gem5 侧测量结果
#   gem5_stats.txt        gem5 原始 stats(gem5 `--stats-file` 产出)
#   verdict.json          比较器判据 verdict
#
# ⚠️ **口径边界(不得越界表述)**:
#   本脚本跑的是**辅助档(常数档)** ⇒ 验证的是「口径与建模语义」
#   (本仓「miss = hit + 存储延迟」分解 + cycle↔ns 换算),
#   **不验证 H7** ⇒ **H7 仍是未验证假设 · R7 维持 🟡 中**。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5/build/NULL/gem5.opt}"
PROBE="$ROOT/scripts/cep/parity/gem5_cache_probe.py"
COMPARE="$ROOT/scripts/cep/parity/parity_compare.py"

log() { printf '[run_parity] %s\n' "$*"; }
fail() { printf '[run_parity] ✗ %s\n' "$*" >&2; exit 1; }

# ① gem5 就绪检查
# ⚠️ gem5 是**第三方基准,不入库** ⇒ 缺席时用 ctest 的 SKIP_RETURN_CODE(77)
#    **跳过**而不是失败(常规 CI 不应因未装 gem5 而红)。
#    构建入口:scripts/cep/parity/build_gem5.sh
if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5 二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

# ② gem5 探针
PROBE_JSON="$OUT_DIR/gem5_probe.json"
STATS_TXT="$OUT_DIR/gem5_stats.txt"
log "跑 gem5 探针(变体 NULL)..."
PARITY_OUT="$PROBE_JSON" PARITY_STATS="$STATS_TXT" \
    "$GEM5_BIN" "--stats-file=$STATS_TXT" "$PROBE" > "$OUT_DIR/gem5_run.log" 2>&1 \
    || { tail -20 "$OUT_DIR/gem5_run.log"; fail "gem5 探针失败(见 $OUT_DIR/gem5_run.log)"; }
[[ -s "$PROBE_JSON" ]] || fail "探针未产出结果 JSON"

# ③ 比较器
VERDICT="$OUT_DIR/verdict.json"
log "跑比较器 ..."
set +e
python3 "$COMPARE" --gem5 "$PROBE_JSON" --out "$VERDICT"
rc=$?
set -e
log "产物:$PROBE_JSON · $STATS_TXT · $VERDICT"
exit $rc
