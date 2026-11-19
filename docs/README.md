# CppTLM 文档中心

> **定位**: 本仓文档的**唯一总索引**。按 AGENTS.md §DOC HYGIENE 的「真源分级 + 4 层文档模型」组织 —— **先看层级，再看目录**。
> **最后更新**: 2026-11-19（docs 重组：删重复 / 清僵尸目录 / 归档 27 份时点快照 / 重建全量索引 / 断链清零）
> **规模**: 245 份文件 · 16 个子目录 · 4 份根级文档
> **权威性**: 本表只写**路径与职责**；**事实状态以各文档本体为准**（本表不复制状态，状态一律从被索引文档机械抽取）

---

## 1. 阅读路径（先按角色选入口）

| 你是 | 从这里开始 |
|------|-----------|
| **新成员** | [`ONBOARDING.md`](./ONBOARDING.md)（知识图谱生成的 ramp-up）→ 本表 §2 真源 1 |
| **写 TLM 模块** | [`guide/DEVELOPER_GUIDE.md`](./guide/DEVELOPER_GUIDE.md) · [`architecture/`](./architecture/)（真源 1） |
| **改 dGPU 业务** | [`designs/`](./designs/)（真源 2 三视角）· [`pcie/`](./pcie/)（实施笔记） |
| **做架构决策** | [`adr/`](./adr/)（真源 3，63 份）· [`architecture/README.md`](./architecture/README.md) 职责分工表 |
| **查历史/调研** | [`research/`](./research/)（附属 1）· [`archive/`](./archive/)（附属 3） |
| **跑验证** | [`validation/`](./validation/) · [`scripts/README.md`](../scripts/README.md) |
| **配 SoC 拓扑** | [`guide/TOPOLOGY_USER_GUIDE.md`](./guide/TOPOLOGY_USER_GUIDE.md) · [`requirements/`](./requirements/) |

---

## 2. 层级模型（真源分级 —— 冲突时的裁决顺序）

> **真源优先**：真源 3 > 真源 2 > 真源 1 > 附属层。冲突时**改附属、不改真源**。
> 详细规则见 [`../AGENTS.md`](../AGENTS.md) §DOC HYGIENE。

| 级别 | 目录 | 角色 | 修改门槛 |
|------|------|------|---------|
| **真源 3**（不可变决策） | [`adr/`](./adr/) | 63 份 ADR；签发后正文不可改，只追加 `## Status Update` | 仅追加 Status Update |
| **真源 2**（dGPU 应用架构） | [`designs/`](./designs/) | Board 内部 + SoC 业务 + Driver 接口 **三视角** | Oracle 评审 or ADR 触发 |
| **真源 1**（通用基础架构） | [`architecture/`](./architecture/) | TLM 框架 + 事务/错误/复位/拓扑/Metrics/可视化/协议中立基线 | Oracle 评审 or ADR 触发 |
| **附属 1**（调研） | [`research/`](./research/) | 框架级调研；**判据定义真源**，不承载「现状真相」 | 不进架构评审 |
| **附属 2**（实施笔记 + 跟踪） | [`implementation/`](./implementation/) · [`pcie/`](./pcie/) · [`superpowers/`](./superpowers/) | 引用真源 + OpenSpec，**不重发明架构决策** | 实施期短期，完结后归档 |
| **附属 3**（历史保留） | [`archive/`](./archive/) | 归档件（调研价值 / superseded 溯源）；**只读，不重写** | 仅 admin |

---

## 3. 目录索引

### 3.1 真源层

| 目录 | 内容 | 份数 | 入口 |
|------|------|-----|------|
| [`adr/`](./adr/) | 架构决策记录（CEP 24 · DGPU 15 · X 18 · 其他 6） | 64 | [`adr/README.md`](./adr/README.md)（全量索引，状态机械抽取） |
| [`designs/`](./designs/) | dGPU 应用架构三视角 | 14 | [`designs/README.md`](./designs/README.md) |
| [`architecture/`](./architecture/) | 通用基础架构（含 `cep/` 9 篇子文档 + `examples/`） | 36 | [`architecture/README.md`](./architecture/README.md) |

### 3.2 附属层

| 目录 | 内容 | 份数 | 入口 |
|------|------|-----|------|
| [`research/`](./research/) | 框架级调研（CEP 一致性 + gem5 协议族 + CppTLM 融合） | 18 | [`research/AGENTS.md`](./research/AGENTS.md)（索引） |
| [`implementation/`](./implementation/) | 实施笔记与契约载体（CEP ArchForge 对齐等） | 16 | [`implementation/README.md`](./implementation/README.md) |
| [`pcie/`](./pcie/) | PCIe 设备实施笔记（D1 Display / D-AXI） | 2 | [`pcie/driver-visible-minimal-soc.md`](./pcie/driver-visible-minimal-soc.md) |
| [`superpowers/`](./superpowers/) | 第 4 层实施跟踪（`plans/` 计划 + `specs/` 规格） | 10 | [`superpowers/README.md`](./superpowers/README.md) |

