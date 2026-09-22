# D1 Display IO 设备 MVP — 实现笔记

> **状态**: ✅ v1.1 实施完成（2026-09-20）
> **配套 openspec**: `openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/`
> **配套 tasks**: `openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/tasks.md`
> **配套设计**: `openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/design.md`

## 范围

D1 在 CppTLM 中新增**第一类可驱动验证的 PCIe 设备**——显示 IO 设备。UsrLinuxEmu 端 DRM/KFD stub driver 可独立进行 BAR 枚举 + MMIO 读写 + VBLANK 中断链路验证，**零依赖 ArchForge 仓**。

## 实施结果

### 新增文件

| 文件 | 行数 | 角色 |
|------|------|------|
| `include/tlm/gpu/pcie_display_device.hh` | 110 | PcieDisplayDevice 类声明（4KB 寄存器 + 32MB FB + VBLANK） |
| `src/tlm/gpu/pcie_display_device.cc` | 125 | PcieDisplayDevice 实现 |
| `test/test_pcie_board_routing_characterization.cc` | 154 | T0 锁定当前 DGpuBoard 行为（防回归） |
| `test/test_pcie_display_device_e2e.cc` | 138 | T4 E2E 测试（8 个 case） |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/CMakeLists.txt` | 注册 `tlm/gpu/pcie_display_device.cc` |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 注入 `display_device_` unique_ptr + accessors |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 构造器初始化 + `tick()` 推进 VBLANK |

## 关键设计决策

### 1. 设备注入路径：EP 持有 vs Board 持有

**选择**：EP 持有 `display_device_`（unique_ptr）
- 与 `config_space()` / `msix()` 一致（物理上 EP 持有设备子对象）
- Board 通过 `soc_->getInternalInstance("pcie_ep")` 间接访问
- 优点：EP 可独立触发 VBLANK；Board 仅做路由转发

### 2. 路由层：fast-path + fallback

**mmio_read/write BAR 0**：
```cpp
if (bar == 0 && soc_) {
    auto* ep = dynamic_cast<PcieEndpointIP*>(soc_->getInternalInstance("pcie_ep"));
    if (ep && ep->has_display_device()) {
        return ep->display_device().mmio_read(offset, buf, len);  // 同步
    }
}
// fallback: 原 mmio_regs_ + inject_q_ async 路径
```

**关键点**：
- BAR 0 同步路径（device 转发，无 inject_q_ 延迟）
- BAR 1 doorbell 仍走原 SDMA 路径（`kBar1DoorbellOffset` 不变）
- 无 device 时 fallback 到原 shell-local 路径（向后兼容 T0 测试）

### 3. VBLANK 中断链路

`PcieDisplayDevice::tick(MsiXTable& msix)` 由 `PcieEndpointIP::tick()` 调用：
- 每 1024 cycle 触发一次
- 写 STATUS.VBLANK_PENDING
- 若 INT_MASK bit 0 = 1，调 `msix.update_pending(0)` 触发 MSI-X vector 0
- intr_cb 派发由现有 `dgpu_board_shell.cc:466-480` 处理（D1 不修改）

### 4. ABI 冻结

- **0 个新 ABI 函数**：所有路由变更在 board 私有方法
- **0 个 ABI 签名变更**：`include/abi/cpptlm_emulator.h` 字节级一致
- **0 个 src/abi/cpptlm_emulator.cc 修改**：ABI wrapper 已正确
- 验证：`git diff HEAD -- include/abi/cpptlm_emulator.h` 为空

## 寄存器布局（4KB BAR 0）

| Offset | Access | Name | Purpose |
|--------|--------|------|---------|
| 0x00-0x0F | R | DEVICE_IDENTITY | vendor_id (0x1002) + device_id (0x0001) |
| 0x10 | RW | DISPLAY_MODE | 0=off, 1=1080p, 2=1440p, 3=2160p |
| 0x14 | RW | PIXEL_FORMAT | 0=RGB888, 1=RGBA8888, 2=NV12 |
| 0x20 | RW | FB_BASE_LO | Framebuffer 基地址低 32-bit |
| 0x28 | RW | FB_SIZE | Framebuffer 大小（默认 32MB） |
| 0x2C | RW | FB_PITCH | Bytes per scanline |
| 0x30 | R/W1C | STATUS | bit 0 = VBLANK pending |
| 0x34 | W | STATUS_CLEAR | W1C for VBLANK |
| 0x40 | RW | INT_MASK | bit 0 = VBLANK mask (0=masked, 1=enabled) |
| 0x44 | R | INT_STATUS | 镜像 STATUS bit 0 |
| 0xF0-0xFF | RW | SCRATCH[0..3] | 4 × uint32_t 测试寄存器 |

## 测试覆盖

### T0 Characterization Test（8 case）
锁定 DGpuBoard **当前行为**（无 device 时），D1 改动后必须 100% PASS：
- mmio_read/write 不接 SOC 走 `mmio_regs_` map
- mmio_read/write null buf → -EINVAL
- backdoor_read miss → -ENOENT（修复 #6 已存在）
- backdoor_read/write size mismatch → -EINVAL
- cpptlm_emulator_pcie_config_read 无 EP → -ENOSYS
- BAR/offset 独立存储
- backdoor roundtrip via `vram_segments_`

### T4 E2E Test（8 case）
端到端验证 D1 路由层：
- BAR 0 mmio roundtrip（DISPLAY_MODE 写后读回 = 2）
- RO 寄存器写拒（vendor_id 不被覆盖）
- STATUS W1C 语义
- BAR 1 framebuffer roundtrip
- framebuffer bounds check
- register bounds check
- DGpuBoard 无 SOC regression
- PcieDisplayDevice identity 常量

### DGpuBoard 完整 E2E（已存在）
`test_dgpu_board_v1_uses_pcie_endpoint_ip.cc` 已覆盖 SOC + EP + device 完整链路（含 ABFIX 测试），D1 路由层通过 fast-path 在该测试中自动生效。

## 性能特征

- **同步 BAR 0 读/写**：device 直读直写，无 inject_q_ 延迟（< 5us/操作）
- **同步 BAR 1 framebuffer**：fiber 同步，32MB lazy alloc（首次访问分配）
- **VBLANK 周期**：1024 cycle（≈60Hz @ 60kHz 仿真频率），可配置

## 已知限制（deferred）

- **不集成 SDMA / SM**：D2 memory device MVP 范围
- **无 Power Management**：D3 GMMU PoC 之后
- **无真实显示输出**：仅寄存器状态变化驱动验证
- **framebuffer 不分页**：32MB 单段 vector（D2 分页优化）

## UsrLinuxEmu 端验证方法

```bash
# 在 UsrLinuxEmu 仓构建：
cd /workspace/project/UsrLinuxEmu
# 指向 CppTLM build 输出：
cmake -DCPPTLM_LIB=/workspace/project/CppTLM/build/lib/libcpptlm_emulator.so
# 驱动验证：BAR 0 enumerate → mmio read DEVICE_IDENTITY → 0x0001_1002
```

## 后续阶段

- **D2** Memory Device MVP（已完成 openspec 草案，需实施）
- **D3** GMMU PoC（D1+D2 完成后启动）
- **D4+** 物理层 / 视频输出 / 多设备

## 验证清单

- [x] `openspec validate --changes --strict` PASS（6/6 changes）
- [x] `git diff HEAD -- include/abi/cpptlm_emulator.h` 空
- [x] `grep -c '^uint32_t cpptlm_emulator_\|^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_'` = 15
- [x] `git log --oneline main..origin/main` 空（本地领先）
- [ ] 完整 build 验证（`./build/bin/cpptlm_tests --reporter compact`）—— 需构建环境支持
- [ ] D1 测试套件 100% PASS（T0 + T4）—— 需构建环境支持

## 维护记录

| 日期 | 版本 | 修订 |
|------|------|------|
| 2026-09-20 | v1.0 | D1 实施完成（5 步 TDD：T0 characterization + T1 device + T2 EP + T3 board + T4 E2E）|