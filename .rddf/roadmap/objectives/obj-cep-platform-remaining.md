# CEP 平台 · 剩余任务总清单（canonical 登记册）

> **性质**: planner 的**剩余任务单一真源**（canonical registry）。台账 `obj-cep-platform.md` 的 Entry 只做**回执 + 指针**，⛔ 不在此处复述清单 ⇒ 避免台账（24k 行 append-only）继续膨胀、也避免清单在多处复述漂移。
> **实测日期**: **2026-11-20** · **实测基线**: 见 §0.1（§0.2 保留 2026-11-16 口径供追溯）。
> **权威关系**: 本册 = **当前有效**的残余登记；台账 §11 的历史 Entry 里的 `R-*` 编号属**历史快照**（⛔ 见 §4 的编号空间冲突说明）。
> **⚠️ 当前有效清单 = §6**（2026-11-20 的缺口清单 + 立项计划 · 经 metis 1 轮 + oracle 1 轮 + critic 3 轮审查）。§1 的 **24** 项是 **2026-11-16 口径**（⛔ 本轮未逐条重测）⇒ 保留作历史对照，**严重度排序与排期以 §6 为准**；§1 的数字刷新归 §6 的 **P-1** 立项单元。

---

## §0 · 实测基线

### §0.1 当前（**2026-11-20 实测** · per §6 的 v4 计划）

| 项 | 实测 | 命令 |
|---|---|---|
| 全仓主 spec | **137** 份（`cep-*` **46** + 非 `cep-*` **91**） | `ls openspec/specs/ \| wc -l` |
| `openspec validate --specs --strict` | **55 红 / 137**；**55 份中 `cep-` 前缀 = 0** ⇒ **CEP 46 份 spec 全绿** | `openspec validate --specs --strict` |
| ├ 非 CEP Purpose 占位 | **45** | 同上（WARNING 类去重） |
| └ 非 CEP Requirement 正文 >500 | **26**（与 Purpose 重叠 16） | 同上 |
| 闸族 | **45** 个 `cep_*.sh` | `ls scripts/test/cep_*.sh \| wc -l` |
| CEP 测试 | **73** 个 `test_cep_*.cc` | `ls test/test_cep_*.cc \| wc -l` |
| OpenSpec | active **0** · 归档 **change 目录数** **164**（CEP **50**）· KPI **0/3**（**空档 ⇒ 可立新提案**） | `ls openspec/changes/ \| grep -v archive \| wc -l` |
| CEP 归档 change 缺独立验收报告 | **31**（50 CEP 归档 − 10 名称 join 命中 − **9 已有验收报告但文件名不匹配** ⇒ 真缺 31；⚠️ 旧「40」= 只做了名称 join、未查报告内容 ⇒ **虚高 9**）| 对账脚本（判据见 §6.2 S3-5） |
| KPI 活口径同步面 | **6 文件 / 7 处** —— 权威清单 = 既有闸 `scripts/test/rddf_stage_gate.sh:243-250` 的 `LIVE_FILES`；活口径行：`AGENTS.md:313` · `.rddf/roadmap.md:5` · `.rddf/roadmap/phases/phase-10.md:209` · `obj-cep-platform.md:474` · `cpptlm-cep-umbrella.md:306` · `cpptlm-cep-nvlink-sharp.md:31` + `:211`（**2 处**）⇒ 取值全部 == `0/3` | 见 §6 S3-3 |

> ✅ **KPI 0/3** ⇒ 三个框架治理 change（`cpptlm-module-param-effect-landing` / `cpptlm-config-type-registration-align` / `cpptlm-module-param-migrate-batch2`）已于 2026-11-18~19 **全部归档** ⇒ **§1.5 整节已作废**（见该节顶部）⇒ **当前可立新提案**（`AGENTS.md` 的 Proposed ≤3 门槛 = **0/3**）。
> ⚠️ **本表 2026-11-20 的数字与上一版（2026-11-18）的差异全部来自那三个 change 的归档** ⇒ **不是**测量口径变更。

### §0.2 历史（2026-11-16 实测 —— 保留供口径追溯）

| 项 | 实测 |
|---|---|
| 全仓主 spec | **129** 份（`cep-*` **43** + 非 `cep-*` **86**） |
| `openspec validate --strict` 红 | **55** 份（全为非 `cep-*`）= **45** 份 Purpose 占位 + **26** 份有「正文 >500」issue |
| 非 strict 口径 | **129/129** 绿 |
| 闸族 | **45** 个 `cep_*.sh` · 全 rc=0 |
| Catch2 `[cep]` | **529** 用例 / **12288** 断言 全绿 |
| 冻结面 | `artifact_count` **86** |
| OpenSpec | active **0** · 归档 **change 目录数** **151** · KPI **0/3** |

---

## §1 · 剩余任务（**24** 项 · 按「谁能做」分类）

> **状态码**：`OPEN`（本仓可直接做）· `ARCH`（须 **architect** 判权）· `OWNER`（须各模块 owner 修）· `PROC`（流程动作，零成本）· `BACKLOG`（低优先）· `N/A`（本机不成立）· `BLOCKED`（外部阻塞，⛔ 不排期）

### §1.1 `OPEN` —— 本仓可直接做（**4** 项）

| # | 任务 | 实测 | 依赖 | 成本 | 判据载体 |
|:--:|---|---|---|:--:|---|
| **R-1a** | **R-1 批 4~6**：剩余 **45** 份 Purpose 占位 | 45 份（起始 75 → 批 1 清 16 → 批 3 清 14） | 批 1/3 的生成器 + 校验器（**判据 C-1~C-10 一经落地不得改语义，只能追加**） | 2.5 d | `scripts/test/cep_spec_purpose_batch{1,3}_*.py` + 归因载体范式 |
| **R-1b** | **R-1 批 2**：**26** 份 Requirement 正文 >500 | 26 份 | ⚠️ **会动 `## Requirements` 段 ⇒ 另一档风险** ⇒ 须另立 change + architect 参与 | 1.5 d | openspec 自身判据（`Requirement text is very long (>500 characters)`） |
| **B-1a** | **3 条 invariant 建模**：`CEN-Ib8`（engine egress 可观测面）· `INV-CEP-08-IC1`（J3 通用形式可判定载体）· `INV-CEP-09-IC2`（多 VC 仲裁器） | `test/cep_invariant_deferrals.json` 触发条件逐条已登记 | ⚠️ **判据须 architect 先定义**（否则 builder 自行发明 invariant 语义 = 越权）⇒ 可与 R-1a **并行启动 architect 方案** | 1.5 ~ 3 d | `test/cep_invariant_assertion_map.json`（`carriers` 61 → 64） |
| **D-1** | **归档 change 缺独立验收报告** —— **统一口径（2026-11-20 复验 · per critic BLOCKER-1 二次订正）= 全仓 164 个归档目录中真缺 134 份**（21 份名称 join 命中 + **9** 个 change「已有验收报告但文件名不匹配」（⚠️ **9 份报告覆盖 9 个 change**，其中 `cep-instance-assembly-and-l4-e2e-verify.md` **一份覆盖 2 个 change**）⇒ 已验收 **30** 份 ⇒ 真缺 **134**）。**分两层**：**乙层 8 份**（归档 **≥ 2026-11-12**，**统一覆盖不按 `cep` 前缀特判**：5 份 CEP + 3 份非 CEP；其中 3 份**带着「未做验收」的 checkbox 就归档**）· **甲层 126 份**（归档 < 2026-11-12；⚠️ **并非「无该实践」** —— 11-06/11-07 已有 **8 份** verifier 报告覆盖 **9 个** change，只是文件名未遵 `<change>-verify.md` 规范）| 乙层 **8**（5 CEP + 3 非 CEP）· 甲层 **126**（含 **9** 个「已验收但命名不规范」change）· 全仓真缺 **134** | 无 | **乙层 ≈5~5.5 d**（8 份 × ≥0.5 d + 闸 ≈0.5 d + 抽检 ≈0.5~1 d；⚠️ **旧「3~4 d」与自身公式矛盾**，按公式下界为 5 d）+ **甲层抽检 ≈0.5~1 d** | `.rddf/reviews/<change>-verify.md`（**精确命名**）+ **机关** `scripts/test/archive_verify_check.sh`（封存边界 = 归档目录日期 **< 2026-11-12**；**先补后上闸**） · **2026-11-21 落地**（I-10 · builder）：乙层真缺实为 **9** 份（= 原 8 份 + `cpptlm-cep-litmus-falsification-harness` —— I-6a 于 2026-11-21 归档 ⇒ 落入乙层窗口）⇒ **9 份追溯补验报告**已落 `.rddf/reviews/<change>-verify.md`（⛔ 逐份含「原归档未验 · 追溯补验」provenance 标记）· **甲层抽检 5 份** → `.rddf/reviews/archive-audit-2026-11-21.md`（命中 **3 份**疑点 ⇒ 定向补验另立单元，⛔ 不扩全量）· **机关** `scripts/test/archive_verify_check.sh`（基线 `scripts/test/archive_verify_baseline.txt` = **30**）· **乙层 30 目录 force-add** ⇒ CI 可见集 = 30（⛔ 否则闸在 CI 上空转绿 = 复刻 `S2-8` 假绿）· 封存边界 = 归档日期 < 2026-11-12（一次性历史封存，⛔ 不按名字豁免）|

### §1.2 `ARCH` —— 须 architect 判权（**8** 项）

| # | 任务 | 实测 | 为什么必须 architect |
|:--:|---|---|---|
| **B-2** | **19 条 ADR 归因例外**收敛 | 19 条（`condition_unmet` 2 · `carrier_buildable_id_blocked` 10 · `carrier_present_id_absent` 2 · `doc_layer_manual_review` 5）；⚠️ 其中 **12** 条 rationale 明写「**⛔ id 被已签批 ADR 正文挡住**」 | 要改的是**已签批 ADR 的正文约束**（或走 Status Update）⇒ builder 不得自行放宽 |
| **B-3** | **DoD 7 口径订正** | `obj §0` DoD 7 写「**66 条守门 invariant 机器检查 100% 覆盖**」⇒ 实测 **61 条有判定级载体 + 5 条 `⏭-unmodelled`** = 66 ⇒ 真源语义是「66 条**全部已处置**」⛔ 不是「66 条全部有载体」 | 改 `obj §0` = 改 objective 的完成定义 ⇒ 判权项 |
| **R-9** | **清单增殖**：同一事实 5 处手工维护 | structural `decision_adrs` / 快照 keys / `NUMBER-MAP` / `docs_sync ADR_LIST` / `OWNERS+NO_INV` | 要定一个 **canonical**（哪个是源、其余改引用） |
| **R-2** | `moesi_gpu` / `mesif` **13 键**无仓内 golden | 二者**不在** `repo_owned_instance_ids`（6 个）⇒ 权威在本仓、无第二来源 | 要定 13 键的 canonical 取值来源 |
| **R-10** | **superseded spec 存废裁定** | `cpptlm-d1-p1-pipeline-scoreboard` 的 spec 带 `Status: SUPERSEDED`；`include/cudart/` **整个目录不存在** ⇒ spec 本身是孤儿 | 删 spec / 改写为历史记录 = 契约面裁决 |
| **R-11** | **4 份同源 spec** 内容重复 | `pcie-host-test-flr-rebar` 的 3 条 Requirement 是兄弟 spec 的**同名复制** | 合并 / 退役 = 契约面裁决 |
| **R-16** | **3 份未实施 spec** 存废 | `dgpu-board-mvp`（`dgpu_board_mvp.hh/.cc` 不存在）· `dgpu-soc-control-hub`（`ControlHub` 0 命中 + `PcieMemoryDevice` 仍在 `configs/dgpu_soc_minimal_v1.json:49`）· `dgpu-soc-physical-ip-port-boundary`（`gmmu_tlm.hh:142` 明写 `wire-format 统一 PcieTlpBundle`，未切 `NoCFlitBundle`） | 同 R-10（孤儿 spec 存废） |
| **E-1** | **R 编号空间被历史轮次复用** ⚠️ **本次新发现** | 历史 Entry 的 `R-1`~`R-9` 各自独立含义（例：`R-9` = ODR 隐患 / 清单增殖；`R-10` = `mktemp` 写系统 tmp（已修）/ superseded spec）⇒ 残余登记**不可寻址** | 编号空间是治理契约 ⇒ 须定「重编号 / 前缀分区 / 单源登记册」三选一 |

### §1.3 `OWNER` —— ⛔ **本节已废除（2026-11-20 · 用户拍板）**

> ⛔ **分类维度本节起取消** —— 理由：本仓**无独立 owner 角色**（单 agent 仓）⇒ `needs: owner` **没有任何执行者** ⇒ 该分类的 8 项**永久挂起**（不是「等待」，是「无人认领」）。
> **用户 2026-11-20 裁决**：废除 OWNER 维度 + **8 项逐项落终态**（⛔ 不是「清空即消失」）；B 类走 openspec change · C 类送 architect。
> ⚠️ **未来若出现真 owner**（如 ArchForge 负责人）⇒ 需**重建**该维度（这是废除的不可逆部分）。
> **8 项终态表**（2026-11-20 逐条复验，**全部仍 live**）：

| # | 任务 | 实测（复验点） | 终态落点 |
|:--:|---|---|---|
| **R-12** | `abi_guards.h` 悬空引用 | `include/cudart/abi_guards.h` 不存在；引用处 = `openspec/specs/pcie-slice-prerequisites/spec.md:14`（**spec 内**）| **B 类** → `cep-spec-stale-refs-r14-r15-r20` |
| **R-13** | spec 内部**旧状态行**与新 Purpose 打架 | `openspec/specs/pcie-ip-microarch/spec.md:296`「等待评审 + Phase 1 启动实施」· `openspec/specs/pcie-ip-integration/spec.md:53`（末行）「Phase 8 最终整合交付中」（**两处都在 spec 内**）| **B 类** → `cep-spec-stale-refs-r14-r15-r20` |
| **R-14** | 引用**已迁 ArchForge** 的文档 | `pcie-ip-integration/spec.md:13,19` 仍要求 `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` | **B 类** → `cep-spec-stale-refs-r14-r15-r20`（openspec change）|
| **R-15** | 引用**已迁 ArchForge** 的文档 | `v1-architecture/spec.md:8,20` 仍要求不存在的 `00-overview.md` | **B 类** → 同上 |
| **R-17** | **2 份来源 change 归档目录缺失** | `dgpu-soc-timing` / `driver-visible-minimal-soc` ⇒ 曾存在于 commit `0345d283` / `689752ef` | **A 类** → `cep-doc-hygiene-kpi-and-impl-state` |
| **R-18** |**doorbell 配置 vs 代码 hardcoded 不一致**（**已落地 2026-11-21** ⛔ Requirement 已改名 ⇒ 现标题为「doorbell 合成偏移是已裁定的现状(per ADR-DGPU-09 §2.2.2 + d3-evolution-roadmap)」）| `minimal-dgpu-soc-architecture` 的 Requirement 标题**就叫**「doorbell 配置 vs 代码 hardcoded 不一致」 —— ⛔ 该 Requirement 已于 I-12 经 openspec **RENAMED** 改名为「doorbell 合成偏移是已裁定的现状…」；原 3 处矛盾断言（A-1 MUST 被全体违反 / A-2 Scenario 与实现相反 / A-3 僵尸条款）已消解| **C 类** → **architect**（Q-12：改代码对齐配置 vs 改契约承认 hardcoded）→ ✅ **已落地 2026-11-21**（change `cpptlm-doorbell-contract-alignment` 已归档 · 7 处连带全落）|
| **R-19** | `AGENTS.md` 记载的 LWC handler 名 **stale** | `AGENTS.md:423` 写 `handle_cr_transaction`；实现实为 `register_doorbell_target` / `doorbell_write`（`lwc_bus_hub.hh:84,107`）| **A 类** → `cep-doc-hygiene-kpi-and-impl-state` |
| **R-20** | 引用**已删除**的 ABI | `dgpu-board-adapter-info/spec.md:10,21` 仍引用已删的 `cpptlm_emulator_get_adapter_info`（per ADR-SOC-20 §2.1）| **B 类** → `cep-spec-stale-refs-r14-r15-r20` |

