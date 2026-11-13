#!/usr/bin/env bash
# scripts/test/cep_full_green_run_check.sh
# 「拓扑级全绿 run」**证据闸**(C1 / C1' / C2 / C3 / C4 / C5)
#
# 作者: builder  日期: 2026-11-13(per 台账 Entry #394 / #396 / #397;
#         change `cpptlm-cep-full-green-run-evidence` · A8)
#
# 判据(逐条独立报红;⛔「rc≠0」**不构成红因** ⇒ 每条红因有**固定签名**):
#   C1  两违例类为**实测数值**(⛔ 非 "not-collected" 且须在 `_measured` 内)  签名:违例类未实测
#   C1' **样本活性见证**:observation_keys.memory_ops_checked_addrs 长度 > 0   签名:检测器无样本见证
#   C2  两违例类实测值 == 0                                                 签名:违例类非零
#   C3  IC3 = **通过态**(report.html 渲染「✓ 一致性闸门通过」且不含另两态)    签名:IC3 非通过态
#   C4  **拓扑链身份锚**(对账清单条目 + 拓扑 config 指纹 + simulation_config
#       指纹 + hypothesis_id 已在预登记 registered 行内)                      签名:非拓扑级 E2E run
#   C5  `07 号` 有**新 SU** 且其正文指向该 run                               签名:证据登记缺失
#
# ⚠️ **本闸 = 基线正控 + 6 条决定性变异 + 还原绿**(一条 ctest 覆盖全部):
#   每条变异在**影子副本**上做 ⇒ **真实仓字节指纹前后必须相等**;
#   变异须**按签名红**(⛔ 单纯 rc≠0 不算)⇒ 断言红因**精确命中**该判据的签名。
#
# 用法:
#   bash scripts/test/cep_full_green_run_check.sh
#   CEP_FGR_RUN_DIR=... CEP_FGR_JSONL=... CEP_FGR_PREREG=... CEP_FGR_DOC07=... \
#     bash scripts/test/cep_full_green_run_check.sh          # 影子副本模式(变异用)

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT" || exit 2

RUN_DIR="${CEP_FGR_RUN_DIR:-experimental/run-20261113-151310-cep-full-green}"
JSONL="${CEP_FGR_JSONL:-test/cep_e2e_runs.jsonl}"
PREREG="${CEP_FGR_PREREG:-experimental/preregistration.json}"
DOC07="${CEP_FGR_DOC07:-docs/architecture/07-coherence-exploration-platform.md}"

