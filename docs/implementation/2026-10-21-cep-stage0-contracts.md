# CEP Stage 0 契约层实施笔记

> **状态**: 🟡 实施中(Phase 1~7 完成,Phase 8 收尾)
> **日期**: 2026-10-21
> **作者**: CppTLM Team(builder mode)
> **关联**:
> - OpenSpec change:[`openspec/changes/cpptlm-cep-umbrella/`](../../openspec/changes/cpptlm-cep-umbrella/)(proposal / design / specs / tasks)
> - 真源 1:[`docs/architecture/07-coherence-exploration-platform.md`](../../docs/architecture/07-coherence-exploration-platform.md) v0.1
> - 真源 3:`docs/adr/ADR-CEP-*.md` **共 21 份**
> - 路线图:[`phase-10`](../../.rddf/roadmap/phases/phase-10.md) · [`objective-cep-platform`](../../.rddf/roadmap/objectives/obj-cep-platform.md)

---

## 0. 一句话

Stage 0 交付的是**契约层**:把 21 份 ADR 里的 **66 条实际守门 invariant**(编号位 67)变成**机器可执行断言**,并建立 capability schema、实验输出契约、集成 seam 契约与**契约同步 lint**。**不交付任何协议实现**。

---

## 1. 交付物清单

### 1.1 新增文件(14)

| 文件 | 内容 |
|------|------|
| `test/cep_adr_parser.hh` | ADR 不变量解析器(区分「提及」与「定义」;5 种定义形态 + 全仓扫描 + owner 语义 + 07 号 §4 第 3 列 ID 区间展开) |
| `test/test_cep_canonical_invariants.cc` | 15 组 invariant 契约 + 07 号总表**集合相等** + 全仓/孤儿/NO_INV 完备性 + C10 组合检查项 |
| `test/test_cep_schema.cc` | capability schema **7 字段组 / 13 属性** + **C9 负向断言**(词边界含下划线) + 跨文档版本值域 |
| `test/test_cep_simulation_mode_lock.cc` | 4 件套 + 13 metric(required **集合相等**) + `verification_mode` |
| `test/test_cep_dummy_signals.cc` | Null 实现 10 流覆盖矩阵契约 + 折叠映射表 + **C1~C10 表行解析**(非裸子串) |
| `test/test_cep_module_factory_seam.cc` | 真实注册机制 + **冻结面 0 diff 守门**(钉死基线 + fail-loud + 机制自证) + CacheTLM 零协议知识(大小写归一) |
| `test/test_cep_tools_contract.cc` | tools **行为断言**(真跑 `--help` / `--check` / `py_compile`)+ ast 零依赖判定 + runner 契约 |
| `test/test_cep_contracts_crosscheck.cc` | 4 个「spec 声称了但测试没断言」的 requirement 补齐(numbering / version locking / route mapping / 06bis disposition) |
| `include/tlm/cep/protocol_instance.schema.json` | 协议实例能力声明 schema(**7 字段组 / 13 个顶层属性**) |
| `experimental/metrics.schema.json` | `metrics.json` schema(13 必纳 + 可选 + verification) |
| `experimental/.gitkeep` | 实验目录占位 |
| `tools/render_report.py` | 输入 3 件套 → `report.html`;**一致性闸门** + metric 类型校验 + 违例 ❌ 横幅 |
| `tools/run_sweep.py` | sweep 驱动 → N 个实验目录 + `sweep_summary.csv`;runner 双通道契约 + `--render-report` / `--require-complete` |
| `tools/ab_compare.py` | A/B 差分 → `delta_metrics.json` + HTML;**一致性闸门** + 中性指标 + 两侧 verification 比对 |
| `scripts/test/cep_docs_sync_check.sh` | **契约同步 lint**(6 组检查;含全仓/孤儿/NO_INV/ID 区间集合相等) |

> 另有本文件 `docs/implementation/2026-10-21-cep-stage0-contracts.md` 本身。

### 1.2 修改文件(12)

