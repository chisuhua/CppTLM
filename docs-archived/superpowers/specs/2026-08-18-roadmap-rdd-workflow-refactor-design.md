# Roadmap Refactor — rdd-workflow v3.0+ 兼容 + 嵌套业务子阶段

> **Document ID**: IMPL-018-RoadmapRefactor
> **Version**: 1.0
> **Date**: 2026-08-18
> **Status**: 🔄 Draft（待用户 review）
> **Author**: Sisyphus(基于 Oracle 5m12s 设计分析 + 4 项 Q&A 用户决策)
> **Parent Roadmap**: [`roadmap.md`](../../roadmap.md) v2.3 → v3.0
> **依赖框架**: [`/workspace/project/rdd-workflow`](../../../../rdd-workflow) v3.0+(`_lib/roadmap_state.py` + `_lib/roadmap_sprint.py`)
> **关联文档**:
> - [`/workspace/project/rdd-workflow/skills/roadmap/SKILL.md`](../../../../rdd-workflow/skills/roadmap/SKILL.md)
> - [`docs/architecture/01-hybrid-architecture-v2.1.md`](../../architecture/01-hybrid-architecture-v2.1.md)
> - [`docs/superpowers/specs/2026-07-17-hsk-4-5-responses.md`](2026-07-17-hsk-4-5-responses.md)（HSK-4/5 跨仓绑定背景）

> **本文档定位**: **设计视角**——以嵌套阶段语法(`phase-N.M`)重构 CppTLM roadmap,使其满足 rdd-workflow v3.0+ 模板的同时保留 APU/dGPU 业务语义。**实施视角**走 `writing-plans` skill(本文档末尾 §11 列出 4 步顺序)。

---

## 0. 阅读引导

本文档解决 **3 个客观问题**:

| 问题 | 来源 | 严重度 |
|------|------|-------:|
| Q1. roadmap.md 与 rdd-workflow 模板结构性不兼容 | `/workspace/project/CppTLM/roadmap.md`(23095 bytes)无 `### .*? \(phase-\d+\)` 形式、无 `#### 任务分类`、无 kebab-case cat_id | 🔴 6 类 CRITICAL |
| Q2. rdd-workflow 默认 regex 只识别平铺 `phase-N`,不支持嵌套 | `_lib/roadmap_state.py` L385 `r"\((phase-\d+)\)"` 严格匹配 `\)` 锚定 | 🟡 解析自然兼容,但 `advance_phase` 聚合逻辑缺失 |
| Q3. 跨仓引用(UsrLinuxEmu ADR-088 v3)与子阶段(7.A-F / 9.0-9.6)语义无法平铺 | CppTLM 业务演进路径与 rdd-workflow 通用 phase-N 模型维度不同 | 🟡 需设计嵌套语法 |

**本文档结构**:
- **§1 范围与目标** — 重构边界 + 成功标准 + 非目标
- **§2 现状分析** — 当前 roadmap 不合规项 + rdd-workflow 解析约束
- **§3 嵌套语法设计** — `phase-N.M` 命名方案 + 业务标签保留
- **§4 5 个顶层 phase-N 映射** — Phase 5..Phase 9 → phase-1..phase-5
- **§5 伞分类表 + 子阶段表达** — 父 phase 结构 + 子阶段 heading + 硬性约束
- **§6 跨仓 ADR 引用** — UsrLinuxEmu ADR-088 v3 的双重链接方案
- **§7 待办段 + AUTO-SPRINT 哨兵位置** — 共存策略
- **§8 rdd-workflow 兼容层改动清单** — `_lib/roadmap_state.py` 3 处 + 测试 + 文档
- **§9 回退方案** — Fallback A / B
- **§10 风险与执行顺序** — 6 项风险 + 4 步串行 + Effort 估算
- **§11 验收标准** — 5 个 Verification Gate
- **§12 决策点汇总** — D1-D8(含用户 Q&A 4 项)
- **§13 修订历史**

---

## 1. 范围与目标(Scope & Goals)

### 1.1 重构目标(Goals)

- **G1. 模板兼容**: roadmap.md 满足 rdd-workflow v3.0+ `_lib/roadmap_state.py` 全部解析路径(`init_state` / `render_status_view` / `validate_change` / `advance_phase` / `get_phase_categories` / `get_phase_themes`)
- **G2. 业务语义保留**: Phase 5..Phase 9 业务名 + 子阶段(7.A-F / 9.0-9.6)继续在 heading 文本中可见,跨仓引用(ADR-088 v3)不丢失
- **G3. 嵌套阶段支持**: 子阶段用 `phase-N.M` 形式,change meta 可指向子阶段(如 `phase: phase-3.3`)
- **G4. 状态推进正确**: `advance_phase` pre-check 聚合父+子阶段,避免父 phase 平凡通过
- **G5. 文档路径 0 断链**: 跨仓/同仓文档对 Phase 4/Phase 7.A-F / Phase 9.0-9.6 的引用全部可达

### 1.2 非目标(Non-Goals)

- **NG1. 不修改 rdd-workflow 模板默认结构**(平铺 `phase-N` 仍完全支持,新增嵌套是可选扩展)
- **NG2. 不重构 CppTLM 业务阶段**(APU/dGPU 演进路径不动,只改表达形式)
- **NG3. 不迁移 openspec/changes 历史归档**(`openspec/changes/archive/` 不动)
- **NG4. 不引入新依赖**(不增加 rdd-workflow 之外的解析库)
- **NG5. 不做 ADR 治理**(UsrLinuxEmu ADR-088 v3 引用形式不改,仅在 roadmap 中加双链接)

### 1.3 成功标准(Success Criteria)

| ID | 标准 | 验证手段 |
|----|------|----------|
| SC-1 | `roadmap validate <change>` 对 3 个活跃 change(cpptlm-d1-p1-pipeline-scoreboard、2026-06-24-gpu-soc-phase8b-core、2026-06-24-gpu-soc-phase8c-advanced)返回 `✅ Change "..." 验证通过` | `bash skills/roadmap/scripts/validate.sh <name>` |
| SC-2 | `rdd-doctor --json` 在 state / plan-tdd / roadmap-meta / proposal-table / tasks-checkbox / migration-residue 6 类检查中返回 `findings: []` | `bash skills/rdd-doctor/scripts/doctor.sh --json` |
| SC-3 | `roadmap_state.render_status_view` 显示当前阶段 phase-3(in_progress)+ phase-3.1 标 completed(2026-06-11)+ 其他子阶段 pending | `python3 _lib/roadmap_state.py::render_status_view(roadmap, state)` |
| SC-4 | `roadmap_state.advance_phase` dry-run 能正确识别下一阶段为 phase-4(跳过 phase-3.1..phase-3.6 子阶段) | python3 单元测试 `tests/unit/test_roadmap_state.py::test_advance_skips_subphases` |
| SC-5 | `roadmap_sprint.update_roadmap` 调用后文件尾部 AUTO-SPRINT 哨兵之间表格更新,**哨兵外内容(包括 `## 待办`)原样保留** | diff `roadmap.md` 前后内容,sentinels 外 bytes 不变 |

---

## 2. 现状分析(Current State)

### 2.1 CppTLM roadmap.md 不合规项(实测)

> 数据来源: `bash rdd-doctor/scripts/doctor.sh --json`(2026-08-17) + python regex 扫描

