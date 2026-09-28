# ADR-DGPU-09: Driver-Visible Minimal SoC 范围定义 (In Scope / Out of Scope)

> **状态**: 📋 提案 (基于 D-AXI proposal.md §1 + design.md §13 不在范围, v1.2 P1 修订)
> **日期**: 2026-09-26
> **关联架构文档**: [../designs/dgpu-driver/architecture.md](../designs/dgpu-driver/architecture.md), [../designs/dgpu-soc/architecture.md §X 演进路线图](../designs/dgpu-soc/architecture.md)
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前, proposal.md §1 + design.md §13)
> **配套 ADR**: [ADR-DGPU-05](ADR-DGPU-05-vram-storage-ownership.md) (B7 单一 VRAM 所有权), [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) (B2 wire-format 边界), [ADR-DGPU-07](ADR-DGPU-07-minimal-soc-evolution-seam.md) (D3-D5 演进 seam), [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) (ABI 冻结)

---

## 1. 背景

### 1.1 问题

D-AXI（Driver-Visible AXI Minimal SoC）是 CppTLM 项目**第一类 driver-visible SoC**。在 v1.0 → v1.8 5 轮评审中，**范围边界**反复出现：

| 评审轮 | 范围相关问题 | 修正编号 |
|--------|------------|---------|
| v1.2 Oracle 二次审查 | 多级页表 / 混合端口模板混入 | N1-N12 must-fix |
| v1.3 用户质疑 | D3 GMMU PoC vs minimal_v1 边界 | B7 单一 VRAM 所有权 |
| v1.6 Oracle 三轮 | MemoryTLM capacity 矛盾 | F2 / F12 MemoryTLM 移除决策 |
| v1.7 Oracle 第四轮 | D2D false-success, wire-format 矛盾 | H1 / H4 |

**关键风险**：范围扩张导致 minimal_v1 实施失控，与 ADR-DGPU-08 ABI 冻结冲突。

### 1.2 历史背景

- **proposal.md §1 范围**: Driver-Visible Minimal SoC（第一类 driver-visible SoC 拓扑）
- **proposal.md §7 不在范围**: GPU 计算/D3、GMMU 多级页表、混合端口模板、`ch_uint<512>` 限制统一化、ArchForge 跨仓
- **design.md §13 不在范围**: 续（5 章节）
- **v1.4 P0-2 延期声明**: 3 项遗留议题（D1 Display FB / doorbell / MemoryTLM capacity_gb）

### 1.3 现状约束

- 4 BAR + 15 ABI + 0 新增（per ADR-DGPU-08）
- 6 模块拓扑（DGpuBoard + PcieEndpointIP + PcieMemoryDevice + GMMU + SDMA + PcieConfigSpace）
- Coherence 边界归 UsrLinuxEmu 端（per spec Requirement "Coherence 域边界"）

---

## 2. 决策

### 2.1 In Scope（纳入范围）

#### 2.1.1 4 BAR 配置（per spec "PcieConfigSpace BAR 寄存器从 bar_sizes 生成"）

| BAR | 大小 | 角色 | 访问路径 |
|-----|------|------|---------|
| **BAR0** | 4KB | MMIO 控制寄存器（vendor_id / device_id / command / status / BAR regs） | `mmio_read/write(0, ...)` |
| **BAR1** | 16MB | FrameBuffer 窗口（host backdoor fast-path） | `mmio_read/write(1, ...)` |
| **BAR2** | 8GB | VRAM aperture（完整 VRAM 访问，64-bit 双 dword） | `mmio_read/write(2, ...)` |
| **BAR3+** | 保留 | PCIe spec 最多 6 BAR | — |

`bar_sizes` 数组为 3 元素（per spec Requirement "PcieConfigSpace BAR 寄存器从 bar_sizes 生成"）。

#### 2.1.2 15 ABI 函数（per spec "15 ABI Functions Unchanged"）

按功能分组：

