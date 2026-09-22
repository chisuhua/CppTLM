# ADR-SOC-23: NSA-aware 分布式 Scale-Up vs 方案 B 选型决策

> **状态**: 📋 Proposed — 2026-09-19
> **日期**: 2026-09-19
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: NSA-aware Scale-Up 演进路径与降级替代选型
> **类别**: SoC 架构 / NSA-aware 演进选型
> **关联文档**:
> - [`docs/soc_arch/architecture/23-dist-scale-up-topology.md`](../architecture/23-dist-scale-up-topology.md) NSA-aware SoC 拓扑 (主选)
> - [`docs/soc_arch/architecture/21-dist-scale-up-topology-b.md`](../architecture/21-dist-scale-up-topology-b.md) 方案 B 传统分布式 (降级)
> - [`docs/soc_arch/architecture/29-nsa-evolution-roadmap.md`](../architecture/29-nsa-evolution-roadmap.md) 5 阶段演进
> - **关联 ADR**: ADR-SOC-22 (NSA Fabric Address, 保留 NSA 族名), ADR-SOC-21 (V3.1-Rev2.0)
> **关联 OpenSpec**: [`openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`](../../../openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/proposal.md)

---

## 1. Context (背景)

### 1.1 三方案对比 (V3.1-Rev2.0 已 ship + NSA-aware 草案)

| 维度 | **方案 A** (V3.1-Rev2.0) | **方案 B** (草案 B) | **方案 C** (NSA-aware) |
|------|---------------------|---------------------|---------------------|
| **架构** | 1 Linux 节点 + 1 PCIe Hierarchy | N+1 Linux 节点 + N PCIe Hierarchy | N+1 Linux 节点 + NSA Switch |
| **NSA-aware 硬件** | ❌ | ❌ | ✅ Remote Atomic + Directory |
| **跨 tray 延时** | N/A (单节点) | 1.8 μs (PCIe over UALink 软件) | 500 ns (NSA 硬件加速) |
| **故障域** | 1 个 (单点) | N 个 (按 Compute Tray) | N 个 (按 Compute Tray) |
| **多租户隔离** | MIG 软件 | SR-IOV (跨节点不成熟) | Capability 硬件强隔离 |
| **量产时间** | 现在 ✅ | 2026-2027 ⚠️ | 2027+ ⚠️ |

### 1.2 选型决策必要性

NSA-aware 草案体系 (`21-dist-scale-up-topology-b.md` + `22-29` 号) 定义了方案 B 和方案 C, 但**未明确**:
- 主选方案: B vs C
- 降级触发条件
- 商业化风险 vs 性能收益权衡
- 量产化时间窗与 CXL 3.0 量产的对齐

需要 ADR 决策, 否则 8 份 NSA 草案无法形成统一演进路径。

### 1.3 关键约束

- ✅ **V3.1-Rev2.0 兼容**: 任何方案必须保留 V3.1-Rev2.0 9 份已 ship 文档
- ✅ **方案 B 作为降级替代**: 方案 C 硬件不就绪时, 必须能降级到 B
- ✅ **NSA Switch 量产依赖 CXL 3.0**: 2025-2027 量产时间窗
- ✅ **0 个新 ABI 函数**: per ADR-088 §D5

---

## 2. Decision (决策)

### D1. 主选方案 C (NSA-aware 分布式 Scale-Up)

**决策**: NSA-aware Scale-Up 的**主选方案是方案 C** (NSA-aware 分布式), 不选方案 B (传统分布式)。

**理由**:
1. **NSA-aware 是业界方向**: NVIDIA NVL72 / AMD MI300X CXD / Intel Xe-HPC + CXL 3.0 都在向 NSA-aware 演进
2. **方案 C 是 NSA Phase 3 终态**: Remote Atomic + Hardware Directory + Capability 完整体系
3. **跨 tray 性能**: ~500 ns (NSA 硬件加速) vs ~1.8 μs (方案 B 软件协议栈)
4. **多租户硬件隔离**: Capability 强隔离 vs MIG 软件切片

### D2. 方案 B 作为降级替代 (非替代方案)

**决策**: 方案 B 不是 NSA-aware 的**替代**, 而是 NSA-aware 硬件**不就绪时的降级路径**。

**理由**:
1. **降级触发条件**: NSA 硬件 (Fabric ID / Remote Atomic / Directory) 量产延期 > 6 个月
2. **降级步骤**: Compute Tray 移除独立 CPU (合并到 Host Tray) → Switch Tray 独立 Linux FM 协调 PCIe over UALink 软件协议栈
3. **降级时延**: 跨 tray ~1.8 μs (vs NSA-aware ~500 ns, 性能损失 3.6×)
4. **降级不影响多租户**: Capability 可在方案 B 软件层实现 (性能损失, 但功能保留)

