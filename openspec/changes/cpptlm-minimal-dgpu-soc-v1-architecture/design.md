# cpptlm-minimal-dgpu-soc-v1-architecture: Design

> **来源**: docs/designs/dgpu-soc/architecture.md (Oracle 5 步解锁链修订后) + ADR-DGPU-10

## 1. 实施步骤

### 1.1 DGpuBoard 字段 rename (`framebuffer_storage_` → `vram_storage_`)

**Where**: `include/tlm/gpu/dgpu_board_shell.{hh,cc}`

```cpp
// include/tlm/gpu/dgpu_board_shell.hh (DGpuBoard 类)
private:
    // v1.0 view 阶段: 字段名 framebuffer_storage_/framebuffer_ptr_ 暂时保留 (per ADR-DGPU-10 §4 Migration Step 1 待实施)
    // 当前: 保持向后兼容;后续 OpenSpec change 实施 rename

    // ADR-DGPU-10 决策命名:
    std::unique_ptr<uint8_t[]> vram_storage_;   // 8GB default-init (D-AXI v1.4 B7)
    uint64_t                    vram_size_      = 0;  // framebuffer 总大小 (顶层 framebuffer_size_bytes 派生)
    // 历史别名 (deprecation):
    [[deprecated("use vram_storage_")]] std::vector<uint8_t> framebuffer_storage_;
```

**调用点修改** (`dgpu_board_shell.cc`):
- `framebuffer_storage_.resize(...)` → `vram_storage_.resize(...)`
- `framebuffer_storage_.data()` → `vram_storage_.get()`
- `framebuffer_ptr_` 字段 → `vram_ptr_`(raw pointer 提取方法)
- 全部 23+ 处 grep audit + replace

**风险**: 字段 rename 是 breaking change,所有调用方需同步。考虑**双轨方案**:v1.0.1 添加 `vram_storage_` 作为新字段,旧 `framebuffer_storage_` 标 `[[deprecated]]`,v2.0 删除旧字段。

### 1.2 MemoryTLM setter rename (`set_backing_store` → `set_backing_view`)

**Where**: `include/tlm/memory_tlm.hh`

```cpp
class MemoryTLM : public ChStreamModuleBase {
    uint8_t*  backing_view_      = nullptr;  // injected-ChStream (per ADR-DGPU-10)
    uint64_t  backing_view_size_ = 0;

public:
    void set_backing_view(uint8_t* ptr, uint64_t size_bytes) noexcept {
        backing_view_ = ptr;
        backing_view_size_ = size;
    }
    // 历史别名 (deprecation):
    [[deprecated("use set_backing_view / backing_view_")]]
    void set_backing_store(uint8_t* ptr, uint64_t size_bytes) noexcept {
        set_backing_view(ptr, size_bytes);
    }
};
```

**调用点**: `src/tlm/gpu/dgpu_board_shell.cc:616` `mem->set_backing_store(...)` → `mem->set_backing_view(...)`

### 1.3 GmmuTLM 字段+setter rename (`backing_` → `mem_view_`, `set_backing` → `set_mem_view`)

**Where**: `include/tlm/gpu/gmmu_tlm.{hh,cc}`

```cpp
namespace tlm::gpu {
class GmmuTLM : public ::SimModule {
    uint8_t*  mem_view_      = nullptr;  // functional-mode injected (per ADR-DGPU-10)
    uint64_t  mem_view_size_ = 0;

public:
    void set_mem_view(uint8_t* ptr, uint64_t sz) noexcept {
        mem_view_ = ptr;
        mem_view_size_ = sz;
    }
    // 历史别名:
    [[deprecated("use set_mem_view / mem_view_")]]
    void set_backing(uint8_t* ptr, uint64_t sz) noexcept {
        set_mem_view(ptr, sz);
    }

    int translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        // ...
        if (!enabled_ || !mem_view_ || pt_base() == 0) return -EIO;
        // ...
        std::memcpy(&pte, mem_view_ + pte_addr, sizeof(pte));
        // ...
    }
};
}
```

**调用点**: `src/tlm/gpu/dgpu_board_shell.cc:635` `gmmu_->set_backing(...)` → `gmmu_->set_mem_view(...)`

### 1.4 PcieMemoryDevice 删除 `memory_backing_` (per ADR-DGPU-05 v1.4 B7)

**Where**: `include/tlm/gpu/pcie_memory_device.{hh,cc}`

