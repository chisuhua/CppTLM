# CEP 平台 · 剩余任务总清单（canonical 登记册）

> **性质**: planner 的**剩余任务单一真源**（canonical registry）。台账 `obj-cep-platform.md` 的 Entry 只做**回执 + 指针**，⛔ 不在此处复述清单 ⇒ 避免台账（24k 行 append-only）继续膨胀、也避免清单在多处复述漂移。
> **实测日期**: 2026-11-16 · **实测基线**: 见 §0。
> **权威关系**: 本册 = **当前有效**的残余登记；台账 §11 的历史 Entry 里的 `R-*` 编号属**历史快照**（⛔ 见 §4 的编号空间冲突说明）。

---

## §0 · 实测基线

### §0.1 当前（2026-11-18 实测）

| 项 | 实测 |
|---|---|
| 全仓主 spec | **135** 份（`cep-*` **46** + 非 `cep-*` **89**） |
| 非 strict 口径 | **135/135** 绿 |
| 闸族 | **45** 个 `cep_*.sh` · 全 rc=0 |
| Catch2 `[cep]` | **540** 用例 / **12413** 断言 全绿 |
| Catch2 全量 | **883242** 断言 / **2372** 用例 全绿 · `pytest` **232 passed** · `ctest` **181/181** |
| 冻结面 | `artifact_count` **89** |
| OpenSpec | active **3** · 归档 **change 目录数** **159** · KPI **3/3**（**已满**） |

> ⚠️ **KPI 3/3 已满** ⇒ 在三个 active change 归档一个之前，**不得立新提案**（per `AGENTS.md` 的 Proposed ≤3 门槛）。

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

## §1.5 · 【框架治理域】三个 active change 的实施序列（2026-11-18 规划）

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
| **A2a** | B2 剩余 **7 键**：`PcieControllerIP`(3) · `StreamingMultiprocessorTLM`(2) · `CoherentXBarTLM`(2) | ⚠️ **「判定明确，态②」是过度陈述（critic / metis）** —— 这 7 键在 C1 的 `design.md` D-1.6 里**从未逐键裁决过** ⇒ 落地前须**逐键定态**;其中 `PcieControllerIP` 3 键的切法见 **§3 Q-7**（同名不同语义）;⚠️ **ADR-X.17 D-3「同一类型内不得同时存在 `load_param` 声明的键与板级直读的键」** ⇒ 切分单位应是**类型**而非逐键 |
| **A2b** | B2 **2 键**（态①，需实现 + 判别测试）：`GpuMeshNoC.dim` / `hops_latency` | `dim_`/`hops_latency_` **真被** `d2d_forward_payload()` 延迟模型用 ⇒ 观测量 = **既有** `last_transfer_latency_cycles()` / `simulated_throughput_GBps()`（D-2.1 禁造新观测面） |
| **A2c** | B2 **3 键**（⛔ **需拍板** ⇒ 见 §3 Q-4）：`CPUTLM.pattern`（态③）+ `start_addr` / `request_interval`（态①） | ⚠️ 接上 `start_addr` 会改变 **5 份配置 / 15 个实例**的**实际流量**（现全部从成员默认 `0x1000` 发 ⇒ 配置值被忽略）;⚠️ 真问题是「**配置意图与行为不符**」—— 不是「互相撞地址」（共享存储下多 CPU 同地址是合法场景）;**缺省规则（边界情况）**：某键**判不出态**时**缺省落态②**（可逆、保留现值语义、装配显式失败）;态① 需**两条证据**（成员被读的调用点 + 已存在观测量）,缺一即落态② |
| **A3** | B3 `RouterTLM.routing`（⛔ **需拍板** ⇒ 见 §3 Q-5）+ `GpuNoC::simulate_instantiate` 内部配置生成同步 + `[simmodule][infra]` 用例绿 | C1 D-3：**必须先于 C2 的 T1**（否则「已准入类型 + 未裁决键」窗口期） |
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
| **波 1** | **R-1a**（45 份 Purpose，分 3 批）· **D-1**（38 份验收报告）· **D-2**（流程，零成本） | 无（判据已就位） | ① 判据 / 生成器 / 载体范式**全部就位** ⇒ **零新设计**；② D-1 可与 R-1a **并行**（不同角色：verifier vs builder）⇒ 但 KPI 并发 ≤ 1 ⇒ OpenSpec change 仍**串行**（D-1 属 verifier 角色，不占 slot） | 3 d |
| **波 2** | **B-1a**（3 条 invariant 建模） | **architect 先出判据** | ① 这是 CEP 本体的**真功能缺口**（不是文档债）⇒ 收益最高；② 与波 1 **无依赖** ⇒ 建议**并行启动 architect 方案**（方案不占 KPI slot）；③ 判据未定义就实施 = builder 自行发明 invariant 语义 = **越权** | 1.5 ~ 3 d |
| **波 3** | **R-1b**（26 份正文裁剪） | 另立 change + architect | ⚠️ **会动 `## Requirements` 段** ⇒ 契约内容变更 ⇒ 需更严判据（不能沿用 Purpose 那套）⇒ **必须另立 change** | 1.5 d |
| **波 4** | **ARCH 批（8 项）** + **OWNER 批（8 项）** + **R-6** | architect 判权 | 全部是「**要定一个 canonical / 权威归属 / 措辞口径 / 编号空间**」的取舍 ⇒ **builder 不得自行拍板** ⇒ 与波 1~3 可并行推进（不同角色）但**不可并行实施** | 3 ~ 4 d |

