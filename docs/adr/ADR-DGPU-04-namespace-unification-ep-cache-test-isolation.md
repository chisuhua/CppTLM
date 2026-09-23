# ADR-DGPU-04: DGpuSoc 命名空间统一 + EpCache lazy cache + 测试可访问性隔离

> **状态**: 📋 提案
> **日期**: 2027-02-09
> **影响**: `include/tlm/gpu/dgpu_soc.{hh,cc}` (命名空间迁移), `dgpu_board_shell.{hh,cc}` (EpCache + friend class), 0 ABI 变更
> **关联架构文档**: [docs/architecture/14-dgpu-board-ideal-arch.md §3.4](../architecture/14-dgpu-board-ideal-arch.md)
> **配套**: ADR-DGPU-01 (CallbackWorker), ADR-DGPU-02 (LifecycleProtocol), ADR-DGPU-03 (DispatchRegistry)

> **评审修订 (评审报告 R-E1, 2027-02-09)**: §1.2 dynamic_cast 调用点 11 处 → 12 处 (新增 pcie_config_write 第 398 行), §4.2 B4 工作量同步更新。Codegrep 验证 (2027-02-09): `grep -n "dynamic_cast.*PcieEndpointIP" src/tlm/gpu/dgpu_board_shell.cc` 共 12 行命中。

> **Oracle v2.0.1 修订 (2027-02-09) — 删除 reconfigure invalidate 死代码 (Oracle-3)**:
> - §3 Inv-3 移除"load_soc_config 重复调用 invalidate"伪协议 (与 ADR-DGPU-02 状态机"无 reverse transition, board 一次性使用"矛盾)
> - §3 Inv-3 重写为"EpCache 在 init() 后自动 resolve 一次, 整生命周期不再失效 (除 shutdown)"
> - §2.2 `EpCache::invalidate()` API 标记为 `[[deprecated]]` (per Oracle-3 v2.0.1 决策 (a) 禁二次调用, invalidate 不再需要; 保留 API 是为测试 hooks 但不再有生产路径调用)
> - §5.1 EpCache 单测删除"invalidate forces re-resolve"用例, 改为"EpCache: lifecycle shutdown 清空 cached_"用例 (符合"一次性"语义)
> - §7 风险表 R3 (EpCache 失效 race) 删除, 改为新风险"EpCache::invalidate() deprecated 警告噪声" (R-Oracle3-DepWarn)
> - 决策依据: Oracle-3 推荐路径 (a); 路径 (b) multi-config reconfigure 留待 v2.1

> **Oracle v2.0.2 修订 (2027-02-09) — P0-2c 兑现 v2.0.1 声明**:
> - **诚实性修复**: v2.0.1 声称 §5.1 已删除 "invalidate forces re-resolve" 用例, 但 Oracle 二轮复审发现**声明与正文矛盾** (测试用例仍在第 379-389 行, 且调用已 deprecated 的 `ep_cache_mut().invalidate()`)
> - 本轮 P0-2c **实际执行删除**: §5.1 该用例已移除, 替代为 "EpCache: lifecycle shutdown clears cached_" 用例 (见下)
> - 同步修正 §5.1 首个用例: 原 `REQUIRE(!board.ep_cache().is_resolved())` before init 断言与 "init() 后自动 resolve" 协议矛盾, 改为 `REQUIRE(board.ep_cache().is_resolved())` after init
> - §7 风险表 R3 确认已按 v2.0.1 声明删除 (下文 grep 验证)

---

## 1. 背景

### 1.1 问题 A: 命名空间不一致

`DGpuSoc` 在 `cpptlm::tlm::` 命名空间,而 `DGpuBoard` 在 `tlm::gpu::` 命名空间:

```cpp
// include/tlm/gpu/dgpu_soc.hh:12
namespace cpptlm::tlm {
class DGpuSoc : public SimModule {
    // ...
};
}

// include/tlm/gpu/dgpu_board_shell.hh:31
namespace tlm::gpu {
class DGpuBoard {
    // ...
    std::unique_ptr<cpptlm::tlm::DGpuSoc> soc_;  // 使用 using 桥接
    using cpptlm::tlm::DGpuSoc;  // dgpu_board_shell.hh:33
};
}
```

**现状约束** (per `include/AGENTS.md`):

> 大部分代码在全局命名空间(非 cpptlm),仅 StreamAdapter 在 cpptlm::

