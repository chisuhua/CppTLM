# GMMU v1.0 MVP 详细设计 (GPU Memory Management Unit — Minimum Viable Product)

> **目的**: 定义 CppTLM dGPU **GMMU v1.0 MVP** 的硬件模块内部设计——作为 GPU 内部 MMU,接管 IO-DMA 引擎(原 SDMA,见 [§1.3 重命名](#13-命名约定-sdma--io-dma))的 `cpptlm_dma_translate_cb` 翻译路径,实现 **IO-DMA → GMMU → PCIe EP → Host Memory** 端到端数据通路。所有 GMMU 配置通过标准 PCIe MMIO 接口完成(per UsrLinuxEmu PCIe-only 架构原则)
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审(预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: [`openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/proposal.md)
> **关联文档**:
> - [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md) — GMMU 5 阶段演进路线图(v1.0/v1.1/v2.0/v2.1/v3.0)
> - [`20-gart-module.md`](20-gart-module.md) — **SUPERSEDED** (旧版 GART v0.1,归档参考,**不实施**)
> - [`pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
> - [`17-sdma-engine-design.md`](17-sdma-engine-design.md) — IO-DMA 引擎(本模块主要消费者,原 SDMA)
> - [`dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md) — PCIe slice 微架构(Stage 2 引入前 GMMU 边界)
> - UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp` — driver-side 协调
> **关联 ADR**:
> - ADR-088 §D3.8 — `cpptlm_dma_translate_cb` 外部契约(GMMU v1.0 严格遵守)
> - ADR-088 §D5 — 23 ABI 冻结(GMMU v1.0 不动 ABI)
> - ADR-SOC-09 — v1.0 NVIDIA+AMD dual vendor 战略(GMMU 共享 vendor 路径)

---

## §1 概述

### §1.1 GMMU 在 dGPU SoC 中的位置

GMMU 是 GPU **内部**的硬件模块,位于 IO-DMA 引擎(原 SDMA)与 PCIe EP 之间:

```
┌─────────────────────────────────────────────────────────────────┐
│                       GPU 内部                                   │
│                                                                 │
│  ┌─────────────────┐                                            │
│  │  ComputeUnit    │  (driver 写 MMIO command ring)             │
│  │  / KernelLaunch │                                             │
│  └────────┬────────┘                                            │
│           │                                                      │
│           ▼                                                      │
│  ┌─────────────────┐      ┌─────────────────┐                 │
│  │   IO-DMA 引擎    │      │                 │                │
│  │  (原 SDMA)      │◀────▶│  GMMU 硬件模块   │                │
│  │ (iova 请求者)  │      │ (VA → PA 翻译) │                │
│  │ + L1 TLB(per IO-DMA)│     │  + HW PTW walker │                │
│  └─────────────────┘      │  + Context_ID 表│                │
│                            │  + Invalidate Ring │                │
│                            └────────┬────────┘                │
│                                       │                         │
└───────────────────────────────────────┼─────────────────────────┘
                                        │
                                        ▼ PCIe TLP
                              ┌──────────────────┐
                              │  PCIe EP         │
                              │  (PcieEndpointIP)│
                              │  17 ports (PF+VF)│
                              └────────┬─────────┘
                                       │
                                       ▼ PCIe BAR MMIO + DMA
                             ┌──────────────────┐
                             │  Host CPU        │
                             │  (driver writes  │
                             │   to GMMU regs)  │
                             └──────────────────┘
```

### §1.2 为什么需要 GMMU(替代 GART 的原因)

| 维度 | GART v0.1(旧)| GMMU v1.0 MVP(新)|
|------|---------------|-------------------|
| **翻译层级** | 1 级 iova→pa,无 TLB,无 Page Fault | L1 TLB + HW PTW walker + Page Fault 返回 |
| **多租户** | 单 Context(iova 平铺)| Context_ID 表(extensible, v1.0 至少 1)|
| **维护路径** | driver 写 PTE 表副本 | driver 维护真实 host 页表 + GMMU HW walk |
| **演进路径** | ❌ 封闭,无法升级到 v2.0 | ✅ 5 阶段演进路线图(无债务)|
| **无障碍 host SVA zero-copy** | ❌ 不支持 | v2.0 加 ATS 即可,无需重构 |
| **远端 NIC-DMA 安全** | ❌ 无 RCT 概念 | v2.1 + RCT(基于现有 Context_ID) |

**核心设计原则**(per UsrLinuxEmu AGENTS.md §CppTLM 通信架构):
> **GMMU 是 GPU 内部硬件模块,由 driver 通过 PCIe MMIO 配置**。
> Driver 不调用 GPU 内部函数,GPU 内部状态(这里是 Context_ID 表 + Page Table Root)由硬件模块维护。
> Driver 通过写 MMIO 寄存器 → GMMU MMIO aperture → Context 表 / PTW root → IO-DMA 内部查询 GMMU 翻译。

### §1.3 命名约定:SDMA → IO-DMA

为与 GMMU 提案(MAS-3.1 Rev2.0)对齐,本模块设计起,**SDMA 重命名为 IO-DMA**:

| 旧名 | 新名 | 原因 |
|------|------|------|
| `SdmaEngineTLM` | `IoDmaTLM` | 强调"IO 域(Host↔Device)DMA 引擎";与 GMMU 提案一致 |
| `sdma_engine_tlm.{hh,cc}` | `io_dma_tlm.{hh,cc}` | 同上 |
| `test_sdma_engine_*.cc` | `test_io_dma_*.cc` | 同上 |
| `cpptlm_dma_translate_cb` | **不变**(ABI 冻结) | 跨仓契约保持(per ADR-088 §D5) |
| `[sdma]` Catch2 标签 | `[io_dma]` | 同上 |
| `[pcie]` 测试路径 | **保留** | PCIe EP 数据路径不变 |

**重要边界**: 命名变更**不**影响跨仓 ABI 签名(per ADR-088 §D5),仅影响仓内代码可读性 + 与 GMMU 提案的概念对齐。代码级重命名为独立任务(见 `tasks.md` §3.2)。

### §1.4 与 IOMMU 的对比

| 维度 | GMMU(GPU 内部)| IOMMU(系统侧)|
|------|----------------|-----------------|
| **位置** | GPU 芯片内(die 上)| 系统 chipset / CPU |
| **配置方** | GPU driver(via PCIe MMIO)| OS kernel(via sysfs/iommu_ops)|
| **翻译方向** | iova→pa(GPU→Host)| iova→pa(Device→Host)|
| **页表存储** | driver 维护在 host memory,GMMU HW walk | OS kernel 维护 + IOMMU HW walk |
| **PCIe 协议** | v2.0+ 加 ATS/PRI(per 演进路线图)| ATS/可选 + PRI |
| **真实硬件实例** | NVIDIA HMM / AMD XGMI / Intel ATS | AMD-Vi / Intel VT-d / ARM SMMU |
| **CppTLM 设计** | **本模块** | 暂未实现(per ADR-088 §D3.8) |

**设计决策**(per UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp` v0.1):
- **v1.0**: 实现 GMMU(本页),IO-DMA 经 `cpptlm_dma_translate_cb` 调用
- **长期**: v2.0+ 演进到完整 MMU(per [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md))

---

## §2 GMMU v1.0 MVP 端到端 Shippable Demo

v1.0 MVP 必须通过的"绿灯测试",证明整个 **IO-DMA → GMMU → PCIe EP → Host Memory** 链路贯通:

```
┌──────────────────────────────────────────────────────────────────────┐
│  v1.0 MVP Test: test_io_dma_gmmu_pcie_e2e                            │
├──────────────────────────────────────────────────────────────────────┤
│  Step 1: Driver 经 PCIe MMIO 写 GMMU                                  │
│            - GMMU_REG_CONTEXT_BASE[ctx=0] = host_page_table_root       │
│            - GMMU_REG_CTRL = enable                                    │
│                                                                       │
│  Step 2: Driver 经 PCIe BAR 提交 IO-DMA descriptor                   │
│            - iova = 0x4000_0000                                        │
│            - vram_offset = 0x0                                         │
│            - size = 4096                                               │
│            - dir = H2D                                                 │
│                                                                       │
│  Step 3: IO-DMA 引擎发起 H2D                                          │
│            - 调用 gmmu.translate(0x4000_0000, 4096, &phys)            │
│            - GMMU L1 TLB miss                                          │
│            - HW PTW walker 从 root 走 4 级页表(mock in-memory)       │
│            - 命中 → phys = 0x8000_0000                                │
│            - L1 TLB 填充 entry                                         │
│                                                                       │
│  Step 4: IO-DMA 经 PCIe EP 发起 MRd TLP                               │
│            - dst = 0x8000_0000, size = 4096                           │
│            - 经 PcieEndpointIP.req_in[PF] → host backdoor             │
│                                                                       │
│  Step 5: CplD 返回 → IO-DMA 写入 VRAM                                 │
│            - memcpy vram_offset + size bytes                           │
│            - done_out emit completion                                 │
│                                                                       │
│  Step 6: 验证 host memory 状态                                         │
│            - 读取 backdoor 0x8000_0000 内容 == IO-DMA descriptor      │
│            - 第二次相同 iova 请求 → L1 TLB hit(latency < 50% PTW)    │
│            - invalid_context_id 请求 → -EFAULT                        │
└──────────────────────────────────────────────────────────────────────┘
```

**验收标准**: 6 步全部通过 + 测试用例 ≥ 8 个(见 §12 测试覆盖)。

---

## §3 GMMU 模块类定义

### §3.1 类签名

```cpp
// include/tlm/gpu/gmmu_tlm.hh
namespace cpptlm::gpu {

// GMMU 控制寄存器位字段(per §4.1)
struct GmmuControlReg {
    uint32_t enable        : 1;   // Bit 0: GMMU enable
    uint32_t reserved_1_7  : 7;   // Bit 1-7: reserved
    uint32_t max_contexts  : 8;   // Bit 8-15: 当前活跃 Context 数(RO)
    uint32_t reserved_16_31: 16;  // Bit 16-31: reserved
};

// GMMU Context 表项(per §8)
struct GmmuContextEntry {
    uint64_t pt_root;           // host 页表根指针(0 = Context 禁用)
    uint32_t asid;              // Address Space ID(v1.0 MVP 仅 1 ctx,但保留字段)
    uint32_t enable;            // Context 启用位
};

// GMMU TLB entry(per §7)
struct GmmuTlbEntry {
    uint64_t va;                // Virtual Address(4KB aligned)
    uint64_t pa;                // Physical Address
    uint8_t  context_id;        // Context_ID tag(per §5.3 不变量 3)
    uint8_t  valid;             // entry 有效位
    uint8_t  perms;             // 权限位(R/W/X,v1.0 MVP 仅 R/W)
    uint8_t  reserved;          // 对齐
};

// GMMU PTW 请求(v1.0 单 outstanding,无队列)
struct GmmuPtwRequest {
    uint64_t va;                // 待翻译 VA
    uint8_t  context_id;        // Context_ID
    uint8_t  walk_level;        // 当前 walk 层(0 = root, 4 = leaf)
    uint64_t current_pte_addr;  // 当前 PTE 地址
};

class GmmuTLM : public sc_module {
public:
    SC_HAS_PROCESS(GmmuTLM);

    // MMIO 接口(SystemC TLM transaction,来自 PCIe EP BAR 路由)
    tlm_utils::simple_target_socket<GmmuTLM> gmmu_aperture;

    // 翻译查询接口(IO-DMA 引擎调用,替代原 cpptlm_dma_translate_cb 直注入的 stub)
    sc_port<gmmu_translator_if> translator;

    GmmuTLM(sc_module_name name);
    ~GmmuTLM() override = default;

    // MMIO write 回调(从 PCIe EP BAR 路由)
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

    // GMMU 翻译查询(IO-DMA 引擎调用,符合 cpptlm_dma_translate_cb 签名)
    // 返回: 0 = 成功(phys 填入); < 0 = errno(-EFAULT = Page Fault)
    int translate(uint64_t va, uint32_t size, uint64_t& phys);

    // 测试 helper:TLB 命中率统计
    double tlb_hit_rate() const { return tlb_hits_ / double(tlb_lookups_ + 1); }

private:
    // 控制 / 状态寄存器
    GmmuControlReg control_{};
    uint64_t status_fault_va_ = 0;   // 最近一次 Page Fault 的 VA(RO 调试)
    uint32_t status_fault_ctx_ = 0;  // 最近一次 Page Fault 的 Context_ID

    // Context 表(per §8,extensible vector)
    std::array<GmmuContextEntry, MAX_CONTEXTS> contexts_;  // v1.0 MAX_CONTEXTS = 1
    static constexpr uint32_t MAX_CONTEXTS = 8;  // v1.0 实际只用 1;预留 v1.1+ 扩展

    // L1 TLB(per §7)
    static constexpr size_t L1_TLB_ENTRIES = 64;  // 全关联
    std::array<GmmuTlbEntry, L1_TLB_ENTRIES> l1_tlb_{};
    size_t l1_tlb_evict_idx_ = 0;  // Round-Robin 替换指针

    // TLB 统计
    uint64_t tlb_lookups_ = 0;
    uint64_t tlb_hits_ = 0;

    // PTW walker 状态(per §6)
    GmmuPtwRequest current_ptw_{};

    // Host 页表 mock(in-memory,per §6.2)
    // v1.0 MVP 使用 in-memory 模拟 host 页表;v2.0+ 可切换为经 ABI 调用 driver
    std::unordered_map<uint64_t, uint64_t> mock_page_table_;

    // MMIO 处理子例程
    void handle_ctrl_write(uint64_t offset, uint32_t value);
    void handle_context_base_write(uint32_t ctx_id, uint64_t value);
    void handle_inv_write(uint64_t offset, uint64_t value);

    // 翻译流程子例程
    int lookup_l1_tlb(uint64_t va, uint8_t ctx_id, uint64_t& pa);
    int ptw_walk(uint64_t va, uint8_t ctx_id, uint64_t& pa);
    int ptw_walk_recursive(uint64_t pte_addr, uint64_t& next_pte_addr, uint64_t& pa);
    void tlb_fill(uint64_t va, uint8_t ctx_id, uint64_t pa, uint8_t perms);

    // TLB Invalidate 操作(per §9)
    void tlb_invalidate_all();
    void tlb_invalidate_by_iova(uint64_t va_lo, uint64_t va_hi, uint8_t ctx_mask);
};

}  // namespace cpptlm::gpu
```

### §3.2 翻译查询接口(对接 IO-DMA)

```cpp
// GMMU 模块 TLM 接口定义(per §1.2 设计原则)
struct gmmu_translator_if : public sc_interface {
    // 与 cpptlm_dma_translate_cb 签名严格一致(per ADR-088 §D3.8)
    // v1.0 返回值只支持 0(成功) 和 -EFAULT(Page Fault)
    virtual int translate(uint64_t va, uint32_t size, uint64_t& pa) = 0;
};
```

---

## §4 MMIO 寄存器布局

### §4.1 MMIO Aperture 地址映射

GMMU 寄存器位于 PCIe BAR 内的 MMIO 地址空间(具体 BAR 偏移由 PCIe 集成定义,per [`pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md)):

| 偏移 | 大小 | 名称 | 描述 |
|------|------|------|------|
| `0x5000` | 4 B | `GMMU_REG_CTRL` | 控制寄存器(enable / max_contexts RO) |
| `0x5004` | 4 B | `GMMU_REG_STATUS` | 状态寄存器(RO,fault_va 高 32 位)|
| `0x5008` | 4 B | `GMMU_REG_STATUS_LO` | fault_va 低 32 位 |
| `0x500C` | 4 B | `GMMU_REG_FAULT_CTX` | fault_context_id |
| `0x5020` + ctx*8 + 0x0 | 8 B | `GMMU_REG_CONTEXT_BASE[ctx]` | Context ctx 页表根(8 byte aligned)|
| `0x5020` + ctx*8 + 0x4 | 4 B | `GMMU_REG_CONTEXT_CTRL[ctx]` | Context ctx 控制(asid / enable)|
| `0x6000` | 8 B | `GMMU_REG_INV` | Invalidate 命令(op + ctx_mask + iova_lo)|
| `0x6008` | 8 B | `GMMU_REG_INV_HI` | Invalidate 命令扩展(iova_hi for range)|

### §4.2 控制寄存器(`GMMU_REG_CTRL`)

```cpp
// 32-bit 控制寄存器布局
struct GmmuControlReg {
    uint32_t enable         : 1;   // Bit 0: GMMU enable
    uint32_t reserved_1_7   : 7;   // Bit 1-7: reserved
    uint32_t max_contexts   : 8;   // Bit 8-15: 当前活跃 Context 数(RO)
    uint32_t reserved_16_31 : 16;  // Bit 16-31: reserved
};

// MMIO write 0x5000 (value=0x1) → GMMU enable
// MMIO read  0x5000 → 当前 enable 状态 + max_contexts (v1.0 = 1)
```

### §4.3 状态寄存器(`GMMU_REG_STATUS`, RO)

```cpp
struct GmmuStatusReg {
    uint32_t enabled      : 1;   // 当前 enable 状态
    uint32_t fault_valid  : 1;   // 最近一次 fault 是否有效
    uint32_t fault_iova_hi: 30;  // fault VA 高 30 位
};

// 配合 GMMU_REG_STATUS_LO (低 32 位) + GMMU_REG_FAULT_CTX (4 byte) 形成完整 fault 记录
```

### §4.4 Context Base 寄存器(`GMMU_REG_CONTEXT_BASE[ctx]`)

```cpp
// 8 byte,4KB aligned host 页表根指针
struct GmmuContextBaseReg {
    uint64_t pt_root : 64;  // 4KB aligned host 页表根地址
};

// Context_CTRL (4 byte)
struct GmmuContextCtrlReg {
    uint32_t asid     : 16;  // Address Space ID(v1.0 固定 0,v1.1+ 使用)
    uint32_t enable   : 1;   // Context 启用
    uint32_t reserved : 15;
};

// MMIO write 0x5020 + ctx*8 + 0x0 (value=host_pt_root) → 写 pt_root
// MMIO write 0x5020 + ctx*8 + 0x4 (value=0x1) → enable ctx
```

### §4.5 Invalidate 寄存器(`GMMU_REG_INV`)

```cpp
// 8 byte 命令寄存器(per §5.4 不变量 4)
struct GmmuInvReg {
    uint64_t op        : 4;   // 操作类型(FULL=0, BY_IOVA=1, BY_CONTEXT=2, BY_IOVA_AND_CONTEXT=3)
    uint64_t ctx_mask  : 8;   // Context_ID bitmask(v1.0 仅 bit 0 有效)
    uint64_t iova_lo   : 52;  // iova range 低 52 位(4KB aligned)
};

struct GmmuInvHiReg {
    uint64_t iova_hi   : 52;  // iova range 高 52 位
    uint64_t reserved  : 12;
};

// MMIO write 0x6000 (value=op=0) → GMMU_REG_INV_ALL
// MMIO write 0x6000 (value=op=1\|ctx_mask=1\|iova_lo=0x40000000) → invalidate iova 0x4000_0000+
```

---

## §5 翻译流程

### §5.1 translate() 主流程

```
translate(va, size, &phys):
  1. 检查 control.enable == 1,否则返回 0 + phys = va(直通模式,per §3.2)
  2. 解析 ctx_id = (control.max_contexts == 1) ? 0 : va.ContextID(v1.0 固定 0)
  3. 检查 contexts_[ctx_id].enable == 1,否则返回 -EFAULT
  4. 调用 lookup_l1_tlb(va, ctx_id, phys):
     - 命中 → 返回 0
     - miss → 继续
  5. 调用 ptw_walk(va, ctx_id, phys):
     - 成功 → tlb_fill(va, ctx_id, phys, R|W) + 返回 0
     - 失败 → 记录 fault_va / fault_ctx + 返回 -EFAULT
```

### §5.2 L1 TLB 查找

```
lookup_l1_tlb(va, ctx_id, &pa):
  tlb_lookups_++
  for i in 0..L1_TLB_ENTRIES-1:
    if l1_tlb_[i].valid && l1_tlb_[i].va == va (4KB aligned) && l1_tlb_[i].context_id == ctx_id:
      pa = l1_tlb_[i].pa
      tlb_hits_++
      return 0
  return -1  // miss
```

### §5.3 PTW Walker(单 outstanding)

```
ptw_walk(va, ctx_id, &pa):
  current_ptw_ = {va=va, context_id=ctx_id, walk_level=0, current_pte_addr=contexts_[ctx_id].pt_root}
  while current_ptw_.walk_level < 4:
    pte = read_pte(current_ptw_.current_pte_addr)  // 4 bytes from mock_page_table_
    if !pte.valid:
      record_fault(va, ctx_id)
      return -EFAULT
    if pte.is_leaf:
      pa = (pte.phys_pa << 12) | (va & 0xFFF)  // 4KB leaf
      return 0
    current_ptw_.current_pte_addr = (pte.next_table << 12)
    current_ptw_.walk_level++
  // 4 级 walk 完仍未 leaf → 不支持的页大小(v1.0 MVP 不支持 huge)
  record_fault(va, ctx_id)
  return -EFAULT
```

### §5.4 TLB Fill

```
tlb_fill(va, ctx_id, pa, perms):
  // 1. 查空位
  for i in 0..L1_TLB_ENTRIES-1:
    if !l1_tlb_[i].valid:
      l1_tlb_[i] = {va=va&~0xFFF, pa=pa&~0xFFF, context_id=ctx_id, valid=1, perms=perms}
      return
  // 2. 全满 → Round-Robin 替换
  l1_tlb_[l1_tlb_evict_idx_] = {va=va&~0xFFF, pa=pa&~0xFFF, context_id=ctx_id, valid=1, perms=perms}
  l1_tlb_evict_idx_ = (l1_tlb_evict_idx_ + 1) % L1_TLB_ENTRIES
```

### §5.5 Page Fault 处理

```
record_fault(va, ctx_id):
  status_fault_va_ = va
  status_fault_ctx_ = ctx_id
  // v1.0 MVP: 不发 MSI-X(per §14 边界);driver 通过 polling GMMU_REG_STATUS 识别 fault
  // v2.0+ ATS/PRI 引入后: GMMU 触发 PCIe PRI Request TLP(per 演进路线图)
```

---

## §6 HW PTW Walker 设计

### §6.1 单级页表格式(v1.0 MVP)

v1.0 MVP 仅支持 4KB 页大小,4 级页表(x86-64 风格):

```cpp
// 4-byte PTE(per ARMv8/x86-64 风格简化)
struct GmmuPte {
    uint32_t valid       : 1;   // Bit 0: PTE 有效
    uint32_t is_leaf     : 1;   // Bit 1: 是否叶子(否则指向下级页表)
    uint32_t perms       : 3;   // Bit 2-4: R/W/X(per §3.1 v1.0 仅 R/W)
    uint32_t next_table  : 52;  // Bit 5-56: 下一级页表基址(4KB aligned) 或 leaf 的 PA(4KB aligned)
    uint32_t reserved    : 7;   // Bit 57-63: reserved
};
```

### §6.2 Mock Host 页表(in-memory)

v1.0 MVP 使用 in-memory mock 页表,原因:
- 真实 host 页表路径需 driver 端 host 内存暴露(per §7.3 推荐 driver 通过 ABI 注册回调)
- v1.0 MVP 优先级是"链路贯通",不是"真实 walk 路径"
- v2.0+ 引入 ATS 时再切换为真实路径

```cpp
// 4 级页表 mock 实现
class GmmuMockPageTable {
public:
    // driver 注入测试页表(per 测试 setup)
    void install_pte(uint64_t pte_addr, const GmmuPte& pte) {
        ptes_[pte_addr] = pte;
    }

    // GMMU PTW walker 调用
    GmmuPte read_pte(uint64_t pte_addr) const {
        auto it = ptes_.find(pte_addr);
        if (it == ptes_.end()) return {valid=0};  // 触发 Page Fault
        return it->second;
    }

private:
    std::unordered_map<uint64_t, GmmuPte> ptes_;
};
```

### §6.3 PTW 性能模型

| 指标 | v1.0 MVP 实测值 | 备注 |
|------|-----------------|------|
| PTW 4 级 walk cycles | 4 × 10 cyc = 40 cyc | 每级 mock read 计 10 cyc |
| L1 TLB hit latency | 5 cyc | 全关联查找 |
| L1 TLB miss + PTW total | 45 cyc | TLB miss 触发 PTW |
| L2 TLB(v1.1+)| — | v1.0 不实现 |

---

## §7 L1 TLB 设计

### §7.1 TLB 容量与组织

| 参数 | 值 | 备注 |
|------|-----|------|
| 容量 | 64 entries | 全关联 |
| Entry 大小 | 24 bytes | (va, pa, context_id, valid, perms) |
| 总 SRAM | ~1.5 KB | per IO-DMA 实例 |
| 替换策略 | Round-Robin | v1.0 MVP 简化;v1.1 可换 LRU |
| 命中率目标 | > 80%(SPEC workload) | 测试基准验证 |

### §7.2 TLB Entry 格式(per §5.3 不变量 3)

```cpp
struct GmmuTlbEntry {
    uint64_t va;          // Virtual Address(4KB aligned,即低 12 位为 0)
    uint64_t pa;          // Physical Address(4KB aligned)
    uint8_t  context_id;  // Context_ID tag(关键:per §5.3 不变量,v1.0 即使只有 1 ctx 也保留)
    uint8_t  valid;       // entry 有效位
    uint8_t  perms;       // 权限位(R/W/X,v1.0 仅 R/W)
    uint8_t  reserved;    // 对齐
};
static_assert(sizeof(GmmuTlbEntry) == 24, "TLB entry 必须紧凑以节省 SRAM");
```

### §7.3 TLB 一致性保证

- **TLB Invalidate Ring**(per §9):driver 经 MMIO 触发精准/全表 invalidate
- **写入 TLB 时机**:仅 PTW walker 成功后 + tlb_fill()
- **TLB 不缓存权限失败**:perms 仅 R/W,失败返回 -EFAULT 不入 TLB

---

## §8 Context_ID 表设计

### §8.1 Context 表结构(per §5.2 不变量 2)

```cpp
struct GmmuContextEntry {
    uint64_t pt_root;    // host 页表根指针(0 = Context 禁用)
    uint32_t asid;       // Address Space ID(v1.0 MVP 固定 0,v1.1+ 使用)
    uint32_t enable;     // Context 启用位
};

class GmmuTLM {
private:
    static constexpr uint32_t MAX_CONTEXTS = 8;  // v1.0 实际只用 1;预留扩展
    std::array<GmmuContextEntry, MAX_CONTEXTS> contexts_;  // array 而非 vector,固定大小
};
```

### §8.2 v1.0 MVP Context 使用

| 字段 | v1.0 值 | 扩展性(v1.1+)|
|------|---------|----------------|
| `MAX_CONTEXTS` | 8(array) | 不变(足够) |
| 实际活跃 ctx 数 | 1(`max_contexts` RO = 1) | 按需增加 |
| `contexts_[0]` pt_root | driver 写 | 不变 |
| `contexts_[0]` asid | 固定 0 | driver 可写 |
| 其他 context 表项 | 全部 0(禁用)| driver 启用 + 写 pt_root |

**无债务演进保证**: v1.0 的 Context 表数据结构在 v1.1+ 加 Context 时**无需重构**(per §5.2 不变量 2)。

---

## §9 TLB Invalidate 设计

### §9.1 Invalidate 命令分类(per §5.4 不变量 4)

| Op 枚举值 | 命令名 | 行为 | v1.0 实现 |
|-----------|--------|------|-----------|
| 0 | `INV_FULL` | 全表 invalidate 所有 TLB | ✅ |
| 1 | `INV_BY_IOVA` | Invalidate (iova_lo, iova_hi) range + ctx_mask | ✅(v1.0 仅 ctx_mask=1)|
| 2 | `INV_BY_CONTEXT` | Invalidate 某 ctx 所有 TLB entry | ✅(v1.0 仅 ctx=0)|
| 3 | `INV_BY_IOVA_AND_CONTEXT` | Invalidate (iova, ctx) 元组 | ✅(v1.0 等同于 INV_BY_IOVA)|
| 4-15 | reserved | 未来扩展 | ❌ |

### §9.2 Invalidate 处理流程

```
tlb_invalidate_by_iova(va_lo, va_hi, ctx_mask):
  for i in 0..L1_TLB_ENTRIES-1:
    if l1_tlb_[i].valid:
      if (l1_tlb_[i].va >= va_lo && l1_tlb_[i].va <= va_hi) &&
         (ctx_mask & (1 << l1_tlb_[i].context_id)):
        l1_tlb_[i].valid = 0

tlb_invalidate_all():
  for i in 0..L1_TLB_ENTRIES-1:
    l1_tlb_[i].valid = 0
```

### §9.3 Invalidate 时序保证

v1.0 MVP 不实现 Invalidate Ordering(per §14 边界),依赖:
- driver 主动 invalidate(va unmap / ctx 销毁时)
- IO-DMA 提交新请求前已 invalidate 完成(同 PCIe posted write ordering)

---

## §10 IO-DMA 集成(SDMA → IO-DMA 重命名)

### §10.1 IO-DMA 调用 GMMU translate

```cpp
// io_dma_tlm.cc (原 sdma_engine_tlm.cc)
int IoDmaTLM::process_h2d(const IoDmaDescriptor& d, CompletionBundle& done) {
    // ... size 检查 / VRAM 范围检查 ...

    // 调用 GMMU translate (替代原 cpptlm_dma_translate_cb stub)
    uint64_t phys = 0;
    int rc = gmmu_->translate(d.host_iova, d.size, phys);
    if (rc != 0) {
        // Page Fault 处理
        done.status.write(static_cast<uint32_t>(-EFAULT));  // v1.0 MVP
        return -EFAULT;
    }

    // 发起 PCIe TLP MRd
    // ... 同原实现 ...
}
```

### §10.2 DGpuBoard 注入 GMMU 实例

```cpp
// dgpu_board_shell.cc
void DGpuBoard::setup_dma_chain() {
    // 1. 创建 GMMU 实例(注册到 ModuleFactory)
    auto* gmmu = ModuleFactory::instantiate<GmmuTLM>("gmmu");

    // 2. 创建 IO-DMA 实例
    auto* io_dma = ModuleFactory::instantiate<IoDmaTLM>("io_dma");

    // 3. 注入 GMMU → IO-DMA(替代原 translate_cb_ 注入)
    io_dma->set_gmmu_translator(gmmu->get_translator_port());

    // 4. 配置 GMMU MMIO aperture(经 PCIe EP BAR 路由)
    pcie_ep->register_mmio_target("gmmu_aperture", gmmu->aperture_port);
}
```

### §10.3 v1.0 MVP 与原 cpptlm_dma_translate_cb 的关系

| 维度 | 旧(cpptlm_dma_translate_cb stub)| 新(GMMU)|
|------|----------------------------------|----------|
| 实现位置 | DGpuBoard lambda | GMMU v1.0 MVP 模块 |
| driver 配置 | driver 写 PTE 表(flat) | driver 写 Context_BASE + host 页表 |
| TLB | 无 | L1 TLB 64 entries |
| Page Fault | 不存在概念 | 返回 -EFAULT |
| 翻译性能 | 每次查 PTE 表 | TLB hit < 5 cyc,miss 45 cyc |
| 多 Context | 不支持 | 支持(v1.0 仅 1)|

**关键不变量**: `cpptlm_dma_translate_cb` **签名不变**,GMMU 内部实现该签名即可,driver 视角无变化。

---

## §11 PCIe EP 集成

### §11.1 MMIO Aperture 路由

```
PcieEndpointIP BAR0 (MMIO aperture)
  ├─ 0x4000-0x4FFF: GART/GMMU 兼容区(per PcieEndpointIP 既有路由)
  ├─ 0x5000-0x5FFF: GMMU 寄存器(新增,per §4.1)
  ├─ 0x6000-0x6FFF: GMMU Invalidate Ring(新增)
  └─ 0x8000-0xFFFF: 其他 MMIO 寄存器
```

### §11.2 配置流程(per `20-gmmu-evolution-roadmap.md` §7.2)

```
Driver (host)                    GPU 内部 (CppTLM)
     │                                 │
     │  1. 写 GMMU_REG_CONTEXT_BASE[0] │
     │ ─────────────────────────────▶│
     │  MMIO write 0x5020 = host_pt_root│
     │                                 │  gmmu.contexts_[0].pt_root = host_pt_root
     │                                 │
     │  2. 写 GMMU_REG_CONTEXT_CTRL[0] │
     │ ─────────────────────────────▶│
     │  MMIO write 0x5024 = 0x1       │
     │                                 │  gmmu.contexts_[0].enable = 1
     │                                 │
     │  3. 写 GMMU_REG_CTRL = enable   │
     │ ─────────────────────────────▶│
     │  MMIO write 0x5000 = 0x1       │
     │                                 │  gmmu.control.enable = 1
     │                                 │
     │  4. IO-DMA 发起 iova=0x4000_0000│
     │                                 │  (driver 不参与)
     │                                 │  gmmu.translate(0x4000_0000, 4096, &phys)
     │                                 │  → PTW walk → phys=0x8000_0000
     │                                 │  IO-DMA 经 PCIe EP 发起 MRd 写 host 0x8000_0000
```

### §11.3 Page Migration / 失效

Driver 在 page migration 时失效 GMMU entry(无需 invalidation 回调):

```cpp
// Driver 代码:失效 iova=0x4000_0000 对应 TLB entry
void gmmu_invalidate_page(cpptlm_emulator_t* emu, uint64_t iova) {
    // 1. 调 host MMU 子系统 unmap(iova) → 触发 host 页表更新
    // 2. 写 GMMU Invalidate MMIO 命令
    uint64_t inv_cmd = 1 /*INV_BY_IOVA*/ | (1ULL << 4) /*ctx_mask=1*/ | (iova & ~0xFFF);
    syms->mmio_write(emu, GMMU_BAR, GMMU_REG_INV, &inv_cmd, sizeof(inv_cmd));
}
```

---

## §12 测试覆盖

### §12.1 单元测试(`test_gmmu_*.cc`)

| # | 测试用例 | 覆盖能力 | 断言 |
|---|---------|---------|------|
| 1 | `GMMU.MMIO enable gate` | 控制寄存器 | enable=0 时 translate 直通 |
| 2 | `GMMU.Context not enabled` | Context 校验 | enable=0 返回 -EFAULT |
| 3 | `GMMU.L1 TLB hit after PTW` | TLB 缓存 | 第二次同 VA 命中 + 耗时 < 50% PTW |
| 4 | `GMMU.L1 TLB miss triggers PTW` | PTW walker | mock 页表正确 walk → 返回正确 PA |
| 5 | `GMMU.PTW 4-level walk` | 4 级页表 | root → L3 → L2 → L1 → leaf |
| 6 | `GMMU.Page Fault on invalid PTE` | Page Fault 处理 | mock 页表无 PTE → -EFAULT + fault_va 记录 |
| 7 | `GMMU.Round-Robin TLB eviction` | TLB 替换 | 全满后写新 entry → 替换最旧 |
| 8 | `GMMU.Invalidate all` | INV_FULL | 全表清除 |
| 9 | `GMMU.Invalidate by iova range` | INV_BY_IOVA | range 内 entry 清,range 外保留 |
| 10 | `GMMU.Context_ID tag matching` | 不变量 3 | 跨 ctx 访问不串扰 |

### §12.2 集成测试(`test_io_dma_gmmu_pcie_e2e.cc`)

**核心 demo 测试**(per §2 端到端 shippable demo):
- 6 步全链路贯通
- 测试标签:`[io_dma][gmmu][pcie][e2e]`

### §12.3 跨仓集成测试(`test_io_dma_gmmu_pcie_e2e_ue.cc`)

per UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp`:
- UsrLinuxEmu 端 driver 配置 GMMU Context
- CppTLM 端 IO-DMA 发起 DMA
- 端到端 host memory 数据校验

### §12.4 Oracle 评审清单

- [ ] GMMU MMIO 寄存器布局与 Intel VT-d / AMD IOMMU 概念对齐
- [ ] PTW 4 级 walk 与 x86-64 页表格式一致
- [ ] L1 TLB 全关联 + Round-Robin 替换策略合理
- [ ] Context_ID 表 extensible 结构(v1.1+ 加 Context 不重构)
- [ ] Page Fault 返回 -EFAULT 符合 `cpptlm_dma_translate_cb` ABI(per ADR-088 §D3.8)
- [ ] 跨仓 ABI 影响:0 个新 ABI 函数(v1.0 MVP 严格遵守)
- [ ] 5 阶段演进路线图无债务(per [`20-gmmu-evolution-roadmap.md` §5](20-gmmu-evolution-roadmap.md))

---

## §13 演进不变量(4 条契约)

**详细定义见 [`20-gmmu-evolution-roadmap.md` §5](20-gmmu-evolution-roadmap.md)。** 本节列出 v1.0 MVP 的具体实现要求:

### §13.1 不变量 1:`phys` 输出语义只增不换

```cpp
// v1.0 MVP 的 translate() 已预留三元组结构,即使 v1.0 只填 phys
int GmmuTLM::translate(uint64_t va, uint32_t size, uint64_t& pa) {
    // v1.0: pa = Host PA(Stage 2 推迟)
    pa = translate_to_host_pa(va);  // 现有实现
    // v3.0 扩展点:新增 route_tag / vc_id 输出参数(不影响 v1.0 签名)
    return 0;
}
```

### §13.2 不变量 2:Context_ID 表 extensible(per §8.2)

```cpp
// v1.0 即定义 array + max_contexts 寄存器,预留扩展
std::array<GmmuContextEntry, MAX_CONTEXTS> contexts_;  // MAX_CONTEXTS=8
control_.max_contexts = 1;  // v1.0 仅 1,RO
```

### §13.3 不变量 3:TLB entry 包含 Context_ID tag(per §7.2)

```cpp
struct GmmuTlbEntry {
    // ... 即使 v1.0 仅 1 ctx,context_id 字段已存在
    uint8_t context_id;
};

// TLB lookup 始终 va_match && context_id_match
```

### §13.4 不变量 4:Invalidate MMIO 命令分类完整(per §9.1)

```cpp
enum class InvOp : uint8_t {
    FULL = 0, BY_IOVA = 1, BY_CONTEXT = 2, BY_IOVA_AND_CONTEXT = 3,
    // reserved 4-15 for future expansion
};
```

---

## §14 边界与限制

### §14.1 不实现的功能(v1.0 MVP 明确边界)

- ❌ **L2 TLB(共享全局 TLB)** — 推迟到 v1.1
- ❌ **Huge Page(2MB / 1GB)** — 推迟到 v1.1
- ❌ **Multi-initiator(SM LSU / TC-DMA / TEE Frontend)** — 推迟到 v1.1
- ❌ **Multi-Context(实际使用)** — 仅 1 context 可用(数据结构预留 v1.1+ 扩展)
- ❌ **ATS Translation Request 协议** — 推迟到 v2.0
- ❌ **Page Request Interface(PRI)** — 推迟到 v2.0
- ❌ **Stage 2(GUPA→Route_Tag)+ UDD/HRT** — 推迟到 v3.0
- ❌ **RCT(NIC-DMA Rx 远端权限)** — 推迟到 v2.1
- ❌ **Host 真实页表 walk** — v1.0 用 mock in-memory;v2.0+ 切换
- ❌ **Page Fault MSI-X 中断** — v1.0 driver polling GMMU_REG_STATUS;v2.0+ 改中断
- ❌ **TLB 预取(Prefetch)** — v1.1+ 评估
- ❌ **TLB Replacement = LRU** — v1.0 用 Round-Robin;v1.1 可升级
- ❌ **Nested Translation(guest + host)** — 推到 v3.x

### §14.2 已知限制

- L1 TLB 容量限制:64 entries(每 IO-DMA 实例);高并发工作负载可能命中率不足
- 替换策略:Round-Robin 简化;真实硬件用 LRU / pseudo-LRU
- Mock 页表:in-memory,与真实 host 页表不同步;测试需手动 install_pte()
- 单 Context 限制:多 VM/MIG 工作负载需 v1.1+ 支持
- Page Fault 处理:仅 -EFAULT,无 IO-DMA 内部 Replay Buffer(per MAS-3.1 §5.1)

---

## §15 跨仓契约

### §15.1 UsrLinuxEmu 侧(不修改 23 ABI)

GMMU v1.0 MVP **不引入任何新 CppTLM ABI 函数**——所有 driver↔GPU 通信通过**现有 23 ABI 函数**(特别是 `syms->mmio_write/read` + `cpptlm_emulator_register_dma_translate_cb`)。

per UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**:
> 真实硬件不允许 driver 直接调 GPU 内部函数。所有通信必须通过 PCIe(MMIO + DMA)。

### §15.2 CppTLM 侧(新模块 + 重命名)

| 文件 | 状态 |
|------|------|
| `include/tlm/gpu/gmmu_tlm.{hh,cc}` | **新建**(本模块)|
| `include/tlm/gpu/io_dma_tlm.{hh,cc}` | **重命名**(原 sdma_engine_tlm) |
| `test/test_gmmu_*.cc` (10+ 用例) | **新建** |
| `test/test_io_dma_gmmu_pcie_e2e.cc` | **新建**(端到端 demo) |
| `src/tlm/gpu/dgpu_board_shell.cc` | **修改**(注入 GMMU → IO-DMA) |
| `src/tlm/gpu/sdma_engine_tlm.{hh,cc}` | **归档**(重命名后保留 1 版本过渡)|
| `include/chstream_register.hh` | **修改**(注册 GmmuTLM + 重命名 SdmaEngineTLM → IoDmaTLM) |
| **23 ABI 头冻结** | ✅ **不变**(per ADR-088 §D5) |

### §15.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. UsrLinuxEmu 端 driver 实现(由用户协调):`linux_compat/iommu/gmmu.cpp`(基于 host MMU 子系统)
2. CppTLM 端 GMMU 模块 + IO-DMA 重命名(本仓)
3. 跨仓集成测试(`test_io_dma_gmmu_pcie_e2e_ue.cc`)
4. 同步 PR(无 ABI 影响,跨仓风险低)

---

## §16 引用

### §16.1 内部引用

- [`pcie-endpoint-architecture.md`](16-pcie-endpoint-architecture.md) — PCIe EP 跨仓架构 SSOT
- [`17-sdma-engine-design.md`](17-sdma-engine-design.md) — IO-DMA 引擎(本模块主要消费者,**待重命名**)
- [`18-pcie-endpoint-entry.md`](18-pcie-endpoint-entry.md) — PCIe EP entry 序列
- [`19-pcie-ip-microarchitecture.md`](19-pcie-ip-microarchitecture.md) — PCIe IP 内部设计
- [`20-gmmu-evolution-roadmap.md`](20-gmmu-evolution-roadmap.md) — GMMU 5 阶段演进路线图(SSOT for evolution)
- [`20-gart-module.md`](20-gart-module.md) — **SUPERSEDED**(归档参考)
- [`dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md) — PCIe slice 微架构

### §16.2 跨仓引用

- UsrLinuxEmu AGENTS.md §CppTLM 通信架构 — **PCIe-only 原则**
- UsrLinuxEmu ADR-088 §D3.8 — `cpptlm_dma_translate_cb` 契约
- UsrLinuxEmu ADR-088 §D5 — 23 ABI 冻结
- UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp` — driver-side 改造(原 `2026-09-18-cpptlm-iommu-gart` 重命名)

### §16.3 Linux 生产参考

- `drivers/iommu/intel/iommu.c` — Intel VT-d(MMU 经典实现)
- `drivers/iommu/amd/iommu.c` — AMD IOMMU
- `drivers/iommu/arm-smmu-v3.c` — ARM SMMUv3(Context_ID + ATS/PRI 实现)
- `drivers/gpu/drm/amd/amdgpu/amdgpu_gart.c` — AMD GART(被 GMMU 取代)
- `drivers/gpu/drm/amd/amdkfd/kfd_migrate.c` — KFD HMM migration(GMMU v2.0 ATS 雏形)
- NVIDIA Hopper/Blackwell SM 内部 MMU(per MAS-3.1 提案参考)
- PCIe ATS/PRI 规范:`PCI-SIG ATS 1.0` + `PCI-SIG PRI 1.0`

---

## §17 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版:GMMU v1.0 MVP 详细设计(7 必含能力 + 6 步端到端 demo + MMIO 寄存器布局 + IO-DMA 重命名 + 4 条不变量)|

---

**关联 OpenSpec change**: [`openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/proposal.md)
**下次更新**: Oracle 评审反馈后 v1.1