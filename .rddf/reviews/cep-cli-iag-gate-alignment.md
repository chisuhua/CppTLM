# architect 方案 —— 装配闸族(IAG)的执行臂补齐(D-2)

> **主题**:per 台账 **Entry #377** 的批 2 —— D-2「装配闸 CLI 未挂 IAG 闸族」。
> **产出角色**:architect(咨询 + 方案) · **执行角色**:planner(立项) → builder(实施) → verifier(验收)
> **上游**:台账 **#377**(批 2 路径)· **#358**(Q1 修复 + 口径澄清)· `docs/guide/cep-usage.md` §12.8 表外第 2 项
> **状态**:✅ **方案已出**(4 条决策 + 影响面实测 + 验收判据)· ⛔ 本文件**不改任何 ADR 正文**、**不建 change**

---

## 一 · 缺口的精确表述(⚠️ **纠正既有登记的窄化**)

### 1.1 既有登记怎么说

`docs/guide/cep-usage.md` §12.8 表外第 2 项 + 台账 **Entry #358** 的口径澄清:

> 「装配闸未挂 `validate_topology` CLI 的**不对称**」—— 该「不对称」指 **CLI 未挂 IAG 闸族**
> ⇒ `instantiateAll` 与 CLI 对同一 config 的结论**可能不一致**

### 1.2 实测:该登记把缺陷描述**窄了**

| 判据 | 实测 | 证据 |
|---|---|---|
| `run_instance_assembly_gate` 在**装配路径**上 | **0 命中** | `grep -n 'run_instance_assembly_gate\|instance_assembly' src/core/module_factory.cc` ⇒ 无结果 |
| `run_instance_assembly_gate` 的**唯一调用点** | **仅 test 侧** | `test/test_cep_instance_assembly.cc` · `test/test_cep_bridge_reattach_guard.cc` |
| 仓内**明文自认** | 有 | `test/test_cep_bridge_reattach_guard.cc:22-27`「契约侧判据 `[BSS-01]` 早已存在…… 但它只被**测试侧**调用 ⇒「只测不拦」⇒ 装配路径**不拦** ⇒ R-3 残余」;`:30-32`「SCE-12 判的是**装配闸**(`run_instance_assembly_gate`)⇒ 本文件判的是**装配路径**(`instantiateAll`)⇒ 两者**互补**」 |

### 1.3 正确表述(本方案采用)

> **不是「两侧不一致」，而是「两侧一致地都不挂 IAG 族」** —— IAG 族(`[IAG-01]~[IAG-08]`)只在 **test 侧**有执行臂。
> ⇒ 一个「bridge 指向未声明实例 / plane 名未登记 / 桥引模块不在拓扑 / bridge_id 重复」的 config,
> **在真实仿真装配路径与离线 CLI 上都不会被拦**。

**⇒ 缺陷面比登记更宽 ⇒ 修法必须两臂同补**:

| 臂 | 现状 | 修后 |
|---|---|---|
| **装配路径臂**(`instantiateAll`) | ⛔ **无** IAG 执行臂 | ✅ 补 |
| **离线 CLI 臂**(`validate_topology`) | ⛔ **无** IAG 挂点 | ✅ 补 |

⛔ **只补 CLI 臂 = 制造反向不一致**（CLI 报红、真实装配放行)⇒ 必须两臂同补,且**共用同一份判据实现**。

### 1.4 对照组(证明这是本仓既有模式,不是新发明)

`[BSS-01]`(同一模块属多个 bridge entry)已补过执行臂 —— `src/core/module_factory.cc:827` 调 `tlm::cep::find_bridge_module_conflicts`(实现在 `include/tlm/cep/bridge_module_membership.hh`,**唯一实现**)⇒ 本方案沿用同一模式。

---

## 二 · 4 条架构决策

### 决策 1 · 装配路径臂挂在 **Step 0.75**

| 候选 | 判定 | 理由 |
|---|:--:|---|
| Step 0.25(与 channel/ordering 同处) | ❌ | IAG-06 需读 **topology 模块表**(桥引模块是否在拓扑里)⇒ 该步只有 config,无 topology ⇒ 会误判 |
| Step 7.5(桥挂载循环内) | ❌ | 该处**模块已创建 + adapter 已注入** ⇒ 失败时副作用已发生;`:815-819` 明文「⛔ **不新增回滚机制**」⇒ 应更早拦 |
| **Step 0.75**(hierarchy 解析后、Step 1 模块创建前) | ✅ | topology 已就绪 **且** 任何模块尚未创建 ⇒ 失败 = `return false`,**零副作用** |

