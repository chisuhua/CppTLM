# 审稿报告 — ADR-X.17（模块参数「读取即声明」机制）

- **对象**：`docs/adr/ADR-X.17-module-param-read-declares.md`（新增，v1.4）
- **日期**：2026-11-17
- **审查序**：oracle（2 轮）→ critic（2 轮）
- **预算**：oracle 每 ADR 1 轮 + 允许 1 次复审 ✅ 用尽;critic 每 change 2 轮 ✅ 用尽
- **⚠️ 预算状态**：critic 第 2 轮仍判 REJECT（1 项新 BLOCKER，见下）⇒ 已按其**可套用措辞**闭合 + 自查，但**未获第 3 轮确认**（预算用尽）⇒ ADR 状态保持 `Proposed`，待实施 change 立项时由 planner / verifier 复核。

---

## 判定轨迹

| 轮 | 审查者 | 判定 | 核心 finding |
|:--:|---|---|---|
| 1 | oracle | **REJECT** | 9 项 must-fix：D-1「结构性消除」被自举反例（`vc_count`）证伪;D-3 迁移顺序制造必填保护真空;Verification 三处判据不成立;Metadata 闭合范围未逐条列;F-1 事实错误（11+9=20 处读取面）;D-2 适用域判据错;关系表缺 `ADR-X.10` §G;「无 params 键也加载」有未声明副作用;示例片段形态不一致 |
| 1（复审） | oracle | **REJECT** | 9 项全闭合 ✅，但**本轮新增内容**引入 2 项 must-fix（同源：事实表漏了**第 3 条消费路径 = 板级直读 `dgpu_board_shell.cc`** ⇒ 把「某消费面不读」误判为「实现根本不读」）：**MF-1** `GmmuTLM.{tlb_size,tlb_miss_latency_cycles}` 不是零消费;**MF-2** 缺 F-15 + D-1/D-2/D-3 未覆盖板级路径 |
| 2 | critic（arch） | **REJECT** | **1 BLOCKER**：`ADR-X.11` 决策 7（CFG-08）未登记 —— 工厂侧 per-type 硬编码必填表是**第四个声明面**，且是 RouterTLM/NICTLM 必填的强制点;+ 8 SHOULD_FIX + 5 NITS |
| 2（复审） | critic（arch） | **REJECT** | BLOCKER 主体已闭合，但**新 BLOCKER B-1**：M4 若连 **NICTLM** 的 CFG-08 分支一起删 ⇒ NICTLM 的 `node_id` 必填**变成空窗**（NICTLM 无 `registerParamRules`，CFG-08 是其唯一强制点;`node_id_` 无初值）;且 3 条既有测试（`test_config_schema_validation.cc:122/138/219`）依赖该分支 ⇒ 与 M6「全绿」不可同时满足。+ 6 SHOULD_FIX + 5 NITS |

---

## 最终闭合状态（B-1 及其余）

### B-1（critic 第 2 轮 BLOCKER）—— **已按其可套用措辞闭合**

| 要求 | 落地 |
|---|---|
| M4 只删 **RouterTLM** 的 CFG-08 分支（`:304-305` 扁平回退 + `:311-319` 逐键检查） | ✅ M4 行已拆分 |
| ⛔ **NICTLM 分支（`:306-307` + `:320-326` + `:327-330`）不得在本 change 删除** | ✅ M4 行明文标注 + 说明「它必须在 NICTLM 迁移 change 内与 `load_param("node_id", …, default = NONE)` 一并删」+「删除前 NICTLM 仍属未迁移类型，受 V-3 ①/M6 零变化约束」+ 列出 3 条依赖测试 |
| `test_d3_param_resolver_registry.cc` 的处置 | ✅ 改为「删除或改写」并给出理由（`:52-59` 幂等断言在注册退役后必失败;`:62` 的 `operator[]` 插入默认项 ⇒ sentinel 断言空转假绿）—— 此前的「同步处理其对 `get_param_rules()` 的调用」描述不准确（该文件无直接调用） |

### SHOULD_FIX（critic 第 2 轮）—— 全部闭合

