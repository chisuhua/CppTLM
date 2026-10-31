# Design — cpptlm-cep-cxl-nonsymmetric-state-spec v1.0

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#10** 第 0 个 change
> **配套**: [`proposal.md`](proposal.md) · [`specs/cep-cxl-nonsymmetric-state-spec/spec.md`](specs/cep-cxl-nonsymmetric-state-spec/spec.md) · [`tasks.md`](tasks.md)
> **口径**: 台账 **Entry #122** + `ADR-CEP-11` Status Update(2026-11-01)· **普查**: `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`

---

## D-α 态集合 = **MESI 4 态**,投影 = **4→4**(删 `F` 行)

**问题**:improvement 草稿原写「**5→4** `perm` 投影表」⇒ 这是 **MESIF 模板形状的未审视继承**
(MESIF = 5 态 `I/S/E/M/F`)。全仓已锁载体**从未给 CXL 钉过态数**。

**证据**:同行评审公开形式化论文(CXL.cache「four stable states: modified, exclusive, shared, invalid」)
+ 公开会议 deck ⇒ **CXL.cache 设备侧稳定态 = M/E/S/I** ⇒ 投影 = **4→4**
(`I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK` = **复用现有 MESIF 表前 4 行**)。

**决策**:载体**恰 4 态**;`F→WRITEBACK` 行**整行删除**;MESIF 的 **`F→E` 秩回落例外同样不可移植**
(CXL 无 `F` 态)⇒ CXL 侧保留的静默升级只有「**`E + Wr → M`** 严格升秩」(⇒ 须落 `perm_upgrades.json`,属第 1 个 change)。

**代价**:与 MESIF 载体的**形态不再完全同构**(4 vs 5 态)⇒ 编译期锁的**条数由内容决定**,不追求与 #9 的 14 条对齐。

**落点**:`cxl_state_spec.hh` 的 `kStates` / `kPermProjection`;判据 AC-1。

---

## D-β **非对称**的结构级可判性(local / remote 事件集划分)

**问题**:「非对称」若只作**标签**,则载体可被写成任何东西 ⇒ 判据无鉴别力。

**证据**:公开来源明确「**H2D Requests are snoops from the host**」+「D2H Requests from the device」
⇒ **H2D Req 只含 snoop 操作码**(`SnpInv` / `SnpData` / `SnpCurr`),**发起方恒为 Home**
⇒ **D2H Req = 设备(local)** ⇒ 两集合**不相交**。

**决策**:载体把事件集显式二分(**结构级**,非行为实现):

```
local  = D2H 侧设备操作(Req/Rsp/Data 中的设备发起部分)
remote = 仅 H2D snoop 操作码(发起方恒为 Home)
```

⇒ **3 条编译期结构断言**(判据 AC-2):
1. `H2DReqOp ⊆ { SnpInv, SnpData, SnpCurr }`
2. `D2HReqOp ∩ H2DReqOp = ∅`
3. 设备侧事件集中**不存在** snoop **发起**型事件

