# v1-architecture Specification

## Purpose
TBD - created by archiving change 2027-02-09-cpptlm-dgpu-soc-v1-architecture. Update Purpose after archive.
## Requirements
### Requirement: v1-architecture-blueprint

The system SHALL 交付 `docs/soc_arch/architecture/00-overview.md` 作为 dGPU SoC v1.0 总架构蓝图，包含：
- 7 层架构（L1..L7）统一编号 + 导航与决策矩阵
- v1.0 MVP / v1.1 完整版范围矩阵（单一权威表）
- D1..D8 关键设计决策与备选方案
- 4 数据流视图（命令流 / 数据流 / 配置流 / 中断流）
- 与 v0.5 / Phase 8 兼容性分析 + 扩展点
- 8 份 ADR-SOC 引用矩阵 + 25+ 模块微架构引用矩阵
- R1..R7 风险与缓解
- 反模式（明确不做）

#### Scenario: 蓝图文档交付且通过 Oracle 评审

- **WHEN** 读取 `docs/soc_arch/architecture/00-overview.md`
- **THEN** 文档必须覆盖上述全部内容（7 层架构 + D1-D8 + 4 数据流视图 + 范围矩阵 + 引用矩阵 + 风险）
- **AND** 文档必须通过 Oracle 评审 PASS（含 P0/P1 修复后复审，2027-02-09 复审 PASS-WITH-CONDITIONS → v3.1-PASS）

#### Scenario: 蓝图与子架构文档一致

- **WHEN** 对照 10 份子系统架构文档（01-10）与 00-overview.md
- **THEN** 编号体系（L1..L7）与范围矩阵保持一致
- **AND** ADR-SOC 引用矩阵指向实际存在的 ADR 文件

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — v1.0 总架构蓝图（00-overview.md 已交付 + Oracle PASS）

