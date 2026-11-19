# ADR — 架构决策记录（真源 3）

> **定位**: 真源 3 · 不可变决策。**签发后不改正文**，状态变化只追加 `## Status Update` 段。
> **本索引**: 状态与日期**机械抽取自各 ADR 文件本体**（`## Status` / `> **状态**:` 字段），**零手抄**。
> **重新生成**: `python3 scripts/` 内无生成器 —— 本文件由 `docs/adr/README.md` 生成流程维护；改动 ADR 后请同步本表。
> **最后更新**: 2026-11-19（docs 重组：重建全量索引，覆盖此前遗漏的 39 份 ADR-CEP / ADR-DGPU）

---

## 1. 全量统计

| 分组 | 份数 | 状态分布 |
|------|------|---------|
| **CEP 系列**（Coherence Exploration Platform） | 24 | ✅ 已签发 20 · 🟡 起草中 3 · 🟢 Proposed 1 |
| **DGPU 系列**（dGPU 应用架构） | 15 | ✅ 已签发 2 · 📋 提案 10 · 🟡 起草中 2 · 🟢 Proposed 1 |
| **X 系列**（框架基础：事务 / 错误 / 复位 / 参数 / 端口） | 18 | Phase 3.1 已完成，Phase 3.2 待实施，Phase 3.3 部… 1 · ✅ Accepted 3 · ✅ 已实施 8 · ✅ 已确认 2 · 📋 待确认 1 · 📋 提案 1 · 🟡 部分实施 1 · 🟢 Proposed 1 |
| **其他**（INC / LIB / METRIC / NV / TEMPLATE） | 6 | ✅ 已实施 3 · ✅ 已确认 3 |
| **合计** | **63** | — |

> **口径说明**：`ADR-CEP-NUMBER-MAP.md` 是**编号预分配表**（非决策），计入 CEP 份数；
> `ADR-P1-TEMPLATE.md` 是**模板**（非决策），计入其他份数。
> 较上层 `AGENTS.md` 的「59 份」为旧口径 —— 以本表 **63 份** 为准。

---

## 2. CEP 系列（Coherence Exploration Platform）

> 治理宪章 = [`ADR-CEP-00-platform-charter-and-governance.md`](./ADR-CEP-00-platform-charter-and-governance.md)（跨仓归口 + 准入门槛）；
> **真源章节锚点表** = [`ADR-CEP-00A-real-source-anchor-map.md`](./ADR-CEP-00A-real-source-anchor-map.md)
> （**引用任何 CEP 章节前必查该表**，禁凭记忆写节号）。

