# 2026-09-20-cpptlm-pcie-display-io-mvp: Display IO 设备 MVP

> **状态**: 📋 Proposed — 2026-09-20
> **优先级**: 🟡 P1（驱动验证驱动链）
> **工期**: 1 周
> **目标**: 在 CppTLM dGPU SoC 中实现**显示 IO 设备**（Display Engine MVP），让 UsrLinuxEmu 端 GPU 驱动（DRM/KFD 子集）**不依赖 ArchForge 仓**即可进行 BAR 枚举 + MMIO 读写 + 中断处理端到端验证。

## Why

CppTLM 当前 dGPU PCIe EP（Phase 1-8 全链路交付）已经能枚举 + 收发 TLP，但 **事务层语义**有 7 个 NO-OP/ENOSYS 根本性错误（per `docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md` §0.1 Oracle 三轮审查）：

| # | 错误 | 实际行为 | 影响 |
|---|------|---------|------|
| 1 | `cpptlm_emulator_register_dma_translate_cb` | `(void)cb` NO-OP | cb 永不入 DGpuBoard |
| 2 | `cpptlm_emulator_register_dma_translate_cb`（1.3c 修复后） | 硬编码 `return 0` (pa=0) | pa ≠ identity |
| 3 | `cpptlm_emulator_pcie_config_read/write` | `return -ENOSYS` | 死路 |
| 4 | `cpptlm_emulator_msix_update_pending` | intr_cb 接线但 `trigger_irq_async` 无调用方 | 中断链断裂 |
| 5 | `cpptlm_emulator_mmio_read` | ret=0 但 buf 未填充 | 数据缺口 |
| 6 | `cpptlm_emulator_backdoor_read` | 未命中 vram_segments_ 返 `len` | 伪装成功 |
| 7 | `cpptlm_emulator_mmio_write` | 立即返 0，数据丢弃 | 写入假成功 |

**核心问题**：驱动端"接线真实但语义空转"——`ret != -ENOSYS` 验证通过但实际数据流未发生。

**驱动验证现状**：
- UsrLinuxEmu `sim_hardware/include/cpptlm/bridge.h` 已封装 ABI（mmio_read/write/config_read/msix_update_pending）
- UsrLinuxEmu 端 DRM/KFD stub driver 可发起 BAR 枚举，但 CppTLM 端读不出真数据 → 驱动永远拿到空数据 → 无法验证

**新增动机**：用户决策（2026-09-20）：
> "我希望 CppTLM 可以有完整的 PCIe 设备（显示 IO 设备，后面再扩展 memory，也许还需要一个证明 GMMU 概念的 PoC 设计），可以让 UsrLinuxEmu 端的驱动可以不依赖 ArchForge 项目进行验证"

**预期收益**：
1. 修复 #3、#5、#6、#7（Config + MMIO + Backdoor 真实化）—— D1 范围
2. 让 UsrLinuxEmu 端 DRM stub driver 完成 **BAR 0 寄存器读写验证** + **VBLANK 中断接收验证**
3. 建立"显示设备"作为 dGPU 的**第一类可驱动验证设备**
4. **不依赖 ArchForge 仓**（设计文档已迁；驱动验证逻辑上完全自洽于 CppTLM）

## What Changes

### §1 D1 范围：显示 IO 设备 MVP

**功能边界**（最小可行）：
- 一个 PCIe 显示设备（`PcieDisplayDevice`），**BAR 0** 暴露 4KB MMIO 寄存器空间
- 寄存器布局：
  - `0x00-0x0F` Device Identity（vendor_id/device_id/revision 镜像）
  - `0x10-0x1F` Display Control（mode/resolution/format）
  - `0x20-0x2F` Framebuffer Info（base address + size + pitch）
  - `0x30-0x3F` Status / Clear (pending)
  - `0x40-0x4F` Interrupt Mask / Status
  - `0xF0-0xFF` Scratch / Mailbox（往返验证用）
- **MSI-X** 1 vector（VBLANK 中断，模拟扫描输出帧切换）
- **BAR 1** 32MB Framebuffer（模拟 VRAM backing store）
- **NO 真实显示输出**（无 scan-out、无像素渲染、无 cursor）—— 寄存器状态变化即可触发驱动验证
- **NO 真实 GPU 计算**（不集成 SDMA/SM）—— D2（memory device）+ D3（GMMU PoC）才接

### §2 必须修复的 NO-OP（事务层真实化）

