# Minimal DGpu SoC v1.0 设计

> **版本**: v1.0
> **日期**: 2027-02-09
> **状态**: 📋 提案(已通过 Oracle 二轮复审,等正式 OpenSpec 提案)
> **前置**: `DGpuBoard v2.0.2`(架构文档 `docs/architecture/14-dgpu-board-ideal-arch.md` + 4 个 ADR)+ gem5 `AbstractMemory`/`PhysicalMemory`/`MemBackdoor` 参考模式
> **配套 ADR**: 无(本设计自身是 ADR-DGPU 系列后续工作的起点,需要时可独立发 ADR)
> **Owner**: CppTLM Team
> **影响**: 新增 `GmmuTLM` 模块;扩展 `MemoryTLM`(增加 `set_backing_store` + `on_config_loaded`);扩展 `DGpuBoard`(新增 `framebuffer_` + backdoor 路径);BAR1 存储路由改造
> **目标存档**: `docs/designs/2027-02-09-minimal-dgpu-soc.md`(本仓实现侧,本仓不与外部 ArchForge 仓同步;Oracle 二轮列出的 ArchForge 选项是设计存档可选去向,本设计仅落在本仓)

---

## 0. 文档版本与评审记录

| 阶段 | 日期 | 评审方 | 结论 |
|------|------|--------|------|
| 探索 | 2027-02-09 | explore ×3 + librarian ×1 | 收集既有组件现状 + gem5 内存模式 |
| 头脑风暴 | 2027-02-09 | 用户澄清(2 轮: GMMU 抽象级别 + BAR 布局 + 访存路径) | 选定一级页表 + BAR0 寄存器 + BAR1 存储+门铃 + 零时路径 |
| Oracle 一轮复审 | 2027-02-09 | oracle | 5 项 Top-5 修复 + 文档落地 |
| Oracle 二轮复审 | 2027-02-09 | oracle | 4 项硬伤(JSON 嵌套/`as<T>()`/translate_cb 签名/落盘路径)+ 6 项次级 |

本 v1.0 已纳入所有 Oracle 二轮复审修订。

---

## 1. 设计目标与边界

### 1.1 目标

构建一个**最小可演示**的 dGPU SoC 示例,端到端贯通 **host ABI → PCIe BAR → SDMA → GMMU → 帧缓冲** 路径:

- **存储**: host 通过 BAR1 直接读写存储空间(零延迟直读直写)
- **SDMA**: DMA 搬运 host ↔ 存储,经 GMMU 翻译 host_iova → 物理地址
- **GMMU**: 一级页表翻译,PT_BASE 寄存器(MMIO 配置),无 TLB,每次 DMA 即时翻译
- **(后续)** CP 模块:本期不做,留接口

### 1.2 非目标

- **NG1**: 不做多级页表/TLB(留待 v2.1 GMMU 扩展,符合 `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp` 路线)
- **NG2**: 不做 PcieDisplayDevice 集成(D1 已交付,本期聚焦存储侧)
- **NG3**: 不改 23 ABI 签名(冻结,per ADR-088 §D5)
- **NG4**: 不改 SdmaEngineTLM 既有 API(完整复用)
- **NG5**: 不引入第三方依赖

### 1.3 关键设计原则

1. **单一 backing 源**: `DGpuBoard::framebuffer_` 是 SoC 全部访存的**真源**,镜像 gem5 `PhysicalMemory::backingStore[].pmem` 角色。MemoryTLM/GMMU/SDMA/host backdoor 都直读直写同一份 `std::vector<uint8_t>`,天然 coherence,无需 flush。
2. **零时路径**: SoC 内部 ChStream 访存走 `MemoryTLM::tick()` 零时 memcpy(镜像 gem5 `SimpleMemory::recvFunctional()` + `AbstractMemory::functionalAccess()`),无 stats 延迟,host 与 device 同时间访问同一份 backing。
3. **callback 单一所有权**: callback 完全在 `CallbackWorker` 内(DGPU v2.0.2),本期不增加 callback 类型。
4. **配置注入**: `framebuffer_size_bytes` 从 `pcie_ep.params.bar_sizes[1]` 派生,避免双真相源;顶层 `framebuffer_size_bytes` 仅作 override。

---

## 2. 模块清单与拓扑

### 2.1 拓扑

```
DGpuBoard (shell, 23 ABI 冻结, v2.0.2)
└── DGpuSoc (SimModule 容器)
    ├── PcieEndpointIP          (17-port, BAR0 MMIO + BAR1 1GB 存储+门铃)
    │   └── PcieBarRouter       (BAR0 寄存器表: GMMU_PT_BASE_LO/HI / GMMU_CTRL / SDMA_STATUS)
    ├── SdmaEngineTLM           (5-port, ring mode + fence, translate_cb → GmmuTLM)
    ├── GmmuTLM                 (新建: 一级页表翻译, 无 TLB)
    ├── MemoryTLM               (扩展: 加 set_backing_store + on_config_loaded + 零时路径)
    └── CompletionRingTLM       (fence 完成 → MSI-X)
```

### 2.2 模块清单(7 个,3 个新建)

