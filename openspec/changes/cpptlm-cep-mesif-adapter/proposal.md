# cpptlm-cep-mesif-adapter: MESIF 协议实例实现层 —— adapter + 真插件引擎 + 第 6 种 SharerTracker 行为实现 + 装配实测

> **状态**: 🟡 **Proposed**(2026-10-30 转正 —— change 目录已建;KPI 并发 **0/3 → 1/3**)
> **转正依据**:KPI slot 可用(`openspec list --json` 实测 CEP 内部 Proposed 并发 **0** < 1,per `obj §4.7.2` step 0 硬前置)
> **队列**: `obj-cep-platform §4.0` **队列 #9**(`cpptlm-cep-mesif-adapter`,Stage 1 / 2028-Q4)· **不新增队列行** —— 一个队列行可承载多个 openspec change / capability(per 队列 #3 / #4 / #8 先例)⇒ **顺序 / 季度口径不变**(五处同改不触发)
> **与规约层的分工**:规约层 `cpptlm-cep-mesif-state-spec`(**已归档**,Entry #101 + #102 + #103)交付**口径来源与推导依据**;本 change 交付**可执行插件**。⇒ 本 change 是规约层的**消费方**,不是替代。
> **关联 ADR**: `ADR-CEP-04 §13`(第 6 种 SharerTracker 实现 + `AggregationAt::HIERARCHICAL`)· `ADR-CEP-12 §8.3` / `§8.4` / `§12` / `§13`(6 类词表分层 / SLICC 备选重评 / `status` 权威)· `ADR-CEP-05 §13.1`(`perm` 形态演进 + `INV-CEP-05-IC1`)· `ADR-CEP-02`(canonical 10 流 + CEN-Ib11 4 值锁)· `ADR-CEP-13` 决策 3(域可见写判据 (a)(b))· `ADR-CEP-16`(排除 programmatic bridge)· `ADR-CEP-07A`(协议私有状态边界)
> **规划输入**: `.rddf/improvements/cpptlm-cep-mesif-adapter.md`(5 段草稿)· **实施计划**: `.rddf/plans/2026-10-30-cpptlm-cep-mesif-adapter.md`(v0.2,经 oracle + metis 双审修订)
> **冻结面**: `protocol_instance.schema.json` / `adapter_schema_contracts.hh` / `CohIfBundle` **零改动**;`openspec/specs/cep-sharer-tracker-base/spec.md` **契约零改动**(第 6 种实现**另立** capability)—— 但该文件有 **2 处活口径**在 WU-3 实施期实测为**事实性错误**(「**唯一**行为实现 `FullMapDirectory`」+「其余 4 种随队列 #6~#9」)⇒ 与 `07 号` **同批**修正(「唯一」→「首个」· 队列 → #10~#11)⇒ 冻结面改动**两件、一次**两步授权
> **估时**: 10 周(实现层;规约层 4 周**已完成**)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1,per `obj §5.4`)

---

## Why

### 三个阻塞已全部清零

| 阻塞 | 状态 | 证据 |
|------|------|------|
| **R-1** 第 6 种 SharerTracker `HierarchicalDirectory` | ⚠️ **裁决已落,行为未实现** | `ADR-CEP-04 §13`(Entry #96)⇒ `sharer_tracker.hh:172` enum + `:186-208` traits 行,**无 class** |
| **G1 / G2** ArchForge 侧无 MESIF 状态机规约 / 无 5→4 投影表 | ✅ 已闭合 | 规约层已归档:`include/tlm/cep/mesif_state_spec.hh`(35 格 `kTable` + 逐格依据 + 9 条偏离 + 3 项语义损失) |
| **跨仓追认债** 31/35 格待追认 | ✅ 已闭合 | ArchForge `42 号` 35 格逐格结论 ⇒ `pending` **0/35**;`origin_kind` = **4/10/15/6** 与对方 §0 **逐数一致**(静态锁 ⑪ + 运行期断言 + 清单列比对**三重**锁定) |

⇒ 队列 #9 的**实现层可启动**。

### 但追认闭合只解了「协议语义从哪来」,没解「实现要补什么」——本仓实测出 4 项残留

| # | 残留 | 性质 | 若不处置的后果 |
|---|------|------|-------------|
| **A** | `HierarchicalDirectory` 的**行为语义**在 ArchForge 与本仓**都无规约** —— 只有 caps 声明 + 一句注释级描述(`sharer_tracker.hh:138-141`「L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray」) | ⚠️ **最大的「看似完整实则杜撰」风险** | 重蹈规约层 G1/G2 覆辙 —— 而规约层是靠「逐格 basis + `pending` 标记 + 跨仓追认」三件套才爬出来的 |
| **B** | `mesif` 的 `E + Wr → M` **静默升级**在平台级**无表达** —— `perm_upgrades.json` 无 `mesif` 行;其 `derived_from.current_state` 自述「`mesif` 无该能力 ⇒ 不出现」是**事实性错误** | ⚠️ 硬钉会在实施中途爆炸 | `test_cep_adapter_schema_contracts.cc:283-285` 钉死 `no_upgrade_instances == ["mesif"]` ⇒ 加声明后必红;且该负控见证会**恒真空转**(见 design `D-θ`) |
| **C** | `mesif.atomic.supported = true` + `intent_field = "consistency_intent"` —— 全仓 **grep 零消费点** | ⚠️ 声明了强能力却无任何任务/判据 | 结构性漏项;且 `chi_rnf` 的先例恰是 `supported = false` + 拒绝闸,两者语义不一致 |
| **D** | `S1-3` 的**计数口径**未定 —— `protocol_instances.json` **今日已有 3 个声明实例**,`mesif.status` 仍是 `planned` | 验收口径 | 验收时扯皮 |

### 为什么不能「照 MOESI 推」

MESIF 5 态与 MOESI 6 态**词表都不同**,同名态语义未必同构(MESIF 的 `F`(Forward) = 单 Owner + 多 Sharer,MOESI 里**无对应态**)⇒ 照搬会产出「看似规约、实则杜撰」的转换表。规约层已把这个坑填平 ⇒ **实现层只消费、不重推**(design `D-α`)。

### 为什么这个队列值得做

1. **它是 Stage 1 最后一个 ⛔ 未触发准入前置(`S1-3`)的唯一解** —— `S1-3` 的判据是「出现第 3 张网**或**第 3 个**`implemented`** 协议实例」;`mesif` 翻转后即第 3 个。
2. **它是 `ADR-CEP-04 §13` 裁决的落地义务** —— 「SharerTracker 5 实现」长期停在「1/5 有行为实现」,而第 6 种已被 ADR 签发却无实现 ⇒ 本 change 是 R-1 阻塞项的**真正销号点**。
3. **`mesif` 是目录型跨卡架构**(分级目录 + 定向 snoop)⇒ 落地它 = 证明 Directory & Ordering Plane 能承载**非 snoop 型**协议 ⇒ 直接 de-risk 队列 #10(CXL 非对称)与 #12(CXL 对称)。
4. **它建立「adapter 消费规约层载体」的单真源模式** —— `moesi_gpu` / `chi_rnf` 的 adapter 都是**自带表**(双真源风险);`MesifAdapter` 改为**委托消费** ⇒ 成为队列 #10 的模板。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `include/tlm/cep/mesif_adapter.hh` | `MesifAdapter` —— **零自有表副本**,委托消费 `MesifStateSpec::kTable` + 三元组;5→4 `perm` 投影;6→3 消息类映射表 |
| `include/tlm/cep/mesif_protocol_engine.hh` | `MesifProtocolEngine` —— `ProtocolEngineBase<2>` 派生的**真插件引擎** + atomic 最小可用路径 + tracker 写回接线 |
| `configs/cep_mesif_pe_minimal.json` | 最小装配 JSON(对齐 `configs/cep_chi_rnf_pe_minimal.json` 先例) |
| `test/test_cep_mesif_adapter.cc` | 契约测试(AC-1 ~ AC-15) |
| `scripts/test/cep_mesif_adapter_reverse_verify.sh` | 反向 fixture 脚本(ctest 注册) |
| `openspec/specs/cep-mesif-adapter/spec.md` | capability `cep-mesif-adapter` 的契约(归档时由 delta spec 合并产生) |
| `openspec/specs/cep-hierarchical-directory/spec.md` | capability `cep-hierarchical-directory` 的契约(第 6 种 SharerTracker 行为实现 + 行为决策登记 + 跨仓追认债) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/tlm/cep/sharer_tracker.hh` | 增 `HierarchicalDirectory` class(第 6 种实现的**行为面**)+ **行为决策登记表**;**不在 M1 冻结集** ⇒ 无需两步授权 |
| `include/tlm/cep/perm_upgrades.json` | 增 `{instance_id: "mesif", from: "UNIQUE", to: "WRITEBACK"}`;**修订** `derived_from.current_state`(删掉「`mesif` 无该能力」这句**事实性错误**) |
| `include/tlm/cep/protocol_instances.json` | `mesif.status` 由 `planned` → `implemented`(per `ADR-CEP-12` 决策 9:`status` 是**本仓权威字段**,不参与 ⑥ 镜像比对) |
| `include/chstream_register.hh` | 增 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED("MesifProtocolEngine", ...)`(先例 `:133`) |
| `test/test_cep_adapter_schema_contracts.cc` | **重基线**:`silent_upgrades.size()` 2→3;`no_upgrade_instances` 1→0;负控见证改造为**双向规则**(design `D-θ`) |
| `scripts/README.md` | 新脚本入 §CEP 契约守门脚本表 ⇒「**12 项 CEP 脚本**」计数 → **13** |
| `docs/architecture/07-coherence-exploration-platform.md` | **冻结面**:漂移修正 **10 处** + 新 `## Status Update` 第十五则 ⇒ **两步授权** |
| `openspec/specs/cep-sharer-tracker-base/spec.md` | **冻结面**:2 处活口径(「唯一行为实现」→「首个」· 队列 #6~#9 → #10~#11);契约面零改动 ⇒ 与上一行**同批**授权 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 队列 #9 行状态 + 准入前置 / 孤儿前置的 **owner 义务再转指**;§5.2 KPI 回填;§11 台账追加 |
| `.rddf/roadmap/phases/phase-10.md` · `.rddf/roadmap.md` · `AGENTS.md` | KPI 当前态 **0/3 → 1/3 → 0/3**(五处同改) |

### 边界(**零改动**)

- `protocol_instance.schema.json`(**M1 冻结面**)**零改动** —— 本 change 不新增 schema 字段
- `adapter_schema_contracts.hh` **零改动** —— 判据本体不动
- `CohIfBundle` **零改动** —— `atomic` 意图位已存在(`cep_coherence_bundles_tlm.hh:114`,1 bit,与 `perm` 正交);**不**为 `consistency_intent` 新增字段(design `D-δ`)
- `openspec/specs/cep-sharer-tracker-base/spec.md`(**M1 冻结面**)**契约零改动** —— 第 6 种实现**另立** capability;仅 **2 处活口径**随 `07 号` 同批修正(见上表)
- `perm` 仍 **4 值** · `CATEGORY_*` 仍 **3 类** · `ordering_assumption` / `vc_binding` / `consistency_labels` / `sharer_tracker` 的**声明值**不动(仅 `status` 翻转)
- **不**新增 invariant 编号(沿用 `INV-CEP-05-IC1` / `INV-CEP-12-IC1~IC3` 的**行为化**)

---

## Non-Goals

- ❌ **不**重新推导 MESIF 状态机 —— 规约层已交付且已逐格追认;实现层**只消费**(design `D-α`)
- ❌ **不**扩 `perm` 4 值锁 / 不扩 `CATEGORY_*` 3 类 / 不扩 `CohIfBundle` 字段
- ❌ **不**实现 `mesif` 的完整 atomic 语义(compare-and-swap 等)—— 仅**最小可用路径**(接受 + VC 路由 + 意图位)
- ❌ **不**实现其余 4 种 SharerTracker(`SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull`)—— 归队列 #10 ~ #11
- ❌ **不**做 H7 对拍基准(gem5 Ruby / ArchForge 参考模型 / RTL benchmark)—— 独立立项,本季度内单独落
- ❌ **不**替 ArchForge 定义 `HierarchicalDirectory` 的行为规约 —— 本仓只做**显式登记 + 挂跨仓追认债**(design `D-β`)
- ❌ **不**改 `ADR-CEP-*.md` 正文(ADR 不可变;状态变化只追加 `## Status Update`)
- ❌ **不**在判据里锁**语义正确性** —— 判据只锁完备性 / 可审计性 / 待追认标记 / 内部自洽(design `D-ζ`)

---

## Impact

### 冻结面

| 成员 | 是否触碰 | 处置 |
|------|---------|------|
| `docs/architecture/07-coherence-exploration-platform.md` | ⚠️ **是** | **两步授权**:commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → commit 清单(**全 change 只此一次**) |
| `include/tlm/cep/protocol_instance.schema.json` | ❌ 否 | 零改动 |
| `include/tlm/cep/fabric.json` / `fabric.schema.json` | ❌ 否 | 零改动 |
| `openspec/specs/cep-sharer-tracker-base/spec.md` | ⚠️ **2 处活口径** | 契约零改动;「唯一行为实现」→「首个」+ 队列范围 ⇒ 与 `07 号` **同批**两步授权 |
| 其余 8 份 main spec | ❌ 否 | 零改动 |
| `include/tlm/cep/sharer_tracker.hh` / `perm_upgrades.json` / `protocol_instances.json` | ❌ 否 | 「诞生即不冻结」/ 不在冻结集 ⇒ 自由改 |

### 残余风险登记(**显式,不隐藏**)

| # | 风险 | 本 change 的处置 |
|---|------|----------------|
| **R-1** | 按档批量的 **14 格**证据强度弱于逐格(档位理由是**归纳**,逐格是**枚举**)⇒ 某格**语义判断**有误时,档位理由覆盖不到 | ⚠️ **作为已知输入登记**,**不得**因「追认闭合」而写成「无残余风险」。缓解依赖 ArchForge 侧的「独立复核 gate 制度化」(该纠正已在 `M+SnpShared` 上实证生效) |
| **R-3** | 纠正 `M+SnpShared`(`S`→`F`)后,「**已写回(干净)**」与「**仍脏**」在 4 值 `perm` 上**同值不可分辨** ⇒ **写回事实在 perm 接口不可见** | ⚠️ **补偿维度 = `LocalView.dirty` 掩码 + 写回接线**(`sharer_tracker.hh:247-252`),**不是**「谁持有」—— 后者是**损失 ①**(「是否还有其他 sharer」)的补偿,与损失 ③ 是**两个不同维度**。⚠️ **补偿不会自动发生** ⇒ 引擎须在 `M+SnpShared→F` 路径上把「写回发生」喂给 tracker(design `D-γ`) |
| **A** | `HierarchicalDirectory` 行为语义**无真源** ⇒ 实现者被迫自推 | 行为决策**逐条 `basis` 式登记** + 标 `pending` + **挂跨仓追认债**(design `D-β`) |

### 跨仓债

| # | 债 | 承接方 | 形式 |
|---|-----|--------|------|
| **1** | `HierarchicalDirectory` 的**行为规约**(层级数 / `ESCALATE_UP` 使用 / fold 合并规则 / 满表拒绝语义 / **F 态表示** —— `SnoopState` 只有 `I/S/E/M`,**无 F**) | **ArchForge** | 本仓 `basis` 登记 + `pending` 标记 ⇒ 跨仓追认提案(类比规约层 D-α) |
| **2** | `mesif` 的 `atomic` 协议侧语义(`consistency_intent` 的定义) | **ArchForge** | 本 change 只做最小可用路径 + 显式登记 |

### KPI

Stage 1 = **1 slot**;并发 **0/3 → 1/3**(建目录时)→ **0/3**(归档时)。变更总数与串行规则**不动** ⇒ `ADR-CEP-00 §2 决策 3` 的 **14 个 change** 算术**不变**(队列 #9 承载多个 change / capability,per 队列 #3 / #4 / #8 先例)。

### 触发 S1-3 后的义务

`S1-3` 的**真实义务**是 `ADR-CEP-12 §8.4` 登记的 **SLICC 式「协议本地虚拟网络 + 装配期绑定」备选重评** —— 该备选的登记理由正是「当前为 0 个 `implemented` 实例 —— 这是将来最便宜的改点,**别让这个选项无声死掉**」。⇒ 归档后必须出重评结论(维持嵌套形态 / 迁移 SLICC 式)。
