# ADR-SOC-22: NSA-aware 64-bit Fabric Address 格式决策 (保留 NSA 族名)

> **状态**: 📋 Proposed — 2026-09-19
> **日期**: 2026-09-19
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: NSA-aware 分布式 Scale-Up 草案体系 (8 份草案) 的地址格式基础
> **类别**: SoC 架构 / NSA-aware 地址空间
> **关联文档**:
> - [`docs/soc_arch/architecture/22-nsa-fabric-address-spec.md`](../architecture/22-nsa-fabric-address-spec.md) NSA-aware Fabric Address 规范 (SSOT)
> - [`21-tee-udd-mvp.md`](../architecture/21-tee-udd-mvp.md) §4.0 GUPA 划分 (V3.1-Rev2.0 兼容基础)
> - [`21-microarch-ifc-mvp.md`](../architecture/21-microarch-ifc-mvp.md) §3.4 GPC UDD Agent HRT
> - **关联 ADR**: ADR-SOC-21 (V3.1-Rev2.0 拓扑修正)
> **关联 OpenSpec**: [`openspec/changes/2026-09-2x-cpptlm-nsa-scale-up-umbrella/`](../../../openspec/changes/2026-09-2x-cpptlm-nsa-scale-up-umbrella/proposal.md) (待创建)

---

## 1. Context (背景)

### 1.1 NSA-aware 分布式 Scale-Up 的地址空间需求

V3.1-Rev2.0 (`21-soc-topology-mvp.md` v3.1) 是 **1 个 Linux 节点 + 1 个 PCIe Hierarchy** 架构, GUPA 64-bit 路由域划分 (per `21-tee-udd-mvp.md` §4.0) 仅覆盖**本地寻址**。

但 NSA-aware 分布式 Scale-Up (方案 C) 需要:
- 多 Compute Tray (每 Compute Tray 独立 CPU + Linux OS)
- 跨 Compute Tray 通过 NSA Switch + PCIe over UALink 通信
- 跨 Fabric 寻址 (Fabric ID 标识 Compute Tray)
- 与 CXL 3.0 Fabric Address 兼容

需要定义 NSA-aware 64-bit Fabric Address 格式, 作为 9 份 NSA 草案的**地址格式基础**。

### 1.2 业界相关地址格式

| 方案 | 厂商 | 64-bit 地址格式 | 备注 |
|------|------|-----------------|------|
| GUPA (V3.1-Rev2.0) | CppTLM 自创 | [Fabric Domain(16b)][Local Routing(32b)][Local Addr(16b)] | 仅本地寻址 |
| **NSA-aware Fabric Address** (草案 1) | CppTLM (本 ADR) | **[Fabric ID(16b)][Local Addr(48b)]** | **本 ADR 决策** |
| **CXL 3.0 Fabric Address** | CXL 3.0 规范 | [Fabric Port ID(16b)][Device PA(48b)] | ✅ 业界标准 |
| NVLink Fabric Address | NVIDIA (Blackwell) | 闭源, 类似概念 | ⚠️ 私有 |
| Infinity Fabric ID | AMD (MI300X) | 类似 Fabric ID | ⚠️ 私有 |

### 1.3 关键约束

- ✅ **CXL 3.0 兼容**: NSA Fabric ID (16b) 必须与 CXL Fabric Port ID (16b) 字段对齐
- ✅ **V3.1-Rev2.0 兼容**: Stage 1 Fabric ID = 0 是默认状态 (无 NSA-aware)
- ✅ **无新 ABI 函数**: per ADR-088 §D5 (NSA-aware 是 RTL/硬件层)
- ✅ **演进无债务**: 8-bit Fabric ID (Stage 1) → 16-bit (Stage 3), 不重构数据结构

---

## 2. Decision (决策)

### D1. 采用 64-bit Fabric Address = [Fabric ID(16b)][Local Addr(48b)] 格式

**决策**: NSA-aware 分布式 Scale-Up 采用 **CXL 3.0 兼容的 64-bit Fabric Address** 格式:

```
[63:48] Fabric ID (16 bits)
[47:00] Local Address (48 bits)

- 阶段 1 (V3.1-Rev2.0): Fabric ID 全为 0 (本地寻址)
- 阶段 2 (NSA Phase 1): Fabric ID [63:56] 启用 (8 bits, 256 节点)
- 阶段 3 (NSA Phase 3): Fabric ID [63:48] 启用 (16 bits, 65K 节点)
```

