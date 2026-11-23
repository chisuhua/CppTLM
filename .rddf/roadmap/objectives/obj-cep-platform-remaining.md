# CEP 平台 · 剩余任务总清单（canonical 登记册）

> **性质**: planner 的**剩余任务单一真源**（canonical registry）。台账 `obj-cep-platform.md` 的 Entry 只做**回执 + 指针**，⛔ 不在此处复述清单 ⇒ 避免台账（24k 行 append-only）继续膨胀、也避免清单在多处复述漂移。
> **实测日期**: **2026-11-22** · **实测基线**: 见 §0.1（§0.2 保留 2026-11-16 口径供追溯）。
> **权威关系**: 本册 = **当前有效**的残余登记；台账 §11 的历史 Entry 里的 `R-*` 编号属**历史快照**（⛔ 见 §4 的编号空间冲突说明）。
> **⚠️ 当前有效清单 = §6**（**v5** · 2026-11-22 实测的缺口清单 + **4 个工作流** + 1 个 user 授权请求 · 经 metis 1 轮 + oracle 1 轮 + critic 1 轮审查）。§1 的 **24** 项是 **2026-11-16 口径**（⛔ 本轮未逐条重测）⇒ 保留作历史对照，**严重度排序与排期以 §6 为准**；§1 的数字刷新归 §6 的 **W2-a** 立项单元。
> **⚠️ v4 → v5 的三处结构性变更**（2026-11-22 · 审查收敛的产物，非初稿）：
> ① **队列 2（I-0~I-12）已全部 `done`**（`scripts/test/rddf_ac_carrier.tsv` 44 行；**W1-b 后为 58 行** —— +14 = 第二载体行，见 §6.4 W1-b）⇒ §6.4 的实施队列**收口**，下一轮**不铸「队列 3」**（⛔ 队列 0 P-1/P-3/P-5 从未进过载体表 = 散文队列 ⇒ 在其未收口时新开一节会复刻 §6.5 要防的登记面漂移）⇒ 改为 **§6 v5 刷新 + 4 个工作流（W1~W4）+ 1 个 user 授权请求**；
> ② **§6.2 缺口清单重测**（v4 的 23 项 → v5 的 13 项，v4 的 `S*-x` 编号**冻结为历史快照**，新登记一律用 `G-*` ⛔ 不重编号 —— 旧 ID 被 improvement 单元与台账 Entry 引用，见 §6.4 的编号空间登记表）；
> ③ **§6.1 增第 5 类严重度 S5「主动不承诺」**（不进清单）⇒ 把「档 2 扩族」「mesh 上跑协议语义」「mesh e2e 规模天花板」三项从 v4 的疑似缺口**降为范围声明**（依据见 §6.2 的 S5 段）。

---

## §0 · 实测基线

### §0.1 当前（**2026-11-23 复测** · per §6 的 v5 计划 · ⛔ **不锚定 HEAD**（HEAD 随每次 commit 过时 ⇒ 自造漂移面）⇒ 各行**取值 == 当场重跑**（基线闸 `scripts/test/cep_registry_baseline_check.py` 逐项比对 ⇒ 单改一侧即红））

| 项 | 实测 | 命令 |
|---|---|---|
| 全仓主 spec | **138** 份（`cep-*` **47** + 非 `cep-*` **91**） | `ls openspec/specs/ \| wc -l` |
| `openspec validate --specs --strict` | **55 红 / 138**（83 passed）；**55 份中 `cep-` 前缀 = 0** ⇒ **CEP 47 份 spec 全绿** | `openspec validate --specs --strict` |
| ├ 非 CEP Purpose 占位 | **45** | 同上（WARNING 类去重） |
| └ 非 CEP Requirement 正文 >500 | **26**（与 Purpose 重叠 16） | 同上（按红 spec 去重计数） |
| 闸族 | **49** 个 `cep_*.sh` **+ 14** 个 `cep_*.py` = **63** | `ls scripts/test/cep_*.sh \| wc -l` · `ls scripts/test/cep_*.py \| wc -l` |
| CEP 测试 | **77** 个 `test_cep_*.cc` | `ls test/test_cep_*.cc \| wc -l` |
| OpenSpec | active **0** · 归档 **change 目录数** **170**（CEP **54**）· KPI **0/3**（**空档 ⇒ 可立新提案**） | `ls openspec/changes/ \| grep -v archive \| wc -l` |
| CEP 归档 change 缺独立验收报告 | **0** —— I-10 落地后的现行口径 = 机关闸 `scripts/test/archive_verify_check.sh` 实测「**扫面 35 份 · 基线 35 · git 跟踪 35**」，三检 + provenance 一致性全绿（⛔ 旧「31」「40」等**逐份对账口径已作废**，现行口径**以闸为准**，⛔ 不得再用份数做判据 —— 份数会随每次归档漂移） | `bash scripts/test/archive_verify_check.sh` |
| KPI 活口径同步面 | **7 处** —— 权威清单 = 既有闸 `scripts/test/rddf_stage_gate.sh` 的 `LIVE_FILES`；P-5 实测输出 = 「✓ P-5 **0 处**活口径全部同步（**锚定 7 处**，逐处取值 == 实测）」⇒ 取值全部 == `0/3` | `bash scripts/test/rddf_stage_gate.sh` |
| 顺序机关载体表 | `scripts/test/rddf_ac_carrier.tsv` = **133** 个数据行（**58** 个 `I-*`（44 原始 + 14 W1-b 第二载体）**全 `done`** + **43** 个 `W-*` 行（W1 / W2 / W3 / W4 的实施 + 验收证据）+ **14** 个 `G-*` 行（`G-15` 3 行 · `G-16` 4 行 · `G-17` 2 行 · `G-18` 2 行 · `G-3` / `G-7` / `G-13` 各 1 行验收证据）· **18** 个旧框架治理 `AC-01~AC-09` 行 `pending`） |

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

## §6 · CEP 缺口清单（有效级排序）+ 后续立项计划（**v5** · 2026-11-22）

> **性质**: 本节的**当前有效清单**（§1 为 2026-11-16 历史对照）。本节**不新建第三个登记面** —— 它是本册（canonical 登记册）的一个新节，⛔ 不新建 `.rddf/roadmap/` 下的新文件（守 Entry #334 ④「不新建登记面」否决理由，`obj:18622`）。
> **审查轨迹（v5）**: **metis 1 轮**（gap 分析 · 指出本轮初稿的 3 处测量错误 + 5 处过度陈述 + 5 类缺失判据）→ **oracle 1 轮**（对抗式 · 判定 **APPROVE** · 指出 1 处事实错误 + 1 处排序纪律违反 + 1 处 CI 分裂风险 + 3 处过度陈述）→ **critic 1 轮**（判定 **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · **无 BLOCKER** · 4 条 SHOULD_FIX + 1 条 NIT）⇒ 每轮 findings **逐条复验**（metis / oracle 指出的测量偏差由 build 复测并订正；oracle 无权复验的 4 条由 build 实跑取证）⇒ **本节是审查收敛后的产物，不是初稿**。
> **v4 → v5 的变更摘要**：① 队列 2 全 `done` ⇒ §6.4 收口 + **不铸「队列 3」**，改 **4 个工作流（W1~W4）+ 1 个 user 授权请求**；② §6.2 缺口清单 **23 项 → 13 项**（v4 的 9 项经实测闭合 + 4 项降为范围声明 + 1 项降级）；③ §6.1 增 **S5 主动不承诺**；④ §6.6 增 **P-10 / P-11** 并订正 **P-9**。
> **编号纪律**: v4 的 `S3-x` / `S2-x` / `S1-x` / `S4-x` **冻结为历史快照**（⛔ **不重编号** —— 旧 ID 被 improvement 单元与台账 Entry 引用，重编号即破坏可寻址性）⇒ **v5 新登记一律用 `G-*`**（工作流用 `W-*`），二者与旧 ID 的映射写在 §6.3 归属表与 §6.4 的编号空间登记表。

### §6.1 有效级定义（严重度）与「谁能做」**严格正交**

| 级 | 判据（可点数） |
|---|---|
| **S0 宣称不成立级** | 存在「已完成 / 已达成」宣称，但其证据面与实况**不一致**（或终态完成定义与实况不符） |
| **S1 能力不成立级** | 平台**自己声明的能力**实际不存在 / 不可用（真功能缺口） |
| **S2 契约失真级** | 契约面（spec / ADR 归因 / 计数口径 / 台账措辞）与实况不一致 |
| **S3 治理流程债** | 登记 / 编号 / 同步 / 追溯类债；不改变能力 |
| **S4 外部阻塞级** | 本仓不可推进（外部规范不可读 / 跨仓待办） |
| **S5 主动不承诺级**（**v5 新增**） | 平台**有意不承诺**、且已有裁决/契约明文划界的范围 —— ⛔ **不进缺口清单**，只登记为**范围声明**（判据见下） |

`needs:` ∈ {architect, user, builder, planner, external, PROC}，**不并入严重度**。
> ⚠️ **枚举订正（2026-11-20 · per metis 歧义 8）**：旧枚举含 **`owner`** —— 该维度**已由用户 2026-11-20 废除**（本仓无独立 owner 角色 ⇒ 不可执行），⛔ **不得再用**；另**补入 `planner`**（旧 S2-5 / S2-6 / S3-6 / S3-7 / S3-12 实际都由 planner 承担，旧枚举漏列 ⇒ 与 P-2「严重级与 needs 正交」的机判面自相矛盾）。
> ⚠️ **注意**：`BACKLOG` 出现在 §1.4 的状态码表（`R-6` / `A-1` 行），但**不是** §6 的 `needs` 取值（v4 的 `S3-4` 直接写 `BACKLOG` 于 `needs` 列）⇒ 两处口径不一致，⛔ 待收敛（**登记在 backlog，不在本轮改**）。

**S5 的判据（可否入缺口清单 · 两条件须同时为假才入）**：
- ① 仓内载体（spec / ADR / DoD 措辞）**声称它存在**，**或** ② 已批准 DoD **要求**它 ⇒ 满足任一 ⇒ **是缺口**；
- 两者皆否 ⇒ 是**范围问题** ⇒ 须先立「意图」再立「缺口」⇒ 归 **S5**，**不进 §6.2 清单**。

> ⚠️ **v5 据此降级为 S5 的三项**（⛔ 不进清单、⛔ 不排期、⛔ 不作能力宣称）：
> | 原疑似缺口 | 降级依据 |
> |---|---|
> | 「档 2 扩到第 3 族」 | `test/cep_evidence_tiers.json` 的 `derivation` = `(completeness==EXACT) && (!broadcast_fallback) && (impl!='不参与')` ⇒ 实测 `tier2_applicable=true` **恰 2 个**（`mesif` / `tilelink_c`）⇒ **档 2 已饱和**，扩族被**推导规则禁止**（不是努力不足） |
> | 「mesh 上跑协议语义」 | `docs/architecture/07-coherence-exploration-platform.md:743`「档 2 与档 1 正交 · 跨档强度不可比」+ 台账 Entry #332 ⑦「拓扑级 E2E 装置 = 框架级观测面，不承载一致性语义」⇒ 属**被裁否决的装置类别**；要它成立须先改 `ADR-CEP-19` 决策 9（**ADR 级动作，不是排期动作**） |
> | 「mesh e2e 规模天花板」（仅 `configs/interconnect/cep_mesh_2x2.json` / `cep_mesh_1x2.json` 两份蓝图） | 仓内载体**未声称**更大规模，已批准 DoD **未要求** ⇒ 无缺口，仅登记为观察项 |

### §6.2 缺口清单（CEP 域 · **v5** · 2026-11-22 实测）

> **本轮口径**：2026-11-22 在 HEAD `a09c1ffa` 上**逐条重测** v4 的 23 项 ⇒ **13 项仍开** + **9 项闭合** + **1 项降级为 S5**（**v5 立项时口径**；现行口径见下一行）。v4 的 `S*-x` 编号**冻结为历史快照**（⛔ 不重编号 —— 旧 ID 被 improvement 单元与台账 Entry 引用）。
> **W1 / W3 / W4 全部落地后的口径（2026-11-23 · **登记册缺口清零** · G-17 闭合后）**：**`G-*` 编号空间仍开 = 0 项** —— `G-1` / `G-2` / `G-3` / `G-4` / `G-5` / `G-6` / `G-7` / `G-8` / `G-9` / `G-10` / `G-11` / `G-12` / `G-13` / `G-14` / `G-15` / `G-16` / `G-17` / `G-18` **全部闭合**（证据见「已闭合项」表）；⛔ **本口径仅计 `G-*` 编号空间** —— `G-*` 之外的**真活义务**（如 §6.2 S3 表的 **backlog 单元**）**不计入**本计数（见 §6.2 / §6.8）；后续若发现新缺口 ⇒ 按 §6.4 的「编号空间登记表」用 `G-19+` 续编（⛔ 不重用旧 ID）。
> ⚠️ **重测纪律**：本节每条缺口都带**一条可复跑命令**；⛔ **禁止**引用 v4 的数字而不重跑（v4 → v5 的实测偏差已达 **4 处**：闸族 45→60、CEP 测试 73→77、S3-6 的字符/字节量纲、归档份数口径；**第 5 处**由 W1 执行期实测发现：`G-1` 的「force-add 3 个归档目录」**前提不成立**（三个目录已在 git index），见 W1-c 与 §6.3）。

#### S0 · 宣称不成立级 —— **0 项**（v5 复测维持）

> **诚实结论**：v4 初稿曾列 2 项 S0（DoD 7 口径 / 引擎强度表），经三轮复核**均降级**。v5 复测**未新增 S0**；且 v4 的两处降级依据在 v5 依然成立（`obj §0 DoD` 12 项逐条有证据支撑；「引擎 5 vs 6」已由 `docs/guide/cep-usage.md §12.2.1` 定义三词消解 ⇒ 见 **G-8**，不再是 S0/S2）。
> ⚠️ §0.1 的 stale（v4 载 active 3 / KPI 3/3）方向是**过 restrictive**（虚报已满）⇒ 不制造假绿 ⇒ v5 已按 §0.1 的实测刷新（active **0** / KPI **0/3**）。

#### 已闭合项（**25 行**（v4 旧 ID **7 行** + `G-1` / `G-2` / `G-3` / `G-4` / `G-5` / `G-6` / `G-7` / `G-8` / `G-9` / `G-10` / `G-11` / `G-12` / `G-13` / `G-14` / `G-15` / `G-16` / `G-17` / `G-18`）· v5 复测确认 + W1 / W3-a / W3-d + G-15 落地 + G-17 状态面刷新 + G-18 backlog 单元状态订正 · ⛔ 不再登记为缺口）

