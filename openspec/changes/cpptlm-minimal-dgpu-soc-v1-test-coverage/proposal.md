# cpptlm-minimal-dgpu-soc-v1-test-coverage: Minimal dGPU SoC v1.0 测试覆盖补全

> **状态**: 📋 Proposed — 2027-02-09 (Oracle 复审修订 2027-02-10, 移除 T-bs-4 错误依赖)
> **配套 change**: [`cpptlm-minimal-dgpu-soc-v1`](../archive/2026-09-24-cpptlm-minimal-dgpu-soc-v1/proposal.md) (v1.0 主实施, 已 38/45 tasks 完成 + 44498→66805 assertions 全绿 + 0 ABI diff)
> **目的**: 补全 v1.0 推迟的 7 项测试任务, 提升 acceptance coverage 至 11/11 AC
> **前置依赖**: v1.0 主 change 已完成 (framebuffer_ + GmmuTLM + BAR 路由 + 接线)
> **关联 backlog**: `docs/designs/2027-02-09-minimal-dgpu-soc.md` §14 (v1.0 已记录)
> **工期估算**: 1.5 人日 (C1: 4h + C2: 8h; C2 主工作为测试编写 + framebuffer 显式挂载)
> **Oracle 复审更正 (2027-02-10)**: 原"C2 依赖 T-bs-4 SOC `instantiateAll` SIGSEGV 修复"前提**不成立** —— T-bs-4 已于 commit `8cd1f033`("D15 true fix - null-guard unregistered module types in instantiateAll")修复, 全套 996 cases / 32587 assertions PASS。`dgpu_board_shell.cc:63` 注释系 `17413e4` 时代陈腐残留, 实际第 77 行 `soc_->simulate_instantiate(soc_cfg)` 调用正常。C2.1 E2E 真实风险是 test harness framebuffer 挂载缺漏, 非 T-bs-4 阻塞。

## Why

`cpptlm-minimal-dgpu-soc-v1` 主 change 实施完成, 但 tasks.md 中 7 项推迟:

- **C1.1** 标签汇总测试
- **C1.2** 边界用例 (framebuffer_size_=0 + 非 4/8 字节 len)
- **C1.3** PT_BASE LO/HI race 特征化 (Inv-4 接受行为锁定)
- **C2.1-C2.4** E2E 测试 (host PT_BASE + SDMA H2D + framebuffer_ readback)
- **C3.1** backlog 确认 (零工作)
- **D2.2** `include/tlm/gpu/AGENTS.md` 行 (目录无此文件)
- **D3.2** format.sh 检查 (诊断代码已零, 格式化随手)

其中:

- **AC8 (E2E 100% PASS)** 是 v1.0 唯一未完成的 acceptance criterion, 当前 10/11 AC
- **C1.x + D2.2 + D3.2** 是防御性测试与 housekeeping, 单独立 change 不值但合并到独立 follow-up 可避免散落

按 oracle 审计结论 (2027-02-09) + 后续复审更正 (2027-02-10): C2 组**不依赖** T-bs-4 修复 (T-bs-4 已于 `8cd1f033` 完成), 单独立 follow-up change 让 C2 验收独立可执行, 不阻塞 v1.0 主 change 归档。

## What Changes

| 文件 | 变化 | 说明 |
|------|------|------|
| `test/test_minimal_dgpu_soc_characterization.cc` | **扩展** | 加 C1.1 标签汇总 SECTION, 加 C1.2 边界用例 (framebuffer_size_=0 + 越界 len) |
| `test/test_dgpu_board_framebuffer.cc` | **扩展** | 加 C1.2 PARTIAL 边界用例 |
| `test/test_gmmu_tlm.cc` | **扩展** | 加 C1.3 PT_BASE LO/HI race 锁定测试 |
| `test/test_minimal_dgpu_soc_e2e.cc` | **新建** | C2.1 双读回一致 + C2.3 fence MSI-X |
| `dgpu_board_shell.cc` | **修改** | D3.2 顺手跑 `./scripts/build/format.sh` |
| `openspec/changes/cpptlm-minimal-dgpu-soc-v1/tasks.md` | **修改** | 标注 C1.x/C2.x 移交到本 change |
| `openspec/changes/cpptlm-minimal-dgpu-soc-v1/design.md` | **修改** | §8 修订注记追加本 change 引用 |

## Scope

### IN-SCOPE