# ── 判据本体(纯读;⛔ 不写任何仓内文件)────────────────────────────────────
check() {  # $1=run_dir $2=jsonl $3=prereg $4=doc07
    python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib, json, os, sys

run_dir, jsonl_path, prereg_path, doc07_path = sys.argv[1:5]
fails = []


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def load_json(path):
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except Exception:
        return None


m = load_json(os.path.join(run_dir, "metrics.json"))
s = load_json(os.path.join(run_dir, "simulation_config.json"))
if m is None or s is None:
    print("✗ 目标 run 缺失或畸形:%s" % run_dir)
    sys.exit(2)

measured = set(m.get("_measured") or [])

# ── C1 / C2:两违例类的「实测性」与「零值」──────────────────────────────
for k in ("deadlock_violations", "memory_violations"):
    v = m.get(k)
    if v == "not-collected":
        fails.append(("违例类未实测", "%s = \"not-collected\"" % k))
    elif isinstance(v, bool) or not isinstance(v, (int, float)):
        fails.append(("违例类未实测", "%s = %r(非法类型)" % (k, v)))
    elif k not in measured:
        fails.append(("违例类未实测", "%s 未登记进 _measured" % k))
    elif v != 0:
        fails.append(("违例类非零", "%s = %r" % (k, v)))

# ── C1':样本活性见证(⛔ 不得以「挂了但样本从未流入」的实测 0 充数)─────────
obs = m.get("observation_keys") or {}
addrs = obs.get("memory_ops_checked_addrs")
if not isinstance(addrs, list) or len(addrs) == 0:
    fails.append(("检测器无样本见证",
                  "observation_keys.memory_ops_checked_addrs 长度 = %s"
                  % (len(addrs) if isinstance(addrs, list) else "N/A")))

# ── C3:IC3 通过态(**报告层渲染面**;⛔ 与 C1/C2 的判定面不重叠)────────────
try:
    html = open(os.path.join(run_dir, "report.html"), encoding="utf-8").read()
except Exception:
    html = ""
if "✓ 一致性闸门通过" not in html:
    fails.append(("IC3 非通过态", "report.html 未渲染「✓ 一致性闸门通过」"))
if "⚪ 一致性闸门不适用" in html:
    fails.append(("IC3 非通过态", "report.html 仍含「⚪ 一致性闸门不适用」"))
if "❌ 一致性违例" in html:
    fails.append(("IC3 非通过态", "report.html 含「❌ 一致性违例」"))

# ── C4:拓扑链身份锚(⛔ 合成 fixture / 模型级实例均无对账条目 ⇒ 自动落选)──
pre = load_json(prereg_path) or {}
hyps = pre.get("hypotheses") or {}
rows = []
try:
    with open(jsonl_path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
except Exception:
    rows = []

want_metrics = os.path.join(run_dir, "metrics.json")
entry = next((r for r in rows if r.get("metrics_path") == want_metrics), None)
if entry is None:
    fails.append(("非拓扑级 E2E run", "对账清单无 %s 的条目" % want_metrics))
else:
    cf = entry.get("config_file") or ""
    if not (cf.startswith("configs/cep_chi_rnf_e2e_") and cf.endswith(".json")):
        fails.append(("非拓扑级 E2E run", "config_file = %r(非拓扑级 e2e 配置)" % cf))
    elif os.path.exists(cf) and entry.get("config_sha256") != sha256(cf):
        fails.append(("非拓扑级 E2E run", "config_sha256 与磁盘不一致"))
    if entry.get("simulation_config_sha256") != sha256(os.path.join(run_dir, "simulation_config.json")):
        fails.append(("非拓扑级 E2E run", "simulation_config_sha256 与磁盘不一致"))
    if entry.get("metrics_sha256") != sha256(want_metrics):
        fails.append(("非拓扑级 E2E run", "metrics_sha256 与磁盘不一致"))
    hid = entry.get("hypothesis_id")
    h = hyps.get(str(hid).upper()) if hid is not None else None
    if h is None:
        fails.append(("非拓扑级 E2E run", "hypothesis_id %r 无预登记行" % hid))
    elif h.get("status") == "pending":
        fails.append(("非拓扑级 E2E run", "hypothesis_id %r 状态 = pending" % hid))

# ── C5:`07 号` 新 SU 存在且其正文指向该 run ──────────────────────────────
try:
    doc = open(doc07_path, encoding="utf-8").read()
except Exception:
    doc = ""
su_titles = [l for l in doc.split("\n") if l.startswith("## ") and "Status Update" in l]
target = [l for l in su_titles if "拓扑级 IC3 通过态" in l]
if not target:
    fails.append(("证据登记缺失", "07 号 无「拓扑级 IC3 通过态」SU"))
else:
    body = doc.split(target[-1], 1)[1].split("\n## ", 1)[0]
    if os.path.basename(run_dir) not in body:
        fails.append(("证据登记缺失", "07 号 新 SU 未指向该 run 目录"))

for sig, detail in fails:
    print("✗ %s: %s" % (sig, detail))
if fails:
    sys.exit(1)
print("✓ 拓扑级「全绿 run」证据闸通过(C1 / C1' / C2 / C3 / C4 / C5)")
PYEOF
}

# ── 真实仓字节指纹(变异前后须相等)────────────────────────────────────────
fingerprint() {
    for f in "$RUN_DIR/metrics.json" "$RUN_DIR/simulation_config.json" "$RUN_DIR/report.html" \
             "$JSONL" "$PREREG" "$DOC07"; do
        if [[ -f "$f" ]]; then sha256sum "$f"; else echo "MISSING $f"; fi
    done
}

# ── 影子副本准备(⛔ 不触碰真实仓)────────────────────────────────────────
SHADOW=""
prepare_shadow() {
    SHADOW="$(mktemp -d)"
    cp -r "$RUN_DIR" "$SHADOW/$(basename "$RUN_DIR")"
    cp "$JSONL" "$SHADOW/e2e_runs.jsonl"
    cp "$PREREG" "$SHADOW/preregistration.json"
    cp "$DOC07" "$SHADOW/doc07.md"
}

# 影子内重算指纹字段 ⇒ C4 保持绿 ⇒ 变异只命中**它自己的**判据
resync_shadow_jsonl() {
    python3 - "$SHADOW" "$RUN_DIR" <<'PYEOF'
import hashlib, json, os, sys
shadow, real_run = sys.argv[1], sys.argv[2]
run_name = os.path.basename(real_run)
sh_run = os.path.join(shadow, run_name)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


p = os.path.join(shadow, "e2e_runs.jsonl")
rows = []
with open(p, encoding="utf-8") as fh:
    for line in fh:
        line = line.strip()
        if line:
            rows.append(json.loads(line))
for r in rows:
    if r.get("metrics_path", "").endswith(run_name + "/metrics.json"):
        r["metrics_path"] = os.path.join(sh_run, "metrics.json")
        r["config_path"] = os.path.join(sh_run, "simulation_config.json")
        r["metrics_sha256"] = sha256(os.path.join(sh_run, "metrics.json"))
        r["simulation_config_sha256"] = sha256(os.path.join(sh_run, "simulation_config.json"))
with open(p, "w", encoding="utf-8") as fh:
    for r in rows:
        fh.write(json.dumps(r, ensure_ascii=False) + "\n")
PYEOF
}

# ── 6 条决定性变异(每条 ⇒ 影子副本上**按签名红**)──────────────────────────
MUT_FAILURES=0
mutate() {  # $1=签名  $2=编号  $3=说明
    local sig="$1" no="$2" desc="$3" out rc
    prepare_shadow
    local sh_run="$SHADOW/$(basename "$RUN_DIR")"
    case "$no" in
    1)  # 违例类改回 not-collected
        python3 - "$sh_run/metrics.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
m = json.load(open(p, encoding="utf-8"))
m["memory_violations"] = "not-collected"
m["_measured"] = [k for k in m.get("_measured", []) if k != "memory_violations"]
m.setdefault("_not_collected", {})["memory_violations"] = "本 run 无内存操作样本 ⇒ 无可检样本"
json.dump(m, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
PYEOF
        ;;
    2)  # 删样本计数键(零样本 ⇒ 活性见证失守)
        python3 - "$sh_run/metrics.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
m = json.load(open(p, encoding="utf-8"))
m.get("observation_keys", {}).pop("memory_ops_checked_addrs", None)
json.dump(m, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
PYEOF
        ;;
    3)  # 实测值改非 0
        python3 - "$sh_run/metrics.json" <<'PYEOF'
import json, sys
p = sys.argv[1]
m = json.load(open(p, encoding="utf-8"))
m["memory_violations"] = 3
json.dump(m, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
PYEOF
        ;;
    4)  # IC3 降级为不适用(报告层渲染面)
        python3 - "$sh_run/report.html" <<'PYEOF'
import sys
p = sys.argv[1]
h = open(p, encoding="utf-8").read()
h = h.replace("✓ 一致性闸门通过", "⚪ 一致性闸门不适用")
open(p, "w", encoding="utf-8").write(h)
PYEOF
        ;;
    5)  # run 换成模型级实例(无对账条目 ⇒ 拓扑链身份锚失守)
        python3 - "$SHADOW/e2e_runs.jsonl" <<'PYEOF'
