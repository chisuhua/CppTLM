# ADR-SOC-08: v5.5+ 系统级硬件仿真集成的前置测试契约

> **状态**: 📋 Proposed — 2026-08-28 · **日期**: 2026-08-28 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**:
> - [`ADR-SOC-07-dgpu-board-soc-layering.md`](./ADR-SOC-07-dgpu-board-soc-layering.md) — 本仓 dGPU SOC 分层
> - UsrLinuxEmu [`ADR-088`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) §C2/§D3.8 — 23 ABI + `cpptlm_dma_translate_cb` 外部契约
> - UsrLinuxEmu [`ADR-089`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-089-v55-system-hw-simulation.md) v0.5 — v5.5+ VFIO/IOMMUFD/vDPA/Migration 仿真
> - UsrLinuxEmu [`ADR-090 v2`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) — PTX-EMU 归属 CppTLM submodule (canonical 真相源)
> **关联 OpenSpec**:
> - [`openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/`](../../../openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/) — 前置测试实施
> - [`openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/`](../../../openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/) — PcieEndpointTLM
> - [`openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/`](../../../openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/) — SdmaEngineTLM

---

## 1. Context

### 1.1 ADR-089 描述的端到端集成链

UsrLinuxEmu ADR-089 v0.5 (Accepted 2026-08-16) 描述 v5.5+ 系统级硬件仿真分 4 阶段:

```
v5.5.1 VFIO 核心仿真     (UsrLinuxEmu 团队 6-8 周)
v5.5.2 IOMMUFD 仿真      (UsrLinuxEmu 团队 6-8 周)
v5.5.3 vDPA 仿真         (UsrLinuxEmu 团队 4-6 周)
v5.5.4 Live Migration    (UsrLinuxEmu 团队 4-6 周)
```

**集成路径**(per ADR-088 §C2):
```
UsrLinuxEmu VFIO 仿真 → cpptlm_emulator_* 23 ABI → CppTLM PcieEndpointTLM/SdmaEngineTLM → 仿真 host 内存
```

### 1.2 当前 CppTLM 测试覆盖的 gap

本仓交付 PcieEndpointTLM / SdmaEngineTLM 时,已覆盖:
- ✅ H2D / D2H 端到端数据搬运(`test_sdma_engine_h2d/d2h.cc`)
- ✅ IOMMU fault 错误路径(`test_sdma_engine_iommu_fault.cc`)
- ✅ BAR0 doorbell + MMIO_WRITE 响应立即(`test_pcie_endpoint_tick_e2e.cc`)
- ✅ MSI-X 中断投递
- ✅ BAR1 MEM 转发 + Config Space

**但存在 6 类 gap,会在 UsrLinuxEmu 集成时暴露**:
1. **wire-format 跨仓字节级一致性**(防止 silent contract drift)
2. **DMA translate callback 边界**(phys 越界 / 异常)
3. **MSI-X 状态机细粒度**(mask/unmask/PBA)
4. **Doorbell 排队/并发**
5. **backdoor ABI 隔离**(MMIO 与 backdoor 路径)
6. **23 ABI shell 包装**(per `cpptlm-dgpu-abi-export` change)

### 1.3 跨仓治理边界(per UsrLinuxEmu ADR-036 + UsrLinuxEmu ADR-088 §C2)

**本 ADR- 责任**:
- ✅ 本仓侧前置测试增强(OpenSpec change 实施)
- ✅ 提议 UsrLinuxEmu ADR-089 / ADR-088 增量修订(供 UsrLinuxEmu owner 决策)

**不在本 ADR- 责任**:
- ❌ 修改 UsrLinuxEmu 仓文档(违反 UsrLinuxEmu ADR-036 3 区分)
- ❌ 在本仓实施 UsrLinuxEmu 端测试(超出本仓范围)

## 2. Decision

### D1: 接受 UsrLinuxEmu ADR-089 v0.5 范围(不修订)

UsrLinuxEmu ADR-089 v0.5 已 Accepted,本仓**不**提议重新打开。本 ADR- 仅作为**本仓侧前置测试的治理载体**,不涉及 UsrLinuxEmu 决策变更。

### D2: 在本仓实施 5 项前置测试(per OpenSpec change)

按 ROI 排序:

| 优先级 | 测试 ID | 测试目标 | 防止的失败模式 | 工作量 |
|---|---|---|---|---:|
| **P0** | (E) | **wire-format 快照测试** | **silent contract drift** — 字段重排会破坏 UsrLinuxEmu 解析 | 半天 |
| **P0** | (A) | DMA translate 边界 | IOMMUFD 集成时 phys 越界 / 异常 | 半天 |
| **P1** | (C) | MSI-X 状态机细粒度 | VFIO SET_IRQS 行为契约偏差 | 1 天 |
| **P1** | (B) | Doorbell 排队/并发 | GPU driver 启动路径 race | 1 天 |
| **P2** | (D) | backdoor ABI 隔离 | 23 ABI backdoor 与 MMIO 路径冲突 | 半天 |

