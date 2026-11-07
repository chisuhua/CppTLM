#!/usr/bin/env bash
# scripts/test/cep_b3b2_d4_quarterly_review.sh
#   —— `B3b-2` 的 **D-4 触发探测**季度复审脚本
#      (per 台账 Entry #274 §④ E1 + Entry #277 的「cadence 断言**不进 ctest**」)
#
# 用法:
#   bash scripts/test/cep_b3b2_d4_quarterly_review.sh              # 校验 + 决定性变异
#   bash scripts/test/cep_b3b2_d4_quarterly_review.sh --validate-only   # 只校验(供变异段复用)
#   bash scripts/test/cep_b3b2_d4_quarterly_review.sh --record      # 追加一条 run-record
#
# ⚠️ **本脚本刻意 NOT 注册 ctest** —— 若把「≤100 天未执行即红」放进 ctest,
#    则 100 天后**所有无关 PR 都会红**(CI 噪声)⇒ cadence 断言只在本脚本里。
#    (per 台账 Entry #277 §⑥②)
#
# 检查 4 组 + 3 个决定性变异:
#   ① **run-record schema**:key 白名单 + outcome 三态(+ 基线行专有的 `not_yet_run`)
#      + incident 长度上限(300)⇒ **正文没有落点**(与探测清单同款纪律)
#   ② **cadence**:最近一次**真实执行**(outcome ∈ {hit,miss,unknown})距今 ≤ 100 天
#      ⇒ 超期 ⇒ 「漏检风险」;⚠️ **从未执行过** ⇒ 「从未执行过」(≠ 全 miss)
#   ③ **覆盖**:每次真实执行须覆盖 8 个槽位(4 族 × 2 路径)⇒ 漏槽位 ⇒ 漏检
#   ④ **计数纪律**:run-record 内不得出现「B3b-2 已达成」形态的计数
#   ⑤ 变异:① 注入正文类字段 ② 非法 outcome ③ cadence 超期 ⇒ 按固定签名变红
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

RUNS="${RUNS:-test/cep_b3b2_d4_probe_runs.jsonl}"
PROBE="test/cep_b3b2_d4_probe.json"
DEFERRALS="test/cep_b3b2_deferrals.json"
MAX_DAYS="${MAX_DAYS:-100}"
SHADOW="${SHADOW_ROOT:-/tmp/opencode/cep-b3b2-d4-quarterly}"

pass=0
fail=0
warn=0
ok() { echo "  ✅ $1"; pass=$((pass + 1)); }
ng() { echo "  ❌ $1"; fail=$((fail + 1)); }

VALIDATE_ONLY=0
if [[ "${1:-}" == "--validate-only" ]]; then
    VALIDATE_ONLY=1
fi

# ── 记录模式(交互式补录 run-record)────────────────────────────────
if [[ "${1:-}" == "--record" ]]; then
    run_id="${RUN_ID:-run-$(date -u +%Y%m%dT%H%M%SZ)}"
    run_at="${RUN_AT:-$(date -u +%Y-%m-%dT%H:%M:%SZ)}"
    executor="${EXECUTOR:-$(whoami)}"
    outcome="${OUTCOME:-miss}"
    incident="${INCIDENT:-null}"
    echo "run_id=$run_id run_at=$run_at executor=$executor outcome=$outcome incident=$incident"
    python3 - "$RUNS" "$run_id" "$run_at" "$executor" "$outcome" "$incident" <<'PYEOF'
import json, sys
path, run_id, run_at, executor, outcome, incident = sys.argv[1:7]
rec = {
    "schema": "cep-b3b2-d4-probe-run/v1",
    "run_id": run_id,
    "run_at": run_at,
    "executor": executor,
    "outcome": outcome,
    "note": "季度复审补录",
    "evidence": {},
    "incident": None if incident == "null" else incident,
    "probes": [],
}
with open(path, "a", encoding="utf-8") as f:
    f.write(json.dumps(rec, ensure_ascii=False) + "\n")
print(f"✅ 已追加 run-record: {run_id} ({outcome})")
PYEOF
    exit 0
fi

