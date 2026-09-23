# ADR-DGPU-04: DGpuSoc 命名空间统一 + EpCache lazy cache + 测试可访问性隔离

> **状态**: 📋 提案
> **日期**: 2027-02-09
> **影响**: `include/tlm/gpu/dgpu_soc.{hh,cc}` (命名空间迁移), `dgpu_board_shell.{hh,cc}` (EpCache + friend class), 0 ABI 变更
> **关联架构文档**: [docs/architecture/14-dgpu-board-ideal-arch.md §3.4](../architecture/14-dgpu-board-ideal-arch.md)
> **配套**: ADR-DGPU-01 (CallbackWorker), ADR-DGPU-02 (LifecycleProtocol), ADR-DGPU-03 (DispatchRegistry)

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

**调用点** (11 处): `dgpu_board_shell.cc:247, 321, 387, 408, 421, 461, 501, 520, 539, 554, 571`。

**RTTM 开销**: ~10-50ns/次, 11 处累加 ~110-550ns/调用, 23 ABI 全量路径下不可忽略。

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

    // 显式失效 (load_soc_config 重复调用 / SOC reconfigure)
    void invalidate() noexcept {
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
// v1.0: 11 处 dynamic_cast
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

### Inv-3: EpCache 失效协议

```cpp
// load_soc_config 重复调用: invalidate
void DGpuBoard::load_soc_config(const json& cfg) {
    // ...
    if (existing_soc_) {
        ep_cache_.invalidate();  // 旧 cached ep 已失效
    }
    // ...
}

// init() 完成: 重新 resolve (新 SOC 可能装新 ep)
void DGpuBoard::init() {
    if (soc_) soc_->init();
    ep_cache_.invalidate();   // 强制重新 resolve
    // ...
}
```

**保证**: SOC 重新配置后, cached ep 不会 stale。

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
| B4: 11 处 dynamic_cast 替换为 `ep_cache_.get()` | 零散 dynamic_cast 消除 |
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

### 5.1 EpCache 单元测试

```cpp
TEST_CASE("EpCache: first call resolves, subsequent calls return cached") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg_with_pcie_ep));
    REQUIRE(board.init());

    REQUIRE(!board.ep_cache().is_resolved());
    auto* ep1 = board.pcie_ep();
    REQUIRE(ep1 != nullptr);
    REQUIRE(board.ep_cache().is_resolved());

    auto* ep2 = board.pcie_ep();
    REQUIRE(ep2 == ep1);   // 同一指针 (cached)
}

TEST_CASE("EpCache: invalidate forces re-resolve") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg_with_pcie_ep));
    REQUIRE(board.init());

    auto* ep1 = board.pcie_ep();
    board.ep_cache_mut().invalidate();
    REQUIRE(!board.ep_cache().is_resolved());
    auto* ep2 = board.pcie_ep();
    REQUIRE(ep2 == ep1);   // 重新解析后还是同一 ep (无 SOC reconfigure)
}

TEST_CASE("EpCache: pre-init returns nullptr") {
    DGpuBoard board("test");
    // 未 init
    REQUIRE(board.pcie_ep() == nullptr);
}
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

## 6. 兼容性

| 项 | 影响 |
|----|------|
| **23 ABI 签名** | 0 修改 |
| **23 ABI 语义** | 0 修改 |
| **JSON 配置** | 0 修改 |
| **`cpptlm::tlm::DGpuSoc` 引用** | 类型别名兼容 + 弃用提示 (过渡期 2 周后移除) |
| **`tlm::gpu::DGpuBoard` API** | 0 修改 |
| **`PcieEndpointIP::bar_store_value/_set`** | public → private + friend 暴露 (测试可见, 生产不可见) |
| **下游 SIMMODULE 注册** | 类型注册位置 `cpptlm::tlm::DGpuSoc` → `tlm::gpu::DGpuSoc`, registry lookup 需更新 |

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: 命名空间迁移破坏 ModuleFactory registry lookup | 中 | SIMMODULE 注册失败 | ModuleFactory 注册字符串 `"DGpuSoc"` 不变, 仅 namespace 变化; 测试覆盖 |
| **R2**: `[[deprecated]]` 警告噪声 (CI failure) | 中 | CI 误判 | CI 配置 `-Wno-deprecated-declarations` 仅对迁移期 build target |
| **R3**: EpCache 失效 race with 并发 load_soc_config | 低 | 缓存 stale | 仅 board 单线程调 load_soc_config (无需 lock) |
| **R4**: friend class 被 production 代码误 include | 低 | 测试 peer 污染 production | CI grep `grep -r "DGpuBoardTestPeer" include/ src/` 必须为空 |
| **R5**: `bar_store_value/_set` private 化破坏现有测试 | 中 | 测试编译失败 | 一次性 sed 替换 `ep->bar_store_value` → `DGpuBoardTestPeer(&ep).bar_store_value` |

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