# simmodule-nested Specification

## Purpose

定义 SimModule 派生类通过 JSON 配置实现 N 层嵌套实例化的能力。CpuCluster 等 SimModule 容器持有内部子模块（含递归 SimModule 孙子模块），通过 `outputs`/`inputs` 字段暴露端口供外部连接。本 spec 描述嵌套激活、暴露端口语义、tick 转发等行为契约。
## Requirements
### Requirement: SimModule JSON 多层嵌套实例化

`SimModule` 派生类 MUST 支持通过 JSON 配置实现 N 层（CpuCluster→CpuCluster→...）嵌套实例化。顶层 `ModuleFactory::instantiateAll(json)` 检测到 `type` 为 `SimModule` 派生类时，**递归触发**该实例的 `simulate_instantiate(cfg)`，后者进一步实例化其内部子模块（包含 `SimModule` 派生类的孙子模块）。

#### Scenario: 单层嵌套（SimModule → SimObject）

- **WHEN** JSON 配置包含一个 `CpuCluster` 类型模块，其 `modules` 字段包含 4 个 `CPUTLM` + 1 个 `CacheTLM` + 1 个 `MemoryTLM`
- **THEN** 顶层 `instantiateAll` 构造 `CpuCluster` 实例并调用 `simulate_instantiate(cfg)`，后者构造 6 个 SimObject 子模块；`CpuCluster::getInternalInstance("cpu0")` 返回非空 `SimObject*` 且 `dynamic_cast<CPUTLM*>` 成功

#### Scenario: 双层嵌套（SimModule → SimModule → SimObject）

- **WHEN** JSON 配置顶层 `CpuCluster` 的 `modules` 字段包含一个嵌套的 `sub_cluster`（`type: "CpuCluster"`），其内部 `modules` 包含 2 个 `CPUTLM`
- **THEN** 顶层 `CpuCluster` 构造后递归激活 `sub_cluster`；`sub_cluster::getInternalInstance("cpu0")` 返回非空 `SimObject*`；`sub_cluster::getInternalOutputPort` 通过 `findInternalPath` 反向解析正确性

#### Scenario: 三层嵌套（SimModule × 3 → SimObject）

- **WHEN** JSON 配置包含 3 层 `CpuCluster→CpuCluster→CpuCluster→CPUTLM` 嵌套
- **THEN** 3 层 `CpuCluster` 实例均被构造并激活；最内层 `CpuCluster` 持有 1 个 `CPUTLM` 子模块；`internal_factory->getAllInstances()` 在每一层分别返回该层的子模块（不跨层共享）

#### Scenario: outputs/inputs 暴露端口

- **WHEN** JSON 配置中 `CpuCluster` 节点包含 `outputs: [{"internal": "cpu0.req_out", "external": "cpu0_to_bus"}]`
- **THEN** `CpuCluster::findInternalPath("cpu0_to_bus")` 返回 `"cpu0.req_out"`；`CpuCluster::isExposedPort("cpu0_to_bus")` 返回 `true`；外部模块 `connections: [{src: "cluster.cpu0_to_bus", ...}]` 解析时按 `internal_to_external_map` 反向解析
- **AND** 对 `getInternalOutputPort("cpu0.req_out")`：内部子模块为 `SimObject` 派生类（非 ChStream 协议模块）时 MUST 返回非空 `MasterPort*`；内部子模块为 **ChStream 协议模块**（`CPUTLM`/`CacheTLM`/`MemoryTLM`/`RouterTLM`）时亦 MUST 返回非空 `MasterPort*` —— 装配期已把 ChStream 端口 mirror 进子模块的 `PortManager`（`module_factory.cc` Step 7）
- **AND** 暴露端口的语义正确性 MUST 通过**真实数据流**验证（见 ADDED requirement「SimModule 暴露端口的数据通路接入」），⛔ 不得再以「仅 `findInternalPath`/`isExposedPort` 可解析」为满足条件

#### Scenario: inputs 暴露端口

- **WHEN** JSON 配置中 `CpuCluster` 节点包含 `inputs: [{"internal": "cpu0.resp_in", "external": "bus_to_cpu0"}]`
- **THEN** `isExposedPort("bus_to_cpu0")` 返回 `true`
- **AND** 对 `getInternalInputPort("cpu0.resp_in")`：内部子模块为 `SimObject` 派生类（非 ChStream 协议模块）时 MUST 返回非空 `SlavePort*`；内部子模块为 **ChStream 协议模块**时亦 MUST 返回非空 `SlavePort*`（同 mirror 机制）
- **AND** 暴露端口的语义正确性 MUST 通过**真实数据流**验证（见 ADDED requirement「SimModule 暴露端口的数据通路接入」）


### Requirement: simulate_instantiate 公开 API

`SimModule` MUST 提供 `void simulate_instantiate(const json& cfg)` 公共方法，**等价于** `instantiate(cfg)` 但语义上标记"模拟激活"（区别于构造期 `instantiate`）。该方法 MUST 被 `ModuleFactory::instantiateAll` 在 `SimModule` 派生类构造后**显式调用**。

