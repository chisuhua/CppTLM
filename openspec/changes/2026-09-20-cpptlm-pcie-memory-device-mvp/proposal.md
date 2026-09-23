# 2026-09-20-cpptlm-pcie-memory-device-mvp: Memory 设备 MVP

> **状态**: 🔄 v1.1 修订 — 2026-09-23
> **v1.0 提案**: 2026-09-22（0/9 tasks）
> **v1.1 修订原因**: D1 v1.1.1 实施后 Oracle 复审（ses_f342ea637ffeDWYBBEQDUUE63U）发现 D2 设计含 3 个与 D1 root cause 4 同型盲点（无条件路由劫持）。本提案在 D2 实施前引入 `memory_routing_enabled_` flag 防劫持。
> **D1 v1.1.1 实施经验**: 归档于 `openspec/specs/display-io-mvp/spec.md`；D1 路由开关 `display_routing_enabled_` 模式作为 D2 设计模板
> **优先级**: 🟡 P1（驱动验证驱动链 D 阶段）
> **工期**: 3-4 工作日（原 v1.0 估算 + 0.5 工作日 v1.1 修订 = 3.5 工作日）
> **目标**: 在 CppTLM dGPU SoC 中实现**内存设备**（PcieMemoryDevice），通过 PCIe BAR 暴露大容量 memory backing，让 UsrLinuxEmu 端 NVMe-like / memory-mapped 驱动可不依赖 ArchForge 仓进行 BAR 枚举 + MMIO 读写验证。

## Why

### 用户决策（2026-09-22）

> "先做显示 IO 设备（D1），后做内存设备（D2），最后做 GMMU PoC（D3）"

D1（Display IO MVP）已建立：
- `PcieDisplayDevice`（4KB BAR 0 MMIO + 32MB BAR 1 FB + VBLANK MSI-X vector 0）
- DGpuBoard BAR 0/1 路由框架（`mmio_read/write` + `backdoor_read/write` 路径）
- 15 ABI 冻结不变

D2 在 D1 路由框架之上新增 **memory device**（不是 GPU 计算设备）：
- UsrLinuxEmu 端驱动可作为 NVMe-like / memory-mapped device 验证（不集成 GPU 计算）
- **零 ABI 新增**（D1 已建立的 15 ABI 不变）
- **不依赖 ArchForge 仓**（运行时）

### 预期收益

1. **新增 `PcieMemoryDevice`** — dGPU 第二类可驱动验证的设备（注册 4KB BAR 0 MMIO 寄存器 + 8GB BAR 2 memory backing）
2. **DGpuBoard 路由 BAR 0/2 → 真实设备** — 复用 D1 路由框架，扩展到 BAR 2
3. **UsrLinuxEmu NVMe-like / memory-mapped 驱动验证闭环** — vendor_id/device_id 来自真实 config space，memory read/write 来自真实设备
4. **零 ABI 新增** — 现有 15 ABI 函数签名零变更
5. **工期短**（3-4 天）— 复用 D1 已验证的路由框架，无需重新设计

## What Changes

### §1 D2 范围：内存设备 MVP

**功能边界**（最小可行）：
- 一个 PCIe 内存设备（`PcieMemoryDevice`），**BAR 0** 暴露 4KB MMIO 寄存器空间
- 寄存器布局：
  - `0x00-0x0F` Device Identity（vendor_id=0x1002 + device_id=0x0002，**区别 D1 的 device_id=0x0001**）
  - `0x10` MEM_SIZE_LO（容量低 32-bit，D2 默认 8GB）
  - `0x14` MEM_SIZE_HI（容量高 32-bit）
  - `0x18` MEM_BASE_LO（主机可见基地址，D2: identity）
  - `0x1C` MEM_BASE_HI
  - `0x20` STATUS（ready/error bits）
  - `0xF0-0xFF` SCRATCH（往返测试）
- **NO MSI-X**（D1 已用 MSI-X vector 0；D2 不占用中断资源）
- **NO VBLANK**（D2 是被动内存设备，无周期性中断）
- **BAR 2** 8GB Memory backing（lazy alloc，首次访问时分配）
- **NO 真实 GPU 计算**（不继承任何 SM/SDMA）—— D2 是纯 memory device
- **零 ABI 新增**：现有 15 ABI 函数 + 4 callback typedef 保持不变

