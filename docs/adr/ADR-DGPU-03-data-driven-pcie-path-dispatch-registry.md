# ADR-DGPU-03: DispatchRegistry data-driven 替代 4 态 switch

> **状态**: 📋 提案
> **日期**: 2027-02-09
> **影响**: `include/tlm/gpu/dgpu_board_shell.{hh,cc}` (新增 `DispatchRegistry` 内部组件), 0 ABI 变更
> **关联架构文档**: [docs/architecture/14-dgpu-board-ideal-arch.md §3.3](../architecture/14-dgpu-board-ideal-arch.md)
> **配套**: ADR-DGPU-01 (CallbackWorker), ADR-DGPU-02 (LifecycleProtocol)

> **评审修订 (评审报告 R-R1 + R-R2 + R-C1-b, 2027-02-09)**:
> - §2.2 BAR1 doorbell handler: `parse_wptr` 改为 `detail::parse_wptr_from_host` (来自 v1.0 dgpu_board_shell.cc 内部命名空间, 沿用字节序约定)
> - §2.2 PciePath::Tlp handler: `*(uint64_t*)d` 强制转换改为 `len` 校验 + `std::memcpy` (避免 strict-aliasing UB / 越界读, 需要 `<cstring>` include)
> - §2.2 BAR1 doorbell handler: 增加 `len` 校验 (`len != 4 && len != 8` 返 -EINVAL)
> - §2.1 / §3 全文件 `struct Entry` → `struct DispatchEntry`, 与架构文档 §3.3 命名对齐 (13 处)

> **Oracle v2.0.1 修订 (2027-02-09) — mmio_regs_ 镜像不再掩盖 dispatch 失败 (Oracle-4)**:
> - §2.3 `mmio_write` 重构: 镜像写入 (`mmio_regs_[bar,offset]=payload`) 仅在 dispatch 成功 (`rc==0`) 时执行; 未映射 BAR 的写直接返 `-ENOSYS` 不镜像不 push
> - §2.3 `mmio_read` 重构: 未映射 BAR 返 `0xFF...FF` (符合 PCI master abort 行为), 不再返 stale 镜像
> - §3 Inv-4 (handler 返回语义) 强化: `rc == 0` = handled; `rc == -ENOSYS` = 未处理, **mmio_write 终止并返 -ENOSYS** (不再"吞掉"); 其他负值 = 错误, mmio_write 终止并返错
> - §7 风险表新增 R6: 23 ABI guard -ENOSYS 路径会暴露 v1.0 隐藏的 test bug (legacy 测试假设 `mmio_write` 返 0 即使 BAR 未启用); 缓解: Phase A 第 0 天 grep 现存 44498 assertions 中假设返 0 的写用例, 逐个标注是否应改为期待 -ENOSYS
> - 设计意图: 修复"spec 名实不符"根因 (M5); 未映射 BAR 不应静默成功, 否则 T-P12-2 之外的所有测试在真实路径未接线时依然全绿

---

## 1. 背景

### 1.1 问题

DGpuBoard v1.0 `dispatch_mmio_to_pcie` (`src/tlm/gpu/dgpu_board_shell.cc:775-800`) 用 4 态 hardcoded switch, **所有路径都是空 break**:

```cpp
void DGpuBoard::dispatch_mmio_to_pcie(uint8_t bar, uint64_t offset,
                                       const void* data, std::size_t len) {
    switch (pcie_path_) {
    case PciePath::Tlp:
        // TLP 路径: mmio_regs_ 镜像已在 mmio_write 写入
        // 完整 TLP 链路 (Encoder → rx_tlp_from_host → CompleterEngine → bar_store_)
        // 由 T-P12-2 E2E 测试覆盖. 此处 mmio_regs_ 作读泵环回退存储.
        break;

    case PciePath::AxiBypass:
        // AXI Bypass 路径: HostBypassTLM::bar_write
        // 当前 mmio_regs_ 镜像已写入; 实际 HostBypassTLM 集成
        // 需完整 SOC 接线 (T-P12-2 E2E 覆盖)
        break;

    case PciePath::Mock:
        // Mock path: PcieMockIP 直调
        // 独立 PcieMockIP 测试在 [mock-ip] 标签覆盖 (T-P9-3)
        break;

    case PciePath::Legacy:
    default:
        // Legacy: mmio_regs_ 既有路径 (已在 mmio_write 完成)
        break;
    }
}
```

