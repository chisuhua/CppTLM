# ADR-X.15: SimModule 暴露端口的真实数据通路径接入

## Status

Accepted — 2026-11-16（决策与实施记录见 `## Status Update`）

## Metadata

- **Author**: architect（main session）
- **Parent**: 无（新增决策）
- **关联 change**: 本 ADR 为 `cpptlm-cep-mesh-interconnect-submodule` 的**前置决策**；实施另立 builder change（未立项 → `backlog: cpptlm-simmodule-exposed-port-wiring`）
- **AC 映射**: 待实施 change 立项后回填；本文件 Verification 章给出前瞻判据

---

## Context（背景与问题）

### 事实基础（全部实测/读码，非推断）

| # | 事实 | 位置 |
|:--:|---|---|
| F-1 | `SimModule` 声明 `outputs` / `inputs` 暴露端口，`findInternalPath(label)` 把外部标签映射到内部路径（如 `r00.req_in[4]`） | `include/core/sim_module.hh:149-161`, `207-235` |
| F-2 | `getInternalInputPort` / `getInternalOutputPort` 经**内部工厂**取到子模块的**真实 Port 对象** | `include/core/sim_module.hh:132-146` |
| F-3 | Step 7 把每个 ChStream 模块的 `req_out[i]/req_in[i]/resp_out[i]/resp_in[i]` **mirror 进该模块的 PortManager**，注释明写目的 =「让 SimModule::getInternalOutputPort 能查到 ChStream 端口」 | `src/core/module_factory.cc:765-778` |
| F-4 | Step 7b（实际生效的连接步骤）只处理 `ch_adapters` 中的 ChStream 模块：`src_ch && dst_ch` 才建 PortPair | `src/core/module_factory.cc:1093-1185`（判据 `1150-1156`） |
| F-5 | Step 6（另一条连接步骤）的端点展名分支 `RegexMatcher::isRegexPattern(x) || Wildcard::match("*", x)` 恒为真（`*` → `^.*$`）⇒ else 分支是死代码；带索引端点（`nic_a.1`）被误判 regex → 展成空集 → 丢弃 | `src/core/module_factory.cc:595-707`；`include/utils/wildcard.hh:10-33` |
| F-6 | `ConnectionResolver::resolveConnections` 能把 exposed 标签解析成 `(owner=r00, port=req_in[4])`，但 Step 5.5 去**顶层** `object_instances` 找 `r00` → 找不到（r00 在 fabric 内部工厂）→ 端口从未创建 | `src/core/connection_resolver.cc:47-77,99-130`；`src/core/module_factory.cc:548-567` |

### 问题

一个容器型 `SimModule`（如 `CepMeshInterconnect`）持有内部子模块（router），这些子模块是 `ChStreamModuleBase`，在**容器内部工厂**里由 Step 7/7b 完成内部互连（可正常传 flit，实测 `r00/r01/r11 flits>0`）。但**外部**模块要连到容器内部的某个 router 端口，只能经暴露端口（`fabric.port0_in`）：

- Step 7b 要求两端都是顶层 `ch_adapters` 的 ChStream 模块 → 容器的暴露端口不在其中 → `continue`；
- Step 6 对带索引端点是死代码（F-5）→ 丢弃；
- Step 5.5 解析到内部 owner 后查不到（F-6）→ 端口从未创建。

⇒ **净效果**：`SimModule` 暴露端口**无法承载真实数据流**。实测证据：扁平拓扑 `[mesh]` 13 用例 / 158 断言全绿（走 Step 7b 直连），而把互联件抽进 `CepMeshInterconnect` 后 `nic_b_recv_dst_node != 3`（流量没到）。

这是「既有特性从未被真实数据流验证」——F-3 的 mirror 注释表明**设计意图是支持**，只是连接步骤从未调用 `getInternalInputPort/OutputPort`。

---

## Decision（决策）

**在一个新增的、隔离的连接阶段（Step 7c）中，让「至少一端是 SimModule 暴露端口的 connection」以单向 PortPair 的方式接入真实数据通路径。** 约束如下：

