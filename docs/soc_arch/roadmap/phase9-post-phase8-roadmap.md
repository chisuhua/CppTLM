# phase9-post-phase8-roadmap: Phase 8 收尾后的 v1.0 阶段规划总览

> **类别**: SoC Architecture > Roadmap · **阶段**: phase9 (W25-W42 起步期) · **优先级**: 🟢 总览
> **日期**: 2026-09-16 · **维护者**: Sisyphus · **跨仓**: 部分(P2)
> **关联 ADR**: [ADR-SOC-16-sm-microarchitecture.md](../adr/ADR-SOC-16-sm-microarchitecture.md) (Task 1-20) · [ADR-SOC-07](../adr/ADR-SOC-07-dgpu-board-soc-layering.md) · [ADR-SOC-08](../adr/ADR-SOC-08-v55-system-hw-integration-preconditions.md)
> **关联 OpenSpec**: [archive/2026-09-10-2026-09-09-cpptlm-pcie-ep-foundation/](../../../openspec/changes/archive/2026-09-10-2026-09-09-cpptlm-pcie-ep-foundation/) (战略基础)

**W 编号基准**:W1 = 2026-09-21(Mon) — 本目录所有阶段文件均按此基准;`2026-09-16` 创建日 ≈ W37 末(已部分逾期,见 §4 注)。

---

## 1. 目标

在 Phase 1-8 PCIe EP 全链路交付 + SM 重构 Task 1-15 完成的基线上,**用 4 个梯队 (P0/P1/P2/P3) 按 W25-W42 共 18 周窗口** 完成(实际工作量 ≈ 8-10 周,含 buffer):
- (P0) 5 项 Minor 问题清理,恢复测试套件 100% 绿
- (P1) SM 重构 Gate 验证 + archive(Task 17-19;Task 20 HSK-9 发布到 PTX-EMU 延后)
- (P2) CP 寄存器接入(经 AXI 桥,Oracle REFINE + YES 决策),为 UE 端 5.5.7/5.5.8 解锁
- (P3) 战略层 ADR 修订 + Phase 9 雏形,定义下一阶段

**预期产出**:
- SM 重构 change 正式归档(W33)
- CP 接入跨仓 change 提案 + Oracle 评审通过(**W38**,与协调节奏对齐)
- v1.0 战略方向清晰,W42 末 phase9 全部收尾,W43+ 进入 Phase 10

---

## 2. 任务清单(4 个梯队)

| 阶段 | 主题 | 关键交付物 | 时间 | 跨仓 | 详情 |
|------|------|------------|------|:----:|------|
| **P0** | Minor 修复 | 5 项 Minor + 2 known fail 修复 | W25-W26 | ❌ | [phase9-p0-minor-fixes.md](./phase9-p0-minor-fixes.md) |
| **P1** | SM Gate | SM Task 17-19(L2/L3 集成测试 + archive);Task 20(G12 HSK-9 公告到 PTX-EMU)延后 W40+ | W27-W33 | 部分(HSK-9 公告) | [phase9-p1-sm-gate-verification.md](./phase9-p1-sm-gate-verification.md) |
| **P2** | CP 接入 | CP 寄存器文件 + EP BAR 转发 + UE 双路径 | W34-W38(待启动) | ✅ | [phase9-p2-cp-attach-via-axi.md](./phase9-cp-attach-via-axi.md) |
| **P3** | 战略 | ADR B3-B4 + B5'-B8' + Phase 9 雏形 | W40-W42 | 部分 | [phase9-p3-strategic.md](./phase9-p3-strategic.md) |

**注**:
- P2 实际命名修订 → `phase9-p2-cp-attach-via-axi.md`(Oracle 报告建议聚焦 CP 接入主线)
- P3 "B5'-B8'" 是新定义(避免与 [revision-plan-v1.0.md](../adr/revision-plan-v1.0.md) §6.1 的 B5-B8 撞号),详见 P3 文件

---

## 3. 依赖关系

```
P0 (Minor) ──┬──> P1 (SM Gate) ──┐
             │                     ├──> P2-1 (CP spec) ──> P2-9 (OpenSpec 提案+评审) ──┐
             └──> UE 7-fix 收尾 ──┘                                                       │
                                                                                          │
                                                                                  P2-2..P2-8 跨仓实施
                                                                                          │
                                                                                  P3 (战略) ──> Phase 10 启动
```

