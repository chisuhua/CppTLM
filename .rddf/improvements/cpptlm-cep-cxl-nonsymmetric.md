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
状态: 📋 草稿(2026-10-30 —— 队列 #10;**两段式**:第 0 个 change = 规约层 + 第 1 个 change = 实现层)
created: 2026-10-30
---

# improvement: cpptlm-cep-cxl-nonsymmetric

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 📋 草稿(2026-10-30)· **两段式**(per #9 先例:先规约层、再实现层)
> **队列**: `obj-cep-platform §4.0` **队列 #10**(`cpptlm-cep-cxl-nonsymmetric`,Stage 1 / **2029-Q1**,8 周)·
> **不新增队列行**(一个队列行可承载多个 change / capability,per 队列 #3 / #4 / #8 / #9 先例)
> **两段式拆分**(per #9 的 `cpptlm-cep-mesif-state-spec` → `cpptlm-cep-mesif-adapter` 先例):
> · **第 0 个 change** = `cpptlm-cep-cxl-nonsymmetric-state-spec`(**规约层**:状态机 + 5→4 `perm` 投影表)
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
| `ADR-CEP-04` | §2 决策 3 的 5×4 表 + `SparseSnoopFilter` 行 | `SparseSnoopFilter` 的 caps = `SYSTEM` / `CONSERVATIVE` / `GLOBAL` / **`HOME`**(聚合于 Home,Home 发起并广播 snoop)⇒ 与 ArchForge `snoop_filter.md` 的模块描述**逐字段一致** ⇒ **本次不产生「自推」语义债**(与 #9 的 `HierarchicalDirectory` 形成对比) |
| `ADR-CEP-12` | §8 能力映射 + §13 权威分区 | 实例声明 13 键 schema;`status` 是**本仓权威字段**;`authority_migrations[]` 的登记口径(CXL 的声明需在本仓立,并**镜像**到 ArchForge) |
| `ADR-CEP-02` | canonical 10 流 | CXL 2.0 非对称用其中 **D2H / H2D 各 3 通道**(Req / Rsp / Data)⇒ 流映射落到 6 类一致性消息词表 |
| `ADR-CEP-05` | `perm` 4 值 + `INV-CEP-05-IC1` | 5 态 → 4 值 `perm` 投影;**瞬态无投影**;`perm` 4 值锁不扩 |
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
2. **5 → 4 `perm` 投影表**(每态投影 + 秩单调口径);
3. **逐格依据自洽 + 偏离清单 + 统计**(与 `mesif_state_spec.hh` 同款 14 条编译期锁);
4. **跨仓追认清单生成器**(先例:`generate_mesif_recognition_checklist.sh`)+ 逐条机器核对;
5. **本仓「自推」登记**:ArchForge 无规约 ⇒ 全部逐格 `pending = true` + **跨仓追认债**登记。

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
| **性能模型 / 对拍基准 / 延迟精度** | H7 仍未验证(per Entry #108:手段已指定、**实测未做**)⇒ 本 change **不**声称任何性能结论 |
| **多版本并存 / 追最新版** | `ADR-CEP-11` 决策 5 的显式排除 |
| **collective 语义** | 非对称模式不引入(per `ADR-CEP-03` 的 `NullCollectivePlane` 可关闭口径) |
| **跨仓追认的结论侧** | 由 **ArchForge** 出具(先例:`42 号`);本仓只负责**请求侧**清单 + 收到结论后的翻转 |

---

## Why

1. **Stage 1 的最后一个协议族入口** —— `07 号 §5.1.1` 的 6 族中,MOESI / MESIF / CHI 已落地,
   **CXL 2.0 非对称**是唯一仍空着的 **Stage 1** 族;落地后 Stage 1 的协议族覆盖收口。
2. **「先规约、后实现」是 #9 换来的教训** —— #9 的 G1 / G2(ArchForge 无状态机规约 / 无投影表)迫使本仓**自推**;
   本仓用「本地推导 + 逐格依据 + 待追认标记」三件套爬出来,但代价是 **35 格跨仓追认债**。
   CXL 非对称侧**同样没有规约** ⇒ 重复一次就是重复付一次债 ⇒ **先立规约层**。
3. **R11 的洞必须在有 CXL 实例之前补上** —— `SparseSnoopFilter` 已被 `moesi_gpu` 声明占用却**无行为实现**;
   若 CXL 实例再声明一次仍无实现,则「声明面承诺的能力」连续第 2、3 次未兑现 ⇒ 平台的自证能力受损。
4. **非对称是首个「Host 侧目录 + 设备不感知其他设备」模型** —— 与已落地的 3 个实例(设备侧目录 / 分级目录 /
   不参与)都不同 ⇒ 它为 **Stage 2 的 CXL 3.0 对称(DCOH)** 提供「非对称 ⇒ 对称」的演进对照基线。
5. **版本侧零缺口** —— 锁表已有 `cxl-2.0`、通用闸已落地 ⇒ 本 change 的不确定性**集中在语义侧**(状态机与投影),
   而不在版本 / schema 侧 ⇒ 风险面比 #9 更窄、更可控。

---

## What Changes

### 第 0 个 change(规约层)

| 载体 | 内容 |
|---|---|
| `include/tlm/cep/cxl_state_spec.hh`(新) | CXL 2.0 非对称态 / 事件 / 转换表 / 5→4 `perm` 投影表 / 逐格 `basis` / 偏离清单 / 编译期锁(与 `mesif_state_spec.hh` 同款结构) |
| `scripts/test/generate_cxl_recognition_checklist.sh` + `.cc`(新) | 逐格追认清单**生成器**(+ `--check`) |
| `docs/implementation/<date>-cep-cxl-nonsymmetric-recognition-checklist.md`(新) | 追认请求清单(**由载体生成**) |
| `openspec/changes/cpptlm-cep-cxl-nonsymmetric-state-spec/`(新) | change 4 件套 + delta spec(capability `cep-cxl-nonsymmetric-state-spec`) |

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

- ❌ 不**自推**并**不**把自推写成「已裁决」—— 规约层全部逐格 `pending = true` + 跨仓债登记;
- ❌ 不扩 `perm` 4 值锁 · 不扩 `CATEGORY_*` 3 类 · 不改 `CohIfBundle` · 不改冻结 schema;
- ❌ 不新增 ABI;不新增 invariant 编号数(除非 ADR 显式新增并同步编号位)。

---

## Acceptance

- [ ] **规约层**:转换表逐格 `basis` 非空 + `kind`/`pending` 一致 + 投影表完备 + 秩口径自洽 + 偏离清单自洽
      ⇒ **编译期锁**逐条覆盖(先例:`mesif_state_spec.hh` 的 14 条);
- [ ] **规约层的跨仓追认入口**:追认清单**由载体生成**(`--check` 模式在 CI 中 rc=0)+ 逐条机器核对
      ⇒ 手改清单 ⇒ 必红;载体变更未重生成 ⇒ 必红;
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
| **4** | **规约层先行的双段式落地法** | 「规约层(逐格 basis + pending + 追认清单)→ 实现层(零副本委托)」的可复用模板(#9 验证过) |

---

## Impact

| 面 | 影响 |
|---|---|
| **队列 / KPI** | #10 `Planned` → `Proposed`;KPI 并发 **0/3 → 1/3**(规约层先占 slot,实现层排队) |
| **SharerTracker** | 行为实现 **2 → 3**;`SparseSnoopFilter` 补上行为面 ⇒ `moesi_gpu` 的声明**兑现** |
| **协议实例** | `implemented` 实例 **3 → 4**;`protocol_version` 新增 `cxl-2.0`(锁表**已有**,无需改锁表) |
| **冻结面** | 预计触碰 `07 号`(§5.1.1 CXL 行 + §11 映射表)+ `ADR-CEP-04`(第 3 个行为实现)+ `ADR-CEP-11`(若锁表需微调)⇒ **两步授权** |
| **跨仓债** | 新增:**CXL 实例声明镜像**(本仓首立 ⇒ 须镜像回 ArchForge `32 号 §6.1`)+ **规约逐格追认**(与 #9 的 35 格同型) |
| **残余风险(不得写成「已消除」)** | ① **R-1 型**:按档批量证据强度弱于逐格 ② **规约自推**(ArchForge 无 CXL 规约 ⇒ 全表 `pending`)③ **R11 降级仅在 `SparseSnoopFilter` 行为落地后成立** ④ **H7 未验证** ⇒ 本 change **不**产出性能结论 |
| **对后续队列的影响** | #12(`cxl-dcoh-symmetric`)可**直接复用**本 change 的规约层与 `SparseSnoopFilter` ⇒ 非对称 → 对称的差异面被**隔离**在 #12 |

---

## 维护记录

| 日期 | 变更 |
|---|---|
| 2026-10-30 | 初版(planner;brainstorm 已批准 —— 按 #9 先例拆 **2 个 change**):ArchForge 侧**无 CXL 规约**的缺口普查结论 + 两段式范围 + 判据口径 |
