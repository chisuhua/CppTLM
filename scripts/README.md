# scripts/ — CppTLM 工具脚本

按用途分 5 个子目录组织：

## 根目录便捷入口（推荐用法）

| 命令 | 用途 |
|------|------|
| `./build.sh` | 独立构建入口（薄包装转 `scripts/build/build.sh`）。`--rebuild` 干净构建；`--ptx-emu` 启用 PTX-EMU 集成；`--debug` Debug 模式；默认 `-j2` OOM-safe |
| `./test.sh` | 测试入口。**默认只跑测试**（binary 须已构建），显式 `./test.sh --build` 一次性 build+test。`--verify-fix` 跑 `verify_test_seam_fix.sh` 一键回归（Oracle Option C + classify_naive fallback 验证）|

## 子目录结构

| 目录 | 内容 | 用途 |
|------|------|------|
| `build/` | `build.sh`, `format.sh`, `build_ptx_emu.sh` | 构建与代码格式化（`build_ptx_emu.sh` 启用 PTX-EMU 集成，dGPU/APU SoC 默认路径）。根 `build.sh` 是薄包装，委托此目录 |
| `test/` | `test_off.sh`, `test_ptx_emu.sh`, `verify_test_seam_fix.sh`, `run_all_tests.sh`, `ci_e2e_test.sh`, `docs_sync_check.sh`, `markdown_link_check.sh`, `gen_wire_format_snapshot.sh`, `check_pcie_ip_facade.sh`, `cep_docs_sync_check.sh`, `cep_doc_ref_check.sh`, `cep_freeze_check.sh`, `cep_archforge_alignment_check.sh`, `cep_decl_alignment_check.sh`, `cep_decl_reverse_verify.sh`, `cep_cohif_reverse_verify.sh`, `cep_protocol_engine_reverse_verify.sh`, `cep_protocol_bridge_reverse_verify.sh`, `cep_chstream_bidirectional_macro_reverse_verify.sh`, `cep_mesif_state_spec_reverse_verify.sh`, `cep_mesif_adapter_reverse_verify.sh`, `generate_mesif_recognition_checklist.sh`, `generate_tilelink_anchor_checklist.sh` | 分模式测试与兼容入口。根 `test.sh --verify-fix` 委托 `verify_test_seam_fix.sh`；其他仍按模式分派。**`cep_*` 12 个 = CEP 守门脚本**(契约同步 / 对外引用 / M1 冻结 / 跨仓锚点 / 声明映射双向 + 队列 #7 的 4 个反向 fixture 脚本),详见 §CEP 契约守门脚本 |
| `pipeline/` | `run_full_pipeline.sh` | 完整仿真流程（生成→仿真→可视化） |
| `topology/` | `topology_validator.py`, `topology_generator.py`, `analyzer.py`, `path_tracer.py`, `credit_flow.py` | 拓扑生成/分析/验证 |
| `stats/` | `stats_annotator.py`, `stats_watcher.py`, `layout_manager.py`, `derive_expr.py` | 性能统计与可视化 |
| `cep/` | `scripts/cep/generate_doc_index.py`, `scripts/cep/parity/`（gem5 对拍探针） | CEP 工具：架构文档**文末索引生成器**（`07 号` 的索引 A/B，2026-11-12 落仓 per 台账 Entry #342）+ 对拍基准探针 |

## CEP 契约守门脚本（`scripts/test/cep_*.sh`）

> **三表划分**(2026-11-12 澄清,per 台账 **Entry #377** 的 T3)——本目录下的 `cep_*.sh` 分属**三张互不重叠的表**,**读法不同**:
>
> | 表 | 小节 | 是否注册 ctest | 判据性质 | 计数口径 |
> |---|---|:--:|---|---|
> | **守门表** | 本节 | ✅ **注册**(`ctest -R cep`) | **守门** —— 判据不绿 ⇒ CI 红 | 本表**逐行登记**;⛔ **不写死总数** |
> | **季度复审表** | 「季度复审脚本」 | ❌ **刻意不注册** | **守门性质**(cadence 断言会让无关 PR 变红) | 同上 |
> | **实验运行器表** | 「实验运行器」 | ⚠️ `cep_experiment_run.sh` 以 **`--selftest`** 模式注册(`cep_experiment_run_selftest`);`cep_h8_ab_runs.sh` **不**注册 | **非守门**(实验面) | 同上 |
>
> ⚠️ **计数口径(单一纪律)**:⛔ **本文件不写死 `cep_*.sh` 的绝对总数** —— 每次新增闸都会使其过期(实测漂移:31 → 35 → 39)。
> ⇒ **现行计数一律以 `ls scripts/test/cep_*.sh` 为准**;**守门表行数**以本表**实际行数**为准(⛔ 不用表内自述数反推磁盘数)。

| 脚本 | 作用 | 状态 |
|------|------|------|
| `cep_docs_sync_check.sh` | CEP 契约面同步检查（21 份 ADR 锚点 + invariant 计数单一真源 + P 编号口径 + 传输层 `CATEGORY_*` 分层纪律），共 **10 组** | ✅ 生效 |
| `cep_doc_ref_check.sh` | 对外文档引用可达性（节号引用目标节存在 + 语义断言 + 负向断言 + 跨仓 `path:line` 锚点） | ✅ 生效（ctest 注册 `--strict`） |
| `cep_freeze_check.sh` | M1 冻结面授权闸（成员数以清单为准）（`--update --authorized-by "Entry #N"` + 两步 commit） | ✅ 生效 |
| `cep_archforge_alignment_check.sh` | 跨仓（ArchForge）对齐锚点守门，33 锚点 | ✅ 生效 |
| `cep_decl_alignment_check.sh` | 协议实例声明 ↔ schema 双向映射（判据 C-4） | ✅ 生效 |
| `cep_decl_reverse_verify.sh` | 上者的**反向验证**（防自证） | ✅ 生效 |
| `cep_cohif_reverse_verify.sh` | CohIf 载体（CC-01 / CC-02）的**反向验证**（静态扫描 + 编译期锁毒化副本） | ✅ 生效（ctest 注册） |
| `cep_protocol_engine_reverse_verify.sh` | ProtocolEngine 端口面（`INV-CEP-07-IC2`）的反向验证 | ✅ 生效（ctest 注册） |
| `cep_protocol_bridge_reverse_verify.sh` | ProtocolBridge 特化桥（CC-03 / CC-05 / CC-06）的反向验证 | ✅ 生效（ctest 注册） |
| `cep_chstream_bidirectional_macro_reverse_verify.sh` | `REGISTER_CHSTREAM_BIDIRECTIONAL` 的**编译期锁**反向验证（N=1 必红且原因命中 `static_assert`） | ✅ 生效（ctest 注册） |
| `cep_mesif_state_spec_reverse_verify.sh` | MESIF 规约判据（队列 #9 `cpptlm-cep-mesif-state-spec` 的 MS-01 ~ MS-05）的反向验证（8 类毒化副本：依据清空 / kind↔pending 错配 / 无锚点引用 / 共享态写突变 / 本地事件秩下降 / 转换表挖洞 / 偏离不真） | ✅ 生效（ctest 注册） |
| `cep_mesif_adapter_reverse_verify.sh` | MESIF **实现层**判据（队列 #9 `cpptlm-cep-mesif-adapter` 的 MA-01 ~ MA-05 / MA-08 / HD-01 ~ HD-05）的反向验证。**每条 fixture 声明期望失败模式**（`compile-lock` 须命中指定 `static_assert` 文案 / `runtime-predicate` 须命中指定判据 id）⇒「因正确原因红」有机器判定；探针按 `[spec]` / `[impl]` / `[decl]` 三层打印失败 id | ✅ 生效（ctest 注册） |

| `cep_cxl_dcoh_state_spec_reverse_verify.sh` | 队列 #12 规约载体（`CDS-01` ~ `CDS-07`）的反向验证（4 类毒化副本） | ✅ 生效（ctest 注册） |
| `cep_tilelink_adapter_reverse_verify.sh` | 队列 #11 实现层 adapter（`TLA-01` ~ `TLA-13`）的反向验证 | ✅ 生效（ctest 注册） |
| `cep_nvlink_sharp_state_spec_reverse_verify.sh` | 队列 #13 规约载体（`NSS-01` ~ `NSS-09`）的反向验证：**9 组 / 10 条**毒化 fixture（每条必红 **且** 归因 id 命中）+ 基线正控 | ✅ 生效（ctest 注册） |
| `cep_nvlink_sharp_reverse_verify.sh` | 队列 #13 **实现层**载体（`NVA-01` ~ `NVA-09`）的反向验证：**9 组**毒化 fixture（每条必红 **且** 归因 id **精确相等**命中）+ 基线正控 | ✅ 生效（ctest 注册） |
| `cep_ualink_inc_state_spec_reverse_verify.sh` | 队列 #14 **规约层**载体（`UIS-01` ~ `UIS-11`）的反向验证：**11 组 / 12 条**毒化 fixture（每条必红 **且** 归因 id **命中** `static assertion failed: <id>: `）+ **基线正控** + **归因 id 双向覆盖自检** + **探针鉴别力自测**（双分支：编译红 or 探针红） | ✅ 生效（ctest 注册） |
| `cep_ualink_inc_reverse_verify.sh` | 队列 #14 **实现层**载体（`UIA-01` ~ `UIA-07` + **V6-d** 四条证书）的反向验证：**11 组 / 11 条**毒化 fixture（每条必红 **且** 归因 id **精确相等**命中 `PROBE:FAIL <id> `）+ **基线正控** + **归因 id 双向覆盖自检** | ✅ 生效（ctest 注册） |
| `cep_b3b2_d4_probe_check.sh` | `B3b-2` 受限族的 **D-4 触发探测清单**闸(2026-11-07,per 台账 Entry #274 §④ E1 + Entry #277):① 探测清单结构 / **覆盖谓词**(4 族 × 2 路径 = 8 槽位逐格 ≥1) / 合规边界(key 白名单 + URL pattern + `page_type` 枚举 + banned token)/ 防漂移(`deferral_trigger_sha256`) ② **5 个决定性变异**按固定签名变红(`BODY-FIELD-FORBIDDEN` / `DOC-URL-IN-METADATA-SLOT` / `COVERAGE-GAP` / `SUBJECTIVE-CRITERION` / `DEFERRAL-TRIGGER-DRIFT`)。⚠️ 只校验**协议清单**;⛔ 不保证执行者未在其他渠道接触规范正文(⇒ `incident` 登记 + 季度人工审计);⛔ **不改**任何 DoD 计数 | ✅ 生效（ctest 注册） |

| `cep_b3b2_deferral_check.sh` | `B3b-2`(受限族 0/4)的 **deferral 机器可查登记**闸:4 族齐全 + 字段齐备 + `ledger_entry` 台账可解析 + 与 **`obj §6.2` B3b-2 行的族枚举交叉校验**(2026-11-12 强化 per 台账 Entry #342:删「grep `$DECL` 自身」的恒真段,改为对**外部真源**的集合级判据)。⚠️ 只校验**登记完整**,⛔ **不改计数**(登记 ≠ 达成) | ✅ 生效（ctest 注册） |
| `cep_b4_coupling_probe.sh` | **B4**(4 件套实验输出契约)的 **coupling 探针**(13 必纳 metric ↔ 假设判据的耦合面) | ✅ 生效（ctest 注册） |
| `cep_b4_observation_keys_check.sh` | **B4** 的 **observation_keys** 契约校验(附加键 ≠ 13 必纳语义) | ✅ 生效（ctest 注册） |
| `cep_gate_selftest.sh` | **守门脚本自身**的自测(闸脚本**不冻结**,但须能修 ⇒ 自测保证「修了没坏」) | ✅ 生效（ctest 注册） |
| `cep_ic3_report_states_check.sh` | **IC3** 报告三态校验(`measurement` / `not-collected` / 判据状态) | ✅ 生效（ctest 注册） |
| `cep_prereg_drift_check.sh` | **预登记 out-of-scope 集合**的漂移闸(⛔ 不得事后自由扩张;真源 = `objective §9.1` ② 表) | ✅ 生效（ctest 注册） |
| `cep_source_effect_check.sh` | **来源类型判据**的**决定性变异**取证(4 类影子副本:`dropped` / `downgraded` / `misclassified` / 影子探针 ⇒ 按固定签名变红)+ 真源 sha256 复原校验 | ✅ 生效（ctest 注册） |
| `cep_cxl_dcoh_adapter_reverse_verify.sh` | 队列 #12 **实现层** adapter 的反向验证(6 条毒化 fixture:必红 **且** 归因 id 命中)+ 基线正控 | ✅ 生效（ctest 注册） |
| `cep_freeze_reverse_verify.sh` | `cep_freeze_check.sh` 对**派生文件**(`include/tlm/cep/fabric.json`)有牙的反向验证(基线绿 → 破坏红(点名 `include/tlm/cep/fabric.json`)→ 还原绿) | ✅ 生效（ctest 注册） |
| `cep_tilelink_engine_failclosed.sh` | TileLink-C 运行时引擎的 **fail-closed 消费面 + 决定性变异**取证(影子树机制 + 真源 sha256 复原校验) | ✅ 生效（ctest 注册） |
| `cep_index_position_check.sh` | `07 号` 文末文档索引闸(**三合一**,2026-11-12 强化 per 台账 Entry #342):① 索引永久置于文末(其后零 `## `)② 实际 `## ` 数 == 自述一级节数 == 索引 A 行数 ③ 实际 Status Update 数 == 自述 SU 数 == 索引 B 行数 ④ **索引 A/B 的「行」列逐行 == 实测行号** + 标题集合相等(防遗漏 / 防重复) | ✅ 生效（ctest 注册） |
| `cep_e2e_reconcile_check.sh` | **双向对账闸**(per 台账 Entry #295 裁定 2 = retention **丙**;Entry #303):台账条目 ↔ 仓内**已提交** run 产物 —— ① 零 orphan(双向)② `metrics_sha256` 相等 ③ 7 字段齐备 ④ verdict 三态词表(⛔ 不得二值绿/红) | ✅ 生效（ctest 注册） |
| `cep_validate_topology_reconcile_check.sh` | **`validate_topology` target 对账闸**(per 台账 Entry #358):① 组合 target 引用的每个 config **必须存在于磁盘**(此前首条引用已被物理删除的文件 ⇒ 整条 target 跑不完 ⇒ 其后 4 条校验 + CEP 装配闸 CLI 挂点**静默 20 天从未执行**)② 组合 target **禁手抄** config 清单(须从单一真源 `CPPTLM_TOPOLOGY_VALIDATION_PAIRS` 派生)③ 装配闸挂点`cep_channel_dependency_gate` **必须在命令面内**(⛔ ctest 侧同名用例独立注册 ⇒ 挂点消失时它仍绿 ⇒ 本闸是该失效的唯一可见点) | ✅ 生效（ctest 注册） |
| `cep_decision_block_check.sh` | CEP **治理判据闸**(per change `cpptlm-cep-invariant-gate-teeth` / 台账 Entry #363):把 `ADR-CEP-00` SU 2026-11-11 §⑤ 自认的两项「有登记、无牙齿」残留落为机器判据 —— ① **R-d** = 22 份 ADR 的 §2 决策块计数**逐份** == 快照(`test/cep_decision_block_snapshot.json`,22 键 + total=125;口径 = D-1 四步 + D-3.2 块 ID 全匹配;期望值 = **闸外快照**,⛔ 禁自证)② **R-c** = 双向对账(正向:三份精确 basename + **无同号拆出件**;反向:新 SU 的 `### ⑥` 重述块;期望值 = **闸内常量**)。附**决定性变异**(影子树 ⇒ 不动真实仓),每条按**固定签名**变红(⛔ 「rc≠0」不构成红因) | ✅ 生效（ctest 注册） |
| `cep_dod_reading_check.sh` | CEP **DoD 读法闸**(per change `cpptlm-cep-dod-reading-consistency` / 台账 Entry #373):① **4 条整行锚**(A1~A4)**整行全等** + 唯一承载文件 + 该文件内行次数 == 1(⛔ 子串锚 —— 实测必红)② 旧文(手册第 3 条现文)**零命中** ③ 作用域 **5 要素**齐备 + `07 号` 内**绝对禁令不复活**④ 新文本面**5 条禁表述**零命中 ⑤ 指针目标存在 + 取代指针存在;配 **10 条决定性变异**(M1~M10,**只作用于影子树**,按固定签名红 ⇒ 还原 ⇒ 绿) | ✅ 生效（ctest 注册） |
| `cep_iag_gate_reconcile_check.sh` | CEP **IAG 执行臂对账闸**(per change `cpptlm-cep-iag-gate-execution-arm` / 台账 Entry #379):① 装配路径臂(`module_factory.cc` Step 0.26)与离线 CLI 臂(`cep_instance_assembly_gate.cc`)**同时**调用 `run_instance_assembly_gate` **且**共用加载器 `load_instance_assembly_inputs`(禁双实现)② 判据本体 `include/tlm/cep/instance_assembly_gate.hh` **零 diff** ③ CLI 已注册 ④ `validate_topology` 的 **IAG 第二段**在命令面内 + CEP config 清单 == 磁盘实数 ⑤ 装配臂行号**早于**模块创建段(失败时零副作用);配 **2 条决定性变异**(M1 破坏 config ⇒ 按签名 `[IAG-02]` 红;M2 摘掉 IAG 段 ⇒ 判据红) | ✅ 生效（ctest 注册） |
### 季度复审脚本（守门性质，**刻意不注册 ctest**）

| 脚本 | 作用 |
|------|------|
| `cep_b3b2_d4_quarterly_review.sh` | `B3b-2` 的 **D-4 触发探测**季度复审(2026-11-07,per 台账 Entry #277 / #278):① run-record schema(key 白名单 + outcome 三态 + incident 长度上限 ⇒ **正文无落点**)② **cadence**(最近一次真实执行距今 ≤ 100 天)③ **覆盖**(每次执行须覆盖 8 槽位)④ 计数纪律(⛔ 「B3b-2 已达成」形态) + **3 个决定性变异**。⚠️ **刻意 NOT 注册 ctest** —— cadence 断言若进 ctest ⇒ 100 天后**所有无关 PR 都红** ⇒ 只放本脚本 |
| `cep_adr_structural_check.sh` | **ADR 结构闸**(2026-11-09,per oracle 裁决 C-1 / 台账 Entry #307 §③ T1):把「ADR 正文缺顶层 `Verification` 章」从暗变可见。4 组判定 —— ① **分区完整性**(磁盘 `ADR-CEP-*.md` == `decision_adrs` ∪ `excluded` ⇒ 新 ADR 必须显式归类)② **未登记即红**(不合规者必须落在 `waivers`(永久豁免,带 `evidence_anchor` 行号锚点,闸校验锚点存活 ⇒ 反后门)/ `pending_signing`(起草中待补章))③ **豁免锚点存活** ④ **登记表健康**(reason / trigger_condition / owner / ledger_entry 齐备)+ **3 个决定性变异**(影子树 ⇒ 不动真实仓)。登记表 = `test/cep_adr_structural_deferrals.json`(⛔ 与 `test/cep_b3b2_deferrals.json` 无关)。✅ **注册 ctest**(`cep_adr_structural_check`) |

### 实验运行器（非守门 —— `cep_experiment_run.sh` 以 **`--selftest` 模式**注册 ctest（`cep_experiment_run_selftest`）;`cep_h8_ab_runs.sh` **不**注册）

| 脚本 | 作用 |
|------|------|
| `cep_experiment_run.sh` | H2 / H3 / H6 实验的 4 件套产出：C++ 侧经 `CEP_EXP_DIR` + `CEP_EXP_ID` 产 simulation_config.json + metrics.json → 本脚本用 `h5py` 补 trace.h5（run 级摘要，不冒充 cycle trace）→ `tools/render_report.py` 产 `report.html` → `--check` 校验。产物落 `experimental/`（已被 `.gitignore` 忽略） |
| `cep_h8_ab_runs.sh` | **H8 A/B** 实验运行器(per `experimental/` 产出;⛔ **不**注册 ctest —— 属实验面非守门面) |

> 结果解读见 `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`（含口径限定与反向 fixture 证据）。

> 配套数据文件：`cep_doc_ref_aliases.tsv`（节号别名）/ `cep_doc_ref_registry.tsv`（语义 + 负向断言登记）。
> **本表所列脚本全部**已注册进 `ctest`（`ctest -R cep`）。⚠️ **2026-11-07 订正 + 缺口闭合**(per 台账 Entry #278):原写「**18 个**」—— 实测磁盘 `scripts/test/cep_*.sh` = **31** 个,原表内 **20** 个 / **11** 个未登记 ⇒ 本次**补齐 11 行** ⇒ 现**守门表 29** + **实验运行器 2** = **31** = 磁盘实际⇒ **「表内计数 ≠ 磁盘实际」的预存缺口已闭合**。⚠️ 另有 **3 个脚本此前既未注册 ctest 也未登记**(⇒ **从不被 CI 执行**,判据可能早已失效而无人知道):`cep_cxl_dcoh_adapter_reverse_verify.sh` / `cep_freeze_reverse_verify.sh` / `cep_tilelink_engine_failclosed.sh` ⇒ 实测三者均**跑绿**(12s / 1s / 4s)⇒ 属**注册债**而非判据漂移 ⇒ 本次**已注册进 ctest**。
> ⚠️ **2026-11-03 订正**（队列 #13 实现层 / Entry #187）：原写「**15 个**」—— 该数是**表内登记行数**，而 `scripts/test/cep_*.sh` 的**磁盘实际**为 **19** 个（另 3 个未列入本表）
> ⇒ **表内计数 ≠ 磁盘实际**；本表只登记**已纳入 ctest 的**脚本，差异项以 `ls scripts/test/cep_*.sh` 为准。
> ⚠️ **2026-11-12 订正 + 缺口闭合**（per 台账 Entry #342）：守门表 **29 → 31** 行（补登 `cep_index_position_check.sh` / `cep_e2e_reconcile_check.sh`）⇒ 当时的构成 = **31（守门）+ 2（季度复审，刻意不入 ctest）+ 2（实验运行器）= 35**（2026-11-12 实测）。⚠️ **该等式已过期** —— 此后 C2 / C3 / C4 各新增闸 ⇒ ⛔ **现行计数一律以 `ls scripts/test/cep_*.sh` 为准**（本文件不再写死绝对数,per 台账 Entry #377 的 T3）。⛔ 上方「本表所列脚本全部已注册进 `ctest`」的读法对**季度复审 / 实验运行器**两表**不成立** —— 那 4 个是**刻意不注册**（cadence 断言会让无关 PR 变红 / 属实验面非守门面）。
> ⚠️ **2026-11-12 补注册**（per 台账 **Entry #343** = verifier 报告 **F-1**）：`cep_decl_reverse_verify.sh` 此前**在守门表内且状态列写「✅ 生效」,却从未 `add_test`** ⇒ **CI 从不执行它** ⇒ 「登记了,但没有牙齿」的同缺陷类。**已补注册进 ctest**（实测 3 连跑 rc=0 · ~5.7s · 不写仓内文件）⇒ 上方「本表所列脚本**全部**已注册进 `ctest`」自此**逐条成立**（可机器核:`ctest --test-dir build -N` 的 CEP 项覆盖守门表每个脚本名）。
> ⚠️ **2026-11-03 补登**（队列 #13 Phase 3，Entry #184）：此前 **队列 #11 / #12 的两个反向脚本**
> 虽已注册进 ctest，却**未列入本表**（预存登记缺口）⇒ 本次一并补上。
> ⚠️ **2026-11-07 闭合**(per 台账 Entry #278):原「目录内另有 `cep_freeze_reverse_verify.sh` 与 `cep_gate_selftest.sh` 两个脚本**未列入本表**」的**预存登记缺口已消解** ⇒ 两者已登记入本表。

### 反向 fixture 登记（队列 #7 `cpptlm-cep-cohif-carrier`）

> **纪律**：每条新判据必须有**可执行**的反向 fixture，且**必须登记**在此表
> （无登记 ⇒ fixture 可能只存在于某次会话里 ⇒ CI 跑不到 ⇒ 等于没有）。
> **登记三要素**：判据 ID · fixture 形态 · **登记位置**（ctest 名 / Catch2 标签）。

| 判据 | 反向 fixture（形态） | 登记位置 |
|------|----------------------|----------|
| **CC-01** ① | 加第 6 个 `flow_id` ⇒ 静态扫描红 | `cep_cohif_reverse_verify.sh`（ctest `cep_cohif_reverse_verify`）+ `[cep][cohif-carrier]` |
| **CC-01** ② | 混入网络层字段 ⇒ 静态扫描红 | 同上 |
| **CC-01** ③ | 域外流名进入 `flow_id` 域 ⇒ 静态扫描红 | 同上（扫描器含「域外流名」判定） |
| **CC-02** ② | 加虚函数 ⇒ `is_trivially_copyable` 编译失败 | `cep_cohif_reverse_verify.sh` + `[cep][cohif-carrier]`「CC-02 反向 fixture ④」 |
| **CC-02** ③ | 字段宽度 +1 ⇒ 宽度锁编译失败 | `cep_cohif_reverse_verify.sh` + `[cep][cohif-carrier]`「CC-02 反向 fixture ③」 |
| **CC-02** ③ | 某字段漏加宽度锁 ⇒ 锁表扫描红 | `[cep][cohif-carrier]`「CC-02 ③ 反向 fixture」 |
| **CC-03** | 篡改字段映射 ⇒ round-trip 必红 | `cep_protocol_bridge_reverse_verify.sh`（ctest `cep_protocol_bridge_reverse_verify`）+ `[cep][protocol-bridge]`「反向 fixture ④」 |
| **CC-04** ① | 断链 ⇒ 端到端断言必红 | `[cep][json-assembly]` / `[cep][cohif-carrier]`「CC-04 反向 fixture」（**结局枚举分类** ⇒ 防「因错误的原因红」） |
| **CC-04** ③ | `N = 1` ⇒ 宏 `static_assert` 编译红（原因命中） | `cep_chstream_bidirectional_macro_reverse_verify.sh`（ctest）+ `[cep][cohif-carrier]`「CC-04 ③」 |
| **CC-05** ① | 只注册不消费 ⇒ 死桥清单非空 | `cep_protocol_bridge_reverse_verify.sh` + `[cep][protocol-bridge]`「反向 fixture ①」 |
| **CC-05** ④ | 重复 `bridge_id_` ⇒ 判红 | 同上「反向 fixture ②」 |
| **CC-05** ④ | `protocol_id_` 与 `family` 不符 ⇒ 判红 | 同上「反向 fixture ③」 |
| **CC-06** | 映射产出超出 3 类值域 ⇒ 判红 | `[cep][protocol-bridge]`「CC-06」（**引用**队列 #6 的 `category_value_domain()`，不重立第二份判据） |

### 反向 fixture 登记（队列 #13 `cpptlm-cep-nvlink-sharp` —— **实现层**）

> **纪律**：每条新判据必须有**可执行**的反向 fixture，且**必须登记**在此表
> ⚠️ **形态**：「毒化 ⇒ 判据**必红**」（**鉴别力**）⇒ 与队列 #11 修**误报**的那一组
> （「毒化 ⇒ 判据**仍绿**」= 回归防护）**形态相反** ⇒ **两组都须登记**，避免混用。
> ⚠️ **归因必须「精确相等」**（⛔ 不用前缀匹配）—— 否则 `CP-03` 会命中 `CP-03-VC-PRECONDITION`。

| 判据 | 反向 fixture（形态） | 登记位置 |
|---|---|---|
| **NVA-01** ① 零自有表副本 | adapter 注入成员字面量 `IngressDecision::` ⇒ 源扫描红 | `cep_nvlink_sharp_reverse_verify.sh`（ctest `cep_nvlink_sharp_reverse_verify`） |
| **NVA-01** ② 无 `transition_*` | adapter 加 `transition_x()` ⇒ 源扫描红 | 同上 |
| **NVA-02** caps 取自 traits | **同时**毒化 traits 行（`Completeness` ⇒ `CONSERVATIVE`）+ tracker 硬编码 caps ⇒ 可见 caps 与 traits 不等 ⇒ 红 | 同上 |
| **NVA-03** 实例声明 | `include/tlm/cep/protocol_instances.json` 删 1 键 ⇒ 13 键判据红 | 同上 |
| **CP-BARRIER-CLASS** V3 双向 | `classify_barrier_key` 两向判成同一类 ⇒ 红 | 同上 |
| **NVA-05** 算子族恰 6 值 | 在 `ReduceOp` 之后**追加** `Prod = 6` ⇒ 源文本枚举项计数 = 7 ⇒ 红 | 同上 |
| **CP-PARTIAL-FAIL** partial failure | 移除 `group_destroy` 的 Fault 产出 ⇒ 无 Fault 事件 ⇒ 红 | 同上 |
| **NVA-07** 集体面绑定 | region 的 `mode` 改成 `normal` ⇒ R3 降级判据红 | 同上 |
| **CP-03-VC-PRECONDITION** V6-c | 绑定一个**未注册**的 plane 名 ⇒ 前提约束红 | 同上 |

### 反向 fixture 登记（队列 #14 `cpptlm-cep-ualink-inc-state-spec` —— **规约层**）

> 同上纪律。本批的判据实现 = **C++ 载体(header-only · 自足)+ 影子副本编译期红 + 跨载体探针**。
> ⇒ fixture 落点 = `scripts/test/cep_ualink_inc_state_spec_reverse_verify.sh`(ctest `cep_ualink_inc_state_spec_reverse_verify`)
> ⇒ **11 组 / 12 条**毒化 fixture **1:1** 对应 design §5 登记的 **11** 个本层归因键(`UIS-01` ~ `UIS-11`)
> （「组」= 归因键、「条」= 毒化变体:`F-11` 有 `a` / `b` 两条,同归因 `UIS-11` ⇒ 组数仍 11)
> ⇒ 归因匹配 = `static assertion failed: <id>: `(**带尾冒号**)—— ⛔ 裸子串会让 `UIS-01` 被 `UIS-10` 误命中
> ⇒ 判定为 **presence**(毒化某格 `kind` 时日志**同时**含该格 id 与 `UIS-10:`)⇒ ⛔ 不得要求「只有一个归因 id」
> ⇒ 驱动**双分支**:编译红 ⇒ 匹配 `static_assert` 文案;编译绿 ⇒ 跑探针 ⇒ 匹配 `PROBE:FAIL <id> `
> ⇒ ⚠️ 探针的**鉴别力自测**另计(运行期;验证探针有牙)⇒ **不**计入下表的 11 组(避免破坏 1:1)。

| 判据 | 反向 fixture（形态） | 登记位置 |
|---|---|---|
| **UIS-01** 原语集恰 4 / 不新增原语 | 影子副本 `kIncOpCount` 4 ⇒ 5 ⇒ 编译期红 | `cep_ualink_inc_state_spec_reverse_verify.sh`（`f-01-inc-op-count`） |
| **UIS-02** reduce 算子只引用既有族 | 影子副本翻转 `kOperatorFamilyDefinedInCarrier` ⇒ 编译期红 | 同上（`f-02-operator-family-lock`） |
| **UIS-03** partial failure ⇒ 显式 Fault | 影子副本翻转 `partial_failure_must_be_explicit` 函数体 ⇒ 编译期红 | 同上（`f-03-partial-failure-silent`） |
| **UIS-04** `ReadReduce` 可见性边界 | 影子副本把该格 `kind` 由 `Pending` 改 `SelfDerived` ⇒ 编译期红 | 同上（`f-04-uis04-pending-to-selfderived`） |
| **UIS-05** 排序 / 原子性边界 | 同上 | 同上（`f-05-uis05-pending-to-selfderived`） |
| **UIS-06** 控制面归属边界 | 同上 | 同上（`f-06-uis06-pending-to-selfderived`） |
| **UIS-07** 集合 VC 继承 + 不一致即禁用 | 影子副本翻转 `vc_inheritance_is_well_defined` 函数体 ⇒ 编译期红 | 同上（`f-07-vc-inheritance-always-true`） |
| **UIS-08** `ualink_4` 的 `flow_support` 待定 | 影子副本把该格 `kind` 由 `Pending` 改 `SelfDerived` ⇒ 编译期红 | 同上（`f-08-uis08-pending-to-selfderived`） |
| **UIS-09** `ualink_4` 的 `perm` 待定 | 同上 | 同上（`f-09-uis09-pending-to-selfderived`） |
| **UIS-10** 逐格 basis 完备 + 类别自洽 | 影子副本把 `SelfDerived` 的 `ref` 冒充成 `A-n` ⇒ 遍历式断言红 | 同上（`f-10-basis-ref-public-impersonate`） |
| **UIS-11** 证书规约面 + ⛔ 不得以重跑 DAG 为判据 | 影子副本翻转 `kDagRerunIsCertificate` ⇒ 编译期红 | 同上（`f-11a-dag-rerun-as-certificate`） |
| **UIS-11** 证书表齐备 | 影子副本**删掉** `C-4` 那一行(第 4 项值初始化)⇒ 编译期红 | 同上（`f-11b-certificate-table-missing`） |
| **跨载体** X-1 / X-1b / X-2 / X-3 | 探针的 8 项运行期判据（载体**自足** ⇒ 跨载体比对只能在探针侧） | `scripts/test/cep_ualink_inc_state_spec_probe.cc`（经驱动脚本进入 ctest） |

### 反向 fixture 登记（队列 #14 `cpptlm-cep-ualink-inc` —— **实现层**）

> 同上纪律。本批的判据实现 = **C++ 载体**（`include/tlm/cep/ualink_adapter.hh` / `include/tlm/cep/fabric_collective_plane.hh` / `include/tlm/cep/collective_plane_contract.hh`）
> + **影子副本**（影子目录 `-I $SHADOW/include` 优先）+ **跨载体探针**。
> ⇒ fixture 落点 = `scripts/test/cep_ualink_inc_reverse_verify.sh`（ctest `cep_ualink_inc_reverse_verify`）
> ⇒ 探针 = `scripts/test/cep_ualink_inc_probe.cc`（**8 项**运行期判据 + **归因 id 精确提取**）
> ⇒ **11 组 / 12 条**毒化 fixture(**组** = 归因键 · **条** = 毒化变体)对应 design §4 登记的 **11** 个归因键
> ⇒ 「组」↔ 归因键 **1:1**;`C-1′` 组有 **2 条**(语义面 + 结构面)⇒ 条数 12、组数仍 11
> —— 本层**新建 7** 个（`UIA-01` ~ `UIA-07`）+ **4** 个 `ADR-CEP-01` E.3.1 **既有** id 的**消费**
> （`CP-03-VC-CERTIFICATE/C-1′` · `CP-09-IC1` · `CP-03-VC-CERTIFICATE/C-3` · `CP-03-VC-CERTIFICATE/C-4`）
> ⇒ 归因匹配 = `PROBE:FAIL <id> `（**完整前缀精确相等**）—— ⛔ 裸子串会让 `CP-03` 命中 `CP-03-VC-PRECONDITION`
> ⇒ ⚠️ **C-3 的两级**归因**分别**:声明级 ⇒ `[CP-03-VC-CERTIFICATE/C-3]`;运行级 ⇒ `UIA-04`
>   (⛔ **不共用**一个 id —— 声明级是**证书判定**、运行级是**行为义务**;per `ADR-CEP-01` SU#5 ②)
> ⇒ 驱动**双分支**:编译红 ⇒ 匹配编译日志;编译绿 ⇒ 跑探针 ⇒ 匹配 `PROBE:FAIL <id> `
> ⇒ ⚠️ **F-11 毒化的是「登记提供方」**(`default_untested_registry`)而非规约载体的**数据** ——
>   后者带自己的 `static_assert`(归因 `UIS-11`)⇒ 毒化它会变成**编译期红**且归因落到**规约层** id
>   ⇒ 破坏本层「归因 1:1」⇒ 该保护已由**规约层**的 `f-11b` 覆盖 ⇒ 本层改为毒化**提供方**以验证
>   **C-4 判据本身**有牙。

| 判据 | 反向 fixture（形态） | 登记位置 |
|---|---|---|
| **UIA-01** adapter 零自有表副本 | 影子副本注入载体成员字面量 `Obligation::` ⇒ 文本扫描红 | `cep_ualink_inc_reverse_verify.sh`（`f-01-member-literal`） |
| **UIA-02** adapter 无 `transition_*` | 影子副本加 `transition_read_reduce()` ⇒ 文本扫描红 | 同上（`f-02-transition`） |
| **UIA-03** plane 数据路径真可达 | 影子副本把收齐分支改成「不产出事件 + 数据路径未实现」⇒ 运行期红 | 同上（`f-03-data-path-not-implemented`） |
| **UIA-04** C-3 控制面与数据面互斥 | 影子副本删掉 `group_register_region` 的 in-flight 检查 ⇒ 运行期红 | 同上（`f-04-c3-check-removed`） |
| **UIA-05** `status` 值域 = 2 值 | 影子声明源 `status` ⇒ `"shared"` ⇒ 运行期红 | 同上（`f-05-status-third-value`） |
| **UIA-06** `certificate_scope` 登记 | 影子声明源清空 `certificate_scope` ⇒ 运行期红 | 同上（`f-06-certificate-scope-empty`） |
| **UIA-07** `ualink_4` 不声明 | 影子实例声明源把一条实例改成 `ualink_4` ⇒ 运行期红 | 同上（`f-07-ualink-4-declared`） |
| **C-1′** 槽位语义继承(语义面) | 影子实例声明源令 `mesif` 的 `ualink_fabric` 映射与 `moesi_gpu` 不一致 ⇒ 运行期红 | 同上（`f-08a-c1-mapping-mismatch`） |
| **C-1′** 槽位语义继承(结构面) | 影子声明源注入 `collective_plane_vc_binding` 块(集合平面**自带**独立绑定)⇒ 运行期红(per `ADR-CEP-01` E.9 的反 fixture) | 同上（`f-08b-c1-self-declared-binding`） |
| **C-2** VC 覆盖完整 | 影子 `include/tlm/cep/fabric.json` 把 `ualink_fabric.vc_count` 4 ⇒ 5 ⇒ 运行期红 | 同上（`f-09-c2-vc-count`） |
| **C-3** 控制面互斥（**声明级**） | 影子 `include/tlm/cep/collective_plane.hh` 把 `enforces_control_plane_exclusion` ⇒ `false` ⇒ 运行期红 | 同上（`f-10-c3-declaration`） |
| **C-3** 控制面互斥（**运行级**） | 影子 `include/tlm/cep/fabric_collective_plane.hh` 删掉 `group_register_region` 的 in-flight 检查 ⇒ 运行期红 | 同上（`f-04-c3-check-removed`） |
| **C-4** 未检验项登记 | 影子契约头让登记项表述含「已验证」⇒ 运行期红 | 同上（`f-11-c4-untested-faked`） |

### 反向 fixture 登记（队列 #11 `cpptlm-cep-tilelink-state-spec`）

> 同上纪律。本批的判据实现**全部在 C++ 载体 + Catch2**（无 Python 守门脚本）⇒ fixture 落点 = Catch2 标签
> `[cep][tilelink][state-spec]`;**锚点交叉核对**面另由生成器承担(`generate_tilelink_anchor_checklist.sh`)。
> ⚠️ **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 载体的结构校验全部提供 `*_for(...)` 运行时可判入口。

| 判据 | 反向 fixture（形态） | 登记位置 |
|------|----------------------|----------|
| **TSS-01** ② | 把 `Tip` 换成 `Trunk`（**仍 3 个**）⇒ 必红 | `[cep][tilelink][state-spec]`「稳定态集合」 |
| **TSS-01** ③ | 加回 `Trunk`（4 个名字）⇒ 必红 | 同上 |
| **TSS-02** | 把 `Report` 换成 `Query` ⇒ 必红 | 同上「事件词表」 |
| **TSS-03** ⑤ | 某格改成 `Unknown` ⇒ 计数层必红（`Unknown` 0→1） | 同上「转移矩阵」 |
| **TSS-03** ⑥ | 空集格数改成 3 ⇒ 必红 | 同上 |
| **TSS-03** ⑦ | `single` ↔ `multi` 互移一格 ⇒ 必红（**5/5 分桶**才有鉴别力） | 同上 |
| **TSS-03** ④ | `Unknown` 格携带非零掩码 ⇒ 必红（零非法组合） | 同上 |
| **TSS-03** ⑤ | `Known` 格缺 `ref` ⇒ 必红 | 同上 |
| **TSS-04** | `perm` 投影加回 `UNIQUE` 行 ⇒ 必红 | 同上「perm 投影」 |
| **TSS-06** | 换成 CXL 形态（`Acquire` 挪到 B 通道）⇒ 必红 | 同上「非对称断言」 |
| **TSS-06** | `Grant` 挪到 A 通道 ⇒ 必红 | 同上 |
| **TSS-06** | `Probe` 挪到 D 通道 ⇒ 必红 | 同上 |
| **TSS-07** ④ | 锚点少一条 `TLC-nn` ⇒ 必红 | 同上「逐格 basis」 |
| **TSS-07** ④ | 载体引用锚点里不存在的条目号 ⇒ 必红 | 同上 |
| **TSS-07** ④ | 载体重复引用同一条目号 ⇒ 必红 | 同上 |
| **TSS-07** ⑥ | ID 后缀非全数字 ⇒ **不得**被收（文法收紧） | 同上 |
| **TSS-07** ④ | 锚点改一个 `TLC-nn` / 一个目标掩码 ⇒ 必红 | `generate_tilelink_anchor_checklist.sh --check` |
| **TSS-07** ④ | 载体改一个 `ref` ⇒ 必红 | 同上 |

### 反向 fixture 登记（队列 #8 `cpptlm-cep-chi-rnf-subset`）

> 同上纪律。本批的**判据实现分布**决定了 fixture 的落点：实例/状态机/引擎面在 C++（判据本体零改动，
> 复用 AC-01~05 + 本 change 新增的引擎观测面），声明源闸面在 Python 守门脚本 ⇒ **不复制第二份实现**。

| 判据 | 反向 fixture（形态） | 登记位置 |
|------|----------------------|----------|
| **CR-01** ① | 删 `chi_rnf` 的**任一** `required` 键（13 个**穷举**）⇒ 判据必红 | `[cep][chi-rnf][cr-01]`（谓词 `schema_violations()`，缺键**记违规不抛异常**） |
| **CR-01** ③ | `protocol_version` 写成锁定表外的值（`chi-r3`）⇒ 必红 | 同上 |
| **CR-01** ② | `perm` 少一个值（4 值锁被破）⇒ 必红 | 同上 |
| **CR-02** ④ | `vc_binding` 的 `vc_id` 越界（3→9）⇒ 装配闸必 FAIL | `[cep][chi-rnf][cr-02]`（内联声明**必须**带边表，否则「假绿」） |
| **CR-02** ⑤ | 删 `vc_binding` 的某消息类 ⇒ 边端点失去绑定 ⇒ 闸必 FAIL | 同上 |
| **CR-02** ② | 消息类写成 6 类词表外的值 ⇒ 红 | `cep_decl_reverse_verify.sh`「破坏 5」 |
| **CR-02** ③ | 删 `ordering_assumption` 的某网络键（键不对称）⇒ 红 | `cep_decl_reverse_verify.sh`「破坏 4」 |
| **CR-03** ① | 删 `snoop` 映射行（仍声明使用）⇒ AC-04 必红 | `[cep][chi-rnf][cr-03]` |
| **CR-03** ② | 映射目标写成第 4 类 ⇒ AC-04 必红 | 同上 |
| **CR-03** ④ | 全表 == 方向默认表且**无** `identity_reason` ⇒ AC-04 必红 | 同上 |
| **CR-04** ② | 让某稳定态「有为稳定态但无投影」⇒ AC-05 必红 | `[cep][chi-rnf][cr-04]` |
| **CR-04** ③ | 改掉 `UC + Store → UD` 的目标态（4 ≠ 3）⇒ AC-05 必红 | 同上 |
| **CR-05** ① | `instance_id` 改成不存在的 id ⇒ AC-02 必红 | `[cep][chi-rnf][cr-05]` |
| **CR-05** ④ | 改成 `UNIQUE → READ`（降权限）⇒ AC-02 必红 | 同上 |
| **CR-06** ② | `impl = 不参与` 但 `caps.scope = SYSTEM` ⇒ 谓词必红 | `[cep][chi-rnf][cr-06]` |
| **CR-07** ③ | 加**未登记**的额外实例 ⇒ ⑥ 必红 | `cep_decl_reverse_verify.sh`「破坏 16」 |
| **CR-07** ④ | 把 `moesi_gpu` 登记进 `repo_owned_instance_ids[]`（撞名）⇒ 必红 | 同上「破坏 18」 |
| **CR-08** ① | 本仓域实例违 schema（`vc_binding` 越出 6 类词表）⇒ ① 扩面必红 | 同上「破坏 17」 |
| **CR-08** ⑤ | 登记 id 指向不存在的实例 ⇒ 登记键自身必红 | 同上「破坏 19」 |
| **CR-07** ② | 仅 `status` 差异 ⇒ 仍**绿**（正控：豁免边界） | 同上「破坏 15 / 20」 |
| **CR-09** ④ | 改 `Status Update` 段 ⇒ hash 校验红 / 跳过 `--update` ⇒ provenance 红 | `cep_freeze_check.sh`（ctest `cep_freeze_check`） |
| **CR-10** ② | 引擎换成 `CohIfNullProtocolEngine` ⇒ 发射谓词必红（Null 零发射） | `[cep][chi-rnf][cr-10]`（正/反向并置：`deliver_read_bundles` 1 vs 0） |
| **CR-10** ③ | 同拓扑把 `type` 换成 Null ⇒ 类型判据 + 抵达判据**双红** | `[cep][chi-rnf][cr-10][json-assembly]` |
| **CR-11** ① | **逐通道**屏蔽（6 条）：该通道观测点 == 0 且汇聚判据红 | `[cep][chi-rnf][cr-11]` |
| **CR-12** ① | 旁路 credit 门控 ⇒ 耗尽态 `emitted` 不再是 0（同款驱动 + 充足 credit 作对照） | `[cep][chi-rnf][cr-12]` |
| **CR-13** ① | 强制在途重复 `transaction_id` ⇒ 必被拒且**不增长在途集** | `[cep][chi-rnf][cr-13]` |
| **CR-14** ① | 抑制 CompAck / 重复完成 ⇒ `comp_ack_sent` 不再是**恰好 1** | `[cep][chi-rnf][cr-14]` |
| **subset 排除项** | 9 个排除 opcode 注入 ⇒ 全拒 + 三条后果（无通道事件 / 不占 TxnID / 不进发射队列） | `[cep][chi-rnf][subset-exclusions]` |
| **消费方覆盖** | 从声明里摘掉 `chi_rnf` ⇒ 装配闸 `per_instance` 缺失该实例 | `[cep][chi-rnf][consumers]` |

## 路径引用注意

- `scripts/CMakeLists.txt` 调度 `topology/topology_validator.py`
- 各 bash 脚本用 `BASH_SOURCE` 解析自身目录，所有跨子目录引用用相对路径 `../`
- Python demo 导入用 `sys.path.insert(0, ...)` 显式添加

## 验证

```bash
# 推荐流程: 先构建再测试
./build.sh && ./test.sh

# 干净重建 + 跑测试
./build.sh --rebuild && ./test.sh

# 一键回归验证 ([dgpu_framebuffer] 修复 + 4 commit)
./test.sh --verify-fix

# 其他用法
./test.sh --e2e
./test.sh --quick
./test.sh --mode ptx-emu  # 仅 PTXIR image H2D DMA，不执行 kernel/CuTe

# CMake target 应仍工作
cmake --build build --target validate_topology

# 文档路径同步检查（pre-commit hook 也会自动运行）
./scripts/test/docs_sync_check.sh --strict
```

## 文档同步检查 (`scripts/test/docs_sync_check.sh`)

- 扫描 `AGENTS.md` / `docs/ONBOARDING.md` / `roadmap.md` / `scripts/README.md` 中所有 `path/to/file.ext` 反引号引用
- 扩展名覆盖代码文件（`.hh` `.cc` `.py` `.json` `.sh` …）**与 `.md`**（2026-11-05 新增；`.md` 仅检查**仓库根起算**的路径，章节内相对引用不在检查范围）
- 通配 / 占位写法（`ADR-*.md` / `{a,b}/x.md` / `<name>.md` / `ADR-05~09-*.md`）自动跳过 —— 它们不对应单一物理文件
- 通过 CMake `INCLUDE_PATH_PREFIXES` 智能补全（处理无 `core/` 前缀的简写形式如 `<file>.hh`）
- 通过 `VIRTUAL_PATHS` 数组排除已删除/归档文件（仅在文档中说明删除原因时引用）
- `--strict` 模式发现任何缺失返回非零退出码（用于 pre-commit hook）
- 收尾会调用 `scripts/test/agents_md_lint.py`（见下节）—— `--strict` 模式下其失败同样阻断
- 当前快照：[`docs/docs_audit_report.md`](../docs/docs_audit_report.md)

## Markdown 链接检查 (`scripts/test/markdown_link_check.sh`)

- 扫描 `docs/` 全树 + `AGENTS.md` / `docs/README.md` / `docs/ONBOARDING.md` 中所有 `[text](relative/path)` 链接，验证**相对路径**可达
- 与 `docs_sync_check.sh` 互补：后者扫「反引号里的代码路径」，本脚本扫「markdown 链接」
- `--strict` 模式发现任何断链返回非零退出码
- **2026-11-05 修正 4 处脚本缺陷**（修正前报出的 108 处中只有 45 处是真断链，其余全是误报；另有 63 处真断链因下列缺陷而**完全不可见**）：
  1. `VIRTUAL_PATH_PATTERNS` 数组**定义了但从未传给 Python** —— 已删除/已归档/跨仓路径被误报为断链
  2. 解析相对路径前**未剥离 `#anchor`** —— 所有带锚点的链接被误报
  3. bash 用 `IFS='||'` 拆分多行输出 —— 多字符 IFS 被逐字符拆，产生空条目，计数虚高
  4. **链接文本整体被行内代码包裹**时（`[\`path\`](../target/)`），去代码后变成 `[]` 而逃过检查 —— 改为用单字符占位符替换行内代码，保留方括号结构
- 豁免清单在 `VIRTUAL_PATH_PATTERNS` 数组内，逐条注明豁免理由（已删除 / 已归档 / 跨仓未克隆 / 旧计划目录）

## AGENTS.md 结构 / 体量守门 (`scripts/test/agents_md_lint.py`)

- 职责边界：`docs_sync_check.sh` 校验「**文档里引用的**路径是否存在」；本脚本校验「**AGENTS.md 自身**的目录树与体量是否失控」
- 3 项检查：
  1. **STRUCTURE 目录树逐条路径存在性** —— 解析 `## STRUCTURE` 的代码围栏，按缩进重建层级后逐条 `stat`（原检查器不扫代码围栏内的树，且目录类路径不在扩展名白名单内 → 长期漏检）
  2. **文件体量上限 64 KiB** —— 阻断 append-only 日志把 AGENTS.md 撑成第二份台账（AGENTS.md 注入每个 agent 的上下文，体量直接等于每会话 token 成本）
  3. **单行长度上限 1000 字符** —— 阻断把流水账压成超长单行的形态
- 独立运行：`python3 scripts/test/agents_md_lint.py [--strict]`
- 过程日志（finding 编号 / 评审轮次 / 归档流水）应落 ADR `## Status Update` 段或 `openspec/changes/archive/`，不进 AGENTS.md

## 测试入口与范围

- 根目录 `test.sh` 是统一测试入口（默认只 test），支持 `--build`、`--mode auto|off|ptx-emu|both`、`--build-only`、`--test-only`、`--verify-fix`、`--quick`、`--python-only`、`--e2e` 和 `--ctest`。
- 根目录 `build.sh` 是独立构建入口（薄包装转 `scripts/build/build.sh`），支持 `--rebuild`、`--ptx-emu`、`--debug`、`--no-tests`、`--no-examples`、`--jobs N`。
- `scripts/test/test_off.sh` 承担 OFF 路径回归；`scripts/test/test_ptx_emu.sh` 承担 PTX-EMU 路径验证。
- PTX-EMU 验证范围严格限于 PTXIR image H2D DMA；kernel 执行与 CuTe 程序执行均明确排除。
- `run_all_tests.sh` 和 `ci_e2e_test.sh` 保留为兼容包装，不是新的 CI 主入口。
- `verify_test_seam_fix.sh` 是 `[dgpu_framebuffer]` 修复（commit `ba32fe97`）+ 4 commit 回归一键验证（7 步：git 状态/构建/fix target/9 核心标签/全量/稳定性/cpptlm-debug 4 件套）。可通过 `test.sh --verify-fix` 或 `scripts/test/verify_test_seam_fix.sh` 直接调用。