| 组件 | 类型 | 来源 | 职责 |
|------|------|------|------|
| `PcieEndpointIP` | 17-port SimModule | ✅ 复用 | BAR 存储 + config space + MSI-X |
| `SdmaEngineTLM` | 5-port ChStreamModule | ✅ 复用 | DMA 搬运 (H2D/D2H), ring mode, fence |
| `CompletionRingTLM` | 4-port ChStreamModule | ✅ 复用 | fence 完成汇聚 → MSI-X vector 0 |
| `PcieBarRouter` | 工具 | ✅ 复用 | BAR0 寄存器表 (PT_BASE_LO/HI 等) |
| **`GmmuTLM`** | **新建** SimModule | 🆕 | 一级页表翻译 (PT_BASE 寄存器, 无 TLB) |
| `MemoryTLM` | ChStreamModule(扩展) | 🔧 改 | 加 `set_backing_store` + 零时 tick + `on_config_loaded` |
| `DGpuBoard` + `DGpuSoc` | shell + 容器 | ✅ 复用(v2.0.2) | 23 ABI 入口 + 路由 + 新 framebuffer_ |

### 2.3 不在 v1.0 范围的组件(后续工作)

- **CP** (Compute Pipeline): v2.0 后续
- **Display IO device** (D1): 已交付 v1.1.1,本期 `display_routing_enabled=false` 不启用
- **Memory Device** (D2): `openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp` 提案;本期通过 framebuffer_ 简化实现其能力
- **Host Bypass / Root Complex**: Phase 8 已交付,本期测试用 host 直连模式(经 `DGpuBoard::mmio_*`)

---

## 3. 存储子系统(gem5 模式)

### 3.1 设计原理

镜像 gem5 `PhysicalMemory::backingStore` + `AbstractMemory::pmemAddr` 分层:

- **gem5 模式**: `PhysicalMemory` 拥有 backing(系统范围),通过 `setBackingStore(uint8_t*)` 注入每个 `AbstractMemory`;`pmemAddr` 是单一真源,设备端口和 host backdoor 都直读直写同一份内存。
- **CppTLM v1.0 映射**: `DGpuBoard::framebuffer_` 是 `PhysicalMemory` 角色;`MemoryTLM::backingPtr_` 是 `AbstractMemory::pmemAddr` 角色,由 board 注入。

### 3.2 模块映射表

| gem5 角色 | CppTLM v1.0 对应 |
|----------|-----------------|
| `PhysicalMemory::backingStore[].pmem` | `DGpuBoard::framebuffer_` (std::vector<uint8_t>, lazy alloc) |
| `AbstractMemory::pmemAddr` | `MemoryTLM::backingPtr_` (uint8_t*, board-injected) |
| `AbstractMemory::setBackingStore()` | `MemoryTLM::set_backing_store(uint8_t*, size)` (新增) |
| `AbstractMemory::toHostAddr()` | `MemoryTLM::host_addr(off) = backingPtr_ + off` |
| `SimpleMemory::recvFunctional()` | `MemoryTLM::tick()` (ChStream req_in, 零时 memcpy) |
| `MemBackdoor::ptr()` | `DGpuBoard::backdoor_read/write(framebuffer_.data())` |

### 3.3 MemoryTLM 扩展

```cpp
// include/tlm/memory_tlm.hh (扩展, 144 → ~190 行)
// 保留既有 REGISTER_CHSTREAM + 端口定义不变
class MemoryTLM : public ChStreamModuleBase {
public:
    // 既有 API 不变 (req_in / resp_out / stats_*)

    // ── 新增: backing 注入 (镜像 SdmaEngineTLM::set_host_backdoor 模式) ──
    void set_backing_store(uint8_t* ptr, uint64_t size_bytes) noexcept {
        backingPtr_  = ptr;
        backingSize_ = size_bytes;
        if (size_ == 0) size_ = size_bytes;  // 默认 size 跟随 backing
    }

    // 新增: 容量配置 (on_config_loaded 调用)
    void set_size_bytes(uint64_t sz) noexcept { size_ = sz; }

    // 新增: on_config_loaded — 让 JSON params 真正生效 (当前为 stub)
    void on_config_loaded() override {
        // 从 cfg 读 capacity_gb → set_size_bytes
        // 当前 JSON 若无此字段, 使用 framebuffer_size 派生值
    }

    // 新增: 访问 helper (供 SDMA / GMMU 直接用, 走非 ChStream 路径)
    uint8_t* host_addr(uint64_t offset) const noexcept {
        return backingPtr_ ? (backingPtr_ + offset) : nullptr;
    }
    uint64_t backing_size() const noexcept { return backingSize_; }

private:
    uint8_t*  backingPtr_  = nullptr;
    uint64_t  backingSize_ = 0;
    uint64_t  size_        = 0;
    bool      writeable_   = true;
};

// tick() 重写 (零时路径, 镜像 gem5 functionalAccess):
void MemoryTLM::tick() override {
    if (!req_in_.valid() || !req_in_.ready()) return;
    const auto& req = req_in_.data();
    const uint64_t addr = req.address.read();
    const uint8_t  sz   = req.size.read();

    bundles::CacheRespBundle resp;
    resp.transaction_id.write(req.transaction_id.read());
    resp.error_code.write(0);
    resp.is_hit.write(1);   // 命中 framebuffer_ 即 hit

    if (!backingPtr_ || addr + sz > backingSize_) {
        resp.error_code.write(1);   // OUT_OF_RANGE
        resp.is_hit.write(0);
    } else if (req.is_write.read()) {
        // 写: req.data 是 ch_uint<64> = 8 字节, 超 8 字节需上游分片 (per CacheReqBundle fragment)
        std::memcpy(backingPtr_ + addr, &req.data.read(), std::min<size_t>(sz, 8));
        ++stats_requests_write_;
    } else {
        // 读: 直接 memcpy backing 到 resp.data
        uint64_t val = 0;
        std::memcpy(&val, backingPtr_ + addr, std::min<size_t>(sz, 8));
        resp.data.write(val);
        ++stats_requests_read_;
    }
    resp_out_.write(resp);
    req_in_.consume();

    // 保留 StreamAdapter 转发 (与既有 tick 一致, 不破坏 ChStream 链路)
    if (adapter_) adapter_->tick();
}
```

