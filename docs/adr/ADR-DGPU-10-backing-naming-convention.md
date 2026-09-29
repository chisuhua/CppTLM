# ADR-DGPU-10: Backing Storage 命名约定 (owner / injected 两级)

> **状态**: 📋 提议 (待 v1.0 架构视图定稿后正式签发)
> **日期**: 2027-02-09 (基于 Oracle 5 步解锁链 Step 5 输出)
> **配套 ADR**: [ADR-DGPU-05](ADR-DGPU-05-vram-storage-ownership.md) (单一 VRAM 所有权,架构决策已完成) + [ADR-DGPU-07](ADR-DGPU-07-minimal-soc-evolution-seam.md) (D3 演进 seam)
> **关联架构文档**: [../designs/dgpu-soc/architecture.md §1.4](../designs/dgpu-soc/architecture.md) (仿真模式声明) + [../designs/dgpu-soc/architecture.md §3](../designs/dgpu-soc/architecture.md) (存储子系统)
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (D-AXI v1.4, B7 已确立单一所有权)
> **Owner**: CppTLM Team

---

## 1. 背景

### 1.1 问题

D-AXI v1.4 B7 已确立"单一 VRAM 真源"架构原则(`DGpuBoard` 拥有 backing,其他组件注入指针)。但**字段命名仍未统一**,5 个相关字段使用 4 种不同术语:

| 组件 | 当前字段名 | 类型 | 角色 |
|------|----------|------|------|
| `DGpuBoard` | `framebuffer_storage_` / `framebuffer_ptr_` | `std::vector<uint8_t>` + raw | 真源 (owner) |
| `MemoryTLM` | `backing_ptr_` / `backing_size_` | raw | ChStream 路径注入 |
| `PcieMemoryDevice` | (v1.4 B7 删除) `memory_backing_` → 改 `backing_ptr_` | raw | ChStream 路径注入 |
| `GmmuTLM` | `backing_` / `backing_size_` | raw | functional-mode 直读 |
| `SdmaEngineTLM` | `vram_backdoor_` / `host_backdoor_` | `void*` | functional-mode 直访 |

### 1.2 命名混乱的具体影响

1. **新模块作者不知该用哪个后缀**:`_storage_` / `_backing_` / `_backdoor_` / `_ptr_` 含义重叠
2. **代码搜索低效**:`grep backing` 命中 4 种字段,无法按角色过滤
3. **审查负担**:每次新 PR 都要决定命名,降低合并速度
4. **D-AXI 命名漂移**:v1.4 B7 把 `framebuffer_storage_` 改为 `vram_storage_`(per ADR-DGPU-05),但 §3.2 映射表和 v1.0 文档还残留旧名
5. **functional-mode 与 ChStream-mode 概念混淆**:`backing_` (GMMU functional 直读) vs `backing_ptr_` (MemoryTLM ChStream 注入) — 同名不同语义

### 1.3 根因

**没有显式的命名约定**。D-AXI v1.4 B7 解决了**架构层**(谁拥有),但**词汇表层**(字段叫什么)未规范化。新增模块时无章可循,自然产生同义词。

---

## 2. 决策

引入**两级命名约定**:`owner` vs `injected`,后者再分 `functional-mode` vs `ChStream-mode`:

### 2.1 owner 后缀:`*_storage_`

**语义**: 该字段**分配并拥有** backing 内存,组件负责生命周期管理。

| 规则 | 示例 |
|------|------|
| 类型必须是 RAII 容器 (`std::vector<uint8_t>` / `std::unique_ptr<uint8_t[]>`) | ✅ `std::vector<uint8_t> framebuffer_storage_;` |
| **绝不**用 raw pointer (`uint8_t*`) 当 owner | ❌ `uint8_t* framebuffer_;` |
| 唯一所有者: `DGpuBoard` (per ADR-DGPU-05) | 仅此一处 |

**当前实现**: `DGpuBoard::framebuffer_storage_` (`dgpu_board_shell.hh:294`) ✅
**D-AXI 命名升级**: `framebuffer_storage_` → `vram_storage_` (per ADR-DGPU-05, 8GB default-init unique_ptr)