| 不合规项 | 当前状态 | 影响 |
|----------|---------|------|
| `###<name> (phase-N)` 形式 | ❌ 0 处(用 `## Phase N` 二级 + `### N.M` 三级,但**无 `(phase-N)` 后缀**) | `validate_change` 全部失败 |
| `**当前阶段**:` 字段 | ⚠️ 自由文本 `**当前阶段**: Phase 7 (APU-first) + Phase 9 (dGPU-first, per ADR-088 v3) **并行**` | `advance_phase` 无法定位 phase-N |
| `**版本**:` 字段 | ❌ 缺失(用 `> **Version**: 2.3` blockquote) | 模板必备 |
| `**最后更新**:` 字段 | ❌ 缺失(用 `> **Last Updated**: 2026-08-15`) | 模板必备 |
| `#### 任务分类` 段 | ❌ 0 处(用 `\| 子任务 \| 状态 \| ... \|` 自定义表格) | `get_phase_categories` / `validate_change` 解析失败 |
| kebab-case cat_id | ❌ 全部不合规(52 个"分类 ID"候选项,如 `7.A GPU 基础设施` 不符 `^[a-z][a-z0-9-]*$`) | `get_phase_categories` 过滤掉全部 |
| `<!-- AUTO-SPRINT-* -->` 哨兵 | ❌ 缺失 | `roadmap_sprint.update_roadmap` 静默 append 到末尾 |
| `.rddf/state/roadmap-state.json` | ❌ 缺失(`.rddf/state/` 下只有 `trace/` 子目录) | `render_status_view` / `advance_phase` 失败 |
| `openspec/changes/*/roadmap-meta.yaml` | ❌ 0 个(3 个活跃 change 未附带 meta) | `validate_change` 需 meta |

### 2.2 rdd-workflow 解析约束(基于 `_lib/roadmap_state.py` 实测)

| 解析函数 | 关键 regex / 逻辑 | 行号 | 对嵌套 `phase-N.M` 的天然行为 |
|---------|-------------------|------|------------------------------|
| `validate_change` phase_pattern | `rf"### .*? \({re.escape(change_phase)}\)"` | L233 | `re.escape("phase-3")` = `phase\-3`,pattern 末尾 `\)` 要求右括号紧跟 `3` → `(phase-3)` **不误匹配** `(phase-3.1)`(因 `3` 后是 `.` 不是 `)`)✅ |
| `validate_change` section 边界 | `### .*? \({phase}\).*?(?=\n### \|\n## \|\Z)` | L238 | 父 phase section 在首个 `###` 子 heading 处截断 → **子阶段必须放在父伞表之后**,否则父伞表被截掉 |
| `advance_phase` 下一阶段 | `re.findall(r"\((phase-\d+)\)", content)` | L385 | `(phase-3.1)` 中 `3` 后非 `)`,**不匹配** → 顶层列表 `[phase-1..phase-5]`,子阶段天然不可见,文档顺序推进 ✅ |
| `advance_phase` pre-check | 只检查 `state["phases"][current]["categories"]` | L353-373 | ⚠️ **子阶段注册在 phase-3.1..3.6 时,父 phase-3 categories 为空 → 平凡通过 → 提前推进风险** |
| `get_phase_categories` | `re.match(r"^[a-z][a-z0-9-]*$", cat_id)` | L474 | 仅过滤 kebab-case,不解析 phase 层级 |
| `get_phase_themes` | 5 列 `\| 分类ID \| 名称 \| 描述 \| 优先级 \| 预期改进方向 \|` | L549-565 | 不关心 phase 层级,只解析当前 phase section 内的表 |

### 2.3 CppTLM 当前业务阶段(从 roadmap.md L36-242 提取)

```
Phase 4: Hierarchy Core                ✅ 完成 2026-Q2 (历史,不进入)
Phase 5: Protocol Bridge              ⏳ 待启动
Phase 6: Multi-Cluster SoC Validation ⏳ 待启动(6.1/6.4 拆至 7.F)
Phase 7: CPU+GPGPU Fused SoC (APU)    🚀 进行中 (子阶段 7.A-F)
  7.A GPU 基础设施 ✅ 2026-06-11
  7.B ComputeUnit 黑盒 🟡
  7.C Coherence Protocol 集成 🟡 (🔴 最高风险)
  7.D TCC Bridge + 内存层次 🟡
  7.E Multi-CU + NoC 🟡
  7.F Full APU SoC Demo 🟡
Phase 8: APU 验证 + 性能基准          📋 计划中 (子阶段 8.1-8.4)
Phase 9: dGPU-first SoC 集成          🔄 启动中 (子阶段 9.0-9.6, 跨 UsrLinuxEmu)
  9.0 CppTLM 基础设施 🟡
  9.1 CppTLM PCIe 模块 🟡 (🔴 最高风险)
  9.2 CppTLM DMA + ComputeUnit 🟡
  9.3 dGPU amdgpu C ABI 完整 🟡
  9.4 callback 路径 + 集成 🟡
  9.5 nouveau path 📋 optional
  9.6 文档同步 + 归档 🟡
Phase 10: APU 升级预留                📋 占位 (不进入)
```

---

## 3. 嵌套语法设计(Nested Phase Syntax)

### 3.1 命名方案:phase-N.M 数字后缀

**语法规则**:
- 顶层 phase: `phase-N` (N ∈ 1..5)
- 子阶段: `phase-N.M` (M ∈ 1..6 或 0..6)
- regex: `phase-\d+(?:\.\d+)?`
- 顶层过滤: `phase-\d+` 且不含 `.`

**示例映射**(Phase 7 子阶段 7.A-7.F → phase-3.1-phase-3.6):

| 业务标签 | 机器 ID | 转换说明 |
|---------|---------|----------|
| 7.A GPU 基础设施 | `phase-3.1` | 字母 → 数字序号,顺序保持 |
| 7.B ComputeUnit 黑盒 | `phase-3.2` | 同上 |
| 7.C Coherence Protocol 集成 | `phase-3.3` | 同上 |
| 7.D TCC Bridge + 内存层次 | `phase-3.4` | 同上 |
| 7.E Multi-CU + NoC | `phase-3.5` | 同上 |
| 7.F Full APU SoC Demo | `phase-3.6` | 同上 |

Phase 9 子阶段 9.0-9.6 → `phase-5.0`-`phase-5.6`,1:1 直接映射(原本就是数字,无损)。

### 3.2 业务标签保留规则

**业务名出现在 heading 文本中,机器 ID 出现在 `(phase-N.M)` 后缀**:

```markdown
### Phase 7: CPU+GPGPU Fused SoC (APU-first) (phase-3)
### 7.A GPU 基础设施 (phase-3.1)
```

- ✅ 跨仓引用(UsrLinuxEmu ADR-088 中 "Phase 7.C"、"9.1 PCIe" 等表述)继续有效
- ✅ AGENTS.md / ONBOARDING.md / README.md 中业务名不需批量修改
- ✅ GitHub anchor 从 heading 文本生成,业务名相同 → 跨仓文档对 anchor 的引用基本不变

### 3.3 命名方案决策对比

| 方案 | 优点 | 缺点 | 选择 |
|------|------|------|:----:|
| A. `phase-N.M` 数字(7.A → phase-3.1) | 数字天然有序;9.0-9.6 1:1 无损;regex 简洁 | 业务标签 7.A 需在 heading 文本手动标注 | ✅ **推荐** |
| B. `phase-N.A` 字母(7.A → phase-3.A) | 与业务标签 1:1,人工直观 | 9.0-9.6 混用风格(`.0` vs `.A`);字母排序需额外定义 | ❌ |
| C. 平铺 `phase-3`…`phase-9`(子阶段升级顶层) | 零嵌套、零改动 | 丢失业务主阶段聚合语义,与用户"伞分类"决策冲突 | ❌ |

---

## 4. 5 个顶层 phase-N 映射(Top-Level Phase Mapping)

### 4.1 映射表

