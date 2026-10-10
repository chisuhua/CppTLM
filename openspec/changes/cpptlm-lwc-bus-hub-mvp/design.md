# Design — cpptlm-lwc-bus-hub-mvp v1.0 (Phase 0 Change B)

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/lwc-bus-hub-mvp/spec.md`](specs/lwc-bus-hub-mvp/spec.md)
> **目标 tag**: `cpptlm-lwc-bus-hub-mvp-completion`
> **估时**: **~8d** (TDD 5-step + 框架扩展 + 异构 Bundle adapter)
> **Frozen surface**: 23 ABI 签名零 diff + **4 frozen headers 零 diff + 1 quasi-frozen** (additive only, per AGENTS.md H3 D-AXI 冻结面: `cpptlm_emulator.h` + `pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh`; quasi = `pcie_endpoint_ip.hh` 仅新增 API, 不改既有 17 端口 signature)
> **依赖**: 无 (self-contained; 实施期间与 Change A 并行)
>
> **🔗 Cross-Reference (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3, 2026-10-10)**:
> 本 change 提议的 `include/tlm/lwc/lwc_bus_hub.hh` 路径已与 `docs/designs/dgpu-soc/architecture.md §0.1.1:129` 引用对齐 (per Metis M-4, 修 `cluster/` → `lwc/` 路径错误)。本 change v0.5.2 修订时**仅需维持 `lwc/` 路径**。DoorbellBundle 字段数应以 4 字段方向为准 (per M-9, 避免 Change B 内部残留字段数)。详见 [cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md §M-9](../../cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md)。

---

## 总体设计

### 核心思路

**目标**: 引入 1 个统一的轻量级总线基础设施 (`LightweightInterconnect` LWC) + 2 个 Bundle 类型 (`CrBundle` + `DoorbellBundle`), 把 BAR0 寄存器访问、BAR1 doorbell、MMIO Read 3 条路径从 process-direct 注入式协调改为真实 wire-format transaction.

**不变量 (Locked)**:
- **§0.1 原则 3.1**: 0 callback 注入 (Hard Invariant)
- **§0.1 原则 3**: wire 接口替代直接函数调用 (Hard Invariant)
- **§0.1.1 LWC 总线原则**: 树形/星形, 与 SocInternalNoc mesh 正交分层
- **23 ABI 签名零变更** (per ADR-088 §D5)
- **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面: `cpptlm_emulator.h` + `pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh`; quasi = `pcie_endpoint_ip.hh` 仅新增 API, 不改既有 17 端口 signature)

### 系统架构 (实施后)

```
                         ┌───────────────────────────────────────────┐
                         │ LightweightInterconnect (LWC) Bus Hub     │
                         │   (1 Hub + 4-32 IP port, tree/star)       │
                         │                                           │
   PCIe EP (TLP→CR/DB)   │  InputStreamAdapter<CrBundle> cr_in       │
   ─────────────────►    │  InputStreamAdapter<DoorbellBundle> dbin │
   (PcieEndpointIP::      │  ┌─────────────────────────────────┐      │
    lwc_hub_->cr_write)        │  │ 4-32 IP ports: (per v1.0.1 B-H1 移除 emit_cr_write; DGpuBoard 直调 hub) │      │
   ─────────────────►    │  │  port0 = GmmuTLM (cr_in)        │      │
   (PcieEndpointIP::      │  │  port1 = SdmaEngineTLM (db_in)  │      │
    emit_doorbell)        │  │  port2..N = (预留)               │      │
                         │  └─────────────────────────────────┘      │
                         │       │                                   │
                         │       │ Internal Routing:                 │
                         │       │  CR transaction → dst.cr_in       │
                         │       │  Doorbell transaction → dst.db_in │
                         │       │ (per Bundle type discriminator)   │
                         └───────────────────────────────────────────┘