⇒ 与 Step 0.25 同型:`FAIL` ⇒ 打印 `rep.log` + 错误 ⇒ `return false` ⇒ 装配失败。

### 决策 2 · 离线臂 = **新建独立 CLI** `cep_instance_assembly_gate`

⛔ **不扩** `cep_channel_dependency_gate`:后者语义 = **声明层**闸(读 `protocol_instances.json`,**无 config 概念**);而 IAG 需 **config + instances + planes** 三份输入 ⇒ 语义不同,混入会让退出码语义失真(该 CLI 现约定 `0=PASS/SKIP · 1=FAIL · 2=用法错`)。

⇒ 新 CLI 与装配路径臂**共用** `run_instance_assembly_gate`(`include/tlm/cep/instance_assembly_gate.hh:99`)**同一份实现** ⇒ **禁双实现**满足。

### 决策 3 · 离线臂挂点 = `validate_topology` 组合 target 的**第二段**

- 组合 target 现有 4 个 config(`mesh_2x2` / `mesh_4x4` / `ring_8` / `hierarchical_2x2`)= **非 CEP** ⇒ IAG 对它们 **SKIP**(无 `cep.bridges`)⇒ **挂了也不覆盖** ⇒ **必须纳入 CEP config**。
- ⚠️ 组合 target 每条 config 都跑 `${TOPOLOGY_VALIDATOR}`(`scripts/topology/topology_validator.py`,**通用**拓扑校验)⇒ CEP config 能否过该 validator **未实测** ⇒ ⛔ **不与通用校验混在同一段**(否则整条 target 可能红)。
- ⇒ **单列第二段** `CPPTLM_CEP_VALIDATION_CONFIGS` = 9 份 `configs/cep_*.json` ⇒ 每条**只**跑 IAG CLI ⇒ 零 python validator 风险。

### 决策 4 · `required_plane_instances` 的派生规则 = **空集合**

读 `instance_assembly_gate.hh:180-206` 的确切语义:

| 情形 | 判定 |
|---|---|
| 实例**不在** `required_plane_instances` 且 plane 绑定为 `null`/空 | ✅ **不判红**(合法 = 该实例不启用 collective) |
| 实例**不在**集合且 plane 绑定**非空** | 该 plane 名**未登记** ⇒ `[IAG-05]` 判红 |
| 实例**在**集合 | 绑定缺失/为空 ⇒ `[IAG-04]` 判红 |

⇒ ⛔ **不能**取 `plane_binding` 全集:`moesi_gpu` / `mesif` 的绑定是 **`null`** ⇒ 会被 `[IAG-04]` **误伤** 2 份 config。
⇒ **装配路径臂传空集合**(只判「config 自洽」);**需强制绑定的专项场景**由调用方显式传(如 test 的 `{"ualink_4"}`)⇒ **装配路径不硬编码实例名单**。

---

## 三 · 影响面实测(9 份 CEP config 逐个预判)

| config | `cep.bridges` | 实例 | plane 绑定 | IAG 预期 |
|---|---|---|---|---|
| `cep_chi_rnf_e2e_a.json` | **无该键** | — | — | **SKIP**(per `instance_assembly_gate.hh:104-110`:无 `bridges` 键 ⇒ SKIP;⚠️ 其 `description` 亦自述「`cep.bridges` 键**缺省** ⇒ 装配闸 `SKIP`」) |
| `cep_chi_rnf_e2e_b.json` | 1 条 | `chi_rnf` | ∉ plane_binding | **PASS** |
| `cep_chi_rnf_pe_minimal.json` | 1 条 | `chi_rnf` | ∉ | **PASS** |
| `cep_cohif_pe_minimal.json` | 1 条 | `moesi_gpu` | **null** | **PASS**(记 log「不启用 collective」,不判红) |
| `cep_mesif_pe_minimal.json` | 1 条 | `mesif` | **null** | **PASS** |
| `cep_moesi_gpu_pe_minimal.json` | 1 条 | `moesi_gpu` | **null** | **PASS** |
| `cep_tilelink_pe_minimal.json` | 1 条 | `tilelink_c` | ∉ | **PASS** |
| `cep_ualink_pe_minimal.json` | 1 条 | `ualink_4` | `FabricCollectivePlane` | **PASS**(该名**已登记**,`collective_plane.hh:803`) |
| `cep_ualink_engine_pe_minimal.json` | 1 条 | `ualink_4` | 同上 | **PASS** |