**理由**:
1. **CXL 3.0 兼容**: 字段位分配与 CXL Fabric Address 完全对齐, 跨 Fabric 路由无障碍
2. **演进无债务**: Stage 1 (8b) → Stage 3 (16b) 渐进启用, 不重构 Entry 数据结构
3. **Local Addr 48 bits 完整沿用**: V3.1-Rev2.0 GUPA 路由域划分**完全保留**
4. **NVLink / Infinity Fabric 兼容**: 概念一致, 16-bit Fabric ID 是业界共识

### D2. 保留 NSA (Network-System Architecture) 族名, 地址格式学名采用 Fabric Address

**决策**: 保留 **NSA (Network-System Architecture)** 作为 CppTLM 内部架构族名（伞状术语, 覆盖 NSA Switch / NSA-aware MMU / NSA Stage 1/2/3 / NSA Fabric Address 等所有衍生术语）, 但地址格式的**学名/对外接口名**采用 CXL 3.0 **Fabric Address** 标准。

**理由**:
1. **NSA 族名是 CppTLM 内部伞状术语**: 9 份 NSA 草案 + 3 份 ADR + umbrella 提案均以 "NSA-aware / NSA Stage 1/2/3" 标识演进阶段, 改名会导致 10+ 文件批量变更
2. **学名对外**: 地址格式实际位分配必须与 CXL 3.0 Fabric Address 完全对齐, 跨厂商互操作无歧义
3. **替代命名备查**: 附录保留 GFS / FAS / UFA 替代命名参考 (per umbrella §"NSA 命名含义澄清")
4. **避免混淆**: NSA 在 CppTLM 含义为 Network-System Architecture, 与美国 National Security Agency (国家安全局) 无关 (per 附录澄清 + umbrella §)

### D3. HRT Entry 32-bit 字段扩展 (NSA-aware 部分)

**决策**: V3.1-Rev2.0 HRT Entry 32 bits 不变, 在剩余 16 bits 中扩展 NSA-aware 字段:

```cpp
struct NsaHrtEntry {
    uint8_t route_tag : 4;       // [31:28] 0:HBM, 1~E:UALink, F:PCIe
    uint8_t vc_id     : 4;       // [27:24] Virtual Channel
    uint8_t qos       : 8;       // [23:16] QoS / Throttle_Group
    uint8_t fabric_id_lo : 4;   // [15:12] Fabric ID 低 4 bits (Stage 2)
    uint8_t fabric_id_hi : 4;   // [11:8]  Fabric ID 高 4 bits (Stage 3)
    uint8_t remote_fabric : 1;  // [7]     Remote Fabric 标志位
    uint8_t capability_id : 3;   // [6:4]   Capability Handle
    uint8_t tenant_id    : 4;   // [3:0]   Tenant ID
};
// 总: 32 bits, 与 V3.1-Rev2.0 兼容
```

**理由**:
1. **SRAM 容量不变**: 4096 entries × 32 bits 双 Bank (per V3.1-Rev2.0)
2. **V3.1-Rev2.0 兼容**: 现有 HRT 写入路径无需修改, 仅 Shadow Bank 写入时新增字段
3. **3 阶段渐进启用**: Stage 1 全为 0 → Stage 2 启用低 4 bits → Stage 3 启用高 4 bits

### D4. NSA-aware TLB Entry 64 bits (Fabric ID 包含)

**决策**: NSA-aware TLB Entry 64 bits, 含 Fabric ID 字段:

```cpp
struct NsaTlbEntry {
    uint64_t va_tag : 29;          // [63:35] Tag (VA 高 29 bits)
    uint64_t fabric_id : 16;       // [34:19] Fabric ID
    uint64_t local_addr : 16;      // [18:3]  Local Addr 高位
    uint64_t permissions : 3;      // [2:0]   R/W/X
};
// Total: 64 bits, 紧凑对齐
```

**理由**:
1. **TLB 容量不变**: 64 bits / Entry, 与 V3.1-Rev2.0 兼容
2. **跨 Fabric 寻址**: Fabric ID 16 bits 支持 65K 节点
3. **Capability 校验**: 通过 Capability Handle (8 bits, per `27-nsa-capability.md` §4.3) 间接引用, 避免 TLB Entry 过大

### D5. CIU 注入 Fabric ID 路径

**决策**: GMMU 输出 Local GUPA (不变), CIU 注入 Fabric ID (经 RCT 配置)。

**理由**:
1. **GMMU 改动最小**: 仅 CIU 新增 Fabric ID 注入逻辑
2. **per-Context Fabric ID**: RCT 支持 per-Context Override (多租户)
3. **V3.1-Rev2.0 GMMU 完全保留**: 输出格式不变 (48 bits Local Addr)

---

## 3. Consequences (影响)

### 3.1 正面影响