> ⚠️ **分类说明（2026-11-20 二次修正 · per 裁决记录 §10.9.3）**：边界判据 = **落点是否在 `openspec/specs/`**。
> **A 类 = 文档/路径**（**2 项**：R-17 · R-19）⇒ 改 `.md` / 归档目录即可。
> **B 类 = spec 契约面**（**5 项**：R-12 · R-13 · R-14 · R-15 · R-20）⇒ 落 `openspec/specs/*/spec.md`，**改契约，须走 openspec change**（⛔ 不是「文档小修」）。
> **C 类 = 架构取舍**（**1 项**：R-18）⇒ ⛔ builder 不能单方面定。
> ⚠️ **R-12 / R-13 原归 A 类是错的** —— 二者引用/落点**都在 spec 内**，而 A 类承接单元 `cep-doc-hygiene-kpi-and-impl-state` 的 AC-6 明文禁止 `openspec/specs/` diff ⇒ 按原归属**物理不可执行**（oracle 复核指出）。
> ⚠️ **判据纪律**：`needs: owner` 在本仓**不可执行** —— 未来新增任务**不得**再用该分类，改按 `OPEN` / `ARCH` / `PROC` / `BACKLOG` / `BLOCKED` 归类。

> *(2026-11-20：原 8 项登记表已**折叠进上方「终态表」**，此处不留重复副本 —— 一个事实只写一处。)*

### §1.4 `PROC` / `BACKLOG` / `N/A` / `BLOCKED`（**4** 项）

| # | 任务 | 状态 | 说明 |
|:--:|---|:--:|---|
| **D-2** | **立项时同步 6 处 KPI 活口径** | `PROC` | 流程缺口（Entry #442 §⑤·3 亲历）：active 窗口期间 `cep_docs_sync_check` / `cep_gate_selftest` 报红（口径写 0/3 而实测 1/3）⇒ **下一轮 planner 动作清单须加入「立项即改 6 处口径」**（零成本，纯流程） |
| **R-6** | `openspec` **版本未 pinning** | `BACKLOG` | 闸打印版本、CI 不锁 ⇒ 版本漂移不可察（0.2 d） |
| **R-3** | ArchForge 缺席致整闸 `exit 0` | `N/A` | **本机不成立** —— `/workspace/main/ArchForge` 在位 ⇒ 闸真跑 7 项全绿；仅 CI 无 ArchForge 时成立（0.2 d，CI 侧提示） |
| **A-1** | **B3b-2 = 0/4**（CXL 2.0 / CXL 3.0 / NVLink / UALink） | `BLOCKED` | 4/4 族 `blocking_type = external`（CXL 规范受 ECA §3 + AI AUP 约束**不可读**）⇒ ⛔ **本仓单方面不可推进** ⇒ **不排期**；治理上正确处置是 ⭕ 登记（`test/cep_b3b2_deferrals.json` + 闸已机器守） |

---

## §1.5 · 【框架治理域】三个 active change 的实施序列（2026-11-18 规划）

> ⛔ **本节整节已作废（2026-11-20）** —— §1.5 的三个 change（`cpptlm-module-param-effect-landing` / `cpptlm-config-type-registration-align` / `cpptlm-module-param-migrate-batch2`）已于 **2026-11-18~19 全部归档**（per 台账 Entry #485 / #487 / #500）⇒ 本节的「118 个未勾 task」「阶段 A/B/C 序列」「波 0 排期」**全部完成出清** ⇒ **本节不再是待办**。
> ⛔ **但本节仍是本仓「顺序由机关保证」的解药来源** —— `scripts/test/rddf_stage_gate.sh`（**已存在 + 已注册 ctest** `test/CMakeLists.txt:286-287`）与 `scripts/test/rddf_ac_carrier.tsv` 就是 §1.5.6 开出的解药（⚠️ 闸头 `:3` 引用本节 ⇒ 本节作废后该引用悬空 ⇒ 转指方案归 §6 的 **Q-6** 裁决 + **I-0** 实施）。
> ⇒ **本节正文逐字保留**（历史快照，不回改）；**当前有效清单见 §6**。

> **本节是当前最大的剩余工作**（118 个未勾 task）—— 三个 change 都属**框架治理**域（参数机制），非 CEP 平台域 ⇒ 单列一节，避免与 §1 的 CEP 域清单混读。
> **依赖图逐条实测自各 change 的 `design.md`**，非推断。

### §1.5.1 现状与依赖图

| change | 简称 | 进度 | 剩余 task | KPI |
|---|---|:--:|:--:|:--:|
| `cpptlm-module-param-effect-landing` | **C1** | 20/44 | 24（其中 **5 个已失效**需清理） | 占 1 |
| `cpptlm-config-type-registration-align` | **R-5** | 7/44 | 37 | 占 1 |
| `cpptlm-module-param-migrate-batch2` | **C2** | 0/57 | 57 | 占 1 |

```
C1（B2 剩余 12 键 → B3 routing → T4/T5 → T7 收口）
  │        ↑ C1 design.md:107（D-3）「B3 优先于 C2 的 T1」
  │        ↑ C2 tasks.md:43（T3.1）「前置：effect-landing 的 B1 与 B3 完成」
  │        ↑ C1 design.md:125-127（D-5）「先给键确定效果，再迁移类型」
  └──────────────► C2（T0 → T1/T2 迁移 20 类型 → T3 → T4 ParamRules 退役 → T5~T8 收口）

R-5（T0 基线 → T1 对齐 → T2 机制 → T3 判据 → T4~T6 收口）
  │        ↑ R-5 design.md:62（D-4）「T1 必须先于 T2」
  └──────────────► 与 C2 的**文件冲突面可接受**（都改 `ModuleFactory`，但**不同位置**：
                   R-5 加预扫描段 :339-:350 之间 / C2 删默认值注入段 :263-291）
                   ⚠️ **但实际不可并行**：本仓**单 worktree** + `.rddf/state/role-claims.json`
                   **每 role 唯一 active claim** ⇒ 不存在两个 builder ⇒ 不会发生 rebase
                   （⚠️ R-5 design D-4 的「先合并者 rebase」是理论表述，本仓不成立）
R-5.AC-08 ◄── C2.T0.2（嵌套失败传播）—— **可选**：C2 未落地则 R-5 该 AC 标
                   `blocked-by` + 显式登记（R-5 design.md:59 允许，⛔ 不许静默跳过）
```

> ⚠️ **过度陈述订正（critic 2026-11-18）**：C1 → C2 **不是全链硬前置** —— C2 只有 **T3**（4 个类型）依赖 C1，且 C1 的 B1 **已完成** ⇒ 实际只剩 **B3**;按 C1 design.md:127（T1a）本可与 C1 尾部并行。**本规划按串行编排是 risk-averse 选择（更易定位回归归属），不是机制约束。**

### §1.5.2 阶段 A —— C1 收尾 → 归档（KPI **3/3 → 2/3**）

| 子步 | 内容 | 依赖 / 判据 |
|---|---|---|
| **A0** | ⚠️ **BLOCKER 修复（critic）** —— C1 的 **AC-01 现在就是红的**：判据是「D-1 表的类型数 / 键×类型对数与 `param_effect_inventory.py` 实测一致」，而 `design.md:27-33` 的 D-1.2 仍列 `GPUTLM`(4 键) · `MemoryClusterTLM`(2) · `SharedMemoryTLM`(2) —— 三者的 `.hh` 已在 commit `39bca057` 删除、3 份配置也已删 ⇒ 脚本实测必不含这三行。**修法**：D-1 表**删三行**（17→14 类 / 42→34 对）+ `:221` 的散文注记**升格为正式范围修订**;AC-01 判据改为「清单 = 脚本输出 ∪ 已登记零命中键」 | 判据 = `python3 scripts/test/param_effect_inventory.py --json \| grep -c "GPUTLM\|MemoryClusterTLM\|SharedMemoryTLM"` = **0**（**现为 3 ⇒ 必红**） |
| **A1** | 清理 **3 个失效 task**（`T2.4`GPUTLM · `T2.9`MemoryClusterTLM · `T2.10`SharedMemoryTLM —— 因**类型归档**失效,Entry #481）;⚠️ **不是 5 个** —— `T2.3`(ComputeUnitTLM) 与 `T2.12`(TCCTLM) **已移交 R-5**（`tasks.md:38,47` 已标「转 R-5」，R-5 T1.4 承接）⇒ **零动作** | 失效 task 用**删除线 + 注明**或 `[x]` + 注明 —— ⚠️ `[~]` **不被 `openspec` 的 task 状态机识别**（只认 `[ ]`/`[x]`）⇒ 带 `[~]` 归档 = 归档目录显示未完成 task ⇒ 与「✅ 已完成归档」语义冲突 |
| **A2a** | B2 剩余 **7 键**：`PcieControllerIP`(3) · `StreamingMultiprocessorTLM`(2) · `CoherentXBarTLM`(2) | ⚠️ **「判定明确，态②」是过度陈述（critic / metis）** —— 这 7 键在 C1 的 `design.md` D-1.6 里**从未逐键裁决过** ⇒ 落地前须**逐键定态**;其中 `PcieControllerIP` 3 键的切法见 **§3 H-Q7**（同名不同语义）;⚠️ **ADR-X.17 D-3「同一类型内不得同时存在 `load_param` 声明的键与板级直读的键」** ⇒ 切分单位应是**类型**而非逐键 |
| **A2b** | B2 **2 键**（态①，需实现 + 判别测试）：`GpuMeshNoC.dim` / `hops_latency` | `dim_`/`hops_latency_` **真被** `d2d_forward_payload()` 延迟模型用 ⇒ 观测量 = **既有** `last_transfer_latency_cycles()` / `simulated_throughput_GBps()`（D-2.1 禁造新观测面） |
| **A2c** | B2 **3 键**（⛔ **需拍板** ⇒ 见 §3 H-Q4）：`CPUTLM.pattern`（态③）+ `start_addr` / `request_interval`（态①） | ⚠️ 接上 `start_addr` 会改变 **5 份配置 / 15 个实例**的**实际流量**（现全部从成员默认 `0x1000` 发 ⇒ 配置值被忽略）;⚠️ 真问题是「**配置意图与行为不符**」—— 不是「互相撞地址」（共享存储下多 CPU 同地址是合法场景）;**缺省规则（边界情况）**：某键**判不出态**时**缺省落态②**（可逆、保留现值语义、装配显式失败）;态① 需**两条证据**（成员被读的调用点 + 已存在观测量）,缺一即落态② |
| **A3** | B3 `RouterTLM.routing`（⛔ **需拍板** ⇒ 见 §3 H-Q5）+ `GpuNoC::simulate_instantiate` 内部配置生成同步 + `[simmodule][infra]` 用例绿 | C1 D-3：**必须先于 C2 的 T1**（否则「已准入类型 + 未裁决键」窗口期） |
| **A4** | T4 测试（态③ 键各一条**非法值拒绝**测试）+ T5 验证（三绿 + 五闸） | — |
| **A5** | T7 收口：台账 Entry · KPI 回填 · spec delta 合并 · **独立验收** · 残留清点 ⇒ `openspec archive` | ⛔ 不用 `--skip-specs`（实现已落地） |

### §1.5.3 阶段 B —— R-5 实施 → 归档（KPI **2/3 → 1/3**）

| 子步 | 内容 |
|---|---|
| **B0** | ⚠️ **BLOCKER 修复（critic）** —— R-5 的 **AC-07 永远不可达**：`proposal.md:101` 写「`cpptlm_tests` 全量绿（≥ **883336 / 2393**）」，而实测已降到 **883242 / 2372**（差值来自归档删测试 −26 case + 新增判别 case）⇒ 按 proposal 原文 R-5 **不可能验收**。**修法**：AC-07 改写为「全量绿且断言数 ≥ **实施日重测基线**」并**内嵌重测命令**（`./build/bin/cpptlm_tests 2>&1 \| tail -3`）—— ⛔ **不得再写死数字** | 判据 = 重测命令的实测值回填进 proposal，且 proposal 内**无残留硬编码下限** |
| **B1** | T0 基线重测（运行时注册表 **63** 类型 —— 归档 4 个后;4 个未注册类型差集;3 份配置仍 exit 0 的实测） |
| **B2** | T1 对齐 4 处（`dgpu_board_v1` 删 `tmu` + 登记派发链现状 · `library_memory_demo` 改 `ArbiterTLM4` + **生成器** + `cpptlm/library/standard.py:187` · `apu_soc_full` 删 `ComputeUnitTLM`×4 + `TCCTLM`×2 + **生成器** + **测试断言**）<br>⚠️ **生成器漂移面比 R-5 自己的清单更大（oracle 复核确认）** —— R-5 T1.3 只列 `standard.py:187`、T1.4 只列 `ComputeUnitTLM`/`TCCTLM`，但实测**仍在 emit 已被 C1 判态②的键**：`examples/generate_apu_soc.py:98/102`（CacheTLM `protocol`）、`:117`（CoherentXBarTLM `snoop_filter`/`domain_protocol`）· `cpptlm/library/standard.py:176`（`memory_type`）`:185`（`mem_type`）⇒ **不补齐 ⇒ R-5 的 AC-05「重跑生成器 ⇒ `git diff --exit-code configs/` 为空」必红**，且阶段 B 需临时扩 scope |
| **B3** | T2 机制：`instantiateAll` **Step 0.3 预扫描**（插件加载后、实例创建前）⇒ 未注册类型 `return false` + `stderr` 报「类型名+实例名+来源文件」;⛔ 不走 `DPRINTF`;⛔ 不加 `--strict` 开关 |
| **B4** | T3 判据：`cpptlm_sim --list-types --json` + 差集脚本 + **ctest 闸**（反向自测 + **零豁免**） |
| **B5** | T4~T6：测试 · 验证 · 收口 ⇒ `openspec archive` ⇒ KPI 1/3<br>⚠️ **BLOCKER 前置（critic）** —— R-5 的 delta spec `spec.md:7` 原文「配置引用的**每一个**模块 `type` SHALL 在运行时注册表中存在」**未限顶层**，而实现（design.md:25,36）**只扫顶层 `modules[]`**、嵌套归 C2 的 T0.2 ⇒ 默认 archive 会把**未实现的嵌套能力**合进 main spec ⇒ 按 AGENTS.md「✅ 已完成归档 = 能力已在契约内」语义，**归档动作本身在撒谎**。**修法（必须在 B5 之前落）**：Requirement 收窄为「配置**顶层 `modules[]`** 引用的每一个模块 `type` SHALL …;内联嵌套 `modules` 的失败传播由 `cpptlm-module-param-migrate-batch2` 承接，本条不覆盖」;并在 C2 的 specs delta 里**预登记**对该条的 MODIFIED 升级（C2 落地时摘除顶层限定） |
| ⚠️ | **T7.8 跨仓**（不在本仓）：ArchForge 三份契约状态改「⏸ 已归档」（`gpu-gputlm.md` / `gpu-shared-memory.md` / `gpu-memory-cluster.md`） |

### §1.5.4 阶段 C —— C2 实施 → 归档（KPI **1/3 → 0/3**，**最大工作量 57 task**）