| rddf phase-N | 业务 Phase | 状态 | 子阶段数 | 风险标签 | 备注 |
|-------------|-----------|:----:|:-------:|---------|------|
| `phase-1` | Phase 5: Protocol Bridge | ⏳ 未开始 | 0 | 🟡 Med | 单阶段,无伞分类,直接列 5.1-5.5 子任务 |
| `phase-2` | Phase 6: Multi-Cluster SoC Validation | ⏳ 未开始 | 0 | 🟡 Med | 6.1/6.4 拆至 phase-3.6,需在描述列说明 |
| `phase-3` | Phase 7: CPU+GPGPU Fused SoC (APU-first) | 🔄 进行中 | 6 | 🔴 最高风险 7.C | **当前活跃**,伞分类表含 gpu-infra/cu-blackbox/coherence/tcc-bridge/multi-cu-noc/apu-demo |
| `phase-4` | Phase 8: APU 验证 + 性能基准 | 📋 计划中 | 4 | 🟡 Med | 伞分类表含 apu-validation/perf-baseline/regression/perf-report |
| `phase-5` | Phase 9: dGPU-first SoC 集成 | 🔄 启动中 | 7 | 🔴 最高风险 9.1 | 跨 UsrLinuxEmu ADR-088 v3,72 C ABI,伞分类表含 cpptlm-infra/pcie-module/dma-computeunit/amdgpu-cabi/callback-path/nouveau-path/docs-archive |

**Phase 4(已完成 2026-Q2)+ Phase 10(占位)不进入重构后 roadmap**。在 roadmap.md 元信息区加一行说明:

```markdown
> **历史**: Phase 4 已于 2026-Q2 完成(见 [`AGENTS.md`](../../AGENTS.md) §已完成里程碑 / git history);
> Phase 10 为占位预留,启动条件未达,不纳入本 roadmap。
```

### 4.2 子阶段 ID 分配表

#### phase-3(Phase 7: APU-first)

| 子阶段 ID | 业务标签 | 状态 | 风险 | 伞分类 ID |
|----------|---------|:----:|:----:|----------|
| `phase-3.1` | 7.A GPU 基础设施 | ✅ 完成 2026-06-11 | 🟢 Low | `gpu-infra` |
| `phase-3.2` | 7.B ComputeUnit 黑盒 | 🟡 Pending | 🟡 Med-High | `cu-blackbox` |
| `phase-3.3` | 7.C Coherence Protocol 集成 | 🟡 Pending | 🔴 **最高风险** | `coherence` |
| `phase-3.4` | 7.D TCC Bridge + 内存层次 | 🟡 Pending | 🟡 Med | `tcc-bridge` |
| `phase-3.5` | 7.E Multi-CU + NoC | 🟡 Pending | 🟢 Low | `multi-cu-noc` |
| `phase-3.6` | 7.F Full APU SoC Demo(承接原 6.1/6.4) | 🟡 Pending | 🟢 Low | `apu-demo` |

#### phase-4(Phase 8: APU 验证)

| 子阶段 ID | 业务标签 | 状态 | 风险 | 伞分类 ID |
|----------|---------|:----:|:----:|----------|
| `phase-4.1` | 8.1 Trace alignment tooling | 📋 计划 | 🟢 Low | `apu-validation` |
| `phase-4.2` | 8.2 端到端 correctness check | 📋 计划 | 🟡 Med | `perf-baseline` |
| `phase-4.3` | 8.3 性能 dashboard | 📋 计划 | 🟢 Low | `regression` |
| `phase-4.4` | 8.4 Regression baseline | 📋 计划 | 🟡 Med | `perf-report` |

#### phase-5(Phase 9: dGPU-first,跨仓)

| 子阶段 ID | 业务标签 | 状态 | 风险 | 伞分类 ID |
|----------|---------|:----:|:----:|----------|
| `phase-5.0` | 9.0 CppTLM 基础设施(`cpptlm_core` STATIC→SHARED) | 🟡 Pending | 🟡 Med | `cpptlm-infra` |
| `phase-5.1` | 9.1 CppTLM PCIe 模块(`PciHostTLM` / `PciDeviceTLM` / `PCIBridgeTLM`) | 🟡 Pending | 🔴 **最高风险** | `pcie-module` |
| `phase-5.2` | 9.2 CppTLM DMA + ComputeUnit | 🟡 Pending | 🟡 Med | `dma-computeunit` |
| `phase-5.3` | 9.3 dGPU amdgpu C ABI 完整 | 🟡 Pending | 🟡 Med | `amdgpu-cabi` |
| `phase-5.4` | 9.4 callback 路径 + 集成(11 callback + 1 register) | 🟡 Pending | 🟡 Med | `callback-path` |
| `phase-5.5` | 9.5 (可选项) nouveau path | 📋 Optional | 🟢 Low | `nouveau-path` |
| `phase-5.6` | 9.6 文档同步 + 归档 | 🟡 Pending | 🟢 Low | `docs-archive` |

### 4.3 phase-1 / phase-2 子任务处理

**phase-1(Phase 5: Protocol Bridge)和 phase-2(Phase 6: Multi-Cluster SoC Validation)** 当前**未拆分**为子阶段,直接列子任务(5.1-5.5 / 6.1-6.4),无需嵌套:

```markdown
### Phase 5: Protocol Bridge (phase-1)
**目标**: ...
**状态**: ⏳ 未开始
**完成条件**:
  - [ ] 所有分类的 change 完成

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| protocol-bridge | Protocol Bridge 集成 | 5.1-5.5 子任务(单阶段,未拆分) | P0 | 协议桥接；MOESI 升级路径 |
```

> **未来拆分**: 若 Phase 5/6 后续拆分为子阶段,按 phase-3.1-3.6 模式追加;新子阶段 ID 自动续编。

---

## 5. 伞分类表 + 子阶段表达(Umbrella Table + Subphase Headings)

### 5.1 父 phase 完整结构(以 phase-3 为例)

