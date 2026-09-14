# Design: cpptlm-stage-1-4-2-1-followups — 6 子任务详细设计

> **Status**: Proposed (2027-02-09)
> **Owner**: Sisyphus (after proposal approval)

---

## §0 边界与交叉引用

### 0.1 来自 predecessor (2026-09-10) 的不变量继承

| ID | 不变量 | 保留状态 |
|----|--------|----------|
| INV-A | D3hot MMIO gate | **扩展为仅 BAR 路径**（cfg 路径不 gate）|
| INV-B | ASPM exit latency | 保持 |
| INV-C | Resizable BAR disable→reprogram→enable | 保持 |
| INV-D | P2P no silent fallback | 保持 + 扩展 SUCCESS 路径 |

### 0.2 新增不变量

| ID | 不变量 | 落地 |
|----|--------|------|
| INV-E | D3hot cfg 写必须经 PMCSR 触发状态机回到 D0 | tick() 路径修正 |
| INV-F | P2P 路由返回值显式语义不依赖 ACS 策略默认 | P2PResult enum 不变 |
| INV-G | ResizableBar size 变更后越界访问被拒绝 | bar_store_ 边界校验 |

---

## §1 子任务 1: INV-A gate 收窄到 BAR only + UR 响应

### 1.1 现状

`src/tlm/pcie/pcie_endpoint_ip.cc:233-235`:
```cpp
if (mmio_gated_) {
    return;  // 早返回, 阻止所有 AXI slave 处理
}
```

问题: cfg 写路径（line 266 调 `pool_.config_of(0).write()`）也在 tick() 内。D3hot 下 cfg 写被 gate → driver 无法经数据路径写 PMCSR 回 D0。

### 1.2 决策 — 移到 cfg 写路径之后

```cpp
void PcieEndpointIP::tick() {
    // Stage 8 M1: AXI 数据路径接线 (HostBypass/RC ↔ PcieAxiAdapter)
    if (auto* ax = PcieAxiAdapter::for_endpoint(getName())) {
        cpptlm::Axi4StreamAdapter& axi = ax->axi();
        if (axi.slave_req_valid()) {
            const bundles::Axi4Bundle& req = axi.slave_req_data();
            const uint64_t awaddr = req.is_write_request() ? req.awaddr.read() : req.araddr.read();
            const bool is_cfg = awaddr < pool_.config_of(0).config_size();

            // INV-A 收窄: 仅 BAR 路径 gate; cfg 路径保留 D3hot 访问 (PCIe spec)
            if (mmio_gated_ && !is_cfg) {
                // 关键: 显式响应 UR / DECERR (PCIe spec Master Abort),
                // 否则 host 等待响应永挂起
                bundles::Axi4Bundle wresp;
                if (req.is_write_request()) {
                    wresp.bid.write(static_cast<uint16_t>(req.awid.read()));
                    wresp.bresp.write(3u);  // AXI bresp = DECERR (11b; 勘误: 2 是 SLVERR)
                    axi.slave_resp(wresp);
                } else {
                    wresp.rid.write(static_cast<uint16_t>(req.arid.read()));
                    wresp.rresp.write(3u);  // AXI rresp = DECERR (11b)
                    wresp.rlast.write(1);
                    axi.slave_resp(wresp);
                }
                axi.slave_req_consume();
                return;
            }
            // ... 现有 cfg / BAR 写处理
        }
    }
}
```

### 1.3 测试设计

新增 `test/test_pcie_power_state_cfg_access.cc`:
- D3hot 下 cfg 写 PMCSR (offset 0x44) → 触发状态机回 D0 (INV-E)
- D3hot 下 cfg 读 Vendor ID → 仍成功 (PCIe spec 要求)
- D3hot 下 BAR 写 → 返回 DECERR (不静默)
- D3hot 下 BAR 读 → 返回 DECERR (不静默)
- D0 下 cfg + BAR 读写 → 全正常 (回归)

---

## §2 子任务 2: P2P DMA SUCCESS 路径

### 2.1 现状

