# cpptlm-cep-iag-gate-execution-arm: 装配闸族(IAG)的执行臂补齐 —— 让「只测不拦」变成「装配路径就拦」

> **状态**: 🟡 **Proposed**(2026-11-12 立项 —— per 台账 **Entry #379** 授权;KPI 并发 **0/3 → 1/3**)
> **立项依据**: `.rddf/improvements/cpptlm-cep-iag-gate-execution-arm.md`(5 段 + 7 条 AC)· architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md`(4 条决策 + 影响面实测 + 6 条判据)· 台账 **Entry #378**(批 1 收口 + backlog 登记)
> **上游裁决**: **Entry #379 ②**(起草期实测纠正)· **D-A1~D-A4**(architect 的 4 条决策,实施期不得临场改口径)
> **范围纪律**:只做 **IAG 族执行臂补齐**;⛔ MUST-FIX-3/4 待用户 ratify ⇒ 不并入
> **估时**: ≈1-2 人日(离线 CLI ≈0.5d · 装配路径臂 ≈0.3d · target 第二段 ≈0.2d · 决定性变异 + 回归 ≈0.5d)

## Why

**一个「bridge 指向未声明实例」的 config,在真实仿真与离线 CLI 上都不会被拦** —— 唯一有执行臂的地方是 test。

| 缺口 | 契约依据(逐字可核) | 实测 | 不修的后果 |
|---|---|---|---|
| **装配路径臂缺失** | `test/test_cep_bridge_reattach_guard.cc:22-27`「契约侧判据 `[BSS-01]` 早已存在…… 但它只被**测试侧**调用 ⇒「只测不拦」⇒ 装配路径**不拦**」 | `grep 'run_instance_assembly_gate' src/core/module_factory.cc` ⇒ **0 命中** | 结构违规的 config **能装配成功** ⇒ 错误现场离根因最远 |
| **离线 CLI 臂缺失** | `ADR-CEP-09` 抓取点 1 的离线挂点先例(`cep_channel_dependency_gate`) | `validate_topology` 的命令面 = 4 个**非 CEP** config + 声明层 CLI ⇒ 对 CEP config **无 IAG 判据** | 「一键校验」承诺落空 |
| **既有登记窄化** | `cep-usage.md §12.8` 表外第 2 项 + Entry #358 口径澄清 | **实测不成立**:IAG 族在 `instantiateAll` 侧**也没有**执行臂 ⇒ 是「**两侧一致地都不挂**」 | ⛔ 只补 CLI 臂 = 制造**反向不一致**(CLI 报红、真实装配放行)⇒ 必须两臂同补 |

**为什么是现在**:C1~C4 已把治理线收口 ⇒ 剩下最后一处「**闸存在但没装在路径上**」的同族缺陷;且 Step 0.25 已有一条同族执行臂(channel/ordering)⇒ 补第二条的边际成本最低。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **共用加载器**(新) | `include/tlm/cep/instance_assembly_loader.hh` | `load_instance_assembly_inputs(config)` ⇒ 装配臂与 CLI **共用同一份**加载规则(禁双实现);⛔ 判据本体 `instance_assembly_gate.hh` **零 diff** |
| 2 | **离线 CLI**(新) | `examples/cep_instance_assembly_gate.cc` | 对 **config** 跑 `run_instance_assembly_gate`;退出码 `0=PASS/SKIP · 1=FAIL/配置错 · 2=用法错`;⛔ 不扩声明层 CLI |
| 3 | **装配路径臂** | `src/core/module_factory.cc` **Step 0.26** | 调**同一份**判据 ⇒ FAIL ⇒ `return false`(**零副作用**:模块创建在 Step 2,臂在 Step 0.26) |
| 4 | **target 第二段** | `scripts/CMakeLists.txt` | `CPPTLM_CEP_VALIDATION_CONFIGS` = 9 份 `configs/cep_*.json`;每条**只**跑 IAG CLI ⇒ ⛔ 不与通用 `${TOPOLOGY_VALIDATOR}` 混跑 |
| 5 | **真装配测试** | `test/test_cep_iag_execution_arm.cc` | 3 个用例:正控绿 ⇒ 破坏按**签名**红 ⇒ 还原绿;`[IAG-02]` / `[IAG-06]` 精确断言 |
| 6 | **对账闸** | `scripts/test/cep_iag_gate_reconcile_check.sh` | 7 条判据 + 2 条决定性变异;ctest **172 → 174**;守门表 **34 → 35** |

**Out of Scope(⛔ 硬边界)**

- ⛔ 不改 `instance_assembly_gate.hh` 的**判据口径**(AC-5 要求零 diff)· ⛔ 不改 `protocol_instances.json` / `collective_planes.json` 的**内容**
- ⛔ 不改任何 ADR 正文 · ⛔ 不新建 ADR · ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不加「体量数字」机器守(backlog,per Entry #378 ④)
- ⛔ 不修 `nvlink_4` 的 IAG-05 覆盖缺口(仓内无对应 config ⇒ 登记不修)

## 验收标准(Acceptance Criteria)

> 判据来源 = `.rddf/improvements/cpptlm-cep-iag-gate-execution-arm.md` 的 **7 条 AC**(同源转录,+ **1 处实施期实测订正**)。
> **统一纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ **「rc≠0」不构成红因**。

- [ ] **AC-1（装配路径臂有牙）**:bridge `protocol_instance` 改成**未声明 id** ⇒ `instantiateAll` ⇒ **必红**,签名 = **`[IAG-02]`** ⇒ 还原 ⇒ 绿
- [ ] **AC-2（离线臂有牙 + 两臂同签名）**:同一破坏 ⇒ IAG CLI ⇒ **必红**,签名**同为 `[IAG-02]`** ⇒ 证明两臂共享同一判据实现 ⇒ 还原 ⇒ 绿
- [ ] **AC-3（两臂同绿 · 9 份 config 逐个）**:9 份 `configs/cep_*.json` 逐个 ⇒ 装配路径绿 + CLI 绿(`cep_chi_rnf_e2e_a.json` = **SKIP** ⇒ 绿)
- [ ] **AC-4（挂点真跑 · Q1 教训）**:`cmake --build build --target validate_topology` ⇒ 命令面**含 9 次 IAG CLI 调用** ⇒ 变异「摘掉 IAG 段」⇒ **必红** ⇒ 还原 ⇒ 绿
- [ ] **AC-5（禁双实现 + 判据只读）**:两处均调 `run_instance_assembly_gate` + 共用 `load_instance_assembly_inputs` ⇒ `instance_assembly_gate.hh` **零 diff**
- [ ] **AC-6（0 回归）**:`ctest` **174/174** ⚠️(**实施期订正**:improvement 原写 173 ⇒ CLI **自身**也注册了 ctest ⇒ 实为 **+2**)· `cep_*.sh` **40/40** rc=0 · `format.sh --check` rc=0 · `docs_sync_check.sh --strict` rc=0 · `cep_doc_ref_check.sh --strict` rc=0 · `agents_md_lint.py` rc=0
- [ ] **AC-7（范围纪律）**:⛔ 判据本体零 diff · ⛔ 声明/平面 JSON 未改 · ⛔ 无 ADR 变更 · ⛔ `B3b-2` 未动 · ⛔ 未加「体量数字」断言 ⇒ `git diff --stat` 逐条可核

## 成本收益

**消 1 处「只测不拦」+ 1 处「离线闸不覆盖 CEP config」vs 1 个新头文件 + 1 个新 CLI + 1 处装配臂 + 1 段 target + 3 个测试用例 + 1 个闸(含 2 条变异)**。

收益侧是**错误现场前移**:结构违规的 config 在**装配那一刻**就被拦(且**零副作用**),而不是跑到一半才炸。

## 诚实边界(⛔ 不可约残余)

1. IAG 族**只覆盖装配期结构**,⛔ 不覆盖运行时一致性;
2. CLI 臂对**非 CEP config** = SKIP ⇒「离线闸全绿」**不等于**「所有 config 都过了 CEP 判据」;
3. 装配路径臂是**新增拦截点** ⇒ 可能拦住过去能装配(但结构违规)的 config ⇒ 属期望行为,实施期须逐条记录「由绿转红」;
4. `nvlink_4` 的 IAG-05 路径**暂无真实 config 可跑**(仓内无对应 config)⇒ **已知覆盖缺口**,登记不修。

## 关联

台账 **Entry #379**(立项 + 4 条裁决 + 起草期纠正)· **#378**(批 1 + backlog)· **#377**(批 2 分解)· **#358**(Q1 + 口径澄清)· **#319**(BSS-01 执行臂先例)· architect 方案 `.rddf/reviews/cep-cli-iag-gate-alignment.md` · `.rddf/improvements/cpptlm-cep-iag-gate-execution-arm.md`
