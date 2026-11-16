# ADR-X.16: Router LOCAL 端口不参与 downstream credit 流控

## Status

Accepted — 2026-11-16（oracle APPROVE + critic APPROVE_WITH_CONDITIONS，0 BLOCKER；审稿报告见 `.rddf/reviews/ADR-X.16.md`）

## Metadata

- **Author**: architect（main session）
- **Parent**: 无（新增决策）；与 `ADR-X.15` 同属「LOCAL 口语义」的相邻决策（X.15 管 exposed 端口的数据通路径，本 ADR 管 LOCAL 口的流控语义）
- **关联 change**: `backlog: cpptlm-router-local-port-credit-exhaustion`（improvement 已写，**未立项**；立项与 AC 编号由 planner 阶段补）
- **AC 映射**: 本文件 Verification 章给出前瞻判据（V-1~V-6），立项后回填 change AC 编号
- **审稿报告**: `.rddf/reviews/ADR-X.16.md`（oracle 1 轮 APPROVE + critic 1 轮 APPROVE_WITH_CONDITIONS · 0 BLOCKER · SHOULD_FIX/NITS 已并入本文件）
- **上游同源**: change `cpptlm-router-credit-flit-leak-to-consumer`（已归档 · 修 credit **泄漏给消费者**；本 ADR 修 credit **记账饿死终端**—— 同一根因「LOCAL 下游不是 credit-aware 实体」的两面）

---

## Context（背景与问题）

### 事实基础（全部读码/实测，非推断）

| # | 事实 | 位置 |
|:--:|---|---|
| F-1 | `downstream_credits_` 是 `[NUM_PORTS][NUM_VCS]` 计数，初值一律 `BUFFER_DEPTH = 8` | `include/tlm/router_tlm.hh:277`；`src/tlm/router_tlm.cc:57` |
| F-2 | 转发时 `consume_credit(out_port, out_vc)` **不区分端口**；credit 不足则 flit 停在 `waiting_for_credit` | `src/tlm/router_tlm.cc:300-307`、`:352-367` |
| F-3 | credit 归还只发生在 **router↔router** 之间（`return_credit_to_upstream(in_port)` → `reverse_port(in_port)` → `BidirectionalPortAdapter::send_credit`）；`reverse_port(LOCAL) == LOCAL` | `src/tlm/router_tlm.cc:323/359/383`；`include/tlm/router_tlm.hh:223-232` |
| F-4 | 终端消费者的网络侧入口是**单向下行** `InputStreamAdapter`（无反向发送通道）⇒ 无法归还 credit | `include/tlm/cep/cohif_nic.hh:118`；`include/framework/stream_adapter.hh:30-95` |
| F-5 | `credit_timeout_` 默认 **0** ⇒ `credit_safety_reset()` 首行 `return`（无死锁检测/恢复） | `include/tlm/router_tlm.hh:298`；`src/tlm/router_tlm.cc:503-506` |
| F-6 | **生产拓扑中 LOCAL 只挂终端**：NICTLM（`mesh_2x2_tlm` / `mesh_4x4_tlm` / `mesh_4x4` / `nic_router_nic` / `test_e2e_simulation` / `test_nic_side_rules` / `test_noc_mesh_binding_semantics`）、CohIfNICTLM（`cep_mesh_e2e` + 其平铺基线）、ComputeUnitTLM（`apu_soc_full` **JSON 声明层**仅入向 —— 但 Step 7b 会为每条 connection 自动补反向 PortPair，故绑定层有反向；且该 config 因 ComputeUnitTLM 未注册当前不可装载）、MemoryTLM（仅 Python fixture） | explore 全仓检索（2026-11-16） |
| F-7 | **LOCAL↔router 仅存在于装配期结构性测试**，且只断言 `isBound()`、不跑流量 ⇒ 不触发 credit 记账 | `test/test_simmodule_exposed_port_wiring.cc:101-157`（EXP-1/2）；`test/test_cep_mesh_e2e.cc:408-434`（MESH-6） |
| F-8 | LOCAL **悬空很常见**：4 份 config 完全不用 LOCAL（`noc_mesh` / `hierarchical_2x2_tlm` / `vc_priorities_mesh` / `nic_tlm_port_groups`），2 份只接入方向 | 同上 explore 检索 |
| F-9 | 实测后果：每 `(LOCAL, vc)` 满 8 个 flit 后投递静默停滞；`NUM_VCS = 4` ⇒ 每 LOCAL 口上限 32 flit。既有 e2e 实测 `nic_*_recv` 在 8 附近被截断（`cpptlm-router-credit-flit-leak-to-consumer` 的判据改造过程记录了该截断） | 台账 Entry #455 / #458 |