`include/tlm/pcie/pcie_bypass_mux_p2p.hh` MVP 恒返 `BLOCKED_BY_ACS`。

### 2.2 决策 — PcieBypassMux 持有 ACS 策略表

```cpp
// include/tlm/pcie/pcie_bypass_mux_p2p.hh (扩展)
namespace tlm::pcie {

    class AcsPolicy {
    public:
        // 默认 strict (MVP 行为); per-BDF pair 可放行
        bool allow(uint32_t src_bdf, uint32_t dst_bdf) const noexcept;

        // 显式放行特定 BDF pair (admin API)
        void grant(uint32_t src_bdf, uint32_t dst_bdf) noexcept {
            grants_.insert(((uint64_t)src_bdf << 32) | dst_bdf);
        }

    private:
        std::unordered_set<uint64_t> grants_;  // default empty → strict
    };

    inline P2PResult p2p_dma_route(uint32_t src_bdf, uint32_t dst_bdf,
                                    uint64_t addr, std::size_t len,
                                    const AcsPolicy* policy = nullptr) noexcept {
        if (src_bdf == 0 || dst_bdf == 0) return P2PResult(P2PResult::Code::NO_ROUTE);
        if (src_bdf == dst_bdf) return P2PResult(P2PResult::Code::NO_ROUTE);
        // 勘误 (Oracle): null 逻辑反转修正 — nullptr → strict (BLOCKED),
        // 保留既有 test_p2p_dma.cc 调用方向后兼容; SUCCESS 仅当显式 policy 放行
        if (!policy || !policy->allow(src_bdf, dst_bdf)) {
            return P2PResult(P2PResult::Code::BLOCKED_BY_ACS);
        }
        return P2PResult(P2PResult::Code::SUCCESS);
    }

} // namespace tlm::pcie
```

### 2.3 测试设计

新增 `test/test_p2p_dma_success.cc`:
- 默认 AcsPolicy (空 grants): 跨 BDF 仍 BLOCKED_BY_ACS (向后兼容)
- 显式 grant(src, dst) 后: SUCCESS
- 不同 BDF pair grant 不影响其他 pair
- 1000-iter fuzz + AcsPolicy 多对 grant

---

## §3 子任务 3: ResizableBar → PcieEndpointIP 集成

### 3.1 现状

`ResizableBar` 是 header-only 独立状态机（`include/tlm/pcie/pcie_resizable_bar.hh`）。未连入 `PcieEndpointIP::bar_store_` 边界。

### 3.2 决策 — PcieEndpointIP 持有 6 个 ResizableBar

```cpp
// include/tlm/pcie/pcie_endpoint_ip.hh (扩展)
class PcieEndpointIP : public ChStreamModuleBase {
public:
    // ... 既有 API ...
    ResizableBar& resizable_bar(unsigned bar_idx) noexcept;  // 0..5

private:
    std::array<ResizableBar, 6> resizable_bars_;
    void on_bar_resize(unsigned bar_idx);  // enable() 触发, 校验 bar_store_ 越界
};
```

```cpp
// src/tlm/pcie/pcie_endpoint_ip.cc (扩展)
void PcieEndpointIP::on_bar_resize(unsigned bar_idx) {
    const uint32_t size = resizable_bars_[bar_idx].size_bytes();
    // INV-G: 校验 bar_store_ 中所有 key < size
    // 勘误 (Oracle): erase-during-iteration UB → erase-it 惯用法
    // 注意: bar_store_ key 是全局裸地址 (pcie_endpoint_ip.cc:385 mmio_write
    // 不含 BAR 号), 清 key >= size 会波及其他 BAR — MVP 接受此简化,
    // 已知限制写入 spec Out of Scope; 跨 BAR 隔离留待后续 PR
    for (auto it = bar_store_.begin(); it != bar_store_.end();) {
        if (it->first >= size) {
            config_warnings_.push_back(
                "BAR" + std::to_string(bar_idx) +
                " resized to " + std::to_string(size) +
                ", key " + std::to_string(it->first) + " out of bounds (cleared)");
            it = bar_store_.erase(it);
        } else {
            ++it;
        }
    }
}
```

