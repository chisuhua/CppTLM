# Design — cpptlm-cep-stub-cache-core v1.0

> **状态**: 🟡 Proposed(2026-10-25,planner 阶段)
> **上游**: `.rddf/improvements/cpptlm-cep-stub-cache-core.md`(5 段草稿)· `obj-cep-platform §4.0` 队列 #3 · `obj §11` Entry #58
> **跨仓权威(只读)**: ArchForge `32-cep-based-coherence-architecture.md §6.0.1` / `§6.0.2` · `22-nsa-fabric-address-spec.md §0.5.4.1`(NS-R2)
> **本轮裁决(2026-10-25,DRI)**:① **一个 change**(沿用 `obj §4.0` 权威名),ADR-CEP-05 §10 的 `-base`/`-impl` 二分作废;② **双 Phase**(M-4 先行 + stub cache 实现)

---

## Context

### 现状

**① 传输层的 flit 类别 ↔ VC 桶映射是「索引对齐」的副产物,且其语义描述是错的。**

`include/bundles/noc_bundles_tlm.hh:60-62` 定义 3 个传输层 flit 类别:

```cpp
static constexpr uint8_t CATEGORY_REQUEST  = 0;
static constexpr uint8_t CATEGORY_RESPONSE = 1;
static constexpr uint8_t CATEGORY_CREDIT   = 2;  // Credit 信号专用类别
```

`include/tlm/cluster/soc_internal_noc.hh:83-86` 定义 4 个物理桶:

```cpp
static constexpr unsigned VC_REQ = 0;
static constexpr unsigned VC_RSP = 1;
static constexpr unsigned VC_DAT = 2;
static constexpr unsigned VC_SNP = 3;
static constexpr unsigned VC_COUNT = 4;
```

两者靠**值域连续**偶然对齐:`route_flit()`(`:120-127`)取 `cat = flit_category` 直接当桶下标。

**2026-10-25 实测的三条事实**:

| # | 事实 | 证据 |
|---|---|---|
| (a) | **`VC_DAT` 桶在所有真实路径中恒为 0** | 全仓唯一写 `CATEGORY_CREDIT` 的点是 `include/framework/bidirectional_port_adapter.hh:235`(`send_credit_impl`),该 flit 经 `resp_out_port_[port]->send(pkt)`(ChStream port)发出,**不经** `SocInternalNoc::route_flit()`;`SocInternalNoc` 的全部入向流量只写 `CATEGORY_REQUEST`(`include/tlm/gpu/sdma_noc_adapter.hh:57`)与 `CATEGORY_RESPONSE`(`include/tlm/vram_noc_adapter.hh:96`) |
| (b) | **现有注释描述了一个从未发生的流量** | `soc_internal_noc.hh:179-181` 自陈「`vc_dat_count()` 实际统计的是 **credit 流量**,不是数据流量」;实测 `vc_dat_count` / `vc_snp_count` 在 `test/` `src/` `examples/` 中**零引用** |
| (c) | **`test_soc_internal_noc_data_flow.cc:108` 是陈旧注释** | 该用例实际写 `CATEGORY_REQUEST`(`:112`)且**只**断言 `flits_dispatched()` / `unknown_dst_count()`(`:116-117`);**不存在**对 `vc_counts_[2]` 的断言 |

**② 跨仓权威已关闭「credit 是否独占 VC」这个问题。**

