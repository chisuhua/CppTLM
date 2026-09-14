# Design: cpptlm-stage-1-4-2-1 — 电源管理 + P2P/Resizable BAR

> **Status**: Proposed v1.0（2026-09-10）
> **Owner**: Sisyphus (serial implementation after 2027-02-09 archive)
> **Predecessor**: `2027-02-09-cpptlm-pcie-endpoint-ip-json-config` (archived ✅)

---

## §0 边界与交叉引用

### 0.1 与已归档 change 的接口边界

| 已交付 API | 来源 change | 本 change 使用方式 |
|---|---|---|
| `PcieConfigSpace::set_config_size(sz)` | 2027-02-09 archive | **不修改**；不动 cfg 边界语义 |
| `PcieConfigSpacePerVf::set_config_size_all(sz)` | 2027-02-09 archive | **不修改**；BAR size 与 config_size 是正交概念 |
| `config_warnings_` 警告收集 | 2027-02-09 archive | 本 change 实施时**新增 bar_sizes / vf_bar*_size 消费**，须同步更新 Scenario 5 测试 |

### 0.2 不变量（强约束，写入代码注释）

| ID | 不变量 | 落地位置 |
|----|--------|----------|
| INV-A | 电源状态转换 `set_power_state(D3hot)` 必须同步 MMIO gate（PMCSR 写拦截），否则 driver 写仍可达 device | §1 tick gating |
| INV-B | ASPM L0s/L1 退出延迟 (PCIe spec: L0s 7-8us, L1 1-4ms) 不可硬编码为 0 cycle | §1.2 PHY idle-timer |
| INV-C | Resizable BAR Cap 必须保持向后兼容（BAR size 调整前先 disable + reprogram) | §3 sequence |
| INV-D | ACS 拒绝 P2P 返回 -EPERM 不允许 silent fallback（误路由数据会污染） | §2 P2P routing |

### 0.3 文件清单 (现况与缺口)

| 文件 | 状态 | 备注 |
|------|------|------|
| `src/tlm/pcie/pcie_link_layer_tlm.cc` | ✓ 512 行 | §0 LTSSM 修改目标 |
| `src/tlm/pcie/pcie_bypass_mux.cc` | ✓ 存在 | §2 P2P 路由修改 |
| `src/tlm/pcie/pcie_ari_router_tlm.cc` | ✗ 缺失 | 仅 header (AriRouter 全 inline, 4 行 set_ari_enabled) — 不阻断 |
| `src/tlm/pcie/pcie_config_space_per_vf_tlm.cc` | ✗ 缺失 | 仅 header (全 inline) — 不阻断 |
| `include/tlm/pcie/pcie_ari_router_tlm.hh` | ✓ 72 行 | ACS + ARI |
| `include/tlm/pcie/pcie_config_space_per_vf_tlm.hh` | ✓ 88 行 | 17 slot container |
| `src/tlm/gpu/dgpu_soc.cc` | ✓ 存在 | §1 D0/D3 状态机集成 |

**关键结论**: 所有声明已存在；本 change 主要是**实现 .cc 文件 + 集成到现有 PcieEndpointIP**，不引入新 header。

---

## §1 阶段 1.4 电源管理 + ASPM (0.5 周)

### 1.1 PM Capability (id=0x01) 寄存器布局

PCIe PM Cap (per spec §7.5)：

| Offset | Field | Type | 行为 |
|--------|-------|------|------|
| 0x00 | PM Cap ID (0x01) | R | |
| 0x01 | Next Cap Ptr | R | 指向下一个 capability |
| 0x02 | PM Capabilities | R | bit 9 = D3hot 支持 |
| 0x04 | PMCSR | RW | bits 1:0 = 当前 power state (D0=00, D3hot=11) |
| 0x06 | PMCSR_BSE | R | 桥支持事件 |

### 1.2 PM 写拦截机制

