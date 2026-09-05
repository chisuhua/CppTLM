# SoC Architecture Decision Records (ADR-SOC)

> **版本**: 2.0
> **创建日期**: 2026-06-14
> **更新日期**: 2027-02-09（v1.0 dGPU SoC 战略 + 6 份新 ADR）
> **范围**: CppTLM SoC 仿真架构决策（CPU/GPU/IO/Cache/NoC 集成层）
> **与 `docs/adr/` 的关系**: 本目录存放**应用层（SoC 设计）**决策；`docs/adr/` 存放**框架层（仿真基础设施）**决策。两层分离，避免 SoC 决策与框架决策混淆。

---

## 命名空间与编号

- **前缀**: `ADR-SOC-NN-<topic>.md`
- **NN**: 两位数字，从 `01` 起递增
- **topic**: 小写连字符短语，简短描述决策主题

## ADR 列表（2027-02-09 v2.0 更新）

| ADR | 议题 | 状态 | 关联阶段 |
|-----|------|:----:|----------|
| [ADR-SOC-01-coherence-protocol-strategy.md](./ADR-SOC-01-coherence-protocol-strategy.md) | 一致性协议分步走策略（I/S/M 三态 → MOESI 升级） | ✅ + Status Update | Phase 7.A → 7.C |
| [ADR-SOC-02-cu-granularity.md](./ADR-SOC-02-cu-granularity.md) | ComputeUnit 黑盒优先（不做 5-stage pipeline） | ⚠️ Superseded | Phase 7.B+ |
| [ADR-SOC-03-wavefront-coalescing-abstraction.md](./ADR-SOC-03-wavefront-coalescing-abstraction.md) | Wavefront Coalescing 抽象（`coalescing_factor` 参数） | ⚠️ Superseded | Phase 7.B+ |
| [ADR-SOC-04-hsapp-cp-dispatcher-simplification.md](./ADR-SOC-04-hsapp-cp-dispatcher-simplification.md) | HSAPP/CP/Dispatcher 极简化（`KernelLaunchTLM` 取代 HSA 三件套） | ⚠️ Superseded partial | Phase 7.B+ |
| [ADR-SOC-05-gpu-directory-structure.md](./ADR-SOC-05-gpu-directory-structure.md) | GPU 目录结构（`include/tlm/gpu/` 子目录） | ✅ + Status Update | Phase 7.A 起 |
| [ADR-SOC-06-cpptlm-v05-mvp.md](./ADR-SOC-06-cpptlm-v05-mvp.md) | cpptlm-v05-mvp（**MVP 切片**：UsrLinuxEmu IOCTL → CppTLM CP→TMU→SQ→CudaCore + PTX-EMU functional/timing 分离，4 阶段 6-10 周，沿用 ADR-X.16 决策） | ✅ Accepted | Phase 10 MVP (W1-10) |
| [ADR-SOC-07-dgpu-board-soc-layering.md](./ADR-SOC-07-dgpu-board-soc-layering.md) | dGPU Board/SOC 两层分离（`DGpuBoard` C++ ABI shell + `DGpuSoc` JSON 拓扑；PCIe Config Space/BAR/MSI-X 归属 SOC 片内 `PcieEndpointTLM`，upstream DMA 归属 `SdmaEngineTLM`；细化 ADR-SOC-06 D4/D5，23 ABI 外部契约不变） | ✅ Accepted | MVP 后续 (s3 之后) |
| [ADR-SOC-08-v55-system-hw-integration-preconditions.md](./ADR-SOC-08-v55-system-hw-integration-preconditions.md) | **v5.5+ 系统级硬件仿真集成的前置测试契约**（per UsrLinuxEmu ADR-089 v0.5 + ADR-088 §C2；本仓 5 项 Tier 2 前置测试 P0/P1/P2 治理 + 4 项 UsrLinuxEmu 增量提议送审；不修订 UsrLinuxEmu 文档，遵守 ADR-035 跨仓治理边界）| 📋 + Status Update | OpenSpec change `2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/` |
| [ADR-SOC-09-v1-nvidia-amd-dual-vendor.md](./ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) | **dGPU SoC v1.0 NVIDIA + AMD 双 driver stack 战略**（共享 PCIe EP/HBM/L2/NoC + L3/L5/L6 双 vendor 路径 + UsrLinuxEmu 跨仓依赖标注） | 📋 Proposed | v1.0 (2027-Q3+) |
| [ADR-SOC-10-module-factory-topology.md](./ADR-SOC-10-module-factory-topology.md) | **dGPU SoC v1.0 ModuleFactory 拓扑层**（9 类 SimModule P2-P5 层级容器 + ApuSoC 顶层 + 单一入口 JSON 拓扑） | 📋 Proposed | v1.0 (2027-Q3+) |
| [ADR-SOC-11-pcie-endpoint-ip.md](./ADR-SOC-11-pcie-endpoint-ip.md) | **PcieEndpointIP 替代 PcieEndpointTLM**（17 ports 整合模块 + `[[deprecated]]` 标注 + 23 ABI 头冻结不变量） | 📋 Proposed | Phase 4-8 (已实施，HEAD `429327d`) |
| [ADR-SOC-12-host-bypass-and-rc.md](./ADR-SOC-12-host-bypass-and-rc.md) | **Host Bypass 软件 bring-up 路径 + 自研 RC PF0-only 简化**（Oracle M1/M2 修复） | 📋 Proposed | Phase 7-8 (已实施，HEAD `429327d`) |
| [ADR-SOC-13-axi-stream-adapter-mapper.md](./ADR-SOC-13-axi-stream-adapter-mapper.md) | **AXI Stream Adapter + AXI4Mapper 集成**（PCIe↔AXI 边界 + outstanding 跟踪 + OOO completion + 512-bit 限制） | 📋 Proposed | Phase 5-6 (已实施) |
| [ADR-SOC-14-v55-integration-revision.md](./ADR-SOC-14-v55-integration-revision.md) | **v5.5+ 系统级硬件仿真集成修订**（Phase 8 后 23 ABI 状态 + 5 项前置测试已实施 + UsrLinuxEmu 集成未闭环） | 📋 Proposed | v1.0+ |
| [ADR-SOC-15-cdna-real-isa-roadmap.md](./ADR-SOC-15-cdna-real-isa-roadmap.md) | **dGPU SoC v1.0 CDNA 真实物理 ISA 演进路线图**（双轨前端 + 统一 Timing 宿主，4 阶段 43-73 人天：InstrDescriptor 中立化 → IMemoryPort 异步内存 Seam → CDNA 引擎接入 → 双轨校准；完整方案设计见 [`architecture/11-cdna-real-isa-integration.md`](../architecture/11-cdna-real-isa-integration.md)） | 📋 Proposed | v1.0+ |
| [ADR-SOC-16-sm-microarchitecture.md](./ADR-SOC-16-sm-microarchitecture.md) | **dGPU SoC v1.0 SM 微架构重构**（反转 ADR-SOC-02 黑盒优先；12 个 ChStream 子模块 + 8 种 Bundle + `IComputeDevice` 15 方法 + SM-owns-state 模式 + bit-exact Gate；实施见 OpenSpec change `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`） | ✅ Accepted | v1.0 (2027-Q3+) |

