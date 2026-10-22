# Design — cpptlm-cep-umbrella v1.0

> **状态**: 📋 Draft v1.0(2026-10-21)
> **配套**: [`proposal.md`](proposal.md) · [`specs/cep-core-contracts/spec.md`](specs/cep-core-contracts/spec.md) · [`tasks.md`](tasks.md)
> **上游真源**: [`docs/architecture/07-coherence-exploration-platform.md`](../../../docs/architecture/07-coherence-exploration-platform.md) v0.1 + `docs/adr/ADR-CEP-*.md` 共 16 份

---

## Context

### 现状

21 份 CEP ADR 已达可签发水准(3 轮 oracle REJECT 关闭 + 2026-10-21 首次**独立** Oracle 架构复审判定"CEP 骨架成立,无需改层"),承载 **67 个 invariant 编号位 / 66 条实际守门 invariant**。但这些 invariant **目前只存在于 Markdown**:

- 无机器可执行的断言 → 契约漂移无感知
- ADR 之间只能引"§5.1"这类自然语言位置 → 引用失效无告警
- 16 份 ADR 的 invariant 分散在各自的 `§3 不变量` 节 → 计数口径易分歧(2026-10-21 前已发生过一次:"36 条单一真源"名存实亡 —— 2 条 invariant 只在提案它们的 ADR 里声明,未落 owner ADR)

### 目标态

Stage 0 交付**契约层**:让 50 条 invariant 变成**可执行断言**,并让后续 9 个 change 全部基于同一套机器可验证的契约。

### 关键约束

| 约束 | 来源 |
|------|------|
| 冻结面 0 diff(23 ABI + 4 frozen headers) | `ADR-DGPU-08` / `ADR-DGPU-09` / `ADR-CEP-07 §2 Decision 6` |
| canonical 10 流零变更 | `ADR-CEP-02` |
| `perm` 4 值锁 | `ADR-CEP-02` `CEN-Ib11` |
| Stage 1~3 **不依赖任何协议生成器** | `ADR-CEP-18` 决策 1 |
| KPI 总数 = 10 个 change,串行 10 季度 | `ADR-CEP-00 §2 Decision 3`(**不修订**) |
| 测试文件平铺在 `test/`,不建子目录 | oracle B7 + `test/CMakeLists.txt` 的 `file(GLOB test_*.cc)` |

---

## Goals / Non-Goals

**Goals:**

1. 把 **66 条实际守门 invariant** 变成**机器可执行断言**,按 `objective §0 DoD 7` 的 15 组分解
2. 定义 **协议实例能力声明 schema**(`ADR-CEP-12`),使后续 adapter 有一致的声明载体
3. 定义 **实验输出契约**(4 件套 + 13 metric + `verification_mode`),使 Stage 1+ 的实验产物格式一次定死
4. 定义 **集成 seam 契约**(ModuleFactory 真实注册 + `simulation_mode` 单一维度 + Null 默认实现)
5. 提供 **文档 lint**(16 份 ADR 锚点 + invariant 计数单一真源),把 2026-10-21 那类漂移**变成 CI 可捕获**
6. 提供 **实验工具雏形**(render_report / run_sweep / ab_compare),使 Stage 0 结束时已能产出可比较数据

**Non-Goals:**

1. ❌ **任何协议实现** —— 无 StubCacheCore 实现、无 SharerTracker 实现、无 ProtocolEngine adapter(→ Stage 1)
2. ❌ **任何完整缓存核心微架构** —— 不做 tag/data SRAM / 复杂预取器 / 精确功耗模型(per `ADR-CEP-05 §4.2`)
3. ❌ **真实 RTL / FPGA / UsrLinuxEmu 集成 / IOMMU 完整协议**
4. ❌ **协议生成器 / DSL / 解析器 / IR**(per `ADR-CEP-18` 决策 1:Stage 1~3 **不依赖任何生成器**)
5. ❌ **多 VC 仲裁策略实现** —— Stage 0 只落 `INV-CEP-09-IC1~3` 的**契约**;无环检查器实现 → Stage 1 `cpptlm-cep-directory-ordering-plane`
6. ❌ **跨域组合实现** —— Stage 0 只落 `INV-CEP-13-IC1~3` 的**契约**;实现 → Stage 2+
7. ❌ **穷尽死锁 / 一致性验证** —— Stage 0 只落 `verification_mode` 契约;穷尽验证受规模上限约束(per `ADR-CEP-06 §3`)

