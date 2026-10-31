#!/usr/bin/env bash
# scripts/cep/parity/run_parity_linear.sh
#   —— H7 对拍基准的**参数同源档**端到端执行入口
#
# 流程(两侧**各自独立**按同一组参数生成访问序列):
#   ① 生成/复用配置(临时副本,避免污染共享 config)
#   ② gem5 侧:`PyTrafficGen.createLinear` 回放 ⇒ **duration 迭代校准**使实际条数
#      == 名义条数(见下)
#   ③ CppTLM 侧:按同一参数生成同一条序列 ⇒ 驱动 `TrafficGenTLM` + `CacheTLM`
#   ④ 比较器:访问数 / 读写条数 / **地址序列 FNV-1a 64bit** / 延迟常数 / P-5
#
# ⚠️ **为什么是「参数同源」而不是「文件同源」**:
#    gem5 v24 的 `ProtoInputStream` 用 protobuf 的 `IstreamInputStream`(零拷贝路径)
#    **逐条消息新建** `io::CodedInputStream` ⇒ **文件较大时提前 EOF**
#    (实测:1000 条读到 966 / 20000 条读到 19735 / 100000 条读到 98874;
#     同一文件用 Python 的 protobuf 库可完整读出 ⇒ 文件本身没问题)。
#    ⇒ 文件同源路径在 gem5 v24 侧**不可靠** ⇒ 改走「参数同源」。
#
# ⚠️ **为什么需要 duration 校准**:
#    gem5 v24 的 `data_limit` 在 **Release 构建下不生效** ⇒ 实际终止条件是 `duration`
#    ⇒ 条数 = `duration / 每条实际耗时`(受背压影响,实测 10ns ~ 165ns)
#    ⇒ **不能靠猜常数** ⇒ 按实测比例迭代校准。
#
# ⚠️ **口径边界**:验的是「同源访问流下的口径一致性」+「路径延迟常数」,
#    **不验证 H7** ⇒ **H7 仍未验证 · R7 维持 🟡 中**。
#    ⚠️ **P-2(平均访问延迟)/ P-3(命中率)是主对拍档判据 ⇒ 本档 N/A**。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="${PARITY_OUT_DIR:-$ROOT/build/parity}"
mkdir -p "$OUT_DIR"

GEM5_BIN="${GEM5_BIN:-$ROOT/external/gem5/build/NULL/gem5.opt}"
CFG_SHARED="$ROOT/scripts/cep/parity/parity_config.json"
CFG="$OUT_DIR/linear_config.json"
PROBE="$ROOT/scripts/cep/parity/gem5_linear_probe.py"
COMPARE="$ROOT/scripts/cep/parity/parity_compare.py"
CALIBRATE="$ROOT/scripts/cep/parity/main_tier_calibrate.py"

log() { printf '[run_parity_linear] %s\n' "$*"; }
fail() { printf '[run_parity_linear] ✗ %s\n' "$*" >&2; exit 1; }

# ① gem5 就绪(缺席 ⇒ ctest skip 77)
if [[ ! -x "$GEM5_BIN" ]]; then
    log "gem5 二进制不存在($GEM5_BIN)⇒ 跳过(构建入口:scripts/cep/parity/build_gem5.sh)"
    exit 77
fi
bash "$ROOT/scripts/cep/parity/build_gem5.sh" --check >/dev/null || fail "gem5 就绪检查失败"

# ② 配置副本(**不污染**共享 config)
cp "$CFG_SHARED" "$CFG"
# ⚠️ **显式设 `driver=linear`** —— 两档用**不同** driver(random = 主对拍档;
#    linear = 参数同源档)⇒ driver 由**各自的配置工具**决定,共享 config 只给默认值。
python3 - "$CFG" <<'PYEOF'
import json, pathlib, sys
p = pathlib.Path(sys.argv[1])
c = json.loads(p.read_text(encoding="utf-8"))
c["gem5"]["linear"]["driver"] = "linear"
p.write_text(json.dumps(c, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PYEOF

TARGET_ACCESSES="$(python3 -c "
import json
print(json.load(open('$CFG'))['gem5']['linear']['nominal_accesses'])
")"
log "参数同源档:名义条数=$TARGET_ACCESSES(≥1e5,per 14 号 §4.1)"

# ③ gem5 侧回放(+ duration 迭代校准)
CAL_OK=0
for attempt in $(seq 1 6); do
    log "gem5 侧回放(第 $attempt 次,条数校准) ..."
    PARITY_CONFIG="$CFG" \
    PARITY_OUT="$OUT_DIR/gem5_linear_probe.json" \
    PARITY_STATS="$OUT_DIR/gem5_linear_stats.txt" \
        "$GEM5_BIN" "--stats-file=$OUT_DIR/gem5_linear_stats.txt" "$PROBE" \
        > "$OUT_DIR/gem5_linear_run.log" 2>&1 \
        || { tail -12 "$OUT_DIR/gem5_linear_run.log"; fail "gem5 回放失败"; }
    ACTUAL="$(python3 -c "
import json
print(int(json.load(open('$OUT_DIR/gem5_linear_probe.json'))['accesses']))
")"
    if python3 "$CALIBRATE" "$CFG" "$ACTUAL" "$TARGET_ACCESSES"; then
        CAL_OK=1
        break
    fi
done
[[ $CAL_OK -eq 1 ]] || fail "条数校准 6 次仍未达标 ⇒ 拒绝出结论"
log "gem5 侧实际条数:$ACTUAL(校准后;本仓侧按**同一**条数生成 ⇒ 指纹逐条一致)"

# ④ CppTLM 侧(同一组参数 ⇒ 独立生成同一条序列)
log "CppTLM 侧回放(同一组参数)..."
PARITY_START_ADDR="$(python3 -c "
import json
print(json.load(open('$CFG'))['gem5']['linear']['start_addr'])
")" \
PARITY_END_ADDR="$(python3 -c "
import json
print(json.load(open('$CFG'))['gem5']['linear']['end_addr'])
")" \
PARITY_BLOCKSIZE="$(python3 -c "
import json
print(json.load(open('$CFG'))['gem5']['linear']['blocksize'])
")" \
PARITY_READ_PERCENT="$(python3 -c "
import json
print(json.load(open('$CFG'))['gem5']['linear']['read_percent'])
")" \
PARITY_NUM_ACCESSES="$ACTUAL" \
PARITY_HIT_NS="${PARITY_HIT_NS:-5}" \
PARITY_MISS_NS="${PARITY_MISS_NS:-50}" \
PARITY_VERIFICATION_MODE="$(python3 -c "
import json
print(json.load(open('$CFG'))['verification']['verification_mode'])
")" \
PARITY_VERIFICATION_SCOPE="$(python3 -c "
import json
print(json.load(open('$CFG'))['verification']['verification_scope'])
")" \
PARITY_OUT="$OUT_DIR/cpptlm_linear_probe.json" \
    "$ROOT/build/bin/cpptlm_tests" "[cep][parity]" \
    || fail "CppTLM 侧回放失败"

# ⑤ 比较
log "比较两侧 ..."
set +e
python3 "$COMPARE" \
    --gem5 "$OUT_DIR/gem5_linear_probe.json" \
    --cpptlm "$OUT_DIR/cpptlm_linear_probe.json" \
    --out "$OUT_DIR/verdict_linear.json"
rc=$?
set -e
log "产物:$OUT_DIR/{gem5_linear_probe.json,cpptlm_linear_probe.json,verdict_linear.json}"
exit $rc
