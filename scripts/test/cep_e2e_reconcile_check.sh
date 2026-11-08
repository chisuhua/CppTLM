#!/usr/bin/env bash
# scripts/test/cep_e2e_reconcile_check.sh
# **双向闸**(per 台账 Entry #295 裁定 2 = retention **丙**;Entry #303)
#
# 目的:让「台账结论」首次**可对账** —— 台账条目 ↔ 仓内**已提交**的 run 产物。
#   ① **零 orphan**:仓内每份**已提交**的 `experimental/run-*/metrics.json`
#      ⇒ 必须**恰好**对应 `test/cep_e2e_runs.jsonl` 的**一条**条目;
#   ② **反向零 orphan**:jsonl 的每条条目 ⇒ 其 `metrics_path` 必须**存在且已被 git 跟踪**;
#   ③ **sha256 相等**:条目的 `metrics_sha256` == 仓内文件的 sha256;
#   ④ **7 字段齐备**:`run_id` / `config_sha256` / `metrics_sha256` / `metrics_13`
#      / `verification_mode` + `verification_scope` / `ic3_verdict` / `hypothesis_id`
#      / `topology_tag`(= 7 字段口径,`verification_mode` 与 `verification_scope` 合为一项);
#   ⑤ **verdict 三态词表**:✓ 通过 / ❌ 违例 fail / ⚪ 不适用(⛔ 不得二值「绿 / 红」)。
#
# ⚠️ **豁免边界**(per Entry #295 ⑤):历史 127 run **grandfather** ⇒
#   本闸只覆盖**本 change 首批入库**的 run(清单 = `test/cep_e2e_runs.jsonl`);
#   历史 run **不入库** ⇒ 不产生 orphan。⛔ 若将来批量回填,须同步扩本闸的覆盖口径。
#
# 用法:bash scripts/test/cep_e2e_reconcile_check.sh
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

LEDGER="test/cep_e2e_runs.jsonl"
problems=0

fail() { echo "✗ $*" >&2; problems=$((problems + 1)); }

if [[ ! -f "$LEDGER" ]]; then
  echo "✗ 缺少台账清单: $LEDGER" >&2
  exit 2
fi

# ── ① 仓内已提交的 run 产物(只认 **git 跟踪** 的)──
mapfile -t tracked_metrics < <(git ls-files 'experimental/run-*/metrics.json')
mapfile -t tracked_configs < <(git ls-files 'experimental/run-*/simulation_config.json')

if [[ ${#tracked_metrics[@]} -eq 0 ]]; then
  echo "✓ 无已提交的 run 产物 ⇒ 双向闸空真通过(首批入库前状态)"
  exit 0
fi

# ── ② 台账条目解析 + 逐项核对 ──
python3 - "$LEDGER" "$problems" <<'PY'
import hashlib, json, os, subprocess, sys

ledger_path = sys.argv[1]
rows = []
with open(ledger_path, encoding='utf-8') as fh:
    for lineno, line in enumerate(fh, 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        try:
            rows.append((lineno, json.loads(line)))
        except json.JSONDecodeError as exc:
            print(f"✗ {ledger_path}:{lineno} 不是合法 JSON: {exc}", file=sys.stderr)
            sys.exit(1)

problems = 0
required = ["run_id", "config_sha256", "metrics_sha256", "metrics_13",
            "verification_mode", "verification_scope", "ic3_verdict",
            "hypothesis_id", "topology_tag"]
verdicts = {"✓ 通过", "❌ 违例 fail", "⚪ 不适用"}

by_metrics_path = {}
for lineno, row in rows:
    for key in required:
        if key not in row or row[key] in (None, ""):
            print(f"✗ {ledger_path}:{lineno} 缺 7 字段之一: {key}", file=sys.stderr)
            problems += 1
    if row.get("ic3_verdict") not in verdicts:
        print(f"✗ {ledger_path}:{lineno} ic3_verdict 非三态词表: {row.get('ic3_verdict')!r}",
              file=sys.stderr)
        problems += 1
    m13 = row.get("metrics_13")
    if not isinstance(m13, dict) or not m13:
        print(f"✗ {ledger_path}:{lineno} metrics_13 必须是非空对象(逐键结论)", file=sys.stderr)
        problems += 1
    else:
        for key, val in m13.items():
            if val == "not-collected" and not row.get("metrics_13_reasons", {}).get(key):
                print(f"✗ {ledger_path}:{lineno} metrics_13[{key}]=not-collected 但无 reason",
                      file=sys.stderr)
                problems += 1
    path = row.get("metrics_path", "")
    if not path:
        print(f"✗ {ledger_path}:{lineno} 缺 metrics_path", file=sys.stderr)
        problems += 1
        continue
    if path in by_metrics_path:
        print(f"✗ {ledger_path}:{lineno} metrics_path 重复: {path}", file=sys.stderr)
        problems += 1
    by_metrics_path[path] = (lineno, row)

tracked = subprocess.run(["git", "ls-files", "experimental/run-*/metrics.json"],
                         capture_output=True, text=True).stdout.split()
for path in tracked:
    if path not in by_metrics_path:
        print(f"✗ orphan: 仓内已提交 {path} 但台账无对应条目", file=sys.stderr)
        problems += 1

for path, (lineno, row) in by_metrics_path.items():
    if not os.path.isfile(path):
        print(f"✗ {ledger_path}:{lineno} metrics_path 不存在: {path}", file=sys.stderr)
        problems += 1
        continue
    tracked_hit = subprocess.run(["git", "ls-files", "--error-unmatch", path],
                                 capture_output=True, text=True)
    if tracked_hit.returncode != 0:
        print(f"✗ {ledger_path}:{lineno} metrics_path 未被 git 跟踪: {path}", file=sys.stderr)
        problems += 1
        continue
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
    if digest != row.get("metrics_sha256"):
        print(f"✗ {ledger_path}:{lineno} metrics_sha256 不等: 期望 {row.get('metrics_sha256')} 实得 {digest}",
              file=sys.stderr)
        problems += 1

print(f"[reconcile] 台账条目 {len(rows)} · 仓内已提交 run {len(tracked)} · 问题 {problems}")
sys.exit(1 if problems else 0)
PY
rc=$?
if [[ $rc -ne 0 ]]; then
  fail "双向闸未通过(见上)"
  exit 1
fi

echo "✓ CEP E2E 双向闸通过(台账 ↔ 仓内 run 产物)"
exit 0
