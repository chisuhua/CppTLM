# phase9-p1-sm-gate-verification: SM 重构收尾(Gate 验证 + Archive)

> **类别**: SoC Architecture > Roadmap · **阶段**: P1 · **优先级**: 🟡 短期高价值(单仓 Gate 验证)
> **日期**: 2026-09-16 · **维护者**: Sisyphus · **跨仓**: ❌(PTX-EMU 公告 W40+)
> **关联 ADR**: [ADR-SOC-16-sm-microarchitecture.md](../adr/ADR-SOC-16-sm-microarchitecture.md) (Task 1-20)
> **关联 OpenSpec**: HSK-9 (跨仓公告,per PTX-EMU 协议)

---

## 1. 目标

完成 ADR-SOC-16 SM 重构的 **Gate 验证** 全部 Task(Task 17-20),**正式归档 SM 重构 change**。这是继 Phase 8 之后的下一个"小里程碑"。

**核心交付物**:
- 8 Bundle 接线测试覆盖(G4 Gate)
- L3 集成测试 + bit-exact Gate(G3/G5 Gate)
- SM 重构 change OpenSpec 归档 + HSK-9 公告(PTX-EMU 端改造延期,本仓先 archive)

---

## 2. 任务清单

| ID | 任务 | 来源 | 成本 | 阻塞 |
|----|------|------|------|------|
| **P1-1** | SM Task 17:L2 Bundle 接线测试(20+ assertions)+ `test_sm_scalar_alu_tlm.cc` | ADR-SOC-16 §6 Task 17 | 3-5 d | P0-3(Task 16) |
| **P1-2** | SM Task 18:L3 集成测试(30+ assertions)+ IComputeDevice 完整 + bit-exact Gate | ADR-SOC-16 §6 Task 18 | 5-7 d | P1-1 |
| **P1-3** | SM Task 19:OpenSpec archive + HSK-9 公告发布 | ADR-SOC-16 §6 Task 19 | 1 d | P1-2 |
| **P1-4** | 修复 `completion_ring_mvp.cc:20` TODO(done_out 转发 + MSI-X delivery) | `src/tlm/gpu/completion_ring_mvp.cc:20` | 1 d | 无 |
| **P1-5** | 修复 `tmu_types_mvp.hh:59` FUTURE 字段(PRIORITY/EVICT_OLDEST/LRU) | `include/tlm/gpu/tmu_types_mvp.hh:59` | 0.5 d | 无 |

### P1-1:SM Task 17(L2 Bundle 接线测试)

**目标 Gate**:**G4** — 8 Bundle 全覆盖
**产出**:
- `test_sm_l2_bundle_wiring.cc`(20+ assertions)
- `test_sm_scalar_alu_tlm.cc` 单模块测试
- 8 Bundle 端口连通性矩阵

**测试覆盖**:
- ComputeReqBundle / ComputeRespBundle(已在 Phase 1)
- SmBundle / WarpBundle / RegFileBundle / HazardBundle(Task 17 新增)
- CacheReqBundle / CacheRespBundle(CP↔Memory,Task 17 增强)

### P1-2:SM Task 18(L3 集成 + bit-exact Gate)

**目标 Gate**:**G3** (SM-owns-state 跨子模块状态) + **G5** (bit-exact Gate) + **G13** (146+ assertions)
**产出**:
- `test_sm_l3_integration.cc`(30+ assertions)
- `test_bit_exact_gate.cc`(PC/cycle 观测 vs SM 模拟对照)
- IComputeDevice 完整 15 方法测试

### P1-3:SM Task 19(OpenSpec Archive + HSK-9)

**流程**:
1. 跑 `openspec validate <change-name> --strict`(P1-2 PASS 后)
2. 提交 `openspec archive <change-name>`(归档到 `openspec/changes/archive/`)
3. 草拟 HSK-9 公告(PTX-EMU 仓 `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md` 镜像)

**公告内容**:
- 12 子模块接口冻结
- IComputeDevice 15 方法契约
- 旧 kernel_launch/compute-unit/ptx-emu 子模块已物理删除
- 跨仓语义变更提示(若 PTX-EMU 侧有耦合)

### P1-4:Completion Ring TODO

**当前状态**:`src/tlm/gpu/completion_ring_mvp.cc:20` 注释 TODO
**修复**:done_out 端口信号转发 + irq_out MSI-X delivery 触发
**测试**:新增 `test_completion_ring_irq_delivery.cc`

### P1-5:TMU Types FUTURE 字段

**当前状态**:`include/tlm/gpu/tmu_types_mvp.hh:59` FUTURE 注释
**修复**:补全 PRIORITY / EVICT_OLDEST / LRU 三种替换策略字段 + 默认值 + 单测

---

## 3. 依赖关系

```
P0-3 (Task 16 删除) ──> P1-1 (L2 Bundle) ──> P1-2 (L3 集成) ──> P1-3 (Archive)
                                              │
                                              ├──> P1-4 (Completion Ring)
                                              └──> P1-5 (TMU Types)
```

**关键依赖**:
- P1-1 必须等 P0-3(避免命名冲突)
- P1-3 必须等 P1-2(无未完成 Gate 测试)
- P1-4 / P1-5 是独立 Minor,可与 P1-2 并行(但建议 P1-2 主线下顺手做)

---

## 4. 完成标准(DoD)

- [ ] P1-1:`test_sm_l2_bundle_wiring.cc` 编译通过,20+ assertions PASS
- [ ] P1-2:`test_sm_l3_integration.cc` + `test_bit_exact_gate.cc` 全绿,G3/G5/G13 PASS
- [ ] P1-3:`openspec archive <change>` 成功,HSK-9 公告草稿发布
- [ ] P1-4:`test_completion_ring_irq_delivery.cc` 验证 MSI-X delivery 触发
- [ ] P1-5:`tmu_types_mvp` 字段补全,默认行为测试
- [ ] 整体:`./test.sh --mode off` 全绿,SM 测试套件覆盖率 ≥ 80%
- [ ] SM 重构 change 进入 `openspec/changes/archive/`,状态 🔒 冻结

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| bit-exact Gate 与 PC/cycle 观测有偏差 | 中 | 高 | 先做小规模 kernel 比对(NN/FFT/MM),扩展到完整 kernel 集合 |
| HSK-9 公告跨仓协调失败 | 低 | 中 | PTX-EMU 端改造延期不影响本仓 archive,先发草稿 |
| P1-4 MSI-X delivery 触发链路深 | 中 | 中 | 复用 Phase 7 MSI-X 现有实现,只补 done_out 触发 |
| Task 18 L3 集成测试发现 SM 设计 bug | 中 | 高 | 留 1 周 buffer,bug 修复不回退 ADR,仅修订 spec |

---

## 6. 跨仓协调事项

**P1-3 的 HSK-9 公告**(per `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md`):
- 在本仓发"待 PTX-EMU 回应"草稿
- PTX-EMU 端改造**延期** 不影响本仓 archive
- W40+ 双仓再 sync PTX-EMU 适配进度

**其他无跨仓**。P1 全部在 CppTLM 仓内。

---

**下次 review**: W30 末(P1-1 完成,验证 G4 Gate)