import json, sys
p = sys.argv[1]
rows = []
for line in open(p, encoding="utf-8"):
    line = line.strip()
    if line:
        rows.append(json.loads(line))
for r in rows:
    if "cep-full-green" in r.get("run_id", ""):
        r["run_id"] = "cep-e2e-2026-11-09-a"
        r["metrics_path"] = "experimental/run-2026-11-09-cep-e2e/metrics.json"
        r["config_path"] = "experimental/run-2026-11-09-cep-e2e/simulation_config.json"
with open(p, "w", encoding="utf-8") as fh:
    for r in rows:
        fh.write(json.dumps(r, ensure_ascii=False) + "\n")
PYEOF
        ;;
    6)  # 移除 07 号 新 SU(证据登记失守)
        python3 - "$SHADOW/doc07.md" <<'PYEOF'
import sys
p = sys.argv[1]
lines = open(p, encoding="utf-8").read().split("\n")
out, skip = [], False
for l in lines:
    if l.startswith("## ") and "Status Update" in l and "拓扑级 IC3 通过态" in l:
        skip = True
        continue
    if skip and l.startswith("## "):
        skip = False
    if not skip:
        out.append(l)
open(p, "w", encoding="utf-8").write("\n".join(out))
PYEOF
        ;;
    esac
    resync_shadow_jsonl
    out="$(CEP_FGR_JUST_CHECK=1 \
           CEP_FGR_RUN_DIR="$sh_run" \
           CEP_FGR_JSONL="$SHADOW/e2e_runs.jsonl" \
           CEP_FGR_PREREG="$SHADOW/preregistration.json" \
           CEP_FGR_DOC07="$SHADOW/doc07.md" \
           bash "$REPO_ROOT/scripts/test/cep_full_green_run_check.sh" 2>&1)" && rc=0 || rc=$?
    if [[ $rc -eq 0 ]]; then
        printf '✗ 变异 %s 未变红(基线仍绿)\n' "$no"
        MUT_FAILURES=$((MUT_FAILURES + 1))
    elif ! grep -qF "$sig" <<<"$out"; then
        printf '✗ 变异 %s 未按签名红(期望 %s)\n%s\n' "$no" "$sig" "$out"
        MUT_FAILURES=$((MUT_FAILURES + 1))
    else
        printf '  ✓ 变异 %s 按签名红:%s(%s)\n' "$no" "$sig" "$desc"
    fi
    rm -rf "$SHADOW"
}

