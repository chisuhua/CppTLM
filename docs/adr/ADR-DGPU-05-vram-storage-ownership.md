# ADR-DGPU-05: VRAM backing 单一所有权归 DGpuBoard (vram_storage_)

> **状态**: 📋 提案 (基于 D-AXI v1.4 B7 根因决策, 待 D-AXI v1.4 实施通过后正式签发)
> **日期**: 2026-09-26
> **对应 D-AXI 修正**: **B7** (v1.4 架构根因) + **B8** (bound 语义) + **B10** (mutex 保留) + **B15** (强制删除 framebuffer_storage_)
> **关联架构文档**: [../designs/dgpu-board/architecture.md §DGpuBoard 新增 framebuffer_](../designs/dgpu-board/architecture.md), [../designs/dgpu-soc/architecture.md §3 存储子系统](../designs/dgpu-soc/architecture.md)
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前, B7-B15 落地)
> **配套 ADR**: [ADR-DGPU-01](ADR-DGPU-01-callback-worker-replaces-detached-threads.md) (CallbackWorker 单一所有权范式)

---

## 1. 背景

### 1.1 问题

D-AXI v1.0-v1.3 阶段，VRAM backing 在多个组件**重复存储**：

| 组件 | 持有 backing 类型 | 来源 |
|------|------------------|------|
| `DGpuBoard::framebuffer_` | `std::vector<uint8_t>` (主源) | 自分配 |
| `PcieMemoryDevice::memory_backing_` | 独立分配 | 自分配 |
| `MemoryTLM::backingPtr_` | 独立分配 | 自分配 |
| `SdmaEngineTLM::vram_backdoor_` | 独立分配 | 自分配 |
| `GmmuTLM::backing_` | 独立分配 | 自分配 |

5 个独立存储 → 数据竞争 / 边界漂移 / 测试同步负担。

### 1.2 Oracle/Metis 评审结论

经过 D-AXI 5 轮评审（B1-B28 + F1-F12 + H1-H7），Oracle 第四轮 + Metis 二轮独立命中 **vram_segments_ 双存储**（B14）、**framebuffer_storage_ 矛盾**（B15）、**attach_framebuffer_for_testing 优先级缺失**（B16）、**消费者计数漂移**（F9）等 11 项隐藏缺陷。

**根因**：分散所有权导致"哪份是真源"语义不清。

### 1.3 现状约束

- 5 消费者路径（backdoor / BAR1 fast-path / BAR2 via memory_device / MemoryTLM backing / SDMA-GMMU legacy ptr）必须共享同一份内存
- 不可破坏 ABI：23 ABI 函数字节级冻结（per ADR-088 §D5）
- 不可破坏现有 [sdma] 测试套件：24 case 需机械迁移而非逻辑修改
- Linux 内核 lazy commit 语义：host backdoor 访问未提交页不应触发 OOM

---

## 2. 决策

引入 **`DGpuBoard::vram_storage_`** 作为 SoC 全部访存的**唯一真源**：

```cpp
// include/tlm/gpu/dgpu_board_shell.hh (DGpuBoard 新增成员)
class DGpuBoard {
    // ── 新增: vram_storage_ (单一 backing 源, gem5 PhysicalMemory 角色) ──
    std::unique_ptr<uint8_t[]> vram_storage_;   // 8GB default-init, Linux lazy commit
    uint64_t                    vram_size_      = 0;  // 来自 bar_sizes[2]
    uint64_t                    bar1_window_size_ = 0;  // 来自 bar_sizes[1]
    std::mutex                  backing_mutex_;  // host/sim 并发保护 (per B10)
};
```

**所有权规则**：
1. `vram_storage_` **唯一所有权归 DGpuBoard**（per B7）
2. 其他组件（MemoryTLM / SDMA / GMMU / PcieMemoryDevice / backdoor）**通过指针注入访问**，不持有分配权
3. `set_backing_store(ptr, size)` 镜像 SdmaEngineTLM::set_host_backdoor 模式（per MemoryTLM v2.2）
4. `set_backing_store` **仅在 sim_thread_ 启动前调用一次**（禁止运行时再注入）

**5 消费者共享 vram_storage_**：