| 文件 | 改动 |
|------|------|
| `docs/architecture/06-bis-coherence-as-network-extension.md` | 顶部加 `## Status Update` 段(正文 0 diff) |
| `docs/architecture/07-coherence-exploration-platform.md` | §4 总表扩至 15 组 + 计数口径纪律;§9 注册 5 份新 ADR;§8.3 状态 |
| `docs/adr/ADR-CEP-00A-real-source-anchor-map.md` | §4 映射行口径同步 |
| `docs/adr/ADR-CEP-NUMBER-MAP.md` | §2 决策 1 编号表**去重**(5 处重复登记 → 每编号 1 行)+ 追溯说明 + v0.2 修订记录 |
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | 权限状态数「4 个」→ **5 个**(数字与枚举自相矛盾) |
| `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md` | 引用同步(文件名拼写修正) |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | 字段组口径统一为 **7 组 / 13 属性** + 修订说明 |
| `docs/adr/ADR-CEP-15-cachetlm-alignment.md` | §4 冻结面验收命令改为**相对 CEP 基线**(原 `git diff HEAD` 在 CI 恒绿) |
| `docs/adr/ADR-CEP-17-ruby-slicc-protocol-family.md` | 字段组口径同步 + 引用同步 |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | **文件名修正**(原 `...-goverance.md`,全仓 17 处引用同错) |
| `src/core/topology_parser.cc` | `coherence_domains` 告警文案 → `ADR-CEP-13` + `ADR-CEP-07A`(原指向 superseded 的 `ADR-X.14`) |
| `include/core/coherence_domain.hh` + `src/core/coherence_domain.cc` | `[[deprecated]]` + 指向 ADR-CEP-07A 的顶部注释(per `INV-CEP-07A-IC1`) |
| `test/CMakeLists.txt` | 注册 **13** 个 `cep_*` ctest(含 `WORKING_DIRECTORY` + tools 语法/冒烟) |
| `.gitignore` | `experimental/` 产物不入库(仅 `.gitkeep` + schema) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | DoD 7 分解 10 → 15 组;§9.2 角色指定;§11 Entry #5/#6 |
| `.rddf/roadmap/phases/phase-10.md` | 16 → 21 份 ADR;10.B → Proposed |

---

## 2. TDD 5-step 记录

| Phase | RED | GREEN | 关键发现 |
|-------|-----|-------|---------|
| 1 ADR 解析器 | 7 cases 全红 | 7 cases / 238 assertions | 解析器两处缺陷:`^` 缺 `multiline` 标志;未限定在 §4 节内(会误抓全文其他表) |
| 2 文档层 | — | 06-bis Status Update + 07 号复核 | 组名以真源为准(`Stub Cache Core` 非 `Stub Cache`) |
| 3 schema | 12 cases / 4 失败 | 12 cases / 95 assertions | **C9 负向断言抓到 2 个真问题**:① `hierarchical` 含子串 `chi`(**假阳性** → 改手写词边界,`std::regex` 默认引擎不支持 lookbehind);② schema 描述里出现 `token`(协议概念)→ 改写为协议中立措辞 |
| 4 Null 覆盖矩阵 | — | 6 cases | 契约层断言,不含实现行为 |
| 5 seam 契约 | — | 7 cases | 冻结面用 `git diff --name-only HEAD` 实测 |
| 6 实验输出 | — | 8 cases / 54 assertions | `network.mode` 否定语境判定:**否定词常在出现点之前**(「不引入 network.mode 4 组合」)→ 判定窗口须前后都覆盖 |
| 7 tools + lint | — | 6 cases + lint 8/8 | 工具消费 4 件套的**子集**各不相同 → 契约须逐 tool 声明 |
| 8 收敛 | — | 全量 0 回归 | — |

---

## 3. 三个值得记录的技术教训

### 3.1 「提及」≠「定义」——计数必须区分

`INV-CEP-04-IC4` 是**永久跳空**编号,被 20+ 处**提及**(说明为何跳空)但**从未定义**。按"提及"计数会**多算 1 条**。

**实现**(`test/cep_adr_parser.hh`):三种**定义位**形态
| 形态 | 例 |
|------|-----|
| 加粗 | `**INV-CEP-04-IC7**(新增...)` |
| 列表项起始 | `- CEN-Ib9: ...` |
| 行内标签 | `**不变量**(INV-CEP-04-IC1): ...` |

且**只在 owner ADR 内判定** → 跨 ADR 引用天然排除。

### 3.2 词边界 vs 子串

C9 负向断言(协议标识符扫描)最初用 `find()` 子串匹配 → `"hierarchical"` 含 `"chi"`、`"concurrent"` 含 `"current"` → **假阳性**。
C++ `std::regex` 默认 ECMAScript 引擎**不支持 lookbehind** → 改**手写词边界判定**(`contains_word`)。