### §2 必须修改的 DGpuBoard 路径（复用 D1 路由框架）

| DGpuBoard 方法 | D1 行为 | D2 行为（扩展） |
|---------------|---------|----------------|
| `mmio_read/write`（BAR 0） | 路由到 `PcieDisplayDevice` | **新增**：memory 路由到 `PcieMemoryDevice`（BAR 0 寄存器 0x10-0x2C） |
| `mmio_read/write`（BAR 1） | 路由到 `PcieDisplayDevice::framebuffer_` | **不变**（D1 继续生效） |
| `mmio_read/write`（BAR 2） | N/A（未映射） | **新增**：路由到 `PcieMemoryDevice::memory_backing_` |
| `backdoor_read/write` | 路由到 `PcieDisplayDevice::framebuffer_` | **新增**：路由到 `PcieMemoryDevice::memory_backing_`（当访问 BAR 2 时） |
| `msix_update_pending` | D1 的 VBLANK 触发 | **不变**（D2 无 MSI-X） |

**关键设计决策（v1.1 修订）**：

- 不修改 `src/abi/cpptlm_emulator.cc`（ABI wrapper 已正确）
- 不新增任何 ABI 函数
- 所有路由变更在 `src/tlm/gpu/dgpu_board_shell.cc` 的私有方法中（不暴露新 ABI）
- `PcieMemoryDevice` 由 `PcieEndpointIP` 持有（通过 `tick()` 注入推进），`DGpuBoard` 通过 `soc_->getInternalInstance("pcie_ep")` 访问
- **D2 BAR 0 与 D1 BAR 0 共用 `bar == 0` 判断** —— D1 v1.1.1 root cause 4 教训：**无条件路由 = 静默劫持**
- **v1.1 修订决策**：D2 引入 `memory_routing_enabled_` flag（默认 false，JSON 显式启用），与 D1 的 `display_routing_enabled_` flag 对称
  - BAR 0 fast-path 条件改为：`memory_routing_enabled_ && bar==0 && soc_ && ep->has_memory_device()`
  - BAR 2 fast-path 条件改为：`memory_routing_enabled_ && bar==2 && soc_ && ep->has_memory_device()`
  - backdoor fast-path 同上加 `memory_routing_enabled_`
  - 各 flag 独立：默认全部 false，SOC 配置按需启用；GPU BAR0 寄存器（GPFIFO_PUT/doorbell 0x00/0x14）不被 memory device 误劫持
  - D2 不强制要求 memory_routing 启用——仅 D2 配置（如 `dgpu_soc_with_memory_device.json`）启用
- D2 BAR 0 priority：**`has_display_device()` 优先于 `has_memory_device()`** —— D1 已经路由 BAR 0 时优先用 D1 device；D2 内存 device 寄存器在 BAR 0 0x10-0x2C（与 D1 DISPLAY_MODE 0x10 重叠，但 D1 优先）

