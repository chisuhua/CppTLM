# Tasks: Display IO 设备 MVP（v1.1 返工版）

> **配套**: [proposal.md](proposal.md) · [design.md](design.md) · [specs/display-io-mvp/spec.md](specs/display-io-mvp/spec.md)
> **v1.0 → v1.1 变化**: Oracle+Metis 审查指出 v1.0 的 7 步任务含 4 处事实错误（路径/API/ABI）。v1.1 简化为 5 步：T0 characterization → T1 device → T2 EP 注入 → T3 board 路由 → T4 测试 → T5 docs。
> **方法**: TDD 5-step（先写 regression characterization test，再 GREEN）

## Step 0: Characterization Test（**先写**，防回归）

### T0.1 RED — 锁定当前 board 行为
- 文件: `test/test_pcie_board_routing_characterization.cc`
- 测试 1: 不接 SOC 的 DGpuBoard → mmio_read/write 走 `mmio_regs_` map 路径（**不应**调用 device）
- 测试 2: DGpuBoard::backdoor_read miss → 返 -ENOENT（修复 #6 已存在行为）
- 测试 3: cpptlm_emulator_pcie_config_read 未接 EP → 返 -ENOSYS
- 目的: D1 改动后这些行为**不变**（仅在 device 存在时新增路径）
- **预期**: 全 PASS（**先**确认当前行为，再做 D1 改动）

### T0.2 VALIDATE
```bash
./build/bin/cpptlm_tests "test_pcie_board_routing_characterization" --reporter compact
# 期望: PASS（全部 case）
```

## Step 1: PcieDisplayDevice 骨架

### T1.1 RED — 写失败的单元测试
- 文件: `test/test_pcie_display_device_basic.cc`
- 测试名: `"PcieDisplayDevice BAR 0 寄存器 round-trip"`
- 断言:
  ```cpp
  PcieDisplayDevice dev;
  uint32_t mode = 2;
  dev.mmio_write(kRegDisplayControl, &mode, sizeof(mode));
  uint32_t read_val = 0;
  dev.mmio_read(kRegDisplayControl, &read_val, sizeof(read_val));
  REQUIRE(read_val == mode);
  ```
- 预期: FAIL（class 尚未实现）

### T1.2 GREEN — 最小实现
- 新文件: `include/tlm/gpu/pcie_display_device.hh` + `src/tlm/gpu/pcie_display_device.cc`
- namespace `tlm::gpu`（与 PcieBarRouter 一致）
- 字段:
  - `std::array<uint8_t, kRegSize> registers_{}` (4KB)
  - `std::vector<uint8_t> framebuffer_` (32MB, lazy alloc)
  - `uint64_t cycle_counter_ = 0`, `uint32_t vblank_count_ = 0`
- 方法:
  - `mmio_read(offset, buf, len)` / `mmio_write(offset, buf, len)` — 1/2/4 字节 + unaligned
  - `backdoor_read(offset, buf, len)` / `backdoor_write(offset, buf, len)`
  - `tick(MsiXTable& msix)` — VBLANK 推进
- 寄存器布局常量: `kRegDeviceIdentity`/`kRegDisplayControl`/`kRegFramebufferInfo`/`kRegStatus`/`kRegStatusClear`/`kRegInterruptMask`/`kRegInterruptStatus`/`kRegScratch`

### T1.3 COVERAGE — 边界测试
- 边界 offset（0xFFF）、unaligned access（1/2/4 字节混合）
- 超出 BAR 0 范围（offset+len > 4096）返 -EINVAL
- 写 RO 寄存器（kRegDeviceIdentity）静默忽略
- 目标覆盖率 ≥ 80%

### T1.4 VALIDATE
```bash
./build/bin/cpptlm_tests "test_pcie_display_device_basic" --reporter compact
```

## Step 2: PcieEndpointIP 注入 display_device