```cpp
// include/tlm/gpu/pcie_config_space_mvp.hh (扩展)
class PcieConfigSpace {
public:
    // 既有 capability 写路径 → 触发 PM CSR 拦截
    bool update_capability_control(std::size_t index, uint16_t control);

private:
    // PM 专用: capability 写回调
    std::function<void(uint16_t new_pmcsr)> pmcsr_write_cb_;
public:
    void set_pmcsr_write_cb(std::function<void(uint16_t)> cb) {
        pmcsr_write_cb_ = std::move(cb);
    }
};
```

### 1.3 PcieEndpointIP 电源状态机

```cpp
// src/tlm/pcie/pcie_endpoint_ip.cc (新增)
enum class PciePowerState : uint8_t { D0 = 0, D3hot = 3 };

class PcieEndpointIP {
public:
    void set_power_state(PciePowerState state);
    PciePowerState power_state() const { return power_state_; }
    void enable_aspm(AspmLevel level);  // L0s / L1
private:
    PciePowerState power_state_ = PciePowerState::D0;
    AspmLevel aspm_level_ = AspmLevel::Off;
    bool mmio_gated_ = false;
    void tick_mmio_gate();   // tick() 调用, INV-A
    void tick_aspm_timer();  // tick() 调用, INV-B
};
```

**MMIO gate 行为** (INV-A):
- D3hot → tick_mmio_gate() 拒绝所有 cfg/BAR writes (return -EPERM)
- D0 → tick_mmio_gate() 放行

**ASPM 退出延迟** (INV-B):
- L0s exit latency: 4 cycles (per spec 范围 1-7us @ Gen5)
- L1 exit latency: 32 cycles (per spec 1-4ms 简化)

### 1.4 LTSSM 残余（Oracle R-E 修订并入本 change）

`pcie_link_layer_tlm.cc` 现有 LTSSM 11 态；本 change 仅补 ASPM 子状态 L0s/L1。

| 子状态 | 转换条件 | 退出 |
|--------|---------|------|
| L0s | idle 100ns 计时到 | 任意 TLP/DLLP 触发退出 |
| L1 | idle 4ms 计时到 | 触发 PME# 或 beacon |

---

## §2 阶段 2.1 P2P + Resizable BAR (1 周)

### 2.1 P2P DMA 路由

```cpp
// include/tlm/pcie/pcie_bypass_mux.hh (新增)
class PcieBypassMux {
public:
    enum class P2PResult {
        OK = 0,
        EPERM = -1,  // ACS blocked
        NOROUTE = -2  // dst_bdf 不存在
    };
    P2PResult p2p_dma_route(uint32_t src_bdf, uint32_t dst_bdf,
                            uint64_t addr, size_t len);
private:
    // INV-D: silent fallback 不允许
    bool acs_check(uint32_t src_bdf, uint32_t dst_bdf) const;
};
```

### 2.2 ACS Capability