### 问题

`RouterTLM` 用**同一套** credit 记账对待 mesh 链路与 LOCAL 端口。前者正确（下游是 router，会归还 credit）；后者**结构性失效**：LOCAL 的典型下游是终端（无归还通道，F-4），而这个失效是**静默的**（F-5：无超时、无计数、无日志）。

⇒ 任何把流量规模拉到 `BUFFER_DEPTH × NUM_VCS = 32` 个 flit 以上的 LOCAL 端口，都会**无声地停止投递**，测试表现为「接收数到 8 就上不去了」，且无任何错误信号。

### 不在本 ADR 范围（相邻但独立）

- `get_param_rules` 声明了 `buffer_size`（默认 16、范围 1–64），但 `BUFFER_DEPTH` 是 `constexpr 8` ⇒ **该参数不生效**（`include/tlm/router_tlm.hh:134` vs `get_param_rules`）。⇒ 独立缺陷，另行登记。
- credit flit **泄漏给消费者**（被当数据消费）—— 已由 change `cpptlm-router-credit-flit-leak-to-consumer` 修复并归档。

## Decision（决策）

**D1 — LOCAL 端口（`RouterPort::LOCAL = 4`）不参与 downstream credit 流控。**
`has_credit(LOCAL, vc)` 恒返回真；`consume_credit(LOCAL, vc)` 不扣减。实现上等价于「LOCAL 的 credit 预算为无穷」。

**D2 — 理由（语义归属）**：credit 是 **router↔router 链路的 VC buffer 反压**机制（每跳一个接收方，归还闭环成立）。LOCAL 是**终端接入点**，其下游反压由 ChStream 的 `valid/ready` 承担。把链路级流控套到终端端口，既无归还方，也无语义依据。业界同构实现（Garnet / gem5 的 local port）同样不参与 credit 流控。

**D3 — 现行拓扑零破坏**：生产拓扑 LOCAL 只挂终端（F-6）；唯一的 LOCAL↔router 是两处结构性测试且不跑流量（F-7）；LOCAL 悬空普遍存在（F-8）。⇒ 该决策不改变任何既有拓扑的**有流量**行为。

**D4 — mesh 侧 credit 死锁必须可观测**：新增 `stats_credit_stalls_`，语义**分两件陈述**（critic 指出原句自相矛盾）：
- **计数器** = **stall 周期累计数** —— 该 `(port, vc)` 处于 `waiting_for_credit` 的**每个周期 +1**（不是「阈值穿越次数」，也不是「≥16 才 +1」）；
- **日志** `DPRINTF` = 每 `(port, vc)` 每 **16 cycles** 打印一次（避免刷屏）。
- **阈值绝对值以 V-3 的 16 cycles 为准**；`2 × BUFFER_DEPTH` 只是该值的**取值依据**（若未来 `buffer_size` param 生效，阈值不随公式漂移）。
⚠️ **定位澄清（oracle 复核后修正）**：该计数器证明的是 **mesh 端口的 credit 死锁可见**，**不是**「LOCAL 误用可见」—— D1 之后 `has_credit(LOCAL,·)` 恒真 ⇒ LOCAL 永不进入 `waiting_for_credit` ⇒ 该计数器对「router 挂 LOCAL」误用**不会**计数。该误用的真实失败模式是下游 router 的 `input_buffer` 满后**静默丢 flit**（`src/tlm/router_tlm.cc:152-154`：先 `consume()` 再静默丢弃），其可见性只能靠 D5 的演进决策与端口注释。
⛔ **不改变 `credit_timeout_` 的默认值（0）**：周期性重置 credit 会**虚增**下游缓冲可用量（超发），对 router↔router 链路是危险行为，且会掩盖真实死锁。