| 子步 | 内容 |
|---|---|
| **C-a** | T0.1 基类调用序列（`SimObject` 提供「`begin` → 模块自读 → `enforce`」路径）⚠️ **不得**出现「基类 `enforce` 先于模块 `load_param`」的顺序 + T0.2 嵌套失败传播（**同时解掉 R-5 的 AC-08**） |
| **C-b** | T1（10 个 ChStream 家族类型）+ T2（10 个 SimModule 家族类型）迁移 |
| **C-c** | T3（4 个**依赖 C1** 的类型 —— ⚠️ **C2 `tasks.md:43` T3.1 须加一条可机械判定的前置命令**（critic SHOULD_FIX）：例 `openspec list --json` 中 C1 已非 active，或 C1 的 `routing` 判别/拒绝测试存在 ⇒ 否则「前置」只靠人读）：`TrafficGenTLM` / `MemoryTLM` / `GmmuTLM` / `GpuNoC`）⇒ **必须等阶段 A 完成** |
| **C-d** | T4 `ParamRules` 机制退役 |
| **C-d′** | ⚠️ **修订 `param-type-conversion` 的 Scenario（critic / oracle）** —— `openspec/specs/param-type-conversion/spec.md:50` 的 Requirement **已**声明「转换 SHALL 在模块内完成、不由 `ModuleFactory` 侧承担」⇒ **不是双真值契约**;但其 **Scenario**（`:57` / `:62`）仍写「**ModuleFactory SHALL convert to 100000 / 4096 before storing**」⇒ **Scenario 与 Requirement 矛盾**;且 C2 change 内 `grep param-type-conversion` **零命中** ⇒ 无人负责修 | 判据 = `grep -c "ModuleFactory SHALL convert" openspec/specs/param-type-conversion/spec.md` = **0**（**现为 2 ⇒ 必红**） |
| **C-e** | T5~T8：测试 · 台账 · spec delta · 验证 · **独立验收** · 残留清点 ⇒ 归档 ⇒ KPI 0/3 |

### §1.5.5 风险与已知事实（含**已评估备选及否决理由**）

| # | 风险 / 冲突 | 处置 |
|:--:|---|---|
| 1 | **KPI 3/3 已满** ⇒ 阶段 A~C 期间**不得立新提案** | C1 归档即释放 1 slot |
| 2 | **C2 的 T8.1 基线数字已过期**（写 `883191 / 2371` ⇒ 现 **883242 / 2372**） | C2 实施前**必须重测**并回填 |
| 3 | **C2 是最大风险**：改基类调用序列 + 20 个类型迁移 + `ParamRules` 退役 | 拆 T1/T2 两批做;每批独立验收 |
| 4 | **R-5 与 C2 都改 `ModuleFactory`**（不同位置 ⇒ 文件冲突面可接受） | ⚠️ **但实际不可并行** —— 本仓**单 worktree** + `role-claims.json` **每 role 唯一 active claim** ⇒ 不存在两个 builder ⇒ **不会发生 rebase**（R-5 design D-4 的「先合并者 rebase」是理论表述，本仓不成立）⇒ 冲突面退化为**纯串行** |
| 6 | **KPI 活口径处数三个 change 不一致** —— C1 `tasks.md:83` T7.2 与 C2 `tasks.md:87` T8.7 写「**6 处**」，而 R-5 `tasks.md:59` T6.2 写「**7 处**」⇒ **首个归档**的 change 回填 KPI 时必有一处写错 | 归档前以 `grep -rn "KPI\|3/3\|2/3" AGENTS.md` **实测**处数;三个 change 的 KPI 回填 task 按实测**统一改写**（当前实测 = **7 处**） |
| 7 | **R-5 的 delta spec 宽于实现**（`spec.md:7`「每一个」vs 只扫顶层）⇒ 默认 archive 冻错契约 | 见 §1.5.3 **B5** 的 BLOCKER 前置（收窄 Requirement + C2 预登记 MODIFIED） |
| 8 | **C1 的 AC-01 当前即红**（D-1.2 仍列 3 个已删类型）⇒ A5 独立验收不可达 | 见 §1.5.2 **A0** |
| 9 | **R-5 的 AC-07 下限不可达**（proposal `883336/2393` vs 实测 `883242/2372`）⇒ B5 独立验收不可达 | 见 §1.5.3 **B0** |
| 5 | **C1 已改 `include/core/sim_object.hh`**（新增 `reject_unsupported_param`）⇒ C2 的 T0.1 **改同一文件** | 已知冲突面;合并时 rebase |
| 10 | 三个 change 归档后 **KPI 归零** | 后续可开新提案（事实，非风险） |

#### 已评估备选（**否决**，落盘以免下轮重提）

| 备选 | 提出方 | 裁决 | 否决理由（三条，均可判定） |
|---|---|:--:|---|
| **A′** C1 以 B1 完成态提前归档（剩余 **19 task** 标 `[~]` 移交 C2），归档当日即 3/3→2/3 | metis | **否决** | ① 移交对象含 C1 的**签批级判据** —— AC-08（`routing` 处置落地）与 delta S5 未满足就做默认归档 = 与 §1.5.3 B5 同类的**撒谎归档**，不是普通 task 搬家;② 若 B3 改入 C2，C2 T3.1「前置：B1 与 B3 完成」变为**内部依赖** ⇒ 需 C2 新增 routing 裁决 task 并修订 T3.1 = **change 边界修订**，须走显式修订流程而非顺手移交;③ `[~]` **不被 `openspec` 的 task 状态机识别**（只认 `[ ]`/`[x]`）⇒ 带 19 个 `[~]` 归档 ⇒ 归档目录显示 19 个未完成 task ⇒ 与「✅ 已完成归档」语义直接冲突 |
| **并行** R-5 ∥ C2 同时开工 | oracle（理论）/ Q-6 选项③ | **否决** | 本仓**单 worktree** + `role-claims.json` 每 role 唯一 active claim ⇒ 不存在两个 builder;且 `docs/guide/cep-usage.md:60`「KPI Proposed **并发 ≤ 1**」⇒ 人力约束独立成立（与文件冲突无关） |

### §1.5.6 本规划自身的**验收判据**（⛔ 规划也须可被证伪）

> 缺口来源（critic 2026-11-18）：「依赖图画得漂亮但**没有任何机关**」—— 顺序目前只由文风保证 ⇒ 下一个 session 可以直接从 C2 T1 开工而不违反任何机制。

| # | 判据 | 载体 / 命令 | 判据形态 |
|:--:|---|---|---|
| **P-1** | **依赖边可验证** —— 每条箭头带 `file:line`;脚本逐条 `grep -n '<关键词>' <file>` 命中该行 | `scripts/test/` 下的 `rddf_stage_gate.sh`（**待建**）（新增） | 命不中即规划失效 |
| **P-2** | **顺序由机关保证**（不是靠文风） | 同上脚本断言：(a) 前一阶段 change 的 `tasks.md` 无 `^- \[ \]`;(b) 后一阶段的 T0/T1 首条任务**未**出现在 `git diff --name-only` 的变更集里 | 红灯即**禁止**进入后一阶段 |
| **P-3** | **AC 可寻址** —— 每个 AC 必须有「载体文件 + case 名」,脚本 `grep -F '<case 名>' <文件>` 命中 | 同上脚本 | C1 的 AC-01~AC-09 需补 carrier 映射表（当前**无** ⇒ A5 会靠人肉复述） |
| **P-4** | **跨阶段判别力不丢** —— C 阶段结束后 `grep -c 'TEST_CASE' test/test_param_effect_landing.cc` ≥ A 阶段归档时的值 | 同上脚本 | 理由：C2 的 `load_param` 重写会覆盖同一读取面，可能**静默吃掉** A 阶段落的态①/③ 判别 |
| **P-5** | **KPI 回填不是「改数字」** —— 每阶段归档 commit 须同时满足：`ls openspec/changes \| grep -v archive \| wc -l` == 目标值，**且** 7 处活口径文件在同一 commit 内全改 | 同上脚本 | 列出未同步文件数 = 0 |
| **P-6** | **风险可失败** —— 每条风险配一条会红的判据 | 例：风险 9 ⇒ `grep -n '883336' openspec/changes/cpptlm-config-type-registration-align/proposal.md` 必须为空 | 硬编码下限残留 = 红灯 |

> ⛔ **P-1~P-6 落盘前，规划只是「文风」** —— 本节由 builder 在阶段 A 的 **A0** 一并落地（脚本 + ctest 注册）。

---

## §2 · 实施路径（**5 波**）

> ⚠️ **波 0 是 2026-11-18 新增** —— 三个 active param change（118 task）构成当前最大剩余工作，且是 §1 里 CEP 域波次的前置（KPI 已满 ⇒ CEP 域新提案需等它归档释放 slot）⇒ **排在最前**。逐子步见 **§1.5**。

| 波 | 内容 | 依赖 | 为什么这个顺序 | 成本 |
|:--:|---|---|---|:--:|
| **波 0** | **框架治理三 change**：阶段 A（C1 收尾 → 归档）→ 阶段 B（R-5 → 归档）→ 阶段 C（C2 → 归档） | 阶段内串行;**C1 → C2 是硬前置**（C1 D-5 / D-3）;R-5 ∥ C2 可并行 | ① KPI **3/3 已满** ⇒ 不先归档一个，**任何新提案都立不了**;② C2 的 T3.1 明写「前置：effect-landing 完成」⇒ **C1 不先做完，C2 会被卡**;③ R-5 修的是**静默失真**缺陷（装配成功但模块缺失）⇒ 严重性高于 C2 | A ~2 d · B ~3 d · C ~8 d |


| 波 | 内容 | 依赖 | 为什么这个顺序 | 成本 |
|:--:|---|---|---|:--:|
| **波 1** | **R-1a**（45 份 Purpose，分 3 批）· **D-1**（⛔ 旧「38 份」口径已废 ⇒ **乙层 8 份**全补 + 甲层封存+抽检，见 §6.2 S3-5）· **D-2**（流程，零成本） | 无（判据已就位） | ① 判据 / 生成器 / 载体范式**全部就位** ⇒ **零新设计**；② D-1 可与 R-1a **并行**（不同角色：verifier vs builder）⇒ 但 KPI 并发 ≤ 1 ⇒ OpenSpec change 仍**串行**（D-1 属 verifier 角色，不占 slot） | 3 d |
| **波 2** | **B-1a**（3 条 invariant 建模） | **architect 先出判据** | ① 这是 CEP 本体的**真功能缺口**（不是文档债）⇒ 收益最高；② 与波 1 **无依赖** ⇒ 建议**并行启动 architect 方案**（方案不占 KPI slot）；③ 判据未定义就实施 = builder 自行发明 invariant 语义 = **越权** | 1.5 ~ 3 d |
| **波 3** | **R-1b**（26 份正文裁剪） | 另立 change + architect | ⚠️ **会动 `## Requirements` 段** ⇒ 契约内容变更 ⇒ 需更严判据（不能沿用 Purpose 那套）⇒ **必须另立 change** | 1.5 d |
| **波 4** | **ARCH 批（8 项）** + ~~**OWNER 批（8 项）**~~ ⛔ **OWNER 维度已废除**（用户 2026-11-20）⇒ 8 项按 **A 2 / B 5 / C 1** 三分派往 **I-11**（B 类 openspec change）· `cep-doc-hygiene-kpi-and-impl-state`（A 类）· **Q-12**（C 类，architect）+ **R-6** | architect 判权 | 全部是「**要定一个 canonical / 权威归属 / 措辞口径 / 编号空间**」的取舍 ⇒ **builder 不得自行拍板** ⇒ 与波 1~3 可并行推进（不同角色）但**不可并行实施** | 3 ~ 4 d |

**排序理由**：
- **波 0 排最前**：它是唯一的**解锁波** —— KPI 已满 ⇒ 不归档一个就开不了任何新提案；且 C2 硬依赖 C1 的产出。
- 波 1 是**同一条「规范面可信度」主线的第 3 段**（Purpose 占位 75 → 45 → 0）⇒ 固定成本只付一次。
- 波 2 是**唯一真功能波** ⇒ 价值最高，但**判据必须 architect 先给** ⇒ 建议**今天就启动 architect 侧**，波 1 期间并行。
- 波 3 **必须另立 change** —— 把「动 Requirements 段」与「只动 Purpose 段」混在一批 = 把两档风险并成一档 ⇒ 判据强度会被稀释。
- 波 4 单独隔离：全部是「定 canonical」类取舍 ⇒ 与波 1~3 无依赖但**不可并行实施**。

---

## §3 · 需要用户 / architect 决策的**开放问题**（**7** 项 · **历史快照** · 编号 `H-Q1~H-Q7`）

> ⛔ **本节是 2026-11-16/18 历史快照，已被 §6 取代；⛔ 本节编号（`H-Q1~H-Q7`）与 §6.4 队列 1 的 `Q-*` 完全不同语义，⛔ 不得据此派工。**
> ⛔ **§3 的 7 项已全部结清**（其中 4 项的结论已落在 §6.4 队列 1/2）：`H-Q1`（R-1b 正文裁剪）→ 队列 1 之外，**不排期** · `H-Q2`（D-1 验收债补到哪一层）→ **已由 Q-8/Q-13 拍板**（分两层 + 统一覆盖）· `H-Q3`（编号空间收敛）→ **已由 Q-6 拍板** · `H-Q4`/`H-Q5`/`H-Q7`（CPUTLM 三键 / RouterTLM.routing / PcieControllerIP 三键）→ 属 §1.5 框架治理三 change，**2026-11-18~19 已全部归档** ⇒ 本节 7 项**无一为当前待决项**。
> ⚠️ **本节保留原正文逐字不改**（历史对照），但 §2 波次表内的「D-1（38 份）」等旧数字**已失效** ⇒ 读数一律以 §6 为准。