### T2.1 RED — 测试 EP 持有 + tick() 推进
- 文件: `test/test_pcie_display_device_vblank.cc`
- 测试: 实例化 EP，verify `ep->has_display_device() == true`
- 测试: tick 1024 次 → `display_device_->vblank_count() == 1`
- 测试: tick 10240 次 → `display_device_->vblank_count() == 10`

### T2.2 GREEN — 修改 pcie_endpoint_ip
- 修改 `include/tlm/pcie/pcie_endpoint_ip.hh`:
  - 添加 `display_device_` 成员: `std::unique_ptr<tlm::gpu::PcieDisplayDevice>`
  - 添加 accessor: `has_display_device()` / `display_device()`
- 修改 `src/tlm/pcie/pcie_endpoint_ip.cc`:
  - 构造函数: `display_device_(std::make_unique<tlm::gpu::PcieDisplayDevice>())`
  - `tick()` 末尾: `if (display_device_) display_device_->tick(msix());`

### T2.3 COVERAGE
- 多 tick 周期（0, 1024, 2048, 10239）边界
- mask=0 时不触发 MSI-X
- mask=1 时 MSI-X vector 0 pending=1

## Step 3: DGpuBoard 路由层

### T3.1 RED — 路由测试
- 在 `test_pcie_display_device_basic.cc` 增加:
  ```cpp
  TEST_CASE("DGpuBoard 通过 EP 路由 BAR 0 到 PcieDisplayDevice") {
    auto* board = create_board_with_display_device();
    cpptlm_emulator_t* emu = wrap(board);
    uint32_t mode = 1;
    cpptlm_emulator_mmio_write(emu, 0, kRegDisplayControl, &mode, 4);
    uint32_t read_val = 0;
    cpptlm_emulator_mmio_read(emu, 0, kRegDisplayControl, &read_val, 4);
    REQUIRE(read_val == mode);  // 真设备状态，非 shell-local
  }
  ```

### T3.2 GREEN — 修改 dgpu_board_shell
- 修改 `src/tlm/gpu/dgpu_board_shell.cc`:
  - `mmio_read`: 加 if-else 分支：`if (bar == 0 && ep && ep->has_display_device())` → 路由到 `ep->display_device()->mmio_read()`
  - `mmio_write`: 同上
  - `backdoor_read/write`: 加 if-else 分支 → 路由到 `ep->display_device()->backdoor_read/write()`
- 保留原 `mmio_regs_` / `vram_segments_` 作为 fallback（无 device 时）

### T3.3 COVERAGE
- 不接 SOC（soc_ == nullptr）→ 走原 fallback 路径（**由 T0 锁定**）
- 接 SOC 但无 display_device → 走原 fallback 路径
- 接 SOC + display_device → 新路由路径
- BAR 2-5（未映射）→ 返 -EINVAL

## Step 4: E2E + ABI 冻结验证

### T4.1 E2E 测试
- 文件: `test/test_pcie_display_device_e2e.cc`
- 标签: `[pcie][display][e2e]`
- 流程（per design §10）:
  1. create emulator with display device config
  2. config read vendor_id/device_id
  3. mmio write DISPLAY_MODE=1, read back → 1
  4. backdoor write framebuffer data, read back → same
  5. tick 1024 cycles
  6. msix check vector 0 pending

### T4.2 ABI 冻结验证
```bash
# D1 实施前后必须验证
git diff HEAD -- include/abi/cpptlm_emulator.h
# 期望: 空输出

# 函数签名计数
grep -c "^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" \
  include/abi/cpptlm_emulator.h
# 期望: 15（不变）
```

### T4.3 回归基线
```bash
# 全部 1480 cases 仍全绿
./build/bin/cpptlm_tests --reporter compact | tail -3
# 期望: 1480 cases / 66584+ assertions（+5 新增）

# docs_sync_check
./scripts/test/docs_sync_check.sh --strict
# 期望: PASS

# openspec validate
openspec validate --changes --strict
# 期望: 5/5 PASS
```

## Step 5: 文档 + AGENTS.md + 提交

