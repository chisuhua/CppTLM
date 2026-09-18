# phase9-p4-axi-outbound-bridge: 通用 SoC AXI Master → PCIe Outbound 桥接缺口评估

> **类别**: SoC Architecture > Roadmap · **阶段**: phase9 P4 · **优先级**: 🟡 战略评估（**不在 v1.0 范围内**）
> **日期**: 2027-09-17 · **维护者**: Sisyphus · **跨仓**: 待定（若需求触发则需 Hub 协调）
> **关联 ADR**: [ADR-SOC-19-axi-master-outbound-bridge.md](../adr/ADR-SOC-19-axi-master-outbound-bridge.md) — 本阶段的 ADR 主文档
> **关联 OpenSpec**: 待创建（如触发实施）

**W 编号基准**: W1 = 2026-09-21(Mon) — 本目录所有阶段文件均按此基准；当前 W37+。

---

## 1. 目标

在 ADR-SOC-19 明确"通用 SoC AXI Master → PCIe Outbound 桥接**当前未提供**"的基础上，对该缺口做**未来可行性评估**：

- **三方案对比**：新增 `axi_to_pcie_bridge` / PcieRequesterEngine AXI4 化 / 维持现状 + 文档化
- **决策矩阵**：按需求触发概率 + ROI + 复杂度评分
- **触发条件定义**：明确何时升级到 P5 实施阶段

**预期产出**:
- 一份**不在 W42 内执行**的评估文件
- 三方案对比表 + ROI 评分
- 触发条件检查清单（trigger checklist）

---

## 2. 缺口定义（per ADR-SOC-19）

### 2.1 当前实现

| SoC 组件类型 | 实现方式 | 状态 |
|--------------|---------|:----:|
| SDMA (H2D / D2H) | C++ API 直调 `PcieRequesterEngine` | ✅ |
| MSI-X pending → MWr | C++ API 直调 `PcieRequesterEngine` | ✅ |
| PcieMockIP device-side | `device_axi_write()` 直接推 `Axi4StreamAdapter` | ✅ |
| **通用 SoC AXI Master** (CUDA core / GPC core / Display / 未来组件) | **❌ 无桥接** | ❌ |

### 2.2 缺口影响范围

如果将来需求触发以下场景，**当前架构无法支持**：

| 场景 | 当前状态 | 需求来源 |
|------|---------|---------|
| CUDA kernel pinned host memory 直接访问 | ❌ 不支持 | NVIDIA zero-copy 模型 |
| GPU zero-copy 渲染（host framebuffer 共享映射） | ❌ 不支持 | Display controller → host memory |
| User-mode driver GPU kernel 发起 PCIe MRd/MWr | ❌ 不支持 | amdgpu/nouveau pinned pages |
| Display controller 拉 host cursor / overlay | ❌ 不支持 | OS framebuffer → GPU display |

### 2.3 与 Host 侧 RC 对称性

| 位置 | 组件 | AXI↔PCIe 桥接 |
|------|------|:--------------:|
| Host 侧 | `PcieRootComplexTLM` (Phase 7) | ✅ 完整 |
| Device 侧 | `PcieEndpointIP` 当前 | ❌ **缺一半**（device→host 方向无标准 AXI Master 接口）|

---

## 3. 三方案对比

### 方案 A: 新增 `axi_to_pcie_bridge` 通用桥接组件

**思路**: 在 `PcieEndpointIP` 内（或作为独立组件）增加 AXI-to-PCIe bridge，把 `axi_slave_in` 接收到的、target=PCIe 地址空间 的事务自动转 PCIe TLP。

**实现位置**:
- `include/tlm/pcie/axi_to_pcie_bridge.{hh,cc}`（新文件）
- 与 `PcieAxiAdapter` 三端口的 `axi_slave_in` 协同

**优点**:
- ✅ 完整实现 AXI↔PCIe 双向桥接（与 Host RC 对称）
- ✅ 任何 SoC 标准 AXI Master 可通过该桥接访问 host memory
- ✅ 接口清晰（AXI4 标准，PCIe spec 符合）

**缺点**:
- ❌ 增加 PcieEndpointIP 复杂度（与 Phase 9+ 简化 ABI 方向冲突）
- ❌ 引入新错误模式（AXI↔PCIe outstanding 跟踪、FC 反压回退）
- ❌ 需要 ATS (Address Translation Service) 或 IOVA→PA 翻译（与现有 `cpptlm_dma_translate_cb` 集成）
- ❌ 工作量大（5-10 人天）

