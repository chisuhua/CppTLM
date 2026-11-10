# Fabric & Transport Plane(第 6 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> **传输层**:装配 seam(ModuleFactory)、「经网络」的定义与拓扑分档判据。⚠️ 本层当前**无 CEP 自有运行时实现**(仅 `fabric.json` 声明 + 装配期死锁自由性闸)。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 6.3 ModuleFactory 集成 seam(per `ADR-CEP-07 §2 Decision 3`)

**真实注册机制**(不发明不存在的宏形式,per oracle v0.4 B7 实证):

| 模块类型 | 基类 | 注册机制 |
|---------|------|----------|
| **ProtocolEngine** 派生类 | `ChStreamModuleBase` | `ModuleFactory::registerObject<T>`(per `include/core/module_factory.hh:122`)|
| **SharerTracker** 派生类 | `SimModule`(per `module_factory.hh:135` `static_assert std::is_base_of_v<SimModule, T>`)| `REGISTER_MODULE(T)`(per `include/modules_cluster.hh:41-105`,合法参数化宏)|
| **FabricCollectivePlane** 派生类 | `SimModule` | `REGISTER_MODULE(T)` |
| **StubCacheCore** | `ChStreamModuleBase` | `ModuleFactory::registerObject<T>` + `ChStreamAdapterFactory::create` |

> **注意**:`REGISTER_CHSTREAM`(`include/chstream_register.hh:50`)与 `REGISTER_OBJECT`(`include/modules.hh:30`)**均无参数**,不存在 `REGISTER_CHSTREAM(T)` / `REGISTER_OBJECT(T)` 形式。

**冻结面**:CEP 不进 `cpptlm_emulator.h`(23 ABI 零 diff);不改 `include/tlm/pcie/pcie_endpoint_tlm.h` `[[deprecated]]` 状态;不接管 `dgpu_board_shell.cc`;不破 `include/tlm/cache_tlm.hh`。


### 6.3.1 「经网络」的定义与拓扑分档(J-9)

> **本节解决什么**:「**事务经网络抵达对端**」这个说法在仓内**极易假绿** —— 本节钉死
> 「网络」与「桥」各指什么,并给出**按拓扑分档的可机判判据**。

**「网络」= ChStream `connections`(可带 `latency`)。**
**桥 = 装配期投影登记器,⛔ 不在数据路径上** —— 它是**无状态投影器**(`pack`/`unpack`
只搬共有字段 + `flow_id → flit_category` 有损投影),`src/` 中**零**调用点。

⇒ ⛔ **「事务穿过桥抵达对端」是假绿表述** ⇒ 必须**拆成两条独立判据**:

| 判据 | 内容 |
|---|---|
| **① 互联可达** | 「一次事务经**带 latency 的 ChStream 连接**抵达对端」—— ⛔ **不经桥** |
| **② 桥投影** | **测试级**断言(构造 `CohIfBundle` → `pack` → 断言 `NoCFlitBundle.flit_category` 正确 → `unpack` → 断言共有字段恒等)⇒ ⛔ **不得**写成「事务穿过桥」 |

> ⚠️ 仓内**不存在** Router + flit 级路径 ⇒ 拓扑的**名称 / 描述 SHALL 如实反映**这一点。

**J-9 拓扑分档判据(机器判;J-9 是 J-1 的分档细化,⛔ 不改 J-1~J-8 的编号与含义)**：

| 拓扑 | 判据 |
|---|---|
| **A(直连)** | 装配闸 `SKIP`(⇒ `cep.bridges` 键**缺省**)∧ `cep.bridges` **不存在** ∧ 所有 `connections` **无 `latency > 0`** |
| **B(经网络)** | 装配闸 `PASS` ∧ `cep.bridges` **恰好 1 条** ∧ 该条 `modules` **恰好 3** ∧ **至少 1 条** `connections` 带 `latency > 0` ∧ 三个模块的**桥指针**指向**同一 `bridge_id`** |

**为什么 A 必须零延迟(因果序 ⛔ 不可颠倒)**：
- **主理由 = 判别力**:若 A 也带 `latency` ⇒ B 的「互联可达」面**退化为 A 的子集** ⇒ 两拓扑并存失去主要依据;
- **次理由 = latency 断言纯度**:A 是全绿载体 ⇒ 其 latency 断言须**只**反映模块/协议处理延迟,⛔ 不混入连接延迟。

**两条实施契约(缺一 ⇒ J-9 落空)**：
1. B 的 bridge entry **必须**带 `protocol_instance` 且命名一个**已声明**实例(缺失或未声明 ⇒ 判红);
2. `ChStreamModuleBase` **无 `bridge()` 访问器**(`attach_protocol_bridge` 是虚函数,`bridge()` 只在 `ProtocolEngineBase`)
   ⇒ 「逐模块桥指针指向同一 `bridge_id`」预设了当前**不存在**的能力 ⇒ 契约须写明:
   **wrapper / home 模型自行存储桥指针并暴露只读访问器**(⛔ 不动基类)。

