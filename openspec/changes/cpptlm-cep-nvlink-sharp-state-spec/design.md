# Design — cpptlm-cep-nvlink-sharp-state-spec

> **状态**: 🟡 Proposed(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/cep-nvlink-sharp-state-spec/spec.md`](specs/cep-nvlink-sharp-state-spec/spec.md)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(下称**锚点摘要**;条目号 `A-1`~`A-5` / `N-1`~`N-7`)

---

## 1. 根因与前提

### 1.1 为什么需要**第 0 个 change**(规约层)

队列 #13 的形态是**丙 = 甲 + 乙合流**:

| 形 | 内容 | 载体 |
|---|---|---|
| **甲** | 第 7 个 coherence 实例 `nvlink_4` + `NvlinkAdapter` | `include/tlm/cep/nvlink_adapter.hh` + `protocol_instances.json` |
| **乙** | collective 平面的 SHARP 扩展 | `include/tlm/cep/collective_plane.*` + `collective_planes.json` |

⇒ 两形**共用**同一批语义载体形态决定项(epoch / reduce 族 / 分类判据 / VC)⇒ 若先写实现层,
这些语义会**散落**在实现代码里 ⇒ **不可审计**、不可回归 ⇒ 故**必须**先有规约层。

### 1.2 规约层必须**钉死**的 4 项(普查已给,Entries #173)

| # | 项 | 钉法 | 锚点 |
|---|---|---|---|
| 1 | **能力域 = 节点内** | `NVLS` 用 NVLink SHARP 做 **intra-node** 归约;跨节点才转 CollNet ⇒ 载体 SHALL NOT 声明跨 fabric 归约 | `A-1` / `A-5` |
| 2 | **多 group 并存** | 第 3 代 SHARP 允许**同一拓扑多棵聚合树**、多作业共享 ⇒ `group_id` **不得**是单例 | `A-4` |
| 3 | **归约点在 fabric ingress** | 归约在网络侧完成 ⇒ 支撑 `ADR-CEP-03 §4` 的 **S1**(唯一分类点);⚠️ **S4 是设计约束,不由公开来源支撑** | `A-3` |
| 4 | **VC = 无隔离 VC(显式声明)** | 两网络 0-3 全占 ⇒ Decision D:`unavailable` + 四字段声明 | `ADR-CEP-01` SU 2026-11-03(D.4) |
| 5 | **reduce 族 = 复用既有 `ReduceOp` 恰 6 值** | 不新增枚举、不改 `collective_plane.hh`;SHARP 逐算子支持 = `N-1` **pending** | `ADR-CEP-03` SU 2026-11-03(E.2) |
| 6 | **epoch 分层**:平台侧 = R3/R4(已定稿)· 协议侧 = `N-3` **pending** | 载体 SHALL NOT 断言协议侧状态机 | `ADR-CEP-03` SU 2026-11-03(E.2) |
| 7 | **③ `perm` 值域 = 维持 4 值锁**;NVLink 实例的**具体子集** = `N-8` **pending** | 不新增 perm 值;不照抄 `tilelink_c` 的 3 值 | 台账 Entry #177 ① |
| 8 | **④ atomic = `request` 流的独立属性**;协议侧 completion 配对 = `N-9` **pending** | 不新增第 11 条流、不占 `perm` 位 | 台账 Entry #177 ② |

### 1.3 已被判为**不可锁**的 9 项(锚点摘要 §3;7 项来自 Task 0 普查 + 2 项来自裁决 ③ / ④)

`N-1` reduce 算子穷举集 · `N-2` 逐操作 ✓/✗ 矩阵 · `N-3` epoch 状态机细节 ·
`N-4` VC 编号 · `N-5` 聚合树构建算法 · `N-6` 报文格式 / opcode 布局 ·
`N-7` 数值 / 精度语义 · `N-8` NVLink 实例的 `perm` 具体子集 · `N-9` NVLink atomic 的协议侧 completion 配对

⇒ 载体 SHALL 把这 **9** 项登记为 `pending`,**SHALL NOT** 出现其具体取值的断言。

> ⚠️ `N-3` 与本 ADR 的 `R4` **不矛盾**:`R4` 是**平台侧**的 epoch 纪律(未 commit ⇒ 放行),
> 公开来源缺的是**协议侧**的 epoch 状态机细节 ⇒ 前者可锁,后者须 pending。
> ⇒ **已裁**(Entry #176):平台侧 R3/R4 **锁**(`self-derived` + 非 pending),协议侧 `N-3` **留 pending**;
> **reduce 族**同理:**平台侧复用既有 `ReduceOp` 恰 6 值**(锁)· **SHARP 逐算子支持 = `N-1` pending**。

---

## 2. 方案对比(载体形态)

| 方案 | 结论 | 理由 |
|---|---|---|
| **A** 载体与实现**同文件**(把规则直接写进 `nvlink_adapter.hh`) | ❌ 否决 | 规约层失去**独立可审计面**;实现改动会连带改契约 ⇒ 无法只审契约 |
| **B** 载体为**纯声明表**(只有数据,没有谓词) | ❌ 否决 | 分类点唯一性 / epoch 双向 / 三步原子**无法**被机械判定 ⇒ 回到纸面契约 |
| **C** 载体为**header-only 的「类型 + 纯函数 + 常量表」** | ✅ 采纳 | 判据可在**编译期 + 运行期**双锁;**零**实现依赖 ⇒ 可在实现层之前独立落地;与 `ADR-CEP-03 §4` 的「判据摘要 + 实现指针」形态一致 |
| **D** 载体落在 `collective_plane.hh`(复用既有平面抽象) | ❌ 否决 | 该文件是**接口 + 行为面**;塞入 SHARP 规约会把「契约」与「行为」混在一处 ⇒ `CP-01` 零调用边判据的载体边界变模糊 |

⇒ 采纳 **C**:新增 `include/tlm/cep/nvlink_sharp_state_spec.hh`(header-only),
**类型 + 纯函数 + 常量表**,不引入任何实现依赖、不改既有头。

---

## 3. 编译期断言设计(NSS-01 ~ NSS-09)

> ⚠️ **4 铁律**:① 纯负向断言须配正控 ② 每条判据须有可执行反向 fixture 且**因正确原因红**
> ③ **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 每条 `static_assert`  SHALL 另有一条**运行期**判据兜住
> ④ 载体状态变迁后须**重新基线化**(锚点**动态定位**)

| # | 判据 | 编译期锁 | 运行期兜底(必需) | 反向 fixture(必红 + 归因 id) |
|---|---|---|---|---|
| **NSS-01** | 分类点唯一 | 源端侧类型**不含**区域表字段(编译期不可构造);**载体自足**(只 include 标准库,照 `cxl_dcoh_state_spec.hh` 先例)⇒ 委托**不**构成源端可见性 | 静态扫描:源端头文件不出现 `collective_region_table` —— ⚠️ **该扫描归实现层**(coherence 侧扫描集合扩展 + 标识清单补项,**复用既有 id**,per Entry #176 ①) | `NSS-01-SRC-OPCODE` |
| **NSS-02** | epoch 双向 | `classify_at_fabric_ingress` 的返回值集合 == {`DiverT`, `Pass`} | 两向各一例 | `NSS-02-EPOCH-SEMANTICS` |
| **NSS-03** | 注册三步原子 | `register_region` 返回值集合 == {`Ok`, `DrainFailed`} | drain 失败一例 | `NSS-03-DRAIN-FAIL` |
| **NSS-04** | 注销回归 | `destroy` 后 `lookup_region` 返回空 | 注销一例 | `NSS-04-DESTROY-LEAK` |
| **NSS-05** | 可观测性 | `DivertCounters` 的字段集合恰 2 | 计数器递增 + fail-stop 一例 | `NSS-05-NO-FAILSTOP` |
| **NSS-06** | ordering 四类 | `OrderingRule` 枚举恰 4 值 | 四类各一例 | `NSS-06-ORDERING-MIS` |
| **NSS-07** | VC = 状态 | `VcReservationStatus::status` 类型为**字符串字面量枚举**,仅 `Unavailable` | 四字段齐全一例 + 独立 VC 编号**不可表达** | `NSS-07-VC-TABLE` |
| **NSS-08** | 能力域 = 节点内 | `CollectiveScope` 枚举恰 {`IntraNode`} | 跨 fabric 值**不可表达** | `NSS-08-SCOPE-OVERREACH` |
| **NSS-09** | 逐格 basis | 每格 `Basis` 含 `kind` / `pending` / `ref` 三字段 | 逐格核对表 + **9 项** pending 逐条可寻址 | `NSS-09-BASIS-MISSING` |

> ⚠️ **NSS-07 / NSS-08 的反向 fixture 形态特殊**:它们靠**「不可表达」**成立 ⇒ 反向 fixture 是
> 「**试图表达 ⇒ 编译红**」⇒ 按铁律 ③,SHALL 另配一条**运行期**判据(如「载体解析出的 scope 恒为 intra-node」)
> ⇒ 否则该判据只是「编译红」,不构成 ctest 判据。

---

## 4. 逐格 `basis` 的**两轴规则**(照 #10 §8.2 / #12 §4)

| 轴 | 取值 | 含义 |
|---|---|---|
| **证据强度** `kind` | `public-direct` / `public-analogical` / `self-derived` | 该格语义**直接**来自公开来源 / **类比**推导 / 平台**自定**(如 `R1`~`R5` 这类 ADR 契约) |
| **钉版强度** `pending` | `true` / `false` | `true` ⇒ 公开来源**不支撑**具体值(⇒ 对应 `N-1`~`N-7` 之一)⇒ **SHALL NOT** 出现该值的断言 |

- `ref` SHALL 指向**锚点摘要的条目号**(`A-n` / `N-n`),**SHALL NOT** 指向规范原文。
- `pending == false` SHALL 同时满足 ① 来源 `pin=strong` ② 引用 / 再分发许可已人工确认。
- **本队列特有**:`R1`~`R5` 与 ordering `A`~`D` 属 `self-derived`(ADR 契约,非公开来源)⇒
  `kind = self-derived` 且 `pending = false`(**契约已定稿**)⇒ 不得因「无公开来源」而误标 `pending`。

---

## 5. 后果 / 代价

### 5.1 正面

| # | 后果 |
|---|---|
| 1 | 分类点唯一性 / epoch 双向 / 三步原子 / 能力域边界**全部可机械判定** ⇒ 实现层无解释空间 |
| 2 | `BroadcastNull` 的行为面在**本队列**首次兑现 ⇒ 闭合 `ADR-CEP-04` 的声明-实现缺口 |
| 3 | **9** 项 `pending` 显式留空 ⇒ 杜绝「看似完整实则杜撰」(队列 #9 式失败模式的正面防线) |
| 4 | VC 以**状态**表达 ⇒ 与 Decision D 一致,**不**留「假装有隔离 VC」的债 |

### 5.2 负面 / 代价

| # | 代价 |
|---|---|
| 1 | **两段式 = 两次 KPI slot** ⇒ 规约层归档前实现层**不得开工** ⇒ 总时长拉长 |
| 2 | 载体是**纯声明** ⇒ 本身**不产生行为**;若实现层始终不落地 ⇒ 规约层即纸面契约 ⇒ Phase 4 SHALL 复核实现层是否已建 |
| 3 | `N-3`(epoch 状态机细节)留 pending ⇒ 平台**无法**据此校验协议侧 epoch 行为 ⇒ 该风险显式转移到实现层的取证流程 |
| 4 | 载体形态决定项仍有 **2 项未裁**(① `NvlinkAdapter` 是否新建头文件 · ⑤ epoch / reduce 族细节)⇒ 规约层须在 Phase 1 前拿到裁决 |

### 5.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| 1 | **能力域写成跨 fabric** | NSS-08(枚举**不可**表达跨 fabric) |
| 2 | **epoch 语义写反** | NSS-02 双向锁(⚠️ 本仓已错过一次) |
| 3 | **9 项 pending 被推理填空** | NSS-09 逐格核对 + 反向 fixture |
| 4 | **VC 被写成「预留独立 VC」** | NSS-07 + Decision D |
| 5 | **规约层自身成为纸面契约** | Phase 2 的**非 pending 格数下限** + Phase 4 的实现层复核 |

---

## 6. 冻结面

| # | 冻结面成员 | 本 change 是否改动 | 处置 |
|---|---|---|---|
| 1 | `docs/adr/ADR-CEP-01` ~ `ADR-CEP-03` | ❌ **零改动** | 契约面已于 Entries #169 / #170 / #171 交割;本 change **只消费** |
| 2 | `include/tlm/cep/collective_planes.json` / `.schema.json` | ❌ **零改动** | Decision D 的改动已在 `2ea09607` 落地 |
| 3 | `include/tlm/cep/collective_plane_contract.hh` | ❌ **零改动** | CP-03 的声明门已在 `2ea09607` 落地 |
| 4 | `include/tlm/cep/protocol_instance.schema.json` | ❌ **零改动** | `nvlink_4` 的 13 键由**实现层**登记(本 change 不碰实例声明) |

⇒ **预期 0 件** ⇒ Phase 0 须**实测确认**(新增载体 `nvlink_sharp_state_spec.hh` 不在 M1 冻结集内)。⚠️ **裁决 ① 的附带结论**(Entry #176):因 state spec 载体**自足** ⇒ collective 侧扫描集合**无需扩展** ⇒ **契约面零改动** ⇒ 冻结面仍为 **0 件**
⇒ 若实测出 1 件 ⇒ 走**两步授权**(台账 Entry → `--update --authorized-by` → commit → `--update` → commit)。