| 分组 | 函数 | 说明 |
|------|------|------|
| **Lifecycle** (4) | `cpptlm_emulator_create` / `destroy` / `load_soc_config` / `init` | 设备生命周期管理 |
| **Config space** (2) | `config_read` / `config_write` | PCIe Config Space 访问 |
| **MMIO** (4) | `mmio_read` / `mmio_write`（BAR 索引参数化）+ 2 reserved | BAR0/1/2 MMIO 访问 |
| **Backdoor** (2) | `backdoor_read` / `backdoor_write` | 特权路径直读直写 VRAM |
| **DMA + 中断** (3) | `trigger_dma_translate_async` / `set_irq_callback` / `set_error_callback` | DMA 完成中断 + 错误通知 |

**约束**：**0 新增**（per ADR-DGPU-08 + spec Requirement "15 ABI Functions Unchanged"）

#### 2.1.3 6 模块拓扑（per design.md §1）

```
DGpuBoard (shell, 23 ABI 入口)
└── DGpuSoc (SimModule 容器)
    ├── PcieEndpointIP          (17-port, BAR0 MMIO + BAR1 16MB + BAR2 8GB)
    ├── PcieMemoryDevice        (chip-internal slave, 持有 backing_ptr_, bound = vram_size_)
    ├── GMMU                    (一级页表翻译, 无 TLB, AXI chip-internal master)
    ├── SDMA                    (5-port, 切型范围限定 per v1.3 B2, ring mode + fence)
    └── PcieConfigSpace         (BAR 寄存器从 bar_sizes 生成 per v1.3 N6)
```

#### 2.1.4 6 模块 + 5 消费者共享 vram_storage_（per ADR-DGPU-05）

5 消费者：
1. DGpuBoard::backdoor_read/write
2. BAR1 fast-path
3. BAR2 via PcieMemoryDevice
4. MemoryTLM backing
5. SDMA-GMMU legacy ptr

#### 2.1.5 3 类典型操作（driver 闭环）

| 类型 | 路径 | 测试覆盖 |
|------|------|---------|
| **H2D** (Host → Device) | BAR1 ring doorbell → SDMA → PcieMemoryDevice | `[sdma][h2d]` |
| **D2H** (Device → Host) | BAR2 mmio_read 直读 VRAM | `[minimal_dgpu_soc][d2h]` |
| **D2D** (Device → Device) | SDMA 内部 mem_out 两拍 (per v1.7 H4) | `[minimal_dgpu_soc][d2d]` |

#### 2.1.6 Coherence 边界声明（per spec "Coherence 域边界"）

coherent/non-coherent 系统内存语义 **SHALL NOT** 在 CppTLM 建模；由 UsrLinuxEmu 端通过模拟 Linux 内部分配器 API（CMA / vmalloc / kmalloc）实现。

### 2.2 Out of Scope（不在范围）

#### 2.2.1 v1.0 不在范围（per proposal.md §7）

| 项 | 不在范围理由 | 后续阶段 |
|----|------------|---------|
| **GPU 计算 / D3 (Compute Pipeline)** | minimal_v1 仅最小存储路径 | D3 立项 |
| **GMMU 多级页表 / TLB** | minimal_v1 仅一级页表 | v2.1 GMMU 扩展 |
| **混合端口模板 (AxiMemBundle + PcieTlpBundle)** | spec 严格分离（per ADR-DGPU-06） | 不扩展 |
| **`ch_uint<512>` 限制统一化** | 已知限制（per Phase 5 M1 文档化） | 不在本 ADR 范围 |
| **ArchForge 跨仓** | CppTLM 仓不与外部 ArchForge 仓同步 | ArchForge 独立维护 |

#### 2.2.2 v1.4 延期声明（per tasks.md P0-2）