1. **解析暴露端口为真实 Port**：用 `findInternalPath` + `getInternalInputPort`/`getInternalOutputPort` 拿到内部子模块的真实 Port 对象（F-2/F-3 已备好）。

2. **方向由「内部端口的名字后缀」推导，而非由 outputs/inputs 标签或 portN_in/out 字面**（⚠️ oracle must-fix 1：标签会骗人）：
   - 反例：`CpuCluster` 暴露 `{"internal": "cpu0.req_out", "external": "cpu0_to_bus"}` —— 一个承载**请求**的 exposed **output**。若按「output=resp」推断会错接。
   - 规则（按 `findInternalPath` 解析出的内部端口名后缀，配对到对端对应角色）：
     | 内部端口后缀 | 内部端口 role（`getInternal*Port`） | 对端端口 | PortPair 顺序（Master, Slave） |
     |---|---|---|---|
     | `req_in[N]` | Slave（`getInternalInputPort`） | `req_out[idx]` | `(peer.req_out[idx], internal)` |
     | `resp_in[N]` | Slave（`getInternalInputPort`） | `resp_out[idx]` | `(peer.resp_out[idx], internal)` |
     | `req_out[N]` | Master（`getInternalOutputPort`） | `req_in[idx]` | `(internal, peer.req_in[idx])` |
     | `resp_out[N]` | Master（`getInternalOutputPort`） | `resp_in[idx]` | `(internal, peer.resp_in[idx])` |
   - exposed 连接**单向、无自动响应镜像**：一条 connection 表达一个方向；往返由作者显式声明两条 connection。对 mesh config 该规则退化为「`rNN.req_in[4]`↔req_out、`rNN.resp_out[4]`↔resp_in」，与本 ADR 原表述一致。

2b. **建 pair 前做 `isBound()` 硬校验**（oracle must-fix 2）：`PortPair` 构造无条件 `bind`、静默覆盖已绑定端（`include/core/simple_port.hh:46-49`）。实现须给 `SimplePort` 加 `bool isBound() const { return pair != nullptr; }`，Step 7c 对两侧分别校验 `isBound()==false`，否则**装配期报错**（⛔ 不得静默覆盖，否则 flit 被劫持且零报错）。

3. **对端必须是顶层 ChStream 索引端点**（`<module>.<idx>`），其端口从 `ch_req_out` / `ch_resp_in` / `ch_req_in` / `ch_resp_out` 取（与 Step 7b 同一来源）。

4. **纯 ChStream 连接（两端都非 exposed）行为完全不变** —— Step 7b 原样保留（含其自动 req+resp 镜像）。

5. **不修改 Step 6 的 wildcard 语义**（F-5 的恒真 bug 不在本决策范围；其现状对带索引端点恒丢弃，不会与 Step 7c 争抢，因为 Step 7c 只认 exposed 端点）。

6. **Step 7c 的失败语义 = 硬错误（不可复用 Step 7b 的静默 continue）**（oracle must-fix 3）。以下四类必须在装配期报错并终止，⛔ 不得静默丢连接：
   - ① 声明了 exposed 标签但 `findInternalPath` 返 `""`；
   - ② `getInternal{Input,Output}Port` 返 `nullptr` —— 含**嵌套容器**路径坑：`findInternalPath` 递归返回 `"child.req_in[4]"`，而 `parsePortSpec` 在**第一个点**切分（`sim_module.hh:237-243`），`getInternal*Port` 拿 `child` 去查该路径会失败 ⇒ **本决策仅支持单层 exposed**（内部路径直指同层子模块端口），多层暴露一律硬错误；
   - ③ 端口索引越界（如 5 端口 router 写 `req_in[5]`，mirror 从未建该名）；
   - ④ 内部端口后缀**不属于** `req_in`/`resp_in`/`req_out`/`resp_out` 四类（如既有 `dgpu_board_v1.json` 的 `sdma.host_out`）⇒ 装配期硬错误，并提示「Step 7c 仅服务 ChStream mirror 端口」。