**触发点 (勘误)**: `ResizableBar::enable()` 无回调机制。新增 wrapper:
```cpp
// PcieEndpointIP (扩展)
bool enable_resizable_bar(unsigned bar_idx) noexcept {
    if (!resizable_bars_[bar_idx].enable()) return false;
    on_bar_resize(bar_idx);   // enable 成功后校验越界 (INV-G)
    return true;
}
```
测试/调用方走 `enable_resizable_bar(idx)` 而非直调 `resizable_bar(idx).enable()`。

### 3.3 测试设计

新增 `test/test_resizable_bar_integration.cc`:
- `resizable_bar(0).reprogram_size(0x100000)` 后, 写 key=0x80000 成功
- `resizable_bar(0).enable()` + 改 size=0x100 → 写 key=0x80000 被警告清除
- 6 个 BAR slot 独立操作

---

## §4 子任务 4: PM Cap 控制域 JSON-driven

### 4.1 现状

`src/tlm/pcie/pcie_endpoint_ip.cc:28` hardcode `control=0x0013`。

### 4.2 决策 — JSON-driven

**勘误 (Oracle)**: PM Cap 安装保留在**构造器**（`pcie_endpoint_ip.cc:24-31`，`init()/do_reset()` 调 `pool_.init_all()` 会 wipe，移入 attach_composition 会重复 add_capability）；JSON 应用走**已有** `update_capability_control()`（`pcie_config_space_mvp.cc:66-75`）。JSON 键位置定为顶层 `pm_cap_control`（proposal 的 `phy_digital.pm_cap_control` 弃用），并加入 warn_unconsumed 顶层白名单：

```cpp
// attach_composition() (扩展, 构造器已安装 cap)
if (json.contains("pm_cap_control")) {
    const auto ctrl = json.value("pm_cap_control", static_cast<uint16_t>(0x0013));
    pool_.config_pool().config_of(0).update_capability_control(/*PM cap index=*/0, ctrl);
}
// warn_unconsumed 顶层 known list 加 "pm_cap_control"
```

JSON:
```json
{
  "pm_cap_control": 5251
}
```
(5251 = 0x1483; note: 0x1483 bit 布局按 PM Capabilities 寄存器语义, version bits[2:0]=3)

### 4.3 测试设计

`test/test_pm_cap_control_json.cc`:
- 默认 0x0013 (version 3 + D3hot)
- JSON pm_cap_control=0x1483 → 写入 control field
- 与 2027-02-09 JSON config 模式一致性

---

## §5 子任务 5: ACS Extended Cap 完整实现

### 5.1 现状

未实现 Extended Cap。

### 5.2 决策 — Extended Cap 协议

PCIe Extended Cap (per spec §7.7):
- Header at offset N (4-byte aligned):
  - bits [15:0]: Cap ID (0x000D for ACS)
  - bits [19:16]: Cap version (must be 1)
  - bits [31:20]: next Cap offset (0 = end)
- ACS Cap Reg (offset+4):
  - bit 0: V (source validation)
  - bit 1: B (bus lock)
  - bit 2: R (request redirect)
  - bit 3: E (Enhanced capability)
  - bit 4-7: reserved
- ACS Control Reg (offset+6): 对应 enable bits

### 5.3 实现

```cpp
// src/tlm/pcie/pcie_acs_extended_cap.cc (新文件)
namespace tlm::pcie {
    void install_acs_extended_cap(PcieConfigSpace& cfg, uint16_t offset = 0x100);
}
```

`install_acs_extended_cap(cfg)` — **勘误 (Oracle)**: header 编码按 PCIe Ext Cap dword 布局 bits[15:0]=ID, [19:16]=version, [31:20]=next:
- 写 offset+0: **`0x0001000D`** (id=0x000D, version=1, next=0；勘误: 原 0x000D0001 解码为 ID=0x0001/AER, 全错)
- 写 offset+4: 0x00000000 (ACS Cap Reg: no caps by default; dword 含 Control Reg 高 16-bit)
- **Control Reg 走 offset+4 dword RMW**（勘误: 原设计写 offset+6 为 16-bit 非 4 对齐，`PcieConfigSpace::write()` 对非对齐 offset 静默丢弃、`read()` 返回 0xFFFFFFFF，不可实现）
- offset 默认 **0x100**（勘误: Extended Cap 规范上必须 ≥0x100 扩展空间; 原 0xE0 在 legacy 256B 区）