| ADR | 议题 | 状态 |
|-----|------|------|
| [ADR-CEP-00-platform-charter-and-governance.md](./ADR-CEP-00-platform-charter-and-governance.md) | ADR-CEP-00: Coherence Exploration Platform 跨仓治理与归口 | 🟡 起草中 |
| [ADR-CEP-00A-real-source-anchor-map.md](./ADR-CEP-00A-real-source-anchor-map.md) | ADR-CEP-00A: Coherence Exploration Platform 真源章节映射表 | 🟡 起草中 |
| [ADR-CEP-01-fabric-collective-plane.md](./ADR-CEP-01-fabric-collective-plane.md) | ADR-CEP-01: Fabric Collective Plane 作为旁路平面 | ✅ 已签发 |
| [ADR-CEP-02-coherence-barrier-semantics-clarification.md](./ADR-CEP-02-coherence-barrier-semantics-clarification.md) | ADR-CEP-02: Coherence Interface 10 条抽象事务流清单 + barrier 流语义澄清 | ✅ 已签发 |
| [ADR-CEP-03-fabric-collective-plane-internals.md](./ADR-CEP-03-fabric-collective-plane-internals.md) | ADR-CEP-03: Fabric Collective Plane 内部接口与跨平面契约 | ✅ 已签发 |
| [ADR-CEP-04-sharer-tracker-abstraction-redesign.md](./ADR-CEP-04-sharer-tracker-abstraction-redesign.md) | ADR-CEP-04: Sharer Tracker 抽象重构(D-5 重构 + caps descriptor) | ✅ 已签发 |
| [ADR-CEP-05-stub-cache-core-interface.md](./ADR-CEP-05-stub-cache-core-interface.md) | ADR-CEP-05: Stub Cache Core 接口(RN 侧投影 + 折叠映射表) | ✅ 已签发 |
| [ADR-CEP-06-experiment-protocol.md](./ADR-CEP-06-experiment-protocol.md) | ADR-CEP-06: Coherence Exploration Platform 实验输出协议 | ✅ 已签发 |
| [ADR-CEP-07-simulation-and-integration-semantics.md](./ADR-CEP-07-simulation-and-integration-semantics.md) | ADR-CEP-07: Coherence Exploration Platform 仿真语义与集成 Seam | ✅ 已签发 |
| [ADR-CEP-07A-06bis-artifact-disposition.md](./ADR-CEP-07A-06bis-artifact-disposition.md) | ADR-CEP-07A: 06-bis 既有产物处置 | ✅ 已签发 |
| [ADR-CEP-08-ordering-point-portability.md](./ADR-CEP-08-ordering-point-portability.md) | ADR-CEP-08: Ordering Point 可迁移性规则 | ✅ 已签发 |
| [ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md](./ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md) | ADR-CEP-09: 多 VC 路由器仲裁与 VC 分配的死锁自由性论证 | ✅ 已签发 |
| [ADR-CEP-11-protocol-version-locking.md](./ADR-CEP-11-protocol-version-locking.md) | ADR-CEP-11: 协议版本锁定与演进策略 | ✅ 已签发 |
| [ADR-CEP-12-protocol-engine-capability-map.md](./ADR-CEP-12-protocol-engine-capability-map.md) | ADR-CEP-12: ProtocolEngine 插件能力映射表 Schema | ✅ 已签发 |
| [ADR-CEP-13-coherence-domain-ownership.md](./ADR-CEP-13-coherence-domain-ownership.md) | ADR-CEP-13: 一致性域归属与跨域组合判据 | ✅ 已签发 |
| [ADR-CEP-14-stub-contract-tests.md](./ADR-CEP-14-stub-contract-tests.md) | ADR-CEP-14: Stub 可验证性合同测试 | ✅ 已签发 |
| [ADR-CEP-15-cachetlm-alignment.md](./ADR-CEP-15-cachetlm-alignment.md) | ADR-CEP-15: CppTLM CacheTLM 与 CEP Stub Cache 的对齐边界 | ✅ 已签发 |
| [ADR-CEP-16-06-evolution-route-mapping.md](./ADR-CEP-16-06-evolution-route-mapping.md) | ADR-CEP-16: 06 §15.4 Coherence 演进路线 → CEP Stage 映射 | ✅ 已签发 |
| [ADR-CEP-17-ruby-slicc-protocol-family.md](./ADR-CEP-17-ruby-slicc-protocol-family.md) | ADR-CEP-17: Ruby-SLICC 协议族接入与协议族抽象语义 | ✅ 已签发 |
| [ADR-CEP-18-protocol-generation-scope.md](./ADR-CEP-18-protocol-generation-scope.md) | ADR-CEP-18: 协议生成路线范围(SSP 描述 → 插件生成 vs 纯手写插件) | ✅ 已签发 |
| [ADR-CEP-19-cohif-carrier-and-protocol-bridge.md](./ADR-CEP-19-cohif-carrier-and-protocol-bridge.md) | ADR-CEP-19: CohIf 载体(业务 Bundle + ChStream 端口面 + ProtocolBridge 特化) | ✅ 已签发 |
| [ADR-CEP-20-topology-ic3-evidence-strength.md](./ADR-CEP-20-topology-ic3-evidence-strength.md) | ADR-CEP-20: 拓扑级 IC3 证据的强度边界与能力阶梯 | ✅ 已签发 |
| [ADR-CEP-21-sharer-bitmap-expansion.md](./ADR-CEP-21-sharer-bitmap-expansion.md) | ADR-CEP-21: Sharer 位宽 —— responder bit 映射的**扩容机制**与触发判据 | 🟢 Proposed |
| [ADR-CEP-NUMBER-MAP.md](./ADR-CEP-NUMBER-MAP.md) | ADR-CEP-NUMBER-MAP: Coherence Exploration Platform 编号预分配 | 🟡 起草中 |

---

## 3. DGPU 系列（dGPU 应用架构）

> 对应三视角架构文档：`docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/architecture.md`（真源 2）。