```markdown
### Phase 7: CPU+GPGPU Fused SoC (APU-first) (phase-3)
**目标**: 端到端实现 APU 形态 CPU+GPGPU 融合 SoC 仿真——2 个 CPU 流量源 + 4 个 GPU Compute Unit + 共享 memory + 单一 Coherence 域
**状态**: 🔄 进行中
**前置阶段**: phase-2
**完成条件**:
  - [ ] phase-3.1 ~ phase-3.6 全部完成
  - [ ] [apu_soc_emitter.py](../../scripts/topology/apu_soc_emitter.py) 端到端运行通过
  - [ ] `[gpu][phase6]` Catch2 标签用例 100% PASS

#### 任务分类                          ← 【硬性约束】必须先于所有 ### 子阶段 heading
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-infra | 7.A GPU 基础设施 | ✅ 完成 2026-06-11 (phase-3.1) | P0 | 发起器建模；内存层次 |
| cu-blackbox | 7.B ComputeUnit 黑盒 | (phase-3.2) | P0 | tick loop；inflight kernel |
| coherence | 7.C Coherence 集成 | 🔴 最高风险 (phase-3.3) | P0 | MOESI 简化版；snoop fanout |
| tcc-bridge | 7.D TCC Bridge + 内存层次 | (phase-3.4) | P1 | write coalescing；snoop fan-in |
| multi-cu-noc | 7.E Multi-CU + NoC | (phase-3.5) | P1 | BidirectionalPortAdapter |
| apu-demo | 7.F Full APU SoC Demo | 承接原 6.1/6.4 (phase-3.6) | P0 | 端到端验证 |

### 7.A GPU 基础设施 (phase-3.1)
**业务标签**: Phase 7.A
**状态**: ✅ 完成 2026-06-11
**验收**:
  - `cpptlm_tests "[gpu]"` 通过
  - `configs/gpu_standalone.json` 可执行
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-bundle | GPU Bundle 扩展 | `include/bundles/compute_bundles_tlm.hh` 新增 kernel_id/workgroup_id/wavefront_id 字段 | P0 | bundle schema 扩展 |
| gpu-init | GPU 基础设施 | `include/tlm/gpu/gpu_tlm.hh` v0 + REGISTER_CHSTREAM + `configs/gpu_standalone.json` | P0 | gpu_standalone 端到端 |

### 7.B ComputeUnit 黑盒 (phase-3.2)
**业务标签**: Phase 7.B
**状态**: 🟡 Pending
**风险**: 🟡 Med-High
**范围**: `ComputeUnitTLM`(tick loop + inflight_kernel_reqs_ map + workgroup_progress_);CrossbarTLM 扩展 GPU 地址路由;CacheTLM 临时 bypass GPU 请求
**验收**:
  - `configs/apu_demo_v1.json` 端到端运行
  - `cpptlm_tests "[gpu][phase6]"` PASS
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cu-tickloop | ComputeUnitTLM tick loop | ... | P0 | tick 实现；inflight |
| xbar-gpu-routing | CrossbarTLM GPU 地址路由 | ... | P1 | GPU 地址解析 |
| cache-bypass | CacheTLM 临时 bypass GPU | ... | P1 | bypass 路径 |

### 7.C Coherence Protocol 集成 (phase-3.3)   ← 🔴 最高风险
**业务标签**: Phase 7.C
**状态**: 🟡 Pending
**风险**: 🔴 **最高风险**
**范围**: CacheTLM protocol-aware 改造(`CacheLine = {data, state, sharers}` + 6×6 状态转换表);CoherenceDomain 与 CacheTLM 集成(snoop callback + lookup_home_node)
**验收**:
  - `configs/apu_demo_v2.json` 跨 cache 一致性
  - `cpptlm_tests "[coherence][gpu]"` PASS
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cache-state-machine | CacheTLM state machine | 6×6 转换表(不复制 gem5 slicc) | P0 | MOESI 简化版 |
| snoop-callback | CoherenceDomain snoop callback | ... | P0 | snoop fanout |
| lookup-home-node | lookup_home_node() 集成 | ... | P1 | address mapping |

### 7.D TCC Bridge + 内存层次 (phase-3.4)
**业务标签**: Phase 7.D
**状态**: 🟡 Pending
**风险**: 🟡 Med
**范围**: `TCC_TLM`(DualPortStreamAdapter,write coalescing + snoop fan-in);`MemoryTLM` 扩展 `hbm_mode` 参数
**验收**:
  - `configs/apu_demo_v3.json` 写合并
  - `cpptlm_tests "[gpu][tcc]"` PASS
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| tcc-dualport | TCC DualPortStreamAdapter | write coalescing + snoop fan-in | P0 | 写合并；fan-in |
| memory-hbm | MemoryTLM hbm_mode | HBM 内存模式参数 | P1 | hbm 配置 |

### 7.E Multi-CU + NoC (phase-3.5)
**业务标签**: Phase 7.E
**状态**: 🟡 Pending
**风险**: 🟢 Low
**范围**: `ComputeUnitTLM` 数组模式(`num_cus=4`);`BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar;复用 `RouterTLM` / `LinkTLM` / `NoCFlitBundle`
**验收**:
  - `configs/apu_demo_v4.json` 4 CU 并发
  - `cpptlm_tests "[gpu][noc]"` PASS
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| multi-cu-noc | Multi-CU 数组 + NoC | `ComputeUnitTLM` 数组 + BidirectionalPortAdapter<N> | P0 | NoC 拓扑；4 CU 并发 |
| cu-bidir-port | BidirectionalPortAdapter<N> | CU ↔ GPU Crossbar 连接 | P1 | 双向端口 |

### 7.F Full APU SoC Demo (phase-3.6)
**业务标签**: Phase 7.F
**状态**: 🟡 Pending
**风险**: 🟢 Low
**范围**: `configs/apu_full_soc.json`(2× TrafficGenTLM + 4× ComputeUnitTLM + TCC + Crossbar + Memory + 单 `CoherenceDomain("apu_domain")`);`test/python/test_apu_soc.py` 端到端验证;统计 dashboard
**验收**:
  - JSON 配置可读、trace 可观察、测试可断言
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| apu-demo | Full APU SoC Demo | `apu_full_soc.json` + `test_apu_soc.py` | P0 | 端到端验证 |
| apu-dashboard | 统计 dashboard | 性能/带宽/延迟可视化 | P1 | dashboard 输出 |
```

### 5.2 硬性约束(关键)

> **【硬性约束】父 phase 的 `#### 任务分类` 伞表必须出现在首个 `###` 子阶段 heading 之前。**

**依据**: `_lib/roadmap_state.py` L238 section 边界 regex `(?=\n### |\n## |\Z)` 在首个 `###` 处截断。若伞表在子 heading 之后,`get_phase_categories(phase-3)` / `validate_change(phase-3, ...)` / `get_phase_themes(phase-3, ...)` 将返回空或失败。

**缓解**:
1. 在 `skills/roadmap/SKILL.md` 模板段写明该约束(新增段落)
2. 可选: 在 `rdd-doctor` 中增加"伞表位置"检查(后续增强,非本次必需)

### 5.3 子阶段 heading 结构

每个子阶段用 `###` 三级 heading,自带 `#### 任务分类` 子表:

```markdown
### 7.X <name> (phase-N.M)
**业务标签**: Phase 7.X            ← 人类可读,机器不解析
**状态**: <icon> <status>
**风险**: <icon> <risk-level>
**范围**: <一句话描述>
**验收**:
  - <验收条目>
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| <kebab-case> | <中文名> | <范围细化> | P0/P1/P2 | <主题>；<主题> |
```

---

## 6. 跨仓 ADR 引用(Cross-Repository ADR Reference)

### 6.1 设计方案

UsrLinuxEmu ADR-088 v3 在 phase-5 (Phase 9) 中以 heading 下方 bullet 形式引用,**不侵入分类表**:

```markdown
### Phase 9: dGPU-first SoC 集成 (phase-5)
**目标**: dGPU-first 形态实现(双独立 CoherenceDomain + PCIe Bridge);为 UsrLinuxEmu GPU 驱动开发提供真硬件行为仿真
**状态**: 🔄 启动中
**前置阶段**: phase-4
**跨仓依赖**: [UsrLinuxEmu ADR-088 v3](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-cpptlm-emu-bridge.md)
  — dlopen("libcpptlm_emulator.so") 消费契约(72 C ABI: 60 forward + 11 callback + 1 register),Phase 9 强绑定
**完成条件**:
  - [ ] phase-5.0 ~ phase-5.6 全部完成(9.5 optional)
  - [ ] 98 个 UsrLinuxEmu Catch2 测试在 `USR_LINUX_EMU_USE_CPPTLM=1` 下全 PASS(0 regression)

#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cpptlm-infra | 9.0 CppTLM 基础设施 | `cpptlm_core` STATIC→SHARED + `libcpptlm_emulator.so` (phase-5.0) | P0 | C ABI 治理 |
| pcie-module | 9.1 CppTLM PCIe 模块 | 🔴 最高风险 (phase-5.1) | P0 | BAR mmap |
| ... | | | | |

### 9.0 CppTLM 基础设施 (phase-5.0)
... (子阶段结构同 §5.3)
```

### 6.2 双链接策略