7. **兼容性检查豁免（显式声明，critic SHOULD_FIX-1）**：Step 7c **不**套用 ADR-X.9 的 L1/L2/L3 兼容性检查（`check_port_compatibility`，Step 7b 于 `module_factory.cc:1159` 调用）。理由：exposed 端点无 `port_specs` 声明面可查。代价：7c 路径不做层次兼容性校验，改由「PortPair 两端 Master/Slave 角色匹配（方向表）+ `isBound()` 硬校验 + 失败硬错误」兜底。此为实现方与 reviewer 可据以判断「有意豁免 vs 漏写」的显式声明。

8. **latency 透传**：建 pair 后按 Step 7b 同规则 `setDelay(latency)`（作用于 initiator 侧）；`cep_mesh_e2e.json` 的 exposed 连接带 `latency:1`，不得漏设。

9. **去重**：Step 6/7b/7c 共用 `processed_connections`；exposed 端点在 Step 7b 的 `src_ch && dst_ch` 判据处自然不通过 ⇒ 不会双连。⚠️ 键空间不对称须登记（oracle 中等项 ⑤）：Step 6 存**展开后**名、7b/7c 存**原始**串；今天无害（exposed 端点对顶层名展开恒空），但若日后修 Step 6 wildcard，须复核 7c 的输入集。

---

## 选项对比

| 方案 | 结论 | 理由 |
|---|---|---|
| **A. 新增 Step 7c 隔离处理 exposed 连接（选中）** | ✅ 采用 | 改动局限在连接阶段末尾；纯 ChStream 路径零触碰；复用既有 mirror + `getInternalInputPort/OutputPort`（F-2/F-3）；方向语义显式、可测 |
| B. 扩展 Step 7b，令 `src_ch/dst_ch` 对 exposed 端点也判真 | ❌ 不选 | 会把单向 exposed 混进 7b 的「自动 req+resp 镜像」，方向语义错乱；且侵入 7b 主循环，回归面大 |
| C. 让 `SimModule` 自身把暴露端口注册进顶层 `ch_req_out/ch_req_in` 映射 | ❌ 不选 | 需容器实现 ChStream 端口向量接口；与「容器不承载业务/端口语义」冲突；侵入面最大 |
| D. 要求容器在 `incorporate_parent` 里 self-wire 到外部对端 | ❌ 不选 | 容器**拿不到**外部对端的顶层 Port 对象（无稳定 API），不可行 |
| E. 放弃暴露端口数据连接，层次化仅停留在 config/命名空间 | ❌ 不选 | 不满足「互联层承载真实流量」的目标（用户已否决扁平化） |

---

## Consequences

**正面**
- 容器型 SimModule（CEPMeshInterconnect / 未来任意 fabric 容器）获得真实数据通路径能力；互联层与业务层可真正分层。
- 不改纯 ChStream 路径 ⇒ 既有 config 零回归风险。
- 复用既有 mirror 机制（F-3），不新增端口基础设施。

**负面 / 代价**
- 新增一套「方向由**内部端口后缀**决定」的连接语义（作者须理解 `req_in`↔req_out、`resp_out`↔resp_in 的配对）—— 增加配置心智负担。注意 exposed **output 也可能承载请求类流量**（如 `CpuCluster::cpu0_to_bus` 内部是 `req_out`）⇒ 不可按 label 字面推断方向。
- Step 7c 与 Step 7b 共用 `ch_req_out/ch_resp_in` 来源，须保证不双重连接（`isBound()` 硬校验 + 去重键）。
- exposed 端点的「往返」不再是单条 connection 自动镜像（与 ChStream 语义不对称）—— 需在用户文档写明。
- **仅支持单层 exposed**（内部路径直指同层子模块端口）；嵌套容器暴露一律硬错误（must-fix 3 ②）。

**风险与缓解**
- 风险：exposed-exposed（容器内部 reroute：`fabric.port0_out -> fabric.port1_in`）—— Step 7c 是否支持？**本决策范围限定为「一端 exposed + 一端顶层 ChStream」**；exposed-exposed 若需要，另立决策（当前 `cep_mesh_e2e` 不依赖）。
- 风险：`getInternalInputPort` 对多端口子模块的端口名（`req_in[4]`）解析 —— 依赖 F-2/F-3，已由 mirror 覆盖；实施须先做最小探针（见 Verification）。
- 风险：Step 6 的恒真 wildcard bug 未来被修时可能影响 exposed 端点判定 —— 缓解：Step 7c 判定走 `SimModule::isExposedPort`/`findInternalPath`，不依赖 Step 6 的展名结果。

