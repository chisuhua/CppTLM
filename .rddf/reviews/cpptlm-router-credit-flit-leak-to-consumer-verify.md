# 独立验收报告 — `cpptlm-router-credit-flit-leak-to-consumer`

- **日期**: 2026-11-16
- **验收者**: verifier（独立于实施者）
- **对象**: 已归档 change `openspec/changes/archive/2026-11-16-cpptlm-router-credit-flit-leak-to-consumer/`
- **决策源**: 台账 Entry #454（立项）/#455（判据裁决）/#456（实施）/#457（归档）/#458（AC-07 收敛）
- **已用轮次**: 1 / 3
- **分流**: `rddf rdd-verify --dry-run` → `No eligible changes to verify (empty queue)`（0 active）⇒ **本地验收路径**（verifier 产出契约 1–5 步），未走 `rdd-verifier` skill

## 判据来源

1. 归档 proposal 的 `## Acceptance`（AC-01 ~ AC-07，含 Entry #455 的 AC-05/AC-06 修订与 Entry #458 的 AC-07 收敛）
2. `.rddf/improvements/cpptlm-router-credit-flit-leak-to-consumer.md` 的 AC 表（tracked 权威副本）
3. 用户指令：实施完 → 按 builder 固定审查者（same-role verifier）独立验收
4. ADR-X.15 的 Status Update「真根因」段

## 验证顺序（早失败早停）

1. 编译/静态检查 ✅（`cmake --build build` 无 error）
2. 单元测试 ✅ `[credit]` 61 断言 / 4 用例
3. 集成 ✅ `[mesh]` 251 断言 / 19 用例 · `ctest -R cep -j2` **100/100**
4. 边界与错误路径 ✅（MESH-7 的空断言防护、MESH-3a 开环边界）
5. 遗留清扫 ✅（见末节）

## 逐条判定 + 证据

| AC | 判定 | 证据（命令 + 结果） |
|:--:|---|---|
| **AC-01** credit 三面零变化 | **PASS** | `./build/bin/cpptlm_tests "[credit]"` → `All tests passed (61 assertions in 4 test cases)`。MESH-7 断言链：先注入真 flit(`dst=3,hops=2`) ⇒ `flits_received()==1` / `last_recv_dst_node()==3` / `last_recv_hops()==2` / `pe_resp_out().valid()` / `data().transaction_id==7` / `credits_dropped()==0`；再注入 credit ⇒ `credits_dropped()==1` / `flits_received()==1` / `last_recv_dst_node()==3` / `last_recv_hops()==2` / `REQUIRE_FALSE(pe_resp_out().valid())` |
| **AC-02** 端到端过滤生效 | **PASS** | MESH-8：`hier.nic_a_credits_dropped > 0`、`hier.nic_b_credits_dropped > 0`、`flat.nic_a_credits_dropped > 0`、`flat.nic_b_credits_dropped > 0`（两侧都有 credit 到达并被丢弃）;`hier.nic_a_last_recv_hops > 0`（无 `hops==0` 计入）。⚠️ flat 侧无对应显式断言，由 AC-03 的 `flat.nic_b_last_recv_hops == 3` 间接覆盖 —— 见 F-2（SHOULD_FIX，非阻断） |
| **AC-03** 观测稳定性 | **PASS** | MESH-8：`hier.nic_b_last_recv_dst == 3` ∧ `hier.nic_b_last_recv_hops == 3` ∧ `flat.nic_b_last_recv_dst == 3` ∧ `flat.nic_b_last_recv_hops == 3` —— **两种装配各自成立**（修复前该组合不可能成立：credit 会把观测覆写成 0/0） |
| **AC-04** 回归守卫（router 四元组不变） | **PASS**（取证方式 = 代码级零 diff，强于数值断言） | `git diff --stat 278f9377..HEAD -- src/tlm/router_tlm.cc include/tlm/router_tlm.hh src/core/module_factory.cc include/core/simple_port.hh include/bundles/` → **全部空**；`include/tlm/cep/` 仅 `cohif_nic.hh +20 -0`（其余 CEP 模块零 diff）⇒ router 转发语义在代码层面不可能改变 |
| **AC-05** 等价判据形态 | **PASS** | **MESH-3a**（开环）：`REQUIRE(hier.routers[i].flits/packets/hops == flat.routers[i].*)` **3 条 `==` 断言完整未放宽**（`sed -n '/MESH-3a/,/^}/p' \| grep -cE "== flat"` → 3）+ `hier.nic_b_recv == flat.nic_b_recv`（实测 **`8 == 8`**，Catch2 `-s` 展开）+ `nic_b_recv > 0`。**MESH-3b**（闭环）：各自 `routers.size()==4` / `routers[0].flits>0` / `routers[3].flits>0` / `routers[3].hops > routers[3].flits` / `nic_a_dropped==0` / `nic_b_dropped==0` / `nic_b_recv>0` + `hier.internal_router_count==4`。**四处重基线未弱化**：MESH-4 的 `stub_completions_in > 0` / `nic_a_recv > 0` 原样保留；`hier.nic_b_recv == flat.nic_b_recv` 未被改成 `> 0`，而是**迁移到开环面**（开环下成立）。见 F-1（数值记录不全，SHOULD_FIX） |
| **AC-06** 已知限制显式化 | **PASS** | `grep -c "credit 耗尽"` → 归档 `design.md` **2 处** + `openspec/specs/cep-cohif-network-adapter/spec.md` **1 处**（spec 的「已知限制」注 + design 的 D-5 / D-3）；测试 run = 300 周期，实测 NIC 注入 ~20 flit < `BUFFER_DEPTH × NUM_VCS` = 32 ⇒ 未越界 |
| **AC-07** 伪 Completion 消失 | **PASS**（判别力已证） | MESH-9：`home.counters().foreign_consumed == 0` ∧ `w.net_completions_in <= home.counters().requests_consumed`（两侧各断言）。**判别力对照**（Entry #458 实测，实施期的临时关过滤实验）：修复前 `foreign_consumed = 7` / `net_completions_in = 11~12 > requests_consumed = 7`（必红）；修复后 `0` / `5~7 <= 7~8`（绿）⇒ 非恒真判据。⚠️ 原第三条（每条 `CohIf` 的 `transaction_id != 0`）已被 AC 收敛掉（不可机械取证），由前两条机械覆盖 —— 见 F-3（记录） |