### T5.1 CppTLM 仓内 docs
- 新文件: `docs/pcie/display-device-mvp.md`
- 内容:
  - 寄存器布局 + ABI 路由关系
  - D1 与 D2/D3 边界
  - UsrLinuxEmu 端验证方法（启动命令 + 预期输出）

### T5.2 AGENTS.md 更新
- "First read" 节: 添加 D1 实现文档链接
- "WHERE TO LOOK" 表: 添加 `[display]` 标签
- 不破坏 `25c469ec` 加入的"已迁 ArchForge"标注

### T5.3 提交策略
```bash
# 6 个独立 commit
git commit -m "test(characterization): 锁定 DGpuBoard 当前路由语义防 D1 回归"
git commit -m "feat(pcie): 新增 PcieDisplayDevice 类（4KB 寄存器 + 32MB FB + VBLANK）"
git commit -m "feat(pcie): PcieEndpointIP 注入 display_device + tick() 推进 VBLANK"
git commit -m "refactor(board): DGpuBoard 路由 BAR 0/1 到 PcieDisplayDevice 替代 shell-local"
git commit -m "test(pcie): D1 单元测试 + E2E（basic + backdoor + vblank + e2e + characterization）"
git commit -m "docs(pcie): 新增 display-device-mvp.md + AGENTS.md 更新"
```

## 工时统计（v1.1）

| Task | 估时 | 累计 |
|------|------|------|
| T0 characterization | 2h | 2h |
| T1 device 骨架 | 4h | 6h |
| T2 EP 注入 | 4h | 10h |
| T3 board 路由 | 3h | 13h |
| T4 E2E + ABI 验证 | 4h | 17h |
| T5 docs + commits | 3h | 20h |
| **合计** | **~20h ≈ 3.5 工作日** | |

（v1.0 估算 26h ≈ 3.3 天，v1.1 减至 20h ≈ 3.5 天，差异在 characterization 多 + 单元测试更聚焦）

## 验证清单

- [ ] 5 个新增 test cases 全绿
- [ ] 1480 现有 test cases 仍全绿
- [ ] openspec validate --changes --strict PASS（5/5）
- [ ] docs_sync_check --strict PASS
- [ ] `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空
- [ ] 15 ABI 函数签名不变（grep 计数 = 15）
- [ ] 4 callback typedef 不变
- [ ] `include/tlm/gpu/pcie_endpoint_tlm.h` 冻结头未触碰
- [ ] ARCHIVE: 无 `.disabled` 测试新增
- [ ] ARCHIVE: 无 TODO 残留

## 不在 D1 范围（deferred）

| 项 | 后置阶段 |
|----|---------|
| 真实显示输出 | 不实现 |
| Cursor / Overlay | 不实现 |
| HDMI / DP PHY | 不实现 |
| SDMA / SM 集成 | D2 |
| GMMU PoC | D3 |
| Power Management | 不实现 |
| 多 VRAM backing 共享 | 不实现 |

## v1.0 → v1.1 主要修正

| v1.0 | v1.1 |
|------|------|
| 7 步任务 | 5 步（含 characterization） |
| 6 个"伪 ABI 修复测试"（不存在 backdoor ABI） | 0 个（走 board 内部） |
| `test_pcie_abifix_config_read.cc` 等 | 并入 e2e 测试 |
| `src/core/cpptlm_emulator_api.cc`（不存在） | 不修改（ABI wrapper 已正确） |
| `include/tlm/pcie/pcie_bar_router_mvp.hh` | `include/tlm/gpu/pcie_bar_router_mvp.hh` |
| `include/tlm/pcie/pcie_display_device.hh` | `include/tlm/gpu/pcie_display_device.hh` |
| `ep->bar_router_->dispatch_read(...)`（不存在） | `ep->display_device()->mmio_read(...)` |
| `PcieEndpointIP::pcie_config_read/write`（不存在） | `ep->config_space().read/write` |