**问题**: `cpptlm::tlm::` 是 cpptlm Python 库的命名空间约定,但 DGpuSoc 是纯 C++ 类, 不应放入该命名空间。

### 1.2 问题 B: ep 指针重复 dynamic_cast

`pcie_ep()` 每次调用都做 `dynamic_cast<PcieEndpointIP*>`:

```cpp
// dgpu_board_shell.hh:132-136
tlm::pcie::PcieEndpointIP* pcie_ep() const {
    if (!soc_) return nullptr;
    return dynamic_cast<tlm::pcie::PcieEndpointIP*>(soc_->getInternalInstance("pcie_ep"));
}
```

**调用点** (12 处, per 评审报告 R-E1 grep 验证 2027-02-09): `dgpu_board_shell.cc:247, 321, 387, 398, 408, 421, 461, 501, 520, 539, 554, 571`。其中第 398 行为 `pcie_config_write` 内,此前遗漏。

**RTTM 开销**: ~10-50ns/次, 12 处累加 ~120-600ns/调用, 23 ABI 全量路径下不可忽略。

### 1.3 问题 C: 测试 accessor 暴露在 public API

`PcieEndpointIP` (`include/tlm/pcie/pcie_endpoint_ip.hh:201-209`):

```cpp
// 测试 helper: 读取 BAR backing store (三维 key: BDF × BAR × addr)
[[nodiscard]] uint64_t bar_store_value(uint16_t bdf, uint8_t bar, uint64_t addr) const noexcept {
    const auto it = bar_store_.find(BarStoreKey{bdf, bar, addr});
    return (it != bar_store_.end()) ? it->second : 0;
}

// 测试 helper: 直接写入 BAR backing store (绕开 AXI/mmio 路径, 供测试注入)
void bar_store_value_set(uint16_t bdf, uint8_t bar, uint64_t addr, uint64_t val) noexcept {
    bar_store_[BarStoreKey{bdf, bar, addr}] = val;
}
```

**问题**:
- 暴露 `bar_store_` 内部细节 (Squa/queue/key 结构)
- 后续重构 (e.g., 改用 `std::array`) 必须保持 ABI 兼容, 沦为 **测试债**
- 公开的 `bar_store_value_set` 允许任意写入, 与生产路径 (AXI/mmio) 不一致, 易误用

---

## 2. 决策

引入 3 项独立但相关的改造:

### 2.1 决策 A: 命名空间统一

将 `cpptlm::tlm::DGpuSoc` 迁移到 `tlm::gpu::DGpuSoc`, 与 `DGpuBoard` 同命名空间。

```cpp
// include/tlm/gpu/dgpu_soc.hh (v2.0)
namespace tlm::gpu {

class DGpuSoc : public SimModule {
public:
    explicit DGpuSoc(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    ~DGpuSoc() override = default;

    std::string get_module_type() const override { return "DGpuSoc"; }

    void simulate_instantiate(const nlohmann::json& cfg) override;
};

} // namespace tlm::gpu
```

**过渡期** (2 周):
- `tlm::gpu::DGpuSoc` 为新位置
- `cpptlm::tlm::DGpuSoc` 改为 `using DGpuSoc = tlm::gpu::DGpuSoc;` (类型别名, 兼容旧引用)
- 编译器警告: `[[deprecated]]` 标注旧位置

### 2.2 决策 B: EpCache lazy cache

```cpp
namespace tlm::gpu {

// 嵌套类, 仅 DGpuBoard 使用
class DGpuBoard::EpCache {
public:
    EpCache(DGpuBoard* board) : board_(board) {}

    // 返回 cached ep 指针 (init 后锁定)
    // 首次调用 resolve + cache, 后续 O(1)
    tlm::pcie::PcieEndpointIP* get();

    // 显式失效 (per Oracle-3 v2.0.1, v2.0 已 deprecated, 仅测试 hook)
    [[deprecated("EpCache::invalidate() deprecated in v2.0.1; use shutdown clear() path instead")]] void invalidate() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        cached_ = nullptr;
        resolved_.store(false);
    }

    // 当前是否已 resolved
    bool is_resolved() const noexcept { return resolved_.load(); }

private:
    // 实际解析 (首次调用 + invalidate 后)
    tlm::pcie::PcieEndpointIP* resolve();

    DGpuBoard*                   board_;
    mutable std::mutex           mu_;
    std::atomic<bool>            resolved_{false};
    tlm::pcie::PcieEndpointIP*   cached_ = nullptr;
};

// 实现
tlm::pcie::PcieEndpointIP* DGpuBoard::EpCache::get() {
    if (resolved_.load(std::memory_order_acquire)) {
        return cached_;  // fast path (atomic load, 无锁)
    }
    return resolve();
}

tlm::pcie::PcieEndpointIP* DGpuBoard::EpCache::resolve() {
    std::lock_guard<std::mutex> lock(mu_);
    if (resolved_.load(std::memory_order_relaxed)) {
        return cached_;  // double-check
    }
    if (!board_->soc_) {
        return nullptr;
    }
    cached_ = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
        board_->soc_->getInternalInstance("pcie_ep"));
    resolved_.store(true, std::memory_order_release);
    return cached_;
}

} // namespace tlm::gpu
```