### §3 文件清单（v1.1 修订）

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/tlm/gpu/pcie_memory_device.hh` | **新** | `PcieMemoryDevice` 类声明（BAR 0 寄存器 4KB + BAR 2 memory backing 8GB） |
| `src/tlm/gpu/pcie_memory_device.cc` | **新** | PcieMemoryDevice 实现（mmio_read/write + memory_backing read/write + tick()） |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | **改** | 添加 `memory_device()` accessor + 持有 `std::unique_ptr<PcieMemoryDevice>` 成员 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | **改** | `tick()` 调用 `memory_device_->tick()`（推进就绪/错误状态）；init 时构造 memory_device |
| `include/tlm/gpu/dgpu_board_shell.hh` | **改** | 新增 `memory_routing_enabled_` 成员 + accessor |
| `src/tlm/gpu/dgpu_board_shell.cc` | **改** | `mmio_read/write` 增加 BAR 0/2 → memory device 分支（**v1.1 新增 `memory_routing_enabled_` 路由开关**）；`backdoor_read/write` 扩展到 memory backing；`load_soc_config` 末尾读 JSON 顶层字段 |
| `examples/dgpu_soc_with_memory_device.json` | **新** | D2 专用配置（含 `"memory_routing_enabled": true`） |
| `include/chstream_register.hh` | **改** | `REGISTER_CHSTREAM(PcieMemoryDevice)` |
| `test/test_pcie_memory_device_basic.cc` | **新** | 单元测试 1：BAR 0 寄存器 round-trip（mmio_read/write） |
| `test/test_pcie_memory_device_backing.cc` | **新** | 单元测试 2：BAR 2 memory backing round-trip（mmap 风格） |
| `test/test_pcie_memory_device_e2e.cc` | **新** | E2E 测试 3：Config → BAR enumerate → MMIO write/read → memory read/write（端到端） |
| `test/test_pcie_memory_device_routing_characterization.cc` | **新** | 回归测试 0（**T0 先写**）：锁定当前 board 路由行为，确保 D2 改动不引入回归；新增 `memory_routing_enabled_` flag 验证 |
| `test/test_dgpu_board_shell_abi.cc` | **改** | v1.1 新增：D2 routing flag + accessor round-trip 测试 |
| `test/CMakeLists.txt` | **改** | 注册新增 4 个 test_*.cc 文件 |
| `docs/pcie/memory-device-mvp.md` | **新** | CppTLM 仓内实现笔记（v1.1 标注 routing flag 决策） |
| `AGENTS.md` | **改** | "WHERE TO LOOK" 添加 `[memory]` 标签 |

**冻结面零触碰**：
- ✅ `include/abi/cpptlm_emulator.h`（15 ABI 函数签名不变；D2 不新增）
- ✅ `include/tlm/gpu/pcie_endpoint_tlm.h`（PcieEndpointTLM 4 端口冻结）
- ✅ `src/abi/cpptlm_emulator.cc`（ABI wrapper 已正确，不修改）
- ✅ `include/tlm/gpu/pcie_display_device.hh`（D1 已建立，不修改）

### §4 ABI 影响

- ✅ **0 个新 ABI 函数**（per ADR-088 §D5）
- ✅ **0 个 ABI 签名变更**（现有 15 函数 + 4 callback typedef 字节一致）
- ✅ 验证方法：`diff <(git show HEAD:include/abi/cpptlm_emulator.h) include/abi/cpptlm_emulator.h` 为空

### §5 不在 D2 范围（后续 D3）

| 项 | 后续阶段 | 原因 |
|----|----------|------|
| GPU 计算集成 | 不实现 | D2 是纯 memory device |
| MSI-X 中断 | 不实现 | D2 被动内存设备不需要中断 |
| VBLANK | 不实现 | D1 独有，D2 不需要 |
| GMMU PoC | D3（D1+D2 后） | 需要 IOMMU 支持 |
| 跨仓 ArchForge 引用 | **零** | 驱动验证不读设计文档 |

### §6 验收标准（DoD）

| 项 | 标准 |
|----|------|
| **设备真实化** | `PcieMemoryDevice::mmio_read(0x10, buf, 4)` 在 mmio_write(0x10, val, 4) 后返 val（不是 shell 编造的 0xFFFFFFFF） |
| **ABI 冻结** | `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空 |
| **配置空间真实** | `cpptlm_emulator_pcie_config_read(emu, 0x00, 4, &val)` 返回 vendor_id=0x1002 + device_id=0x0002（来自 memory device），不再是 -ENOSYS |
| **MMIO 路由 BAR 0** | 驱动 `mmio_write(emu, bar=0, off=0x10, &size_lo, 4)` 后 `mmio_read(emu, bar=0, off=0x10, buf, 4)` 返 size_lo（来自 device） |
| **BAR 2 memory 真实** | 驱动 `mmio_write(emu, bar=2, off=0x1000, &data, 8)` 后 `mmio_read(emu, bar=2, off=0x1000, buf, 8)` 返 data（来自 device） |
| **路由开关（v1.1 新增）** | `memory_routing_enabled_` 默认 false；显式启用后路由；未启用时不劫持 GPU BAR0 |
| **回归基线** | 现有 test cases 仍全绿；新增 ≥4 个 test cases |
| **跨仓独立** | UsrLinuxEmu 端构建 + E2E 测试**不**需要 clone ArchForge |