### 3.3 4 件套的**产出方向** —— 自指矛盾

`render_report.py --check` 最初要求 `report.html` 存在 —— 但 `report.html` 正是**它自己**的产物 → **check 永远红**。`run_sweep.py` 的 `validate_exp` 同样要求 runner 产出 `report.html` → **runner 产不出**,链路不自洽。

**修正**:4 件套按**产出方向**区分
| 件套 | 产出者 | 消费者 |
|------|--------|--------|
| `simulation_config.json` / `trace.h5` / `metrics.json` | runner | render_report / ab_compare / run_sweep 校验 |
| `report.html` | **render_report** | 完整实验目录校验(`run_sweep --require-complete`) |

`run_sweep --render-report` 在每次运行后**后置**调用 render_report 补齐第 4 件套。

### 3.4 runner 契约缺「输出目录」通道

`run_sweep.py` 早期只把 `--<key>=<value>` 传给 runner,**从未告诉 runner 往哪写** → 每次运行都失败。
**修正**:双通道告知 —— 环境变量 `CEP_EXP_DIR` / `CEP_EXP_ID` + 命令行 `--exp-dir=`(同值,便于脚本化接入);并捕获 `OSError` 优雅报错。

### 3.5 lint 脚本的「范围」比「规则」更容易出错

P6 禁引检查最初扫全仓 → 命中 `06-inter-module-network.md` 自身的 §11.4 P6 表、历史 archive 的 `P1 → P5 → P6` 依赖链、调研附录 → **35 处假阳性**。
**收窄到 38 个 CEP 专属文档 + 逐出现点判否定语境** → 53 处全部合规。

### 3.6 命名一致性:两个 tool 的件套常量名不同步

`render_report.py` 用 `INPUT_ARTIFACTS` / `OUTPUT_ARTIFACT`,`run_sweep.py` 起初用 `ARTIFACTS` → 契约测试断言"两 tool 都须有 `INPUT_ARTIFACTS`"时红。**统一命名**后消除。

### 3.7 `ab_compare` 的「更好方向」漏了 2 个指标

`LOWER_IS_BETTER` 未穷举 `NUMERIC_METRICS` → `total_transactions` / `transactions_per_cycle`
落单 → `b_is_better = None` → 其 delta 会显示但**不带优劣标记**,**易被误读成"没有差异"**。
**修正**:拆出 `HIGHER_IS_BETTER`(吞吐类越大越好),并对仍方向未定义的指标显式标注"(方向未定义)"
+ 在 `delta_metrics.json` 里输出 `unknown_direction` 列表。

### 3.8 lint 的两处**漏检路径**

| 漏检 | 修复 |
|------|------|
| `OWNERS` 硬编码 → **新增第 22 份 ADR** 时其 invariant **不被计数**,且**静默**(计数会变成 65 而 07 号仍写 66) | 加**完备性检查**:目录内 ADR 必须 ∈ `OWNERS` ∪ `NO_INV` 白名单;否则 fail 并影响 `--strict` 退出码 |
| 链接检查只覆盖 `../adr/ADR-CEP-*.md` → 指向 `docs/architecture/` / `.rddf/` 的断链漏检 | 扩为**全量相对 `.md` 链接** |

### 3.9 两个新检查的**反向验证**(证明有牙,不是摆设)

| 破坏方式 | 期望 | 实测 |
|---------|------|------|
| 把 `ADR-CEP-12` 移出 `OWNERS` | lint 红 + 连带报 4 处下游不一致 | ✅ `--strict exit=1`,报「未纳入 OWNERS」+「组无法映射」+「实际定义数 63≠66」+「00A 映射行不符」 |
| 把 schema `protocol_version` pattern 改回有缺陷的 `^[a-z0-9]+(-[0-9.]+)?$` | 跨文档测试红 | ✅ 红,报「schema pattern 拒绝锁定表合法取值: chi-r2」 |

> **纪律**:新增的机器检查必须**反向验证**能抓到问题,否则只是"看起来在测"。

---

## 4. 契约同步 lint(6 组检查)

`scripts/test/cep_docs_sync_check.sh [--strict]`