| # | 问题 | 选项 | 建议 |
|:--:|---|---|---|
| **H-Q1** | **R-1b（26 份正文裁剪）是否值得做？** —— 裁剪**会动 `## Requirements` 段**（契约内容）⇒ 需重新定义判据 + 可能丢语义 | ① 做（另立 change + architect 参与）② 不做（`--strict` 保持 26 份红，**非 strict 口径仍全绿** ⇒ 不阻塞使用）③ 只做「拆 Requirement 而非删字」子集 | **②/③** —— `--strict` 红的这 26 份**不阻塞**任何非严格用法；收益主要是「CI 严格模式绿」⇒ 先量 CI 是否真的用 `--strict` |
| **H-Q2** | **D-1（38 份验收债）补到哪一层？** —— 38 份是 2026-11 起的**全部** CEP change | ① 全补（38 份 × ~0.3 d ≈ 11 d）② 只补**有契约/判据变更**的（估 10 ~ 12 份）③ 立「验收债豁免规则」（老 change 免补，仅新 change 强制） | **③ + ②** —— 验收流程**当时未强制** ⇒ 追溯 38 份是惩罚性成本；建议**规则化**（新 change 强制 verifier 报告）**并**只补高价值子集 |
| **H-Q3** | **E-1（编号空间冲突）怎么收敛？** | ① 单源登记册（本册）+ 旧编号只作历史快照（本册已采用）② 重编号（`R-1`~`R-20` → 语义前缀，如 `SPEC-01` / `ADR-01` / `PROC-01`）③ 前缀分区 | **① + ②** —— 本册已做到「单一真源」；但仍需给旧编号加**弃用标记**（本册 §1 的编号与历史编号冲突 ⇒ 建议新登记一律用 `S-01` 起的**新前缀空间**，旧 `R-*` 冻结为历史） |
| **H-Q4** | **`CPUTLM` 三键怎么处置？** —— `pattern`（**`CPUTLM` 全仓 14 处全是 `SEQUENTIAL`** ⇒ 合法值唯一 ⇒ 态③ 伪参数;⚠️ `TrafficGenTLM` 的 `pattern` 有 4 个值 —— **两者不同类，别混**）· `start_addr` / `request_interval`（成员**真被**运行期读 ⇒ 态① 可实现） | ① 全态①（接上 `pattern` 需实现多模式流量生成 ⇒ **改全局流量行为**）② `pattern` 态③ + 其余态①（**推荐**）③ 全态②（拒绝三个键，配置侧全清） | **②** —— `pattern` 合法值唯一 ⇒ 伪参数（态③）;⚠️ 态③ 落地形态 = **给 CPUTLM 新增读取面**（现无 `on_config_loaded`）+ 读入校验 ∈ {`SEQUENTIAL`} + 非法值失败 —— ⛔ **不顺手做模式实现**（那是新功能）。`start_addr` 接上 = 消除「**配置意图与行为不符**」（实测 **5 份配置 / 15 个实例**：4 份 example_emitter 家族 14 处 + `tlm_e2e_test.json` 1 处 = 默认值 `0x1000` ⇒ 零变化）⇒ **不是「互相撞地址」**（共享存储下多 CPU 同地址是合法场景）;⚠️ 断言影响面需**以「统计分布对比」复核**，不可预设变红 |
| **H-Q5** | **`RouterTLM.routing` 走态② 还是态③？** —— 该键合法值唯一（设计如此） | ① 态③（读入 + 校验，非法值 ⇒ 装配失败并报「键名 + 合法值集合」）② 态②（生成方停产出 + 拒绝） | **① 态③** —— 合法值唯一 ⇒ 该键的价值是「挡住拼错」而非「选行为」⇒ 正是 D-2 态③ 的定义;⚠️ **必须先于 C2 的 T1**（C1 D-3）;⚠️ 实测 `routing` 在 `configs/` **零命中**（仅由 `GpuNoC::simulate_instantiate` 运行时生成）⇒ 态③ 在真实路径**永不触发** ⇒ 态③ 与态② 实际等价安全 |
| **H-Q6** | **阶段 B（R-5）与阶段 C（C2）谁先？** | ① R-5 先（缺陷更严重：装配成功但模块被静默丢弃 ⇒ **结果静默失真**）② C2 先（工作量 57 task，早启动早铺开） | **①** —— R-5 的缺陷是**结果层面的静默错误**（最难被发现的一类）;且 R-5 先做完能给 C2 提供「类型面已校验」的底座。⚠️ **「并行」选项已删** —— 本仓单 worktree + `role-claims.json` 每 role 唯一 active claim ⇒ 实际不可并行（且 `cep-usage.md:60`「Proposed 并发 ≤ 1」）|
| **H-Q7** | **`PcieControllerIP` 的 3 键怎么切？** —— `bar_bases` / `bar_sizes` / `link_speed_gts` 在本类**零消费**（facade 故意「Reads sizes dynamically from PcieEndpointIP（no static snapshot）」）;⚠️ 但**同名不同语义** —— 板级 `dgpu_board_shell.cc:76-79` 读的是 **`pcie_ep` 节点**的 `bar_sizes`（`PcieEndpointIP.bar_sizes_` **有**消费面:写 config space BAR 寄存器）⇒ 同名键一个有一消费一个零消费 | ① 三个键**全部**态②（显式拒绝 + 清 3 份配置）② `bar_sizes` 移入「表 A · 板级直读」+ 其余两键态② ③ 全部态③ | **①** —— `PcieControllerIP` 上**无**消费面 ⇒ 态②;⚠️ ADR-X.17 D-3 的「同一类型内不得混合」对 `PcieControllerIP` **技术上不触发**（板级读的是 `pcie_ep` 节点）⇒ 真风险是「**同名不同语义**」⇒ 处置时须**逐节点分清**，⛔ 不得按键名一刀切。② 会把零消费的键登记成「有消费」⇒ **造第二份真源** ⇒ 否决 |

---

## §4 · ⚠️ 口径声明（避免下轮误判）

1. **本册只收「当前有效」的登记。** 台账 §11 历史 Entry 的 `R-*` 编号属**历史快照** —— 同一编号在不同轮次含义不同（例：台账 `:5590` 的 `R-1` = 「obj §4.0 表体 15 行」vs `:24283` 的 `R-10` = 「superseded spec 存在性裁定」）⇒ **引用残余时必须带轮次/Entry 上下文**，⛔ 不得只写 `R-N`。
2. **三处数字已用权威口径订正**（2026-11-16）：
   - 「正文 >500」= **26** 份（openspec 自身判据）—— ⛔ 非我早前的近似统计 **28** 份（近似法按「整段长度」切，与工具的「描述段长度」口径不同 ⇒ **以工具为准**）。
   - ADR 归因例外「被已签批 ADR 挡住」= **12** 条（rationale 含「挡」）—— 其中 `carrier_buildable_id_blocked` 这个 **disposition** 本身是 **10** 条 ⇒ 两个数口径不同，勿混用。
   - 缺独立验收报告 = **38** 份（2026-11 起归档的 CEP change）—— ⛔ 非 Entry #438 早前估的「5 份」（那 5 份只覆盖了 11-13 之后的 5 个）。
3. **`--strict` 红数 = 55**，构成 = **45** 份 Purpose 占位 + **26** 份「正文 >500」（两者有重叠）⇒ ⛔ **不是** 71。
4. **本册存在两套基线日期**（2026-11-18 更新）—— §0.1（当前）/ §1.5 / §2 波 0 是**新口径**;§0.2 与 **§1 的 CEP 域清单（24 项）仍是 2026-11-16 口径，本轮未重测** ⇒ 引用 §1 数字时必须带日期，不得与 §0.1 混读。

---

## §5 · 关联

> **本轮（2026-11-18）规划与实施记录**：
> - 三个 active change：`openspec/changes/cpptlm-module-param-effect-landing/`（**C1**）· `cpptlm-config-type-registration-align/`（**R-5**）· `cpptlm-module-param-migrate-batch2/`（**C2**）
> - 台账 `obj-cep-platform.md` —— Entry **#480**（R-5 立项）· **#481**（设计类型归档：GPUTLM / SharedMemoryTLM / MemoryClusterTLM / GpuSocTLM + 3 配置 + 1 孤儿模板）
> - 类型归档判据与逐项影响面 —— R-5 的 `design.md` **D-8**（ArchForge 契约状态 = 「该 IP 有无计划」的真源）
> - 跨仓待办（不在本仓）—— ArchForge 三份契约状态改「⏸ 已归档」+ `coherent_xbar.md` 的 Header 路径漂移



- 台账 `obj-cep-platform.md` —— **Entry #444**（2026-11-16 全量核对 + 立项）· **#445**（批 3 收口）· **#446**（批 3 verifier 验收）· **#441**~**#443**（批 1）· **#438**（上轮全量梳理）
- 批 1：`openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/` · 批 3：`openspec/changes/archive/2026-11-16-cpptlm-spec-purpose-batch3/`
- 判据载体：`scripts/test/cep_spec_purpose_batch1_check.py` · `scripts/test/cep_spec_purpose_batch3_check.py` · `scripts/test/cep_spec_purpose_batch3_gen.py`
- 载体：`test/spec_purpose_batch1_attribution.json` · `test/spec_purpose_batch3_attribution.json` · `test/spec_purpose_batch3_reconcile.json`
- 未结义务导航：`docs/guide/cep-usage.md` §12.8（**导航，非状态源** —— ⚠️ 其「ADR 归因 18 条」已 stale，实测 **19** 条）
- 外部阻塞登记：`test/cep_b3b2_deferrals.json` · `test/cep_invariant_deferrals.json` · `test/cep_adr_structural_deferrals.json`

---

## §6 · CEP 缺口清单（有效级排序）+ 后续立项计划（**v4** · 2026-11-20）

> **性质**: 本节的**当前有效清单**（§1 为 2026-11-16 历史对照）。本节**不新建第三个登记面** —— 它是本册（canonical 登记册）的一个新节，⛔ 不新建 `.rddf/roadmap/` 下的新文件（守 Entry #334 ④「不新建登记面」否决理由，`obj:18622`）。
> **审查轨迹**: **metis 1 轮**（gap 分析）→ **oracle 1 轮**（对抗式，判 REJECT · 10 项必修）→ **critic 3 轮**（第 1/2 轮 REJECT · 第 3 轮 **APPROVE_WITH_CONDITIONS**）⇒ 每轮 findings **逐条复验**（metis 声称 shell 被拒的项由 build 复测；oracle 的 10 项全成立；critic 的 4 项 BLOCKER 全成立）⇒ **本节是审查收敛后的产物，不是初稿**。
> **编号纪律**: 本节**不新建编号空间** —— 缺口以 `S3-x` / `S2-x` / `S1-x` 标注（⛔ 与登记册历史 `R-*` 空间冲突，见 §4），其与登记册 ID 的映射写在 §6.3 归属表。

### §6.1 有效级定义（严重度）与「谁能做」**严格正交**

| 级 | 判据（可点数） |
|---|---|
| **S0 宣称不成立级** | 存在「已完成 / 已达成」宣称，但其证据面与实况**不一致**（或终态完成定义与实况不符） |
| **S1 能力不成立级** | 平台**自己声明的能力**实际不存在 / 不可用（真功能缺口） |
| **S2 契约失真级** | 契约面（spec / ADR 归因 / 计数口径 / 台账措辞）与实况不一致 |
| **S3 治理流程债** | 登记 / 编号 / 同步 / 追溯类债；不改变能力 |
| **S4 外部阻塞级** | 本仓不可推进（外部规范不可读 / 跨仓待办） |

`needs:` ∈ {architect, user, builder, planner, external, PROC}，**不并入严重度**。
> ⚠️ **枚举订正（2026-11-20 · per metis 歧义 8）**：旧枚举含 **`owner`** —— 该维度**已由用户 2026-11-20 废除**（本仓无独立 owner 角色 ⇒ 不可执行），⛔ **不得再用**；另**补入 `planner`**（S2-5 / S2-6 / S3-6 / S3-7 / S3-12 实际都由 planner 承担，旧枚举漏列 ⇒ 与 P-2「严重级与 needs 正交」的机判面自相矛盾）。
> ⚠️ **注意**：`BACKLOG` 出现在 §1.4 的状态码表（`R-6` / `A-1` 行），但**不是** §6 的 `needs` 取值（§6 的 `S3-4` 直接写 `BACKLOG` 于 `needs` 列）⇒ 两处口径不一致，⛔ 待收敛（**登记在 backlog，不在本轮改**）。

### §6.2 缺口清单（CEP 域 · **23** 项）

#### S0 · 宣称不成立级 —— **0 项**

> **诚实结论**：初稿曾列 2 项 S0（DoD 7 口径 / 引擎强度表），经三轮复核**均降级**。critic 独立扫描 `obj §0 DoD`（`:34-76`）· `§6.2 B1/B2/B3a/B3b`（`:528-553`）· Entry #232/#233/#236/#264/#274/#332/#334 · `cep_freeze_check.sh:40-139` · 三份 deferrals JSON · `protocol-engine.md:40-49` · `ADR-CEP-20:82-147` · `cep-usage.md:38-70,190-201` · 登记册 `:15-25,176-195` ⇒ **未发现「已完成宣称 + 证据不一致」实例**。
> ⚠️ §0.1 的 stale（active 3 / KPI 3/3）方向是**过 restrictive**（虚报已满）⇒ 不制造假绿 ⇒ 按 **S2-6d** 处理。

#### S1 · 能力不成立级（**2** 项 · 真功能缺口）

| # | 缺口 | 证据 | `needs` |
|:--:|---|---|---|
| **S1-1** | **5 条守门 invariant 未建模**：`CEN-Ib8`（engine egress 可观测面）· `INV-CEP-08-IC1`（J3 通用形式可判定载体）· `INV-CEP-09-IC2`（多 VC 仲裁器）〔**批 1**〕· `INV-CEP-13-IC1` · `INV-CEP-13-IC3`〔**批 2**：schema 13→14 + `ADR-CEP-12` 联动 + litmus change〕。⚠️ 66 分母**永不可缩** · `INV-CEP-04-IC4` **永久跳空**（编号位 67）· 新增 carrier 不得占用 67 编号位 —— ✅ **2026-11-21 已全部有判定级载体**（批 1 = `CEN-Ib8`/`INV-CEP-08-IC1`/`INV-CEP-09-IC2`；批 2 = `INV-CEP-13-IC1`/`INV-CEP-13-IC3`，change `cpptlm-cep-invariant-modeling-batch2` 已归档）⇒ `⏭-unmodelled` 计数归 **0**、`⏭-pending` 归 **0**。⚠️ ⛔ **「66/66」只能读作「66 条全部有载体」**，⛔ 不得读作「每条都已给出判定结论」；`INV-CEP-04-IC4` 仍**永久跳空**（编号位 67）| `test/cep_invariant_deferrals.json`（**5** 条）· `scripts/test/cep_invariant_map_check.py:18` 判据 ③ =「有牙载体 ∪ 授权⭕ == 66 集合」 | architect（批 1 判据）→ builder（批 1 实施）；user + architect（批 2） |
| **S1-2** | **档 2（协议级）观测面只覆盖 1 族 / 1 拓扑 / 1 实例**（`mesif` 单族一次实例）。⚠️ `test/cep_evidence_tiers.json:45` note 明写「本表**不表示**各族的证据强度已完成」⇒ `tilelink_c` 的 `tier2_applicable = true` 只是**必要条件**。⚠️ **硬依赖** `CEN-Ib8`（engine egress 可观测面）⇒ **不可独立排期**。⚠️ data-value 维生产流恒 0（无本地缓存数据版本）⇒ 可达形态**仅限**「注入陈旧副本」负控 | `test/cep_evidence_tiers.json` · `07:719-737` | builder（依赖 S1-1 批 1） |

#### S2 · 契约失真级（**7** 项）

| # | 缺口 | 证据 | `needs` |
|:--:|---|---|---|
| **S2-2** | **「引擎」无口径定义 ⇒ 计数 5 vs 6 并存**：`07:138` + `protocol-engine.md:44` = **5**（协议族引擎）vs `ADR-CEP-20:88,126` + `cep-usage.md:50` = **6**（engine 文件，多 `cohif_protocol_engine.hh` = Null 外壳）⇒ **两者数的不是同一物，但都没定义「引擎」**。⛔ **禁止手写分级表**（`cep-usage.md:50`「⛔ 本节不给分级清单（会腐化）」+ 指向 `obj §4.0`/`§4.0-a`/Entry #246）⇒ 修法 = 定义三词词表 + 强度读法只指向既有载体 | 同左 | architect |
| **S2-3** | **B3b 分母两套并存**：B3b-1 **2/2** · B3b-2 **0/4** · coverage 文档「1/6 → 2/6」· 终态「2 达成/分母 6」⇒ 4 处并存，易把 `2/6` 读成 `2/4` | `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md:109-114` | architect（并入 S2-2 一次裁决） |
| **S2-4** | **「DoD 12/12 ✅ ⇒ 平台完成」这条具体读法路径无拦截点**（`obj:553` 挂起≠拒绝 + `obj:18451` N=0 永不 ✅ + `obj:34` 标题含「2030-Q1 终」**已部分覆盖**；真缺口 = 该具体路径无拦截）⇒ **S2 非 S0**（12 项 DoD 逐项有证据支撑） | `obj:34-76` / `:553` / `:18451` | architect |
| **S2-5** | **`obj §4.0-a` 8 行摘要表的 UALink 行**：2 条 ⛔ 禁令语**故意**不在摘要内（Entry #334 ratify 形态：≤40 字摘要 + 原文快照落 `docs/implementation/`），表下 `:18643` 已有指引 ⇒ **不是**缺口本体，真缺口 = 指引语强度。⛔ **不得**改摘要行 | `obj:18624-18643` | planner |
| **S2-6** | **口径 stale 集（4 处）**：a `docs/guide/cep-usage.md` 的「**ADR 归因**(18 条)」行（⚠️ **行号漂移**：登记册旧写 `:195`，实测 **`:220`** ⇒ ⛔ 改用**唯一措辞**而非行号；写「18 条 · doc_layer_manual_review **4**」⇒ 实测 **19/5**）· b `obj:971`（L2「至今未修」⇒ coverage 文档 `:36,39` 已改 ⇒ 过期的是台账）· c `cep_freeze_check.sh:52-54`（「目录不存在 ⇒ 0 匹配」⇒ 实有 9 份 `.md`）· d §0.1（active 3/KPI 3/3/specs 135 ⇒ 实测 0/0-3/137） | 见左 | planner（a/b/d）· builder（c） |
| **S2-7** | **R-2：`moesi_gpu`/`mesif` 13 键无仓内 golden** —— 二者**不在** `repo_owned_instance_ids`（6 个）⇒ 权威在本仓、无第二来源 | §1.2 | architect |
| **S2-8** | **R-3：ArchForge 缺席时 `cep_archforge_alignment_check.sh` 打中文「跳过」+ `exit 0` ⇒ ctest 记 **Passed** ⇒ CI 假绿** | 本机 N/A（`/workspace/main/ArchForge` 在位 ⇒ 闸真跑 7 项全绿 ⇒ 缺口**仅 CI 成立**）；⚠️ 真实缺陷 = **无 `[SKIP]` token**（`scripts/test/cep_archforge_alignment_check.sh:30-34` **有**中文跳过提示）| **✅ 已裁（用户 2026-11-20）方案甲** = 缺席改打 **`[SKIP]`** + CMake `SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"` ⇒ ctest 显示 **Skipped 而非 Passed**（对齐 `cep_decl_alignment_check` 先例，⛔ 不 checkout ArchForge）⇒ `cep-archforge-ci-failclosed`（≈3 行）|

