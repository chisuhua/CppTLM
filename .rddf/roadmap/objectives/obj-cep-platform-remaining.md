# CEP 平台 · 剩余任务总清单（canonical 登记册）

> **性质**: planner 的**剩余任务单一真源**（canonical registry）。台账 `obj-cep-platform.md` 的 Entry 只做**回执 + 指针**，⛔ 不在此处复述清单 ⇒ 避免台账（24k 行 append-only）继续膨胀、也避免清单在多处复述漂移。
> **实测日期**: 2026-11-16 · **实测基线**: 见 §0。
> **权威关系**: 本册 = **当前有效**的残余登记；台账 §11 的历史 Entry 里的 `R-*` 编号属**历史快照**（⛔ 见 §4 的编号空间冲突说明）。

---

## §0 · 实测基线（2026-11-16）

| 项 | 实测 |
|---|---|
| 全仓主 spec | **129** 份（`cep-*` **43** + 非 `cep-*` **86**） |
| `openspec validate --strict` 红 | **55** 份（全为非 `cep-*`）= **45** 份 Purpose 占位 + **26** 份有「正文 >500」issue |
| 非 strict 口径 | **129/129 绿** |
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
| **D-1** | **38 份** 2026-11 起归档的 CEP change 缺独立验收报告 | 38 份（2026-10 及以前另 10 份 · 已验收 9 份） | 无 | 0.5 d | `.rddf/reviews/<change>-verify.md` |

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

### §1.3 `OWNER` —— 须各模块 owner 修（**8** 项）

| # | 任务 | 实测 |
|:--:|---|---|
| **R-12** | `abi_guards.h` 悬空引用 | `include/cudart/abi_guards.h` 不存在；引用处 = `pcie-slice-prerequisites/spec.md:14` |
| **R-13** | spec 内部**旧状态行**与新 Purpose 打架 | `pcie-ip-microarch:296`「等待评审 + Phase 1 启动实施」· `pcie-ip-integration` 末行 |
| **R-14** | 引用**已迁 ArchForge** 的文档 | `pcie-ip-integration` 要求交付的 `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`（仓内仅占位 README） |
| **R-15** | 引用**已迁 ArchForge** 的文档 | `v1-architecture` 引用的 `docs/soc_arch/architecture/00-overview.md` 不存在 |
| **R-17** | **2 份来源 change 归档目录缺失** | `dgpu-soc-timing` / `driver-visible-minimal-soc` ⇒ 曾存在于 commit `0345d283` / `689752ef`（`git log --all --diff-filter=A` 实测）⇒ 归因已走 `git-history-proposal` |
| **R-18** | **doorbell 配置 vs 代码 hardcoded 不一致** | `minimal-dgpu-soc-architecture` 的 Requirement **明写未收敛** ⇒ 本轮 Purpose 已回避（不写进契约） |
| **R-19** | `AGENTS.md` 记载的 LWC handler 名 **stale** | `AGENTS.md` 写 `handle_cr_transaction` / `handle_doorbell_transaction` ⇒ 实现实为 `register_doorbell_target` / `doorbell_write` |
| **R-20** | 引用**已删除**的 ABI | `dgpu-board-adapter-info/spec.md:10,21` 引用已删除的 `cpptlm_emulator_get_adapter_info`（per ADR-SOC-20 §2.1） |

### §1.4 `PROC` / `BACKLOG` / `N/A` / `BLOCKED`（**4** 项）

