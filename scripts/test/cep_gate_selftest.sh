#!/usr/bin/env bash
# scripts/test/cep_gate_selftest.sh —— 状态一致性闸(第 11 组)的**回归 fixture 集**
#
# 为什么需要:闸本身原先**没有任何回归样本** → 「断言声称能 fire」与「断言真能 fire」
# 只能靠人肉记忆(2026-10-24 因此连续两轮误判)。本脚本把关键场景固化成
# **可执行断言**:每个 fixture 声明「变异 → 期望 rc」。
#
# 关键:一律用 `cep_gate_harness.py`(跑在**真拷贝**临时根上)——
# 改冻结面 ADR 会让整脚本 rc=1(第 7 组冻结校验),与第 11 组无关,不可据此判断。
#
# 用法:bash scripts/test/cep_gate_selftest.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 2

HARNESS="scripts/test/cep_gate_harness.py"
PASS=0
FAIL=0

run_case() {   # $1=名字 $2=期望(0=绿 / 1=红) $3=变异 python 代码
  local name="$1" want="$2" code="$3"
  local out rc
  out="$(python3 "$HARNESS" --mutate "$code" 2>&1)"
  rc="$(printf '%s\n' "$out" | sed -n 's/.*第 11 组 rc = \([0-9]\).*/\1/p' | tail -1)"
  [ -z "$rc" ] && rc="?"
  if [ "$rc" = "$want" ]; then
    printf "  \033[0;32m✓\033[0m %-52s rc=%s (期望 %s)\n" "$name" "$rc" "$want"
    PASS=$((PASS + 1))
  else
    printf "  \033[0;31m✗\033[0m %-52s rc=%s (期望 %s)\n" "$name" "$rc" "$want"
    printf '%s\n' "$out" | grep -E "✗" | head -3 | sed 's/^/      · /'
    FAIL=$((FAIL + 1))
  fi
}

echo -e "\033[1mCEP 状态一致性闸 —— 回归 fixture\033[0m"

# ── 基线:无变异 → 必须全绿 ──────────────────────────────────────
run_case "基线(无变异)" 0 'pass'

# ── 断言⑤:ADR 回归锁 ───────────────────────────────────────────
run_case "删掉真 Status Update 段 → ⑤ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/docs/adr/ADR-CEP-00-platform-charter-and-governance.md")
t = p.read_text(encoding="utf-8")
i = t.find("## Status Update 2026-10-23"); j = t.find("\n---", i)
p.write_text(t[:i] + t[j:], encoding="utf-8")'

run_case "回填日期 SU 顶替真段 → ⑤ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/docs/adr/ADR-CEP-00-platform-charter-and-governance.md")
t = p.read_text(encoding="utf-8")
i = t.find("## Status Update 2026-10-23"); j = t.find("\n---", i)
t = t[:i] + "## Status Update 2026-10-22 —— 追加登记\n\n> 本条登记:umbrella 仍为 Proposed,Stage 0 未归档。\n" + t[j:]
p.write_text(t, encoding="utf-8")'

# ── 断言②:旧状态投影(同义变体 / 裸字符陷阱)────────────────────
run_case "旧状态投影(违反准入规则)→ ② 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "## §2 范围", "## §2 范围\n\nStage 0 尚未归档违反准入规则。")
p.write_text(t, encoding="utf-8")'

run_case "旧状态投影(不符合单源纪律原则)→ ② 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "## §2 范围", "## §2 范围\n\numbrella 尚未归档不符合单源纪律原则。")
p.write_text(t, encoding="utf-8")'

run_case "旧状态投影(全角逗号连接)→ ② 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "## §2 范围", "## §2 范围\n\nStage 0 尚未归档，slot 已释放。")
p.write_text(t, encoding="utf-8")'