1. **9 份 NSA 草案地址格式统一**: 所有草案引用本 ADR 的 Fabric Address 格式
2. **CXL 3.0 兼容**: Stage 3 与 CXL Fabric Switch 直接互操作
3. **演进无债务**: Stage 1 → 3 渐进启用, 不破坏 V3.1-Rev2.0 基础
4. **术语分层**: NSA (Network-System Architecture) 为 CppTLM 内部架构族名 (伞状术语), 地址格式学名采用 CXL 3.0 Fabric Address (跨厂商互操作)
5. **TLB/SRAM 容量不变**: 仅 Entry 字段位分配调整

### 3.2 负面影响

1. **HRT Entry 字段冲突风险**: 32 bits 字段需精确分配 (16 bits NSA-aware)
2. **CIU 注入路径复杂度**: RCT 需新增 per-Context Fabric ID Override
3. **跨 Fabric HRt 查表**: Stage 3 需多级 HRT (L1/L2/L3), 增加复杂度

### 3.3 风险与缓解

| 风险 | 等级 | 缓解策略 |
|------|------|----------|
| HRT Entry 32 bits 字段不够用 | 中 | Stage 3 扩展为多级 HRT (L1 per-GPC, L2 per-GPU, L3 per-Compute-Tray) |
| CIU 注入延迟 | 中 | Fabric ID 注入与 HRT 查表**并行执行**, 增加 ~2 ns 关键路径 |
| TLB Entry 含 Capability Handle 8 bits 不足 | 低 | Capability Database 扩展 (per Tenant 256 → 1024) |
| 跨 Compute Tray Page Table 查询延迟 | 高 | Hardware Directory L1/L2/L3 (per `25-nsa-hardware.md` §4) |
| CXL 3.0 Fabric 量产延期 | 中 | 方案 B 降级路径 (per `21-dist-scale-up-topology-b.md`) |

---

## 4. 5 阶段约束

### 4.1 v1.0 MVP (Stage 0, 已 ship)

- V3.1-Rev2.0 完整保留, Fabric ID 全为 0
- HRT Entry / TLB Entry 字段位不变, 仅定义 NSA-aware 字段预留位
- 0 个新 ABI 函数 (per ADR-088 §D5)

### 4.2 v1.x (NSA Phase 1)

- 启用 Fabric ID [63:56] (8 bits, 256 节点)
- HRT Entry 新增 fabric_id_lo (4 bits)
- CIU 注入 Fabric ID (经 RCT)
- 仍 0 个新 ABI 函数

### 4.3 v3.x (NSA Phase 3)

- 启用 Fabric ID [63:48] (16 bits, 65K 节点)
- HRT Entry 新增 fabric_id_hi (4 bits)
- Hardware Directory L1/L2/L3 (per `25-nsa-hardware.md` §4)
- Remote Atomic Unit 硬件 (per `25-nsa-hardware.md` §3)
- NSA Switch (CXL 3.0 Fabric Switch Tier 1)
- 仍 0 个新 ABI 函数

### 4.4 v3.x+ (远期)

- 跨数据中心 (Multi-Fabric, 65K 节点全局)
- CXL 3.0 Fabric 完整兼容
- 多租户 GPU 云 (Capability + CXL Fabric)

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

- **V3.1-Rev2.0** (`21-soc-topology-mvp.md` + `21-tee-udd-mvp.md` + `21-microarch-ifc-mvp.md`): NSA-aware 升级基础
- **CXL 3.0 规范**: Fabric Address 字段对齐
- **ADR-SOC-21**: V3.1-Rev2.0 拓扑修正决策基础

### 5.2 下游依赖 (本 ADR 决策基础)

- ADR-SOC-23 (NSA-aware vs 方案 B 选型)
- ADR-SOC-24 (Capability-Based 隔离选型)
- [`22-nsa-fabric-address-spec.md`](../architecture/22-nsa-fabric-address-spec.md) NSA-aware 规范 SSOT
- 8 份 NSA-aware 草案 (草案 2-8 + 方案 B)

### 5.3 关联 OpenSpec

- [`openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/`](../../../openspec/changes/2026-09-19-cpptlm-nsa-scale-up-umbrella/proposal.md)
  - umbrella 提案涵盖 8 份 NSA 草案 + 3 份 ADR (ADR-SOC-22/23/24)

---

## 6. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: NSA-aware 64-bit Fabric Address 格式决策 (D1-D5 + 5 阶段约束 + 5 项风险缓解) |
| 2026-09-20 | v1.1-rev | Sisyphus | Oracle 评审修订: D2 改"保留 NSA 族名" + 命名空间重命名 ADR-22 → ADR-SOC-22 |
