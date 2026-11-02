# Design — cpptlm-cep-cxl-dcoh-symmetric

> **口径**: 台账 **Entry #159**(规约层归档)· **#154**(4 项 architect 裁决)· **#153**(Task 0 普查)
> **规约载体**: `include/tlm/cep/cxl_dcoh_state_spec.hh`(**已归档**,只读消费)
> **范围**: **1 个新增 adapter** + **1 个新增 tracker 行为类** + **1 个实例声明** —— **零 ADR 正文改动 / 零既有载体改动**

---

## 1. 载体形态定稿(Phase 1 的产出 —— 本节即定稿件)

### 1.1 adapter 的**零副本**形态(禁词表 + 正控)

| 面 | 内容 |
|---|---|
| **类名** | `CxlDcohAdapter`(header-only · **独立** · 不进 `ModuleFactory` · 不注册 23 ABI) |
| **别名(允许)** | `using DevCacheState = CxlDcohStateSpec::DevCacheState;` 等 —— **类型名**允许 |
| **禁词(成员字面量)** | `DevCacheState::` · `DcohViewState::` · `HomeDirState::` · `CxlMemChannel::` · `CxlCacheChannel::` · `ChannelDirection::` · `Perm::` |
| **禁词(表名)** | `kPermProjection` · `kDeviceInitiatedSnoopSet` · `kCxlMemChannelDirection` · `kBasisDevCacheState` · `kBasisDcohViewState` · `kBasisHomeDirState` · `kBasisCxlMemChannel` · `kBasisCxlCacheChannel` · `kBasisPermProjection` · `kBasisSymmetricPredicate` · `kPendingSemanticItems` —— ⚠️ **本 adapter SHALL NOT 暴露任何「表」**(通道词表 / 方向表 / 投影表 / basis 表)⇒ **它连「引用载体表名」都不做** ⇒ 这既满足**零副本**语义,也让零副本**静态可判**(否则「引用」与「复制」在文本上**不可分辨** ⇒ 判据失效) |
| **判据形态** | **去注释后**静态扫描 + **配正控**(同机制必须能发现**确实存在**的标识符)⇒ 纯负向断言无正控 = 判据可能没生效 |
| **⚠️ 与队列 #10 / #11 的差异** | 那两族的成员名是 `State` / `Event` / `Perm`;本族有 **3 套状态空间** ⇒ 禁词表**必须按本族名字**列,不能照抄 |
| **⚠️ 与队列 #10 / #11 的差异 ②** | 命名空间是 `tlm::cep::cxl_dcoh`(不是 `CxlDcohStateSpec` 之类**类**名)⇒ 别名写作 `cxl_dcoh::DevCacheState`;⚠️ **命名空间不能作 type alias** ⇒ 测试侧用 `namespace Spec = tlm::cep::cxl_dcoh;` |

### 1.2 委托契约(adapter **只**调访问器)

| 面 | 委托对象 | 形式 |
|---|---|---|
| 基数常量 | `kDevCacheStateCount` / `kDcohViewStateCount` / `kHomeDirStateCount` / `kCxlMemChannelCount` / `kCxlCacheChannelCount` | `static constexpr` **别名**载体常量 |
| `perm` 投影 | 载体的 `perm_of` | **委托调用**(返回类型 `Perm` 是**类型名** ⇒ 允许) |
| 对称判别式 | 载体的 `is_cxl3_symmetric` | **委托** |
| ⛔ **通道词表 / 方向表 / snoop 集合** | —— | **刻意不暴露**(见 §1.1「禁词(表名)」行)⇒ 只暴露**标量** |

### 1.2bis ⚠️ **实现期发现**:「引用载体表」与「自有表」在**文本上不可分辨**

原禁词表把**载体表名**也列入禁词 ⇒ 但「**委托**读载体的表」是**合法**的 ⇒ 若允许引用,
则静态扫描**无法**区分「引用」与「复制」⇒ 判据失效。
⇒ **解法**:adapter **连引用都不做** —— 只暴露**标量**(5 个基数 + `perm_of` + 判别式 + 2 个名字常量)
⇒ 零副本既是**语义**要求,也是**静态可判**的要求。(载体侧仍持全部表。)

### 1.3 ⛔ adapter **不暴露** transition API(CDX-03)

规约载体**没有转移表**(对称模式的转移语义在公开来源里**没有** ⇒ 转移表会要求**推理填空** ⇒ 禁止)
⇒ adapter **不得**提供 `transition_*` ⇒ **否则等于自造转移表**。

### 1.4 `DcohSymmetric` 行为实现的**边界**

