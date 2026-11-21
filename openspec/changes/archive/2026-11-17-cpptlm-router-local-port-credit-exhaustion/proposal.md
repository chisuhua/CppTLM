# change: cpptlm-router-local-port-credit-exhaustion

**主题**: 让 `RouterTLM` 的 `LOCAL` 端口成为**显式定义的终端端口** —— 不再参与 router↔router 链路的 downstream credit 流控 ⇒ 消除「每 `(LOCAL, vc)` 满 8 个 flit 后本地投递**静默停滞**」的既有缺陷；同时补上 mesh 侧 credit 死锁的**可观测面**。

> **决策源**: `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`（**Accepted** · oracle APPROVE + critic APPROVE_WITH_CONDITIONS · 0 BLOCKER · 审稿报告 `.rddf/reviews/ADR-X.16.md`）
> **上游**: 台账 Entry **#452**（F-3 初判）· **#453**（根因订正）· **#454**（同源另一面立项）· **#455**（判据裁决）· **#458**（AC-07 收敛）· **#459**（本 change 立项 + metis 修订）
> **improvement**: `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`（5 段 + AC-01~AC-07 + G-1/G-2 guard）
> **同源另一面（已归档）**: change `cpptlm-router-credit-flit-leak-to-consumer` —— 修 credit **泄漏给消费者**；本 change 修 credit **记账饿死终端**

---

## Why

### 第 1 条 —— credit 记账对 `LOCAL` 与 mesh 链路一视同仁，但 `LOCAL` 的下游**结构上无法归还 credit**

`src/tlm/router_tlm.cc` 的 `has_credit` / `consume_credit` 不区分端口语义（`:300-307`、`:352-367`）；`downstream_credits_` 初值一律 `BUFFER_DEPTH = 8`。而 credit 归还只在 **router↔router** 之间成立（`return_credit_to_upstream(in_port)` → `reverse_port(in_port)`，`:323/359/383`）；`LOCAL` 的典型下游是**终端**（`NICTLM` / `CohIfNICTLM` / `ComputeUnitTLM`），其网络侧入口是**单向下行** adapter ⇒ 无通往 router credit-receive 端口的通道。

⇒ 每 `(LOCAL, vc)` 8 个 flit、`NUM_VCS = 4` ⇒ 每 LOCAL 口 **32 个 flit 后彻底停**。

### 第 2 条 —— 失败是**静默**的

`credit_timeout_` 默认 **0** ⇒ `credit_safety_reset()` 首行 `return`（`router_tlm.hh:298`；`router_tlm.cc:503-506`）⇒ 无恢复、无计数、无日志。测试只会看到「接收数到 8 就上不去」，且无法区分「设计如此」与「缺陷」。

### 第 3 条 —— 全仓检索：生产拓扑的 `LOCAL` **只挂终端**

NICTLM（`mesh_2x2_tlm` / `mesh_4x4_tlm` / `mesh_4x4` / `nic_router_nic` + 4 处测试）· CohIfNICTLM（`cep_mesh_e2e` + 平铺基线）· ComputeUnitTLM（`apu_soc_full`）· MemoryTLM（仅 Python fixture）。
**LOCAL↔router 仅存在于 2 处装配期结构性测试**（EXP-1/2、MESH-6），且**只断言 `isBound()`、不跑流量** ⇒ 不触发 credit 记账。另有 4 份 config 完全不用 LOCAL。

⇒ 「`LOCAL` = 终端端口」这一前提在**所有有流量的拓扑**上成立。

### 第 4 条 —— ADR-X.16 的 D4 锚点需在实施中按订正口径落地

metis 复核发现：ADR 原文以 `waiting_for_credit` 状态位为计数锚点，但 credit 归零后 **VA 阶段先 `!has_credit` 即 `release_vc` 并跳过**（`:226-229`）、SA 阶段直接不选 winner（`:266`）⇒ flit 停在 `input_buffer_`，`waiting_for_credit` **保持 false** ⇒ 按 ADR 原文实现，计数器**恒为 0**。

⇒ 本 change 按**订正后锚点**（VA / SA 的拒绝条件）实施；ADR 文本不改（不可变原则），由 **architect** 追加 Status Update 澄清。

---

## What Changes

1. **`LOCAL` 端口不参与 downstream credit 流控**（ADR D1/D2）：`has_credit(LOCAL, vc)` 恒真；`consume_credit(LOCAL, vc)` 不扣减。
   - ⚠️ **实施约束**：短路必须在 `out_port` / `vc` **边界检查之后** ⇒ 非法 `vc` 不得因短路被静默放行（原 `:448-450` 的边界检查不得被绕过）。
2. **mesh 侧 credit 死锁可观测**（ADR D4 + 锚点订正）：`RouterTLM` 的 `stat_group_` 增标量 `credit_stall_cycles` —— 口径 = 「**存在待发 flit 且 `has_credit(out_port,out_vc)==false`**」的**周期累计数**（VA / SA 拒绝条件）；stall 达 16 cycles 打印一次诊断日志。⛔ `credit_timeout_` 默认值**保持 0**。
3. **`RouterPort::LOCAL` 注释显式声明终端语义**（ADR D5）+ **`downstream_credits_[LOCAL][*]` 死状态显式处置**（注释说明 或 跳过记账循环，二选一）。
4. **spec**：新建 capability `router-local-port-flow-control`（两条 Requirement：终端语义 / stall 可观测）+ 修订 `cep-cohif-network-adapter` 的「已知限制」句（已由新 capability 承接）。

