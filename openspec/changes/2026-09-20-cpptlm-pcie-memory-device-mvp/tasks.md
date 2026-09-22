# Tasks: Memory 设备 MVP

> **配套**: [proposal.md](proposal.md) · [design.md](design.md) · [specs/memory-device-mvp/spec.md](specs/memory-device-mvp/spec.md)
> **方法**: TDD 5-step（先写 regression characterization test，再 GREEN）
> **工期**: 3-4 工作日（比 D1 短，因复用 D1 路由框架）

## Step 0: Characterization Test（**先写**，防回归）

### T0.1 RED — 锁定当前 board BAR 2 未映射行为
- 文件: `test/test_pcie_memory_device_routing_characterization.cc`
- 测试 1: 不接 SOC 的 DGpuBoard → BAR 2 mmio_read/write 返回 -EINVAL（BAR 2 未映射）
- 测试 2: 接 SOC 但无 memory_device → BAR 2 mmio_read/write 返回 -EINVAL
- 测试 3: backdoor_read BAR 2 offset → 走 shell-local vram_segments_ 路径
- 目的: D2 改动后这些行为**不变**（仅在 memory_device 存在时新增路由）
- **预期**: 全 PASS（**先**确认当前行为，再做 D2 改动）

### T0.2 VALIDATE
```bash
./build/bin/cpptlm_tests "test_pcie_memory_device_routing_characterization" --reporter compact
# 期望: PASS（全部 case）
```

## Step 1: PcieMemoryDevice 骨架

### T1.1 RED — 写失败的单元测试
- 文件: `test/test_pcie_memory_device_basic.cc`
- 测试名: `"PcieMemoryDevice BAR 0 寄存器 round-trip"`
- 断言:
  ```cpp
  PcieMemoryDevice dev;
  uint32_t size_lo = 0x00000000;
  dev.mmio_write(kRegMemSizeLo, &size_lo, sizeof(size_lo));
  uint32_t read_val = 0;
  dev.mmio_read(kRegMemSizeLo, &read_val, sizeof(read_val));
  REQUIRE(read_val == size_lo);
  ```
- 预期: FAIL（class 尚未实现）

### T1.2 GREEN — 最小实现
- 新文件: `include/tlm/gpu/pcie_memory_device.hh` + `src/tlm/gpu/pcie_memory_device.cc`
- namespace `tlm::gpu`（与 PcieDisplayDevice 一致）
- 字段:
  - `std::array<uint8_t, kRegSize> registers_{}` (4KB)
  - `std::vector<uint8_t> memory_backing_` (8GB, lazy alloc)
  - `uint64_t cycle_counter_ = 0`
- 方法:
  - `mmio_read(offset, buf, len)` / `mmio_write(offset, buf, len)` — 1/2/4 字节 + unaligned
  - `memory_read(offset, buf, len)` / `memory_write(offset, buf, len)` — BAR 2 backing
  - `tick()` — cycle counter 推进（无 MSI-X）
- 寄存器布局常量: `kRegDeviceIdentity`/`kRegMemSizeLo`/`kRegMemSizeHi`/`kRegMemBaseLo`/`kRegMemBaseHi`/`kRegStatus`/`kRegScratch`
- device_id = 0x0002（区别 D1 的 0x0001）

### T1.3 COVERAGE — 边界测试
- 边界 offset（0xFFF）、unaligned access（1/2/4 字节混合）
- 超出 BAR 0 范围（offset+len > 4096）返 -EINVAL
- 写 RO 寄存器（kRegDeviceIdentity）静默忽略
- memory_read/write out-of-range（offset+len > 8GB）返 -EINVAL
- 目标覆盖率 ≥ 80%

### T1.4 VALIDATE
```bash
./build/bin/cpptlm_tests "test_pcie_memory_device_basic" --reporter compact
```

## Step 2: PcieEndpointIP 注入 memory_device

### T2.1 RED — 测试 EP 持有 + tick() 推进
- 文件: `test/test_pcie_memory_device_basic.cc`（或新建）
- 测试: 实例化 EP，verify `ep->has_memory_device() == true`
- 测试: tick 1024 次 → `memory_device_->cycle_counter() == 1024`

### T2.2 GREEN — 修改 pcie_endpoint_ip
- 修改 `include/tlm/pcie/pcie_endpoint_ip.hh`:
  - 添加 `memory_device_` 成员: `std::unique_ptr<tlm::gpu::PcieMemoryDevice>`
  - 添加 accessor: `has_memory_device()` / `memory_device()`
- 修改 `src/tlm/pcie/pcie_endpoint_ip.cc`:
  - 构造函数: `memory_device_(std::make_unique<tlm::gpu::PcieMemoryDevice>())`
  - `tick()` 末尾: `if (memory_device_) memory_device_->tick();`

### T2.3 COVERAGE
- 多 tick 周期（0, 1, 1024, 10240）边界
- cycle_counter_ 溢出（8GB memory 场景无关紧要）

## Step 3: DGpuBoard 路由层

### T3.1 RED — 路由测试
- 在 `test/test_pcie_memory_device_backing.cc` 增加:
  ```cpp
  TEST_CASE("DGpuBoard 通过 EP 路由 BAR 2 到 PcieMemoryDevice") {
    auto* board = create_board_with_memory_device();
    cpptlm_emulator_t* emu = wrap(board);
    uint64_t data = 0xDEADBEEFCAFELL;
    cpptlm_emulator_mmio_write(emu, 2, 0x1000, &data, 8);
    uint64_t read_val = 0;
    cpptlm_emulator_mmio_read(emu, 2, 0x1000, &read_val, 8);
    REQUIRE(read_val == data);  // 真设备状态，非 shell-local
  }
  ```

