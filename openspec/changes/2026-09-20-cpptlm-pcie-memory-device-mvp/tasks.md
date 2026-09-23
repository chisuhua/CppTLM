# Tasks: Memory 设备 MVP（v1.1 修订版）

> **配套**: [proposal.md](proposal.md) · [design.md](design.md) · [specs/memory-device-mvp/spec.md](specs/memory-device-mvp/spec.md)
> **v1.1 修订触发**: D1 v1.1.1 Oracle 复审（ses_f342ea637ffeDWYBBEQDUUE63U）发现 D2 设计含 3 个与 D1 root cause 4 同型盲点
> **v1.1 修订**: 引入 `memory_routing_enabled_` flag（与 D1 v1.1.1 `display_routing_enabled_` 对称）+ T0.5 路由 flag 组合测试 + T3.5 flag 实施任务
> **方法**: TDD 6-step（T0 characterization → T0.5 路由 flag → T1 device → T2 EP → T3 board → T3.5 flag → T4 测试 → T5 docs）
> **工期**: 3.5 工作日（原 3-4 天 + v1.1 修订 +0.5 天）

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
  - **v1.1 标注**：`memory_routing_enabled_` flag 决策 + 与 D1 v1.1.1 对称

### T5.2 AGENTS.md 更新
- "WHERE TO LOOK" 表: 添加 `[memory]` 标签
- 不破坏 D1 和既有架构标注

### T5.3 提交策略
```bash
# 7 个独立 commit（v1.1 新增 1 个 T3.5）
git commit -m "test(characterization): 锁定 DGpuBoard BAR 2 未映射行为防 D2 回归"
git commit -m "test(routing): D2 v1.1 routing flag 组合测试（防劫持 root cause 4）"
git commit -m "feat(pcie): 新增 PcieMemoryDevice 类（BAR 0 寄存器 + BAR 2 8GB backing）"
git commit -m "feat(pcie): PcieEndpointIP 注入 memory_device + tick() 推进"
git commit -m "refactor(board): DGpuBoard 路由 BAR 0/2 到 PcieMemoryDevice"
git commit -m "feat(board): memory_routing_enabled_ 路由开关（D1 v1.1.1 对称防劫持）"
git commit -m "test(pcie): D2 单元测试 + E2E + ABI routing 回归"
git commit -m "docs(pcie): 新增 memory-device-mvp.md + AGENTS.md 更新"
```

## 工时统计

| Task | 估时 | 累计 |
|------|------|------|
| T0 characterization | 1h | 1h |
| **T0.5 路由 flag 组合测试（v1.1 新增）** | 1h | 2h |
| T1 device 骨架 | 3h | 5h |
| T2 EP 注入 | 2h | 7h |
| T3 board 路由 BAR 0/2 | 3h | 10h |
| **T3.5 memory_routing_enabled_ flag（v1.1 新增）** | 2h | 12h |
| T4 E2E + ABI 验证 | 2h | 14h |
| T5 docs + commits | 1.5h | 15.5h |
| **合计** | **~15.5h ≈ 3.5 工作日** | |

## 验证清单

- [ ] 4 个新增 test cases 全绿（basic + backing + e2e + characterization）
- [ ] **v1.1 新增**：T0.5 路由 flag 组合测试全绿
- [ ] 现有 test cases 仍全绿（含 D1 v1.1.1 39 case）
- [ ] openspec validate --changes --strict PASS（6/6）
- [ ] docs_sync_check --strict PASS
- [ ] `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空
- [ ] 15 ABI 函数签名不变（grep 计数 = 15）
- [ ] 4 callback typedef 不变
- [ ] `include/tlm/gpu/pcie_endpoint_tlm.h` 冻结头未触碰
- [ ] `include/tlm/gpu/pcie_display_device.hh` 未修改（D1 保持不变）
- [ ] **v1.1 新增**：`memory_routing_enabled_` 默认 false；`dgpu_soc_with_memory_device.json` 显式启用
- [ ] **v1.1 新增**：BAR 0 routing 在 `display_routing_enabled=true` + `memory_routing_enabled=true` 组合下，D1 优先（D1 v1.1.1 路由测试 PASS）

## 不在 D2 范围（deferred）

| 项 | 后置阶段 |
|------|---------|
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
| 路由开关 | `display_routing_enabled_`（D1 v1.1.1） | `memory_routing_enabled_`（D2 v1.1 对称） |
| 路由 priority | display | D1 display 优先 → D2 memory 兜底 |
| tick() 行为 | 推进 VBLANK + 触发 MSI-X | 仅推进 cycle_counter_ |

## v1.1 → v1.0 主要修正（Oracle 复审触发）

| v1.0 提案 | v1.1 修订 |
|----------|------------|
| "memory 其后"路由 priority（隐含无条件劫持） | 显式 `memory_routing_enabled_` flag，默认 false |
| T0 仅测 BAR 2 未映射 | T0.5 新增：路由 flag 组合测试（display+memory 双启用时 BAR 0 行为） |
| 6 个 commit | 7 个 commit（新增 T3.5 flag commit） |
| 风险 R1 "display 优先" 中等 | R1 提升到 🔴 高 + flag 防劫持 |
| 估算 17h (3 工作日) | 估算 15.5h (3.5 工作日，含 v1.1 修订 +0.5h flag）|
