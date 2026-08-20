# SoC Architecture Decision Records (ADR-SOC)

> **版本**: 1.0
> **创建日期**: 2026-06-14
> **范围**: CppTLM SoC 仿真架构决策（CPU/GPU/IO/Cache/NoC 集成层）
> **与 `docs/adr/` 的关系**: 本目录存放**应用层（SoC 设计）**决策；`docs/adr/` 存放**框架层（仿真基础设施）**决策。两层分离，避免 SoC 决策与框架决策混淆。

---

## 命名空间与编号

- **前缀**: `ADR-SOC-NN-<topic>.md`
- **NN**: 两位数字，从 `01` 起递增
- **topic**: 小写连字符短语，简短描述决策主题

## ADR 列表

| ADR | 议题 | 状态 | 关联阶段 |
|-----|------|:----:|----------|
| [ADR-SOC-01-coherence-protocol-strategy.md](./ADR-SOC-01-coherence-protocol-strategy.md) | 一致性协议分步走策略（I/S/M 三态 → MOESI 升级） | ✅ 已确认 | Phase 7.A → 7.C |
| [ADR-SOC-02-cu-granularity.md](./ADR-SOC-02-cu-granularity.md) | ComputeUnit 黑盒优先（不做 5-stage pipeline） | ✅ 已确认 | Phase 7.B+ |
| [ADR-SOC-03-wavefront-coalescing-abstraction.md](./ADR-SOC-03-wavefront-coalescing-abstraction.md) | Wavefront Coalescing 抽象（`coalescing_factor` 参数） | ✅ 已确认 | Phase 7.B+ |
| [ADR-SOC-04-hsapp-cp-dispatcher-simplification.md](./ADR-SOC-04-hsapp-cp-dispatcher-simplification.md) | HSAPP/CP/Dispatcher 极简化（`KernelLaunchTLM` 取代 HSA 三件套） | ✅ 已确认 | Phase 7.B+ |
| [ADR-SOC-05-gpu-directory-structure.md](./ADR-SOC-05-gpu-directory-structure.md) | GPU 目录结构（`include/tlm/gpu/` 子目录） | ✅ 已确认 | Phase 7.A 起 |

## 与 `docs/adr/ADR-X.17-cpptlm-v05-mvp.md` 的 cross-reference

[`ADR-X.17`](../../adr/ADR-X.17-cpptlm-v05-mvp.md)(cpptlm-v05-mvp,2026-08-19)涉及 SoC 应用层决策(CP→TMU→Cuda Core 完整链路),在框架层 ADR(位于 `docs/adr/`)实施。具体 SoC 模块设计见:
- [`dgpu-board.md`](../modules/dgpu-board.md)(DGpuBoardTLM 5 组件包装)
- [`command-processor.md`](../modules/command-processor.md)(CP 5-state FSM)
- [`pm4-decoder.md`](../modules/pm4-decoder.md)(Mesa-style TYPE3)
- [`tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md)(TMU Glue)
- [`cuda-core-adapter.md`](../modules/cuda-core-adapter.md)(新概念,per-warp step 入口)
- [`ptx-emu-submodule-mvp.md`](../modules/ptx-emu-submodule-mvp.md)(PTX-EMU adapter + 编译防火墙)

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
| **当前规模** | 13 个（X.1-X.13） | 5 个（SOC-01-SOC-05） |

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