```

### Bundle 选型 (per §0.1.1 锁定)

| Bundle | 文件 | 字段 | 协议语义 | 修复 Gap |
|--------|------|------|----------|----------|
| **`CrBundle`** | `include/bundles/cr_bundles_tlm.hh` (新增) | 8 字段 (addr + wdata + rdata + write + ready + valid + error + transaction_id) | APB-lite 风格 (轻量级配置寄存器访问) | Gap H + Gap K |
| **`DoorbellBundle`** | `include/bundles/doorbell_bundles_tlm.hh` (新增) | 5 字段 (wdata[4B] + ring_id[8b] + posted[1b] + valid[1b] + ready[1b]) | PCIe posted write → SDMA ring consumer (`valid` 由 producer 写, `ready` 由 SDMA consumer 写) | Gap J |
| **`MsixBundle`** | (deferred, Change E Phase 2) | (future) | MSI-X 中断交付 | Gap L (后续) |
| **`ErrBundle`** | (deferred, Change F Phase 3) | (future) | Error aggregation bus | Gap O (后续) |

---

## API 设计

### 1. `LwcBusHub` 模块 (新增, `include/tlm/lwc/lwc_bus_hub.hh`)

```cpp
namespace tlm::lwc {

// LWC Bus Hub: 1 Hub + 4-32 IP port (tree/star)
class LwcBusHub : public ChStreamModuleBase {
public:
    explicit LwcBusHub(const std::string& n, EventQueue* eq);
    
    // IP port 注册 (per IP type)
    void register_cr_target(const std::string& ip_name,
                           cpptlm::InputStreamAdapter<bundles::CrBundle>* cr_in);
    void register_doorbell_target(const std::string& ip_name,
                                  cpptlm::InputStreamAdapter<bundles::DoorbellBundle>* db_in);

    // Hub 主入口: 接收 PCIe EP 的 transaction
    void cr_write(const bundles::CrBundle& tx);

    // [v1.0.1 H-B3 FIX] CR-Bus 同步读 (mmio_read 路径, per §3 mmio_read 改造)
    std::future<bundles::CrBundle> cr_read(const bundles::CrBundle& tx);

    // 线程模型 (per v1.0.1 B-B2 FIX): 跨 host↔sim 线程安全
    // - cr_write (host thread, DGpuBoard::mmio_write) push 到 pending_cr_queue_
    // - LwcBusHub::tick (sim thread) drain pending → push to target cr_in_ → set_valid
    std::mutex pending_cr_queue_mutex_;
    std::queue<bundles::CrBundle> pending_cr_queue_;
    std::mutex pending_doorbell_queue_mutex_;
    std::queue<bundles::DoorbellBundle> pending_doorbell_queue_;
    void doorbell_write(const bundles::DoorbellBundle& tx);
    
    // 测试钩子
    uint64_t routed_count() const noexcept { return routed_count_; }
    uint64_t dropped_count() const noexcept { return dropped_count_; }
    
    // ChStreamModule Base 接口
    void tick() override;
    
private:
    std::unordered_map<std::string, cpptlm::InputStreamAdapter<bundles::CrBundle>*> cr_targets_;
    std::unordered_map<std::string, cpptlm::InputStreamAdapter<bundles::DoorbellBundle>*> db_targets_;
    uint64_t routed_count_ = 0;
    uint64_t dropped_count_ = 0;
};

}  // namespace tlm::lwc
```

### 2. `CrBundle` (新增, `include/bundles/cr_bundles_tlm.hh`)

```cpp
namespace tlm::bundles {

// CrBundle: APB-lite 风格 (8 字段, max width 64-bit per P0 fix 2026-10-06)
class CrBundle : public ChStreamBundleBase {
public:
    using Bundle = CrBundle;
    
