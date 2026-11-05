#!/usr/bin/env bash
# scripts/test/cep_h8_ab_runs.sh
# H8 的 3 组 A/B run 产出 + 工具链结论(per Entry #224 ⑤ H8-A/B-1/2/3 + Task C)
#
# 作者: CppTLM Team / 日期: 2026-11-05(H8,B7 的最后一块)
#
# 组别(per Entry #224 ⑤ H8-A/B-2 —— 3 组装置各一对配置,A/B 变体**不进 exp_id**):
#   组 1  h6(VC 模型)    A = iso(隔离,默认)      B = shared(共享)
#   组 2  h4(完成域模型) A = correct(正确,默认)  B = mislabel(错标)
#   组 3  h2(目录策略)   A = fullmap(集中)       B = broadcast(广播)
#
# 链路(与 cep_experiment_run.sh 同构):
#   C++ 产 simulation_config.json + metrics.json
#     → h5py 补 trace.h5(run 级摘要,不冒充 cycle-level)
#     → tools/render_report.py 产 report.html
#     → render_report.py --check 逐 run 校验(**预期 verdict** 见下面 RUNS 表)
#     → tools/ab_compare.py(compare 模式)产 delta_metrics.json + ab_compare.html
#     → tools/recommend.py 产建议(结论文本落 conclusions.txt)
#
# ⚠️ 产物目录默认在 experimental/ 下(被 .gitignore 忽略,产物不入库)。
# ⚠️ g1b / g2b 的 --check **预期红**(实测死锁违例 = 被淘汰配置的实测证据,per SU#3
#    违例态;**不是**装置缺陷、也**不是**闸失效)—— 本脚本断言该红**因正确原因红**。
#    g3a / g3b 预期绿:违例类 both not-collected ⇒ IC3「不适用」(per SU#1 §④ ⑤)。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BIN="build/bin/cpptlm_tests"
if [[ ! -x "$BIN" ]]; then
    echo "✗ 未找到 $BIN,请先构建: ./build.sh" >&2
    exit 2
fi

OUT="experimental/run-$(date +%Y%m%d-%H%M%S)-h8-ab"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        -h|--help) sed -n '10,32p' "$0"; exit 0 ;;
        *) echo "✗ 未知参数: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$OUT"

emit_trace() {
    # 用 h5py 写**真实** HDF5;run 级摘要(per ADR-CEP-06 §3 诚实纪律,同 cep_experiment_run.sh)
    local exp_dir="$1" exp_id="$2" variant="$3"
    python3 - "$exp_dir" "$exp_id" "$variant" <<'PY'
import json, sys, h5py

exp_dir, exp_id, variant = sys.argv[1], sys.argv[2], sys.argv[3]
metrics = json.load(open(f"{exp_dir}/metrics.json", encoding="utf-8"))

with h5py.File(f"{exp_dir}/trace.h5", "w") as f:
    f.attrs["source"] = "cep_experiment_runner"
    f.attrs["experiment_id"] = exp_id
    f.attrs["exp_variant"] = variant
    f.attrs["grain"] = "run-summary"
    f.attrs["note"] = (
        "H8 A/B(Entry #224 ⑤ 组 1/2/3)为模型级实验,不产逐周期信号;本数据集为 run 级摘要,"
        "不冒充 cycle-level trace(per ADR-CEP-06 §3)"
    )
    grp = f.create_group("run_summary")
    for k, v in metrics.items():
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            grp.create_dataset(k, data=v)
        elif isinstance(v, dict):
            sub = grp.create_group(k)
            for kk, vv in v.items():
                if isinstance(vv, (int, float)) and not isinstance(vv, bool):
                    sub.create_dataset(kk, data=vv)
    grp.attrs["experiment_id"] = exp_id
    grp.attrs["exp_variant"] = variant
PY
}

