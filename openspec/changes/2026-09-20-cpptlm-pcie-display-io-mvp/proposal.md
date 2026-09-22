# 2026-09-20-cpptlm-pcie-display-io-mvp: Display IO 设备 MVP

> **状态**: 📋 Proposed (reworked) — 2026-09-20
> **v1.1 返工**: Oracle `ses_f37ee373fffeFvaUmM12lCFZ0O` + Metis `ses_f37ee3587ffexJ089actUHbn4c` 审查后
> **优先级**: 🟡 P1（驱动验证驱动链）
> **工期**: 4-5 工作日
> **目标**: 在 CppTLM dGPU SoC 中实现**显示 IO 设备**（PcieDisplayDevice），让 UsrLinuxEmu 端 GPU 驱动（DRM/KFD 子集）**不依赖 ArchForge 仓**即可进行 BAR 枚举 + MMIO 读写 + 中断处理端到端验证。

## Why

### 当前状态（Oracle 2026-09-20 审查后的事实基线）

CppTLM 当前 dGPU PCIe EP（Phase 1-8 全链路交付）已具备完整的链路层 + 传输层 + 配置空间 + MSI-X 基础设施。原 v1.0 提案声称的"7 个 NO-OP 错误"在 Oracle 审查时被证实**多数已修复**：

| 原 v1.0 声称 | 当前代码事实（2026-09-20 验证） |
|------------|----------------------------------|
| `cpptlm_emulator_pcie_config_read/write` 返 -ENOSYS | **已修** — `dgpu_board_shell.cc:350-371` 路由到 `ep->config_space().read/write` |
| `cpptlm_emulator_mmio_read/write` 数据缺口 | **已修** — `dgpu_board_shell.cc:223-348` 通过 BAR-keyed `mmio_regs_` map + self-drain 真实回填 buf |
| `cpptlm_emulator_backdoor_read` 伪装返 len | **已修** — miss 返 `-ENOENT`（line 401-411，注释自承"不再伪装成功"） |
| `cpptlm_emulator_msix_update_pending` 中断链断裂 | **已修** — `dgpu_board_shell.cc:466-480` 调 `trigger_irq_async`（含 coalescing） |

### 真正的剩余缺口（v1.1 重新定位）

虽然 ABI 层和 DGpuBoard 内部分派都已修复，但 **所有路径仍停留在 DGpuBoard 的 shell-local 存储**：
- `mmio_regs_[(bar,offset)]` — in-memory map，未连接到任何真实硬件
- `vram_segments_[offset]` — in-memory map，未连接到 VRAM
- `bar_router_` — 数据驱动的 BAR0 寄存器表，**只处理 doorbell side-effect**，不路由到显示设备

**结果**：UsrLinuxEmu 驱动发起 MMIO write→read round-trip 时，数据确实"流通"了，但只是 shell 内部 map 自洽——**没有任何真实设备状态在背后**。驱动看到的寄存器值是 shell 编造的，无法验证"驱动对真实硬件的假设"。

### 驱动验证现状

- UsrLinuxEmu `sim_hardware/include/cpptlm/bridge.h` 已封装 15 ABI（含 `mmio_read/write`/`config_read/write`/`msix_update_pending`）
- UsrLinuxEmu DRM/KFD stub driver 可发起 BAR 枚举 + MMIO write/read
- **但**：读到的值是 shell 编造的，与任何真实设备行为无关 → 驱动验证结论不可信

### 用户决策（2026-09-20）

> "我希望 CppTLM 可以有完整的 PCIe 设备（显示 IO 设备，后面再扩展 memory，也许还需要一个证明 GMMU 概念的 PoC 设计），可以让 UsrLinuxEmu 端的驱动可以不依赖 ArchForge 项目进行验证"

### 预期收益（v1.1 重新表述）

1. **新增 `PcieDisplayDevice`** — dGPU 第一类可驱动验证的设备（注册 4KB BAR0 寄存器 + 32MB BAR1 framebuffer + 1 个 MSI-X VBLANK vector）
2. **DGpuBoard 路由 BAR 0/1 → 真实设备** — 替代 shell-local 路径，让 MMIO 真正读写设备状态
3. **UsrLinuxEmu DRM/KFD stub driver 验证闭环** — vendor_id/device_id 来自真实 config space，VBLANK 中断来自真实 tick()
4. **零 ABI 新增** — 现有 15 ABI 函数签名零变更；UsrLinuxEmu 端 `bridge.h` 不变

## What Changes

### §1 D1 范围：显示 IO 设备 MVP