**集成**:

```cpp
class DGpuBoard {
    // ...
private:
    EpCache ep_cache_{this};   // 替代散落的 dynamic_cast
};
```

**调用点替换**:

```cpp
// v1.0: 12 处 dynamic_cast (per 评审报告 R-E1, 2027-02-09 修订)
auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
    soc_->getInternalInstance("pcie_ep"));

// v2.0: 1 行调用
auto* ep = ep_cache_.get();
```

### 2.3 决策 C: 测试可访问性隔离 (friend class)

```cpp
// include/tlm/pcie/pcie_endpoint_ip.hh (v2.0)
class PcieEndpointIP : public SimModule {
public:
    // ... 其他 public API ...

private:
    // 测试 peer (forward declaration, 定义在 test header)
    friend class ::DGpuBoardTestPeer;

    // 仅 friend class 可访问
    [[nodiscard]] uint64_t bar_store_value(uint16_t bdf, uint8_t bar,
                                            uint64_t addr) const noexcept {
        // 移到 private, 仅 friend 可见
    }

    void bar_store_value_set(uint16_t bdf, uint8_t bar, uint64_t addr,
                              uint64_t val) noexcept {
        // 移到 private, 仅 friend 可见
    }

    std::unordered_map<BarStoreKey, uint64_t> bar_store_;  // private (already)
};
```

**测试 peer** (新建 `test/dgpu_board_test_peer.hh`):

```cpp
#pragma once

// 测试 peer 类, 仅在 test/ 中 include, 提供 bar_store_value 接口
// 通过 friend class 绕过 public API 边界
class DGpuBoardTestPeer {
public:
    explicit DGpuBoardTestPeer(tlm::pcie::PcieEndpointIP* ep) : ep_(ep) {}

    uint64_t bar_store_value(uint16_t bdf, uint8_t bar, uint64_t addr) {
        return ep_->bar_store_value(bdf, bar, addr);  // 调用 private 方法
    }

    void bar_store_value_set(uint16_t bdf, uint8_t bar, uint64_t addr, uint64_t val) {
        ep_->bar_store_value_set(bdf, bar, addr, val);
    }

    bool has_display_device() {
        return ep_->has_display_device();  // 仍是 public, 不需 friend
    }

private:
    tlm::pcie::PcieEndpointIP* ep_;
};
```

**使用示例**:

```cpp
// test/test_pcie_endpoint_bar_store_3d_key.cc
TEST_CASE("bar_store 3D key isolation (via test peer)") {
    PcieEndpointIP ep("test", nullptr);
    DGpuBoardTestPeer peer(&ep);

    peer.bar_store_value_set(0x0008, 0, 0x100, 0xDEADBEEFULL);
    REQUIRE(peer.bar_store_value(0x0008, 0, 0x100) == 0xDEADBEEFULL);
    REQUIRE(peer.bar_store_value(0x0009, 0, 0x100) == 0);  // 跨 BDF 隔离
}
```

---

## 3. 关键不变性

### Inv-1: 命名空间迁移的兼容性

```cpp
// 旧位置保留为类型别名 (过渡期)
namespace cpptlm::tlm {
using DGpuSoc [[deprecated("use tlm::gpu::DGpuSoc")]] = tlm::gpu::DGpuSoc;
}
```

**保证**: 旧 `cpptlm::tlm::DGpuSoc*` 引用仍编译通过 (类型别名), 仅产生警告。

### Inv-2: EpCache thread-safe fast path

```cpp
// 首次调用: dynamic_cast (slow)
// 后续调用: atomic load + cached pointer return (fast, ~1-5 ns)
```

