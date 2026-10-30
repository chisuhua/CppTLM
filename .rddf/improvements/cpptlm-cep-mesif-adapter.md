---
name: cpptlm-cep-mesif-adapter
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-04
  - ADR-CEP-12
  - ADR-CEP-05
  - ADR-CEP-02
  - ADR-CEP-13
  - ADR-CEP-16
  - ADR-CEP-07A
状态: 📋 草稿(2026-10-30 —— 队列 #9 **实现层**;规约层 `cpptlm-cep-mesif-state-spec` 已归档(Entry #101)+ 跨仓逐格追认已闭合(Entry #102 / #103)⇒ **可启动**)
created: 2026-10-30
---

# improvement: cpptlm-cep-mesif-adapter

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 📋 草稿(2026-10-30)· **实现层** · 规约层已归档 + 追认已闭合
> **队列**: `obj-cep-platform §4.0` **队列 #9**(`cpptlm-cep-mesif-adapter`,Stage 1 / 2028-Q4)· **不新增队列行**(一个队列行可承载多个 change / capability,per 队列 #3 / #4 / #8 先例)
> **与规约层的分工**:规约层 `cpptlm-cep-mesif-state-spec`(**已归档**)交付**口径来源与推导依据**;本 change 交付**可执行插件**。二者是**前置 vs 实现**,不是替代。
> **关联 ADR**: `ADR-CEP-04`(第 6 种 SharerTracker 实现 + `AggregationAt::HIERARCHICAL`)· `ADR-CEP-12`(实例声明 schema + 权威分区)· `ADR-CEP-05`(`perm` 4 值投影 + `INV-CEP-05-IC1`)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)· `ADR-CEP-13`(域可见写判据 (a)(b))· `ADR-CEP-16`(排除 programmatic bridge)· `ADR-CEP-07A`(协议私有状态边界)
> **实施计划**: `.rddf/plans/2026-10-30-cpptlm-cep-mesif-adapter.md`(v0.2,经 oracle + metis 双审修订)
> **估时**: 10 周(实现层;规约层 4 周**已完成**)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1,per `obj §5.4`;当前实测 **0/3**)

---

## 架构依据

| 依据 | ADR | 决策要点 |
|------|-----|---------|
| 第 6 种 SharerTracker 实现 | `ADR-CEP-04 §13` | 新增 `HierarchicalDirectory`(`SYSTEM`/`EXACT`/`GLOBAL`/`HIERARCHICAL`)+ `AggregationAt::HIERARCHICAL`;`caps` **仍为 4 属性**(层级深度是实现参数,不是能力属性);**否决**「扩 caps 为 5 维」与「用 `TreeManager` 降级近似」 |
| 实例声明 schema + 权威分区 | `ADR-CEP-12 §8.3` / `§12` / `§13` | 6 类一致性消息词表落在 **Coherence Interface 层**,与链路层 `CATEGORY_*`(3 类)的关系由 **adapter 处的显式映射表**承担(非恒等);`status` 是**本仓权威字段**,豁免**严格限于** `status` 一键,其余 12 键逐字段严格 |
| `perm` 4 值投影 | `ADR-CEP-05 §13.1` | `perm` 维持 **4 值锁**;`INV-CEP-05-IC1` = 瞬态无投影;「UNIQUE 可静默升 WRITEBACK」必须由**声明源**表达 |
| canonical 10 流 + 4 值锁 | `ADR-CEP-02` | `CEN-Ib11` = `perm` 4 值锁;`atomic` 是**与 `perm` 正交**的独立属性(载体已有 1-bit 意图位) |
| 域可见写判据 | `ADR-CEP-13` 决策 3 | 判据 (a)(b) = 「干净独占态上的本地写命中 ⇒ 无总线事务 ⇒ 4 值 `perm` 无法表达」⇒ **静默升级必须声明** |
| 装配走 JSON 链 | `ADR-CEP-16` | 明确**排除** programmatic bridge 的 C++ 路径 |
| 协议私有状态边界 | `ADR-CEP-07A` | 协议自身的状态机是**协议层语义**,与 Stub Cache 的 5 态投影是**两个层级** |