---

## Verification（可勾选验收判据）

> 前瞻判据，供实施 change 的 verifier 对照；**不是**已完成实测日志。

- [ ] **V-1 最小探针**：单容器（1 router + 1 exposed out + 1 exposed in）+ 1 外部 ChStream 模块，`portN_in`/`portN_out` 连接后 flit 双向可通行（`flits_received>0`）。**补一条嵌套容器负例**（内部路径含 ≥2 个点）预期装配期硬错误。
- [ ] **V-2 方向按后缀正确**：内部 `req_in[N]`↔对端 `req_out[idx]`、`resp_in[N]`↔对端 `resp_out[idx]`、`req_out[N]`↔对端 `req_in[idx]`、`resp_out[N]`↔对端 `resp_in[idx]`。`CpuCluster` 的 `cpu0_to_bus`（内部 `req_out`）连对端 `req_in` 为正控，错接须始终零 flit。
- [ ] **V-3 无静默覆盖**：建 pair 前两侧 `isBound()==false` 校验通过；构造一个"同端二次连接"负例，预期装配期报错（而非静默覆盖）。
- [ ] **V-4 纯 ChStream 零回归**：既有 `[cep]` / `[e2e]` / `[mesh]`（扁平基线）全绿，扁平 `cep_mesh_e2e`（改造前）逐断言一致。
- [ ] **V-5 层次化等价**：`CepMeshInterconnect` 装配后 `nic_b_recv_dst_node==3`、`last_recv_hops>=1`、`r*.flits>0`，与扁平观测一致。
- [ ] **V-6 Step 6 未改语义**：`git diff` 中 Step 6 wildcard 分支无改动。
- [ ] **V-7 显式失败**：四类失败（`findInternalPath` 空 / `getInternal*Port` 空含嵌套 / 索引越界 / 后缀非四类）在装配期报错，不静默丢连接。
- [ ] **V-8（可选 lint）**：装配期对「同一内部 router 端口只声明 in 未声明 out（或反之）」发 WARN —— 漏声明时运行时表现为 `send()` 返 false 静默丢响应。

---

## 关联

- 触发 change：`openspec/changes/cpptlm-cep-mesh-interconnect-submodule/`（其 proposal/design.md 的 D-5「不改 module_factory」前提被本 ADR 推翻，须回填指针）
- 实施 change：`openspec/changes/cpptlm-simmodule-exposed-port-wiring/`（已实施 · Step 7c + `SimplePort::isBound()`）
- **待办（交实施阶段，architect 不越界改 openspec 产物）**：`openspec/changes/cpptlm-cep-mesh-interconnect-submodule/design.md` 的 D-3 表第 4-7 行 in/out 标注与落地 `configs/cep_mesh_e2e.json` **相反**（config 正确，表 stale），须在实施前订正；同时把该 change 的 D-5 边界改写为「依赖本 ADR」。

---

## Status Update

**2026-11-16 — 实施完成，Status: Proposed → Accepted。**

实现落 `openspec/changes/cpptlm-simmodule-exposed-port-wiring/`：`src/core/module_factory.cc` 新增 Step 7c（隔离增强，Step 6/7b 零改动）+ `include/core/simple_port.hh` 新增 `isBound()`。判据 EXP-1~EXP-10（`test/test_simmodule_exposed_port_wiring.cc`）全绿；`[cep]`/`[e2e]`/`[mesh]`/`[simmodule]`/`[pcie]`/`[realip]` 全绿。

实施期发现并落实的两处修正（均属本 ADR 范围的**精度**订正，不改变决策框架）：