#### S3 · 治理流程债（**12** 项）

| # | 缺口 | `needs` |
|:--:|---|---|
| **S3-1** | **E-1** `R-*` 编号空间被历史轮次复用 ⇒ 残余**不可寻址**（修法：旧 `R-*` 冻结为历史快照，新登记用**语义前缀** ⇒ **本节以登记册 ID 为准，不新建第三套编号**） | architect |
| **S3-2** | **R-9** 同一事实 **5 处**手工维护 ⇒ 定 canonical | architect |
| **S3-3** | **D-2 KPI 活口径同步面 = 6 文件 / 7 处**（权威清单 = 既有闸 `rddf_stage_gate.sh:243-250` 的 `LIVE_FILES`；活口径行取值全部 == `0/3`）。⛔ **判据不得用子串存在性**（见 S3-11）。另：其中 2 处单行 **6.3~6.8K 字符**（归档流水堆积）⇒ 与 S3-6 同源 | PROC |
| **S3-4** | **R-6** `openspec` 版本未 pinning | BACKLOG |
| **S3-5** | **D-1** 归档 change 缺独立验收报告 —— **统一口径（2026-11-20 复验 · per metis B1/B2）= 全仓 164 个归档目录中真缺 135 份**（21 份名称 join 命中 + 8 份「已有验收报告但文件名不匹配」⇒ 已验收 29）。**✅ 已裁（用户 2026-11-20，含 Q-13 扩容）**：分两层处置（**乙层 8 份** = 5 CEP + 3 非 CEP，归档 ≥ 2026-11-12 ⇒ **全部真补**；**甲层 126 份** ⇒ **永久封存 + 抽检 5 份**）+ **废除「0.5 d / 38 份」**改「每份 **≥0.5 d 下限**」+ **机关** `scripts/test/archive_verify_check.sh`（**统一覆盖，不按 cep 前缀特判**；封存边界 = 归档日期 **< 2026-11-12**；**先补后上闸**）<br>⚠️ **事实更正（metis B1 复验成立）**：旧表述「甲层 35 份 = 规则缺位期 · 仓内最早 verify 报告 = 2026-11-12」**为假** —— 实测 **11-06/11-07 已有 8 份 verifier 报告**（报告头明文声明验收对象，覆盖 8 个甲层 change），只是文件名未遵 `<change>-verify.md` 规范 ⇒ **分界线不是「有无实践」而是「命名与四段格式是否统一」**（⚠️ **计数单位是 change 不是报告** —— 一份报告可覆盖多个 change，per critic BLOCKER-1）；⚠️ **旧「40」（§0.1）= 只做名称 join 未查报告内容 ⇒ 虚高 8**；⚠️ **旧「43」是混合口径**（乙层 8 含 3 非 CEP + 甲层 35 是 CEP-only）⇒ **不匹配任何单一规则** ⇒ 已统一为 **135** | builder + verifier（落实在 `cep-d1-archive-verify-backfill` ⇒ **队列 2 的 I-10**）|
| **S3-6** | **L3** 4 份文档 KPI 行单行 6.3~6.8K 字符 + `obj §5.2` 订正链 8.5K ⇒ token 成本债 | planner |
| **S3-7** | **L4** 3 处 improvement 状态漂移（`cpptlm-cep-dcoh-nvlink-refkind-migration` / `cpptlm-cep-ualink-coherence-engine` / `cpptlm-cep-basis-source-kind-and-cell-free` 仍写「提案/已批准实施」但已归档） | planner |
| **S3-8** | **OWNER 批 —— ✅ 已裁（用户 2026-11-20）：废除 OWNER 维度**（本仓无独立 owner 角色 ⇒ 该分类无执行者）。**8 项逐条复验全部仍 live**（⛔ 此前「4 项」是窄检口径）。**三分（per 裁决记录 §10.9.3 · 边界判据 = 落点是否在 `openspec/specs/``）**：**A 类 2 项**（R-17 2 份来源 change 归档目录缺失 · R-19 `AGENTS.md` LWC handler 名 stale）→ **`cep-doc-hygiene-kpi-and-impl-state`**；**B 类 5 项**（R-12 `abi_guards.h` 悬空 · R-13 spec 内旧状态行 · R-14 / R-15 / R-20 的 spec 引用失效 —— **落点全在 `openspec/specs/`** ⇒ 改契约）→ **`cep-spec-stale-refs-r14-r15-r20`**（openspec change）；**C 类 1 项**（R-18 doorbell 配置 vs hardcoded）→ **architect（Q-12）** ⚠️ **R-12 / R-13 原归 A 类是错的**（落点在 spec 内 ⇒ A 类单元 AC-6 禁止 specs diff ⇒ 物理不可执行）| builder + architect |
| **S3-9** | **§1.5 整节过时**（框架治理三 change 2026-11-18~19 全部归档 ⇒ 118 task 整节作废）+ §0.1 数字 stale ⇒ **不更新登记册会把执行者导向一个整节作废的计划** | planner |
| **S3-10** | **`obj §6.2` B2 行标题与登记形态的张力** —— `obj:529`（标题「逐条有牙载体 100% 覆盖」· per Entry #232 **改判**）vs `obj:530`（同节下一行即给「N 有牙 + M 授权⭕」登记形态）· `obj:531`（B2 ✅ per Entry #236）⇒ 属**读法级**张力，「是否够格称契约失真」**本身需 architect 判**。⛔ **不得**由 planner 原地改写 Entry #232 已授权措辞（`obj:14978-15004` 明写「**性质：改判**（⛔ 不是澄清）」· architect 裁决 + 双审 / planner 落 entry）⇒ 处置全部并入 **Q-2** | architect |
| **S3-11** | **🆕 既有闸 `rddf_stage_gate.sh` 的 P-5「KPI 同步」判据对 6 个同步面中的 4 个无鉴别力** —— 影子副本决定性变异实测：把 6 个 `LIVE_FILES` 的**活口径行**全改 `3/3` ⇒ P-5 **仅判红 2/6**（`roadmap.md` · `phase-10.md`）；另 **4 个仍判「已同步」**（`AGENTS.md:316` 历史快照行 · `obj-cep-platform.md` **382** 处 · `umbrella.md` 3 处 · `nvlink-sharp.md` 3 处）⇒ 根因 = P-5 用**子串存在性** `grep -qE "$_active/3"`（`:255`）⇒ 历史流水里的 `0/3` 即可满足。⚠️ **真实仓当前 6 处活口径取值全部 == `0/3` ⇒ 当前无漂移**（非假绿事故）；风险 = **未来漂移 4/6 抓不到** ⇒ 判据须改为「**锚定活口径行** + 该行取值 == 实测 `N/3`」 | architect（Q-6 判据形态）→ builder（I-0 实施） |
| **S3-12** | **🆕 台账 `obj-cep-platform.md:474` 行内自相矛盾** —— 同行散文写「`openspec/changes/` 当前 **3 个 active**」而同行结论写「当前 Proposed 并发 = **0/3**」⇒ 实测 active = **0** ⇒ **散文 stale**（结论对）⇒ 读法陷阱 ⇒ 修法 = 追加式订正（⛔ 台账 append-only） | planner |

#### S4 · 外部阻塞级（**2** 组）

| # | 缺口 | 现状 |
|:--:|---|---|
| **S4-1** | **B3b-2 = 0/4**（`cxl_2_0` frozen / `cxl_3_0_symmetric` deferred / `nvlink` deferred / `ualink` deferred；**4/4 `blocking_type = external`**）| 已登记 `test/cep_b3b2_deferrals.json` + 机器守门 `cep_b3b2_deferral_check.sh` + D-4 探针已真实执行（8/8 槽位全覆盖，4 external + 4 internal **全部 miss**）⇒ **不得**改写为已达成。⚠️ **E2 季度复审义务**（`obj:981`）仍无载体（**E1 已被 `cep_b3b2_d4_quarterly_review.sh` 覆盖** —— 该脚本已存在且是硬闸，刻意 NOT 注册 ctest）⇒ 见 **Q-11** |
| **S4-2** | **ArchForge 跨仓 4 项**（C1/R-5/C2 的 T7.8 遗留）：3 份契约状态改「⏸ 已归档」+ `coherent_xbar.md` Header 路径漂移 | §1.5.3 / §5 |

#### 仓级（非 CEP 域 · **4** 项 · 本轮**仅登记不排期**）

45 份非 CEP spec 的 Purpose 占位 · 26 份非 CEP Requirement 正文 >500 · `param-type-conversion` 的 Scenario 前提失效（`:61-62` 写「MemoryTLM **未准入**类型」，但 C2 已准入）· **R-10 / R-11 / R-16 孤儿 / 同名复制 / 未实施 spec 存废裁定（5 份，对象全为非 CEP spec ⇒ 与「排除 GPU/PCIe/dGPU 域」冲突 ⇒ 不排进本计划队列）**。

### §6.3 缺口归属表（**唯一认领**；⛔ 重复认领 / 无人认领即红）

| 缺口 | 归属 | 完成定义形态 |
|---|---|---|
| S2-6a / S2-6c | **I-1** | 活文档 / 脚本 ⇒ 旧串 `grep -c` = **0** |
| S2-6b / S2-6d | **P-1** | 台账 / 登记册 ⇒ **新增订正 Entry 存在 + grep 命中订正文本**（⛔ 不要求历史 Entry 旧串消失 —— 台账 append-only） |
| S2-5 | **P-3** | 台账 ⇒ 订正 Entry 存在 + **8 行摘要表 sha256 逐字未变** |
| S3-10 | **Q-2** | architect 裁决 ⇒ 裁决落盘；⛔ **不得** planner 原地改 |
| S3-3 / S3-11 | **I-0**（闸判据）+ **P-1**（口径行取值） | 闸 P-5 判据改「锚定活口径行 + 取值 == 实测」；口径行取值 == 实测 `N/3` |
| S3-9 | **P-1** | 登记册 §1.5 标作废（**本节落盘时已做**）+ §0.1 三数刷新 |
| S3-5 | **I-10** | 乙层 8 份追溯补验报告落 `.rddf/reviews/<change>-verify.md`（精确命名）· 闸 `archive_verify_check.sh` 存在 + 已注册 ctest + 三检全绿 |
| S3-8 · B 类 5 项 | **I-11** | 新建 1 个 openspec change（占 1 KPI slot）· 归档后 5 处旧串 `grep -c` == **0** |
| S3-8 · A 类 2 项 | **`cep-doc-hygiene-kpi-and-impl-state`** | R-17：2 份来源 change 归档目录缺失（补目录 or 改引用）· R-19：`AGENTS.md` LWC handler 名 stale（旧串 `handle_cr_transaction` `grep -c` == 0） |
| S3-8 · C 类 1 项 | **Q-12** | architect 裁决落 `.rddf/reviews/`（改代码对齐配置 vs 改契约承认 hardcoded）· ✅ **已落地 2026-11-21**（I-12 · change `cpptlm-doorbell-contract-alignment` 归档 · 3 份 main spec 合并 + 1 处 Requirement 改名 + ADR SU 追加）|
| 其余 | 见 §6.2 的 `needs` 与 §6.4 各行 | — |

> ⚠️ **归属表的兜底行（「其余 → 见 §6.2/§6.4」）本身不可机判「无人认领」** —— 已由上方 **S3-5 / S3-8 三个子行** + **队列 2 的 I-0~I-11** 覆盖原先落在兜底行的缺口；⛔ **新增缺口必须在本表显式列行**（⛔ 不得再落兜底行）。
> ⛔ **单元 ↔ 队列对账（metis B10 的 AC）**：`for f in .rddf/improvements/cep-*.md; do grep -q "$(basename "$f" .md)" .rddf/roadmap/objectives/obj-cep-platform-remaining.md || echo "无归属: $f"; done` ⇒ **输出须为空**（除 §6.8 已声明「不属本轮清单」的 5 份历史单元）。

#### §6.3.1 裁决吸收矩阵（architect 裁决 → improvement 判据回填 · **P-0 义务**）

> **义务来源**：architect 裁决记录 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §8.1 **M-4**（oracle 复核订正）—— 「裁决一经落盘，**planner 必须把每条裁决的可机判判据逐条回填**进归属 improvement 单元的 `## Acceptance` 段」⇒ ⚠️ **先于队列 2 的任何开工**（否则队列 2 的「前置命中判据」形同虚设）。
> **状态**：✅ **已回填**（2026-11-20 · planner）。