**补充实测**:桥引模块**全部在拓扑内**(9 份 config 逐个比对 `bridges[].modules` ⊆ `modules[].name`)⇒ IAG-06 不红。

⇒ **结论**:既有 config **无一会被新执行臂判红** ⇒ 补执行臂**不破坏**现有 config 的可装配性。
⚠️ **但装配路径臂是新增拦截点** ⇒ 实施期必须跑**全量 `[cep]` + ctest** 验证 ⇒ 若有 config 由绿转红,须逐条判定是「真缺陷」还是「误伤」。

---

## 四 · 验收判据(必须含决定性变异 · ⛔ 不断言 `rc≠0`)

| # | 判据 | 形式 |
|:--:|---|---|
| **1** | **装配路径臂有牙**:把某 CEP config 的 bridge `protocol_instance` 改为**未声明 id** ⇒ `instantiateAll` ⇒ **必红**,红因签名 = `[IAG-02]`,且**断言模块未创建**(零副作用) | 决定性变异 ① |
| **2** | **离线臂有牙**:同一破坏 ⇒ IAG CLI ⇒ **必红**,红因签名 = `[IAG-02]` ⇒ **与装配路径同签名** ⇒ 证明两臂共享同一判据实现 | 决定性变异 ② |
| **3** | **两臂同绿**:9 份 config 逐个 ⇒ 装配路径绿 + CLI 绿(`e2e_a` = SKIP ⇒ 绿) | 正控 |
| **4** | **挂点真跑**:`cmake --build build --target validate_topology` ⇒ 命令面**含 9 次 IAG CLI 调用** ⇒ ⛔ 不得是「挂点存在但命令面没调」(Q1 的教训:挂点静默失效 20 天) | 变异 ③(摘掉命令面 ⇒ 必红) |
| **5** | **禁双实现**:两处均调 `run_instance_assembly_gate` ⇒ ⛔ 不复制判据逻辑 | 机器 grep |
| **6** | **0 回归**:`ctest` 全绿 · `cep_*.sh` 全绿 · format / docs_sync / doc_ref 全绿 | 回归 |

---

## 五 · 不可约残余(必须随交付报告落盘)

1. IAG 族**只覆盖装配期结构**(声明层 + 平面层 + 拓扑层)⇒ ⛔ 不覆盖运行时一致性;
2. CLI 臂对**非 CEP config** = SKIP ⇒ 「离线闸全绿」**不等于**「所有 config 都过了 CEP 判据」;
3. 装配路径臂是**新增拦截点** ⇒ 可能拦住过去能装配(但结构违规)的 config ⇒ 属**期望行为**,但实施期须逐条记录「由绿转红」的 config;
4. 本方案**不覆盖** `nvlink_4`(`plane_binding` 有绑定但仓内无对应 config)⇒ 该实例的 IAG-05 路径**暂无真实 config 可跑** ⇒ 属**已知覆盖缺口**,登记不修。

---

## 六 · 后续步骤(交给 planner / builder)

| 步 | 角色 | 产物 |
|:--:|---|---|
| 1 | architect | ✅ 本方案 |
| 2 | planner | improvement 5 段草稿 + change 骨架 + 台账 Entry(立项授权) |
| 3 | builder | ① 离线 CLI `cep_instance_assembly_gate` ② 装配路径臂(Step 0.75)③ `validate_topology` 第二段 ④ 决定性变异 ⑤ ctest 注册 + 守门表 |
| 4 | verifier | 逐条 AC 验收 |

⛔ **范围纪律**:⛔ 不改任何 ADR 正文 · ⛔ 不新建 ADR · ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不顺手修 §4.0 预存 grep 漂移 · ⛔ 不在本方案内加「体量数字」机器守(已登记为 backlog,per Entry #378 ④)。

**关联**:台账 **#378**(批 1 + 体量数字缺口登记)· **#377**(批 2 分解)· **#358**(Q1 + 口径澄清)· **#319**(BSS-01 执行臂先例)· **#301** · `include/tlm/cep/instance_assembly_gate.hh` · `src/core/module_factory.cc` · `scripts/CMakeLists.txt` · `examples/CMakeLists.txt`