> ⚠️ **桥成员互斥**:`ProtocolEngineBase::bridge_` 是**单槽**且 `attach_protocol_bridge` 直接赋值
> ⇒ 同一模块被**多个** bridge entry 挂载 ⇒ **桥被静默覆盖而装配仍返回 true** ⇒ ⛔ 不得让同一模块出现在两个 entry。

## 混排装配规则(多协议实例共享同一 fabric)

> **本节回答什么**:同一张 `fabric`(network)上**混排多个协议实例**时,VC 分配与通道依赖如何协调。
> ⚠️ **本节是规则面,不是状态面** —— 逐实例的声明取值在 `include/tlm/cep/protocol_instances.json`,机器判定在 `include/tlm/cep/collective_plane_contract.hh`(C-1′)与 `include/tlm/cep/channel_dependency_graph.hh`。

### R1 · 跨实例 VC 语义一致性(**混排准入条件**)

**规范载体**:`ADR-CEP-01` 的 **C-1′**(「槽位语义继承」)—— 集合平面**不声明**自己的 VC 绑定,而**继承**该 network 上**全部「在该 network 上有 `vc_binding` 的」既有实例**的 `(message_class → vc_id)` 映射,且**逐类相等**。
⇒ **本节是 C-1′ 的镜像 + 补两个 C-1′ 未覆盖的维度**。

**结构(实测,方向以 `protocol_instances.json` 为准)**:`vc_binding` 是 **`network → 消息类 → VC`**(⛔ **不是** VC → 消息类);`ordering_assumption` 是 **`network → vc → 排序假设`**。

**两条一致性要求**:

| # | 要求 | 形式化 |
|:--:|---|---|
| **a** | **VC → 消息类集合**逐 VC 相等 | 对同一 `network` 的每个 `vc`:`{cls \| vc_binding[network][cls] == vc}` 在所有绑定该 network 的实例上**集合相等** |
| **b** | **VC → 排序假设**逐 VC 相等 | `ordering_assumption[network][vc]` 在所有绑定该 network 的实例上**逐 VC 相等** |

> ⚠️ **词表差异(⛔ 不可直接求交)**:`vc_binding` 用 **6 类消息词表**(`request` / `response` / `snoop` / `snoop_resp` / `atomic` / `query`),而 `flow_support` 用 **canonical 10 流名**(`response` 的对应位置是 **`completion`**)⇒ ⛔ **不得**把二者求交当作「有效消息类」(那会把 `response` / `atomic` / `query` 全部误丢,且**改变 C-1′ 的判据语义**)。
> ⇒ 要求 (a) **按 `vc_binding` 原始映射比对**(与 C-1′ 实现一致);「某实例是否承载某流」由 `flow_support` **独立表达**,R1 **不据此过滤**。

**实测(2026-11-11,读 `protocol_instances.json` 的 8 个实例)**:

| network | 绑定实例 | a(VC→类集合) | b(VC→排序假设) |
|---|---|---|---|
| `compute_noc` | **7** | ❌ **不一致** | ❌ **不一致** |
| `ualink_fabric` | **3** | ✅ 一致 | ✅ 一致 |

**`compute_noc` 的差异点(唯一来源 = `tilelink_c`)**:

| 实例 | VC→消息类(`compute_noc`) | VC→排序假设(`compute_noc`) |
|---|---|---|
| `moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4` | `request=0` `response=1` `snoop=2` `snoop_resp=3` | `0:unordered` `1:p2p_ordered` `2:unordered` `3:p2p_ordered` |
| **`tilelink_c`** | `request=0` **`snoop=1`** **`snoop_resp=2`** **`response=3`** | `0:unordered` **`1:unordered`** **`2:p2p_ordered`** `3:p2p_ordered` |

⇒ **`vc=1`** 对前者是 `response`(且 `p2p_ordered`),对 `tilelink_c` 是 `snoop`(且 `unordered`)⇒ **同 VC 承载两类消息 + 两种排序假设**。

> ⚠️ **这不是本节的新发现** —— `ADR-CEP-01` E.3.1 段**已登记**:`compute_noc` 上 `tilelink_c` 与其余 6 实例不一致 ⇒ **该网不得用 M-2**(集合平面共享 VC 模式)。
> ⇒ 本节的**真实增量 = 补上要求 b(排序假设逐 VC 一致)**—— 这一维度 C-1′ **不覆盖**;并把 C-1′ 的**适用域限制**显式化。