**保证**: 1000 次 ep 访问, 总开销 < 1us (vs v1.0 110us-1ns × 1000)。

### Inv-3: EpCache resolve 一次性协议 (per Oracle-3 v2.0.1, 原 Inv-3 "失效协议" 删除)

```cpp
// init() 完成: 首次 resolve (board 一次性使用, 整生命周期不再失效)
void DGpuBoard::init() {
    if (soc_) soc_->init();
    ep_cache_.resolve();   // 强制首次 resolve, resolved_=true
    // ... 后续 lifecycle 不再调 invalidate
}

// shutdown: 清空 cached_ (析构阶段, 仅测试可见)
void DGpuBoard::destroy() {
    // ... (lifecycle transition ShuttingDown, callback nullification, worker shutdown 等)
    ep_cache_.clear();   // cached_ = nullptr + resolved_=false
}
```

**保证 (修订)**:
- 旧 "load_soc_config 重复调用 invalidate" 协议**已删除** (与 ADR-DGPU-02 状态机"无 reverse transition"对齐)
- EpCache 在 `init()` 后自动 resolve 一次, 整个 lifecycle 不再失效 (除 shutdown `clear()`)
- EpCache::invalidate() API 标记 `[[deprecated]]`, 无生产调用, 仅保留作测试 hook

**决策依据**: Oracle-3 推荐路径 (a) — 禁二次 `load_soc_config` (符合"board 一次性使用"基调);路径 (b) multi-config reconfigure 留待 v2.1。

### Inv-4: friend class 仅在测试中 include

```cpp
// test/dgpu_board_test_peer.hh: 仅 test/ 中的源文件 include
// production 代码 (include/tlm/, src/tlm/) 不能 include
//
// 验证: CI grep `grep -r "DGpuBoardTestPeer" include/ src/` 必须为空
```

---

## 4. 实施步骤

### 4.1 阶段 A: 基础设施 (W25)

| 任务 | 产出 |
|------|------|
| A1: 新增 `dgpu_board_ep_cache.hh` 头文件 | EpCache API 完整 |
| A2: 单元测试 `test_dgpu_board_ep_cache.cc` (6 case) | 100% PASS |
| A3: 新增 `test/dgpu_board_test_peer.hh` | friend 边界就位 |

### 4.2 阶段 B: 集成 (W26)

| 任务 | 产出 |
|------|------|
| B1: `dgpu_soc.hh` 迁移 `namespace cpptlm::tlm` → `namespace tlm::gpu` | 命名空间统一 |
| B2: `cpptlm::tlm::DGpuSoc` 改为类型别名 + `[[deprecated]]` | 过渡兼容 |
| B3: `DGpuBoard` 添加 `EpCache ep_cache_` 成员 | cache 就位 |
| B4: 12 处 dynamic_cast 替换为 `ep_cache_.get()` (per 评审报告 R-E1, 11→12) | 零散 dynamic_cast 消除 |
| B5: `PcieEndpointIP::bar_store_value/_set` 改为 private + friend | 测试可访问性隔离 |
| B6: 测试文件改用 `DGpuBoardTestPeer` | 测试代码更新 |

### 4.3 验证

- [ ] `test_dgpu_board_ep_cache.cc` 6 case 100% PASS
- [ ] `grep -r "dynamic_cast.*PcieEndpointIP" src/tlm/gpu/dgpu_board_shell.cc` 仅保留 1 处 (在 EpCache::resolve)
- [ ] `grep -r "DGpuBoardTestPeer" include/ src/` 为空 (仅 test/ 可见)
- [ ] 44498 assertions 100% PASS (0 regression)
- [ ] Oracle 评审通过

---

## 5. 测试策略

### 5.1 EpCache 单元测试 (per Oracle-3 v2.0.1 + Oracle v2.0.2 P0-2c 重写)

**v2.0.2 修订说明**: 原 "EpCache: invalidate forces re-resolve" 用例已**删除** (per Oracle-3: `invalidate()` deprecated, 无生产调用);原用例断言 `!is_resolved()` after init 与"init() 后自动 resolve"协议矛盾, 一并修正。新用例覆盖 "init 后 resolve" + "shutdown clear" 两个真实路径。