## 失败项与路由建议

| # | 分类 | 内容 | 路由 |
|:--:|---|---|---|
| — | **无 implementation_gap** | 全部 7 条 AC 通过 | — |
| **F-1** | `noted`（SHOULD_FIX，不阻断） | **AC-05 的「逐条列出旧值 → 新值 + 推导依据」只部分满足**：归档 `design.md` D-3.1 列了 router 侧三组实测（15/8/7/15 等），但 **NIC 侧开环值未记录**（`nic_b_recv = 8`）。建议补进 design.md（归档件在 `.gitignore` 内 ⇒ 只作本地一致；可持久记录已由 Entry #458 承载） | 可选（builder 后续归档时补） |
| **F-2** | `noted`（SHOULD_FIX，不阻断） | **AC-02 的「无 `hops==0` 计入」在 flat 侧无显式断言** —— 由 AC-03 的 `flat.nic_b_last_recv_hops == 3` 间接覆盖。判据实际成立（flat 侧若被 credit 覆写则为 0，AC-03 会红），但显式断言更强 | 可选（后续加 `flat.nic_a_last_recv_hops > 0`） |
| **F-3** | `noted`（记录） | **AC-07 由 3 条收敛为 2 条** —— 原第三条不可机械取证（`CohIfResponderTLM::consume()` 的 default 分支不记录 transaction_id 且不暴露逐条观测）。收敛逻辑：伪 Completion 的**唯一**来源是 credit ⇒ 「无伪 Completion」⟺「无全零 Completion」；前两条已机械覆盖。已在 Entry #458 与 improvement（tracked）登记 | 无需动作（记录） |

> 说明：F-1 / F-2 / F-3 均为**判据记录粒度**问题，**不影响**已验证的能力；AC 的功能要求全部有工具证据支撑。

## 遗留清扫（verifier 复核实施者的自查）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX（`cohif_nic.hh` / `test_cep_mesh_e2e.cc`） | 无 |
| 临时诊断残留（`TEMP-DIAG` / `TEMP-EXP` / `ac07_diag` / `mesh3_diag`） | 无（`grep -rn` 全仓 0 命中；`/tmp/opencode/` 的实验备份与日志已清） |
| 被 skip / disabled 的测试 | 无（无 `*.disabled`，无 `SKIP(` / `.skip`） |
| 未跟踪的仓库残渣 | 无（`git status --porcelain` 干净） |
| 注释掉的代码块（本次 diff） | 0 |
| 未替换的 mock | 无 |

## 结论

**PASS** —— AC-01 ~ AC-07 全部通过，**无 `implementation_gap`**。change 的核心承诺已由工具证据支撑：

1. credit flit 在 ingress 被丢弃（三面零变化 + 可观测计数）；
2. NIC 侧观测恢复可信（`last_recv_*` 在**两种装配下都稳定**为真请求值）；
3. 等价性判据**未弱化** —— 从「闭环稳态计数」换到「开环转发语义」（MESH-3a 三条 `==` 完整，实测 `8 == 8`）；
4. 伪 Completion 消失有**判别力**（修复前必红、修复后绿）；
5. `RouterTLM` / `module_factory` / bundles **代码级零 diff**。

**证据边界**：AC-04 的取证采用**代码级零 diff**（比数值断言更强，无须额外运行）；AC-05 的 NIC 侧开环数值未进 design.md（F-1）；AC-02 的 flat 侧无显式 `hops==0` 断言（F-2，由 AC-03 间接覆盖）。除上述三点外，所有判定均有工具证据。