| ADR | 议题 | 状态 |
|-----|------|------|
| [ADR-DGPU-01-callback-worker-replaces-detached-threads.md](./ADR-DGPU-01-callback-worker-replaces-detached-threads.md) | ADR-DGPU-01: CallbackWorker 替代 detached 线程 | 📋 提案 |
| [ADR-DGPU-02-lifecycle-protocol-uniform-destruction.md](./ADR-DGPU-02-lifecycle-protocol-uniform-destruction.md) | ADR-DGPU-02: LifecycleProtocol 状态机 + 统一析构顺序 + callback nullification 协议 | 📋 提案 |
| [ADR-DGPU-03-data-driven-pcie-path-dispatch-registry.md](./ADR-DGPU-03-data-driven-pcie-path-dispatch-registry.md) | ADR-DGPU-03: DispatchRegistry data-driven 替代 4 态 switch | 📋 提案 |
| [ADR-DGPU-04-namespace-unification-ep-cache-test-isolation.md](./ADR-DGPU-04-namespace-unification-ep-cache-test-isolation.md) | ADR-DGPU-04: DGpuSoc 命名空间统一 + EpCache lazy cache + 测试可访问性隔离 | 📋 提案 |
| [ADR-DGPU-05-vram-storage-ownership.md](./ADR-DGPU-05-vram-storage-ownership.md) | ADR-DGPU-05: VRAM backing 单一所有权归 DGpuBoard (vram_storage_) | 📋 提案 |
| [ADR-DGPU-06-axi-mem-bundle-boundary.md](./ADR-DGPU-06-axi-mem-bundle-boundary.md) | ADR-DGPU-06: AxiMemBundle vs PcieTlpBundle 边界严格分离 | 📋 提案 |
| [ADR-DGPU-07-minimal-soc-evolution-seam.md](./ADR-DGPU-07-minimal-soc-evolution-seam.md) | ADR-DGPU-07: Minimal SoC → 完整 GPU 演进 Seam 设计 (D3-D5 接口零变更) | 📋 提案 |
| [ADR-DGPU-08-abi-freeze-policy.md](./ADR-DGPU-08-abi-freeze-policy.md) | ADR-DGPU-08: ABI 冻结策略 (23 ABI 签名级 + 二进制级 0 diff + 0 新增约束) | 📋 提案 |
| [ADR-DGPU-09-driver-visible-minimal-soc-scope.md](./ADR-DGPU-09-driver-visible-minimal-soc-scope.md) | ADR-DGPU-09: Driver-Visible Minimal SoC 范围定义 (In Scope / Out of Scope) | 📋 提案 |
| [ADR-DGPU-10-backing-naming-convention.md](./ADR-DGPU-10-backing-naming-convention.md) | ADR-DGPU-10: Backing Storage 命名约定 (owner / injected 两级) | 📋 提案 |
| [ADR-DGPU-11-timing-mode-soc-scope.md](./ADR-DGPU-11-timing-mode-soc-scope.md) | ADR-DGPU-11: Timing-Mode (AT, cycle-approximate) SoC 与 functional-mode minimal_v1 并列共存 | ✅ 已签发 |
| [ADR-DGPU-12-soc-internal-tlm-wiring-principle.md](./ADR-DGPU-12-soc-internal-tlm-wiring-principle.md) | ADR-DGPU-12: SoC 内部 wiring 原则 (SoC 内部 = TLM, SoC ↔ Board = 方法注入) | ✅ 已签发 |
| [ADR-DGPU-13-pnn-protocol-neutral-network.md](./ADR-DGPU-13-pnn-protocol-neutral-network.md) | ADR-DGPU-13: PNN 协议中立通信网络范围定义 | 🟡 起草中 |
| [ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md](./ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md) | ADR-DGPU-14: StreamAdapter ↔ ProtocolBridge Seam 决策 | 🟡 起草中 |
| [ADR-DGPU-15-pcie-endpoint-ip-cpptlm-archforge-boundary.md](./ADR-DGPU-15-pcie-endpoint-ip-cpptlm-archforge-boundary.md) | ADR-DGPU-15: PcieEndpointIP 的 CppTLM / ArchForge 职责边界 —— 键面契约升格 + 已消费/零消费二分 | 🟢 Proposed |

---

## 4. X 系列（框架基础）

