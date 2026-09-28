# ADR-DGPU-07: Minimal SoC → 完整 GPU 演进 Seam 设计 (D3-D5 接口零变更)

> **状态**: 📋 提案 (基于 D-AXI design.md §X 演进路线图, v1.4 已就位 + v1.6 锁定)
> **日期**: 2026-09-26
> **关联架构文档**: [../designs/dgpu-soc/architecture.md §3.5 vram_segments_ 兼容策略 + §X 演进路线图](../designs/dgpu-soc/architecture.md), [../designs/dgpu-driver/architecture.md §5 演进路径](../designs/dgpu-driver/architecture.md)
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前, design.md §X + §Y)
> **配套 ADR**: [ADR-DGPU-05](ADR-DGPU-05-vram-storage-ownership.md) (vram_storage_ 所有权), [ADR-DGPU-09](ADR-DGPU-09-driver-visible-minimal-soc-scope.md) (Driver-Visible 范围)

---

## 1. 背景

### 1.1 问题

Minimal DGpu SoC（v1.0）是"第一类 driver-visible SoC MVP"，仅覆盖最小能力（H2D/D2H/D2D + GMMU 一级页表）。但 GPU 完整能力（HBM timing 多通道、Compute Pipeline、SR-IOV VF）**不在 minimal 范围**。

**演进挑战**：
- D3 真 HBM timing → 现有 `PcieMemoryDevice::handle_slave_port ↔ backing_ptr_` 之间如何插入 VramControllerTLM？
- D4 MemoryCluster 多通道 HBM → `bind_memory_backings()` 注入点如何扩展？
- D5 SR-IOV VF → PcieEndpointIP 17 端口契约如何扩展？
- driver 源码必须**零迁移**（per ADR-DGPU-08 ABI 冻结）

### 1.2 现状约束

- minimal_v1 接口必须保留（per ADR-DGPU-08 ABI 冻结 + ADR-DGPU-09 Driver-Visible 范围）
- 5 消费者共享 `vram_storage_`（per ADR-DGPU-05）
- `handle_slave_port` ↔ `backing_ptr_` 之间是**唯一**的 backing 路径（per F4 PcieMemoryDevice 退化）
- driver 视角承诺：D3/D4/D5 演进过程 driver 源码零修改（per spec "Driver Code Compatibility"）

### 1.3 5 大能力扩展路径

per D-AXI design.md §X.3：

| 路径 | 触发条件 | 插入点 | 接口影响 |
|------|---------|--------|---------|
| **D3 VramController** | 引入 HBM timing 模型 | `pcie_memory_device.cc::handle_slave_port()` ↔ `backing_ptr_` | **零 API 变更** |
| **D4 MemoryCluster** | 多通道 HBM 控制器 | `dgpu_board_shell.cc::bind_memory_backings()` | **零 API 变更** |
| **D5 SR-IOV VF** | 多 VF 端口扩展 | PcieEndpointIP 17 端口契约 | driver 视角零迁移（per spec） |
| **D1 Display FB 收编** | D1 32MB FB 同构归 board 持有 | `dgpu_board_shell.cc` 注入点（与 pcie_memory 并列） | D3 立项 |
| **GPU 计算 (StreamingMultiprocessor)** | 引入 Compute Pipeline | 新增 `tlm/gpu/sm/` 子模块 | 与 v1.4 解耦 |

---

## 2. 决策

### 2.1 Seam 设计原则（5 条）

per D-AXI design.md §X.2：

1. **接口零变更原则**：演进路径**不修改**现有 ABI / 端口契约 / 模块接口
2. **注入点固定原则**：5 消费者注入点（`bind_memory_backings`）位置固定，演进仅替换 backing 类型
3. **Seam 可插拔原则**：`handle_slave_port ↔ backing_ptr_` 之间可插入任意 controller（VramController / MemoryCluster）
4. **driver 视角承诺**：minimal → D3/D4/D5 演进过程 driver 源码**零修改**（per spec "Driver Code Compatibility"）
5. **可逆性原则**：每个 seam 插入不应破坏 minimal 行为，可回滚至 minimal_v1 基线