    // 8 字段 (compiled via BUNDLE_FIELD_WIDTH macro)
    ch_uint<32> address;        // register file byte address (low 32 bits)
    ch_uint<32> wdata;          // 32-bit write data
    ch_uint<32> rdata;          // 32-bit read response data
    ch_uint<1>  write;          // 1=write, 0=read
    ch_uint<1>  ready;          // ready (从 target 返)
    ch_uint<1>  valid;          // valid (从 source 发)
    ch_uint<2>  error;          // 0=OK, 1=DECERR, 2=SLVERR
    ch_uint<8>  transaction_id; // req id (8-bit 足够 APB lite)
};

}  // namespace tlm::bundles
```

**字段语义注释** (用于后续团队对齐):
- `address`: register file offset, 与 PCI BAR0 子窗口 (4KB) 对齐
- `wdata`: 仅 `write=1` 时有意义 (写 PT_BASE_LO/HI/ENABLE 等 32-bit 寄存器)
- `rdata`: 仅 `write=0` 时有意义 (读 PT_BASE_LO/HI 等)
- `error`: SLVERR 传播 per D-AXI v1.4 B9 (write-mirror 删除, 用 ready/valid 同步)

### 3. `DoorbellBundle` (新增, `include/bundles/doorbell_bundles_tlm.hh`)

```cpp
namespace tlm::bundles {

// DoorbellBundle: PCIe posted write 语义 (5 字段, v0.5.1 修订: 加 valid 字段 per B-H4 完整)
class DoorbellBundle : public ChStreamBundleBase {
public:
    using Bundle = DoorbellBundle;

    // 5 字段 (max width 64-bit)
    ch_uint<32> wdata;     // 4-byte write payload (doorbell wptr)
    ch_uint<8>  ring_id;   // 0=doorbell ring 0
    ch_uint<1>  posted;    // 1=posted (PCIe posted write, no completion TLP)
    ch_uint<1>  valid;     // 1=producer writes valid (per B-H4 v0.5.1 修订: ready 是 consumer 驱动, valid 才是 producer 写)
    ch_uint<1>  ready;     // SDMA ring consumer writes ready (consumer 驱动, ready 表明 SDMA 已消费)
};

}  // namespace tlm::bundles
```

### 4. `MultiPortHeterogeneousBundleAdapter` (新增, `include/framework/stream_adapter.hh`)

```cpp
namespace cpptlm {

// 异构 Bundle Multi-Port Adapter (per D-AXI v1.5 B25 决策沿袭)
// 支持同 IP 上挂多个不同 Bundle 类型的 port
template <typename... BundleTs>
class MultiPortHeterogeneousBundleAdapter : public StreamAdapterBase {
public:
    explicit MultiPortHeterogeneousBundleAdapter(
        std::tuple<InputStreamAdapter<BundleTs>*...> inputs);
    
    void tick() override;
    
    // 异构 tuple → 访问单个 bundle
    template <typename Bundle>
    InputStreamAdapter<Bundle>* get() noexcept;
    
private:
    std::tuple<InputStreamAdapter<BundleTs>*...> inputs_;
    // dispatch logic: per Bundle type
};

}  // namespace cpptlm
```

**实现要点**:
- `std::tuple` 持有 N 个不同类型的 InputStreamAdapter
- `tick()` 遍历所有 InputStreamAdapter, 处理 valid 的 transaction
- 每个 IP 可以声明 `cr_in: InputStreamAdapter<CrBundle>` + `doorbell_in: InputStreamAdapter<DoorbellBundle>` 双异构 port

### 5. PcieEndpointIP 集成 (per v1.0.1 B-H1 移除)

> **v1.0.1 B-H1 修订**: 本 change **不修改** `PcieEndpointIP::match_bar` 和 `PcieEndpointIP` 类（quasi-frozen header 0 修改）. 

**集成路径**: `DGpuBoard::mmio_write` 直调 `lwc_hub_->cr_write(tx)` (host thread push 到 `pending_cr_queue_`, per v1.0.1 B-B2 线程模型). **不走 PcieEndpointIP 桥接** (YAGNI defer to Change D `cpptlm-pcie-tlp-entry-layer`, 届时经 `submit_tlp()` API 整合).

**冻结面状态**:
- `include/tlm/pcie/pcie_endpoint_ip.hh`: **0 修改** (既无新增 API 也无删除)
- 既有 17 端口 signature 全保留

> 后续 Change D `cpptlm-pcie-tlp-entry-layer` Phase 1.3 实施 `submit_tlp(const PcieTlpBundle&)` 时, 将整合 LWC 总线入口 (与本 change 的 `cr_in` + `doorbell_in` 协议对齐, 经 BAR decode 后 dispatch).

### 6. `GmmuTLM` 集成 (修改, `include/tlm/gpu/gmmu_tlm.hh`)

```cpp
class GmmuTLM : public ChStreamModuleBase {
public:
    // 既有 API 不变
    