**D5 — 演进 seam（未来 LOCAL 需要流控时）**：若出现「LOCAL 挂 router」的真实需求，须**另立决策**引入 per-port `credit_aware` 声明（默认值、配置面、装配期校验），不在本 ADR 内。本 ADR 要求把 `RouterPort::LOCAL` 的注释从「本地（连接 NICTLM）」改为**显式声明终端语义**（现注释未表达该约束）。

## 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **A. LOCAL 短路 credit（选中）** | ✅ 采用 | 与 LOCAL 的终端语义一致；零配置面变更（不动 JSON schema）；不改变任何有流量的既有拓扑行为（F-6/F-7/F-8） |
| **B. per-port `credit_aware` 声明（JSON 配置）** | ❌ 不采用 | 需要新增配置字段 + 装配期校验 + 蓝图改动 ⇒ **配置契约变更**；而当前**没有任何生产消费者**需要 `credit_aware=true` 的 LOCAL 端口（唯一实例是两处不跑流量的结构性测试）⇒ 为一个无需求的场景付契约成本。留作 D5 的演进 seam |
| **C. 开启 `credit_timeout_` 默认值（周期性重置 credit）** | ❌ 不采用 | 重置 = 假定下游已腾出缓冲 ⇒ **虚增**可用额度 ⇒ router↔router 链路可能超发（buffer 溢出）；且会**掩盖**真实死锁而非暴露它。与「缺陷应可见」相反 |
| **D. 要求终端消费者实现 credit 归还** | ❌ 不采用 | 终端消费者的网络侧入口是**单向下行** adapter（F-4），结构上无反向通道；要支持需改 adapter 的端口面（跨模块契约 + 所有消费者受影响），成本远高于收益 |
| **E. 保持现状（仅登记为已知限制）** | ❌ 不采用 | 静默停滞是**不可观测的正确性缺陷**（F-5/F-9）：测试会把「8 就上不去」当正常。已知限制不能替代修复，但 D4 保留「可观测」这一半 |

## Consequences

### 正面

- LOCAL 端口的投递不再在 32 个 flit 后静默停滞 ⇒ 长 run 的本地投递语义正确。
- LOCAL 的语义从「隐式假设下游会归还 credit」变成**显式**的终端端口 ⇒ 与 mesh 链路的流控语义分离，减少后续误用。
- 零配置面变更；`RouterTLM` 的 JSON schema 与所有既有拓扑文件**零 diff**。

### 负面

- 若未来有人在 LOCAL 上挂 router，该链路**失去流控**（当前无生产用法，且 D5 明示须另立决策）。⇒ 用「端口注释显式声明 + D4 的 stall 计数」缓解。
- `downstream_credits_[LOCAL][*]` 成为**不维护**的死状态（应注释说明，或从记账循环中跳过以省开销）。
- **回滚路径**：本决策**零配置面变更**（不动 JSON schema），改动局限 `has_credit` / `consume_credit` 的 LOCAL 分支 + 新增 stall 计数 ⇒ `git revert` 即完整回滚，无数据 / 契约迁移成本。
- **沉默边界迁移（oracle 复核登记）**：D1 之前，8-flit 停滞客观上把 router→终端 节流在终端最低消费速率；D1 之后 router 以最多 1 flit/cycle 持续投递，**若未来某终端的消费速率 < 1 flit/cycle，丢包点会迁移到终端 ingress** —— `DualPortStreamAdapter::process_net_response_input` 在模块未消费时**静默丢包且不 release packet**（`include/framework/dual_port_stream_adapter.hh:237-243`），`CohIfNICTLM::handle_net_ingress` 无条件覆写 `pe_resp_out_`。现行 LOCAL 终端均 1/cycle 消费（`src/tlm/nic_tlm.cc:80-95`、`include/tlm/cep/cohif_nic.hh:273-292`）⇒ **无当前回归**（V-4 兜底），但属「修了一个静默点、留下另一个静默点」，须显式登记。