**风险**:
- **M5**: spec 名实不符 — `PciePath::Tlp/AxiBypass/Mock` 看起来已实现, 实际只是空 break, T-P12-2 E2E 测试必失败
- 4 态 switch 难以扩展: 新增路径 (e.g., `PciePath::Cxl`, `PciePath::Capi`) 需改 switch
- 测试无法注入: 单元测试难以 mock 不同 dispatch 行为

### 1.2 同类成功案例

`PcieBarRouter` (`include/tlm/gpu/pcie_bar_router_mvp.hh`) 已使用 data-driven 模式:

```cpp
struct RegisterEntry {
    uint32_t    offset;
    std::string name;
    Access      access;
    SideEffect  side_effect;
    uint32_t    value;
    uint32_t    doorbell_stream_id;
};

class PcieBarRouter {
    void add_register(uint32_t offset, const std::string& name, Access access,
                      SideEffect side_effect, uint32_t doorbell_stream_id = 0);
    uint32_t mmio_read(uint32_t offset) const;
    bool     mmio_write(uint32_t offset, uint32_t value, uint32_t trans_id = 0);
private:
    std::unordered_map<uint32_t, RegisterEntry> regs_;
};
```

**优点**: 数据驱动, JSON 注入, 零 if-else 硬编码, 易于扩展 (per ADR-SOC-07 D2)。

### 1.3 现状约束

- `mmio_write` 在 `display_routing_enabled_` + bar==0 路径下走 `PcieDisplayDevice` fast-path (`dgpu_board_shell.cc:320-327`)
- BAR1+0x10010000 doorbell 走 SDMA `mmio_write` 转发 (`dgpu_board_shell.cc:353-376`)
- `pcie_path_` 4 态 switch (Legacy/AxiBypass/Tlp/Mock)

**目标**: 把上述 3 类 dispatch (display routing + BAR1 doorbell + PCIe path) 统一为单一注册表。

---

## 2. 决策

引入 **`DispatchRegistry`** (data-driven dispatch, 与 PcieBarRouter 同风格) 替代所有 hardcoded dispatch 路径。

### 2.1 组件 API

