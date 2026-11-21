# minimal-dgpu-soc-architecture Specification

## Purpose
确立最小 dGPU SoC 的架构基线：显存只有一份真源且由板卡独占，多个消费者共享同一 backing；BAR 布局与 backing 字段的命名必须与代码保持同源，使配置与代码不会各说各话。

## Requirements

### Requirement: 单一 VRAM backing 真源(per ADR-DGPU-05 v1.4 B7)

`DGpuBoard` SHALL be SoC 全部访存的唯一 backing owner,其他组件通过 raw pointer 注入访问,**不持有分配权**。

#### Scenario: backing 注入 5 消费者共享
- **GIVEN** `DGpuBoard::vram_storage_` 已分配 (8GB default-init, Linux lazy commit)
- **WHEN** `DGpuBoard::bind_memory_backings()` 调用
- **THEN** 5 消费者路径 (backdoor / BAR1 fast-path / BAR2 via memory_device / MemoryTLM backing / SDMA-GMMU legacy ptr) **均通过 set_backing_view / set_vram_backdoor / set_mem_view 等 setter 接收 raw pointer**
- **AND** 全部指向同一地址 (`assert(all_pointers_equal)`)
- **AND** 任何消费者对 backing 的写入立即对其他消费者可见 (天然 coherence)

### Requirement: Backing 字段命名约定(per ADR-DGPU-10)

三类后缀语义严格分离。模块 SHALL NOT 违反命名约定,否则不通过代码审查。

#### Scenario: owner 后缀唯一性
- **GIVEN** SoC 内任意模块声明 backing 字段
- **WHEN** 字段名以 `_storage_` 结尾
- **THEN** 该模块必须是 `DGpuBoard`(per ADR-DGPU-10 §3 Inv-1)
- **AND** 类型必须是 RAII 容器 (`std::vector<uint8_t>` / `std::unique_ptr<uint8_t[]>`)
- **AND** 该字段在所有其他模块 **0 匹配**(`grep -rn '_storage_' include/ src/ test/ | grep -v 'dgpu_board_shell\|vram_storage_'` 必须空)

#### Scenario: injected-ChStream 后缀
- **GIVEN** SoC 内 ChStream slave 模块 (MemoryTLM / PcieMemoryDevice) 接收 backing 注入
- **WHEN** 字段名以 `_backing_view_` 结尾
- **THEN** 类型必须是 raw pointer (`uint8_t*`),**永不**用 std::vector
- **AND** 配 size 字段 `_backing_view_size_`
- **AND** setter 命名 `set_backing_view(ptr, size)`

#### Scenario: injected-functional 后缀
- **GIVEN** SoC 内 functional-mode 模块 (SdmaEngineTLM / GmmuTLM) 接收 backing 注入
- **WHEN** 字段名以 `_backdoor_` 或 `_mem_view_` 结尾
- **THEN** 区分场景:
  - `_backdoor_` — host/sim 跨进程访问 (SDMA host/vram backdoor)
  - `_mem_view_` — sim 内部只读 view (GMMU 页表访问)

### Requirement: 仿真模式声明(per §1.4)

Minimal SoC v1.0 SHALL be **Functional Mode (LT, zero-delay)**,5 条简化是设计意图而非技术债。

#### Scenario: 仿真模式识别
- **WHEN** 新评审者阅读 architecture.md §1.4
- **THEN** 必须能识别 5 条具体简化项 (SDMA memcpy / GMMU sync / MemoryTLM seam holder / 无 Crossbar / 混合 wire-format)
- **AND** 每条简化项有 gem5 等价物 (MemBackdoor / translateFunctional / 等)
- **AND** 每条有解除条件 (v1.1 引入 AXI 时升级)

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

### Requirement: GMMU 是 Functional Translation Service(per §5)

GmmuTLM 在 v1.0 SHALL be 同步翻译函数,不是 SoC 模块(无 ChStream 端口)。

#### Scenario: 翻译 API 调用
- **GIVEN** `GmmuTLM::set_pt_base_lo/hi/enabled` 已配置,`set_mem_view` 已注入 backing
- **WHEN** SDMA `translate_cb_(iova, size, &phys)` 调用
- **THEN** GmmuTLM::translate() 同步返回 paddr (= backing offset)
- **AND** 翻译时延 < 100ns(零时延 functional mode)
- **AND** 返回的 paddr 是 **SoC 物理地址 = backing offset**(per Oracle Q9)

### Requirement: 5 消费者路径数据流(per §6.1)

SDMA H2D 完整路径 SHALL 走 functional-mode memcpy,**不**经 MemoryTLM::tick()。

#### Scenario: H2D 数据搬运
- **GIVEN** host_iova = 0, vram_offset = 0, size = 4096
- **WHEN** SDMA `translate_cb_(0, 4096, &phys)` 返回 phys (= backing offset 0x20000)
- **THEN** SDMA 执行 `memcpy(vram_backdoor_+0, host_backdoor_+0x20000, 4096)`
- **AND** **不**经过任何 ChStream 端口(per §1.4 functional-mode 声明)
- **AND** `MemoryTLM::tick()` 不被调用(MemoryTLM 是 seam holder)
- **AND** 数据立即对 framebuffer_storage_ 可见(天然 coherence)

### Requirement: Architecture.md 关联文档(per §0 同步规则)

架构视图 SHALL 引用关联 ADR + OpenSpec changes,逆向同步不强制。

#### Scenario: 文档同步审计
- **GIVEN** architecture.md 顶部 ## 关联 OpenSpec changes 段
- **WHEN** 用户打开任一关联 change 路径
- **THEN** 必须能跳转到对应 proposal.md / design.md
- **AND** architecture.md 必须引用 ADR-DGPU-05/06/07/10(配套 ADR 列表)

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
