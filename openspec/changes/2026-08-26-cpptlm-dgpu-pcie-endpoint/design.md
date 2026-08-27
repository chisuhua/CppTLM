# cpptlm-dgpu-pcie-endpoint: Design

> **配套**: [`proposal.md`](./proposal.md) · [`tasks.md`](./tasks.md)
> **状态**: 📐 Design · **Owner**: CppTLM Team
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D2

## 1. 组件定位

`PcieEndpointTLM` 是 dGPU SOC 片内的 PCIe Endpoint IP 模型（slave 方向）。真实硬件中它位于 GPU die 内，对外呈现 Config Space / BAR / MSI-X，对内把 host 事务路由到寄存器块与显存。本组件是 Board C++ shell 与 SOC 内部拓扑之间的**唯一事务入口**。

```text
DGpuBoard (C++ ABI shell)
    │  cpptlm_emulator_mmio_write() → 构造 PcieTlpBundle
    ▼  注入
┌─────────────────────────────────────────────────┐
│ PcieEndpointTLM                                  │
│   slave_in ──► BAR 路由 ─┬─► mmio_out ──► CP    │
│                          └─► mem_out  ──► VRAM  │
│   MSI-X engine ──► irq_out ──► Board 回调        │
└─────────────────────────────────────────────────┘
```

## 2. Bundle 定义（`include/bundles/pcie_bundles_tlm.hh`）

```cpp
namespace bundles {

// host→endpoint 统一事务（config / BAR0 MMIO / BAR1 mem）
struct PcieTlpBundle {
    enum class Kind : uint8_t { CFG_READ, CFG_WRITE, MMIO_READ, MMIO_WRITE,
                                MEM_READ, MEM_WRITE };
    Kind     kind = Kind::MMIO_READ;
    uint8_t  bar_index = 0;      // MMIO/MEM 时有效
    uint64_t offset = 0;         // config offset 或 BAR 内偏移
    uint32_t size = 4;           // 访问字节数（1/2/4/8，MEM 可为块；语义见 §2.3）
    uint64_t data = 0;           // 写数据 / 读回数据（≤8B inline；>8B 走 §2.3 路径）
    uint16_t requester_id = 0;   // PCIe Requester ID（bus/dev/fn），诊断用
    uint32_t trans_id = 0;       // 事务关联
};

// endpoint→host 中断投递
struct MsiXDeliveryBundle {
    uint16_t vector = 0;
    uint32_t msg_data = 0;
    uint64_t msg_addr = 0;       // MSI-X table 中地址（diagnostic）
    uint32_t trans_id = 0;
};

} // namespace bundles
```

设计原则：bundle 是**轻量值类型**（per `include/bundles/cache_bundles_tlm.hh` 惯例，`stream_adapter.hh:32,104` 按值存储），不含指针所有权；MEM 块访问通过 `offset+size` 描述，payload 内存访问由 mem_out 下游组件执行。

**范围冻结**：本 change 交付的 `pcie_bundles_tlm.hh` 仅含 `PcieTlpBundle` 与 `MsiXDeliveryBundle` 两类型。DMA descriptor 类（如 `DmaDescriptor`）由配套的 `cpptlm-dgpu-sdma-engine` change 在独立文件 `include/bundles/dma_bundles_tlm.hh` 中定义，本 change 不预留字段（避免超前设计）。

### 2.3 BAR1 MEM 块访问数据面（per ADR-SOC-07 Status Update Q3 裁决）

`PcieTlpBundle.data` 字段是 `uint64_t`（8B inline）。BAR1 aperture 256MB 显存窗口，driver 4KB 块 DMA 远超 8B。**禁止**破坏 POD 惯例用 `shared_ptr` 携带 payload。落地规则：

- **size ≤ 8**：构造完整 `PcieTlpBundle`（data 内联）走 timed 路径 → `mem_out` → `MemoryTLM`。
- **size > 8**：host 侧实际数据经 23 ABI 已有的 `backdoor_read/write` 通道（语义即"绕过 timing 的批量数据通道"，与 SystemC TLM DMI 同构，由 DGpuBoard shell 路由到 VRAM 单一 owner 存储）；shell 再发一个 **descriptor-only `MEM_WRITE` TLP**（`size = actual_size, data = 0`）经 `mem_out` 推进带宽模型。MemoryTLM 收到 descriptor 后按 `size` 字段注入 `ceil(size/burst_width)` 拍延迟（参考 `CacheReqBundle` 的 `fragment_total` 分片机制）。
- 读路径对称（descriptor-only `MEM_READ` + backdoor 回收数据）。
- **VRAM 单一 owner 是 `MemoryCluster`**：`PcieEndpointTLM` 不持有 VRAM 存储（per ADR-SOC-07 D2）；backdoor 路径必须落到同一存储，否则 timed 路径与 backdoor 路径数据漂移。