ArchForge `32 号 §6.0.1`「明确排除」表:`CATEGORY_CREDIT` 是**流控类别**,非一致性消息类,**credit 随所在 VC 的流一起走,不单独占 VC**。本仓镜像 `include/tlm/cep/protocol_instance.schema.json:131` 同义。回执 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md:394-397` 复核确认。

⇒ 「credit 是否独占 VC」**不是开放问题**;本仓若决策「credit 独占 VC」即直接违反跨仓权威。

**③ 显式映射表的归属已被分配出去。**

回执 A14-3(`:352`)裁定:6 类一致性消息类 → `CATEGORY_*` 的关系由 **adapter 处的显式映射表**承担(**非恒等**),该表归 **CEP 侧 Stage 1** 交付。台账 Entry #55 已把该表分配给 **队列 #6**(`cpptlm-cep-chi-rnf-subset`,S1-5 表体 + contract test)。

**④ Stub Cache Core 只有纸面契约,仓内无实现。**

`ADR-CEP-05` 已定义 6 接口(决策 1)/ 双端口形态(决策 2)/ 折叠映射表(决策 3)/ bypass 模式(决策 4)/ 5 态状态机(§4.1)/ 6 条 `INV-SC*`(§4.3),但**零代码**。Stage 1 后续 5 个队列的准入判据都需要可运行的 stub 作为观测对象。

### 目标态

| 层 | 目标 |
|---|------|
| **传输层** | 映射关系与**每个桶的流量实况**成为**可回归的契约**;零流量桶的「恒 0」有正控;`vc_id` 与 `flit_category` 冲突的归属规则显式化。**零逻辑改动** |
| **Stub Cache Core** | `ADR-CEP-05` 的定义**可执行**:6 接口 + 5 态 + `INV-SC1` ~ `INV-SC6` 各有行为断言 |
| **跨仓** | **零改动** —— 本 change 服从上游权威,不要求上游回改 |
| **冻结面** | **1 个成员**改动(`ADR-CEP-05` 追加 Status Update),其余 0 diff |

### 关键约束

| # | 约束 | 来源 |
|---|---|---|
| K1 | `CATEGORY_*` 取值域恒为 **3 类**;6 类一致性词表落 Coherence Interface 层,**不得**为承载它而扩传输层 | `ADR-CEP-12 §8` IC2 ③ 分层纪律 + 回执 §8.4.3(已判「扩到 6 类」**不可接受**) |
| K2 | `vc_count` 的单点权威在 ArchForge `22-nsa §0.5.4.1`(NS-R2);本仓 `fabric.json` 是**派生副本**,已在 M1 冻结集 | `fabric.schema.json` 自述 + Entry #56 |
| K3 | 冻结面改动须走**两步授权**:先 commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 再 commit 清单 | `cep_freeze_check.sh:48-62` + Entry #9 |
| K4 | `SocInternalNoc` 是**已归档** e1 change 的产物 —— 其 public surface 变更会让「归档件契约 ↔ 现行行为」漂移 | `soc_internal_noc.hh:2-7` |
| K5 | lint 第 10 组①的机器化**前提**就是「索引对齐不会静默折叠任何类别」 | `cep_docs_sync_check.sh:573-576` |
| K6 | 断言「某常量存在」永远不够 —— 必须断言「**输入改变时输出必须改变**」;纯负向断言必须有正控 | 队列 #2 critic 7 轮提炼的六类「有牙」缺口判据 ①③ |

---

## Goals / Non-Goals

### Goals

- **G1** 把 M-4 行为侧决策落成**可回归的契约**(不是 design.md 里的散文),且**先于** stub cache 产生的任何新断言。
- **G2** 让 `ADR-CEP-05` 的接口 / 状态机 / `INV-SC*` 变成**可执行**契约。
- **G3** 本 change 的每个 Scenario 都**可在本仓验证** —— 不留永假验收条款。
- **G4** 冻结面改动**仅 1 个成员**,且走完两步授权。

### Non-Goals

见 `proposal.md` 的 Non-Goals 九条(不扩 `CATEGORY_*` / 不动 `fabric.schema.json` / 不建显式映射表(→ #6)/ 不改 `chip_select` 编码 / 不改 `VC_DAT` 常量名 / 不实现 `ADR-CEP-05 §4.2` 不实现清单 / 不做真实 ProtocolEngine / 不引入协议实例声明 / 不写 TG 与 bypass)。

---

## Decisions

### D1:change 形态 = **单一 change 双 Phase**(M-4 先行 + stub cache 实现)

**裁决**:2026-10-25 DRI 拍板(命名 + 范围切分两问)。

**内容**:

| Phase | 内容 | 为什么在这个位置 |
|---|---|---|
| **Phase 0** | M-4 决策落盘(§D2 ~ §D9)+ 契约化(`cep-vc-mapping-contract`) | **先于**任何新断言 —— Entry #55 ③ 警示「M-4 改现状语义 → 越早定越好(否则期间新断言会固化现有索引语义)」;而 stub cache 正是 CEP 第一个会产生此类新断言的实现型 change |
| **Phase 1** | Stub Cache Core 实现 + 契约化(`cep-stub-cache-core`) | M-4 的**观测载体** —— 只有真实 stub 产生流量,「`VC_DAT` 恒 0」这类断言才有意义 |

**备选与排除**:

| 备选 | 排除理由 |
|---|---|
| **拆 2 个 change**(`-base` + `-impl`,按 `ADR-CEP-05 §10`) | 破 `ADR-CEP-00 §2 Decision 3` 的「11 change / 串行 11 季度」算术;且 M-4 会在「无真实消费者」的真空里被裁决 —— 正是队列 #2 之前的失败模式 |
| **M-4 独立成 change,stub cache 推到 #3 之后** | 同上(多占 1 slot);且 §4.0 排期需重排(五处同改) |
| **M-4 只写 design.md 决策节** | 「散文不是契约」—— M-4 会被 stub 实现期的新断言**静默覆盖** |

---

### D2:M-4 协议层裁定 —— credit **不独占** VC(已被跨仓权威关闭)

**内容**:`CATEGORY_CREDIT` 是**流控类别**,非一致性消息类;**credit 随所在 VC 的流一起走,不单独占 VC**。本仓**不重新裁决**该问题。

**依据(三路互证)**:

1. ArchForge `32 号 §6.0.1`「明确排除」表(`CATEGORY_CREDIT` 行):「credit 随所在 VC 的流一起走,不单独占 VC」
2. 本仓镜像 `include/tlm/cep/protocol_instance.schema.json:131` 同义声明
3. 回执 `docs/implementation/2026-10-21-cep-archforge-response-receipt.md:394-397` 复核确认

**验收**:A-P0-1(决策可寻址,带权威章节号)。

---

### D3:M-4 传输层裁定 —— **保持索引对齐**,**不建**显式映射表

**内容**:传输层 `CATEGORY_*` → VC 桶**维持值域连续映射**(`cat` 直接作桶下标),**不引入**映射表。

**理由(3 条,各带证据)**:

| # | 理由 | 证据 |
|---|---|---|
| 1 | lint 第 10 组①(`CATEGORY_*` 恰 3 个 + 值域连续 0..2)的**机器化前提**就是「索引对齐不会静默折叠任何类别」;改显式表 = 推翻一条已落盘的机器契约,而 3 类恒等映射下显式表与索引对齐**等价**(零收益) | `cep_docs_sync_check.sh:573-576` |
| 2 | 显式映射表的归属**已被跨仓协议分配给队列 #6**;本 change 在传输层再建一张表 = **双层重复映射 + 权责冲突** | 回执 A14-3(`:352`)+ 台账 Entry #55 的队列分配 |
| 3 | `VC_DAT` 桶在真实路径中**恒为 0**(实测 a)→ 为一个零流量桶建映射表是**过度工程** | 实测 (a) |

**备选与排除**:

| 备选 | 排除理由 | 证据 |
|---|---|---|
| **A. 扩 `CATEGORY_*` 到 6 类 + `vc_counts_[6]`** | 与「ARM CHI 4 通道」物理模型矛盾;且 `atomic`/`query` 被**静默折叠**进 `VC_SNP`(无报错、无计数) | `soc_internal_noc.hh:10`;回执 `:378` |
| **B. 保持 3 类但按 6 类词表顺序落 4 桶** | **2 类错义**(`snoop`→`VC_DAT`、`snoop_resp`→`VC_SNP`)+ **2 类静默丢失**(`atomic`/`query`) | 回执 `:380-384` |
| **C. 传输层自建显式映射表** | 违反理由 2(权责冲突)+ 理由 1(推翻机器契约前提)+ 理由 3(零收益) | 同上 |
| **D. credit 跟随 flit 的 `vc_id` 字段归桶** | 引入 `vc_id` vs `flit_category` 的仲裁复杂度;破坏索引对齐 ⇒ lint ① 失效;**无受益者**(credit 不经本 NoC,实测 a) | `bidirectional_port_adapter.hh:246`;`soc_internal_noc.hh:125-127` |

**验收**:A-P0-1 + A-P0-8(lint ①③ 不回退)。

---

### D4:语义正名 —— `VC_DAT` = **预留 DAT 桶**(当前恒 0)

**内容**:修订 `soc_internal_noc.hh:68-71` 与 `:179-181` 注释:

- `VC_DAT = 2` 的描述从「`CATEGORY_CREDIT` —— 索引对齐的副产物」改为「**预留 DAT 桶** —— 当前无 `CATEGORY_*` 数据类;`CATEGORY_CREDIT` 落此桶是**索引对齐的副产物且无实际流量**」
- `vc_dat_count()` 的描述从「实际统计的是 credit 流量」改为「**预留 DAT 桶,当前恒 0**」

**为什么必须改(而不是留着)**:现状注释描述了一个**从未发生的流量**(实测 b)。留着的后果 = 下一个读者据此以为「VC2 装的是 credit」→ 写出新的错误断言 → 与 M-4 想消除的**同一类**误判(回执 §8.4.3 记录的「幽灵类别」误判模式)。

**为什么不改代码**:桶的**值**与**下标**是索引对齐契约的一部分;语义正名不需要动值。

**验收**:A-P0-2 + A-P0-4(正控)。

---

### D5:冲突输入规则 —— 传输层**不仲裁** `vc_id` vs `flit_category`

**内容**:`NoCFlitBundle` 同时携带 `vc_id` 与 `flit_category`(实测两者都写,`test/test_vram_noc_adapter.cc:140-145`)。当两者不一致时:

| 归属 | 规则 | 现状 |
|---|---|---|
| **`vc_counts_` 计数** | 归 **`flit_category`** | `route_flit` `:125-127` 已是 |
| **`chip_select` 编码** | 归 **`vc_id`** | `test_vram_noc_adapter.cc:140` 已是 |
| **传输层** | **不仲裁、不报错、不纠正** | — |

**为什么由传输层「不仲裁」**:仲裁需要**一致性词表知识**(6 类消息类 ↔ `CATEGORY_*` 的对应),那是 **adapter 层**(队列 #6 显式映射表)的职责。传输层加校验 = 把**协议层语义泄漏进物理层**,违反 `ADR-CEP-12 §8` IC2 ③ 的分层纪律(`cep_docs_sync_check.sh:570-571` 注释自述)。

**为什么不能反过来让传输层仲裁**:会误伤既有合法用法 —— `test_vram_noc_adapter.cc:141-149` 就是在构造 `vc_id=VC_DAT` + `category=CATEGORY_REQUEST` 来测编码路径。

**验收**:A-P0-5。

---

### D6:桶名与 accessor **不改名、不弃用**

**内容**:`VC_DAT` 常量名与值保留;`vc_dat_count()` 保留、**不加** `[[deprecated]]`。

**理由**:

- `VC_DAT` 已进入 `chip_select = (vc << 4) | src` 的**跨模块 wire 编码契约**(`test_vram_noc_adapter.cc:140,145` 断言 `0x21`)→ 改名 = 改编码语义常量
- credit 本身亦无实际流量(实测 a)→ 改成 `VC_CREDIT` 同样名不副实
- `vc_dat_count()` 外部消费者 **0 命中**(实测 b)→ 弃用标注零收益;且 `SocInternalNoc` 是**已归档** e1 change 的产物(K4)→ 弃用标注会制造「归档件契约 ↔ 现行行为」漂移

**验收**:A-P0-6(冻结面零 diff 校验覆盖此点)。

---

### D7:`fabric.schema.json` 的 `vc_count.maximum = 4` —— **不需要动**

**内容**:本 change **零改动** `include/tlm/cep/fabric.schema.json` 与 `include/tlm/cep/fabric.json`。

**理由**:

| # | 理由 |
|---|---|
| 1 | 传输层 `VC_COUNT = 4` 不变(`soc_internal_noc.hh:85`)→ lint 第 10 组③ 继续通过 |
| 2 | 协议层 `vc_id` 上界 0-3 不变 → `fabric.schema.json:21` 的「与 `protocol_instance.schema.json` 对齐」声明保持成立 |
| 3 | 权威侧 NS-R2 已锁 `compute_noc` / `ualink_fabric` `vc_count=4`;本仓 `fabric.json` 是**派生副本**(K2)。**动**它的任何路径都须先 ArchForge 侧改 NS-R2 + 本仓走冻结授权闸 —— 本决策**无此需求** |
| 4 | D3 已排除一切需要扩 VC 数 / 扩类别的方案 ⇒ 无触发条件 |

> ⚠️ **纠正一处此前的规划前提**:立项时曾假定「改 M-4 须同步 `fabric.schema.json` 的 `vc_count.maximum=4`」。经 D3 的方案排除 + 权威核对,该假定**不成立** —— 冻结面预算因此从「至少 2 个成员」降为「1 个成员(`ADR-CEP-05`)」。

**验收**:A-P0-6。

---

### D8:lint 第 10 组的迁移形态 —— **既有 3 条不回退,新增断言**

**逐条**:

| 断言 | 内容 | 迁移形态 |
|---|---|---|
| **①**(`:583-626`) | `CATEGORY_*` 恰 3 个声明 + 值域连续 0..2 + 不可静态求值即判红 | **0 迁移** —— D3 保持 `CATEGORY_*` 不增删。**新增 1 条**:值域连续性 ⇒ 索引对齐不折叠任何类别(已有,无需新增) |
| **②**(`:628-641`) | 消费点引用的常量必须真实存在(幽灵类别防护,覆盖**注释与代码**) | **0 迁移**(断言本体),但**新注释受其约束** —— 修订后的 `soc_internal_noc.hh` 注释只许引用真实存在的 `CATEGORY_REQUEST` / `CATEGORY_RESPONSE` / `CATEGORY_CREDIT`;`CATEGORY_DAT` 之类词**一概禁写**(`:636` 注释提及亦计) |
| **③**(`:643-648`) | `VC_COUNT = 4` + `soc_internal_noc.hh` 含「4 独立 VC」字样 | **0 迁移** —— `VC_COUNT` 不变、`:10` 物理模型声明保留 |
| **④** | **新增**:`VC_DAT` 注释**不得**再声称「统计 credit 流量」,且**必须**含「预留」语义 | 新 lint 断言(Phase 0) |
| **⑤** | **新增**:`soc_internal_noc.hh` 必须显式声明 `vc_id` / `flit_category` 的冲突归属规则 | 新 lint 断言(Phase 0) |
| **⑥** | **新增**:`test_soc_internal_noc_data_flow.cc` 不得含陈旧的 `vc_counts_[2]` 表述 | 新 lint 断言(Phase 0) |

**新断言的纪律**(K6 + 队列 #2 先例):

- 断言④⑤⑥ 须配**反向 fixture**(`cep_gate_selftest`:变异必红 + GREEN 样本)
- 若断言④涉及「`VC_DAT` 恒 0」,**必须有正控**:注入 `CATEGORY_CREDIT` flit → `vc_dat_count()` 必须 0→1(判据③)
- 新断言须与**它守护的语义等价**(判据⑤)—— 不是「注释里出现某词」这种可绕过的形态

**验收**:A-P0-8 + A-P0-4。

---

### D9:既有断言迁移清单

| # | 既有位置 | 实测现状 | 迁移后形态 |
|---|---|---|---|
| 1 | `test/test_soc_internal_noc_data_flow.cc:108` | **陈旧注释**;该用例实际写 `CATEGORY_REQUEST`(`:112`),只断言 `flits_dispatched()` / `unknown_dst_count()`(`:116-117`) | 修正注释文字;**断言集合 0 变化** |
| 2 | `test/test_vram_noc_adapter.cc:140,145` | `VC_DAT` 参与 `chip_select = (vc << 4) \| src`,断言 `0x21` | **0 改动**(常量名与值保留,D6) |
| 3 | `include/framework/bidirectional_port_adapter.hh:235` | 写 `CATEGORY_CREDIT`,经 `resp_out_port_` 发出,**不经** `SocInternalNoc` | **0 改动** |
| 4 | `include/tlm/cluster/soc_internal_noc.hh:173-182` | 5 个 accessor;`vc_dat_count()` / `vc_snp_count()` 外部 **0 消费者** | 全部保留;**仅注释**正名(D4) |
| 5 | `NoCFlitBundle` 的 `vc_id` + `flit_category` 双字段 | 计数链只读 category;编码链只读 vc_id | 现状即分离,仅**注释显式化**(D5) |

**验收**:A-P0-3(断言集合等值) + A-P0-6(零 diff)。

---

### D10:Stub Cache Core 的实现范围与**不可验证条款的降级**

**内容**(逐条指向 `ADR-CEP-05`):

| 项 | 范围 | 依据 |
|---|---|---|
| 6 接口 | `request` / `snoop` / `snoop_resp` / `writeback` / `completion` + core-side 原语 `evict_line` | 决策 1(命名逐字对齐 `ADR-CEP-02 §2` canonical 10 流) |
| 5 态 | `NONE → I → S → E → M`(协议无关);**禁止**暴露 CHI UC/SC/UD/SD 七态 | §4.1 + `INV-CEP-05-IC1` |
| 双端口 | `cpu_side`(功能操作,阻塞)/ `net_side`(RN 投影 5 条)分离;hit 路径零网络流量 | 决策 2 + `INV-SC6` |
| `INV-SC1` ~ `INV-SC6` | 各至少 1 条**行为**断言,用 **mock ProtocolEngine** 驱动 `net_side` | §4.3 |
| 折叠映射 | **单向形态**可静态断言:接口面不出现 `forward_data` / `back_invalidate` / `barrier` 字样 | 决策 3 |

**不可验证条款的降级**(G3 的落地):

| `ADR-CEP-05 §3` 契约测试 | 为何本仓不可验证 | 处置 |
|---|---|---|
| #1 `forward_data` 不可分辨性 | 要求「peer 转发的 completion」与「真实协议的 barrier 流」→ 需**真实 ProtocolEngine**;仓内首个协议实例在队列 #6 | **不进 spec**;登记「触发条件 = 队列 #6」 |
| #4 barrier 不可见 | 同上 | **不进 spec**;同上 |

**为什么这样处置**:本仓已发生过「判据 C-4 长期标『✅ 可自动化』,而该检查器从未实现」的失败模式(per `cep_decl_reverse_verify.sh` 背景注释)。把不可验证的条款写进 spec = 制造**永假的验收条款**。

**验收**:A-P1-1 ~ A-P1-6。

---

### D11:`ADR-CEP-05 §10` 的 `-base` / `-impl` 二分 —— **作废**

**内容**:2026-10-25 裁决(单一 change)后,`ADR-CEP-05 §10` 的两个后续 OpenSpec 名(`cpptlm-cep-stub-cache-core-base` / `cpptlm-cep-stub-cache-core-impl`,各 1 KPI slot)**永久失指**。处置:给 `ADR-CEP-05` 追加 `## Status Update`,声明该二分已被 `obj §4.0` 的重排取代(§10 **正文不改**,per ADR 不可变 + Status Update 先例)。