ACS Extended Capability (id=0x000D, per PCIe spec §7.7）：

| Offset | Field | 行为 |
|--------|-------|------|
| 0x04 | ACS Cap | R: bit 0=V(源验证), 1=B(总线锁), 2=R(请求重定向), 3=...(8 个) |
| 0x06 | ACS Control | RW: 每位 enable/disable 对应 Cap |

### 2.3 Resizable BAR Capability (id=0x0015)

| Offset | Field | 行为 |
|--------|-------|------|
| 0x04 | Cap Reg | bit 0 = BAR index (0-5) |
| 0x08 | Control Reg | bit 0-2 = BAR size (1MB/2MB/4MB.../512GB) |
| 0x0C | Status Reg | bit 0 = size negotiation 完成 |

**调整序列** (INV-C):
1. driver 写 `Control = 0` (disable)
2. driver 写 `Control = new_size_field` (reprogram)
3. 内核态确认 → 写回 `Control = new_size_field | 0x1` (enable)
4. device 状态更新在下一 tick 生效

---

## §3 JSON 配置消费（与已归档 change 协同）

### 3.1 本 change 解锁的消费项

| JSON 键 | 当前状态 | 本 change 消费方式 |
|---------|----------|-------------------|
| `transaction_layer.bar_sizes` | 2027-02-09 标记为 warning | **消费**：`bar_window_[6]` 数组从 config space BAR regs 解析 + JSON 可选覆盖 |
| `sr_iov.vf_bar0_size` / `vf_bar1_size` | 2027-02-09 标记为 warning | **消费**：VF BAR size 写入对应 slot 的 config space BAR regs |
| `sr_iov.num_vfs` | 2027-02-09 标记为 warning | **不消费**：runtime NUM_PORTS 重构不在本 change 范围 |

### 3.2 警告白名单更新

`attach_composition()` 中白名单需更新：
```cpp
// "transaction_layer" 子组从 {"config_size", "msix_num_vectors"}
// 改为 {"config_size", "msix_num_vectors", "bar_sizes"}
// "sr_iov" 子组从 {"ari_capable", "vf_msix_vectors"}
// 改为 {"ari_capable", "vf_msix_vectors", "vf_bar0_size", "vf_bar1_size"}
```

**强制同步**: `test_pcie_endpoint_ip_json_config.cc` Scenario 5 子组断言须同步修改（Oracle F2）。

---

## §4 测试设计

### 4.1 新测试文件

| 文件 | 覆盖 |
|------|------|
| `test/test_pm_capability.cc` | PM Cap 寄存器读写 + PMCSR 写拦截 |
| `test/test_power_state_transition.cc` | D0 ↔ D3hot 状态机 + MMIO gating |
| `test/test_aspm.cc` | L0s/L1 idle-timer + 退出延迟 |
| `test/test_p2p_dma.cc` | P2P 路由 + ACS 拒绝路径 |
| `test/test_acs_capability.cc` | ACS Cap R/W + 8 个 control bit 独立 enable/disable |
| `test/test_resizable_bar.cc` | Resizable BAR Cap + disable/reprogram/enable 序列 |

### 4.2 同步更新

| 文件 | 变更 |
|------|------|
| `test/test_pcie_endpoint_ip_json_config.cc` | Scenario 5 移除 `bar_sizes`/`vf_bar0_size` 警告断言，改负断言 |
| `examples/dgpu_soc_with_pcie_ip.json` | 可选补 `transaction_layer.bar_sizes` 与 `sr_iov.vf_bar*_size` (示范消费) |

---

## §5 风险评估

| 风险 | 等级 | 缓解 |
|------|------|------|
| PMCSR 写拦截误拦截其它 cap 写 | 中 | capability_id 验证 (id=0x01 才拦截) |
| ASPM 退出延迟硬编码 vs spec 范围 | 中 | 用 PCIe spec 范围中值，可配置化作为未来 change |
| Resizable BAR 大小不匹配 MMIO 映射 | 高 | INV-C 序列 + tick 同步验证 |
| P2P 路由触达未知 BDF 静默丢失 | 高 | INV-D: 强制返回 NOROUTE/-EPERM，never silent |
| 与 2027-02-09 已交付的 `config_warnings_` 接口协同 | 低 | 入口 clear + 白名单更新 |

---

## §6 实施顺序 (串行 TDD)

| 阶段 | 任务 | 依赖 |
|------|------|------|
| 1 | §0 LTSSM 残余 (L0s/L1 子状态) | 无 |
| 2 | §1.1 PM Capability 寄存器布局 | §0 |
| 3 | §1.3 Power state 状态机 + INV-A MMIO gating | §1 |
| 4 | §1.4 ASPM L0s/L1 idle-timer + INV-B 退出延迟 | §1 |
| 5 | §3.2 JSON 白名单同步 + Scenario 5 测试更新 | §1-4 |
| 6 | §2.2 ACS Capability + INV-D silent-fallback-forbidden | §1 |
| 7 | §2.1 P2P DMA 路由 | §6 |
| 8 | §2.3 Resizable BAR + INV-C disable/reprogram/enable 序列 | §6 |
| 9 | Oracle 复评 + docs sync | §1-8 |
