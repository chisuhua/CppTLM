#!/usr/bin/env bash
# scripts/test/cep_validate_topology_reconcile_check.sh
# **`validate_topology` target 的对账闸**(per 台账 Entry #358)
#
# 纪律(逐字):「**target 引用的每个文件都必须真实存在,且闸挂点必须真的挂上**」
#            ——「组合 target 手抄 config 清单」是 2026-11-12 之前 `validate_topology`
#            **跑不完**的根因(首条引用了已被 T-bs-5 物理删除的 `dgpu_board_v1_mvp.json`
#            ⇒ FileNotFoundError ⇒ 其后 4 条拓扑校验 + CEP 装配闸 CLI 挂点
#            **全部从未执行**),且该失效**静默 20 天**(`ADR-CEP-00` 2026-10-25 SU 已登记
#            「修复需单独决策」,至 2026-11-12 仍未兑现)。
#
# 判据(全部可机器判;**期望值来源 = 外部可推导的三方**,⛔ 不是闸的自述):
#   ① **调用面 ↔ 磁盘**:组合 target 实际会跑的每个 config 路径**必须存在**;
#      ⛔ 不存在的路径 ⇒ 整条 target 跑不完 ⇒ 其后所有命令(含闸挂点)静默不执行。
#   ② **调用面 ↔ 单一真源**:组合 target 的 config 清单**必须从
#      `CPPTLM_TOPOLOGY_VALIDATION_CONFIGS` 派生** ⇒ ⛔ 不得手抄副本
#      (手抄 = 本 bug 的结构性根因)。
#   ③ **闸挂点存在性**:`cep_channel_dependency_gate` **必须**出现在组合 target 的
#      命令面里 ⇒ 否则「装配闸离线挂点」这条契约(per `ADR-CEP-09` 抓取点 1)静默失效,
#      而 ctest 侧的同名用例仍绿 ⇒ **假绿**。
#   ④ **个体 target 一致性**:组合 target 的 config 集合 == 个体 target 的 config 集合
#      ⇒ 两者同源于 ② ⇒ 不得有一方漏项。
#
# ⚠️ **为什么 ③ 必须单列**:ctest 的 `cep_channel_dependency_gate` 用例**独立注册**
#   (examples/CMakeLists.txt:140)⇒ 即使 `validate_topology` 的闸挂点消失,
#   ctest 仍全绿 ⇒ **无本闸则该失效不可见**。
#   本闸把「组合 target 的命令面」当作**被检对象**,从**外部**(CMake 源码 + 磁盘)推导
#   期望值 ⇒ ⛔ 不是自证。
#
# 用法:bash scripts/test/cep_validate_topology_reconcile_check.sh
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

CMAKE_LIST="scripts/CMakeLists.txt"
EXAMPLES_CMAKE_LIST="examples/CMakeLists.txt"
GATE_TARGET="cep_channel_dependency_gate"

if [[ ! -f "$CMAKE_LIST" ]]; then
    echo "✗ 未找到 $CMAKE_LIST" >&2
    exit 2
fi

python3 - "$CMAKE_LIST" "$EXAMPLES_CMAKE_LIST" "$GATE_TARGET" "$REPO_ROOT" <<'PY'
import os
import re
import sys

cmake_list, examples_list, gate_target, repo_root = sys.argv[1:5]

problems = []


def fail(msg):
    problems.append(msg)
    print("✗ %s" % msg, file=sys.stderr)


text = open(cmake_list, encoding="utf-8").read()

# ── ① 解析 pairs 单一真源 ─────────────────────────────────────────────
# 形态:set(CPPTLM_TOPOLOGY_VALIDATION_PAIRS  "<name>|<path>" ...)
pairs_block = re.search(
    r"set\(CPPTLM_TOPOLOGY_VALIDATION_PAIRS\s*(.*?)\)", text, re.S)