### 3.4 DGpuBoard 新增

```cpp
// include/tlm/gpu/dgpu_board_shell.hh (新增成员)
class DGpuBoard {
    // ... 既有 v2.0.2 API 不变 ...

    // 新增: framebuffer_ (单一 backing 源, gem5 PhysicalMemory 角色)
    std::vector<uint8_t> framebuffer_;
    uint64_t framebuffer_size_ = 0;

    // 新增: backdoor_read / backdoor_write 直接读写 framebuffer_ (替代 vram_segments_ 主路径)
    int backdoor_read(uint64_t offset, void* buf, size_t len);
    int backdoor_write(uint64_t offset, const void* buf, size_t len);

    // 新增: 在 init() 阶段注入 backing 到所有访存者
    void bind_memory_backings();
};

// src/tlm/gpu/dgpu_board_shell.cc (新增 init 片段)
void DGpuBoard::init() {
    // ... 既有 init 逻辑 (load_soc_config 已派 framebuffer_size_ from bar_sizes[1]) ...

    // 1. 分配 framebuffer (必须在所有 set_backing_* 之前, 保证指针稳定)
    if (framebuffer_.empty()) {
        framebuffer_.resize(framebuffer_size_, 0);
    }

    // 2. 绑定 backing 到访存者
    bind_memory_backings();

    // 3. ... 既有 sim_thread_ 启动逻辑 ...
}

void DGpuBoard::bind_memory_backings() {
    if (!soc_) return;
    if (auto* mem = dynamic_cast<MemoryTLM*>(soc_->getInternalInstance("memory"))) {
        if (mem) mem->set_backing_store(framebuffer_.data(), framebuffer_size_);
    }
    if (auto* sdma = dynamic_cast<SdmaEngineTLM*>(soc_->getInternalInstance("sdma"))) {
        constexpr uint64_t kSdmaVramOffset = 0;  // v1.0 简化: SDMA VRAM 起点 = BAR1 起点
        if (sdma) {
            sdma->set_vram_backdoor(framebuffer_.data() + kSdmaVramOffset,
                                     framebuffer_size_ - kSdmaVramOffset);
            sdma->set_translate_cb([this](uint64_t iova, uint32_t size, uint64_t& phys) {
                return gmmu_->translate(iova, size, phys);
            });
        }
    }
    if (auto* gmmu = dynamic_cast<GmmuTLM*>(soc_->getInternalInstance("gmmu"))) {
        if (gmmu) gmmu->set_backing(framebuffer_.data(), framebuffer_size_);
    }
}
```

### 3.5 `vram_segments_` 兼容策略

`DGpuBoard::vram_segments_` (稀疏 map) 保留作为**fallback**——D2 PcieMemoryDevice 提案之外没有竞争场景。新 `backdoor_read/write` **优先**走 `framebuffer_` 连续路径,失败才回落到 segments map。`vram_segments_` 数据迁移在 v2.1 评估。

### 3.6 Coherence 保证

**单一真源** + **零时路径** + **不缓存** = 天然 coherence:

- device ChStream 写 → `MemoryTLM::tick()` → memcpy 到 `framebuffer_`(同一份)
- host BAR1 写 → `DGpuBoard::mmio_write` → PcieStorage route → `MemoryTLM::backing` 直写
- host backdoor_read → `DGpuBoard::backdoor_read` → `framebuffer_.data()` 直读
- GMMU 页表读 → `framebuffer_.data() + PT_BASE + idx*8` 直读

无 cache、无 flush、无锁(同一线程访问无需 lock)。如未来加多线程访问,需补 mutex。

---

## 4. BAR 路由与读写路径

### 4.1 BAR 布局

| BAR | 大小 | 偏移 | 路由目标 | 备注 |
|-----|------|------|---------|------|
| **BAR0** | 4 KB | 0x00 | GMMU_PT_BASE_LO (RW) | 一级页表物理基址低 32 位 |
| | | 0x04 | GMMU_PT_BASE_HI (RW) | 高 32 位(64-bit PT_BASE) |
| | | 0x08 | GMMU_CTRL (RW) | bit0: enable |
| | | 0x10 | SDMA_STATUS (RO) | doorbell 计数 |
| **BAR1** | 1 GB | [0x0, 0x10010000) | MemoryTLM (framebuffer_ 直读直写) | 存储主区域 |
| | | [0x10010000, 0x10010008) | SdmaEngineTLM (doorbell wptr) | 门铃 8 字节窗口 |
| | | (0x10010008, 1GB] | MemoryTLM (framebuffer_ 直读直写) | doorbell 页 dead bytes |

