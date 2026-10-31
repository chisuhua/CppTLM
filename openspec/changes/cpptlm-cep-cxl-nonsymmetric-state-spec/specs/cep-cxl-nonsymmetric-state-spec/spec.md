# cep-cxl-nonsymmetric-state-spec Specification

## Purpose
定义 **CXL 2.0 非对称**(CXL.cache 子协议 · host-centric / 单一一致性点)在 CEP 平台上的
**规约层可执行契约**:设备侧稳定态集合 / 事件词表与 **local–remote 划分** / 转换表 /
**`perm` 投影** / 逐格 `basis` 与**强度标注** / **非对称的结构级断言**。

**口径已裁决**(台账 `obj-cep-platform.md` **Entry #122** + `ADR-CEP-11` 的
`## Status Update(2026-11-01)`)⇒ 本能力**不重推**语义,只把裁决**机器化**。

> **边界**:不改 `perm` 4 值锁 · 不扩 `CATEGORY_*` 3 类 · 不改 `CohIfBundle` · 不改冻结 schema ·
> 不新增 ABI · 不新增 invariant 编号数 · **不产出任何性能结论** ·
> **不实现** adapter / 目录行为类 / 实例声明(属第 1 个 change `cpptlm-cep-cxl-nonsymmetric`)。
>
> **合规边界**:basis **只来自公开非规范来源**;⛔ CXL 规范正文**不得**投喂任何 AI
> (ECA §3 + AI Acceptable Use Policy)⇒ 只能靠规范正文确定的语义登记为 `pending`。
>
> **口径边界**:本能力 SHALL 锁定**结构**(态 / 事件划分 / 投影 / 断言 / `basis` 自洽),
> SHALL **不**锁定 opcode **编码** / 字段宽度 / credit 规则 / transient 态命名 / 错误语义
> —— 后者只能靠规范正文确定(本仓不可得)。

---

## ADDED Requirements

### Requirement: cep-cxl-nonsymmetric-state-spec

CEP SHALL 为 **CXL 2.0 非对称**(CXL.cache · host-centric)提供一份**可执行规约载体**:
4 态稳定态集合 · 事件词表与 local/remote 划分 · 转换表 · **4→4** `perm` 投影 ·
逐格 `basis` 与强度三档 · 偏离清单 · 编译期锁。

#### Scenario: CSS-01 —— 稳定态集合 SHALL **恰为 MESI 4 态**,且投影**恰为 4 行**

- **WHEN** 核对规约载体的稳定态集合与 `perm` 投影表
- **THEN** 子条件 ① SHALL 成立:稳定态集合 SHALL **恰为 4 个**(`M` / `E` / `S` / `I`)
- **AND** 子条件 ② SHALL 成立:`perm` 投影 SHALL **恰为 4 行**,
  且逐行 SHALL 为 `I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK`
  (⇒ 复用既有 MESIF 投影表的**前 4 行**)
- **AND** 子条件 ③ SHALL 成立:**`F` 态 SHALL NOT 存在**,其投影行 SHALL NOT 存在
  (⇒ 反向 fixture:把 `F` 行加回 ⇒ **必须因正确原因红**)
- **AND** 子条件 ④ SHALL 成立:秩口径 SHALL 只含「`E + Wr → M` 静默升级**严格升秩**」;
  MESIF 的 `F→E` **秩回落例外 SHALL NOT** 被移植(CXL 无 `F` 态)

> **语义**:「5→4」是 MESIF 模板形状的未审视继承 ⇒ 保留 `F` 行即**把模板形状当协议事实**。

#### Scenario: CSS-02 —— 事件集 SHALL 被划分为 local / remote,且**结构级**可判

- **WHEN** 核对规约载体的事件词表
- **THEN** 子条件 ① SHALL 成立:**remote** 事件集 SHALL **恰为** H2D snoop 操作码
  (`SnpInv` / `SnpData` / `SnpCurr`),其发起方 SHALL 恒为 Home
- **AND** 子条件 ② SHALL 成立:**local** 事件集 SHALL 为 D2H 侧设备操作
- **AND** 子条件 ③ SHALL 成立:**local ∩ remote = ∅**
- **AND** 子条件 ④ SHALL 成立:设备侧事件集中 SHALL **不存在** snoop **发起**型事件
- **AND** 子条件 ⑤ SHALL 成立:以上四条 SHALL 由**编译期断言**覆盖
  (⇒ 反向 fixture:毒化任一条 ⇒ **必须因正确原因红**)
- **AND** 子条件 ⑥ SHALL 成立:事件词表 SHALL 以**枚举名**表达,SHALL **不**写死 opcode 编码

> **语义**:本要求把「非对称」从**文档标签**变成**编译期事实** ⇒ 载体可被机器判「是否仍是非对称」。
> ⚠️ 精确表述 = **host-centric / 单一一致性点**;「设备互不感知其他设备」在 CXL 2.0 **平凡成立**
> (每 root port 只 1 个 cache device)⇒ SHALL **不**被当作 2.0 的区分性语义。