| 消费者 | 访问路径 | bound | 来源 |
|--------|---------|-------|------|
| `DGpuBoard::backdoor_read/write` | 直读直写 `vram_storage_.get()` | `vram_size_` | driver ABI + 测试 |
| BAR1 fast-path (`mmio_read/write(1, ...)`) | 直读直写 `vram_storage_.get()` | `bar1_window_size_` | driver MMIO |
| BAR2 via PcieMemoryDevice | `set_backing_store` 注入 | `vram_size_` | driver MMIO BAR2 |
| MemoryTLM backing | `set_backing_store` 注入 | `vram_size_` (per B8) | CPU/cache 路径 |
| SDMA / GMMU legacy ptr | 注入 raw pointer | `vram_size_` (per B19) | DMA + 翻译 |

### 2.1 替代方案权衡

| 方案 | 优势 | 劣势 | 决定 |
|------|------|------|------|
| **A. DGpuBoard 单一所有权（采纳）** | 5 消费者共享无歧义；Linux lazy commit 语义清晰；与 gem5 `PhysicalMemory` 范式一致 | DGpuBoard 内部变重；需 mutex 保护 | ✅ |
| B. PcieMemoryDevice 持有所有权 | D2 路径直读 | MemoryTLM/SDMA/GMMU 路径需双跳转发；D2 v1.1 已淘汰 | ❌ |
| C. 全局静态 single-ton | 无所有权传递 | 多 SoC 实例化不可行；测试隔离差 | ❌ |
| D. shared_ptr 共享 | API 友好 | 隐式所有权变更；测试 mocking 困难 | ❌ |

---

## 3. 关键不变性 (Invariants)

### Inv-1: 5 消费者共享同一 vram_storage_ 指针

**Where**: `dgpu_board_shell.cc::bind_memory_backings()`

```cpp
void DGpuBoard::bind_memory_backings() {
    if (!soc_) return;
    // PcieMemoryDevice (F4: 仅 backing, 无 registers_)
    if (auto* mem = dynamic_cast<PcieMemoryDevice*>(soc_->getInternalInstance("pcie_memory"))) {
        mem->set_backing_store(vram_storage_.get(), vram_size_);
    }
    // MemoryTLM (CPU/cache 路径)
    if (auto* mem = dynamic_cast<MemoryTLM*>(soc_->getInternalInstance("memory"))) {
        mem->set_backing_store(vram_storage_.get(), vram_size_);
    }
    // SDMA (DMA 搬运)
    if (auto* sdma = dynamic_cast<SdmaEngineTLM*>(soc_->getInternalInstance("sdma"))) {
        sdma->set_vram_backdoor(vram_storage_.get(), vram_size_);
    }
    // GMMU (页表读取)
    if (auto* gmmu = dynamic_cast<GmmuTLM*>(soc_->getInternalInstance("gmmu"))) {
        gmmu->set_backing(vram_storage_.get(), vram_size_);
    }
}
```

**测试**：`[minimal_dgpu_soc][driver_visible]` E2E + `[pcie-memory]` 24 cases

### Inv-2: 唯一分配权在 DGpuBoard

**Where**: `DGpuBoard::init()`

```cpp
void DGpuBoard::init() {
    if (!vram_storage_) {
        vram_storage_ = std::unique_ptr<uint8_t[]>(new uint8_t[vram_size_]);  // default-init
    }
    bind_memory_backings();  // 必须在 sim_thread_ 启动前
    sim_thread_ = std::thread(&DGpuBoard::tick, this);
}
```

**测试**：`assert(vram_storage_.get() != nullptr)` post-init

### Inv-3: PcieMemoryDevice 不持有 backing

**Where**: `pcie_memory_device.{hh,cc}` (per v1.4 B7)

```cpp
class PcieMemoryDevice : public SimModule {
    // 已删除: std::unique_ptr<uint8_t[]> memory_backing_;  // v1.4 B7 移除
    uint8_t* backing_ptr_ = nullptr;     // 注入, 不持有
    uint64_t backing_size_ = 0;         // 注入
public:
    void set_backing_store(uint8_t* ptr, uint64_t size) noexcept {
        backing_ptr_  = ptr;
        backing_size_ = size;
    }
    // memory_read/write 返回 -ENODEV 若 backing_ptr_ == nullptr (per B12)
};
```

