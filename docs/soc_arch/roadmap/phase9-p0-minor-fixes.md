# phase9-p0-minor-fixes: 5 项 Minor 问题清理

> **类别**: SoC Architecture > Roadmap · **阶段**: P0 · **优先级**: 🔴 立即做(零跨仓/零风险)
> **日期**: 2026-09-16 · **维护者**: Sisyphus · **跨仓**: ❌
> **关联 ADR**: [revision-plan-v1.0.md](../adr/revision-plan-v1.0.md) (B1 已完成,B2 待执行) · [ADR-SOC-16-sm-microarchitecture.md](../adr/ADR-SOC-16-sm-microarchitecture.md) (Task 16)
> **关联 OpenSpec**: 无(纯仓内修复)

---

## 1. 目标

清理 Phase 8 + SM 重构遗留的 Minor 问题,**恢复测试套件 100% 绿**,为 P1 SM Gate 验证提供干净基线。累计工作量约 1 周,**全部在 CppTLM 仓内**,不涉及跨仓协调。

**核心原则**:**additive + flag-gated**,不破坏现有测试。

---

## 2. 任务清单

| ID | 任务 | 来源 | 成本 | 阻塞 |
|----|------|------|------|------|
| **P0-1** | 执行 ADR-SOC revision-plan **B2** | `revision-plan-v1.0.md` §B2 | 2-3 h | 无 |
| **P0-2** | 修复 `test_pcie_endpoint_ip_full_e2e.cc` 2 个 known fail(`_config` / `_bar`) | `test/test_pcie_endpoint_ip_full_e2e.cc` | 4-6 h | 无 |
| **P0-3** | SM **Task 16**:物理删除 deprecated 类 + 15 旧测试 | `ADR-SOC-16-sm-microarchitecture.md` §6 Task 16 | 1 d | ✅ 已完成 |
| **P0-4** | 修复 `dgpu_board_shell.cc:250` TODO T-bs-3c | `src/tlm/gpu/dgpu_board_shell.cc:250` | 2-4 h | 无 |
| **P0-5** | 修复 `dgpu_board_shell.cc:562` TODO T-bs-3b | `src/tlm/gpu/dgpu_board_shell.cc:562` | 2-4 h | 无 |

### P0-1:ADR revision-plan B2 执行

**内容**:
- §B2 A1+ADR-08 Status Update 追加(纯文档)
- 修订 `docs/soc_arch/adr/ADR-SOC-08-v55-system-hw-integration-preconditions.md` 状态更新
- 涉及:5 项 Tier 2 前置测试 P0/P1/P2 治理结果 + 4 项 UE 增量提议进展

**执行步骤**:
1. 读 B1 commit(429327d,`[[deprecated]]` 标注)确认基线
2. 追加 ADR-08 Status Update 段
3. 评审提交

### P0-2:2 个 known fail 修复

**位置**:`test/test_pcie_endpoint_ip_full_e2e.cc` 的 `_config` 和 `_bar` 测试用例
**当前状态**(Oracle 报告修正):**代码已含低 2 bit 屏蔽**(`pcie_endpoint_ip.cc:359, 403` `awaddr & ~0x3ULL`,AGENTS.md KEY INVARIANT 亦称 v1.1 已实现)。真正的修复工作是**更新测试期望值**适配现有正确编码,不是加新屏蔽。
**修复方向**:
- 读取 `pcie_endpoint_ip.cc:355-410` cfg 写/读路径,确认实际行为
- 对比 `test_pcie_endpoint_ip_full_e2e.cc` `_config` / `_bar` 期望值,定位差异
- 更新测试期望值 + 必要时调整测试输入(若旧期望基于"无屏蔽"假设)
- 若实测仍 fail,根因可能在 BAR 范围判定逻辑而非 cfg 屏蔽 — 报告后回退 P2-2 范围

### P0-3:SM Task 16 物理删除 [**MUST 先于 P1-1**]