- **C1.1**: 标签汇总 SECTION — `./build/bin/cptlm_tests "[memory_backing],[gmmu],[dgpu_framebuffer]"` 全 PASS
- **C1.2**: 边界用例 — `framebuffer_size_=0` (resize(0) 后全访问 OUT_OF_RANGE) + BAR1 非 4/8 字节 len 行为锁定
- **C1.3**: PT_BASE LO/HI race 锁定 — LO 已写 HI 未写时 translate 用中间态 (Inv-4 接受行为)
- **C2.1**: E2E Scenario "全链路 H2D + 双读回一致"
- **C2.3**: 授权 SIGnature "fence → MSI-X vector 0" (C2.2 修复链路 PASS 跟随)
- **C2.4**: 诊断残留清零复查

### OUT-OF-SCOPE

- **C3.1 backlog 确认**: 零工作, 直接 close
- **D2.2 AGENTS.md 行**: 目标文件 `include/tlm/gpu/AGENTS.md` **不存在**, 直接 close
- **C2.2 修复链路 (C2.1 后修 until PASS)**: 失败走 cpptlm-debug SKILL 6 步定位 (首要根因为 framebuffer 挂载, 次要为 SDMA/GMMU/PCIeEP 子模块数据通路)

## Acceptance

- [ ] **AC1**: 标签汇总 PASS (`[memory_backing]+[gmmu]+[dgpu_framebuffer]` 全绿)
- [ ] **AC2**: 边界用例 PASS (framebuffer_size_=0 + 非 4/8 字节 len)
- [ ] **AC3**: PT_BASE LO/HI race 特征化锁定 (LO 已写 HI 未写时 translate 返回 -EIO 或用中间态可锁定行为)
- [ ] **AC4**: E2E Scenario "全链路 H2D + 双读回一致" PASS
- [ ] **AC5**: E2E Scenario "fence → MSI-X vector 0" PASS (MSI-X coalescing disabled)
- [ ] **AC6**: 诊断代码清零 (`grep -l "static FILE\* diag" include/ src/ -r` 为空)
- [ ] **AC7**: 既有 66805 assertions 零回归 (C1 + C2 均不引入新失败; C2 失败走 cpptlm-debug SKILL 独立定位)
- [ ] **AC8**: ABI 字节级兼容 (0 diff)
- [ ] **AC9**: `openspec validate cpptlm-minimal-dgpu-soc-v1-test-coverage --strict` PASS

## Capabilities

### New Capabilities

- `minimal-dgpu-soc-test-coverage`: v1.0 测试覆盖补全 (含边界用例 + race 锁定 + E2E 端到端)

### 涉及 spec

- **新建**: `openspec/changes/cpptlm-minimal-dgpu-soc-v1-test-coverage/specs/minimal-dgpu-soc-test-coverage/spec.md`

## Impact

**新增代码** (估算):

| 模块 | LOC |
|------|----:|
| C1 边界 + race 锁定 | ~150 |
| C2 E2E 测试 | ~250 |
| 总计 | **~400 LOC** |

**修改 ABI**: **0 个** (纯测试代码)

**测试覆盖**:

| 标签 | 新增用例 | 依赖 |
|------|----------|------|
| `[characterization]` (扩展) | +3 SECTION | — |
| `[dgpu_framebuffer]` (扩展) | +2 SECTION | — |
| `[gmmu]` (扩展) | +1 SECTION | — |
| `[minimal_dgpu_soc][e2e]` (新建) | 2 | — |

**风险**:

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| R1 | C2.1 E2E FAIL 暴露 test harness / 子模块数据通路 bug | 🟡 中 | 走 cpptlm-debug SKILL 6 步定位; 首要排查 `framebuffer_size_=0` (load_soc_config 不消费 JSON 顶层字段), 次要排查 SDMA/GMMU/PCIeEP 数据通路 |
| R2 | C1.2 边界用例发现新 framework 集成问题 | 🟢 低 | 现有覆盖已防回归 |
| R3 | C1.3 race 锁定与 Inv-4 "v1.0 接受 race" 冲突 | 🟢 低 | 锁定为"接受 LO 已写 HI 未写时用中间态"语义, 与 spec 一致 |

**关联 change**:

- `openspec/changes/cpptlm-minimal-dgpu-soc-v1/` (v1.0 主实施, 已完成)
- `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/` (GMMU 完整版路线, 本 change 推迟项 v1.0 端到端是其 v1.0 子集)

---

**下次更新**: C1 完成时 (1d) / C2 完成时 (1.5d, 含 framebuffer 显式挂载)