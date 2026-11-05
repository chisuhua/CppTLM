#!/usr/bin/env bash
# scripts/test/cep_b4_coupling_probe.sh
# H3 v2 的**耦合探针**(per ADR-CEP-06 SU#4 §② 约束 6)
#
# 功能:验证「跨平面阻塞」**没有平凡化**内存一致性测量 ——
#   · 两份**独立** metrics.json:阻塞开(h3 默认) vs 阻塞关(h3 variant=blocking_off);
#   · 比较 memory_violations 两桶(swmr / data_value)的 Δ;
#   · Δ 必须 = 0 或与**预测**方向一致(预测落独立锚文件,⛔ 不写在 metrics.json 里);
#   · 锚文件须**早于** run 产出(时间戳 + 实现 commit 祖先关系可查)。
#
# 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #226 / improvement cep-b4-evidence-device)
#
# 用法: bash scripts/test/cep_b4_coupling_probe.sh [--out <dir>]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BIN="build/bin/cpptlm_tests"
ANCHOR="experimental/predictions/h3_coupling_v2.json"
if [[ ! -x "$BIN" ]]; then
    echo "✗ 未找到 $BIN,请先构建: ./build.sh" >&2
    exit 2
fi

OUT="experimental/run-$(date +%Y%m%d-%H%M%S)-b4-coupling"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        -h|--help) sed -n '3,20p' "$0"; exit 0 ;;
        *) echo "✗ 未知参数: $1" >&2; exit 2 ;;
    esac
done

# ── ① 预测锚必须存在(⛔ 无锚不产出:事后补写预测不成立)────────────────────
if [[ ! -f "$ANCHOR" ]]; then
    echo "✗ 缺预测锚 $ANCHOR ⇒ 不得产出探针 run(per SU#4 §② 约束 6:预测须先于产出)" >&2
    exit 1
fi

# ── ② 锚的「实现 commit」须是 HEAD 的祖先(⇒ 锚指向的代码确实在树里)──────
impl_commit="$(python3 -c "import json;print(json.load(open('$ANCHOR'))['implementation_commit'])")"
if ! git merge-base --is-ancestor "$impl_commit" HEAD; then
    echo "✗ 预测锚的 implementation_commit($impl_commit)不是 HEAD 的祖先 ⇒ 锚与代码不对应" >&2
    exit 1
fi

# ── ③ 产两份独立 run(阻塞开 / 阻塞关)─────────────────────────────────────
mkdir -p "$OUT"
emit_run() {
    local dir="$1" variant="$2"
    CEP_EXP_DIR="$dir" CEP_EXP_ID="h3" CEP_EXP_VARIANT="$variant" \
        "$BIN" "[cep][exp][emit]" -r compact >/dev/null
    python3 - "$dir" <<'PY'
import json, sys
import h5py

d = sys.argv[1]
m = json.load(open(f"{d}/metrics.json", encoding="utf-8"))
with h5py.File(f"{d}/trace.h5", "w") as f:
    f.attrs["source"] = "cep_b4_coupling_probe"
    f.attrs["grain"] = "run-summary"
    grp = f.create_group("run_summary")
    for k, v in m.items():
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            grp.create_dataset(k, data=v)
PY
    python3 tools/render_report.py "$dir" >/dev/null
}

echo "── 探针:阻塞开(h3 默认)────────────────────────────────────"
emit_run "$OUT/blocking_on" ""
echo "── 探针:阻塞关(h3 variant=blocking_off)─────────────────────"
emit_run "$OUT/blocking_off" "blocking_off"

# ── ④ 锚须早于 run 产出(时间戳可查)─────────────────────────────────────
anchor_mtime="$(stat -c %Y "$ANCHOR")"
for d in "$OUT/blocking_on" "$OUT/blocking_off"; do
    run_mtime="$(stat -c %Y "$d/metrics.json")"
    if (( run_mtime < anchor_mtime )); then
        echo "✗ run 产出早于预测锚 ⇒ 预测是事后补写(不成立)" >&2
        exit 1
    fi
done

# ── ⑤ 比较两桶 Δ,并与预测核对 ────────────────────────────────────────────
python3 - "$ANCHOR" "$OUT/blocking_on/metrics.json" "$OUT/blocking_off/metrics.json" <<'PY'
import json, sys

anchor = json.load(open(sys.argv[1], encoding="utf-8"))
on = json.load(open(sys.argv[2], encoding="utf-8"))
off = json.load(open(sys.argv[3], encoding="utf-8"))

problems = []
for bucket in ("swmr", "data_value"):
    a = on["memory_violation_kinds"][bucket]
    b = off["memory_violation_kinds"][bucket]
    delta = b - a
    expected = anchor["expected_delta"].get(bucket, 0)
    print(f"  {bucket}: 阻塞开={a} 阻塞关={b} Δ={delta:+d}(预测 {expected:+d})")
    if delta != expected:
        problems.append(f"{bucket} 漂移 Δ={delta:+d} 与预测 {expected:+d} 不符")
if on["memory_violations"] != off["memory_violations"]:
    problems.append("memory_violations 总量在两配置下不一致")

if problems:
    for p in problems:
        print(f"  ✗ {p}", file=sys.stderr)
    sys.exit(1)
print("  ✓ 耦合探针通过:阻塞开/关两配置的内存违例计数与预测一致(无未解释漂移)")
PY

echo "✓ 耦合探针完成: $OUT(锚 $ANCHOR,实现 commit $impl_commit)"