| 裁决 | 内容 | 回填落点（improvement · AC） | 状态 |
|:--:|---|---|:--:|
| **Q-5** | 13 键仓内 golden（派生副本 + 双向比对）| `cep-invariant-modeling-batch1` · **AC-9** | ✅ |
| **Q-9 / L-1** | OWNER 4 项改归 builder（scope 变更须显式）| `cep-registry-stale-cleanup` · **AC-12** ＋ `cep-doc-hygiene-kpi-and-impl-state` · `## 范围` In-Scope | ✅ |
| **Q-6e** | R 编号空间冻结 + 语义前缀 | `cep-stage-gate-p5-hardening` · **AC-12** | ✅ |
| **Q-6c / L-2** | `TODO` 行时限（可被 user 修订）| `cep-stage-gate-p5-hardening` · **AC-13** | ✅ |
| **M-1** | `INV-CEP-08-IC1` 偏离落点（不进 deferrals）| `cep-invariant-modeling-batch1` · **AC-10** | ✅ |
| **C-1** | `CEN-Ib8` 不新增字段（既有 `Kind`/`Origin`）| `cep-invariant-modeling-batch1` · **AC-11** | ✅ |
| **C-2** | `INV-CEP-09-IC2` 的 grep 模式写死（⛔ 不用 `Arbiter`）| `cep-invariant-modeling-batch1` · **AC-12** | ✅ |
| **M-2** | P-5 锚点表按实测措辞逐文件映射 + 行内取值 | `cep-stage-gate-p5-hardening` · **AC-11** | ✅ |
| **M-3** | ArchForge 闸真实缺陷 = **无 `[SKIP]` token**（⛔ 非「无输出」）| `cep-archforge-ci-failclosed` · **AC-1**（已订正）| ✅ |
| **Q-7 / C-3** | 「同一事实 5 处」canonical = 台账 §11（含否决记录）| `cep-registry-stale-cleanup` · **AC-11** | ✅ |
| **L-3** | `cep-freeze-lexicon-pointers` 排入队列 0 尾部 | 本节 §6.4 队列 0 · **P-5** | ✅ |
| **§9.1 M-2 订正** | 锚点表 `obj-cep-platform.md` 行改**粗体精确锚**（宽子串实测 3 处 ⇒ 不可用）+ 判据语义钉死为**联合命中** | `cep-stage-gate-p5-hardening` · **AC-11**（已同步）| ✅ |
| **§9.2 SF-1** | `Origin` 枚举事实陈述补全（**6** 值，per `protocol_engine_base.hh:177-184`）| `cep-invariant-modeling-batch1` · **AC-11** | ✅ |
| **§9.2 SF-2** | OWNER 4 项**写入归属钉死**（§1.3 重分类 = registry-stale；4 项文档修正 = doc-hygiene）| `cep-registry-stale-cleanup` · **AC-12** + `cep-doc-hygiene-kpi-and-impl-state` · In-Scope | ✅ |
| **§9.2 SF-3** | I-2 压缩 KPI 行时**锚点措辞逐字保留**（交叉引用 stage-gate AC-11 锚点表）| `cep-doc-hygiene-kpi-and-impl-state` · **AC-2** | ✅ |
| **§9.3 N-1** | 判据 ③ 的引用位置订正为 `cep_invariant_map_check.py:152-154`（单处）| `cep-invariant-modeling-batch1` · **AC-10** | ✅ |
| **§9.3 N-2** | Q-8 与 Q-9 的 user 决策面**拆为两个独立的门** | `cep-registry-stale-cleanup` · **AC-12** | ✅ |
| **§9.3 N-3** | ArchForge 用词统一为 `[SKIP]`（⛔ 不再用「标记位」）| `cep-archforge-ci-failclosed` · What Changes #1 | ✅ |
| **Q-8 · user 拍板** | D-1 验收债**分两层**处置：乙层全补 · 甲层封存+抽检 · 废除「0.5 d/38 份」改「每份 ≥0.5 d 下限」· **机关 = 独立闸**（封存边界 归档日期 < 2026-11-12，先补后上闸）<br>⚠️ **拍板时的「乙层 5 份 / 甲层 35 份 / ≈3~4 d」已被 Q-13 扩容 + metis B1/B2 复验订正为「乙层 8 份 / 甲层 127 份（统一口径）· ≈5~5.5 d」**（⛔ 拍板的**方向**不变：分层 + 统一覆盖 + 先补后上闸）| `cep-d1-archive-verify-backfill`（新建）| ✅ |
| **Q-9 · user 拍板** | **废除 OWNER 维度** + 8 项逐项终态（A 类 4 → doc-hygiene · B 类 3 → openspec change · C 类 1 → architect）| 登记册 §1.3 已重写 + `cep-spec-stale-refs-r14-r15-r20`（新建）+ Q-12 | ✅ |
| **Q-10 · user 拍板** | **采方案甲**：缺席打 `[SKIP]` + `SKIP_REGULAR_EXPRESSION` ⇒ ctest 记 **Skipped 而非 Passed** | `cep-archforge-ci-failclosed` · AC-1 / What Changes #1 | ✅ |
| **Q-13 · user 拍板** | **闸统一覆盖 + 乙层扩到 8 份**（5 CEP + 3 非 CEP；⛔ 不按 `cep` 前缀特判、⛔ 不做名字白名单豁免）⇒ 成本 2~2.5 d → **3~4 d** | `cep-d1-archive-verify-backfill`（scope 已扩，见 §10.7 / §10.9.1）| ✅ |
| **§10.9.3 · R-12/R-13 改归 B 类** | 二者落点**都在 `openspec/specs/`** ⇒ A 类单元 AC-6 禁止 specs diff ⇒ 原归属物理不可执行 ⇒ **A 类 2 项 · B 类 5 项 · C 类 1 项** | `cep-spec-stale-refs-r14-r15-r20`（3 → **5** 项）· `cep-doc-hygiene-kpi-and-impl-state`（只留 R-17 / R-19）| ✅ |

#### §6.3.2 旧口径残留 grep（**传播的反向判据** · per metis 缺失 AC 5）

> **为什么**：本轮 D-b / D-c / D-d 三处传播缺口**全部**是「上游已改、下游漏改」⇒ 上游的「✅ 已回填」**不能**防住它。⇒ 每条已落地裁决必须有一条**能自动抓到下游旧口径**的 grep。

| 裁决 | 旧口径残留串（应 == **0**） | 搜哪个文件 |
|---|---|---|
| Q-8 / Q-13 扩容 | `乙层 5 份` / `乙层 5 份全补` | 登记册 + `cep-d1-archive-verify-backfill.md` |
| Q-8 成本 | `≈3~4 d` | 登记册 + `cep-d1-archive-verify-backfill.md` |
| D-1 口径 | `43 份` / `甲层 35 份` | 登记册 + `cep-d1-archive-verify-backfill.md`（⛔ **白名单**：§3 的历史节正文 `H-Q2` 保留原样）|
| D-1 理由 | `最早 verify 报告 = 2026-11-12` / `无该实践` | 登记册 + `cep-d1-archive-verify-backfill.md` |
| §0.1 数字 | `**40**（50 CEP 归档` | 登记册 §0.1 |
| Q-9 三分 | `A 类 4 项` / `B 类 3 项` | 登记册 + `cep-doc-hygiene-*.md` + `cep-registry-stale-cleanup.md` |
| Q-10 状态 | `待 user 决策` | `cep-archforge-ci-failclosed.md` |
| 编号空间 | `\*\*Q-[1-7]\*\*`（§3 表体首列形态）| 登记册 §3（应已改名 `H-Q*`）|
| 队列落地 | `I-0~I-9` | 登记册 §6.4（应含 I-10 / I-11）|
| **队列 1 状态漂移** | §6.4 队列 1 中 `Q-1~Q-7` / `Q-11` 的行**不含**「已裁」⇒ **误标待裁** | 登记册 §6.4 队列 1（⛔ **Q-12 例外** —— 它**确实**待裁）|

> ⚠️ **本表的 grep 尚未接入闸**（⛔ 无载体 ⇒ 只能人工跑）⇒ 已在 §6.6 **P-9** 登记为待收敛项（收敛路径 = 并入 `rddf_stage_gate.sh` 的载体表，或由 `docs_sync_check.sh` 承接）。

#### §6.3.3 这张表怎么跑（**排除约定** · per critic SHOULD_FIX 4）

> **问题**：本表列的「旧口径残留串」**必然命中「订正引述」行**（本表自己 + 各文件的「旧表述为假 / 已废 / 拍板时 / 初稿」引述）⇒ **字面执行的结果是「抓到自己」**，与表的目的相反。

**排除约定（三条同时满足才算命中）**：

```bash
# 判据：命中行 ⛔ 必须【不含】任何「订正标记」词
MARK='已废|为假|旧表述|拍板时|初稿|订正|已移除|已统一|不可执行|白名单'
# 例（以「甲层 35 份」为例）：
grep -n '甲层 35 份' <文件> | grep -vE "$MARK"   # ⇒ 期望输出为空
```

⛔ **本节（§6.3.2 + §6.3.3）自身必然含这些串**（它们是**判据的定义**）⇒ 跑 grep 时**必须排除本节**：
```bash
# 只扫 §6.3.2 之前的内容（⛔ 本节自含判据串 ⇒ 必须排除）
sed '/^#### §6.3.2/,$d' <登记册> | grep -n '甲层 35 份' | grep -vE "$MARK"   # ⇒ 期望空
```

**逐条 grep 的实际跑法**（登记册 + d1 两处都要跑，⚠️ 每条**两个文件各跑一次**）：

| 残留串 | 跑法（⛔ **登记册一律先 `sed '/^#### §6.3.2/,$d'` 截断**）| 期望 |
|---|---|---|
| `甲层 35 份` / `乙层 5 份` / `≈3~4 d` / `43 份` | `grep -n '<串>' <file> \| grep -vE "$MARK"` | **空**（登记册 + d1 各一次）|
| `A 类 4 项` / `B 类 3 项` | 同上，扫登记册 + `cep-doc-hygiene-*` + `cep-registry-stale-cleanup` | **空** |
| `**40**（50 CEP 归档` | 同上，扫登记册 §0.1 | **空** |
| `待 user 决策` | ⚠️ **只扫 front-matter 状态行**（`grep -n '^状态:'`），⛔ 不扫全文（各文件「审查轨迹」段合法引述旧状态）| **空** |
| `I-0~I-9` | 扫登记册 §6.4（⛔ 排除本节）| **空**（应含 I-10 / I-11）|
| §3 表体 `Q-[1-7]` | `sed -n '/^## §3 /,/^## §4 /p' <登记册> \| grep -cE '^\| \*\*Q-[1-7]\*\*'` | **0**（应已改名 `H-Q*`）|

⚠️ **`MARK` 词表本身是一份约定** ⇒ 若将来新增订正引述用了不在词表里的说法 ⇒ 该条会误报 ⇒ ⛔ **新增订正引述时必须同步扩 `MARK`**（⛔ 不得靠「人眼判断这行是引述」）。


### §6.4 后续立项计划（**3 队列**；用「前置命中判据」排序，⛔ 不用天数）

> **硬前置**：触碰**冻结面成员**（`scripts/test/cep_freeze_check.sh:47-105` 的 `FROZEN_GLOBS`，spec 侧为 **glob 全量**）⇒ 改 hash ⇒ 必须走 `--update --authorized-by <台账 Entry>` 五步链，且 `docs/guide/cep-usage.md §12.6` 要求**台账与清单不得合并提交**。
> **台账 = `--authorized-by` 的授权目标，不是被哈希冻结的成员** ⇒ 改台账 living 段**不触**五步链；改冻结成员才触。
> ⛔ `include/tlm/cep/protocol_instances.json` 与本册**均不在冻结集**。
> **并发**：`docs/guide/cep-usage.md:80`「Proposed **并发 ≤ 1**」（⚠️ 旧写 `:60` ⇒ 已漂移）。

#### 队列 0 · PROC（不触冻结面 · 立即可开工）

| 序 | 立项 | 缺口 | 完成判据（可机判） |
|:--:|---|---|---|
| **P-1** | 登记册基线刷新 + §1.5 整节作废标注 + **追加式台账订正 Entry** 回填 S2-6b + **闸头引用转指**（`rddf_stage_gate.sh:3` 引用 §1.5.6，§1.5 作废后悬空） | S2-6b/d · S3-9 | 登记册 §0.1 三数 == `ls openspec/changes \| grep -v archive \| wc -l` / `ls openspec/specs \| wc -l` 实测值；§1.5 已标作废；订正 Entry 存在 + grep 命中；闸头引用已改指 |
| **P-2** | ~~台账 B2 行措辞订正~~ —— **已撤销**（Entry #232 授权的改判，⛔ 不得 planner 原地改 ⇒ 并入 Q-2） | — | — |
| **P-3** | `obj §4.0-a` 指引强化（**追加式**）—— ⛔ 不动 8 行摘要表与 Entry #334 段原文 | S2-5 | 订正 Entry 存在 + grep 命中；**8 行摘要表 sha256 逐字未变** |
| **P-4** | ~~freeze 注释清理~~ —— **已撤销**，内容并入 **I-1**（消除 S2-6c 重复认领） | — | — |
| **P-5** | **冻结面「引擎口径」指针**（**【裁决吸收 · L-3】** 排入队列 0 **尾部**）：`docs/architecture/07-coherence-exploration-platform.md:138` + `docs/architecture/cep/protocol-engine.md:44` + `docs/adr/ADR-CEP-20`（`:88`/`:126`）三处加「引擎口径见 `cep-usage.md §12.2.1`」指针（⛔ **ADR 只追加 SU，原文不动**）| S2-2 | **一次**五步链（三文件**同批**，⛔ 不分两次）⇒ 授权 Entry 先落 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单**单独 commit** → 无参复验 rc=0；⚠️ **理由** = 否则 Q-1 只解了**使用手册侧**，**冻结面的 5 vs 6 冲突仍会被新读者读到**；⚠️ ⛔ **零 architect 决策前置**（只走五步链）⇒ 可与 P-1 同批 |

#### 队列 1 · 决策（`needs` = architect / user · 产出裁决，不产出代码）