1. **后缀识别取内部路径的「最后一段」**（非首段）：否则嵌套容器路径 `inner.r0.req_out[4]` 会被误判为「后缀非四类」（决策 6 ④），而非「内部端口不可解析」（决策 6 ②）。已实现并在 EXP-6 验证。
2. **决策 6 ① 的可达性限定**：只在「该 `SimModule` **声明过** exposed 标签」时把未声明标签判为硬错误；**未声明** exposed 的 SimModule 的 `<实例>.<端口>` 引用（如 `pcie_ep.axi_slave_in`、`cpu0_l2cache.l1_0`）维持既有普通端口语义（Phase 8 E2E + `library_cpu_demo.json` 装配验证零变化）。

**供后续 change 参考的两个非本 ADR 范围的发现**（已登记，不在本 ADR 决策内）：

- `SimModule` 的**内联 `modules`** 能力要求模块条目**同时含 `connections` 数组**（`ModuleFactory::validateConfig` 的顶层必需字段）——否则内部工厂校验失败且该失败被 `SimModule::simulate_instantiate` **静默忽略**（既有嵌套 config 均遵循此约定）。容器作者须注意。
- 层次化把容器子模块的 tick 收成**连续一组**（`SimModule::tick` 转发），与平铺时各模块按哈希序交织不同 ⇒ **NIC 侧顺序敏感计数器**（`last_recv_*`、回声命中数）会变；**mesh 级观测（router 转发/hop 计数）逐项不变**。等价性判据须用 mesh 级观测，不得用 `last_recv_*`。

**2026-11-16（追加）— 上条「顺序敏感 / 回声」的机理已定位；V-5 判据口径随之收敛。**

上面最后一条只记录了**现象**（NIC 侧顺序敏感计数器会变），其机理在实施后排查中定位如下。本条是**记录性补充**，不改变本 ADR 的任何决策条目，也不改动 `## Verification` 章的原文（该章不可变，见下「对 V-5 的影响」）。

1. **根因 = in-band credit 泄漏给非 router 消费者。** `BundleDirectionTraits<NoCFlitBundle>::fill_credit`（`include/framework/bidirectional_port_adapter.hh:132-136`）把 credit 编成一个 flit（`flit_category = CATEGORY_CREDIT`，`dst_node` / `hops` 保持默认 0），经与数据**同一通道** `resp_out_port_` 发出（同文件 `:319` 调用 `fill_credit`、`:323` 置 `pkt->type = PKT_CREDIT_RETURN`）。`RouterTLM` 对每个转发成功的入向 flit 回 credit（`src/tlm/router_tlm.cc:323` / `:359` / `:383`），而 `reverse_port(LOCAL) == LOCAL`（`include/tlm/router_tlm.hh:229`）⇒ 经 LOCAL 口进入的 flit，其 credit 沿 `resp_out[LOCAL]` **回流到挂在 LOCAL 上的下游**。`CohIfNICTLM::handle_net_ingress`（`include/tlm/cep/cohif_nic.hh:260`）不辨 credit ⇒ ① 计入 `flits_received`；② 覆写 `last_recv_dst_node` / `last_recv_hops`（被写成 0/0）；③ 向业务侧投影出一条**全零 Completion**。**平铺基线同样受影响**（命中时机不同），故这不是层次化引入的。

2. **该缺陷不改变本 ADR 的决策范围。** credit 回流与 LOCAL 口语义属既有链路层 / 路由器行为；本 ADR 只负责让 exposed 端点承载真实数据通路。修复追踪见 `.rddf/improvements/cpptlm-router-credit-flit-leak-to-consumer.md`（未立项 backlog）与决策台账 Entry #453。

3. **对 V-5 的影响。** V-5 原文的 `nic_b_recv_dst_node==3` / `last_recv_hops>=1` **不可作为等价性判据** —— 它们受上述 credit 污染，且取值取决于「最后一个到达的是数据还是 credit」，随周内 tick 交织变化（tick 交织又因装配方式而变）。等价性判据以 **mesh 级观测**为准：router `flits` / `packets` / `hops` 逐项相等 + NIC 收到的 flit 数相等，已在实施 change 的测试中落地。**V-5 原文不改**（Verification 章不可变），本段即为其实施期口径的权威说明。