### 4.2 BAR 路由表

| Host 操作 | DGpuBoard 路由路径 | 命中 |
|----------|---------------------|------|
| `mmio_write(BAR0, off)` | PcieBarRouter → DGpuBoard BAR0 hook → GmmuTLM::mmio_write | GMMU 寄存器 |
| `mmio_read(BAR0, off)` | PcieBarRouter → DGpuBoard BAR0 hook → GmmuTLM::mmio_read | GMMU / SDMA_STATUS |
| `mmio_write(BAR1, off ∈ [0, 1GB))` | **新建 PcieStorage route** → MemoryTLM 直写 framebuffer_ | 存储空间 |
| `mmio_read(BAR1, off ∈ [0, 1GB))` | **新建 PcieStorage route** → MemoryTLM 直读 framebuffer_ | 存储空间 |
| `mmio_write(BAR1, off == 0x10010000)` | 既有 doorbell 路径(shell:313,353-373)→ sdma_engine_->mmio_write(1, kBar1DoorbellOffset, data) | SDMA ring consume |
| `mmio_read(BAR1, off == 0x10010000)` | 既有 doorbell 路径 → 返回 last wptr | SDMA 状态 |
| `host backdoor_read/write` | `DGpuBoard::backdoor_*` 直读直写 framebuffer_ | 旁路 ChStream |

### 4.3 双 ingress 路径钩点

BAR1 写入有两条 ingress,需**两条都改造**:

1. **ABI 路径** (`DGpuBoard::mmio_write`,shell:313,353):既有 doorbell 检测,新增 PcieStorage 检测补全
2. **TLP/AXI 路径** (`PcieEndpointIP::mmio_write` + `tick()` AXI slave, pcie_endpoint_ip.cc:584-598):当前 BAR1 非 doorbell 写落入 `bar_store_` 稀疏 map,本期须改造为转发到 framebuffer_

v1.0 优先实现 ABI 路径(测试驱动 host 直连),TLP/AXI 路径在 v1.1 跟进。

### 4.4 BAR0 寄存器写 → GmmuTLM 传播

`PcieBarRouter::mmio_write` 只更新内部 `regs_` 表,无机制传到 `GmmuTLM::set_pt_base`。**v1.0 方案**:

```cpp
// DGpuBoard::mmio_write BAR0 路径 (在 PcieBarRouter 写成功后追加):
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    // ... 既有 lifecycle guard ...
    
    int rc = dispatch_registry_.dispatch_write(bar, offset, buf, len);  // per v2.0.2 P0-1
    if (rc != 0) return rc;
    
    // BAR0 路由后续: 转发 GMMU 寄存器写
    if (bar == 0 && offset < 0x100 && gmmu_) {
        uint32_t val32;
        if (len >= 4) std::memcpy(&val32, buf, 4);
        else return -EINVAL;
        
        if (offset == 0) { gmmu_->set_pt_base_lo(val32); return 0; }
        if (offset == 4) { gmmu_->set_pt_base_hi(val32); return 0; }
        if (offset == 8) { gmmu_->set_enabled(val32 & 1); return 0; }
    }
    
    // ... 既有 mmio_regs_ 镜像 + inject_q push ...
}
```

> 说明: 实际 GMMU 寄存器可能是 32 位(由 `RegisterEntry::value` 决定),PT_BASE 64-bit 需 2 个 32-bit 寄存器拼装。

---

## 5. GMMU 设计(新建,一级页表)

### 5.1 API 骨架

```cpp
// include/tlm/gpu/gmmu_tlm.hh (新建)
class GmmuTLM : public SimModule {
public:
    explicit GmmuTLM(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    ~GmmuTLM() override = default;
    
    std::string get_module_type() const override { return "GmmuTLM"; }
    
    // ── 注册 (DGpuBoard::bind_memory_backings 调用) ──
    void set_backing(uint8_t* ptr, uint64_t sz) noexcept {
        backing_ = ptr; backing_size_ = sz;
    }
    void set_pt_base_lo(uint32_t lo) noexcept { pt_base_lo_ = lo; }
    void set_pt_base_hi(uint32_t hi) noexcept { pt_base_hi_ = hi; }
    void set_enabled(bool en) noexcept { enabled_ = en; }
    
    uint64_t pt_base() const noexcept {
        return (uint64_t(pt_base_hi_) << 32) | uint64_t(pt_base_lo_);
    }
    
    // ── 翻译 API (镜像 DmaTranslateCb 签名) ──
    // 语义: iova → paddr 一次性翻译, 不缓存
    int translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        if (!enabled_ || !backing_ || pt_base() == 0) return -EIO;
        
        const uint64_t page_size = 4096;
        const uint64_t page_mask = page_size - 1;
        const uint64_t idx       = iova >> 12;
        const uint64_t pte_addr  = pt_base() + idx * 8;
        
        if (pte_addr + 8 > backing_size_) return -EIO;  // PTE 越界
        
        // 直接 memcpy backing 读 PTE
        uint64_t pte;
        std::memcpy(&pte, backing_ + pte_addr, sizeof(pte));
        
        const bool     valid  = (pte & 1ULL) != 0;
        const uint64_t  paddr  = pte & ~page_mask;     // 高位为 paddr, 低 12 位为 flags
        
        if (!valid) return -EIO;                      // PTE 无效
        
        out_paddr = paddr | (iova & page_mask);       // pa = pte.paddr | (iova & 0xFFF)
        return 0;
    }
    
    // ── 配置 (JSON → on_config_loaded) ──
    void on_config_loaded() override {
        // 从 cfg 读 page_size_bytes (默认 4096)
        // 留作 v1.1 扩展 (不同页大小需要 PTE 格式变更)
    }
    
    // ── MMIO 接口 (DGpuBoard BAR0 hook 调用) ──
    int mmio_read(uint32_t offset, uint64_t* value, bool is_write);
    
private:
    uint32_t  pt_base_lo_   = 0;
    uint32_t  pt_base_hi_   = 0;
    bool      enabled_      = false;
    uint8_t*  backing_      = nullptr;
    uint64_t  backing_size_ = 0;
};
```

