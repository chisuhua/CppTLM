# Tasks: Display IO 设备 MVP (TDD 5-step)

> **配套**: [proposal.md](proposal.md) · [design.md](design.md) · [specs/display-io-mvp/spec.md](specs/display-io-mvp/spec.md)
> **关联设计**: `docs/pcie/display-device-mvp.md` (CppTLM 仓内, D1 创建)
> **工期**: 5-6 工作日
> **方法**: TDD (Red → Green → Refactor → Coverage → Validation)

## Step 1: PcieDisplayDevice 骨架 (Red-Green, TDD §1)

### T1.1 RED — 写失败的测试
- 文件: `test/test_pcie_display_device_basic.cc`
- 测试名: `"PcieDisplayDevice BAR 0 寄存器 round-trip"`
- 断言:
  ```cpp
  PcieDisplayDevice dev;
  dev.mmio_write(0x10, &mode_val, sizeof(mode_val));  // DISPLAY_MODE
  dev.mmio_read(0x10, &read_val, sizeof(read_val));
  REQUIRE(read_val == mode_val);
  ```
- 预期: FAIL（class 尚未实现）

### T1.2 GREEN — 最小实现
- 文件: `include/tlm/pcie/pcie_display_device.hh` + `src/tlm/pcie/pcie_display_device.cc`
- 最小可工作的 `mmio_read/write`（使用 `registers_[4096]` 数组 + memcpy）

### T1.3 COVERAGE — 边界测试
- 边界 offset（0xFFF）、unaligned access、超出 BAR 0 范围（应返 -EINVAL）
- 目标覆盖率 ≥ 80%

### T1.4 VALIDATE
```bash
./build/bin/cpptlm_tests "test_pcie_display_device_basic" --reporter compact
```

## Step 2: PcieEndpointIP 注入 (Red-Green, TDD §2)

### T2.1 RED — 测试 tick() 推进 VBLANK
- 文件: `test/test_pcie_display_device_msix.cc`
- 测试: tick 1024 次 → VBLANK pending=1, MSI-X pending=1

### T2.2 GREEN — 注入到 PcieEndpointIP
- 修改 `src/tlm/pcie/pcie_endpoint_ip.cc`
- `tick()` 中调用 `display_device_->tick()`

### T2.3 COVERAGE — 多周期测试
- tick 10240 次 → 应触发 10 次 VBLANK
- 验证 vblank_count_ counter

## Step 3: BAR 路由器扩展 (Red-Green, TDD §3)

### T3.1 RED — 路由测试
- `test/test_pcie_display_device_basic.cc` 增加: BAR 1 读 framebuffer

### T3.2 GREEN — 扩展 pcie_bar_router_mvp
- 修改 `include/tlm/pcie/pcie_bar_router_mvp.hh` + `src/tlm/pcie/pcie_bar_router_mvp.cc`
- 添加 BAR 0/1 → PcieDisplayDevice 路由

### T3.3 COVERAGE
- BAR 0/1 边界测试
- 不存在的 BAR (BAR 2-5) 应返 -EINVAL

## Step 4: ABI 修复 (Red-Green, TDD §4) 🔴 **关键路径**

### T4.1 RED — 4 个 ABI 修复测试
- `test/test_pcie_abifix_config_read.cc`
- `test/test_pcie_abifix_config_write.cc`
- `test/test_pcie_abifix_mmio_read.cc`
- `test/test_pcie_abifix_mmio_write.cc`
- `test/test_pcie_abifix_backdoor_read.cc`
- `test/test_pcie_abifix_backdoor_write.cc`

每个测试:
```cpp
// test_pcie_abifix_config_read.cc
TEST_CASE("ABI: cpptlm_emulator_pcie_config_read 真实读取 DEVICE_ID", "[abi][fix][pcie]") {
    cpptlm_emulator_t* emu = cpptlm_emulator_create("configs/example/pcie-display-device-v1.json");
    REQUIRE(emu != nullptr);

    uint32_t val = 0;
    int ret = cpptlm_emulator_pcie_config_read(emu, 0x00, 4, &val);  // offset 0x00, width 4 bytes
    REQUIRE(ret == 0);
    REQUIRE(val != 0);  // 修复前返 -ENOSYS (val=0)；修复后返 DEVICE_ID
    REQUIRE((val & 0xFFFF) == 0x1002);  // vendor_id = 0x1002 (AMD/ATI)

    cpptlm_emulator_destroy(emu);
}
```