### 2.2 D3 VramController Seam 位置

**Where**: `pcie_memory_device.cc::handle_slave_port()` ↔ `backing_ptr_` 之间

```cpp
// minimal_v1 现状:
int PcieMemoryDevice::handle_slave_port(AxiMemBundle& req) {
    if (!backing_ptr_) return -ENODEV;       // B12
    if (req.address + req.size > backing_size_) return -EIO;  // B8
    std::memcpy(backing_ptr_ + req.address, &req.data, req.size);  // 直读直写
    return 0;
}

// D3 演进: VramControllerTLM 插入 (HBM timing + 真实延迟)
int PcieMemoryDevice::handle_slave_port(AxiMemBundle& req) {
    if (!backing_ptr_) return -ENODEV;
    if (!vram_controller_) {  // D3 seam: 未配置 controller 时降级为 minimal 行为
        return handle_slave_port_minimal(req);
    }
    return vram_controller_->access(req);  // 委托给 controller
}
```

**接口影响**：
- ❌ 不修改 `handle_slave_port` 签名
- ❌ 不修改 `AxiMemBundle` wire-format
- ✅ 新增 `set_vram_controller(VramControllerTLM*)` 注入方法（per F1 GMMU bound 归位范式）

### 2.3 D4 MemoryCluster Seam 位置

**Where**: `dgpu_board_shell.cc::bind_memory_backings()` 注入点扩展

```cpp
// minimal_v1 现状:
void DGpuBoard::bind_memory_backings() {
    if (!soc_) return;
    if (auto* mem = dynamic_cast<PcieMemoryDevice*>(soc_->getInternalInstance("pcie_memory"))) {
        mem->set_backing_store(vram_storage_.get(), vram_size_);
    }
    // ... 4 其他消费者
}

// D4 演进: MemoryCluster 多通道 HBM 控制器注入
void DGpuBoard::bind_memory_backings() {
    if (memory_cluster_) {  // D4 seam: 配置了 MemoryCluster 时走 cluster
        // 5 消费者经 cluster 访问多通道 HBM
        memory_cluster_->set_backing_store(vram_storage_.get(), vram_size_);
        mem->set_vram_controller(memory_cluster_.get());  // D3 seam
    } else {
        // minimal_v1 直读直写
        mem->set_backing_store(vram_storage_.get(), vram_size_);
    }
}
```

**接口影响**：
- ❌ 不修改 `set_backing_store` 签名
- ✅ 新增 `set_memory_cluster(MemoryClusterTLM*)` 注入方法

### 2.4 D5 SR-IOV VF 扩展

**Where**: PcieEndpointIP 17 端口契约（per spec "D5 SR-IOV VF 端口扩展不改 EP 4 端口契约"）

- ❌ 不修改 EP 4 端口契约（board-level PCIe 端口）
- ✅ 通过 PcieSriovVfPoolTLM 子模块扩展（已有，per Phase 4）
- driver 视角零迁移（per spec "driver compiled against minimal_v1 works on full GPU with zero source changes"）

### 2.5 D3-D5 Seams 表（per design.md §X.5）

| D3-D5 需求 | 插入点 | 接口影响 | 状态 |
|-----------|--------|---------|------|
| 引入 VramControllerTLM (HBM timing) | `pcie_memory_device.cc::handle_slave_port()` ↔ `backing_ptr_` | **零 API 变更**（controller 替代直接 memcpy） | v1.4 已就位, v1.6 锁定 |
| 引入 MemoryClusterTLM (多通道 HBM) | `dgpu_board_shell.cc::bind_memory_backings()` 注入点 | **零 API 变更**（controller 变第 6 指针消费者） | v1.4 已就位 |
| D1 Display FB 收编 | `dgpu_board_shell.cc` 注入点（与 pcie_memory 并列） | D3 立项 | v1.4 延期, D3 收编 |
| GPU 计算 (StreamingMultiprocessor) | 新增 `tlm/gpu/sm/` 子模块 | 与 v1.4 解耦 | D3+ 立项 |
| 多 PcieMemoryDevice 实例 | minimal_v1 单实例；多实例需 bound/allocator 重审 | D3+ | 延期 |