**为什么需要动这个冻结面成员**:否则下一个读者按 §10 去建 `-base` change → 与 `obj §4.0` 的权威名冲突 → 双份 change 目录 → KPI slot 记账分歧。

**授权**:Entry #58 → `cep_freeze_check.sh --update --authorized-by "Entry #58"` 两步流程。

**同批的第二个冻结面成员**:`docs/adr/ADR-CEP-00-platform-charter-and-governance.md` —— KPI baseline 断言② 扫 ADR 的 Status Update 块且**归档日及之后的块不豁免**;队列 #2 归档时追加的段落写「**当前态**:并发 0/3」,队列 #3 建目录后该值成为**历史快照** → 须把措辞改为「**当时态(已被下一段取代)**」(**数值保留**,仅状态措辞 —— per Entry #18 的「定义句零改动、状态陈述刷新」先例)并追加 2026-10-25 新段。处置与队列 #2 **同形**。

**验收**:A-P0-7。

---

## Risks / Trade-offs

| 风险 | 影响 | 缓解 | 关联 AC |
|---|---|---|---|
| **spec 写出永假验收条款** | 归档时才发现不可验证 → KPI 级联阻塞 | spec 只收本仓可执行判据;每条点名锚点;对账测试断言 Scenario 数 == 锚点数 | A-P1-6 / A-P1-7 |
| **「恒 0」断言无正控 → 无牙** | 判据退化时仍然绿 | 强制正控:注入 `CATEGORY_CREDIT` flit → 计数必须 0→1 | A-P0-4 |
| **新 lint 断言可被绕过**(如行尾贴白名单词) | 队列 #2 critic 5 轮实测过的绕过形态 | 新断言须配**反向 fixture**(变异必红 + GREEN 样本) | A-P0-8 |
| **stub 实现期顺手扩 `CATEGORY_*` / 提前建显式映射表** | 越权到队列 #6 + 推翻 lint ① 前提 | Non-Goals + lint ① 持续守护 + §D3 理由 2 的交叉引用 | A-P0-8 |
| **`ADR-CEP-05` 的 public surface 变更造成归档件漂移** | 已归档 e1 契约 ↔ 现行行为不一致 | D6 明确不改名 / 不弃用 | A-P0-6 |
| **Phase 1 的 8 周估时与实际范围脱节** | 排期失真 | Phase 0 / Phase 1 边界逐条划清;不可验证项走降级登记;估时口径只在 `obj §4.2` | — |
| **单开发者仓无外部真人评审**(独立性问题) | 评审结论的独立性受限 | 本仓的「评审」由 oracle / metis / critic subagent 承担 —— **不等价于**真人外部评审,结论的独立性缺口已在 `obj §8.1` R10 登记 | — |