### ⛔ 明确不做（ADR 选项 B/C/D/E 否决 + 范围外项）

| 项 | 理由 |
|---|---|
| per-port `credit_aware` 配置 | 零生产消费者 ⇒ 为无需求场景付配置契约变更成本（ADR D5 保留为演进 seam） |
| 开启 `credit_timeout_` 默认值 | 重置 = 虚增下游额度 ⇒ mesh 超发；且掩盖真实死锁 |
| 终端消费者实现 credit 归还 | 缺通往 router credit-receive 端口的通道 + 缺 credit 生成逻辑 |
| 修 `buffer_size` param 不生效 | 独立缺陷（`get_param_rules` 声明 vs `BUFFER_DEPTH` constexpr 8） |
| 修 `NICTLM` 的 credit flit 过滤 | 同族独立缺陷（`src/tlm/nic_tlm.cc` 无 `CATEGORY_CREDIT` 检查） |
| 改 `RouterTLM` 的 `vc_count` / `buffer_size` param 语义 | 范围外 |

---

## Acceptance

判据对应 `ADR-X.16` 的 Verification（V-1~V-6），并按 metis 审查补齐可执行性。**测试装置与观测面**见 `design.md` D-2（裸 `RouterTLM` 自环注入 + 全局 1 flit/cycle + `resp_out[4]` 读后 `clear_valid()` 计数）。

| # | 判据 | 可验收点（机器可判） | ADR |
|:--:|---|---|:--:|
| **AC-01** | LOCAL 单 VC 40 flit 全到 | 裸 `RouterTLM` 自环注入 40 个 flit（全局 1 flit/cycle，≥120 cycles）⇒ 「读到的 flit 数」== 40（无 8 平台期） | V-1 |
| **AC-02** | 4 VC 80 flit 全到 | 4 个不同 packet 同时在飞、`in_vc` 0..3（手写 `vc_id` 直接注入；`out_vc` 由 `allocate_vc` 决定）⇒ drain 计数 == 80 | V-2 |
| **AC-03** | stall 可观测 + 反向判据 | 裸 `RouterTLM`（`adapter_=nullptr` ⇒ credit 永不归还）向 **mesh 向**注入 8+ flit ⇒ `credit_stall_cycles` ≥16 cycles 后递增；挂上 `BidirectionalPortAdapter` 后**停止增长**；`credit_timeout_` 默认值仍为 0 | V-3 |
| **AC-04a** | 具名回归集全绿 | `test_cep_mesh_e2e.cc`（MESH-3a/3b/4/5/7/8/9）· `test_noc_mesh_binding_semantics.cc` · `test_e2e_simulation.cc` 的 nic 用例 · `[cep]`/`[e2e]`/`[pcie]` 全量 | V-4 |
| **AC-04b** | 数值变化须逐条重推导 | 任何 `flits`/`packets`/`hops`/`recv` 变化 ⇒ `design.md` 列「旧值 → 新值 → 为何是 D1 的必然结果」；⛔ 禁止就地改 `REQUIRE` 常量 | V-4 |
| **AC-05** | 死状态显式处置 | `downstream_credits_[LOCAL][*]` 的处置已落地且可 grep 验证 | ADR Consequences |
| **AC-06** | 真源同步 | `cep-cohif-network-adapter` 的「已知限制」句改为「已由 `router-local-port-flow-control` 承接」；新 spec 显式声明 `port-type-system` 的 `LOCAL` `BI_DIRECTIONAL` role **不变** | — |
| **AC-07** | LOCAL 侧不误报 | AC-01 / AC-02 场景下 `credit_stall_cycles` 保持 0 | — |

**附加检查（guard，不占验收编号）**：G-1 `LOCAL↔router` 结构性测试（EXP-1/2、MESH-6）仍绿；G-2 `RouterPort::LOCAL` 注释已声明终端语义。

---

## Capabilities

| # | 能力 | capability spec | 新增 `INV-CEP-*` |
|:--:|---|:--:|:--:|
| C-1 | `LOCAL` 端口的终端流控语义 | 新建 `router-local-port-flow-control`（Requirement 1） | ❌ 0 |
| C-2 | mesh 侧 credit 死锁可观测 | 同上（Requirement 2） | ❌ 0 |

---

## Out of Scope

- ⛔ `module_factory.cc` / `CohIfNICTLM` / `NICTLM` / JSON schema（**零配置面变更**）。
- ⛔ `buffer_size` param 生效化、`NICTLM` credit 过滤、`vc_count` param 语义 —— 三者均为独立缺陷，另行立项。
- ⛔ per-port `credit_aware` 机制 —— ADR D5 的演进 seam，须另立决策。
