#!/usr/bin/env bash
# scripts/cep/parity/run_parity_linear.sh
#   —— H7 对拍基准的**参数同源**端到端执行入口
#
# 流程(两侧**各自独立**按同一组参数生成访问序列):
#   ① gem5 侧:`PyTrafficGen.createLinear`(地址序列无 RNG;`read_percent=100` ⇒ 读写也无 RNG)
#   ② CppTLM 侧:按同一参数生成同一条序列 ⇒ 驱动 `TrafficGenTLM` + `CacheTLM`
#   ③ 比较器:访问数 / 读写条数 / **地址序列 FNV-1a 64bit** / 延迟常数
#
# ⚠️ **为什么用「参数同源」而不是「文件同源」**:
#    gem5 v24 的 `ProtoInputStream` 用 protobuf 的 `IstreamInputStream`(零拷贝路径)
#    **逐条消息新建** `io::CodedInputStream` ⇒ **文件较大时提前 EOF**
#    (实测:1000 条读到 966 / 20000 条读到 19735 / 100000 条读到 98874;
#     同一文件用 Python 的 protobuf 库可完整读出 ⇒ 文件本身没问题)。
#    ⇒ 文件同源路径在 gem5 v24 侧不可靠(登记为已知限制,per 台账 Entry #115)。
#    ⇒ 「参数同源」反而是**更强**的证据:两侧**独立生成**的序列逐条相同。
#
# ⚠️ **口径边界**:验的是「同源访问流下的口径一致性」+「路径延迟常数」,
#    **不验证 H7** ⇒ **H7 仍未验证 · R7 维持 🟡 中**。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5/build/NULL/gem5.opt}"
CFG="$ROOT/scripts/cep/parity/parity_config.json"

log() { printf '[run_parity_linear] %s\n' "$*"; }
fail() { printf '[run_parity_linear] ✗ %s\n' "$*" >&2; exit 1; }

# ① gem5 就绪(缺席 ⇒ ctest skip 77)
if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5 二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

# ② 参数(**唯一真源 = parity_config.json**;两侧都从这里读 ⇒ 口径必然一致)
read_cfg() {
    python3 -c "
import json
c = json.load(open('$CFG'))
lin = c['gem5']['linear']
print(lin['$1'])
"
}
START_ADDR="$(read_cfg start_addr)"
END_ADDR="$(read_cfg end_addr)"
BLOCKSIZE="$(read_cfg blocksize)"
READ_PERCENT="$(read_cfg read_percent)"
DATA_LIMIT="$(read_cfg data_limit)"
NUM_ACCESSES=$(( DATA_LIMIT / BLOCKSIZE ))
log "参数:start=$START_ADDR end=$END_ADDR blk=$BLOCKSIZE reads=$READ_PERCENT% "
log "      条数=$NUM_ACCESSES(≥1e5,per 14 号 §4.1)"

# ③ gem5 侧
log "gem5 侧回放(参数同源)..."
PARITY_OUT="$OUT_DIR/gem5_linear_probe.json" \
PARITY_STATS="$OUT_DIR/gem5_linear_stats.txt" \
    "$GEM5_BIN" "--stats-file=$OUT_DIR/gem5_linear_stats.txt" \
    "$ROOT/scripts/cep/parity/gem5_linear_probe.py" \
    > "$OUT_DIR/gem5_linear_run.log" 2>&1 \
    || { tail -20 "$OUT_DIR/gem5_linear_run.log"; fail "gem5 回放失败"; }

# ④ CppTLM 侧(同一组参数 ⇒ 独立生成同一条序列)
log "CppTLM 侧回放(同一组参数)..."
PARITY_START_ADDR="$START_ADDR" \
PARITY_END_ADDR="$END_ADDR" \
PARITY_BLOCKSIZE="$BLOCKSIZE" \
PARITY_READ_PERCENT="$READ_PERCENT" \
PARITY_DATA_LIMIT="$DATA_LIMIT" \
PARITY_HIT_NS="${PARITY_HIT_NS:-5}" \
PARITY_MISS_NS="${PARITY_MISS_NS:-50}" \
PARITY_OUT="$OUT_DIR/cpptlm_linear_probe.json" \
    "$ROOT/build/bin/cpptlm_tests" "[cep][parity]" \
    || fail "CppTLM 侧回放失败"

# ⑤ 比较
log "比较两侧 ..."
set +e
python3 "$ROOT/scripts/cep/parity/parity_compare.py" \
    --gem5 "$OUT_DIR/gem5_linear_probe.json" \
    --cpptlm "$OUT_DIR/cpptlm_linear_probe.json" \
    --out "$OUT_DIR/verdict_linear.json"
rc=$?
set -e
log "产物:$OUT_DIR/{gem5_linear_probe.json,cpptlm_linear_probe.json,verdict_linear.json}"
exit $rc
