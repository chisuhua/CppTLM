# cpptlm-minimal-dgpu-soc-v1-architecture: Minimal DGpu SoC 架构视图定稿 + Backing 命名约定代码实施

> **状态**: 📋 提案 — 2027-02-09 (基于 Oracle 5 步解锁链 + ADR-DGPU-10)
> **来源**: docs/designs/dgpu-soc/architecture.md 现状(已修订) + ADR-DGPU-10(已新建)
> **配套 ADR**: [ADR-DGPU-05](../docs/adr/ADR-DGPU-05-vram-storage-ownership.md) (单一 VRAM 所有权) + [ADR-DGPU-10](../docs/adr/ADR-DGPU-10-backing-naming-convention.md) (backing 字段命名约定) + ADR-DGPU-06/07
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../cpptlm-driver-visible-minimal-soc/) (D-AXI v1.4 实施)
> **优先级**: 🟡 P2 (架构对齐 + 命名规范,非紧急)
> **工期**: 1.5-2.5 工作日 (代码 rename 0.5-1d + 测试 1d + 归档 0.5d)

## Why

### 上下文:Minimal DGpu SoC v1.0 架构视图已对齐代码,但代码 rename 未实施

架构审查发现:
1. `docs/designs/dgpu-soc/architecture.md` 文档与 D-AXI v1.4 代码实际状态存在大量不一致(BAR1 大小、doorbell offset、命名约定等)
2. Oracle 5 步解锁链 (2027-02-09) 完成 **文档层对齐**:
   - §1.4 仿真模式声明(Functional Mode LT zero-delay)
   - §4.1 BAR 布局与实际 16MB+ doorbell 0x10010000 一致
   - §5 GMMU 改为 Functional Translation Service
   - §7.1 JSON 示例与 `dgpu_soc_minimal_v1.json` 对齐
   - §10.2 E2E 测试用例修复(API 错误、端序依赖)
3. **ADR-DGPU-10** 已签发命名约定 (owner/injected 两级),但 §4 Migration 8 步代码 rename 未实施
4. PcieMemoryDevice 仍有 `memory_backing_` 字段(ADR-DGPU-05 v1.4 B7 应删除)

### 用户决策 + Oracle 评审结论

| # | 来源 | 决策 |
|---|------|------|
| O1 | Oracle 5 步解锁链 Step 1 | §1.4 仿真模式声明为元答案,8 个问题依赖此声明 |
| O2 | Oracle Step 2 | 文档权威源 = 代码 + D-AXI v1.4,本文档降级为"视图" |
| O3 | Oracle Step 3 | SDMA memcpy = functional-mode 第一类原语,**不是 hack** |
| O4 | Oracle Step 4 | MemoryTLM = seam holder,v1.1 拆分 |
| O5 | Oracle Step 5 | ADR-DGPU-10 命名约定 + Migration 8 步需 OpenSpec change 实施 |
| A1 | ADR-DGPU-05 v1.4 B7 | 单一 VRAM 所有权,但 PcieMemoryDevice::memory_backing_ 仍存在 |

### 为什么需要本 OpenSpec change

| 现状 | 影响 |
|------|------|
| 文档层 (architecture.md) | ✅ 已对齐 (Oracle 5 步) |
| ADR-DGPU-10 | ✅ 已签发 (命名约定清晰) |
| ADR-DGPU-05 Status Update | ✅ 已追加 ADR-DGPU-10 引用 |
| **代码层字段命名** | ❌ framebuffer_storage_ 仍是旧名 |
| **MemoryTLM setter** | ❌ set_backing_store 旧名 |
| **GmmuTLM 字段+setter** | ❌ backing_ + set_backing 旧名 |
| **PcieMemoryDevice::memory_backing_** | ❌ v1.4 B7 未实施 |

## What Changes

### §1 范围:架构视图定稿 + Backing 命名约定代码实施

**功能边界**:

| 模块/文件 | 改动 |
|----------|------|
| `include/tlm/gpu/dgpu_board_shell.{hh,cc}` | `framebuffer_storage_` → `vram_storage_` (字段 + 调用点 23+ 处) |
| `include/tlm/memory_tlm.{hh}` | `set_backing_store` → `set_backing_view`;`backing_ptr_` → `backing_view_` |
| `include/tlm/gpu/gmmu_tlm.{hh,cc}` | `backing_` → `mem_view_`;`set_backing` → `set_mem_view` |
| `include/tlm/gpu/pcie_memory_device.{hh,cc}` | 删除 `memory_backing_` 字段 (per ADR-DGPU-05 v1.4 B7) |
| `src/tlm/gpu/dgpu_board_shell.cc` | `bind_memory_backings` 调用方同步 + `dgpu_soc_minimal_v1.json` 引用同步 |
| `test/` | 24-case `[pcie-memory]` 套件机械迁移至新 setter 名 |
| `docs/designs/dgpu-soc/architecture.md` | §3.4 代码示例同步新命名;§3.6 路径说明同步 |

### §2 不在范围内 (Non-Goals)

- **NG1**: 不改 23 ABI 签名 (per ADR-088 §D5)
- **NG2**: 不改 SDMA 5 端口 wire-format 混合 (PcieTlpBundle v1.0 + AxiMemBundle v1.1)
- **NG3**: 不引入新模块(纯 rename)
- **NG4**: 不修改 `kBar1DoorbellOffset = 0x10010000`(7 阶段 PCIe EP 共识;通过 §4.1 BAR1 ≥256MB+64KB 解决)
- **NG5**: 不重构 DGpuBoard::init() 顺序 (resize → bind_memory_backings → sim_thread_)

### §3 关联变更

| 项 | 关系 |
|---|------|
| ADR-DGPU-05 (单一 VRAM 所有权) | 本 change 实施其 §3 Inv-3 "PcieMemoryDevice 不持有 backing" |
| ADR-DGPU-07 (演进 seam) | D3 演进 seam 接口零变更,本 change 不破坏 |
| ADR-DGPU-10 (命名约定) | 本 change 实施其 §4 Migration 8 步 |
| D-AXI v1.4 B7/B11/B15 | 本 change 落 D-AXI 修订项 |

### §4 预期收益

1. **词汇表一致性** — 文档 + ADR + 代码三处命名完全对齐
2. **可演化性** — v1.1 SDMA 切 AXI + GMMU 异步化时无命名混乱
3. **新模块作者可遵循** — `*_storage_` (owner) / `*_backing_view_` (ChStream) / `*_mem_view_` (functional)
4. **PcieMemoryDevice B7 落地** — 5 消费者共享 vram_storage_ 闭环

## Status

_(本节将在 change 实施 + Oracle 评审后追加)_