### 5.2 翻译流程

```
host mmio_write(BAR0, GMMU_PT_BASE_LO, lo32)  → DGpuBoard::mmio_write → gmmu_->set_pt_base_lo()
host mmio_write(BAR0, GMMU_PT_BASE_HI, hi32)  → gmmu_->set_pt_base_hi()
host mmio_write(BAR0, GMMU_CTRL, enable=1)    → gmmu_->set_enabled(true)
host mmio_write(BAR1, PTE_array_addr, pte[])  → framebuffer_[PTE_array_addr..] 落页表
SDMA H2D/D2H DMA 请求
  → translate_cb(iova, size, &phys) → GmmuTLM::translate()
    idx = iova >> 12
    pte = *(framebuffer_.data() + PT_BASE + idx * 8)   // 直读 backing
    校验 valid
    pa = pte.paddr | (iova & 0xFFF)
    返回 phys
SDMA 用 phys + size 在 host_backdoor 范围内 memcpy
```

### 5.3 PTE 格式(8 字节)

```
bit 0      : valid (1 = 已映射)
bit 1-11   : reserved (0)
bit 12-63  : paddr (4KB-aligned)
```

### 5.4 升级路径(留待 v2.1)

- **多级页表**: 当前仅一级,后续可扩展 4 级(x86-64 风格)+ TLB
- **Page size**: 当前固定 4KB,后续可支持 2MB/1GB huge page
- **Context table**: 当前无 context 区分,后续可加(参考 GMMU MVP proposal)

---

## 6. SDMA / Fence / MSI-X 链路(全复用)

### 6.1 数据流

```
host mmio_write(BAR1, 0x10010000, wptr)
  → DGpuBoard::mmio_write 检测 kBar1DoorbellOffset
    → sdma_engine_->mmio_write(1, kBar1DoorbellOffset, wptr)   // 既有路径, 不变
      → SdmaEngineTLM::mmio_write (ring mode consume)
        → RPTR..WPTR 区间 descriptors
          → DmaDescriptor 解析
            → translate_cb_(iova, size, &phys)
              → GmmuTLM::translate(iova, size, phys)  // v1.0 注入点
            → memcpy(vram_backdoor_+vram_offset, host_backdoor_+phys, size)  // framebuffer_ 上搬运
            → fence → submit_fence() → process_fence_queue()
              → completion_ring_->push(entry)
                → DGpuBoard::sdma_fence_complete()
                  → msix_update_pending(kSdmaFenceVector=0)
                    → host irq_cb_(0)
```

### 6.2 SDMA 接线(详见 §3.4 bind_memory_backings)

```cpp
sdma->set_vram_backdoor(framebuffer_.data() + kSdmaVramOffset,
                         framebuffer_size_ - kSdmaVramOffset);
sdma->set_translate_cb([this](uint64_t iova, uint32_t size, uint64_t& phys) {
    return gmmu_->translate(iova, size, phys);
});
```

### 6.3 复用 vs 新增

| 项 | 来源 | v1.0 改动 |
|----|------|----------|
| `set_translate_cb` | 既有(ADR-DGPU-01 §2.1/§2.4) | 仅注入 GmmuTLM::translate(无 API 变化) |
| `set_vram_backdoor` | 既有(SdmaEngineTLM §5.4) | 仅指向 framebuffer_(无 API 变化) |
| `submit_fence` / `set_completion_ring` | 既有 | 不变 |
| DGpuBoard::sdma_fence_complete | 既有(dgpu_board_shell.cc:718-721) | 不变 |

---

## 7. JSON 配置

### 7.1 最小 SoC 配置示例

```json
{
  "name": "minimal_dgpu_soc_v1",
  "display_routing_enabled": false,
  "framebuffer_size_bytes": 1073741824,
  "modules": [
    {
      "name": "soc",
      "type": "DGpuSoc",
      "modules": [
        {
          "name": "pcie_ep",
          "type": "PcieEndpointIP",
          "params": {
            "bar_sizes": [4096, 1073741824],
            "bar0_registers": [
              { "offset": 0,  "name": "GMMU_PT_BASE_LO", "access": "rw" },
              { "offset": 4,  "name": "GMMU_PT_BASE_HI", "access": "rw" },
              { "offset": 8,  "name": "GMMU_CTRL",      "access": "rw" },
              { "offset": 16, "name": "SDMA_STATUS",    "access": "ro" }
            ]
          }
        },
        {
          "name": "memory",
          "type": "MemoryTLM",
          "params": { "capacity_gb": 1 }
        },
        {
          "name": "sdma",
          "type": "SdmaEngineTLM",
          "params": { "vram_size_bytes": 1073741824 }
        },
        {
          "name": "gmmu",
          "type": "GmmuTLM",
          "params": { "page_size_bytes": 4096 }
        },
        {
          "name": "completion",
          "type": "CompletionRingTLM"
        }
      ]
    }
  ]
}
```