### 2.2 injected-ChStream-mode 后缀:`*_backing_view_`

**语义**: 该字段是 raw pointer,**由 owner 注入**(通常在 `bind_memory_backings()`),用于 ChStream 数据路径读写 backing。**不分配、不释放**。

| 规则 | 示例 |
|------|------|
| 类型必须是 raw pointer (`uint8_t*`) | ✅ `uint8_t* backing_view_;` |
| **永不**配 `std::vector` (避免所有权混淆) | ❌ `std::vector<uint8_t> backing_;` |
| 配 size 字段:`*_view_size_` | ✅ `uint64_t backing_view_size_;` |
| 注入时机: sim_thread_ 启动前 (per ADR-DGPU-05 Inv-2) | |
| 消费者: MemoryTLM / PcieMemoryDevice | |

**当前实现**:
- `MemoryTLM::backing_ptr_` → `backing_view_` (`memory_tlm.hh:27`)
- `PcieMemoryDevice::backing_ptr_` → `backing_view_` (`pcie_memory_device.hh`)
- `GmmuTLM::backing_` → `backing_view_` (`gmmu_tlm.hh:83`)

> **命名冲突解决**: `GmmuTLM::backing_` 是 functional-mode 直读(见下),**不应**用 `_backing_view_` 后缀(那是 ChStream-mode 约定)。GmmuTLM 用 `_mem_view_` 后缀区分。

### 2.3 injected-functional-mode 后缀:`*_backdoor_` / `*_mem_view_`

**语义**: raw pointer,**由 owner 注入**用于 functional-mode 直访(零时延 memcpy,不经 ChStream)。

| 子类 | 适用 | 字段命名 | 原因 |
|------|------|----------|------|
| **fenced simulation access**(跨 host/sim 进程) | `SdmaEngineTLM::host_backdoor_` / `vram_backdoor_` | `*_backdoor_` | gem5 `MemBackdoor` 等价(per §1.4 functional-mode 声明) |
| **intra-simulation view**(同一进程内只读 view) | `GmmuTLM::backing_` (页表只读访问) | `*_mem_view_` | 避免与 ChStream-mode 混淆 |

| 规则 | 示例 |
|------|------|
| 类型必须是 raw pointer | ✅ `void* host_backdoor_;` / ✅ `uint8_t* mem_view_;` |
| 配 size 字段 | ✅ `uint64_t host_backdoor_size_;` / `uint64_t mem_view_size_;` |
| 注入时机: sim_thread_ 启动前 OR 测试 setup | |
| 消费者: SdmaEngineTLM / GmmuTLM | |

**当前实现**:
- `SdmaEngineTLM::host_backdoor_` / `vram_backdoor_` (`sdma_engine_tlm.hh:367-370`) ✅ 已合规
- `GmmuTLM::backing_` → `mem_view_` (需重命名,语义更准确)

### 2.4 命名对照表(完整)