    // [v1.0 NEW] CR-Bus SlavePort (per H-B6 part-of-LWC)
    cpptlm::InputStreamAdapter<bundles::CrBundle>& cr_in() { return cr_in_; }
    
    // [v1.0 DEPRECATED] 旧 setter 标记 deprecation (但保留 0.5d 兼容窗口)
    [[deprecated("use CrBundle via cr_in()")]]
    void set_pt_base_lo(uint32_t lo) noexcept { 
        // 兼容性 fallback: 直接赋值 (per H-B3)
        pt_base_lo_ = lo;
    }
    
private:
    cpptlm::InputStreamAdapter<bundles::CrBundle> cr_in_;
};
```

### 7. `SdmaEngineTLM` 集成 (修改, `include/tlm/gpu/sdma_engine_tlm.hh`)

```cpp
class SdmaEngineTLM : public ChStreamModuleBase {
public:
    // 既有 API 不变
    
    // [v1.0 NEW] DoorbellBus SlavePort
    cpptlm::InputStreamAdapter<bundles::DoorbellBundle>& doorbell_in() { return doorbell_in_; }
    
    // [v1.0 DEPRECATED] 旧 setter 兼容
    [[deprecated("use DoorbellBundle via doorbell_in()")]]
    int mmio_write(uint8_t /*bar*/, uint64_t /*offset*/, uint64_t wptr) noexcept;
    
private:
    cpptlm::InputStreamAdapter<bundles::DoorbellBundle> doorbell_in_;
};
```

---

## 集成路径修改

### 1. BAR0 CR-Bus 迁移 (Phase 3)

**修改前** (`dgpu_board_shell.cc:463-472`, 4 处直调):
```cpp
if (offset == 0x00) {
    gmmu_->set_pt_base_lo(val32);    // ← 删除
} else if (offset == 0x04) {
    gmmu_->set_pt_base_hi(val32);    // ← 删除
} else if (offset == 0x08) {
    gmmu_->set_enabled(...);          // ← 删除
}
```

**修改后** (CR-Bus 路径):
```cpp
if (bar == 0 && offset < 0x100 && gmmu_ != nullptr && lwc_hub_ != nullptr) {
    tlm::bundles::CrBundle cr_tx;
    cr_tx.address.write(static_cast<uint32_t>(offset));
    cr_tx.wdata.write(val32);
    cr_tx.write.write(1u);          // 1=write
    cr_tx.valid.write(1u);
    cr_tx.transaction_id.write(0);  // future increment
    
    // 路由到 LWC Hub → GMMU cr_in
    lwc_hub_->cr_write(cr_tx);
}
```

### 2. BAR1 doorbell DoorbellBus 迁移 (Phase 4)

**修改前** (`dgpu_board_shell.cc:537-538`):
```cpp
sdma_engine_->mmio_write(/*bar=*/1, /*offset=*/kBar1DoorbellOffset,
                         /*data=*/doorbell_wptr);  // ← 删除