**诚实边界(⚠️ 精确表述)**:
- ⛔ **不是「无机器判定」** —— C-1′ 的实现**已存在**(`include/tlm/cep/collective_plane_contract.hh` 的逐实例逐类比较,不等 ⇒ FAIL,归因 `[CP-03-VC-CERTIFICATE/C-1′]`,折进 **CP-03** 行);
- ⚠️ **但其适用域受 `certificate_scope` 限制**(当前**仅 `ualink_fabric`**)⇒ **`compute_noc` 当前无判定**;
- ⛔ **要求 b(排序假设)当前无机器判定** ⇒ 登记为**缺口**;
- ⚠️ `INV-CEP-12-IC2` 的三条件(per-network 键对称 / vc_id 槽位上界 / 6 类词表)**不含跨实例一致性**;装配期无环闸对 `vc_binding` 语义**结构性恒真**(见 R2)⇒ **两条既有防线都不拦截本项**。

**下一步(独立 change,⛔ 不在本节)**:**扩展 C-1′ 的适用域**(或把 C-1′ 抽成 network-agnostic 判据,供装配闸与 CP-03 共用)⇒ ⛔ **不在装配闸另造第二实现**(违「唯一实现 + 禁双实现」纪律 —— BSS-01 先例:唯一实现放 `bridge_module_membership.hh`,装配闸与 `ModuleFactory` 共用)。配套**决定性变异**:把某实例的某 vc 改成另一消息类 ⇒ 必红。

### R2 · 通道依赖边表是 **network 级**属性

**规则**:`protocol_instances.json` 的 `channel_dependency_edges.per_network` 按 **network** 给出 VC 间的偏序边(实测 `compute_noc` 5 条、`ualink_fabric` 3 条),对**该 network 上每个实例**同时生效 ⇒ **混排不改变偏序**。

**实测**(`compute_noc`):`0→1`、`0→2`、`0→3`、`1→3`、`3→2`;(`ualink_fabric`):`0→3`、`1→3`、`2→3`。

> ⚠️ **与 R1 的关系(精确表述)**:R1 **是** R2 的**语义前提**(R2 的边按 VC 读,只有当各实例的 VC↔类映射一致时,同一条边才在各实例读作同一类序)。但 **R1 违例时 R2 的边仍良定义**(VC 级偏序不依赖语义分配)⇒ 坏掉的是**跨实例语义读法漂移**,⛔ **不是「无定义」**。
> 依据 `ADR-CEP-01` §E.4:任一实例的图 = 该 network 声明边表的**诱导子图** ⇒ DAG 的子图仍是 DAG ⇒ **无环闸在结构上不可能报出由 `vc_binding` 引起的环** ⇒ 它的 verdict **与类→VC 的语义分配无关**。

### R3 · collective VC 预留(**当前为空 = M-2 共享模式**)

**规则**:`collective_planes.json` 的 `collective_vc_reservation` 为 collective 消息预留**独占 VC**;为空 ⇒ collective 与主干**共享 VC**(M-2)。

**实测**:该数组**为空**(`[]`);plane 绑定 = `moesi_gpu` → `null` · `mesif` → `null` · `nvlink_4` → `LocalCollectivePlane` · `ualink_4` → `FabricCollectivePlane`。

**M-2 共享模式的义务 = `ADR-CEP-01` 的四条证书**(C-1′ 槽位语义继承 / C-2 VC 覆盖完整 / C-3 控制面与数据面互斥 / C-4 未检验项显式登记),由 **V6-a**(空预留门)+ **V6-d**(四证书)折进 **CP-03** 行 ⇒ ✅ **有机器判定**。
> ⚠️ 本节**不另造**「collective 消息类不得同名」的约束 —— collective **消息类词表尚不存在**(主干只有 6 类词表)⇒ 在无词表的前提下自造禁令**不可判定**;⛔ 若将来引入 collective 专属类,须**同步补机器判定**。

### R4 · 桥成员互斥(见 §6.3.1)

同一模块被**多个** bridge entry 挂载 ⇒ **桥被静默覆盖而装配仍返回 true** ⇒ ⛔ 不得让同一模块出现在两个 entry。⇒ 已在装配路径落地**结构性关口**(`[BSS-01]` 执行臂)。

### R5 · 混排的准入前提(汇总)

| # | 前提 | 当前机器判定 |
|:--:|---|---|
| R1 | 跨实例 VC 语义一致((a) 类集合 + (b) 排序假设) | ⚠️ **(a) 已有载体**(C-1′)但**适用域受限**(`certificate_scope` 仅 `ualink_fabric`)⇒ **`compute_noc` 无判定**;**(b) 无载体** |
| R2 | 通道依赖边表(network 级) | ✅ 装配期无环检查(**对 `vc_binding` 语义恒真** ⇒ 不拦截 R1) |
| R3 | collective VC 预留 / M-2 四证书 | ✅ V6-a + V6-d(折进 CP-03) |
| R4 | 桥成员互斥 | ✅ `[BSS-01]` |

⇒ **结论**:混排的**通道依赖面**(R2)与**桥装配面**(R4)已机器化;**M-2 准入面**(R1-a)有载体但**域未覆盖**;**R1-b / R1-c** 无载体 ⇒ ⚠️ **混排的语义正确性当前依赖声明正确性 + 人工核对**,不依赖机器全量拦截。