**测试**：`assert(pcie_memory->memory_backing_ == nullptr)` (编译期不存在)

### Inv-4: Linux lazy commit + stable pointer

**Where**: `DGpuBoard::init()`

```cpp
// std::unique_ptr<uint8_t[]> default-init, 禁 vector::resize / 禁 new uint8_t[N]() 零初始化
vram_storage_ = std::unique_ptr<uint8_t[]>(new uint8_t[vram_size_]);
```

**特性**：
- default-init（不调用 T 构造函数）：访问未提交页时 Linux 内核 lazy commit 0 页
- stable pointer：地址永不变（其他 4 消费者持有 raw pointer 安全）

**测试**：`assert(vram_storage_.get() == pre_init_address)` post-init

### Inv-5: bound = injected backing_size_

**Where**: 所有消费者 `memory_read/write` / `set_backing_store` / `set_vram_backdoor`

```cpp
// PcieMemoryDevice
int memory_read(uint64_t off, void* buf, size_t len) {
    if (!backing_ptr_) return -ENODEV;                    // B12
    if (off + len > backing_size_) return -EIO;            // B8
    std::memcpy(buf, backing_ptr_ + off, len);
    return 0;
}
// MemoryTLM / SDMA / GMMU 类似
```

**测试**：`assert(memory_read(out_of_range) == -EIO)` + `assert(memory_read(invalid_offset) == -EIO)`

---

## 4. 实施步骤

per D-AXI tasks.md **B7-B15 + B19** (v1.4 架构根因 + v1.5 隐藏缺陷):