```

**修改后** (DoorbellBus 路径):
```cpp
if (bar == 1 && offset == kBar1DoorbellOffset && lwc_hub_ != nullptr) {
    tlm::bundles::DoorbellBundle db_tx;
    db_tx.wdata.write(static_cast<uint32_t>(doorbell_wptr));
    db_tx.ring_id.write(0);          // SDMA ring 0 (BAR1 doorbell)
    db_tx.posted.write(1u);          // PCIe posted write (no completion TLP)
    db_tx.valid.write(1u);           // [v0.5.1 B-H4 完整修订] producer 写 valid; SDMA consumer tick 写 ready 表明已消费
    db_tx.valid.write(1u);           // [v1.0.1 B-H4 FIX] posted write producer 写 valid; ready 由 SDMA consumer tick 写
    
    lwc_hub_->doorbell_write(db_tx);
}
```

### 3. MMIO Read 重写 (Phase 5, 关键 invasive change)

**修改前** (`dgpu_board_shell.cc:305-388` + 495-501 + 526-529`, ~50 行):
- `mmio_regs_[bar,offset] = payload` write-mirror
- 异步 `inject_q_.push_back()` + `pending_resp_.wait_for(kMmioWaitTimeout)` 50ms

**修改后** (CR-Bus 同步 AR/R):
```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    // ... lifecycle guard (保留) ...
    
    // CR-Bus AR for BAR0
    if (bar == 0 && lwc_hub_ != nullptr) {
        tlm::bundles::CrBundle cr_tx;
        cr_tx.address.write(static_cast<uint32_t>(offset));
        cr_tx.write.write(0u);       // 0=read
        cr_tx.valid.write(1u);
        cr_tx.transaction_id.write(0);
        
        // 等待 ready/valid handshake (50ms timeout = implicit PCIe completion timeout)
        auto fut = lwc_hub_->cr_read(cr_tx);
        if (fut.wait_for(kMmioWaitTimeout) == std::future_status::ready) {
            auto resp = fut.get();
            std::memcpy(buf, &resp.rdata, std::min(len, sizeof(resp.rdata)));
            return 0;
        }
        return -ETIMEDOUT;  // implicit PCIe completion timeout
    }
    
    // BAR1/BAR2 仍走现有 fast-path (per backward compat, 暂不修改)
    // ... 既有 BAR1 fast-path / BAR2 VramController 路径 ...
}
```

**backward compat**:
- `mmio_regs_` 删除 (H-B2 Hard Invariant)
- `inject_q_` 仅用于 mmio_read (not mmio_write); mmio_write 仍走现有 BAR-keyed 写镜像 (作为 write-mirror 仅在 mmio_write, 不在 mmio_read)
- `kMmioWaitTimeout` 50ms 保留 (作为 implicit PCIe completion timeout)

### 4. `LWC Bus Hub` 注册与装配

**注册** (`include/modules_cluster.hh`):
```cpp
REGISTER_CHSTREAM(LwcBusHub);  // [v1.0.1 B-B1 FIX] ChStreamModuleBase 派生, 用 REGISTER_CHSTREAM (非 REGISTER_MODULE, 否则 static_assert<SimModule> 失败)
```

**装配** (`DGpuBoard::bind_memory_backings()`):
```cpp
// [v1.0 NEW] LWC Hub 注入 (per §0.1.1)
if (auto* lwc_hub = dynamic_cast<tlm::lwc::LwcBusHub*>(
        soc_->getInternalInstance("lwc_hub"))) {
    lwc_hub_ = lwc_hub;
    
    // 注册 GMMU
    if (auto* gmmu = dynamic_cast<GmmuTLM*>(
            soc_->getInternalInstance("gmmu"))) {
        lwc_hub->register_cr_target("gmmu", &gmmu->cr_in());
    }
    
    // 注册 SDMA
    if (auto* sdma = dynamic_cast<SdmaEngineTLM*>(
            soc_->getInternalInstance("sdma"))) {
        lwc_hub->register_doorbell_target("sdma", &sdma->doorbell_in());
    }
}
```

**JSON 顶层配置** (`configs/dgpu_soc_minimal_v1.json`):
```json
{
  "name": "lwc_hub",                              // [v1.0 NEW]
  "type": "LwcBusHub",
  "params": {
    "max_ip_ports": 8,
    "bundle_types": ["cr", "doorbell"]
  }
}
```

---

## Backward Compatibility 策略

### Backward compat 表

