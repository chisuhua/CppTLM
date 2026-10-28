# Design — cpptlm-cep-chi-rnf-subset v1.0(Stage 1)

> **状态**: 🟡 **Proposed**(2026-10-28 转正,Entry #91)
> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/cep-chi-rnf-subset/spec.md`](specs/cep-chi-rnf-subset/spec.md) · [`specs/cep-instance-authority-partition/spec.md`](specs/cep-instance-authority-partition/spec.md)
> **上游决策**: `ADR-CEP-12`(实例声明 schema)· `ADR-CEP-11 §3`(`chi-r2` 锁定)· `ADR-CEP-02`(canonical 10 流)· `ADR-CEP-05 §13.1`(静默升级判据)· `ADR-CEP-07A`(协议私有状态边界)
> **规划输入**: `.rddf/improvements/cpptlm-cep-chi-rnf-subset.md`

---

## D-α —— 核心架构问题:CHI 实例声明的**落点**

### 事实

1. `include/tlm/cep/protocol_instances.json` 是 **ArchForge 32 号 §6.1/§6.2 的逐字段派生副本**(该文件 `derived_from.mirror_note` 明写);
2. `scripts/test/cep_decl_alignment_check.sh` 第 ⑥ 项:
   - 对 ArchForge 已声明的每个实例,逐键(除 `status`)严格比对 ⇒ 漂移报红;
   - **`extra_ids = set(repo_insts) - set(ids)` ⇒ 「派生副本含 ArchForge 未声明的实例」报红**(`:466-468`);
3. ArchForge 侧**没有** CHI RN-F 声明(grep `32 号` / `09 号`:零命中);
4. ①~⑤ 的语义校验(schema 13 属性 / `vc_count` 上界 / 词表 / 网络集)**只对 ArchForge 提取的声明跑** ⇒ 本仓副本靠 ⑥ 的逐字段相等**传递性**获得信任。

### 候选方案对比

| 方案 | 做法 | 致命缺陷 |
|------|------|---------|
| **(B) 独立声明源** | 新建 `chi_rnf_instance.json`,走队列 #6 的 `perm_upgrades.json` 先例 | ① **6 处消费方全部读单一文件**(`channel_dependency_graph.hh:423` 缺省路径 + `:434-482` config 单路径 / `module_factory.cc:766` / `collective_plane_contract.hh:909` / `adapter_contracts_loader.hh:82` / 5+ 测试)⇒ 要么给每个消费方发明合并层(**违反「不发明新抽象层」**),要么 CHI **绕过装配闸** ⇒ 直接违反交付面「装配期校验实测」与 `INV-CEP-12-IC2`;② **AC-02 存在性判据必 FAIL**(`adapter_schema_contracts.hh:279-284`:`perm_upgrades.json` 的 `instance_id` 必须存在于 `protocol_instances.json` 的 `instances[]`);③ 本仓自持文件**零** schema / ①~⑤ 校验接线 |
| **(C) 等 ArchForge 补声明** | 跨仓阻塞 | 跨仓无界阻塞;与 `07 号 §5.1.1` 定位矛盾(CHI 是本仓工业族口径,ArchForge 只贡献 2 实例);对齐文档 G10 早已预留「族边界权威方」问题 |
| **(A) 直接加进副本** | 不动 ⑥ 语义 | ⑥ 立刻报红 ⇒ 守门被破坏 |

### 裁决:**A′ —— 同文件 + 实例级权威登记 + ⑥ 收窄 + ①~⑤ 扩面**

```
protocol_instances.json
├── instances[]                          ← 既有 13 属性数组
│   ├── moesi_gpu                        (ArchForge 权威)
│   ├── mesif                            (ArchForge 权威)
│   └── chi_rnf                          (本仓权威 —— 顶层登记)
├── repo_owned_instance_ids[]            ← 新增顶层键:["chi_rnf"]
├── channel_dependency_edges             (不变)
└── derived_from                         (不变)
```

**⑥ 收窄后的语义**(3 条):

| 规则 | 判定 |
|------|------|
| **R1** | 镜像逐字段比对只覆盖 `ArchForge ids ∩ repo ids`;ArchForge 声明的实例仍 **12 键逐字段严格**(镜像漂移 ⇒ 红) |
| **R2** | `extra_ids − repo_owned_instance_ids` 才报红;**未登记**的额外实例 ⇒ 红 |
| **R3** | 登记 id 与 ArchForge 已声明 id **撞名** ⇒ 红(权威迁移闸 —— 不允许本仓「接管」ArchForge 的实例) |

**①~⑤ 必须扩面**(不可省):否则 repo-owned 实例**不受** schema / `vc_count` / 词表 / 网络集 / 边端点绑定校验 ⇒ 传递性信任断裂 ⇒ 改造只放松不加强 ⇒ **背离「扩面 = 不放松」的意图**。

**为什么 A′ 支配 B/C**:
1. 6 处消费方**零改动** —— 装配闸 / 装载器 / CC-05 ④ / 装配期校验自动看见 CHI;
2. AC-02 存在性判据**天然通过**(CHI 在 `instances[]` 里);
3. 与 `status` 豁免先例(`:453-461`)**同构** —— 「本仓权威」从**字段级**推广到**实例级**;
4. **可规模化** —— 后续 5 个非 ArchForge adapter(cxl-nonsymmetric / tilelink / cxl-dcoh / nvlink / ualink)各填一行登记即可,不再重复发明;
5. 对齐文档 `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md` G10 早已预留「Ruby 族由 CppTLM 侧插件提供,族边界权威方需明确」。

**治理归属**:本裁决**扩大**了 `ADR-CEP-12 §10` 裁决 ① 的**前提**(「`instances[]` 全集 = ArchForge 声明全集」)⇒ 属**决策变更**,不是状态登记 ⇒ 走 **architect 车道**(Phase 0):追加 `## Status Update`,并**顺带追认**队列 #6 的 `D-ζ`(当年**未**追加 ADR 的 `status` 字段级豁免)。

> **schema 零改动**:`protocol_instance.schema.json` 校验的是**单个实例对象**(`additionalProperties: false` + 13 `required`),**不校验 envelope** ⇒ 顶层加 `repo_owned_instance_ids` **不需要**改 schema ⇒ **不触碰**该冻结面。`ADR-CEP-12` 的追加 Status Update 是**唯一**的冻结面改动。

---

## D-β —— CHI RN-F 的 6 通道 → CohIf 5 流的映射

队列 #7 的 `CohIfBundle` 只覆盖 **5 条 RN-side 流**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`,per `include/bundles/cep_coherence_bundles_tlm.hh`)。逐通道映射:

| CHI 通道(RN-F 视角) | 承载事务 | CohIf 流 | 字段落点 |
|---|---|---|---|
| **TXREQ** | ReadShared / ReadUnique / MakeUnique | `Request` | `address` / `perm` / `size` / `transaction_id` |
| **TXREQ**(同道) | WriteBackFull / WriteCleanFull | `Writeback` | `address` / `data` / `dirty` —— ⚠️ **TXREQ 双流承载**,靠 `flow_id` 区分 |
| **RXRSP** | Comp / CompDBIDResp / RetryAck | `Completion`(无 data) | `perm` / `error` / `transaction_id` |
| **RXDAT** | CompData / DataSepResp | `Completion`(**带 data**) | `data` |
| **RXSNP** | Snp* | `Snoop` | `address` / `snoop_type` / `transaction_id` |
| **TXRSP** | SnpResp(无数据) | `SnoopResp` | `hit` / `line_state` / `dirty` |
| **TXRSP** | **CompAck** | **无 CohIf 流** | PE 内部终结 + 网络侧发射(流 10 被 `CohIfBundle` 类型层排除) |
| **TXDAT** | CopyBackWrData | `Writeback`(带 data) | `data` |
| **TXDAT** | SnpRespData(带数据) | `SnoopResp`(**带 data**) | ⚠️ CohIf 层合法;**6 类 / VC 层必须分流**(见 D-γ) |

### ⚠️ 两层映射必须**两张表**(合并必自相矛盾)

| 层 | 表 | `SnpRespData` 落点 |
|---|---|---|
| **CohIf 流层** | 通道 → 5 流 | `SnoopResp` 流,`data` 可空 |
| **6 类 / VC 层** | 消息类 → `CATEGORY_*` + VC | **不得**进 `snoop_resp` 类(VC-R2:`snoop_resp` **不携带数据**)⇒ 走 `forward_data` 载体 → `compute_noc` VC2 |

### subset 的**显式排除** + 机器断言

| 排除项 | 理由 | 断言形态 |
|---|---|---|
| 非 CopyBack 写(`WriteUnique` / `WriteNoSnp` 的 `NCBWrData`) | `Request` 流**无 data 字段**(`CohIfBundle` 12 字段设计如此) | 注入该 opcode ⇒ adapter **拒绝**;静默收下 ⇒ 红 |
| `atomic` | CHI RN-F subset 不实现 Atomic* 事务 ⇒ `atomic.supported = false`、**零**映射行 | 注入 atomic intent ⇒ 不产生 atomic 类事务 |
| DVM / Retry / PCrdGrant | subset 假设「对端 HN 不 Retry」 | design 登记前提假设(⚠️ **无现有判据能发现** ⇒ 登记为已知缺口) |

**结论**:`CohIfBundle` **零扩展** ⇒ 队列 #7 的 CC-01(5 流域锁)/ CC-02(宽度锁表)零触碰。

---

## D-γ —— 6→3 消息类映射(4 行)

### 「实际使用」的界定规则

> 某消息类「实际使用」⟺ subset 声明的 opcode 集能在该类上**发出或吸收**一个 wire 事务。

| 消息类 | CHI 侧 | 依据 |
|---|---|---|
| `request` | ✅ | TXREQ(ReadShared / ReadUnique / MakeUnique / WriteBack) |
| `response` | ✅ | RXRSP(Comp / CompDBIDResp) |
| `snoop` | ✅ | RXSNP(Snp*)—— ⚠️ **入向**(只收不发) |
| `snoop_resp` | ✅ | TXRSP(SnpResp) |
| `atomic` | ❌ | subset 不实现 Atomic* |
| `query` | ❌ | CHI 的 home 解析走**静态 SAM**,subset 内无 wire query 事务 ⇒ `address_resolve = false` |

### 映射行(4 行)

| 消息类 | → `CATEGORY_*` | 与平台方向默认表 |
|---|---|---|
| `request` | `CATEGORY_REQUEST` | **相同** |
| `response` | `CATEGORY_RESPONSE` | **相同** |
| `snoop` | `CATEGORY_REQUEST` | **相同**(入向 snoop 仍**需要应答** ⇒ 链路事务轴不变) |
| `snoop_resp` | `CATEGORY_RESPONSE` | **相同** |

### ⚠️ AC-04 的**恒等语义缺口**(如实登记,不静默放过)

`adapter_schema_contracts.hh:388-398` 的恒等判定**遍历全 6 类**默认表,任一类**未命中** ⇒ `identical = false`。CHI 只用 4 类 ⇒ 机械判「**非恒等**」。

- **结构上真**(确实 ≠ 全 6 类整表)、**语义上误导**(4 行全部与方向默认表一致);
- ⇒ 本 change **登记**该缺口(「子集 adapter 的恒等判定是已知语义缺口,`mesif`(#9)及后续 subset adapter 都会复发」),**不**在本 change 修 AC-04 机制(机制改动 = MODIFIES `cep-adapter-schema-contracts` main spec + 独立 change ⇒ 挤占 KPI slot 且越界);
- ⇒ `kMappingIsIdentity` 置 **`true`** + 携带 `kIdentityReason`(与 `moesi_gpu_adapter.hh:141-148` 同款纪律:**恒等是合法状态,但必须被登记**)。

---

## D-δ —— CHI RN-F 的状态模型与 `perm` 投影

### 状态集(5 稳定态 + 瞬态内部化)

| 稳定态 | 语义 | → `perm` | 投影精确性 |
|---|---|---|---|
| `I` | 不在缓存中 | `NONE` | 精确 |
| `SC` | Shared Clean | `READ` | 精确(`READ` = 「可读(S)」,不声称独占) |
| `UC` | Unique Clean | `UNIQUE` | 精确(`UNIQUE` = 「独占干净(E)」) |
| `UD` | Unique Dirty | `WRITEBACK` | 精确(`WRITEBACK` = 「独占脏(M)」) |
| `SD` | Shared Dirty | `WRITEBACK` | ⚠️ **拉伸** —— 见下 |
| 瞬态 | 等 snoop / 等数据 | **无投影** | per `INV-CEP-05-IC1`(Stub 只暴露稳定态) |

`UDP`(部分行脏)**排除** —— 写回合并复杂度对 Stage 1 无价值,design 登记排除理由。

### ⚠️ `SD → WRITEBACK` 的语义损失(必须登记)

`WRITEBACK` 字面语义是「**独占**脏」,而 `SD` 是**共享**脏 ⇒ 拉伸。

**与 `moesi_gpu` 的 `O → WRITEBACK` 合并损失同型**(`moesi_gpu_adapter.hh:124-127` 的 `kPermMergeNote` 已开先例)⇒ `ChiRnfAdapter` 写自己的 merge note。

**⚠️ 但补偿机制不同(措辞必须写准)**:
- `moesi_gpu` 的损失由 `SparseSnoopFilter`(`scope=SYSTEM`)补偿;
- `chi_rnf` 的 `sharer_tracker` = **`不参与`**(见 D-ε)⇒ 「是否还有其他 sharer」在 RN 侧**物理上不可得**(该信息只存在于 HN-F)⇒
  ⇒ merge note 登记为「**协议信息拓扑的固有损失**,非实现缺口,RN 侧**不可补偿**」。

### `perm` 数组 = 全 4 值 `[NONE, READ, UNIQUE, WRITEBACK]`

### 静默升级(`UC → UD`)

`UC` 上 store 命中 ⇒ `UD`,**零总线事务**(已持 Unique 独占权,只需本地置脏)⇒ 4 值 `perm` 无法区分「静默变脏」⇒ per `ADR-CEP-05 §13.1` 判据 (a)(b) 成立 ⇒ `perm_upgrades.json` 加行:

```json
{ "instance_id": "chi_rnf", "from": "UNIQUE", "to": "WRITEBACK" }
```

### ⚠️ 台账 Entry #71 的 F-1 需**勘误**(append-only)

F-1 原文:「而 CHI RN-F 是**显式**请求升级(`07 号 §5.1.1`)⇒ **不需要**该演进」。

**核实结论**:F-1 的**引证失锚** —— `07 号 §5.1.1` 的 CHI 行(`docs/architecture/07-coherence-exploration-platform.md:316`)**无**任何「显式/静默升级」字样。F-1 混淆了**两条升级轴**:

| 轴 | 内容 | CHI 的行为 |
|---|---|---|
| **轴 1** | 获取独占(S→U / READ→UNIQUE) | **显式** —— ReadUnique / MakeUnique / CleanUnique 走 TXREQ |
| **轴 2** | 独占干净→脏(UC→UD / UNIQUE→WRITEBACK) | **静默** —— 零总线事务 |

⇒ **F-1 的排期结论不受影响**(perm 形态演进跟唯一真实消费者 `moesi_gpu` 走,机制已由队列 #6 交付;本 change「复用之,只填行」—— `perm_upgrades.json` 的一行**就是「行」」);要更正的只是 **F-1 的协议事实括注** ⇒ 追加**勘误 entry**(台账 append-only,原文不改)。

### ⚠️ 一手锚义务(Phase 1 的硬门)

`UC→UD` 静默是 AMBA CHI 的**既定性质**(HN-F 不跟踪 RN 侧脏状态 ⇒ 脏发现只能靠 snoop 数据响应)。仓内目前只有**间接**佐证(gem5 `CHI-cache.sm` 的 `UC`/`UD`/`SC`/`SD` 状态族,per `docs/research/gem5-ruby-protocol-bridge-survey.md:532`)⇒ **design 阶段须引 IHI0050(AMBA CHI r2,`ADR-CEP-11 §3` 的锁定锚)的一手 cache-state transition 章节作 E1 锚**;**引不到则本 change 停在 Phase 1**(不得以二手佐证冒充一手锚)。

---

## D-ε —— `sharer_tracker` 选型

**裁决:`impl = "不参与"` + 退化 caps 行**。

| 依据 | 内容 |
|---|---|
| RN-F **不维护** sharer set | sharer 由 **HN-F**(Home Node)维护 ⇒ 超出 RN-F adapter 作用域 |
| CHI 在 RN 侧是**副本携带权限**协议 | `UC`/`UD`/`SC`/`SD` 本地编码权限 —— 正是 schema 枚举描述的原话(`protocol_instance.schema.json`:`impl` 的「不参与」= 「由副本携带权限的协议 / 自失效类协议」) |
| `INV-CEP-04-IC7` 同款形态 | token 类协议 tracker 不跟踪 sharer set ⇒ 实现上等价「tracker 不参与」 |

**退化 caps 行**(对齐 `ADR-CEP-04:98` 的 `BroadcastNull` 退化行):`scope=NONE` / `completeness=EXACT` / `authority=NONE` / `aggregation_at=NONE`。

**机器断言**:`impl = "不参与"` ⇒ caps 必须等于该退化行;反向 fixture:`不参与` + `scope=SYSTEM` ⇒ 红。

**为何不选 `BroadcastNull`**:它表达「无目录 + 广播 snoop」的**系统级** tracker 选型,与 CHI 的 HN-F 目录架构**矛盾**,会误导 `ADR-CEP-13` 跨域判据的输入。

---

## D-ζ —— `ordering_assumption` / `vc_binding` / `consistency_labels`

### `vc_binding`(compute_noc)

| 消息类 | vc_id | 依据 |
|---|---|---|
| `request` | 0 | ArchForge 32 号 §6.0.2:`compute_noc` VC0 = `request` |
| `response` | 1 | 同上 VC1 = `response` |
| `snoop` | 2 | 同上 VC2 = `snoop` + `forward_data` |
| `snoop_resp` | 3 | 同上 VC3 = `snoop_resp` + `snoop_completion_ack` |

⇒ 与 `moesi_gpu` 同槽位 ⇒ 满足 **VC-R1**(请求向与响应向异槽)· 边端点已绑定(compute_noc 的 5 条边端点 = VC0~VC3,全部被绑定)。

### `ordering_assumption`(compute_noc)

**照抄 fabric 既有担保**:`{0: unordered, 1: p2p_ordered, 2: unordered, 3: p2p_ordered}` —— 与 `moesi_gpu` 一致。

**理由**:CHI 的 `TxnID` + `CompAck` 机制**存在的意义**就是容忍无序传输、由协议层保证端到端保序 ⇒ subset 无需更强的排序声明。

**判定依据**:`ordering_point_checker.hh` 的 J1 判据只要求**响应向**(`response` / `snoop_resp`)**必须** `p2p_ordered` ⇒ VC1 / VC3 = `p2p_ordered` ⇒ PASS。

### `consistency_labels`

`read = "SC"` / `write = "SC"` —— CHI 是顺序一致协议(per `ADR-CEP-12` 决策 3 的两级标签)。

### `atomic` / `address_resolve` / `flow_support`

| 字段 | 值 | 理由 |
|---|---|---|
| `atomic.supported` | `false` | subset 不实现 Atomic* |
| `flow_support.address_resolve` | `false` | home 解析走静态 SAM,无 wire query |
| `flow_support.request` / `completion` / `snoop` / `snoop_resp` / `writeback` | `true` | 5 条流均承载 |
| `flow_support.forward_data` | `true` | `SnpRespData` 携带数据(骑 snoop 槽) |
| `flow_support.back_invalidate` | `false` | RN-F 不发反向失效 |
| `flow_support.barrier` | `false` | subset 不含 barrier |
| `flow_support.snoop_completion_ack` | `true` | CompAck |

### `protocol_version` = `chi-r2` / `family` = `industrial` / `adapter` = `ChiRnfAdapter`

per `ADR-CEP-11 §3` 锁定表 + `07 号 §5.1.1` 的 adapter 名 + `protocol_instance.schema.json` 的 `family` 枚举。

---

## D-η —— 完整判据清单(9 条 = 6 实例面 + 3 权威分区面)

> **纪律**(队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**守门/闸的新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

### A. 实例面(`cep-chi-rnf-subset`,6 条)

| ID | 判据 | 反向 fixture | 正控 |
|---|---|---|---|
| **CR-01** | `chi_rnf` 实例声明符合**冻结** schema 全 13 属性;`perm` = 4 值全含;`protocol_version` = `chi-r2`;`family` = `industrial` | 删任一 required 键 ⇒ 红;`protocol_version` 写表外值 ⇒ 红 | `moesi_gpu` / `mesif` 两条既有声明仍 PASS |
| **CR-02** | `ordering_assumption` ⊆ `vc_binding`(逐网络)+ `vc_binding` 消息类 ∈ 6 类词表 + 键对称 + `vc_id < vc_count` | 破坏任一子条件 ⇒ 红(逐条) | 两条既有实例仍 PASS |
| **CR-03** | 6→3 映射表完备(每个使用中消息类恰好一行)+ 目标 ∈ 3 类值域 + 恒等须带 `identity_reason` | 删 `snoop` 行 ⇒ 红;目标写第 4 类 ⇒ 红;恒等无 reason ⇒ 红 | `moesi_gpu` 映射仍 PASS |
| **CR-04** | 6→4 `perm` 投影完备(每个稳定态有投影)+ 瞬态无投影 + `UC+Store→UD` 存在 + perm 秩 `UNIQUE→WRITEBACK` 升高 | 让瞬态有投影 ⇒ 红;改掉该转换 ⇒ 红 | `moesi_gpu` 投影仍 PASS |
| **CR-05** | `chi_rnf` 在 `perm_upgrades.json` 声明 `UNIQUE→WRITEBACK`,被引用完整性 + 单调性判据 PASS | 引用不存在的 id ⇒ 红;`UNIQUE→READ` ⇒ 红 | `moesi_gpu` 行仍 PASS |
| **CR-06** | `impl = "不参与"` ⇒ caps 必须等于退化行 | `不参与` + `scope=SYSTEM` ⇒ 红 | `moesi_gpu` 的 `SparseSnoopFilter` 行仍 PASS |

### B. 权威分区面(`cep-instance-authority-partition`,3 条)

| ID | 判据 | 反向 fixture | 正控 |
|---|---|---|---|
| **CR-07** | ⑥ 镜像闸:ArchForge 声明的实例 **12 键逐字段严格**(镜像漂移 ⇒ 红);`extra_ids − repo_owned` ⇒ 红;登记 id 撞 ArchForge id ⇒ 红 | 改任一非 `status` 键 ⇒ 红(穷举);加未登记实例 ⇒ 红;撞名 ⇒ 红 | `moesi_gpu` / `mesif` 镜像仍严格 |
| **CR-08** | ①~⑤ 语义校验**扩面**覆盖 repo-owned 实例(schema / `vc_count` / 词表 / 网络集 / 边端点绑定) | 登记实例违 schema ⇒ 红;违词表 ⇒ 红 | 两条既有实例仍 PASS |
| **CR-09** | 登记 id 与 ArchForge id **撞名** ⇒ 红(**权威迁移闸**) | 登记 `moesi_gpu` ⇒ 红 | 登记 `chi_rnf` ⇒ 绿 |

### C. 真实载体首验(Phase 6 的执行判据,详见 `tasks.md` 6.x)

| ID | 判据 | 反向 fixture | 正控 |
|---|---|---|---|
| **CR-10** | **真插件驱动**:一次 CHI 事务经 `ProtocolBridge` **穿过** JSON 装配抵达对端,且由 `ChiRnfProtocolEngine`(**非 Null**)驱动 | 引擎换成 `CohIfNullProtocolEngine` ⇒ 必红(Null **零发射**) | 引擎换成真插件 ⇒ 绿 |
| **CR-11** | 6 通道**各有**观测点事务(TXREQ/RXRSP/RXDAT/RXSNP/TXRSP/TXDAT) | 屏蔽任一通道 ⇒ 红 | 6 通道全通 ⇒ 绿 |
| **CR-12** | Credit 流控:credit 耗尽 ⇒ 停发;补 credit ⇒ 恢复 | 旁路 credit 记账 ⇒ 红 | 补 credit 恢复 ⇒ 绿 |
| **CR-13** | TxnID 管理:在途唯一 + 完成后释放可复用 | 强制在途重复 TxnID ⇒ 红 | 释放后复用 ⇒ 绿 |
| **CR-14** | CompAck:需 CompAck 的读类事务恰好发 1 个 | 抑制 CompAck ⇒ 红 | 不需 CompAck 者零发送 ⇒ 绿 |

> **CR-10 ~ CR-14 是队列 #8 相对 #7 的核心增量**。判据钉在 **Null 结构上不可能**有的行为上(`protocol_engine_base.hh:420-455`:全部 `on_*` 把参数 `(void)` 丢弃、**零发射**)⇒ 「真插件 vs Null」有**判别力**,不是「都绿」的同款断言。

---

## D-θ —— 装载层参数化(机制扩孔,**判据本体零改动**)

现状:`adapter_contracts_loader.hh:143-171` **硬编码** `MoesiGpuAdapter::` 产出 AC-04 / AC-05 的输入 ⇒ CHI 拿不到自己的 verdict。

**拆分两层**(判据实现 `adapter_schema_contracts.hh` **零改动**):

| 层 | 内容 | 判据 |
|---|---|---|
| **全局输入** | `perm` 值域 + 实例声明集 + 静默升级声明源 + 无升级能力实例集 | AC-01 / AC-02 / AC-03 |
| **逐 adapter 输入** | 消息类映射行 + 使用中消息类 + 恒等标志/理由 + 稳定态投影 + 升级转换目标态 + 权限秩 | AC-04 / AC-05 |

**签名形态**(design 期定稿,避免实施期即兴):

```cpp
// 全局(一次装载)
inline LoadedAdapterContracts load_adapter_contract_inputs(const std::string& repo_root);

// 逐 adapter(模板参数分派 —— 避免运行期字符串分发)
template <typename AdapterT>
inline void fill_adapter_inputs(AdapterSchemaContracts::Inputs& in);
```

**为什么不运行期字符串分发**:adapter 是**编译期类型**(header-only 常量)⇒ 模板分派零开销 + 编译期发现缺符号。

---

## D-ι —— 判据实现的**单一性**与**零新增 invariant**

| 项 | 结论 |
|---|---|
| 判据本体 | `adapter_schema_contracts.hh` **零改动** ⇒ 不 MODIFIES `cep-adapter-schema-contracts` main spec ⇒ 不挤占 KPI slot |
| 新增 invariant 数 | **0** ⇒ `test/test_cep_canonical_invariants.cc` **零 diff** |
| `StubCacheCore` | **零 diff** |
| `CATEGORY_*` | 仍 **3 类**(`static_assert` 钉住) |
| `perm` | 仍 **4 值**(`protocol_instance.schema.json` 冻结面,零改动) |

---

## D-κ —— 转正路径(6 步,与队列 #7 同款)

```bash
# 前置:openspec list --json 实测 CEP 内部 active == 0(队列 #7 已归档,Entry #90)✅
# 1) 删各文件顶部的「change 目录未建」标注段(状态行改 🟡 Proposed)
# 2) openspec validate cpptlm-cep-chi-rnf-subset --strict   ⇒ rc=0
# 3) 台账追加 Entry(记录转正 + KPI 0/3 → 1/3)
# 4) §4.0 行 #8 状态列 📋 Planned → 🟡 Proposed
# 5) phase-10.md 对应子任务行同步
# 6) improvement 草稿 front-matter 状态同步
```

---

## D-λ —— Phase 划分(8 周)

| Phase | 内容 | 冻结面 |
|---|---|---|
| **0** | 台账勘误 entry(F-1 双轴澄清)+ **ADR-CEP-12 Status Update**(architect)+ `07 号` §5.1.1 状态 + 冻结清单重算 | **2 件**(`ADR-CEP-12` + `07 号`)⇒ 两步授权 |
| **1** | `ChiRnfAdapter`(状态模型 + 6→4 投影 + 6→3 映射 + merge note + `identity_reason`)+ **E1 一手锚**(IHI0050) | 零 |
| **2** | 实例声明落地(`protocol_instances.json` 的 `chi_rnf` + `repo_owned_instance_ids` + `perm_upgrades.json` 加行与自述改写) | 零 |
| **3** | ⑥ 收窄 + ①~⑤ 扩面 + 3 条新反向 fixture + 装载层参数化 | 零 |
| **4** | `ChiRnfProtocolEngine`(真插件:6 通道观测点 + Credit + TxnID + CompAck) | 零 |
| **5** | JSON 装配链实测(装配期校验实测 + CC-05 ④ family 一致性) | 零 |
| **6** | 判据收口(CR-01 ~ CR-14)+ 反向 fixture 登记 + 零 diff 复核 | 零 |
| **7** | 台账 / 真源同步 / 归档 | 零 |

---

## D-μ —— 已知缺口登记(诚实,不静默放过)

| # | 缺口 | 归属 |
|---|---|---|
| 1 | **AC-04 的恒等语义**:判定按全 6 类整表 ⇒ 子集 adapter 机械判「非恒等」 | 修归**独立 change**;`#9` mesif 会复发 |
| 2 | **AC-05 无 SKIP 路径**:任何未来无静默升级的 adapter 套它必假红 | 修归**独立 change**;`#8` 不动机制 |
| 3 | **AC-03 远期无牙**:`#9` 若给 mesif 加行 ⇒ `no_upgrade_instances` 变空 ⇒ AC-03 退化 SKIP | `#9` 开工时须自带新负控见证 |
| 4 | **`⑦` 反向 fixture 的索引假设**:现有 fixture 只变异 `silent_upgrades[0]`(moesi 行)⇒ 加 chi 行后 chi 行**无 fixture 覆盖** | 本 change **补**一条变异 `[1]` 的用例 |
| 5 | **Retry / PCrdGrant 无对应流** | subset 假设「对端 HN 不 Retry」⇒ **无现有判据能发现** ⇒ 登记 |
| 6 | **CHI 状态机一手锚** | Phase 1 的硬门(见 D-δ) |

---

## D-ν —— 冻结面改动清单(2 件,走两步授权序列)

| # | 产物 | 改什么 |
|---|---|---|
| 1 | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | 追加 `## Status Update`:实例级权威分区裁决 + 追认队列 #6 的 `D-ζ` 字段级豁免 |
| 2 | `docs/architecture/07-coherence-exploration-platform.md` | §5.1.1 CHI 行状态 → 「已落地」(队列 #8)+ 口径注记 |

**序列**(per `obj §4.7.3` D2 + 队列 #7 的 Entry #89 先例):
① commit 产物 → ② `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` → ③ commit 清单。
**⚠️ 先跑 `--update` 会导致 provenance 失败** —— 必须先 commit 产物。
