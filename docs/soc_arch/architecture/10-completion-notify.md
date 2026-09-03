# 10. dGPU SoC v1.0 Completion & Notification 架构 — Doorbell + CompletionRing + MSI-X

> **类别**: SoC Architecture > 子系统架构 (L2 完成通知)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（§3.2 L2.4 CompletionRing + §3.2 L2.5 MSI-X）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/completion-ring.md`](../modules/completion-ring.md)
> - [`docs/architecture/14-pcie-ip-microarchitecture.md §4 (MSI-X)`](../modules/msix_table_mvp.md)
> - [`docs/architecture/14-pcie-ip-microarchitecture.md §4 (PCIe Config Space)`](../modules/pcie_config_space_mvp.md)（MSI-X 状态机）
> **关联研究综述**:
> - [`docs/research/PCIe/PCIe_上的保序write.md`](../../research/PCIe/PCIe_上的保序write.md)（PCIe 强序 write 机制）
> - [`docs/research/CP/amd/US20210191730A1_未映射队列聚合门铃_解析.md`](../../research/CP/amd/US20210191730A1_未映射队列聚合门铃_解析.md)（AMD 聚合 doorbell）
> **关联 ADR**: ADR-SOC-08 P1（MSI-X mask/unmask/PBA 状态机）/ ADR-SOC-06 D5（UsrLinuxEmu IOCTL 0x27/0x29/0x01）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.2 L2 命令流层的**完成与通知子系统**详细化文档。

- 想快速理解范围 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 Doorbell 强序 write → 读 §3
- 想理解 CompletionRing 完成环 → 读 §4
- 想理解 MSI-X 中断投递 → 读 §5
- 想理解 GPU→Host 完成路径 → 读 §6
- 想理解 v1.0 战略对齐 → 读 §7
- 想理解 CppTLM 实施 → 读 §8
- 想理解配置 Schema → 读 §9
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §10
- 想评估风险 → 读 §11

---

## 1. 范围与目标

### 1.1 L2 Completion & Notification 子系统定位

**L2 Completion & Notification** = dGPU SoC v1.0 系统拓扑的**完成与中断层**,负责:

- **Doorbell**:Host 通知 GPU 新任务到达(强序 write)
- **CompletionRing**:GPU 通知 Host 任务完成(doorbell 触发 + CQ entry)
- **MSI-X interrupt**:GPU→Host 中断投递(mask/unmask/PBA)
- **Host→GPU→Host 完成路径**:doorbell → GPU 执行 → CompletionRing + MSI-X

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R25)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| CompletionRing + MSI-X | ✅ 基础(mask/unmask/PBA per ADR-SOC-08 P1)| 同 v1.0 | L2 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.1 PASS 的 §3.2 L2.4-L2.5 + §4-bis 范围矩阵 R25 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

```
                ┌─────────────────────────────────────────────┐
                │            Host CPU                          │
                │  - Doorbell write(强序,通知 GPU)            │
                │  - CompletionRing read(查询完成)            │
                │  - MSI-X interrupt handler                  │
                └───────────────────┬─────────────────────────┘
                                        │
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │  PcieEndpointIP 17 ports                  │
                  │  - BAR0 window(Doorbell MMIO)            │
                  │  - Completion tracker(per stream_id)    │
                  │  - MSI-X interrupt delivery              │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ Doorbell MMIO      │    │ MSI-X interrupt      │
              │ window (BAR0)      │    │ delivery (MSI-X Table)│
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                           │
                        ▼                           ▼
                  ┌──────────────────────────────────────────┐
                  │  GPU 内部                                 │
                  │  - CompletionRing.push(entry)            │
                  │  - MSI-X interrupt pending bit           │
                  │  - CQ entry: txn_id + status + data     │
                  └──────────────────────────────────────────┘

   ──────────────────  Host→GPU→Host 完成路径  ──────────────────

   Host CPU 写入 Doorbell(BAR0 MMIO)
      ↓
   PcieEndpointIP → CommandProcessor(per L3)
      ↓
   GPU 执行 + 写 CompletionRing entry
      ↓
   GPU 置 MSI-X pending bit
      ↓
   MSI-X interrupt TLP → Host CPU interrupt handler
      ↓
   Host 读 CompletionRing entry + 处理结果