### 2.6 重构触发条件（per design.md §X.6）

**何时触发 seam 插入**：

| 触发条件 | seam | 评估项 |
|---------|------|--------|
| 引入 HBM timing 模型 (3D-stacked DRAM 延迟) | D3 | `vram_controller_` 注入点位置 |
| MemoryTLM capacity_gb ≠ bar_sizes[2] (per F12) | D4 | `memory_cluster_` 注入点位置 |
| SR-IOV VF 数 > 1 | D5 | `sriov_vf_pool_` 扩展 |
| Display IO device D1 32MB FB 同构归 board 持有 | D3 | `display_backing_` 注入点（与 pcie_memory 并列） |
| Compute Pipeline (StreamingMultiprocessor) | D3+ | 新增 `tlm/gpu/sm/` |

---

## 3. 关键不变性 (Invariants)

### Inv-1: handle_slave_port ↔ backing_ptr_ seam

**Where**: `pcie_memory_device.cc::handle_slave_port()`

```cpp
// seam 不变量: backing_ptr_ 为 nullptr 时降级为 minimal 行为
if (!backing_ptr_) return -ENODEV;
```

**测试**: `assert(pcie_memory->backing_ptr_ == nullptr)` 时返 -ENODEV

### Inv-2: D3 插入不改邻居

**Where**: `set_vram_controller` 注入方法

```cpp
void set_vram_controller(VramControllerTLM* ctrl) {
    vram_controller_ = ctrl;
    // 不修改 handle_slave_port 签名
    // 不修改 backing_ptr_ / backing_size_ 语义
}
```

**测试**: 注入 controller 与不注入 controller 行为对偶（per F1 GMMU bound 归位范式）

### Inv-3: D4 替换不破 consumer

**Where**: `bind_memory_backings` 注入扩展

```cpp
// 5 消费者（backdoor / BAR1 / MemoryTLM / sdma / gmmu / pcie_memory）
// 经 `vram_read/vram_write` 抽象访问 (per H7 提前改造)
```

**测试**: 5 消费者注入点位置不变，controller 变第 6 指针消费者

### Inv-4: D5 VF 扩展不改 EP 4 端口契约

**Where**: PcieEndpointIP 17 端口契约

```cpp
// EP 4 端口契约冻结: req_in / resp_out / awaddr / araddr
// D5 扩展通过 PcieSriovVfPoolTLM 子模块实现
```

**测试**: EP 4 端口签名 hash 不变

---

## 4. 实施步骤

D-AXI v1.4 已就位 5 个 seam 位置（per design.md §X.5），v1.6 锁定 D3-D5 Seams 表。具体实施分 D3 / D4 / D5 三个 phase 启动：

| Phase | seam | 触发条件 | 预计工时 |
|-------|------|---------|---------|
| **D3 VramController** | handle_slave_port ↔ backing_ptr_ | HBM timing 模型立项 | 4-6 周 |
| **D4 MemoryCluster** | bind_memory_backings 注入扩展 | MemoryTLM capacity_gb ≠ bar_sizes[2] | 3-4 周 |
| **D5 SR-IOV VF** | PcieSriovVfPoolTLM 子模块 | SR-IOV VF 数 > 1 | 6-8 周 |

**D3 立项前需先完成**：D-AXI v1.4 实施 + Oracle 评审 + 当前 seam 位置验证

---

## 5. 测试策略

### 5.1 Seam 位置单元测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_seam_handle_slave_port_no_controller | 未配置 controller 时降级为 minimal 行为 | `[seam][d3][minimal]` |
| test_seam_bind_memory_backings_5_consumers | 5 消费者均注入正确指针 | `[seam][d4][binding]` |
| test_seam_d5_vf_extension_no_ep_change | D5 VF 扩展不改 EP 4 端口契约 | `[seam][d5][ep-freeze]` |

