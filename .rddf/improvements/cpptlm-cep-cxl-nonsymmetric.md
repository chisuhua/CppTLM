---
name: cpptlm-cep-cxl-nonsymmetric
priority: P2
phase: phase-10
依赖:
  - ADR-CEP-11
  - ADR-CEP-04
  - ADR-CEP-12
  - ADR-CEP-02
  - ADR-CEP-05
  - ADR-CEP-03
  - ADR-CEP-17
状态: ✅ **已完成并归档**(2026-11-01,per **Entry #127**(规约层)/ **#128** / **#132**(实现层)—— 队列 #10;**两段式**:第 0 个 change(规约层)= ✅ **已完成归档**(`cpptlm-cep-cxl-nonsymmetric-state-spec` ⇒ main spec `openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`)+ 第 1 个 change(实现层)= ✅ **已完成归档**(`cpptlm-cep-cxl-nonsymmetric` ⇒ main specs `cep-cxl-nonsymmetric` / `cep-sparse-snoop-filter`;KPI 0/3 → 1/3 → 0/3) —— ⚠️ **change 完成 ≠ 族级达成**:本族在 `B3b-2` 的**族级**状态为 **⛔ 显式冻结**(68 格中 42 格 Unknown,覆盖率 26/68,per `07 号` SU · 四 D-3)⇒ ⛔ **不得**读成「CXL 2.0 族已达成」)
created: 2026-10-30
---

# improvement: cpptlm-cep-cxl-nonsymmetric

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 📋 草稿(2026-10-30)· **两段式**(per #9 先例:先规约层、再实现层)
> **队列**: `obj-cep-platform §4.0` **队列 #10**(`cpptlm-cep-cxl-nonsymmetric`,Stage 1 / **2029-Q1**,8 周)·
> **不新增队列行**(一个队列行可承载多个 change / capability,per 队列 #3 / #4 / #8 / #9 先例)
> **两段式拆分**(per #9 的 `cpptlm-cep-mesif-state-spec` → `cpptlm-cep-mesif-adapter` 先例):
> · **第 0 个 change** = `cpptlm-cep-cxl-nonsymmetric-state-spec`(**规约层**:状态机 + **N→4** `perm` 投影表;N 由**公开来源普查**钉死 —— 公开来源实证 N=4 ⇒ MESI 4 态 ⇒ **4→4**,**无 `F` 行**)
> · **第 1 个 change** = `cpptlm-cep-cxl-nonsymmetric`(**实现层**:`CxlCacheAdapter` + `SparseSnoopFilter` 行为实现 + 实例声明 + 装配)
> **为何必须拆**:ArchForge 侧**无 CXL 2.0 非对称的状态机规约与投影表**(实测:ArchForge 全仓无 CXL 协议状态转换表;
> `32 号 §6.1` 只有 `moesi_gpu` / `mesif` 两份实例声明)⇒ 与 #9 的 G1 / G2 **同型缺口** ⇒ 不先立规约层,
> 实现层必然**自推**(重蹈 #9 的「看似完整实则杜撰」覆辙)。
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1,per `obj §5.4`;当前实测 **0/3**)
> **估时**: 规约层 4 周 + 实现层 8 周(共 12 周;**串行** —— 实现层以规约层为前置)

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-11` | §3 规范版本锁定表 | 锁表**已含 `cxl-2.0`**(CXL 2.0 非对称)⇒ **版本锁定无缺口**;`INV-CEP-11-IC1` 的通用闸已于 Entry #109 落地 ⇒ 实例声明的 `protocol_version` 落表内即可过闸 |
| `ADR-CEP-04` | §2 决策 3 的 5×4 表 + `SparseSnoopFilter` 行 | `SparseSnoopFilter` 的 caps = `SYSTEM` / `CONSERVATIVE` / `GLOBAL` / **`HOME`**(聚合于 Home,Home 发起并广播 snoop)⇒ 与 ArchForge `snoop_filter.md` §0.2 的 caps **逐字段一致**(机器可校)⇒ ⚠️ **但该锚只锚 4 个枚举值,不锚事件集合 / 状态机 / 更新时机** ⇒ **不得**外推为「本次不产生自推语义债」(per Entry #122 ⑥⑤) |
| `ADR-CEP-12` | §8 能力映射 + §13 权威分区 | 实例声明 13 键 schema;`status` 是**本仓权威字段**;`authority_migrations[]` 的登记口径(CXL 的声明在**本仓首立**)⇒ ⚠️ **不镜像进 ArchForge `32 号 §6.1`**(那是**产品侧**实例注册表 ⇒ 镜像 = 污染 + 与 ArchForge「GPU Die 无 CXL」立法冲突;per Entry #122 ①-e) |
| `ADR-CEP-02` | canonical 10 流 | CXL 2.0 非对称用其中 **D2H / H2D 各 3 通道**(Req / Rsp / Data)⇒ 流映射落到 6 类一致性消息词表 |
| `ADR-CEP-05` | `perm` 4 值 + `INV-CEP-05-IC1` | **N → 4** `perm` 投影(**N 由普查钉死**;公开来源实证 = **MESI 4 态** ⇒ N=4 ⇒ **4→4**);**瞬态无投影**;`perm` 4 值锁不扩;⚠️ 「**5→4**」是 MESIF 模板形状的未审视继承(MESIF 有 `F` 态)CXL **无 `F` 态** ⇒ `F→WRITEBACK` 行**整行删除** |
| `ADR-CEP-03` | Collective Plane 分层 | CXL 配置下 Collective Plane 绑 `NullCollectivePlane` 可关闭(per `07 号` §2.2)⇒ **非对称模式不引入 collective 语义** |
| `ADR-CEP-17` | 协议族接入 | 「adapter 数 = 协议实例数」口径;CXL 2.0 非对称 = **1 个实例**(与 CXL 3.0 对称的**同一 adapter 的另一种模式**,但实现阶段是 2 个 change) |
| ArchForge `snoop_filter.md` | §0.2 | Host Snoop Filter 模块的 caps 与 `SparseSnoopFilter` **基准值一致** ⇒ 本仓实现须与该锚点**逐字段相等**(跨仓锚点,机器可校) |
| ArchForge `32 号` | §6.1 / §6.2 | **只有 2 份实例声明**(`moesi_gpu` / `mesif`)⇒ CXL 实例声明**在本仓首立** ⇒ 触发镜像与追认义务 |
| 台账 Entry #107 | §8.1 **R11** | `SparseSnoopFilter` **已被 `moesi_gpu` 声明占用**却**仅声明层、无行为实现** ⇒ 本 change 是**该缺口的收口点**(不得让第 4 个实例再复制「声明超前」) |

---

## 范围

### In Scope

**第 0 个 change —— 规约层(`cpptlm-cep-cxl-nonsymmetric-state-spec`)**

1. **CXL 2.0 非对称状态机规约**:态集合 + 事件集合 + **转换表**(逐格 `basis{kind, pending, ref, note}`);
2. **N → 4 `perm` 投影表**(每态投影 + 秩单调口径;**N 由普查钉死**;公开来源实证 N=4 ⇒ 4→4);
3. **逐格依据自洽 + 偏离清单 + 统计** + 编译期锁(与 `mesif_state_spec.hh` **同款结构**;
   ⚠️ **锁的条数由规约内容决定**,不由 #9 的「14 条」配额决定);
4. **本仓锚点摘要的逐格核对表生成器**(+ `--check`,手改必红)+ **仓内**二方复核流程
   (先例形态参照 `generate_mesif_recognition_checklist.sh`,但**对象不同**:非「发给 ArchForge 的请求件」);
5. **`basis.kind` 值域先定** —— `public-direct` / `public-analogical` / `archforge-anchor`(仅 caps 等**已对齐**格)/
   `self-derived`;**pending 策略随 kind 分布定**(不是「全部逐格 `pending = true`」);
   ⇒ **禁止**把 `analogical` / `inferred` 升格为 `direct`。
6. **「非对称」可判定义段**(三维度逐条挂引用):**目录侧**(Host 侧 Home/SF 聚合)· **snoop 发起侧**
   (**仅** Host/Home 发起;H2D Req 只含 snoop 操作码)· **设备态集**(MESI 4 态;设备**无** snoop **发起**型事件);
   ⚠️ 精确表述 = **host-centric / 单一一致性点**(非「设备互不感知」—— 2.0 单设备下平凡成立)。

**第 1 个 change —— 实现层(`cpptlm-cep-cxl-nonsymmetric`)**

6. **`CxlCacheAdapter`(非对称模式)**:**零自有表副本**(委托消费规约载体,与 `MesifAdapter` 同款形态);
7. **`SparseSnoopFilter` 行为实现**:类 + 目录安装面 + 聚合于 `HOME` 的查询面
   ⇒ **补 R11 的洞**(此前该实现只有 traits 行);
8. **实例声明落地**:`protocol_instances.json` 增 CXL 2.0 非对称实例(13 键 · `protocol_version = cxl-2.0` ·
   `sharer_tracker.impl = SparseSnoopFilter`)+ `vc_binding` / `ordering_assumption` / `perm` / `atomic` / `consistency_labels`;
9. **装配实测**:最小装配 JSON + 注册行 + 一次事务**穿过**引擎抵达对端 + **4 实例共存**下通道依赖闸无环;
10. **判据与反向 fixture**:契约测试 + 「因正确原因红」机器化(声明期望失败模式 + 判据 id 归因)。

### Out Scope

| 排除项 | 理由 |
|---|---|
| **CXL 3.0 对称**(DCOH / BISnp / BIRsp / DPA 归属) | 属队列 **#12** `cpptlm-cep-cxl-dcoh-symmetric`(Stage 2)· 非对称模式**不**预支 |
| **CXL Fabric Manager / Fabric Coherency Engine** | ArchForge `32 号` 明示「**不在 CEP 范围**(属控制面,非一致性语义)」 |
| **性能模型 / 对拍基准 / 延迟精度** | H7 已实测但**结论带范围限定**(per Entry #120:「在【声明范围:均匀随机负载】内成立」;非均匀负载上**估算规则 R-B 失效**(误差 15.5~72.3pp))⇒ 本 change **不**声称任何性能结论;⚠️ **CXL 非对称的 workload 结构性非均匀 ⇒ 恰好落在 R-B 失效区** ⇒ **禁止**附着任何延迟/性能读数 |
| **多版本并存 / 追最新版** | `ADR-CEP-11` 决策 5 的显式排除 |
| **collective 语义** | 非对称模式不引入(per `ADR-CEP-03` 的 `NullCollectivePlane` 可关闭口径) |
| **跨仓追认的结论侧** | ⚠️ **本 change 不设**「ArchForge 出具追认结论」这一项(per Entry #122 ①-c/①-d):CXL 2.0 是**公开标准**、ArchForge **明确立法不做 CXL 栈** ⇒ 它**既无追认义务也无追认依据** ⇒ 追认 = **本仓锚点摘要的条目映射二方复核**(仓内闭环);**规范正文相关**的 8 项残余风险走「**人类成员条款摘要**」流程 |

---

## Why

1. **Stage 1 的协议实例覆盖** —— `07 号 §5.1.1` 的 6 族中,MOESI / MESIF / CHI 已落地,
   **CXL 2.0 非对称**是 Stage 1 的第 4 个**协议实例**;
   ⚠️ **Stage 1 仍有 TileLink-C 空着**(队列 #11)⇒ 本 change **不**构成「Stage 1 协议族覆盖收口」
   (**原表述「唯一仍空着的 Stage 1 族」有事实错误**,per Entry #122 ⑥①)。
2. **「先规约、后实现」是 #9 换来的教训** —— #9 的 G1 / G2(ArchForge 无状态机规约 / 无投影表)迫使本仓**自推**,
   靠「本地推导 + 逐格依据 + 待追认标记」三件套爬出来,代价是 **35 格跨仓追认债**
   (且**残余「按档批量证据强度弱于逐格」至今挂着**,per `obj §4.0` #9 行自认)。
   ⚠️ **但 CXL 侧的「债」形态不同**(per Entry #122 ①-c/①-d):CXL 2.0 是**公开标准**
   ⇒ basis 可引**公开来源**;而 ArchForge **明确立法不做 CXL 栈**(`ADR-SOC-21` D2)
   ⇒ 它对 CXL **既无追认义务也无追认依据** ⇒ **「全表 pending + 跨仓追认」在 CXL 侧必然永久挂账**
   ⇒ 仍须**先立规约层**(缺的是**本仓可执行规约载体**,不是 ArchForge 规约),但**追认形态换为仓内闭环**。
3. **R11 的洞必须在有 CXL 实例之前补上** —— `SparseSnoopFilter` 已被 `moesi_gpu` 声明占用却**无行为实现**;
   若 CXL 实例再声明一次仍无实现,则「声明面承诺的能力」连续第 2、3 次未兑现 ⇒ 平台的自证能力受损。
4. **非对称是首个「Host 单一一致性点」模型** —— 与已落地的 3 个实例(设备侧目录 / 分级目录 / 不参与)都不同
   ⇒ 为 **Stage 2** 提供演进对照。
   ⚠️ **两处口径修正**(per Entry #122 ①-a/E-5):①「对称」指 **HDM-DB / CXL.mem 的多 host 共享内存**,
   **不是** CXL.cache 变对称 ⇒ **CXL.cache 全代次都非对称**;②「设备不感知其他设备」在 **CXL 2.0 是平凡成立**
   (每 root port **只有 1 个** cache device,3.0 才最多 16 个)⇒ 精确表述 = **host-centric / 单一一致性点**。
5. **版本侧零缺口** —— 锁表已有 `cxl-2.0`、通用闸已落地 ⇒ 本 change 的不确定性**集中在语义侧**(状态机与投影),
   而不在版本 / schema 侧 ⇒ 风险面比 #9 更窄、更可控。

---

## What Changes

### 第 0 个 change(规约层)

| 载体 | 内容 |
|---|---|
| **Task 0(硬门)**:公开来源普查 | 态 / 事件词表枚举 + 逐格「公开来源有明示 vs 须推导」覆盖矩阵 ⇒ **产出锚点摘要骨架**(`docs/research/`,**不整段复制任何原文**);⚠️ **普查未完成 ⇒ change 目录不建**;公开来源钉不死的格 ⇒ **回退裁决** + 不进入编码 |
| `include/tlm/cep/cxl_state_spec.hh`(新) | CXL 2.0 非对称态 / 事件 / 转换表 / **N→4** `perm` 投影表(N 由普查钉死)/ 逐格 `basis{kind, pending, ref, note}` / 偏离清单 / 编译期锁(与 `mesif_state_spec.hh` 同款结构)+ **非对称结构级断言**(remote 事件集 ⊆ H2D snoop 集 ∧ local∩remote = ∅ ∧ 设备侧无 snoop 发起型事件) |
| `scripts/test/generate_cxl_anchor_checklist.sh` + `.cc`(新) | **本仓锚点摘要的逐格核对表生成器**(+ `--check`,手改必红) |
| `docs/research/<date>-cep-cxl-nonsymmetric-anchor-summary.md`(新) | **公开来源锚点摘要**(条款/文献级定位 + 强度三档 `direct`/`analogical`/`inferred` + **钉版信息**:arXiv 版本 + DOI / PDF sha256 / 文档号 / 博客抓取日期)⇒ **本仓 basis 的唯一人工审计面** |
| ~~`openspec/changes/cpptlm-cep-cxl-nonsymmetric-state-spec/`~~ → `openspec/changes/archive/2026-11-01-cpptlm-cep-cxl-nonsymmetric-state-spec/` | ✅ **已完成归档**(2026-11-01,Entry #127)⇒ 活路径 = `openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`(capability `cep-cxl-nonsymmetric-state-spec`;归档件**不入库**,per `.gitignore`) |

### 第 1 个 change(实现层)

| 载体 | 内容 |
|---|---|
| `include/tlm/cep/cxl_cache_adapter.hh`(新) | `CxlCacheAdapter`:零自有表副本,委托消费规约载体;6→3 消息类映射 + 恒等/非恒等**显式登记** |
| `include/tlm/cep/sharer_tracker.hh`(改) | 新增 `SparseSnoopFilter` **class**(行为实现;聚合于 `HOME`)⇒ **R11 收口**;`SnoopMechanics` **复用**(禁双实现) |
| `include/tlm/cep/protocol_instances.json`(改) | 增 CXL 2.0 非对称实例(13 键);`status` 由 `planned` → `implemented`(**与计数同步同批**,per `INV-CEP-12-IC3`) |
| `include/tlm/cep/perm_upgrades.json`(改) | 仅当规约层确有静默升级路径时才增行;**先删掉** `current_state` 里任何事实性错误 |
| `configs/cep_cxl_pe_minimal.json`(新)+ `include/chstream_register.hh`(改) | 最小装配 + 注册行(若需专用引擎;否则复用 `CohIfProtocolEngine` 形态) |
| `test/test_cep_cxl_adapter.cc` / `test_cep_cxl_instance_decl.cc` 等(新) | 契约测试 + 反向 fixture |
| `scripts/test/cep_cxl_reverse_verify.sh`(新) | 「因正确原因红」机器化(声明期望失败模式 + 判据 id 归因) |

### 明确不做(本 change 内)

- ❌ 不**自推**并**不**把自推写成「已裁决」—— `basis.kind = self-derived` 的格**必须** `pending = true`;
  ⚠️ 但**不**再「全部逐格 pending」+「跨仓追认债」(追认形态已改,per Entry #122 ①-c);
- ❌ **禁止**把 CXL 规范正文投喂任何 AI(ECA §3 + AI Acceptable Use Policy 覆盖 past/current/future 规范);
- ❌ **不镜像** CXL 实例声明进 ArchForge `32 号 §6.1`(产品侧注册表);
- ❌ **不写死**态数(普查前只写「N→4」);⚠️ 公开来源实证 N=4 ⇒ `F` 行不存在 ⇒ **`F→WRITEBACK` 行不保留**;
- ❌ 不扩 `perm` 4 值锁 · 不扩 `CATEGORY_*` 3 类 · 不改 `CohIfBundle` · 不改冻结 schema;
- ❌ 不新增 ABI;不新增 invariant 编号数(除非 ADR 显式新增并同步编号位)。

---

## Acceptance

- [ ] **规约层**:转换表逐格 `basis` 非空 + `kind`/`pending` 一致 + 投影表完备 + 秩口径自洽 + 偏离清单自洽
      ⇒ **编译期锁**逐条覆盖(先例形态:`mesif_state_spec.hh`;⚠️ **条数由内容决定**,不由 #9 配额决定);
- [ ] **defining 语义(非对称)可判**(per Entry #122 ⑤):① `H2DReqOp ⊆ {SnpInv, SnpData, SnpCurr}`
      ∧ `D2HReqOp ∩ H2DReqOp = ∅` ∧ 设备侧**无** snoop **发起**型事件(编译期结构断言);
      ② **负向**:设备节点**收不到**指向其他设备的 snoop + 设备侧视图**不含**其他设备 sharer
      ⇒ ⚠️ 现有 AC **无一条测语义** ⇒ 此条**不可省**;
- [ ] **协议序列正向**(per Entry #122 ⑤):设备 read miss → SF lookup(命中/未命中)→ Home 广播 snoop →
      `snoop_resp` 聚合 → `completion` 携带**正确 `perm` 值**返回(**全流程**,非「穿过引擎抵达对端」装配级);
- [ ] **hazard 契约继承**:`ADR-CEP-04 §4` 已冻结 `LookupHazard{NONE, STALL, CHAIN}` + 5 种实现的 hazard 行为表
      ⇒ 实现层**不得**交付「caps 全对、hazard 语义不对」的类;
- [ ] **`moesi_gpu` 回归**:traits 行 → 真行为实现会**改变运行时行为**(`CONSERVATIVE` ⇒ snoop 数变多)
      ⇒ 须「既有测试 **0 回归**」或「行为变化**显式登记** + 重基线」;
- [ ] **规约层的 basis 审计入口**:**本仓锚点摘要**(公开来源 · 钉版 · 强度三档)的**逐格核对表由载体生成**
      (`--check` 模式在 CI 中 rc=0)+ 逐条机器核对
      ⇒ 手改核对表 ⇒ 必红;载体变更未重生成 ⇒ 必红;摘要与载体不一致 ⇒ 必红;
      ⇒ ⚠️ **不是**「发给 ArchForge 的追认请求件」(per Entry #122 ①-c:追认 = **仓内**二方复核闭环);
- [ ] **adapter 零自有表副本**:去注释后**零**状态/事件/`perm` 字面量 ⇒ **逐格 pass-through** 与规约载体**全等**
      (反向 fixture:在 adapter 内伪造一份表 ⇒ 必红);
- [ ] **`SparseSnoopFilter` 三方一致**:`caps` / `behavior` 在 **traits 行 ↔ class 成员 ↔ ArchForge `snoop_filter.md` 锚点**
      **逐字段相等**(`SYSTEM` / `CONSERVATIVE` / `GLOBAL` / `HOME`);聚合于 `HOME` **可观测**(根级视图 ⊇ 级内视图,
      且存在**严格更窄**的级内视图 ⇒ 不是单级克隆);snoop 由 Home 发起并广播的行为**有判据**;
- [ ] **R11 收口**:`SparseSnoopFilter` 由**仅声明层**升级为**行为实现** ⇒ `moesi_gpu` 的既有声明**不再指向空实现**
      ⇒ 台账 §8.1 R11 的降级条件满足并更新;
- [ ] **实例声明**:13 键齐备 + `protocol_version = cxl-2.0` 过 **IC1 通用闸**(Entry #109 落地的那条)
      + `vc_binding` / `ordering_assumption` 过 `INV-CEP-12-IC2` 三条件 + `status` 翻转**与 `implemented` 计数同批**
      ⇒ 「JSON 说 implemented、文档说 N 个」的**无闸窗口**为 0;
- [ ] **装配实测**:最小装配 JSON 可实例化 + 一次事务**穿过**引擎抵达对端 + **4 实例共存**(含 `moesi_gpu` / `mesif` /
      `chi_rnf` / CXL)下通道依赖闸**无环** + 逐 `(instance, network, vc)` 标注齐备;
- [ ] **反向 fixture 机器化**:每条 fixture 声明**期望失败模式**(`compile-lock` / `runtime-predicate`)且命中**指定的判据 id**
      ⇒ 「因正确原因红」有机器判定(先例:`cep_mesif_adapter_reverse_verify.sh`);
- [ ] **门禁**:13 项 CEP 脚本 + `docs_sync --strict` + `openspec validate --strict` + `format --check` 全 rc=0;
      `ctest -R cep` 全绿;全仓 `cpptlm_tests` 全绿;M1 冻结面按**两步授权**执行(commit 产物 → `--update` → commit 清单)。

---

## Capabilities

| # | 能力 | 说明 |
|---|---|---|
| **1** | **CXL 2.0 非对称协议族接入** | 第 4 个 `status = implemented` 实例(Stage 1 的最后一个协议族)· D2H / H2D 各 3 通道 |
| **2** | **`SparseSnoopFilter` 行为实现** | 第 3 个**行为**实现(继 `FullMapDirectory` / `HierarchicalDirectory` 之后)· 聚合于 `HOME` · **收口 R11** |
| **3** | **非对称目录模型** | Host 侧持有 sharer 视图 + Home 发起并广播 snoop;设备**不感知**其他设备 ⇒ 为 Stage 2 DCOH 对称提供演进对照 |
| **4** | **规约层先行的双段式落地法** | 「规约层(逐格 basis + 锚点摘要 + 仓内复核)→ 实现层(零副本委托)」的落地形态 —— ⚠️ **#9 只验证了「两段式」本身**,**不**验证其「跨仓追认」环节在 CXL 语境下可用(该环节已改判,per Entry #122 ①-c)⇒ 勿当「已验证的可复用模板」 |

---

## Impact

| 面 | 影响 |
|---|---|
| **队列 / KPI** | #10 `Planned` → `Proposed`;KPI 并发 **0/3 → 1/3 → 0/3**(规约层先占 slot → **已归档释放**;实现层排队,启动时再占 1 slot) |
| **SharerTracker** | 行为实现 **2 → 3**;`SparseSnoopFilter` 补上行为面 ⇒ `moesi_gpu` 的声明**兑现** |
| **协议实例** | `implemented` 实例 **3 → 4**;`protocol_version` 新增 `cxl-2.0`(锁表**已有**,无需改锁表) |
| **冻结面** | 预计触碰 `07 号`(§5.1.1 CXL 行 + §11 映射表)+ `ADR-CEP-04`(第 3 个行为实现)⇒ **两步授权**;**⚠️ `07 号 §5.1.1` 的编辑清单 SHALL 显式含「对齐 `ADR-CEP-11` SU ③⑦ 的措辞」**(实测:该行现写「设备不感知其他设备」与「CXL 3.0 对称」⇒ 前者在 2.0 **平凡成立**、后者按裁决 ⑦ 指 **HDM-DB/CXL.mem** 而非 CXL.cache ⇒ 现为**真源 1 vs 真源 3 措辞冲突**,per oracle 复核 2026-11-01;⚠️ **不**单独立即走冻结面授权 —— 收益不抵成本,随本次实现层编辑一并执行);⚠️ **`ADR-CEP-11` 锁表不改**(实测:`ADR-CEP-11 决策 4` 的路径判定**不触发** ⇒ 改锁表反而多付一次冻结面授权)⇒ 队列 #10 的口径**以 `ADR-CEP-11` 的 Status Update 段登记**(per Entry #122 的授权记录) |
| **跨仓债** | ⚠️ **与初版相反**(per Entry #122 ①-c/①-d/①-e):**不镜像** CXL 实例声明进 ArchForge `32 号 §6.1`(产品侧注册表 ⇒ 污染 + 与其「GPU Die 无 CXL」立法冲突)⇒ 跨仓至多一条**定位注记**;**不设**「与 #9 的 35 格同型」的逐格跨仓追认 ⇒ **无 #9 式异步跨仓往返** ⇒ 风险面实质下降 |
| **残余风险(不得写成「已消除」)** | ① **R-1 型**:按档批量证据强度弱于逐格 ② **规范正文不可得**:CXL 规范受 ECA §3 + AI AUP 约束 ⇒ **AI 不得处理规范正文** ⇒ 8 项语义(见 Entry #122 ③)**只能靠人类成员条款摘要** ③ **公开来源的强度天花板**:「SF 宁多探不漏探」的判定规则无公开 CXL 原文(只能类比 CHI/CCI)⇒ 不得标 `direct` ④ **ASPLOS'25 的 perfect-tracking 假设** ⇒ 不得当作 `SparseSnoopFilter`(`CONSERVATIVE`)的**精度**依据,只可作**协议骨架** ⑤ **R11 降级须「行为落地 ∧ 补偿路径有判据」双条件**,且 ⚠️ **补偿是「保守补偿」而非「等价恢复」**(`SparseSnoopFilter` = `CONSERVATIVE`:宁多探不漏探 ⇒ 只能给出「**可能**还有其他 sharer」的保守近似,**不能**精确恢复 `O` 态携带的「没有其他 sharer」信息)⇒ V-2 的判据 SHALL 判**保守补偿**语义,SHALL NOT 声称等价恢复(否则实现层会过度声称) ⑥ **R-B 失效区**:CXL 非对称 workload 落在 H7 声明范围**之外** ⇒ 本 change **不产出**任何性能结论 ⑦ **「设备互不感知」在 2.0 平凡成立** ⇒ 精确表述为 host-centric |
| **对后续队列的影响** | #12(`cxl-dcoh-symmetric`)可**直接复用**本 change 的规约层与 `SparseSnoopFilter` ⇒ 非对称 → 对称的差异面被**隔离**在 #12 |

---

## 维护记录

> **2026-11-01 修订(per 台账 Entry #122)** —— 依据 = **oracle 5 裁决** + **metis 6 必须改** + **librarian 18 公开来源调研** + **合规核实**(CXL ECA §3 / AI AUP)
> ⇒ **10 大项 / 16 个编辑点**已落:
> ① Why #1 事实错误(TileLink-C 也是 Stage 1)② 追认机制改判(ArchForge 无追认依据 ⇒ 仓内闭环)
> ③ Why #4 两处口径修正(「3.0 对称」指 HDM-DB;CXL.cache 全代次非对称 / 2.0 单设备下平凡成立)
> ④ 「5→4」→「N→4」+ 删 `F` 行 ⑤ caps 一致 ≠ 无自推债 ⑥ 删镜像义务 ⑦ 锁数由内容决定
> ⑧ 追认清单 → **本仓锚点摘要核对表** ⑨ `basis.kind` 值域先定 ⑩ 非对称可判定义段(3 维度)
> ⑪ Task 0 **公开来源普查硬门**(普查未完成 ⇒ change 目录不建) ⑫ 补 4 条 AC(defining 语义 / 协议序列 / hazard / moesi_gpu 回归)
> ⑬ R-B 失效区登记 ⑭ Capabilities #4 降调 ⑮ Impact 跨仓债两行改写 + 删 `ADR-CEP-11` 微调预留 ⑯ 合规禁令入「明确不做」
> ⚠️ **3 处是三方清单之外、实测击穿点**:#4「5→4」的模板形状继承 / #3「3.0 对称」的语义滑移 / 「设备互不感知」的平凡成立
> ⇒ **估时**:12 周**不变但重分配**(规约层 4 周 = **1 周公开来源普查硬门** + 3 周载体/摘要/生成器/编译期锁)

| 日期 | 变更 |
|---|---|
| 2026-10-30 | 初版(planner;brainstorm 已批准 —— 按 #9 先例拆 **2 个 change**):ArchForge 侧**无 CXL 规约**的缺口普查结论 + 两段式范围 + 判据口径 |
