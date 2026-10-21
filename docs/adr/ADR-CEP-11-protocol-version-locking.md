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

> **表的使用规则**:
> 1. 本表是**唯一真源**;schema / 代码 / 文档**只引用**,不复制
> 2. 表中**不含**任何未列入 §5.1 的协议族
> 3. 变更本表 = 触发决策 4 的路径判定

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
| 锁定表 | §3,6 个协议族的规范版本;**唯一真源** | ✅ 已拍板 |
| 装配期校验 | 表外取值 = fail,不静默降级 | ✅ 已拍板 |
| 升级路径 | A 仅锁表更新 / B 语义适配 + adapter 变更;C 并行版本**不支持** | ✅ 已拍板 |
| 显式排除 | 多版本并存 / 追最新版 / adapter 版本入 schema / 表外取值 | ✅ 已拍板 |
| 与生成路线一致 | 生成器同样锁版本,锁定表仍是唯一真源 | ✅ 已拍板 |

---

## 8. 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | `objective-cep-platform §8.1 R2` + `ADR-CEP-12` 的 `protocol_version` 字段 | 版本分离 + 锁定表 + 校验 + 升级路径 |
| 决策 2 锁定表 | 各协议族规范的**可实现**版本选型(非"最新版") | 6 族版本号 + 关注语义子集 |

---

**版本**: v0.1 草案
**最后更新**: 2026-10-21
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