**排序理由**：
- **波 0 排最前**：它是唯一的**解锁波** —— KPI 已满 ⇒ 不归档一个就开不了任何新提案；且 C2 硬依赖 C1 的产出。
- 波 1 是**同一条「规范面可信度」主线的第 3 段**（Purpose 占位 75 → 45 → 0）⇒ 固定成本只付一次。
- 波 2 是**唯一真功能波** ⇒ 价值最高，但**判据必须 architect 先给** ⇒ 建议**今天就启动 architect 侧**，波 1 期间并行。
- 波 3 **必须另立 change** —— 把「动 Requirements 段」与「只动 Purpose 段」混在一批 = 把两档风险并成一档 ⇒ 判据强度会被稀释。
- 波 4 单独隔离：全部是「定 canonical」类取舍 ⇒ 与波 1~3 无依赖但**不可并行实施**。

---

## §3 · 需要用户 / architect 决策的**开放问题**（**7** 项）

| # | 问题 | 选项 | 建议 |
|:--:|---|---|---|
| **Q-1** | **R-1b（26 份正文裁剪）是否值得做？** —— 裁剪**会动 `## Requirements` 段**（契约内容）⇒ 需重新定义判据 + 可能丢语义 | ① 做（另立 change + architect 参与）② 不做（`--strict` 保持 26 份红，**非 strict 口径仍全绿** ⇒ 不阻塞使用）③ 只做「拆 Requirement 而非删字」子集 | **②/③** —— `--strict` 红的这 26 份**不阻塞**任何非严格用法；收益主要是「CI 严格模式绿」⇒ 先量 CI 是否真的用 `--strict` |
| **Q-2** | **D-1（38 份验收债）补到哪一层？** —— 38 份是 2026-11 起的**全部** CEP change | ① 全补（38 份 × ~0.3 d ≈ 11 d）② 只补**有契约/判据变更**的（估 10 ~ 12 份）③ 立「验收债豁免规则」（老 change 免补，仅新 change 强制） | **③ + ②** —— 验收流程**当时未强制** ⇒ 追溯 38 份是惩罚性成本；建议**规则化**（新 change 强制 verifier 报告）**并**只补高价值子集 |
| **Q-3** | **E-1（编号空间冲突）怎么收敛？** | ① 单源登记册（本册）+ 旧编号只作历史快照（本册已采用）② 重编号（`R-1`~`R-20` → 语义前缀，如 `SPEC-01` / `ADR-01` / `PROC-01`）③ 前缀分区 | **① + ②** —— 本册已做到「单一真源」；但仍需给旧编号加**弃用标记**（本册 §1 的编号与历史编号冲突 ⇒ 建议新登记一律用 `S-01` 起的**新前缀空间**，旧 `R-*` 冻结为历史） |
| **Q-4** | **`CPUTLM` 三键怎么处置？** —— `pattern`（**`CPUTLM` 全仓 14 处全是 `SEQUENTIAL`** ⇒ 合法值唯一 ⇒ 态③ 伪参数;⚠️ `TrafficGenTLM` 的 `pattern` 有 4 个值 —— **两者不同类，别混**）· `start_addr` / `request_interval`（成员**真被**运行期读 ⇒ 态① 可实现） | ① 全态①（接上 `pattern` 需实现多模式流量生成 ⇒ **改全局流量行为**）② `pattern` 态③ + 其余态①（**推荐**）③ 全态②（拒绝三个键，配置侧全清） | **②** —— `pattern` 合法值唯一 ⇒ 伪参数（态③）;⚠️ 态③ 落地形态 = **给 CPUTLM 新增读取面**（现无 `on_config_loaded`）+ 读入校验 ∈ {`SEQUENTIAL`} + 非法值失败 —— ⛔ **不顺手做模式实现**（那是新功能）。`start_addr` 接上 = 消除「**配置意图与行为不符**」（实测 **5 份配置 / 15 个实例**：4 份 example_emitter 家族 14 处 + `tlm_e2e_test.json` 1 处 = 默认值 `0x1000` ⇒ 零变化）⇒ **不是「互相撞地址」**（共享存储下多 CPU 同地址是合法场景）;⚠️ 断言影响面需**以「统计分布对比」复核**，不可预设变红 |
| **Q-5** | **`RouterTLM.routing` 走态② 还是态③？** —— 该键合法值唯一（设计如此） | ① 态③（读入 + 校验，非法值 ⇒ 装配失败并报「键名 + 合法值集合」）② 态②（生成方停产出 + 拒绝） | **① 态③** —— 合法值唯一 ⇒ 该键的价值是「挡住拼错」而非「选行为」⇒ 正是 D-2 态③ 的定义;⚠️ **必须先于 C2 的 T1**（C1 D-3）;⚠️ 实测 `routing` 在 `configs/` **零命中**（仅由 `GpuNoC::simulate_instantiate` 运行时生成）⇒ 态③ 在真实路径**永不触发** ⇒ 态③ 与态② 实际等价安全 |
| **Q-6** | **阶段 B（R-5）与阶段 C（C2）谁先？** | ① R-5 先（缺陷更严重：装配成功但模块被静默丢弃 ⇒ **结果静默失真**）② C2 先（工作量 57 task，早启动早铺开） | **①** —— R-5 的缺陷是**结果层面的静默错误**（最难被发现的一类）;且 R-5 先做完能给 C2 提供「类型面已校验」的底座。⚠️ **「并行」选项已删** —— 本仓单 worktree + `role-claims.json` 每 role 唯一 active claim ⇒ 实际不可并行（且 `cep-usage.md:60`「Proposed 并发 ≤ 1」）|
| **Q-7** | **`PcieControllerIP` 的 3 键怎么切？** —— `bar_bases` / `bar_sizes` / `link_speed_gts` 在本类**零消费**（facade 故意「Reads sizes dynamically from PcieEndpointIP（no static snapshot）」）;⚠️ 但**同名不同语义** —— 板级 `dgpu_board_shell.cc:76-79` 读的是 **`pcie_ep` 节点**的 `bar_sizes`（`PcieEndpointIP.bar_sizes_` **有**消费面:写 config space BAR 寄存器）⇒ 同名键一个有一消费一个零消费 | ① 三个键**全部**态②（显式拒绝 + 清 3 份配置）② `bar_sizes` 移入「表 A · 板级直读」+ 其余两键态② ③ 全部态③ | **①** —— `PcieControllerIP` 上**无**消费面 ⇒ 态②;⚠️ ADR-X.17 D-3 的「同一类型内不得混合」对 `PcieControllerIP` **技术上不触发**（板级读的是 `pcie_ep` 节点）⇒ 真风险是「**同名不同语义**」⇒ 处置时须**逐节点分清**，⛔ 不得按键名一刀切。② 会把零消费的键登记成「有消费」⇒ **造第二份真源** ⇒ 否决 |

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