| v4 ID | 闭合证据（2026-11-22 复测） |
|---|---|
| **S1-1**（5 条 invariant 未建模） | `python3 scripts/test/cep_invariant_map_check.py` → 「**66/66** = 66 项有牙载体 + **0** 项 ⭭-unmodelled + **0** 项 ⭭-pending」（批 1 + 批 2 已于 2026-11-21 归档）⇒ ⛔ 「66/66」仍**只能读作「66 条全部有载体」**，`INV-CEP-04-IC4` 仍**永久跳空**（编号位 67） |
| **S1-2**（档 2 只 1 族） | `test/cep_evidence_tiers.json` 档 2 登记 = `mesif-observation` + `tilelink_c-observation`（**2 族**，I-7a/I-7b 于 2026-11-21 落地）⇒ ⚠️ **扩到第 3 族不成立**（见 §6.1 的 S5 段） |
| **S2-7**（13 键无仓内 golden） | `test/cep_instance_golden.json` 在位，**且已有闸** `scripts/test/cep_instance_golden_check.py` + `test/CMakeLists.txt:755` 已注册 ⇒ §6.6 P-9 的「当前无闸载体」表述**已过时**（见 W2-c） |
| **S2-8**（ArchForge 闸 CI 假绿） | `scripts/test/cep_archforge_alignment_check.sh` 已打 `[SKIP]` + `test/CMakeLists.txt:648` 注册 `SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"` ⇒ ctest 记 **Skipped 而非 Passed** |
| **S3-1 / S3-3 / S3-4 / S3-10 / S3-11** | 分别由 Q-6 裁决 + I-0 / I-4 / I-8 / Q-2 落地；`bash scripts/test/rddf_stage_gate.sh` rc=0 且 P-5 输出「锚定 **7** 处，逐处取值 == 实测」 |
| **S3-5**（归档验收债） | I-10 于 2026-11-21 落地：机关闸 `scripts/test/archive_verify_check.sh` 实测「**扫面 35 份 · 基线 35 · git 跟踪 35**」全绿 + 甲层抽检 5 份 + 定向补验 3 份（**0 项实现债**）⇒ ⛔ 现行口径**以闸为准**，⛔ 不得再用「份数」做判据 |
| **S3-8 的 B 类 5 项 + C 类 1 项** | I-11（`cpptlm-spec-stale-refs-b-class`）+ I-12（`cpptlm-doorbell-contract-alignment`）均于 2026-11-21 归档；R-13a 经用户授权直改（台账 Entry #517）⇒ **A 类 2 项仍未闭合**（见 **G-11**） |
| **G-2**（**v5 新增 ID** · 2026-11-22 随 **W2-a** 闭合） | **登记册基线 stale** ⇒ 每次归档都重演一次 —— v4 §0.1 载 specs **137**（cep 46）/ 归档 **164**（CEP 50）⇒ v5 实测 **138**（cep **47**）/ **170**（CEP **54**）⇒ 一次性刷新已完成；**残余面 = 「一次性刷新会再次漂移」**⇒ ✅ **已闭合**（2026-11-22 · **W2-a** 落地 · commit `40fe6b69`）：新增活基线比对闸 `scripts/test/cep_registry_baseline_check.py`（已注册 ctest **#153**）⇒ 把 §0.1 的「文档取值」与「当场重算」互为锁（**单改一侧即红**）⇒ **缺口类别被消灭**（不只是这一次刷新）；实测「5 项文档取值 == 实测」· 决定性变异 4 条（改数字 ×3 ⇒ rc=1；破坏行形态 ⇒ rc=2 `STRUCTURE-ERROR`）⇒ 漂移复发形态已被闸覆盖 |
| **G-12**（**v5 新增 ID** · 2026-11-22 随 **W3-c + W3-b** 闭合） | **已裁未落地 = 2 条**（v4 记 3 条 · v5 订正）—— ✅ **两条均已闭合**（2026-11-22）：**P-3**（`obj §4.0-a` 指引强化 · 追加式 · ⛔ 摘要表 sha256 逐字不变）⇒ **W3-c** 闭合（台账 **Entry #523** 落成「指引（强化版）」· **8 行表 sha256 `3f1fdc90…` 前后逐字相等** · 4 个 grep 锚点全命中）；**Q-7**（同一事实 5 处 canonical 化）⇒ ✅ **以「撤回 + 转缺口」形式闭合**（architect **追加裁决 §11**，commit `302465d8`）：⛔ 撤回「5 处 canonical 化 + 机械生成 + 双向对账闸」执行路线（**三条前提实测不成立**：① 生成器只管 07 号文末索引 ② 五处承载**不同语义域**且当前各自自洽 ③ 裁决内「4 处 vs 5 个名字」表述不精确）⇒ 其可执行残余转 **G-15**（S3 · 已随实施闭合）⇒ **W3-b 随之收口** |
| **G-1**（**v5 新增 ID，非 v4 项** · 2026-11-22 随 **W1** 闭合） | ① **clean-clone 免费通过**已消除：`unavailable` 分支**不再置 `ac_hit`** ⇒ 只 `skip` + 计入可见计数 + 收尾环豁免（`scripts/test/rddf_stage_gate.sh:365-374` / `:386-400` / `:402-410`）；② **自指同义反复**已消除：新增 `P-3b`（`:412-438`）+ 载体表补 **14 条第二载体行** ⇒ 闸实测「✓ P-3b 自指载体 **14** 个 AC，均有第二条非登记册载体」。<br>**决定性变异**（`--self-test` · rc=0 · 6 项全绿）：④ clean-clone 态（影子不拷 `openspec/` ⇒ 7 条不可分发 + 合成探针 `Z-CLEAN` 走收尾环豁免 + 收尾环零 fail）· ⑤ 自指缺第二载体必红 · ⑥ 合法形态零误红；**真实仓字节指纹前后相等**（`f7c7c7c37d9e95cd4c90f27f425f3c8a`）。<br>⚠️ **W1 执行期实测的两处前提订正**：`G-1` 原写「force-add 3 个归档目录」—— 实测**三个目录已在 git index**（`git ls-files` 4+4+6=**14 文件**）⇒ 零动作；原写的 `:350` 行号亦已随改动位移。<br>验收报告：`.rddf/reviews/cep-w1-stage-gate-integrity-verify.md` · 提交 `0c2a7ea1`<br>⚠️ **两条必须保留的结论**（防未来回归）：① `unavailable` 的 `skip` **语义本身不得改判 fail**（`.gitignore:64` 排除整个 `openspec/changes/archive/` ⇒ 干净 clone 上归档内容**根本不存在** ⇒ 既不能 fail 也不能 pass）⇒ 修法是「skip + 可见计数 + 收尾环豁免」，⛔ 不是「判红」；② v4 的「顺序机关**已空转**」是**过度陈述** —— P-2a 的队列 2 跳序判据**一直有牙**，无牙的只有上述两个面 |
| **G-8**（**v5 新增 ID** · 2026-11-22 随 **W3-a** 闭合） | 冻结面三处「引擎」三词口径指针落地（`07 号:138` + `protocol-engine.md:44` **同行追加** + `ADR-CEP-20` **文末追加 SU**）⇒ **一次** M1 五步链（台账 **Entry #520** → 产物 commit `d860430a` → `--update --authorized-by` → 清单**单独** commit `e92aa61d` → 无参复验 rc=0）⇒ 清单差集**恰 3 处 `~ hash 变更`**（零新增 / 零移出 ⇒ `artifact_count` **90 → 90** 不变）；⚠️ **零行数位移** ⇒ 07 号索引 A/B 的「行」列与跨仓锚点 `protocol-engine.md:16` 全部不变；⚠️ **本 ID 此前只在 §6.3 归属表与 §6.4 队列 0 出现、从未进过 §6.2 的缺口表**（v5 缺行，本次补记） · 单元 `cep-freeze-lexicon-pointers`（commit `68025dc0`）· 验收报告 `.rddf/reviews/cep-freeze-lexicon-pointers-verify.md`（AC-1~AC-8 = 8/8 ✅）|
| **G-15**（**v5 新增 ID** · 2026-11-22 随 **architect 追加裁决 §11** 闭合） |  | **决策块级机器检查的射程依赖 ADR 章节标题形态** —— `ADR-CEP-21`（🟢 **Proposed** · `## 决策` **无编号**形态）不在决策块快照的覆盖义务内（`cep_decision_block_check.py` 的覆盖义务谓词复用 `section2()`，只认 `## 2 …` 编号章；脚本注释自述「`## 决策` 形态（先例 `ADR-CEP-21`）**不适用**」）⇒ **该 ADR 签发后**，其决策块将落在所有机器射程之外 ⇒ ⚠️ **前瞻性缺口（pre-merge）**：**须在其签发 / 归档前**完成射程修复，否则形成「已签发 ADR 的覆盖漏洞」。<br>⛔ **不得**靠「把标题改成 `## 2. 决策`」达成（那是改已签发 ADR 正文）⇒ 修法**只能在判据侧**：覆盖义务谓词 + 章定位函数 `section2()` **同时**支持两种形态；快照补录 **块数 = 0**（现有 `DECISION_RE` 口径）⇒ `count` **23 → 24** · `total` **不变**；⛔ **不改 `DECISION_RE`**（改它会连带改变其余 23 份的计数口径）\| `bash scripts/test/cep_decision_block_check.sh`（当前 rc=0）· 实测 `grep -c '^### 决策' docs/adr/ADR-CEP-21-*.md` == **0** · 快照 `count` = **23**（磁盘 `ADR-CEP-*.md` = **24**） · 授权 = 台账 **Entry #524** ✅ **已闭合**（2026-11-22 · 台账 **Entry #524** 授权 + **Entry #525** 回执 · 实施单元 `cep-registry-stale-cleanup`）：判据侧**射程去形态化**（新增共享常量 `DECISION_CH_HEADING_RE` ⇒ `## 2 …` 与 `## 决策` 两形态都算决策章；⛔ 精确匹配 ⇒ 不误伤 `## N. 决策汇总`）· 快照 **schema 升 v2** + 补录 `ADR-CEP-21`（**块数 = 0** · `count` 23→24 · `total` **不变** · `authorized_by` → **Entry #524**）· ⛔ `DECISION_RE` **零改动**（其余 23 份计数口径零变化）· ⛔ **零 ADR 正文改动** ⇒ **AC-6 未被违反**；**决定性变异 4 条全按签名红**（M1 覆盖义务对 ADR-CEP-21 开火 / M2「只改谓词不改章定位 ⇒ 登记即红」/M3 快照值自钉住 / M4 驱动壳变异⑥ 语义反转）· 验收报告 `.rddf/reviews/cep-g15-decision-block-coverage-verify.md` \|
| **G-9**（**v5 新增 ID** · 2026-11-22 随 **W3-d** 闭合） | 台账 **Entry #522** 落成 Q-11 的「**责任方 + 触发条件 + 落点**」**合一形态**（追加式）：**责任方** = planner + architect（⛔ 不含 `librarian` —— 本仓无该治理角色）· **触发条件** = 季度窗口 **或** E1 命中（含 E1 结果 / 四族 T1–T4 / `B3b-2` 是否改判）· **落点** = 台账 §11 新 Entry（⛔ 不新建脚本 ——E1 的 cadence 探针已覆盖技术面 ⇒ E2 是判定复核）⇒ **7 处 grep 锚点全命中**；并**追加式**订正 `:16732` 的 N-4 行（原写「librarian + planner」⇒ 与裁决不符），⛔ 台账 append-only ⇒ 原行逐字保留 |