---

## Decisions

### D1:契约层与实现层严格分离(Stage 0 断言什么、不断言什么)

**决策**:Stage 0 的契约测试断言的是「**契约存在且自洽**」,而非「**实现满足契约**」。

| 层 | 断言内容 | 落点 |
|----|---------|------|
| **契约层(Stage 0)** | 清单完整(10 流 / 6 接口 / 5 实现 / **7 字段组 / 13 属性** / 4 件套 / 13 metric / 3 判据 / 4 档 `verification_mode`);字段值域收敛;不变量的**存在性 + 语义文本**与 ADR 一致;schema 的**结构约束**(key 解析失败即 fail) | 本 change 的 4 个测试 + 1 个 schema + 1 个 lint |
| **实现层(Stage 1+)** | 实现**行为**满足契约(如 `INV-SC1` 的 outstanding 上限在真实并发下成立) | 各 change 的 OpenSpec AC |

**理由**:若 Stage 0 就断言实现行为,必须先实现 StubCacheCore 等实体 —— 那就把 Stage 0 变成了 Stage 1,违反 `ADR-CEP-00 §2 Decision 3` 的串行排队与 KPI 约束。
**代价**:契约层测试**不能**发现实现 bug —— 这是刻意的,实现验证由 Stage 1+ 承担。

### D2:invariant 契约的机器检查形态 —— **解析 ADR 文档 + 校验结构**,而非硬编码副本

**决策**:契约测试**解析** `docs/adr/ADR-CEP-*.md` 的 `§3 不变量` 节,提取 invariant id 列表,与**硬编码的期望集合**比对。

```
解析 ADR-CEP-02 §3  →  CEN-Ib1..CEN-Ib11   (期望 11)
解析 ADR-CEP-05 §4.3 → INV-SC1..6 + INV-CEP-05-IC1  (期望 7)
...
合计期望 67 编号位 / 66 实际(IC4 **唯一跳空**;校验须区分「提及」与「定义」)
```

**理由**:
- 若测试里**复制** invariant 文本,则 ADR 改了测试不会红 → 双真源(正是 2026-10-21 暴露的问题的同构形态)
- 解析文档 + 校验**结构**(id 集合 / 数量 / 章节存在 / 关键字段值),能捕获:数量漂移 / id 跳号 / 章节丢失 / 字段值域越界

**代价**:测试**耦合文档路径** —— ADR 重命名/移动会红。这正是期望行为(需同步改测试)。

**配套**:`scripts/test/cep_docs_sync_check.sh` 做**章节锚点级**校验(比测试更细:章节行号 + 伪引清单),两者互补不重复。

### D3:capability schema 用 **JSON Schema**,不用 C++ header

**决策**:`include/tlm/cep/protocol_instance.schema.json` 采用 JSON Schema draft 2020-12。

| 候选 | 优点 | 否决理由 |
|------|------|---------|
| **JSON Schema** ✅ | 纯数据;Python / C++ / jq 皆可校验;diff 友好;不进编译单元 | — |
| C++ struct + `static_assert` | 编译期强制 | **数据与代码耦合** → 需在 `include/` 里放数据文件,违反"Stage 0 仅 `.gitkeep`"的冻结面思路;且 JSON 已被 `experimental/metrics.schema.json` 先例采用 |

**理由**:schema 描述的是**协议实例的声明数据**(不是运行时对象),JSON 是自然载体;`experimental/metrics.schema.json` 已有先例,口径一致。

**解析时机**:`ADR-CEP-09` 的装配期无环检查器(Stage 1)读取它;Stage 0 只做**结构校验**。

### D4:3 个 tools 用**纯标准库**,零项目内依赖

**决策**:`tools/render_report.py` / `run_sweep.py` / `ab_compare.py` **不 import `cpptlm/` 包**,只用 Python 标准库(`json` / `csv` / `argparse` / `pathlib` / `statistics`)。

**理由**:
- 工具的输入是 4 件套(**文件**),输出也是文件 → 无需项目内 API
- Stage 1+ 换 `trace.h5` 后端时,工具接口不变
- 避免"工具依赖仿真构建产物"导致的循环依赖(仿真要 build,工具不能等 build)

**代价**:`trace.h5` 解析用 `h5py`(`numpy`/`h5py` 已在仓内其它处使用,需在实现时确认;若无则 Stage 0 工具先支持 `metrics.json` 单件套,`trace.h5` 解析延后)。

