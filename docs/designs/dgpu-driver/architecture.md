# Minimal DGpu SoC Driver 视角架构 (v1.8)

> **版本**: v1.8
> **日期**: 2026-09-26
> **状态**: ✅ v1.8 normative 文本收口完成
> **配套 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 P0 N1-N5)
> **业务视角**: [../dgpu-soc/architecture.md](../dgpu-soc/architecture.md) | **基础架构**: [../dgpu-board/architecture.md](../dgpu-board/architecture.md)

## 0. 文档定位

本文档从 **Linux kernel driver 视角** 描述 Minimal DGpu SoC 的外部接口契约（ABI + BAR + 中断），不涉及 CppTLM 内部实现。适用于：

- UsrLinuxEmu 端 Linux driver 编写者（"我通过这 15 个 ABI + 4 个 BAR 能看到什么"）
- 跨仓 driver-visible 测试用例设计者（"driver 视角应满足哪些 Scenarios"）
- 架构评审者验证 driver 闭环完整性

## 1. 入口：15 ABI 函数表

**约束**：**0 新增**（per ADR-088 §D5；冻结面 4 个头文件零 diff）

ABI 集合定义于 [include/abi/cpptlm_emulator.h](../../../include/abi/cpptlm_emulator.h)。完整 15 函数表与签名级 + 二进制级 0 diff 仲裁见 [../dgpu-board/architecture.md §5.1](../dgpu-board/architecture.md)。

按功能分组：

| 分组 | 函数数 | 说明 |
|------|--------|------|
| Lifecycle | 4 | create / destroy / load_soc_config / init |
| Config space | 2 | config_read / config_write |
| MMIO (BAR0/1/2) | 4 | mmio_read / mmio_write（BAR 索引参数化） |
| Backdoor (特权) | 2 | backdoor_read / backdoor_write |
| DMA + 中断 | 3 | trigger_dma_translate_async / set_irq_callback / set_error_callback |

## 2. BAR 视角

### BAR0 — 4KB MMIO（控制寄存器）

| 偏移 | 名称 | 读写 | 说明 |
|------|------|------|------|
| 0x00 | DEVICE_IDENTITY | R | vendor_id + device_id (例: 0x0001_1002) |
| 0x04 | COMMAND_STATUS | R/W | PCIe 命令/状态寄存器 |
| 0x08 | MSI-X_CTRL | R/W | MSI-X 使能 + vector 数 |
| 0x10 | GMMU_PT_BASE_LO | R/W | GMMU 页表基址低 32-bit |
| 0x14 | GMMU_PT_BASE_HI | R/W | GMMU 页表基址高 32-bit |
| 0x18 | GMMU_CTRL | R/W | GMMU 使能 + 翻译策略 |
| 0x20 | SDMA_STATUS | R | SDMA 状态（**v1.6 F6**: write-mirror，不反映真设备状态）|
| 0x10000000 | DOORBELL | W | SDMA doorbell（**测试专用合成偏移**，**v1.8 H1**: BAR0 0x10010000 超出 4KB 实窗） |
| 0x10001000 | RING_DOORBELL | W | Descriptor ring doorbell（**测试专用合成偏移**） |

### BAR1 — 16MB FrameBuffer 窗口（host backdoor fast-path）

- **窗口大小**: 16MB（PCIe 物理 BAR）
- **真 VRAM 大小**: 8GB（与 BAR1 窗口不等，per ADR-...-B11 双 size 拆分）
- **访问路径**: host → `mmio_read/write(1, ...)` → `DGpuBoard::bar1_fast_path` → `framebuffer_` 直读直写
- **测试合成**: doorbell 0x10010000 与 ring 0x10000000 区间超出 16MB 实窗，由 `dgpu_board_shell.cc:306` doorbell 路径优先级匹配（bound 检查前），**不**触发越窗

### BAR2 — 8GB vram aperture（完整 VRAM 访问）

