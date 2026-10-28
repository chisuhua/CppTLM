# cep-instance-authority-partition Specification

## Purpose
定义 CEP **协议实例声明源**的**权威归属分区**契约:「哪些协议实例由 ArchForge 声明、哪些由本仓声明」
如何在**同一份声明文件**中机器可查地表达,以及 ⑥ 镜像闸如何在**不放宽**既有约束的前提下支持该分区。

**背景**:`include/tlm/cep/protocol_instances.json` 是 ArchForge 32 号 §6.1/§6.2 的**逐字段派生副本**;
`scripts/test/cep_decl_alignment_check.sh` 第 ⑥ 项据此强制「`instances[]` 全集 = ArchForge 声明全集」。
队列 #8 `cpptlm-cep-chi-rnf-subset` 交付**第 1 个**「非 ArchForge 贡献」的工业协议实例
(`chi_rnf`,per `07 号 §5.1.1` 的框架级工业族口径)⇒ 该前提在**实例粒度**上首次不成立。

本能力提供**平台级机制**;**后续 5 个非 ArchForge adapter**(`cxl-nonsymmetric` / `tilelink` /
`cxl-dcoh-symmetric` / `nvlink` / `ualink`)**复用**它,只填各自的登记行。

> **与 `cep-adapter-schema-contracts` 不同层**:后者是**实例声明内容**的契约(`perm` / `perm_upgrade` /
> 6→3 映射表 / 镜像闸的逐字段比对);本能力是**声明源权威归属**的契约。⇒ 两者 capability 名不同、
> 判据不重叠。**本能力 SHALL NOT 改动** `adapter_schema_contracts.hh` 的判据本体。

## ADDED Requirements

### Requirement: cep-instance-authority-partition

CEP SHALL 提供**实例级权威分区**机制:本仓自持的协议实例经**顶层登记键**进入 `instances[]`,
⑥ 镜像闸的**逐字段镜像比对**收窄到 `ArchForge ids ∩ repo ids`,而 ①~⑤ 的**语义校验扩面**到
repo-owned 实例(**不放松**)。

> **边界**:登记键落在 `protocol_instances.json` 的**顶层**(envelope 层)⇒
> `protocol_instance.schema.json` **零改动**(它校验单个实例对象,不校验 envelope);
> 改造落点 = 守门脚本(**不在** M1 冻结集)+ `ADR-CEP-12` 追加 `## Status Update`
> (**在** M1 冻结集 ⇒ 两步授权序列);`adapter_schema_contracts.hh` **零改动**。

#### Scenario: CR-07 —— ⑥ 镜像闸 SHALL 收窄到「ArchForge ids ∩ repo ids」,且未登记的额外实例 SHALL 判红

- **WHEN** 核对 `protocol_instances.json` 与 ArchForge 32 号 §6.1/§6.2 的镜像关系
- **THEN** 子条件 ① SHALL 成立:对 **`ArchForge ids ∩ repo ids`** 的每个实例,除 `status` 之外的
  **12 键 SHALL 逐字段严格相等**(类型严格、递归比较;键只存在于一侧即判红)
- **AND** 子条件 ② SHALL 成立:`status` SHALL **继续**被跳过(本仓权威字段,per 队列 #6 design `D-ζ`;
  本次一并**追认**进 ADR Status Update)
- **AND** 子条件 ③ SHALL 成立:本仓副本中**未登记**在 `repo_owned_instance_ids[]` 的额外实例 SHALL **判红**
  (「派生副本含未声明的实例」)
- **AND** 子条件 ④ SHALL 成立:登记 id 与 ArchForge 已声明 id **撞名** SHALL **判红**(权威迁移闸 ——
  不允许本仓「接管」ArchForge 的实例)