| # | 检查 | 目的 |
|---|------|------|
| 1 | 21 份 ADR 存在 + 均有「不变量」或「决策」节 | 防止 ADR 缺失或空壳 |
| 2 | 07 号 §4 分组条数 ↔ owner ADR **实际定义**数;跳空 ID 确认未定义;07 号声明数 ↔ 实测;ADR-CEP-00A §4 口径;**OWNERS 完备性**(目录 ADR 必须 ∈ OWNERS ∪ NO_INV) | **直接封堵 Oracle N1/N2 的计数漂移** |
| 3 | P6 禁引(逐出现点判否定语境) | per `ADR-CEP-16 IC1` |
| 4 | ADR 交叉相对链接可达(**全量 .md 链接**,不限 `../adr/`) | 防断链 |
| 5 | umbrella change 4 件套齐备 | Stage 0 契约载体 |
| 6 | 契约产物(schema / experimental)存在 | — |

---

## 5. 已知局限(诚实边界)

1. **Stage 0 不交付实现** —— 全部测试断言的是「**契约存在且自洽**」,**不**断言「实现满足契约」(per `ADR-CEP-14` 决策 1)。实现验证在 Stage 1+ 各 change 的 AC。
2. **~~`named_network` → `vc_id` 解析~~ 的真实强制在 Stage 1 装配期检查器** —— ⚠️ **该谓词已于 2026-10-22 整体消失**
   (per `ADR-CEP-12 §8`:`ordering_assumption` / `vc_binding` 改 **per-network 嵌套形态**,key = network,
   其下 `vc_id` 显式给出)。现行形态的真实强制同样在 Stage 1 装配期检查器;Stage 0 的解析规则
   以 test 内部 helper 实现(`resolve_ordering_assumptions`)并以 fixture 钉死(per `ADR-CEP-12 IC2`)。
3. **`verification_mode` 的"不构成已证明"措辞**目前靠 lint + 测试**字符串存在性**校验,**不**是语义校验。
4. **`trace.h5` 是 stub** —— `render_report` **从不解析**它(既不 import `h5py` 也不读内容),只校验其**存在**。契约上诚实:4 件套里 `trace.h5` 目前是**占位**,真正解析在 Stage 1。(v0.1 曾误称"依赖 h5py / 缺失时降级提示",该说法与代码不符,已订正。)
5. **Stage 0 验收的独立性未闭合**(R10):三项角色指向同一人,以 M1 产物冻结 / M2 跨仓准入门 / M3 对拍基准外置 / **M4 独立 AI 复审** 四条机制部分替代。**2026-10-21 Task 8.4 结论**:验收等级 = **「部分独立」**(M1 ✅ 已冻结 / M2 ⛔ 未放行 / M3 ⏳ 待 Stage 1 / M4 ✅ 2 轮已执行)—— **不得表述为「完全独立验收」**;M4 不可替代真人外部评审(见 §7.4)。
6. **`include/core/coherence_domain.hh` 的 `enum class Protocol { MESI, MOESI }` 仍在**(per `ADR-CEP-07A` 决策 1 产物 1 / 决策 4 显式排除「不删」)。Stage 0 已按 `INV-CEP-07A-IC1` 加 `[[deprecated]]` + 顶部注释;**协议标识符本体**待 Stage 1 迁移到 Protocol Engine 侧(per `ADR-CEP-05` / `ADR-CEP-07A` 决策 3)。

---

## 6. 验证结果

```
[cep]                     75 cases / 1227 assertions  全绿
ctest -R "cep_"           13 / 13                      全绿(含新增 tools 语法 + 3×--help 冒烟)
全量 cpptlm_tests         81112 assertions / 1854 cases,0 回归
cep_docs_sync_check.sh    6 组检查全过,--strict exit=0
tools 端到端冒烟          render_report / run_sweep(+--render-report
                          /--require-complete) / ab_compare 均通过
反向验证(见 §7.3)        6 组注入破坏 → 对应检查必红
```

---

## 7. 独立复审处置台账(2026-10-21)

本节记录**第一次真正的独立复审**的发现与处置。复审通道:`oracle` / `critic` 在本环境
从注册表消失(`Agent not found`),改用 `general`(架构) + `explore`(代码对抗式)承担。

### 7.1 复审发现的 6 个 BLOCKER(全部已修)

