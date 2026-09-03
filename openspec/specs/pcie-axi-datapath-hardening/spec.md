# pcie-axi-datapath-hardening Specification

## Purpose
TBD - created by archiving change 2026-09-03-cpptlm-pcie-axi-datapath-hardening. Update Purpose after archive.
## Requirements
### Requirement: axi-rw-discrimination

`PcieEndpointIP::tick()` SHALL 使用 `Axi4Bundle::is_write_request()` 谓词判别 AXI 写/读请求，
不再使用 `awid!=0 || awaddr!=0 || awlen!=0` 启发式判别。

#### Scenario: 边界写请求被正确识别

- **WHEN** 收到写请求 `awid=0, awaddr=0, awlen=0, wlast=1, wdata!=0`
- **THEN** 调用 `is_write_request()` 返回 true
- **AND** 进入 cfg 或 BAR 写分支处理

#### Scenario: 读请求不被误判为写

- **WHEN** 收到纯读请求 `awid=0, awaddr=0, awlen=0, arid!=0`
- **THEN** 调用 `is_write_request()` 返回 false
- **AND** 进入 cfg 或 BAR 读分支处理

### Requirement: bar-store-4byte-granularity

BAR backing store SHALL 使用 4-byte 对齐 key (`addr & ~0x3`) 与 4-byte 写入语义，
遵守 `wstrb` 按字节 mask。

#### Scenario: 单笔 4B 写入只影响目标 slot

- **WHEN** 写入 `awaddr=0x1000, wdata=0xAABBCCDD, wstrb=0xF`
- **THEN** 仅 `bar_store_[0x1000]` 受影响
- **AND** 不影响 `bar_store_[0x1004]`(若存在)

#### Scenario: wstrb 部分写保留未选字节

- **WHEN** 写入 `awaddr=0x1000, wdata=0xCAFEBABE, wstrb=0x3` (仅低 2 字节有效)
- **THEN** 仅 `bar_store_[0x1000]` 的低 2 字节被更新
- **AND** `bar_store_[0x1000]` 的高 2 字节保持原值 (mask=0xFFFF0000)

#### Scenario: 4B 读返回 slot 的 4B 值

- **WHEN** 读 `araddr=0x1000`
- **THEN** 返回 `bar_store_[0x1000]` (低 4B)

### Requirement: cfg-bar-boundary

cfg 范围 SHALL 严格按 `awaddr < config_size` 判定,边界值正确路由。

#### Scenario: 边界值 0x1000 走 BAR (4096B config)

- **WHEN** 写入 `awaddr=0x1000` (config_size=4096=0x1000)
- **THEN** 不在 cfg 范围,走 BAR 路径
- **AND** 不应触发 cfg 写入

#### Scenario: 256B config 变体 (deferred — 见 §Deferred Scope)

> **Deferral note**: `PcieSriovVfPool::PcieConfigSpacePerVf` 内部固定 4096 字节 config_size,本仓无 256B 切换接口。本场景延期至独立 change `cpptlm-pcie-config-size-parameterization`,需先扩展 `PcieSriovVfPool` 暴露 config_size 注入。

