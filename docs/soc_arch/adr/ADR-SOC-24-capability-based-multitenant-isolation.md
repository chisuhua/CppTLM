# ADR-SOC-24: Capability-Based 多租户隔离选型决策

> **状态**: 📋 Proposed — 2026-09-19
> **日期**: 2026-09-19
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: NSA-aware 分布式 Scale-Up 多租户隔离机制选型
> **类别**: SoC 架构 / 多租户隔离
> **关联文档**:
> - [`docs/soc_arch/architecture/27-nsa-capability.md`](../architecture/27-nsa-capability.md) Capability-Based 多租户隔离 SSOT
> - [`docs/soc_arch/architecture/25-nsa-hardware.md`](../architecture/25-nsa-hardware.md) NSA-aware MMU Capability 校验
> - [`docs/soc_arch/architecture/26-gsp-rm-firmware.md`](../architecture/26-gsp-rm-firmware.md) GSP-RM Tenant Manager
> - **关联 ADR**: ADR-SOC-22 (NSA Fabric Address), ADR-SOC-23 (NSA-aware vs 方案 B), ADR-SOC-21 (V3.1-Rev2.0)
> **关联 OpenSpec**: [`openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`](../../../openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/proposal.md)

---

## 1. Context (背景)

### 1.1 多租户隔离的必要性

V3.1-Rev2.0 (`21-tee-udd-mvp.md` §3.4) 的 RCT (Routing Context Table) 是**软件维护的 Base/Limit 校验**, 多租户隔离**依赖软件信任链**, 不是硬件强制。

NSA-aware Scale-Up (方案 C) 需要**硬件强制**的多租户隔离, 因为:
- 多 Compute Tray + 多 Linux 节点: 攻击面扩大
- 跨 Fabric 寻址: 越权访问可能跨 Compute Tray 边界
- Host FM 失联 / 恶意 Host OS: 软件隔离不可靠

### 1.2 业界多租户隔离方案对比

| 方案 | 厂商 | 隔离粒度 | 硬件强制 | 跨 Fabric |
|------|------|---------|---------|----------|
| **MIG** | NVIDIA (Ampere+) | 硬件切片 (SM/HBM) | ✅ | ❌ 本地 PCIe |
| **SR-IOV** | 业界标准 (PCI-SIG) | VF 资源 | ✅ | ⚠️ 跨节点不成熟 |
| **CXD** | AMD (MI300X+) | MIG 类似 | ✅ | ⚠️ 跨节点不成熟 |
| **Intel SGX/CCA** | Intel | 飞地 (Enclave) | ✅ | ⚠️ CPU 中心 |
| **CHERI** | Cambridge/ARM | Capability-Based | ✅ | ✅ 任意处理器 |
| **seL4** | NICTA/DARPA | Capability-Based | ✅ (形式化验证) | ✅ 微内核 |
| **NSA-Capability** (本 ADR) | CppTLM | Capability-Based + HW 校验 | ✅ | ✅ 跨 Fabric (本 ADR 决策) |

### 1.3 关键约束

- ✅ **V3.1-Rev2.0 兼容**: Capability 须兼容 RCT (软件 Base/Limit)
- ✅ **HW 强制**: 每次 MMU Lookup 自动校验, 零软件开销
- ✅ **跨 Fabric 传递**: Capability 含 Fabric Address (per `22-nsa-fabric-address-spec.md`)
- ✅ **NSA-aware MMU 协同**: HW Capability 校验集成在 MMU Lookup 路径 (per `25-nsa-hardware.md` §0.5.3 Oracle C5 修正)
- ✅ **0 个新 ABI 函数**: per ADR-088 §D5

---

## 2. Decision (决策)

### D1. 采用 Capability-Based 多租户隔离 (NSA-Capability)

**决策**: NSA-aware 分布式 Scale-Up 采用 **Capability-Based 多租户硬件强隔离** (借鉴 CHERI + seL4 Capability 思想, 简称 NSA-Capability), 替代 MIG/SR-IOV/CXD 软件切片。

**理由**:
1. **HW 强制**: 每次 MMU Lookup 集成校验, 零软件开销 (per `25-nsa-hardware.md` §0.5.3)
2. **跨 Fabric 传递**: Capability 含 Fabric Address Base, 跨 Compute Tray 自动校验
3. **正交互补 MIG**: MIG 资源空间切分 + Capability 地址访问权, 两者协同 (而非替代)
4. **形式化可验证**: 借鉴 seL4 Capability 思想, 关键路径可形式化验证
5. **CXL 3.0 兼容**: Capability Token 与 CXL Security Protocol 协同