| caps 字段 | 值 | 行为侧约束 |
|---|---|---|
| `scope` | `LOCAL_MEMORY_REGION` | lookup **只**在本区域(HDM)内 |
| `completeness` | `CONSERVATIVE` | **宁多探不漏探** ⇒ 保守近似,**不得**声称精确 |
| `authority` | `LOCAL_HINT` | 答案**只是 hint** ⇒ **不可当权威** ⇒ `LocalView` **零改动** ⇒ **双重语义登记**(CDX-05) |
| `aggregation_at` | `HOME` | 聚合点**恒为 home**(per Entry #154 ①) |
| `hazard` | `STALL` | 同址并发 lookup ⇒ 返 `STALL`(不是 NONE / 不是 CHAIN) |
| `broadcast_fallback` | `true` | ⚠️ **公开来源未见依据** ⇒ 按 traits 实现 + **登记 pending**;**不改 traits**(改需另一次裁决) |

### 1.5 `AC-05` / `perm_upgrades.json` 的处置

| 面 | 裁决 | 理由 |
|---|---|---|
| **`AC-05`** | **复用**既有族无关化 ⇒ **零改动** | 本族**无** unique-clean 态 ⇒ 走 **SKIP**(不是 FAIL,不得记 PASS)—— 与队列 #11 的 TileLink **同款** |
| **`perm_upgrades.json`** | **不声明**静默升级 | E→M 静默升级在本族**未确证**(公开来源**没有**)⇒ 加了即**假声明** ⇒ ⇒ `no_upgrade_instances` **恰含 2** 个(`tilelink_c` + 本族)⇒ 既有 **2 处**计数断言**同步更新** |
| **实例 `perm`** | **4 值**(NONE/READ/UNIQUE/WRITEBACK) | 设备缓存含 `E` ⇒ `E→UNIQUE` 需可表达 ⇒ 与 `cxl_2_0` **同款**;IC-2 判据**只**查 `combination_capabilities.hierarchical`(实测)⇒ 填 `hierarchical=true` 即可 |
| ⚠️ **IC-2 的措辞** | **登记 pending** | IC-2 的 detail 写「UNIQUE(**E 可静默升 M**)」⇒ 本族的 E→M 语义**未确证** ⇒ 属**沿用**通用措辞 ⇒ 登记 |

### 1.6 实例声明(13 键)

| 键 | 值 | 依据 |
|---|---|---|
| `id` | `cxl_3_0_symmetric` | 与 `cxl_2_0` 同族命名 |
| `family` | `industrial` | 与 `cxl_2_0` 同族 |
| `adapter` | `CxlDcohAdapter` | 本 change |
| `status` | `implemented` | — |
| `protocol_version` | `cxl-3.0` | 与 `cxl_2_0` 的 `cxl-2.0` 同款 |
| `flow_support` | `back_invalidate` = **true**(★ 与 `cxl_2_0` 的 `false` **不同** —— BISnp 走 canonical `back_invalidate` 流)· 其余照 `cxl_2_0` | 锚点摘要 §2 / §4 |
| `perm` | 4 值 | §1.5 |
| `atomic.supported` | `false` | 同 `cxl_2_0` |
| `consistency_labels` | `SC` / `SC` | 同 `cxl_2_0` |
| `ordering_assumption` | vc0 unordered / vc1 p2p_ordered / vc2 unordered / vc3 p2p_ordered | 同 `cxl_2_0`(J1:响应向 vc 全 p2p_ordered) |
| `vc_binding` | request 0 / response 1 / snoop 2 / snoop_resp 3 —— `back_invalidate` **归 snoop 的 VC**(BISnp 是 snoop 家族) | 与 `cxl_2_0` 同结构 |
| `sharer_tracker` | `impl=DcohSymmetric` + caps **逐字段**取 `kTrackerTraits[2]` | §1.4 |
| `combination_capabilities` | concurrent / hierarchical / heterogeneous **全 true** | IC-2 要求 `hierarchical=true` |

---

## 2. 方案对比(adapter 形态)

| # | 方案 | 优点 | 代价 | 判定 |
|---|---|---|---|---|
| **A** | **独立 adapter** `CxlDcohAdapter` + 独立规约载体 | 零副本**结构性保证**不变;两族语义各自独立(1 套 4 态 vs **3 套** 4/3/3)⇒ 一个 adapter 一套真相源 | 多一个 adapter 类 | ✅ **裁决采用**(per architect 裁决 #4) |
| **B** | 扩 `CxlCacheAdapter` 支持**双载体** | 类少一个 | ⚠️ 迫使该类引入**载体选择逻辑** ⇒ **破坏**零副本的结构性保证;两套状态空间共处一载体 ⇒ 两套真相源 | ❌ |
| **C** | `CxlCacheAdapter` 的规约载体**参数化** | 复用类骨架 | ⚠️ 同样引入载体选择;零副本判据(静态扫描)需重写 ⇒ **判据面扩大** | ❌ |

---

## 3. `DcohSymmetric` 行为类的形态对比(⚠️ **Phase 2.3 实施期订正** ⇒ A ⇒ **A′**)

| # | 方案 | 优点 | 代价 | 判定 |
|---|---|---|---|---|
| **A′** | **独立头** `include/tlm/cep/cxl_dcoh_tracker.hh`(**Phase 2.3 裁决采用**) | ① `sharer_tracker.hh` 是**协议无关通用层** ⇒ 本族语义**协议专有** ⇒ 独立头**避免层级倒置**;② 同队列 #11 `TreeManager` **同款先例** ⇒ 平台可比较性不降;③ **既有判据零改动**(`test_cep_sharer_tracker_base.cc` 的 deferred 名单断言「`sharer_tracker.hh` 内**不得**有 `class DcohSymmetric`」⇒ 落该头即**天然绿**);④ 规避 `sharer_tracker.hh` 与 `.clang-format` **不兼容**(实测 2140 行 diff) | 多一个头文件 | ✅ **裁决采用** |
| **A** | 行为类纳入 `sharer_tracker.hh` | 与既有 3 个行为类**物理同文件** | ⚠️ **通用层反向依赖协议族** ⇒ 层级倒置;⚠️ **撞既有判据** ⇒ 须同步改 `test_cep_sharer_tracker_base.cc` 的 deferred 名单 + 冻结面 `cep-sharer-tracker-base` spec 的「其余 2 种」计数;⚠️ 须**手工控格式**(该文件与 `.clang-format` 不兼容) | ❌ **Phase 2.3 推翻** |
| **B** | 独立头但**不改名**(沿用 `cxl_dcoh_*` 之外的命名) | —— | —— | ❌ 命名与同族载体(`cxl_dcoh_state_spec.hh` / `cxl_dcoh_adapter.hh`)**不自洽** ⇒ 不采用 |

### 3.1 ⚠️ Phase 2.3 实施期发现(裁决反转的依据)

| # | 发现 | 处置 |
|---|---|---|
| 1 | **既有判据会红**:`test/test_cep_sharer_tracker_base.cc:547-550` 的 deferred 名单断言「`sharer_tracker.hh` 内**不得**出现 `class DcohSymmetric`」⇒ 落 `sharer_tracker.hh` ⇒ **该判据必红**(且它**不是**本 change 的判据 ⇒ 改它 = 动既有守门判据) | ⇒ 落**独立头** ⇒ **既有判据零改动** |
| 2 | **层级倒置**:`sharer_tracker.hh` 是**协议无关通用层**(per 其头注释)⇒ 在其中 `#include` CXL DCOH 规约载体 ⇒ **通用层反向依赖协议族** | ⇒ 独立头(与 #11 `TreeManager` **同款**) |
| 3 | **`sharer_tracker.hh` 与 `.clang-format` 不兼容**(实测 **2140 行** diff)⇒ 改它须**手工控格式** | ⇒ 独立头**规避**该风险 |
| 4 | 原 §3 方案 A 的理由(「本族同属 SharerTracker 家族 ⇒ 仍归 `sharer_tracker.hh`」)与 #11 的理由(「协议专有 ⇒ 独立头」)**不自洽** ⇒ 原裁决**基于错误的分类依据** | ⇒ 本节**显式推翻**并给出**正确依据**(层级方向 + 既有判据 + 格式兼容) |

> ⚠️ **裁决纪律**:方案 A 的**目标**(同构 + 可比较)**不变**,变的只是**物理落点**
> ⇒ 平台可比较性(同一 `SharerTrackerIf` + 同一 `kTrackerTraits[2]`)**不降**。

---

## 4. 后果 / 代价

| 面 | 后果 |
|---|---|
| **adapter** | 新增 1 个 header-only 文件;**零副本**由静态扫描 + 正控保证 |
| **tracker** | **独立头** `include/tlm/cep/cxl_dcoh_tracker.hh` **+1** 行为类(traits 已登记 ⇒ 首次兑现)⇒ 闭合 07 号「残留 A」;⇒ `sharer_tracker.hh` **零改动** |
| **声明面** | `protocol_instances.json` **+1** 实例 · `perm_upgrades.json` **零行** · `no_upgrade_instances` 变 **2** 个 ⇒ **2 处**既有断言更新 |
| **零改动** | `ADR-CEP-*` 正文 · `protocol_instance.schema.json` · `CxlCacheAdapter` · `cxl_state_spec.hh` · `cxl_dcoh_state_spec.hh` · `adapter_schema_contracts.hh` |
| **已知边界** | ⚠️ `LOCAL_HINT` 的**双重语义**(`LocalView` 零改动)· ⚠️ `broadcast_fallback` 无依据仍 pending · ⚠️ `IC-2` 的「E 可静默升 M」措辞沿用 |
| **KPI** | 1 slot(实现层归档后回 0/3) |

---

## 5. 冻结面

- `ADR-CEP-*` / `protocol_instance.schema.json` / `cep-sharer-tracker-base` —— **零改动**
- 新增 adapter + `sharer_tracker.hh` **不在** M1 冻结集内(实测清单含 `include/` 下仅 3 件)
  ⇒ **0 件** ⇒ **无需两步授权** —— ⚠️ **须 Phase 0 实测确认**