### 风险

| # | 风险 | 缓解 |
|:--:|---|---|
| R-1 | 存在未被 explore 发现的 LOCAL↔router 生产拓扑 ⇒ 该链路退化 | ⚠️ **唯一防线 = 端口注释显式声明终端语义（V-6）+ D5 另立决策**。⛔ `stats_credit_stalls_` **不能**兜底该误用（D1 后 LOCAL 永不进入 `waiting_for_credit`）；其真实失败模式是下游 router 的 `input_buffer` 满后静默丢 flit |
| R-2 | 去掉 LOCAL 的 credit 后，**上游**（router 的输入侧）反压行为改变 ⇒ 上游 buffer 占用上升 | V-4 要求既有 `[cep]`/`[e2e]`/`[pcie]` 全绿；router 四元组逐项不变 |
| R-3 | D4 的 stall 计数阈值取值不当 ⇒ 噪音或漏报 | 阈值**已定死** = `2 × BUFFER_DEPTH` = **16 cycles**（D4），纳入 V-3 判据 |

## Verification（可勾选验收判据）

> 前瞻判据（实施后逐条勾选；对应 improvement AC-01~AC-05）

- [ ] **V-1** 单 LOCAL 下游、单 VC 注入 **40** 个 flit ⇒ 下游接收数 **== 40**（不出现 8 的平台期）
- [ ] **V-2** 4 个 VC 各注入 20 个 flit ⇒ 全部到达（累计 80），无 flit 滞留 `waiting_for_credit`
- [ ] **V-3** 停滞可观测（**钉 mesh out_port，非 LOCAL**）：构造「mesh 下游不归还 credit」的场景 ⇒ `stats_credit_stalls_` 在累计 ≥16 cycles 后递增（且 `credit_timeout_` 默认值**仍为 0**，未被改动）。⚠️ 该用例同时补上「RouterTLM 的 mesh 反压无正向用例」这一验证空洞（oracle 指出：全仓 `test/` 目前无 `has_credit` / `waiting_for_credit` 的正向断言）
- [ ] **V-4** 回归：`[cep]` / `[e2e]` / `[pcie]` 全绿；mesh 拓扑的 router `flits` / `packets` / `hops` **逐项不变**（本决策只改流控记账，不改转发路径）
- [ ] **V-5** LOCAL↔router 的结构性测试（`EXP-1/2`、`MESH-6`）仍绿 ⇒ `isBound()` 语义未受影响
- [ ] **V-6** 端口注释已显式声明终端语义（`RouterPort::LOCAL` 处）

## 关联

- improvement（backlog，未立项）: `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`（AC-01~AC-05 与本 ADR 的 V-1~V-5 对应）
- 同源另一面（已归档）: change `cpptlm-router-credit-flit-leak-to-consumer` + `ADR-X.15` 的 Status Update
- 事实来源: 台账 Entry **#452 / #453**（根因订正）· **#454 / #455 / #456 / #457 / #458**（同源另一面的立项 → 判据裁决 → 实施 → 归档 → AC-07 收敛）
- 相邻独立缺陷（不在本 ADR 范围，须另行登记）:
  1. `buffer_size` param 不生效 —— `get_param_rules` 声明了该参数（默认 16，范围 1–64），但 `BUFFER_DEPTH` 是 `constexpr 8`（`include/tlm/router_tlm.hh:134`）。
  2. **`NICTLM` 无 credit flit 过滤**（oracle 审查时发现 · 同族缺陷）—— `src/tlm/nic_tlm.cc` 全文无 `CATEGORY_CREDIT` 检查 ⇒ 在 `mesh_2x2_tlm` / `mesh_4x4_tlm` 类拓扑里，router 对 LOCAL ingress 的正常 credit 回流（`reverse_port(4)==4`）会进入 `NICTLM::reassemble`，污染 `flits_received` 计数并产出垃圾 `pe_resp_out`。`CohIfNICTLM` 侧已由 change `cpptlm-router-credit-flit-leak-to-consumer` 过滤；`NICTLM` 侧**未修**。
