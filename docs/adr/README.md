# ADR - 架构决策记录

> **版本**: 1.3
> **最后更新**: 2026-06-23

---

## ADR 列表

### P1 级决策

| ADR | 议题 | 状态 |
|-----|------|------|
| [ADR-P1-TEMPLATE.md](./ADR-P1-TEMPLATE.md) | P1 级议题模板 | ✅ 模板 |

### X 级决策（交易、错误处理与 Phase 3+）

| ADR | 议题 | 状态 | 阶段 |
|-----|------|------|------|
| [ADR-X.1-transaction-id.md](./ADR-X.1-transaction-id.md) | 事务追踪 ID 分配 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.2-error-handling.md](./ADR-X.2-error-handling.md) | 错误处理策略 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.3-reset-strategy.md](./ADR-X.3-reset-strategy.md) | 复位策略 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.4-plugin-system.md](./ADR-X.4-plugin-system.md) | 插件系统 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.5-build-system.md](./ADR-X.5-build-system.md) | 构建系统 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.6-transaction-integration.md](./ADR-X.6-transaction-integration.md) | TransactionContext 整合 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.7-transaction-handling.md](./ADR-X.7-transaction-handling.md) | 模块/框架职责划分 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.8-fragment-handling.md](./ADR-X.8-fragment-handling.md) | 细粒度分片处理 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.9-port-type-system.md](./ADR-X.9-port-type-system.md) | Phase 3+ 端口类型系统 | ✅ 已实施 | Phase 3.2 |
| [ADR-X.10-parameter-framework.md](./ADR-X.10-parameter-framework.md) | Phase 3+ 参数框架 | 📋 待实施 | Phase 3.3 |
| [ADR-X.11-config-inheritance-and-fixes.md](./ADR-X.11-config-inheritance-and-fixes.md) | 配置继承与缺陷修复策略 | ✅ 已实施 | Phase 3.1 |
| [ADR-X.12-python-config-generator.md](./ADR-X.12-python-config-generator.md) | Python 配置生成器 | 📋 提案 | Phase 3.2 |
| [ADR-X.13-stub-multi-extension.md](./ADR-X.13-stub-multi-extension.md) | 多 TLM 扩展 stub 标记 | ✅ 已确认 | Phase 7+ |
| [ADR-X.14-coherence-domains-stub.md](./ADR-X.14-coherence-domains-stub.md) | `coherence_domains` 字段 stub 标记 | ✅ 已确认 | Phase 7+ |
| [ADR-X.15-cpptlm-v3-dgpu-extract.md](./ADR-X.15-cpptlm-v3-dgpu-extract.md) | cpptlm-v3-dgpu-extract (角色反转 + v3.0.0 BREAKING bump + 11 项物理删除清单, HSK-6 P0-1 门禁已完成 `fa2b3ec`) | ✅ Accepted | Phase 9 (W1-9) |
| [ADR-X.16-cpptlm-v05-redo.md](./ADR-X.16-cpptlm-v05-redo.md) | cpptlm-v05-redo (Net-new 取代 v3.0-extract,PTX-EMU submodule + adapter + per-warp instruction precision,8 项决策锁定, 反转 ADR-X.15 决策清单) | ✅ Accepted | Phase 10 (W1-12) |
| [ADR-INC-01-incorporate-parent-late-binding.md](./ADR-INC-01-incorporate-parent-late-binding.md) | ApuSoC::incorporate_parent 真实 late-binding 语义 (1A+2A+3A + 双层幂等 + 软失败 + 命名可配置) | ✅ 已实施 | P1 (`04399c8`) |
| [ADR-LIB-01-cpptlm-library-python-higher-cluster-factories.md](./ADR-LIB-01-cpptlm-library-python-higher-cluster-factories.md) | cpptlm.library Python 高级复合 cluster 工厂 API (`cpu_nested_cluster` / `memory_cluster_hierarchical` / `gpu_topology`) | ✅ 已实施 | F5 (`140fffd`) |
| [ADR-METRIC-01-cputlm-cache-memory-telemetry.md](./ADR-METRIC-01-cputlm-cache-memory-telemetry.md) | CPUTLM/CacheTLM/MemoryTLM Telemetry 统计收集框架 (`system.{cpu,cache,memory}` 命名空间) | ✅ 已实施 | F10 (`66d9674`) |

### NV 级决策（NVIDIA GPU 仿真）

| ADR | 议题 | 状态 | 阶段 |
|-----|------|:---:|------|
| [ADR-NV-01-gpu-soc-architecture-target.md](./ADR-NV-01-gpu-soc-architecture-target.md) | gpu_soc 独立 SoC 仿真目标（与 apu_soc 并行；14 新模块 + 3 阶段 + 共享 GpuCluster 子模块 + 借鉴 gpgpu-sim 不集成） | ✅ 已确认 | Phase 8.A-C |
| [ADR-NV-02-phase8b-d1-strategy.md](./ADR-NV-02-phase8b-d1-strategy.md) | Phase 8.B D1-Full 全栈注入策略（WarpScheduler + Scoreboard + Pipeline + TensorCore 通过 SMContext 注入 PTX-EMU） | ✅ 已确认 | Phase 8.B |

### 汇总文档

| 文档 | 说明 |
|------|------|
| [ADR-X-SUMMARY.md](./ADR-X-SUMMARY.md) | X 系列决策汇总 |

---

## ADR 状态说明

| 状态 | 说明 |
|------|------|
| ✅ 已确认/已实施 | 已批准或已实施，必须遵循 |
| 📋 待实施 | 已记录，按计划实施 |
| ⏳ 待讨论 | 需要进一步讨论 |

---

**维护**: CppTLM 开发团队
