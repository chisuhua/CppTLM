## ADDED Requirements

### Requirement: SimModule JSON 多层嵌套实例化

`SimModule` 派生类 MUST 支持通过 JSON 配置实现 N 层（CpuCluster→CpuCluster→...）嵌套实例化。顶层 `ModuleFactory::instantiateAll(json)` 检测到 `type` 为 `SimModule` 派生类时，**递归触发**该实例的 `simulate_instantiate(cfg)`，后者进一步实例化其内部子模块（包含 `SimModule` 派生类的孙子模块）。

#### Scenario: 单层嵌套（SimModule → SimObject）

- **WHEN** JSON 配置包含一个 `CpuCluster` 类型模块，其 `modules` 字段包含 4 个 `CPUTLM` + 1 个 `CacheTLM` + 1 个 `MemoryTLM`
- **THEN** 顶层 `instantiateAll` 构造 `CpuCluster` 实例并调用 `simulate_instantiate(cfg)`，后者构造 6 个 SimObject 子模块；`CpuCluster::getInternalInstance("cpu0")` 返回非空 `SimObject*` 且 `dynamic_cast<CPUTLM*>` 成功

#### Scenario: 双层嵌套（SimModule → SimModule → SimObject）

- **WHEN** JSON 配置顶层 `CpuCluster` 的 `modules` 字段包含一个嵌套的 `sub_cluster`（`type: "CpuCluster"`），其内部 `modules` 包含 2 个 `CPUTLM`
- **THEN** 顶层 `CpuCluster` 构造后递归激活 `sub_cluster`；`sub_cluster::getInternalInstance("cpu0")` 返回非空 `SimObject*`；`sub_cluster::getInternalOutputPort` 可访问其内部端口

#### Scenario: 三层嵌套（SimModule × 3 → SimObject）

- **WHEN** JSON 配置包含 3 层 `CpuCluster→CpuCluster→CpuCluster→CPUTLM` 嵌套
- **THEN** 3 层 `CpuCluster` 实例均被构造并激活；最内层 `CpuCluster` 持有 1 个 `CPUTLM` 子模块；`internal_factory->getAllInstances()` 在每一层分别返回该层的子模块（不跨层共享）

#### Scenario: outputs/inputs 暴露端口

- **WHEN** JSON 配置中 `CpuCluster` 节点包含 `outputs: [{"internal": "cpu0.req_out", "external": "cpu0_to_bus"}]`
- **THEN** `CpuCluster::findInternalPath("cpu0_to_bus")` 返回 `"cpu0.req_out"`；`CpuCluster::isExposedPort("cpu0_to_bus")` 返回 `true`；`getInternalOutputPort("cpu0.req_out")` 返回非空 `MasterPort*`；外部模块 `connections: [{src: "cluster.cpu0_to_bus", ...}]` 解析时按 `internal_to_external_map` 反向解析

#### Scenario: inputs 暴露端口

- **WHEN** JSON 配置中 `CpuCluster` 节点包含 `inputs: [{"internal": "cpu0.resp_in", "external": "bus_to_cpu0"}]`
- **THEN** `CpuCluster::getInternalInputPort("cpu0.resp_in")` 返回非空 `SlavePort*`；`isExposedPort("bus_to_cpu0")` 返回 `true`

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