### D3. 5 阶段演进路径 (per `29-nsa-evolution-roadmap.md`)

**决策**: NSA-aware Scale-Up 5 阶段演进, 与 V3.1-Rev2.0 4 份 Roadmap 对齐:

| NSA 阶段 | 时间 | 内容 | 文档 |
|---------|------|------|------|
| NSA Stage 0 (已 ship) | 2024-2026 | V3.1-Rev2.0 (方案 A) | 9 份已 ship |
| NSA Stage 1 | 2026-2027 | NSA-aware MMU + GSP-RM (软件层) | 草案 1 + 4 + 5 + 6 |
| NSA Stage 2 | 2027-2028 | NSA-aware 硬件 + 分布式 Scale-Up (主选方案 C) | 草案 2 + 7 |
| NSA Stage 3 | 2028-2029 | CXL 3.0 Fabric 完整 | 草案 7 + 28-cxl-3-fabric |
| NSA Stage 4 | 2029+ | 商业化 | 草案 8 终态 |

### D4. 降级路径触发条件与步骤

**决策**: NSA Stage 2 硬件延期 > 6 个月时, 触发降级到方案 B (per `21-dist-scale-up-topology-b.md` §5):

```
触发条件 (任一):
  - NSA Switch 量产延期 > 2027-06 (NSA Stage 2 计划时间)
  - Remote Atomic Unit 量产失败
  - Hardware Directory 良率 < 80%
  - CXL 3.0 Fabric Switch 量产延期

降级步骤:
  1. 保留 Compute Tray 独立 CPU + Linux OS (方案 B 基础)
  2. Switch Tray 独立 Linux FM 协调 PCIe over UALink (软件协议栈)
  3. Capability 切换到软件层 (性能损失 ~10×)
  4. HRT 维持 V3.1-Rev2.0 (Fabric ID = 0, 单 Compute Tray 内寻址)
  5. 文档降级状态: per `21-dist-scale-up-topology-b.md` §5.2

降级时延影响:
  - 跨 tray GPU↔GPU: 1.8 μs (vs NSA-aware 0.5 μs, 性能损失 3.6×)
  - Capability 校验: ~50 ns (软件) vs ~2 ns (NSA-aware 硬件)
  - 总性能影响: ~30% 退化 (但功能完整)
```

---

## 3. Consequences (影响)

### 3.1 正面影响

1. **NSA-aware 是演进方向**: 与 NVIDIA NVL72 / AMD MI300X CXD / Intel Xe-HPC 对齐
2. **跨 tray 性能**: ~500 ns (NSA 硬件加速) vs ~1.8 μs (方案 B)
3. **多租户硬件隔离**: Capability 强隔离 vs MIG 软件切片
4. **CXL 3.0 兼容**: NSA Fabric Address 与 CXL Fabric Address 字段对齐
5. **降级路径完整**: NSA 硬件延期时, 仍可出货 (方案 B)

### 3.2 负面影响

1. **量产化时间 2-3 年**: 方案 C 依赖 CXL 3.0 Fabric Switch 量产 (2025-2027)
2. **自研 NSA Switch 风险**: 需自研 GSP-RM 微控制器 + 硬件加速
3. **多 Linux 节点管理复杂**: 跨节点 GMMU Page Table 同步 / TLB Invalidate 协议复杂
4. **降级路径复杂度**: 方案 B + 方案 C 双轨维护

### 3.3 风险与缓解

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| CXL 3.0 量产延期 | 中 | 降级到方案 B (per `21-dist-scale-up-topology-b.md`) |
| NSA Switch 自研失败 | 中 | 复用商业 UALink Switch 芯片 (Microchip / Astera Labs) |
| 多 Linux 节点 KMD 协调失败 | 低 | 跨节点 PCIe over UALink 协议 + 分布式 KMD |
| 商业化与 NVIDIA 竞争劣势 | 中 | 开放标准 (CXL 3.0 + UALink) vs NVIDIA 闭源 NVLink |
| 客户不愿从 V3.1-Rev2.0 升级 | 低 | 兼容路径: V3.1-Rev2.0 → 方案 B → 方案 C 平滑升级 |

---

## 4. 5 阶段约束 (per `29-nsa-evolution-roadmap.md`)

### 4.1 NSA Stage 0 (V3.1-Rev2.0, 已 ship)