if not pairs_block:
    print("✗ %s: 未找到 `CPPTLM_TOPOLOGY_VALIDATION_PAIRS` 单一真源清单" % cmake_list,
          file=sys.stderr)
    sys.exit(1)

pairs = []
for line in pairs_block.group(1).splitlines():
    line = line.strip()
    m = re.match(r'^"([^"|]+)\|(.+)"$', line)
    if m:
        pairs.append((m.group(1).strip(), m.group(2).strip()))

if not pairs:
    print("✗ %s: 单一真源清单为空(组合 target 将不校验任何 config)" % cmake_list,
          file=sys.stderr)
    sys.exit(1)

src = "${CMAKE_SOURCE_DIR}"
names = [n for n, _ in pairs]
paths = [p.replace(src, repo_root) for _, p in pairs]

# ── ② 组合 target 必须从单一真源派生(⛔ 禁手抄副本)────────────────────
combined = re.search(r"add_custom_target\(validate_topology(.*?)\n    \)", text, re.S)
if not combined:
    print("✗ %s: 未找到 `add_custom_target(validate_topology ...)`" % cmake_list,
          file=sys.stderr)
    sys.exit(1)
combined_body = combined.group(1)

# ⛔ 注释行不构成命令面 ⇒ 判据只看**有效**行(去 CMake 注释 `#...`)。
#    否则「把挂点注释掉」会因字面量仍在而假绿(实测踩到)。
combined_code = "\n".join(
    l.split("#", 1)[0] if not l.lstrip().startswith("#") else ""
    for l in combined_body.splitlines()
)

# ② 组合 target 必须从单一真源派生(⛔ 禁手抄副本)
if "CPPTLM_TOPOLOGY_VALIDATION_COMMANDS" not in combined_code:
    fail("%s: 组合 target **未**从 `CPPTLM_TOPOLOGY_VALIDATION_COMMANDS` 派生 —— "
         "手抄 config 清单是 2026-11-12 之前 `validate_topology` 跑不完的结构性根因"
         % cmake_list)

# 手抄检测:组合 target 的命令面里若直接出现 `configs/...json` 路径 ⇒ 副本
inline_paths = re.findall(r"configs/[A-Za-z0-9_./-]+\.json", combined_body)
if inline_paths:
    fail("%s: 组合 target 内**手抄**了 config 路径 %s —— 必须经单一真源派生"
         % (cmake_list, sorted(set(inline_paths))))

# ── ③ 闸挂点必须真在组合 target 的命令面里 ───────────────────────────
# 按 **CMake 变量语义**追推导链(⛔ 不匹配字面量):
#   组合 target 必须引用 `${CEP_GATE_COMMAND}`;
#   `CEP_GATE_COMMAND` 必须由 `set(... $<TARGET_FILE:<闸目标>> ...)` 赋值;
#   该赋值必须在 `if(TARGET <闸目标>)` 守卫内;
#   <闸目标> 必须被 `add_executable` 真正定义。
if "CEP_GATE_COMMAND" not in combined_code:
    fail("%s: 组合 target 的命令面**不含** `${CEP_GATE_COMMAND}` ⇒ 装配闸离线挂点"
         "静默失效(`ADR-CEP-09` 抓取点 1);而 ctest 侧同名用例仍绿 ⇒ 假绿"
         % cmake_list)

gate_set = re.search(r"set\(CEP_GATE_COMMAND\s+(.*?)\n\s*\)", text, re.S)
if not gate_set:
    fail("%s: 未找到 `set(CEP_GATE_COMMAND ...)` ⇒ 闸挂点来源不可推导" % cmake_list)
else:
    gate_set_body = gate_set.group(1)
    if ("$<TARGET_FILE:%s>" % gate_target) not in gate_set_body:
        fail("%s: `CEP_GATE_COMMAND` 的赋值里不含 `$<TARGET_FILE:%s>` ⇒ 它可能指向别的"
             "可执行文件 ⇒ 装配闸挂点名存实亡" % (cmake_list, gate_target))