| **G-4**（**v5 新增 ID** · 2026-11-23 闭合） | **登记册缺口陈述本身失真（伪缺口）** —— v4 §6.4 的 I-11 行携带「`PcieTlpBundle` 无布局断言」，该缺口**已被证伪**：`test/test_pcie_slice_wire_format_snapshot.cc:62-71` 有 **29 条 `static_assert`** 固化 `PcieTlpBundle` 的 `sizeof(56)` 与 7 字段偏移，且 `openspec/specs/pcie-slice-prerequisites/spec.md:21` 的 Scenario **点名该文件**、已注册 ctest（`test/CMakeLists.txt:96`）⇒ 原判定只 grep 了 `include/bundles/`、**漏了 test 侧载体**。 ✅ **已闭合**（2026-11-23 · **W4-b**）：§6.4 的 I-11 行**追加**订正（**保留原行** ⛔ 不删原文，⛔ 不破坏 TSV anchor 「**spec 契约面 stale 引用修正**」）⇒ 订正行同时含「**伪缺口 / 已订正**」标记 + 完整证伪证据；⚠️ 附带登记：「登记册缺口陈述缺少**可信度自查**机制」这一条**残余仍在**（本轮只修该条陈述本身，未建自查机制） |
| **G-5**（**v5 新增 ID** · 2026-11-23 闭合） | **improvement 状态行漂移面扩大** —— v4 的 S3-7 只记 **3 份**；v5 实测 `grep -L "已归档"` = **11** 份 ⇒ ⚠️ **口径须按文件全文**（⛔ 非仅 `状态:` 行）且**白名单须含「已落地」**（本仓通用完成态 token；旧白名单只认四种 ⇒ 漏它） ✅ **已闭合**（2026-11-23 · **W4-c**）：**7 份**单元的状态行由「🟡 已立项 / 待立项」回填为「✅ **已落地**」（per §6.4 队列 2 的 **I-5 / I-7a / I-7b / I-8 / I-9 / I-10 / I-11**；其中 4 份的队列行**未记**归档 change 名 ⇒ 状态行如实写「登记册该行未记归档 change 名」⛔ 不臆造）＋ §6.8 的白名单口径与自查命令**同步订正** ⇒ 现行口径 = **命令的实测输出 = 1 份**（`cep-freeze-lexicon-pointers` · **真实活义务** · 2026-11-22 新建） |
| **G-6**（**v5 新增 ID** · 2026-11-23 闭合） | **improvement 单元无归属** —— `cep-memory-model-operational-trace`（2026-11-21 新建 · 状态 token「🟡 待立项」· 文内含「⏳ 待用户 ratify」· **真实活义务**）⇒ 原落 §6.3 归属表**兜底行** ⇒ 违反 §6.3「⛔ 新增缺口必须在本表显式列行」 ✅ **已闭合**（2026-11-23 · **W4-d**）：§6.3 归属表**新增该单元的独立行**（⛔ 不落兜底行）⇒ 单元 ↔ 登记册对账**逐行**可机判（⛔ 原口径只查「是否被提到」⇒ **提到 ≠ 有归属行** ⇒ 旧口径会假绿） |
| **G-16**（**v5 新增 ID** · 2026-11-23 闭合） | **`ADR_LIST` 缺反向完整性检查** —— `scripts/test/cep_docs_sync_check.sh` 的 `ADR_LIST` 是**正向**覆盖集（ADR_LIST → 磁盘：存在 + 含「不变量/决策」节）；⛔ **无**「磁盘 → ADR_LIST」的反向检查 ⇒ 该脚本注释自述「漏登记 = 新 ADR **静默脱检**」；⚠️ 结构闸的分区完整性（磁盘 == `decision_adrs` ∪ `excluded`）可兜底**归类**，但归类后**仍可能**漏加 `ADR_LIST` ⇒ 与 G-15 **非同型**（G-15 = 射程依赖形态；本项 = 缺反向检查） ✅ **已闭合**（2026-11-23 · **独立批次** · 台账 **Entry #527** 授权 + #528 回执）：`cep_docs_sync_check.sh` 增**第 [13/13] 组**「ADR 清单反向完整性（磁盘 ⇒ 覆盖集）」——判据 = 磁盘 `ADR-CEP-*.md` **全量** ⇒ 每份必须落在「`ADR_LIST`（复用脚本内既有数组 = **单一真源**，⛔ 不另建第二份清单）∪ **显式排除集**（新数据文件 `scripts/test/cep_adr_exclude.txt`，每条须带**非空理由** + **磁盘上真实存在** + **不得**与 `ADR_LIST` 重复 ⇒ 三条自校验堵死「纸面排除 = 无声脱检」）」之内 ⇒ **新增 ADR 忘登记即判红**（点名具体文件，非计数 ≥1） |
| **G-3**（**v5 新增 ID** · 2026-11-23 闭合） | **台账行内自相矛盾** —— 同行散文写「`openspec/changes/` 当前 **3 个 active**」而同行结论写「当前 Proposed 并发 = **0/3**」⇒ 实测 active = **0** ⇒ 散文 stale（结论对）⇒ 读法陷阱 ✅ **已闭合**（2026-11-23 · **W4-a** · 台账 **Entry #529** 追加式订正）：`obj-cep-platform.md:474`（`### 5.2 当前真实 baseline` 的 **living 现状行**）的散文**订正为 0 个 active**（实测 active = **0**）⇒ **同行现自洽**（散文 0 / 结论 0/3）⇒ 读法陷阱消除；⛔ **追加式**（未回改任何历史 Entry 正文）⇒ 台账内「3 个 active」残余 **4 处**逐处核实**全部非活口径**（3 处历史 Entry 正文内 + 1 处历史前置条件快照 + 1 处历史 diff 描述）⇒ **活口径面 0 处残留** |
| **G-7**（**v5 新增 ID** · 2026-11-23 闭合） | **KPI 行仅剩 1 处超阈** —— v4 的 S3-6 结论「4 份文档 KPI 行单行 6.3~6.8K 字符」**已过时**（v4 实测用 `awk length` = **字节**；v5 按**字符** `wc -m` 重测：`.rddf/roadmap.md:5`=**148** · `phase-10.md:209`=**165** · `cpptlm-cep-umbrella.md:306`=**90** · `cpptlm-cep-nvlink-sharp.md:31`=**125** · `:211`=**139** · `AGENTS.md:313`=**839** ⇒ **全部 ≤ 1000 ✅**）⇒ **唯一仍超 = `obj-cep-platform.md:474` = 2964 字符**，而闸头 `scripts/test/rddf_stage_gate.sh:419-421` **明写它不在 I-2 的 In-Scope 范围内**（闸注释自述该行 2963 字符）⇒ ⛔ **结论须订正**：不是「4 处未达标」，而是「**1 处超阈 + 1 处范围错配**」 ✅ **已闭合**（2026-11-23 · **W4-e** · 台账 **Entry #529**）：**第三条路** —— 该节**自身**（台账 `:476` / `:477`）已写明「**本节只承载活口径**，逐次 KPI 变更的归档流水**不堆在本节** ⇒ 全部见 §11 台账」与「**已由 §11 台账完整承载**(append-only)⇒ **删除不丢信息**」⇒ 删去该行 **33 段**归档流水 ⇒ **2963 → 253 字符** ⇒ **超阈自解**（⛔ 无需扩 `KPI_LINE_MAX_FILES`、⛔ 无需「显式接受」—— 原二选一裁决的**前提**是「保留流水」，而流水本就不该在此）；⛔ **零信息损失**：该行引用的 **28** 个台账 Entry **28/28 全部真实存在**（逐条可回查）⇒ **零 append-only 违规**（append-only 约束的是**历史 Entry 正文**，`§5.2` 是 living 现状行）· 台账行数**零位移** ⇒ 其后行号锚点不受影响 |
| **G-13**（**v5 新增 ID** · 2026-11-23 闭合） | **5 份 PCIe spec 的中文 trailer 状态行陈旧** —— `link-layer-and-fc`「Phase 1 实施中」· `130b-encoding`「Phase 2 实施中」· `axi-stream-adapter`「Phase 5 实施中」· `axi4-mapper`「Phase 6 实施中」· `host-bypass-and-rc`「Phase 7 实施中」⇒ 与 `AGENTS.md` §PHASE STATE「**Phase 1-8 全部交付**」矛盾 ✅ **已闭合**（2026-11-23 · **user 授权**「继续推进」/「继续」· 台账 **Entry #530**）：5 份**全部**改为 `✅ 已实施 —— Phase N 交付`（per 已交付范本 `pcie-ip-microarch/spec.md:296` 的形态）⇒ 机判判据 **`grep -c "Phase [1-7] 实施中"` 5 → 0**（实测）；⚠️ **判据订正（实施期实测）**：登记册原写「+ **一次五步链同批**」—— 该前提**不成立**：这 5 份 `openspec/specs/<name>/spec.md` **均不在** `cep_freeze_check.sh` 的 `FROZEN_GLOBS`（冻结面只含 `openspec/specs/cep-*/spec.md` 等 ⇒ 这 5 份非 `cep-*` 前缀）⇒ 改它们**不触五步链**（实测 `cep_freeze_check.sh` rc=0、90 产物 hash 不变）⇒ 「台账与清单不得合并提交」的约束随之**不适用**；另实测 `openspec validate --specs --strict` **改前改后同为 83 passed / 55 failed**（这 5 份**逐个 valid**）⇒ 改动**零回归** |
| **G-10**（**v5 新增 ID** · 2026-11-23 闭合） | **剩余 grep 判据未接闸** —— §6.3.2 的旧口径串表在本册 §6.3.2 处自写「⚠️ **本表的 grep 尚未接入闸**」⇒ 下游漂移无自动守门 ✅ **已闭合**（2026-11-22 · **W2-b**，commit `40fe6b69`）：新增 `scripts/test/cep_registry_stale_refs_check.sh`（已注册 ctest **#154**）⇒ §6.3.2 的旧串 + `MARK` 词表**移进闸文件** + `sed` 截断排除约定机器化 ⇒ 实测「**40 项全部为 0**」 ⇒ 判据从「靠人跑」变为「机器守门」（⚠️ §6.3.2 的「尚未接入闸」自述已于 2026-11-23 同步订正） |
| **G-11**（**v5 新增 ID** · 2026-11-23 闭合） | **R-17 / R-19（A 类 2 项）** —— ① **R-19**：`AGENTS.md` 记载的 LWC handler 名 **stale**（`handle_cr_transaction()` / `handle_doorbell_transaction()` ⇒ 真实名 = **`cr_write()` / `doorbell_write()`**，per `include/tlm/lwc/lwc_bus_hub.hh:94/107`） ② **R-17**：2 份来源 change 的**归档形态从未进过 git**（`.gitignore:64` 排除整个 `openspec/changes/archive/`） ⇒ **正确判据 = `git-history-proposal` 形态**（台账 Entry 的 M-3 已订正）：`git show 0345d283:…/cpptlm-dgpu-soc-timing-mvp/proposal.md`（168 行）与 `git show 689752ef:…/cpptlm-driver-visible-minimal-soc/proposal.md`（228 行）⇒ **实测可达** ✅ **已闭合**（2026-11-23 · **W4-f**）：R-19 的 `AGENTS.md:423` **同行替换**两个方法名（⛔ 行数不变 ⇒ P-5 锚点 `AGENTS.md:313` 零位移）＋ R-17 的完成定义形态在 §6.3 归属表**订正**为 `git-history-proposal`（⛔ **不要求**归档目录入库） |
| **G-14**（**v5 新增 ID** · 2026-11-23 闭合） |  | **`--phase A\|B\|C` 旧阶段模式恒红** —— 三个框架治理 change 于 2026-11-18~19 全部归档且 task 全勾 ⇒ 旧模式永久判红（既有失败，HEAD 版同签名）⇒ ⚠️ CI 不受影响（ctest 只注册无参调用）⇒ 但闸头文档化了该用法 ⇒ 机关自身报假红 = 噪声源 ✅ **已闭合**（2026-11-22 · **W2-e**，commit `40fe6b69`）：闸内新增退役判定（**三 change 均不在工作树** ⇒ 报「已退役」并 rc=0） ⇒ `--phase A` / `B` / `C` 三条**实测均 rc=0**；⛔ 旧分支逻辑**保留**（change 被重新立项时仍按原语义判序） \|
| **G-17**（**v5 新增 ID** · 2026-11-23 闭合） | **登记册自身状态面未随实施结果刷新** —— §6.2 口径头已声明「`G-*` 仍开 = 0 项」，但 §6.4 队列 0 / 队列 1、§6.4 的 W2 / W3 / W4 表、§6.9 实施路径表的**状态性标注**仍写「未落地 / 待授权 / 仍开 / 未处置」；§0.1 标题**锚定 HEAD**（必然随每次 commit 过时 ⇒ 自造漂移面）；§6.2 的 S2 / S3 标题份数与表体不符（**6** 项 vs 0 行 · **4** 项 vs 1 行）；编号空间登记表与 §6.3 兜底注仍写 `G-1~G-14`（漏 `G-15` / `G-16`）；载体表**缺 `W-4a` / `W-4e`** ⇒ 违反 §6.9 的「每完成一个工作流 ⇒ 立刻把载体表 `state` 置 `done`」的**子项粒度**  ✅ **已闭合**（2026-11-23）：登记册 **29 处**刷新（25 处状态面 + 4 处 G-17 回填）+ 载体表 **+6 行**（`W-4a` / `W-4e` / `G-17` 各 2 行）⇒ 载体表 `W-*` 覆盖 **W-1a~W-4e 无缺号**；⚠️ **量纲纪律**（per §6.6 P-11）：S2 / S3 标题的份数**删除**（⛔ 不用「N 份」—— 每次归档都漂移）⇒ **消灭类别而非实例**；⚠️ **零锚点破坏**（载体表既有 **5 个**登记册 anchor 逐处核对**均不在**本次刷新位置内）· ⛔ 零冻结面改动（登记册不在 `FROZEN_GLOBS` ⇒ **不触五步链**）· ⚠️ **附带残余**：状态标注与实测的一致性**几乎无守门闸**（变异实测 M1~M4：5 条判据**仅 1 条**有机检 ← baseline 闸；判据 1/4/5 与子项级覆盖为**纯人工**）⇒ 下次实施仍会漂移且无闸自动发现 ⇒ 与 **G-4** 附带残余**同型** ⇒ **待单独立项**（⛔ 本轮不新建闸脚本类型）|
| **G-18**（**v5 新增 ID** · 2026-11-23 闭合） | **backlog 单元的状态行陈旧 + 登记册归属面未反映真实待决点** —— `cep-memory-model-operational-trace` 的 frontmatter 状态行写「🟡 **待立项**」+「⏳ **待用户 ratify**(… + **I-6 的 R-2**)」⇒ **双陈旧**：① **R-2a~R-2f 已于 2026-11-21 ratify**（裁决记录 §一 决策 1 + §五）⇒ 该半句失效 ② 立项**形态**早已拍板（§一 决策 2「登记 backlog，独立立项解决」）⇒ 「待立项」亦失效；⚠️ 且单元草稿写的 **AC-16 开工门「当前 0」**已过时（实测 = **1**，I-6 落地后） ✅ **已闭合**（2026-11-23 · **G-18**）：单元状态 token ⇒ **「🟢 已登记·backlog」**（含「立项形态已拍板 / R-2 已 ratify / AC-16 开门门实测 = 1 / 待触发条件 / 开工前置链 = architect 裁 AC-1 → 用户 ratify → planner 立 change → builder 实施」）；登记册 §6.2 S3 表行 + 说明行 + §6.8 单元表行 + 白名单（⛔ 加**带上下文**的 `已登记·backlog`，⛔ **不用裸词** ⇒ 鉴别力损失受控）同步订正 ⇒ §6.8 自查命令实测输出**仍 = 1 份**（口径不变）；⛔ **未立项 ⇒ 不占 KPI slot**；⛔ 零冻结面改动（单元 + 登记册均不在 `FROZEN_GLOBS` ⇒ **不触五步链**）· ⚠️ **附带残余**：§6.8 白名单闸的**域 = 文件全文**（W4-c 口径订正 ②）⇒ 「状态行漂移」**可被正文偶然词掩盖**（实测：本单元改前即因其正文 AC-16 行含「已落地」而被排除 ⇒ **本次白名单扩词对闸行为无变化**，只消除**偶然依赖**）⇒ 该类漂移**闸不可发现** ⇒ ⛔ **本轮不改口径域**（改闸的判据面属范围外 + 刚经 W4-c 裁定）⇒ **待单独立项**|
#### S1 · 能力不成立级 —— **0 项**（v5 复测）

> ⛔ **S1 空集是结论不是遗漏**：v4 的 2 项（S1-1 / S1-2）已闭合；而三项曾被当作「真功能缺口」的候选项（档 2 扩族 / mesh 上跑协议语义 / mesh e2e 规模天花板）经 §6.1 的 S5 判据**降为范围声明** ⇒ 本级无「平台自己声明的能力实际不存在」的实例。

#### S2 · 契约失真级（v5）

> **本级表体为空** —— v5 立项时记 6 项，其后逐项闭合并移入「已闭合项」表（v4 的 `S2-x` ID **冻结为历史快照**，per 本节口径头；各 `S2-x` 的承接 `G-*` 闭合证据链见「已闭合项」表 + §6.3 归属表）⇒ ⛔ **空表体是结论不是遗漏**（per §6.6 P-4：v5 逐项重测无新实例）。

| # | 缺口 | 证据 | `needs` |
|:--:|---|---|---|

#### S3 · 治理流程债（v5）

> **本级表体仅存 1 行** —— 该行是**单元 ↔ 登记册归属**（`cep-memory-model-operational-trace` · 归属 **W4-d** · ✅ **已闭合** 2026-11-23），⛔ **不是**未闭合缺口；v5 立项时记 4 项（`G-1` / `G-9` / `G-14` 等）⇒ 其后逐项闭合并移入「已闭合项」表。
> ⚠️ **该单元本身仍是真活义务** —— 状态 token「🟢 **已登记·backlog**」（2026-11-23 · **G-18** 处置）：立项**形态**早已拍板（裁决记录 §一 决策 2）+ **R-2a~R-2f 早已 ratify** + I-6 已落地 ⇒ **AC-16 开工门实测 = 1** ⇒ ⏭ **待的不是用户拍板，而是 architect 裁 AC-1**（「每域模型来源 + 域间边构造规则」⇒ 新建 ADR）→ **然后**才轮到你 ratify；⏸ 当前处于 **backlog · 待触发条件**（「需要复合一致性公理可判时」，per 裁决记录 §三）⇒ ⛔ **不占 KPI slot**（未立项）⇒ ⛔ **不计入** §6.2 口径头的「`G-*` 仍开 = 0 项」计数（见 §6.8 单元表）。

| # | 缺口 | `needs` |
|:--:|---|---|
| `cep-memory-model-operational-trace` | **W4-d**（独立批次 · ✅ **归属已登记** · 状态 token「🟢 **已登记·backlog**」⇒ **未立项 ⇒ 不占 KPI slot**） | §6.3 **显式列行**（⛔ 不落兜底行）⇒ 单元 ↔ 登记册对账**逐行**可机判（⛔ 原口径只查「是否被提到」⇒ 提到 ≠ 有归属行）|

#### S4 · 外部阻塞级（**2** 组 · v5 维持）

| # | 缺口 | 现状 |
|:--:|---|---|
| **S4-1** | **B3b-2 = 0/4**（`cxl_2_0` frozen / `cxl_3_0_symmetric` deferred / `nvlink` deferred / `ualink` deferred；**4/4 `blocking_type = external`**）| 已登记 `test/cep_b3b2_deferrals.json` + 机器守门 `cep_b3b2_deferral_check.sh` + D-4 探针已真实执行（8/8 槽位全覆盖，4 external + 4 internal **全部 miss**）⇒ **不得**改写为已达成。⚠️ **E2 季度复审义务**的载体形态见 **G-9**（E1 已被 `cep_b3b2_d4_quarterly_review.sh` 覆盖 —— 该脚本已存在且是硬闸，刻意 NOT 注册 ctest） |
| **S4-2** | **ArchForge 跨仓 4 项**（C1/R-5/C2 的 T7.8 遗留）：3 份契约状态改「⏸ 已归档」+ `coherent_xbar.md` Header 路径漂移 | §1.5.3 / §5 |

#### 仓级（非 CEP 域 · **4** 项 · 本轮**仅登记不排期**）

45 份非 CEP spec 的 Purpose 占位 · 26 份非 CEP Requirement 正文 >500 · `param-type-conversion` 的 Scenario 前提失效（`:61-62` 写「MemoryTLM **未准入**类型」，但 C2 已准入）· **R-10 / R-11 / R-16 孤儿 / 同名复制 / 未实施 spec 存废裁定（5 份，对象全为非 CEP spec ⇒ 与「排除 GPU/PCIe/dGPU 域」冲突 ⇒ 不排进本计划队列）**。

#### 观察项（**4** 项 · ⛔ 既不是缺口也不是范围声明 · 仅登记）

| # | 观察项 | 登记理由 |
|:--:|---|---|
| **O-1** | `cxl_state_spec` 68 格中 **42 格 Unknown**（覆盖 26/68） | 与 S4-1 的「4/4 `blocking_type=external`」存在**口径张力** —— 载体侧的 Unknown 是**本仓内**事实，而阻塞判定为**外部** ⇒ 二者关系本轮**未裁** ⇒ ⛔ 不擅自改写 S4-1 的分母，也不擅自开工补表 |
| **O-2** | 档 0 证据面样本极薄 —— `test/cep_e2e_runs.jsonl` 仅 **3 条**（2 条 `grandfathered` + 1 条 `cep-full-green-2026-11-13`） | 档 0 的载体形态与档位声明义务已由 `ADR-CEP-20` 决策 6 + 闸 T1~T3 锁定 ⇒ 样本量非契约面缺口 ⇒ 仅登记 |
| **O-3** | `obj-cep-platform.md:970` 的 L1「CI 红线」与 `:16457` 的「已清」互斥 | 台账 append-only ⇒ 只能追加式订正；本轮未裁 ⇒ 登记待下轮 |
| **O-4** | `cxl_3_0_symmetric` / `cxl_dcoh` / `nvlink_sharp` / `ualink_inc` 四份 state_spec **无转移表** | 与 `test/cep_b3b2_deferrals.json` 的 T1 阻塞登记**同源** ⇒ 不另立缺口，避免与 S4-1 重复登记 |