### 7.2 JSON 字段说明

| 字段 | 位置 | 含义 |
|------|------|------|
| `display_routing_enabled` | 顶层 | D1 显示路由开关,v1.0 设为 false |
| `framebuffer_size_bytes` | 顶层 | 可选 override,默认从 `bar_sizes[1]` 派生 |
| `bar_sizes` | pcie_ep.params | `[BAR0字节数, BAR1字节数]` |
| `bar0_registers` | pcie_ep.params | BAR0 MMIO 寄存器表(offset/name/access) |
| `capacity_gb` | memory.params | MemoryTLM 容量(MemoryTLM::on_config_loaded 读) |
| `vram_size_bytes` | sdma.params | SDMA VRAM 窗口大小(必须 ≥ framebuffer_size_) |
| `page_size_bytes` | gmmu.params | GMMU 页大小(v1.0 固定 4096,参数保留以备扩展) |

### 7.3 JSON 层级约束

⚠️ **顶层 `modules` 数组的第一个元素必须是 DGpuSoc 节点**,这是 `DGpuBoard::load_soc_config` 的硬约束(`dgpu_board_shell.cc:74-77`)。否则 `simulate_instantiate` 只处理 `modules[0]`,其余模块被静默丢弃。

### 7.4 GmmuTLM 注册

```cpp
// include/modules_cluster.hh 末尾添加
#include "tlm/gpu/gmmu_tlm.hh"
const bool _reg_gmmutlm = (REGISTER_MODULE(GmmuTLM), true);
```

---

## 8. 关键不变性(Invariants)

### Inv-1: 单一 backing 真源

framebuffer_ 是唯一存储真源。device(ChStream)、host backdoor、GMMU 页表读、SDMA VRAM 搬运都直读直写同一份 `std::vector<uint8_t>`。无 cache、无 flush、无 lock(单线程访问)。

### Inv-2: 指针稳定性

`framebuffer_.resize()` 必须在所有 `set_backing_*` 调用之前。resize 后 `data()` 失效;因此:
- init 顺序: (a) resize → (b) bind_memory_backings() → (c) start sim_thread_

### Inv-3: BAR1 路由优先级

`mmio_write(BAR1, off)`:
- `off == kBar1DoorbellOffset` (8 字节窗口) → sdma_engine_
- 否则 → PcieStorage route → framebuffer_

`off > kBar1DoorbellOffset + 8` 范围在 framebuffer_ 中为 dead bytes(可接受,设计明确)。

### Inv-4: GMMU PT_BASE 原子性

PT_BASE 是 64-bit,拆为 LO/HI 两寄存器。host 必须**先写 LO 再写 HI** 两次。期间 `pt_base()` 返回的中间态(LO 已更新 / HI 未更新)不应被 GMMU translate 使用——v1.0 接受此 race(简单实现),v2.1 可加写锁或合并为 64-bit MMIO 读写。

### Inv-5: translate_cb 签名

```cpp
using DmaTranslateCb = std::function<int(uint64_t iova, uint32_t size, uint64_t& phys)>;
```

GmmuTLM::translate 必须严格匹配此签名(`uint32_t size`, `uint64_t& phys`,返回值 0/-EIO)。

### Inv-6: 零时路径

`MemoryTLM::tick()` 内 `memcpy` 直读直写 backing,**无延迟、无 stats_latency**。与 v1.1+ 计时模式的区别在于 latency 字段。保留 `stats_requests_read_/write_` 计数。

---

## 9. 实施路径(4 阶段,3 周)

| 阶段 | 任务 | 估时 |
|------|------|------|
| **A1** | `MemoryTLM::set_backing_store` + `on_config_loaded` + 零时 tick | 1d |
| **A2** | `DGpuBoard::framebuffer_` + `backdoor_read/write` 走 framebuffer_ 路径(替代 vram_segments_ 主路径) | 1d |
| **A3** | `GmmuTLM` 骨架 + 一级页表 translate() + 模块注册 | 1d |
| **B1** | DGpuBoard init 注入 framebuffer_ → MemoryTLM + SDMA + GmmuTLM | 1d |
| **B2** | BAR1 PcieStorage 路由(ABI 路径) + 既有 doorbell 路径保留 | 0.5d |
| **B3** | BAR0 GMMU 寄存器写 → GmmuTLM::mmio_write 转发 | 0.5d |
| **C1** | 单元测试: MemoryTLM backdoor 隔离、GMMU 页表遍历、SDMA translate_cb 注入 | 1d |
| **C2** | E2E 测试: host 写 PT_BASE → SDMA H2D → framebuffer_ 数据校验 + BAR1 直读直写 | 1d |
| **C3** | (可选) TLP/AXI 路径 PcieStorage 路由(v1.1 跟进项) | 0d(本期不实施) |

**总计**: 7 人日 ≈ 1.5 周(单 dev)/3 周(含评审与 buffer)

---

## 10. 测试策略

### 10.1 单元测试