---

## Open Questions

| # | 问题 | 当前处置 | 归属 |
|---|---|---|---|
| **Q1** | **Traffic Generator 与 bypass 模式**是否在本 change 范围内? | **默认不进** —— `ADR-CEP-05 §5`(TG 接入)与决策 4(bypass)的归属待定;本 change 的 `cpu_side` 由**测试夹具**驱动。stub 的 `cpu_side` 若长期无真实驱动者,契约测试只能靠 mock | 待裁决(可能归队列 #5 `cpptlm-cep-collective-plane-base` 或独立 change) |
| **Q2** | `ADR-CEP-05 §3` 的契约测试 #2 / #3 / #5 ~ #10 中,是否有更多条目需真实 ProtocolEngine 而非 mock? | Phase 1 实施时逐条判定;命中即走 D10 的降级登记 | Phase 1 实施期 |
| **Q3** | `stub_cache_core` 是否需要进 `ModuleFactory`? | **不进** —— stub 是 testbench 定位的组件,与 `ADR-CEP-05` 的「三明治挂载」形态一致(由测试夹具装配) | Phase 1 实施期(若需改 → ADR 追加 Status Update) |
| **Q4** | Phase 0 完成后,`ADR-CEP-09` / `ADR-CEP-12` 是否需追加 Status Update? | **默认不需要** —— D2 / D3 的裁定均**服从**既有权威,未产生新决策 | 复评时确认 |