| 当前字段 | 决策新名 | 角色 | 实施 |
|----------|----------|------|------|
| `DGpuBoard::framebuffer_storage_` | `DGpuBoard::vram_storage_` | owner | D-AXI v1.4 B7 |
| `DGpuBoard::framebuffer_ptr_` | `DGpuBoard::vram_storage_ptr()` (方法) | owner 访问 | 删除字段,改方法 |
| `MemoryTLM::backing_ptr_` | `MemoryTLM::backing_view_` | injected-ChStream | v1.1 rename |
| `MemoryTLM::backing_size_` | `MemoryTLM::backing_view_size_` | injected-ChStream | v1.1 rename |
| `MemoryTLM::size_cap_` | (保留,语义不同) | ChStream 容量上限 | 不变 |
| `PcieMemoryDevice::backing_ptr_` | `PcieMemoryDevice::backing_view_` | injected-ChStream | v1.4 B7 已部分改 |
| `PcieMemoryDevice::backing_size_` | `PcieMemoryDevice::backing_view_size_` | injected-ChStream | v1.4 B7 已部分改 |
| `GmmuTLM::backing_` | `GmmuTLM::mem_view_` | injected-functional | v1.1 rename |
| `GmmuTLM::backing_size_` | `GmmuTLM::mem_view_size_` | injected-functional | v1.1 rename |
| `SdmaEngineTLM::host_backdoor_` | `SdmaEngineTLM::host_backdoor_` (不变) | injected-functional | ✅ 已合规 |
| `SdmaEngineTLM::vram_backdoor_` | `SdmaEngineTLM::vram_backdoor_` (不变) | injected-functional | ✅ 已合规 |
| `SdmaEngineTLM::host_backdoor_size_` | `SdmaEngineTLM::host_backdoor_size_` (不变) | injected-functional | ✅ 已合规 |
| `SdmaEngineTLM::vram_backdoor_size_` | `SdmaEngineTLM::vram_backdoor_size_` (不变) | injected-functional | ✅ 已合规 |

---

## 3. 关键不变性 (Invariants)

### Inv-1: owner 后缀只能在唯一真源

**Where**: `DGpuBoard` (per ADR-DGPU-05)

```cpp
class DGpuBoard {
    // 唯一 *_storage_ owner
    std::unique_ptr<uint8_t[]> vram_storage_;  // 8GB default-init
    uint64_t vram_size_ = 0;

    // 提供方法访问 (不暴露 raw field)
    uint8_t* vram_storage_ptr() const noexcept { return vram_storage_.get(); }
};
```

**检查命令**: `grep -rn '_storage_' include/tlm src/tlm | grep -v 'vram_storage_\|framebuffer_storage_'` 必须 0 匹配(除 DGpuBoard 外)

### Inv-2: `_backing_view_` 是 ChStream-mode 注入约定

**Where**: `MemoryTLM` / `PcieMemoryDevice`

```cpp
class MemoryTLM : public ChStreamModuleBase {
    uint8_t*  backing_view_      = nullptr;  // injected, 不持有
    uint64_t  backing_view_size_ = 0;        // injected

public:
    void set_backing_view(uint8_t* ptr, uint64_t size) noexcept {
        backing_view_ = ptr;
        backing_view_size_ = size;
    }
};
```

**setter 命名**: `set_backing_store()` → `set_backing_view()` (语义更准确)

### Inv-3: `_backdoor_` 是 functional-mode 注入约定(只用于 SdmaEngineTLM)

**Where**: `SdmaEngineTLM`

```cpp
class SdmaEngineTLM : public ChStreamModuleBase {
    void*    host_backdoor_      = nullptr;  // functional 直访 host 内存
    uint64_t host_backdoor_size_ = 0;
    void*    vram_backdoor_      = nullptr;  // functional 直访 VRAM
    uint64_t vram_backdoor_size_ = 0;

public:
    void set_host_backdoor(void* ptr, uint64_t size) { /*...*/ }
    void set_vram_backdoor(void* ptr, uint64_t size) { /*...*/ }
};
```