| 序 | 立项 | 缺口 | 完成判据（可机判） |
|:--:|---|---|---|
| **Q-1** | **✅ 已裁（architect §2.1 立定义：三词边界 + B3b 分母）** — ⛔ 勿重复拍板 · 「引擎 / 协议族引擎 / adapter 实例」三词词表 + B3b 分母定义| S2-2 · S2-3 | **封闭判据**：活文档集 = `docs/architecture/**`+`docs/guide/**`+`docs/adr/**`+`include/tlm/cep/**`+`docs/implementation/**`（**排除** `docs/validation/**`+`docs/archive/**`+`docs/research/**`）⇒ `grep -rniE "5 ?个引擎\|6 ?个引擎\|五个\|六个"` 命中数 **==** 裁决随附豁免清单条目数 |
| **Q-2** | **✅ 已裁（architect §2.2 裁决：不改 Entry #232 的 B2 行措辞）** — ⛔ 勿重复拍板 · **DoD 7 / B2 口径修复面判定（独家处理 S3-10）**| S3-10 | 裁决**逐条回答三问**：① Entry #232 已 ratify 的改判措辞**是否需再改** ② 改则**是否需 user re-ratify** ③ **回滚路径**；并给「是否动 `07 号 §4`」明确二选一 |
| **Q-3** | **✅ 已裁（architect §2.3 落 `docs/guide/cep-usage.md` §12.7）** — ⛔ 勿重复拍板 · 「DoD 12/12 ✅ ⇒ 平台完成」读法拦截点落点| S2-4 | 限定可 grep 命中；`obj:34-76` 的 12 个 ✅ token 逐字未变 |
| **Q-4** | **✅ 已裁（architect §2.4 逐条给判据形态；判据载体 = 裁决记录 §2.4）** — ⛔ 勿重复拍板 · 3 条可建模 invariant 的 architect 判据定义（`CEN-Ib8` / `INV-CEP-08-IC1` / `INV-CEP-09-IC2`）| S1-1 批 1 | 每条给「可断言对象 + 载体形态 + 判据写法」；⛔ 不得 builder 自行发明语义 |
| **Q-5** | **✅ 已裁（architect §2.5 裁决：仓内 golden 为 canonical）** — ⛔ 勿重复拍板 · 13 键 golden canonical 来源裁定| S2-7 | `moesi_gpu`/`mesif` 的 13 键各有仓内可对账落点 |
| **Q-6** | **✅ 已裁（architect §2.6 四项一次定：P-5 形态 / 登记面收敛 / TODO 规则 / 闸头转指 / 编号空间）** — ⛔ 勿重复拍板 · **R 编号空间冻结 + 新登记语义前缀 + 登记面收敛**（既有 `rddf_ac_carrier.tsv` + `rddf_stage_gate.sh` 队列绑定扩展 + **P-5 判据形态裁决** + 闸头引用转指方案）| S3-1 · S3-11 | 新登记全带语义前缀；旧 `R-*` 标历史快照；裁决含「载体表扩行 + 闸队列绑定 + **P-5 锚定形态（行号 vs 唯一措辞）** + `LIVE_FILES` 是否增补 + TSV `TODO` 行升级时限/顺序规则 + 闸头转指」的具体形态；⛔ 不新建第三个登记面 |
| **Q-7** | **✅ 已裁（architect §2.7 裁决：台账 §11 为源）** — ⛔ 勿重复拍板 · 清单 5 处 canonical 化| S3-2 | 5 处中 1 处为源，其余 4 处改引用 |
| **Q-8** | **✅ 已裁（用户 2026-11-20）** — ⛔ 勿重复拍板 · ：D-1 验收债分两层处置（乙层全补 / 甲层封存+抽检 / 废除「0.5 d / 38 份」/ 机关 = 独立闸 `archive_verify_check.sh`；⚠️ 份数与成本已被 **Q-13 扩容** + **metis B1/B2 复验**订正为「乙层 **8** 份 / 甲层 **127** 份 / ≈**5~5.5 d**」）| S3-5 | 实施见 **队列 2 的 I-10**；⛔ 本行**不再产出裁决** |
| **Q-9** | **✅ 已裁（用户 2026-11-20）** — ⛔ 勿重复拍板 · ：**废除 OWNER 维度**（本仓无独立 owner 角色）+ 8 项逐项终态（A 2 / B 5 / C 1，见 §1.3 终态表）| S3-8 | 实施见 **I-11**（B 类 openspec change）+ `cep-doc-hygiene-kpi-and-impl-state`（A 类）+ **Q-12**（C 类）；⛔ 本行**不再产出裁决** |
| **Q-10** | **✅ 已裁（用户 2026-11-20 · 方案甲）** — ⛔ 勿重复拍板 · **：ArchForge 闸缺席时打 `[SKIP]` + CMake `SKIP_REGULAR_EXPRESSION` ⇒ ctest 记 **Skipped 而非 Passed**（⛔ 不 checkout ArchForge）| S2-8 | 实施见 **I-9**；⛔ 本行**不再产出裁决** |
| **Q-13** | **✅ 已裁（用户 2026-11-20）** — ⛔ 勿重复拍板 · **：归档验收闸**统一覆盖**（⛔ 不按 `cep` 前缀特判、⛔ 不做名字白名单豁免）⇒ 乙层扩到 **8 份**（5 CEP + 3 非 CEP），成本 2~2.5 d → **3~4 d** | S3-5 | 实施见 **I-10**；⛔ 本行**不再产出裁决** |
| **Q-11** | **✅ 已裁（architect §2.11 裁决：登记为季度义务）** — ⛔ 勿重复拍板 · E2 季度复审义务的责任方 / 调度落点登记| S4-1 | 台账写明责任方 + 触发条件 + 落点；⚠️ **不得**把「脚本已存在」当完成判据 |
| **Q-12** | **✅ 已裁（architect 2026-11-20 · 裁决文件 = `.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`）** —— 采**选项②改契约**（⛔ 非用户倾向①；理由见裁决 §5：① 的三形态各自不可执行 + 需穿透 3 处硬编码 + **真问题在契约面失真而非取值来源**）⇒ **裁决对象 = `minimal-dgpu-soc-architecture/spec.md` 的 3 处矛盾断言（A-1 MUST 被全体违反 / A-2 Scenario 与实现相反 / A-3 僵尸条款）+ 3 处连带 spec**（⛔ **只改 `:101` 一行 ⇒ 裁决不成立**）| S3-8 · C 类 1 项 | 落地见 **I-12**（新建 OpenSpec change · 占 1 KPI slot）；⚠️ **锚点订正**：旧 AC 的「是否动 `configs/dgpu_soc_minimal_v1.json:49`」**锚错行**（`:49` 是 `sdma.params.max_inflight`，`bar_sizes` 在 `:27`）⇒ 改锚定措辞「`bar_sizes` 数组第 2 元素」；⚠️ 裁决**不**改配置取值（N-2）| 落地见 **I-12**（新建 OpenSpec change · 占 1 KPI slot）· ✅ **已落地 2026-11-21**（change `cpptlm-doorbell-contract-alignment` 已归档 · 验收报告 `.rddf/reviews/cpptlm-doorbell-contract-alignment-verify.md`）

#### 队列 2 · 实施（`needs` = builder · 前置命中判据可机判）

| 序 | 立项 | 缺口 | 前置（一条命令） | 完成判据（可机判） |
|:--:|---|---|---|---|
| **I-0** | **顺序机关落地 + P-5 判据修正**（扩 `rddf_ac_carrier.tsv` 载体行 + 扩 `rddf_stage_gate.sh` 队列绑定 + **P-5 改为「锚定活口径行 + 取值 == 实测」**） | 顺序保证 · S3-11 | **Q-6 已裁**（`.rddf/reviews/` 有裁决且 grep 命中「登记面收敛」） | `rddf_ac_carrier.tsv` 新增本计划各 I-* 的 AC↔载体↔锚点行；`bash scripts/test/rddf_stage_gate.sh` rc=0（**已注册 ctest** ⇒ CI 强制）；**影子副本决定性变异**：6 个活口径行全改 `3/3` ⇒ P-5 **必须判红 ≥ 5/6**（当前仅 2/6 ⇒ 修正前必红 ⇒ 判据有鉴别力）；⛔ `ls scripts/test/rddf_*` 仍只 **2** 个文件；✅ **已验收（2026-11-20 · verifier 轮 1 · PASS_WITH_NOTES）**：报告 `.rddf/reviews/cep-stage-gate-p5-hardening-verify.md` ⇒ 载体表 `I-0` 行 state = `done` ⇒ 队列下一项已推进至 **I-1** |
| **I-1** | 口径 stale 集 a/c 清理（a = `cep-usage.md` 的 ADR 归因行；c = freeze 注释） | S2-6a/c | 无 | a：`grep -cF 'ADR 归因**(18 条)' docs/guide/cep-usage.md` == **0**（⚠️ **必须含 markdown 粗体星号** —— 原文是 `**ADR 归因**(18 条)`；⚠️ 旧判据写全角「（18 条）」⇒ `grep -c` 恒为 0 = **恒绿假绿**）；c：`grep -c "当前 \`docs/architecture/cep/\` 不存在" scripts/test/cep_freeze_check.sh` == 0；`cep_freeze_check.sh` rc 不变 |
| **I-9** | S2-8 CI 假绿修复 | S2-8 | **Q-10 已裁（方案甲）** ⇒ 判据 = `grep -F "方案甲" .rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` 命中 **且** `grep -F "\[SKIP\]" scripts/test/cep_archforge_alignment_check.sh` 命中（⚠️ ⛔ 不得用 `grep "补"` —— 「补 0 份」在 Q-8 段大量出现 ⇒ 无鉴别力）| 闸在 ArchForge 缺席时打 `[SKIP]` + `test/CMakeLists.txt` 注册 `SKIP_REGULAR_EXPRESSION "\[SKIP\]"` ⇒ ctest 记 **Skipped 而非 Passed**；⚠️ 环境前提：CMake **≥ 3.16**（`SKIP_REGULAR_EXPRESSION` 恰于 3.16 引入，地板零余量）|
| **I-5** | invariant 建模批 1 | S1-1 批 1 | **Q-4 已裁**（`.rddf/reviews/` 有判据文件且 `grep -F` 命中三条 id） | `python3 scripts/test/cep_invariant_map_check.py` 绿且 `⭭-unmodelled` 数由 **5 降为 2**；`test/cep_invariant_assertion_map.json` 由脚本**重生成**（⛔ 不得手改） |
| **I-2** | L3 KPI 行堆积清理 | S3-6 | 无 | 4 份文档 KPI 行**单行 ≤ 1000 字符**（与 `scripts/test/agents_md_lint.py` 既有单行阈同阈） |
| **I-3** | L4 3 处 improvement 状态漂移回填 | S3-7 | 无 | 3 份 `.rddf/improvements/*.md` 状态行含「已归档」 |
| **I-4** | KPI 活口径同步固定化 | S3-3 | **命令型**：`bash scripts/test/rddf_stage_gate.sh` rc=0 **且** P-5 输出「全部同步」 | 立项 commit 内 6 个 `LIVE_FILES` 的**活口径行**取值 == 实测 `N/3`；⛔ **不以子串存在性为判据**（S3-11） |
| **I-6a** | **litmus change 落地**（`INV-CEP-13-IC1` 的证据来源）· ✅ **已落地 2026-11-21**（归档 `openspec/changes/archive/2026-11-21-cpptlm-cep-litmus-falsification-harness/` · 台账 Entry #513）| S1-1 批 2 的前置 | **Q-4 已裁**（判据形态）| 1 个 openspec change 归档（`ls openspec/changes/archive/ \| grep -c <litmus>` ≥ 1）；⇒ **I-6 的前置之一**（⚠️ 初稿把该前置挂在 I-6 上却**无任何单元产出它** ⇒ 永久卡死，per metis B6）|
| **I-8** | `openspec` 版本 pinning · ✅ **已落地 2026-11-21**（锁文件 `.openspec-cli-version` + 闸 `scripts/test/openspec_version_check.sh` + CI 从锁文件读）| S3-4 | 无 | CI 配置锁版本；`openspec --version` 与锁定值一致 |
| **I-10** | **归档验收债补齐 + 机关落地**（= `cep-d1-archive-verify-backfill`） · ✅ **已落地 2026-11-21**（9 份乙层追溯补验报告 + 甲层抽检 5 份 + 闸 `scripts/test/archive_verify_check.sh` + 乙层 30 目录 force-add）| S3-5 · Q-8 · Q-13 | **本行自身即载体**（⛔ 无外部前置）| ① 乙层 **8 份**追溯补验报告落 `.rddf/reviews/<change>-verify.md`（**精确命名**）· 每份含「原归档未验 · 追溯补验」标注 + 角色 = verifier；② `scripts/test/archive_verify_check.sh` 存在 + `test/CMakeLists.txt` 已注册（`ctest -N \| grep archive_verify_check` 命中 1）；③ 闸的**三检**（存在 ∧ 3 标题 ∧ 去空白 ≥400 字符）在**全部 29 个 ≥11-12 归档目录**上 rc=0；④ ⚠️ **archive 目录在 CI 不可见时须 fail-closed**（`.gitignore:64` ⇒ 目录不入库 ⇒ CI 上只有 40/164 个目录被 track）⇒ 扫面目录数 < 基线 ⇒ **判红**，⛔ 不得静默 rc=0 |
| **I-11** | **spec 契约面 stale 引用修正**（= `cep-spec-stale-refs-r14-r15-r20`，**5 项 B 类**） · ✅ **已落地 2026-11-21**（change `cpptlm-spec-stale-refs-b-class` 已归档 · 4 项 delta 合并：R-12 悬空 `abi_guards.h` / R-14 跨仓指针 / R-15 `SHALL` 自洽 / R-20 已删 ABI；**R-13b 随归档自动闭合**（中文 trailer 被丢弃）；⚠️ **R-13a（`pcie-ip-microarch:296`）未闭合 ⇒ 待 architect 裁**；新增缺口登记 = `PcieTlpBundle` 无布局断言）| S3-8 · Q-9 | ① **openspec 可行性 spike 通过**（⛔ 未过 ⇒ 改走「architect 裁 + 主管契约面直接修订」，不得硬套 delta）② 立项时 `openspec/changes/` active **≤ 0** | 新建 1 个 openspec change（占 1 KPI slot）· 归档后 5 处旧串 `grep -c` == **0**（R-12 / R-13 / R-14 / R-15 / R-20 各一）· ⚠️ R-15 选「本仓不承诺」则 `SHALL` 语义**必须同步改写**（不得自相矛盾）|
| **I-12** | **doorbell 契约面失真修正**（= architect 裁决 Q-12 的落地 · **dGPU 域**）· ✅ **已落地 2026-11-21**（change `cpptlm-doorbell-contract-alignment` 已归档 ⇒ `openspec/changes/archive/2026-11-21-cpptlm-doorbell-contract-alignment/`；7 处连带全落：3 份 main spec 合并（含 1 处 Requirement **改名**：旧「doorbell 配置 vs 代码 hardcoded 不一致(per §13 R4)」→ 新「doorbell 合成偏移是已裁定的现状…」）· `dgpu-soc-control-hub` 核对**不含** ⇒ 显式记录不改 · ADR-DGPU-09 追加 Status Update（+10/−0 行）；3 条决定性变异 M-1/M-2/M-3 全部有鉴别力证据；⚠️ **2 处工具约束**（openspec 禁止 MODIFIED 内重命名 Scenario ⇒ 旧标题保留 + 正文给结论；MODIFIED 的 Requirement 标题须存在 ⇒ 改名走 `## RENAMED Requirements`）；⚠️ **裁决 AC-4 括注被实测证伪**（删门铃分支后 `rc` 仍为 0 ⇒ 真红因是 `pcie_ep_doorbell_count()` 断言）| S3-8 · C 类 1 项（Q-12）· ⚠️ **dGPU 域**（借用 CEP 队列作载体，⛔ **不表示 CEP 范围扩张** —— 先例 = I-10 的「仓级域」）| ① 裁决已落（`.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`）② **openspec 可行性已验证**（⛔ 未过 ⇒ 回 architect）③ 立项时 `openspec/changes/` active **≤ 0**（占 1 KPI slot）| 新建 1 个 OpenSpec change（**7 处连带**：3 主 spec + 3 连带 spec + 1 ADR SU）⇒ 裁决 AC-1 ~ AC-10 逐条达成（含 **3 条决定性变异** M-1/M-2/M-3 + **复活锁** R-1/R-2/R-3 择一写死）⚠️ **⛔ 只改 `:101` 一行 ⇒ 裁决不成立**（A-1 MUST 被全体违反 / A-2 Scenario 与实现相反 仍在）⚠️ **⛔ 不改代码取值来源**（3 处硬编码保持）· ⛔ **不改 `configs/**`** · ⛔ **不改 driver ABI 表** |
| **I-6** | invariant 建模批 2 | S1-1 批 2 · ✅ **已落地 2026-11-21**（change `cpptlm-cep-invariant-modeling-batch2` 已归档 ⇒ `openspec/changes/archive/2026-11-21-cpptlm-cep-invariant-modeling-batch2/`；`properties` 13→14 + `required` **仍 13**（IC1 载体零改动）· 本仓权威域 **6** 实例声明 `bridging.early_ack`（**全 6 = `unknown`** ⇒ ⛔ 不是「已取得协议事实」）· 新载体 `test/test_cep_combination_contract.cc`（2 个 `INV-CARRIER`）· 新闸 `scripts/test/cep_invariant_batch2_ledger_check.py` · 台账 Entry **#514** · `⏭-unmodelled` **2 → 0** · 两份 ADR 只增行 SU）|① **Q-4 裁决已落**（`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二 · oracle APPROVE + critic APPROVE_WITH_CONDITIONS · 无 BLOCKER）**且** **用户 ratify R-2a~R-2f**（2026-11-21）✅ ② **litmus change 已归档**（= I-6a）✅ ③ **Q-2 已裁** ∧ `obj §0 DoD 7` 的「100% 覆盖」措辞已订正（⚠️ 否则「66/66」会**看起来**被验证为真）④ ⏳ **待立项**（openspec change ⇒ builder 实施）| 同上（`⭭` 由 2 降为 0）；⚠️ ⛔ **「66/66」只能读作「66 条全部有载体」**，⛔ 不得读作「全部可判」（`INV-CEP-09-IC2` = 超出范围的负断言）；`INV-CEP-04-IC4` 仍跳空；编号位仍 67 · ⚠️ **实测偏离 1 处**：裁决 AC-5⑤ 写「清单只改 3 个 hash」，实为 **4 个**（`openspec/specs/cep-core-contracts/spec.md` 因 delta 归档合并而变 ⇒ 它**也是**冻结成员 ⇒ 与「schema 变更须入 delta spec」不可兼得 ⇒ 取后者，见 change 的 design.md D-6）|
| **I-7** | 档 2 扩面到第 2 族（`tilelink_c`） | S1-2 |**已拆分**（architect 裁决 §四 · 2026-11-21）⇒ **I-7a** = `TreeManager::view_of`（const 纯查询 · `.rddf/improvements/cep-tilelink-tree-manager-view-of.md`）→ **I-7b** = home 侧消息簿记层 + tracker 侧观测 + 档 2 第二族（`.rddf/improvements/cep-tilelink-home-side-state-driver.md`）；⚠️ 裁决 Q-a = **具体类**上补 `view_of`（⛔ 不动 `SharerTrackerIf` —— 其「恰 4 个虚方法」契约 + 两处 `count_member_virtuals == 5` 断言）；⚠️ 原「硬前置 = `CEN-Ib8`(engine egress)」前提被裁决**否掉**（TLE-05 禁止引擎持状态 ⇒ 驱动不进引擎）；⚠️ 旧单元 `cep-tier2-second-family-tilelink` 已标 **⏸ 已拆分 · 不可开工**| `test/cep_evidence_tiers.json` 档 2 登记 ≥ 2 族；`scripts/test/cep_e2e_reconcile_check.sh` T4/T5 绿；档 2 仍 ⭔ 无 run / 无 verdict / 无通过态 |