**第 6 项**(23 ABI shell 包装测试)由姊妹 change `cpptlm-dgpu-abi-export` 实施,不在本 ADR-。

### D3: 向 UsrLinuxEmu owner 提议 4 项增量(送审,不直接修订)

| # | 提议 | 落地点 | 紧迫度 |
|---|---|---|---|
| 1 | ADR-089 §D7 "IOMMUFD consumer" 表追加 **CppTLM 跨仓契约验证段** — 通过 PcieEndpointTLM/SdmaEngineTLM 的 wire-format 快照,验证 IOMMUFD 仿真在调用 ABI 时的 bundle 解析一致 | UsrLinuxEmu ADR-089 | 中 |
| 2 | ADR-089 §D7 "VFIO consumer" 表明确 **BAR0 寄存器行为的 CppTLM 侧契约来源** (即本文档 §6 测试覆盖矩阵) | UsrLinuxEmu ADR-089 | 中 |
| 3 | ADR-088 §C2 增量补注 "前置测试来源" 段(per `cpptlm-dgpu-pcie-slice-prerequisites` change) | UsrLinuxEmu ADR-088 | 低 |
| 4 | UsrLinuxEmu 仓在 `cpptlm-dgpu-abi-export` change archive 前,消费本仓的 Tier 2 前置测试 snapshot(防止 ABI 行为偏差) | UsrLinuxEmu workflow | 高 |

### D4: OpenSpec change 命名约定

`openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/`

**字母代号约定**(与姊妹 changes 共享,per ADR-088 + ADR-SOC-07):
- **change E** = 本 change(前置测试)
- **change A** = `cpptlm-dgpu-pcie-endpoint`(已 archive)
- **change B** = `cpptlm-dgpu-sdma-engine`(已 archive)
- **change C** = `cpptlm-dgpu-board-soc-split`(上游消费者)
- **change D** = `cpptlm-dgpu-abi-export`(下游消费者)

### D5: 前置测试失败的 governance 处理

**任一 Tier 2 前置测试失败**:

1. 阻止 `cpptlm-dgpu-abi-export` change archive(违反前置 gate)
2. 必须 fix 失败 → 新增 OpenSpec change 或 patch 当前 change
3. 不允许"已知失败"状态进入 archive(per `cpptlm-dgpu-pcie-endpoint` 99b/85 教训)

## 3. Implementation Plan

### 3.1 OpenSpec change (5 任务)

```text
T-prereq-1: wire-format 快照测试 (P0-E)
T-prereq-2: DMA translate 边界测试 (P0-A)
T-prereq-3: MSI-X 状态机细粒度测试 (P1-C)
T-prereq-4: Doorbell 排队/并发测试 (P1-B)
T-prereq-5: backdoor ABI 隔离测试 (P2-D)
```

### 3.2 跨仓协调 (per D3)

- [ ] 提交本 ADR- SOC-08 后,创建 UsrLinuxEmu issue/ADR 提议(由本仓 owner 发起,UsrLinuxEmu owner 决策)
- [ ] UsrLinuxEmu ADR-089 §D7 增量(可选)
- [ ] UsrLinuxEmu ADR-088 §C2 增量补注(可选)
- [ ] 跨仓 commit 顺序(per UsrLinuxEmu ADR-035 §R5.1):CppTLM PR#1(本 ADR- + 前置测试) → UsrLinuxEmu PR#2(消费前置测试)

## 4. Consequences

### 正面
1. **UsrLinuxEmu 端到端测试 surprise rate 显著降低**(per §1.2 6 类 gap 填补)
2. **跨仓契约字节级快照测试**(防止 silent drift,per ADR-090 v2 §C0 教训)
3. **本仓测试覆盖从 18 用例提升到 ~33-38 用例**
4. **为 `cpptlm-dgpu-abi-export` 提供前置 gate**

### 负面
1. **3-4 天工作量**(per D2 工作量估算)
2. **跨仓协调开销**(本 ADR- + UsrLinuxEmu issue/PR)

## 5. Migration / Risks