| 场景 | 行为 |
|------|------|
| **既存 `[lwc]` 测试** | 0 cases → 24+ 新 cases (per Acceptance) |
| **既有 `[minimal_dgpu_soc]` 测试** | ✅ backward compat: setter 标记 deprecated 但 0.5d 兼容窗口保留; 路径内部 try-catch 检查 (per R3 缓解) |
| **既有 `[dgpu_soc_timing]` 测试** | ✅ 0 触动 (LWC 仅在 functional-mode 启用) |
| **既有 23 ABI 测试** | ✅ 0 diff (新增 API 在 chip-internal 层, 不触 abi) |
| **既有 4 frozen + 1 quasi-frozen headers** | ✅ 0 diff (`pcie_endpoint_ip.hh` quasi 仅新增 API, 不删既有) |

### deprecation 兼容策略

| Item | deprecation 处理 | 落地时间 |
|------|------------------|---------|
| `GmmuTLM::set_pt_base_lo/hi/enabled()` | `[[deprecated]]` 标记 + fallback (直接赋值) | 0.5d 兼容 → 1d 后正式删 |
| `SdmaEngineTLM::mmio_write(1, kBar1DoorbellOffset)` | `[[deprecated]]` 标记 + fallback | 同 |
| `DGpuBoard::mmio_regs_` (write-mirror) | 删除 | 仅在 mmio_write path 保留 (BAR-keyed); mmio_read 删除 |
| `inject_q_` 异步 drain | 仍保留 (mmio_write path + 异步注入); mmio_read 删除 | 同 |

---

## 关键设计不变式

- **H-B1**: LwcBusHub 类不破坏既有 SimModule 签名
- **H-B2**: 0 callback 注入 (Hard Invariant, per §0.1 原则 3.1)
- **H-B3**: 0 mmio_regs_ write-mirror (Hard Invariant, per Gap K 修复)
- **H-B4**: 23 ABI 签名零变更 (per ADR-088 §D5 + AGENTS.md H3)
- **H-B5 (v0.5.1 修订)**: **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面: `cpptlm_emulator.h` + `pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh`; quasi = `pcie_endpoint_ip.hh` 仅新增 API, 不改既有 17 端口 signature)
- **H-B6**: LWC 总线树形/星形, 与 SocInternalNoc mesh 正交分层 (§0.1.1 Hard Invariant)
- **H-B7**: MultiPortHeterogeneousBundleAdapter 模板必须支持异构 Bundle 注入同 port (per design §4)
- **H-B8**: 异构 Bundle adapter 实施时同时 extend `framework/stream_adapter.hh` (per LWC 风险 R1 缓解)

---

## 测试策略

### 测试分层 (per AGENTS.md + `cpptlm-debug` skill 5 步 + 4 件套验证)

1. **LWC 单元测试 (`[lwc][unit]`)** — 12 cases:
   - LwcBusHub 创建/销毁
   - 4 port / 8 port / 32 port 配置
   - 单 CrBundle transaction routing 到正确 target
   - 单 DoorbellBundle transaction routing 到正确 target
   - 异构 Bundle 同 IP 注册 (e.g., 1 IP 同时接 CrBundle + DoorbellBundle)
   - Unknown target → dropped_count 增, 0 crash
   - 32 port max + 不存在的 port 33 → fail soft
   - 并发 4 transaction → routed_count 正确
   - 重入测试 (LWC Hub tick() 多次调用)
   - `register_*_target` setter 测试
   - 0 路由 fallback (no target) 测试
   - routed/dropped 计数器

2. **LWC 集成测试 (`[lwc][integration]`)** — 6 cases:
   - BAR0 PT_BASE_LO 写 → GMMU.PT_BASE_LO 寄存器更新 (经 CrBundle)
   - BAR0 PT_BASE_HI 写 → GMMU.PT_BASE_LO 寄存器更新
   - BAR0 GMMU_CTRL 写 → GMMU.enabled 寄存器更新
   - BAR0 PT_BASE_LO 读 → 同寄存器读回 (经 CrBundle R data)
   - BAR1 doorbell 写 → SDMA ring wptr 更新 (经 DoorbellBundle)
   - BAR1 doorbell 写 → SDMA 触发 ring consume

