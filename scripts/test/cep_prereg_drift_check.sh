#!/usr/bin/env bash
# scripts/test/cep_prereg_drift_check.sh
#   —— 预登记 out-of-scope 集合的**真源 ↔ 机器投影 drift 检测**
#
# 背景(per docs/adr/ADR-CEP-06-experiment-protocol.md SU#2 ①②):
#   `ADR-CEP-06` SU#1 §④ 要求 run-validity 判据④「`not-collected` 集合 ⊆ 实验设计
#   **预登记**的 out-of-scope 集合」,但未指定载体 ⇒ SU#2 裁决**双层**:
#     · 真源(评审过的设计面) = `.rddf/roadmap/objectives/obj-cep-platform.md` §9.1 的 ② 表
#     · 机器投影(tools 判据④ 的读本) = `experimental/preregistration.json`
#   两侧**必须逐假设集合相等** —— 任一侧单改 ⇒ 本检查报红。
#
# ⚠️ **为什么必须有本闸**(SU#2 ② 门禁②):
#   两侧互为锁 —— 改投影绕过评审 ⇒ 冻结授权记录(`--update --authorized-by`)暴露;
#   改真源想静默生效 ⇒ 本闸报红。**没有本闸 ⇒ 双份必然漂移**,
#   而漂移的方向恰好是危险方向(悄悄扩大 out-of-scope ⇒ 悄悄放宽 not-collected)。
#
# ⚠️ **本闸只校验「集合相等」,不校验「集合是否合理」** ——
#   后者是评审域(`objective §9.1` ② 表的人工裁决)的事,机器不可替代。
#
# 用法:
#   bash scripts/test/cep_prereg_drift_check.sh          # 校验(漂移 ⇒ 退出 1)
#
# 退出码: 0 = 一致 · 1 = drift(集合不等) · 2 = 输入缺失/格式错

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; NC='\033[0m'

TRUTH="$ROOT/.rddf/roadmap/objectives/obj-cep-platform.md"
PROJ="$ROOT/experimental/preregistration.json"

[ -f "$TRUTH" ] || { echo -e "${RED}✗ 缺真源: $TRUTH${NC}"; exit 2; }
[ -f "$PROJ" ]  || { echo -e "${RED}✗ 缺机器投影: $PROJ${NC}"; exit 2; }

echo -e "${YELLOW}[cep-prereg-drift] 预登记 out-of-scope 集合:真源 ↔ 机器投影 drift 检测${NC}"

python3 - "$TRUTH" "$PROJ" "$ROOT" <<'PYEOF'
import json, os, re, sys

truth_path, proj_path, repo_root = sys.argv[1], sys.argv[2], sys.argv[3]
RED, GREEN, YELLOW, NC = "\033[0;31m", "\033[0;32m", "\033[0;33m", "\033[0m"

ALL13 = [
    "total_cycles", "total_transactions", "transactions_per_cycle",
    "avg_latency_cycles", "p50_latency_cycles", "p95_latency_cycles",
    "p99_latency_cycles", "max_latency_cycles", "coherence_stalls",
    "collective_stalls", "directory_misses", "deadlock_violations",
    "memory_violations",
]

# ── 解析真源(§9.1 ② 表)────────────────────────────────────────────────
text = open(truth_path, encoding="utf-8").read()
try:
    sec = text.split("**② 预登记 out-of-scope 集合")[1].split("**②-2 机器投影")[0]
except IndexError:
    print(f"{RED}✗ 真源里找不到「② 预登记 out-of-scope 集合」小节(结构漂移?){NC}")
    sys.exit(2)

def parse_scope(cell):
    """把 ② 表的 out-of-scope 单元格解析成 (status, 集合)。"""
    c = cell.strip()
    # pending 行:`status = pending` ⇒ 空集 + pending(⛔ 不是「全部 13 项」)
    if "pending" in c:
        return "pending", []
    # 「全部 13 项」/「*(全部 13 项)*」⇒ 契约级断言 / 外部对拍
    if "全部 13" in c:
        return "registered", list(ALL13)
    # 逐项列举(以 `·` 分隔)
    items = [x.strip().strip("`*").strip() for x in c.split("·")]
    items = [x for x in items if x]
    bad = [x for x in items if x not in ALL13]
    if bad:
        print(f"{RED}✗ 真源 ② 表含非 13 必纳 metric 的名字: {bad}{NC}")
        sys.exit(2)
    return "registered", items

truth = {}
for m in re.finditer(r"^\|\s*\*\*(H[1-8])\*\*\s*\|\s*(.+?)\s*\|\s*(.+?)\s*\|\s*$", sec, re.M):
    h, scope_cell, _reason = m.group(1), m.group(2), m.group(3)
    truth[h] = parse_scope(scope_cell)

if not truth:
    print(f"{RED}✗ 真源 ② 表解析出 0 行(结构漂移?){NC}")
    sys.exit(2)

# ── 解析投影 ───────────────────────────────────────────────────────────
try:
    proj = json.load(open(proj_path, encoding="utf-8"))