### 2.4 `size` 字段语义

**全文档冻结**：`PcieTlpBundle.size` 语义为**字节数**（非拍数、非 dword 数），含 MEM 块。MMIO 时典型值 1/2/4/8；MEM 块可任意不超过 BAR1 aperture。

## 3. 内部结构

```cpp
class PcieEndpointTLM : public ChStreamModuleBase {
    PcieConfigSpace cfg_space_;   // 256B/4KB 数组 + capability chain
    PcieBarRouter   bar_router_;  // BAR0 寄存器路由表（数据化）
    MsiXTable       msix_;        // vector table + PBA + pending
    // 端口: slave_in / mmio_out / mem_out / irq_out (4 端口)
};
```

- **PcieConfigSpace**：`config_size` 参数（256 或 4096，JSON `params` 注入）；capability chain 由 JSON 声明（`capabilities: [{id, offset, next}, ...]`），支持 PM / MSI-X / PCIe / Vendor-Specific。
- **PcieBarRouter**：寄存器表数据化——JSON `params.bar0_registers` 声明 `{offset, name, access, side_effect}`；门铃条目 `side_effect: "doorbell"` 触发 `mmio_out` 门铃事务（沿用 s2 `Doorbell` strong-order 语义，250-700ns 区间）。**禁止** C++ `if (offset == 0x0014)` 硬编码。
- **MsiXTable**：`num_vectors` 参数化（默认 16，对齐 UsrLinuxEmu `MSIX_DEFAULT_VECTORS`）；`update_pending(vector)` → `irq_out` 投递；`clear_pending(vector)` 由 driver EOI 路径触发。

## 4. tick() 行为

- `slave_in` 事务在到达周期解码：CFG_* → `cfg_space_`；MMIO_* → `bar_router_.route()`（命中 doorbell → 生成 `mmio_out` 事务）；MEM_* → 转发 `mem_out`。
- `irq_out` 在 `update_pending` 后同周期或按 `irq_latency` 参数延迟投递。
- 读事务响应经 slave_in 的返回通道（StreamAdapter resp 方向）回到 Board shell。

## 5. 与 s2 的语义迁移对照

| s2 `DGpuBoardTLM` 硬编码 | 本组件目标形态 |
|--------------------------|---------------|
| `write_reg(0x0014)` if-else 门铃 | `bar0_registers` 表条目 `side_effect: "doorbell"` → `mmio_out` |
| `bar0_size()=0x10000` / `bar1_size()=256MB` 常量 | JSON `params.bar_sizes[]` |
| `read_vram/write_vram` memcpy | `mem_out` 事务 → MemoryCluster（latency 可配） |
| `DGpuBar` 256B 寄存器数组 | `PcieConfigSpace` + 路由表 |

## 6. 测试策略

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_pcie_endpoint_config_space.cc` | `[pcie][endpoint]` | config r/w、capability walk、越界返回 0xFFFFFFFF |
| `test_pcie_endpoint_bar_routing.cc` | `[pcie][endpoint]` | BAR0 doorbell→mmio_out（数据化寄存器表）、BAR1 `size ≤ 8` → mem_out、BAR1 `size > 8` → shell backdoor + descriptor-only TLP（per §2.3 Q3）、未知偏移丢弃 |
| `test_pcie_endpoint_msix.cc` | `[pcie][msix]` | pending 置位/投递/清除、vector 越界拒绝 |
| `test_pcie_endpoint_from_config.cc` | `[pcie][json]` | JSON 实例化、4 端口 adapter 注入（非空断言）、connections 解析 |

## 7. 风险与缓解

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | 多端口 adapter 路径（`set_stream_adapter(adapters[])`）在现有模块中已有先例：`CrossbarTLM`（`include/tlm/crossbar_tlm.hh`）与 `ArbiterTLM`（`include/tlm/arbiter_tlm.hh:99`）均 override 数组版本；本组件复用相同模式 | 复用 `registerMultiPortAdapter` 模式；PE-G5 专项验证 |
| R2 | BAR1 大块 MEM 访问经 bundle 描述但不带 payload，需下游 memory 组件支持 offset+size 块访问 | bundle 只描述窗口；实际数据面由 MemoryCluster 端口完成，测试覆盖 |
| R3 | capability chain 数据化 schema 漂移 | PE-G2 覆盖 chain walk；schema 在 spec.md 冻结 |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 待评审后实施