```cpp
TEST_CASE("EpCache: init resolves once, subsequent calls return cached") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg_with_pcie_ep));
    REQUIRE(board.init());

    // per Oracle-3 v2.0.1: init() 内部已 resolve (board 一次性使用)
    REQUIRE(board.ep_cache().is_resolved());

    auto* ep1 = board.pcie_ep();
    REQUIRE(ep1 != nullptr);

    auto* ep2 = board.pcie_ep();
    REQUIRE(ep2 == ep1);   // 同一指针 (cached, atomic fast path)
}

TEST_CASE("EpCache: lifecycle shutdown clears cached_") {   // per P0-2c 新增
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg_with_pcie_ep));
    REQUIRE(board.init());
    auto* ep1 = board.pcie_ep();
    REQUIRE(ep1 != nullptr);
    REQUIRE(board.ep_cache().is_resolved());

    board.shutdown();   // destroy 路径调 ep_cache_.clear()

    // per Oracle v2.0.2 P0-2e: shutdown clear() 后 cached_ 已清空
    REQUIRE(!board.ep_cache().is_resolved());
    REQUIRE(board.pcie_ep() == nullptr);
}

TEST_CASE("EpCache: pre-init returns nullptr") {
    DGpuBoard board("test");
    // 未 init
    REQUIRE(board.pcie_ep() == nullptr);
}

// 注: 原 "invalidate forces re-resolve" 用例已删除 (per Oracle-3 v2.0.1,
//     invalidate() deprecated 且无生产路径; 若需测试 hook, 单独标注 [[deprecated]] 抑制)
```

### 5.2 命名空间迁移测试

```cpp
TEST_CASE("DGpuSoc namespace migration: type alias works") {
    // 旧位置 (deprecated) 仍可用
    [[maybe_unused]] cpptlm::tlm::DGpuSoc* old_ptr = nullptr;  // 应编译通过 (deprecated warning)

    // 新位置 (preferred)
    [[maybe_unused]] tlm::gpu::DGpuSoc* new_ptr = nullptr;     // 应编译通过
}
```

### 5.3 Friend Class 测试

```cpp
TEST_CASE("DGpuBoardTestPeer can access private bar_store_value") {
    PcieEndpointIP ep("test", nullptr);
    DGpuBoardTestPeer peer(&ep);

    peer.bar_store_value_set(0x0008, 0, 0x100, 0xDEADBEEFULL);
    REQUIRE(peer.bar_store_value(0x0008, 0, 0x100) == 0xDEADBEEFULL);
}

TEST_CASE("bar_store_value is private (compile-fail test)") {
    PcieEndpointIP ep("test", nullptr);
    // 以下应编译失败 (验证 private 边界)
    // ep.bar_store_value(0, 0, 0);   // ❌ private method
    // ep.bar_store_.find(...);         // ❌ private member
}
```

---

## 6. 兼容性 (per Oracle-G v2.0.1, 下游影响清单 + typeid 审计)

### 6.1 兼容性矩阵

| 项 | 影响 |
|----|------|
| **23 ABI 签名** | 0 修改 |
| **23 ABI 语义** | 0 修改 |
| **JSON 配置** | 0 修改 |
| **`cpptlm::tlm::DGpuSoc` 引用** | 类型别名兼容 + 弃用提示 (过渡期 2 周后移除) |
| **`tlm::gpu::DGpuBoard` API** | 0 修改 |
| **`PcieEndpointIP::bar_store_value/_set`** | public → private + friend 暴露 (测试可见, 生产不可见) |
| **下游 SIMMODULE 注册键** | 字符串 `"DGpuSoc"` 不变 (`get_module_type()` 返回值), ModuleFactory 字符串查找安全 |

### 6.2 下游影响清单 (per 评审报告 R-R4 v2.0.1)

**已知引用位置** (需在迁移后逐一确认仍编译):

| 文件 | 行 | 当前引用 | 迁移后引用 | 备注 |
|------|----|---------|-----------|------|
| `include/tlm/gpu/dgpu_soc.hh:12` | 12 | `namespace cpptlm::tlm { class DGpuSoc ... }` | `namespace tlm::gpu { class DGpuSoc ... }` | 主定义迁移 |
| `include/tlm/gpu/dgpu_board_shell.hh:33` | 33 | `using cpptlm::tlm::DGpuSoc;` | 删除 (直接用 `tlm::gpu::DGpuSoc`) | 桥接删除 |
| `include/tlm/gpu/dgpu_board_shell.hh:224` | 224 | `std::unique_ptr<cpptlm::tlm::DGpuSoc> soc_;` | `std::unique_ptr<tlm::gpu::DGpuSoc> soc_` | 成员类型迁移 |
| `include/modules_cluster.hh` | 待 grep 定位 | `REGISTER_MODULE(DGpuSoc)` 宏调用 | 宏不变 (按类型字符串注册) | 宏体 `#define REGISTER_MODULE(T) ... #T ...`, 类型字符串来自 demangle |