| 模块 | 测试用例 |
|------|---------|
| `MemoryTLM` | (1) backing=nullptr 时返 OUT_OF_RANGE (2) tick 写入后 backdoor 读出相同字节 (3) tick 读 → resp.data 等于 backing 内容 (4) stats_requests_read_/write_ 计数正确 |
| `GmmuTLM` | (1) disabled → -EIO (2) pt_base=0 → -EIO (3) valid PTE → 正确 paddr (4) invalid PTE → -EIO (5) PTE 越界 → -EIO (6) set_pt_base_lo/hi 正确组装 |
| `DGpuBoard::backdoor` | (1) 直读 framebuffer_ (2) 直写 framebuffer_ (3) 越界返 -EINVAL |
| BAR1 路由 | (1) mmio_write 非 doorbell → framebuffer_ 字节相同 (2) mmio_write doorbell → sdma ring consume (3) mmio_read 非 doorbell → framebuffer_ 字节 |

### 10.2 E2E 测试

```cpp
TEST_CASE("minimal-dgpu-soc: host write PT_BASE + SDMA H2D + BAR1 backdoor readback") {
    // 1. 加载 JSON
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(minimal_cfg));
    REQUIRE(board.init());
    
    // 2. 准备 host 端 4KB 数据 (host_backdoor 模拟)
    std::vector<uint8_t> host_buf(4096, 0);
    for (int i = 0; i < 4096; ++i) host_buf[i] = i & 0xFF;
    board.set_host_backdoor(host_buf.data(), host_buf.size());
    
    // 3. 写入 PT_BASE + CTRL (host 侧页表准备)
    uint64_t pt_base = 0x10000;  // page table 放在 framebuffer_ 偏移 0x10000
    REQUIRE(board.mmio_write(0, 0, &pt_base, 4) == 0);  // LO
    REQUIRE(board.mmio_write(0, 4, ((uint8_t*)&pt_base)[4], 4) == 0);  // HI
    uint32_t ctrl = 1;
    REQUIRE(board.mmio_write(0, 8, &ctrl, 4) == 0);  // enable
    
    // 4. 写 PTE 到 framebuffer_ 的 pt_base 位置 (host 经 BAR1)
    struct { uint64_t pte; } __attribute__((packed)) pte = { 1ULL | (0x20000ULL << 12) };  // valid + paddr
    REQUIRE(board.mmio_write(1, pt_base, &pte, sizeof(pte)) == 0);  // 第 0 页 (iova=0)
    
    // 5. 触发 SDMA H2D: host_iova=0 → 应翻译到 paddr=0x20000 → 从 host 读 4096 字节 → 写入 VRAM 偏移 0
    auto* sdma = ...;
    DmaDescriptor desc(DmaDescriptor::Dir::H2D, /*iova=*/0, /*vram_offset=*/0, /*size=*/4096, /*tag=*/1);
    sdma->submit_descriptor(desc);  // 通过 wire-format 提交
    
    // 6. 等 SDMA 完成 (通过 MSI-X 或轮询)
    board.tick(); board.tick(); // ... 推进
    
    // 7. 验证: BAR1 读 VRAM 偏移 0 应等于 host_buf
    std::vector<uint8_t> readback(4096);
    REQUIRE(board.mmio_read(1, 0, readback.data(), 4096) == 0);
    REQUIRE(readback == host_buf);
    
    // 8. 验证: backdoor 读 framebuffer_ 也等于 host_buf
    std::vector<uint8_t> backdoor_buf(4096);
    REQUIRE(board.backdoor_read(0, backdoor_buf.data(), 4096) == 0);
    REQUIRE(backdoor_buf == host_buf);
}
```

### 10.3 回归测试

确保既有 44498 assertions + 23 ABI 字节级兼容 + DGpuBoard v2.0.2 单元测试全部 PASS。

---

## 11. 兼容性约束

| 项 | 影响 |
|----|------|
| 23 ABI 签名 | 0 修改(冻结) |
| 23 ABI 语义 | 0 修改(BAR0 寄存器新增为扩展语义,不破坏既有 PcieBarRouter 用法) |
| DGpuBoard v2.0.2 API | 0 修改(仅新增 framebuffer_ + bind_memory_backings + backdoor_*) |
| SdmaEngineTLM API | 0 修改(仅 set_translate_cb/set_vram_backdoor 注入) |
| MemoryTLM 公共 API | 0 修改(REGISTER_CHSTREAM 不变,仅扩展实现) |
| JSON schema | 扩展(顶层 framebuffer_size_bytes, modules 内新增 GmmuTLM type) |
| 既有配置 | 兼容(GMMU_CTLR.enable 默认 0,无 framebuffer_size_bytes 时从 bar_sizes[1] 派生) |

---

## 12. 度量与验证

### 12.1 完成定义(DoD)