```cpp
namespace tlm::gpu {

// 内部组件, 仅 DGpuBoard 使用
class DGpuBoard::DispatchRegistry {
public:
    // 匹配范围
    enum class Scope : uint8_t {
        BarExact,    // 精确 BAR 编号匹配 (用于 BAR 1 0x10010000 doorbell)
        BarRange,    // BAR 编号 + offset 区间匹配 (用于 BAR 0 display)
        Global,      // 全局匹配 (用于 PciePath)
    };

    // dispatch entry
    // per Oracle v2.0.2 P0-1a: 拆 read/write 双 handler
    //   原因: 旧统一签名 HandlerFn(uint8_t, uint64_t, const void*, size_t) 的 data 是 const,
    //         handler 无法把读出数据写回 caller 的 buf, mmio_read 只能落 mmio_regs_ 镜像,
    //         真实读路径无数据回路 (Oracle 二轮发现的设计空洞)
    struct DispatchEntry {
        Scope       scope;
        Kind        kind;             // per P0-1a: Read / Write, handler 不可混用
        uint8_t     bar;              // BarExact / BarRange
        uint64_t    offset_lo;        // BarRange (含)
        uint64_t    offset_hi;        // BarRange (含)
        uint64_t    offset;           // BarExact
        int          priority;          // 大数优先
        std::string  name;             // 诊断 (e.g., "PcieDisplayDevice BAR 0")
        using ReadHandler  = std::function<int(uint8_t, uint64_t, void* out_buf, size_t len)>;
        using WriteHandler = std::function<int(uint8_t, uint64_t, const void* data, size_t len)>;
        ReadHandler   read_handler;    // kind==Read 时使用, 必须写 *out_buf
        WriteHandler  write_handler;   // kind==Write 时使用
    };

    // 匹配范围
    enum class Kind : uint8_t { Read, Write };   // per P0-1a

    // 注册 handler (按 Kind 区分)
    void register_read_handler(const DispatchEntry& e);    // e.kind == Read
    void register_write_handler(const DispatchEntry& e);   // e.kind == Write

    // 便利 API (供 DGpuBoard::init() 中显式调用)
    void register_bar0_display(DispatchEntry::WriteHandler fn);   // D1 v1.1.1 (写)
    void register_bar0_display_read(DispatchEntry::ReadHandler fn); // D1 v1.1.1 (读, P0-1a 新增)
    void register_bar1_doorbell(DispatchEntry::WriteHandler fn);   // Stage 1.3a (仅写)
    void register_pcie_path_legacy();                              // 默认 Legacy (no-op)
    void register_pcie_path_tlp_write(DispatchEntry::WriteHandler fn);    // T-P12-2 真实 TLP 写
    void register_pcie_path_tlp_read(DispatchEntry::ReadHandler fn);      // T-P12-2 真实 TLP 读 (P0-1a)
    void register_pcie_path_axi_bypass(DispatchEntry::WriteHandler fn);   // T-P12-2 AXI
    void register_pcie_path_mock(DispatchEntry::WriteHandler fn);         // PcieMockIP

    // dispatch (按 priority 降序, 首个匹配胜出; per P0-1a 拆读写)
    int dispatch_read(uint8_t bar, uint64_t offset, void* out_buf, size_t len) const;
    int dispatch_write(uint8_t bar, uint64_t offset, const void* data, size_t len) const;

    // 测试 accessors
    size_t entry_count() const noexcept;
    size_t read_entry_count() const noexcept;    // per P0-1a
    size_t write_entry_count() const noexcept;   // per P0-1a
    std::vector<std::string> matched_read_handlers(uint8_t bar, uint64_t offset) const;
    std::vector<std::string> matched_write_handlers(uint8_t bar, uint64_t offset) const;
    void clear() noexcept;

private:
    mutable std::mutex  mu_;
    std::vector<DispatchEntry>  entries_;   // 按 priority 降序 (插入时排序)

    // 内部: 匹配 entry
    bool matches(const DispatchEntry& e, uint8_t bar, uint64_t offset) const;
};

} // namespace tlm::gpu
```

### 2.2 默认注册顺序 (在 DGpuBoard::init() 中)