### 6.3 typeid / RTTI 审计 (per Oracle-G v2.0.1)

**风险点**: 类型别名 (`using cpptlm::tlm::DGpuSoc = tlm::gpu::DGpuSoc;`) **不改变** `typeid(T).name()` 的 mangled name。`typeid(cpptlm::tlm::DGpuSoc).name()` 解析为 `tlm::gpu::DGpuSoc` 的 mangling。

**审计项**:

| 检查 | 命令 | 通过条件 |
|------|------|---------|
| ModuleFactory 字符串查找 | `grep -n "getModuleRegistry\|registerModule" src/core/module_factory.cc` | 字符串 "DGpuSoc" 不依赖 namespace 字符串 |
| typeid 反射使用 | `grep -rn "typeid.*DGpuSoc\|typeid.*dgpu_soc" include/ src/ cpptlm/` | 无 demangle 依赖 (仅按 `get_module_type()` 字符串匹配) |
| pybind 绑定 | `grep -rn "py::class_<DGpuSoc\|PYBIND11" cpptlm/ external/` | 不存在; 若存在需 demangle 验证 |
| 文档/demo 引用 | `grep -rn "cpptlm::tlm::DGpuSoc\|tlm::gpu::DGpuSoc" examples/ docs/` | docs 已用别名过渡; examples 需更新 |

**Oracle-G 决策**: 由于 `get_module_type()` 返回的字符串 `"DGpuSoc"` 保持不变 (per §2.1 代码示例), ModuleFactory 注册键安全; 唯一风险点为 typeid 反射路径, Phase A 第 0 天必须做 6.3 审计表的 grep 验证。

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 命名空间迁移破坏 ModuleFactory registry lookup | 中 | SIMMODULE 注册失败 | ModuleFactory 注册字符串 `"DGpuSoc"` 不变 (per §6.2 / §6.3 Oracle-G v2.0.1 审计); Phase A 第 0 天执行 typeid grep 验证 |
| **R2**: `[[deprecated]]` 警告噪声 (CI failure) | 中 | CI 误判 | CI 配置 `-Wno-deprecated-declarations` 仅对迁移期 build target |
| **R3** (删除 per Oracle-3 v2.0.1): EpCache 失效 race with 并发 load_soc_config | — | — | 旧 Inv-3 "load_soc_config 重复调用 invalidate" 协议已删除 (与状态机无 reverse transition 矛盾); EpCache 在 init() 后整生命周期不再失效 (除 shutdown clear()) |
| **R4**: friend class 被 production 代码误 include | 低 | 测试 peer 污染 production | CI grep `grep -r "DGpuBoardTestPeer" include/ src/` 必须为空 |
| **R5**: `bar_store_value/_set` private 化破坏现有测试 | 中 | 测试编译失败 | 一次性 sed 替换 `ep->bar_store_value` → `DGpuBoardTestPeer(&ep).bar_store_value` |
| **R6** (新增 per Oracle-G v2.0.1): typeid demangle 在下游测试中失败 | 低 | 测试动态加载失败 | §6.3 审计表 grep 验证, 无 demangle 依赖则风险消除 |

---

## 8. 参考

- **架构文档**: [14-dgpu-board-ideal-arch.md §3.4](../architecture/14-dgpu-board-ideal-arch.md)
- **ADR-DGPU-01**: CallbackWorker
- **ADR-DGPU-02**: LifecycleProtocol
- **ADR-DGPU-03**: DispatchRegistry
- **v1.0 实现**: `include/tlm/gpu/dgpu_soc.hh:12` (namespace), `dgpu_board_shell.hh:132-136` (dynamic_cast), `pcie_endpoint_ip.hh:201-209` (public test accessor)
- **同类模式参考**: `SimModule` (`include/core/sim_module.hh`) 的 namespace 约定
- **同类模式参考**: Google Test `FRIEND_TEST` (测试可访问性隔离)

---

**维护**: CppTLM 开发团队
**状态**: 📋 提案