### §6.3 缺口归属表（**唯一认领**；⛔ 重复认领 / 无人认领即红）

| 缺口 | 归属 | 完成定义形态 |
|---|---|---|
| **G-1** | **W1 · ✅ 已完成**（2026-11-22 · `0c2a7ea1`） | 闸 `rddf_stage_gate.sh`：`unavailable` ⇒ skip + **计入「不可分发载体 N 个」告警计数（⛔ 永不设阈值判红）** + 收尾环豁免 `unavailable-only` 的 AC（实测 `:365-374` / `:386-400` / `:402-410`）；自指行须另有第二条非登记册载体（**先补 14 行、同一 commit 再启用判红** ⇒ 实测 `P-3b` 绿）；~~force-add 3 个框架治理 change 归档目录~~（**旧表述**）⇒ **W1-c 实测前提不成立：三目录已在 git index（4+4+6=14 文件）⇒ 零动作**；3 条决定性变异入 `--self-test`（实测 6 项全绿） |
| **G-14** | ✅ **已完成**（2026-11-23 · 证据见 §6.2「已闭合项」表）· **W2-e** | `--phase A\|B\|C` 在「三 change 均归档」状态下不得判红：**退役**或**显式报「已退役」并 rc=0** 二选一，且闸头用法说明同步；⛔ 不得保留「永久红」 |
| **G-2** | **W2-a · ✅ 已完成**（2026-11-22 · commit `40fe6b69`） | 登记册 §0.1 每行改成「取值 == 该命令输出」+ 独立活基线比对闸（`scripts/test/cep_registry_baseline_check.py` · ctest **#153**）⇒ **手改一个数字即判红** |
| **G-3** | **W4-a · ✅ 已完成**（2026-11-23 · 台账 **Entry #529** 追加式订正） | 台账**追加式**订正 Entry 存在 + 旧串在「非订正引述」行上为 0（⛔ 台账 append-only ⇒ 不回改历史 Entry）—— 实测：`:474` 散文**订正为 0 个 active** ⇒ 同行自洽（散文 0 / 结论 0/3）⇒ 活口径面「3 个 active」**0 处残留**（残余 4 处全在历史 Entry 正文 / 历史快照内）|
| **G-4** | ✅ **已完成**（2026-11-23 · 证据见 §6.2「已闭合项」表）· **W4-b** | 登记册 §6.4 的 I-11 行**追加**订正（⛔ 不删原文）+ 订正行同时含「伪缺口 / 已订正」标记 + 载明证伪证据（29 条 `static_assert` + spec Scenario 点名） |
| **G-5** | ✅ **已完成**（2026-11-23 · 证据见 §6.2「已闭合项」表）· **W4-c** | 8 份 stale improvement 状态行含「已归档」**且** §6.8 自查命令输出为空（白名单口径） |
| **G-6** | ✅ **已完成**（2026-11-23 · 证据见 §6.2「已闭合项」表）· **W4-d** | 本表**显式列行**登记 `cep-memory-model-operational-trace`（⛔ 不落兜底行）⇒ 单元 ↔ 登记册对账命令输出为空 |
| **G-7** | **W4-e · ✅ 已完成**（2026-11-23 · 台账 **Entry #529**） | S3-6 行收口（结论改为「1 处超阈 + 1 处范围错配」）+ `obj:474` 的处置**第三条路**（该节自身 `:476`/`:477` 已写明「归档流水不堆本节」+「已由 §11 台账完整承载 ⇒ 删除不丢信息」）⇒ 删去 33 段流水 ⇒ **2963 → 253 字符** ⇒ **超阈自解**（⛔ 无需扩阈值 / 无需显式接受）⇒ 闸头注释与实际取值仍一致 |
| **G-8** | **W3-a**（队列 0 的 **P-5**）· 🆕 **归属单元 = `cep-freeze-lexicon-pointers`**（2026-11-22 立项 · per architect 裁决记录 §5.2 末段「归属 `cep-registry-stale-cleanup`？⇒ ⛔ **不** ⇒ **另立单元**」—— 理由：该单元的 **AC-6** 明写「`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/`」⇒ 物理不可执行）| 冻结面三处加「引擎口径见 `cep-usage.md §12.2.1`」指针（⛔ ADR 只追加 SU）⇒ **一次**五步链同批（⛔ 不分两次）+ 无参 `cep_freeze_check.sh` rc=0；⚠️ **三处均「同行追加 / 文末追加 SU」⇒ 零行数位移**（`07 号` 索引 A/B 行号列 + 跨仓锚点 `protocol-engine.md:16` 全部不受影响）|
| **G-9** | **W3-d · ✅ 已完成**（2026-11-22 · 台账 Entry #522） | 台账新增行**同时**含「责任方 / 触发条件 / 落点」三词（承认 `:16732` / `:23123` 散件已存在，只做合一形态）⇒ 实测 **7 处锚点命中**；并追加式订正 `:16732` 的责任方（⛔ 台账 append-only ⇒ 原行保留）|
| **G-10** | ✅ **已完成**（2026-11-23 · 证据见 §6.2「已闭合项」表）· **W2-b** | §6.3.2 的 9 条旧串 + `MARK` 词表**移进独立小闸** + `sed` 截断排除约定机器化 ⇒ 把任一订正文本改回旧串即判红 |
| **G-11** | ✅ **已完成**（2026-11-23 · 证据见 §6.2「已闭合项」表）· **W4-f** | `cep-doc-hygiene-kpi-and-impl-state` 收口：R-19 旧串 `handle_cr_transaction` `grep -c` == **0** + R-17 的 2 份来源 change 按 **`git-history-proposal` 形态**可达（`git show 0345d283:openspec/changes/cpptlm-dgpu-soc-timing-mvp/proposal.md` 168 行 · `git show 689752ef:openspec/changes/cpptlm-driver-visible-minimal-soc/proposal.md` 228 行 ⇒ **实测可达**；⛔ **不要求**归档目录入库 —— 该两者的归档形态**从未进过 git**（`.gitignore:64` 排除整个 archive/），台账 Entry 的 M-3 已把归因订正为 `git-history-proposal` 形态 ⇒ **比『归档目录可解析』强且可达**） |
| **G-12** | **W3-c + W3-b · ✅ 两条均已闭合**（2026-11-22） | P-3：订正 Entry 存在（**#523**）+ grep 命中 + **8 行摘要表 sha256 逐字未变**；Q-7：architect **追加裁决 §11**（commit `302465d8`）⇒ ⛔ 撤回执行路线 + 残余转 **G-15**（✅ 已闭合）⇒ **W3-b 收口** |
| **G-13** | **user 已授权 · ✅ 已完成**（2026-11-23 · 授权来源 = 用户「**继续推进**」/「**继续**」两轮指令 · 台账 **Entry #530**）| 一条授权 Entry（用户指令）· 5 个文件路径**逐条枚举**（`link-layer-and-fc` / `130b-encoding` / `axi-stream-adapter` / `axi4-mapper` / `host-bypass-and-rc` 的 `spec.md`）· 机判前后判据 `grep -c "Phase [1-7] 实施中"` **5 → 0**（实测）· ⚠️ **判据订正**：这 5 份**不在冻结面** ⇒ **不触五步链**（登记册原写「一次五步链同批」前提不成立）|
| **G-15** | **独立批次**（`cep-registry-stale-cleanup` · ✅ **2026-11-22 已完成** · 台账 Entry #524 授权 + #525 回执） | 覆盖义务谓词 + 章定位函数 `section2()` **同时**支持 `## 2 …` 与 `## 决策` 两形态；快照补 `ADR-CEP-21`（**块数 = 0** · `count` 23→24 · `total` **不变** · `authorized_by` 指向 **Entry #524**）；⛔ 不改 `DECISION_RE` · ⛔ 不改 ADR 正文 |
| **G-16** | **独立批次**（`cep-registry-stale-cleanup` · ✅ **2026-11-23 已完成** · 台账 **Entry #527** 授权 + **#528** 回执 · 验收报告 `.rddf/reviews/cep-g16-adr-list-reverse-verify.md`） | `cep_docs_sync_check.sh` 增**反向**完整性检查（磁盘 `ADR-CEP-*.md` 全量 ⇒ 须被 `ADR_LIST` ∪ 显式排除集覆盖）⇒ 新 ADR 漏登记即判红 |
| **G-17** | **独立批次**（✅ **2026-11-23 已完成** · 台账 **Entry #531** · 验收报告 `.rddf/reviews/cep-g17-registry-status-sync-verify.md`）| 登记册 **25 处**状态面刷新（队列 0 / 队列 1 / W2 表 / W3 / W4 批 2 / §6.9 四阶段 / §6.2 口径头 + S2 / S3 标题 / §6.3 兜底注 / 编号空间表 / §0.1 标题）+ **载体表 +4 行**（`W-4a` / `W-4e`）⇒ 载体表 `W-*` 覆盖 W-1a~W-4e **无缺号**；⛔ **只改状态标注，不动判定实质**；⛔ 载体表既有 **5 个**登记册 anchor 零破坏；⛔ 零冻结面改动 ⇒ **不触五步链** |
| **G-18** | **独立批次**（✅ **2026-11-23 已完成** · 台账 **Entry #532** · 验收报告 `.rddf/reviews/cep-b1-backlog-status-verify.md`）| `cep-memory-model-operational-trace` 的状态 token ⇒ **「🟢 已登记·backlog」**（⛔ 删掉已失效的「⏳ 待用户 ratify（… + I-6 的 R-2）」与「🟡 待立项」）+ 登记册 §6.2 S3 表行 / 说明行 / §6.8 单元表行 / 白名单同步订正 ⇒ §6.8 自查实测输出仍 = **1 份**；⛔ **未立项 ⇒ 不占 KPI slot**；⛔ 零冻结面改动 ⇒ **不触五步链** |
| **S4-1** | ⭕ **不排期**（治理上正确处置 = 登记）| `test/cep_b3b2_deferrals.json` + `cep_b3b2_deferral_check.sh` 已机器守门 ⇒ ⛔ 不得进入任何工作流的 AC |
| **S4-2** | ⭕ **不排期**（跨仓）| 登记于台账；⛔ 本仓不可推进 |
| **O-1 ~ O-4** | ⭕ **仅登记** | 观察项，⛔ 不进任何工作流 |
| 其余（v4 的 `S*-x`） | 见 §6.2 的「已闭合项」表 | — |

> ⚠️ **归属表的兜底行（「其余 → 见 §6.2/§6.4」）本身不可机判「无人认领」** ⇒ v5 已把 **G-1~G-18 全部显式列行**（⛔ 不得再落兜底行；`G-1` 已完成后**保留行**并标 ✅，以便与 §6.2 已闭合项对账）；兜底行**仅**覆盖已闭合项与观察项。
> ⛔ **单元 ↔ 登记册对账（metis B10 的 AC）**：`for f in .rddf/improvements/cep-*.md; do grep -q "$(basename "$f" .md)" .rddf/roadmap/objectives/obj-cep-platform-remaining.md || echo "无归属: $f"; done` ⇒ **输出须为空**（v5 实测已为空；⚠️ **空 ≠ 合规** —— 该命令只查「是否被提到」，⛔ **不查「是否有归属表行」** ⇒ v5 起**以本表的显式列行为准**）。

#### §6.3.1 裁决吸收矩阵（architect 裁决 → improvement 判据回填 · **P-0 义务**）

> **义务来源**：architect 裁决记录 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §8.1 **M-4**（oracle 复核订正）—— 「裁决一经落盘，**planner 必须把每条裁决的可机判判据逐条回填**进归属 improvement 单元的 `## Acceptance` 段」⇒ ⚠️ **先于任何工作流的开工**（否则「前置命中判据」形同虚设）。
> **状态**：✅ **已回填**（2026-11-20 · planner）· **2026-11-22 补记**：v5 复测发现 **Q-3 早已落地**（`docs/guide/cep-usage.md:169` 的 `## 12.7` + `:190` 的两条拦截点）⇒ v4 把 Q-3 记为「未落地」是**事实错误**，本表订正。

| 裁决 | 内容 | 回填落点（improvement · AC） | 状态 |
|:--:|---|---|:--:|
| **Q-3**（v5 订正） | 「DoD 12/12 ⇒ 平台完成」读法拦截点 | **✅ 已落地**于 `docs/guide/cep-usage.md §12.7`（`:169`）+ 拦截点 `:190`（引用方 `:67` 已引）⇒ **本轮无需回填**（v4 误记为待落地） | ✅ |
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
| **Q-8 · user 拍板** | D-1 验收债**分两层**处置：乙层全补 · 甲层封存+抽检 · 废除「0.5 d/38 份」改「每份 ≥0.5 d 下限」· **机关 = 独立闸**（封存边界 归档日期 < 2026-11-12，先补后上闸）| `cep-d1-archive-verify-backfill`（新建）| ✅ **2026-11-21 已落地**（I-10）|
| **Q-9 · user 拍板** | **废除 OWNER 维度** + 8 项逐项终态（A 类 2 → doc-hygiene · B 类 5 → openspec change · C 类 1 → architect）| 登记册 §1.3 已重写 + `cep-spec-stale-refs-r14-r15-r20`（新建）+ Q-12 | ✅ **B/C 类已落地**（I-11 / I-12）；**A 类 2 项仍开**（见 G-11）|
| **Q-10 · user 拍板** | **采方案甲**：缺席打 `[SKIP]` + `SKIP_REGULAR_EXPRESSION` ⇒ ctest 记 **Skipped 而非 Passed** | `cep-archforge-ci-failclosed` · AC-1 / What Changes #1 | ✅ |
| **Q-13 · user 拍板** | **闸统一覆盖 + 乙层扩到 8 份**（5 CEP + 3 非 CEP；⛔ 不按 `cep` 前缀特判、⛔ 不做名字白名单豁免）| `cep-d1-archive-verify-backfill`（scope 已扩）| ✅ **2026-11-21 已落地**（I-10）|
| **§10.9.3 · R-12/R-13 改归 B 类** | 二者落点**都在 `openspec/specs/`** ⇒ A 类单元 AC-6 禁止 specs diff ⇒ 原归属物理不可执行 ⇒ **A 类 2 项 · B 类 5 项 · C 类 1 项** | `cep-spec-stale-refs-r14-r15-r20`（3 → **5** 项）· `cep-doc-hygiene-kpi-and-impl-state`（只留 R-17 / R-19）| ✅ |

#### §6.3.2 旧口径残留 grep（**传播的反向判据** · per metis 缺失 AC 5 · v5 增补）

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
| 🆕 **v5 · 闸族计数** | `**45** 个 \`cep_*.sh\`` | 登记册 §0.1（v5 载 **47** `.sh` **+ 13** `.py`）· ⛔ **白名单**：§0.2 历史节保留原样 |
| 🆕 **v5 · 测试计数** | `**73** 个 \`test_cep_*.cc\`` | 登记册 §0.1（v5 载 **77**）|
| 🆕 **v5 · 归档目录计数** | `**164**（CEP **50**）` | 登记册 §0.1（v5 载 **170**（CEP **54**））|
| 🆕 **v5 · spec 总数** | `**137** 份（\`cep-*\` **46**` | 登记册 §0.1（v5 载 **138**（cep **47**））|
| 🆕 **v5 · S3-6 字节口径** | `3091 字符` / `3116 字符` / `3015 字符` / `2921 字符` / `2761 字符` | 登记册尾部证据块（⇒ v5 已订正为**字符**口径）|
| 🆕 **v5 · Q-3 落地态** | `Q-3` 标「未落地」/「已裁未落地」 | 登记册 §6.2 / §6.3.1（⇒ v5 已订正为**已落地**）|

