# Tasks: cpptlm-stage-1-1-pcie-ep-fixes

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5-1 周（实施顺序 #3 → #6 → #5 → #7）
> **TDD 纪律**: 每任务 5 步（write failing test → verify fail → implement → verify pass → commit）

## §1 阶段 1.1.1: 修复 #3 (pcie_config_read/write 转发)

### 任务 1.1.1.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_pcie_config.cc::test_pcie_config_read_vendor_id_returns_0x10DE`
- [ ] **Verify fail**: 当前返 -ENOSYS

### 任务 1.1.1.2: 实施转发
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:151-164`
  - pcie_config_read: null check → 转发 `ep_->cfg_space_->read(offset, width, val)`
  - pcie_config_write: null check → 转发 `ep_->cfg_space_->write(offset, width, val)`
- [ ] **Verify pass**: 测试通过

### 任务 1.1.1.3: Commit
- [ ] **Commit**: `fix(dgpu-board): forward pcie_config_read/write to cfg_space (修复 #3)`

## §2 阶段 1.1.2: 修复 #6 (backdoor_read miss 返 -ENOENT)

### 任务 1.1.2.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_backdoor.cc::test_backdoor_read_miss_returns_enoent_not_len`
- [ ] **Verify fail**: 当前 miss 返 `len` (例如 4)

### 任务 1.1.2.2: 实施返值修正
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:168-190`
  - 加 buf nullptr 检查
  - miss 返 `-ENOENT`（-38）
  - size mismatch 返 `-EINVAL`
  - hit 返 0 + memcpy
- [ ] **Verify pass**: 测试通过

### 任务 1.1.2.3: Commit
- [ ] **Commit**: `fix(dgpu-board): backdoor_read miss returns -ENOENT (修复 #6)`

## §3 阶段 1.1.3: 修复 #5 (mmio_read 数据拷贝)

### 任务 1.1.3.1: 写失败测试
- [ ] **Write test**: `test/test_dgpu_mmio.cc::test_mmio_read_real_data_not_garbage`
- [ ] **Verify fail**: 当前 buf 未填充，data 是 garbage

### 任务 1.1.3.2: 实施数据拷贝
- [ ] **Modify**: `src/tlm/gpu/dgpu_board_shell.cc:106-133`
  - 新数据结构 `pending_data_: std::unordered_map<uint64_t, std::vector<uint8_t>>`
  - sim_loop drain 真实化（`drain_injection_queue()` 注入器 set_value + set data）
  - mmio_read 等待后 memcpy data → buf
  - 返值改为 byte count 或负 errno
- [ ] **Modify**: `src/tlm/pcie/pcie_endpoint_ip.cc` (sim_loop drain 实现)
- [ ] **Verify pass**: 测试通过（buf 真实填充）

### 任务 1.1.3.3: Commit
- [ ] **Commit**: `fix(dgpu-board): mmio_read real data copy (修复 #5)`

## §4 阶段 1.1.4: 修复 #7 (mmio_write 文档矛盾澄清)

### 任务 1.1.4.1: 修改 design.md
- [ ] **Modify**: `openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md §3.1`
  - 删除 "blocks synchronously" 措辞
  - 添加 "mmio_write returns 0 immediately (async); data is drained by sim_loop on next tick"

### 任务 1.1.4.2: 验证架构文档一致性
- [ ] **Verify**: `architecture/18-pcie-endpoint-entry.md §2.1` 已说 "MMIO Write 是异步"
- [ ] **Verify**: 代码 `dgpu_board_shell.cc:148` 返 0 立即返回

### 任务 1.1.4.3: Oracle 轻量复审
- [ ] **Oracle review**: 1 次轻量复审，确认保持 async 裁决
- [ ] **Commit**: `docs(cpptlm): clarify mmio_write async semantics (修复 #7)`

## §5 集成测试

### 任务 5.1: 完整 PCIe EP 基础测试套件
- [ ] **Run**: `./build/bin/cpptlm_tests "[dgpu][pcie]"` — 4 修复全部测试通过
- [ ] **Run**: `./build/bin/cpptlm_tests "[abi]"` — 既有 ABI 测试零回归
- [ ] **Verify**: 152/152 tests PASS (新增 4 测试)

### 任务 5.2: 跨仓集成测试 (UE 侧)
- [ ] **Verify**: UsrLinuxEmu 桥接层测试（`test_bridge_dgpu_with_real_pcie_ep`）PASS
- [ ] **Verify**: UsrLinuxEmu 5.5.6 profile_real 测试从 "ret != -ENOSYS" 升级到数据正确性

### 任务 5.3: 更新既有 ABI 测试期望（修复 #6 打破零回归）
- [ ] **Modify**: `test/test_dgpu_board_shell_abi.cc`
  - L219: `REQUIRE(board.backdoor_read(0xDEADBEEF, buf, 64) == 32)` → 改为 `REQUIRE(board.backdoor_read(0xDEADBEEF, buf, 64) == -ENOENT)`
  - L222: `REQUIRE(board.backdoor_read(0x1000, buf, 32) == 32)` (size mismatch) → 改为 `REQUIRE(board.backdoor_read(0x1000, buf, 32) == -EINVAL)`
  - 理由：修复 #6 把 miss 返 `len` 改为返 `-ENOENT`、size mismatch 返 `-EINVAL`，既有 2 断言预期是 stub 行为必须更新
- [ ] **Verify**: 更新后 test_dgpu_board_shell_abi PASS（既有 ABI 测试**预期**因修复 #6 行为变更而调整，非"零回归"）

## §6 Oracle Gate E 复审（v1.0 → v1.1 升档）

### 任务 6.1: Oracle 实施后复审
- [ ] **Oracle review**: 1 次完整实施复审，4 修复质量评估
- [ ] **Verify**: ADR-035 §R2 v1.0 → v1.1 升档流程通过

### 任务 6.2: 同步双仓 entry
- [ ] **Modify**: UsrLinuxEmu `entry §11` ADR-088 / `cpptlm-pcie-ep-foundation` 关联行
- [ ] **Verify**: CppTLM `18-doc §11` 同步状态

### 任务 6.3: Archive 本 change
- [ ] **Run**: `openspec archive 2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes`
- [ ] **Verify**: spec 提升到 `openspec/specs/cpptlm-stage-1-1-fixes/spec.md`

## 总计

- **任务数**: 17 (4 修复 × 3-4 tasks + 5 集成 + 6.1-6.3)
- **预期 commit**: 4-5 (每修复一个 + 集成测试)
- **工期**: 0.5-1 周（实施）+ 0.5 天（Oracle 复审）
- **下游依赖**: UsrLinuxEmu `2026-09-10-ue-stage-1-1-bridge-sync` change 解锁

---
