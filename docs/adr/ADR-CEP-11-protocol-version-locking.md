# ADR-CEP-11: 协议版本锁定与演进策略

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 引用的外部协议规范版本锁定 + 版本漂移的处置路径**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §5.1(协议族 → 实例 → adapter)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md` §3(supersede 表)+ §5(gem5 类比)
> - 真源 3:`ADR-CEP-12-protocol-engine-capability-map.md`(`protocol_version` 字段)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
> - 路线图:`.rddf/roadmap/objectives/obj-cep-platform.md` §8.1 R2(规范演进返工)
>
> **一手锚点**:
> - AMBA CHI 规范版本(per *Gen 族论文参考文献 [5]:`https://developer.arm.com/architectures/system-architectures/amba/amba-5`,accessed 2024-07-15)
> - CXL 2.0 / 3.0 / TileLink-C / NVLink / UALink 各自的规范版本号(见决策 2 的锁定表)

---

## 1. 背景

### 1.1 问题

CEP 的每个协议 adapter 都对应一个**外部规范**(CHI / CXL / TileLink-C / NVLink / UALink)。外部规范会演进:

- CXL 3.0 → 4.0(新增能力 / 改语义)
- UALink Common 2.0 → 2.x
- NVLink 代际更替(v4 → v5)
- AMBA CHI 版本推进

`objective-cep-platform §8.1 R2` 已登记该风险:**"规范演进返工"** 等级 🟡 中。

**未定义**的三件事:
1. CEP **锁定**哪个版本 —— 每个 adapter 对应哪个规范的哪一版
2. 规范升级时,CEP 的**兼容策略**是什么 —— 旧 adapter 继续可用?必须同步升?
3. 版本信息**存在哪里** —— `ADR-CEP-12` 的 schema 已有 `protocol_version` 字段,但其**取值来源**与**唯一真源**未定义

### 1.2 两条不同的"版本"必须区分

| 版本类型 | 含义 | 变更频率 | 变更后果 |
|---------|------|---------|---------|
| **规范版本**(spec version) | 外部协议规范自身的版本号(CHI r2 / CXL 3.0 / UALink 2.0) | 外部驱动,数月~数年 | **可能**破坏 adapter 语义 |
| **adapter 版本**(adapter version) | CEP 内该 adapter 实现的版本 | 内部驱动,按需 | 只影响 CEP 内部 |

**混用两者的典型错误**:把"CHI 从 r1 升到 r2"写成 adapter 版本变化,导致无法判断"升级是否必须改代码"。

---

## 2. 决策

### 决策 1:两者分离记录,`protocol_version` 只记**规范版本**

**决策**:`ADR-CEP-12` schema 的 `protocol_version` 字段**只承载规范版本**;adapter 实现版本由 git commit 天然承载,**不**进 schema。

**理由**:adapter 实现版本由版本控制系统管,再记一份就是双真源(正是 2026-10-21 反复出现的那类漂移)。

**字段格式**:`<spec_name>-<spec_version>`,例:
- `chi-r2`(ARM AMBA CHI,Revision 2)
- `cxl-3.0`
- `tilelink-c-1.3`
- `nvlink-4.0`
- `ualink-2.0`

### 决策 2:规范版本锁定表(**唯一真源**)

**决策**:本 ADR §3 的锁定表是 CEP 协议版本的**唯一真源**;`ADR-CEP-12` 的 schema 只做**引用校验**,不重复列版本号。

| 协议族 | 锁定规范版本 | 备注 |
|--------|-------------|------|
| **CHI** | **AMBA CHI r2**(`amba-5` 文档族) | RN-F 子集;仅需 request / snoop / response / data 4 通道语义(per `07 号 §5.1.1`) |
| **CXL 2.0 非对称** | **CXL 2.0** | Host 侧 Snoop Filter |
| **CXL 3.0 对称** | **CXL 3.0** | DCOH / BISnp / BIRsp;基于 DPA 归属 |
| **TileLink-C** | **TileLink-C 1.3** | Tree Tip 权限模型 |
| **NVLink** | **NVLink 4.0** | TL Read / Write / Atomic / Probe;NVSwitch SHARP |
| **UALink** | **UALink Common 2.0** | Station / Port / Lane;INC 原语 |
| **MOESI/GPU** | **MOESI/GPU 1.0** | ArchForge `09 号` §3 的 6 态 × 6 事件;snoop 型(SoC 内) |
| **MESIF** | **MESIF 1.0** | ArchForge `09 号` §3 的分级目录型(跨卡) |