```

---

## 3. Doorbell 强序 write

### 3.1 PCIe 保序 write 机制(per `docs/research/PCIe/PCIe_上的保序write.md`)

**核心机制**:
- PCIe Posted Write 必须保证**顺序**(Per PCIe 规范)
- MMU ordering pipe + dummy non-posted read flush
- **Gen5 x16 latency 周期精度**:仅 PCIe 范围(per `docs-archived/adr/ADR-X.16` §周期精度注记)

### 3.2 Doorbell 角色

**Doorbell**(per `00-overview` §3.2 L2.3):
- Host CPU 通过 **BAR0 window** 写入 Doorbell MMIO 寄存器
- 通知 GPU 有新任务到达
- **强序保证**:Doorbell write 在 PushBuffer 写入**之后**可见
- **Gen5 x16 latency**:250-700ns 区间断言(per ADR-X.16)

### 3.3 实施要点

- **BAR0 window 映射**:`PcieEndpointIP.resp_out[stream_id].bar0_window`
- **Doorbell MMIO 寄存器**:`Doorbell` 模块(per `completion-ring.md`)
- **PushBuffer/Ring Buffer 物理地址**:BAR0 window 内的 MMIO 寄存器

---

## 4. CompletionRing 完成环

### 4.1 角色

**CompletionRing**(per `00-overview` §3.2 L2.4 + `completion-ring.md`):

- GPU 完成任务后写 CQ entry
- 包含 txn_id + status + optional data
- Host 通过读 CQ entry 确认完成
- **Doorbell 强序 + CQ 保序**:NVIDIA Completion Queue + Doorbell 强序(per `completion-ring.md` §蓝图)

### 4.2 CQ Entry 结构

```cpp
struct CQEntry {
    uint64_t txn_id;         // 事务 ID(per stream_id)
    uint32_t status;         // 完成状态(0=success, error_code, etc.)
    uint32_t size;           // 数据大小(可选)
    uint64_t timestamp;      // 完成时间
    uint8_t  data[16];       // 可选数据(per read 响应)
};
```

### 4.3 关键专利参考

| 来源 | 主题 |
|------|------|
| NVIDIA Completion Queue + Doorbell | 强序保证 + CQ entry 格式 |
| AMD 聚合 Doorbell | per US20210191730A1(未映射队列聚合门铃)|
| US20210191730A1 | AMD 聚合门铃用于未映射队列 |

### 4.4 CppTLM 实施(per `completion-ring.md`)

**位置**:SOC 内 `CompletionRingTLM`(per ADR-SOC-07 D1/D7)

- T-bs-2b rename(从 `completion_ring_mvp.hh` → `completion_ring_tlm.hh`)
- 4 端口 ChStreamModuleBase(per `2026-08-26-cpptlm-dgpu-board-soc-split`):
  - GPU 写入口
  - Host 读出口
  - Doorbell 通知
  - MSI-X 触发

### 5. MSI-X 中断(per ADR-SOC-08 P1)

**MSI-X 中断投递**(per `00-overview` §3.2 L2.5 + ADR-SOC-08 P1):

- **mask/unmask/PBA** 状态机
- GPU → Host 中断投递
- 与 PcieEndpointIP.msix_of(stream_id) 集成

**测试覆盖**(per ADR-SOC-08 P1):
- ✅ `test/test_pcie_endpoint_msix_state.cc`(Phase 8 已实施)

---

## 5. MSI-X 中断

### 5.1 MSI-X 角色

**MSI-X**(Message Signaled Interrupts - eXtended):

- GPU → Host 中断投递(替代 legacy INTx)
- mask/unmask/PBA(Pending Bit Array)状态机
- 支持 per-VF 中断(per SR-IOV)

### 5.2 状态机(per ADR-SOC-08 P1)

```cpp
class MsiXTable {
public:
    // mask:1=中断屏蔽, 0=中断使能
    uint32_t mask_[MAX_VECTORS];      // per-VF 中断屏蔽
    
    // pending:1=中断挂起, 0=无挂起
    uint32_t pending_[MAX_VECTORS];   // per-VF 中断挂起
    
    void mask_vector(uint16_t vector_id);
    void unmask_vector(uint16_t vector_id);
    void set_pending(uint16_t vector_id);  // GPU 触发
    void clear_pending(uint16_t vector_id);  // Host 处理后清
};
```

### 5.3 per-VF 中断(per SR-IOV)

- **1 PF + 16 VF** 各自独立中断向量
- mask/pending 数组大小 = `(1 + 16) × 4 vectors × 4 bytes = 272 bytes`
- 与 PcieEndpointIP.msix_of(stream_id) 集成

### 5.4 CppTLM 实施

- **`MsiXTable` 模块**(per `msix_table_mvp.md`)
- **`PcieConfigSpace` 模块**(per `pcie_config_space_mvp.md`, MSI-X capability)
- **测试**:`test/test_pcie_endpoint_msix_state.cc` PASS

---

## 6. GPU → Host 完成路径

### 6.1 完整流程

```
1. Host CPU 写入 PushBuffer/Ring Buffer(doorbell 之前)
     ↓
