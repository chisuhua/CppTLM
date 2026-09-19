# GART 模块设计 (Graphics Address Remapping Table)

> ## ⚠️ SUPERSEDED — 此文档已归档，不再实施
>
> **归档日期**: 2026-09-19
> **归档原因**: GART v0.1 设计被 **GMMU v1.0 MVP** 取代。GART 仅作为历史参考保留,**不实施、不修改**。
>
> **取代文档**:
> - **5 阶段演进路线图**: [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md)
> - **GMMU v1.0 MVP 详细设计**: [`20-gmmu-mvp.md`](20-gmmu-mvp.md)
> - **OpenSpec change**: [`openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/proposal.md)
>
> **取代理由** (per [`20-gmmu-evolution-roadmap.md` §1.2](20-gmmu-evolution-roadmap.md)):
> 1. **GART v0.1 从未 ship** — 无代码实现、无测试、无跨仓 driver 集成;继续按 GART 路线走,会先 ship 一个简化方案再升级,产生"未来清理 GART"的债务
> 2. **概念与 GMMU 提案脱节** — GMMU 提案(MAS-3.1 Rev2.0)是行业标准 GPU MMU 设计;现行 GART 与 NVIDIA Hopper/Blackwell / AMD CDNA 3 / Intel ATS 无映射关系
> 3. **无 Context 隔离 / 无 Page Fault 处理** — GART 单 Context 设计,无法支撑 NVIDIA MIG / AMD Multi-VF / SVA zero-copy 等生产场景
>
> **跨仓 driver change 同步重命名**: UsrLinuxEmu `2026-09-18-cpptlm-iommu-gart` → `2026-09-19-cpptlm-dgpu-gmmu-mvp`(由用户协调)
>
> **保留理由**: 历史参考 + 跨仓 PR 记录保留,避免 Git history 断裂
>
> ---
>
> **目的**: 定义 CppTLM dGPU 的 **GART (Graphics Address Remapping Table)** 硬件模块的内部设计——作为 GPU 内部 IOMMU 替代品，将 GPU VA (iova) 翻译为 Host PA，支持 SDMA/DMA engine 的 iova→pa 翻译查询。所有 GART 配置通过标准 PCIe MMIO 接口完成（per UsrLinuxEmu PCIe-only 架构原则）
>
> **状态**: ⚠️ **SUPERSEDED** (2026-09-19) — 见上方 banner
> **审计**: 不再适用(已被 GMMU v1.0 MVP 取代)
> **关联**:
> - [`pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
> - [`sdma-engine-design.md`](17-sdma-engine-design.md) — SDMA 引擎（GART 主要消费者,正在重命名为 IO-DMA）
> - UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
> - UsrLinuxEmu ADR-088 §D3.8 — DMA translate 设计
> - UsrLinuxEmu change [2026-09-18-cpptlm-iommu-gart](../../../UsrLinuxEmu/openspec/changes/2026-09-18-cpptlm-iommu-gart/) — Driver-managed GART 架构决策(已被取代为 [`2026-09-19-cpptlm-dgpu-gmmu-mvp`](../../../openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/))

---

## §1 概述

### §1.1 GART 在 GPU SoC 中的位置

GART 是 GPU **内部**的硬件模块，位于 GPU 的 SDMA/DMA engine 与 Host 物理内存之间：

```
┌─────────────────────────────────────────────────────────────┐
│                       GPU 内部                                │
│                                                             │
│  ┌─────────────────┐                                          │
│  │  CommandBuffer  │  (driver 写 MMIO command ring)            │
│  │  (PM4/AQL)      │                                          │
│  └────────┬────────┘                                          │
│           │                                                    │
│           ▼                                                    │
│  ┌─────────────────┐      ┌─────────────────┐              │
│  │   SDMA/DMA       │      │                 │              │
│  │   Engine         │◀────▶│  GART 硬件模块  │              │
│  │ (iova 请求者)   │      │ (iova→pa 翻译)  │              │
│  └─────────────────┘      └────────┬────────┘              │
│                                       │                       │
└───────────────────────────────────────┼───────────────────────┘
                                        │
                                        ▼ PCIe BAR MMIO
                              ┌──────────────────┐
                              │  Host CPU         │
                              │  (driver writes   │
                              │   to GART regs)   │
                              └──────────────────┘
```

### §1.2 为什么需要 GART

GPU 引擎（SDMA、Compute Shader）发出的 DMA 请求使用 **iova (I/O Virtual Address)**——这是 GPU 视角的"虚拟地址"。GART 负责在 GPU 内部将这些 iova 翻译为 **Host PA**，让 DMA 引擎能够访问真正的系统物理内存。

**关键设计原则**（per UsrLinuxEmu AGENTS.md §CppTLM 通信架构）：
> **GART 是 GPU 内部硬件模块，由 driver 通过 PCIe MMIO 配置**。
> Driver 不调用 GPU 内部函数，GPU 内部状态（这里是 GART PTEs 表）由硬件模块维护。
> Driver 通过写 MMIO 寄存器 → GART MMIO aperture → GART PTEs 表 → DMA engine 内部查询 GART 翻译。

### §1.3 GART 与 IOMMU 的对比

| 维度 | GART（GPU 内部） | IOMMU（系统侧） |
|------|----------------|-----------------|
| **位置** | GPU 芯片内 | 系统 chipset / CPU |
| **配置方** | GPU driver（via PCIe MMIO）| OS kernel（via sysfs/iommu_ops）|
| **翻译方向** | iova→pa（GPU→Host）| iova→pa（Device→Host）|
| **PCIe 协议** | N/A（GPU 内部） | ATS（可选）/ MMIO |
| **真实硬件实例** | AMD GART / NVIDIA BAR / Intel VTD | AMD-Vi / Intel VT-d / ARM SMMU |
| **CppTLM 设计** | **本模块** | 暂未实现（per ADR-088 §D3.8） |

**设计决策（per UsrLinuxEmu change 2026-09-18-cpptlm-iommu-gart v0.2）**：
- **短期**: 实现 GART（GPU 内部），对齐 Linux AMD KFD `amdgpu_gart_map` 模式
- **长期**: 可选扩展实现 IOMMU（系统侧）+ ATS 协议（per QEMU Intel VT-d 参考）

---

## §2 GART 寄存器布局

### §2.1 MMIO Aperture 地址映射

GART 寄存器位于 PCIe BAR 内的 MMIO 地址空间（具体 BAR 偏移由 PCIe 集成定义，per [`pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md)）：

| 偏移 | 大小 | 名称 | 描述 |
|------|------|------|------|
| `0x4000` | 4 B | `GART_REG_CTRL` | 控制寄存器 |
| `0x4004` | 4 B | `GART_REG_STATUS` | 状态寄存器（RO）|
| `0x4008` | 4 B | `GART_REG_INV_ALL` | 全表失效（写 1 触发）|
| `0x8000 + N*8` | 8 B | `GART_REG_PTE_BASE[N]` | 第 N 个 PTE entry（4KB aligned） |

### §2.2 控制寄存器 (`GART_REG_CTRL`)

```cpp
// 32-bit 控制寄存器布局
struct GartControlReg {
    uint32_t enable     : 1;   // Bit 0: GART enable
    uint32_t invalidate : 1;   // Bit 1: 全表失效（写 1 触发，硬件自动清零）
    uint32_t reserved    : 30;  // Bit 31:2: reserved
};

// MMIO write 0x4004 (with value=0x1) → GART enable
// MMIO write 0x4004 (with value=0x3) → GART enable + 全表失效
```

### §2.3 状态寄存器 (`GART_REG_STATUS`, RO)

```cpp
struct GartStatusReg {
    uint32_t enabled    : 1;   // 当前 enable 状态
    uint32_t fault_iova : 31;  // 最近一次翻译失败的 iova（调试用）
};
```

### §2.4 PTE Entry (`GART_REG_PTE_BASE[N]`)

```cpp
// 每 PTE 8 字节，4KB page-aligned
struct GartPteEntry {
    uint64_t valid : 1;     // Bit 0: PTE 有效位
    uint64_t iova  : 20;    // Bit 1-20: 4KB aligned IOVA（用于反向查找）
    uint64_t pa    : 40;    // Bit 21-60: Host 物理地址
    uint64_t flags : 3;     // Bit 61-63: R/W/U 权限位
};

// MMIO write 0x8000 + iova/4096 * 8 → 写入对应 PTE
// MMIO write 0x8000 with value=0 → 清除 PTE（valid=0）
```

---

## §3 GART PTE 表数据结构

### §3.1 默认 Aperture 大小

```cpp
// 默认 4MB aperture（1024 PTE entries × 4KB/page = 4MB）
constexpr size_t GART_NUM_PTES = 1024;
constexpr size_t GART_PAGE_SIZE = 4096;
constexpr size_t GART_APERTURE_SIZE = GART_NUM_PTES * GART_PAGE_SIZE;  // 4MB
```

**注**: Aperture 大小可在编译时通过宏配置（`GART_NUM_PTES`），与 BAR 大小相关。CppTLM 设计支持 1MB-2GB aperture（256-524288 PTE entries）。

### §3.2 PTE 表内部结构

```cpp
namespace cpptlm::internal {

class GartModule : public sc_module {
public:
    SC_HAS_PROCESS(GartModule);
    
    // MMIO 接口（SystemC TLM transaction）
    tlm_utils::simple_target_socket<GartModule> gart_aperture;
    
    // 翻译查询接口（SDMA/DMA engine 调用）
    sc_port<gart_translator_if> translator;
    
    GartModule(sc_module_name name) : sc_module(name),
        gart_aperture("gart_aperture"),
        translator("translator") {
        gart_aperture.register_b_transport(this, &GartModule::b_transport);
        SC_THREAD(invalidate_thread);
    }
    
    // MMIO write 回调（从 PCIe BAR 路由）
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
    
    // GART 翻译查询（SDMA/DMA engine 调用）
    uint64_t translate(uint64_t iova) {
        if (!control_.enable) return iova;  // Bypass when disabled
        
        size_t idx = (iova >> 12) & (GART_NUM_PTES - 1);
        const auto& pte = ptes_[idx];
        if (!pte.valid) return iova;  // Bypass when PTE invalid
        
        return (pte.pa << 12) | (iova & 0xFFF);  // 4KB page-aligned
    }
    
private:
    // 控制/状态寄存器
    GartControlReg control_{};
    GartStatusReg  status_{};
    
    // PTE 表（RAM backed）
    std::array<GartPteEntry, GART_NUM_PTES> ptes_{};
    
    // 失效队列（用于 invalidate 后的延迟清理）
    sc_fifo<uint64_t> invalidate_queue_;
    
    void invalidate_thread();  // 处理失效请求
};

}  // namespace
```

---

## §4 Driver 配置流程

### §4.1 配置时序

```
Driver (host)                    GPU 内部 (CppTLM)
     │                                 │
     │  1. 写 GART_REG_CTRL=enable    │
     │ ─────────────────────────────▶│
     │  MMIO write 0x4000 = 0x1       │
     │                                 │  GartModule::control.enable = 1
     │                                 │
     │  2. 写 PTE entry (iova=0x1000)  │
     │ ─────────────────────────────▶│
     │  MMIO write 0x8000 = PTE       │
     │                                 │  ptes_[0] = PTE
     │                                 │
     │  3. SDMA 发起 iova=0x1000      │
     │                                 │  (driver 不参与)
     │                                 │  GartModule::translate(0x1000)
     │                                 │  → ptes_[0] → pa=0x200000
     │                                 │  SDMA 写入 host 0x200000
```

### §4.2 MMIO write 处理逻辑

```cpp
// GartModule::b_transport 实现（SystemC TLM callback）
void GartModule::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    uint64_t addr = trans.get_address();
    uint64_t* data = trans.get_data_ptr();
    uint8_t* byte_data = reinterpret_cast<uint8_t*>(data);
    
    if (trans.is_write()) {
        // 控制寄存器 (0x4000-0x400F)
        if (addr >= 0x4000 && addr < 0x4010) {
            uint32_t value = *reinterpret_cast<uint32_t*>(data);
            handle_ctrl_write(addr - 0x4000, value);
        }
        // PTE 表 (0x8000+)
        else if (addr >= 0x8000) {
            uint64_t pte_value = *reinterpret_cast<uint64_t*>(data);
            size_t idx = (addr - 0x8000) / 8;
            if (idx < GART_NUM_PTES) {
                ptes_[idx] = decode_pte(pte_value);
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
}

void GartModule::handle_ctrl_write(uint64_t offset, uint32_t value) {
    switch (offset) {
        case 0x0:  // CTRL
            control_.enable = value & 0x1;
            control_.invalidate = (value >> 1) & 0x1;
            if (control_.invalidate) {
                // 全表失效
                for (auto& pte : ptes_) pte.valid = false;
                control_.invalidate = 0;  // 硬件自动清零
            }
            break;
    }
}
```

### §4.3 Page Migration / 失效

Driver 在 page migration 时失效 GART entry（无需 invalidation 回调）：

```cpp
// Driver 代码：失效 iova=0x1000 对应 PTE
void gart_unmap_page(cpptlm_emulator_t* emu, uint64_t iova) {
    uint64_t zero_pte = 0;  // valid=0
    uint32_t pte_offset = GART_REG_PTE_BASE + (iova >> 12) * 8;
    syms->mmio_write(emu, GART_BAR, pte_offset, &zero_pte, sizeof(zero_pte));
    // Driver 自己知道映射失效，不需要 GPU 通知
}
```

**无 invalidation 回调原因**（per UsrLinuxEmu AGENTS.md §PCIe-only 原则）：
- Driver 自己管理 iova→pa 映射（不依赖 GPU 通知）
- GART 表是 driver 视角的"副本"（Driver 写入，GPU DMA engine 读取）
- 无需双向同步

---

## §5 DMA Engine 集成

### §5.1 SDMA Engine 通过 GART 翻译

```cpp
// SDMA engine 在发起 DMA 前查询 GART
class SdmaEngine : public sc_module {
public:
    sc_port<gart_translator_if> gart_port;  // GART 模块实例
    
    void execute_packet(const DmaPacket& pkt) {
        // pkt.iova → 需要翻译为 pa
        uint64_t pa = gart_port->translate(pkt.iova);
        
        // 发起 PCIe DMA 写（使用 pa）
        tlm::tlm_generic_payload trans;
        trans.set_address(pa);
        trans.set_data_ptr(pkt.data);
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        pcie_initiator_socket->b_transport(trans, delay);
    }
};
```

### §5.2 Compute Shader / Other DMA Initiators

GART 也服务于其他 DMA 发起者：
- SDMA engine（专用 DMA）
- Compute Shader（shader 输出）
- Video decoder（媒体解码输出）
- Display controller（扫描输出）

所有 DMA 发起者都通过 `gart_translator_if::translate()` 查询 GART。

---

## §6 与外部模块的集成

### §6.1 上游依赖（consumers）

GART 模块被以下模块使用：
- SDMA engine（主消费者）
- Compute dispatcher
- Video decoder
- Display controller

### §6.2 下游依赖（producer）

GART 接收来自：
- PCIe EP BAR MMIO 路由（driver 配置入口）
- GART 控制寄存器（enable / invalidate）

### §6.3 SystemC TLM 接口

```cpp
// GART 模块 TLM 接口定义
struct gart_translator_if : public sc_interface {
    virtual uint64_t translate(uint64_t iova) = 0;
};

// PCIe EP BAR 路由到 GART
class PcieEndpointBarRouter {
    sc_port<mmio_target_if> gart_aperture_port;
    
    void route_mmio(uint64_t bar, uint64_t offset, void* data, size_t len) {
        if (bar == GART_BAR) {
            gart_aperture_port->b_transport(trans);
        }
    }
};
```

---

## §7 验证与测试

### §7.1 单元测试

```cpp
TEST_CASE("GART MMIO write updates PTE") {
    GartModule gart("gart");
    uint64_t pte_value = encode_pte(/*iova=*/0x1000, /*pa=*/0x200000, /*flags=*/RW);
    tlm_write(GART_BAR, GART_REG_PTE_BASE, &pte_value, sizeof(pte_value));
    REQUIRE(gart.translate(0x1000) == 0x200000);
}

TEST_CASE("GART translate with invalid PTE passthrough") {
    GartModule gart("gart");
    // 不写 PTE
    REQUIRE(gart.translate(0x1000) == 0x1000);  // Bypass
}

TEST_CASE("GART enable bit gates translation") {
    GartModule gart("gart");
    uint64_t pte = encode_pte(0x1000, 0x200000, RW);
    tlm_write(GART_BAR, GART_REG_PTE_BASE, &pte, sizeof(pte));
    tlm_write(GART_BAR, GART_REG_CTRL, &zero, 4);  // disable
    REQUIRE(gart.translate(0x1000) == 0x1000);  // Bypass when disabled
    tlm_write(GART_BAR, GART_REG_CTRL, &enable, 4);  // enable
    REQUIRE(gart.translate(0x1000) == 0x200000);  // translate when enabled
}
```

### §7.2 集成测试（跨仓）

```cpp
// UsrLinuxEmu test/integration/test_dgpu_gart_config_ue.cc
TEST_CASE("Driver writes GART PTEs via MMIO, SDMA reads via GART") {
    cpptlm_emulator_t* emu = create_emulator("d2d1");
    CpptlmBridge bridge;
    bridge.init(kCpptlm, "configs/dgpu_board_v1.json");
    
    // Driver: gart_map_page(emu, iova=0x1000, pa=0x200000, RW)
    gart_map_page(emu, 0x1000, 0x200000, RW);
    
    // SDMA: DMA_WRITE(iova=0x1000, data=...)
    sdma_submit_packet(emu, 0x1000, test_data);
    
    // 验证：Host PA 0x200000 收到数据
    REQUIRE(memcmp(host_memory[0x200000], test_data, ...) == 0);
}
```

### §7.3 Oracle 评审清单

- [ ] GART 寄存器布局与 AMD GART 规范兼容
- [ ] MMIO aperture 在 BAR 地址空间合理
- [ ] DMA engine 集成 TLM 接口符合 SystemC 惯例
- [ ] Driver 配置时序与 Linux AMD KFD `amdgpu_gart_map` 一致
- [ ] Cross-repo ABI 影响：0 个新符号（per UsrLinuxEmu PCIe-only 原则）

---

## §8 边界与限制

### §8.1 不实现的功能（明确边界）

- ❌ **IOMMU**（系统侧）— 暂未实现。per UsrLinuxEmu ADR-088 §D3.8 可选扩展
- ❌ **ATS 协议**（PCIe 协议仿真）— GART 不需要 ATS，driver 直接写 MMIO
- ❌ **PRI（Page Request Interface）**— 不需要，GART 无 page fault 概念
- ❌ **Nested Translation**（guest IOMMU + host GART）— 暂未实现

### §8.2 已知限制

- Aperture 大小限制：默认 4MB（1024 entries × 4KB），可通过编译宏调整
- PTE 格式：4KB page-aligned（不支持 2MB/1GB huge page）
- 权限位：R/W/U 3 位（不支持 NX / supervisor 等复杂权限）
- 无 TLB：每次翻译都查 PTE 表（实际硬件会有 ATC，模拟暂未实现）

---

## §9 未来扩展（Roadmap）

### §9.1 v1.0（当前）
- ✅ GART 硬件模块（本文档）
- ✅ MMIO aperture 暴露
- ✅ SDMA engine 集成

### §9.2 v2.0（可选）
- [ ] ATC（Address Translation Cache）— 性能优化
- [ ] 支持 huge page（2MB / 1GB）
- [ ] GART 错误中断（fault_iova → MSI-X）

### §9.3 v3.0（长期）
- [ ] 可选 IOMMU 仿真（per UsrLinuxEmu ADR-088 §D3.8）
- [ ] ATS 协议支持（per QEMU Intel VT-d 参考）

---

## §10 跨仓契约

### §10.1 UsrLinuxEmu 侧（不修改 ABI）

GART 模块**不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU 通信通过**现有 15 个 ABI 函数**（特别是 `syms->mmio_write/read`）。

per UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**：
> 真实硬件不允许 driver 直接调 GPU 内部函数。所有通信必须通过 PCIe（MMIO + DMA）。

### §10.2 CppTLM 侧（新模块）

- **新文件**: `src/dgpu_board/gart_module.h/.cpp`（新增）
- **新接口**: `gart_translator_if`（SystemC sc_interface）
- **MMIO 路由**: 通过 PCIe EP BAR 路由
- **不修改 ABI**: 保持现有 15 个符号不变

### §10.3 跨仓 PR 协调

GART 模块作为 **CppTLM 仓新增模块**，不涉及 ABI 变更。跨仓 PR 流程（per ADR-091 §R5.1）：
1. UsrLinuxEmu 端 driver 实现（`linux_compat/iommu/gart.cpp`）
2. CppTLM 端 GART 模块实现
3. 跨仓集成测试（test_dgpu_gart_config_ue.cc）
4. 同步 PR（无 ABI 影响，跨仓风险低）

---

## §11 引用

### §11.1 内部引用

- [`pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
- [`sdma-engine-design.md`](17-sdma-engine-design.md) — SDMA 引擎（GART 主要消费者）
- [`pcie-endpoint-entry.md`](18-pcie-endpoint-entry.md) — PCIe EP entry 序列
- [`pcie-ip-microarchitecture.md`](19-pcie-ip-microarchitecture.md) — PCIe IP 内部设计

### §11.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D3.8 — DMA translate 设计
- UsrLinuxEmu change [2026-09-18-cpptlm-iommu-gart](../../../UsrLinuxEmu/openspec/changes/2026-09-18-cpptlm-iommu-gart/) — Driver-managed GART 决策
- UsrLinuxEmu change [2026-09-17-cpptlm-attach-inliner](../../../UsrLinuxEmu/openspec/changes/2026-09-17-cpptlm-attach-inliner/) — v0.4 ABI drift 修复

### §11.3 Linux 生产参考

- `drivers/gpu/drm/amd/amdgpu/amdgpu_gart.c` — AMD GART 实现
- `drivers/gpu/drm/amd/amdkfd/kfd_migrate.c` — KFD HMM migration
- `drivers/iommu/intel/iommu.c` — Intel VT-d（IOMMU 替代参考）
- AMD GART Hardware Specification (public docs)

---

**最后更新**: 2026-09-18 (Draft v0.1)  
**下次更新**: Oracle 评审后 v0.2