3. **BAR0 集成测试 (`[lwc][bar0]`)** — 3 cases:
   - host mmio_write(BAR0, GMMU_PT_BASE_LO, val32) → GMMU 收到 CrBundle → PT_BASE_LO 寄存器更新
   - host mmio_write(BAR0, GMMU_PT_BASE_HI, val32) → GMMU 收到 CrBundle → PT_BASE_HI 寄存器更新
   - host mmio_write(BAR0, GMMU_CTRL, enable=1) → GMMU.enabled == true

4. **Doorbell 集成测试 (`[lwc][bar1-doorbell]`)** — 3 cases:
   - host mmio_write(BAR1, 0x10010000, wptr) → SDMA ring wptr == wptr
   - host mmio_write(BAR1, 0x10010000, wptr=0) → SDMA ring 0 stop (consumer 空)
   - host mmio_write(BAR1, 0x10010000, wptr=N) → SDMA ring consumer 触发 N 次 (per ring 协议)

5. **MMIO Read 测试 (`[lwc][mmio-read]`)** — 3 cases:
   - host mmio_read(BAR0, GMMU_PT_BASE_LO) → 同步 CR-Bus AR/R, 返回 PT_BASE_LO 值
   - host mmio_read(BAR0, GMMU_CTRL) → 同上 ENABLE 状态
   - host mmio_read(BAR0, non-existent offset) → 同步 CR-Bus AR + SLVERR propagation

6. **回归锁** — 既有 6 套件 (per Acceptance)

### 总测试规模

| 测试套 | cases | 估时 |
|--------|-------|------|
| `[lwc][unit]` | 12 | 2d |
| `[lwc][integration]` | 6 | 0.5d |
| `[lwc][bar0]` | 3 | 0.5d |
| `[lwc][bar1-doorbell]` | 3 | 0.5d |
| `[lwc][mmio-read]` | 3 | 0.5d |
| 既有 6 套件 0 回归锁 | (随 Phase 5.3) | 0.5d |
| **总计** | **27 cases / 100+ assertions** | **4.5d** |

### 关键测试 invocation

```bash
# Phase 1-4 TDD RED/GREEN
./build/bin/cpptlm_tests "[lwc]" --reporter compact

# Phase 5 回归锁
./build/bin/cpptlm_tests "[minimal_dgpu_soc]" --reporter compact
./build/bin/cpptlm_tests "[driver_visible]" --reporter compact
./build/bin/cpptlm_tests "[dgpu_soc_timing]" --reporter compact
./build/bin/cpptlm_tests "[pcie-memory]" --reporter compact
./build/bin/cpptlm_tests "[pcie]" --reporter compact
./build/bin/cpptlm_tests "[realip]" --reporter compact

# Change A 同步验证 (并行 Phase 0)
./build/bin/cpptlm_tests "[vram_controller][functional]" --reporter compact
```

---

## 风险与缓解

| 风险 | Phase | 缓解 |
|------|-------|------|
| **R1** LWC 异构 Bundle Multi-Port Adapter 框架限制 (per D-AXI v1.5 B25) | Phase 2 | 同步扩展 `framework/stream_adapter.hh` 加 `MultiPortHeterogeneousBundleAdapter<BundleTs...>` 模板 (per H-B7 + H-B8) |
| **R2** frozen headers diff (v0.5.1 修订: 4 frozen + 1 quasi) | Phase 3-5 | 既有 signature 0 改动 (quasi `pcie_endpoint_ip.hh` 仅新增 API); commit 前 `git diff HEAD -- <4-frozen>` 必须为空 |
| **R3** GmmuTLM::set_pt_base_* 后向兼容窗口 break | Phase 3 | 标记 `[[deprecated]]` 保留 0.5d 兼容窗口 (per H-B3); LWC path 优先, 旧路径 fallback 走直接赋值 |
| **R4** SdmaEngineTLM::mmio_write (1, kBar1DoorbellOffset,...) 兼容 break | Phase 4 | 同 R3 策略 |
| **R5** MMIO Read 重写路径失败 (50ms timeout 触发) | Phase 5 | 50ms 保留为 implicit PCIe completion timeout (per R5 framing); 同步 ready/valid + 上下文 hold |
| **R6** 异构 Bundle tuple dispatch 性能退化 | Phase 2 | 测试 + 性能 sanity-check (per ADR-DGPU-11 H2); 若 >0.1us overhead, 考虑 inline specialization |
| **R7** 与 Change A 同步冲突 (LWC 路径 vs VramController 路径 重叠) | Phase 0 | Change A 走 `pcie_memory.handle_slave_port` 数据路径; Change B 走 `DGpuBoard::mmio_*` 寄存器/doorbell 路径; **不同函数**, 0 冲突 |