**关键依赖**:
- P0-3 (Task 16 物理删除) **MUST 先于** P1-1(SM Task 17 L2 Bundle 接线测试),否则符号冲突
- P0 完成后 → P1 才能跑(G3/G4/G5 Gate 测试需要清干净的代码基线)
- P1 完成后 → SM 重构 change 可正式归档
- P0 + P1 + UE 端 7-fix 收尾 → P2 才能跨仓启动(共享前置)
- **P2-1 (CP 寄存器 spec) 必须在 P2-2..P2-8 实施之前完成**,且 P2-9 (OpenSpec 提案+评审) 紧随 P2-1 之后 — 严格遵循"先提案后实施"纪律(避免 UE Wave 6 stage-3 先行冲突)
- P2 启动后 → P3 战略层 ADR 修订有真实证据基础

---

## 4. 完成标准(DoD)

| 时点 | DoD |
|------|-----|
| **W26 末** | ✅ P0 全部完成,`./test.sh --mode off` 全绿(目标 ≥ 47634 assertions,0 known fail;Task 16 删 15 旧测试会减少断言数,实际数字需 W26 末验证) |
| **W33 末** | ✅ P1 全部完成,SM 重构 change 归档,Gate G3/G4/G5/G7/G13 全 PASS(**G9 属 P0-3/Task 16,不在 P1 验收范围**) |
| **W38 末** | ✅ P2 跨仓 change 提案 + Oracle 评审通过,UE 5.5.8 重启 |
| **W42 末** | ✅ P3 ADR B3-B4 + B5'-B8' 完成,Phase 9 启动文件就位 |

**注**:文件创建于 2026-09-16 ≈ W37 末(已部分入 W25-W36),故 W26/W33 节点属"追溯性基线";后续 W38/W42 才是真正未来时点。

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| **P0-3 (Task 16 物理删除) 与现有测试命名冲突** | 中 | 中 | 提前 grep 旧测试符号,Task 16 必须在 P1-1 之前完成 |
| **P2 跨仓协调节奏失控** | 中 | 高 | 每周一次 CppTLM + UE 同步会,以 [18-pcie-endpoint-entry.md](../../architecture/18-pcie-endpoint-entry.md) 为 SSOT |
| **P3 双 vendor ADR 评审翻车** | 中 | 中 | 先 Metis 预评审 → 提案 → Oracle 复评,不要直接动手 |
| **新方向(P2)实施偏离 Oracle REFINE 口径** | 低 | 高 | 严格按"CP 寄存器 = EP BAR 子区域"实施,不新建 AXI-Lite slave/crossbar |

---

## 6. 跨仓协调事项

**P2 阶段是跨仓核心议题**:
- CppTLM 侧:CP 寄存器文件定义 + EP BAR 转发(`dgpu_board_shell.cc` 新增 forwarding 规则,照 SDMA doorbell 模式)
- UE 侧:5.5.8 change `cp_attach` 重启 + 后端选择机制 + `docs/02_architecture/cp-register-map.md` 镜像文档

**协调节奏**:
- W34:OpenSpec change 提案,双仓评审;UE 端预创建 `cp-register-map.md` stub
- W36:Oracle 评审,确定接口冻结
- W38:双仓并行实施 + 集成测试

**SSOT**:
- CppTLM:本目录 `phase9-p2-cp-attach-via-axi.md`
- CppTLM 架构:`docs/soc_arch/architecture/18-pcie-endpoint-entry.md` (v0.9)
- UE:`docs/02_architecture/pcie-endpoint-entry.md` (v0.2.4) + 待创建 `docs/02_architecture/cp-register-map.md`

**跨仓版本对齐**:UE 端 v0.2.4 vs CppTLM 端 v0.9 不同步 — W34 提案时应统一到同步版本号。

---

## 附:本阶段(phase9)与传统 ADR/Change 流程的衔接

- **ADR-SOC-16** Task 1-15 已完成,Task 17-20 进入 P1
- **ADR-SOC-08** v5.5+ 集成前置测试已就位,UE 侧 4 项增量提议待回
- **ADR-SOC-09/10** 双 vendor + ModuleFactory 拓扑 → P3 战略阶段
- **pcie-ep-foundation change** (Oracle 已评审,archive 2026-09-10) 是 P2 战略基础
- **2026-09-09 用户决策**:"先打通 PCIe EP 协同,再考虑 CP" → P2 完全符合该决策

---

**下次 review**: W26 末(P0 完成后,验证排期)