#### Scenario: simulate_instantiate 幂等

- **WHEN** 同一 `SimModule` 实例上重复调用 `simulate_instantiate(cfg)` 两次
- **THEN** 第二次调用应在入口处检测到 `internal_factory->getAllInstances()` 已被 populate，**早退**（early-return，**不递增 `depth_`、不构造子模块、不消耗 depth 配额**）；现有 `getAllInstances()` 数量不变；depth_ 在第二次调用前后保持一致

#### Scenario: simulate_instantiate 与 set_config 顺序

- **WHEN** `SimModule` 构造后先调 `set_config(params)` 后调 `simulate_instantiate(cfg)`
- **THEN** `params` 中的字段（如 `num_cpus`）在 `simulate_instantiate` 之前已被基类存储；`cfg["modules"]` 中的子模块按 JSON 顺序构造

### Requirement: SimModule::tick 转发到子模块

`SimModule::tick()` 默认实现 MUST **转发**到所有内部子模块的 `tick()`，保证嵌套结构中子模块按调度顺序执行。

#### Scenario: tick 转发计数

- **WHEN** CpuCluster 持有 4 个 `CPUTLM` + 1 个 `CacheTLM` + 1 个 `MemoryTLM`
- **THEN** 调用 1 次 `cluster->tick()` 后，6 个子模块的 `tick()` 各被调用 1 次；测试可通过 `static int tick_count` 在每个子模块的 `tick()` 中 `++tick_count` 验证

#### Scenario: tick 不递归触发 CpuCluster 嵌套

- **WHEN** 三层 `CpuCluster→CpuCluster→CpuCluster→CPUTLM`，顶层 `startAllTicks()` 调用每个顶层 `SimObject::tick()`
- **THEN** 中间层 `CpuCluster::tick()` 转发到最内层 `CpuCluster::tick()`，最内层 `CpuCluster::tick()` 转发到 `CPUTLM::tick()`；CPUTLM 的 `tick()` **只被调用 1 次**（不重复触发）

### Requirement: SimModule 暴露端口的数据通路接入

`ModuleFactory` MUST 提供一条**隔离的连接阶段**（Step 7c），使「至少一端为 `SimModule` 暴露端口（`outputs`/`inputs` 声明的标签）」的 connection 能够接入**真实数据通路径** —— 即在暴露端口所指的内部子模块端口与对端顶层 ChStream 模块端口之间建立真实的 `PortPair`，使数据可在容器内外双向流动。

#### Scenario: 暴露端口连接建立真实 PortPair

- **WHEN** JSON 配置声明一个 `SimModule` 容器的 `outputs`/`inputs`，且顶层 `connections` 中存在以该容器暴露标签为端点的条目
- **THEN** 装配 MUST 在该容器的内部子模块端口与对端顶层 ChStream 模块端口之间建立 `PortPair`
- **AND** 两侧端口的 `isBound()` MUST 返回 `true`
- **AND** 数据 MUST 可经该连接双向流动（⛔ 不得静默丢弃连接）

#### Scenario: 方向按内部端口后缀推导

- **WHEN** 暴露端口的内部路径后缀为 `req_in[N]` / `resp_in[N]` / `req_out[N]` / `resp_out[N]`
- **THEN** 对端端口 MUST 分别取 `req_out` / `resp_out` / `req_in` / `resp_in`，且 `PortPair` 的两端 MUST 满足 Master（initiator）/ Slave（target）角色匹配
- **AND** 方向 MUST **NOT** 由暴露标签的字面（如 `portN_in` / `portN_out`）推断 —— 承载请求的 exposed **output**（如内部为 `req_out` 的暴露端口）MUST 接到对端的 `req_in`

#### Scenario: 失败 MUST 显式报错而非静默丢弃

- **WHEN** 暴露端口连接出现下列任一情形：①在**声明过暴露端口**的容器上引用未声明的标签；②内部端口不可解析（含嵌套容器暴露路径）；③对端索引越界或非数字；④内部端口后缀不属于 `req_in`/`resp_in`/`req_out`/`resp_out` 四类；⑤连接两端**都**是暴露端口
- **THEN** 装配 MUST 报错并**终止**（⛔ 不得沿用「静默 continue」惯例）
- **AND** 端口已绑定时 MUST 报错，⛔ 不得静默覆盖既有 `PortPair`

#### Scenario: 未声明暴露端口的 SimModule 自身端口引用 MUST 不受影响

- **WHEN** 一个**未**声明 `outputs`/`inputs` 的 `SimModule` 实例被以 `<实例名>.<端口名>` 形式引用（其端口位于该实例自身的 `PortManager`）
- **THEN** 该引用 MUST 继续按**自身普通端口**语义处理，行为与引入 Step 7c 之前**逐字节一致**
- **AND** MUST NOT 触发任何暴露端口相关的失败路径

#### Scenario: 纯 ChStream 连接路径 MUST 零变化

- **WHEN** 一条 connection 的两端都**不是**暴露端口
- **THEN** 其解析与连线 MUST 完全由既有阶段承担，MUST NOT 受 Step 7c 影响