### 方案 B: PcieRequesterEngine 暴露为标准 AXI4 Master 接口

**思路**: 把 `PcieRequesterEngine` 重构为标准 AXI4 Master（AXI4StreamAdapter），而不是 C++ API；SDMA + MSI-X 仍调用，但其他 SoC Master 也可经同一 AXI Master 发起 MRd/MWr。

**优点**:
- ✅ 复用现有 `PcieRequesterEngine` 实现（增量改动小）
- ✅ 接口与 Host 侧 RC 对称（AXI4 标准）
- ✅ 不增加新错误模式（outstanding/FC 已在 RequesterEngine）

**缺点**:
- ❌ SDMA 当前 `set_request_engine()` C++ API 需要重写为 AXI4 Master 客户端
- ❌ 与 PcieMockIP `device_axi_write()` 方向混淆（Mock 是单向，AXI 是双向）
- ❌ PcieRequesterEngine 当前直接调 `link_layer_->tx_tlp()`，跳过 AXI Mapper，标准化会引入额外 outstanding 跟踪
- ❌ 工作量 3-5 人天

### 方案 C: 维持现状 + 文档化（当前 ADR-SOC-19 决策）

**思路**: 当前 SDMA + MSI-X 已闭环，**不实现通用 AXI outbound**；文档化缺口，作为 Phase 10+ 评估项。

**优点**:
- ✅ 零工作量
- ✅ 不增加 PcieEndpointIP 复杂度
- ✅ 与 Phase 9+ ABI 简化方向一致

**缺点**:
- ❌ 未来需求触发时需临时返工
- ❌ 与 Host 侧 RC 长期不对称（架构不平衡）

---

## 4. 决策矩阵

| 维度 | 方案 A (新桥接) | 方案 B (AXI4 化) | 方案 C (现状 + 文档) |
|------|:---------------:|:----------------:|:--------------------:|
| **实现工作量** | 5-10 人天 | 3-5 人天 | 0 人天 |
| **架构复杂度** | 🟡 +1 组件 | 🟢 复用现有 | 🟢 不变 |
| **接口对称性** | ✅ 与 RC 对称 | ✅ 与 RC 对称 | ❌ 不对称 |
| **未来扩展性** | ✅ 全通用 | ⚠️ 半通用（Master-only）| ❌ 不通用 |
| **回归风险** | 🔴 高（新组件 + outstanding） | 🟡 中（重构 RequesterEngine）| 🟢 零 |
| **跨仓需求** | ❌ 无 | ❌ 无 | ❌ 无 |
| **可逆性** | 🟡 中（需保留旧路径） | 🟢 易（仅 API 包装） | 🟢 N/A |
| **当前需求 ROI** | 🟢 低 | 🟢 低 | 🟢 **最高** |
| **未来需求 ROI** | 🟢 高 | 🟡 中 | ❌ 零 |
| **综合评分 (W=权重, S=1-5)** | Σ=22 | Σ=18 | **Σ=24** |

**结论**: 当前选 C（维持现状 + 文档化）。若 Phase 10+ 触发需求，重新评估 A vs B。

---

## 5. 触发条件检查清单（Phase 10+ 升级标准）

只有以下**任意一项**触发时，本阶段升级为 P5 实施阶段：

| 触发条件 | 检测信号 | 优先级 |
|---------|---------|:------:|
| **CUDA zero-copy 需求** | 用户明确要求 `cudaMemcpy` 访问 pinned host memory | P0 |
| **Display controller 拉 host buffer** | 显示子系统需要从 host 内存取 framebuffer | P0 |
| **GPU user-mode driver 需求** | amdgpu/nouveau 提交需要 GPU kernel 直访 host memory | P0 |
| **PcieMockIP 通用化** | 用户希望 Mock profile 支持通用 SoC Master 发起 host 访问 | P1 |
| **Host 侧 RC 镜像需求** | RC 端要求 EP 端实现对称 AXI-to-PCIe bridge | P1 |
| **ATS (Address Translation Service)** | 大规模 PASID-tagged 事务需求出现 | P2 |