> ✅ **本表的 grep 已接入闸**（2026-11-22 · **W2-b** · 2026-11-23 订正自述）：新增 `scripts/test/cep_registry_stale_refs_check.sh`（已注册 ctest **#154**）⇒ 本表的旧口径串 + `MARK` 词表**移进闸文件** + `sed` 截断排除约定机器化 ⇒ 实测「**40 项全部为 0**」⇒ ⛔ **不再需要人工跑**（⛔ 判据的**域**仍以本表定义；闸只判「旧串是否回来」）

#### §6.3.3 这张表怎么跑（**排除约定** · per critic SHOULD_FIX 4）

> **问题**：本表列的「旧口径残留串」**必然命中「订正引述」行**（本表自己 + 各文件的「旧表述为假 / 已废 / 拍板时 / 初稿」引述）⇒ **字面执行的结果是「抓到自己」**，与表的目的相反。

**排除约定（三条同时满足才算命中）**：

```bash
# 判据：命中行 ⛔ 必须【不含】任何「订正标记」词
MARK='已废|为假|旧表述|拍板时|初稿|订正|已移除|已统一|不可执行|白名单'
# 例（以「甲层 35 份」为例）：
grep -n '甲层 35 份' <文件> | grep -vE "$MARK"   # ⇒ 期望输出为空
```

⛔ **本节（§6.3.2 + §6.3.3）自身必然含这些串**（它们是**判据的定义**）⇒ 跑 grep 时**必须排除本节**；⚠️ **另须排除 §0.2 历史节**（2026-11-16 的口径快照，**逐字保留** ⇒ 天然含旧计数 ⇒ 否则 v5 的闸族/测试计数两条会**假红**）：
```bash
# 只扫 §0.1（现行口径）—— 排除 §0.2 历史节 + 本节自含判据串
sed -e '/^### §0.2/,/^## §1 /d' -e '/^#### §6.3.2/,$d' <登记册> \
  | grep -n '甲层 35 份' | grep -vE "$MARK"   # ⇒ 期望空
```
> ⚠️ **两道排除缺一不可**：只排 §6.3.2 ⇒ §0.2 的旧计数串命中（假红）；只排 §0.2 ⇒ 本节自含判据串命中（抓到自己）。⇒ 跑法固定为上面的**两条 `-e`**（v5 实测：`**45** 个 \`cep_*.sh\`` / `**73** 个 \`test_cep_*.cc\`` / `甲层 35 份` / `A 类 4 项` 四条在两道排除后**均为 0**）。

**逐条 grep 的实际跑法**（登记册 + d1 两处都要跑，⚠️ 每条**两个文件各跑一次**）：

| 残留串 | 跑法（⛔ **登记册一律先 `sed '/^#### §6.3.2/,$d'` 截断**）| 期望 |
|---|---|---|
| `甲层 35 份` / `乙层 5 份` / `≈3~4 d` / `43 份` | `grep -n '<串>' <file> \| grep -vE "$MARK"` | **空**（登记册 + d1 各一次）|
| `A 类 4 项` / `B 类 3 项` | 同上，扫登记册 + `cep-doc-hygiene-*` + `cep-registry-stale-cleanup` | **空** |
| `**40**（50 CEP 归档` | 同上，扫登记册 §0.1 | **空** |
| `待 user 决策` | ⚠️ **只扫 front-matter 状态行**（`grep -n '^状态:'`），⛔ 不扫全文（各文件「审查轨迹」段合法引述旧状态）| **空** |
| `I-0~I-9` | 扫登记册 §6.4（⛔ 排除本节）| **空**（应含 I-10 / I-11）|
| §3 表体 `Q-[1-7]` | `sed -n '/^## §3 /,/^## §4 /p' <登记册> \| grep -cE '^\| \*\*Q-[1-7]\*\*'` | **0**（应已改名 `H-Q*`）|
| 🆕 v5 的 5 条计数残留串 | 同上，扫登记册（`sed '/^#### §6.3.2/,$d'` 截断）| **空**（§0.1 已刷新为 47+13 / 77 / 170+54 / 138+47）|
| 🆕 v5 的 5 条字节口径串 | 同上，扫登记册 | **空**（尾部证据块已订正为**字符**口径）|

⚠️ **`MARK` 词表本身是一份约定** ⇒ 若将来新增订正引述用了不在词表里的说法 ⇒ 该条会误报 ⇒ ⛔ **新增订正引述时必须同步扩 `MARK`**（⛔ 不得靠「人眼判断这行是引述」）。


### §6.4 后续立项计划（**v5** · 4 个工作流 + 1 个 user 授权请求；用「前置命中判据」排序，⛔ 不用天数）

> **硬前置**：触碰**冻结面成员**（`scripts/test/cep_freeze_check.sh` 的 `FROZEN_GLOBS`，spec 侧为 **glob 全量**）⇒ 改 hash ⇒ 必须走 `--update --authorized-by <台账 Entry>` 五步链，且 `docs/guide/cep-usage.md §12.6` 要求**台账与清单不得合并提交**。
> **台账 = `--authorized-by` 的授权目标，不是被哈希冻结的成员** ⇒ 改台账 living 段**不触**五步链；改冻结成员才触。
> ⛔ `include/tlm/cep/protocol_instances.json` 与本册**均不在冻结集** ⇒ **本册的任何修订都不触五步链**（W4 亦然）。
> **并发**：`docs/guide/cep-usage.md:80`「Proposed **并发 ≤ 1**」+ 本仓单 worktree + `.rddf/state/role-claims.json` 每 role 唯一 active claim ⇒ **4 个工作流实质串行**，⛔ 不得声称可并行。
> **排序纪律（critic SF-1）**：每条排序边 = **一条可机判的前置命中命令**；「是否触冻结面 / 是否占 KPI slot」**降为约束注解**（决定五步链 / openspec change 形态），⛔ **不作排序依据**。

#### 队列 0 · PROC（**✅ 已收口** · P-3′ 由 **W3-c** 落地 · P-5 由 **W3-a** 落地 · P-1 残余由 **W2-a / W4-a** 落地）

| 序 | 立项 | 缺口 | 完成判据（可机判） | v5 状态 |
|:--:|---|---|---|---|
| **P-1** | 登记册基线刷新 + §1.5 整节作废标注 + **追加式台账订正 Entry** 回填 S2-6b + **闸头引用转指** | S2-6b/d · S3-9 | 登记册 §0.1 三数 == 实测值；§1.5 已标作废；订正 Entry 存在 + grep 命中；闸头引用已改指 | ✅ **已收口**（2026-11-22 · **W2-a** `40fe6b69` + 2026-11-23 · **W4-a** 台账 Entry #529）：§1.5 ✅ · **§0.1 已随 v5 刷新**（见 §0.1，且已由基线闸逐项锁住）· 追加式订正 Entry（#529）+ 闸头转指 ✅ ⇒ **残余全部落地** |
| **P-3** | ~~台账 B2 行措辞订正~~ —— **已撤销**（Entry #232 授权的改判，⛔ 不得 planner 原地改 ⇒ 并入 Q-2） | — | — | ✅ 已撤销 |
| **P-3′** | `obj §4.0-a` 指引强化（**追加式**）—— ⛔ 不动 8 行摘要表与 Entry #334 段原文 | S2-5 | 订正 Entry 存在 + grep 命中；**8 行摘要表 sha256 逐字未变** | ✅ **已落地**（2026-11-22 · **W3-c** · 台账 **Entry #523** · 4 锚点命中 · 8 行摘要表 sha256 逐字未变）|
| **P-4** | ~~freeze 注释清理~~ —— **已撤销**，内容并入 **I-1**（消除 S2-6c 重复认领） | — | — | ✅ 已撤销 |
| **P-5** | **冻结面「引擎口径」指针**（**【裁决吸收 · L-3】**）：`docs/architecture/07-coherence-exploration-platform.md:138` + `docs/architecture/cep/protocol-engine.md:44` + `docs/adr/ADR-CEP-20`（`:88`/`:126`）三处加「引擎口径见 `cep-usage.md §12.2.1`」指针（⛔ **ADR 只追加 SU，原文不动**）| S2-2 | **一次**五步链（三文件**同批**，⛔ 不分两次）⇒ 授权 Entry 先落 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单**单独 commit** → 无参复验 rc=0 | ✅ **已落地**（2026-11-22 · **W3-a** · 台账 **Entry #520** 授权 + 一次五步链 · 清单差集恰 **3** 处 `~ hash 变更` · 验收报告 `.rddf/reviews/cep-freeze-lexicon-pointers-verify.md`）|

#### 队列 1 · 决策（`needs` = architect / user · **已全部裁毕**）

| 序 | 立项 | 缺口 | 完成判据（可机判） |
|:--:|---|---|---|
| **Q-1** | **✅ 已裁（architect §2.1 立定义：三词边界 + B3b 分母）** — ⛔ 勿重复拍板 | S2-2 · S2-3 | 活文档集 = `docs/architecture/**`+`docs/guide/**`+`docs/adr/**`+`include/tlm/cep/**`+`docs/implementation/**`（**排除** `docs/validation/**`+`docs/archive/**`+`docs/research/**`）⇒ `grep -rniE "5 ?个引擎\|6 ?个引擎\|五个\|六个"` 命中数 **==** 裁决随附豁免清单条目数 · ✅ 词义本体已落 `cep-usage.md §12.2.1`；**残余 = 冻结面三处无指针 ⇒ W3-a** |
| **Q-2** | **✅ 已裁（architect §2.2 裁决：不改 Entry #232 的 B2 行措辞）** — ⛔ 勿重复拍板 · **DoD 7 / B2 口径修复面判定（独家处理 S3-10）**| S3-10 | 裁决**逐条回答三问**；并给「是否动 `07 号` §4」明确二选一 |
| **Q-3** | **✅ 已裁且已落地（architect §2.3 落 `docs/guide/cep-usage.md` §12.7）** — ⛔ 勿重复拍板 | S2-4 | ✅ `cep-usage.md:169`（§12.7）+ `:190`（拦截点）在位 · **v5 订正**：v4 误记为未落地 |
| **Q-4** | **✅ 已裁（architect §2.4 逐条给判据形态）** — ⛔ 勿重复拍板 · 3 条可建模 invariant 的 architect 判据定义 | S1-1 批 1 | 每条给「可断言对象 + 载体形态 + 判据写法」；⛔ 不得 builder 自行发明语义 |
| **Q-5** | **✅ 已裁（architect §2.5 裁决：仓内 golden 为 canonical）** — ⛔ 勿重复拍板 · 13 键 golden canonical 来源裁定 | S2-7 | ✅ `test/cep_instance_golden.json` 在位 **且已有闸** `cep_instance_golden_check.py`（`test/CMakeLists.txt:755`） |
| **Q-6** | **✅ 已裁（architect §2.6 四项一次定）** — ⛔ 勿重复拍板 | S3-1 · S3-11 | ✅ 落地于 I-0；**残余** = P-5 指针（W3-a）+ 队列 0 收口（W2 / W3） |
| **Q-7** | **✅ 已裁（architect §2.7 裁决：台账 §11 为源）** — ⛔ 勿重复拍板 · 清单 5 处 canonical 化 | S3-2 | ✅ **已落地**（2026-11-22 · **W3-b** · 台账 **Entry #523** / **#524**）—— 以「**撤回执行路线 + 残余转 G-15**」形式收口（architect 追加裁决 §11 · commit `302465d8`）⇒ ⛔ 撤回「5 处 canonical 化 + 机械生成 + 双向对账闸」（三条前提实测不成立）⇒ 其可执行残余转 **G-15**（✅ 已闭合）；⇒ **不再落兜底行**（§6.3 已显式列 `G-12` 行）|
| **Q-8** | **✅ 已裁（用户 2026-11-20）** — ⛔ 勿重复拍板 · D-1 验收债分两层处置 | S3-5 | ✅ **已落地**（I-10 · 2026-11-21）|
| **Q-9** | **✅ 已裁（用户 2026-11-20）** — ⛔ 勿重复拍板 · **废除 OWNER 维度** + 8 项逐项终态 | S3-8 | ✅ **B 类 5 项 / C 类 1 项已落地**（I-11 / I-12）；**A 类 2 项亦已闭合**（2026-11-23 · **W4-f** · R-19 的 `AGENTS.md:423` 同行替换 + R-17 完成定义形态订正为 `git-history-proposal`）⇒ **8 项全部有终态** |
| **Q-10** | **✅ 已裁（用户 2026-11-20 · 方案甲）** — ⛔ 勿重复拍板 | S2-8 | ✅ **已落地**（I-9）|
| **Q-11** | **✅ 已裁（architect §2.11 裁决：登记为季度义务）** — ⛔ 勿重复拍板 · E2 季度复审义务的责任方 / 调度落点登记 | S4-1 | ⚠️ **裁决在、登记形态不对**（台账散件已在，未按「责任方 + 触发条件 + 落点」合一形态）⇒ **W3-d** |
| **Q-12** | **✅ 已裁（architect 2026-11-20）** — ⛔ 勿重复拍板 | S3-8 · C 类 1 项 | ✅ **已落地**（I-12 · 2026-11-21）|
| **Q-13** | **✅ 已裁（用户 2026-11-20）** — ⛔ 勿重复拍板 | S3-5 | ✅ **已落地**（I-10 · 2026-11-21）|

#### 队列 2 · 实施（**✅ 已收口** · 2026-11-21 · `rddf_ac_carrier.tsv` 的 **I-\*** 行全 `done` —— W1-b 补第二载体后为 **58 行**：44 原始 + 14 第二载体）

> ⚠️ **本段各行逐字保留**（⛔ 含 14 个被 `scripts/test/rddf_ac_carrier.tsv` 用作 `anchor` 的子串 —— 改动它们 ⇒ 闸 P-3 立即红 ⇒ 换锚点须同 commit 说明理由，per TSV 头 `:24`）。

| 序 | 立项 | 缺口 | 前置（一条命令） | 完成判据（可机判） |
|:--:|---|---|---|---|
| **I-0** | **顺序机关落地 + P-5 判据修正** | 顺序保证 · S3-11 | **Q-6 已裁** | ✅ **已验收**（2026-11-20 · verifier 轮 1 · PASS_WITH_NOTES）· ⚠️ **残余面 = G-1**（P-3 双面无牙）⇒ W1 |
| **I-1** | 口径 stale 集 a/c 清理 | S2-6a/c | 无 | ✅ **已验收**（a/c 旧串 `grep -c` == 0）|
| **I-9** | S2-8 CI 假绿修复 | S2-8 | **Q-10 已裁** | ✅ **已落地**（`[SKIP]` + `SKIP_REGULAR_EXPRESSION`）|
| **I-5** | invariant 建模批 1 | S1-1 批 1 | **Q-4 已裁** | ✅ **已落地**（`⭭-unmodelled` 5 → 2）|
| **I-2** | L3 KPI 行堆积清理 | S3-6 | 无 | ⚠️ **已标 `done` 但实测口径须订正**（v4 用**字节**、v5 按**字符** ⇒ 4 份实为 839/148/165/90/125/139 **全部 ≤1000 ✅**；唯一仍超 = `obj:474` = 2964 **字符** ⇒ **范围错配**）⇒ **G-7 / W4-e** |
| **I-3** | L4 3 处 improvement 状态漂移回填 | S3-7 | 无 | ⚠️ **已标 `done` 但漂移面已扩大到 8 份**（v4 记 3 份）⇒ **G-5 / W4-c** |
| **I-4** | KPI 活口径同步固定化 | S3-3 | **命令型**：`bash scripts/test/rddf_stage_gate.sh` rc=0 **且** P-5 输出「全部同步」 | ✅ **已落地**（P-5 实测「锚定 **7** 处，逐处取值 == 实测」）|
| **I-6a** | **litmus change 落地**（`INV-CEP-13-IC1` 的证据来源） | S1-1 批 2 的前置 | **Q-4 已裁** | ✅ **已落地**（归档 `2026-11-21-cpptlm-cep-litmus-falsification-harness`）|
| **I-6** | invariant 建模批 2 | S1-1 批 2 | ① Q-4 裁决已落 + **user ratify R-2a~R-2f** ✅ ② litmus change 已归档 ✅ ③ Q-2 已裁 ✅ | ✅ **已落地**（归档 `2026-11-21-cpptlm-cep-invariant-modeling-batch2` · `⭭-unmodelled` 2 → 0）|
| **I-7** | 档 2 扩面到第 2 族（`tilelink_c`） | S1-2 | 已拆 I-7a / I-7b | ✅ **全部完成**（I-7a + I-7b 均于 2026-11-21 归档；档 2 = **2 族**）|
| **I-8** | `openspec` 版本 pinning | S3-4 | 无 | ✅ **已落地**（锁文件 + 闸 + CI 从锁文件读）|
| **I-10** | **归档验收债补齐 + 机关落地** | S3-5 · Q-8 · Q-13 | 本行自身即载体 | ✅ **已落地**（机关闸实测「扫面 35 · 基线 35 · git 跟踪 35」）· ⚠️ 三处偏离经用户追认（Entry #517）|
| **I-11** | **spec 契约面 stale 引用修正** | S3-8 · Q-9 | ① openspec 可行性 spike 通过 ② 立项时 active ≤ 0 | ✅ **已落地**（归档 `2026-11-21-cpptlm-spec-stale-refs-b-class`）· ⚠️ 其携带的「`PcieTlpBundle` 无布局断言」缺口**已被证伪 ⇒ G-4 / W4-b** | · ✅ **订正（2026-11-23 · G-4 / W4-b）**：该缺口是**伪缺口**，已订正 —— 证伪证据 = `test/test_pcie_slice_wire_format_snapshot.cc:62-71` 的 **29 条 `static_assert`**（固化 `PcieTlpBundle` 的 `sizeof(56)` 与 7 字段偏移）+ `openspec/specs/pcie-slice-prerequisites/spec.md:21` 的 Scenario **点名该文件** + `test/CMakeLists.txt:96` 已注册 ctest ⇒ 原判定只 grep 了 `include/bundles/`、**漏了 test 侧载体** ⇒ **「伪缺口 / 已订正」** |
| **I-12** | **doorbell 契约面失真修正**（dGPU 域）| S3-8 · C 类 1 项（Q-12） | ① 裁决已落 ② openspec 可行性已验证 ③ 立项时 active ≤ 0 | ✅ **已落地**（归档 `2026-11-21-cpptlm-doorbell-contract-alignment`）|

