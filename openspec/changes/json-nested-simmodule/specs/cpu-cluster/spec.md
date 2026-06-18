## ADDED Requirements

### Requirement: CpuCluster 增强并移出 legacy

`CpuCluster` MUST 从 `include/modules/legacy/cpu_cluster.hh` 移出至 `include/tlm/cluster/cpu_cluster.hh`，重新实现 `tick()`、`set_config(params)`、`get_module_type()`，并通过 `REGISTER_MODULE` 宏在 `include/modules.hh` 中**无条件注册**（不再受 `BUILD_LEGACY_MODULES` 守卫）。

#### Scenario: CpuCluster 位置迁移

- **WHEN** 编译 `BUILD_LEGACY_MODULES=OFF`（默认）
- **THEN** `include/tlm/cluster/cpu_cluster.hh` 存在；`include/modules/legacy/cpu_cluster.hh` **不存在**；`#include "tlm/cluster/cpu_cluster.hh"` 编译通过

#### Scenario: CpuCluster 无条件注册

- **WHEN** `BUILD_LEGACY_MODULES=OFF`（默认）
- **THEN** `ModuleFactory::getRegisteredModuleTypes()` 返回列表**包含** `"CpuCluster"`；`ModuleFactory::getInstance<CpuCluster>("my_cluster")` 通过 JSON `"type": "CpuCluster"` 构造返回非空 `CpuCluster*`

### Requirement: CpuCluster 参数透传

`CpuCluster` MUST 支持通过 JSON `params` 字段透传 `num_cpus`（默认 4）和 `cluster_id`（可选字符串），并通过 `set_config(params)` 注入。

#### Scenario: 默认 num_cpus=4

- **WHEN** JSON 配置 `"type": "CpuCluster"` 不带 `params` 字段
- **THEN** `CpuCluster::set_config({})` 后 `num_cpus_ == 4`；`cluster_id_` 为空字符串

#### Scenario: 显式 num_cpus=8

- **WHEN** JSON 配置 `"type": "CpuCluster", "params": {"num_cpus": 8}`
- **THEN** `set_config` 后 `num_cpus_ == 8`

#### Scenario: 显式 cluster_id

- **WHEN** JSON 配置 `"type": "CpuCluster", "params": {"cluster_id": "cluster_a"}`
- **THEN** `set_config` 后 `cluster_id_ == "cluster_a"`；可通过 `cluster->get_module_type() + "_" + cluster_id_` 用于 hierarchical debug 日志

### Requirement: CpuCluster 与 CPUTLM/CacheTLM/MemoryTLM 端到端跑通

`CpuCluster` 内部持有 `CPUTLM` + `CacheTLM` + `MemoryTLM` 子模块时，MUST 支持完整的 E2E req/resp 数据流（CPUTLM 发起读请求 → CacheTLM 处理 → MemoryTLM 响应 → CacheTLM 回写 → CPUTLM 收到响应）。

#### Scenario: 4 CPU + 1 Cache + 1 Memory 数据流

- **WHEN** JSON 配置 `CpuCluster` 内部包含 `cpu0/cpu1/cpu2/cpu3`（CPUTLM）、`cache`（CacheTLM）、`mem`（MemoryTLM），connections 串接 `cpu0→cache→mem` 等
- **THEN** 运行 100 周期后，`cpu0.last_response_transaction_id_ > 0`（CPUTLM 端到端响应可达性指标）；`cache` 命中/未命中统计可读；`mem` 接收请求数 ≥ 1

#### Scenario: outputs/inputs 暴露端口端到端

- **WHEN** JSON 配置 CpuCluster 暴露 `outputs: [{"internal": "cpu0.req_out", "external": "cpu0_to_bus"}]`，外部 `connections: [{src: "cluster.cpu0_to_bus", dst: "bus.cpu_in", ...}]`
- **THEN** `ConnectionResolver` 正确解析 `cluster.cpu0_to_bus` → `cpu0.req_out`；外部模块 `bus` 通过 `CpuCluster::getInternalOutputPort("cpu0.req_out")` 拿到 `MasterPort*` 并完成连接

### Requirement: CpuCluster::tick 转发

`CpuCluster::tick()` MUST 显式转发到 `internal_factory` 的所有 SimObject 子模块。

#### Scenario: tick 转发不重复

- **WHEN** CpuCluster 持有 4 个 CPUTLM + 1 个 CacheTLM + 1 个 MemoryTLM
- **THEN** 顶层 `startAllTicks()` 调用 1 次 `cluster->tick()` 后，6 个子模块的 `tick()` 各被调用 1 次（用 static counter 验证）

#### Scenario: 嵌套 CpuCluster tick 递归转发

- **WHEN** 三层 CpuCluster 嵌套，最内层持有 1 个 CPUTLM
- **THEN** 顶层 `startAllTicks()` 调用 1 次 `outer_cluster->tick()` 后，最内层 CPUTLM 的 `tick()` 被调用 1 次（每层 `tick()` 各调用 1 次转发，3 层共 3 次转发，但 CPUTLM 只在最内层被调用 1 次）