| 议题 | 延期理由 | D3 收编计划 |
|------|---------|------------|
| **D1 PcieDisplayDevice 32MB FB 同构** | `pcie_display_device.hh` 是冻结面；minimal_v1 `display_routing_enabled=false` 不触发 | D3 按 Option D 模式（`set_backing_store(ptr, size)`）将 D1 FB 也归 board 持有 |
| **BAR1 doorbell offset 0x10010000 > 16MB 实窗** | 测试专用合成偏移；不依赖 BAR1 实窗大小；改常量弄断既有 `[sdma][doorbell]` 测试 | 不修 |
| **MemoryTLM `capacity_gb=1` vs `vram_size_=8GB` 交互** | minimal_v1 保持 `memory.params.capacity_gb=1`（CPU 侧 cache 路径合理容量） | D3 引入真 VramController 时一并评估 capacity 同步策略 |

#### 2.2.3 后续阶段延后项（per design.md §13）

- Compute Pipeline (StreamingMultiprocessor)
- D5 SR-IOV VF 多实例
- 多 PcieMemoryDevice 实例
- HBM timing 模型（D3 VramController）

---

## 3. 关键不变性 (Invariants)

### Inv-1: 4 BAR 配置（BAR0/1/2 必选 + BAR3+ 保留）

**Where**: `include/tlm/pcie/pcie_config_space_mvp.cc` + JSON `bar_sizes` 数组

```cpp
// PcieConfigSpace::init() 时从 bar_sizes 写入 BAR 寄存器
for (int i = 0; i < bar_sizes.size() && i < 6; ++i) {
    config_regs_.write(0x10 + i * 8, bar_sizes[i] & 0xFFFFFFFF);     // 低 32-bit
    if (bar_sizes[i] > 0x100000000ULL) {                              // > 4GB
        config_regs_.write(0x14 + i * 8, bar_sizes[i] >> 32);        // 高 32-bit
    }
}
```

**测试**: `assert(bar_sizes.size() == 3)` + 64-bit 双 dword 编码正确

### Inv-2: 15 ABI + 0 新增

**Where**: `include/abi/cpptlm_emulator.h`（per ADR-DGPU-08）

```cpp
#define CPPTLM_ABI_FUNCTION_COUNT 15
static_assert(CPPTLM_ABI_FUNCTION_COUNT == 15, "D-AXI requires 15 ABI functions");
```

**测试**: `[abi][minimal_dgpu_soc]` 28 assertions 全绿

### Inv-3: 6 模块拓扑

**Where**: `configs/dgpu_soc_minimal_v1.json` + ModuleFactory::instantiateAll

```json
{
  "modules": [
    "pcie_ep",
    "pcie_memory",
    "gmmu",
    "sdma",
    "pcie_config_space"
  ]
}
```

**测试**: minimal_v1 JSON 加载后 SoC 拓扑包含 5 + 1 = 6 模块（DGpuBoard shell + 5 子模块）

### Inv-4: Coherence 边界归 UsrLinuxEmu

**Where**: spec Requirement 显式声明 + 代码无任何 coherence 字段

```bash
# grep 检查 CppTLM 不建模 coherence
grep -rn "coherence\|cache_coherent\|dma_coherent" include/tlm/ src/tlm/ 2>/dev/null | grep -v "// " | grep -v "/\\*"
# 期望: 零匹配（除注释说明）
```

---

## 4. 实施步骤

| 步骤 | 任务 | 位置 |
|------|------|------|
| 1 | spec.md Requirement "PcieConfigSpace BAR 寄存器从 bar_sizes 生成" 落地 | OpenSpec spec.md |
| 2 | PcieConfigSpace::init() 实现 BAR 寄存器生成 | src/tlm/pcie/pcie_config_space_mvp.cc |
| 3 | spec.md Requirement "15 ABI Functions Unchanged" Scenario 验证 | OpenSpec spec.md |
| 4 | minimal_v1 JSON 6 模块拓扑加载 | configs/dgpu_soc_minimal_v1.json |
| 5 | 5 消费者注入 vram_storage_ (per ADR-DGPU-05) | dgpu_board_shell.cc |
| 6 | Coherence 边界声明 grep 验证 | CI 检查 |
| 7 | spec.md Requirement "Coherence 域边界" Scenario 验证 | OpenSpec spec.md |