> ⚠️ **本表与 §3 是同一内容的两种表述** —— 决策 2 表给**规范版本**(`AMBA CHI r2`),
> §3 表给 **`protocol_version` 取值**(`chi-r2`)。**§3 是唯一真源**(per `INV-CEP-11-IC2`),
> 但决策 3 的判据文字指向**本表** → **变更必须同步两表**,否则表外取值 = 装配期 fail。
> (2026-10-22 补记:加 `moesi-gpu-1.0` / `mesif-1.0` 时发现该双表述 —— 已同步,未改判据文字。)

> ⚠️ **本表的版本号是"本 ADR 起草时的选型",不是"外部规范的最新版本"** —— CEP 锁定的是**可实现的那一版**,而非追最新版。升级走决策 4 的流程。

### 决策 3:版本一致性由**装配期**校验,不靠人工核对

**决策**:每个协议实例的 `protocol_version` **必须**落在决策 2 的锁定表内;否则**装配期 fail**(与 `ADR-CEP-12 IC2` 的 `vc_id` 解析失败同款处置)。

**理由**:`ADR-CEP-12 IC2` 已确立"配置层错误 → 装配期 fail,不静默降级"的模式;版本校验是同一类问题,应复用同一处置。

**落点**:`scripts/test/cep_docs_sync_check.sh`(Stage 0,lint 层)+ 装配期检查器(Stage 1)。

### 决策 4:规范升级的处置路径(三条)

**决策**:规范版本升级**必须**走下列三条之一,按影响面选择:

| 路径 | 适用 | 动作 | KPI 影响 |
|------|------|------|----------|
| **A. 仅锁表更新** | 新版**未引入** adapter 关注的语义变化 | 只改本表 + schema 引用校验;**adapter 代码不动** | 0 slot(doc-only) |
| **B. 语义适配 + adapter 变更** | 新版引入新通道 / 新状态 / 新握手 | 新增 adapter change(按 `ADR-CEP-00 §2 Decision 3` 排队)+ 本表更新 | 1 slot |
| **C. 并行版本** | 需同时支持两版 | **当前不支持**(见决策 5) | — |

**决策登记**:任何路径 A/B 都**必须**在 `objective-cep-platform §11` 台账追加 `scope-change` 或 `adr-amendment` entry。

### 决策 5:显式排除

| 排除项 | 理由 |
|--------|------|
| **不**支持同一协议族的**多版本并存** | 每个协议实例 = 1 个 adapter = 1 个版本;多版本需 N 个 adapter,与"adapter 数 = 协议实例数"的口径(per `ADR-CEP-17` 决策 1)冲突 |
| **不**追外部规范的最新版本 | CEP 锁"可实现的那一版";追最新版会把外部变更的返工转嫁到 CEP |
| **不**把 adapter 实现版本记进 schema | 决策 1(避免双真源) |
| **不**允许 schema 出现锁定表之外的版本 | 决策 3 |

### 决策 6:与 `ADR-CEP-18`(协议生成路线)的一致性

**决策**:本 ADR **不**因 `ADR-CEP-18` 的长期演进位(SSP 描述 → 插件生成)而改变版本锁定策略 —— 未来若引入生成路线,**生成器同样锁定规范版本**,锁定表仍是唯一真源。

---

## 3. 规范版本锁定表(**唯一真源**)