> **序语义（2026-11-20 用户裁定路径 A）**：本表行序 = **队列的推进序** —— **已完成项构成前缀**（`I-0` → `I-1` → `I-9` → `I-5`），未完成项保持原相对序。⚠️ `I-9` / `I-5` 原按设计顺序排在后面，但已按用户指示**先做** ⇒ 纳入前缀，以免 `P-2a`（严格串行判据）与事实冲突（乱序标 `done` 会让 `--phase queue2` 与 `--self-test` 双红）。⛔ 重排**不改任何单元的判据 / 前置**，只改行序。

> **互斥**：P-1/P-3/I-0~I-12 全部落在同一 worktree ⇒ 本仓单 worktree + `role-claims.json` 每 role 唯一 active claim ⇒ **队列 2 整体串行**，⛔ 不得声称可并行。唯一可并行 = 队列 0 / 队列 1（台账 / 文档 / 裁决）与队列 2 文件面不相交时。

#### 编号空间登记表（**同一前缀不得跨语义域** · per metis B8）

> **为什么要有这张表**：本册把「编号空间被复用 ⇒ 残余不可寻址」登记为 **E-1 / S3-1** 缺口 ⇒ **登记册自身不得重演它**（初稿即犯：§3 的 `Q-1~Q-7` 与 §6.4 队列 1 的 `Q-1~Q-12` 同前缀异义；§1.5.6 的 `P-1~P-6` 与 §6.6 的 `P-1~P-9` 亦然）。

| 前缀 | 语义域 | 位置 | 备注 |
|---|---|---|---|
| `H-Q1~H-Q7` | **历史**开放问题 | §3（2026-11-16/18 快照）| 全部已结清；⛔ **不得据此派工** |
| `Q-1~Q-13` | **当前**待裁 / 已裁问题 | §6.4 队列 1 | 被 10 份 improvement + 裁决记录引用 ⇒ **不重编号** |
| `I-0~I-12` + **`I-6a`**（子 ID）| 实施单元 | §6.4 队列 2 | 对应 improvement 单元（对账 AC 见 §6.3）；⚠️ `I-6a` 是 `I-6` 的**前置产出单元**（litmus change）⇒ 它**不占**独立编号位（与 I-6 同族）—— per critic NIT | ⚠️ `I-6a` / `I-7a` / `I-7b` 均为**前置产出单元 / 扩面单元**，⛔ **不占独立编号位**（⛔ **不新增 `QUEUE2` 槽位** —— 载体表挂父编号 `I-6` / `I-7` ⇒ 「父项全部行 done 才算完成」）
| `P-1~P-9` | **本计划自身的可证伪判据** | §6.6 | ⛔ **不是**队列 0 的立项单元 |
| `R-*` | **登记册历史 ID** | §1 + 台账历史 Entry | ⛔ 引用必须带轮次 / Entry 上下文（§4 #1）|
| `S0~S4` / `S1-x` / `S2-x` / `S3-x` / `S4-x` | **严重度 + 缺口 ID** | §6.1 / §6.2 | 严重度与 `needs` **严格正交** |

⛔ **登记纪律**：新增编号时先查本表 ⇒ 若目标前缀已有其它语义域 ⇒ **换前缀**（⛔ 不复用）。⚠️ **队列 0 的 `P-1~P-5`**（§6.4）仍是遗留冲突（与 §6.6 的 `P-*` 同前缀异义）⇒ **待 Q-6 裁决统一改名**（metis B8 建议改名为 `PROC-01~05`，正好复用 Q-6e 即将建立的语义前缀）⇒ ⛔ **本轮不改**（会牵动 §6.3 三行 + §6.3.1 L-3 + 4 份草稿的引用 ⇒ 属 Q-6 的裁决范围）。

### §6.5 顺序机关（**取代**「checklist 文件」方案）

§1.5.6 自诊断过同型病（「依赖图画得漂亮但**没有任何机关** ⇒ 顺序只由文风保证」）并开出解药 —— **该解药已落地**：`scripts/test/rddf_stage_gate.sh`（**已存在 + 已注册 ctest** `test/CMakeLists.txt:286-287`）实现 P-1~P-6，数据源 `scripts/test/rddf_ac_carrier.tsv`（4 列 TSV：AC 编号 / 载体类型 / 载体路径 / 锚点，35 行）是「AC ↔ 判据载体 ↔ 锚点」的**唯一数据源**；该闸 P-2a 现自解于「三个阶段均已归档 ⇒ 无下一阶段可提前开工」（`:158`）⇒ 处于**无目标**状态 ⇒ **可承载本计划队列**。
⇒ **不新建第三个登记面**（守 Entry #334 ④「不新建登记面」否决理由，`obj:18622`）⇒ 机关 = **扩既有载体表 + 扩闸绑定**（**I-0**）+ 登记面收敛裁决（**Q-6**）。
⚠️ §1.5.6 自己的标准（「⛔ P-1~P-6 落盘前，规划只是『文风』」）⇒ **队列 2 开工前必须先跑 I-0**（队列 0 / 队列 1 不依赖 I-0）。

### §6.6 本计划自身的可证伪判据（**P-1~P-9**）

| # | 判据 | 命令 / 载体 |
|:--:|---|---|
| P-1 | 每项缺口都有 `file:line` 或单条命令 | 逐条自查 |
| P-2 | 严重级与 `needs` 正交，不混 | 每项必带 `needs:` |
| P-3 | **S0 = 0 是结论不是遗漏** | S3-10 / S2-2 / S2-5 三项降级依据逐条可追 |
| P-4 | **每个 `0` 必须带「本装置无该形态」的证据，否则视为未测** —— 逐项证据列 | 逐条列出该证据；无证据 ⇒ 判为未测 |
| P-5 | **S4 项不得出现在任何实施队列的 AC 内** | 逐项对照队列 2 |
| P-6 | **KPI 活口径同步面 = 6 文件 / 7 处** —— 判据用**修正后**的闸 P-5（锚定活口径行 + 取值 == 实测），⛔ 不用子串存在性 | `bash scripts/test/rddf_stage_gate.sh` + 影子副本变异（I-0 完成判据） |
| P-7 | **前置命中判据可机判**（每条一条命令判真假，⛔ 不用天数、不用事件） | 队列 2 每行的「前置」列可执行 |
| P-8 | **不新建编号空间** | 本节全部引用登记册 ID；新项标「待收录」；语义前缀空间由 Q-6 一次性建立 |
| P-9 | **无闸载体风险自登记** —— 本轮引入的载体逐个登记（⛔ **不是 0 个** —— 旧表述「= 0 个」为假，已按 metis B12 订正）：① `scripts/test/archive_verify_check.sh`（**已注册 ctest** · 归 **I-10** · 收敛路径 = 未来若需并入既有闸则由 `rddf_stage_gate.sh` 收编）② `test/cep_instance_golden.json`（golden 数据源 · 归 **I-5** · ⚠️ **当前无闸载体** ⇒ 须由 `cep_invariant_map_check.py` 校验或由 I-5 补一条 ctest ⇒ **收敛路径 = 纳入 `cep_invariant_map_check.py`**）③ `.rddf/reviews/archive-audit-<date>.md`（甲层抽检记录 · 归 **I-10** · ⚠️ **当前无闸载体** ⇒ 收敛路径 = 抽检结论回写登记册 §1.3 终态表） | 本节 §6.5 + 各单元 `## Impact` 表 |

### §6.7 明确不做（本轮范围外）

1. ❌ 不改任何已签批 ADR 正文（只走 SU）
2. ❌ 不动 `07 号` 正文与既有 SU（只追加，append-only）
3. ❌ 不重排 B3b-2 计数（仍 `0/4 ⭔`）
4. ❌ **不新增任何「分级清单 / 强度表」类文档表格**（`docs/guide/cep-usage.md` 的「⛔ 本节不给分级清单」禁令 + 无闸表格必腐化）
5. ❌ 不新建架构文档
6. ❌ 不并行立多个 OpenSpec change（`docs/guide/cep-usage.md:80`）
7. ❌ 不立仓级 spec 卫生提案，也不把非 CEP spec 存废排进 CEP 队列（**仅登记**）
8. ❌ 不改 `obj §4.0-a` 的 8 行摘要表（守 Entry #334 ratify 形态）
9. ❌ **不新建第三个登记面**（复用 `rddf_ac_carrier.tsv` + `rddf_stage_gate.sh`）
10. ❌ **不由 planner 原地改写 Entry #232 已授权的改判措辞**（处置并入 Q-2）

### §6.8 关联

- 审查轨迹：metis 1 轮 + oracle 1 轮 + critic 3 轮（第 3 轮 **APPROVE_WITH_CONDITIONS**）—— 详见本节顶部与 §6.6
- 立项单元落点（**本轮 10 份**）：`.rddf/improvements/cep-registry-stale-cleanup.md` · `cep-stage-gate-p5-hardening.md` · `cep-invariant-modeling-batch1.md` · `cep-invariant-modeling-batch2.md` · `cep-tier2-second-family-tilelink.md` · `cep-doc-hygiene-kpi-and-impl-state.md` · `cep-openspec-version-pinning.md` · `cep-archforge-ci-failclosed.md` · **`cep-d1-archive-verify-backfill.md`**（= I-10）· **`cep-spec-stale-refs-r14-r15-r20.md`**（= I-11）
- ⚠️ **`.rddf/improvements/` 下其余 CEP 单元不属于本轮清单**（`cep-b4-evidence-device` / `cep-evidence-tier-declaration` / `cep-evidence-tier2-minimal-slice` / `cep-responder-bitmap-expansion` / `cep-tier2-verification-closure` —— 均对应**已归档** change，**状态行含「已归档」** ⇒ 不持有活义务）；自查命令：`grep -L "已归档" .rddf/improvements/cep-*.md`（输出应恰为上列 10 份）
- 关键既有载体：`scripts/test/rddf_stage_gate.sh` · `scripts/test/rddf_ac_carrier.tsv` · `scripts/test/cep_freeze_check.sh` · `test/cep_b3b2_deferrals.json` · `test/cep_invariant_deferrals.json` · `test/cep_evidence_tiers.json` · `docs/architecture/07-coherence-exploration-platform.md` · `docs/architecture/cep/*.md` · `docs/guide/cep-usage.md`

### §6.9 实施路径（**2026-11-20 用户裁定：按本文件路径实施**）

> 本节 = 队列 2 的**执行路径**（⛔ 不是新登记面：单元与判据仍以 §6.4 为唯一源，本节只定**先后与门槛**）。
> **纪律**：每完成一个单元 ⇒ 立刻把 `scripts/test/rddf_ac_carrier.tsv` 的 `state` 置 `done`（否则 `P-2a` 与事实脱节）；⛔ 不跳阶段。

| 阶段 | 单元（= §6.4 队列 2 的行）| 前置 | 本阶段判据 |
|:--:|---|---|---|
| **①** | **I-2**（L3 KPI 行堆积清理）· **I-3**（L4 状态漂移回填）· **I-4**（KPI 活口径同步固定化）| 均无外部阻塞 | I-2：4 份文档 KPI 行 ≤ 1000 字符 **且** `P-5` 仍输出「锚定 **7** 处，逐处取值 == 实测」**且** 每份文档的台账 `Entry #` 号集合与压缩前相等（AC-2/AC-3）· I-3：3 份状态行含「已归档」· I-4：`P-5` 判据即其判据（已由 I-0 实现）|
| **②** | **I-6a**（litmus change 落地）| Q-4 已裁 | 1 个 openspec change 归档（占 1 KPI slot；当前 0/3）|
| **③（决策门 · ✅ 2026-11-21 已裁 + 已 ratify ⇒ 剩「立项 + 实施」）** | **I-6**（invariant 建模批 2）· **I-7**（档 2 扩面到 `tilelink_c` ⇒ 已拆 **I-7a** / **I-7b**）| ① litmus change 归档 ② schema 13→14 形态（architect）③ `ADR-CEP-12` 追加式修订 + **user ratify** | ⛔ **未过门不得开工** —— 其 improvement 明写「本轮只作登记,**不排入下一轮开工序列**」<br>**待裁**：(a) 把 I-6 一次性标 `blocked-by-design` 移到队末，让 I-7/I-8/I-10 前移（一次重排，避免走到才卡）(b) 先请 architect 出 schema 形态 + 用户 ratify 打通 I-6 |
| **④** | **I-8**（`openspec` 版本 pinning）· ⇒ **I-10**（归档验收债补齐）· ⇒ **I-11** / **I-12**（openspec change 各占 1 slot）| I-8：无 · I-10：本行自身即载体 · I-11 / I-12：各自的 openspec 可行性 spike 已过 | I-8：锁值 == CI 安装版本 == 本机 CLI（三方一致，机器判据）· I-10：先补 8 份追溯报告 ⇒ 再上 `archive_verify_check.sh`（⛔ 顺序不可颠倒）· I-11 / I-12：归档后旧串 `grep -c` == 0 |
| **⑤** | **I-10**（归档验收债补齐 + 机关落地）| 本行自身即载体 | **⛔ 顺序硬约束**：先补 8 份追溯报告（`<change>-verify.md`）⇒ 再上 `scripts/test/archive_verify_check.sh`；**颠倒则 CI 常红** |
| **⑥** | **I-11**（spec 契约面 stale 引用修正）· **I-12**（doorbell 契约面失真修正 · dGPU 域）—— ✅ **两者均已落地 2026-11-21**（I-11 = `cpptlm-spec-stale-refs-b-class`；I-12 = `cpptlm-doorbell-contract-alignment`）| 各自需 openspec 可行性 spike 已过 | 各新建 1 个 openspec change（各占 1 slot；当前 0/3 足够）⇒ 归档后旧串 `grep -c` == 0 |

**本轮的实测依据（供阶段 ① 直接引用，⛔ 重跑前不得当既成事实）**：

```
5 条超长 KPI 行（全部含 P-5 锚点）：
  .rddf/roadmap.md:5                            3091 字符  锚 = `当前真实 baseline = `
  .rddf/roadmap/phases/phase-10.md:209          3116 字符  锚 = `当前真实 baseline = `
  .rddf/improvements/cpptlm-cep-umbrella.md:306  3015 字符  锚 = `当前真实 baseline = `
  .rddf/improvements/cpptlm-cep-nvlink-sharp.md:31   2921 字符  锚 = `当前实测 **`
  .rddf/improvements/cpptlm-cep-nvlink-sharp.md:211  2761 字符  锚 = `当前实测 **`
I-3 的近零成本依据：3 份状态行写「已完成并归档」，而 AC-4 的字面判据是「已归档」
  ⇒ `已归档` **不是** `已完成并归档` 的子串（语义已满足，字面未满足）⇒ 只需措辞对齐
I-4 的近零成本依据：`bash scripts/test/rddf_stage_gate.sh` 的 P-5 实测输出
  = `✓ P-5 0 处活口径全部同步（锚定 7 处，逐处取值 == 实测）` ⇒ 判据已实现
```
