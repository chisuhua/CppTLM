# phase9-p3-strategic: 战略阶段(ADR 修订 + Phase 9 雏形)

> **类别**: SoC Architecture > Roadmap · **阶段**: P3 · **优先级**: 🟢 长期战略(W40+ 启动)
> **日期**: 2026-09-16 · **维护者**: Sisyphus · **跨仓**: 部分(ADR-SOC-09/10/14 涉及 UE 端)
> **关联 ADR**: [revision-plan-v1.0.md](../adr/revision-plan-v1.0.md) (B3-B4 已对齐) · [ADR-SOC-09-v1-nvidia-amd-dual-vendor.md](../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) · [ADR-SOC-10-module-factory-topology.md](../adr/ADR-SOC-10-module-factory-topology.md) · [ADR-SOC-14](../adr/)
> **关联 OpenSpec**: 待启动(战略 change)

**编号纪律**:本目录 P3-3 的 "**B5'-B8'**" 是**新定义**(避免与 [revision-plan-v1.0.md §6.1](../adr/revision-plan-v1.0.md) 的 B5=C1+C2 / B6=C4+C5+C6 / B7=A6+A7+README / B8=归档 撞号)。后者是 revision-plan 既定项,前者是本阶段独立 ADR 撰写项,**互不冲突**。

---

## 1. 目标

在 P0/P1/P2 三个执行梯队完成后,**定义下一阶段 (Phase 9+) 的方向**,包括:
- ADR 修订 B3-B4 收尾(状态升级 + Superseded 标注)
- ADR-SOC-09/10/14 等 Proposed 状态的 ADR 评审升级
- 本阶段独立新 ADR(B5'-B8')撰写(基于 P2 实施经验)
- Phase 9 雏形文件起草(基于 P2 实施经验)

**核心原则**:**战略阶段不直接动手实现,只产出决策文档**。

---

## 2. 任务清单

| ID | 任务 | 来源 | 成本 | 阻塞 |
|----|------|------|------|------|
| **P3-1** | ADR revision-plan **B3** 状态升级 | `revision-plan-v1.0.md` §B3 | 0.5 d | 无 |
| **P3-2** | ADR revision-plan **B4** Superseded 标注 | `revision-plan-v1.0.md` §B4 | 0.5 d | 无 |
| **P3-3** | **本阶段独立新 ADR(B5'-B8')**撰写 + 评审 | 基于 P2 实施经验 | 1 周 | P3-1/2 |
| **P3-4** | ADR-SOC-09 v1.0 双 vendor 战略升级 | ADR-SOC-09 Proposed → Accepted | 1 周 + Oracle | P2 完成 |
| **P3-5** | ADR-SOC-10 ModuleFactory 拓扑升级 | ADR-SOC-10 Proposed → Accepted | 1 周 + Oracle | P2 完成 |
| **P3-6** | ADR-SOC-14 v5.5+ 集成战略 | ADR-SOC-14 Proposed → Accepted | 1 周 + Metis | UE 反馈 |
| **P3-7** | Phase 9 启动文件:候选主题 + 范围 + 优先级 | 新建 `phase10-*.md` 雏形 | 1 周 | P3-3/4/5 |

### P3-1 / P3-2 / P3-3:ADR Revision Plan B3-B4 + 本阶段新 ADR B5'-B8'

**B3**(状态升级):已 Accepted 的 ADR 追加实施结果 Status Update(类似 B1 已做的)
- 涉及:ADR-SOC-01/05/06/07/08(已 Accepted)+ 实施证据

**B4**(Superseded 标注):v0.5 时代 ADR 与 v1.0 战略矛盾的明确标注
- 涉及:ADR-SOC-02/03(CU 黑盒 → SM 重构后已 superseded)
- ADR-SOC-04 标记 partial superseded(HSAPP 简化)
- **superseded-by 链接统一指向 [ADR-SOC-16-sm-microarchitecture.md](../adr/ADR-SOC-16-sm-microarchitecture.md)**(per SM 重构的实际实施依据);同时保留 revision-plan 中"Superseded by ADR-SOC-06 D2" 的早期参考链接

**B5'-B8'**(本阶段新 ADR + 评审):基于 P2 实施经验
- **B5'**:CP 寄存器架构(per P2-1 spec)
- **B6'**:UE 双路径模式(per P2-7)
- **B7'**:7-fix 闭环验收标准(per P2-9 Oracle 评审)
- **B8'**:Phase 9 总览

### P3-4:ADR-SOC-09 v1.0 双 vendor 升级

**当前状态**:Proposed
**目标**:升级到 Accepted(基于 P2 真实化经验)
**前置**:
- Metis 预评审(避免直接动手)
- P2-8(UE 5.5.8 真实化)经验数据
- UE 侧 ADR-088/089 反馈

**关键决策**:
- 双 driver stack 共享 PCIe EP / HBM / L2 / NoC(per ADR-SOC-09 D1)
- L3/L5/L6 双 vendor 路径(per ADR-SOC-09 D2)
- UsrLinuxEmu 跨仓依赖标注(per ADR-SOC-09 D3)