---

## 文件清单 (实施追踪)

| 文件 | 类型 | 估时 |
|------|------|------|
| `include/tlm/lwc/lwc_bus_hub.hh` | 新增 (~120 行) | 0.5d |
| `include/tlm/lwc/lwc_bus_hub.cc` | 新增 (~150 行) | 0.5d |
| `include/bundles/cr_bundles_tlm.hh` | 新增 (~80 行) | 0.25d |
| `include/bundles/doorbell_bundles_tlm.hh` | 新增 (~60 行) | 0.25d |
| `include/framework/stream_adapter.hh` | 扩展 (+~80 行模板) | 0.5d |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 修改 (+~30 行 API) **(v1.0.1 B-H1 移除, defer to Change D)** | 0.25d → 0 |
| `include/tlm/gpu/gmmu_tlm.hh` | 修改 (+~30 行 cr_in + deprecation) | 0.25d |
| `include/tlm/gpu/sdma_engine_tlm.hh` | 修改 (+~25 行 doorbell_in + deprecation) | 0.25d |
| `include/modules_cluster.hh` | 修改 (+1 行 REGISTER_MODULE) | 0.05d |
| `src/tlm/gpu/dgpu_board_shell.cc` | 修改 (~80 行 invasive change) | 1d |
| `configs/dgpu_soc_minimal_v1.json` | 修改 (+~10 行 lwc_hub 节点) | 0.1d |
| `test/test_lwc_bus_hub.cc` | 新增 (~300 行) | 1d |
| `test/test_lwc_bus_hub.cc` | 新建 (~300 行, 27 cases [lwc][unit]/[integration]/[bar0]/[bar1-doorbell]/[mmio-read]) | 2d |
| `docs/implementation/2026-10-09-lwc-bus-hub-mvp.md` | 新增 (~80 行) | 0.25d |
| `docs/designs/dgpu-soc/architecture.md` §0.2 Gap H/J/K ✅ 修订 | 文档同步 | 0.25d |
| `docs/designs/dgpu-soc/architecture.md` §0.1.1 cross-reference (LWC 总线基础设施规则) | 文档同步 | 0.1d |
| `AGENTS.md` KEY INVARIANTS LWC 锁定 + H-B5 | 文档同步 | 0.25d |
| **总计** | (估计 8d) | **~7.5d** (含 0.5d buffer) |

---

## 相关文档

- 业务定位锁定: `docs/designs/dgpu-soc/architecture.md` §0.1 (原则 3 + 双重无注入)
- 总体实施计划: `docs/designs/dgpu-soc/architecture.md` §0.2 (Phase 0 Change B)
- 修复路线图方向: `docs/designs/dgpu-soc/architecture.md` §0.1.1 (LWC 总线原则)
- D-AXI v1.4 实施笔记: `docs/pcie/driver-visible-minimal-soc.md` (B25 决策沿袭)
- 异构 Bundle Multi-Port Adapter 决策: D-AXI v1.5 B25 (`docs/pcie/driver-visible-minimal-soc.md` §8.1)
- Codebase types: `include/bundles/`, `include/framework/stream_adapter.hh`, `include/tlm/cluster/`

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-09 (v1.0 起草, Phase 0 Change B 启动)