2. Host CPU 写 Doorbell(BAR0 MMIO,强序)
     ↓
3. PcieEndpointIP req_in[stream_id] 接收 doorbell TLP
     ↓
4. CommandProcessor FSM:IDLE → FETCH → DECODE → DISPATCH
     ↓
5. TMU/WDU/SM/CU 执行任务
     ↓
6. SM/CU 完成 → CompletionRing.push(entry)
     ↓
7. MSI-X pending bit set
     ↓
8. MSI-X interrupt TLP → PcieEndpointIP.resp_out[stream_id]
     ↓
9. Host CPU interrupt handler
     ↓
10. Host 读 CompletionRing entry + 处理结果
```

### 6.2 关键约束

- **Doorbell 强序**:Host 写 Doorbell 必须在 PushBuffer 写入之后
- **CQ 保序**:GPU 写 CQ entry 必须在 Host 读 CQ entry 之前
- **MSI-X 投递**:在 CQ entry 写入之后触发

### 6.3 错误路径

- **IOMMU fault**(per ADR-SOC-08 P3):MSI-X interrupt 投递错误路径
- **Completion 超时**:Host 等待 CQ entry 超时检测
- **MSI-X 屏蔽**:Host mask 中断后 pending bit 保留,unmask 后投递

---

## 7. v1.0 战略对齐

### 7.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| L2 Doorbell | ✅ 强序 write | ✅ §3 |
| L2 CompletionRing | ✅ NVIDIA + AMD CQ 模式 | ✅ §4 |
| L2 MSI-X | ✅ mask/unmask/PBA | ✅ §5 |
| GPU→Host 完成路径 | ✅ 6 步 | ✅ §6 |

### 7.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| Doorbell BAR0 MMIO | ✅ | ✅ |
| CompletionRing | ✅ | ✅ |
| MSI-X mask/unmask/PBA | ✅ | ✅ |
| per-VF 中断 | ✅ | ✅ |
| IOMMU fault 错误路径 | ✅ | ✅ |
| live migration interrupt | ❌ | ✅ v5.5.4(per ADR-SOC-08)|

### 7.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-06 D5 | UsrLinuxEmu IOCTL 0x27/0x29/0x01(完成路径入口)|
| ADR-SOC-08 P1 | MSI-X mask/unmask/PBA 状态机 |
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 战略 |

---

## 8. CppTLM 实施

### 8.1 模块清单

| 模块 | 路径 | 角色 |
|------|------|------|
| **CompletionRing** | `include/tlm/gpu/completion_ring_mvp.hh` (rename 为 `_tlm` 后缀待执行,per §6 表 T-bs-2b) | GPU 完成环 |
| **MsiXTable** | `include/tlm/gpu/msix_table_mvp.hh` | MSI-X 状态机 |
| **PcieConfigSpace** | `include/tlm/gpu/pcie_config_space_mvp.hh` | Config Space(MSI-X capability)|
| **Doorbell** | (PcieEndpointIP 内部)| Doorbell MMIO 寄存器 |

### 8.2 关键共享方法

```cpp
class CompletionRing {
public:
    void push(uint16_t stream_id, const CQEntry& entry);
    CQEntry pop(uint16_t stream_id);
    bool empty(uint16_t stream_id) const;
};