**预计工时**: 2-3 工作日（与 ADR-DGPU-05 / 06 同步实施）

---

## 5. 测试策略

### 5.1 范围边界单元测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_in_scope_4_bar | BAR0/1/2 + BAR3+ 保留配置 | `[scope][bar]` |
| test_in_scope_15_abi | 15 ABI 函数计数 | `[scope][abi]` |
| test_in_scope_6_modules | 6 模块拓扑加载 | `[scope][modules]` |
| test_coherence_not_modeled | CppTLM 无 coherence 字段 | `[scope][coherence]` |

### 5.2 集成测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_minimal_dgpu_soc_e2e | driver 经 15 ABI + 4 BAR 闭环 | `[minimal_dgpu_soc][driver_visible]` |
| test_h2d_d2h_d2d_via_15_abi | 3 类典型操作 | `[minimal_dgpu_soc][driver_visible]` |
| test_5_consumers_share_vram | 5 消费者共享 vram_storage_ | `[minimal_dgpu_soc][vram]` |

### 5.3 回归测试

- 既有 `[sdma]` 套件 baseline 全绿
- `[pcie-memory]` 24 cases 全绿
- `[minimal_dgpu_soc]` 41 assertions + `[abi][minimal_dgpu_soc]` 28 assertions 全绿

---

## 6. 兼容性

### 6.1 ABI 兼容性

- 15 ABI 冻结（per ADR-DGPU-08）
- 23 ABI 签名级 + 二进制级 0 diff
- 新功能必须经现有 ABI 闭环（0 新增）

### 6.2 JSON 配置兼容性

- minimal_v1 → D3/D4/D5 JSON schema 兼容（per spec Requirement "minimal_v1 JSON 配置扩展"）
- `bar_sizes` 数组可扩展支持更多 BAR

### 6.3 测试兼容性

- minimal_v1 baseline 全绿
- 跨仓 UsrLinuxEmu driver binary 在 minimal → D5 演进过程零迁移

---

## 7. 风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| 范围扩张（minimal_v1 实施混入 D3 特性） | 🔴 H | 本 ADR 显式声明 In/Out Scope；每变更检查范围边界 |
| BAR 寄存器生成 bug 导致 host 枚举失败 | 🔴 H | v1.3 N6 + B5 64-bit 双 dword 严格测试 |
| ABI 函数计数漂移 | 🔴 H | `[abi]` 28 assertions 强制验证 |
| Coherence 边界破坏（UsrLinuxEmu 端误用） | 🟡 M | spec 显式声明 + grep 检查 |

### Oracle/Metis 评审重点

- **v1.2 Oracle 二次审查**: 8 must-fix (N1-N12)
- **v1.3 用户质疑**: B7 单一 VRAM 所有权根因
- **v1.4 P0-2**: 3 项遗留议题延期声明
- **v1.6 Oracle 三轮 F12**: MemoryTLM 移除决策
- **v1.7 Oracle 第四轮 H1/H4**: wire-format + D2D

---

## 8. 参考

- D-AXI OpenSpec change: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8)
- D-AXI proposal.md §1 范围 + §7 不在范围
- D-AXI design.md §1 系统拓扑 + §13 不在范围
- D-AXI spec.md Requirement "驱动视角完整最小设备" + "15 ABI Functions Unchanged" + "Coherence 域边界"
- D-AXI tasks.md P0-2 v1.4 延期声明
- 实施笔记: [../../pcie/driver-visible-minimal-soc.md §1 范围 + §6 遗留议题](../../pcie/driver-visible-minimal-soc.md)
- 配套 ADR: ADR-DGPU-05 (B7 单一 VRAM 所有权), ADR-DGPU-06 (B2 wire-format 边界), ADR-DGPU-07 (D3-D5 演进 seam), ADR-DGPU-08 (ABI 冻结)

---

## Status Update

_(本节将在 D-AXI v1.4 实施通过 Oracle 评审后追加，记录签发时间与实施 commit hashes)_