**功能边界**（最小可行）：
- 一个 PCIe 显示设备（`PcieDisplayDevice`），**BAR 0** 暴露 4KB MMIO 寄存器空间
- 寄存器布局：
  - `0x00-0x0F` Device Identity（vendor_id/device_id/revision 镜像，从 `ep->config_space()` 读）
  - `0x10-0x1F` Display Control（mode/resolution/format）
  - `0x20-0x2F` Framebuffer Info（base address + size + pitch）
  - `0x30-0x3F` Status / Clear (VBLANK pending, W1C)
  - `0x40-0x4F` Interrupt Mask / Status
  - `0xF0-0xFF` Scratch / Mailbox（驱动往返验证用）
- **MSI-X** 1 vector（VBLANK 中断，由 `PcieDisplayDevice::tick()` 推进）
- **BAR 1** 32MB Framebuffer（模拟 VRAM backing store）
- **NO 真实显示输出**（无 scan-out、无像素渲染、无 cursor）—— 寄存器状态变化即可触发驱动验证
- **NO 真实 GPU 计算**（不集成 SDMA/SM）—— D2（memory device）+ D3（GMMU PoC）才接
- **零 ABI 新增**：现有 15 ABI 函数 + 4 callback typedef 保持不变

### §2 必须修改的 DGpuBoard 路径（取代 v1.0 错误的"ABI 修复"）

| DGpuBoard 方法 | 当前行为（v1.0 之前） | D1 行为（v1.1 修复后） |
|---------------|---------------------|---------------------|
| `pcie_config_read/write` | 已路由 `ep->config_space()` | **不变**（D1 复用已有实现） |
| `mmio_read/write`（BAR 0） | shell-local `mmio_regs_` map | **路由到 `PcieDisplayDevice::mmio_read/write`**（保留 map 作为 fallback） |
| `mmio_read/write`（BAR 1） | shell-local `mmio_regs_` map | **路由到 `PcieDisplayDevice::framebuffer_` 数组** |
| `backdoor_read/write` | `vram_segments_` map | **改为路由到 `PcieDisplayDevice::framebuffer_`**（bar=1 + forward via board 内部调用，非 ABI） |
| `msix_update_pending` | 已调 `trigger_irq_async` | **不变**；D1 让 `PcieDisplayDevice` 主动调 `msix_->update_pending(0)` 触发 VBLANK |

**关键设计决策**：
- 不修改 `src/abi/cpptlm_emulator.cc`（ABI wrapper 已正确）
- 不新增任何 ABI 函数
- 所有路由变更在 `src/tlm/gpu/dgpu_board_shell.cc` 的私有方法中（不暴露新 ABI）
- `PcieDisplayDevice` 由 `PcieEndpointIP` 持有（通过 `tick()` 注入推进），`DGpuBoard` 通过 `soc_->getInternalInstance("pcie_ep")` 访问