| 协议族 | `protocol_version` 取值 | 关注的语义子集 | 锁定依据 |
|--------|----------------------|--------------|---------|
| CHI | `chi-r2` | RN-F 6 通道(TXREQ / RXRSP / RXDAT / RXSNP / TXRSP / TXDAT);Credit 流控;TxnID;CompAck | AMBA CHI `amba-5` 文档族 |
| CXL 2.0 非对称 | `cxl-2.0` | D2H / H2D 各 3 通道;Host Snoop Filter | CXL 2.0 |
| CXL 3.0 对称 | `cxl-3.0` | + BISnp / BIRsp;DCOH;DPA 归属 | CXL 3.0 |
| TileLink-C | `tilelink-c-1.3` | A/B/C/D/E 通道;Tree Tip 权限;Probe / Acquire / Grant | TileLink-C 1.3 |
| NVLink | `nvlink-4.0` | TL Read / Write / Atomic / Probe;NVSwitch SHARP / atomic / barrier | NVLink 4.0 |
| UALink | `ualink-2.0` | Station / Port / Lane;TL Load / Store / Atomic;INC 原语 | UALink Common 2.0 |
| MOESI/GPU | `moesi-gpu-1.0` | 6 态 × 6 事件;snoop 型(SoC 内);E→M 静默升级是跨域可见写判据的争点(per `ADR-CEP-13`;**该争点已裁定,见 §9 Status Update**) | ArchForge `09 号` §3 + `ADR-SOC-25` |
| MESIF | `mesif-1.0` | 分级目录型(跨卡);目录查询 / 定向 snoop | ArchForge `09 号` §3 + `ADR-SOC-25` |

> **表的使用规则**:
> 1. 本表是**唯一真源**;schema / 代码 / 文档**只引用**,不复制
> 2. 表中**不含**任何未列入 §5.1 的协议族
> 3. 变更本表 = 触发决策 4 的路径判定
> 4. **「协议族」此处按 `protocol_version` 行计**(本表 **8 行**)—— 与 `07 号 §5.1.1` 的「**6 族 / 8 实例**」是**两种粒度**(CXL 2.0 / 3.0 同族、`moesi_gpu` / `mesif` 同族,各族含 2 实例;per `07 号 §5.1.1` 族计数补记)。**本表不复制实例计数**

---

## 4. 不变量

- **INV-CEP-11-IC1**:每个协议实例的 `protocol_version` **必须**落在 §3 锁定表内;表外取值 = **装配期 fail**(不得静默降级)
- **INV-CEP-11-IC2**:§3 锁定表是规范版本的**唯一真源**;schema / 代码 / 文档**只引用不复制**
- **INV-CEP-11-IC3**:规范版本变更**必须**在 `objective-cep-platform §11` 台账留 `scope-change` / `adr-amendment` entry

> **编号位说明**:`INV-CEP-11-IC1` ~ `IC3` 共 **3** 条。

---

## 5. 抓取点

1. **版本表外取值**:配置中出现锁定表外的 `protocol_version` ⇒ 装配期 fail(per `INV-CEP-11-IC1`)
2. **双真源检测**:lint 检查 schema / ADR / 代码中**不得**重复列出规范版本号(per `INV-CEP-11-IC2`)
3. **变更留痕**:锁定表变更**必须**伴随台账 entry(per `INV-CEP-11-IC3`)
4. **adapter 版本不入 schema**:schema 中**不得**出现 adapter 实现版本字段

---

## 6. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-12`(能力映射表)| ✅ v0.1 | `protocol_version` 字段的**取值来源**与校验规则由本 ADR 定义 |
| `ADR-CEP-17`(协议族接入)| ✅ v0.3 | 协议族清单的**来源**;本表是其版本侧的对偶 |
| `ADR-CEP-18`(协议生成路线)| ✅ v0.1 | 决策 6:生成路线不改变版本锁定策略 |
| `ADR-CEP-07A`(06-bis 产物处置)| ✅ v0.1 | 若 06-bis 的 `MesiGpuProtocol` / `MesiThreeLevelProtocol` 被采纳,其规范版本锁定在此表登记 |
| `objective-cep-platform` §8.1 R2 | — | 本 ADR 是 R2 的**缓解措施**;R2 状态可由 🟡 中降为 🟢 低 |
| `scripts/test/cep_docs_sync_check.sh` | 🟡 Stage 0 | 决策 3 的 lint 落点 |

---

## 7. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 两版本分离 | `protocol_version` 只记规范版本;adapter 版本由 git 承载 | ✅ 已拍板 |
| 锁定表 | §3,8 个协议族的规范版本;**唯一真源** | ✅ 已拍板 |
| 装配期校验 | 表外取值 = fail,不静默降级 | ✅ 已拍板 |
| 升级路径 | A 仅锁表更新 / B 语义适配 + adapter 变更;C 并行版本**不支持** | ✅ 已拍板 |
| 显式排除 | 多版本并存 / 追最新版 / adapter 版本入 schema / 表外取值 | ✅ 已拍板 |
| 与生成路线一致 | 生成器同样锁版本,锁定表仍是唯一真源 | ✅ 已拍板 |