**当前状态**: 0 项触发，方案 C 持续有效。

---

## 6. Phase 10+ 实施预估（仅在触发后启动）

### 6.1 方案 A 实施预估

```
Phase 10-W1: axi_to_pcie_bridge 设计 (OpenSpec proposal)
Phase 10-W2-W3: 实施 + 单元测试
Phase 10-W4: 集成测试 (SDMA + MSI-X + 通用 AXI Master)
Phase 10-W5: 跨仓协调 (若 Hub 侧需联动)
Phase 10-W6: Oracle 评审 + archive

总计: 5-6 周 (含 Oracle 评审 1-2 周)
```

### 6.2 方案 B 实施预估

```
Phase 10-W1: PcieRequesterEngine 重构设计 (OpenSpec proposal)
Phase 10-W2: SDMA 客户端改造为 AXI4 Master
Phase 10-W3: MSI-X 投递链 AXI4 化
Phase 10-W4: 集成测试 + 回归
Phase 10-W5: Oracle 评审 + archive

总计: 4-5 周
```

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| 需求触发时未及时升级到 P5 | 🟡 中 | 临时返工 | 本文件 + ADR-SOC-19 双轨记录触发条件 |
| 方案 A 实现引入新错误模式（outstanding/FC） | 🔴 高 | 回归 | 实施时强制 Phase 5-style 测试覆盖 + Oracle 评审 |
| 方案 B 重构 RequesterEngine 破坏现有 SDMA / MSI-X 集成 | 🟡 中 | SDMA H2D 链路断裂 | 保留 C++ API 兼容层，新 AXI4 接口为可选 |
| 与 Host 侧 RC 不对称导致 driver 设计复杂 | 🟡 中 | 跨仓影响 | ADR-088 §D3.8 与 Hub 协调（若触发） |

---

## 8. 跨仓协调事项

**当前 (W42 内)**: 无跨仓协调需求（方案 C 不触发新接口）

**未来 (Phase 10+ 触发后)**:
- Hub 侧 (UsrLinuxEmu) 协调新 ABI（如 `cpptlm_emulator_register_axi_outbound_cb`）以支持标准 AXI Master 暴露
- 跨仓 HSK-12 创建（待方案选定后）
- 协调节奏：W1 提案 → W3 Oracle 评审 → W5 双仓并行实施 → W6 集成测试

---

## 9. 与其他 Phase 阶段关系

```
phase9-p0  ──→ p1  ──→ p2  ──→ p3  ──→ p4 (本文件)
                ↓       ↓
           SM Gate  CP 接入  ADR 修订  战略评估
                          (本期)
                                │
                                ↓
                          Phase 10+ 启动
                          (若 P4 触发条件成立)
```

- **P3 (战略层 ADR 修订)**: 与本 P4 互补——P3 是**已实施**主题的 ADR 修订，B3-B4 + B5'-B8'
- **P4 (本文件)**: 是**未实施**架构缺口的评估，对应 ADR-SOC-19
- **Phase 10**: 是 v1.0 后的真正实施阶段，可能包含本 P4 触发的方案 A/B 实施

---

## 10. 维护纪律

- **不在 W42 内执行**——这是评估文件，不是任务清单
- **每季度 review**：检查 §5 触发条件
- **触发后升级**：本文件归档为 `phase9-p4-axi-outbound-bridge-COMPLETED.md`，创建 `phase10-p1-axi-outbound-bridge.md` 实施文件
- **跨仓通知**：若触发，HSK-12 创建并同步到 UE 仓

---

## 11. 关联文档

- **ADR 主文档**: [ADR-SOC-19](../adr/ADR-SOC-19-axi-master-outbound-bridge.md)
- **顶层总览**: [phase9-post-phase8-roadmap.md](./phase9-post-phase8-roadmap.md)
- **模块文档**: [`dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md) §"AXI 接口语义矩阵"
- **PCIe EP 微架构**: [`docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`](../../architecture/19-pcie-ip-microarchitecture.md) §Phase 9+
- **父 OpenSpec**: [archive/2026-09-16-cpptlm-pcie-tlp-wire-datapath/](../../../openspec/changes/archive/2026-09-16-2026-09-16-cpptlm-pcie-tlp-wire-datapath/)

---

## 12. 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Strategic Assessment — 等待触发条件满足后升级为实施阶段