### T3.2 GREEN — 修改 dgpu_board_shell
- 修改 `src/tlm/gpu/dgpu_board_shell.cc`:
  - `mmio_read/write`: 加 if-else 分支：`if (bar == 2 && ep && ep->has_memory_device())` → 路由到 `ep->memory_device()->memory_read/write()`
  - `backdoor_read/write`: 加 if-else 分支 → 路由到 `ep->memory_device()->memory_read/write()`
- 保留原 `mmio_regs_` / `vram_segments_` 作为 fallback（无 memory_device 时）

### T3.3 COVERAGE
- 不接 SOC（soc_ == nullptr）→ 走原 fallback 路径（**由 T0 锁定**）
- 接 SOC 但无 memory_device → 走原 fallback 路径
- 接 SOC + memory_device → 新路由路径
- BAR 2 已映射但 offset > 8GB → 返 -EINVAL

## Step 4: E2E + ABI 冻结验证

### T4.1 E2E 测试
- 文件: `test/test_pcie_memory_device_e2e.cc`
- 标签: `[pcie][memory][e2e]`
- 流程（per design §6）:
  1. create emulator with memory device config
  2. config read vendor_id/device_id（device_id=0x0002）
  3. mmio write MEM_SIZE_LO/HI, read back
  4. backdoor write memory data, read back
  5. tick 1024 cycles（验证 cycle counter 推进）

### T4.2 ABI 冻结验证
```bash
# D2 实施前后必须验证
git diff HEAD -- include/abi/cpptlm_emulator.h
# 期望: 空输出

# 函数签名计数
grep -c "^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" \
  include/abi/cpptlm_emulator.h
# 期望: 15（不变）
```

### T4.3 回归基线
```bash
# 全部现有 test cases 仍全绿
./build/bin/cpptlm_tests --reporter compact | tail -3
# 期望: 现有 + 新增 4 个

# docs_sync_check
./scripts/test/docs_sync_check.sh --strict
# 期望: PASS

# openspec validate
openspec validate --changes --strict
# 期望: 6/6 PASS（5 D1 + 1 D2）
```

## Step 5: 文档 + AGENTS.md + 提交

### T5.1 CppTLM 仓内 docs
- 新文件: `docs/pcie/memory-device-mvp.md`
- 内容:
  - 寄存器布局 + ABI 路由关系
  - D2 与 D1/D3 边界
  - UsrLinuxEmu 端验证方法（启动命令 + 预期输出）

### T5.2 AGENTS.md 更新
- "WHERE TO LOOK" 表: 添加 `[memory]` 标签
- 不破坏 D1 和既有架构标注

### T5.3 提交策略
```bash
# 6 个独立 commit
git commit -m "test(characterization): 锁定 DGpuBoard BAR 2 未映射行为防 D2 回归"
git commit -m "feat(pcie): 新增 PcieMemoryDevice 类（BAR 0 寄存器 + BAR 2 8GB backing）"
git commit -m "feat(pcie): PcieEndpointIP 注入 memory_device + tick() 推进"
git commit -m "refactor(board): DGpuBoard 路由 BAR 2 到 PcieMemoryDevice"
git commit -m "test(pcie): D2 单元测试 + E2E（basic + backing + routing + e2e）"
git commit -m "docs(pcie): 新增 memory-device-mvp.md + AGENTS.md 更新"
```

## 工时统计

| Task | 估时 | 累计 |
|------|------|------|
| T0 characterization | 2h | 2h |
| T1 device 骨架 | 4h | 6h |
| T2 EP 注入 | 3h | 9h |
| T3 board 路由 | 3h | 12h |
| T4 E2E + ABI 验证 | 3h | 15h |
| T5 docs + commits | 2h | 17h |
| **合计** | **~17h ≈ 3 工作日** | |

## 验证清单

- [ ] 4 个新增 test cases 全绿
- [ ] 现有 test cases 仍全绿
- [ ] openspec validate --changes --strict PASS（6/6）
- [ ] docs_sync_check --strict PASS
- [ ] `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空
- [ ] 15 ABI 函数签名不变（grep 计数 = 15）
- [ ] 4 callback typedef 不变
- [ ] `include/tlm/gpu/pcie_endpoint_tlm.h` 冻结头未触碰
- [ ] `include/tlm/gpu/pcie_display_device.hh` 未修改（D1 保持不变）

## 不在 D2 范围（deferred）

| 项 | 后置阶段 |
|----|---------|
| GPU 计算集成 | 不实现 |
| MSI-X 中断 | 不实现 |
| GMMU PoC | D3 |
| Power Management | 不实现 |
| 多 device BAR 共享 | 不实现 |

## D1 → D2 主要差异

| 项目 | D1 | D2 |
|------|-----|-----|
| device_id | 0x0001 | 0x0002 |
| BAR 0 寄存器 | DISPLAY_CONTROL/FB_INFO/VBLANK | MEM_SIZE/MEM_BASE/STATUS |
| MSI-X | 有（vector 0 VBLANK） | 无 |
| VBLANK | 有 | 无 |
| BAR 1 | 32MB framebuffer | N/A |
| BAR 2 | N/A | 8GB memory backing |
| 路由 priority | display_device 优先 | display 优先，memory 兜底 |
| tick() 行为 | 推进 VBLANK + 触发 MSI-X | 仅推进 cycle_counter_ |