# ── 主流程 ──────────────────────────────────────────────────────────────
# ⚠️ **影子模式**:只跑判据本体(供变异调用;⛔ 避免递归跑变异)
if [[ "${CEP_FGR_JUST_CHECK:-0}" == "1" ]]; then
    check "$RUN_DIR" "$JSONL" "$PREREG" "$DOC07"
    exit $?
fi

echo "── 基线正控(真实仓)────────────────────────────────────"
if ! check "$RUN_DIR" "$JSONL" "$PREREG" "$DOC07"; then
    echo "✗ 基线不绿 ⇒ 变异无意义 ⇒ 中止"
    exit 1
fi

FP_BEFORE="$(fingerprint)"

echo "── 6 条决定性变异(影子副本)──────────────────────────────"
mutate "违例类未实测"     1 "memory_violations 改回 not-collected"
mutate "检测器无样本见证" 2 "删 observation_keys.memory_ops_checked_addrs"
mutate "违例类非零"       3 "memory_violations 改非 0"
mutate "IC3 非通过态"     4 "report.html 渲染降级为「⚪ 不适用」"
mutate "非拓扑级 E2E run" 5 "对账条目换成模型级 run"
mutate "证据登记缺失"     6 "移除 07 号 新 SU"

echo "── 还原后复验(真实仓)──────────────────────────────────"
FP_AFTER="$(fingerprint)"
if [[ "$FP_BEFORE" != "$FP_AFTER" ]]; then
    echo "✗ 真实仓字节指纹前后**不等** ⇒ 变异污染了真实仓"
    diff <(echo "$FP_BEFORE") <(echo "$FP_AFTER") || true
    exit 1
fi
echo "  ✓ 真实仓字节指纹前后相等"
if ! check "$RUN_DIR" "$JSONL" "$PREREG" "$DOC07"; then
    echo "✗ 还原后基线不绿"
    exit 1
fi

if [[ $MUT_FAILURES -ne 0 ]]; then
    echo "✗ $MUT_FAILURES 条变异未按签名红"
    exit 1
fi
echo "✓ 拓扑级「全绿 run」证据闸通过(基线 + 6 条决定性变异 + 还原绿 + 指纹相等)"
