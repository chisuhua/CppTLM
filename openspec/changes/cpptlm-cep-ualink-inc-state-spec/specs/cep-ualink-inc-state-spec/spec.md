# cep-ualink-inc-state-spec Specification(delta)

## Purpose

定义 **UALink INC** 在 CEP 平台上的**规约层可执行契约**:INC 四原语的**可判定**语义 ·
与既有原语集的**覆盖关系**(⛔ 不新增原语)· **逐格 `basis`** · **9 项 `pending`** ·
四条证书(C-1′ ~ C-4)的**规约面**。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #194**(improvement)· **#193**(三则 ADR SU + 冻结面授权)
· `ADR-CEP-01` SU 2026-11-03(#4)· `ADR-CEP-03` · `ADR-CEP-04` SU 2026-11-03 · `ADR-CEP-11`)
⇒ 本能力**不重推语义**,只把裁决**机器化**。

> **边界**:不改任何 ADR 正文 · 不改 `protocol_instance*.json` / `fabric.json` / `collective_planes.*` ·
> 不改 canonical **10 流**清单 · **不**定义 reduce 算子族(唯一真源 = 既有 `ReduceOp` 恰 6 值)·
> **不**改 `CollectivePlaneIf` 的 **7 方法**面 · **0 新增** ABI · **0 新增** `INV-` 编号 · **不产出任何性能结论**
>
> **合规边界**:UALink Common 2.0 的**规范正文**受 **Evaluation Copy Agreement** 约束(实测)
> ⇒ 与本仓 **CXL ECA §3** 同型 ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本能力的全部语义指向**仓内已签锚点**
> ⇒ **9 项**细粒度语义维持 `pending` ⇒ 本层**不得**推理填空。

---

## ADDED Requirements

### Requirement: cep-ualink-inc-state-spec

CEP SHALL 为 **UALink INC** 提供一份**可执行规约层契约**:四原语的可判定语义 · 与既有原语集的覆盖关系 ·
逐格 `basis` · `pending` 白名单 · 四条证书的规约面。

#### Scenario: UIS-01 —— INC 原语 SHALL **不新增**原语,且覆盖集 ⊆ 既有原语集

- **WHEN** 读取规约层载体的 INC 四原语声明
- **THEN** SHALL **恰为 4** 个:`ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce`
      (⚠️ 此「恰 4」指 **INC 的四个操作**,与平台原语族 `CollectiveOp` 的「恰 4」
      (`ADR-CEP-01 §7 V7` / `ADR-CEP-03 §9 V2`)**是两个不同的事实**)
- **AND** 其**覆盖集** SHALL ⊆ 既有原语集(`REDUCE` / `BROADCAST` / `MULTICAST` / `BARRIER`)
      ⇒ 「覆盖」判 ⊆(跨载体元测试 **X-1**)·「非副本」判 ⊊(**X-1b**)
- **AND** `AllReduce` SHALL 被映射为 **`REDUCE` + `BROADCAST` 的复合**(⛔ **不是**第 5 原语)
- **AND** **反向 fixture**:载体加第 5 个 INC 原语 ⇒ 判据 SHALL 必红(归因 `UIS-01`)

#### Scenario: UIS-02 —— reduce 算子 SHALL **只引用**既有算子族

- **WHEN** 规约层涉及归约算子
- **THEN** SHALL **只引用**既有 `ReduceOp`(恰 **6** 值)⇒ 载体 SHALL **不**定义 / 复制任何算子枚举
- **AND** **反向 fixture**:载体自带算子枚举 ⇒ 判据 SHALL 必红(归因 `UIS-02`)

#### Scenario: UIS-03 —— partial failure SHALL 走**显式 `Fault`**(不得静默)

- **WHEN** 集合面出现成员失败
- **THEN** SHALL 走既有 `CollectiveEvt::Fault` 通道(不得静默丢弃)
- **AND** ⛔ SHALL **不**新增 `CollectivePlaneIf` 方法(7 方法面已硬锁)
- **AND** **反向 fixture**:把失败路径改为静默 ⇒ 判据 SHALL 必红(归因 `UIS-03`)

#### Scenario: UIS-04 —— `ReadReduce` 的 **coherence 可见性边界** SHALL 显式 pending

- **WHEN** 涉及 `ReadReduce` 结果对缓存副本的可见性
- **THEN** 该格 SHALL 标记为 **`pending`**(引锚点摘要 `N-3`)⇒ ⛔ **不得**被当作已定稿语义
- **AND** **反向 fixture**:影子副本把该格的 `kind` 由 `Pending` 改为 `SelfDerived` ⇒ 判据 SHALL 必红
      (归因 `UIS-04`,**编译期红**:影子副本的 `static_assert` 文案以 `UIS-04: ` 开头)

#### Scenario: UIS-05 —— INC 与 coherence 流的**排序 / 原子性**要求 SHALL 显式 pending

- **WHEN** 涉及 INC 原语与 coherence 流的排序 / 原子性关系
- **THEN** 该格 SHALL 标记为 **`pending`**(引 `N-4`)⇒ ⛔ **不得**推理填空
- **AND** **反向 fixture**:把该格改成 `self-derived` ⇒ 判据 SHALL 必红(归因 `UIS-05`)

#### Scenario: UIS-06 —— INC 的**控制面归属** SHALL 显式 pending

- **WHEN** 涉及 INC 的 group / epoch / 成员集合是否由 UALink 定义
- **THEN** 该格 SHALL 标记为 **`pending`**(引 `N-5`)⇒ 若规范**不**定义 ⇒ 须**另行登记**为「本仓自推导」
- **AND** **反向 fixture**:把该格改成 `self-derived` ⇒ 判据 SHALL 必红(归因 `UIS-06`)

#### Scenario: UIS-07 —— 集合 VC SHALL **继承**既有映射,且映射不一致 ⇒ 该网**不得**用 M-2

- **WHEN** 判定某 network 是否可用共享 VC 模式(M-2)
- **THEN** 集合平面 SHALL **不声明**自己的 `(message_class → vc_id)` 映射 ⇒ 它**继承**该 network 上
  **全部有绑定的**既有实例的映射
- **AND** 该继承**良定义**的充要条件 = 该 network 上全部有绑定的既有实例的映射**逐类相等**
- **AND** **若该 network 存在实例间映射不一致 ⇒ 该 network SHALL 不得使用 M-2**
- **AND** 参照域 SHALL 为**两域并集**(ArchForge 派生域 + 本仓 `repo_owned_instance_ids[]`)
- **AND** **反向 fixture**:给集合平面加独立映射 ⇒ 判据 SHALL 必红(归因 `UIS-07`)

#### Scenario: UIS-08 —— `ualink_4` 的 **`flow_support`** SHALL 待定(不得先填后证)

- **WHEN** 声明 `ualink_4` 的 `flow_support.snoop` / `snoop_resp`
- **THEN** 在 Task 0 普查给出条款级依据前 SHALL 保持**待定**(引 `N-8`)
- **AND** 它 SHALL **决定** tracker 选型(候选集内取值 + 逐实例理由登记,per `ADR-CEP-04` SU 2026-11-03)
- **AND** ⛔ SHALL **不**由「公开来源未提 snoop」推断「无 snoop」(那是**推理填空**)
- **AND** ⚠️ **载体归属**(per `ADR-CEP-01` 附录「**悬空判据禁令**」:载体尚不存在者该判据在载体落地前不可执行):
      `ualink_4` 的**实例声明**(含 `flow_support` / `perm` 的**具体取值**)由**实现层 change**
      `cpptlm-cep-ualink-inc` 交付 ⇒ 本 change 只承载「该格**保持 pending**」这一条 ⇒ **无悬空判据**
- **AND** **反向 fixture**:影子副本把该格的 `kind` 由 `Pending` 改为 `SelfDerived`(冒充已定稿)
      ⇒ 判据 SHALL 必红(归因 `UIS-08`,**编译期红**:文案以 `UIS-08: ` 开头)

#### Scenario: UIS-09 —— `ualink_4` 的 **`perm` 子集** SHALL 待定

- **WHEN** 声明 `ualink_4` 的 `perm`
- **THEN** SHALL 区分**值域**(4 值锁)与**支持子集**;在依据到位前**子集** SHALL 待定(引 `N-9`)
- **AND** ⚠️ **载体归属**同 `UIS-08`:`ualink_4` 的**实例声明**由**实现层 change** 交付
      ⇒ 本 change 只承载「该格**保持 pending**」这一条
- **AND** **反向 fixture**:影子副本把该格的 `kind` 由 `Pending` 改为 `SelfDerived`(冒充已定稿)
      ⇒ 判据 SHALL 必红(归因 `UIS-09`,**编译期红**:文案以 `UIS-09: ` 开头)

#### Scenario: UIS-10 —— 逐格 `basis` SHALL 完整,且 `pending` 白名单 SHALL **逐项相等**

- **WHEN** 读取载体的 basis 表
- **THEN** **每格** SHALL 有 `ref`(指向锚点摘要的 `A-n` / `C-n` / `N-n`)⇒ **无 `ref` 的格即红**
- **AND** `kind = self-derived` 的格 SHALL **不**以公开来源(`A-n`)为依据(公开来源撑不住细粒度语义)
- **AND** 载体的 `pending` 条目集合 SHALL 与锚点摘要的 `N-1` ~ `N-9` **逐项相等**(不多不少)
      ⇒ 由**跨载体元测试 X-2**承担(Phase 2 · **可执行**:读锚点摘要解析 `N-n` 集合)+ 人工审计面
      ⇒ 载体**只**保证**自洽型**(表大小 == `kPendingCount` · id 唯一且落在 `N1`~`N9` · id ↔ ref 一一对应)
      ⇒ ⛔ **不**要求「id == 数组下标」⇒ **可逆性**:语义转正时审计键 id **保持稳定**
- **AND** 每格 `basis` 的 `kind` SHALL 与其 `ref` 的**类别**自洽
      (`self-derived` ⇒ **全部** token 为 `C-` 类;`pending` ⇒ **至少一个** `N-` 类 token ·
      **不得**含 `A-` 类 · 其余 token **只允许** `C-` 类 ⇒ **未知类 token 判否**)
- **AND** **反向 fixture**(`F-10`):删任一格 `ref` / 多报一项 pending / 把 pending 改成 self-derived
      / 把 `self-derived` 的 `ref` 冒充成 `N-n` 或 `A-n` / 把某格 `ref` **纯**冒充成 `C-n`
      ⇒ 判据 SHALL 必红(归因 `UIS-10`)

#### Scenario: UIS-11 —— 四条证书的**规约面** SHALL 齐备,且 ⛔ **不得**以「重跑 DAG」为判据

- **WHEN** 核对 M-2 证书的规约面
- **THEN** 四条证书的**逐边依据 + 参照域** SHALL 齐备;「该网映射不一致 ⇒ 不得用 M-2」SHALL **可判定**
- **AND** **U-1**(「VC 隔离确实避免协议级死锁」)与 **U-2**(同 VC 内 coherence ↔ collective 的相互阻塞
  **不被 VC 级依赖图表达**)SHALL 登记为**未检验**
- **AND** ⛔ **不得**把「把集合平面喂进 `evaluate_channel_dependency_gate` 重跑」当作证书判据
  (实测**恒真**:边按 network 声明 ⇒ 任一实例的图 = 声明边表的**诱导子图** ⇒ DAG 的子图仍无环)
- **AND** **反向 fixture**(`F-11`):影子副本翻转 `kDagRerunIsCertificate`(把证书规约面改成
      「重跑 DAG 即可」)⇒ 判据 SHALL 必红(归因 `UIS-11`,**编译期红**:文案以 `UIS-11: ` 开头)

---

> **锚点**:`include/tlm/cep/ualink_inc_state_spec.hh` 的编译期断言 + `test/test_cep_ualink_inc_state_spec.cc`
> 的运行期兜底判据 + **11 组反向 fixture**(影子毒化 ⇒ 必红 + 归因 id **命中**)+ `scripts/test/` 的探针
> + `test/CMakeLists.txt` 的 `ctest` 注册。
>
> ⚠️ **归因 id 的唯一登记处 = 本 change 的 `design §5`**;本 spec 只引用,不重复登记。
> ⚠️ **证书的判据面**属 `cep-collective-plane-contract`(由**实现层**的 delta 承载)⇒ 本 spec **只**给规约面,⛔ **不重复实现**(禁双实现)。