# ── 校验主体 ────────────────────────────────────────────────────────
validate() {
    python3 - "$RUNS" "$PROBE" "$DEFERRALS" "$MAX_DAYS" <<'PYEOF'
import json, sys
from datetime import datetime, timezone

runs_path, probe_path, deferrals_path, max_days = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
ALLOWED = {"schema", "run_id", "run_at", "executor", "outcome", "note", "evidence", "incident", "probes"}
BODY_KEYS = {"body", "snippet", "excerpt", "content", "text", "raw", "full_text"}
OUTCOMES = {"hit", "miss", "unknown"}
BASELINE_OUTCOME = "not_yet_run"
errs, warns = [], []

probes = json.load(open(probe_path, encoding="utf-8"))["probes"]
slots = {(p["family"], p["path"]) for p in probes}

records = []
for i, line in enumerate(open(runs_path, encoding="utf-8"), 1):
    line = line.strip()
    if not line:
        continue
    try:
        rec = json.loads(line)
    except Exception as exc:
        errs.append(f"RUN-RECORD-PARSE: line {i}: {exc}")
        continue
    records.append((i, rec))

if not records:
    errs.append("NO-RUN-RECORD: 从未执行过(⛔ 与「全部 miss」不同)")

for i, rec in records:
    for k in rec:
        if k not in ALLOWED:
            errs.append(f"BODY-FIELD-FORBIDDEN: line {i}: 未知 run-record 键 '{k}'"
                        + ("(⛔ 正文类字段名)" if k.lower() in BODY_KEYS else ""))
    out = rec.get("outcome")
    if out not in OUTCOMES and out != BASELINE_OUTCOME:
        errs.append(f"OUTCOME-INVALID: line {i}: outcome='{out}'(须 hit|miss|unknown;"
                    f"基线行允许 {BASELINE_OUTCOME})")
    inc = rec.get("incident")
    if inc is not None and len(str(inc)) > 300:
        errs.append(f"INCIDENT-TOO-LONG: line {i}: incident 超过 300 字符")
    try:
        datetime.fromisoformat(str(rec.get("run_at", "")).replace("Z", "+00:00"))
    except Exception:
        errs.append(f"RUN-AT-UNPARSEABLE: line {i}: run_at='{rec.get('run_at')}'")

real = [(i, r) for i, r in records if r.get("outcome") in OUTCOMES]
if not real:
    warns.append("CADENCE-NEVER-RUN: 从未执行过任何一次探测(⛔ 不等于「全部 miss」)")
else:
    def ts(r):
        return datetime.fromisoformat(str(r["run_at"]).replace("Z", "+00:00"))
    latest = max(real, key=lambda x: ts(x[1]))
    age = (datetime.now(timezone.utc) - ts(latest[1])).days
    if age > max_days:
        errs.append(f"CADENCE-OVERDUE: 最近一次真实执行距今 {age} 天 > {max_days} 天(漏检风险)")
    else:
        warns.append(f"CADENCE-OK: 最近一次真实执行距今 {age} 天(line {latest[0]})")

for i, rec in real:
    covered = {(p.get("family"), p.get("path")) for p in rec.get("probes", [])}
    missing = sorted(slots - covered)
    if missing:
        errs.append(f"RUN-COVERAGE-GAP: line {i}: 未覆盖槽位 {missing}")

for i, rec in records:
    blob = json.dumps(rec, ensure_ascii=False)
    if ("B3b-2 达成" in blob) or ("B3b-2 ✅" in blob):
        errs.append(f"COUNT-DISCIPLINE: line {i}: run-record 出现「B3b-2 已达成」形态")

for e in errs:
    print(e)
for w in warns:
    print("WARN " + w)
if errs:
    sys.exit(1)
print(f"QUARTERLY-OK: {len(records)} run-record(s)")
PYEOF
}

echo "══ ① run-record schema + cadence + 覆盖 + 计数纪律 ══"
if [[ ! -f "$RUNS" ]]; then
    ng "run-record 文件不存在($RUNS)⇒ **从未执行过**(漏检风险)"
