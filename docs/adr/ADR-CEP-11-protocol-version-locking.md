# ADR-CEP-11: 协议版本锁定与演进策略

> **状态**: ✅ **已签发**(v1.0,2026-10-30 —— 见 §10 Status Update)
> **日期**: 2026-10-21(起草)/ **2026-10-30**(签发)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 引用的外部协议规范版本锁定 + 版本漂移的处置路径**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §5.1(协议族 → 实例 → adapter)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
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
- `tilelink-c-1.7.1`
- `nvlink-4.0`
- `ualink-2.0`

### 决策 2:规范版本锁定表(**唯一真源**)

**决策**:本 ADR §3 的锁定表是 CEP 协议版本的**唯一真源**;`ADR-CEP-12` 的 schema 只做**引用校验**,不重复列版本号。

| 协议族 | 锁定规范版本 | 备注 |
|--------|-------------|------|
| **CHI** | **AMBA CHI r2**(`amba-5` 文档族) | RN-F 子集;仅需 request / snoop / response / data 4 通道语义(per `07 号 §5.1.1`) |
| **CXL 2.0 非对称** | **CXL 2.0** | Host 侧 Snoop Filter |
| **CXL 3.0 对称** | **CXL 3.0** | DCOH / BISnp / BIRsp;基于 DPA 归属 |
| **TileLink-C** | **TileLink-C 1.7.1**(SiFive) | Tree Tip 权限模型;**3 稳定态**(`Nothing`/`Branch`/`Tip`)+ **4 类权限转移**(`Cap`/`Grow`/`Prune`/`Report`) |
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