# ── 断言② 的误报防护:条件句(正确文档)必须放行 ──────────────────
run_case "条件句(规则陈述)→ 必须 GREEN" 0 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "## §2 范围", "## §2 范围\n\n若 `cpptlm-cep-umbrella` 未归档,则该 Stage 0 不启动。")
p.write_text(t, encoding="utf-8")'

# ── 断言①:当前态数值漂移 + 合法历史叙述 ─────────────────────────
run_case "obj 豁免块写当前 baseline=<≠实测> → ① 必须 fire" 1 '
from pathlib import Path
import json, subprocess
_d = json.loads(subprocess.run(["openspec", "list", "--json"], capture_output=True, text=True).stdout)
_n = len(_d if isinstance(_d, list) else _d.get("changes", []))
# 挑一个 != 实测的 N/3(不得硬编码 —— 否则 fixture 与 live baseline 耦合)
_drift = "2/3" if _n != 2 else "1/3"
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "### 9.3 R10 处置(2026-10-23)", "### 9.3 R10 处置(2026-10-23)\n\n**当前 baseline = " + _drift + "**")
p.write_text(t, encoding="utf-8")'

run_case "obj 豁免块的合法历史叙述 → 必须 GREEN" 0 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "### 9.3 R10 处置(2026-10-23)", "### 9.3 R10 处置(2026-10-23)\n\n> **历史**:截至 2026-10-20,KPI 并发 **3/3**(上限触发);现已释放。")
p.write_text(t, encoding="utf-8")'

# ── 断言③:冻结成员数回退 ───────────────────────────────────────
# 2026-10-25(Entry #56):断言③ 改**双形态** —— 文档可写数字(须 == 清单)或
# 「以清单为准」(无数字 → 无从写错)。故 fixture 改为「**注入一个错误数字**」→
# ③ 仍必须 fire(防漂移能力不减);另加一条「全部无数字 → ③ 豁免」的 GREEN 用例。
run_case "活文档注入错误冻结成员数 → ③ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/scripts/README.md")
t = p.read_text(encoding="utf-8")
anchor = "M1 冻结面授权闸（成员数以清单为准）"
assert anchor in t, f"锚点漂移:README 未含 {anchor}"
p.write_text(t.replace(anchor, "M1 冻结面 **29 成员**授权闸", 1), encoding="utf-8")'

run_case "全部活文档以「清单为准」表述 → ③ 必须 GREEN" 0 '
pass'

# ── critic 第 12 轮新增:句首「当前」不得被当条件句跳过 ─────────────
run_case "句首「当前 Stage 0 尚未归档」→ ② 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "## §2 范围", "## §2 范围\n\n当前 Stage 0 尚未归档。")
p.write_text(t, encoding="utf-8")'

run_case "行尾贴「旧值」+ 当前态限定词 → ① 必须 fire" 1 '
from pathlib import Path
import json, subprocess
_d = json.loads(subprocess.run(["openspec", "list", "--json"], capture_output=True, text=True).stdout)
_n = len(_d if isinstance(_d, list) else _d.get("changes", []))
# 挑一个 != 实测的 N/3(不得硬编码 —— 否则 fixture 与 live baseline 耦合)
_drift = "2/3" if _n != 2 else "1/3"
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "### 9.3 R10 处置(2026-10-23)", "### 9.3 R10 处置(2026-10-23)\n\n**当前 baseline = " + _drift + "**(旧值)")
p.write_text(t, encoding="utf-8")'

# ── 断言④:旧任务计数 ────────────────────────────────────────────
run_case "旧任务计数形态(105 / 99)→ ④ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
t = sub(t, "## §2 范围", "## §2 范围\n\ntasks.md 105 / 99 已完成")
p.write_text(t, encoding="utf-8")'