#### Scenario: CSS-03 —— 转换表逐格 `basis` SHALL 非空,且 `kind` / `pending` SHALL 一致

- **WHEN** 核对转换表每一格的 `basis{kind, pending, ref, note}`
- **THEN** 子条件 ① SHALL 成立:每格 `basis` SHALL **非空**(四字段齐备)
- **AND** 子条件 ② SHALL 成立:`kind` SHALL ∈ {`public-direct`, `public-analogical`,
  `archforge-anchor`, `self-derived`}(**封闭枚举**)
- **AND** 子条件 ③ SHALL 成立:`kind = self-derived` ⇒ `pending = true`;
  `kind = public-analogical` ⇒ `pending = true`;`kind = archforge-anchor` SHALL 仅用于
  **已逐字段对齐**的格(`ADR-CEP-04 §2` 的 caps 4 值)
- **AND** 子条件 ④ SHALL 成立:`ref` SHALL 指向**本仓锚点摘要的条目号**,
  SHALL **不**指向 CXL 规范原文
- **AND** 子条件 ⑤ SHALL 成立:**强度 SHALL NOT 升格** —— `public-analogical` / `inferred`
  SHALL NOT 被标记为 `public-direct`(⇒ 反向 fixture:升格 ⇒ **必须因正确原因红**)

> **语义**:`kind` 分档是**抗自欺**机制 ⇒ 「结构完备、语义空洞」必须可机器检出。
> ⚠️ 「全部逐格 pending + 跨仓追认」**不采用**:ArchForge 对 CXL 既无追认义务也无追认依据
> ⇒ 债**必然永久挂账**且被误读为「已尽责」。

#### Scenario: CSS-04 —— 载体 SHALL **零自有表副本**地供第 1 个 change 委托消费

- **WHEN** 核对规约载体的公开接口形态
- **THEN** 子条件 ① SHALL 成立:载体 SHALL 提供**常量表 + 查询函数**的稳定消费面
- **AND** 子条件 ② SHALL 成立:载体 SHALL **不**含任何 adapter / 目录行为实现
- **AND** 子条件 ③ SHALL 成立:载体头注释 SHALL 含「**≠ dGPU SoC 实现路径**」的消歧声明
  (⇒ 反向 fixture:删除该声明 ⇒ **必须因正确原因红**)

> **语义**:#9 的形态学教训 —— adapter 自带表 = 第二真源 ⇒ 规约改一格、adapter 不红。

#### Scenario: CSS-05 —— 残余风险 SHALL 逐项登记,且与锚点摘要**状态一致**

- **WHEN** 核对载体与锚点摘要中的残余风险登记
- **THEN** 子条件 ① SHALL 成立:以下 8 项 SHALL **逐项**登记 —— opcode 编码/字段宽度/credit 规则 ·
  request 权威枚举与逐状态合法性表 · transient 态命名 · 错误语义 · SF completeness 判定规则 ·
  版本记账 · 伪态/厂商扩展态的存在性 · HDM-DB/BISnp/BIRsp 细节
- **AND** 子条件 ② SHALL 成立:每项 SHALL 标注其**处置**(载体不建模 / 登记 / 队列 #12 / 人类成员条款摘要)
- **AND** 子条件 ③ SHALL 成立:载体与摘要的残余风险**项集 SHALL 一致**
  (⇒ 核对表生成器 `--check` ⇒ 缺项 / 多项 ⇒ **必须因正确原因红**)
- **AND** 子条件 ④ SHALL 成立:SHALL **不**声称任何**性能结论**
  (⇒ CXL 非对称 workload 落在 H7 已声明范围**之外** ⇒ 估算规则 R-B 失效区)

> **语义**:**「搜不到」≠「不存在」** ⇒ 未闭合项必须**登记为未闭合**,不得写成「已消除」。

#### Scenario: CSS-06 —— 锚点摘要 SHALL 为 basis 的**唯一人工审计面**,且可机器比对

- **WHEN** 运行核对表生成器的 `--check` 模式
- **THEN** 子条件 ① SHALL 成立:载体与摘要 SHALL 逐格**一致**(手改任一侧 ⇒ **必须因正确原因红**)
- **AND** 子条件 ② SHALL 成立:载体变更未重生成核对表 ⇒ **必须因正确原因红**
- **AND** 子条件 ③ SHALL 成立:摘要 SHALL 只含**条款级/文献级定位 + 断言摘要**,
  SHALL **不**整段复制任何来源原文
- **AND** 子条件 ④ SHALL 成立:每个 `public-direct` 断言 SHALL 有**可钉版定位**
  (文献版本 / DOI / 文档号 / 快照 URL + 抓取日期 / sha256);取不到者 SHALL 如实记「取不到 + 原因」
  ⇒ **禁止**编造钉版信息

> **语义**:锚点摘要 = **仓内**审计面 ⇒ 替代 #9 的**跨仓**逐格追认 ⇒ **无跨仓往返债**。
> ⚠️ **假钉版的危害 > 诚实的缺口** ⇒ 取不到必须如实登记。