### T4.2 GREEN — 修改 ABI wrapper 实现
- 文件: `src/core/cpptlm_emulator_api.cc`（or 类似 wrapper）
- 6 个 ABI 函数: config_read/write, mmio_read/write, backdoor_read/write

### T4.3 COVERAGE — 双向验证
- 不仅"能读"，还要"读正确"
- 不仅"能写"，还要"写后再读回相同值"

## Step 5: MSI-X VBLANK 中断链 (TDD §5)

### T5.1 RED — intr_cb 接收测试
- `test/test_pcie_display_device_msix.cc` 增加: 驱动注册 intr_cb，验证 VBLANK 后 cb 被调

### T5.2 GREEN — 完整中断链
- PcieEndpointIP::tick() 触发后调用 `msix_->update_pending(0)`
- msix_table 调用 intr_cb（如果已注册）

### T5.3 COVERAGE
- 多个 VBLANK 事件（测试稳定性）
- 不同 VBLANK 频率参数化测试

## Step 6: 集成测试 (E2E)

### T6.1 E2E 测试
- 文件: `test/test_pcie_display_device_e2e.cc`
- 测试: 完整流程 Config → BAR enumerate → MMIO round-trip → VBLANK MSI-X
- 标签: `[pcie][display][e2e]`

### T6.2 拓扑配置
- 文件: `configs/example/pcie-display-device-v1.json`
- 单 PCIe EP + PcieDisplayDevice + Host Bypass

## Step 7: 文档 + ABI 提交

### T7.1 CppTLM 仓内 docs
- 文件: `docs/pcie/display-device-mvp.md`
- 内容: 寄存器布局 + ABI 修复说明 + 验证方法

### T7.2 AGENTS.md 更新
- "First read" 节添加 D1 链接
- 测试标签添加 `[display]`

### T7.3 提交策略
```bash
git add openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/
git add include/tlm/pcie/pcie_display_device.hh src/tlm/pcie/pcie_display_device.cc
git add test/test_pcie_display_device_*.cc test/test_pcie_abifix_*.cc
git add configs/example/pcie-display-device-v1.json docs/pcie/display-device-mvp.md
git add include/tlm/pcie/pcie_bar_router_mvp.hh src/tlm/pcie/pcie_bar_router_mvp.cc
git add include/tlm/pcie/pcie_endpoint_ip.hh src/tlm/pcie/pcie_endpoint_ip.cc
git add AGENTS.md

git commit -m "feat(pcie): add Display IO device MVP + fix 4 NO-OP ABI bugs

Display IO device MVP enables UsrLinuxEmu driver validation
without ArchForge dependency. Fixes 4 NO-OP ABI bugs:
- cpptlm_emulator_pcie_config_read/write (#3)
- cpptlm_emulator_mmio_read/write (#5/#7)
- cpptlm_emulator_backdoor_read/write (#6)

Adds 9 new test cases (5 display + 4 ABI fix). All ABI signatures
unchanged (22 ABI freeze honored)."
```

## 工时统计

| Task | 估时 | 累计 |
|------|------|------|
| T1 骨架 | 4h | 4h |
| T2 注入 | 2h | 6h |
| T3 BAR 路由 | 4h | 10h |
| T4 ABI 修复 | 6h | 16h |
| T5 MSI-X 链 | 4h | 20h |
| T6 E2E | 4h | 24h |
| T7 Docs | 2h | 26h |
| **合计** | **26h ≈ 3.3 工作日** | |

## 验证清单

- [ ] 9 个新增 test cases 全绿
- [ ] 1480 现有 test cases 仍全绿
- [ ] openspec validate --changes --strict PASS
- [ ] docs_sync_check --strict PASS
- [ ] 22 ABI 签名未变（diff `include/abi/cpptlm_emulator.h`）
- [ ] 冻结头未触碰（diff `include/tlm/gpu/pcie_endpoint_tlm.h`）

## 不在 D1 范围（deferred）

| 项 | 后置阶段 |
|----|---------|
| 真实显示输出 | 不用 |
| Cursor / Overlay | 不用 |
| HDMI / DP PHY | 不用 |
| SDMA / SM 集成 | D2 |
| GMMU PoC | D3 |
| Power Management | 不用 |