4. **登记一处已被证伪的假设**（记录于此以免复发）：排查中一度判定「`RouterTLM::routing_table_` 以 `transaction_id` 单独为键、不含方向 ⇒ 请求与其响应共用 tx id 时，响应命中残留的 `out_port = LOCAL` 条目而被误投」。该判断**不成立** —— `CohIfNICTLM::pack_flit` 对每个 flit 写 `FLIT_HEAD_TAIL`，而 `is_head()` 对 `FLIT_HEAD_TAIL` 返回 true ⇒ `stage_route_computation` 恒走 HEAD 分支（`computeRoute`），BODY/TAIL 的 `routing_table_` 查表分支**不可达** ⇒ 该碰撞在本场景不可能发生。

**2026-11-17（追加）— 上条定位的缺陷已修复并归档；等价判据随之改为「开环」；同源另一面已分离立项。**

1. **消费者侧 credit 过滤已落地**（change `cpptlm-router-credit-flit-leak-to-consumer`，已归档）：`CohIfNICTLM` 在网络 ingress 按 `flit_category == CATEGORY_CREDIT` **丢弃** credit flit 并计入 `credits_dropped()`；**不**计入 `flits_received`、**不**写 `last_recv_dst_node` / `last_recv_hops`、**不**向 PE 侧投影 ⇒ 上条描述的「计数虚增 / 观测被覆写 / 全零伪 Completion」三面后果均已消除。契约面同步：`openspec/specs/cep-cohif-network-adapter/spec.md` 增「网络 ingress SHALL 丢弃 credit flit」Scenario。

2. **等价判据随之改为「开环」**（决策台账 Entry #455 裁决）。修复后 credit 不再回流，闭环拓扑下层次化与平铺的 router 计数不再逐项相等 —— 差异来自**闭环反馈回路**（伪 Completion 不再回喂业务侧 ⇒ 平铺侧注入流变化 ⇒ 转发数差 1），而非单纯的 tick 交织噪声 ⇒ 等价性判据改为两条：**开环**（断开跨 mesh 回程后，层次化与平铺的 router `flits`/`packets`/`hops` 逐项严格相等）+ **闭环不变式**（各自 `flits > 0`、`hops > flits`、无静默丢弃、内部 router 数 == 4）。⚠️ **V-4 的字面条款**（「扁平 `cep_mesh_e2e`（改造前）逐断言一致」）亦因该修复 change 对**双侧**断言做了有意重基线（MESH-3 → MESH-3a/3b，平铺与层次化同口径开环）而**不再字面成立**；**V-4 的实质结论不受影响** —— Step 7c 对纯 ChStream 的零回归仍成立（`src/core/module_factory.cc` 零 diff，`[cep]` / `[e2e]` / `[mesh]` 全量绿）。V-5 的 mesh 级口径由上述两条承接；**V-4 / V-5 原文均不改**（Verification 章不可变）。

3. **AC-07 已收敛为可机械取证的两条**，并以 `MESH-9` 作判别力对照（修复前必红 / 修复后绿）。

4. **同源另一面已分离立项。** 上述 credit 回流的另一面是「LOCAL 终端侧 credit 记账饿死」（本地投递停滞），与本 ADR 的 exposed 端口数据通路无关 ⇒ 分离为 `ADR-X.16`（Accepted：Router LOCAL 端口不参与 downstream credit 流控）+ change `cpptlm-router-local-port-credit-exhaustion`（已立项）。

5. **登记一处同族缺口**：`NICTLM`（`src/tlm/nic_tlm.cc`）全文无 `CATEGORY_CREDIT` 检查 ⇒ 在 `mesh_2x2_tlm` / `mesh_4x4_tlm` 类拓扑中，router 对 LOCAL ingress 的 credit 回流会进入 `NICTLM::reassemble`，污染计数并产出垃圾 `pe_resp_out`。`CohIfNICTLM` 侧已修，`NICTLM` 侧**未修**（per `ADR-X.16` 的 `## 关联` 章登记）。⛔ 不在本 ADR 范围内修。