| ADR | 议题 | 状态 |
|-----|------|------|
| [ADR-X-SUMMARY.md](./ADR-X-SUMMARY.md) | ADR-X 系列议题决策汇总 | Phase 3.1 已完成，Phase 3.2 待实施，Phase 3.3 部… |
| [ADR-X.1-transaction-id.md](./ADR-X.1-transaction-id.md) | 事务追踪 ID 分配策略 | ✅ 已实施 |
| [ADR-X.10-parameter-framework.md](./ADR-X.10-parameter-framework.md) | Phase 3+ 参数框架 | 🟡 部分实施 |
| [ADR-X.11-config-inheritance-and-fixes.md](./ADR-X.11-config-inheritance-and-fixes.md) | 配置继承机制与 Phase 3.1 缺陷修复策略 | ✅ 已实施 |
| [ADR-X.12-python-config-generator.md](./ADR-X.12-python-config-generator.md) | Python 配置生成器设计 | 📋 提案 |
| [ADR-X.13-stub-multi-extension.md](./ADR-X.13-stub-multi-extension.md) | tlm_stub 多 Extension 升级 | ✅ 已实施 |
| [ADR-X.14-coherence-domains-stub.md](./ADR-X.14-coherence-domains-stub.md) | Mark `coherence_domains` field as stub | ✅ Accepted |
| [ADR-X.15-simmodule-exposed-port-data-path.md](./ADR-X.15-simmodule-exposed-port-data-path.md) | SimModule 暴露端口的真实数据通路径接入 | ✅ Accepted |
| [ADR-X.16-router-local-port-terminal-credit.md](./ADR-X.16-router-local-port-terminal-credit.md) | Router LOCAL 端口不参与 downstream credit 流控 | ✅ Accepted |
| [ADR-X.17-module-param-read-declares.md](./ADR-X.17-module-param-read-declares.md) | 模块参数的「读取即声明」机制（内联加载 + 消费即登记，不使用宏） | 🟢 Proposed |
| [ADR-X.2-error-handling.md](./ADR-X.2-error-handling.md) | 错误处理策略（最终确认版） | ✅ 已确认 |
| [ADR-X.3-reset-strategy.md](./ADR-X.3-reset-strategy.md) | 复位策略 | 📋 待确认 |
| [ADR-X.4-plugin-system.md](./ADR-X.4-plugin-system.md) | 插件系统 | ✅ 已实施 |
| [ADR-X.5-build-system.md](./ADR-X.5-build-system.md) | 构建系统 | ✅ 已实施 |
| [ADR-X.6-transaction-integration.md](./ADR-X.6-transaction-integration.md) | TransactionContext 与现有 Packet/Extension 整合方案 | ✅ 已实施 |
| [ADR-X.7-transaction-handling.md](./ADR-X.7-transaction-handling.md) | 模块与框架层交易处理职责划分 | ✅ 已实施 |
| [ADR-X.8-fragment-handling.md](./ADR-X.8-fragment-handling.md) | 细粒度分片交易处理与 TLM/RTL 职责分离 | ✅ 已确认 |
| [ADR-X.9-port-type-system.md](./ADR-X.9-port-type-system.md) | Phase 3+ 端口类型系统 | ✅ 已实施 |

---

## 5. 其他

| ADR | 议题 | 状态 |
|-----|------|------|
| [ADR-INC-01-incorporate-parent-late-binding.md](./ADR-INC-01-incorporate-parent-late-binding.md) | ADR-INC-01: ApuSoC::incorporate_parent 真实 Late-Binding 语义 | ✅ 已实施 |
| [ADR-LIB-01-cpptlm-library-python-higher-cluster-factories.md](./ADR-LIB-01-cpptlm-library-python-higher-cluster-factories.md) | ADR-LIB-01: cpptlm.library Python 高级复合 Cluster 工厂 API | ✅ 已实施 |
| [ADR-METRIC-01-cputlm-cache-memory-telemetry.md](./ADR-METRIC-01-cputlm-cache-memory-telemetry.md) | ADR-METRIC-01: CPUTLM/CacheTLM/MemoryTLM Telemetry 统计收集框架 | ✅ 已实施 |
| [ADR-NV-01-gpu-soc-architecture-target.md](./ADR-NV-01-gpu-soc-architecture-target.md) | ADR-NV-01: gpu_soc 独立 SoC 仿真目标 | ✅ 已确认 |
| [ADR-NV-02-phase8b-d1-strategy.md](./ADR-NV-02-phase8b-d1-strategy.md) | ADR-NV-02: Phase 8.B D1-Lite 渐进策略——基于 PTX-EMU 真实接口 | ✅ 已确认 |
| [ADR-P1-TEMPLATE.md](./ADR-P1-TEMPLATE.md) | 混合仿真架构 P1 级决策 | ✅ 已确认 |

---

## 6. 归档与取代

已归档（物理文件在 `docs-archived/adr/`，**不再维护**）：

| 原 ADR | 归档位置 | 取代者 |
|--------|---------|--------|
| ADR-X.15-cpptlm-v3-dgpu-extract | `docs-archived/adr/` | ADR-SOC-06 MVP 切片（迁 ArchForge） |
| ADR-X.16-cpptlm-v05-redo | `docs-archived/adr/` | ADR-SOC-06 MVP 切片（迁 ArchForge） |
| ADR-SOC-01 ~ ADR-SOC-27（27 份） | `docs/soc_arch/` 占位 → ArchForge 仓 | 见 ArchForge `docs/adr/` |

> ⚠️ **ADR 不可变**：本仓 `docs/adr/**` 的决策内容**不可单方面修改**。
> 变更决策须新开 ADR 走「取代」流程（`Parent: superseded by ADR-NNN` + Status Update）。

---

## 7. 状态标签口径

| 标签 | 含义 |
|------|------|
| ✅ 已签发 / Accepted | 决策已定稿并签发，正文不可变 |
| ✅ 已实施 | 决策对应的能力已落地（实施状态可能另有跟踪文档） |
| ✅ 已确认 | 早期决策，确认但无独立签发流程 |
| 🟢 Proposed | 决策已作出，待实施 / 待立项 |
| 🟡 起草中 / 部分实施 | 文档未定稿，或决策已作出但仅部分落地 |
| 📋 提案 / 待确认 | 尚未形成决策 |

---

**维护**: architect（决策内容）· builder（索引同步）