```cpp
void DGpuBoard::init() {
    if (soc_) soc_->init();

    // ── DispatchRegistry 默认注册 ──
    dispatch_registry_.clear();

    // 1. D1 display routing (BAR 0, 整个范围 0x00-0xFFF, display_routing_enabled)
    if (display_routing_enabled_) {
        // write 路径 (per P0-1a)
        dispatch_registry_.register_bar0_display(
            [this](uint8_t, uint64_t off, const void* d, size_t len) -> int {
                auto* dev = ep_cache_->get()->display_device();
                if (dev) {
                    return dev.mmio_write(off, d, len);
                }
                return -ENOSYS;  // device 未装, 走 fallback
            });
        // read 路径 (per P0-1a 新增, 修复 mmio_read 数据回路空洞)
        dispatch_registry_.register_bar0_display_read(
            [this](uint8_t, uint64_t off, void* out_buf, size_t len) -> int {
                auto* dev = ep_cache_->get()->display_device();
                if (dev) {
                    return dev.mmio_read(off, out_buf, len);
                }
                return -ENOSYS;
            });
    }

    // 2. BAR1+0x10010000 doorbell (BAR 1, offset 0x10010000, 仅 write)
    dispatch_registry_.register_bar1_doorbell(
        [this](uint8_t, uint64_t, const void* d, size_t len) -> int {
            if (sdma_engine_) {
                // per 评审报告 R-R1 (2027-02-09): parse_wptr 来自 v1.0 helper
                // dgpu_board_shell.cc 私有命名空间 detail::parse_wptr_from_host,
                // 沿用 v1.0 字节序约定 (little-endian, 取 min(len, 8) 字节零扩展到 uint64_t)
                uint64_t wptr = detail::parse_wptr_from_host(d, len);
                sdma_engine_->mmio_write(1, kBar1DoorbellOffset, wptr);
                ++pcie_ep_doorbell_count_;
            }
            return 0;
        });

    // 3. PCIe path 4 态 (按 pcie_path_ 选择, per P0-1a 拆 read/write)
    switch (pcie_path_) {
    case PciePath::Legacy:
        dispatch_registry_.register_pcie_path_legacy();   // no-op (mmio_regs_ 已有)
        break;
    case PciePath::AxiBypass:
        dispatch_registry_.register_pcie_path_axi_bypass(
            [this](uint8_t bar, uint64_t off, const void* d, size_t len) -> int {
                return host_bypass_->bar_write(bar, off, d, len);  // T-P12-2 实施
            });
        break;
    case PciePath::Tlp:
        // write 路径 (per P0-1a)
        dispatch_registry_.register_pcie_path_tlp_write(
            [this](uint8_t bar, uint64_t off, const void* d, size_t len) -> int {
                // per 评审报告 R-R2 (2027-02-09): 长度校验 + memcpy 替代强制转换
                // 避免 *(uint64_t*)d 在 len != 8 时的 strict-aliasing UB / 越界读
                if (len != 4 && len != 8) return -EINVAL;
                uint64_t value = 0;
                std::memcpy(&value, d, std::min(len, sizeof(value)));
                return ep_cache_->get()->mmio_write(bar, off, value);
            });
        // read 路径 (per P0-1a 新增)
        dispatch_registry_.register_pcie_path_tlp_read(
            [this](uint8_t bar, uint64_t off, void* out_buf, size_t len) -> int {
                if (len != 4 && len != 8) return -EINVAL;
                uint64_t value = ep_cache_->get()->mmio_read(bar, off);
                std::memcpy(out_buf, &value, std::min(len, sizeof(value)));
                return 0;
            });
        break;
    case PciePath::Mock:
        dispatch_registry_.register_pcie_path_mock(
            [this](uint8_t bar, uint64_t off, const void* d, size_t len) -> int {
                return mock_ip_->bar_write(bar, off, d, len);
            });
        break;
    }

    // 启动 sim_thread + worker (per ADR-DGPU-01)
    worker_.start();
    sim_thread_ = std::thread(&DGpuBoard::sim_loop, this);

    lifecycle_.transition(BoardState::Configured, BoardState::Initialized, "init()");
}
```

### 2.3 dispatch 替换点

**v1.0** (`dgpu_board_shell.cc:303-379` mmio_write):

```cpp
// 现状: 多个 hardcoded if/else 路径
if (display_routing_enabled_ && bar == 0 && soc_) { /* display device */ }
// ... 然后
if (bar == 1 && offset == kBar1DoorbellOffset) { /* SDMA */ }
// ... 然后
dispatch_mmio_to_pcie(bar, offset, buf, len);
```

**v2.0** (统一 dispatch, per Oracle-4 v2.0.1 mmio_regs_ 不再掩盖失败 + Oracle v2.0.2 P0-1a 拆读写):