### P3-5:ADR-SOC-10 ModuleFactory 拓扑升级

**当前状态**:Proposed
**目标**:升级到 Accepted
**关键决策**:
- 9 类 SimModule P2-P5 层级容器(已 ✅ 完成 + Oracle 评审)
- ApuSoC 顶层(已 ✅ 完成)
- 单一入口 JSON 拓扑(已 ✅ 完成)

**升级依据**:P2-2 EP 转发规则实现 + dgpu_board_shell 现有 forwarding 模式验证

### P3-6:ADR-SOC-14 v5.5+ 集成战略

**当前状态**:Proposed
**目标**:升级到 Accepted(基于 UE 反馈)
**前置**:UsrLinuxEmu ADR-088/089 实际反馈 + 5.5.7/5.5.8 实施数据

### P3-7:Phase 9 雏形

**产出**:`docs/soc_arch/roadmap/phase10-*.md` 草案(尚未命名)
**范围**:基于 P0-P2 完成经验,定义下一阶段:
- 候选主题 1:SoC 级 crossbar 引入(NVIDIA NvSwitch / AMD XGMI 模式)
- 候选主题 2:多 host 共享 CP(VM 分区)
- 候选主题 3:IOMMU 4 级集成(SMMU / AMD IOMMUv2)
- 候选主题 4:用户态 doorbell(避免 kernel MMIO 瓶颈)
- 候选主题 5:PTX-EMU 真实化(per HSK-9 公告)

**优先级**:基于 P3-4/5/6 评审结果 + UE 反馈

---

## 3. 依赖关系

```
P0/P1/P2 全部完成 ──┬──> P3-1 (B3 状态升级) ──┐
                    │                            │
                    ├──> P3-2 (B4 Superseded) ──┼──> P3-3 (B5-B8 新 ADR)
                    │                            │
                    │   P2-8 (UE 真实化) 反馈 ──┼──> P3-4 (ADR-SOC-09)
                    │                            │
                    │   P2-7 (双路径模式) ──────┼──> P3-5 (ADR-SOC-10)
                    │                            │
                    │   UE ADR-088/089 反馈 ────┼──> P3-6 (ADR-SOC-14)
                    │                            │
                    └──> P3-3/4/5/6 完成 ──────────> P3-7 (Phase 9 雏形)
```

**关键依赖**:
- P3-4 / P3-5 依赖 P2 完成(真实化经验)
- P3-6 依赖 UE 反馈(双向)
- P3-7 是综合产出

---

## 4. 完成标准(DoD)

- [ ] P3-1:ADR-SOC-01/05/06/07/08 Status Update 已追加
- [ ] P3-2:ADR-SOC-02/03/04 Superseded 标注已加,链接指向 ADR-SOC-16
- [ ] P3-3:B5-B8 新 ADR 撰写 + Metis 预评审 + Oracle 复评 PASS
- [ ] P3-4:ADR-SOC-09 升级到 Accepted
- [ ] P3-5:ADR-SOC-10 升级到 Accepted
- [ ] P3-6:ADR-SOC-14 升级到 Accepted
- [ ] P3-7:`phase10-*.md` 草案发布到 `docs/soc_arch/roadmap/`,含 5 候选主题 + 优先级
- [ ] 整体:W42 末 phase9 全部完成,W43+ 进入 Phase 10

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| **P3-4 双 vendor 决策翻车**(大方向) | 中 | 高 | **强制**先 Metis 预评审 → 提案 → Oracle 复评,不直接动手;参考 NVIDIA Hopper / AMD MI300 公开材料 |
| **P3-3 B5-B8 新 ADR 范围爆炸** | 中 | 中 | 每个新 ADR 限定 ≤ 1 主题,避免大杂烩 |
| **P3-6 UE 反馈延迟** | 高 | 中 | 异步推进,UE 反馈到位再补 commit |
| **P3-7 Phase 9 雏形太早**(战略与执行脱节) | 中 | 中 | 雏形文件明确标"待 Phase 10 启动时细化",不强制承诺 |

---

## 6. 跨仓协调事项

**P3-4** 涉及 UE 端 driver stack 双 vendor 协调
**P3-6** 依赖 UsrLinuxEmu ADR-088/089 反馈

**协调节奏**:
- W40:W42 每周一次同步会
- W42 末 Phase 9 收官 + Phase 10 启动

**SSOT**:
- CppTLM 侧:`docs/soc_arch/architecture/00-overview.md` (v1.0 总架构蓝图)
- UE 侧:UsrLinuxEmu `roadmap.md`

---

## 7. 战略阶段对其他阶段的反馈机制

P3 战略产出将作为下一轮 P0-P2 的输入:

```
P3 战略 ───> [下一轮 P0: Phase 10 Minor 修复]
        ───> [下一轮 P1: Phase 10 Gate 验证]
        ───> [下一轮 P2: Phase 10 跨仓核心议题]
```

**反馈周期**:约 8 周 / 轮,形成可持续的"修复→验证→跨仓→战略"循环。

---

**下次 review**: W42 末(Phase 9 收官,Phase 10 启动)