### D2. NSA-Capability 与 MIG 正交互补 (非替代)

**决策**: NSA-Capability 不替代 MIG, 而是与 MIG **正交互补**:
- **MIG**: GPU 资源空间切分 (per-Instance 独立 SM/HBM/带宽)
- **NSA-Capability**: GPU 地址访问权 (per-Tenant Capability Token)
- 两者协同: MIG 实例内 + Capability 隔离 = 二维隔离矩阵

**理由**:
1. **MIG 限本地 PCIe Hierarchy**: 跨 Compute Tray MIG 不可用 (per 业界现状)
2. **Capability 跨 Fabric 传递**: NSA-aware Scale-Up 跨 Compute Tray 需 Capability 扩展
3. **细粒度叠加**: MIG 实例内可进一步用 Capability 隔离地址空间

### D3. Capability Token 128 bits 格式 (per `27-nsa-capability.md` §2)

**决策**: Capability Token 128 bits:

```
[127:96] Fabric Addr Base (32 bits, 含 Fabric ID 8 bits + Local Addr 24 bits)
[95:64]  Length (32 bits)
[63:48]  Tenant ID (16 bits, 256 租户)
[47:32]  Permissions (16 bits: R/W/X/U/S/DMA/Peer/Atomic)
[31:16]  Object Type (16 bits: Memory/Queue/Interrupt/DMA/Mailbox)
[15:8]   Flags (8 bits: Read-Only/Execute-Only/Cacheable/etc.)
[7:0]    Version + Checksum (8 bits)
```

**理由**:
1. **128 bits 紧凑**: 4 个 32 bits 字, 与 ARM CHERI (128 bits) 对齐
2. **跨 Fabric 支持**: Fabric Addr Base 含 Fabric ID (8 bits Stage 2, 16 bits Stage 3)
3. **多租户隔离**: Tenant ID 16 bits 支持 256 硬件强隔离租户

### D4. NSA-aware MMU HW 校验 (per `25-nsa-hardware.md` §0.5.3)

**决策**: Capability HW 校验在 MMU Lookup 路径**并行执行** (Oracle C5 修正), 不串行:

```
NSA-aware TLB Lookup (并行):
  - Tag 比较 (~3 cycles)
  - Capability 校验 (~5 cycles, 并行)
  - RCT 校验 (~10 cycles, Global UDD Hub 异步)
  
总关键路径: max(Tag 比较, Capability 校验) ≈ ~5 cycles @ clk_core = ~2.5 ns
```

**理由**:
1. **零关键路径延迟**: Capability 校验与 TLB Lookup 并行, 不增加关键路径
2. **HW 强制**: 每次 MMU Lookup 自动校验, 无软件介入
3. **Capability Handle 优化**: TLB Entry 仅存 8-bit Capability Handle (per `27-nsa-capability.md` §4.3), 不存完整 128 bits Token

### D5. NSA-Capability 与 V3.1-Rev2.0 RCT 兼容

**决策**: NSA Stage 1 (v1.x) 期间 RCT 与 Capability **双层校验** (粗粒度 RCT + 细粒度 Capability), NSA Stage 3 (v3.x) **Capability 完全替代 RCT**。

**理由**:
1. **演进无债务**: Stage 1 双层校验 (粗+细), Stage 3 仅 Capability (细粒度)
2. **向后兼容**: V3.1-Rev2.0 RCT 软件维护路径不变, 仅新增 HW Capability 校验
3. **Capability 覆盖 RCT**: Stage 3 Capability Token 含 Fabric Addr Base + Length, 等价 RCT Base/Limit

### D6. NSA-Capability 生命周期 (per `27-nsa-capability.md` §3)

**决策**: Capability 5 阶段生命周期:
1. 请求 (Host Driver → FM, ~10 μs)
2. 验证 (FM 配额检查, ~50 μs)
3. 签发 (FM → GSP-RM Tenant Manager, ~1 μs)
4. 使用 (SM Load/Store → NSA-aware MMU 校验, 零开销)
5. 撤销 (FM 主动 / 自动失效)

---

## 3. Consequences (影响)

### 3.1 正面影响

