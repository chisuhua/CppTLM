# cep-mesif-state-spec Specification

## Purpose
定义 **MESIF**(`mesif` 协议实例,per `ADR-CEP-12` 决策 1~10 + `ADR-CEP-11 §3` 锁表 `mesif-1.0`)的
**规约层**契约:**5 态状态机** + **5→4 `perm` 投影表** + **逐格推导依据** + **待追认标记**。

**背景**:ArchForge 全仓检索(2026-10-29)确认 MESIF **只有 5 态词表**,**无**状态转换规约、**无** 5→4 投影表
⇒ 队列 #9 `cpptlm-cep-mesif-adapter` 的**实现层**缺少可判据化的语义来源。本能力交付**规约层**,
实现层由队列 #9 承担。

> **边界**:不改 `protocol_instance.schema.json`(冻结面)· 不改 `adapter_schema_contracts.hh` ·
> 不改 `CohIfBundle` · 不扩 `perm` 4 值 · 不引入新的 SharerTracker 实现 ·
> **不**实现 `MesifAdapter`(队列 #9)。

>
> **判据口径边界**:本能力的判据 SHALL 锁定规约的**完备性 / 可审计性 / 待追认标记 / 内部自洽**,
> SHALL **不**锁定各格**目标态**的**语义正确性** —— 后者由 ArchForge 侧逐格追认闭合
> (per `ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md` 的风险 ②:第 ②/③ 档依据
> 可被追认纠正 ⇒ 未追认的值 SHALL **带着 `pending` 标记**留在规约里,SHALL NOT 被固化为「既定事实」)。

## ADDED Requirements

### Requirement: cep-mesif-state-spec

CEP SHALL 为 **MESIF** 提供:5 态状态机规约 + 5→4 `perm` 投影表 + **逐格推导依据** + **待追认标记**。

> **语义分层**:规约 = **协议语义**;`perm` 投影 = **跨域接口表达**;SharerTracker = **目录形态**
> (三者**正交**,per `ADR-CEP-02` 的分层纪律)。本能力只交付前两者。
>
> **规约的规范性载体** = `include/tlm/cep/mesif_state_spec.hh`(可执行:表驱动 + 编译期锁);
> `openspec/changes/**/design.md` 只承载**推导口径与统计**,**不**复写逐格表(避免归档后断链的双真源)。


#### Scenario: MS-01 —— MESIF 的 5 态 SHALL 全部有 4 值 `perm` 投影

- **WHEN** 核对 MESIF 规约的状态 → `perm` 投影
- **THEN** 子条件 ① SHALL 成立:`M` / `E` / `S` / `I` / `F` **每个**态 SHALL 有 4 值投影
  (缺一即红)—— 4 值锁不破(per `ADR-CEP-02` CEN-Ib11)
- **AND** 子条件 ② SHALL 成立:`I` 的投影 SHALL 为 `NONE`;`F` 的投影 SHALL 为 `WRITEBACK`
  (per ArchForge `32 号:529` 的一手原文)
- **AND** 子条件 ③ SHALL 成立:**瞬态 SHALL 无投影**(per `INV-CEP-05-IC1`)—— 本规约当前**不声明瞬态**
  ⇒ 该子条件平凡成立,但载体 SHALL 保留瞬态列表以支持未来追认后的扩展
- **AND** **反向 fixture**:让某个态**无投影** ⇒ 必红;给瞬态加投影 ⇒ 必红
- **AND** **正控**:`chi_rnf` / `moesi_gpu` 的既有投影 SHALL **零回归**

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-01 断言 + 2 条反向 fixture + 正控断言

#### Scenario: MS-02 —— MESIF 的状态转换表 SHALL 完备,且**写只在独占态合法**

- **WHEN** 核对 MESIF 的 (状态 × 事件) 转换表
- **THEN** 子条件 ① SHALL 成立:每个 (态 × 事件) 组合 SHALL 有定义(无空洞、无越界)—— 5 态 × 6 事件 = **30** 格
- **AND** 子条件 ② SHALL 成立:`E + Wr → M` 的转换 SHALL 存在(目标态 = `M`)
- **AND** 子条件 ③ SHALL 成立:**写只在独占态合法** ⇒ `S` 上的写 SHALL **状态不变**(须先 `Upgrade`)
  (per ArchForge `25-nsa:552-554` 的 Owner 唯一性 + 同型先例 IHI0050 §B4.1)
- **AND** 子条件 ④ SHALL 成立:`perm` 秩 SHALL 在**本地事件**(`Rd` / `RdUnique` / `Wr` / `Upgrade`)上
  **单调不降**;且 `E + Wr → M` SHALL **严格升高**(`UNIQUE` → `WRITEBACK`)
  —— ⚠️ **口径限定到本地事件**:远端 snoop 必然降秩(如 `M + SnpShared → S` 为 3 → 1),不可纳入
- **AND** 子条件 ⑤ SHALL 成立:**唯一允许的秩回落例外** SHALL 是 `F → E`(`RdUnique` / `Upgrade` 两格),
  且该例外 SHALL 被**显式登记**为语义损失(per MS-04 的损失 ②)
- **AND** **反向 fixture**:挖掉转换表的一格 ⇒ 必红;让 `S` 上的写改变状态 ⇒ 必红;
  让本地事件上的秩下降(非 `F → E` 例外) ⇒ 必红;让 `E + Wr` 不再严格升高 ⇒ 必红
- **AND** **正控**:`chi_rnf` 的同类断言 SHALL 仍 PASS

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-02 断言 + 4 条反向 fixture + 正控断言

#### Scenario: MS-03 —— 规约的每一格 SHALL 带**推导依据**与**待追认状态**

- **WHEN** 核对规约的推导可审计性
- **THEN** 子条件 ① SHALL 成立:转换表(30 格)与投影表(5 格)的**每一格** SHALL 带**非空**的
  `basis`(`kind` + `ref` + `note` 三字段)⇒ 35 格**无一遗漏**
- **AND** 子条件 ② SHALL 成立:`kind` 与 `pending` SHALL **双向一致** ——
  `kind == Primary`(一手原文)⇒ `pending == false`;`kind != Primary`(推导 / 类比)⇒ `pending == true`
  ⇒ 「一手原文」与「待追认」**不得**错配(否则跨仓债被静默放过 / 已确认项被反复追认)
- **AND** 子条件 ③ SHALL 成立:**待追认格数 SHALL 可查**(供「追认进度」观测),
  且 SHALL 与推导记录中的统计**逐数一致**
- **AND** 子条件 ④ SHALL 成立:`kind == Primary` 的格 SHALL 引用 **ArchForge 的具体位置**
  (形如 `32 号:529` / `25-nsa:599` 的文件 + 行/条款)⇒ 不得写成无定位的泛化表述
- **AND** **反向 fixture**:清空某格的 `basis` ⇒ 必红;把一手原文格的 `ref` 改成无定位表述 ⇒ 必红;
  把一手原文格标为待追认 ⇒ 必红;把推导格标为已追认 ⇒ 必红;改动载体使统计与推导记录不一致 ⇒ 必红
- **AND** **正控**:`F` 态投影的 `basis` SHALL 为 `Primary` 且指向 `32 号:529` ⇒ 待追认计数**不**含它

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-03 断言 + 5 条反向 fixture + 正控断言

#### Scenario: MS-04 —— `F` 与 `M` 的投影合并 SHALL 被**显式登记**语义损失

- **WHEN** 核对 `F`(Forward:单 Owner + 多 Sharer)与 `M` 同投影 `WRITEBACK` 的后果
- **THEN** 子条件 ① SHALL 成立:该合并 SHALL 被**显式登记**(不得静默)
- **AND** 子条件 ② SHALL 成立:登记 SHALL 注明「是否还有 sharer」在 4 值下**不可分辨**
- **AND** 子条件 ③ SHALL 成立:登记 SHALL 注明该损失**可由 Sharer Tracker 补偿** ——
  `mesif` 的 `sharer_tracker.impl = HierarchicalDirectory`(caps `SYSTEM`/`EXACT`/`GLOBAL`/`HIERARCHICAL`,
  per `ADR-CEP-04 §13`)⇒ EXACT + GLOBAL 目录**能**回答「谁持有」
  ⇒ ⚠️ 与 `chi_rnf` 的 `SD`/`UD` 合并损失(**不可补偿**,RN 侧无 sharer set)**措辞必须不同**
- **AND** 子条件 ④ SHALL 成立:登记 SHALL 注明**次生损失** —— `F → E` 的**秩回落**(`WRITEBACK` → `UNIQUE`),
  且 SHALL 注明该例外的**成因**是「`F` 无独立的 `perm` 值」
- **AND** 子条件 ⑤ SHALL 成立:登记 SHALL **不得**以「给 `F` 新增第 5 个 `perm` 值」作为消除手段
  (per `ADR-CEP-02` CEN-Ib11 的 4 值锁)
- **AND** **反向 fixture**:登记缺「可由 Sharer Tracker 补偿」的限定 ⇒ 必红(误用 `chi_rnf` 的措辞);
  登记缺次生损失(秩回落)⇒ 必红

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-04 断言 + 2 条反向 fixture

#### Scenario: MS-06 —— 逐格追认清单 SHALL 与**载体**逐格一致

- **WHEN** 核对仓内逐格追认清单(`docs/implementation/2026-10-29-cep-mesif-state-spec-recognition-checklist.md`)与载体的 `kTable`
- **THEN** 子条件 ① SHALL 成立:清单的**转换表** SHALL **恰好 30 行**,每行的
  `(态, 事件, 目标态, kind, pending)` SHALL 与载体**逐格相等**
- **AND** 子条件 ② SHALL 成立:清单的**投影表** SHALL **恰好 5 行**,每行的
  `(态, perm, kind, pending)` SHALL 与载体**逐格相等**
- **AND** 子条件 ③ SHALL 成立:清单登记的**待追认格数** SHALL 与载体 `pending_count()` **相等**
  ⇒ 跨仓债的刻度**唯一**
- **AND** 子条件 ④ SHALL 成立:清单**不**得存在**载体里没有**的行(防清单里出现未登记的「追认项」)
- **AND** **反向 fixture**:把清单某行的 `kind` 改掉 / 删掉一行 ⇒ 必红
- **AND** **正控**:清单的 30 + 5 行与载体**零差异**

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-06 断言 + 2 条反向 fixture
> (反向 fixture 以「同谓词 + 变异输入」实现:解析出的行集合与载体比对)

#### Scenario: MS-05 —— 规约与**协议模板**的偏离 SHALL 被**显式登记**

- **WHEN** 核对 MESIF 规约相对 CHI / MOESI 模板的偏离
- **THEN** 子条件 ① SHALL 成立:**每一处**偏离模板取值 SHALL 被**显式登记**(偏离格 + 模板取值 + 本表取值 + 理由)
- **AND** 子条件 ② SHALL 成立:偏离清单 SHALL 覆盖 MESIF **特有**的 `F`(Forward)态相关格
  (CHI **无**对应态)⇒ 这部分 SHALL 被标注为**追认价值最高**
- **AND** 子条件 ③ SHALL 成立:偏离登记 SHALL 与载体的实际取值**一致**(逐格核对)⇒ 清单与载体不得脱节
- **AND** **反向 fixture**:删除清单中的一条偏离 ⇒ 必红;清单登记的取值与载体不符 ⇒ 必红

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-05 断言 + 2 条反向 fixture