- **AND** **反向 fixture**:改 ArchForge 实例的**任一非 `status` 键**(12 键**穷举**)⇒ 断言**必红**
- **AND** **反向 fixture**:加一个**未登记**的额外实例 ⇒ 断言**必红**
- **AND** **反向 fixture**:把 `moesi_gpu` 登记进 `repo_owned_instance_ids[]`(撞名)⇒ 断言**必红**
- **AND** **正控**:仅 `status` 差异 ⇒ 断言**仍绿**(队列 #6 的既有正向 fixture 保持有效)

> **锚点**:`scripts/test/cep_decl_reverse_verify.sh` 的 ⑥ 收窄 fixture 组(既有「破坏 10 / 11」+
> 本 change 新增 3 条)+ `test_cep_chi_rnf_subset.cc` 的正控断言

#### Scenario: CR-08 —— ①~⑤ 的语义校验 SHALL **扩面**覆盖 repo-owned 实例

- **WHEN** 执行 `cep_decl_alignment_check.sh` 的 ①~⑤ 项
- **THEN** 子条件 ① SHALL 成立:**repo-owned 实例** SHALL 同样接受 ① 的 schema 全 13 属性校验
  (`jsonschema` 校验,**不降级**)
- **AND** 子条件 ② SHALL 成立:**repo-owned 实例** SHALL 同样接受 ② 的 `vc_id < vc_count` + 键对称 +
  `vc_id` 可解析检查
- **AND** 子条件 ③ SHALL 成立:**repo-owned 实例** SHALL 同样接受 ③ 的
  `ordering_assumption ⊆ vc_binding` 检查
- **AND** 子条件 ④ SHALL 成立:**repo-owned 实例** SHALL 同样接受 ④ 的 6 类消息类词表检查
- **AND** 子条件 ⑤ SHALL 成立:**repo-owned 实例** SHALL 同样接受 ⑤ 的网络集 ⊆ fabric 检查,
  且其 `vc_binding` 的**边端点** SHALL 落在该网络的已绑定 VC 内
- **AND** **反向 fixture**:让 repo-owned 实例违 schema(删 `required` 键)⇒ 断言**必红**
- **AND** **反向 fixture**:让 repo-owned 实例的消息类越出 6 类词表 ⇒ 断言**必红**
- **AND** **正控**:`moesi_gpu` / `mesif` 两条 ArchForge 实例 SHALL 仍全部 PASS

> **锚点**:`scripts/test/cep_decl_reverse_verify.sh` 的新增破坏用例 + `test_cep_chi_rnf_subset.cc` 的正控断言
>
> **⚠️ 不得放松**:扩面 = **增加**受检对象,不是**减少**约束。⇒ repo-owned 实例**不得**成为
> 「schema 不校验 / 词表不校验」的盲区 —— 否则本机制会退化成「绕过守门的通道」。

#### Scenario: CR-09 —— 权威归属的裁决 SHALL 被**追加式**登记进 ADR(不可变原则)

- **WHEN** 核对 `ADR-CEP-12-protocol-engine-capability-map.md` 的状态登记
- **THEN** 子条件 ① SHALL 成立:该 ADR SHALL 存在 `## Status Update` 段登记**实例级权威分区**裁决
  (含「原前提 → 新例外 → 不变项 → 触发 → 机器化」五要素)
- **AND** 子条件 ② SHALL 成立:该 Status Update SHALL **顺带追认**队列 #6 的 `status` 字段级豁免
  (当年**未**追加 ADR ⇒ 本次一并补上)
- **AND** 子条件 ③ SHALL 成立:该 ADR 的 **§2 决策 1 ~ §10 正文 SHALL 逐字未改**(per ADR 不可变原则)
- **AND** 子条件 ④ SHALL 成立:该追加 SHALL 走**两步授权序列**
  (先 commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → commit 清单)
- **AND** **反向 fixture**:把该 Status Update 的 `## Status Update` 标题改动 ⇒
  `cep_freeze_check.sh` 的 hash 校验 SHALL **必红**
- **AND** **反向 fixture**:跳过 `--update` 授权步骤直接改清单 ⇒ provenance 自检 SHALL **必红**

> **锚点**:`scripts/test/cep_freeze_check.sh`(既有 M1 产物 hash 校验 + provenance 自检)·
> `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` 的 `## Status Update` 段

#### Scenario: 权威分区的**消费方** SHALL 无需改动即看见 repo-owned 实例

- **WHEN** 装配期校验与协议桥消费路径读取实例声明
- **THEN** 子条件 ① SHALL 成立:`channel_dependency_graph.hh` 的**缺省声明源**消费 SHALL 看见
  repo-owned 实例(装配闸的逐 `(network, vc)` verdict SHALL 覆盖它)
- **AND** 子条件 ② SHALL 成立:`module_factory` 的桥消费路径 SHALL 看见 repo-owned 实例
  (桥的 `protocol_id_` 与实例声明 `family` 的**编号对齐**判据 SHALL 对它生效)
- **AND** 子条件 ③ SHALL 成立:`adapter_contracts_loader.hh` 的**全局输入**(实例声明集)SHALL 包含
  repo-owned 实例 ⇒ AC-02 的存在性判据 SHALL 对它生效
- **AND** 子条件 ④ SHALL 成立:`collective_plane_contract.hh` 的 VC 占用快照 SHALL 包含 repo-owned 实例
- **AND** **反向 fixture**:断开装配闸对该实例的消费 ⇒ 装配期 verdict SHALL 缺失该实例 ⇒ 断言**必红**
- **AND** **正控**:全部消费方 SHALL 对 `chi_rnf` 可见

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-08 消费方覆盖断言 + 装配期实测
>
> **⚠️ 为什么这是判据而非「实现细节」**:本机制的价值**全部**来自「6 处消费方零改动即看见 CHI」。
> 若任一消费方对 repo-owned 实例失明 ⇒ 机制退化为「把 CHI 藏在闸门外」⇒ 必须有断言钉住。

#### Scenario: 装载层 SHALL **参数化**,判据本体 SHALL **零改动**

- **WHEN** 判据实现被多个 adapter 复用
- **THEN** 子条件 ① SHALL 成立:`adapter_schema_contracts.hh` SHALL **零改动**
  (⇒ 不 MODIFIES `cep-adapter-schema-contracts` main spec ⇒ 不挤占 KPI slot)
- **AND** 子条件 ② SHALL 成立:装载层 SHALL 分为**全局输入**(AC-01/02/03:perm 值域 + 实例声明集 +
  静默升级声明源 + 无升级能力实例集)与**逐 adapter 输入**(AC-04/05:映射行 + 使用中消息类 +
  恒等标志/理由 + 稳定态投影 + 升级转换目标态 + 权限秩)
- **AND** 子条件 ③ SHALL 成立:逐 adapter 输入 SHALL 以**模板参数分派**供给(编译期类型,
  非运行期字符串分发)
- **AND** **反向 fixture**:让某个 adapter 漏填 AC-04 / AC-05 的必需输入 ⇒ 断言 SHALL **必红**
  (不得静默以默认值蒙混)
- **AND** **正控**:`moesi_gpu` 与 `chi_rnf` **两个** adapter 的 AC-01 ~ AC-05 verdict SHALL **同时**为 PASS

> **锚点**:`test_cep_chi_rnf_subset.cc` 的双 adapter verdict 断言 + `examples/cep_adapter_contracts.cc`
> 与 Catch2 测试的 **verdict 逐条相同**(单一真源纪律)