- 9 份架构文档已 ship, 5 大子系统基础功能
- GUPA 64-bit 路由域划分 (Fabric ID = 0)
- Oracle 评审 9.62/10 PASS

### 4.2 NSA Stage 1 (v1.x, 2026-2027)

- NSA-aware MMU / TLB (8-bit Fabric ID)
- GSP-RM 微控制器 + NV-RTOS (控制面下沉)
- Capability 多租户 (per `27-nsa-capability.md`)
- 薄 Host driver (5x LOC 减少)

### 4.3 NSA Stage 2 (v3.x, 2027-2028) — 方案 C 完整

- Remote Atomic Unit 硬件 (per `25-nsa-hardware.md` §3)
- Hardware Directory L1/L2/L3 (per `25-nsa-hardware.md` §4)
- 16-bit Fabric ID 完整启用 (CXL 3.0 兼容)
- Compute Tray 独立 CPU + Linux (per `21-dist-scale-up-topology-b.md` §2)
- NSA Switch (UALink + CXL Fabric 双角色, per `28-cxl-3-fabric.md`)
- PCIe over UALink 硬件加速

### 4.4 NSA Stage 3 (v3.x, 2028-2029) — CXL 3.0 Fabric 完整

- 跨数据中心 (Multi-Fabric, 65K 节点全局)
- CXL 3.0 Fabric 完整兼容
- 多租户 GPU 云 (Capability + CXL Fabric)

### 4.5 NSA Stage 4 (2029+) — 商业化

- 与 NVIDIA NVL72 / AMD MI300X 对标
- 商业化模型 (License / Open-source)
- 生态集成 (CUDA / ROCm / oneAPI)

---


> **NSA 命名含义澄清** (适用于所有 NSA 草案, 2026-09-19 决策):
>
> 本草案中 **NSA** 含义为 **"Network-System Architecture"** (多 Linux 节点 + 跨 Fabric 寻址架构), 与美国 **National Security Agency (国家安全局)** **无关**, 仅 CppTLM 内部使用。
>
> NSA 在本草案中衍生术语 (GFS Stage 1/2/3 命名规范保留):
>
> | NSA 衍生术语 | 含义 | 与 CXL 3.0 / NVLink / Infinity Fabric 对应 |
> |---|---|---|
> | NSA Switch | NSA Switch (跨 Fabric 路由器) | ≈ CXL Fabric Switch Tier 1 |
> | NSA Fabric Address | 64-bit NSA-aware 地址格式 (16+48 bits) | ≈ CXL Fabric Address |
> | NSA-aware MMU | 含 Fabric ID + Capability 的 MMU | ≈ CXL-aware IOMMU |
> | NSA Stage 1/2/3 | 5 阶段演进阶段 | ≈ CXL 3.0 Fabric 量产节奏 |
> | NSA-aware SoC | NSA-aware 分布式 SoC 终态 | ≈ CXL 3.0 Fabric-aware SoC |
>
> **替代命名参考** (若未来需替换): GFS (Global Fabric System) / FAS (Fabric Address Space) / UFA (Unified Fabric Address), **当前决策保持 NSA + 备注澄清**。
>
## 5. 关联文档与 ADR

### 5.1 上游依赖

- **ADR-SOC-22**: NSA-aware 64-bit Fabric Address 格式决策 (保留 NSA 族名)
- **ADR-SOC-21**: V3.1-Rev2.0 拓扑修正决策
- **V3.1-Rev2.0**: 9 份已 ship 架构文档

### 5.2 下游依赖

- **ADR-SOC-24**: Capability-Based 隔离选型决策
- [`23-dist-scale-up-topology.md`](../architecture/23-dist-scale-up-topology.md) NSA-aware SoC 拓扑 SSOT
- [`21-dist-scale-up-topology-b.md`](../architecture/21-dist-scale-up-topology-b.md) 方案 B 降级替代
- [`29-nsa-evolution-roadmap.md`](../architecture/29-nsa-evolution-roadmap.md) 5 阶段演进
- 8 份 NSA 草案 (草案 2-8)

### 5.3 关联 OpenSpec

- [`openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`](../../../openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/proposal.md)
- umbrella 提案涵盖 8 份 NSA 草案 + 3 份 ADR (ADR-SOC-22/23/24)

---

## 6. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: NSA-aware vs 方案 B 选型决策 (D1-D4 + 5 阶段约束 + 5 项风险缓解) |
| 2026-09-20 | v1.1-rev | Sisyphus | Oracle 评审修订: 命名空间 ADR-23 → ADR-SOC-23 + OpenSpec 链接修正 |
