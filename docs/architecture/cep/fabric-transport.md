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