| # | 任务 | 状态 | 说明 |
|:--:|---|:--:|---|
| **D-2** | **立项时同步 6 处 KPI 活口径** | `PROC` | 流程缺口（Entry #442 §⑤·3 亲历）：active 窗口期间 `cep_docs_sync_check` / `cep_gate_selftest` 报红（口径写 0/3 而实测 1/3）⇒ **下一轮 planner 动作清单须加入「立项即改 6 处口径」**（零成本，纯流程） |
| **R-6** | `openspec` **版本未 pinning** | `BACKLOG` | 闸打印版本、CI 不锁 ⇒ 版本漂移不可察（0.2 d） |
| **R-3** | ArchForge 缺席致整闸 `exit 0` | `N/A` | **本机不成立** —— `/workspace/main/ArchForge` 在位 ⇒ 闸真跑 7 项全绿；仅 CI 无 ArchForge 时成立（0.2 d，CI 侧提示） |
| **A-1** | **B3b-2 = 0/4**（CXL 2.0 / CXL 3.0 / NVLink / UALink） | `BLOCKED` | 4/4 族 `blocking_type = external`（CXL 规范受 ECA §3 + AI AUP 约束**不可读**）⇒ ⛔ **本仓单方面不可推进** ⇒ **不排期**；治理上正确处置是 ⭕ 登记（`test/cep_b3b2_deferrals.json` + 闸已机器守） |

---

## §2 · 实施路径（**4 波**）

| 波 | 内容 | 依赖 | 为什么这个顺序 | 成本 |
|:--:|---|---|---|:--:|
| **波 1** | **R-1a**（45 份 Purpose，分 3 批）· **D-1**（38 份验收报告）· **D-2**（流程，零成本） | 无（判据已就位） | ① 判据 / 生成器 / 载体范式**全部就位** ⇒ **零新设计**；② D-1 可与 R-1a **并行**（不同角色：verifier vs builder）⇒ 但 KPI 并发 ≤ 1 ⇒ OpenSpec change 仍**串行**（D-1 属 verifier 角色，不占 slot） | 3 d |
| **波 2** | **B-1a**（3 条 invariant 建模） | **architect 先出判据** | ① 这是 CEP 本体的**真功能缺口**（不是文档债）⇒ 收益最高；② 与波 1 **无依赖** ⇒ 建议**并行启动 architect 方案**（方案不占 KPI slot）；③ 判据未定义就实施 = builder 自行发明 invariant 语义 = **越权** | 1.5 ~ 3 d |
| **波 3** | **R-1b**（26 份正文裁剪） | 另立 change + architect | ⚠️ **会动 `## Requirements` 段** ⇒ 契约内容变更 ⇒ 需更严判据（不能沿用 Purpose 那套）⇒ **必须另立 change** | 1.5 d |
| **波 4** | **ARCH 批（8 项）** + **OWNER 批（8 项）** + **R-6** | architect 判权 | 全部是「**要定一个 canonical / 权威归属 / 措辞口径 / 编号空间**」的取舍 ⇒ **builder 不得自行拍板** ⇒ 与波 1~3 可并行推进（不同角色）但**不可并行实施** | 3 ~ 4 d |

**排序理由**：
- 波 1 是**同一条「规范面可信度」主线的第 3 段**（Purpose 占位 75 → 45 → 0）⇒ 固定成本只付一次。
- 波 2 是**唯一真功能波** ⇒ 价值最高，但**判据必须 architect 先给** ⇒ 建议**今天就启动 architect 侧**，波 1 期间并行。
- 波 3 **必须另立 change** —— 把「动 Requirements 段」与「只动 Purpose 段」混在一批 = 把两档风险并成一档 ⇒ 判据强度会被稀释。
- 波 4 单独隔离：全部是「定 canonical」类取舍 ⇒ 与波 1~3 无依赖但**不可并行实施**。

---

## §3 · 需要用户 / architect 决策的**开放问题**（**3** 项）