**理由**:这 3 条把「Host 单一一致性点 / 设备不发起 snoop」从**文档断言**变成**编译期事实** ⇒
「非对称」不再是标签。⚠️ 精确表述 = **host-centric**;「设备互不感知」在 CXL 2.0 **平凡成立**(每 root port 只 1 个 cache device)
⇒ 不得当作 2.0 的区分性语义(per Entry #122 ①③)。

**代价**:事件词表**必须**枚举(local 侧 **14** 条 D2H Req + remote 侧 **3** 条 snoop —— 与摘要 §2 的枚举逐项一致)
⇒ 公开来源的 opcode 清单在**不同年代的 deck 间有差异**(`MemWr`↔`WrCur`)⇒ ⇒ 载体用**枚举名**,
**不写死编码**,且该差异登记为 `R-2`。

**落点**:`cxl_state_spec.hh` 的 `kEvents` / `is_remote_event()` + 3 条 `static_assert`;判据 AC-2。

---

## D-γ `basis.kind` 四档 + `pending` 策略**随 kind 分布定**

**问题**:草稿原写「**全部**逐格 `pending = true` + 跨仓追认债」⇒ ArchForge **明确立法不做 CXL 栈**
⇒ 它对 CXL **既无追认义务也无追认依据** ⇒ 该形态**必然永久挂账**,且挂账状态会被误读为「已尽责」。

**决策**:`basis.kind` 四档,**先定值域再逐格填**:

| `kind` | 含义 | `pending` 规则 |
|---|---|---|
| `public-direct` | 公开来源**明确这样写** | 可 `false`,**但须同时满足「`pin=strong` ∧ 许可已人工确认」** |
| `public-analogical` | 跨协议 / 跨代次**类比** | **`true`**(强度天花板) |
| `archforge-anchor` | **仅** caps 等**已逐字段对齐**的格(`ADR-CEP-04 §2` 的 4 值) | 可 `false` |
| `self-derived` | **本仓推导** | **必须 `true`** |

⚠️ **两轴不得混读**(2026-11-01 裁决,per oracle 复核 + critic 第 1 轮 SHOULD_FIX #1):

| `pin` | `kind` | `pending` |
|---|---|---|
| `strong`(字节已取 + sha256) | 可 `public-direct` | 可 `false`(另须许可确认) |
| `weak`(仅 URL + 抓取日) | **可保留** `public-direct` | **`true`** |
| `transcribed`(原文本环境未读到) | **降** `public-analogical` | **`true`** |

⇒ **理由**:「证据强度」(来源措辞)与「钉版强度」(字节可否本地复核)是**两个轴** ——
Cloudflare 403 不会把一条直接引语变成「类比」;而**转述**支持「大意存在」但不支持「措辞如此」
⇒ 后者「明确这样写」这一谓词**不可判定**,故须降档。细则见锚点摘要 §8.2。

⇒ **禁止**把 `analogical` / `inferred` 升格为 `public-direct`(判据 AC-4)。

**理由**:CXL 是**公开标准** ⇒ basis 可引公开来源 ⇒ **追认 = 条目映射的二方复核,仓内闭环**
⇒ **无跨仓往返** ⇒ 周期更短、风险更小。

**代价**:**放弃** #9 的「35 格跨仓逐格追认」这道**外部审计** ⇒ 补偿手段 =
① 锚点摘要作为**唯一人工审计面** ② `--check` 机器比对载体 ↔ 摘要 ③ 二方复核翻 `pending`
④ **残余风险登记**(8 项)。

**落点**:`cxl_state_spec.hh` 的 `BasisKind` / `basis_entry_is_consistent()`;判据 AC-3 / AC-4。

---

## D-γbis 转换表 = **三态格语义**(不是二态「合法/未知」)

**问题**:Phase 2b 的转换矩阵需要「**逐状态合法性表**」(某事件在某态下是否合法)⇒
该表**公开来源不可得**(残余 **R-2**,且不同年代 deck 已有 `MemWr`↔`WrCur` 差异)
⇒ 4 态 × 17 事件 = **68 格**中大量格的合法性**无法确证**。

**已排除的二态方案(与 `mesif` 先例同形)**:`0xFF = 未定义(空洞)` ⇒ 消费者把「未知」读成「非法」
⇒ ⇒ 这**本身是一处新的未实测断言**(「我断言此转换非法」≠「我不知道」)。

**决策**:每格 SHALL ∈ **三态** ——

| 格语义 | 含义 | 承载 |
|---|---|---|
| `Known(target, basis)` | 已确证的目标态 | `basis` 四字段 |
| `Illegal(basis)` | 已确证**不合法**(须有证据,不能靠「未找到」) | `basis` 四字段 |
| `Unknown(pending → R-2)` | **不知道**(不是非法) | 显式 `Unknown` 态,`transition()` 返回它 |

⇒ `transition()` 对 `Unknown` SHALL 返回**显式 unknown 态**,SHALL NOT 返回目标态、SHALL NOT abort。

**反秃头判据(coverage,判据 AC-6)**:载体 SHALL 声明
`kLegalTransitionCoverageNum` / `kLegalTransitionCoverageDen`(已确证格数 / 68),测试 SHALL 断言
该比值与**载体实际逐格统计相等**(防虚报),台账 SHALL 在每次推进时复核**单调不减**(回退必红 + 须授权)。

**理由**:规约层的鉴别力在**结构层**(4 态 / 投影 / 划分 / 断言,已编译期锁定),不在转换表填满度;
「**半空但诚实**」严格优于「填满但杜撰」(#9 的 35 格债即前车)。

**代价**:消费者须处理 `Unknown` 分支 ⇒ 第 1 个 change 的 adapter SHALL 在 `Unknown` 路径**显式降级**
(保守行为 + 事件登记),SHALL NOT 静默当作 `Illegal`。

**落点**:`cxl_state_spec.hh` 的转换表 + `kLegalTransitionCoverage*`;判据 AC-6。

---

## D-δ 追认清单 ⇒ **锚点摘要逐格核对表**(对象变更)

**决策**:`generate_cxl_anchor_checklist.sh` 的产出 = 「**载体 ↔ 锚点摘要**的逐格核对表」,
**不是**「发给外部的追认请求件」;`--check` 比对两侧 ⇒ 手改任一侧必红。
⇒ 锚点摘要落 `docs/research/`(框架级调研留在本仓,per 真源分级)⇒ **不整段复制任何原文**(IP 风险)。

**落点**:`scripts/test/generate_cxl_anchor_checklist.sh` + 摘要文件;判据 AC-3。

---

## D-⑤ ⛔ 合规禁令:规范正文**不得**投喂 AI

**问题**:CXL 规范受 **Evaluation Copy Agreement §3** + **AI Acceptable Use Policy**(2026-02-12,覆盖 past/current/future)
+ **NOTICE TO NON-MEMBERS** 约束 ⇒ 非会员用 AI 处理规范正文 = 明文禁止。

**决策**:本 change 的 basis **只来自公开非规范来源**;**8 项**只能靠规范正文的语义(`R-1~R-8`)
⇒ 登记为 `pending` + 走「**人类成员条款摘要**」流程(人类读规范写摘要,**AI 不接触正文**)。

**理由**:oracle 原设计的「引钉版 spec 条款」**前提未实测** ⇒ 在 AI-agent 语境下**不可执行**。

**代价**:细粒度语义(编码 / 逐状态合法性表 / transient 命名 / 错误语义)在本 change 内**不可闭合**
⇒ 载体**只建模稳定态**、用**枚举名**、逐格标 `pending`。

**落点**:摘要 §0 合规声明 + §6 残余风险;判据 AC-6。

---

## D-⑥ 载体**零自有表副本**的消费方向预留(为第 1 个 change)

**决策**:本 change **只**交付规约载体;第 1 个 change 的 `CxlCacheAdapter` SHALL **零自有表副本**
(委托消费本载体,与 `MesifAdapter` 同款形态)⇒ 本载体 SHALL 提供**可委托消费的稳定接口**
(常量表 + 查询函数),**不**提供行为实现。

**理由**:#9 的形态学教训 —— adapter 自带表 = 第二真源 ⇒ 规约改一格、adapter 不红。

**落点**:`cxl_state_spec.hh` 的公开接口形态;本 change 的判据不含 adapter(属第 1 个)。

---

## 已排除的替代方案

| 方案 | 排除理由 |
|---|---|
| 沿用 5 态(MESIF 模板) | CXL 无 `F` 态 ⇒ `F` 行与 `F→E` 秩回落例外**无依据**(公开来源实证 4 态) |
| 「全部逐格 `pending` + 跨仓追认」 | ArchForge **既无追认义务也无依据** ⇒ 债**永久挂账**且被误读为「已尽责」 |
| 引 CXL 规范条款作 basis | ⛔ **合规禁止** AI 处理规范正文(ECA §3 + AI AUP) |
| 把 `F` 行保留为「未来可能用」 | 保留即**模板形状污染协议事实** ⇒ 违反「不得把自推写成已裁决」 |
| 建模 transient state | 公开来源明说 transient「not officially part of the CXL.cache specification」⇒ 无依据 |
| 现在就写 adapter / 行为类 | 属第 1 个 change;且 R11 归属**已被 Entry #112 裁决**去重到第 1 个 ⇒ 本 change 不得抢 |
| 镜像实例声明进 ArchForge `32 号 §6.1` | 那是**产品侧**注册表 ⇒ 污染 + 与其「GPU Die 无 CXL」立法冲突 |
| 把「设备互不感知」当作 2.0 的区分性语义 | CXL 2.0 每 root port **只 1 个** cache device ⇒ **平凡成立** ⇒ 无区分力 |
