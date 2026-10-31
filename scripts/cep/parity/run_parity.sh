#!/usr/bin/env bash
# scripts/cep/parity/run_parity.sh
#   —— H7 对拍基准的**常数档(辅助档)**端到端执行入口
#
# 流程:
#   ① `build_gem5.sh --check` ⇒ gem5 二进制就绪(否则提示先构建)
#   ② 跑 gem5 探针(产出 gem5 侧 JSON + stats 文本)
#   ③ 生成 CppTLM 侧 JSON(参数抽取 + **P-5 字段注入**)
#   ④ 跑比较器(产出 verdict JSON + 人读结论)
#
# ⚠️ **口径边界(不得越界表述)**:
#   本脚本跑的是**辅助档(常数档)** ⇒ 验证的是「口径与建模语义」
#   (本仓「miss = hit + 存储延迟」分解 + cycle↔ns 换算),
#   **不验证 H7** ⇒ **H7 仍未验证 · R7 不降级**。
#   ⚠️ **P-2(平均访问延迟)/ P-3(命中率)是主对拍档判据 ⇒ 本档 N/A**。
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

# ③ CppTLM 侧 JSON(常数档:参数抽取 + P-5 字段注入)
# ⚠️ 常数档的 CppTLM 侧**没有**独立的 trace 回放 ⇒ 其结果由**参数抽取**给出
#    (`cpptlm_params.collect()`)⇒ **不含** `verification_mode` / `verification_scope`
#    ⇒ P-5 会红 ⇒ 必须在这里**显式注入**(两侧同源 ⇒ 取值一致)。
CPPTLM_JSON="$OUT_DIR/cpptlm_probe.json"
log "生成 CppTLM 侧结果(参数抽取 + P-5 字段注入)..."
python3 "$ROOT/scripts/cep/parity/parity_cpptlm_side.py" "$CPPTLM_JSON" \
    || fail "CppTLM 侧结果生成失败"

# ④ 比较器
VERDICT="$OUT_DIR/verdict.json"
log "跑比较器 ..."
set +e
python3 "$COMPARE" --gem5 "$PROBE_JSON" --cpptlm "$CPPTLM_JSON" --out "$VERDICT"
rc=$?
set -e
log "产物:$PROBE_JSON · $STATS_TXT · $CPPTLM_JSON · $VERDICT"
exit $rc