### D5:lint 脚本与既有 `docs_sync_check.sh` **分工不重叠**

**决策**:新建 `scripts/test/cep_docs_sync_check.sh`,只做 **CEP 专属**校验;**不**修改既有 `scripts/test/docs_sync_check.sh`。

| 脚本 | 范围 |
|------|------|
| `docs_sync_check.sh`(既有) | 通用:AGENTS.md / ONBOARDING.md / roadmap.md / scripts/README.md 的路径漂移 + `VIRTUAL_PATHS` |
| `cep_docs_sync_check.sh`(新) | CEP 专属:**16 份 ADR** 的章节锚点 + `07 号` 章节号 + **invariant 计数单一真源**(4 处文档一致)+ `07 号 §5.1` adapter 计数口径 |

**理由**:既有脚本的 4 个核心文档清单是 AGENTS.md 规定的;把 CEP 加进去会污染其职责边界,且两者的 `VIRTUAL_PATHS` 语义不同。

**pre-commit 集成**:两者**都**接入,但 `cep_docs_sync_check.sh` 只在 `docs/adr/ADR-CEP-*.md` 或 `docs/architecture/07-*.md` 变更时触发(用 `git diff --cached --name-only` 过滤),避免每次提交都全量扫 16 份。

### D6:`experimental/` 目录约定 —— **产物目录 + schema,不入库产物**

**决策**:
```
experimental/
  .gitkeep                  # 保目录
  metrics.schema.json       # metrics.json 的 schema
  <exp_id>/                 # 每次实验一个子目录
    simulation_config.json
    trace.h5
    metrics.json
    report.html
```
- `<exp_id>` 命名:`<protocol_instance_id>__<workload>__<seed>`
- **实验产物默认不入库**(`.gitignore` 放行 `.gitkeep` 与 `*.schema.json`);对外分享走显式打包

**理由**:`trace.h5` 可能很大;入库会撑爆仓库。schema 入库是因为它是**契约**。

### D7:`verification_mode` / `verification_scope` 落在 `simulation_config.json`

**决策**:验证深度元数据放在 **`simulation_config.json`**(实验的输入侧),不放 `metrics.json`(输出侧)。

**理由**:验证深度是**实验的声明**(前提),不是**实验的观测**(结果)。放输入侧才能表达"这个结果是在什么前提下得到的"。

```json
{
  "simulation_mode": "timing",
  "verification_mode": "bounded_random",
  "verification_scope": "2 节点 × 3 协议实例 × 10000 事务;未穷尽 4 节点配置",
  "protocol_instance": { "... ADR-CEP-12 schema 片段 ..." }
}
```

### D8:10 组 invariant → 4 个测试文件的映射

**决策**(避免一个巨型测试文件,也避免重复断言):

| 测试文件 | 覆盖的组 | 断言形态 |
|---------|---------|---------|
| `test_cep_canonical_invariants.cc` | 1 Coherence Interface(11)、2 Stub Cache(7)、3 Sharer Tracker(6)、7 Ordering Point(3)、8 Multi-VC(3)、9 Capability Map(3)、10 Coherence Domain(3) | **ADR 文档解析 + 结构校验**;schema 字段组由 `test_cep_schema.cc` 承担(见下) |
| `test_cep_dummy_signals.cc` | — | `NullProtocolEngine` / `NullCollectivePlane` 的 **10 流覆盖矩阵**(逐流可观测) |
| `test_cep_module_factory_seam.cc` | 5 Simulation + Seam(7) | 注册机制 + 冻结面 0 diff + `simulation_mode` 单一维度 |
| `test_cep_simulation_mode_lock.cc` | 4 Experiment Protocol(5) | 4 件套 + 13 metric + `verification_mode` 四档 + 一致性闸门 |
| `test_cep_schema.cc`(**新增第 5 个**) | 9 Capability Map 的 schema 结构部分 | JSON Schema 8 字段组 + `named_network` → `vc_id` 解析 fail |
| `scripts/test/cep_docs_sync_check.sh` | 6 Numbering(3) | 16 份 ADR 锚点 + invariant 计数单一真源 |

> **修正**:proposal 里写的是 4 个测试文件;设计细化后 schema 断言独立成第 5 个文件更清晰(避免 `canonical_invariants` 膨胀到 400+ 行)。**文件数由 12 → 13**,`tasks.md` 按此执行,proposal 的文件表在实施时同步。

### D9:契约文本的一致性检查用**规范化比对**,不做自然语言理解