**类型是 `void*`** 而非 `uint8_t*`:允许注入任意对齐 buffer (e.g. host mmap'd page-aligned region),保留原始对齐语义。

### Inv-4: `_mem_view_` 是 functional-mode 注入约定(只用于 GmmuTLM)

**Where**: `GmmuTLM`

```cpp
namespace tlm::gpu {
class GmmuTLM : public ::SimModule {
    uint8_t*  mem_view_      = nullptr;  // functional 直读 PTE
    uint64_t  mem_view_size_ = 0;

public:
    void set_mem_view(uint8_t* ptr, uint64_t sz) noexcept {
        mem_view_ = ptr;
        mem_view_size_ = sz;
    }
};
}
```

**与 `_backdoor_` 区别**: `_backdoor_` 是 host/sim 跨进程访问(gem5 `MemBackdoor`),`_mem_view_` 是 sim 内部只读 view(同一进程)。

---

## 4. 实施步骤

**实施渠道**: 本 ADR §4 8 步代码 rename 通过 OpenSpec change [**cpptlm-minimal-dgpu-soc-v1-architecture**](../../openspec/changes/cpptlm-minimal-dgpu-soc-v1-architecture/) 提案管理(2027-02-09, 1.5-2.5 工作日)。

| 步骤 | 任务 | 位置 | 影响 | OpenSpec 任务 |
|------|------|------|------|--------------|
| 1 | `DGpuBoard::framebuffer_storage_` → `vram_storage_` (rename + `[[deprecated]]` 兼容层) | `dgpu_board_shell.{hh,cc}` | DGpuBoard 内部 + 23+ 调用点 | T4 (1d) |
| 2 | `DGpuBoard::framebuffer_ptr_` 字段删除,改 `vram_storage_ptr()` 方法 | `dgpu_board_shell.{hh,cc}` | 字段访问点需更新 | T4 (1d) |
| 3 | `MemoryTLM::backing_ptr_` → `backing_view_` + `set_backing_store` → `set_backing_view` | `memory_tlm.{hh}` | D-AXI v1.4 已有 vram_size_ 区分,此处对齐命名 | T1 (0.5d) |
| 4 | `PcieMemoryDevice::backing_ptr_` → `backing_view_` + **删除** `memory_backing_` 字段 (D-AXI v1.4 B7 落地) | `pcie_memory_device.{hh,cc}` | 与 ADR-DGPU-05 Inv-3 同步 | T3 (0.5d) |
| 5 | `GmmuTLM::backing_` → `mem_view_` + setter 改 `set_mem_view` | `gmmu_tlm.{hh,cc}` | functional-mode 命名区分 | T2 (0.5d) |
| 6 | `DGpuBoard::bind_memory_backings()` 注入点调用方更新 | `dgpu_board_shell.cc:609` | 字段重命名后注入方法同步 |
| 7 | 所有调用方同步更新 | (grep audit) | `grep -rn 'backing_ptr_\|framebuffer_\|backing_store\b' include/ src/ test/` |
| 8 | ADR-DGPU-05 追加 Status Update | `ADR-DGPU-05-vram-storage-ownership.md` | 引用本 ADR |

**预计工时**: 0.5-1 工作日 (机械 rename,加单测 + grep audit)
**v1.0 实施时机**: 与 `docs/designs/dgpu-soc/architecture.md` v1.0 视图定稿同步
**v1.0 是否必须**: **否** — 本 ADR 是词汇表,代码可保留旧字段名,只是新代码必须遵守。**强烈建议** v1.0 实施。

---

## 5. 测试策略

### 5.1 命名合规测试

```cpp
TEST_CASE("backing-naming-convention: no rogue *_storage_ outside DGpuBoard") {
    // 编译期 + 运行期检查
    const auto storage_hits = grep_files(workspace, "*_storage_");
    for (const auto& hit : storage_hits) {
        REQUIRE(hit.file.find("dgpu_board_shell") != std::string::npos);
    }
}

TEST_CASE("backing-naming-convention: backing_view_ vs backdoor_ vs mem_view_ disjoint") {
    // 检查不存在既是 _backing_view_ 又是 _backdoor_ 的字段
    auto all_backing_fields = collect_fields(workspace, {"backing_view_", "backdoor_", "mem_view_"});
    for (const auto& [field, files] : all_backing_fields) {
        REQUIRE(files.size() == 1);  // 每个字段名只在 1 个类出现
    }
}
```

### 5.2 注入模式测试

```cpp
TEST_CASE("vram_storage_ single owner + 4 injected views") {
    DGpuBoard board;
    board.load_soc_config(minimal_cfg);
    board.init();

    // Owner: DGpuBoard::vram_storage_ 唯一
    REQUIRE(board.vram_storage_ptr() != nullptr);

    // 4 injected views (per ADR-DGPU-05 5 消费者):
    auto* soc = board.get_internal_instance("soc");
    REQUIRE(get_backing_view(soc, "memory") == board.vram_storage_ptr());
    REQUIRE(get_backing_view(soc, "pcie_memory") == board.vram_storage_ptr());
    REQUIRE(get_backdoor(soc, "sdma_vram") == board.vram_storage_ptr());
    REQUIRE(get_mem_view(soc, "gmmu") == board.vram_storage_ptr());

    // 全部指向同一地址 (single source of truth)
    REQUIRE(all_point_to_same_address(...));
}
```

### 5.3 回归测试

- D-AXI `[minimal_dgpu_soc][driver_visible]` 41 assertions 全绿
- D-AXI `[pcie-memory]` 24 cases 全绿
- `[abi][minimal_dgpu_soc]` 28 assertions 全绿
- Phase 8 既有 66951 assertions 0 regression

---

## 6. 兼容性

### 6.1 ABI 兼容性

- **签名级**: 0 diff (per ADR-088 §D5)
- **二进制级**: 0 diff (per ADR-DGPU-08)
- 字段名 rename **不**影响 ABI (ABI 是 C extern "C" 函数,非 C++ 字段)

### 6.2 JSON 配置兼容性

- 0 diff (`set_backing_view` 内部 setter 不暴露 JSON 字段)
- `dgpu_soc_minimal_v1.json` 不需修改

### 6.3 测试兼容性

- v1.0 测试若直接访问 `framebuffer_storage_` 等字段 → 需更新至 `vram_storage_` 命名
- 测试通过 public 方法 (`vram_storage_ptr()`) 访问 → 0 修改
- 推荐策略: 公共测试改用公共方法,避免绑死字段名

### 6.4 文档兼容性

- `docs/designs/dgpu-soc/architecture.md` §3.2 映射表需更新 (本 ADR 签发后)
- `docs/designs/dgpu-board/architecture.md` 同步
- `docs/pcie/driver-visible-minimal-soc.md` 同步

---

## 7. 风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| rename 漏改调用方导致编译错 | 🟡 M | grep audit + 全量构建 + ctest |
| 字段名与第三方库冲突 | 🟢 L | CppTLM 内部库,无外部依赖 |
| 测试绑死旧字段名 | 🟡 M | 测试改用公共方法 (vram_storage_ptr()) |
| ADR-DGPU-05 已部分实施 (`vram_storage_`) 与本 ADR 部分冲突 | 🟢 L | ADR-DGPU-05 v1.4 B7 已经把 `framebuffer_storage_` 改为 `vram_storage_`,本 ADR 是其延续; 同步对齐 |

### Oracle 评审重点

- **Q4 (Oracle 答复)**: "命名约定" + "owner / injected 两级" 与 Oracle 答复一致 (Risk if wrong: 每新模块再造同义词,代码搜索/审查成本持续上升)
- **§1.4 仿真模式声明**: backing_view_ (ChStream) vs backdoor_/mem_view_ (functional) 的命名划分直接对应 §1.4 的两条仿真路径

---

## 8. 参考

- ADR-DGPU-05: [VRAM backing 单一所有权归 DGpuBoard](ADR-DGPU-05-vram-storage-ownership.md)
- ADR-DGPU-07: [Minimal SoC → 完整 GPU 演进 Seam](ADR-DGPU-07-minimal-soc-evolution-seam.md)
- D-AXI OpenSpec: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.4)
- 实施笔记: [../../pcie/driver-visible-minimal-soc.md](../../pcie/driver-visible-minimal-soc.md)
- gem5 参考: `src/mem/abstract_mem.hh` (`backingStore`), `src/mem/physical.{hh,cc}` (`PhysicalMemory`), `src/mem/backdoor.hh` (`MemBackdoor`)
- 关联架构文档: [../designs/dgpu-soc/architecture.md §1.4 仿真模式声明](../designs/dgpu-soc/architecture.md)
- Oracle 评审: 5 步解锁链 Step 5 (2027-02-09)

---

## Status Update

_(本节将在 ADR 签发后追加,记录实施 commit hashes 与实际 rename 影响范围)_