| # | 问题 | 根因 | 处置 |
|---|------|------|------|
| **B1** | 冻结面守门在 CI 里**恒绿** | `git diff --name-only HEAD` 比较的是「工作树 vs HEAD」—— CEP commit 落地后 HEAD 已含改动 → diff 恒空;且 pathspec 相对 CWD 解析,ctest 在 `build/test` 下**空转**;且 `pclose` 返回值被丢弃,git 出错也判「未改动」 | 基线**钉死**为 CEP 起始 commit `4ae6bfc4`(可用 `CEP_BASELINE_COMMIT` 覆盖);改用 `git -C <root>`;`pclose` 非 0 → **抛异常**(fail-loud);ctest 加 `WORKING_DIRECTORY`;新增**机制自证**用例(用两个 ref 间的真实改动证明守门非恒绿) |
| **B2** | 一致性闸门(`deadlock_violations == 0 && memory_violations == 0`)在**三个工具里全部不生效** | 三个工具**只查 metric 键是否存在**、从不查**值** → 违例实验照样判成功并出报告 | 三工具同步加 `consistency_violations()` 闸门 + `metric_type_problems()` 类型/非负校验;`render()` 出 ❌ 横幅且**非零退出**;补 `memory_violations` 与 `memory_violation_kinds` 的**自洽性**校验 |
| **B3** | 「全仓无未登记的已定义 invariant」**名不副实**(只扫 15 份 owner ADR) | `all_defined_invariant_ids()` 只遍历 `owner_specs()`;6 份 non-owner ADR 完全不可见 → 往 ADR-CEP-17 注入一条 invariant,测试与 lint **双双全绿** | C++ 侧新增**全仓扫描** + **owner 语义**(ID 只有在其 owner ADR 内才算定义)+ **孤儿检查**(前缀无 owner = 契约违反)+ **NO_INV 反向检查**;lint 侧同步 |
| **B4** | 07 号 §4 的 **ID 区间列(第 3 列)完全未被校验** | 两个 gate 都只比「条数」→ 区间写错 / **组间互换 owner** 全绿(ADR-CEP-08 的 3 条 invariant 从此无人引用) | 解析第 3 列并展开为 ID 集合(支持全名区间 / 裸后缀区间 / 斜杠紧凑式),做**集合相等**断言;并加**独立锚点**:全部组引用的 ID 并集 == 实际定义总数且每个 ID 恰好被引用一次 |
| **B5** | tools 契约测试**从不执行任何 tool** | 测试名「可 --help」但 body 只有 `text.find(...)` → 往 `render_report.py` 塞语法错误,8 个 ctest **依然全绿** | 改为**行为断言**:真跑 `py_compile` / `--help` / `--check`;构造违例 / 类型非法 / 缺件套的实验目录并断言**退出码**;「零项目内依赖」改用 Python **ast** 真判定(原 grep 可被 `import os, cpptlm` / `__import__` / `importlib` 绕过);新增 4 个 ctest(`cep_tools_syntax` + 3× `cep_tool_help_*`) |
| **B6** | `ab_compare.py` 缺 `import html` → `unknown_direction` 非空时 **NameError 崩溃** | 局部变量 `html` 还**遮蔽**了模块名 | `import html` + 局部变量改名 `html_doc`;`unknown_direction` 改为**静态**求差集(否则「方向未定义但本次恰好无差异」的指标不会被标注) |

### 7.2 架构复审发现的关键项(已修)