- [ ] `MemoryTLM::set_backing_store()` + `on_config_loaded()` + 零时 tick 实现
- [ ] `DGpuBoard::framebuffer_` + `backdoor_read/write` 走 framebuffer_ 路径
- [ ] `GmmuTLM` 完整实现 + `REGISTER_MODULE` 注册
- [ ] BAR1 PcieStorage 路由 + doorbell 8 字节窗口保留
- [ ] BAR0 GMMU 寄存器写 → GmmuTLM 转发(PT_BASE_LO/HI + CTRL)
- [ ] SDMA `set_translate_cb` + `set_vram_backdoor` 接线到 framebuffer_
- [ ] 单元测试 100% PASS(MemoryTLM + GmmuTLM + DGpuBoard backdoor + BAR1 路由)
- [ ] E2E 测试 100% PASS(host → PT_BASE → SDMA → framebuffer_ 回路)
- [ ] 既有 44498 assertions 100% PASS(0 regression)
- [ ] 23 ABI 字节级兼容(`git diff HEAD -- include/abi/cpptlm_emulator.h` 仅含 24 号 ABI 末尾追加块)

### 12.2 性能目标

| 指标 | 目标 |
|------|------|
| BAR1 直读直写延迟 | < 1us(零时 memcpy) |
| SDMA H2D 4KB 翻译开销 | < 5us(GMMU 一级页表 1 次 PTE 读) |
| host backdoor_read 吞吐 | > 1 GB/s(vector 直接 memcpy) |
| framebuffer_ 内存开销 | 默认 1GB(可配置,v2.1 评估 mmap 大页优化) |

---

## 13. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: PT_BASE LO/HI 写 race(中间态被 translate) | 低 | 偶发翻译错误 | 接受(v1.0);v2.1 加写锁 |
| **R2**: 1GB framebuffer_ 内存占用大 | 低 | 测试慢 | 默认 256MB,v1.1 评估 mmap |
| **R3**: doorbell 页 dead bytes 浪费 8 字节 | 低 | 无 | 接受(对齐 8 字节,小) |
| **R4**: TLP/AXI 路径 BAR1 写入未钩到 framebuffer_ | 中 | E2E 测试覆盖不全 | v1.0 仅 ABI 路径,v1.1 补 |
| **R5**: GMMU 单线程访问,加多线程需补锁 | 低 | race | v1.0 单线程,v2.1 加 mutex |
| **R6**: framebuffer_.resize() 后续指针失效 | 中 | 静默 UAF | Inv-2 强制 resize→set_backing 顺序 |
| **R7**: 完成性 vram_segments_ → framebuffer_ 迁移路径 | 低 | 数据丢失 | 保留 vram_segments_ 作 fallback |

---

## 14. 已知推迟项(v2.1 backlog)

| 项 | 来源 | 说明 |
|----|------|------|
| **GMMU 多级页表 + TLB** | Oracle 一轮报告 | 当前仅一级,详见 `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/` |
| **PcieStorage TLP/AXI 路径钩** | Oracle 二轮 §1c | v1.0 仅 ABI 路径 |
| **BAR1 routing doorbell carve-out 完整化** | Oracle 二轮 §1c | 当前 8 字节 dead bytes,v2.1 优化 |
| **D2 PcieMemoryDevice 提案** | OpenSpec | 替代/互补 framebuffer_ 路线 |
| **CP 模块** | 用户要求 | 本期不做 |
| **framebuffer_ mmap 大页** | R2 | 1GB 内存优化 |
| **FrameBuffer → vram_segments_ 数据迁移** | R7 | 兼容性收尾 |

---

## 15. 参考

- **DGpuBoard 架构文档**: `docs/architecture/14-dgpu-board-ideal-arch.md` (v2.0.2)
- **ADR-DGPU-01~04**: callback 单一所有权、LifecycleProtocol、DispatchRegistry、命名空间统一
- **ADR-088**: 23 ABI 字节级冻结(`include/abi/cpptlm_emulator.h`)
- **gem5 `AbstractMemory` / `PhysicalMemory`**: 单一 backing 源 + 注入模式
  - `https://github.com/gem5/gem5/blob/stable/src/mem/abstract_mem.{hh,cc}`
  - `https://github.com/gem5/gem5/blob/stable/src/mem/physical.{hh,cc}`
  - `https://github.com/gem5/gem5/blob/stable/src/mem/backdoor.hh`
  - `https://github.com/gem5/gem5/blob/stable/src/mem/simple_mem.{hh,cc}`
- **GMMU MVP Proposal**: `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/`(完整版多级页表 + TLB 路线)
- **D2 PcieMemoryDevice Proposal**: `openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/`
- **D1 PcieDisplayDevice**: `include/tlm/gpu/pcie_display_device.{hh,cc}`(framebuffer_ 模板)
- **SdmaEngineTLM**: `include/tlm/gpu/sdma_engine_tlm.{hh,cc}`(translate_cb + vram_backdoor API)
- **MemoryTLM 现有实现**: `include/tlm/memory_tlm.hh`(144 行,纯头文件)
- **DGpuBoard v2.0.2**:`include/tlm/gpu/dgpu_board_shell.{hh,cc}`

---

## 16. 文档维护

- **版本**: v1.0(Oracle 二轮复审后定稿)
- **最后更新**: 2027-02-09
- **下次评审触发点**:
  - Phase A 完成后(实施层偏差校验)
  - Phase B 完成后(架构落地校验)
  - Phase C 测试覆盖后(测试策略有效性)
- **下游依赖**:
  - 实施任务表 (`openspec/changes/<name>/tasks.md` 待开)
  - 测试用例 (`test/test_minimal_dgpu_soc*.cc` 待开)
  - ADR (若需正式决策记录,补 ADR-DGPU-05)

---

**Owner**: CppTLM Team
**版本**: v1.0
**最后更新**: 2027-02-09