### §3 文件清单（修正路径与命名）

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/tlm/gpu/pcie_display_device.hh` | **新** | `PcieDisplayDevice` 类声明（BAR 0 寄存器 4KB + BAR 1 framebuffer 32MB + VBLANK counter） |
| `src/tlm/gpu/pcie_display_device.cc` | **新** | PcieDisplayDevice 实现（mmio_read/write + backdoor_read/write + tick()） |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | **改** | 添加 `display_device()` accessor + 持有 `std::unique_ptr<PcieDisplayDevice>` 成员 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | **改** | `tick()` 调用 `display_device_->tick()` 推进 VBLANK；init 时构造 display_device |
| `src/tlm/gpu/dgpu_board_shell.cc` | **改** | `mmio_read/write` 增加 BAR 0/1 → display device 分支；`backdoor_read/write` 转发到 display framebuffer（**不**改 ABI wrapper） |
| `src/tlm/gpu/dgpu_board_shell.hh` | **改** | 持有 `PcieDisplayDevice*` 引用 + 访问 helper（可选，若通过 EP 间接访问可省） |
| `include/chstream_register.hh` | **改** | `REGISTER_CHSTREAM(PcieDisplayDevice)`（**不**注册为独立 module，而是 EP 子对象） |
| `test/test_pcie_display_device_basic.cc` | **新** | 单元测试 1：BAR 0 寄存器 round-trip（mmio_read/write） |
| `test/test_pcie_display_device_backdoor.cc` | **新** | 单元测试 2：BAR 1 framebuffer round-trip（backdoor_read/write） |
| `test/test_pcie_display_device_vblank.cc` | **新** | 单元测试 3：MSI-X VBLANK 触发（tick 推进 → pending=1） |
| `test/test_pcie_display_device_e2e.cc` | **新** | E2E 测试 4：Config → BAR enumerate → MMIO write/read → VBLANK MSI-X（端到端） |
| `test/test_pcie_board_routing_characterization.cc` | **新** | 回归测试 0（**T1 先写**）：锁定当前 board 路由行为，确保 D1 改动不引入回归 |
| `test/CMakeLists.txt` | **改** | 注册新增 5 个 test_*.cc 文件 |
| `docs/pcie/display-device-mvp.md` | **新** | CppTLM 仓内实现笔记（详见 §4） |
| `AGENTS.md` | **改** | "First read" 节添加 ARDA § 入口 + 标签 `[display]` 加入"WHERE TO LOOK" |

**冻结面零触碰**：
- ✅ `include/abi/cpptlm_emulator.h`（15 ABI 函数签名不变；D1 不新增）
- ✅ `include/tlm/gpu/pcie_endpoint_tlm.h`（PcieEndpointTLM 4 端口冻结）
- ✅ `src/abi/cpptlm_emulator.cc`（ABI wrapper 已正确，不修改）

### §4 ABI 影响（v1.1 精确表述）

- ✅ **0 个新 ABI 函数**（per ADR-088 §D5）
- ✅ **0 个 ABI 签名变更**（现有 15 函数 + 4 callback typedef 字节一致）
- ✅ 验证方法：`diff <(git show HEAD:include/abi/cpptlm_emulator.h) include/abi/cpptlm_emulator.h` 为空
- ✅ UsrLinuxEmu 端 `bridge.h` 不需要改动（已对齐 15 ABI）

### §5 不在 D1 范围（后续 D2/D3）

| 项 | 后续阶段 | 原因 |
|----|----------|------|
| 真实显示输出（像素渲染） | 不实现 | 软件仿真无显示器 |
| Cursor / Overlay | 不实现 | 非 MVP |
| HDMI / DP PHY | 不实现 | 物理层 |
| SDMA / SM 集成 | D2 (memory device) | 显示设备不需要 |
| GMMU PoC | D3（D1+D2 后） | 显示设备用 identity mapping |
| Power Management（D-states） | 不实现 | 5.5.7+ 范围 |
| 多 VRAM backing 共享 | 不实现 | 单设备 MVP |
| 跨仓 ArchForge 引用 | **零** | 驱动验证不读设计文档 |

### §6 验收标准（DoD）

| 项 | 标准 |
|----|------|
| **设备真实化** | `PcieDisplayDevice::mmio_read(0x10, buf, 4)` 在 mmio_write(0x10, val, 4) 后返 val（不是 shell 编造的 0xFFFFFFFF） |
| **ABI 冻结** | `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空 |
| **配置空间真实** | `cpptlm_emulator_pcie_config_read(emu, 0x00, 4, &val)` 返回 vendor_id=0x1002（来自 display device），不再是 -ENOSYS |
| **MMIO 路由** | 驱动 `mmio_write(emu, bar=0, off=0x10, &mode=1)` 后 `mmio_read(emu, bar=0, off=0x10, buf, 4)` 返 1（来自 device，不是 shell map） |
| **Backdoor 真实** | 驱动 `backdoor_read(emu, bar=1, off=0x1000, buf, len)` 后 framebuffer 数据持久 |
| **VBLANK 中断** | 1024 cycle tick 后 MSI-X vector 0 pending=1，可通过 `cpptlm_emulator_msix_update_pending(emu, 0)` 验证（device 内部触发，非外部注入） |
| **回归基线** | 现有 1480 cases / 66584 assertions 仍全绿；新增 5 个 test cases |
| **端到端** | E2E 测试覆盖：init → config read vendor_id → mmio enumerate → write/read round-trip → tick 1024 cycle → msix pending=1 |
| **跨仓独立** | UsrLinuxEmu 端构建 + E2E 测试**不**需要 clone ArchForge |
| **文档** | 新增 `docs/pcie/display-device-mvp.md` |

### §7 时间线（v1.1 修正）

| Day | 工作量 | 内容 |
|-----|--------|------|
| Day 1 | 4h | T1: PcieDisplayDevice 骨架（class + 4KB 寄存器 + 32MB framebuffer + VBLANK counter） |
| Day 1 | 2h | T2: Characterization test（**先写**锁定当前 board 行为，防回归） |
| Day 2 | 4h | T3: PcieEndpointIP 注入（unique_ptr + tick() 推进 + init 构造） |
| Day 2 | 3h | T4: DGpuBoard 路由层（mmio_read/write + backdoor_read/write 分支） |
| Day 3 | 4h | T5: 单元测试（basic + backdoor + VBLANK）+ VBLANK 中断触发 |
| Day 4 | 4h | T6: E2E 测试 + 拓扑配置 + ABI 冻结验证 |
| Day 5 | 3h | T7: docs/pcie/display-device-mvp.md + AGENTS.md 更新 + 提交 |
| **合计** | **~24h = 4 工作日** | |

