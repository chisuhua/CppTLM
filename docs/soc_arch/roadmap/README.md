# SoC Architecture Roadmap

> **目录**: `docs/soc_arch/roadmap/` · **范围**: dGPU SoC 子项目的实施路线图与阶段规划
> **维护**: CppTLM Team (Sisyphus) · **最后更新**: 2026-09-16

## 目录索引

| 文件 | 视角 | 状态 | 摘要 |
|------|------|:----:|------|
| [`roadmap-mvp-to-v05.md`](./roadmap-mvp-to-v05.md) | v0.5 (历史快照) | 🔒 冻结 | MVP 切片 4 阶段 6-10 周(per ADR-SOC-06) |
| [`phase9-post-phase8-roadmap.md`](./phase9-post-phase8-roadmap.md) | v1.0 (W25+ 起步) | 🟢 活跃 | Phase 8 PCIe EP 整合收尾后的近期规划总览 |
| [`phase9-p0-minor-fixes.md`](./phase9-p0-minor-fixes.md) | v1.0 P0 | 🟢 活跃 | 5 项 Minor 问题清理(纯文档/Minor 修复,零跨仓) |
| [`phase9-p1-sm-gate-verification.md`](./phase9-p1-sm-gate-verification.md) | v1.0 P1 | 🟢 活跃 | SM 重构收尾(Task 17-20:Gate 验证 + archive) |
| [`phase9-p2-cp-attach-via-axi.md`](./phase9-p2-cp-attach-via-axi.md) | v1.0 P2 | 🟠 跨仓 | CP 寄存器接入 + UE 双路径(Oracle REFINE + YES 决策) |
| [`phase9-p3-strategic.md`](./phase9-p3-strategic.md) | v1.0 P3 | 🟡 战略 | ADR 修订 + Phase 9 雏形 |

## v0.5 → v1.0 演进关系

```
v0.5 (2026-Q2)            v1.0 (2026-Q3+ 起步)
─────────────────         ──────────────────────
ADR-SOC-06                Phase 1-8 完成
MVP 切片 4 阶段           SM 重构 Task 1-15 完成
                          PCIe EP 整合 (commit 832b26f7)
                          ↓
                          W25+ 进入下一阶段
                          (本目录 5 个 phase9-* 文件覆盖)
```

**关键转折点**:
- **v0.5 时代**:ADR-SOC-06 主导,MVP 切片式开发
- **v1.0 时代**:Phase 1-8 全部完成,进入"全链路收尾 + 跨仓集成"阶段
- **演进逻辑**:不是替代 v0.5,而是基于 v0.5 完成产物向上构建

## 命名规范

- **`roadmap-mvp-to-v05.md`**:历史快照(v0.5 视角,不再更新)
- **`phase9-post-phase8-roadmap.md`**:W25+ 总览
- **`phase9-p[0-3]-*.md`**:具体阶段文件
  - `p0`:纯 Minor 修复(零跨仓)
  - `p1`:单仓 Gate 验证(SM 重构收尾)
  - `p2`:跨仓协调(CP 接入)
  - `p3`:战略规划(ADR 修订 + Phase 9)

## 文件结构(每阶段文件统一模板)

```markdown
# [Stage Name]: [一句话目标]

> 元数据块:类别 / 阶段 / 优先级 / 日期 / 跨仓 / 关联 ADR / OpenSpec

## 1. 目标
## 2. 任务清单(表格化)
## 3. 依赖关系
## 4. 完成标准(DoD)
## 5. 风险与缓解
## 6. 跨仓协调事项(如适用)
```

## 关联文档

- 顶层:`docs/roadmap/` (历史快照,🚧 HISTORICAL 标注)
- 战略层:`docs/soc_arch/architecture/00-overview.md` (v1.0 总架构蓝图)
- ADR:`docs/soc_arch/adr/revision-plan-v1.0.md` (ADR 修订规划)
- OpenSpec 流程:`openspec/changes/` (变更提案)
- UE 端:`/workspace/project/UsrLinuxEmu/roadmap.md`
- 历史:`openspec/changes/archive/2026-09-10-2026-09-09-cpptlm-pcie-ep-foundation/` (CP 接入的战略决策基础)

## 更新纪律

- 阶段文件**不在任务完成后删除**,而是改 status 字段(🔒 冻结)
- 总览文件每周 review(周五),更新 status 看板
- 新阶段加入:复制模板 → 改名 `phase9-pX-*` → 在 README 索引
- 跨仓议题在阶段文件 §6 单独标注