#### 🆕 工作流 W1~W4（**v5 本轮立项** · 队列 2 收口后的下一轮）

> ⛔ **不铸「队列 3」**：队列 0（P-1/P-3/P-5）**从未进过载体表**（`QUEUE2` 只有 `I-*`，TSV 无 `P-*` 行）⇒ 它至今仍是无机关的散文队列 ⇒ 在其未收口时新开一节会**复刻 §6.5 要防的登记面漂移**（小型版）⇒ v5 改为「**本节承接 + 4 个工作流**」，且 **W1~W4 全部以可机判判据收口**。

##### W1 · 机关完整性（承载 **G-1**）· ✅ **已完成（2026-11-22 · 提交 `0c2a7ea1`）**

> **验收报告**：`.rddf/reviews/cep-w1-stage-gate-integrity-verify.md`（5 个子项逐条判定 ✅ · 已用轮次 1/3）· 载体表 `W-1a`~`W-1e` 各 2 行（实施 + 验收证据）全 `done`。

| 子项 | 内容 | 判据（可机判） |
|---|---|---|
| **W1-a** ✅ | 三态语义落地：`unavailable` ⇒ skip + **计入「不可分发载体 N 个」告警计数（⛔ 永不设阈值判红 —— 该计数由文件在盘与否决定，是环境相关量，设阈会把环境差异重新引入分裂）**；**收尾环须豁免 `unavailable-only` 的 AC**（⛔ 否则三态只改一半 ⇒ 干净 clone 上全红而本机绿）。<br>⚠️ **原写的 `:362-368` 行号已随改动位移 ⇒ 现行落点**：`:365-374`（unavailable 分支）· `:386-400`（收尾环豁免）· `:402-410`（可见计数） | 本机实测 `✓ P-3 不可分发载体 0 条`；影子（不拷 `openspec/`）实测 **7 条不可分发 + 合成探针 `Z-CLEAN` 走收尾环豁免 + 收尾环零 fail** |
| **W1-b** ✅ | 自指判据：`target` = 本册的行须另有**第二条非登记册载体**（台账 Entry 唯一措辞 / 验收报告 / 归档 change 路径）⇒ ⛔ **先补 14 行的第二载体、同一 commit 再启用判红**（顺序颠倒即红）| 闸实测 `✓ P-3b 自指载体 14 个 AC，均有第二条非登记册载体`；变异⑤（删 I-1 的非登记册载体行）⇒ `P-3b [I-1]` 必红 |
| **W1-c** ✅ **零动作** | ~~force-add 3 个框架治理 change 归档目录~~（**旧表述 · 已订正**）⇒ ⚠️ **前提实测不成立**：三目录**已在 git index**（`2026-11-18-cpptlm-module-param-effect-landing` 4 文件 · `2026-11-18-cpptlm-config-type-registration-align` 4 文件 · `2026-11-19-cpptlm-module-param-migrate-batch2` 6 文件 = **14**）⇒ **无可 force-add 的对象，零动作**。⛔ **「不晚于 W1-b 判红启用」的时序约束随之失效**（已满足） | `git ls-files 'openspec/changes/archive/2026-11-18-cpptlm-module-param-effect-landing' …` 三段命中 |
| **W1-d** ✅ | 3 条决定性变异入 `--self-test`（该段**刻意不注册 ctest**，理由见 `scripts/test/rddf_stage_gate.sh` 的 `--self-test` 段头注）：clean-clone 态模拟 / 自指无第二载体必红 / 合法行不误红 | `bash scripts/test/rddf_stage_gate.sh --self-test` **rc=0 · 6 项变异全绿** + 真实仓字节指纹前后相等（`f7c7c7c37d9e95cd4c90f27f425f3c8a`） |
| **W1-e** ✅ | 队列 2 收口标注（§6.4 已标 ✅）+ 「无下一目标」时改为**报收口计数**而非空转 | `--phase queue2` 实测输出「队列 2 **收口**：14 个 I-* 的载体行全部标 done（**58/58 行**）」 |

> ⚠️ **W1 执行期顺带修掉的两处既有失效**（**非 W1 引入** · 但阻塞 `--self-test` 全绿 ⇒ 一并修复并登记，避免下轮重复诊断）：
> | # | 现象 | 根因 | 处置 |
> |---|---|---|---|
> | **F-1** | 变异②（队列 2 跳序）**不判红** + 变异③（合法顺序）**空洞通过** | 队列 2 于 2026-11-21 全部 `done` ⇒「当前下一项」为空 ⇒ 变异②的构造**空转**（什么都没改）；变异③把已 `done` 的项再标 `done` = 无变化 ⇒ 「零误红」是平凡通过。⚠️ **与 W1 无关**（W1 之前的载体表上同样 `下一项 = None`） | **改变异构造、不动判据**：变异②反向构造（把队列首项标回 `pending` ⇒ 再标后继 `done`）；变异③改为「全部标 `pending` + 只把首项标 `done`」 |
> | **F-2** | `P-3b` **静默不执行**（变异⑤ 不判红） | 键展开写成 `${!arr[@]+"${!arr[@]}"}` —— 那是**间接展开**（按元素值展开同名变量）⇒ **恒为空** ⇒ 循环体一次都没进 | 改为 `"${!arr[@]}"`；已在代码注释登记该写法禁忌（⛔ 不得再写成后者） |

**W1 前置命中判据**：~~`grep -c 'unavailable' scripts/test/rddf_stage_gate.sh` **≥ 1**（现状 `:350`）~~ ⇒ ✅ 已满足（该串仍在闸内，行号已位移）。

##### W2 · 机械化（承载 **G-2 / G-10 / G-14** + 判据补强）

| 子项 | 内容 | 判据（可机判） |
|---|---|---|
| **W2-a** | **活基线比对闸**（新脚本 `scripts/test/cep_registry_baseline_check.py`，照 `scripts/test/cep_prereg_drift_check.sh` 的「真源 ↔ 机器投影双锁」先例）⇒ 把 §0.1 每行变成「取值 == 该命令输出」⇒ **消灭缺口类别而非缺口实例**（否则每次归档都重演 G-2）| 手改 §0.1 一个数字 ⇒ 闸判红 · ✅ **已落地**（2026-11-22 · `40fe6b69`）—— 基线闸已注册 ctest **#153** · 实测「5 项文档取值 == 实测」 |
| **W2-b** | **旧口径残留扫描闸**（新脚本 `scripts/test/cep_registry_stale_refs_check.sh`）⇒ §6.3.2 的 9 条旧串 + `MARK` 词表**移进闸文件** + `sed` 截断排除约定机器化。<br>⚠️ **不进 `rddf_stage_gate.sh`**（该闸已有自调用无限递归前科 `:277-285`，且已缠 P-2a / P-3 / P-5 三类职责）；⛔ **不进** `cep-stage-gate-p5-hardening` / `cep-archforge-ci-failclosed` / `cep-doc-hygiene-kpi-and-impl-state` 三个单元的执行范围（三者均有「⛔ 不新建闸脚本类型」纪律）⇒ 按 §6.6 P-9 样式**独立登记** | 把任一订正文本改回旧串 ⇒ 闸必红 · ✅ **已落地**（2026-11-22 · `40fe6b69`）—— 残留扫描闸已注册 ctest **#154** · 实测「40 项全部为 0」 |
| **W2-c** | §6.6 P-9 订正（`test/cep_instance_golden.json` **已有闸** `cep_instance_golden_check.py` + `test/CMakeLists.txt:755` ⇒ 「当前无闸载体」**已过时**）| §6.6 该条带「已收敛」标记 · ✅ **已落地**（2026-11-22 · **W2-c**）—— §6.6 P-9 该条已带「已收敛」标记 |
| **W2-d** | §6.6 增 **P-10（规划必要性）** + **P-11（量纲口径）** | 对每个工作流逐条施加 P-10 ⇒ 预期不通过者不得进队列 · ✅ **已落地**（2026-11-22 · **W2-d**）—— §6.6 已增 P-10 / P-11 / P-12 |
| **W2-e** 🆕 | **旧阶段模式退役（承载 G-14）** —— `--phase A\|B\|C` 在三个框架治理 change 全部归档后**永久红**（「下一阶段已有 44 个 task 被勾选」）。二选一：① **退役**该模式（参数只留 `queue2`，传旧值报错并给出替代）；② 保留但**改为显式报「该模式已退役：三 change 均归档」并 rc=0**。<br>⚠️ **⛔ 不得**保留「已归档状态下仍判红」—— 那是机关自身报假红（噪声源），且 `--phase A\|B\|C` 是闸头文档化用法。<br>⚠️ **不触 CI**（ctest 只注册无参调用 `test/CMakeLists.txt:286-287` ⇒ 无参 rc=0），所以这是**可用性缺陷**不是 CI 缺陷 | `bash scripts/test/rddf_stage_gate.sh --phase A`、`--phase B`、`--phase C` 三条命令**均 rc=0**（或均报「已退役」且 rc=0）；且闸头用法说明与实际行为一致；⛔ 不得靠「删除相关性断言」达成（P-2a 的队列 2 分支须仍全绿） · ✅ **已落地**（2026-11-22 · **W2-e** · `40fe6b69` · 承载 **G-14**）—— `--phase A\|B\|C` 三条实测均 rc=0 |

**W2 前置命中判据**：`grep -c '12.2.1' docs/guide/cep-usage.md` **≥ 1**（词义本体已定义 ⇒ W2 只做指针与机械化）⇒ ✅ **已满足且本工作流已收口**（验收报告 `.rddf/reviews/cep-w2-mechanization-verify.md`）。

##### W3 · 已裁 → 已落地（承载 **G-8 / G-9 / G-12**）· ✅ **已收口**（2026-11-22 · W3-a / W3-b / W3-c / W3-d 全部落地）

| 子项 | 内容 | 判据（可机判） |
|---|---|---|
| **W3-a** | **P-5 冻结面三处「引擎口径」指针**（`07:138` + `protocol-engine.md:44` + `ADR-CEP-20` 只加 SU）⇒ **一次**五步链同批（⛔ 不分两次）⇒ 🆕 **实施单元 = `cep-freeze-lexicon-pointers`**（2026-11-22 立项；⛔ **不归 `cep-registry-stale-cleanup`** —— 其 AC-6 禁止 `docs/architecture/` diff）| 无参 `cep_freeze_check.sh` rc=0 + 清单 diff 恰 **3** 处（`~ hash 变更`· 零新增 · 零移出）+ **台账与清单不得合并提交**；⚠️ **三处均「同行追加 / 文末追加 SU」⇒ 零行数位移** · ✅ **已落地**（2026-11-22 · 台账 **Entry #520** 授权 + 一次五步链 · 清单差集恰 **3** 处 `~ hash 变更` · 验收报告 `.rddf/reviews/cep-freeze-lexicon-pointers-verify.md`）|
| **W3-b** ✅ **已完成（2026-11-22 · architect 追加裁决 §11 · commit `302465d8`）** | **Q-7「同一事实 5 处」canonical 化** —— ⏔ **撤回执行路线**（三条前提实测不成立：生成器与四域零关系 / 五处是不同语义域且各自自洽 / 裁决内数目矛盾）⇒ 其可执行残余转 **G-15**（§11.1 D2）⇒ **W3-b 以「撤回 + 转缺口」收口**（⛔ 不是「未做」，⛔ 也不是「已做完」）| **完成形态 = 裁决落盘 + 残余已登记**：§11 追加裁决在位（59 增 / **0** 删 ⇒ §1~§10 零改动）· oracle 1 轮 `PASS-WITH-NOTES` + critic 1 轮 `APPROVE_WITH_CONDITIONS`（0 BLOCKER）· 台账 **Entry #524**（快照变更授权 + D4 订正授权）|
| **W3-c** ✅ **已完成（2026-11-22 · 台账 Entry #523）** | **P-3 `obj §4.0-a` 指引强化**（追加式）<br>✅ `docs/guide/cep-usage.md` **不在** `FROZEN_GLOBS` ⇒ **不触五步链**（⛔ 执行方勿误套五步链流程）| 订正 Entry 存在（#523）· grep 命中（**4 个锚点**：`§4.0-a 指引（强化版）` / `⛔ **不得读成『协议语义已实现』**` / `⛔ **B3b 维持 ⏭ 0/4**` / sha256 行）· **8 行摘要表 sha256 `3f1fdc90…` 前后逐字相等** |
| **W3-d** ✅ **已完成（2026-11-22 · 台账 Entry #522）** | **Q-11 三元组合一登记**（承认台账 `:16732` / `:23123` 散件已存在，只做「责任方 + 触发条件 + 落点」合一形态）| 台账新增行**同时**含三词（实测 7 处锚点命中）· 并**追加式**订正 `:16732` 的 N-4 行责任方（原写「librarian + planner」⇒ 裁决为 planner + architect）|

**W3 前置命中判据**：W3-a = `grep -c '12.2.1' docs/guide/cep-usage.md` **≥ 1**；W3-b = §6.3 已显式列行（**当前不满足 ⇒ W3-b 自身含「先补承接单元」这一步**）。

##### W4 · 一揽子文本订正（承载 **G-3 / G-4 / G-5 / G-6 / G-7 / G-11**）

| 子项 | 内容 | 判据（可机判） |
|---|---|---|
| **W4-a** | G-3 台账 `:474` **追加式**订正 Entry（⛔ 不回改历史 Entry 正文）| 旧串在「非订正引述」行上为 0 |
| **W4-b** | G-4 §6.4 I-11 行「`PcieTlpBundle` 无布局断言」**追加订正**（⛔ 不删原文）+ 载明证伪证据 | 订正行**同时**含「伪缺口 / 已订正」标记 |
| **W4-c** | G-5 **8 份**状态行回填 + §6.8 自查命令改白名单口径 | 8 份状态行含「已归档」+ §6.8 自查命令输出为空 |
| **W4-d** | G-6 §6.3 **显式列行**登记 `cep-memory-model-operational-trace` | §6.3 可 grep 到该单元名 + 归属表有行 |
| **W4-e** | G-7 S3-6 行收口（结论改为「1 处超阈 + 1 处范围错配」）+ `obj:474` 处置（**含新裁决**：扩 `KPI_LINE_MAX_FILES` **或** 显式接受）| 闸头注释与实际取值一致 + 裁决落盘 |
| **W4-f** | G-11 `cep-doc-hygiene-kpi-and-impl-state` 收口（R-17 / R-19）| R-19 旧串 `handle_cr_transaction` `grep -c` == **0** + R-17 的 2 份来源 change 归档目录可解析 |