> **实现现状(2026-10-30 签发时实测,§10.2 逐条)**:**per-instance 硬钉**已落地 3 例
> (`chi_rnf` → `chi-r2` / `moesi_gpu` → `moesi-gpu-1.0` / `mesif` → `mesif-1.0`);
> **通用闸(任意实例的 `protocol_version` ∈ §3 锁表)** **尚未实现** ⇒ 本决策的
> 「**装配期 fail**」目前**只对已硬钉的 3 个实例**成立,**新增实例不受约束**。
> 补齐义务已登记(转 builder 实现,台账 Entry #109)⇒ **不得**表述为「IC1 已完全落地」。

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
| TileLink-C | `tilelink-c-1.7.1` | A/B/C/D/E 通道;Tree Tip 权限;Probe / Acquire / Grant;**3 稳定态**(`Nothing`/`Branch`/`Tip`,`Trunk` 为过渡态)+ **4 类权限转移**(`Cap`/`Grow`/`Prune`/`Report`) | TileLink-C 1.7.1(SiFive;PDF sha256 `30c09b5f…a2dd`) |
| NVLink | `nvlink-4.0` | TL Read / Write / Atomic / Probe;NVSwitch SHARP / atomic / barrier | NVLink 4.0 |
| UALink | `ualink-2.0` | Station / Port / Lane;TL Load / Store / Atomic;INC 原语 | UALink Common 2.0 |
| MOESI/GPU | `moesi-gpu-1.0` | 6 态 × 6 事件;snoop 型(SoC 内);E→M 静默升级是跨域可见写判据的争点(per `ADR-CEP-13`;**该争点已裁定,见 §9 Status Update**) | ArchForge `09 号` §3 + `ADR-SOC-25` |
| MESIF | `mesif-1.0` | 分级目录型(跨卡);目录查询 / 定向 snoop | ArchForge `09 号` §3 + `ADR-SOC-25` |

> ⚠️ **2026-11-01 增补块(per 台账 Entry #135 —— 原地更新留痕)**:本表的 **TileLink-C 行**
> (及决策 2 表 + 决策 1 字段格式示例中的同一取值)由 `tilelink-c-1.3` **原地更新**为
> `tilelink-c-1.7.1`(SiFive)⇒ 走**决策 4 路径 A**(仅锁表更新 · adapter 代码不动 · **0 KPI slot**)。
> **原地更新而非「只追加 Status Update」的理由**:本 ADR 决策 2 注释明写
> 「§3 是唯一真源 …… **变更必须同步两表**,否则表外取值 = 装配期 fail」
> ⇒ 只在文末追加而不同步表内 = **同一 ADR 内部双真源**。
> 依据 / 未变更项 / 后果 / 验证判据 ⇒ 见 **文末 `## Status Update(2026-11-01,per 台账 Entry #135)`**。

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

### 4.1 各不变量的**机器闸现状**(2026-10-30 签发时实测)

> ⚠️ 本表是**签发时的诚实标注** —— 「有 invariant」≠「有机器闸」。**不得**把本 ADR 读成
> 「三条 invariant 均已可执行」。缺口与补齐义务见 §10.2。

| invariant | 断言内容 | 机器闸 | 状态 |
|---|---|---|---|
| **IC1** | 实例的 `protocol_version` ∈ §3 锁表,表外 = fail | ① **锁表 ⊆ schema pattern**:有(`test_cep_schema.cc` 逐值 `regex_match` + 锁表解析**双向相等**)<br>② **实例声明侧 ∈ 锁表**:**仅 per-instance 硬钉 3 例**(`chi_rnf` / `moesi_gpu` / `mesif` 各钉一个具体值);**通用闸缺** | 🟡 **部分落地** |
| **IC2** | §3 锁表是**唯一真源**,schema / 代码 / 文档**只引用不复制** | **双向相等检查**已落地(锁表解析值集合 ↔ 期望集合);⚠️ 但期望集合在 **2 处测试**中各存一份 ⇒ 属**已登记的「双表述」**(同决策 2 表 vs §3 表的双表述),**须同批同步** | 🟡 **已落地 + 双表述已登记** |
| **IC3** | 规范版本变更**必须**在台账留 `scope-change` / `adr-amendment` entry | 台账机制(人工 + `obj` 台账口径);**无自动闸** | 🟡 **制度性** |

---

## 5. 抓取点

1. **版本表外取值**:配置中出现锁定表外的 `protocol_version` ⇒ 装配期 fail(per `INV-CEP-11-IC1`)
   —— ⚠️ **当前覆盖 = per-instance 硬钉 3 例**;**通用闸待补**(§4.1 / §10.2)
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

> §3 锁定表的行集 / 版本号 / 协议实例数(8 族 / 8 实例)· 决策 1~5 · `protocol_version` 字段格式 ·
> 本 ADR 其余全部正文 —— **全部零改动**。

---

## 10. Status Update(2026-10-30)—— **签发**(v1.0)

> **授权**:台账 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 **Entry #109** `[adr-amendment]`。
> **本节不改 §1 ~ §9 的任何正文**(除 §4.1 / §5 抓取点 1 的**实现状态标注**外)——
> 那两处是**签发时的如实标注**,不是决策变更;决策 1 ~ 6 与 §3 锁定表**零改动**。

### 10.1 签发依据(逐条核对)

| 决策 | 签发时的核对结果 |
|---|---|
| 决策 1(两版本分离) | ✅ `protocol_instance.schema.json` 的 `protocol_version` **description** 明写「adapter 实现版本由 git 承载,不入本 schema」⇒ 落地一致 |
| 决策 2(锁表唯一真源) | ✅ §3 锁表**8 行** = 声明侧现存 3 个实例(`chi-r2` / `moesi-gpu-1.0` / `mesif-1.0`)+ 5 个待接入族;**双表述**(决策 2 表 vs §3 表)已在决策 2 下登记 |
| 决策 3(装配期校验) | 🟡 **部分落地**(§4.1)—— 通用闸待补 |
| 决策 4(升级三路径) | ✅ 路径 A / B / C 的判定口径完备;**路径 C 显式不支持**的代价已在决策 5 登记 |
| 决策 5(显式排除) | ✅ 4 项排除与 §3 表的使用规则 1~4 一致 |
| 决策 6(与 `ADR-CEP-18` 一致) | ✅ 生成路线不改变锁定策略 |

### 10.2 签发时的**已知缺口**(不掩盖,转补齐义务)

| # | 缺口 | 影响 | 补齐义务 |
|---|---|---|---|
| **G-1** | **IC1 的通用闸缺失**:只有 per-instance 硬钉(3 例),**新增实例**写表外 `protocol_version` **无人报错** | 「表外取值 = fail」对新增实例**不成立** ⇒ R2(规范演进返工)在**新族接入**路径上**无闸** | **转 builder** 实现:通用断言(逐实例 `protocol_version` ∈ §3 锁表,**从锁表解析**而非硬编码)+ 反向 fixture(表外取值 ⇒ 必红) |
| **G-2** | **IC2 的期望值集合在 2 处测试各存一份** | 双处不同步 ⇒ 误判为回归 | 与 G-1 同批收敛为**单份解析**(锁表为唯一真源) |
| **G-3** | **IC3 无自动闸**(变更留痕靠人工) | 锁表变更可能漏登记 | 登记为**残余风险**;自动化列入 backlog(非本 ADR 签发的前置) |

> **签发 ≠ 全部落地**:本 ADR 的**决策面**完备且可作为后续实现与审稿的**判据依据**;
> 其 **3 条 invariant 的机器闸覆盖度**以 §4.1 为准 —— **不得**表述为「IC1 已完全落地」。

### 10.3 对 `R2`(规范演进返工)的影响

- **缓解路径确定**:R2 的缓解 = 本 ADR 的锁表 + 决策 4 的升级流程 ⇒ **「怎么锁、怎么升」已无歧义**。
- ⚠️ **但 R2 状态维持 🟡 中**:「锁表外的取值是否会被拦住」取决于 **G-1** 的通用闸 ⇒ 在 G-1 落地前,
  R2 的**残余**仍存在(新族接入时可能引入表外版本而无人报错)。
- ⇒ **R2 降级的条件** = G-1 通用闸落地 **且** G-2 收敛(登记见台账 Entry #109)。

### 10.4 关联

- 台账:`obj-cep-platform §11` **Entry #109**(`adr-amendment`)· #13(2026-10-22 的 `moesi-gpu-1.0` 不升版判定)
- 实现落点:`include/tlm/cep/protocol_instance.schema.json`(字段与 pattern)·
  `test/test_cep_schema.cc`(锁表 ⊆ pattern + 双向相等)· `test/test_cep_chi_rnf_subset.cc` /
  `test/test_cep_moesi_gpu_adapter.cc` / `test/test_cep_mesif_instance_decl.cc`(per-instance 硬钉 3 例)
- 待补(G-1 / G-2):通用闸 + 单份解析 ⇒ **转 builder**,台账 Entry #109 承接

---

## 11. Status Update(2026-10-30)—— **R2 降级落地**(per 台账 Entry #112)

> **授权**:台账 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 **Entry #112** `[adr-amendment]`。
> **本节不改 §1 ~ §10 的任何正文** —— §10.3 已写明的降级条件在本节**判定达成**并落地。

### 11.1 降级条件逐条核对

| 条件(§10.3) | 达成证据 | 判定 |
|---|---|---|
| **G-1 通用闸落地** | `test/test_cep_schema.cc` 的 `[cep][schema][ic1]`:逐实例查**从 ADR 解析**的锁表 + 可变异谓词 + 双向反向 fixture;**鉴别力实测**:临时毒化 `mesif.protocol_version = mesif-9.9` ⇒ 报红并归因到具体实例 ⇒ 还原 ⇒ 绿(台账 Entry #109 §③) | ✅ 达成 |
| **G-2 收敛** | 锁表取值集合原本在 **2 处**测试各存一份 ⇒ 收敛为「**1 处独立基准**(反向核对用 tripwire)+ 其余从 ADR 解析」;共享解析器 `test/cep_adr_parser.hh::parse_protocol_version_lock_table()` 为 test 内部唯一实现点(Entry #109 §②) | ✅ 达成 |

⇒ **R2(规范演进返工)由 🟡 中降为 🟢 低**。

### 11.2 降级的语义边界(**不得**越界表述)

- 被验证的是**机制存在且会拦截**(新族接入时表外 `protocol_version` 必被通用闸拦下),
  **不是**「外部规范演进不会造成返工」这一**结论**。
- **R2 降级不蕴含 R7 变化** —— R7(探索平台结论不可信 · 无对拍验证)的降级条件是
  **首轮对拍 P-1 ~ P-5 达标**(per `docs/architecture/cep/parity-baseline.md`),
  该实测**尚未发生** ⇒ **R7 维持 🟡 中**。
  ⚠️ 两条风险相邻且都挂在「外部规范 / 外部基准」上 ⇒ 后人**不得**把 R2 的 🟢 读成 R7 也已闭合。
- **G-3(IC3 变更留痕无自动闸)不因本次降级而关闭** —— 仍为**残余风险**(见 §10.2),自动化列 backlog。

### 11.3 未变更项(明示)

§1 ~ §9 正文 · §3 锁定表 8 行 · 决策 1 ~ 6 · 3 条 invariant 的**定义** · §4.1 机器闸现状表的
**闸状态**(IC1 仍是「🟡 部分落地」—— 落地的是**声明侧通用闸**,§5 抓取点 1 的**装配期运行时检查器**
仍属 Stage 1 待办)—— **全部零改动**。

---

## Status Update(2026-11-01,per 台账 Entry #122)—— 队列 #10 `cxl-2.0` 的**语义子集边界**与 **basis 权威**裁决

> **正文不改**(ADR 不可变;§3 锁定表 8 行 · 决策 1 ~ 6 · 3 条 invariant 的**定义** · §11 全部保留)。
> 本条登记**队列 #10 规约层起手前**对「`cxl-2.0` 锁的是什么 / basis 权威是谁」的裁决。
> ⚠️ **§3 锁定表零改动** —— 决策 4(规范升级的三条路径)**不触发** ⇒ 改锁表反而多付一次冻结面授权。

### 触发

队列 #10(`cpptlm-cep-cxl-nonsymmetric`)第 0 个 change(规约层)起手前,**oracle 架构审查 + metis gap 分析 +
公开来源调研**三方结论一致指出:草稿的「5→4 `perm` 投影表」与「全部逐格 `pending` + 跨仓追认」两项
**均无可执行依据** ⇒ 必须先裁决语义子集边界与 basis 权威,否则规约层必然「把模板形状当协议事实」。

### 结论

| # | 裁决 | 理由(实测依据) |
|---|---|---|
| **1** | 「CXL 2.0 非对称」= **CXL.cache 子协议**(**host-centric / 单一一致性点**):设备以 **MESI** 缓存 Host 内存;Host 侧 Home / Snoop Filter 为**唯一**一致性点;**H2D 承载 snoop**;设备侧**无** snoop **发起**型事件。**显式不含** CXL.mem 池化语义与 CXL 3.0 DCOH / BISnp / BIRsp | 三处已锁口径互证(`07 号 §5.1.1` CXL 行 · 本 ADR §3 `cxl-2.0` 行的「D2H/H2D 各 3 通道;Host Snoop Filter」· `ADR-CEP-04 §2` 的「CXL 2.0 Host SF」行) |
| **2** | 设备侧稳定态 = **MESI 4 态**(M/E/S/I)⇒ `perm` 投影 = **4→4**(`I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK`)。⚠️ **「5→4」作废** —— `F` 态是 MESIF 特有,CXL **无 `F` 态** ⇒ 该投影行**不存在** | 同行评审公开形式化论文 + 公开会议 deck(CXL.cache「four stable states」);`RdOwn` 取所有权(E **或** M) |
| **3** | 「非对称」的精确表述 = **host-centric / 单一一致性点**。⚠️ **「设备互不感知其他设备」在 CXL 2.0 是平凡成立**(每 root port **只有 1 个** cache device;3.0 才最多 16 个)⇒ 不得把它当作 CXL 2.0 的**区分性**语义 | 公开 CXL 3.0 tutorial 的 root-port 拓扑口径 |
| **4** | **basis 权威 = 本仓钉版的「公开来源锚点摘要」** —— **既非 CXL 规范正文、亦非 ArchForge**。强度三档 `direct` / `analogical` / `inferred` **不得升格**;**追认 = 条目映射的二方复核,仓内闭环**(⇒ **无**跨仓往返债) | ArchForge 对 CXL **既无追认义务也无追认依据**(它**明确立法**不做 CXL 栈)⇒ 「全部逐格 pending + 跨仓追认」在 CXL 语境下**必然永久挂账**;而 CXL 2.0 是**公开标准** ⇒ basis 可引**公开来源** |
| **5** | ⛔ **合规禁令**:CXL 规范受 **Evaluation Copy Agreement §3** + **AI Acceptable Use Policy**(覆盖 past / current / future 规范)+ **NOTICE TO NON-MEMBERS** 约束 ⇒ **禁止**把规范正文投喂任何 AI。**只能靠规范正文确定的 8 项语义** 走「**人类成员条款摘要**」流程 | 合规条款原文(2026-02-12 采纳版) |
| **6** | CXL 实例 = **CEP 探索实例**,显式声明「**≠ dGPU SoC 实现路径**」;**不镜像**进 ArchForge `32 号 §6.1`(那是**产品侧**实例注册表 ⇒ 镜像 = 污染 + 与其「GPU Die 无 CXL」立法冲突)⇒ 跨仓至多一条**定位注记** | ArchForge `ADR-SOC-21` D2 + `21-fabric-switch-mvp.md` 的「GPU Die 内不存在 CXL 协议状态机」 |
| **7** | ⚠️ **「CXL 3.0 对称」指的是 HDM-DB / CXL.mem 的多 host 共享内存**,**不是** CXL.cache 变对称 ⇒ **CXL.cache 全代次都非对称** | 公开 CXL 3.0 资料把 BI 归在 **Memory Protocol**;同行评审论文明示 CXL.cache 始终 asymmetric |

### 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR 自身** | **零改动** —— §3 锁定表 / 决策 / invariant 定义全部保留;决策 4 **不触发** |
| **队列 #10 规约层** | 载体形态从 #9 的「ArchForge 锚 + 跨仓追认」改为「**公开来源锚点摘要 + 仓内复核**」⇒ `has_archforge_anchor()` 一类接口**不适用** ⇒ 追认清单生成器的**对象**改为「本仓摘要的逐格核对表」 |
| **跨仓债** | **由 #9 式 35 格异步跨仓往返 ⇒ 归零** ⇒ 规约层**周期显著缩短**(估时 12 周不变但重分配:规约层 4 周 = **1 周公开来源普查硬门** + 3 周载体) |
| **残余风险(不得写成「已消除」)** | ① **规范正文不可得** ⇒ 8 项语义只能靠人类成员条款摘要(开放项,非本 ADR 能闭合)② **公开来源的强度天花板** —— 「SF 宁多探不漏探」的判定规则无公开 CXL 原文(只能从 CHI / CCI **类比**)③ **公开来源的形式化模型假设 host perfect tracking** ⇒ 不得当作 `SparseSnoopFilter`(`CONSERVATIVE`)的**精度**依据,只可作**协议骨架** ④ 「搜不到」**≠**「不存在」(是否存在 P / pseudo / 厂商扩展态不可判定) |
| **对 R7 / H7** | **无影响** —— 本条只裁协议语义子集,不产生性能结论;⚠️ CXL 非对称的 workload **结构性非均匀** ⇒ 落在 H7 已声明范围(均匀随机)**之外** ⇒ 本队列**禁止**附着任何延迟 / 性能读数 |

### Verification(前瞻判据 · 可勾选)

- [ ] **V-1** 队列 #10 规约层载体的 `basis.kind` 值域 = {`public-direct`, `public-analogical`, `archforge-anchor`, `self-derived`},且 **`archforge-anchor` 仅用于 caps 等已逐字段对齐的格**(`ADR-CEP-04 §2` 的 4 值)—— 机器可校
- [ ] **V-2** 载体的态集合**恰为 4 态**且 `perm` 投影**恰为 4 行**;`F` 行**不存在** —— 编译期锁 + 反向 fixture(把 `F` 行加回 ⇒ 必红)
- [ ] **V-3** 编译期结构断言成立:`H2DReqOp ⊆ {SnpInv, SnpData, SnpCurr}` ∧ `D2HReqOp ∩ H2DReqOp = ∅` ∧ 设备侧无 snoop **发起**型事件 —— 毒化任一条 ⇒ 必红
- [ ] **V-4** 锚点摘要的每条 `direct` 断言均有**可钉版定位**(arXiv 版本 + DOI / PDF sha256 / 文档号 / 博客抓取日期);`analogical` / `inferred` **未**被升格 —— `--check` 机器比对载体 ↔ 摘要 ⇒ 手改任一侧必红
- [ ] **V-5** 载体的**头注释**含「**≠ dGPU SoC 实现路径**」消歧声明;且 ArchForge 侧**未**新增 CXL 实例声明(仅允许一条定位注记)
- [ ] **V-6** §3 锁定表**零 diff**(本 Status Update 之外的冻结面改动**仅限**本段与清单 hash 刷新)

**关联**: 台账 `obj-cep-platform.md` **Entry #122**(口径裁决 + 授权记录 + 8 项残余风险 + 草稿修订清单)
· `ADR-CEP-04 §2 / §4`(caps + hazard 契约)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)
· `07 号 §5.1.1`(协议族表)· `docs/architecture/cep/parity-baseline.md`(H7 的范围限定与 R-B 失效区)

---

## Status Update(2026-11-01,per 台账 Entry #135)—— 队列 #11 `cpptlm-cep-tilelink-tree-manager` 的 **版本锁定**裁决(`tilelink-c-1.3` → `tilelink-c-1.7.1`)

> **授权**:台账 `objective-cep-platform §11` **Entry #135** [adr-amendment] —— **冻结面授权记录**。
> **形态**:**决策 4 路径 A**(「仅锁表更新」—— 新版**未引入** adapter 关注的语义变化 ⇒ adapter 代码不动)⇒ **0 KPI slot(doc-only)**。
> **改动面**:决策 1 字段格式示例 + 决策 2 表 + §3 锁定表(**3 处同步**) + §3 表后增补块 + 本段。

### 触发

队列 #11 的 **Task 0 硬门**(公开来源普查)结论 = **有条件通过**,但**唯一阻塞点**是版本锁定:
`ADR-CEP-11 §3` 原锁 `tilelink-c-1.3`,而**该版本正文本环境取不到**(≥6 次尝试:riscv.org 404 ·
RISC-V adoc 不存在 · `openspecification/tilespec` 仓库 404 · GitHub 搜索无命中)⇒ **不可确证**;
且 RISC-V「1.3」与 SiFive 版本线的**谱系关系未证实** ⇒ 该**标识符本身可能指错版本线**。
⇒ 而可取的 **1.7.1** / **1.9.3** 属 **SiFive 版本线**,且二者**语义已漂移**(事件词表 10 → 12 条;
节点类别 4 → 5 类)⇒ **不得拿它们冒充 1.3**。

### 结论

| # | 裁决 | 理由(实测依据) |
|---|---|---|
| **1** | TileLink-C 的锁定版本 = **`tilelink-c-1.7.1`**(发布方 **SiFive**)⇒ 走**决策 4 路径 A** | 本环境 `curl` 实取 PDF(735 145 B)+ sha256 `30c09b5f…a2dd` ⇒ `pin=strong`;**决策 4 路径 A 的适用条件成立**(adapter 尚未建 ⇒ 无代码要改) |
| **2** | ⚠️ **1.7.1 不是最新版本**(1.9.3 是)⇒ 本裁决**恰好落在决策 5 的允许范围内** | 决策 5 明写「**不**追外部规范的**最新**版本 \| CEP 锁"**可实现的那一版**"」⇒ 理由**不得**写成「选最新」 |
| **3** | §3 表的「关注的语义子集」列**同步补齐**为「**3 稳定态**(`Nothing`/`Branch`/`Tip`)+ **4 类权限转移**(`Cap`/`Grow`/`Prune`/`Report`)」 | 载体形态已由锚点摘要推导完成(3 态 × 4 事件 = 12 格,**零 `Unknown`**);锁表的语义子集列**就是**给 adapter 实现看的 |
| **4** | 「锁定依据」列**写明发布方 + PDF sha256** | 闭合锚点摘要 **R-T8**(版本线歧义):新取值本身唯一指向 SiFive 1.7.1;再写发布方 + 字节指纹 ⇒ **同类歧义不再复发** |
| **5** | `Trunk` **不进**稳定态集合(过渡态) | §8.1.4 转移记号全集 = **N/B/T** ⇒ **无任何 `Trunk` 参与的转移**;⚠️ 但该表表头写 `Permissions: None, Branch, Trunk` ⇒ **表头疑为笔误** ⇒ **以转移记号为准**并登记矛盾(锚点摘要 R-T5) |
| **6** | **未选 1.9.3** | `Tip` 拆为 `Tip(no Branches)`/`Tip(with Branches)` ⇒ 稳定态膨胀到 4~5 值;且 1.9.3 自身的 `Trunk` 矛盾(正文说无读/写权限而同份 `Table 29` 给 `Get`)**落在 `perm` 投影上** ⇒ 载体风险更高 |

### 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR 自身** | 决策 1 / 3 / 4 / 5 / 6 **零改动**;invariant 集合**零增量**(**新增 invariant 数 = 0**);§3 表**行集不变**(仍 **8 行**) |
| **schema** | `protocol_instance.schema.json` **零改动** —— pattern `^[a-z0-9]+(-[a-z0-9.]+)*$` **已容纳** `tilelink-c-1.7.1` |
| **实例声明** | `protocol_instances.json` **零改动** —— **无任何实例**声明 `tilelink-c-*`(队列 #11 的实例尚未建) |
| **非冻结面连带** | `test/test_cep_schema.cc` 的锁表取值列表 **+1 处**同步(该测试断言「schema 值域 ⊇ §3 锁表全部取值」) |
| **队列 #11 规约层** | **阻塞解除** —— 载体形态已可推导(3 态 × 4 事件 = 12 格,零 `Unknown`)⇒ 可起 change 目录;⚠️ **另有两项 architect 裁决未闭合**:`ADR-CEP-09`(通道 5 → VC 4 的 R-T2)与 `ADR-CEP-12`(6 类消息类词表的 R-T3)—— **本条不动这两份 ADR** |
| **残余风险(不得写成「已消除」)** | ① **1.3 的内容永久不可确证** —— 本裁决**放弃**该版本,不追补 ② **两版各有内部表述不一致**(D1「six subsets」vs 4 类;D1 表头 `Trunk` vs 记号 `T`=Tip;D2 正文与 `Table 29` 矛盾)⇒ 已登记 R-T5,**以转移记号为准** ③ **`Trunk` 是否本意为稳定态** —— 若规范本意含之 ⇒ 态数变 4 ⇒ **须以转移记号为准**并登记矛盾 |

### Verification(前瞻判据 · 可勾选)

- [ ] **V-1** §3 锁定表 **8 行不变**,且 TileLink-C 行的 `protocol_version` 取值 = **`tilelink-c-1.7.1`**;决策 2 表与决策 1 示例**同值** ⇒ **三处同步**(机器可校:三处取值相等)
- [ ] **V-2** `test/test_cep_schema.cc` 的锁表取值列表**含** `tilelink-c-1.7.1` 且 schema pattern **接受**它 ⇒ 既有断言持续绿
- [ ] **V-3** `protocol_instances.json` 的**任一**实例的 `protocol_version` ∈ §3 锁表(**通用闸**,per 决策 3;⚠️ 截至本条该闸**仍未通用实现** —— 见决策 3 的「实现现状」段,补齐义务在台账 Entry #109)—— **本条不关闭该义务**
- [ ] **V-4** `ADR-CEP-09` / `ADR-CEP-12` **零 diff**(本条只动本 ADR);`ADR-CEP-11` 的**其余**段落**零 diff**(冻结面改动**仅限**锁表 3 处 + 增补块 + 本段)
- [ ] **V-5** 锚点摘要 `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md` 的 **D1 行**(sha256 `30c09b5f…a2dd`)与 §3 表「锁定依据」列的指纹**一致**

**关联**: 台账 `obj-cep-platform.md` **Entry #135**(授权记录 + 逐条依据)· **#134**(队列 #11 Task 0 硬门 + ④bis 二次普查)
· `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(§8 版本建议 / §9 钉版 / §7 R-T1·R-T5·R-T8)
· `ADR-CEP-09`(通道 5 → VC 4,裁决未闭合)· `ADR-CEP-12`(6 类消息类词表,裁决未闭合)

## Status Update 2026-11-11(per 台账 Entry #341)—— **对拍基准规格的路径迁移**(顶层 `14-…` → `cep/parity-baseline.md`)

> **性质**:末尾追加 SU(append-only)。⛔ **既有正文与既有 `## Status Update` 段逐字未变**;⛔ **新增 invariant 数 = 0**;⛔ **不改 §3 锁表任何取值**。
> **背景**:per 用户裁决「**A · 仅 14 号移位 + 2 处标注修正**」(台账 **Entry #341**)⇒ `docs/architecture/14-coherence-parity-baseline.md` 归位至 `docs/architecture/cep/parity-baseline.md`(**去数字前缀**,per 治理裁定表 §六 硬规则 ②「`cep/` 无数字前缀,避与顶层 01~14 撞车」)⇒ `docs/architecture/` 顶层**只剩 `07 号` 1 份 CEP 文档**(总纲)+ `cep/` 9 篇子文档。
> **本 ADR 两处引用的处置**:`:318` 与 `:376` 的路径引用已按 ADR 不可变的**第二种合法形态**——「**正文就地澄清(零行数变化)**」(per `ADR-CEP-00A` 的跨仓锚点治理不变量:只改既有行的文字、⛔ 不插入内容 ⇒ **既有行号零位移** ⇒ 跨仓机器锚点不受影响)——就地改为新路径。
> ⛔ **决策内容零改动**:§3 锁表 8 行、`protocol_version` 取值、判定口径**全部未变**;本段只登记**路径事实订正**。

### 新路径(唯一)

| 旧路径 | **新路径** |
|---|---|
| `docs/architecture/14-coherence-parity-baseline.md` | **`docs/architecture/cep/parity-baseline.md`** |

> ⚠️ **该规格文档的内容零改动**(§2 三选项对比 / §3 裁决 / §4 对拍手段 P-1 ~ P-5 / §7 成本分期 **全部原样**)⇒ 本 ADR 的 **H7 口径**与 **R-B 失效区**判定**不变**。

### Verification(可勾选 · 前瞻)

- [x] 本 ADR 的既有 **决策正文与既有 SU 段逐字未变**;唯二例外 = `:318` / `:376` 的**路径就地澄清**(零行数变化,per 上文处置口径)
- [x] 对拍基准规格**内容零改动** ⇒ H7 口径 / R-B 失效区判定不变
- [x] 新路径已存在于仓内(`docs/architecture/cep/parity-baseline.md`),并被 `cep_freeze_check.sh` 的 glob `docs/architecture/cep/*.md` 纳入(冻结产物 **77 → 78**)

**关联**:台账 **Entry #341**(授权 + 范围 + 连带修正清单)· **#340**(下一阶段任务梳理与实施路径)· `docs/architecture/cep/parity-baseline.md`(H7 对拍基准规格)· `docs/architecture/07-coherence-exploration-platform.md`(总纲 —— §7 实验输出协议是**运行面**,对拍基准是**手段面**,两者互补不重叠)· 治理裁定表 §六(硬规则 ②)