1. **HW 强制多租户隔离**: 每次 MMU Lookup 自动校验, 零软件开销
2. **跨 Fabric 隔离**: Capability 含 Fabric Address, 跨 Compute Tray 自动校验
3. **正交互补 MIG**: MIG 资源切分 + Capability 地址权 = 二维隔离矩阵
4. **形式化可验证**: 借鉴 seL4 Capability, 关键路径可形式化验证
5. **CXL 3.0 兼容**: Capability Token 与 CXL Security Protocol 协同
6. **兼容 V3.1-Rev2.0**: Stage 1 双层校验 (RCT + Capability), Stage 3 替代

### 3.2 负面影响

1. **Capability Token 128 bits 较大**: TLB Entry 仅存 8-bit Handle, 完整 Token 在 Capability Database
2. **HW 校验逻辑复杂**: NSA-aware MMU 需扩展 Capability 校验单元 (~5 cycles)
3. **Capability Database 容量**: per Tenant 1 MB SRAM (Capability Token 池)
4. **Capability 撤销 race condition**: in-flight 事务的 Capability 撤销需原子性

### 3.3 风险与缓解

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| Capability 撤销 race condition | 中 | Capability Token Version 字段 + TLB Flush 原子操作 |
| HW 校验逻辑开销 | 低 | 并行执行 (与 TLB Lookup), 关键路径仅 ~2.5 ns |
| Capability Database 容量 | 低 | per Tenant 1 MB 可接受 (Stage 2 启用) |
| 跨节点 Capability 共享 | 中 | Stage 3 启用 16-bit Fabric ID + Capability 远程查询 |
| 与 MIG 协同路径 | 低 | MIG 实例内 + Capability 隔离 = 正交互补 (per §D2) |

---

## 4. 5 阶段约束

### 4.1 NSA Stage 0 (V3.1-Rev2.0, 已 ship)

- 仅 RCT (软件维护 Base/Limit)
- 无 NSA-Capability
- MIG 硬件切片 (NVIDIA 业界参考)

### 4.2 NSA Stage 1 (v1.x, 2026-2027)

- **双层校验**: RCT (粗粒度, 软件) + NSA-Capability (细粒度, HW)
- Capability Token 128 bits 启用 (per `27-nsa-capability.md` §2)
- HW Capability 校验集成在 NSA-aware MMU (per `25-nsa-hardware.md` §0.5.3)
- 0 个新 ABI 函数 (per ADR-088 §D5)

### 4.3 NSA Stage 2 (v3.x, 2027-2028)

- Capability 完全替代 RCT (Stage 3 终态)
- 跨 Fabric Capability 传递 (per `28-cxl-3-fabric.md`)
- Capability 远程查询 (经 NSA Hardware Directory)
- 多租户 GPU 云 (Capability + MIG 协同)

### 4.4 NSA Stage 3 (v3.x, 2028-2029)

- 跨数据中心 Capability 共享 (Multi-Fabric)
- Capability + CXL 3.0 Security Protocol 协同

### 4.5 NSA Stage 4 (2029+) — 商业化

- 与 NVIDIA MIG / AMD CXD Capability 互操作
- 商业化模型 (License / Open-source)

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

- **ADR-SOC-22**: NSA-aware 64-bit Fabric Address 格式决策 (Capability 含 Fabric Addr)
- **ADR-SOC-23**: NSA-aware vs 方案 B 选型决策 (Capability 跨 Fabric 必需)
- **ADR-SOC-21**: V3.1-Rev2.0 RCT 基础

### 5.2 下游依赖

- [`27-nsa-capability.md`](../architecture/27-nsa-capability.md) NSA-Capability SSOT
- [`25-nsa-hardware.md`](../architecture/25-nsa-hardware.md) NSA-aware MMU HW Capability 校验
- [`26-gsp-rm-firmware.md`](../architecture/26-gsp-rm-firmware.md) GSP-RM Tenant Manager
- [`28-cxl-3-fabric.md`](../architecture/28-cxl-3-fabric.md) 跨 Fabric Capability 传递
- [`29-nsa-evolution-roadmap.md`](../architecture/29-nsa-evolution-roadmap.md) 5 阶段演进

### 5.3 关联 OpenSpec

- [`openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`](../../../openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/proposal.md)
- umbrella 提案涵盖 8 份 NSA 草案 + 3 份 ADR (ADR-SOC-22/23/24)

---

## 6. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: Capability-Based 多租户隔离选型决策 (D1-D6 + 5 阶段约束 + 5 项风险缓解) |
| 2026-09-20 | v1.1-rev | Sisyphus | Oracle 评审修订: 命名空间 ADR-24 → ADR-SOC-24 + OpenSpec 链接修正 |