（v1.1 比 v1.0 减少 1 天：删除"ABI 修复"步骤，简化 characterization tests）

## Impact

### Who is affected

- **UsrLinuxEmu 端**：DRM/KFD stub driver 获得真实 BAR 0 寄存器响应，可验证 VBLANK 中断链路
- **CppTLM 端**：PcieEndpointIP 增加显示设备子对象（向后兼容，可选启用）
- **现有测试**：1480 cases 不受影响（仅新增 5 个）

### Dependencies

**依赖**：
- `PcieEndpointIP`（Phase 4 已交付，✅）
- `MsiXTable`（`pcie_msix_per_vf_tlm.hh`，✅）
- `PcieConfigSpace`（`pcie_config_space_per_vf_tlm.hh`，✅）
- `DGpuBoard`（✅，T4 修改）
- `host_bypass_tlm`（Phase 7 已交付，✅，支持 E2E 测试）

**被依赖**：D2 (memory device) + D3 (GMMU PoC) 都将基于 PcieDisplayDevice 的 BAR 路由框架

**不依赖 ArchForge**：✅（设计文档全部迁出后，驱动验证逻辑上完全自洽）

### Risks

| 风险 | 等级 | 缓解策略 |
|------|------|---------|
| **R1**: T4 改 DGpuBoard mmio_read/write 可能影响现有 SDMA doorbell 路径（已存在的 `bar=1, offset=kBar1DoorbellOffset` 路由） | 🟡 中 | T2 characterization test 先锁现有行为；T4 用 if-else 优先 doorbell 路径 |
| **R2**: `PcieDisplayDevice` 由 EP 持有 vs 由 board 持有 —— 注入层次选择影响 ABI 访问路径 | 🟢 低 | 选 EP 持有（与 config_space/msix 一致），board 通过 `soc_->getInternalInstance("pcie_ep")` 间接访问 |
| **R3**: VBLANK 中断频率（1024 cycle ≈ 60Hz @ 60kHz）可能不合适 | 🟢 低 | D1 MVP 选合理默认；后续 D2/D3 调整 |
| **R4**: `PcieDisplayDevice` 通过 `unique_ptr` 持有，EP 析构顺序可能 double-free | 🟢 低 | EP 默认析构 + unique_ptr 自动管理；测试验证 |
| **R5**: BAR 1 framebuffer 32MB 在仿真启动时全部 0 初始化，可能影响测试时序 | 🟢 低 | 延迟分配（首次 write 时 lazy alloc）或显式 init 一次性 |

## Tasks 概要（详细见 `tasks.md`）

- T1: Characterization test（**先写**，防回归）
- T2: PcieDisplayDevice 骨架（class + BAR 0/1 内存布局 + VBLANK counter）
- T3: PcieEndpointIP 注入（unique_ptr + tick() 推进 + init 构造）
- T4: DGpuBoard 路由层（mmio_read/write BAR 0/1 → device + backdoor → framebuffer）
- T5: 单元测试（basic + backdoor + VBLANK 3 个）+ VBLANK 中断触发
- T6: E2E 测试 + 拓扑配置 + ABI 冻结验证
- T7: docs/pcie/display-device-mvp.md + AGENTS.md 更新 + 提交

## 关联

- **上游 openspec**: `2026-09-19-cpptlm-pcie-ep-foundation`（基础必备 + 性能增强）
- **后续 openspec**:
  - D2: `2026-09-XX-cpptlm-pcie-memory-device-mvp`（memory device MVP）
  - D3: `2026-09-XX-cpptlm-pcie-gmmu-poc`（GMMU PoC，D1+D2 之后启动）
- **跨仓文档**（已迁 ArchForge，本次零依赖）：
  - `ArchForge/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`
  - `ArchForge/docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`

---

**作者**: CppTLM Team (Sisyphus)
**v1.0 Oracle session**: `ses_f37ee373fffeFvaUmM12lCFZ0O`（BLOCKING 触发返工）
**v1.0 Metis session**: `ses_f37ee3587ffexJ089actUHbn4c`（BLOCKING 触发返工）
**v1.1 Oracle 二次审查**: 待 D1 实施后启动