# run 规格:dir|device|variant|期望 --check verdict(0=绿,1=红)|说明
#   预期红的判SetText:f 死锁违例的 run(g1b / g2b)= 被淘汰配置的实测证据
RUNS=(
  "g1a|h6|iso|0|A 侧:VC 隔离(既有 h6 行为,死锁=0)"
  "g1b|h6|shared|1|B 侧:VC 共享(实测死锁=1 = 被淘汰配置的实测证据;预期红)"
  "g2a|h4|correct|0|A 侧:完成域正确(既有 h4 行为,死锁=0)"
  "g2b|h4|mislabel|1|B 侧:错标 group-scoped(实测死锁=1 = 误分类的代价;预期红)"
  "g3a|h2|fullmap|0|A 侧:FullMap(集中;违例类未采集 ⇒ IC3 不应用 ⇒ 绿)"
  "g3b|h2|broadcast|0|B 侧:BroadcastNull(广播;同上 ⇒ 绿)"
)

status=0
CONCL="$OUT/conclusions.txt"
: > "$CONCL"
declare -A DIR_OF

for spec in "${RUNS[@]}"; do
    IFS='|' read -r rid exp variant expect_check note <<<"$spec"
    dir="$OUT/$rid"
    DIR_OF[$rid]="$dir"
    echo "── $rid (device=$exp variant=$variant) ─────────────────────"
    echo "   $note"
    CEP_EXP_DIR="$dir" CEP_EXP_ID="$exp" CEP_EXP_VARIANT="$variant" \
        "$BIN" "[cep][exp][emit]" -r compact >/dev/null
    emit_trace "$dir" "$exp" "$variant"
    # render:违例 run 的 report.html 照写但退出 1(横幅)⇒ 显式捕获,不因 set -e 中断
    if python3 tools/render_report.py "$dir" >/dev/null 2>&1; then
        render_rc=0
    else
        render_rc=$?
    fi
    # --check 实际 verdict:红 = 输出含死锁/一致性违例描述
    check_out="$(python3 tools/render_report.py "$dir" --check 2>&1 || true)"
    check_rc=0
    if echo "$check_out" | grep -q "死锁违例\|一致性违例"; then
        check_rc=1
    fi
    if [[ "$check_rc" == "$expect_check" ]]; then
        echo "  ✓ 4 件套产出完成(--check verdict: $([[ $expect_check == 0 ]] && echo 绿 || echo 红·违例预期))"
    else
        echo "  ✗ --check 预期 verdict 不符:期望 $expect_check 实得 $check_rc" >&2
        echo "$check_out" | head -5 >&2
        status=1
    fi
    for f in simulation_config.json trace.h5 metrics.json report.html; do
        [[ -f "$dir/$f" ]] || { echo "  ✗ 缺 $dir/$f" >&2; status=1; }
    done
    echo "   (render 退出码 $render_rc —— 违例 run 预期 1)"
done

# ── ab_compare + recommend(per pair;--check 是独立闸不在此用)────────────────
for pair in \
    "g1a g1b 组1-死锁风险vs带宽效率" \
    "g2a g2b 组2-误分类的代价" \
    "g3a g3b 组3-目录开销vs一致性覆盖"; do
    set -- $pair
    a="${DIR_OF[$1]}"
    b="${DIR_OF[$2]}"
    label="$3"
    out="$OUT/compare-$1-vs-$2"
    python3 tools/ab_compare.py "$a" "$b" -o "$out" >/dev/null
    rec="$(python3 tools/recommend.py "$a" "$b" 2>&1)"
    rec_rc=$?
    {
        echo "── $label($a vs $b)"
        echo "recommend(exit $rec_rc): $rec"
        echo "ab_compare: delta_metrics.json + ab_compare.html → $out"
        echo ""
    } >> "$CONCL"
    echo "  ✓ $label:ab_compare + recommend(exit $rec_rc)"
done

if [[ $status -eq 0 ]]; then
    echo "✓ H8 的 3 组 A/B 全部产出: $OUT(建议见 $CONCL)"
else
    echo "✗ 存在失败项" >&2
fi
exit $status