| ABI 函数 | D1 修复 | 实现路径 |
|----------|---------|----------|
| `cpptlm_emulator_pcie_config_read/write` | ✅ 真实读写 config space | 通过 `PcieEndpointIP::pcie_config_read/write` |
| `cpptlm_emulator_mmio_read/write` | ✅ 路由至显示设备 BAR 0 | 通过 `PcieEndpointIP::bar_router_->dispatch(bar, offset, len)` |
| `cpptlm_emulator_backdoor_read/write` | ✅ 真实访问 backing store | 注入 `PcieDisplayDevice` 内部 VRAM 数组 |
| `cpptlm_emulator_msix_update_pending` | ✅ 真实调用 `trigger_irq_async` | D1 不修（D2/D3 处理中断链） |

### §3 文件清单

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/tlm/pcie/pcie_display_device.hh` | **新** | `PcieDisplayDevice` 类声明（BAR 0 寄存器 + framebuffer + MSI-X VBLANK） |
| `src/tlm/pcie/pcie_display_device.cc` | **新** | PcieDisplayDevice 实现 |
| `include/tlm/pcie/pcie_bar_router_mvp.hh` | **改** | 扩展 BAR 0/1 路由到显示设备 |
| `src/tlm/pcie/pcie_display_device_router.cc` | **新** | BAR 0 寄存器 read/write 路由入口 |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | **改** | 注入 `PcieDisplayDevice` 实例 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | **改** | `tick()` 推进 VBLANK counter + 触发 MSI-X（每 1024 cycle 一次） |
| `src/core/module_factory.cc` | **改** | 注册 `PcieDisplayDevice`（ChStream 类） |
| `include/chstream_register.hh` | **改** | `REGISTER_CHSTREAM(PcieDisplayDevice)` 宏入口 |
| `configs/example/pcie-display-device-v1.json` | **新** | D1 拓扑 |
| `test/test_pcie_display_device_basic.cc` | **新** | 单元测试 1：BAR 0 寄存器 round-trip |
| `test/test_pcie_display_device_mmio.cc` | **新** | 单元测试 2：MMIO 真实数据路径（修 #5/#7） |
| `test/test_pcie_display_device_config.cc` | **新** | 单元测试 3：Config space 真实访问（修 #3） |
| `test/test_pcie_display_device_msix.cc` | **新** | 单元测试 4：MSI-X VBLANK 触发 + intr_cb 接收 |
| `test/test_pcie_display_device_e2e.cc` | **新** | E2E 测试 5：Config → BAR enumerate → MMIO round-trip → MSI-X VBLANK |
| `test/test_pcie_abifix_*.cc`（4 个）| **新** | ABI 修复测试 |
| `test/CMakeLists.txt` | **改** | 注册新增 9 个 test_*.cc 文件 |

**冻结面零触碰**：
- `include/abi/cpptlm_emulator.h`（22 ABI 签名不变）
- `include/tlm/gpu/pcie_endpoint_tlm.h`（PcieEndpointTLM 4 端口冻结）

### §4 ABI 影响

- ✅ **0 个新 ABI 函数**（per ADR-088 §D5）
- ✅ **0 个 ABI 签名变更**
- ✅ D1 修复的所有 NO-OP 都是**修正实现 bug**，不是 ABI 变更
- ✅ UsrLinuxEmu 端 `bridge.h` 不需要改动（已对齐 ABI）

### §5 不在 D1 范围（后续 D2/D3）

| 项 | 后续阶段 | 原因 |
|----|----------|------|
| 真实显示输出（像素渲染） | 不实现（无意义） | 软件仿真的"显示"无法显示 |
| Cursor / Overlay | 不实现 | 非 MVP |
| HDMI / DP PHY | 不实现 | 物理层 |
| SDMA / SM 集成 | D2 (memory device) | 显示设备不需要 |
| GMMU PoC | D3（D1+D2 后） | 显示设备用 identity mapping 即可 |
| Power Management（D-states） | 不实现 | 5.5.7+ 范围 |
| 多 VRAM backing 共享 | 不实现 | 单设备 MVP |
| 跨仓 ArchForge 引用 | **零** | 驱动验证不读设计文档 |

### §6 验收标准（DoD）

| 项 | 标准 |
|----|------|
| **功能** | UsrLinuxEmu 端 DRM stub driver 能完成完整 BAR 枚举（vendor_id/device_id）+ VBLANK 中断接收（至少 1 次） |
| **NO-OP 修复** | `cpptlm_emulator_pcie_config_read/write` / `mmio_read/write` / `backdoor_read/write` 在 D1 范围内不再返回 -ENOSYS 或丢弃数据 |
| **回归基线** | 现有 1480 cases / 66584 assertions 仍全绿；新增 9 个 test cases |
| **端到端** | test_pcie_display_device_e2e 覆盖 Config → BAR enumerate → MMIO round-trip → MSI-X VBLANK |
| **ABI 冻结** | 22 ABI 签名不变 |
| **跨仓独立** | UsrLinuxEmu 端构建 + 跑 E2E 测试**不**需要 clone ArchForge |
| **文档同步** | 新增 `docs/pcie/display-device-mvp.md`（CppTLM 仓内），ArchForge 不需要（这是实现级文档） |

### §7 时间线

| Day | 工作量 |
|-----|--------|
| Day 1-2 | PcieDisplayDevice 实现 + BAR 路由（4500 行） |
| Day 3 | ABI 修复（mmio + config + backdoor 真实化） |
| Day 4 | MSI-X VBLANK 集成 + tick() 推进 |
| Day 5 | 5 个测试用例编写 + 4 个 ABI 修复测试 |
| Day 6 | E2E 测试 + 拓扑配置 + docs |
| **合计** | **5-6 工作日** |

## Impact

### Who is affected

- **UsrLinuxEmu 端**：DRM/KFD stub driver 获得真实 BAR 0 寄存器响应，可验证 VBLANK 中断链路
- **CppTLM 端**：PcieEndpointIP 增加显示设备子模块（向后兼容，可选启用）
- **现有测试**：1480 cases 不受影响（仅新增）

### Dependencies

- **依赖**：
  - `PcieEndpointIP`（Phase 4 已交付，✅）
  - `pcie_bar_router_mvp`（MVP 阶段，✅，需扩展）
  - `msix_table_mvp`（✅，已对齐 UsrLinuxEmu MSIX_DEFAULT_VECTORS）
  - `dgpu_board_shell`（✅，可选集成）
  - `host_bypass_tlm`（✅，Phase 7 已交付，支持 E2E 测试）
- **被依赖**：D2 (memory device) + D3 (GMMU PoC) 都将基于 PcieDisplayDevice 的 BAR 路由框架
- **不依赖 ArchForge**：✅（设计文档全部迁出后，驱动验证逻辑上完全自洽）

### Risks

- **风险 1**：BAR 路由扩展可能触及冻结头 `pcie_endpoint_tlm.h`？—— **不会**，仅修改 `.cc` 实现 + 新增 `.hh` 子模块
- **风险 2**：ABI 修复可能引入回归？—— 仅 D1 范围内 4 个 NO-OP（Config/MMIO/Backdoor），其他 ABI 不动
- **风险 3**：VBLANK 中断频率选择（1024 cycle）可能不合适？—— D1 是 MVP，60Hz 等价物即可；后续 D2/D3 调整
- **风险 4**：UsrLinuxEmu 端 DRM driver 期望 BAR 0 寄存器有 GPU 寄存器语义（不只 display）？—— D1 范围明确为 display-only；GPU 寄存器放 D2/D3 一起

## Tasks 概要（详细见 `tasks.md`）

- T1: PcieDisplayDevice 骨架（class + BAR 0/1 内存布局）
- T2: PcieEndpointIP 注入（tick() 推进 + MSI-X 触发）
- T3: BAR 路由器扩展（BAR 0 → PcieDisplayDevice）
- T4: ABI 修复（Config + MMIO + Backdoor 真实化）
- T5: MSI-X VBLANK 中断链
- T6: 5 个测试用例 + 4 个 ABI 修复测试
- T7: E2E 测试 + 拓扑 + docs

## 关联

- **上游 openspec**: `2026-09-19-cpptlm-pcie-ep-foundation`（基础必备 + 性能增强 + ABI 修复）
- **后续 openspec**:
  - D2: `2026-09-XX-cpptlm-pcie-memory-device-mvp`（memory device MVP）
  - D3: `2026-09-XX-cpptlm-pcie-gmmu-poc`（GMMU PoC，D1+D2 之后启动）
- **跨仓文档**（已迁 ArchForge，本次零依赖）：
  - `ArchForge/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`
  - `ArchForge/docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`

---

**作者**: CppTLM Team (Sisyphus) · **审查**: 待 Oracle (D3 启动时一并审查 D1+D2+D3)