| # | 内容 | 处置 |
|:--:|---|---|
| **S-A** | F-16 锚点 `:478-481` 指向死代码（与 F-5 自述矛盾） | ✅ 改 `:311-326`（逐键检查）/ `:327-330`（无 `params` 源直接失败），并显式加注「锚点不取 `:478-481`」 |
| **S-B** | F-4「其余面无强制力」与「④ 唯一对必填有强制力」冲突（面 ② 对 RouterTLM 的 `required` 有强制力） | ✅ F-4 改为「**强制力分布**：② 对 RouterTLM 的 `required`（+ `default_int`）有强制力（仅当 `params` 存在，legacy 扁平形式绕过）· ④ 对 NICTLM 的 `node_id` 与两类型整数类型有强制力 ⇒ **必填强制点有两个（② 与 ④）**」 |
| **S-C** | `ADR-X.16` 行断言「**已**追加 errata」与事实不符 | ✅ 改「**待追加**」;并在本轮**实际执行**了该 errata 追加（见下「其他 ADR 修订」） |
| **S-D** | M0 只在 Metadata 定义，未进迁移计划表 ⇒ 无可勾选门 | ✅ 迁移计划表新增 **M0** 行（门 = diff 仅追加、§A~§G 零改动） |
| **S-E** | V-4 判据「配置键集 ⊆ 源码声明键集」对含板级直读键的类型字面不可满足 ⇒ 闸首日必红 | ✅ 改「⊆ **源码声明键集 ∪ 表 A 键集**」 |
| **S-F** | M4 对 `test_d3_param_resolver_registry.cc` 的描述不准 | ✅ 见 B-1 的测试处置一行 |

### NITS（critic 第 2 轮）—— 全部闭合

| # | 处置 |
|:--:|---|
| **N-a** `:125` 括注残留按键口径 | ✅ 改纯理由「理由：同键双真值时覆盖顺序决定行为」 |
| **N-b** R-2 仍用旧键类表述 | ✅ 与 D-2 的覆盖域表述对齐 |
| **N-c** 表 A 与 D-3 三态 ②/③ 关系未写明 | ✅ 表 A 补「**板级直读键仅适用于 D-3 三态 ②/③ 的类型**」 |
| **N-d** `param-validation` 未列 `:38-45`「Required parameter enforcement」 | ✅ 补列 + 指出「marked required」与「`default = NONE`」的术语差 |
| **N-e** Metadata M0 的 carrier 口径不清（2 个 carrier 对应 ⚠️ 行而非 ❌ 行） | ✅ 明确「❌ 行的未闭合项须各自开 4 个新单;2 个既有 carrier 对应 ⚠️ 行，M0 不得因此少开单」 |

---

## 其他 ADR 修订（"修订过期和冲突的其他 ADR"）

| ADR | 动作 | 性质 |
|---|---|---|
| `ADR-X.10-parameter-framework.md` | 在 `## Status Update` **末尾追加**子段，指向 ADR-X.17 并逐条列「部分取代 / 部分闭合 / 未闭合」 | **append-only**（§A~§G 既有内容零改动） |
| `ADR-X.16-router-local-port-terminal-credit.md` | 在 `## Status Update` **末尾追加** errata：`:113` 的「`buffer_size` param 不生效（默认 16）」已被 change `cpptlm-router-buffer-size-param` 闭合（声明 16→8、参数生效、非法值装配期 fail、行号已漂移 `:134`→`:143`） | **append-only**（正文不改） |
| `docs/adr/README.md` | 索引新增 ADR-X.17 行（`📝 Proposed`） | 索引登记 |
| `docs/adr/ADR-X-SUMMARY.md` | 表格新增 ADR-X.17 行 | 索引登记 |

> ⚠️ 既有遗留（**不在本 ADR 职责内，仅记录**）：`README.md:34-35` 的 `ADR-X.15` / `ADR-X.16` 指向 `docs-archived/adr/` 的**旧同名 ADR**，而 `docs/adr/` 下已有同号新 ADR —— 编号双份是历史遗留，需独立处置。

---

## 闸（收口后复跑）

| 闸 | 结果 |
|---|---|
| `scripts/test/cep_docs_sync_check.sh` | ✅ 全通过 |
| `scripts/test/cep_doc_ref_check.sh` | ✅ 全通过 |
| `scripts/test/agents_md_lint.py` | ✅ 合规 |

---

## 结论

- **oracle 2 轮**：9 项 + 2 项 must-fix 全部闭合并经其实测核对。
- **critic 2 轮**：第 1 轮 1 BLOCKER + 8 SHOULD_FIX + 5 NITS 全闭合;第 2 轮新增 1 BLOCKER（B-1）+ 6 SHOULD_FIX + 5 NITS —— **全部按其可套用措辞闭合并自查**。
- ⚠️ **未获第 3 轮确认**（critic 预算用尽）⇒ ADR 状态 **`Proposed`**，实施 change 立项时须由 planner / verifier 复核 B-1 的闭合（重点：NICTLM 分支的删除时机）。