| 链接类型 | 用途 | 示例 |
|---------|------|------|
| **GitHub URL** | 任意 clone 位置可达(主链接) | `https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-cpptlm-emu-bridge.md` |
| **本地相对路径(注释)** | 本机双仓布局下有效(辅助) | `../../UsrLinuxEmu/docs/00_adr/adr-088-cpptlm-emu-bridge.md`(在括号注释中,不作为链接文本) |

### 6.3 决策对比

| 方案 | 优点 | 缺点 | 选择 |
|------|------|------|:----:|
| A. heading 下方 bullet + GitHub URL 为主 | 不侵入表格 cell;表格 regex 解析安全;GitHub URL 稳定 | 需手动维护 GitHub org 路径 | ✅ **推荐** |
| B. 每个 change 单独 `openspec/changes/*/roadmap-meta.yaml` 声明 cross_repo_adrs | 机器可读,自动校验 | 需新建多个 meta 文件,工作量较大 | ❌ |
| C. 同时维护 markdown 链接 + 全局 `roadmap-meta.yaml` | 双轨兼容 | 需维护两套,容易漂移 | ❌ |

---

## 7. 待办段 + AUTO-SPRINT 哨兵位置

### 7.1 文件结构总览

```markdown
# CppTLM Roadmap
> **版本**: v3.0 (rdd-workflow v3.0+ 兼容)
> **当前阶段**: phase-3 (Phase 7: APU-first)
> **最后更新**: 2026-08-18
> **历史**: Phase 4 已于 2026-Q2 完成(见 AGENTS.md);Phase 10 为占位预留,不纳入

## 元信息
- **当前阶段**: phase-3
- **最后更新**: 2026-08-18
- **版本**: v3.0

### Phase 5: Protocol Bridge (phase-1)
...
### Phase 6: Multi-Cluster SoC Validation (phase-2)
...
### Phase 7: CPU+GPGPU Fused SoC (APU-first) (phase-3)
#### 任务分类                          ← 父伞表,先于子 heading
| ... |
### 7.A GPU 基础设施 (phase-3.1)       ← 子阶段 heading
#### 任务分类
| ... |
### 7.B ComputeUnit 黑盒 (phase-3.2)
...

### Phase 8: APU 验证 + 性能基准 (phase-4)
#### 任务分类
| ... |
### 8.1 Trace alignment tooling (phase-4.1)
...

### Phase 9: dGPU-first SoC 集成 (phase-5)
#### 任务分类
| ... |
### 9.0 CppTLM 基础设施 (phase-5.0)
...

## 待办                              ← 跨阶段临时任务,人工维护
- [x] ~~Phase 5(ProtocolBridge)启动后回填 5.1-5.5 子任务细节~~ ✅ 2026-08-15 已完成 (roadmap v2.2)
- [x] ~~Phase 6.2 / 6.3 启动后回填 cross-cluster coherence + bridge 集成测试细节~~ ✅ 2026-08-15
- [x] ~~Phase 7.A 启动前需更新 AGENTS.md STRUCTURE 节(添加 include/tlm/gpu/ 子目录)~~ ✅ 2026-06-11
- [x] ~~Phase 7.F 完成后把 roadmap.md 升级为 v2.2~~ ✅ 2026-08-15
- [x] ~~Phase 9 启动前评估 UsrLinuxEmu ADR-088 v3 影响并修订 roadmap.md~~ ✅ 2026-08-15 (roadmap v2.3)
- [ ] **Phase 9 启动前与 UsrLinuxEmu Architecture Team 协调时序**(避免与 Phase 8.B 冲突)
- [ ] **Phase 9.0 启动前写 ADR-SOC-06 (C ABI 治理)、ADR-SOC-07 (BAR mmap)、ADR-SOC-08 (dGPU-first 策略)**
- [ ] **Phase 9.0 启动前评估 `cpptlm_core` STATIC→SHARED 单行改动的兼容性影响**
- [ ] **Phase 9.1 启动前评估 `include/tlm/pcie/` 子目录创建**
- [ ] Phase 7.C 启动前需更新 scripts/README.md(如新增 gpu_kernel_trace_gen.py)
- [ ] 考虑扩展 docs_sync_check.sh 扫描范围(Markdown 链接、include/AGENTS.md 一致性)
- [ ] Phase 7.B 启动前评估 `docs/soc_arch/specs/apu-soc-design.md` 是否需要补全
- [ ] Phase 7.F 完成后写 ADR `ADR-0022-apu-validation-methodology.md` (Phase 8 入口)
- [ ] Phase 8 启动前评估是否新增 `docs/superpowers/specs/cpptlm-validation-spec.md`
- [ ] Phase 9 完成后评估是否新增 `docs/superpowers/specs/cpptlm-emu-bridge.md`
- [ ] Phase 10 启动前写 ADR `ADR-SOC-09-apu-upgrade-from-dgpu.md`

<!-- AUTO-SPRINT-START -->           ← 机器自动渲染区,roadmap_sprint.py 维护
... sprint 表(自动生成,人类不编辑) ...
<!-- AUTO-SPRINT-END -->
```

### 7.2 共存安全性分析

| 段 | 上游维护方 | 是否被 AUTO-SPRINT 渲染影响 |
|----|----------|---------------------------|
| `# CppTLM Roadmap` ~ `### Phase 9 ...` 之前 | 人工 | 否(在哨兵外) |
| `## 待办` | 人工 | 否(在哨兵外) |
| `<!-- AUTO-SPRINT-START -->` 与 `<!-- AUTO-SPRINT-END -->` 之间 | `roadmap_sprint.update_roadmap` 自动 | ✅ 是(原位重写) |

**依据**: `roadmap_sprint.py` L143 `_split_around_sentinels` 只切分哨兵**内部**内容并原位重写。哨兵外的 `## 待办` 完全不受影响,两者可永久共存。

### 7.3 `## 待办` 段位置决策对比

| 位置 | 优点 | 缺点 | 选择 |
|------|------|------|:----:|
| 头部(元信息后) | 早期可见 | 挤掉元信息区,顺序混乱 | ❌ |
| 尾部(所有阶段后、AUTO-SPRINT 前) | 顺序合理:"阶段主线 → 跨阶段杂项 → 机器渲染区";`## ` 级 heading 自然终止最后阶段 section(L238 边界 `\n## ` 分支) | 无明显缺点 | ✅ **推荐** |
| 分散到各 phase 的'前置/后续'注释 | 与 phase 关联 | roadmap 模板不直接支持这种结构,需要创造新约定 | ❌ |

---

## 8. rdd-workflow 兼容层改动清单

### 8.1 必须改动(代码)

#### 文件 1: `/workspace/project/rdd-workflow/_lib/roadmap_state.py`

**改动 1**: 模块顶部新增 3 个 regex 常量(约 3 行)

```python
# 嵌套阶段 ID 支持(向后兼容,平铺 phase-N 仍完全可用)
PHASE_ID_RE = r"phase-\d+(?:\.\d+)?"            # 任意 phase ID(顶层+子阶段)
TOP_PHASE_RE = r"phase-\d+"                       # 仅顶层 phase ID
SUB_PHASE_RE = r"phase-(\d+)\.(\d+)"              # 子阶段(捕获父/子序号)
```

**改动 2**: `advance_phase` 函数 L385 显式顶层过滤(1 行)

```python
# 原:phases = re.findall(r"\((phase-\d+)\)", content)
# 新:phases = [p for p in re.findall(r"\((phase-\d+(?:\.\d+)?)\)", content) if "." not in p]
phases = [p for p in re.findall(r"\((phase-\d+(?:\.\d+)?)\)", content) if "." not in p]
```

**改动 3**: `advance_phase` pre-check 聚合子阶段(L353-373,约 10 行,**关键修复**)

