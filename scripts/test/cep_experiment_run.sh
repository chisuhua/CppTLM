#!/usr/bin/env bash
# scripts/test/cep_experiment_run.sh
# H2 / H3 / H4 / H6 实验的 4 件套产出与契约校验
#
# 作者: CppTLM Team / 日期: 2026-11-04(2026-11-05 增 H4,per 台账 Entry #222)
#
# 用法:
#   bash scripts/test/cep_experiment_run.sh [--out <dir>]
#
# 链路(runner 契约 per tools/run_sweep.py 文件头):
#   C++ 产 simulation_config.json + metrics.json
#     → 本脚本用 h5py 补 trace.h5(真实 HDF5,语义见文件内注释)
#     → tools/render_report.py 产 report.html
#     → render_report.py --check 校验 4 件套
#
# ⚠️ 产物目录默认在 experimental/ 下,该目录被 .gitignore 忽略(产物不入库)。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BIN="build/bin/cpptlm_tests"
if [[ ! -x "$BIN" ]]; then
    echo "✗ 未找到 $BIN,请先构建: ./build.sh" >&2
    exit 2
fi

OUT="experimental/run-$(date +%Y%m%d-%H%M%S)-h2h3h4h6"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "✗ 未知参数: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$OUT"

emit_trace() {
    # 用 h5py 写**真实** HDF5。内容为 run 级摘要(本模型不产逐周期信号),
    # 并在 attributes 中显式标注来源 ⇒ 不冒充 cycle-level trace(per ADR-CEP-06 §3 诚实纪律)
    local exp_dir="$1" exp_id="$2"
    python3 - "$exp_dir" "$exp_id" <<'PY'
import json, sys, h5py

exp_dir, exp_id = sys.argv[1], sys.argv[2]
metrics = json.load(open(f"{exp_dir}/metrics.json", encoding="utf-8"))

with h5py.File(f"{exp_dir}/trace.h5", "w") as f:
    f.attrs["source"] = "cep_experiment_runner"
    f.attrs["experiment_id"] = exp_id
    f.attrs["grain"] = "run-summary"
    f.attrs["note"] = (
        "H2/H3/H4/H6 为模型级实验,不产逐周期信号;本数据集为 run 级摘要,"
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
PY
}

status=0
for exp in h2 h3 h4 h6; do
    dir="$OUT/$exp"
    echo "── $exp ─────────────────────────────────────────────"
    CEP_EXP_DIR="$dir" CEP_EXP_ID="$exp" "$BIN" "[cep][exp][emit]" -r compact >/dev/null
    emit_trace "$dir" "$exp"
    python3 tools/render_report.py "$dir" >/dev/null
    if python3 tools/render_report.py "$dir" --check >/dev/null; then
        echo "  ✓ 4 件套 + 契约校验通过: $dir"
    else
        echo "  ✗ 契约校验失败: $dir" >&2
        status=1
    fi
    for f in simulation_config.json trace.h5 metrics.json report.html; do
        [[ -f "$dir/$f" ]] || { echo "  ✗ 缺 $dir/$f" >&2; status=1; }
    done
done

if [[ $status -eq 0 ]]; then
    echo "✓ 全部实验产出完成: $OUT"
else
    echo "✗ 存在失败项" >&2
fi
exit $status