**为什么队列 #9 是 Stage 1 的收官队列**:它是 Stage 1 最后一个 ⛔ 未触发的准入前置(`S1-3` = 出现第 3 张网**或**第 3 个协议实例)的**唯一解**,且是「SharerTracker 5 实现」里唯一被点名要落地第 6 实现的队列。

---

## 范围

### In Scope

| # | 交付 | 载体形态 |
|---|------|---------|
| **A** | **`MesifAdapter`** —— 5 态 × 6 事件 + 5→4 `perm` 投影 + 6→3 消息类映射 | `include/tlm/cep/mesif_adapter.hh`(header-only) |
| **B** | **`HierarchicalDirectory`** —— 第 6 种 SharerTracker 的**首个行为实现** | `include/tlm/cep/sharer_tracker.hh` 增 class |
| **C** | **`MesifProtocolEngine`** —— 真插件引擎 + atomic 最小可用路径 | `include/tlm/cep/mesif_protocol_engine.hh` + `configs/cep_mesif_pe_minimal.json` + `chstream_register.hh` 注册行 |
| **D** | **平台级静默升级声明** + 装配实测 + 通道依赖闸 3 实例共存 | `include/tlm/cep/perm_upgrades.json` + `protocol_instances.json` |
| **E** | **判据 + 反向 fixture** | `test/test_cep_mesif_adapter.cc` + `scripts/test/cep_mesif_adapter_reverse_verify.sh` |

### Out of Scope