except Exception as e:
    print(f"{RED}✗ 机器投影 JSON 解析失败: {e}{NC}")
    sys.exit(2)

ph = proj.get("hypotheses")
if not isinstance(ph, dict):
    print(f"{RED}✗ 机器投影缺 hypotheses 对象{NC}")
    sys.exit(2)

# 投影的 required_metrics 必须就是 13 必纳
req = proj.get("required_metrics")
if req != ALL13:
    print(f"{RED}✗ 机器投影 required_metrics ≠ 13 必纳{NC}")
    print(f"    实得: {req}")
    sys.exit(1)

problems = []

# ① 假设集合必须双向相等
tk, pk = set(truth), set(ph)
for h in sorted(tk - pk):
    problems.append(f"真源有 {h} 但投影**缺行**(⇒ 工具判红「未登记」)")
for h in sorted(pk - tk):
    problems.append(f"投影有 {h} 但真源 ② 表**缺行**")

# ② 逐假设:status + out_of_scope 集合必须相等
for h in sorted(tk & pk):
    t_status, t_set = truth[h]
    p_entry = ph[h]
    p_status = p_entry.get("status")
    p_set = p_entry.get("out_of_scope")
    if not isinstance(p_set, list):
        problems.append(f"{h}: 投影 out_of_scope 非数组")
        continue
    if p_status != t_status:
        problems.append(f"{h}: status 不一致 —— 真源 {t_status} vs 投影 {p_status}")
    # 集合相等(顺序无关)
    if set(p_set) != set(t_set):
        only_t = sorted(set(t_set) - set(p_set))
        only_p = sorted(set(p_set) - set(t_set))
        msg = f"{h}: out_of_scope 集合漂移"
        if only_t:
            msg += f" —— 仅真源有: {only_t}"
        if only_p:
            msg += f" —— 仅投影有: {only_p}"
        problems.append(msg)
    # 投影内的名字必须是 13 必纳
    bad = [x for x in p_set if x not in ALL13]
    if bad:
        problems.append(f"{h}: 投影 out_of_scope 含非 13 必纳 metric: {bad}")

# ── ③ emit 白名单 ⊆ 已登记假设(SU#2 门禁③ 的「顺序锁」机器化)──────────
# 背景:SU#2 门禁③ 承诺「登记 H4 预登记行的同一 PR 才允许加 run_h4」——
#   但该承诺此前**只存在于散文**(纯流程承诺)⇒ C1 的根因教训恰恰是「纯承诺 = 测试给缺口盖章」。
#   本断言把承诺变成机器闸:emit 能产出的假设 id,必须已在投影里 registered。
#   ⛔ 反向不检查(「已 registered 但 emit 无入口」是**合法**的 —— 预登记先于产出,H4/H8 即此态)。
EMIT_TS = "test/test_cep_experiment_emit.cc"
# 仓根 = 脚本所在 scripts/test/ 的上两级(不依赖 truth_path 的位置 ⇒ 与台账文件无关)
emit_path = os.path.join(repo_root, EMIT_TS)
if not os.path.isfile(emit_path):
    print(f"{YELLOW}  · [SKIP] emit 白名单断言:{EMIT_TS} 不存在(非本仓路径?){NC}")
else:
    src = open(emit_path, encoding="utf-8").read()
    # 提取 emit 的实验 id 白名单:`r = run_hN(...)` 的 N
    emit_ids = sorted(set(re.findall(r"=\s*run_(h\d+)\s*\(", src)))
    # 投影里 status == registered 的假设
    registered = {h for h, v in ph.items() if v.get("status") == "registered"}
    # 假设 id 与 emit id 的映射:h2 ↔ H2(大小写归一)
    reg_upper = {h.upper() for h in registered}
    for eid in emit_ids:
        if eid.upper() not in reg_upper:
            problems.append(
                f"emit 能产出 `{eid}` 但投影里**无 registered 行** ⇒ "
                f"违反 SU#2 门禁③ 的顺序锁(预登记必须先于产出)")
    print(f"{YELLOW}  · emit 白名单 = {emit_ids}(⊆ registered 才合规){NC}")

if problems:
    print(f"{RED}✗ 预登记集合 drift(共 {len(problems)} 项){NC}")
    for p in problems:
        print(f"    · {p}")
    print(f"{YELLOW}  ⇒ 真源 = .rddf/roadmap/objectives/obj-cep-platform.md §9.1 ② 表{NC}")
    print(f"{YELLOW}  ⇒ 投影 = experimental/preregistration.json{NC}")
    print(f"{YELLOW}  ⇒ 两侧必须同 PR 同改(per ADR-CEP-06 SU#2 ② 门禁②){NC}")
    sys.exit(1)

print(f"{GREEN}✓ 真源 §9.1 ② 表 ↔ 机器投影 逐假设集合相等({len(truth)} 个假设){NC}")
reg = sum(1 for h in truth if truth[h][0] == "registered")
pen = sum(1 for h in truth if truth[h][0] == "pending")
print(f"  · registered = {reg}(可产出 run)· pending = {pen}(禁产出 run)")
sys.exit(0)
PYEOF