# 赋值必须落在 `if(TARGET <闸目标>)` 守卫内 ⇒ 否则守卫形同虚设
guard = re.search(r"if\(TARGET\s+%s\)(.*?)\n\s*endif\(\)" % re.escape(gate_target), text, re.S)
if not guard:
    fail("%s: 未找到 `if(TARGET %s)` 守卫 ⇒ 闸挂点的存在性无法被条件化"
         % (cmake_list, gate_target))
elif "set(CEP_GATE_COMMAND" not in guard.group(1):
    fail("%s: `set(CEP_GATE_COMMAND ...)` 不在 `if(TARGET %s)` 守卫内 ⇒ "
         "守卫与赋值脱钩" % (cmake_list, gate_target))

# 该 target 必须真的被定义(否则 `if(TARGET ...)` 恒假 ⇒ 挂点永远为空)
if not os.path.isfile(examples_list):
    fail("%s: 未找到 %s(无法确认 `%s` 是否被定义)" % (cmake_list, examples_list, gate_target))
else:
    ex_text = open(examples_list, encoding="utf-8").read()
    if ("add_executable(%s " % gate_target) not in ex_text:
        fail("%s: `%s` 未被 `add_executable` 定义 ⇒ `if(TARGET ...)` 恒假 ⇒ 挂点永远为空"
             % (examples_list, gate_target))

# ── ④ 调用面 ↔ 磁盘:每个 config 必须真实存在 ─────────────────────────
for name, raw, real in zip(names, (p for _, p in pairs), paths):
    if not os.path.isfile(real):
        fail("%s: config `%s`(target `%s`)在磁盘**不存在**:%s"
             " ⇒ 该 target 首条即失败 ⇒ 其后所有命令(含闸挂点)静默不执行"
             % (cmake_list, raw, name, real))

# ── ⑤ 个体 target 必须由同一清单派生(不得有一方漏项)──────────────────
# 个体 target 的创建必须发生在遍历 pairs 的循环里(变量名)⇒ 二者结构上同源。
loop = re.search(r"foreach\(_pair IN LISTS CPPTLM_TOPOLOGY_VALIDATION_PAIRS\)(.*?)\n    endforeach\(\)",
                 text, re.S)
if not loop:
    fail("%s: 未找到遍历 `CPPTLM_TOPOLOGY_VALIDATION_PAIRS` 的 foreach ⇒ "
         "个体 target 的来源不可推导" % cmake_list)
else:
    loop_body = loop.group(1)
    if "add_topology_validation(" not in loop_body:
        fail("%s: pairs 循环内**未**调用 `add_topology_validation(...)` ⇒ "
             "个体 target 与组合 target 不再同源" % cmake_list)
    if "list(APPEND CPPTLM_TOPOLOGY_VALIDATION_CONFIGS" not in loop_body:
        fail("%s: pairs 循环内**未**追加 `CPPTLM_TOPOLOGY_VALIDATION_CONFIGS` ⇒ "
             "组合 target 的 config 清单与个体 target 脱钩" % cmake_list)

# 组合 target 的命令列表必须由该清单派生
if not re.search(r"foreach\(_cfg IN LISTS CPPTLM_TOPOLOGY_VALIDATION_CONFIGS\)"
                 r"(.*?)\n    endforeach\(\)", text, re.S):
    fail("%s: 未找到遍历 `CPPTLM_TOPOLOGY_VALIDATION_CONFIGS` 的 foreach ⇒ "
         "组合 target 的命令面来源不可推导" % cmake_list)

# ── 汇总 ───────────────────────────────────────────────────────────────
if problems:
    print("✗ validate_topology 对账闸: %d 项问题" % len(problems), file=sys.stderr)
    sys.exit(1)

print("✓ validate_topology 对账闸通过:"
      " %d 个 config 全部存在于磁盘 · 组合 target 经单一真源派生 · 闸挂点 `%s` 在位"
      % (len(paths), gate_target))
PY
rc=$?
exit $rc
