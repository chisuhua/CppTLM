# cep-mesif-adapter Specification

## Purpose
定义 **MESIF**(`mesif` 协议实例,per `ADR-CEP-12` 决策 1~10 + `ADR-CEP-11 §3` 锁表 `mesif-1.0`)的
**实现层**可执行契约:**adapter 载体面** + **真插件引擎** + **静默升级声明** + **实例声明落地** +
**装配实测**。

MESIF 是 CEP 平台的**第 3 个**协议实例(第 1 个 = `moesi_gpu`,第 2 个 = `chi_rnf`)⇒ 落地即触发
`S1-3`(第 3 个 `implemented` 实例)—— Stage 1 最后一个未触发的准入前置。

**规约层已归档**(`cpptlm-cep-mesif-state-spec`,Entry #101 + #102 + #103)⇒ 本能力**只消费**其载体
(`include/tlm/cep/mesif_state_spec.hh`),**不重新推导**状态机。

> **边界**:不改 `protocol_instance.schema.json`(冻结面)· 不改 `adapter_schema_contracts.hh` ·
> 不改 `CohIfBundle` · 不扩 `perm` 4 值 · 不扩 `CATEGORY_*` 3 类 · 不改
> `openspec/specs/cep-sharer-tracker-base/spec.md`(第 6 种实现**另立** capability `cep-hierarchical-directory`)·
> 新增 invariant 编号数 = 0。
>
> **判据口径边界**:本能力的判据 SHALL 锁定**完备性 / 可审计性 / 待追认标记 / 内部自洽**,SHALL **不**锁定
> 各格**目标态**的**语义正确性** —— 后者由 ArchForge 侧逐格追认闭合(残余风险 R-1 显式登记)。
> **例外**:3 个锚点格(`I → NONE` / `E + Wr → M` / `S + Wr → S`)锁其**自洽性**。

## Requirements

### Requirement: cep-mesif-adapter

CEP SHALL 为 **MESIF** 提供:adapter 载体面 + 4 值 `perm` 投影 + 6→3 消息类映射 + 静默升级声明 +
实例声明落地 + **真插件引擎** + 装配实测。

> **语义分层**:协议语义 = 规约层载体(已归档)· `perm` 投影 = 跨域接口表达 · SharerTracker = 目录形态
> (三者**正交**,per `ADR-CEP-02` 分层纪律)。本能力交付**后两者的实例侧**,**不复写**规约层。

#### Scenario: MA-01 —— `MesifAdapter` SHALL **零自有表副本**,委托消费规约载体

- **WHEN** 核对 `MesifAdapter` 的状态转换来源
- **THEN** 子条件 ① SHALL 成立:`MesifAdapter` SHALL **不**携带自有 5×6 转换表 / 投影表副本 ⇒
  SHALL 以 `constexpr` 委托消费 `MesifStateSpec::kTable` + `perm_of()` +
  三元组(`kUniqueCleanState` / `kWriteEvent` / `kUniqueDirtyState`)
- **AND** 子条件 ② SHALL 成立:运行期 **35 格 pass-through** SHALL 成立 ——
  `MesifAdapter::transition(s,e) == MesifStateSpec::transition(s,e)` 且
  `MesifAdapter::perm_of(s) == MesifStateSpec::perm_of(s)`
- **AND** 子条件 ③ SHALL 成立:`mesif_adapter.hh` 的**去注释后**文本 SHALL **不**含 5×6 表字面量
  (静态扫描)
- **AND** **反向 fixture**:在 `mesif_adapter.hh` 内**伪造一份表** ⇒ 必红
- **AND** **正控**:载体自身 35 格全通 + 静态扫描无命中

#### Scenario: MA-02 —— 5→4 `perm` 投影 SHALL 与规约逐格相等

- **WHEN** 核对 `MesifAdapter` 的 `perm` 投影
- **THEN** 子条件 ① SHALL 成立:`M` / `E` / `S` / `I` / `F` **每个**态 SHALL 有 4 值投影(缺一即红)
  —— 4 值锁不破(per `ADR-CEP-02` CEN-Ib11)
- **AND** 子条件 ② SHALL 成立:**瞬态 SHALL 无投影**(per `INV-CEP-05-IC1`)—— 本规约 5 态**全稳定**
  ⇒ 平凡成立,但载体 SHALL 保留瞬态列表以支持未来扩展
- **AND** 子条件 ③ SHALL 成立:`F` 与 `M` 合并为 `WRITEBACK` 的**语义损失 ①** SHALL 登记,
  且措辞 SHALL 与 `chi_rnf` 的同类损失**可机器区分**(本实例**可补偿** / `chi_rnf` **不可补偿**)
- **AND** **反向 fixture**:改一格投影 ⇒ 必红
- **AND** **正控**:5 态投影齐全 + 瞬态列表为空

#### Scenario: MA-03 —— 6→3 消息类映射 SHALL 完备 + 恒等与否 SHALL 显式登记

- **WHEN** 核对 `MesifAdapter` 的 6→3 消息类映射表
- **THEN** 子条件 ① SHALL 成立:映射表 SHALL **完备覆盖**本实例用到的消息类
- **AND** 子条件 ② SHALL 成立:映射 SHALL 与 `adapter_schema_contracts.hh` 的
  `identity_reference_map()` **对拍一致**(平台方向默认表)
- **AND** 子条件 ③ SHALL 成立:**恒等或非恒等 SHALL 显式登记并携带理由**
  (per `ADR-CEP-12 §8.3` ② 分层纪律 —— 两维度正交,关系由 adapter 处的显式映射表承担,**非恒等**;
  但若本实例确实恒等,则 SHALL 登记恒等 + 理由,per 队列 #6 先例)
- **AND** 子条件 ④ SHALL 成立:`forward_data` SHALL **不**出现在 6 类词表里
  (per `ADR-CEP-12 §8.3` 补充约定 —— 它是 `snoop` 槽的**承载实例**)
- **AND** **反向 fixture**:删一行映射 / 写错类别 ⇒ 必红
- **AND** **正控**:映射表完备 + 对拍一致 + 恒等登记存在

#### Scenario: MA-04 —— 静默升级声明 SHALL 落地,且负控见证 SHALL **双向规则化**

- **WHEN** 核对 `perm_upgrades.json` 与其消费判据
- **THEN** 子条件 ① SHALL 成立:`silent_upgrades` SHALL 含 `mesif` 行(`UNIQUE` → `WRITEBACK`,
  对应 `E + Wr → M` 的静默升级,per `ADR-CEP-13` 决策 3 判据 (a)(b))
- **AND** 子条件 ② SHALL 成立:`derived_from.current_state` SHALL **不**再声称「`mesif` 无该能力」
  (该陈述在规约层落地后已是**事实性错误**)
- **AND** 子条件 ③ SHALL 成立:`no_upgrade_instances` 的消费 SHALL 为**规则级双向判据** ——
  **有声明 ⇒ 不在集合;无声明 ⇒ 在集合** —— SHALL **不**退化为「集合为空即恒真」的**空转断言**
- **AND** 子条件 ④ SHALL 成立:**反控 SHALL 可执行**:传入**合成输入**(移除 `mesif` 声明)
  ⇒ `mesif` SHALL **必然**出现在 `no_upgrade_instances` 中
- **AND** 子条件 ⑤ SHALL 成立:原「`mesif` 注入 ⇒ 红」的毒化形态 SHALL **已作废并替换**
  (`mesif` 变合法后不再是毒化)
- **AND** **反向 fixture**:破坏声明源的派生逻辑 ⇒ 必红
- **AND** **正控**:真实数据下 3 实例全有声明 ⇒ 集合为空

#### Scenario: MA-05 —— `mesif` 实例声明 SHALL 落地且其余 12 键 SHALL 逐字段全等

- **WHEN** 核对 `protocol_instances.json` 的 `mesif` 声明
- **THEN** 子条件 ① SHALL 成立:该实例 SHALL 含 `protocol_instance.schema.json` 的**全部 13 个
  `required` 顶层属性**(缺一即红)
- **AND** 子条件 ② SHALL 成立:`status` 在实现落地后 SHALL 为 `implemented`
  (per `ADR-CEP-12` 决策 9:`status` 是**本仓权威字段**,**不**参与 ⑥ 逐字段镜像比对)
- **AND** 子条件 ③ SHALL 成立:**其余 12 个非 `status` 键** SHALL 与 ArchForge `32 号 §6.2`
  **逐字段全等**(Entry #102 实测口径)⇒ `status` 豁免 SHALL **严格限于** `status` 一键
- **AND** 子条件 ④ SHALL 成立:`perm` SHALL 为**全部 4 值**;`family` SHALL = `industrial`;
  `adapter` SHALL = `MesifAdapter`;`protocol_version` SHALL = `mesif-1.0`
- **AND** **反向 fixture**:改 `mesif` 的任一**非 `status`** 键 ⇒ 必红
- **AND** **正控**:`moesi_gpu` / `chi_rnf` 两条既有声明 SHALL 仍 PASS

#### Scenario: MA-06 —— `MesifProtocolEngine` SHALL 是**真插件**,且 SHALL 支持 atomic 最小可用路径

- **WHEN** 核对 `MesifProtocolEngine` 的载体面
- **THEN** 子条件 ① SHALL 成立:引擎 SHALL 派生自 `ProtocolEngineBase<N>`(`N > 1`)
  且 SHALL **实现全部 10 条 canonical 流的 `on_*`**(per `ADR-CEP-02` canonical 10 流)
- **AND** 子条件 ② SHALL 成立:引擎 SHALL 通过 **JSON 链**装配(per `ADR-CEP-16` 明确排除
  programmatic bridge 的 C++ 路径)⇒ SHALL 有 `configs/cep_mesif_pe_minimal.json`
- **AND** 子条件 ③ SHALL 成立:引擎 SHALL **接受** atomic 事务(**不**触发拒绝闸)并按 `vc_binding`
  路由至 `ualink_fabric` 的 **VC 1**
- **AND** 子条件 ④ SHALL 成立:atomic 意图 SHALL 由 `CohIfBundle::atomic` 承载
  (1 bit,与 `perm` **正交**,per CEN-Ib11)⇒ `CohIfBundle` SHALL **零改动**
- **AND** 子条件 ⑤ SHALL 成立:`consistency_intent` **无独立字段**的裁决 SHALL **显式登记**
  (由 `perm` + `consistency_labels` 承载)⇒ SHALL **不**沉默
- **AND** 子条件 ⑥ SHALL 成立:引擎 SHALL 有**可观测点**能区分「走了 adapter 表」与「空转」
  (对齐 `NullProtocolEngineCounters` 的判别力口径 —— 只记调用次数的计数器**不算**有判别力)
- **AND** **反向 fixture**:atomic 走错 VC / atomic 被拒绝闸误杀 ⇒ 必红
- **AND** **正控**:合法 atomic 事务被接受且落 VC1

#### Scenario: MA-07 —— R-3 的补偿 SHALL **可观测**(dirty 掩码 + 写回接线)

- **WHEN** 驱动 `M + SnpShared → F` 序列并核对补偿路径
- **THEN** 子条件 ① SHALL 成立:转换前后 `perm` SHALL **同为 `WRITEBACK`** ——
  这是「已写回(干净)」与「仍脏」在 4 值接口上**不可分辨**的**正控**
- **AND** 子条件 ② SHALL 成立:tracker 视图的 **dirty 位 SHALL 由置转清** —— 这是**可分辨**的判据
- **AND** 子条件 ③ SHALL 成立:补偿维度 SHALL 为 **`LocalView::dirty` 掩码**(per 语义损失 ③)
  SHALL **不**与语义损失 ① 的「sharer 集合」维度混写
- **AND** 子条件 ④ SHALL 成立:引擎 SHALL 在该路径上把「写回发生」喂给 tracker ⇒
  补偿 SHALL **不**是自动发生的
- **AND** **反向 fixture**:**断掉**引擎→tracker 的写回接线 ⇒ dirty 位不转 ⇒ 必红
- **AND** **正控**:接线在位时 dirty 位转清

#### Scenario: MA-08 —— 装配实测 SHALL 通过,且 3 实例共存时通道依赖闸 SHALL 无环

- **WHEN** 装配 `mesif` 实例并跑装配期闸门
- **THEN** 子条件 ① SHALL 成立:`INV-CEP-12-IC2`(取代后 3 条件)**逐网络**校验 SHALL 通过 ——
  ① `ordering_assumption[n]` 的每个 `vc_id` ∈ `vc_binding[n].values()`;
  ② `vc_binding[n].values()` 中每个 `vc_id` **<** `fabric_config.networks[n].vc_count`;
  ③ `vc_binding[n]` 的每个消息类 ∈ 6 类规范词表
- **AND** 子条件 ② SHALL 成立:`evaluate_channel_dependency_gate` 在 **3 实例共存**声明下
  verdict SHALL **无环**,且 SHALL **逐 `(instance, network, vc)` 标注**
- **AND** 子条件 ③ SHALL 成立:一次 CohIf 事务 SHALL **穿过**引擎抵达对端
  ⇒ `INV-CEP-07-IC4` 的第 3 个实例实证
- **AND** **反向 fixture**:破坏 `mesif` 的 `vc_binding` ⇒ 必红
- **AND** **正控**:3 实例共存时闸绿 + 事务穿越

#### Scenario: MA-09 —— 判据 SHALL **配正控** + **可执行反向 fixture** + **「因正确原因红」机器判定**

- **WHEN** 核对本能力的判据面
- **THEN** 子条件 ① SHALL 成立:每条判据 SHALL **配正控**(纯负向断言 SHALL **不**裸奔)
- **AND** 子条件 ② SHALL 成立:每条判据 SHALL 有**可执行反向 fixture** ⇒ 破坏即必红
- **AND** 子条件 ③ SHALL 成立:反向验证脚本 SHALL 让每条 fixture **声明期望失败模式**
  (`compile-lock` / `runtime-predicate`),probe SHALL 输出**失败判据 id** ⇒ 脚本 SHALL
  **grep 断言「红在预期的判据上」**
- **AND** 子条件 ④ SHALL 成立:脚本 SHALL **不**沿用「编译 / 运行合并计数」口径
  ⇒ 「正确原因」SHALL 有机器区分(per `ADR-CEP-05` 判据纪律第 3 条:编译期锁**不得**只以
  「编译红」充当 ctest 判据)
- **AND** 子条件 ⑤ SHALL 成立:若 fixture 毒化**规约载体**,probe SHALL 把**两层判据 id 分开打印**
  ⇒ 归因不模糊
- **AND** 子条件 ⑥ SHALL 成立:**既有** reverse-verify 套件 SHALL **零回归**
  (`cep_mesif_state_spec_reverse_verify.sh` 仍 11/11)⇒ 防 fixture 空转
- **AND** **反向 fixture**:脚本命中 0 次 ⇒ SHALL **响亮报红**(SHALL **不**静默跳过)
- **AND** **正控**:干净载体全部判据通过