```cpp
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    if (!lifecycle_.is_at_least(BoardState::Configured)) return -ENOSYS;
    if (lifecycle_.is_at_least(BoardState::ShuttingDown)) return -ESHUTDOWN;

    // 1. 单一 write dispatch (per registry 优先级, P0-1a: dispatch_write)
    int rc = dispatch_registry_.dispatch_write(bar, offset, buf, len);
    if (rc != 0) {
        // per Oracle-4: 任何非零返回都终止, 不再"吞掉 -ENOSYS 静默成功"
        // 未映射 BAR → -ENOSYS (无镜像, 不 push inject_q, driver 立即收到错误)
        // dispatch 失败 → 错误码原样返回
        metrics_.unmapped_mmio_count.fetch_add(1);   // per Oracle-4 观测性
        return rc;
    }

    // 2. 仅在 dispatch 成功后写入镜像 (用于已确认 handled 的读 roundtrip, 不是 fallback)
    std::vector<uint8_t> payload(static_cast<const uint8_t*>(buf),
                                  static_cast<const uint8_t*>(buf) + len);
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        mmio_regs_[std::make_pair(bar, offset)] = payload;
    }

    // 3. inject_q push (drain 路径, 仅在 dispatch 成功后)
    PendingReq req;
    // ... (原有逻辑)
    return 0;
}

// mmio_read: 未映射 BAR 返 0xFF...FF (per Oracle-4 PCI master abort 行为)
// per Oracle v2.0.2 P0-1b: 走 dispatch_read, read_handler 直接写回 out_buf
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (!lifecycle_.is_at_least(BoardState::Configured)) return -ENOSYS;
    if (lifecycle_.is_at_least(BoardState::ShuttingDown)) return -ESHUTDOWN;

    // 1. read dispatch (同步响应, 模拟真实 BAR 读; P0-1a: dispatch_read)
    //    read_handler 签名 int(uint8_t, uint64_t, void* out_buf, size_t len),
    //    通过 out_buf 直接写回读出数据 — 修复 v2.0.1 "读只能镜像回读" 空洞
    int rc = dispatch_registry_.dispatch_read(bar, offset, buf, len);
    if (rc == 0) {
        return 0;   // handler 已写满 buf, 真实读路径 (display / TLP)
    }

    // 2. rc == -ENOSYS (未映射) → 填 0xFF...FF 返 0 (PCI master abort 行为)
    //    driver probe 时可据此判定 BAR 大小
    if (rc == -ENOSYS) {
        std::memset(buf, 0xFF, len);
        metrics_.unmapped_mmio_count.fetch_add(1);
        return 0;
    }

    // 3. 其他错误 (handler 真实错误如 -EINVAL) → 原样返回, 不吞错 (per Oracle-4 修订)
    metrics_.unmapped_mmio_count.fetch_add(1);
    return rc;
}
```

### 2.4 priority 排序约定

```cpp
// priority 约定 (大数优先):
const int kPriDisplayBar0    = 100;  // BAR 0 display routing (D1)
const int kPriBar1Doorbell   = 90;   // BAR 1 doorbell (Stage 1.3a)
const int kPriPciePathTlp    = 80;   // TLP 路径 (T-P12-2)
const int kPriPciePathAxi    = 80;   // AXI Bypass (T-P12-2)
const int kPriPciePathMock   = 80;   // Mock IP (T-P9-3)
const int kPriPciePathLegacy = 10;   // Legacy 默认 (no-op)
```

**说明**: display routing 最高优先级 (100), 因为它必须先于 PCIe path 处理; BAR1 doorbell 90; PCIe path 80; Legacy 10 (兜底)。同一优先级按注册顺序 (FIFO)。

---

## 3. 关键不变性

### Inv-1: dispatch 按 priority 降序匹配首个 (per P0-1a 拆 read/write)

