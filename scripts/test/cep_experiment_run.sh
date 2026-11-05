#!/usr/bin/env bash
# scripts/test/cep_experiment_run.sh
# H2 / H3 / H4 / H6 实验的 4 件套产出与契约校验
#
# 作者: CppTLM Team / 日期: 2026-11-04(2026-11-05 增 H4,per 台账 Entry #222)
#
# 用法:
#   bash scripts/test/cep_experiment_run.sh [--out <dir>]
#   bash scripts/test/cep_experiment_run.sh --selftest   # 只跑「4 件套完整性」自检(ctest 用,不产实验)
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

OUT="experimental/run-$(date +%Y%m%d-%H%M%S)-h2h3h4h6"
SELFTEST=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --selftest) SELFTEST=1; shift ;;
        -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
        *) echo "✗ 未知参数: $1" >&2; exit 2 ;;
    esac
done

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
            if k == "observation_keys":
                continue  # ⚠️ 由下方专用分支处理(值是列表,通用分支只写数值叶子)
            sub = grp.create_group(k)
            for kk, vv in v.items():
                if isinstance(vv, (int, float)) and not isinstance(vv, bool):
                    sub.create_dataset(kk, data=vv)
    grp.attrs["experiment_id"] = exp_id
    # ── 观测键镜像(per ADR-CEP-06 SU#4 §② 约束 5)────────────────────────
    #   观测键是「同一事务流」判据的**机器锚**载体 ⇒ 必须随 4 件套落 trace.h5,
    #   否则文件层无法校验(⛔ 不得只留在 metrics.json 顶层而被 trace 丢掉)。
    obs = metrics.get("observation_keys") or {}
    if obs:
        og = grp.create_group("observation_keys")
        for k, v in obs.items():
            if isinstance(v, list):
                og.create_dataset(k, data=v)
        og.attrs["note"] = (
            "事务级/地址级观测键(SU#4 §② 约束 5):同一事务流判据的机器锚;grain=run-summary"
        )
PY
}

# ── INV-CARRIER: INV-CEP-06-IC1 ──────────────────────────────────────────
# `ADR-CEP-06 §4` IC1:每次 CEP 实验产出 4 件套,缺一件 = 实验 fail。
# 变红理由(per ADR-CEP-06 SU#4 规则 B):齐备却被判缺件 / 缺任一却判通过 ⇒ 自检 exit 1。
FOUR_ARTIFACTS=(simulation_config.json trace.h5 metrics.json report.html)

check_four_artifacts() {
    local dir="$1" missing=0 f
    for f in "${FOUR_ARTIFACTS[@]}"; do
        if [[ ! -f "$dir/$f" ]]; then
            echo "  ✗ 缺 $dir/$f" >&2
            missing=1
        fi
    done
    return $missing
}

# 反向 fixture(判据的**可执行**证据):齐备 ⇒ 通过;缺任一 ⇒ 失败
selftest_four_artifacts() {
    local tmp rc=0 f
    tmp="$(mktemp -d)"
    for f in "${FOUR_ARTIFACTS[@]}"; do : > "$tmp/$f"; done
    if ! check_four_artifacts "$tmp" >/dev/null 2>&1; then
        echo "  ✗ 反向 fixture 失败:4 件套齐备却被判缺件" >&2
        rc=1
    fi
    for f in "${FOUR_ARTIFACTS[@]}"; do
        rm -f "$tmp/$f"
        if check_four_artifacts "$tmp" >/dev/null 2>&1; then
            echo "  ✗ 反向 fixture 失败:缺 $f 却判通过(『缺一件 = fail』失守)" >&2
            rc=1
        fi
        : > "$tmp/$f"
    done
    rm -rf "$tmp"
    return $rc
}

if [[ "$SELFTEST" -eq 1 ]]; then
    if selftest_four_artifacts; then
        echo "✓ 4 件套完整性闸自检通过(齐备 ⇒ 通过;缺任一 ⇒ 失败)"
        exit 0
    fi
    echo "✗ 4 件套完整性闸自检失败" >&2
    exit 1
fi

BIN="build/bin/cpptlm_tests"
if [[ ! -x "$BIN" ]]; then
    echo "✗ 未找到 $BIN,请先构建: ./build.sh" >&2
    exit 2
fi

mkdir -p "$OUT"

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
    check_four_artifacts "$dir" || status=1
    # ── 文件层断言(AC-5 的文件侧):h3 的观测键须落 metrics.json **且** trace.h5 ──
    if [[ "$exp" == "h3" ]]; then
        if ! python3 scripts/test/cep_b4_observation_keys_check.py "$dir"; then
            status=1
        fi
    fi
done

if [[ $status -eq 0 ]]; then
    echo "✓ 全部实验产出完成: $OUT"
else
    echo "✗ 存在失败项" >&2
fi
exit $status