### 3.3 使用与需求

| 目录 | 内容 | 份数 | 入口 |
|------|------|-----|------|
| [`guide/`](./guide/) | 使用与开发指南（上手 / 开发 / 拓扑 / Python / CEP / UE driver） | 8 | [`guide/README.md`](./guide/README.md) |
| [`user-guide/`](./user-guide/) | Python 用户手册（E2E 建模模板 / 层次化 SoC / ConfigBuilder / Dashboard） | 2 | [`user-guide/README.md`](./user-guide/README.md) |
| [`requirements/`](./requirements/) | 产品需求（PRD） | 3 | [`requirements/README.md`](./requirements/README.md) |
| [`development/`](./development/) | 贡献指南（pre-commit / clang-format / 测试规范） | 1 | [`development/CONTRIBUTING.md`](./development/CONTRIBUTING.md) |

### 3.4 验证与跨仓

| 目录 | 内容 | 份数 | 入口 |
|------|------|-----|------|
| [`validation/`](./validation/) | 验证报告（CEP 实验 + 拓扑 oracle-pass + `raw/` 原始产物） | 18 | [`validation/README.md`](./validation/README.md) |
| [`cross_repo/`](./cross_repo/) | 跨仓镜像文档（HSK-9 SM 重构 / HSK-10 TLP 线 / HSK-11~12 ABI 精简） | 5 | [`cross_repo/README.md`](./cross_repo/README.md) |

### 3.5 归档与占位

| 目录 | 内容 | 份数 | 说明 |
|------|------|-----|------|
| [`archive/`](./archive/) | 历史归档（`architecture/` `designs/` `implementation/` `validation/` `tracking/` `process/` `roadmap/` `skills/`） | 42 | [`archive/README.md`](./archive/README.md)（分类索引）；**只读** |
| [`roadmap/`](./roadmap/) | **重定向存根** —— 已归档（2026-10-19），当前路线图真源 = [`../.rddf/roadmap.md`](../.rddf/roadmap.md) | 1 | 不再新增内容 |
| [`soc_arch/`](./soc_arch/) | **ArchForge 仓占位** —— dGPU SoC 设计文档已迁独立仓（VIRTUAL_PATHS 机制） | 1 | 设计变更走 ArchForge PR |

### 3.6 根级文档

| 文档 | 用途 |
|------|------|
| [`ONBOARDING.md`](./ONBOARDING.md) | 新人上手（知识图谱生成） |
| [`README.md`](./README.md) | 本文件 —— 总索引 |
| [`docs_audit_report.md`](./docs_audit_report.md) | 路径漂移审计状态（活状态 + 重生成命令） |
| [`migration-v2.2.md`](./migration-v2.2.md) | v2.1 → v2.2 迁移指南（BREAKING；被 `openspec/specs/legacy-modules-removal/spec.md` 引用） |

---

## 4. 4 层文档模型（同一份文档的四种角色）

```
┌─ 第 1 层 ─ 长期维护架构 ─ docs/architecture/ + docs/designs/
├─ 第 2 层 ─ 架构决策 ───── docs/adr/ADR-*.md
├─ 第 3 层 ─ OpenSpec ───── openspec/changes/<name>/{proposal,design,spec,tasks}.md
└─ 第 4 层 ─ 实施跟踪 ───── docs/superpowers/plans/ + specs/
```

**同步规则（单向：OpenSpec → 架构文档）**：改 change 的 `design.md` 若涉及设计意图变化 → 同步更新 `docs/designs/<topic>/architecture.md`；反向不强制（架构文档可以领先于实施）。

---

## 5. 维护约定

| 约定 | 说明 |
|------|------|
| **命名** | 架构 `NN-topic.md` · ADR `ADR-<域>-<NN>-<topic>.md` · 调研/笔记 `<YYYY-MM-DD>-<topic>.md` |
| **状态标注** | 文档本体写状态；**索引不复制状态**（ADR 索引由机械抽取生成） |
| **归档** | 实施期文档完结后迁 `docs/archive/<域>/`；归档件**不重写**，只追加标注 |
| **删除** | 仅删**真重复** / **空目录** / **僵尸目录**；有引用价值的一律归档而非删除 |
| **路径漂移** | `scripts/test/docs_sync_check.sh --strict`（pre-commit 自动跑） |
| **ADR 不可变** | `docs/adr/**` 签发后正文不改；变更决策须新开 ADR 走取代流程 |

---

## 6. 状态口径

- **计数**：本表份数为 `2026-11-19` 重组后的实测值 —— 目录内容变动时**请重算**，不要沿用旧值。
- **ADR 份数**：以 [`adr/README.md`](./adr/README.md) 的 **63 份**为准（上层 `AGENTS.md` 的「59 份」为旧口径，本次同步）。
- **CEP spec / change 计数**：见 [`../AGENTS.md`](../AGENTS.md) §PHASE STATE（**实时计数用 `openspec list --json` 取**，不要固化精确值）。

---

**维护**: CppTLM 开发团队 · architect（真源内容）· builder（索引同步）