⚠️ **W4 的 commit 序 =「刻意单 commit」**（critic SF-3）：登记册 §6.2 / §6.3 / §6.4 + TSV 锚点 + 追加式台账 Entry + 审稿记录**同一 commit** ⇒ ⛔ **拆 commit 会造「锚点指向尚不存在的新文本」的中间态红**；⛔ **不触五步链**（本册不在冻结集）。
> ✅ **W4 批 1（零授权子项）已完成 2026-11-23**：`W4-b`（I-11 行追加订正 · G-4 ✅）· `W4-c`（**7** 份单元状态行回填 + §6.8 白名单口径订正 · G-5 ✅）· `W4-d`（§6.3 归属表**独立行** · G-6 ✅）· `W4-f`（`AGENTS.md:423` handler 名订正 + R-17 完成定义形态订正为 `git-history-proposal` · G-11 ✅）⇒ **4 个缺口闭合**。
> ✅ **W4 批 2（已完成 2026-11-23 · 台账 Entry #529 · commit `44cfa300`）**：`W4-a` + `W4-e`（**同一行** `obj-cep-platform.md:474`）—— 该行属 `### 5.2 当前真实 baseline`（**living 现状节** · 自述「只承载活口径 ⇒ 归档流水应移到别处」）⇒ 处置 = **删去约 30 条重复 slot 流水**（每条都带**已存在的**台账 Entry ⇒ **零信息损失**）⇒ 该行由 **2964** 字符降到约 90 ⇒ **超阈自解** ⇒ ⛔ **不需要**「扩 `KPI_LINE_MAX_FILES` / 显式接受」的二选一裁决；⚠️ 但「删现状行里的历史流水」仍需**用户点头**（待授权项 1）。
**W4 前置命中判据**：`grep -c 'cep-memory-model-operational-trace' .rddf/roadmap/objectives/obj-cep-platform-remaining.md` **≥ 1**（本次修订已满足 ⇒ W4 的实体工作可开工）。

#### 🔑 授权请求（**user** · G-13）— ✅ **2026-11-23 已授权并落地**（用户「继续推进」/「继续」· 台账 Entry #530）

| 项 | 内容 |
|---|---|
| 对象 | `openspec/specs/{link-layer-and-fc,130b-encoding,axi-stream-adapter,axi4-mapper,host-bypass-and-rc}/spec.md` 的 `**状态**` 行 |
| 形态 | **一条**授权 Entry **枚举 5 个文件路径** + 机判前后判据（`grep -c "Phase [1-7] 实施中"` **5 → 0**）+ **一次**五步链同批（⛔ 台账与清单不得合并提交） |
| ⛔ 不走 architect | 无设计取舍可裁 —— 这是**事实同步**（`AGENTS.md` §PHASE STATE 早已写「Phase 1-8 全部交付」，trailer 与它矛盾）⇒ architect 裁决只能写成「事实如此」，浪费一轮 |
| ⛔ 不走 openspec change | F-4 已实证**合并器丢弃中文 trailer**（而要修的**恰恰是**中文状态行）⇒ 工具形态与目标互相吞噬 |
| ⛔ 不逐份再授权 | 成本 ×5 + 授权间隙漂移（先例：R-13a 于 2026-11-21 已由用户一条授权直改） |
| ⚠️ 拍板前 | 计划文本**保持「授权请求」形态**，⛔ **不得写成已决** |


#### 编号空间登记表（**同一前缀不得跨语义域** · per metis B8 · v5 增补）

> **为什么要有这张表**：本册把「编号空间被复用 ⇒ 残余不可寻址」登记为缺口（v4 的 S3-1 / v5 的编号纪律）⇒ **登记册自身不得重演它**（v4 初稿即犯：§3 的 `Q-1~Q-7` 与 §6.4 队列 1 的 `Q-1~Q-13` 同前缀异义；§1.5.6 的 `P-1~P-6` 与 §6.6 的 `P-1~P-9` 亦然）。
> ⚠️ **v5 的处置原则**：**冻结旧空间，不重编号** —— 旧 ID 被 improvement 单元与台账 Entry 引用 ⇒ 重编号即破坏可寻址性 ⇒ 新登记一律用**新前缀**。

| 前缀 | 语义域 | 位置 | 备注 |
|---|---|---|---|
| `H-Q1~H-Q7` | **历史**开放问题 | §3（2026-11-16/18 快照）| 全部已结清；⛔ **不得据此派工** |
| `Q-1~Q-13` | **待裁 / 已裁问题** | §6.4 队列 1 | 被 10 份 improvement + 裁决记录引用 ⇒ **不重编号** |
| `I-0~I-12` + **`I-6a`**（子 ID）| **已收口**的实施单元 | §6.4 队列 2 | ⚠️ 其行内 14 个子串是 `scripts/test/rddf_ac_carrier.tsv` 的 `anchor` ⇒ ⛔ **改动即红**（换锚点须同 commit 说明理由，per TSV 头 `:24`）|
| `P-1~P-11` | **本计划自身的可证伪判据** | §6.6 | ⛔ **不是**队列 0 的立项单元 |
| `R-*` | **登记册历史 ID** | §1 + 台账历史 Entry | ⛔ 引用必须带轮次 / Entry 上下文（§4 #1）|
| `S0~S4` / `S1-x` / `S2-x` / `S3-x` / `S4-x` | **v4 轮次**的严重度 + 缺口 ID | §6.1 / §6.2 的「已闭合项」表 | ⛔ **冻结为 v4 历史快照**（旧 ID 被 improvement 单元与台账 Entry 引用 ⇒ **不重编号**）|
| **`G-1~G-18`**（v5 新增 · **已至 G-18**）| **v5 实测轮次**的缺口 ID | §6.2 / §6.3 / §6.4 | ⚠️ 与 `S*-x` **同语义域、不同轮次** ⇒ 新登记一律用 `G-*`；⛔ 不得与 `S-*` 混引。⚠️ `G-1` 已闭合（2026-11-22 · W1）但 **ID 冻结**（§6.2 已闭合项 + §6.3 归属表保留其行）
| **`W-1~W-4`**（v5 新增）| **v5 的工作流**（机关 / 机械化 / 裁决落地 / 文本订正）| §6.4 | ⚠️ **不是队列编号** ⇒ ⛔ 不得写成「队列 3」 |
| **`O-1~O-4`**（v5 新增）| **观察项**（既不是缺口也不是范围声明）| §6.2 | ⛔ 不进任何工作流 |

⚠️ **遗留冲突（未在 v5 修）**：§6.4 队列 0 的 `P-1~P-5` 与 §6.6 的 `P-*` **同前缀异义** ⇒ 仍待统一改名（Q-6e 裁决要求「语义前缀」，落地归 **W2** / **W3**）⇒ ⛔ v5 **不改**（会牵动 §6.3 归属表 + §6.3.1 的 L-3 + 4 份草稿的引用 ⇒ 属 Q-6 落地范围）。

### §6.5 顺序机关（**取代**「checklist 文件」方案 · v5 更新）

§1.5.6 自诊断过同型病（「依赖图画得漂亮但**没有任何机关** ⇒ 顺序只由文风保证」）并开出解药 —— **该解药已落地**：`scripts/test/rddf_stage_gate.sh`（**已存在 + 已注册 ctest** `test/CMakeLists.txt:286-287`）实现 P-1~P-6，数据源 `scripts/test/rddf_ac_carrier.tsv`（**5 列 TSV**：AC 编号 / 载体类型 / 载体路径 / 锚点 / `state`，**62** 个 AC 行）是「AC ↔ 判据载体 ↔ 锚点」的**唯一数据源**；该闸 P-2a 现自解于「三个阶段均已归档 ⇒ 无下一阶段可提前开工」（`:158`）⇒ 处于**无目标**状态 ⇒ **可承载本计划工作流**。
⇒ **不新建第三个登记面**（守 Entry #334 ④「不新建登记面」否决理由，`obj:18622`）⇒ 机关 = **扩既有载体表 + 扩闸绑定**（**I-0**）+ 登记面收敛裁决（**Q-6**）。
⚠️ §1.5.6 自己的标准（「⛔ P-1~P-6 落盘前，规划只是『文风』」）⇒ **队列 2 开工前必须先跑 I-0**（队列 0 / 队列 1 不依赖 I-0）—— ✅ I-0 已于 2026-11-20 落地。

> ✅ **v5 · 机关的两个残余面（G-1 ⇒ W1）已于 2026-11-22 闭合**（提交 `0c2a7ea1` · 验收报告 `.rddf/reviews/cep-w1-stage-gate-integrity-verify.md`）：
> ① **clean-clone 免费通过** —— 已消除：`unavailable` 分支**不再置 `ac_hit`**，改为 skip + 可见计数 + 收尾环豁免（落点：`scripts/test/rddf_stage_gate.sh:365-374` / `:386-400` / `:402-410`）。⚠️ 该分支的**原始设计理由依然成立**（`.gitignore:64` 排除整个 `openspec/changes/archive/` ⇒ 干净 clone 上归档内容根本不存在 ⇒ **不能判 fail 也不能判 pass**）⇒ 修法**不是**「不该 skip」而是「skip 要计入可见计数 + 收尾环要豁免」；
> ② **自指同义反复** —— 已消除：载体表补 14 条第二载体行 + 新增 `P-3b` 判据（`:412-438`）⇒ 闸实测「✓ P-3b 自指载体 **14** 个 AC，均有第二条非登记册载体」。
> ⚠️ **保留的结论**：v4 的「顺序机关**空转**」始终是**过度陈述** —— P-2a 的队列 2 跳序判据**一直有牙**（`--self-test` 变异② 实测判红）⇒ 无牙的只有上述两个面。

### §6.6 本计划自身的可证伪判据（**P-1~P-12** · v5 增补 P-10~P-12）

| # | 判据 | 命令 / 载体 |
|:--:|---|---|
| P-1 | 每项缺口都有 `file:line` 或单条命令 | 逐条自查 |
| P-2 | 严重级与 `needs` 正交，不混 | 每项必带 `needs:` |
| P-3 | **S0 = 0 / S1 = 0 是结论不是遗漏** | v5 降级依据逐条可追（S1-1/S1-2 已闭合 + 3 项降为 S5，见 §6.1 / §6.2）|
| P-4 | **每个 `0` 必须带「本装置无该形态」的证据，否则视为未测** —— 逐项证据列 | 逐条列出该证据；无证据 ⇒ 判为未测 |
| P-5 | **S4 项不得出现在任何工作流的 AC 内** | 逐项对照 §6.4 的 W1~W4 |
| P-6 | **KPI 活口径同步面 = 7 处** —— 判据用**修正后**的闸 P-5（锚定活口径行 + 取值 == 实测），⛔ 不用子串存在性 | `bash scripts/test/rddf_stage_gate.sh`（实测输出「锚定 **7** 处」）+ 影子副本变异（I-0 完成判据）|
| P-7 | **前置命中判据可机判**（每条一条命令判真假，⛔ 不用天数、不用事件） | §6.4 每个工作流的「前置命中判据」列可执行 |
| P-8 | **不新建编号空间** | 全部引用登记册 ID；v5 新登记用 `G-*` / `W-*` / `O-*`（已在 §6.4 的编号空间登记表登记）|
| P-9 | **无闸载体风险自登记** —— 本轮引入的载体逐个登记（⛔ **不是 0 个**）：① `scripts/test/archive_verify_check.sh`（**已注册 ctest** · 归 **I-10** · 收敛路径 = 未来若需并入既有闸则由 `rddf_stage_gate.sh` 收编）② `test/cep_instance_golden.json`（golden 数据源 · 归 **I-5** · ✅ **v5 订正：已有闸** `scripts/test/cep_instance_golden_check.py`，已注册 `test/CMakeLists.txt:755` ⇒ 「当前无闸载体」的表述**已过时**，**收敛路径 = 已收敛**（W2-c · 2026-11-22））③ `.rddf/reviews/archive-audit-<date>.md`（甲层抽检记录 · 归 **I-10** · ⚠️ **当前无闸载体** ⇒ 收敛路径 = 抽检结论回写登记册 §1.3 终态表）④ 🆕 `scripts/test/cep_registry_baseline_check.py`（**已落地** · 归 **W2-a** · 已注册 ctest `#153` · 收敛路径 = 未来并入 `rddf_stage_gate.sh` 的 P-3 载体表）⑤ 🆕 `scripts/test/cep_registry_stale_refs_check.sh`（**已落地** · 归 **W2-b** · 已注册 ctest `#154` · 收敛路径 = 同上）| 本节 §6.5 + 各单元 `## Impact` 表 |
| 🆕 **P-10** | **规划必要性**（per metis 缺失 AC 5）—— 若某工作流的**全部**单元都能由**既有闸**判红 ⇒ 该工作流**必须并入既有闸**，⛔ 不得新立 | 对每个工作流逐条施加 ⇒ 预期不通过者不得进队列 |
| 🆕 **P-11** | **量纲与口径**（per metis 缺失 AC 4）—— ① 字符用 `wc -m`（⛔ **不用** `awk length` = **字节** —— v4 的 S3-6 结论即由此误报）② 闸计数列**全扩展名**（`.sh` **+** `.py`）③ 份数类判据必须以**闸的扫面集合**为准（⛔ 不用「N 份」—— 每次归档都会漂移） | 逐条自查 + `wc -m` 复测 |
| 🆕 **P-12** | **反向判据必须带「排除订正引述」的机检** —— 正向（新文本存在）不够；须有「旧串 0 命中 ∧ 命中行不含订正标记词」，且 `MARK` 词表**进闸文件**（⛔ 不留在文档里靠人判） | W2-b 的闸内置 `MARK` 词表 + `sed` 截断排除约定 |

### §6.7 明确不做（本轮范围外 · v5 更新）

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
11. 🆕 ❌ **不铸「队列 3」** —— 队列 0（P-1/P-3/P-5）**从未进过载体表**（`QUEUE2` 只有 `I-*`，TSV 无 `P-*` 行）⇒ 它至今仍是无机关的散文队列 ⇒ 在其未收口时新开一节会**复刻 §6.5 要防的登记面漂移** ⇒ v5 改为「§6 v5 刷新 + 4 个工作流 + 1 个授权请求」
12. 🆕 ❌ **不改 `unavailable` 分支的 skip 语义本身** —— `rddf_stage_gate.sh:76-79` 的原始设计理由成立（干净 clone 上归档内容不存在 ⇒ 不能判 fail 也不能判 pass）⇒ 只加「可见计数 + 收尾环豁免」
13. 🆕 ❌ **不重编号 v4 的 `S*-x` 缺口 ID** —— 旧 ID 被 improvement 单元与台账 Entry 引用 ⇒ 重编号即破坏可寻址性 ⇒ 新登记用 `G-*`
14. 🆕 ❌ **不把 v5 的观察项（`O-1~O-4`）升格为缺口** —— 它们要么是**口径张力未裁**（O-1 / O-3），要么**已有裁决/契约覆盖**（O-2 / O-4）

### §6.8 关联