```python
# Pre-check: all changes complete(包含子阶段聚合)
all_complete = True
# 1. 检查当前 phase 自身 categories
for cat_id, cat_data in phase_data.get("categories", {}).items():
    total = len(cat_data.get("changes", []))
    completed = len(cat_data.get("completed_changes", []))
    if completed < total:
        all_complete = False
        print(f"❌ 分类 {cat_id} 未完成: {completed}/{total}")
# 2.【新增】聚合所有 phase-<current>.M 子阶段
sub_ids = sorted(pid for pid in state.get("phases", {})
                if re.match(rf"^{re.escape(current)}\.\d+$", pid))
for pid in sub_ids:
    for cat_id, cat_data in state["phases"][pid].get("categories", {}).items():
        total = len(cat_data.get("changes", []))
        completed = len(cat_data.get("completed_changes", []))
        if completed < total:
            all_complete = False
            print(f"❌ 子阶段 {pid} 分类 {cat_id} 未完成: {completed}/{total}")
```

**模块 docstring 更新**: 在 `_lib/roadmap_state.py` L1-31 模块 docstring 中追加"嵌套阶段语法"段落,说明 `phase-N.M` 约定和聚合规则。

#### 文件 2: 不改

- `_lib/roadmap_sprint.py` — **不改**(本次 grep 未发现其引用 phase regex;哨兵作用域与 phase 解析无关)

### 8.2 必须改动(测试)

#### 文件 3: `/workspace/project/rdd-workflow/tests/unit/test_roadmap_state.py`

新增嵌套 fixture(约 30 行):

```python
def test_phase_pattern_does_not_match_subphase():
    """验证 (phase-3) 不误匹配 (phase-3.1) 的 section 边界。"""
    roadmap_content = """
### Phase 7: APU (phase-3)
**目标**: ...
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-infra | 7.A GPU 基础设施 | (phase-3.1) | P0 | bundle schema |

### 7.A GPU 基础设施 (phase-3.1)
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-bundle | GPU Bundle | ... | P0 | ... |
"""
    # 验证 validate_change 解析 phase-3 找到伞分类 gpu-infra
    assert "gpu-infra" in get_phase_categories(roadmap_content, "phase-3")
    # 验证 validate_change 解析 phase-3.1 找到子分类 gpu-bundle
    assert "gpu-bundle" in get_phase_categories(roadmap_content, "phase-3.1")

def test_advance_phase_aggregates_subphases():
    """验证 advance pre-check 聚合 phase-N.M 子阶段,避免父 phase 平凡通过。"""
    state = {
        "current_phase": "phase-3",
        "phases": {
            "phase-3": {
                "status": "in_progress",
                "categories": {},  # 父伞表为空
                "gate_status": {"all_changes_complete": False, "checklist": {}},
            },
            "phase-3.1": {
                "status": "completed",
                "categories": {
                    "gpu-bundle": {"changes": ["c1"], "completed_changes": ["c1"]},
                },
                "gate_status": {"all_changes_complete": True, "checklist": {}},
            },
            "phase-3.2": {
                "status": "in_progress",
                "categories": {
                    "cu-tickloop": {"changes": ["c2"], "completed_changes": []},  # 未完成
                },
                "gate_status": {"all_changes_complete": False, "checklist": {}},
            },
        },
    }
    # pre-check 应该报告 phase-3.2 未完成,不应平凡通过
    # 实施方案:通过 advance_phase() 间接测试(写临时 roadmap.md 含 phase-3 + 子 heading);
    # 或抽取 _aggregate_phase_completion(state, phase_id) helper 后直接调用
    # (后者需配合 §8 改动 3 同步重构,见改动 3 注释)
    import io, contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = advance_phase("roadmap.md", ".rddf/state/test-state.json")
    # advance_phase 在 pre-check 失败时返回 1,不应返回 0
    assert rc == 1
    assert "phase-3.2" in buf.getvalue() and "未完成" in buf.getvalue()

def test_advance_phase_skips_subphases_for_next():
    """验证 advance_phase 下一阶段查找跳过 phase-N.M,仅在顶层 phase-N 间推进。"""
    roadmap_content = """
### Phase 7 (phase-3)
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 |
|--------|------|------|--------|
| gpu-infra | A | (phase-3.1) | P0 |
### 7.A GPU (phase-3.1)
### Phase 8 (phase-4)
"""
    # 顶层列表应为 [phase-3, phase-4],跳过 phase-3.1
    top_phases = [p for p in re.findall(r"\((phase-\d+(?:\.\d+)?)\)", roadmap_content) if "." not in p]
    assert top_phases == ["phase-3", "phase-4"]
```

#### 文件 4: `/workspace/project/rdd-workflow/tests/unit/test_roadmap_state_themes.py`

新增嵌套用例(约 15 行):

```python
def test_get_phase_themes_for_subphase():
    """验证 get_phase_themes 在 phase-N.M 子阶段上工作。"""
    roadmap_content = """
### 7.C Coherence Protocol 集成 (phase-3.3)
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| cache-state-machine | CacheTLM state machine | ... | P0 | MOESI 简化版；snoop fanout |
"""
    themes = get_phase_themes(roadmap_content, "phase-3.3", "cache-state-machine")
    assert themes == ["MOESI 简化版", "snoop fanout"]
```

### 8.3 必须改动(文档)

#### 文件 5: `/workspace/project/rdd-workflow/skills/roadmap/SKILL.md`

在 `### Phase N: <name> (phase-N)` 模板段后新增"嵌套阶段语法"段(约 20 行):

```markdown
## 嵌套阶段语法(可选扩展)

对于业务演进包含子阶段的项目(如 CppTLM Phase 7.A-7.F),roadmap 支持嵌套 ID:

```markdown
### Phase 7: CPU+GPGPU Fused SoC (phase-3)
**目标**: ...
**完成条件**:
  - [ ] phase-3.1 ~ phase-3.6 全部完成

#### 任务分类                    ← 【硬性约束】必须先于所有 ### 子阶段 heading
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-infra | 7.A GPU 基础设施 | (phase-3.1) | P0 | ... |
| ... | | | | |