```cpp
class PcieMemoryDevice {
    // 删除: std::vector<uint8_t> memory_backing_;   // v1.4 B7
    uint8_t* backing_view_      = nullptr;  // injected by DGpuBoard::bind_memory_backings
    uint64_t backing_view_size_ = 0;

public:
    void set_backing_view(uint8_t* ptr, uint64_t size) noexcept {
        backing_view_ = ptr;
        backing_view_size_ = size;
    }
    // memory_read/write 返回 -ENODEV 若 backing_view_ == nullptr
};
```

**修改点**:
- `pcie_memory_device.cc:66-83`:`memory_backing_.resize` / `memory_backing_[offset]` 改为 `backing_view_[offset]`
- `pcie_memory_device.hh:72`:`has_memory_backing()` → `has_backing_view()`

### 1.5 测试套件迁移

**Where**: `test/pcie/*.cc` (24 cases) + `test/test_minimal_dgpu_soc*.cc`

**改动**:
- `pcie_memory` 测试套件: `memory_backing_` 引用 → `backing_view_`
- `set_backing_store` 调用 → `set_backing_view`
- `framebuffer_storage_` 引用 → `vram_storage_`

**测试通过标准**:
- D-AXI `[minimal_dgpu_soc][driver_visible]` 41 assertions 全绿
- `[pcie-memory]` 24 cases 全绿(机械迁移)
- `[abi][minimal_dgpu_soc]` 28 assertions 全绿
- Phase 8 既有 66951 assertions 0 regression

### 1.6 文档同步

**Where**: `docs/designs/dgpu-soc/architecture.md`

- §3.2 映射表:已对齐 (Oracle 5 步解锁链 Step 1-4)
- §3.4 DGpuBoard 新增代码示例:`framebuffer_storage_` → `vram_storage_`
- §3.6 Coherence 保证:同步字段名
- §6.1 数据流:`vram_backdoor_` 引用 OK (符合 ADR-DGPU-10 §3 Inv-3)

## 2. 实施顺序(避免 build break)

```
T0  (1h)  全量 build baseline + 记录 0 regressions
T1  (0.5d)  MemoryTLM: 新增 set_backing_view + deprecate 旧 setter,build 测试
T2  (0.5d)  GmmuTLM: 新增 set_mem_view + deprecate 旧 setter,build 测试
T3  (0.5d)  PcieMemoryDevice: 新增 backing_view_ + deprecate memory_backing_,build 测试
T4  (1d)    DGpuBoard: 新增 vram_storage_ + deprecate framebuffer_storage_,build 测试
T5  (1d)    所有调用方 + 24 case 测试迁移 + grep audit
T6  (0.5d)  Oracle 评审 + Status Update 追加
```

## 3. 兼容性

- **签名级 ABI**: 0 diff (新增 setter 是 overload-safe,旧 setter 标 `[[deprecated]]`)
- **二进制级 ABI**: 0 diff (旧 setter 仍可调用)
- **JSON schema**: 0 diff (字段 rename 是内部细节,不暴露 JSON)
- **测试兼容**: `[pcie-memory]` 24 case 机械迁移,语义不变

## 4. 风险

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| 旧 setter `[[deprecated]]` 警告刷屏 | 🟡 M | 编译期 `-Wno-deprecated-declarations` 临时压制 |
| PcieMemoryDevice v1.4 B7 删除 memory_backing_ 破坏现有测试 | 🔴 H | 24 case 同步迁移;测试在 deprecation 阶段不删,只迁移 |
| DGpuBoard 字段 rename 漏改一处 | 🟡 M | grep audit `framebuffer_storage_` 0 匹配 (除兼容层) |
| Oracle 二轮评审发现新 must-fix | 🟢 L | 标准 OpenSpec 流程,任务可分批 archive |

## 5. 替代方案

| 方案 | 描述 | 决定 |
|------|------|------|
| **A. 双轨 (deprecate)** | 保留旧 setter + 新 setter 并存,渐进迁移 | ✅ 推荐 |
| B. 一步到位 (无 deprecation) | 直接 rename,所有调用方同步 | ❌ 破坏现有测试 + 8+ 处代码改动风险 |
| C. 仅文档对齐 | 代码不动,ADR §4 Migration 标 deferred | ❌ ADR 与代码长期不一致 |

## 6. Status

_(本节将在 change 实施 + Oracle 评审后追加)_