- 审查轨迹（v5）：**metis 1 轮**（gap 分析 · 指出初稿 3 处测量错误 + 5 处过度陈述 + 5 类缺失判据）→ **oracle 1 轮**（对抗式 · **APPROVE** · 1 处事实错误 + 1 处排序纪律违反 + 1 处 CI 分裂风险 + 3 处过度陈述）→ **critic 1 轮**（**APPROVE_WITH_CONDITIONS** · `mergeable: yes` · **无 BLOCKER** · 4 条 SHOULD_FIX + 1 条 NIT）⇒ 逐条复验并吸收，详见本节顶部与 §6.6
- **v5 落点（4 个工作流 ⇒ 待立单元）**：`W1`（机关完整性 · 归 `cep-stage-gate-p5-hardening` 的后续单元）· `W2`（机械化 · 归 `cep-registry-stale-cleanup`）· `W3`（已裁未落地 · 归 `cep-registry-stale-cleanup` + `cep-stage-gate-p5-hardening`；⚠️ **其中 W3-a 改归新建单元 `cep-freeze-lexicon-pointers`** —— 走冻结面五步链，⛔ 不受 `cep-registry-stale-cleanup` 的 AC-6「零 `docs/architecture/` diff」约束）· `W4`（一揽子文本订正 · 归 `cep-registry-stale-cleanup` + `cep-doc-hygiene-kpi-and-impl-state`）⇒ ⚠️ **单元文件名待立**（⛔ 不新建第三个登记面 ⇒ 复用既有 4 份 improvement 单元扩写；⛔ **例外 = W3-a** —— architect 裁决记录 §5.2 末段明写「**另立一个单元**」⇒ 该单元是唯一新建的 improvement 草稿）
- **`.rddf/improvements/` 下全部 CEP 单元的 v5 处置（19 份 · ⛔ 单元 ↔ 登记册对账须输出为空）**：

| 单元 | v5 处置 | 归属缺口 / 工作流 |
|---|---|---|
| 🆕 `cep-freeze-lexicon-pointers` | **已立项**（2026-11-22 · planner）· **W3-a 的实施单元**（冻结面五步链 · 三处「引擎」三词口径指针 · 新增 invariant 数 = **0** · 零 OpenSpec change） | G-8 / W3-a |
| `cep-stage-gate-p5-hardening` | 已归档载体（I-0 / I-4）· **W1 已落地（✅ 2026-11-22 · `0c2a7ea1`）⇒ G-1 闭合**；剩余承接面 = **W2-e**（G-14） | G-1（✅ 闭合）/ G-14 |
| `cep-registry-stale-cleanup` | 已立项 · **扩写承接 W2 / W3 / W4** | G-2 / G-6 / G-9 / G-10 / G-12 |
| `cep-doc-hygiene-kpi-and-impl-state` | 已立项 · **扩写承接 W4**（含 R-17 / R-19 收口） | G-7 / G-11 |
| `cep-invariant-modeling-batch1` | ✅ 已落地（I-5）· 不持有活义务 | — |
| `cep-invariant-modeling-batch2` | ✅ 已落地（I-6）· 不持有活义务 | — |
| `cep-tier2-second-family-tilelink` | ⏸ **已拆分 · 不可开工**（目标由 I-7a / I-7b 承接）| — |
| `cep-tilelink-tree-manager-view-of` | ✅ 已落地（I-7a）· 不持有活义务 | — |
| `cep-tilelink-home-side-state-driver` | ✅ 已落地（I-7b）· 不持有活义务 | — |
| `cep-archforge-ci-failclosed` | ✅ 已落地（I-9）· 不持有活义务 | — |
| `cep-d1-archive-verify-backfill` | ✅ 已落地（I-10）· 不持有活义务 | — |
| `cep-spec-stale-refs-r14-r15-r20` | ✅ 已落地（I-11 · 归档名 `cpptlm-spec-stale-refs-b-class`）· ⛔ 其携带的「`PcieTlpBundle` 无布局断言」缺口**已被证伪** ⇒ **G-4 订正落点** | G-4 |
| `cep-openspec-version-pinning` | ✅ 已落地（I-8）· 不持有活义务 | — |
| `cep-memory-model-operational-trace` | ✅ **已登记·backlog**（2026-11-21 新建 · 2026-11-23 **G-18** 处置：立项形态已拍板 + R-2 已 ratify + I-6 已落地 ⇒ AC-16 开工门 = 1 ⇒ ⏭ **待 architect 裁 AC-1** → 用户 ratify；⏸ **待触发条件**「需要复合一致性公理可判时」）⇒ **真实活义务**（未立项 ⇒ **不占 KPI slot**）⇒ 由 §6.3 **显式列行** | G-6 |
| `cep-b4-evidence-device` | ✅ 已实施（状态行「已实施」= **合法变体**）· 不持有活义务 | — |
| `cep-evidence-tier-declaration` | ✅ 已归档 · 不持有活义务 | — |
| `cep-evidence-tier2-minimal-slice` | ✅ 已归档 · 不持有活义务 | — |
| `cep-responder-bitmap-expansion` | ✅ 已归档 · 不持有活义务 | — |
| `cep-tier2-verification-closure` | ✅ 已实施并验收 · 不持有活义务 | — |

- ⚠️ **`.rddf/improvements/` 下其余 CEP 单元不属于本轮清单** —— 自查命令已按 v5 口径改写：
  ```bash
  # ⛔ v4 的 `grep -L "已归档"` 会**误报合法变体**（实测 11 命中，其中 3 份是「已实施」/「已拆分」/「待 ratify」）
  grep -L -E "已归档|已实施|已落地|已拆分|已作废|已登记·backlog" .rddf/improvements/cep-*.md
  # ⇒ **2026-11-23 实测（W4-c 后）**：输出 = **1 份** = `cep-freeze-lexicon-pointers`（**真实活义务** · 2026-11-22 新建 · W3-b/G-15 的实施单元 ⇒ **未归档**）
  #    其余 8 份为**状态行漂移** ⇒ G-5 / W4-c）
  ```
  ⚠️ **口径订正（2026-11-23 · W4-c）**：① 白名单**补入「已落地」**（本仓通用完成态 token；旧白名单只认四种 ⇒ 漏了它 ⇒ 7 份单元回填「✅ 已落地」后仍会被误判）② 白名单的域 = **文件全文**（非仅 `状态:` 行）③ **7 份**单元的状态行已由「🟡 已立项 / 待立项」回填为「✅ **已落地**」（per 登记册 §6.4 队列 2 的 I-5 / I-7a / I-7b / I-8 / I-9 / I-10 / I-11）⇒ ⚠️ **v4/v5 的「8 份 / 9 份 / 10 份 / 11 命中」等旧计数一律作废**（⛔ 「份数」口径随每次回填漂移 ⇒ 现行口径 = **上方命令的实测输出**）
  ⚠️ **口径订正（2026-11-23 · G-18）**：④ 白名单**补入 backlog 态 token「已登记·backlog」**（`cep-memory-model-operational-trace` 的真实形态 = **已登记 · 待触发条件**，⛔ 不是「待拍板」）⇒ ⛔ **用带上下文的精确形式**（`已登记·backlog`）⛔ **不用裸词** `已登记` ⇒ 误用「已登记」二字**不豁免** ⇒ **鉴别力损失受控**；⇒ 现行实测输出**仍 = 1 份**（= `cep-freeze-lexicon-pointers` · 真实活义务）⇒ 计数**不变**；⚠️ **该单元改前即未被报出** —— 全文匹配（②）下其正文 **AC-16 行偶然含「已落地」** ⇒ 命中白名单 ⇒ **本次扩词对闸行为无变化**（只消除**偶然依赖**）；⚠️ 该偶然性本身 = **「全文匹配」口径的副作用** ⇒ 「状态行漂移」**可被正文偶然词掩盖** ⇒ 登记为 **G-18 的附带残余**（⛔ 本轮不改口径域 —— 那是改闸的判据面，属范围外）
- **新增载体登记**（per §6.6 P-9）：`scripts/test/cep_registry_baseline_check.py`（W2-a）· `scripts/test/cep_registry_stale_refs_check.sh`（W2-b）⇒ ⛔ **不得**归入带「⛔ 不新建闸脚本类型」纪律的三个 improvement 单元
- 关键既有载体：`scripts/test/rddf_stage_gate.sh` · `scripts/test/rddf_ac_carrier.tsv` · `scripts/test/cep_freeze_check.sh` · `test/cep_b3b2_deferrals.json` · `test/cep_invariant_deferrals.json` · `test/cep_evidence_tiers.json` · `docs/architecture/07-coherence-exploration-platform.md` · `docs/architecture/cep/*.md` · `docs/guide/cep-usage.md`

### §6.9 实施路径（**v5** · 队列 2 已收口 ⇒ 4 个工作流串行）

> 本节 = v5 的**执行路径**（⛔ 不是新登记面：单元与判据仍以 §6.4 为唯一源，本节只定**先后与门槛**）。
> **纪律**：每完成一个工作流 ⇒ 立刻把 `scripts/test/rddf_ac_carrier.tsv` 的 `state` 置 `done`；⛔ 不跳阶段。
> ⚠️ **互斥**：Proposed 并发 ≤ 1 + 单 worktree + 每 role 唯一 active claim ⇒ **4 个工作流实质串行**，⛔ 不得声称可并行。

| 阶段 | 单元 | 前置命中判据（一条命令）| 本阶段判据 |
|:--:|---|---|---|
| **①** | **W1**（机关完整性 · G-1）· ✅ **已完成 2026-11-22**（`0c2a7ea1` · 验收报告 `.rddf/reviews/cep-w1-stage-gate-integrity-verify.md`） | `grep -c 'unavailable' scripts/test/rddf_stage_gate.sh` **≥ 1**（✅ 已满足） | ✅ 三态语义 + 收尾环豁免 + 自指判据 + 3 条 `--self-test` 变异（实测 6 项全绿）+ 真实仓字节指纹前后相等；**W1-c 实测前提不成立 ⇒ 零动作**（三归档目录已在 index） |
| **②** | **W2**（机械化 · G-2 / G-10 / **G-14**）· ✅ **已完成 2026-11-22**（`40fe6b69` · 验收报告 `.rddf/reviews/cep-w2-mechanization-verify.md`）| `grep -c '12.2.1' docs/guide/cep-usage.md` **≥ 1** | ✅ 活基线比对闸（ctest **#153**）：手改 §0.1 一个数字即判红 · ✅ 残留扫描闸（ctest **#154**）：把任一订正文本改回旧串即判红 · ✅ P-9 订正 + P-10/P-11/P-12 落 §6.6 · ✅ **W2-e：`--phase A\|B\|C` 三条均 rc=0** |
| **③** | **W3**（已裁 → 已落地 · G-8 / G-9 / G-12） | 同 ② | ✅ **W3-a**（五步链 rc=0 · 清单恰 3 处 hash 变更）· ✅ **W3-c**（Entry #523 · 8 行表 sha256 逐字未变 · 4 锚点命中）· ✅ **W3-d**（Entry #522 · 三词合一 7 锚点命中）· ✅ **W3-b**（architect 追加裁决 §11：Q-7 执行路线**撤回** + 残余转 **G-15**）⇒ **W3 全部收口**；✅ **G-15**（独立批次 · 2026-11-22 落地 · 判据侧射程去形态化 + 快照 v2 补录）· ✅ **G-16**（独立批次 · 2026-11-23 已落地 · commit `efeffb84` · 台账 Entry #527 授权 + #528 回执 · `cep_docs_sync_check.sh` 增第 **[13/13]** 组「ADR 清单反向完整性」· 验收报告 `.rddf/reviews/cep-g16-adr-list-reverse-verify.md`）⇒ 两者均归 `cep-registry-stale-cleanup` |
| **④** | **W4**（一揽子文本订正 · G-3~G-7 / G-11）—— ⚠️ **刻意单 commit**（**本次按「是否需授权」分两批**，每批内部自洽：锚点与目标文本同 commit）| `grep -c 'cep-memory-model-operational-trace' .rddf/roadmap/objectives/obj-cep-platform-remaining.md` **≥ 1** | ✅ **批 1**（2026-11-23）：`W4-b` / `W4-c` / `W4-d` / `W4-f` 完成 ⇒ **G-4 / G-5 / G-6 / G-11 闭合**；✅ **批 2**（已完成 2026-11-23 · 台账 Entry #529 · commit `44cfa300`）：`W4-a` + `W4-e`（`obj-cep-platform.md:474` 瘦身 **2963 → 253 字符**）⇒ **G-3 / G-7 已闭合**；⛔ 不触五步链；W4-e 的「扩阈值 / 显式接受」二选一**已被第三条路取代**（按 §5.2 自述规则删去重复流水 ⇒ 超阈自解）|
| **⑤** | **授权请求**（G-13 · user）· ✅ **已授权并落地 2026-11-23**（授权来源 = 用户「**继续推进**」/「**继续**」两轮指令 · 台账 Entry #530 · commit `183df0a8`）| — | ✅ user 一条授权 Entry（枚举 5 文件 + 机判 `grep -c "Phase [1-7] 实施中"` **5 → 0**）⇒ **已落地**；⚠️ **判据订正**：那 5 份 `openspec/specs/<name>/spec.md` **不在** `FROZEN_GLOBS`（非 `cep-*` 前缀）⇒ **不触五步链**（登记册原写「一次五步链同批」前提不成立）|

> ⚠️ **阶段 ①~④ 的顺序是**「先修机关 ⇒ 再机械化 ⇒ 再落裁决 ⇒ 最后批量文本订正」⇒ 理由：机关不修 ⇒ 其余阶段的判据本身不可信；机械化不先做 ⇒ 文本订正会立刻被漂移打脸；裁决落地放在文本订正之前 ⇒ 避免同一批 commit 里既改判据又改被判文本。
> ⚠️ **本表不使用天数排序**（守 §6.4 的排序纪律）；成本不写在本表（⛔ v4 的「3~4 d」类估时已被 I-2 的实测证明量纲不可信，per §6.6 P-11）。



**v5 的实测依据（2026-11-22 · ⛔ 重跑前不得当既成事实）**：

```
⚠️ v4 本块用的是**字节**（`awk length`）⇒ 结论「5 条超长 KPI 行」**已作废**（§6.6 P-11）。
v5 按**字符**（`wc -m`）复测（闸阈 = 1000）：
  AGENTS.md:313                                   839 字符  ✅ ≤1000
  .rddf/roadmap.md:5                              148 字符  ✅
  .rddf/roadmap/phases/phase-10.md:209            165 字符  ✅
  .rddf/improvements/cpptlm-cep-umbrella.md:306    90 字符  ✅
  .rddf/improvements/cpptlm-cep-nvlink-sharp.md:31 125 字符  ✅
  .rddf/improvements/cpptlm-cep-nvlink-sharp.md:211 139 字符 ✅
  .rddf/roadmap/objectives/obj-cep-platform.md:474 2964 字符 ❌ 唯一仍超
    ⇒ 且该行**明确不在** I-2 的 In-Scope 范围内（scripts/test/rddf_stage_gate.sh:419-421
      的闸头注释自述 2963 字符 ⇒ 与本处 2964 差 1 = 换行符计数口径）⇒ **范围错配** ⇒ G-7 / W4-e
G-5 的实测：`grep -L "已归档" .rddf/improvements/cep-*.md` = 11 命中
  ⇒ 其中 3 份是合法变体（✅ 已实施 / ⏸ 已拆分 / 待用户 ratify）⇒ **真 stale = 8 份**
  ⇒ v4 的 §6.8 自查命令「输出应恰为上列 10 份」**当前即失败** ⇒ 已按 §6.8 的白名单口径改写
G-1 的实测（v5 立项时 · 已被 W1 修复）：44 个 I-* 行全 done；其中 14 行 target = 本册自身 ⇒ 32/62 行无独立证据
  ⇒ 且 `scripts/test/rddf_stage_gate.sh:350`（**W1 前**）的 unavailable 分支 skip 后仍 ac_hit=1
  ⇒ ✅ **W1 后复测（HEAD 0c2a7ea1）**：`✓ P-3 不可分发载体 0 条` · `✓ P-3b 自指载体 14 个 AC，均有第二条非登记册载体`
     · `--self-test` rc=0（6 项变异）· TSV 数据行 86（I-* 58 全 done + W-1* 10 + AC-* 18 pending）
I-4 的近零成本依据：`bash scripts/test/rddf_stage_gate.sh` 的 P-5 实测输出
  = `✓ P-5 0 处活口径全部同步（锚定 7 处，逐处取值 == 实测）` ⇒ 判据已实现
```

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
