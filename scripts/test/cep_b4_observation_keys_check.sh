#!/usr/bin/env bash
# scripts/test/cep_b4_observation_keys_check.sh
# H3 v2 观测键的**文件层**校验闸(per `ADR-CEP-06` SU#4 §② 约束 5 / AC-5 文件侧)
#
# 功能:产一个 h3 v2 run(4 件套)⇒ 校验观测键落 metrics.json **且** trace.h5,
#       且「同一事务流」实例级覆盖率为 1.0(退化装置 ⇒ 判红)。
# 作者: CppTLM Team / 日期: 2026-11-05(per 台账 Entry #226)
#
# ⚠️ **强度边界(逐字,per 台账 Entry #342 —— ⛔ 不得宣称本闸验证了 trace 内容)**:
#   本闸 = **文件层协议镜像校验** —— `metrics.json` 是 **cpptlm_tests 的真实产出**;
#   `trace.h5` 由**本脚本从 metrics.json 镜像合成**(`[synthetic-fixture]`)。
#   「真实 run 的 h5 层校验」归**实验 4 件套运行时**(`scripts/test/cep_experiment_run.sh`),
#   ⛔ 不归本闸。**为什么不能优先取真实 run**:retention = 丙(per `ADR-CEP-06` SU Entry #295)
#   ⇒ `.gitignore` 的 `experimental/*` 使 `trace.h5` **永不入库** ⇒ CI 里「优先真实 run」
#   的分支**恒不命中** ⇒ 那是半死代码(oracle MUST-5)⇒ 本闸只做**显式降级标注**。
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BIN="build/bin/cpptlm_tests"
if [[ ! -x "$BIN" ]]; then
    echo "✗ 未找到 $BIN,请先构建: ./build.sh" >&2
    exit 2
fi

OUT="experimental/run-$(date +%Y%m%d-%H%M%S)-b4-obskeys"
mkdir -p "$OUT/h3"

echo "[synthetic-fixture] trace.h5 由 metrics.json 镜像合成(非真实 run 产物;retention 丙 ⇒ experimental/* 不入库)⇒ 本闸强度 = 文件层协议镜像校验,真实 run 的 h5 层校验归实验 4 件套运行时"

CEP_EXP_DIR="$OUT/h3" CEP_EXP_ID="h3" "$BIN" "[cep][exp][emit]" -r compact >/dev/null
python3 - "$OUT/h3" <<'PY'
import json, sys
import h5py

d = sys.argv[1]
m = json.load(open(f"{d}/metrics.json", encoding="utf-8"))
with h5py.File(f"{d}/trace.h5", "w") as f:
    f.attrs["source"] = "cep_b4_observation_keys_check"
    f.attrs["grain"] = "run-summary"
    grp = f.create_group("run_summary")
    for k, v in m.items():
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            grp.create_dataset(k, data=v)
    obs = m.get("observation_keys") or {}
    if obs:
        og = grp.create_group("observation_keys")
        for k, v in obs.items():
            if isinstance(v, list):
                og.create_dataset(k, data=v)
PY

exec python3 scripts/test/cep_b4_observation_keys_check.py "$OUT/h3"