**删除范围**(per `ADR-SOC-16-sm-microarchitecture.md` §6 Task 16):
- 3 个 deprecated 文件:
  - `include/tlm/gpu/vector_regfile_tlm.hh`
  - `include/tlm/gpu/minimal_warp_scheduler_tlm.hh`
  - `include/tlm/gpu/wavefront_tlm.hh`
- **同步清理引用点**(build break 防御):
  - `include/chstream_register.hh:21-23` 移除 3 个 deprecated header 的 include
  - `modules_cluster.hh` 任何 RegisterAll 引用(grep 验证)
  - UE 仓 `tests/integration/cpptlm_pcie_init.hh` 验证无引用(已 grep 确认无冲突,但 commit 前再 grep)
- 15 个旧测试文件(`test/test_*.cc` 中依赖以上 deprecated 类的)
- 4 个 JSON 配置
- DOC HYGIENE:删除对应文档中的引用

**前提**:Task 10 已标 `[[deprecated]]`(✅ 已完成),Task 12 已物理删除 kernel_launch_tlm 等(✅ 已完成)

**风险**:旧测试文件名/符号与新 SM 重构测试可能冲突 → **必须先 grep 全文确认无命名冲突**

### P0-4 / P0-5:`dgpu_board_shell` TODO 修复

- **P0-4 (T-bs-3c)**:drain 响应 payload 拷贝真实数据到调用方 buf(当前是 `set_value(0)` 占位)
- **P0-5 (T-bs-3b)**:真实 quantum 边界 + TickEvent 自续处理

详见 `docs/superpowers/plans/2026-08-29-dgpu-board-shell-injection.md:136,170,197`

---

## 3. 依赖关系

```
P0-1 ──┐
P0-2 ──┤
P0-3 ──┼──> [P0 全部完成] ──> P1 启动(G3/G4/G5 Gate 需要干净基线)
P0-4 ──┤     ↑
P0-5 ──┘     │ MUST
             └─ P0-3 完成 ❌ P1-1 开始
```

**关键依赖**:
- **P0-3 (Task 16) MUST 先于 P1-1 (SM Task 17 L2 Bundle 接线测试)** — **硬约束**(违反导致符号冲突 / 编译失败)。DoD 需 P0-3 commit hash 出现在 P1-1 commit 信息中。
- P0-4 / P0-5 是 mmio/quantum 准确性,与 UE 集成测试的真实性相关(可选 P2 启动前置)

---

## 4. 完成标准(DoD)

- [ ] P0-1:ADR-08 Status Update 段已追加,commit 已 push
- [ ] P0-2:`./build/bin/cpptlm_tests "[pcie]"` 全绿,0 known fail
- [ ] P0-3:grep 无旧 deprecated 符号残留,删除文件已 commit
- [ ] P0-4:`mmio_read` 返回真实数据(非 0 占位),新测试断言
- [ ] P0-5:quantum 边界正确性测试 PASS
- [ ] 整体:`./test.sh --mode off` 全绿(47634 assertions,0 known fail)
- [ ] pre-commit hook 通过(commit message 符合 Conventional Commits)

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| P0-2 修复后引入新 fail | 中 | 中 | 在修复前先备份 `_config` / `_bar` 期望值,逐步修正 |
| P0-3 删除文件命名冲突 | 中 | 高 | **强制**先 grep `wavefront_tlm`、`vector_regfile`、`minimal_warp_scheduler`,确认无 P1 任务依赖 |
| P0-4 / P0-5 dGPU board shell 影响 Phase 8 E2E 测试 | 低 | 高 | 先在 P0-2 修复后跑 `[pcie][e2e]` 测试基线,再动 dgpu_board_shell |

---

## 6. 跨仓协调事项

**无**。P0 全部在 CppTLM 仓内,不涉及 UsrLinuxEmu。

**单向通知**(可选):
- W25 末 commit 后,在 UE 仓 RoadMap 同步"P0 完成,基线绿"
- UE 端无需变更

---

**下次 review**: W26 末(P0 全部完成,验证 `[pcie]` 测试 100% 绿)