---

## 8. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 9. Status Update(2026-10-22,per 台账 Entry #13)

> **授权**:台账 `objective-cep-platform §11` **Entry #13** [adr-amendment]。
> **触发**:ArchForge 答复回执(2026-10-22)§1.1 **Q-3a = 是** —— 判据 (b) **成立**。
> **本节不改本表任何行** —— 按「表的使用规则 1:本表是唯一真源,**只引用**,不复制」,
> 判据 2 的**状态值**归 `ADR-CEP-05 §13.1`(宿主 ADR)所有,本节**只引用**。

### 9.1 `moesi-gpu-1.0` 条目的「争点」表述已失效

- **§3 表行原文**:`E→M 静默升级是跨域可见写判据的争点(per ADR-CEP-13)`
- **现状**:该**争点已裁定** —— 判据 (b) **成立** → `moesi_gpu` 的静默 `E→M` **是域可见写**。
- **状态值权威**:`ADR-CEP-05 §13.1`(判据 2 状态 = **已触发**)。
  **本表不复制状态值**,仅在表行内以「已裁定,见 §9」指引 —— 避免双真源(per Q-3c 立场)。

### 9.2 `protocol_version` **不升版** + 「表的使用规则 3」的适用性澄清

- **结论**:`moesi-gpu-1.0` **保持不变**(不引入 `moesi-gpu-1.1`)。
  依据:`ADR-CEP-11 决策 1` —— `protocol_version` **只承载规范版本**;
  `perm` 是 **CEP 侧** Crossbar Interface 投影词汇表(`ADR-CEP-02 CEN-Ib11`),**不是**协议规范的一部分;
  `moesi-gpu-1.0` 声明的协议语义(6 态 × 6 事件 / snoop 型)**完全未变**。
- **⚠️ 「表的使用规则 3」的适用性**:规则 3 原文「**变更本表 = 触发决策 4 的路径判定**」。
  本次**本表的行集 / 版本号 / 协议实例数 / 语义子集均零变更**(仅表内一格的**状态描述**更新)
  → **决策 4 的路径判定**经本节 §9.2 论证为**不适用**。
  - 决策 4 的三条路径是「**规范版本升级**的处置路径」;R-2(`perm` 形态变更)是 **CEP 侧 schema 演进**,
    **不构成规范版本升级** → 决策 4 **不适用**(既非 A 也非 B),其「决策登记」义务**不触发**。
  - 若强行套用路径 A 的**动作**判据(「adapter 代码不动」)则**不成立** ——
    `perm` 产出方**必须**能区分静默/显式升级 → adapter/plugin 侧**必然有代码变更**
    (现状:`perm` **仅在 schema 中定义,无任何 C++ 实现** → 该变更整体属 **Stage 1**)。
  - **净结论**:**可观测后果**与 ArchForge 的判定(路径 A)**完全一致**
    (版本号不动 / adapter 语义子集不动 / 不占 adapter change slot),差异**仅在标签与登记方式**。
  - 登记见台账 `Entry #13 ②`;R-2 的实现落 **Stage 1**(`S1-7`)。
- **ArchForge 侧判定**:其答复回执 §1.4 判为**路径 A**并请本仓确认。
  本仓**接受其实质结论**、**不采用「路径 A」标签** —— 已在本节与台账双向留痕,**不构成对对方的反驳**。

### 9.3 未变更项(明示)

- **§3 锁定表的行集 / 版本号 / 协议实例数(8 族 / 8 实例)** —— **零改动**
- **决策 1**(版本分离)/ **决策 2**(锁定表唯一真源)/ **决策 3**(装配期校验)/ **决策 4**(三条路径)/ **决策 5** —— **零改动**
- **`protocol_version` 字段格式** —— **零改动**
- 本 ADR 的其余全部正文 —— **零改动**

---

**版本**: v0.3 草案 + Status Update 2026-10-22
**最后更新**: 2026-10-22
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