| 项 | 问题 | 处置 |
|----|------|------|
| **解析器形态** | 只认 3 种定义形态 → 4 份 owner ADR 的**首定义位**实际用 P4/P5 写法;实测注入 `**不变量 \`INV-CEP-04-IC8\`**:` 到 ADR-CEP-04 → **7 case 全绿**,即新增 invariant 可**完全绕过**契约层 | 补 **P4**(加粗 span 内含 ID)与 **P5**(`### 决策 N:` 标题)两种形态;C++ 与 lint 两侧同步 |
| **ADR-CEP-NUMBER-MAP 编号重复** | §2 决策 1 表里 **CEP-07A/11/14/15/16 各登记 2 行**(旧 🟡 行 + 新 ✅ 行未删旧)→ 直接违反 `INV-CEP-NUM-IC1`「每个编号恰好一条表项」 | 折叠为每编号 **1 行**(gap 编号并入描述、依赖取**并集**)、按编号排序;补「编号唯一性」机器断言 |
| **文件名拼写错误** | `ADR-CEP-00-platform-charter-and-**goverance**.md`(少一个 'n')—— 全仓 17 处引用同错 | 重命名为 `...-**governance**.md` + 全仓引用同步 |
| **字段组口径 4 处不一致** | `ADR-CEP-12` IC1 写「6 组」却枚举 7 组;同 ADR 决策汇总写「8」;`ADR-CEP-17` 写「6」;spec 写「8」却枚举 7 组 —— **无人断言** | 统一为 **7 个字段组 / 13 个顶层属性**(= schema 实际属性数),4 处同步 + **机器断言**(含「ADR 不得再出现旧口径」) |
| **C10 零覆盖** | `ADR-CEP-14` 决策 4 明确把「组合接缝检查项清单存在」划给 **Stage 0** 并指派到 `test_cep_canonical_invariants.cc`,但全 `test/` 目录 grep「E 态」/「静默升级」**0 命中** | 补断言:「E 态静默升级」须在 ADR-CEP-08(组合必查项)与 ADR-CEP-13(跨域必查项)声明;「缺项 = fail / 不得静默跳过」规则须被声明 |
| **`ADR-CEP-05` 数字与枚举自相矛盾** | 「4 个**稳定**权限状态(`NONE/I/S/E/M`)」—— 5 个值写 4 个 | 改为「**5** 个」 |
| **`contains_word` 词边界** | `_` 被当**词字符** → `chi_rnf` / `tilelink_c` / `token_capable` 全逃过 C9 负向断言,而 schema 的 id pattern `^[a-z0-9][a-z0-9_-]*$` **允许**下划线(契约允许的形态恰好能绕过契约检查) | `_` 不算词字符;补**负向自测** |
| **`INV-CEP-07A-IC1` 未落地** | `ADR-CEP-07A` 决策 2 把「加 `[[deprecated]]` + 顶部注释」划给 **Stage 0**,但未做 | `include/core/coherence_domain.hh` / `src/core/coherence_domain.cc` 已加 `[[deprecated]]` + 指向 ADR-CEP-07A 的顶部注释(⚠️ 协议枚举本体按决策 4 保留,待 Stage 1) |
| **`ADR-CEP-07A` 产物 4** | 告警文案指向已 superseded 的 `ADR-X.14` | `src/core/topology_parser.cc` 改为指向 `ADR-CEP-13`(消费在 Stage 2)+ `ADR-CEP-07A`(处置) |
| **P6 禁引的反向语义** | lint 的 NEG 词表含「排除」→ 「**不**排除 P6」被判合规 → 假阴性 | 加 **REV 反向语义词表**(`不排除`/`未排除`/`不能排除`/`未必排除`),命中即**判违规**,优先于 NEG。⚠️ **不**收紧 NEG —— 词序有「排除 P6」/「P6 排除」/「排除** P6」多种形态,收紧会造成假阳性(实测 3 处) |

### 7.2.1 剩余 4 项弱断言的处置(第二轮)

复审标为 SHOULD_FIX 但**属实质弱断言**的 4 项,本轮一并处置:

| 项 | 原弱点 | 处置 |
|----|--------|------|
| **S3** | `network.mode` 否定语境判定是「±200/+120 **字符窗口**内出现任一否定词」—— 反向语义可骗过(「network.mode 与 simulation_mode 并列,均可使用(不得混淆二者)」含「不得」→ 绿,但契约已破);正向误报(距否定词 >200 字的合法否决提及 → 红);且**只扫 1 份 ADR**,spec:143 要求「机器 grep 可查」 | 改为**行级**判定(否定词须与 `network.mode` **同一行**);扫描面扩到 `include/ src/ configs/ tools/`,并断言代码/配置中命中数 == 0 |
| **S12** | `resolve_named_networks` 的 named_network 分支**写死 `resolved = false`** → 「能解析到唯一 vc_id」这条**从未被正向验证**,成功分支不可达 | 给 resolver 补 **fabric network → vc_id 映射**输入,成功路径真实可走并被**正向**断言;另补「解析到**多个** vc_id ⇒ fail」与「未提供映射 ⇒ fail」两条 |
| **S13** | 「status 只有 implemented 计入 adapter 计数」= 在枚举里数 `"implemented"` 出现 1 次 —— **恒真**,与 ADR 声称的计数口径毫无关系;spec:257 要求与 `07 号 §5.1` 口径**一致**,却**完全没查 07 号** | ① 把计数规则实现为**可测函数** `count_implemented()` 并用混合状态 fixture 断言;② 解析 `07 号 §5.1` 的两张表,断言实例行数 **6 + 8 = 14** 与声明数一致;③ 断言 14 是**下界**且 §5.1.4 的 5 个语义索引**不重复计 adapter**;④ 交叉核对 `MESI_Three_Level_HTM` 的 `excluded` 用法 |
| **S16** | P6 禁引是**单点黑名单** → 引用「P7」/「P1」**不红**;spec:369 要求的是**白名单**(P2.0/P2.1/P3/P3.5/P4/P5);扫描面只有 7 类 glob | 改为**白名单**规则:提取全部 `P<数字>` 引用,**「06 §15.4 引用」判定** = 小数形式(P2.0/P2.1/P3.5,优先级永不为小数)**或**同段出现 `15.4`;落在白名单外且无排除语境 ⇒ 违规。扫描面 38 → **56** 份文档 |