# ── 断言⑥:glob 覆盖(归档件被移走 → 命中数 < 4)────────────────────
#   注:harness 现已**整目录拷贝** openspec(critic SHOULD_FIX-2,不再 symlink),
#   故此处删「拷贝里」的归档件即可 —— 不再需要先 unlink symlink。
run_case "归档件被移走 → ⑥ 必须 fire" 1 '
import os, shutil, glob
rt = os.environ["RT"]
src = os.path.join(rt, "openspec")
real = os.path.join(os.environ["CEP_GATE_SRC"], "openspec")
files = sorted(glob.glob(os.path.join(real, "changes/archive/*cpptlm-cep-umbrella/**/*.md"), recursive=True))
assert len(files) == 4, f"仓内 umbrella 归档件应为 4 个,实得 {len(files)}"
# 把拷贝里 3 个归档件**移走** → glob 命中 3 < 4 → ⑥ 必须报红
for f in files[:3]:
    d = os.path.join(src, os.path.relpath(f, real))
    if os.path.exists(d):
        os.unlink(d)'

# ── 断言 ⑧:S1 有效期登记(2026-10-24 新增,per critic SHOULD_FIX-1)────────
run_case "断言 ⑧ 基线(spike 源含有效期锚)→ 必须 GREEN" 0 '
pass'

run_case "移除 spike 源的有效期锚 → ⑧ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/examples/cep_poc_spike_s1.cc")
t = p.read_text(encoding="utf-8")
assert "S1_VALIDITY_UNTIL=queue6-perm-schema-evolution" in t
p.write_text(t.replace("S1_VALIDITY_UNTIL=queue6-perm-schema-evolution", "S1_VALIDITY_X"), encoding="utf-8")'

run_case "schema 引入 perm 形态字段(有效期届满条件)→ ⑧ 必须 fire" 1 '
from pathlib import Path
import json
p = Path(os.environ["RT"] + "/include/tlm/cep/protocol_instance.schema.json")
d = json.loads(p.read_text(encoding="utf-8"))
d["properties"]["perm_form"] = {"type": "string"}
p.write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")'

# ── 断言 ⑦:移交链的可达性与 owner 活性 ────────────────────────────
run_case "断言 ⑦ 基线(不移除任何标记)→ 必须 GREEN" 0 '
pass'

run_case "从 obj §4.0 移除 S1-3 提及 → ⑦ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
i = t.index("### 4.0 ")
j = t.index("\n### ", i + 1)
sec = t[i:j]
assert "S1-3" in sec, "锚点漂移:§4.0 未含 S1-3"
sec = sec.replace("S1-3", "(已移除)", 1)
p.write_text(t[:i] + sec + t[j:], encoding="utf-8")'

run_case "把 §4.0 活队列行的 change 名改成已归档的 → ⑦ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/objectives/obj-cep-platform.md")
t = p.read_text(encoding="utf-8")
i = t.index("### 4.0 ")
j = t.index("\n### ", i + 1)
sec = t[i:j]
# 用**活**队列行(队列 #2 归档后已不在活集内)→ 队列 #3 stub-cache-core
assert "`cpptlm-cep-stub-cache-core`" in sec, "锚点漂移:§4.0 未含活队列 #3 的 change 名"
sec = sec.replace("`cpptlm-cep-stub-cache-core`", "`cpptlm-cep-umbrella`", 1)
p.write_text(t[:i] + sec + t[j:], encoding="utf-8")'

run_case "活文档出现归档件里不存在的 S1-9 → ⑦ 必须 fire" 1 '
from pathlib import Path
p = Path(os.environ["RT"] + "/.rddf/roadmap/phases/phase-10.md")
t = p.read_text(encoding="utf-8")
p.write_text(t + "\n> 承接 Stage 1 准入前置:S1-9(占位)\n", encoding="utf-8")'

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo -e "\033[0;32m✅ 闸的回归 fixture 全部通过\033[0m ($PASS 项)"
  exit 0
fi
echo -e "\033[0;31m❌ 闸的回归 fixture 有 $FAIL 项失败\033[0m (通过 $PASS 项)"
exit 1