else
    out="$(validate 2>&1)"
    rc=$?
    n_warn=0
    while read -r line; do
        case "$line" in
            WARN\ *) n_warn=$((n_warn + 1)); echo "  ⚠️  ${line#WARN }" ;;
            *) echo "$line" ;;
        esac
    done <<< "$out"
    warn=$((warn + n_warn))
    if [[ $rc -eq 0 ]]; then
        ok "run-record 校验: $(echo "$out" | grep -m1 'QUARTERLY-OK' || echo '通过')"
    else
        ng "run-record 校验未通过: $(echo "$out" | grep -E '^(BODY|RUN-|OUTCOME|INCIDENT|NO-RUN|CADENCE|COUNT)' | head -3 | tr '\n' ' ')"
    fi
fi

# ── 决定性变异(影子副本 ⇒ 按固定签名变红)────────────────────────────
if [[ "$VALIDATE_ONLY" -eq 0 ]]; then
    echo "══ ② 决定性变异 ══"
    mkdir -p "$SHADOW"

    # 变异 ①:注入正文类字段 ⇒ BODY-FIELD-FORBIDDEN
    mut="$SHADOW/runs_body.jsonl"
    python3 - "$RUNS" "$mut" <<'PYEOF'
import json, sys
lines = [l for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
rec = json.loads(lines[-1])
rec["body"] = "S1.5 UALink Coherency ..."
lines[-1] = json.dumps(rec, ensure_ascii=False) + "\n"
open(sys.argv[2], "w", encoding="utf-8").writelines(lines)
PYEOF
    if out="$(RUNS="$mut" bash "$0" --validate-only 2>&1)"; then
        ng "变异①未变红(实际: $out)"
    elif echo "$out" | grep -q 'BODY-FIELD-FORBIDDEN'; then
        ok "变异①按签名变红: $(echo "$out" | grep -m1 'BODY-FIELD-FORBIDDEN')"
    else
        ng "变异①签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
    fi

    # 变异 ②:非法 outcome ⇒ OUTCOME-INVALID
    mut="$SHADOW/runs_outcome.jsonl"
    python3 - "$RUNS" "$mut" <<'PYEOF'
import json, sys
lines = [l for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
rec = json.loads(lines[-1])
rec["outcome"] = "probably-fine"
lines[-1] = json.dumps(rec, ensure_ascii=False) + "\n"
open(sys.argv[2], "w", encoding="utf-8").writelines(lines)
PYEOF
    if out="$(RUNS="$mut" bash "$0" --validate-only 2>&1)"; then
        ng "变异②未变红(实际: $out)"
    elif echo "$out" | grep -q 'OUTCOME-INVALID'; then
        ok "变异②按签名变红: $(echo "$out" | grep -m1 'OUTCOME-INVALID')"
    else
        ng "变异②签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
    fi

    # 变异 ③:cadence 超期(200 天前)⇒ CADENCE-OVERDUE
    mut="$SHADOW/runs_old.jsonl"
    python3 - "$RUNS" "$mut" <<'PYEOF'
import json, sys
from datetime import datetime, timedelta, timezone
lines = [l for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
rec = json.loads(lines[-1])
rec["outcome"] = "miss"
rec["run_at"] = (datetime.now(timezone.utc) - timedelta(days=200)).strftime("%Y-%m-%dT%H:%M:%SZ")
lines[-1] = json.dumps(rec, ensure_ascii=False) + "\n"
open(sys.argv[2], "w", encoding="utf-8").writelines(lines)
PYEOF
    if out="$(RUNS="$mut" bash "$0" --validate-only 2>&1)"; then
        ng "变异③未变红(实际: $out)"
    elif echo "$out" | grep -q 'CADENCE-OVERDUE'; then
        ok "变异③按签名变红: $(echo "$out" | grep -m1 'CADENCE-OVERDUE')"
    else
        ng "变异③签名不符: $(echo "$out" | head -2 | tr '\n' ' ')"
    fi
fi

echo "────────────────────────────────────────"
if [[ "$fail" -eq 0 ]]; then
    echo "✅ cep_b3b2_d4_quarterly_review: $pass PASS / 0 FAIL / $warn WARN"
    echo "   ⛔ 本脚本**不**注册 ctest(cadence 断言放这里,避免 100 天后所有无关 PR 都红)"
    exit 0
fi
echo "❌ cep_b3b2_d4_quarterly_review: $fail FAIL / $warn WARN"
exit 1