- ❌ **不扩 `perm` 4 值锁** —— MESIF 5 态必须投影进现有 4 值(per `ADR-CEP-02` CEN-Ib11)
- ❌ **不扩 `CATEGORY_*` 词表** —— 仍 3 类链路层 flit 类别;6 类一致性词表与它的关系由 adapter 映射表承担
- ❌ **不扩 `CohIfBundle` 字段** —— `consistency_intent` **不新增独立字段**(见 Impact 段裁决)
- ❌ **不实现** `mesif` 的完整 atomic 语义(compare-and-swap 等)—— 仅**最小可用路径**(接受 + 路由 + 意图位)
- ❌ **不改** `protocol_instance.schema.json`(零改动,per 队列 #6 / #8 先例)
- ❌ **不实现**其余 4 种 SharerTracker(`SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull`)—— 归队列 #10 ~ #11
- ❌ **不做** H7 对拍基准(gem5 Ruby / ArchForge / RTL benchmark)—— 独立立项,本季度内单独落(per J-2)
- ❌ **不替 ArchForge 定义** `HierarchicalDirectory` 的行为规约 —— 本仓只做**显式登记 + 挂追认债**

---

## Why

### 三个阻塞已全部清零

| 阻塞 | 状态 | 证据 |
|------|------|------|
| R-1 第 6 种 SharerTracker | ⚠️ **裁决已落,行为未实现** | `ADR-CEP-04 §13`(Entry #96)⇒ `sharer_tracker.hh:172` enum + `:186-208` traits 行,**无 class** |
| ArchForge 规约缺口 G1 / G2 | ✅ 已闭合 | 规约层已归档(`mesif_state_spec.hh`,35 格 `kTable` + 逐格依据 + 9 条偏离 + 3 项语义损失) |
| 跨仓追认债 31/35 格 | ✅ 已闭合 | ArchForge `42 号` 35 格逐格结论 ⇒ `pending` **0/35**;`origin_kind` = **4/10/15/6** 与对方 §0 逐数一致(静态锁 ⑪ + 运行期断言 + 清单列比对**三重**锁定) |

### 为什么不追认完就不能开写

追认闭合只解了「**协议语义从哪来**」,没解「**实现要补什么**」。本仓实测出 **4 项残留**,其中 3 项若不在计划里显式登记,会在实施中途爆炸:

| # | 残留 | 性质 | 若不处置的后果 |
|---|------|------|-------------|
| **A** | `HierarchicalDirectory` 的**行为语义**在 ArchForge 与本仓**都无规约**(只有 caps 声明 + 一句注释级描述「L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray」) | ⚠️ **最大的「看似完整实则杜撰」风险** | 重蹈规约层 G1/G2 覆辙 —— 而规约层是靠「逐格 basis + pending 标记 + 跨仓追认」三件套才爬出来的 |
| **B** | `mesif` 的 `E + Wr → M` **静默升级**在平台级**无表达**(`perm_upgrades.json` 缺 `mesif` 行;其 `current_state` 自述「`mesif` 无该能力 ⇒ 不出现」是**事实性错误**) | ⚠️ 硬钉会在实施中途爆炸 | `test_cep_adapter_schema_contracts.cc:283-285` 钉死 `no_upgrade_instances == ["mesif"]` ⇒ 加 mesif 声明后必红;且该负控见证会**恒真空转**(见 AC-5) |
| **C** | `mesif.atomic.supported = true` + `intent_field = "consistency_intent"` —— 全仓 **grep 零消费点** | ⚠️ 声明了强能力却无任何任务/判据 | 结构性漏项;且 `chi_rnf` 的先例恰是 `supported=false` + 拒绝闸,两者语义不一致 |
| **D** | `S1-3` 的**计数口径**未定(`protocol_instances.json` **今日已有 3 个声明实例**,`mesif.status` 仍是 `planned`) | 验收口径 | 验收时扯皮 |

### 为什么不能「照 MOESI 推」

MESIF 5 态与 MOESI 6 态**词表都不同**,同名态语义未必同构(MESIF 的 `F`(Forward) = 单 Owner + 多 Sharer,MOESI 里**无对应态**)⇒ 照搬会产出「看似规约、实则杜撰」的转换表。规约层已把这个坑填平 ⇒ **实现层只消费、不重推**(见 AC-1:零自有表副本)。

### 为什么实现层要交付「adapter 消费规约载体」这个新模式

`moesi_gpu` / `chi_rnf` 的 adapter 都是**自带表**(`moesi_gpu_adapter.hh:73` 内嵌 `kStateTransition`)⇒ 双真源风险。`MesifAdapter` 改为**委托消费** `MesifStateSpec::kTable` ⇒ 「规约层 → 实现层」的单真源链成立,并成为队列 #10(CXL)的模板。

---

## What Changes

### 5 项交付的落点

| # | 文件 | 新增 / 修改 | 性质 |
|---|------|-----------|------|
| **A** | `include/tlm/cep/mesif_adapter.hh` | **新增**(header-only) | `MesifAdapter`:委托消费 `MesifStateSpec::kTable` + 5→4 投影 + 6→3 映射表 |
| **B** | `include/tlm/cep/sharer_tracker.hh` | **修改** | 增 `HierarchicalDirectory` class + **行为决策登记表**;`sharer_tracker.hh` **不在 M1 冻结集** ⇒ 无需两步授权 |
| **C** | `include/tlm/cep/mesif_protocol_engine.hh` | **新增** | `MesifProtocolEngine`(`ProtocolEngineBase<N>` 派生)+ atomic 最小可用路径 |
| **C** | `configs/cep_mesif_pe_minimal.json` | **新增** | 最小装配 JSON(对齐 `configs/cep_chi_rnf_pe_minimal.json`) |
| **C** | `include/chstream_register.hh` | **修改** | `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED("MesifProtocolEngine", ...)`(先例 `:133`) |
| **D** | `include/tlm/cep/perm_upgrades.json` | **修改** | 增 `{instance_id: "mesif", from: "UNIQUE", to: "WRITEBACK"}`;修订 `derived_from.current_state`(删掉「`mesif` 无该能力」这句**事实性错误**) |
| **D** | `include/tlm/cep/protocol_instances.json` | **修改** | `mesif.status` → `implemented`(**本仓权威字段**,不参与 ⑥ 镜像比对) |
| **D** | `test/test_cep_adapter_schema_contracts.cc` | **修改** | **重基线**:`silent_upgrades.size()` 2→3;`no_upgrade_instances` 1→0;负控见证改造为**双向规则** |
| **E** | `test/test_cep_mesif_adapter.cc` | **新增** | 契约测试(AC-1 ~ AC-15) |
| **E** | `scripts/test/cep_mesif_adapter_reverse_verify.sh` | **新增** | 反向 fixture(ctest 注册) |
| **E** | `scripts/README.md` | **修改** | 新脚本入 §CEP 守门脚本表 ⇒「12 项 CEP 脚本」计数 → **13**(三处同步) |
| — | `docs/architecture/07-coherence-exploration-platform.md` | **修改**(冻结面) | 漂移修正 **10 处** + 新 Status Update 第十五则 ⇒ **两步授权** |

### 实施顺序(per `.rddf/plans/2026-10-30-cpptlm-cep-mesif-adapter.md` v0.2)

```
WU-0 角色切换(planner → builder)
  └─ WU-1 improvement 草稿(本文件)
      └─ WU-2 建 change 目录(4 件套)—— 硬前置:openspec list --json 实测 < 1
          └─ WU-3 Phase 0  语义载体(A + B)
              └─ WU-4 Phase 1  插件引擎(C)
                  └─ WU-5 Phase 2  装配实测 + 平台级声明(D)
                      └─ WU-6 Phase 3  判据 + 反向 fixture(E)
                          └─ WU-7 Phase 4  status 翻转 + 07 号漂移修正 + 完成归档
                              └─ WU-8 S1-3 触发后:SLICC 备选重评 + 承接项 2 滚动复核
  └─ WU-9 H7 对拍基准(独立,不进本 change 硬前置)
```

> **WU-7 批内序(写死,不得乱序)**:① `status` 翻转 + `IC3` 计数同步 → ② 冻结面活口径修正(`07 号` 10 处 + `cep-sharer-tracker-base/spec.md` 2 处)→ ③ commit 冻结面产物 → ④ `cep_freeze_check.sh --update --authorized-by` → ⑤ commit 清单 → ⑥ `openspec archive` → ⑦ KPI / 状态列回填(**KPI 只能在归档之后改**)。

---

## Non-Goals

- ❌ **不**重新推导 MESIF 状态机 —— 规约层已交付且已逐格追认;实现层**只消费**
- ❌ **不**扩 `perm` 值域 / 不扩 `CATEGORY_*` / 不扩 `CohIfBundle` 字段
- ❌ **不**实现其余 4 种 SharerTracker
- ❌ **不**做 H7 对拍基准
- ❌ **不**替 ArchForge 定义 `HierarchicalDirectory` 行为规约(只登记 + 挂债)
- ❌ **不**在判据里锁**语义正确性** —— 判据只锁完备性 / 可审计性 / 待追认标记 / 内部自洽(per 规约层 design `D-ζ`)
- ❌ **不**主动 `git push`

---

## Acceptance

> 判据纪律 4 条铁律:① 纯负向断言**必须有正控** ② 每条判据须有**可执行反向 fixture**且**因正确原因红** ③ **编译期锁不得只以「编译红」充当 ctest 判据** ④ fixture 在状态变迁后会**空转** ⇒ 必须重新基线化且**响亮报红**(命中 0 次 ≠ 通过)

- [ ] **AC-1** `MesifAdapter` **零自有表副本**(委托消费 `MesifStateSpec::kTable` + 三元组)—— 运行期 35 格 pass-through + 静态扫描(文件内无 5×6 表字面量);反向 fixture:伪造一份表 ⇒ 必红
- [ ] **AC-2** 5→4 `perm` 投影逐格相等 + 瞬态无投影(`INV-CEP-05-IC1`,本规约 5 态全稳定 ⇒ 平凡成立)
- [ ] **AC-3** `HierarchicalDirectory` 的 **caps 4 字段 + behavior 2 字段**(`kTrackerTraits[5]`)与实例声明**三方相等**(JSON ↔ traits 行 ↔ class 成员);且**行使多级聚合**这一与 `FullMapDirectory` 的**差异点**(否则 = 「FullMap 克隆 + 纸面 caps」);contention 下 `STALL` 行为断言
- [ ] **AC-4** `MesifProtocolEngine` 可被 **JSON 链**装配;一次 CohIf 事务**穿过**引擎抵达对端;**引擎可观测点能区分「走了 adapter 表」与「空转」**(对齐 `NullProtocolEngineCounters` 的判别力口径)
- [ ] **AC-5** `perm_upgrades.json` 含 `mesif` 行 + `current_state` 自述已修订;`no_upgrade_instances` 负控见证改造为**双向规则**(有声明 ⇒ 不在;无声明 ⇒ 在),**反控用合成输入**(移除 mesif 声明 ⇒ mesif 必出现在集合里)⇒ 杜绝恒真空转;旧毒化形态「`mesif` 注入 ⇒ 红」**已作废并替换**
- [ ] **AC-6** atomic 最小可用路径:不被拒绝闸误杀 + 按 `vc_binding` 路由至 `ualink_fabric` VC1 + `CohIfBundle::atomic` 承载意图位;`consistency_intent` **无独立字段**的裁决**已显式登记**在 `design.md`
- [ ] **AC-7** **R-3 补偿可观测**:驱动 `M + SnpShared → F` 后 —— ① `perm` 前后**同为 `WRITEBACK`**(不可分辨的**正控**)② tracker 视图 **dirty 位由置转清**(可分辨的**判据**);反向 fixture:断掉引擎→tracker 写回接线 ⇒ 必红
- [ ] **AC-8** 6→3 消息类映射表:完备覆盖本实例用到的消息类 + 与 `identity_reference_map()` 对拍 + **恒等或非恒等必须显式登记并携带理由**;反向 fixture:删一行 / 写错类别 ⇒ 必红
- [ ] **AC-9** 通道依赖闸在 **3 实例共存**声明下 verdict **无环** + **逐 `(instance, network, vc)` 标注**;反向 fixture:破坏 `mesif` 的 `vc_binding` ⇒ 必红
- [ ] **AC-10** `mesif.status == "implemented"` + 其余 **12 个非 `status` 键**与 ArchForge `32 号 §6.2` **逐字段全等** + `INV-CEP-12-IC3` 计数同步
- [ ] **AC-11** **S1-3 口径钉死 = `implemented` 实例数 ≥ 3**;触发证据三件套(`status` + `obj §4.0` 行改 + 台账 Entry);**真实义务 = `ADR-CEP-12 §8.4` 的 SLICC 式备选重评**
- [ ] **AC-12** 承接项 2 的**滚动复核第 2 次**(节奏源 umbrella design D4.5)结论已落台账
- [ ] **AC-13** 每条判据配正控 + 可执行反向 fixture + **「因正确原因红」机器判定**(每条 fixture 声明期望失败模式 `compile-lock` / `runtime-predicate`;probe 输出**失败判据 id** ⇒ 脚本 grep 断言「红在预期判据上」;**两层判据 id 分离**);**既有 reverse-verify 套件零回归**(仍 11/11)
- [ ] **AC-14** 收尾:`tasks.md` **全勾** → **完成归档**(delta spec **合并**进 `openspec/specs/`)+ `obj §4.7.3` **17 落点逐项**已改 + 队列 #9 行的 **owner 义务已再转指** + M1 **一次**两步授权已完成 + 脚本计数 **12 → 13**
- [ ] **AC-15** 门禁全绿:13 项 CEP 脚本 + `docs_sync --strict` + `openspec validate --changes --strict` + `format --check` rc=0;`ctest -R cep` 全绿;全仓 `cpptlm_tests` 全绿;工作区干净

---

## Capabilities

| # | 能力 | 消费者 |
|---|------|-------|
| **C-1** | MESIF 协议实例的**可执行状态机**(5 态 × 6 事件,表驱动,消费规约载体) | 装配期校验 / 实验平台 |
| **C-2** | MESIF 的 **5→4 `perm` 投影**(含 `F`/`M` 合并的语义损失登记) | Stub Cache 投影面 |
| **C-3** | 第 6 种 SharerTracker 实现 `HierarchicalDirectory` 的**行为载体**(多级聚合 + `EXACT` 目录) | `mesif` 实例声明;R-1 销号点 |
| **C-4** | **R-3 补偿能力** —— 由 tracker 的 `dirty` 掩码回答「谁持有脏副本」,弥补 `perm` 接口上「已写回 / 仍脏」不可分辨 | 写回路径的正确性判别 |
| **C-5** | MESIF 的**真插件引擎**(CohIf 承载 + JSON 链装配 + 通道账目) | `INV-CEP-07-IC4` 的第 3 个实例实证 |
| **C-6** | **平台级静默升级表达**(`perm_upgrades.json` 的第 3 条) | `ADR-CEP-13` 判据 (a)(b) 的平台级落地 |
| **C-7** | **atomic 最小可用路径**(接受 + VC 路由 + 意图位) | `mesif.atomic.supported = true` 的兑现 |
| **C-8** | **「adapter 消费规约载体」的单真源模式** | 队列 #10(CXL)的模板 |

---

## Impact

### 冻结面

| 成员 | 是否触碰 | 处置 |
|------|---------|------|
| `docs/architecture/07-coherence-exploration-platform.md` | ⚠️ **是**(漂移修正 10 处 + 新 Status Update) | **两步授权**:commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → commit 清单(**全 change 只此一次**) |
| `openspec/specs/cep-sharer-tracker-base/spec.md` | ⚠️ **2 处活口径** | 契约零改动;「**唯一**行为实现」→「**首个**」+ 队列 #6~#9 → #10~#11 ⇒ 与 `07 号` **同批**授权 |
| `include/tlm/cep/protocol_instance.schema.json` | ❌ 否 | 零改动(per 队列 #6 / #8 先例) |
| `include/tlm/cep/fabric.json` / `fabric.schema.json` | ❌ 否 | 零改动 |
| `ADR-CEP-*.md` | ❌ 否 | 零改动(ADR 不可变) |
| `include/tlm/cep/sharer_tracker.hh` | ❌ 否 | **不在 M1 冻结集** ⇒ 自由改 |
| `include/tlm/cep/perm_upgrades.json` / `protocol_instances.json` | ❌ 否 | 「诞生即不冻结」⇒ 自由改 |

### 残余风险登记(**显式,不隐藏**)

| # | 风险 | 本 change 的处置 |
|---|------|----------------|
| **R-1** | 按档批量的 **14 格**证据强度弱于逐格(档位理由是**归纳**,逐格是**枚举**)⇒ 某格**语义判断**有误时,档位理由覆盖不到 | ⚠️ **作为已知输入登记**,**不得**因「追认闭合」而写成「无残余风险」。缓解依赖 ArchForge 的「独立复核 gate 制度化」(该纠正已在 `M+SnpShared` 上实证生效) |
| **R-3** | 纠正 `M+SnpShared`(`S`→`F`)后,「**已写回(干净)**」与「**仍脏**」在 4 值 `perm` 上**同值不可分辨** ⇒ **写回事实在 perm 接口不可见** | ⚠️ **补偿维度 = `LocalView.dirty` 掩码 + 写回接线**(`sharer_tracker.hh:247-252`),**不是**「谁持有」—— 后者是**损失 ①**(「是否还有其他 sharer」)的补偿,与损失 ③ 是**两个不同维度**。⚠️ **补偿不会自动发生** ⇒ 引擎须在 `M+SnpShared→F` 路径上把「写回发生」喂给 tracker ⇒ AC-7 使其**可观测** |
| **A** | `HierarchicalDirectory` 行为语义**无真源** ⇒ 实现者被迫自推 | 行为决策**逐条 `basis` 式登记** + 标 pending + **挂跨仓追认债**(类比规约层 D-α)⇒ **不得**写成「已裁决」 |
| **C** | `atomic.supported = true` 与实现可能脱节 | **最小可用路径** + `consistency_intent` 无独立字段的裁决**显式登记** ⇒ 不沉默 |

### 跨仓债

| # | 债 | 承接方 | 形式 |
|---|-----|--------|------|
| **1** | `HierarchicalDirectory` 的**行为规约**(层级数 / `ESCALATE_UP` / fold 合并规则 / 满表拒绝 / **F 态表示** —— `SnoopState` 只有 `I/S/E/M`,**无 F**) | **ArchForge** | 本仓 `basis` 登记 + `pending` 标记 ⇒ 跨仓追认提案(类比规约层 D-α) |
| **2** | `mesif` 的 `atomic` 语义细节(`consistency_intent` 的协议侧定义) | **ArchForge** | 本 change 只做最小可用路径 + 显式登记 |
| **3** | **引擎侧建模决策 E-1 ~ E-6**(`F` 的 wire 投影 / `CLEAN`+`FORWARD` 合并 / 节点侧不建模 home / 静默升级两段式 / 在飞集合不建模 / VC 只记意图) | **ArchForge** | `basis` 登记 + `pending`(design `D-λ`);与债 #1 同款机制 |

### 对既有载体的影响

| 载体 | 影响 |
|------|------|
| `test_cep_adapter_schema_contracts.cc` | **必须重基线**(负控见证恒真空转陷阱,见 AC-5) |
| `cep_mesif_state_spec_reverse_verify.sh` | **零回归要求**(11/11);若毒化锚点因载体变更而失效,须**重新基线化**并**响亮报红** |
| `07 号 §5.2` / `§5.1.1` / `§4` | 漂移修正 10 处;**invariant 计数不得变**(66 / 编号位 67 / 15 组) |
| `obj §4.0` 队列 #9 行 | 状态列 + 准入前置 / 孤儿前置的 **owner 义务再转指** |
| `scripts/README.md` + `AGENTS.md` + `obj` | 「12 项 CEP 脚本」计数 → **13**(三处同步) |

### 风险与回滚

| 风险 | 等级 | 缓解 | 回滚 |
|------|------|------|------|
| 实现层自己拍一套转换表 | 🟡 中 | AC-1(零副本 + 静态扫描 + 伪造表 fixture)⇒ **结构性不可能** | — |
| `HierarchicalDirectory` 行为语义自推 | 🟡 中 | 行为决策 `basis` 登记 + 跨仓追认债 + AC-3 差异化行为 | 归档后若发现 ⇒ 追加台账 + 修订 main spec |
| R-3 补偿被「声称」而未被「演示」 | 🟡 中 | AC-7(perm 同值正控 + dirty 位判据 + 断接线 fixture) | 同上 |
| 负控见证恒真空转 | 🟡 中 | AC-5(双向规则 + 合成输入反控) | — |
| 冻结面误触 | 🟡 中 | `cep_freeze_check.sh` rc≠0 即拦 | `git revert` + `--update` 回填 |
| 17 落点漏同步 | 🟡 中 | §4.7.3 清单逐项勾 + AC-14 | 补齐 + 台账勘误 entry |
