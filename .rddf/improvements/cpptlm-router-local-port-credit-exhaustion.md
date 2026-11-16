---
name: cpptlm-router-local-port-credit-exhaustion
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🟡 **已立项**(2026-11-17 · 台账 Entry **#459** · KPI **0/3 → 1/3** · **决策源 = `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`(Accepted)**,方案由 architect 裁决为**方案 A** · 经 metis 审查修订(15 条全采纳))
created: 2026-11-16
---

# improvement: cpptlm-router-local-port-credit-exhaustion

**主题**：`RouterTLM` 的 `LOCAL` 端口（索引 4）用**同一套** downstream credit 记账对待 mesh 链路与终端接入点。mesh 链路正确（下游是 router，会归还 credit）；`LOCAL` 的典型下游是**终端**（`NICTLM` / `CohIfNICTLM` / `ComputeUnitTLM`），其网络侧入口是**单向下行** adapter、**无归还 credit 的通道** ⇒ 每 `(LOCAL, vc)` 投递满 `BUFFER_DEPTH = 8` 个 flit 后本地投递**静默停滞**（`credit_timeout_` 默认 0 ⇒ 无超时、无计数、无日志）。

> **决策源**: `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`（**Accepted** · oracle APPROVE + critic APPROVE_WITH_CONDITIONS · 0 BLOCKER · 审稿报告 `.rddf/reviews/ADR-X.16.md`）
> **发现场景**: 层次化 CEP mesh 验收期间（`ADR-X.15` 关联段的 credit 机理追查）⇒ 台账 Entry **#452** / **#453**
> **同源另一面**: `cpptlm-router-credit-flit-leak-to-consumer`（credit **泄漏给消费者**）—— 已归档 + 独立验收 PASS

---

## Why

### 第 1 条 —— credit 记账对所有 `out_port` 一视同仁

`src/tlm/router_tlm.cc` 的 `has_credit` / `consume_credit` **不区分端口语义**（`:300-307`、`:352-367`）；`downstream_credits_` 初值一律 `BUFFER_DEPTH = 8`（`router_tlm.hh:134,277`；`router_tlm.cc:57`）。credit 不足 ⇒ flit 无法推进。

### 第 2 条 —— credit 归还是 **router↔router 链路**的机制

`return_credit_to_upstream(in_port, vc)`（`router_tlm.cc:323/359/383`）→ `reverse_port(in_port)`（`router_tlm.hh:223-232`）→ `BidirectionalPortAdapter::send_credit`。⇒ 归还闭环只在**两个 router 之间**成立。

### 第 3 条 —— `LOCAL` 的下游结构上无法归还

终端消费者的网络侧入口是**单向下行** `InputStreamAdapter`（`include/framework/stream_adapter.hh:30-95`；`include/tlm/cep/cohif_nic.hh:118`）⇒ 无通往 router credit-receive 端口的通道。⇒ 每 `(LOCAL, vc)` 8 个 flit、`NUM_VCS = 4` ⇒ 每 LOCAL 口 **32 个 flit 后彻底停**。

### 第 4 条 —— 失败是**静默**的

`credit_timeout_` 默认 **0** ⇒ `credit_safety_reset()` 首行 `return`（`router_tlm.hh:298`；`router_tlm.cc:503-506`）⇒ 无死锁恢复、无计数、无日志。测试只会看到「接收数到 8 就上不去」。

### 第 5 条 —— 全仓检索确认前提：生产拓扑的 `LOCAL` 只挂终端

| 类别 | 实例 |
|---|---|
| `NICTLM` | `configs/mesh_2x2_tlm.json` / `mesh_4x4_tlm.json` / `mesh_4x4.json` / `configs/test/nic_router_nic.json` + 4 处测试 |
| `CohIfNICTLM` | `configs/cep_mesh_e2e.json`（经 `fabric` 暴露）+ `test/test_cep_mesh_e2e.cc` 平铺基线 |
| `ComputeUnitTLM` | `configs/apu_soc_full.json`（JSON 声明层仅入向；绑定层 Step 7b 自动补反向；该 config 因 `ComputeUnitTLM` 未注册当前不可装载） |
| `MemoryTLM` | 仅 Python 分析器 fixture |
| **LOCAL↔router** | **仅 2 处装配期结构性测试**（`test/test_simmodule_exposed_port_wiring.cc:101-157` 的 EXP-1/2、`test/test_cep_mesh_e2e.cc:408-434` 的 MESH-6），且**只断言 `isBound()`、不跑流量** ⇒ 不触发 credit 记账 |
| **LOCAL 悬空** | 4 份 config 完全不用 LOCAL + 2 份只接入方向 |

### 第 6 条 —— 同源另一面已修，本 improvement 修剩余那一面

`cpptlm-router-credit-flit-leak-to-consumer` 修 **credit flit 泄漏进消费者数据面**；本 improvement 修 **credit 记账把终端端口饿死**。同源于「`LOCAL` 下游不是 credit-aware 实体」。

---

## What Changes（**方案 A 已由 architect 裁决**，per ADR-X.16 D1~D5）

1. **D1 — `LOCAL` 端口不参与 downstream credit 流控**：`has_credit(LOCAL, vc)` 恒真；`consume_credit(LOCAL, vc)` 不扣减。
   - **理由**（ADR D2）：credit 是 router↔router 链路的 VC buffer 反压（归还闭环成立）；`LOCAL` 是**终端接入点**，反压由 ChStream 的 `valid/ready` 承担。业界同构实现（Garnet / gem5 的 local port）同样不参与 credit 流控。
   - ⚠️ **实施约束（metis）**：短路必须**在 `out_port` / `vc` 边界检查之后** —— 否则 `vc >= NUM_VCS` 的非法 VC 会被判成「有 credit」而静默放行（原实现 `:448-450` 的边界检查不得被绕过）。
2. **D4 — mesh 侧 credit 死锁可观测**：新增 stall 计数。⚠️ **锚点已按 metis 复核订正**（见下「⚠️ ADR D4 锚点订正」）：
   - **计数口径** = 「**存在待发 flit 且 `has_credit(out_port, out_vc) == false`**」的**周期累计数** —— 即 **VA / SA 阶段的拒绝条件**（`router_tlm.cc:226-229`、`:266`），**不是** `waiting_for_credit` 位。
   - **API 形态**：在 `RouterTLM::stat_group_` 增 **标量** `credit_stall_cycles`（跨 `(port, vc)` 累计），经既有 `stats()` 观测面读出（与 `flits_forwarded` 同面）。
   - **日志** `DPRINTF` = 每 `(port, vc)` 每 **16 cycles** 一次（阈值绝对值以 AC-03 为准；`2 × BUFFER_DEPTH` 仅为取值依据）。
   - ⛔ **不改 `credit_timeout_` 默认值（0）**：周期性重置 credit 会**虚增**下游可用额度（超发）且掩盖真实死锁。
3. **D5 — 演进 seam**：`RouterPort::LOCAL` 注释从「本地（连接 NICTLM）」改为**显式声明终端语义**；未来若需 LOCAL↔router 流控，**另立决策**引入 per-port `credit_aware`（本 change **不做**）。
4. **`downstream_credits_[LOCAL][*]` 死状态**：**二选一**并在实现中显式处置 —— (a) 加注释说明其不再被维护，或 (b) 从记账循环中跳过（省开销）。二者择一，须可 grep 验证。
5. **spec 落点**：**新建 capability spec `router-local-port-flow-control`**（全仓检索确认：**没有任何 main spec 承载 RouterTLM 的流控语义** —— RouterTLM 仅在配置类 spec 中作为模块类型被提及）。
   - spec 内用**两条 Requirement** 明确「`LOCAL` 终端语义」与「mesh credit stall 计数」是同一能力的两个面（metis：当前名只表达第一半，与 C-2 内容不匹配）。
   - **新 spec 须显式声明**：`openspec/specs/port-type-system/spec.md` 里 `LOCAL` 的 `role = BI_DIRECTIONAL` **不变**，仅 credit 记账语义终止（避免两处口径无人对齐）。
   - **既有真源同步**：`openspec/specs/cep-cohif-network-adapter/spec.md` 的「已知限制」句（指回本 improvement 的 LOCAL credit 耗尽）在 D1 落地后**变成过期陈述** ⇒ 由本 change 的 delta spec 改为「已解决 / 由新 spec 承接」。

### ⚠️ ADR D4 锚点订正（需 architect 追加 Status Update）

metis 复核发现：**ADR-X.16 的 D4 锚点（`waiting_for_credit` 状态）与真实停滞状态不重合** —— `waiting_for_credit` 只在 ST 阶段「SA 已放行、ST 复查发现 credit 为 0」时置位（`router_tlm.cc:352-361`）；而 credit 归零后 **VA 阶段先 `!has_credit` 就 `release_vc` 并跳过**（`:226-229`）、SA 阶段直接不选 winner（`:266`）⇒ flit 停在 `input_buffer_`，`waiting_for_credit` **保持 false** ⇒ 按 ADR 原文实现的计数器**会恒为 0**。

⇒ 本 change 按**订正后锚点**（VA/SA 拒绝条件）实施；ADR 文本**不改**（不可变原则），由 **architect** 在 `ADR-X.16` 追加一条 Status Update 澄清锚点（**planner 无权改 ADR 决策**）。

### ⛔ 明确不做（ADR 选项 B/C/D/E 的否决结论）

| 选项 | 否决理由 |
|---|---|
| B. per-port `credit_aware` 配置 | 零生产消费者（唯一 LOCAL↔router 实例不跑流量）⇒ 为无需求场景付**配置契约变更**成本；含 programmatic setter 变体同样零调用方。留作 D5 的演进 seam |
| C. 开启 `credit_timeout_` 默认值 | 重置 credit = **虚增**下游可用额度 ⇒ mesh 链路超发；且掩盖真实死锁 |
| D. 终端消费者实现 credit 归还 | 缺「通往 router credit-receive 端口的通道」（Step 7b 不建该连线）+ 缺 credit 生成逻辑 ⇒ 要动所有消费者与拓扑契约 |
| E. 只登记为已知限制 | 静默停滞是**不可观测的正确性缺陷** ⇒ 测试会把「8 就上不去」当正常 |

### ⛔ 实施时不得顺手改（本 change 范围外的同文件项）

- `RouterTLM` 的 `vc_count` / `buffer_size` param 语义（`buffer_size` 本就不生效，属独立缺陷）；
- `NICTLM` 的 credit flit 过滤（同族缺陷）。

## Acceptance

> 判据对应 `ADR-X.16` 的 **Verification（V-1~V-6）**，并按 metis 审查补齐可执行性（**立项后回填 change AC 编号**）

### 测试装置与观测面（AC-01 / AC-02 通用，metis 写死）

| 项 | 规定 |
|---|---|
| **装置** | **裸 `RouterTLM`（`adapter_ == nullptr`）+ 直接向 `req_in[4]` 注入**（复用 `test/test_noc_integration.cc:14-38` 的自环手法：`dst_node` = 本节点 ⇒ `computeRoute` 选 `out_port = LOCAL`）。**不需要**新拓扑、**不需要**「来自 mesh 另一侧」的注入源、**不需要**新 sink 模块 |
| **注入节拍** | 全局 **1 flit/cycle**（否则 `router_tlm.cc:149-155` 的输入 buffer 满 ⇒ flit 被**静默丢弃**，得到与 D1 无关的假失败） |
| **观测** | 每周期读 `resp_out[4]` 后**立即 `clear_valid()`**，以「**读到的 flit 数**」计为下游接收数（⛔ 不可用「valid 为真的周期数」—— `OutputStreamAdapter::write()` 不清 valid，会恒为真且每周期覆盖写）。`router.stats()` 的 `flits_forwarded` 仅作**辅助**（它在入 `pending_link_` 时 +1，而 `pending_link_` 是**全局单队列、每周期只 drain 1 个** ⇒ 领先真实 egress） |
| **周期预算** | 按**全局 egress 1 flit/cycle** 放大：40 flit 用例 **≥ 120 cycles**；80 flit 用例 **≥ 200 cycles**（含 pipeline 填充与队列积压） |
| **事务 id** | 每个 packet **唯一 `transaction_id`**（`routing_table_` 以其为键，复用会导致 TAIL 擦除后查不到路由） |

| # | 判据（机器可判） | ADR |
|:--:|---|:--:|
| **AC-01** | 裸 `RouterTLM` 自环注入、**单 VC**、40 个 flit ⇒ 「读到的 flit 数」**== 40**（不出现 8 的平台期） | V-1 |
| **AC-02** | **4 个不同 packet 同时在飞**、分别置于 `in_vc` 0..3（`out_vc` 由 `allocate_vc(out_port)` 决定、**非** `in_vc` 直通）⇒ 全部到达（累计 80）且 **drain 计数 == 80**。⚠️ 源须**直接注入并手写 `vc_id`**（`CohIfNICTLM` 恒 `vc_id = 0` ⇒ 4 VC 维度不可达；`NICTLM` 的 credit 过滤未修 ⇒ 不作计数基准） | V-2 |
| **AC-03** | **停滞可观测（钉 mesh out_port）**：裸 `RouterTLM`（`adapter_ == nullptr` ⇒ `send_credit_signal` 在 `router_tlm.cc:492-495` 直接 return ⇒ credit 永不归还）向 **mesh 向**注入 8+ 个 flit ⇒ `stats()` 的 `credit_stall_cycles` 在累计 ≥16 cycles 后**递增**（按订正后锚点 = VA/SA 拒绝周期）；且 `credit_timeout_` 默认值**仍为 0**。**反向判据**：挂上 `BidirectionalPortAdapter`（或双 router 互联）后同一计数**停止增长**（防「每周期无脑 +1」的假实现） | V-3 |
| **AC-04a** | **具名回归集全绿**：`test/test_cep_mesh_e2e.cc` 的 MESH-3a / MESH-3b / MESH-4 / MESH-5 / MESH-7 / MESH-8 / MESH-9 · `test/test_noc_mesh_binding_semantics.cc` · `test/test_e2e_simulation.cc` 的 nic 用例 · `[cep]` / `[e2e]` / `[pcie]` 全量 | V-4 |
| **AC-04b** | **数值变化须逐条重推导**：上述任何 `flits` / `packets` / `hops` / `recv` 数值若发生变化 ⇒ 必须在 change 的 `design.md` 列出「**旧值 → 新值 → 为何是 D1 的必然结果**」。⛔ **禁止就地改 `REQUIRE` 常量**（禁止静默重基线） | V-4 |
| **AC-05** | **死状态处置**：`downstream_credits_[LOCAL][*]` 的处置（(a) 注释说明 / (b) 跳过记账循环）已落地且**可 grep 验证** | ADR Consequences |
| **AC-06** | **真源同步**：`cep-cohif-network-adapter` 的「已知限制」句已由本 change 的 delta spec 改为「已解决 / 由 `router-local-port-flow-control` 承接」；新 spec 显式声明 `port-type-system` 的 `LOCAL` `BI_DIRECTIONAL` role **不变** | metis P1-7 |
| **AC-07** | **LOCAL 侧不误报**：AC-01 / AC-02 场景下 `credit_stall_cycles` **保持 0**（证明 D1 与 D4 不互相污染） | metis P1-8 |

### 附加检查（**guard，不占验收编号** —— metis P1-9）

| # | 内容 |
|:--:|---|
| G-1 | `LOCAL↔router` 的结构性测试（EXP-1/2、MESH-6）仍绿 ⇒ `isBound()` 语义未受影响（**结构上不可能因 D1 变红**，故降级为 guard） |
| G-2 | `grep -n "LOCAL" include/tlm/router_tlm.hh` ⇒ `RouterPort::LOCAL` 注释已显式声明终端语义 |

### ⚠️ 口径说明

- **以 ADR V-3 为准**：improvement 旧版 AC-03 允许「`credit_timeout_` 触发告警」作为替代口径，ADR V-3 更严（钉死 mesh 场景 + 默认值仍为 0）。
- **AC-01/AC-02 与「避免用 `NICTLM`」**：注入源一律**直接注入并手写 `vc_id`** ⇒ 不与该红线冲突；全拓扑口径（`CohIfNICTLM` 作下游）作为**第二条独立用例**，此时 `credits_dropped() > 0` 属正常，**不得**断言其为 0。

## Capabilities

| # | 能力 | capability spec | 新增 `INV-CEP-*` |
|:--:|---|:--:|:--:|
| C-1 | `LOCAL` 端口的**终端流控语义**（不参与 credit） | ✅ 新建 `router-local-port-flow-control`（Requirement 1） | ❌ 0 |
| C-2 | mesh 侧 credit 死锁可观测（不再静默） | ✅ 同上（Requirement 2） | ❌ 0 |

## Impact

- **改动**：`src/tlm/router_tlm.cc`（`has_credit` / `consume_credit` 的 LOCAL 分支 + stall 计数）、`include/tlm/router_tlm.hh`（`stat_group_` 增 `credit_stall_cycles` + `RouterPort::LOCAL` 注释）、`test/` 新单测（AC-01~AC-04b）、**新建** `openspec/specs/router-local-port-flow-control/`（经 change delta spec 归档时合并）+ 既有 `cep-cohif-network-adapter` 的已知限制句修订。
- **⛔ 不动**：`module_factory.cc`、`CohIfNICTLM`、`NICTLM`、JSON schema（**零配置面变更**）。
- **回归面**：所有基于 `RouterTLM` 的拓扑（`[cep]` / `[e2e]` / `[pcie]` / mesh）。⚠️ 既有 `LOCAL` 口流量大的测试可能因「不再停滞」而**行为变化**（接收数变多）⇒ 按 AC-04b 逐条复核。
- **回滚路径**：零配置面 ⇒ `git revert` 即完整回滚，无数据 / 契约迁移成本。

### ⛔ 范围外（ADR 关联段登记，**须另行立项**）

1. **`buffer_size` param 不生效** —— `get_param_rules` 声明该参数（默认 16、范围 1–64），但 `BUFFER_DEPTH` 是 `constexpr 8`（`include/tlm/router_tlm.hh:134`）。
2. **`NICTLM` 缺 credit flit 过滤**（同族缺陷）—— `src/tlm/nic_tlm.cc` 全文无 `CATEGORY_CREDIT` 检查 ⇒ `mesh_2x2_tlm` / `mesh_4x4_tlm` 类拓扑里 router 对 `LOCAL` 的正常 credit 回流会进 `NICTLM::reassemble`，污染 `flits_received` 计数并产出垃圾 `pe_resp_out`。`CohIfNICTLM` 侧已修，`NICTLM` 侧未修。

## ⚠️ 关联

- **决策源**: `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`（Accepted）· 审稿报告 `.rddf/reviews/ADR-X.16.md`
- 同源另一面（已归档）: change `cpptlm-router-credit-flit-leak-to-consumer` · `.rddf/reviews/cpptlm-router-credit-flit-leak-to-consumer-verify.md`
- 台账: Entry **#459**（本 change 立项 + metis 修订）· **#458** · **#455** · **#454** · **#453** · **#452**
- 相邻决策: `ADR-X.15`（Status Update 已把本条显式推迟为 backlog ⇒ 本 change 正是接该路径）