## 与 `docs/adr/ADR-X.17-cpptlm-v05-mvp.md` 的 cross-reference(已迁回本目录)

[`ADR-SOC-06`](./ADR-SOC-06-cpptlm-v05-mvp.md) 原文件位于 `docs/adr/ADR-X.17-cpptlm-v05-mvp.md`,per Phase I.4 (2026-08-20) 迁回本目录,因为这是 SoC 应用层决策(CP→TMU→SQ→CudaCore 完整链路 + PTX-EMU 深度集成),符合 `docs/soc_arch/adr/` 命名空间与层划分。

具体 SoC 模块设计见:
- [`dgpu-board.md`](../modules/dgpu-board.md)(DGpuBoardTLM **8 组件**包装,含新增 SubmitQueue)
- [`command-processor.md`](../modules/command-processor.md)(CP 5-state FSM,NVIDIA method packet)
- [`pm4-decoder.md`](../modules/pm4-decoder.md)(NVIDIA method packet 解析)
- [`tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md)(TMU Glue,反压停 fetch)
- [`submit-queue.md`](../modules/submit-queue.md)(🆕 WDU 分发网络,per `docs/research/WDUtoSM/overview.md`)
- [`cuda-core-adapter.md`](../modules/cuda-core-adapter.md)(per Phase I.2 **SM 微架构探索器**)
- [`ptx-emu-submodule-mvp.md`](../modules/ptx-emu-submodule-mvp.md)(per Phase I.1 **PTX functional facade**)

路线图:[`roadmap-mvp-to-v05.md`](../roadmap/roadmap-mvp-to-v05.md)(MVP 4 阶段 6-10 周 + 可选 v0.5 完整版 12 周)

---

## 状态说明

| 状态 | 说明 |
|------|------|
| ✅ 已确认 | 已批准或已实施，必须遵循 |
| 📋 提案 | 起草中，需进一步讨论 |
| ⏳ 待讨论 | 需要进一步讨论 |
| ❌ 已废弃 | 决策被替代 |

---

## 与 `docs/adr/`（框架 ADR）的关系

| 维度 | `docs/adr/` | `docs/soc_arch/adr/` |
|------|-------------|---------------------|
| **层** | 框架层（仿真基础设施） | 应用层（SoC 设计） |
| **示例议题** | 事务追踪、错误处理、端口类型 | 一致性协议、CU 建模粒度、Host-GPU 接口 |
| **稳定性** | 极稳定，跨 SoC 共用 | 较不稳定，随 SoC 拓扑变化 |
| **模板** | `docs/adr/ADR-P1-TEMPLATE.md` | 本目录 ADR 沿用 X.N 结构（Context/Decision/Implementation） |
| **当前规模** | 13 个（X.1-X.13） | 8 个（SOC-01-SOC-08） |

---

## ADR 撰写规范

每个 ADR 必须包含：

1. **Status / Date / Impact / Category** 元信息
2. **Context（背景）**: 决策的"为什么"——列选项、对比
3. **Decision（决策）**: ✅ 标记的采纳选项 + 理由
4. **Implementation（实施）**: 各阶段任务 + 验收标准
5. **Risks / Mitigations（如适用）**
6. **References（参考文献）**: 调研、roadmap、微架构、SoC spec 链接

参考模板：[`ADR-SOC-05-gpu-directory-structure.md`](./ADR-SOC-05-gpu-directory-structure.md)（最完整示例）

---

## 维护

- 新增 ADR: 复制最新 SOC-NN 模板 → 编号递增 → 提交后更新本 README 索引
- 状态变更: 追加 `## Status Update` 段落（不修改历史决策内容，遵循 ADR 不可变原则）
- 跨引用: 关联 ADR 时用相对路径 `./ADR-SOC-NN-*.md`

---

**维护**: CppTLM 开发团队
