# cep-cxl-dcoh-symmetric-state-spec Specification

## Purpose
定义 **CXL 3.0 对称(DCOH)**在 CEP 平台上的**规约层可执行契约**:
**3 套状态空间**(`DevCacheState` 4 / `DcohViewState` 3 / `HomeDirState` 3,**不得合并**)·
CXL.mem **6** 通道 + CXL.cache 双向各 3 · ⭐ `device_initiated_snoop_set == {BISnp}` ·
`perm` **仅**从缓存代理域 **4→4** 投影 · 窄义 **5 谓词**「对称」定义 · 逐格 `basis` 两轴标注。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #154** / **#153** / **#122**
+ `ADR-CEP-04` 的 `## Status Update(2026-11-02)` + `ADR-CEP-02` / `ADR-CEP-04` / `ADR-CEP-11` / `ADR-CEP-12` / `ADR-CEP-17`)
⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改任何 ADR 正文 · 不改 `protocol_instance.schema.json`(**值级枚举已含 `REQUESTER` ⇒ 零改动**)
> · 不改 `CxlCacheAdapter` / `cxl_state_spec.hh` · 不改 canonical 10 流清单
> · **不建模** transient 态 / 错误语义 / poison / viral · **不写死** opcode / 字段宽度 / credit / 排序
> · **0 新增** ABI · **0 新增** invariant 编号数 · **不产出任何性能结论**
>
> **合规边界**:CXL 规范受 **Evaluation Copy Agreement §3** + **AI Acceptable Use Policy** 约束
> ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本能力的全部 basis 指向**公开非规范来源**的锚点摘要条目号
> ⇒ **10 项**细粒度语义登记 `pending`,走「**人类成员条款摘要**」流程。

---

## Requirements

### Requirement: cep-cxl-dcoh-symmetric-state-spec

CEP SHALL 为 **CXL 3.0 对称(DCOH)**提供一份**可执行规约层契约**:
3 套状态空间 · 6+3 通道 · `{BISnp}` 判别式 · `perm` 按域分离 · 窄义 5 谓词「对称」定义 · 逐格 `basis` 两轴标注。

#### Scenario: CDS-01 —— **3 套状态空间** SHALL **不合并**

- **WHEN** 读取规约载体的三套状态空间
- **THEN** `DevCacheState` SHALL 为 **4** 态(`M` / `E` / `S` / `I`)· `DcohViewState` SHALL 为 **3** 态(`I` / `S` / `A`)· `HomeDirState` SHALL 为 **3** 态(`I` / `S` / `E`)
- **AND** 三套 SHALL **按域**互斥 —— ⚠️ 三套**各自**都含名为 `I` / `S` 的元素 ⇒ 互斥性 SHALL **按域**比较,**不得**按名字比较
- **AND** 编译期断言 SHALL 覆盖「基数 == 4 / 3 / 3」+「两两交集为空」
- **AND** **反向 fixture**:合并任两套 SHALL 必红且归因命中

#### Scenario: CDS-02 —— 通道 SHALL 恰 6 + 3,且**不写死 opcode**

- **WHEN** 读取载体的通道词表
- **THEN** CXL.mem SHALL 为 **6** 条(`M2S Req` / `M2S RwD` / `S2M NDR` / `S2M DRS` / `S2M BISnp` / `M2S BIRsp`)· CXL.cache SHALL 为**每方向 3** 条(`Req` / `Rsp` / `Data`)
- **AND** 通道 SHALL 以**枚举名**表达 ⇒ **SHALL NOT** 写死 opcode / 字段宽度 / credit(per `pending` R-S1 / R-S2)
- **AND** 编译期断言 SHALL 覆盖两处基数

#### Scenario: CDS-03 —— ⭐ `device_initiated_snoop_set` SHALL == **`{BISnp}`**

- **WHEN** 读取「设备发起的 snoop」集合
- **THEN** 该集合 SHALL **非空**且**恰含 1** 类(= `BISnp`)
- **AND** 该集合 SHALL **SHALL NOT** 为空 —— ⚠️ 队列 #10(CXL 2.0 非对称)的形态是「设备侧**无** snoop 发起」⇒ **本族禁止沿用该断言**
- **AND** **反向 fixture**:置空 ⇒ 必红且归因命中

#### Scenario: CDS-04 —— `perm` SHALL **仅**从 `DevCacheState` 投影,且为 **4→4**

- **WHEN** 读取 `perm` 投影表
- **THEN** 投影行 SHALL 恰 **4** 行(`I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK`),且 **SHALL NOT** 含 `F` 行
- **AND** `DcohViewState` / `HomeDirState` 的元素 SHALL **不**出现在投影行中
- **AND** ⚠️ `DcohViewState` 的 `A` SHALL 被显式表达为「**不可单值投影**」(它是 host 可能处于 M/E/S/I 的**集合**)
- **AND** `perm` 枚举 SHALL **维持 4 值**(per `ADR-CEP-02` CEN-Ib11)⇒ **SHALL NOT** 新增第 5 值

#### Scenario: CDS-05 —— 「对称」SHALL 钉为窄义 **5 谓词**合取

- **WHEN** 读取载体的「对称」判别式
- **THEN** 它 SHALL 为 **5** 个谓词的合取:P1 设备发起的 BI snoop 通道(target = host)· P2 hosting device 实现覆盖其 HDM 区间的 SF / 目录 · P3 硬件一致性跨 ≥ 2 个独立 host · P4 P2P 访问不经 host CPU · P5 被 snoop 的是 caching host 而**非** peer device cache
- **AND** 载体 SHALL **SHALL NOT** 把「对称」表述为 peer-to-peer 全对称(公开来源明写 CXL 3.0 **不满足**经典 symmetric)
- **AND** **反向 fixture**:把任一谓词置假 ⇒ 判别式 SHALL 必红

#### Scenario: CDS-06 —— 逐格 `basis` SHALL 带 `kind` / `pending` / `ref` 两轴标注

- **WHEN** 读取载体的每个格的 `basis`
- **THEN** 每格 SHALL 同时带 **证据强度轴**(`kind` ∈ `public-direct` / `public-analogical` / `self-derived`)与**钉版强度轴**(`pending` ∈ `true` / `false`)
- **AND** 每格 SHALL 带 `ref` ⇒ 其值 SHALL 指向**锚点摘要的条目号**,**SHALL NOT** 指向规范原文
- **AND** `pending == false` SHALL **同时**满足 ① 来源 `pin=strong` **且** ② **引用 / 再分发许可已人工确认**
- **AND** **反向 fixture**:去掉 `pending` 标记 ⇒ 必红且归因命中

#### Scenario: CDS-07 —— **10 项 `pending`** SHALL 逐条登记

- **WHEN** 核对载体登记的 `pending` 项
- **THEN** SHALL **恰好**覆盖 **10** 项:opcode / 字段宽度 / credit / 排序 · BISnp 变体与 BIRsp payload、block 粒度 · bias 表编码 · bias 表**替换 vs 复用** · home 目录 dirty 语义与 SF 精度 · transient 命名 · 错误语义 · peer 集语义与 GFD 规模 · 是否存在扩展态
- **AND** 这些项 SHALL **SHALL NOT** 被推理填空 ⇒ 载体中**不得**出现其**具体取值**的断言
- **AND** 每项 SHALL 可从锚点摘要**逐条寻址**

> **锚点**:`test/test_cep_cxl_dcoh_state_spec.cc` 的编译期断言 + 逐格 basis 核对 + 4 组反向 fixture