**决策**:invariant 的"语义一致"检查采用**关键词 + 关键 token 存在性**校验(如 `CEN-Ib11` 必须含 `perm` / `4` / `ATOMIC` / `独立属性`),**不做** NLP 语义比对。

**理由**:自然语言语义比对不可靠且不可维护;token 级校验能捕获"删了条件""改了数量""漏了字段"这类**实际发生过**的漂移形态(2026-10-21 的 N1/N2 都是这类)。

### D10:`experimental/` 与 `tools/` 的依赖方向单向

**决策**:`tools/` →(读)→ `experimental/`;`experimental/` **不依赖** `tools/`;`tools/` **不依赖** 仿真构建产物。

**理由**:保证 Stage 0 结束时**已能**用历史产物跑工具做 A/B 对比(工具链独立于仿真构建)。

---

## Risks / Trade-offs

### R1:契约测试耦合 ADR 文档路径,ADR 重命名会红

- **影响**:中等 —— 未来 ADR 结构调整可能引发测试批量修改
- **缓解**:这是**期望行为**(契约变更必须显式);lint 脚本的错误信息直接给出"ADR 路径 + 期望节号"
- **权衡**:接受。相比"复制 invariant 文本"的双真源风险,此项风险**可控且可诊断**

### R2:Stage 0 契约测试无法发现实现 bug,存在"绿灯但实现错"的风险

- **影响**:中高 —— 若 Stage 0 全绿就宣称 CEP 可用,是**误判**
- **缓解**:Stage 0 验收**明确限定**为"契约层就绪",不宣称"CEP 可用";`objective §0 DoD 1~6` 才是平台可用性判定
- **权衡**:接受。这是串行排队的必然代价(per `ADR-CEP-00 §2 Decision 3`)

### R3:`§9.2` 三项角色未指定 → Stage 0 验收 No-Go

- **影响**:高 —— **本 change 可实施但不可验收**
- **缓解**:`proposal.md` 与 `spec.md` 均显式标注该 gate;`tasks.md` 的最终 Phase 把"角色已指定"作为**前置断言**
- **未决**:验收方 / 结论消费者 / 平台维护方的具体人选属**用户决策**,planner 已登记为 P0 候选(per `objective §10`)

### R4:`trace.h5` 解析依赖 `h5py`,可能不在环境

- **影响**:低 —— 影响 `render_report.py` 的完整形态,不影响 metrics 单件套路径
- **缓解**:Stage 0 的工具先支持 `metrics.json` 单件套;`trace.h5` 解析作为**可选增强**(依赖缺失时明确降级提示,不静默失败)
- **权衡**:接受

### R5:16 份 ADR 的 lint 脚本可能较慢

- **影响**:低
- **缓解**:pre-commit 触发条件限定为"ADR 或 07 号有变更";全量扫描仅在 CI 与手动调用时执行

### R6:单 change 体积(13 文件 / 8 Phase)偏大

- **影响**:低 —— 但 umbrella 天然较大
- **缓解**:tasks 按 8 个 Phase 切分,每个 Phase 独立可验证 + 独立 commit;任一 Phase 可独立回滚
- **权衡**:umbrella 的定位就是"一次把骨架立住";拆分会打乱 KPI 串行队列

---

## Backward Compatibility 策略

| 面 | 策略 |
|----|------|
| 既有测试 | **0 回归** —— 新测试用独立标签 `[cep]`;不改任何既有 case |
| `test/CMakeLists.txt` | **不改** —— `file(GLOB test_*.cc)` 已平铺收编 |
| `include/abi/` | **0 diff** —— 23 ABI 冻结 |
| frozen headers | **0 diff** |
| `cpptlm_emulator.h` | 不进 |
| `docs/architecture/06-bis-*.md` | **正文不动**,仅顶部加 `## Status Update` 段 |

---

## 关键设计不变式(本 change 自身)

- **INV-D1**:契约测试**不复制** invariant 文本 —— 一律解析 ADR 文档(避免双真源)
- **INV-D2**:Stage 0 只断言**契约层**,不断言**实现层**(per D1)
- **INV-D3**:`tools/` 零项目内依赖(per D4)
- **INV-D4**:既有 `docs_sync_check.sh` **不改**(per D5)
- **INV-D5**:实验产物默认**不入库**(per D6)

---

**版本**: v1.0
**最后更新**: 2026-10-21
**维护者**: CppTLM Team(builder mode)