```cpp
int DispatchRegistry::dispatch_read(uint8_t bar, uint64_t offset,
                                     void* out_buf, size_t len) const {
    std::lock_guard<std::mutex> lock(mu_);
    // 仅扫描 kind==Read 的 entries (per P0-1a)
    // 加速: 先按 bar 索引过滤 read_entries_by_bar_, 再按 priority 降序匹配
    for (const auto& e : read_candidates(bar)) {   // 已按 priority 排序
        if (matches(e, bar, offset)) {
            return e.read_handler(bar, offset, out_buf, len);   // 写回 out_buf
        }
    }
    return -ENOSYS;  // 未匹配
}

int DispatchRegistry::dispatch_write(uint8_t bar, uint64_t offset,
                                      const void* data, size_t len) const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& e : write_candidates(bar)) {  // 已按 priority 排序
        if (matches(e, bar, offset)) {
            return e.write_handler(bar, offset, data, len);
        }
    }
    return -ENOSYS;  // 未匹配
}
```

**保证**: 单一调用只 dispatch 到首个匹配的 handler (类似责任链模式); Read/Write 两类 entries 互不干扰 (per P0-1a)。

### Inv-2: 匹配语义 (同 v2.0.1, 无变化)

```cpp
bool DispatchRegistry::matches(const DispatchEntry& e, uint8_t bar, uint64_t offset) const {
    switch (e.scope) {
    case Scope::BarExact:
        return e.bar == bar && e.offset == offset;
    case Scope::BarRange:
        return e.bar == bar && offset >= e.offset_lo && offset <= e.offset_hi;
    case Scope::Global:
        return true;  // 全局匹配 (用于 PciePath)
    }
    return false;
}
```

### Inv-3: 注册顺序保留 (priority 降序, per P0-1a 按 kind 分桶)

```cpp
void DispatchRegistry::register_read_handler(const DispatchEntry& e) {
    std::lock_guard<std::mutex> lock(mu_);
    read_entries_.push_back(e);   // read_entries_ 独立容器 (P0-1a)
    std::stable_sort(read_entries_.begin(), read_entries_.end(),
                     [](const DispatchEntry& a, const DispatchEntry& b) {
                         return a.priority > b.priority;
                     });
    read_entries_by_bar_[e.bar].push_back(read_entries_.size() - 1);  // bar 索引
}

void DispatchRegistry::register_write_handler(const DispatchEntry& e) {
    std::lock_guard<std::mutex> lock(mu_);
    write_entries_.push_back(e);  // write_entries_ 独立容器 (P0-1a)
    std::stable_sort(write_entries_.begin(), write_entries_.end(),
                     [](const DispatchEntry& a, const DispatchEntry& b) {
                         return a.priority > b.priority;
                     });
    write_entries_by_bar_[e.bar].push_back(write_entries_.size() - 1);
}
```

### Inv-4: handler 返回 0=成功, 非 0=未处理/错误 (per Oracle-4 v2.0.1 + P0-1a)

```cpp
// handler 返回 0: 成功处理, dispatch 结束 (read: out_buf 已写满; write: 数据已消费)
// handler 返回 -ENOSYS: 未处理 (read: mmio_read 填 0xFF...FF; write: mmio_write 返 -ENOSYS)
// handler 返回其他负值: 错误, dispatch 终止返错 (read: mmio_read 原样返回; write: mmio_write 原样返回)
// 同一 entry 的 read_handler 与 write_handler 不可同时非空 (per P0-1a kind 校验)
```

---

## 4. 实施步骤

### 4.1 阶段 A: 基础设施 (W25)

| 任务 | 产出 |
|------|------|
| A1: 新增 `dgpu_board_dispatch_registry.hh` 头文件 | DispatchRegistry API 完整 |
| A2: 单元测试 `test_dgpu_board_dispatch_registry.cc` (12 case) | 100% PASS |

### 4.2 阶段 B: 集成 (W26)

| 任务 | 产出 |
|------|------|
| B1: `DGpuBoard` 添加 `DispatchRegistry dispatch_registry_` 成员 | 注册表就位 |
| B2: `init()` 中按 §2.2 顺序注册 5 类 handler | 集成 dispatch |
| B3: `mmio_write` 重构为单一 dispatch (移除硬编码 if/else) | 简化代码 |
| B4: 移除 `dispatch_mmio_to_pcie` switch (改由 registry 替代) | 减少代码 |