> ⚠️ **S16 的关键坑**:本仓 `P<数字>` 有**三种互不相干的词汇** ——
> ① 06 §15.4 阶段编号(契约管制对象)② 优先级 / rdd-planner 阶段(P0/P1/P2)③ 调研文档的**发现编号**(P1..P28)。
> 直接对全仓套白名单会误报 **59 处**;P6 的禁令同样必须限定「是不是 06 编号引用」——
> 故 Rule B 也加 06 引用标记(§11.4 / §15.4 / inter-module-network)限定,
> 但**反向语义**(「不排除 P6」)对**任何** P6 出现都生效(全局判违规)。
> 另:`docs/implementation/**` **不**纳入 P 编号扫描面 —— 实施笔记是「附属 2」层,
> 其内容是关于检查本身的**元讨论**,必然含 P6 字样。

### 7.3 反向验证(证明新检查**有牙**,不是摆设)

> **纪律**:新增/修改的机器检查必须**反向验证**能抓到问题 —— 故意破坏 → 必须红。

| 破坏方式 | 期望 | 实测 |
|---------|------|------|
| ADR-CEP-04 注入 P4 形态 `**不变量 \`INV-CEP-04-IC8\`**:` | C++ 守门红 | ✅ 2 case 红(此前 0 红) |
| ADR-CEP-17(no-inv)注入 `- **INV-CEP-17-IC1**:` | C++ + lint 双红 | ✅ C++ 1 case 红;lint 报「孤儿 invariant(前缀无 owner)」 |
| 07 号 §4 的 Ordering Point ID 区间改成 `INV-CEP-09-IC1 ~ IC3` | C++ + lint 双红 | ✅ C++ 1 case 红(集合不等);lint 报「并集与实际定义不一致:缺 ADR-CEP-08 的 3 条」 |
| 把 `ADR-CEP-12` 移出 lint 的 `OWNERS` | lint 红 + 连带报下游不一致 | ✅ `--strict exit=1` |
| 把 schema `protocol_version` pattern 改回有缺陷的 `^[a-z0-9]+(-[0-9.]+)?$` | 跨文档测试红 | ✅ 红(报「拒绝锁定表合法取值: chi-r2」) |
| `render_report.py` 追加语法错误 | tools 契约测试红 | ✅ 3 case 红(此前 0 红) |
| schema 注入 `chi_rnf_note` 属性 | C9 负向断言红 | ✅ 红(报「schema 出现协议标识符: CHI」) |
| ADR-CEP-18 注入「本 ADR **不排除** P6 的可能性」 | lint P6 禁引红 | ✅ 红(反向语义,全局生效) |
| ADR-CEP-16 注入「本 ADR 承接 `06 §15.4` 的 **P7** 条目」 | lint P 编号白名单红 | ✅ 红(`--strict exit=1`) |
| `07 号 §5.1.1` 加第 7 个工业实例行 | adapter 计数锚点红 | ✅ 红(报「§5.1.1 行数=7 §5.1.2 行数=8」) |

### 7.4 Task 8.4 验收前置结论(2026-10-21,§11 台账 Entry #7)