class MsiXTable {
public:
    void mask_vector(uint16_t vector_id);
    void unmask_vector(uint16_t vector_id);
    void set_pending(uint16_t vector_id);
    void clear_pending(uint16_t vector_id);
    bool is_pending(uint16_t vector_id) const;
};
```

### 8.3 测试覆盖

| 测试 | 场景 |
|------|------|
| `test_pcie_endpoint_tick_e2e.cc` | Doorbell + CQ 端到端 |
| `test_pcie_endpoint_msix_state.cc` | MSI-X mask/unmask/PBA(per ADR-SOC-08 P1)|
| `test_pcie_endpoint_doorbell_queue.cc` | Doorbell 排队/并发(per ADR-SOC-08 P4)|
| `test_pcie_endpoint_ip_full_e2e.cc` | Phase 8 全链路 E2E |

---

## 9. 配置 Schema

```json
{
  "name": "completion_ring_0",
  "type": "CompletionRing",
  "params": {
    "ring_size": 1024,
    "entry_size_bytes": 32,         // CQEntry 结构大小
    
    "bar0_base": "0x10000000",      // Doorbell MMIO 基地址
    "doorbell_offset": 0x1000,      // Doorbell 寄存器偏移
    
    "msix_table_size": 4,           // per-VF 中断向量数
    "msix_enable": true,
    "msix_pba_support": true,       // Pending Bit Array
    
    "cq_entry_format": "v1",        // CQ entry 格式版本
    "cq_optional_data": true,       // 是否在 CQ entry 携带数据
    
    "completion_timeout_cycles": 1000000,  // Host 等待超时
    "iommu_fault_msix": true        // IOMMU fault 错误路径(per ADR-SOC-08 P3)
  }
}
```

---

## 10. ADR/微架构/OpenSpec 引用矩阵

### 10.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-06 D5 | UsrLinuxEmu IOCTL 0x27/0x29/0x01(完成路径入口)|
| ADR-SOC-08 P1 | MSI-X mask/unmask/PBA 状态机 |
| ADR-SOC-08 P3 | IOMMU fault 错误路径 |
| ADR-SOC-08 P4 | Doorbell 排队/并发 |
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 战略 |

### 10.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **CompletionRing** | [`docs/soc_arch/modules/completion-ring.md`](../modules/completion-ring.md) |
| **MsiXTable** | [`docs/architecture/14-pcie-ip-microarchitecture.md §4 (MSI-X)`](../modules/msix_table_mvp.md) |
| **PcieConfigSpace** | [`docs/architecture/14-pcie-ip-microarchitecture.md §4 (PCIe Config Space)`](../modules/pcie_config_space_mvp.md) |
| **PcieEndpointIP** | [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md) |

### 10.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/PCIe/PCIe_上的保序write.md`](../../research/PCIe/PCIe_上的保序write.md) | PCIe 强序 write + MMU ordering pipe + Gen5 250-700ns |
| [`docs/research/CP/amd/US20210191730A1_未映射队列聚合门铃_解析.md`](../../research/CP/amd/US20210191730A1_未映射队列聚合门铃_解析.md) | AMD 聚合 doorbell |

### 10.4 关联 OpenSpec changes

| Change | 关联内容 |
|--------|---------|
| `2026-08-26-cpptlm-dgpu-board-soc-split/` | CompletionRing 4 端口 ChStreamModuleBase(per ADR-SOC-07 D1/D7) |

---

## 11. 风险与缓解 R1-R4

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | Doorbell 强序 write 与 PushBuffer 写入乱序 | 🟢 低 | per PCIe 规范 Posted Write 保证;per `docs/research/PCIe/PCIe_上的保序write.md` MMU ordering pipe |
| **R2** | MSI-X pending bit 在 mask 期间丢失 | 🟡 中 | mask 时保留 pending bit,unmask 后投递 |
| **R3** | CQ entry 物理地址映射 | 🟢 低 | BAR0 window 内 MMIO 寄存器显式映射 |
| **R4** | live migration 中断状态(per ADR-SOC-08 D4) | 🟢 低 | v1.1 完整版追加(UsrLinuxEmu v5.5.4 节奏决定) |

---

## 12. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L2 Completion & Notification 子系统架构,基于 Doorbell + CompletionRing + MSI-X + GPU→Host 完成路径 + AMD 聚合门铃 + PCIe 强序 write) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS

---

## 📋 子架构文档完成清单(v1.0 Draft)

| # | 文档 | 状态 | 行数(估) |
|---|------|------|----------:|
| 00 | 总架构蓝图 | ✅ v3.1 PASS(Oracle 2 轮)| 873 |
| 01 | Host Interface | ✅ Draft v1 | 715 |
| 02 | Command Processor | ✅ Draft v1 | ~600 |
| 03 | Task Management Unit | ✅ Draft v1 | ~580 |
| 04 | Work Distribution | ✅ Draft v1 | ~580 |
| 05 | SM/CU Compute Unit | ✅ Draft v1 | ~650 |
| 06 | Tensor Core | ✅ Draft v1 | ~500 |
| 07 | Memory System | ✅ Draft v1 | ~580 |
| 08 | NoC Interconnect | ✅ Draft v1 | ~500 |
| 09 | Coherence Protocol | ✅ Draft v1 | ~560 |
| 10 | Completion & Notification | ✅ Draft v1 | ~500 |
| **合计** | | | **~6,640** |

**10 份子系统架构文档全部完成 v1.0 Draft**,待 Oracle 评审(并行 10 次评审,预计 1-2 hr)。