### 4.3 验证

- [ ] `test_dgpu_board_dispatch_registry.cc` 12 case 100% PASS
- [ ] 44498 assertions 100% PASS (0 regression)
- [ ] Oracle 评审通过 (T-P12-2 真实路径实施由独立 change 跟踪)

---

## 5. 测试策略

### 5.1 单元测试

```cpp
TEST_CASE("DispatchRegistry basic priority ordering") {
    DGpuBoard::DispatchRegistry reg;
    int order_called = 0;

    reg.register_entry({Scope::Global, 0, 0, 0, 0,
                        10, "low_prio",
                        [&](uint8_t, uint64_t, const void*, size_t) {
                            order_called = order_called * 10 + 1;
                            return 0;
                        }});

    reg.register_entry({Scope::Global, 0, 0, 0, 0,
                        100, "high_prio",
                        [&](uint8_t, uint64_t, const void*, size_t) {
                            order_called = order_called * 10 + 2;
                            return 0;
                        }});

    REQUIRE(reg.dispatch(0, 0, nullptr, 0) == 0);
    REQUIRE(order_called == 2);  // 仅高优先级调用
}

TEST_CASE("DispatchRegistry BarExact matching") {
    DGpuBoard::DispatchRegistry reg;
    bool called = false;
    reg.register_entry({Scope::BarExact, 1, 0, 0, 0x10010000ULL,
                        90, "bar1_doorbell",
                        [&](uint8_t, uint64_t, const void*, size_t) {
                            called = true;
                            return 0;
                        }});

    REQUIRE(reg.dispatch(1, 0x10010000ULL, nullptr, 0) == 0);
    REQUIRE(called);
    REQUIRE(reg.dispatch(1, 0x10010001ULL, nullptr, 0) == -ENOSYS);  // 偏移不匹配
}

TEST_CASE("DispatchRegistry BarRange matching") {
    DGpuBoard::DispatchRegistry reg;
    int count = 0;
    reg.register_entry({Scope::BarRange, 0, 0x00, 0xFFF, 0,
                        100, "bar0_display",
                        [&](uint8_t, uint64_t, const void*, size_t) {
                            ++count;
                            return 0;
                        }});

    REQUIRE(reg.dispatch(0, 0x00, nullptr, 0) == 0);
    REQUIRE(reg.dispatch(0, 0xFFF, nullptr, 0) == 0);
    REQUIRE(reg.dispatch(0, 0x1000, nullptr, 0) == -ENOSYS);
    REQUIRE(reg.dispatch(1, 0x00, nullptr, 0) == -ENOSYS);
    REQUIRE(count == 2);
}

TEST_CASE("DispatchRegistry handler returns -ENOSYS continues to next") {
    DGpuBoard::DispatchRegistry reg;
    int count = 0;
    reg.register_entry({Scope::Global, 0, 0, 0, 0,
                        50, "no_op",
                        [&](uint8_t, uint64_t, const void*, size_t) {
                            return -ENOSYS;
                        }});
    reg.register_entry({Scope::Global, 0, 0, 0, 0,
                        10, "fallback",
                        [&](uint8_t, uint64_t, const void*, size_t) {
                            ++count;
                            return 0;
                        }});

    REQUIRE(reg.dispatch(0, 0, nullptr, 0) == 0);
    REQUIRE(count == 1);
}
```

### 5.2 集成测试

```cpp
TEST_CASE("DGpuBoard mmio_write routes via DispatchRegistry") {
    DGpuBoard board("test");
    REQUIRE(board.load_soc_config(simple_cfg));
    REQUIRE(board.init());

    int display_called = 0;
    int sdma_called = 0;

    // (测试 helper) 替换注册表中的 handler
    board.dispatch_registry_mut()->register_entry({Scope::BarRange, 0, 0, 0xFFF, 0,
                                                    100, "mock_display",
                                                    [&](uint8_t, uint64_t, const void*, size_t) {
                                                        ++display_called;
                                                        return 0;
                                                    }});

    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    REQUIRE(board.mmio_write(0, 0x10, data, 4) == 0);
    REQUIRE(display_called == 1);
}
```