- **窗口大小**: 8GB（per v1.3 B5 64-bit 双 dword：`read(0x20)==0` + `read(0x24)==2`）
- **访问路径**: host → `mmio_read/write(2, ...)` → `DGpuBoard::bar2_fast_path` → `PcieMemoryDevice::memory_backing_`
- **driver 视角闭环**: driver 经 BAR2 mmio_write PTE → GMMU 经 chip-internal AXI 读 PTE → driver 经 BAR2 mmio_read 验证

### BAR3+ — 保留（PCIe spec 最多 6 BAR）

`bar_sizes` 数组为 3 元素（per spec Requirement "PcieConfigSpace BAR 寄存器从 bar_sizes 生成"）。

## 3. 冻结面（0 diff 约束）

**签发后不可修改**，仅可追加 `[[deprecated]]` 属性：

- [include/tlm/gpu/pcie_endpoint_tlm.h](../../../include/tlm/gpu/pcie_endpoint_tlm.h)
- [include/tlm/gpu/pcie_display_device.hh](../../../include/tlm/gpu/pcie_display_device.hh)
- [include/tlm/gpu/pcie_bundles_tlm.hh](../../../include/tlm/gpu/pcie_bundles_tlm.hh)
- [include/abi/cpptlm_emulator.h](../../../include/abi/cpptlm_emulator.h)

**新功能必须经现有 15 ABI 闭环**（无新签名）；新增 PCIe 能力通过扩展 ABI 行为或 PcieEndpointIP 子类实现。

## 4. Driver 闭环数据流

### H2D (Host → Device)：descriptor 注入 + DMA 写入 VRAM

1. driver 写 descriptor 到 BAR1 ring window (offset ∈ [0x10000000, 0x10001000))
2. driver 触发 doorbell (BAR0+0x10000000)
3. SDMA enqueue descriptor（**v1.8 H5**: doorbell 仅置 flag，consume 移入 tick）
4. host_backdoor → vram_backdoor memcpy 落地 bulk data（**v1.8 H3**: BAR1 窗口即 host 内存仿真）
5. 完成信号经 fence → MSI-X vector 0 通知 driver（intr_cb 异步路径）
6. driver 后续 BAR2 mmio_read 验证数据

### D2H (Device → Host, host-pull)：driver 直接读 VRAM

1. driver 调 `mmio_read(2, off, buf, len)`（BAR2）
2. `DGpuBoard::bar2_fast_path` → `PcieMemoryDevice::memory_backing_` 直读
3. 同步返回 vram 内容

### D2D (Device → Device)：SDMA 内部搬运

1. driver 提交 D2D descriptor (src + dst IOVA) 到 BAR1 ring 区间
2. SDMA 内部经 `mem_out` 两拍（read + write 同一 backing）
3. driver 视角：走 H2D/D2H 同一 ring 路径，无独立 ABI

## 5. 演进路径（minimal → full GPU, driver 视角零迁移）

**承诺**：driver 源码在 minimal → D3 → D5 演进过程中**零修改**。

| 阶段 | BAR0/1/2 布局 | ABI 行为 | driver 影响 |
|------|---------------|---------|------------|
| minimal_v1 (本设计) | BAR0 4KB + BAR1 16MB + BAR2 8GB | 15 ABI 全功能 | 基线 |
| D3 VramController | 同上（size 可能增大） | 同上 | 0 |
| D4 MemoryCluster | 同上 | 同上 | 0 |
| D5 SR-IOV VF | 同上（VF 扩展 EP 端口） | 同上 | 0 |

**关键 seam**: `handle_slave_port ↔ backing_ptr_` 之间可插入 VramControllerTLM / MemoryClusterTLM（接口零变更）。

## 6. 与 OpenSpec / ADR 关系

- **OpenSpec change**: [cpptlm-driver-visible-minimal-soc](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前) — 实施指导与 5 轮 Oracle/Metis 评审补丁
- **ADR 候选**: ADR-DGPU-08 (abi-freeze-policy) + ADR-DGPU-09 (driver-visible-minimal-soc-scope) 待签发

## 关联 OpenSpec changes

- [cpptlm-driver-visible-minimal-soc](../../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前)

**同步规则**：本架构文档代表 driver 接口长期契约；具体实施指导与评审补丁见 OpenSpec change 的 design.md。