| Step | 机制 | 结论 |
|------|------|------|
| 2 | **M4 独立 AI 复审** | ✅ **2 轮**(`general` 架构 + `explore` 代码对抗式)—— 6 BLOCKER + 15 SHOULD_FIX + 架构复审关键项**全部处置**;10 组反向验证有牙。⚠️ `oracle`/`critic` 已从 agent 注册表消失 → 通道强度弱于原生 oracle,已如实登记 |
| 3 | **M1 产物冻结** | ✅ **28 个产物** SHA-256 冻结(`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`)+ 校验器 `scripts/test/cep_freeze_check.sh` + lint 第 7 组 + ctest。**反向验证**:改冻结 ADR → 红;恢复 → 绿 |
| 4 | **M2 跨仓准入门** | ⛔ **未放行** —— ArchForge 侧 coherence 接口 PR **未合入**(6 commit 无 PR 痕迹,只有设计文档)。`cpptlm-cep-umbrella` 降级为 **Proposed Draft**,baseline **1/3 → 0/3** |
| 5 | **R10 独立性残余** | ✅ Stage 0 验收等级 **「部分独立」** —— M1 ✅ / M2 ⛔ / M3 ⏳ / M4 ✅;**不得表述为「完全独立验收」** |
| 6 | **台账 entry** | ✅ `objective-cep-platform §11` **Entry #7**(append-only) |

> **⛔ 结论**:Stage 0 **不 archive** —— M2 准入门未放行 + R10 独立验收未闭合。

**M1 冻结范围的设计取舍**:冻结 **规范性契约产物**(21 ADR + `07 号` + 2 schema + 3 tools + umbrella `spec.md` = 28 件);
**两个 checker 脚本自身不冻结** —— checker 必须可修,否则一次 checker bug 就锁死整个体系;
且 checker 不承载契约语义,承载语义的是**被它检查的产物**。

### 7.4.1 M2 准入门:ArchForge 侧接口对齐清单(2026-10-21)

Task 8.4 Step 4 取证到「准入门未放行」后,进一步产出了**可执行前置**:
`docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`

| 段 | 内容 |
|----|------|
| §2 | **两侧接口面清单** —— ArchForge 11 项(A1~A11,带 file:line)+ CEP 9 项(C1~C9) |
| §3 | **Gap 矩阵 13 项** —— G1~G13,每项带两侧 file:line + 性质(冲突/缺失/口径)+ 级别(P0 6 / P1 4 / P2 4) |
| §4 | **ArchForge 侧 PR 范围** —— P0 5 项(放行 M2 的最小集)/ P1 4 项 / P2 4 项 |
| §5 | **需 DRI 向 ArchForge 澄清的 6 问** —— 两仓无法自行裁决者(Q1 状态归属 / Q2 抽象基类归属 / Q3 snoop callback 路径 / Q4 VC 权威方 / Q5 族边界 / Q6 perm 词表) |
| §6 | **M2 的 5 条可检查放行判据** —— C-1~C-5(其中 C-4/C-5 可自动化,C-1~C-3 依赖外部人工核对 = R10 的具体形态) |
| §7 | **漂移检测** —— `scripts/test/cep_archforge_alignment_check.sh`,32 个锚点(ArchForge 20 / CppTLM 12);已接入 lint **第 8 组** + ctest `cep_archforge_alignment_check`。ArchForge 仓缺席时**优雅跳过** |

**最重的 3 项 P0 冲突**:
- **G1** ArchForge 的 `CoherenceState { I,S,E,M,O,T }` 是**顶层协议特化枚举**,而 CEP 要求协议状态住 Plugin 内(`INV-CEP-05-IC1`)
- **G2** ArchForge 侧 `enum class Protocol { MESI, MOESI }` 仍是**配置入口**(`set_protocol`)—— 正是 CppTLM 侧已 `[[deprecated]]` 的同一形态(`INV-CEP-07A-IC1`)
- **G11** ArchForge 的 `register_cache_callback(CacheTLM*)` **直连** CacheTLM,而 `INV-CEP-15-IC3` 只允许经 adapter 交互

**反向验证**:ArchForge 侧符号改名 → 红;CppTLM 侧 invariant 改名 → 红;恢复 → 绿 ✅

### 7.5 复审的独立性说明

`oracle` / `critic` 两个专用 agent 在本环境**从注册表消失**(`Agent not found`),改用
`general`(架构复审)+ `explore`(代码对抗式复审)。两轮复审共抓出 **6 个 BLOCKER**
+ 约 15 项 SHOULD_FIX,均为作者 4 轮自审**全漏**的问题 —— 再次印证
`objective-cep-platform §8.1 R10` 的判断:**形式满足 ≠ 实质闭合**,M4(AI 复审)不能
替代真人独立验收。本轮处置已逐条落盘于本节,可追溯。

---

**版本**: v0.2
**最后更新**: 2026-10-21(v0.2:独立复审 6 BLOCKER + 架构复审关键项处置台账)
**维护者**: CppTLM Team(builder mode)