---

## 6. 兼容性

| 项 | 影响 |
|----|------|
| **23 ABI 签名** | 0 修改 |
| **23 ABI 语义** | 0 修改 (行为保持一致) |
| **JSON 配置** | 0 修改 |
| **`PciePath` 枚举** | 保留, 仅 dispatch 行为变化 (从空 break → 真实 handler) |
| **`display_routing_enabled_` 字段** | 保留 |
| **BAR1 doorbell 行为** | 保留 (`kBar1DoorbellOffset = 0x10010000`) |

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **R1**: T-P12-2 真实 handler 仍为 stub | 中 | spec 名实不符 | 保留 v1.0 no-op stub, 标 TODO 跟随 T-P12-2 |
| **R2**: priority 排序错乱导致路由失效 | 中 | D1 测试 FAIL | 单元测试覆盖 priority 排序 + 集成测试覆盖 D1 routing |
| **R3**: handler 闭包持有外部引用 use-after-free | 低 | 严重 | 仅捕获 `this` 和成员指针, 文档约束 |
| **R4**: dispatch 性能 (注册表扫描) | 低 | 高频路径延迟 | `entries_` 通常 < 10 项, O(n) 扫描 < 100ns |

---

## 8. 参考

- **架构文档**: [14-dgpu-board-ideal-arch.md §3.3](../architecture/14-dgpu-board-ideal-arch.md)
- **ADR-DGPU-01**: CallbackWorker (不涉及)
- **ADR-DGPU-02**: LifecycleProtocol (init() 集成)
- **v1.0 实现**: `src/tlm/gpu/dgpu_board_shell.cc:775-800` (dispatch_mmio_to_pcie)
- **同类模式参考**: `PcieBarRouter` (`include/tlm/gpu/pcie_bar_router_mvp.hh:37-129`)
- **D1 v1.1.1 routing**: `display-device-mvp.md` §Task 4

---

**维护**: CppTLM 开发团队
**状态**: 📋 提案
---

## Status Update

> 2027-02-10 前置清理已完成（per openspec/changes/cpptlm-dgpu-board-abi-hygiene change）

- **B4 (移除 `dispatch_mmio_to_pcie` switch) 前置清理已落地**：v1.0 中的 `DGpuBoard::dispatch_mmio_to_pcie` 4 态空 switch 与上方的 `// ── T-P12-1: dispatch_mmio_to_pcie ──` 分区注释、`mmio_write` 中的调用行、以及 `dgpu_board_shell.hh:257` 公共声明，**均已在 change `cpptlm-dgpu-board-abi-hygiene` (Wave 3b) 中整体删除**（探针：`grep -rn "dispatch_mmio_to_pcie" src/ include/ test/` 返 0；零行为变更验证：`[pcie-bypass-tlp]` 17/3 PASS + 全量 66864/1553 零回归）。
- **本 ADR 仍为 📋 提案**：B4 后续的"由 registry 替代"工作（DispatchEntry / ReadHandler / WriteHandler 双签名 / Scope 优先级）尚未实施。该工作将在 ADR 落地阶段（计划 W25-27）展开，前置清理为零冲突起点。
- **不变性保留**：`PciePath` 枚举（4 态 `Legacy`/`AxiBypass`/`Tlp`/`Mock`）、`pcie_path_` 成员、`attach_profile()` 与 `pcie_path()` accessor 全部保留；`endpoint_bar_store_value` 保留；`PcieEndpointIP` 短名生产注册补回（Wave 3.0，已修复 `msix_*`/`pcie_config_*` 在短名 config 下静默 -ENOSYS 的 bug）。
- **历史记录不可修改**：本 ADR §1-§8 内容未做任何文字修改（per AGENTS.md "ADR 不可变" 规则）。