| 步骤 | 任务 | 位置 |
|------|------|------|
| 1 | DGpuBoard::vram_storage_ 引入 (8GB default-init) | dgpu_board_shell.{hh,cc} |
| 2 | bind_memory_backings() 5 消费者注入 | dgpu_board_shell.cc |
| 3 | PcieMemoryDevice::memory_backing_ 删除 (B7) | pcie_memory_device.{hh,cc} |
| 4 | 强制删除 framebuffer_storage_ (B15) | dgpu_board_shell.cc |
| 5 | 消灭 vram_segments_ MAP (B14) | dgpu_board_shell.cc |
| 6 | attach_framebuffer_for_testing 优先级规则 (B16) | dgpu_board_shell.{hh,cc} |
| 7 | mutex backing_mutex_ 保留 (B10) | dgpu_board_shell.hh |
| 8 | 双 size 字段 bar1_window_size_ vs vram_size_ (B11) | dgpu_board_shell.hh |
| 9 | -ENODEV + kRegMemSizeLo/Hi 读 injected size (B12) | pcie_memory_device.cc |
| 10 | 24-case [pcie-memory] 机械迁移 (B13) | test/pcie/*.cc |
| 11 | SDMA vram_size_bytes 由 board 注入 (B19) | sdma_engine_tlm.cc |

**预计工时**: 5-7 工作日 (per t1.md Phase 10)

---

## 5. 测试策略

### 5.1 单元测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_vram_storage_init | unique_ptr 初始化 + stable pointer | `[vram][storage]` |
| test_bind_memory_backings | 5 消费者均注入正确指针 | `[vram][binding]` |
| test_pcie_memory_no_own | PcieMemoryDevice 无 memory_backing_ 字段 | `[pcie-memory][compile]` |
| test_bound_injected | memory_read/write bound = injected size | `[vram][bound]` |

### 5.2 集成测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_minimal_soc_driver_visible_e2e | driver 经 15 ABI 闭环 | `[minimal_dgpu_soc][driver_visible][e2e]` |
| test_pcie_memory_suite_24 | 24 case [pcie-memory] 套件迁移 | `[pcie-memory]` |
| test_h2d_d2h_d2d_via_15_abi | 三类典型操作 | `[sdma][h2d][d2h][d2d]` |

### 5.3 回归测试

- 既有 `[sdma]` 套件零逻辑改动 (per v1.3 B6 dual-mode)
- `[minimal_dgpu_soc]` baseline 41 assertions + `[abi][minimal_dgpu_soc]` 28 assertions 全绿
- D-AXI Phase 0 baseline 66951 assertions 全绿保持

---

## 6. 兼容性

### 6.1 ABI 兼容性

- 签名级 0 diff（per ADR-088 §D5 + v2.0.2 P0-3e DoD 仲裁）
- 二进制级 0 diff（更严格）
- 冻结面 4 个头文件零 diff：pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh / cpptlm_emulator.h

### 6.2 JSON 配置兼容性

- minimal_v1 JSON 顶层 `framebuffer_size_bytes` 可 override `vram_size_`
- 默认从 `pcie_ep.params.bar_sizes[2]` 派生

### 6.3 测试兼容性

- `[sdma]` dual-mode 测试零逻辑改动（`set_vram_backdoor()` 保留为无条件 dual-mode API per v1.3 B6）
- `[pcie-memory]` 24 case 机械迁移而非逻辑重写

---

## 7. 风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| host/sim 并发数据竞争 | 🔴 H | `backing_mutex_` 保护 (per B10) |
| 双重分配 (memory_backing_ 未删干净) | 🔴 H | v1.5 B14/B15 强制验证；code review grep `memory_backing_` 零匹配 |
| 边界漂移 (framebuffer_size_ 复用为两个语义) | 🟡 M | B11 双 size 字段拆分 |
| 测试 mocking 困难 | 🟢 L | `attach_framebuffer_for_testing` 优先级规则 (B16) |
| 5 消费者数量漂移 | 🟡 M | F9 消费者计数统一为 "4 注入点 + 1 host backdoor" |

### Oracle/Metis 评审重点

- **Oracle 第四轮 H3**: host 内存无注入者 → 引入 N9 host_backdoor 注入（per v1.7 P0.35）
- **Metis 二轮**: "single VRAM 对 MemoryTLM 消费者为 `min(vram_size_, MemoryTLM.size_cap_)`"（per spec Requirement 显式声明）

---

## 8. 参考

- D-AXI OpenSpec change: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8)
- D-AXI design.md §1: 系统拓扑（v1.4 单一 VRAM 所有权）
- D-AXI design.md §X: 演进路线图（D3 VramController 替换 vram_storage_）
- D-AXI tasks.md §v1.4 P0: B7-B13 任务清单
- 实施笔记: [../../pcie/driver-visible-minimal-soc.md §2.2](../../pcie/driver-visible-minimal-soc.md)
- 实施计划: [../superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md Phase 10](../superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md)
- 配套 ADR: ADR-DGPU-01 (CallbackWorker 单一所有权范式)

---

## Status Update

### 2027-02-09 — ADR-DGPU-10 配套补充 (Oracle 5 步解锁链 Step 5)

本 ADR 已签发"单一 VRAM 真源"架构决策(章节 2),但**字段命名未统一**(5 个组件使用 4 种不同术语)。Oracle 评审 5 步解锁链 Step 5 触发配套 ADR:

**配套 ADR**: [ADR-DGPU-10 backing 命名约定 (owner / injected 两级)](ADR-DGPU-10-backing-naming-convention.md)

**关键映射**(本 ADR 决策 → ADR-DGPU-10 命名约定):

| 本 ADR §2 提到的字段 | ADR-DGPU-10 决策命名 | 角色 |
|---------------------|----------------------|------|
| `DGpuBoard::vram_storage_` | `*_storage_` (owner) | 保持(已是合规命名) |
| `PcieMemoryDevice::backing_ptr_` | `backing_view_` (injected-ChStream) | v1.4 B7 已部分实施,需完成 setter 重命名 |
| `MemoryTLM::backingPtr_` | `backing_view_` (injected-ChStream) | v1.1 rename |
| `GmmuTLM::backing_` | `mem_view_` (injected-functional) | v1.1 rename |
| `SdmaEngineTLM::vram_backdoor_` | `vram_backdoor_` (injected-functional) | ✅ 已合规 |
| `SdmaEngineTLM::host_backdoor_` | `host_backdoor_` (injected-functional) | ✅ 已合规 |

**关联文档修订**:
- [../designs/dgpu-soc/architecture.md](../designs/dgpu-soc/architecture.md) — §1.4 仿真模式声明 + 全文 `framebuffer_` → `framebuffer_storage_` 清理 (Oracle 5 步解锁链 Step 1-4 + 后续 cleanup)
- §4.1 BAR 布局: 1GB → 16MB (与 `dgpu_soc_minimal_v1.json` 对齐)
- §5 GMMU: 改为 "Functional Translation Service" (同步翻译服务,等价 gem5 atomic mode)
- §7.1 JSON 示例: 同步真实配置文件

**后续工作** (per ADR-DGPU-10 §4 Migration 8 步,工时 0.5-1 工作日):
1. 代码字段机械 rename: `framebuffer_storage_` → `vram_storage_` (DGpuBoard)
2. `MemoryTLM::backing_ptr_` → `backing_view_`
3. `GmmuTLM::backing_` → `mem_view_`
4. `set_backing_store` → `set_backing_view` (MemoryTLM)
5. 全量 grep audit + ctest 回归

**审查范围**: Oracle 5 步解锁链实施日期 2027-02-09;关联 commit 待签发后追加。

### 2027-09-29 — ADR-DGPU-10 §4 Migration 8 步实施完成 (Phase 2)

**实施 change**: `openspec/changes/cpptlm-minimal-dgpu-soc-v1-architecture` (T0-T6 全部完成)

**§3 不变性验证**:

| Invariant | 状态 | 验证 |
|-----------|------|------|
| Inv-3: PcieMemoryDevice 无 `memory_backing_` 字段 | ✅ | 已删除; 使用 `backing_view_` (uint8_t*) 注入 |
| Inv-5: bound = injected backing_size_ | ✅ | memory_read/write 使用 `backing_view_size_` 而非 `kDefaultMemSize` |

**5 消费者共享 vram_storage_ 验证** (全部指向同一地址):
- DGpuBoard::backdoor_read/write — `vram_storage_.get()` ✅
- BAR1 fast-path (mmio_read/write(1,...)) — `framebuffer_ptr_` (= vram_storage_.get()) ✅
- BAR2 via PcieMemoryDevice — `set_backing_view(vram_storage_.get(), ...)` ✅
- MemoryTLM backing — `set_backing_view(vram_storage_.get(), ...)` ✅
- SDMA/GMMU — `set_vram_backdoor` / `set_mem_view(vram_storage_.get(), ...)` ✅

**字段命名对齐** (per ADR-DGPU-10):
- `framebuffer_storage_` → `vram_storage_` (owner, unique_ptr)
- `MemoryTLM::backing_ptr_` → `backing_view_` (injected-ChStream)
- `PcieMemoryDevice::memory_backing_` → 删除; `backing_view_` (injected-ChStream)
- `GmmuTLM::backing_` → `mem_view_` (injected-functional)

**验证结果**: ctest 76/76 PASS, 0 regression; grep audit 旧名 0 匹配 (除 deprecation wrapper)

### 2027-02-09 — Phase 3 实施完成 (driver-visible-minimal-soc v1.8)

**实施 change**: `openspec/changes/cpptlm-driver-visible-minimal-soc` (Phase 3: T3-FIX + T4)

**§3 不变性验证 (Phase 3 增量)**:

| Invariant | 状态 | 验证 |
|-----------|------|------|
| N12 条件注入: soc 有 pcie_memory → 注入 PcieMemoryDevice; 否则 legacy MemoryTLM | ✅ | `bind_memory_backings()` pcie_memory 分支 + legacy fallback 保留 |
| EP `set_memory_device` 注入 (BAR2 fast-path 依赖 has_memory_device) | ✅ | `pcie_ep()->set_memory_device(pcie_mem)` 在 pcie_memory 分支内 |
| B20: `set_translate_cb` + `set_sdma_engine` 无条件注入 (不在 pcie_memory 分支内) | ✅ | 主函数顶部统一注入 |
| B19: SDMA `vram_size_bytes` 由 board 注入 (JSON 删除 sdma.params.vram_size_bytes) | ✅ | `configs/dgpu_soc_minimal_v1.json` 无该字段 |
| BAR2 fast-path (mmio_read/write(2,...)) → `ep->memory_device().memory_read/write` | ✅ | `memory_routing_enabled_` 门控 + `has_memory_device()` 检查 |

**验证结果**: `[driver_visible]` 2 cases / 29 assertions + `[pcie-memory]` 24 cases + `[minimal_dgpu_soc]` 12 cases + ctest 76/76 PASS, 0 regression。