| 风险 | 概率 | 缓解 |
|------|-----|------|
| wire-format 快照测试发现 UsrLinuxEmu 解析偏差 | 低 | 阻断 abi-export,发起新 OpenSpec change 修复 |
| DMA translate 边界测试发现 SdmaEngineTLM 现有逻辑缺陷 | 中 | patch sdma-engine change(仍可 archive 状态) |
| MSI-X 状态机细粒度测试暴露性能问题 | 低 | 性能问题暂不阻塞 archive |
| UsrLinuxEmu owner 不采纳本 ADR 增量提议 | 中 | 本仓前置测试独立交付,不影响 UsrLinuxEmu 实施 |

## 6. Open Questions

1. **本仓测试是否需要 exfiltrate 给 UsrLinuxEmu?** — 提议:在 `cpptlm-dgpu-abi-export` change archive 前,生成 `wire-format-snapshot.tar.gz` 作为 release attachment(per abi-export spec)
2. **wire-format 快照是否纳入 CI gate?** — 提议:对每次 PR,自动运行 `static_assert(sizeof(PcieTlpBundle) == ...)` 防止字段重排

## 7. References

- 跨仓 OpenSpec (UsrLinuxEmu 消费侧):
  - `openspec/changes/cpptlm-dgpu-board-soc-split/` (Board shell + SOC JSON)
  - `openspec/changes/cpptlm-dgpu-abi-export/` (23 ABI SHARED lib)
- 本仓 OpenSpec (本 ADR- 关联):
  - `openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/` (PcieEndpointTLM)
  - `openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/` (SdmaEngineTLM)
  - `openspec/changes/2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/` (本 ADR 实施载体)
- UsrLinuxEmu 文档:
  - ADR-088 §C2 (23 ABI 外部契约)
  - ADR-089 v0.5 (v5.5+ 系统级硬件仿真)
  - ADR-090 v2 (PTX-EMU submodule + Canonical 真相源)

---

**起草**: Sisyphus (2026-08-28, per ADR-088 §C2 + ADR-089 v0.5 评估)
**Owner**: CppTLM Team
**送审**: UsrLinuxEmu Architecture Team(供 ADR-089 / ADR-088 增量修订评估)
**状态**: 📋 Proposed — 待评审 + OpenSpec change 实施

---

## Status Update

- **2027-02-09**: 5 项前置测试当前状态（全部已实施，per Phase 8 整合交付）：

  | 测试 | 状态 | 测试文件 |
  |------|:----:|---------|
  | **P1 MSI-X 状态机细粒度（mask/unmask/PBA）** | ✅ 已实施 | `test/test_pcie_endpoint_msix_state.cc` |
  | **P2 wire-format 跨仓字节级一致性** | ✅ 已实施 | `test/test_pcie_slice_wire_format_snapshot.cc` |
  | **P3 DMA translate callback 边界（phys 越界）** | ✅ 已实施 | `test/test_sdma_engine_iommu_fault.cc` |
  | **P4 Doorbell 排队/并发** | ✅ 已实施 | `test/test_pcie_endpoint_doorbell_queue.cc` |
  | **P5 backdoor ABI 隔离（MMIO 与 backdoor 路径）** | ✅ 已实施 | `test/test_sdma_engine_backdoor_isolation.cc` |

- **2027-02-09**: 4 项 UsrLinuxEmu 跨仓提议反馈状态（全部待回应）：
  - 1 ADR-089 §D7 IOMMUFD consumer 追加 CppTLM 跨仓契约验证段：**待 UsrLinuxEmu 回应**
  - 2 ADR-089 §D7 VFIO consumer BAR0 寄存器行为契约来源：**待 UsrLinuxEmu 回应**
  - 3 ADR-088 §C2 CppTLM C ABI 范围扩展：**待 UsrLinuxEmu 回应**
  - 4 ADR-089 v0.5 更新到 v5.5+ 节奏：**待 UsrLinuxEmu 回应**

- **2027-02-09**: 仓内 23 ABI 实现进展（per [`00-overview.md`](../architecture/00-overview.md) §4-bis R26/R27 + [`ADR-SOC-14-v55-integration-revision.md`](./ADR-SOC-14-v55-integration-revision.md)）：
  - **头冻结**：✅ 19 声明（per `include/abi/cpptlm_emulator.h`）
  - **仓内实现**：✅ 19/19 函数 + 4 回调 typedef 契约（per `src/abi/cpptlm_emulator.cc` 433 行）
  - **UsrLinuxEmu 侧集成与完整 23 函数闭环**：❌ 待 UsrLinuxEmu v5.5+ 节奏（per UsrLinuxEmu ADR-089 v0.5）

- **2027-02-09**: v1.0 战略下双 vendor 跨仓依赖追加（per [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](./ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D3）：UsrLinuxEmu ADR-088 当前 NVIDIA-only，AMD KFD 路径需 UsrLinuxEmu owner 联签/分阶段实施