### 5.2 演进兼容性测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_d3_insert_vram_controller | 注入 VramController 后 minimal_v1 行为保持 | `[seam][d3][controller]` |
| test_d4_insert_memory_cluster | 注入 MemoryCluster 后 5 消费者接口不变 | `[seam][d4][cluster]` |
| test_d5_add_vf_no_driver_change | D5 VF 扩展后 driver 源码零修改 | `[seam][d5][driver-compat]` |

### 5.3 driver 兼容性回归

per spec Requirement "Driver Code Compatibility SHALL be preserved across minimal_v1 → full GPU":

```cpp
TEST_CASE("driver compiled against minimal_v1 works on full GPU", "[driver-compat][minimal_v1→full]") {
    // driver binary linking cpptlm_emulator library
    // runs against minimal_v1 config then D5 full GPU config (same JSON schema evolution)
    REQUIRE(all_abi_calls_compatible);
    REQUIRE(driver_code_revision_field_may_change);  // 用于区分代际, 非行为变更
}
```

---

## 6. 兼容性

### 6.1 ABI 兼容性

- 演进过程**不修改** 23 ABI（per ADR-DGPU-08）
- D3-D5 通过现有 ABI 行为扩展 + JSON 配置扩展实现
- driver 源码**零修改**（per spec "Driver Code Compatibility"）

### 6.2 JSON 配置兼容性

- minimal_v1 → D3/D4/D5 JSON schema 兼容（per spec Requirement "minimal_v1 JSON 配置扩展"）
- `bar_sizes` 数组可扩展支持更多 BAR

### 6.3 测试兼容性

- minimal_v1 baseline 41 assertions + ABI smoke 28 assertions 全绿保持
- D3/D4/D5 演进通过新增 seam 单元测试覆盖

---

## 7. 风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| Seam 位置错位导致无法插入 | 🔴 H | D-AXI v1.4 已就位 + v1.6 锁定；实施期强制 seam 位置单元测试 |
| Controller 接口与 backing_ptr_ 不兼容 | 🟡 M | `vram_controller_->access(req)` 抽象层 |
| D3/D4/D5 引入新 ABI 函数 | 🔴 H | per ADR-DGPU-08 0 新增约束；扩展通过现有 ABI 行为 |
| driver 行为漂移（D3 HBM timing 改变可见行为） | 🟡 M | spec Requirement 显式声明 "driver 视角承诺不变" |
| 5 消费者接口变更 | 🔴 H | Inv-3 seam 设计原则保证接口零变更 |

### Oracle/Metis 评审重点

- **D-AXI v1.6 锁定**: F12 MemoryTLM 移除决策 + D3-D5 Seams 表
- **D-AXI v1.7 H7**: inject_q_ 资源上限 + design.md §8 陈旧引用清理
- **D-AXI v1.8 §X**: 演进路线图设计（5 大能力扩展路径 + 重构触发条件）

---

## 8. 参考

- D-AXI OpenSpec change: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8)
- D-AXI design.md §X 演进路线图
- D-AXI design.md §X.5 D3-D5 Seams 表
- D-AXI design.md §X.6 重构触发条件
- D-AXI spec.md Requirement "SoC 架构 SHALL 支持 minimal → full GPU 无重大重构"
- 实施笔记: [../../pcie/driver-visible-minimal-soc.md §7 D3 演进 seam](../../pcie/driver-visible-minimal-soc.md)
- 配套 ADR: ADR-DGPU-05 (vram_storage_ 所有权), ADR-DGPU-08 (ABI 冻结), ADR-DGPU-09 (Driver-Visible 范围)

---

## Status Update

_(本节将在 D-AXI v1.4 实施通过 Oracle 评审后追加，记录签发时间与实施 commit hashes)_
