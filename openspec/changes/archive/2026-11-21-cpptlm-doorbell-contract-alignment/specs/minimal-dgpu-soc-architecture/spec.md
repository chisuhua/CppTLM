## RENAMED Requirements

- FROM: `### Requirement: doorbell 配置 vs 代码 hardcoded 不一致(per §13 R4)`
- TO: `### Requirement: doorbell 合成偏移是已裁定的现状(per ADR-DGPU-09 §2.2.2 + d3-evolution-roadmap)`

## MODIFIED Requirements

### Requirement: BAR 布局与代码真相对齐(per §4.1)

`kBar1DoorbellOffset = 0x10010000ULL` SHALL be hardcoded 常量(7 阶段 PCIe EP 共识)。该偏移是**测试专用合成偏移** —— doorbell 路由**不依赖** `pcie_ep.params.bar_sizes[1]` 的大小(BAR1 实窗为 16MB 时同样命中);⛔ 本仓**不**设置"实窗大小"类约束(依据 per `ADR-DGPU-09 §2.2.2`)。

#### Scenario: doorbell 配置命中
- **GIVEN** `pcie_ep.params.bar_sizes[1] = 268435464` (256MB+64KB+8 字节)
- **WHEN** host `mmio_write(BAR1, 0x10010000, wptr)`
- **THEN** doorbell 路由命中,SDMA ring consume
- **AND** 5+ 测试用例 (test_dgpu_board_doorbell_routing.cc 等) 全绿

#### Scenario: doorbell 配置过小(检测 config bug)
- **GIVEN** `pcie_ep.params.bar_sizes[1] = 16777216` (16MB, 如 dgpu_soc_minimal_v1.json 现状)
- **WHEN** host `mmio_write(BAR1, 0x10010000, wptr)`
- **THEN** doorbell 写**仍命中** doorbell 路由(合成偏移走 BAR1 内的**优先匹配**,先于实窗 bound 检查)
- **AND** 本 Scenario 的 **config-bug 检测结论 = 否**:配置过小**不**构成 config bug(合成偏移与实窗大小无关)⇒ 无「config 大小不足」类告警
- **AND** ⛔ 不破坏 SDMA ring consume 路径

### Requirement: doorbell 合成偏移是已裁定的现状(per ADR-DGPU-09 §2.2.2 + d3-evolution-roadmap)

doorbell 使用**测试专用合成偏移** (`kBar1DoorbellOffset = 0x10010000`) SHALL 被视为**已裁定的现状**,而非待修缺陷 —— 依据 `ADR-DGPU-09 §2.2.2`(「测试专用合成偏移;不依赖 BAR1 实窗大小;**不修**」)+ `docs/designs/dgpu-soc/d3-evolution-roadmap.md` 的 doorbell 议题结论(「维持现状,不修」)。⛔ 本仓**不**维护「配置大小 vs 代码常量」类对账条款。

#### Scenario: 风险追踪
- **GIVEN** 合成偏移与 BAR1 实窗大小**无关**(per `ADR-DGPU-09 §2.2.2`「不依赖 BAR1 实窗大小」+ d3-roadmap「维持现状,不修」)
- **WHEN** 复核本 Requirement
- **THEN** 该偏移作为**已裁定的现状**被承认,⛔ **不**列为待修缺陷
- **AND** ⛔ 本仓**不**维护「配置大小 vs 代码常量」类对账条款

#### Scenario: 现状被承认(合成偏移不依赖 BAR1 实窗)
- **GIVEN** 任意含 SoC 的配置(`pcie_ep.params.bar_sizes[1]` 为 16MB 或其他值)
- **WHEN** host `mmio_write(BAR1, 0x10010000, wptr)`
- **THEN** doorbell 路由命中,SDMA ring consume(合成偏移走**优先匹配**,与实窗大小无关)
- **AND** 该行为**不**被记为 config bug,⛔ 无「config 大小不足」类告警

#### Scenario: 复活条件(语义触发 · per d3-evolution-roadmap)
- **GIVEN** 后续**真 driver 集成**时 doorbell **物理地址 ≠ 0x10010000**(合成偏移不再是可用的替代地址)
- **WHEN** 复核本 Requirement
- **THEN** 必须回头修订本 Requirement(⛔ 不得沿用「维持现状」措辞)
- **AND** 修订须走 ADR `## Status Update` 流程(per `ADR-DGPU-09 §2.2.2`)

#### Scenario: 复活条件(可机判触发带 · 数值比较)
- **GIVEN** 任一含 SoC 的配置出现 `pcie_ep.params.bar_sizes[1] >= 268435464`(= 256MB+64KB)⇒ 合成偏移将落入 BAR1 实窗内 ⇒「测试专用偏移」的前提失效
- **WHEN** 复核本 Requirement(机判:`grep '"bar_sizes"' configs/*.json examples/*.json | awk -F'[:,]' '{b=$3+0; if (b>=268435464) print}' | wc -l` 命中数 ≥ **1**)
- **THEN** 必须回头修订本 Requirement
- **AND** 修订须走 ADR `## Status Update` 流程