`set_acs_bit_enabled(cfg, bit_idx, enable)`:
- RMW offset+4 dword: `val = cfg.read(offset+4); val = enable ? (val | (1u<<bit_idx)) : (val & ~(1u<<bit_idx)); cfg.write(offset+4, val);`
- ACS Control Reg 语义在 dword 高 16-bit (offset+6 = offset+4 dword 的高半)

### 5.4 测试设计

`test/test_acs_extended_cap.cc`:
- 默认安装: read offset=0x100 → `0x0001000D`
- 写 Control bit 0 (V): 读 offset+4 → 0x00010000 (高 16-bit bit0)
- ACS Extended Cap 与 PcieEndpointIP 集成（构造期调用 install, 需 config_size=4096 默认）

---

## §6 子任务 6: PMCSR PWS=1/2 保留值忽略

### 6.1 现状

`src/tlm/pcie/pcie_endpoint_ip.cc:28` `static_cast<PciePowerState>(new_pws)` — PWS=1/2 产生非法枚举值。

### 6.2 决策 — Mask

```cpp
// set_pmcsr_write_cb lambda 改为:
cfg_pf.set_pmcsr_write_cb([this](uint16_t new_pws) {
    if (new_pws != 0 && new_pws != 3) {
        // PCI PM spec: D1/D2 若 device 不支持, write 忽略
        return;
    }
    set_power_state(static_cast<PciePowerState>(new_pws));
});
```

### 6.3 测试设计

扩展现有 `test/test_pm_capability.cc` Scenario 3:
- 写 PWS=1 → `power_state_` 保持 D0 (无变化)
- 写 PWS=2 → 同上
- 写 PWS=3 → D3hot (现有逻辑)

---

## §7 实施顺序与依赖

| 步骤 | 依赖 | 工作量 |
|---|---|---|
| §1 INV-A 收窄 | 无 | 0.5d |
| §6 PMCSR mask | 无 | <0.1d |
| §4 PM Cap JSON | §6 | 0.1d |
| §5 ACS Extended Cap | 无 | 0.2d |
| §3 ResizableBar 集成 | §1 收窄不冲突 | 0.3d |
| §2 P2P SUCCESS | 无 | 0.3d |

串行执行: §1 → §6 → §4 → §5 → §3 → §2
总: 1.5d + Oracle 复评 0.5d

## §8 测试覆盖目标

| 子任务 | 新增 cases | assertions |
|---|---|---|
| §1 | 5 | 30 |
| §2 | 4 | 2000+ (含 fuzz) |
| §3 | 3 | 15 |
| §4 | 2 | 8 |
| §5 | 3 | 12 |
| §6 | 2 | 6 |
| **总计** | **19** | **~2071** |

全 [pcie] 回归: 320+ cases / 19000+ assertions (零回归)

## §9 风险评估

| 风险 | 等级 | 缓解 |
|---|---|---|
| INV-A 收窄后 cfg 路径可能有未测试用例 | 中 | 新增 cfg 路径测试 + 全 [pcie] 回归 |
| ACS Extended Cap 写入影响既有 capability 链表 | 中 | 用 offset=0x100 (扩展空间起始, 勘误: 0xE0 在 legacy 256B 区), 不与现有 0x01 冲突; 需 config_size=4096 |
| ResizableBar 集成触发 bar_store_ 清空可能丢数据 | 高 | 仅在 enable() 时清, 重 size 前有 disable 缓冲 |
| PMCSR mask 改变触发回调语义 | 低 | sentinel 0xFFFFu + last_pmcsr_pws_ 保护幂等 |
| P2P SUCCESS 路径可能误判 ACS | 中 | 默认 strict 保持向后兼容, 需显式 grant |