## Impact

### Who is affected

- **UsrLinuxEmu 端**：NVMe-like / memory-mapped stub driver 获得真实 BAR 0/2 读写，可验证 memory backing 持久性
- **CppTLM 端**：PcieEndpointIP 增加 memory device 子对象（向后兼容，可选启用）
- **现有测试**：D1 测试不受影响（仅扩展路由层）

### Dependencies

**依赖**：
- `PcieEndpointIP`（Phase 4 已交付，✅）
- `PcieDisplayDevice`（D1 已交付，✅）— D2 复用其 BAR 0 路由框架与 v1.1.1 `display_routing_enabled_` flag 模式
- `DGpuBoard`（✅，T3 修改扩展 BAR 2 + routing flag）
- `MsiXTable`（D1 依赖，但 D2 不使用 MSI-X）

**被依赖**：D3 (GMMU PoC) 将基于 D1+D2 的 BAR 路由框架

**不依赖 ArchForge**：✅（设计文档全部迁出后，驱动验证逻辑上完全自洽）

### Risks

| 风险 | 等级 | 缓解策略 |
|------|------|---------|
| **R1**（v1.1 修订）: BAR 0 routing priority（display vs memory 冲突，dGPU BAR0 0x00/0x14 冲突风险） | 🔴 高 | **v1.1 修订**：引入 `memory_routing_enabled_` flag（与 D1 `display_routing_enabled_` 对称），默认 false。`has_display_device()` 优先于 `has_memory_device()`（D1 已路由时不路由 memory）。**新 TDD 步骤 T0.5**：先写 `display_routing_enabled=true` + `memory_routing_enabled=true` 组合下 BAR 0 0x00/0x14 行为锁定测试，避免 D1 root cause 4 重演 |
| **R2**: 8GB memory backing 分配影响启动时间 | 🟢 低 | lazy alloc（首次 BAR 2 访问时分配） |
| **R3**: D2 tick() 与 D1 tick() 并行推进（无 MSI-X 但仍有 cycle counter） | 🟢 低 | memory device tick() 仅推进 cycle_counter_，不触发中断 |

## Tasks 概要（详细见 `tasks.md`）

- T0: Characterization test（**先写**，防回归）— 锁定当前 board BAR 2 未映射行为
- **T0.5（v1.1 新增）**: 路由 flag 组合测试——`display_routing_enabled=true` + `memory_routing_enabled=true` + BAR 0 0x00/0x14 行为锁定
- T1: PcieMemoryDevice 骨架（class + BAR 0 寄存器 4KB + BAR 2 memory backing 8GB）
- T2: PcieEndpointIP 注入 memory_device（unique_ptr + tick() 推进）
- T3: DGpuBoard 路由层（mmio_read/write + backdoor_read/write BAR 0/2 → device）
- **T3.5（v1.1 新增）**: `memory_routing_enabled_` flag 实施（与 D1 v1.1.1 对称）——`load_soc_config` 读 JSON 顶层字段，4 处 fast-path 加 `&& memory_routing_enabled_`
- T4: 单元测试（basic + backing + routing 3 个）+ E2E 测试
- T5: docs/pcie/memory-device-mvp.md + AGENTS.md 更新 + 提交

## 关联

- **上游 openspec**: `2026-09-20-cpptlm-pcie-display-io-mvp`（D1，已建立路由框架；v1.1.1 引入 `display_routing_enabled_` flag 模板）
- **后续 openspec**:
  - D3: `2026-09-XX-cpptlm-pcie-gmmu-poc`（GMMU PoC，D1+D2 之后启动）
- **跨仓文档**（已迁 ArchForge，本次零依赖）：
  - `ArchForge/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`
  - `ArchForge/docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`

---

**作者**: CppTLM Team (Sisyphus)
**v1.1 修订依据**: Oracle session `ses_f342ea637ffeDWYBBEQDUUE63U`（D1 实施后复审，含潜在问题评估）
**D1 v1.1.1 复审 session**: `ses_f3620ca4effe0HziEXTIIexKkP`（实施 v1.1.1 修订触发）