| # | 问题 | 选项 | 建议 |
|:--:|---|---|---|
| **Q-1** | **R-1b（26 份正文裁剪）是否值得做？** —— 裁剪**会动 `## Requirements` 段**（契约内容）⇒ 需重新定义判据 + 可能丢语义 | ① 做（另立 change + architect 参与）② 不做（`--strict` 保持 26 份红，**非 strict 口径仍全绿** ⇒ 不阻塞使用）③ 只做「拆 Requirement 而非删字」子集 | **②/③** —— `--strict` 红的这 26 份**不阻塞**任何非严格用法；收益主要是「CI 严格模式绿」⇒ 先量 CI 是否真的用 `--strict` |
| **Q-2** | **D-1（38 份验收债）补到哪一层？** —— 38 份是 2026-11 起的**全部** CEP change | ① 全补（38 份 × ~0.3 d ≈ 11 d）② 只补**有契约/判据变更**的（估 10 ~ 12 份）③ 立「验收债豁免规则」（老 change 免补，仅新 change 强制） | **③ + ②** —— 验收流程**当时未强制** ⇒ 追溯 38 份是惩罚性成本；建议**规则化**（新 change 强制 verifier 报告）**并**只补高价值子集 |
| **Q-3** | **E-1（编号空间冲突）怎么收敛？** | ① 单源登记册（本册）+ 旧编号只作历史快照（本册已采用）② 重编号（`R-1`~`R-20` → 语义前缀，如 `SPEC-01` / `ADR-01` / `PROC-01`）③ 前缀分区 | **① + ②** —— 本册已做到「单一真源」；但仍需给旧编号加**弃用标记**（本册 §1 的编号与历史编号冲突 ⇒ 建议新登记一律用 `S-01` 起的**新前缀空间**，旧 `R-*` 冻结为历史） |

---

## §4 · ⚠️ 口径声明（避免下轮误判）

1. **本册只收「当前有效」的登记。** 台账 §11 历史 Entry 的 `R-*` 编号属**历史快照** —— 同一编号在不同轮次含义不同（例：台账 `:5590` 的 `R-1` = 「obj §4.0 表体 15 行」vs `:24283` 的 `R-10` = 「superseded spec 存在性裁定」）⇒ **引用残余时必须带轮次/Entry 上下文**，⛔ 不得只写 `R-N`。
2. **三处数字已用权威口径订正**（2026-11-16）：
   - 「正文 >500」= **26** 份（openspec 自身判据）—— ⛔ 非我早前的近似统计 **28** 份（近似法按「整段长度」切，与工具的「描述段长度」口径不同 ⇒ **以工具为准**）。
   - ADR 归因例外「被已签批 ADR 挡住」= **12** 条（rationale 含「挡」）—— 其中 `carrier_buildable_id_blocked` 这个 **disposition** 本身是 **10** 条 ⇒ 两个数口径不同，勿混用。
   - 缺独立验收报告 = **38** 份（2026-11 起归档的 CEP change）—— ⛔ 非 Entry #438 早前估的「5 份」（那 5 份只覆盖了 11-13 之后的 5 个）。
3. **`--strict` 红数 = 55**，构成 = **45** 份 Purpose 占位 + **26** 份「正文 >500」（两者有重叠）⇒ ⛔ **不是** 71。

---

## §5 · 关联

- 台账 `obj-cep-platform.md` —— **Entry #444**（2026-11-16 全量核对 + 立项）· **#445**（批 3 收口）· **#446**（批 3 verifier 验收）· **#441**~**#443**（批 1）· **#438**（上轮全量梳理）
- 批 1：`openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/` · 批 3：`openspec/changes/archive/2026-11-16-cpptlm-spec-purpose-batch3/`
- 判据载体：`scripts/test/cep_spec_purpose_batch1_check.py` · `scripts/test/cep_spec_purpose_batch3_check.py` · `scripts/test/cep_spec_purpose_batch3_gen.py`
- 载体：`test/spec_purpose_batch1_attribution.json` · `test/spec_purpose_batch3_attribution.json` · `test/spec_purpose_batch3_reconcile.json`
- 未结义务导航：`docs/guide/cep-usage.md` §12.8（**导航，非状态源** —— ⚠️ 其「ADR 归因 18 条」已 stale，实测 **19** 条）
- 外部阻塞登记：`test/cep_b3b2_deferrals.json` · `test/cep_invariant_deferrals.json` · `test/cep_adr_structural_deferrals.json`