### 7.A GPU 基础设施 (phase-3.1)  ← 子阶段 heading
#### 任务分类
| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|--------|--------------|
| gpu-bundle | ... | ... | P0 | ... |
```

**约束**:
1. 父 phase 伞表必须先于所有 `###` 子阶段 heading(`get_phase_categories` section 边界 regex 限制)
2. change meta `roadmap.phase` 可指向子阶段 ID(`phase-3.3`)
3. `advance_phase` 自动聚合子阶段完成度
4. 嵌套 ID 语法:`phase-N.M`(数字后缀),业务标签(7.A/9.0)出现在 heading 文本
```

### 8.4 改动量总结

| 文件 | 改动类型 | 行数 |
|------|---------|-----:|
| `_lib/roadmap_state.py` | 代码 + docstring | ~15 |
| `tests/unit/test_roadmap_state.py` | 测试 | ~45 |
| `tests/unit/test_roadmap_state_themes.py` | 测试 | ~15 |
| `skills/roadmap/SKILL.md` | 文档 | ~30 |
| `_lib/roadmap_sprint.py` | **不改** | 0 |
| 其他 `_lib/*.py` | **不改** | 0 |
| **总计** | | **~105 行** |

### 8.5 不需要新增的文件

**不需要** `_lib/roadmap_nested.py` 之类独立模块。嵌套支持 = 1 个 regex 常量 + 1 处聚合逻辑 + 测试,塞进现有 `roadmap_state.py` 即可。独立模块反而制造"哪些逻辑在哪个文件"的认知负担。

---

## 9. 回退方案(Fallback Strategy)

### 9.1 Fallback A:零上游改动

**适用场景**: rdd-workflow 上游补丁合并受阻期间(用户未接受 PR、CI 失败等)。

**实现方式**: CppTLM 端纯文档约定嵌套,change meta 只指向顶层 `phase-3`,子阶段靠 category ID 区分(如 `category: p7c-coherence`——kebab-case 允许,首字符须字母)。

**代价**:
- ✅ 立即可用,无需 rdd-workflow 协调
- ❌ 失去子阶段级状态追踪(`advance_phase` 不能聚合子阶段)
- ❌ change meta 粒度变粗,后续审计/回溯时无法精确到子阶段

**恢复路径**: 上游补丁合入后,补 3 个 change 的 `roadmap-meta.yaml` 指向子阶段 ID,删除 `p7c-` 类人造 category 前缀。

### 9.2 Fallback B:CppTLM 端 adapter 脚本

**适用场景**: 长期坚持嵌套格式但 rdd-workflow 上游不接受嵌套扩展。

**实现方式**: CppTLM 端新建 `scripts/roadmap-adapter.py`,维护 `phase-N.M ↔ phase-N + category` 双向映射,roadmap.md 内部使用嵌套语法,adapter 脚本生成平铺视图喂给 rdd-workflow CLI。

**代价**:
- ✅ roadmap.md 保持业务语义
- ❌ 增加一个持续维护的转换层
- ❌ 双向同步存在漂移风险

**恢复路径**: 上游接受嵌套后删除 adapter 脚本,直接使用嵌套 ID。

### 9.3 默认策略

| 阶段 | 策略 |
|------|------|
| t=0(本次重构) | 主推方案(§8 rdd-workflow 最小补丁)+ Fallback A 同步准备(roadmap-meta.yaml 用顶层 ID 兜底) |
| t+1 周 | 上游 PR 评审结果 |
| ├─ 接受 | 切主方案,补 3 个 change meta 指向子阶段 |
| ├─ 拒绝 / 阻塞 | 继续用 Fallback A,持续向上游 push |
| └─ 长期阻塞 | 评估 Fallback B 必要性 |

---

## 10. 风险与执行顺序

### 10.1 风险矩阵

| ID | 风险 | 等级 | 影响范围 | 缓解措施 |
|----|------|:----:|----------|----------|
| R1 | 跨仓断链(UsrLinuxEmu ADR-088 v3 链具体 anchor) | 🟡 中 | 跨仓引用不可达 | 迁移前 grep `roadmap.md#` 跨仓引用;只链文件级则安全;若链 anchor,补 GitHub anchor 迁移说明 |
| R2 | openspec/changes 已有 phase-旧式引用 | 🟢 低 | change meta 解析失败 | 当前 3 个活跃 change 未见 roadmap-meta.yaml;若发现用映射表 remap |
| R3 | 父伞表位置错放(子 heading 之后) | 🟡 中 | `validate_change` / `get_phase_themes` 静默失败 | SKILL.md 写明约束;rdd-doctor 后续增加"伞表位置"检查 |
| R4 | AGENTS.md / ONBOARDING.md 中 Phase 4/10 引用断链 | 🟢 低 | 文档内失效引用 | business 名继续存在,roadmap 元信息区加一行"历史见 AGENTS.md"说明;无需批量改 |
| R5 | 回退成本 | 🟢 低 | 重构不可逆 | git tag `roadmap-v2.3` 留底;rdd-workflow 补丁向后兼容 |
| R6 | rdd-workflow 上游同步 | 🟢 低 | 共享框架回归 | rdd-workflow 与 CppTLM 同机,实为本地 commit + tests;补丁向后兼容(平铺 phase-N 仍完全支持) |

### 10.2 执行顺序(4 步串行依赖)

```
┌─────────────────────────────────────────────────────────────┐
│ Step 1: rdd-workflow 侧补丁                                   │
│   - roadmap_state.py 改动 1/2/3 + 模块 docstring              │
│   - tests/unit/test_roadmap_state.py + test_roadmap_state_themes.py│
│   - skills/roadmap/SKILL.md 嵌套语法段                       │
│   - 验证: pytest tests/unit/ 全部通过                        │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 2: CppTLM 侧 roadmap.md 重写 + state 初始化               │
│   - 写新 /workspace/project/CppTLM/roadmap.md(v3.0)            │
│   - 创建 /workspace/project/CppTLM/.rddf/state/roadmap-state.json│
│     (phase-3 标 in_progress, phase-3.1 标 completed 2026-06-11)│
│   - git tag roadmap-v2.3(留底)                                │
│   - 验证: rdd-doctor --json 返回 findings: []                  │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 3: 3 个 openspec/changes 补 roadmap-meta.yaml            │
│   映射依据:                                                    │
│   - cpptlm-d1-p1-pipeline-scoreboard = Scoreboard/Pipeline/    │
│     TensorCore 注入(ComputeUnit 子功能)→ phase-3.2,           │
│     cu-tickloop                                                │
│   - 2026-06-24-gpu-soc-phase8b-core = "Phase 8.B 核心仿真"     │
│     (cu-blackbox 时代)→ phase-3.2, cu-tickloop                │
│   - 2026-06-24-gpu-soc-phase8c-advanced = "Phase 8.C 高级特性" │
│     (multi-cu 时代)→ phase-3.5, multi-cu-noc                 │
│   注: phase8b/8c 命名来自旧 Phase 8 模型(8.A infra / 8.B core │
│   / 8.C advanced),与新模型 phase-3 业务重合,按功能映射;      │
│   phase-3.2 / phase-3.5 现有的 cat_id 与 change 实现目标匹配。 │
│   - 验证: 3 个 change 跑 roadmap validate <name> 返回 ✅       │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 4: 跨仓断链排查 + AUTO-SPRINT 验证                        │
│   - grep "roadmap.md#" 跨仓引用(UsrLinuxEmu)                  │
│   - 若有具体 anchor 链接,补 GitHub anchor 迁移说明             │
│   - 跑 roadmap_sprint.update_roadmap() 验证哨兵外内容不变     │
│   - 验证: SC-1 ~ SC-5 全部满足                                │
└─────────────────────────────────────────────────────────────┘
```

### 10.3 Effort 估算

| 工作量 | rdd-workflow 补丁 | CppTLM roadmap 重写 + state | 跨仓断链排查 | 实施计划 + 测试 |
|--------|:------------------:|:---------------------------:|:------------:|:---------------:|
| 等级 | Short (1-4h) | Short (1-4h) | Quick (<1h) | Medium (4-8h) |
| **总计** | | | | **Short-Medium (6-17h)** |

---

## 11. 验收标准(Verification Gates)

### VG-1: rdd-workflow 单元测试通过

```bash
cd /workspace/project/rdd-workflow
pytest tests/unit/test_roadmap_state.py -v
pytest tests/unit/test_roadmap_state_themes.py -v
```

**预期**: 全部测试 PASS,包括新增的嵌套 fixture:
- `test_phase_pattern_does_not_match_subphase` ✅
- `test_advance_phase_aggregates_subphases` ✅
- `test_advance_phase_skips_subphases_for_next` ✅
- `test_get_phase_themes_for_subphase` ✅

### VG-2: rdd-doctor 6 类检查全部通过

```bash
cd /workspace/project/CppTLM
bash /workspace/project/rdd-workflow/skills/rdd-doctor/scripts/doctor.sh --json
```

**预期**: `findings: []`,`summary: {critical: 0, warning: 0, info: 0}`

### VG-3: 3 个活跃 openspec/changes 验证通过

```bash
cd /workspace/project/CppTLM
for change in cpptlm-d1-p1-pipeline-scoreboard 2026-06-24-gpu-soc-phase8b-core 2026-06-24-gpu-soc-phase8c-advanced; do
    bash /workspace/project/rdd-workflow/skills/roadmap/scripts/validate.sh "$change"
done
```

**预期**: 每个 change 返回 `✅ Change "..." 验证通过` + 阶段 + 分类

### VG-4: AUTO-SPRINT 哨兵外内容不变

```bash
cd /workspace/project/CppTLM
cp roadmap.md /tmp/roadmap.before.md
python3 -c "
import sys
sys.path.insert(0, '/workspace/project/rdd-workflow')
from _lib.roadmap_sprint import update_roadmap
update_roadmap('roadmap.md', {'changes': [], 'current_phase': 'phase-3'})
"
diff <(grep -v -E '<!-- AUTO-SPRINT-(START|END) -->|^_Phase:' /tmp/roadmap.before.md) \
     <(grep -v -E '<!-- AUTO-SPRINT-(START|END) -->|^_Phase:' roadmap.md)
```

**预期**: 无 diff(哨兵外内容字节级相同)

### VG-5: 跨仓文档断链数 = 0

```bash
cd /workspace/project/CppTLM
# 检查 UsrLinuxEmu ADR-088 v3 是否链具体 anchor
grep -r "roadmap.md#" /workspace/project/UsrLinuxEmu/ 2>/dev/null
# 或在 GitHub 上: https://github.com/chisuhua/UsrLinuxEmu/search?q=roadmap.md%23
```

**预期**: 无结果(只链文件级)或所有 anchor 链接仍可达(经手动验证)

---

## 12. 决策点汇总(Decisions)

### D1: 嵌套命名方案

- **决策**: `phase-N.M` 数字后缀(7.A → phase-3.1, 9.0 → phase-5.0)
- **依据**: Oracle 5m12s 分析;数字天然有序;9.0-9.6 1:1 无损
- **替代方案**: B. 字母后缀(7.A → phase-3.A,混风格);C. 平铺(丢失伞分类)
- **用户确认**: 4 Q&A Q2 选 "混合:子阶段作为 phase,但每个业务主阶段有'伞分类'" → 落地为方案 A

### D2: 顶层 phase-N 数量

- **决策**: 5 个顶层 phase-1..phase-5(对应 Phase 5..Phase 9)
- **依据**: 用户 Q&A Q1 选"仅保留 5 个核心阶段(从 Phase 5 开始)"
- **排除**: Phase 4(已完成 2026-Q2) + Phase 10(占位)

### D3: 伞分类表粒度

- **决策**: 一行一子阶段(kebab-case cat_id + 业务标签 + phase-N.M 引用)
- **依据**: 用户 Q&A Q2 选"混合"中的"伞分类"选项
- **替代方案**: α 单行(丢失主题粒度);γ 子阶段单独成 phase(18 phase 过多)

### D4: 子阶段表达

- **决策**: `###` 三级 heading + 自带 `#### 任务分类` 子表
- **依据**: L238 section 边界 regex `(?=\n### |\n## |\Z)` 天然截断;`validate_change` 对 phase-N.M 精确匹配
- **硬性约束**: 父伞表必须先于子 heading

### D5: 跨仓 ADR 引用形式

- **决策**: heading 下方 bullet + GitHub URL(主) + 本地相对路径(括号注释)
- **依据**: 不侵入表格 cell(避免污染 cat_name 解析)
- **替代方案**: B. 每个 change 单独 meta(工作量大);C. 双轨兼容(易漂移)

### D6: 待办段位置

- **决策**: 尾部,所有阶段后、AUTO-SPRINT 哨兵前
- **依据**: roadmap_sprint.py L143 只切分哨兵内部;`## ` heading 自然终止最后阶段 section

### D7: rdd-workflow 改动范围

- **决策**: 最小补丁(`roadmap_state.py` ~15 行 + 测试 ~60 行 + 文档 ~30 行)
- **依据**: Oracle 实测 regex 天然兼容;唯一必须修复的是 pre-check 子阶段聚合
- **替代方案**: 全模块重写(过度工程)

### D8: 历史 / 占位 Phase 处理

- **决策**: Phase 4(已完成) + Phase 10(占位)不进入重构后 roadmap;roadmap.md 元信息区加一行说明
- **依据**: 用户 Q&A Q1 选"仅保留 5 个核心阶段"
- **替代方案**: 保留为 phase-0(历史) + phase-6(占位)— 增加噪声

---

## 13. 修订历史

| 版本 | 日期 | 修订内容 | 作者 |
|------|------|---------|------|
| 1.0 | 2026-08-18 | 初稿:基于 Oracle 5m12s 设计分析 + 4 项 Q&A 用户决策 | Sisyphus |
| (待用户 review) | | | |

---

## 附录 A:用户 Q&A 决策摘要

| Q# | 问题 | 答案 |
|----|------|------|
| Q1 | 已完成的历史 Phase(如 Phase 4)如何处理? | 仅保留 5 个核心阶段(从 Phase 5 开始) |
| Q2 | 子阶段(7.A-F、9.0-9.6)如何映射? | 混合:子阶段作为 phase,但每个业务主阶段有"伞分类" |
| Q3 | roadmap.md L275-292 待办列表如何处理? | 保留为独立 `## 待办` 段(不属 phase-N 结构) |
| Q4 | UsrLinuxEmu 跨仓 ADR 引用如何处理? | Oracle 分析给出完整建议,同时给出 rdd-workflow 改进(可解析新格式) |

## 附录 B:Oracle 设计洞察摘要

> 完整分析见 `task(subagent_type="oracle", session_id="ses_fef4ec68fffeHdqBs7ASQQimOx")`

| 洞察 | 影响 |
|------|------|
| 现有 regex `\(phase-\d+\)` 因 `\)` 锚定,天然不误匹配 `phase-N.M` | 0 改动即可精确识别顶层 |
| L385 `re.findall` 因同样锚定,自动跳过子阶段 | advance 下一阶段查找天然正确 |
| L238 section 边界 `(?=\n### \|\n## \|\Z)` 在首个 `###` 截断 | 强制父伞表必须先于子 heading(硬性约束) |
| `advance_phase` pre-check 只查 `state["phases"][current]` | **真实缺口**: 子阶段注册在 phase-N.M 时父 phase 平凡通过 |
| 修复缺口需 ~10 行聚合逻辑 + 3 行 regex 常量 + ~60 行测试 | 改动量 Short (1-4h) |

## 附录 C:关键文件路径速查

| 用途 | 路径 |
|------|------|
| CppTLM roadmap(本设计目标) | `/workspace/project/CppTLM/roadmap.md` |
| CppTLM roadmap state(新建) | `/workspace/project/CppTLM/.rddf/state/roadmap-state.json` |
| rdd-workflow 解析主模块 | `/workspace/project/rdd-workflow/_lib/roadmap_state.py` |
| rdd-workflow sprint 渲染 | `/workspace/project/rdd-workflow/_lib/roadmap_sprint.py` |
| rdd-workflow 技能文档 | `/workspace/project/rdd-workflow/skills/roadmap/SKILL.md` |
| rdd-workflow 测试目录 | `/workspace/project/rdd-workflow/tests/unit/` |
| rdd-workflow doctor 检查 | `/workspace/project/rdd-workflow/skills/rdd-doctor/scripts/doctor.sh` |
| CppTLM 3 个活跃 change | `/workspace/project/CppTLM/openspec/changes/{cpptlm-d1-p1-pipeline-scoreboard,2026-06-24-gpu-soc-phase8b-core,2026-06-24-gpu-soc-phase8c-advanced}/` |

---

**End of Document**
