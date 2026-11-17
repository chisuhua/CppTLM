# 审稿报告 — ADR-X.10 Status Update 追加

- **对象**：`docs/adr/ADR-X.10-parameter-framework.md` 追加的 `## Status Update` 段 + `docs/adr/README.md` + `docs/adr/ADR-X-SUMMARY.md` + `docs/architecture/06-inter-module-network.md` §9.4
- **日期**：2026-11-17
- **触发**：change `cpptlm-router-buffer-size-param` 归档后的残留任务 **T0.4**（复核 D-1 默认值裁决）与 **T6.1**（是否需要「参数声明-实际一致性」通用纪律 ADR）
- **审查序**：oracle（1 轮）→ critic（1 轮，arch 档）
- **预算**：critic 每 change ≤ 2 轮 ⇒ 本轮为第 1 轮；因 **0 BLOCKER**，**未触发复审**（§评审预算：只有 BLOCKER 触发返工与复审）

---

## 判定汇总

| 审查者 | 判定 | 关键结论 |
|---|---|---|
| **oracle** | **APPROVE** | ① T0.4 的 A 案（16 → 8）**成立且不依赖被订正的理由②**；② T6.1 裁定为「**不新造 ADR**」；③ metis 的「删 json 文件」路径**否决** |
| **critic**（arch 档） | **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · **0 BLOCKER** · `scope_exceeded: true` | 7 条 SHOULD_FIX（S1~S7）+ 4 条 NITS（N1~N4）+ 1 项 scope_exceeded 的范围外漂移 |

---

## oracle 的 3 处事实更正（我方原前提被推翻，已采纳）

| # | 原断言 | oracle 更正 | 复现 |
|:--:|---|---|---|
| C-1 | `configs/param_rules/*.json` 零代码加载 ⇒ 纯死文档 | **错** —— `cpptlm_config/validator.py:16-26` 的 `load_param_rules()` 加载它并强制 PARAM-01（`:263`）/ PARAM-02（`:281`）；标准测试路径 `scripts/test/test_off.sh:84` 跑 `pytest cpptlm_config/tests/` | `grep -n "load_param_rules" cpptlm_config/validator.py` |
| C-2 | 仓外存量 `buffer_size: 16` 会从 16 深静默变 8 深 | **方向反了** —— 改动前 `on_config_loaded` 根本不读 `buffer_size` ⇒ 显式 16 的**旧行为**是「参数被忽略、深度恒 8」；新行为才是深度 16 | `git show 708b13c8^:src/tlm/router_tlm.cc` 无 `buffer_size` 分支 |
| C-3 | C++ registry 是死声明面 | **不成立** —— `required` + `default_int` 是**活**的（`module_factory.cc:276-286`）；只有 `type`/`min`/`max` 面死 | `src/core/module_factory.cc:263-291` |

⇒ C-1 的连带后果：**「删除 json 文件」不是可选项**（删除会静默关闭生成期校验 + 打挂标准 pytest + 触发 AGENTS.md STRUCTURE lint 联动 + 与并行 NICTLM session 撞车 + 销毁「双声明面漂移」的活体样本）。

---

## critic 的 7 条 SHOULD_FIX —— 逐条处置（**全部已闭合**）

| # | 内容 | 处置 |
|:--:|---|---|
| **S1** | A 表把决策 4.6 登记为「✅ 已实施」**过宽** —— 该决策含三个成分，仅落地其一：① 校验位置（`set_config` → `validate_params`）未实施（全仓**无 `validate_params` 函数**，仅 `static validate_module_params` 零调用方）；② 异常类型（`std::invalid_argument` / `ParamValidationError`）未实施（实际抛 `std::runtime_error`；`include/core/param_errors.hh:11` 的 `ParamValidationError` 无任何 throw 点）；③ 回滚（`cleanup(); return false;`）未实施（`src/` 全仓无 `cleanup()`） | ✅ **已改** —— A 表决策 4.6 行改为「⚠️ **部分实施** —— 通路已就位，但三个成分仅落地其一」并逐项列出证据行号 |
| **S2** | A 表**漏登决策 1**（本 ADR 主决策）+ 未登记**字段级 schema 偏离**（正文 `:83-93` 的 `default_val`/`min_val`/`max_val`/`derive_expr`/`description` 字符串型 ≠ 实现 `param_rules.hh:31-42` 的 typed optional，**无 `derive_expr`/`description`**） | ✅ **已改** —— A 表**新增决策 1 行**（载体 ✅ / 存放位置部分偏离 / 字段级偏离）；C 段漂移清单补「另有字段级 schema 偏离 ⇒ 即便参数名对齐，字段仍不可互译」 |
| **S3** | C 段末句「以 registry 为 C++ 运行时唯一 rules 源是决策 4.5 的**忠实实例化**，不构成反转」与决策 4.5 正文 `:243`「两者共享同一数据源（JSON 配置或全局规则文件）」**存在落差** —— C++ registry 是**第三个**物化面 | ✅ **已改** —— 改写为「构成一处**已登记偏离**（证据 = 本段漂移清单）⇒ C++ registry 是第三个物化面;**归正方式待 F 段独立裁决**」。T6.1 裁定不受影响（依据仍是决策 1 缺点栏 `:57` 已把两面相等写成已接受义务 + `openspec/specs/param-derivation/spec.md:16` 已规定 `get_param_rules()` 机制） |
| **S4** | D 段① 括注「本 ADR 状态至今 `📋 待实施`」与新状态行 + A 表自相矛盾，且「Phase 3.3 从未实施」与 A 表冲突；② 「该文件当时只有 `vc_count`」与文件实际含 **5 条**规则不符 | ✅ **已改** —— ① 改为「Phase 3.3 的**参数框架升级（决策 1/4.5/5/6/7）从未整体实施**（其 plan 亦已归档）」；② 改为「该文件当时**无 `buffer_size` 条目** —— 实测当时有 5 条规则 `node_x`/`node_y`/`mesh_x`/`mesh_y`/`vc_count`」（`git show 708b13c8^:configs/param_rules/router_tlm.json` 实测确认） |
| **S5** | T0.4 与 T6.1 的**裁决本身未落盘** —— D/C 段只承载前提与理由，读者无法从 ADR 判定两个任务已收口 | ✅ **已改** —— Status Update 开头新增「**本段收口的两项裁决**」块（T0.4 = D-1 A 案成立 + 两处 errata;T6.1 = 不新造 ADR + 理由 + 载体分工见 G 段） |
| **S6** | C 段与 06 §9.4 把 `configs/param_rules/*.json` 描述为「活」声明面，但**唯一消费方 `cpptlm_config` 已标 Deprecated**（`cpptlm_config/AGENTS.md:4`，2026-06-17，已被 `cpptlm.topo` 等替代），继任者 `cpptlm/` 全仓**零** `param_rules` 引用 ⇒ F 段「不可逆后果」清单漏了这一项 | ✅ **已改** —— F 段补「**存续寿命另受一处约束**」段（须一并回答「生成期校验能力迁往何处」，否则该面可能被整体弃用而非迁移）；06 §9.4 表的「生成期声明面」行同步补该限定 |
| **S7** | `ADR-X-SUMMARY.md:5` 与 `:521` 两处粗粒度状态仍写「Phase 3.2/3.3 待实施」，与同文件 `:23` 的「🟡 部分实施」**同文件内部口径不一致** | ✅ **已改** —— 两处改为「Phase 3.2 待实施，**Phase 3.3 部分落地**（ADR-X.10 决策 4.6;其余待实施）」 |

## critic 的 4 条 NITS —— 处置

| # | 内容 | 处置 |
|:--:|---|---|
| **N1** | 「实测**全仓唯一注册方**」字面不成立 —— `test/test_d3_param_resolver_registry.cc:53` 亦调用 `registerParamRules`（幂等断言） | ✅ **已改** —— ADR-X.10 C 段与 06 §9.4 均改为「**生产代码中唯一注册方**;测试侧另有一处幂等调用 `test/test_d3_param_resolver_registry.cc:53`」 |
| **N2** | 06 §9.4「**06 P1 不动** — 现有机制足够」与新增「两面应当相等但当前无闸强制」并列易生误读 | ✅ **已改** —— 限定为「**06 P1 范围不动** — 对 §11.2 P1 范围（bundle 分类 / `simulation_mode` schema / seam locator）足够;⚠️ **不等于「参数声明面已受约束」**」 |
| **N3** | 状态编码不一致：ADR 头行同时挂「📋 待实施」与「部分实施」，而索引行用 🟡；🟡 在 ADR 语料已多义 | ✅ **已改** —— 头行改为**单一编码** `🟡 **部分实施**（2026-11-17 起：决策 4.6 通路已落地，4.5/5/6/7 未落地；Phase 3.3 计划整体未实施 —— 见 §Status Update）`;「Phase 3.3 计划整体未实施」下沉到 A 表语境。**注**：`docs/adr/README.md` 的状态图例已补 `🟡 部分实施` 一行并定义其含义（「以 ADR 文件 §Status Update 为准」）⇒ 编码在索引侧有定义 |
| **N4** | 06 §9.4 新增的 `file:line` 引用宜登记进文末「行号对照表」 | ⏭ **不适用（记录理由）** —— 该表（`06-inter-module-network.md` 文末附录）登记的是**行号漂移**（`module_factory.hh` / `chstream_module.hh` 因队列 #7 Phase 3 加内容导致旧引用语义漂移）。本次**未改**这两个头文件，新增引用（`module_factory.cc` / `router_tlm.cc` / `validator.py` / `test_d3_param_resolver_registry.cc`）是**首次引用**而非漂移登记 ⇒ 不触发该表的登记条件。critic 自述此为「按该惯例宜登记一次」的 soft 建议 |

---

## scope_exceeded（critic 判定：**true**）—— 记 backlog，**本轮不返工**

| # | 内容 | 处置 |
|:--:|---|---|
| SE-1 | `docs/adr/ADR-X.16-router-local-port-terminal-credit.md:113`（**正文**，不可变）仍写「`buffer_size` param 不生效 —— `get_param_rules` 声明了该参数（**默认 16**，范围 1–64），但 `BUFFER_DEPTH` 是 `constexpr 8`（`include/tlm/router_tlm.hh:134`）」⇒ 该断言**两处失效**（参数已生效;声明默认已为 8），且行号已漂移（`BUFFER_DEPTH` 现为 `router_tlm.hh:143`） | ⏭ **超出本轮 4 文件范围** ⇒ 按纪律**记 backlog，不进返工队列**。落点建议：给 ADR-X.16 **追加 `## Status Update`** 登记「该相邻独立缺陷已于 2026-11-17 由 change `cpptlm-router-buffer-size-param` 闭合」（正文不改）。**需由 planner/builder 侧登记**（architect 写集不含台账 `.rddf/roadmap/**`） |
| SE-2 | （同域观察，不单列 finding）ADR-X.10 正文「优先级排序」`:458`/`:464` 的 NICTLM `buffer_depth`（范围 1-64）在 `include/tlm/nic_tlm.hh` 中无对应声明 | ⏭ 既有正文问题 + 本次零 diff ⇒ 仅记录 |

---

## architect 域外 · 需交回 builder（**本次不可改**）

`src/tlm/router_tlm.cc:86-89` 的注释称「`instantiateAll` 会先跑 `validateConfig` → `validate_params`（已按 ParamRules 的 min/max 做 parse + 范围校验）⇒ 本分支的抛异常在标准装配路径上**不可达**」—— **三处事实错 + 一处主动风险**：

1. `validateConfig` **不调**校验器（其 param 校验器 `validate_module_params` 是零调用方死代码）；
2. **没有** `validate_params` 这个函数（全仓唯一出现处就是这条注释本身）;
3. rules 来自 **registry**，不是 json;
4. 「不可达」结论**相反** —— 该分支正是 AC-04（非法 `buffer_size` ⇒ 装配期 fail）的**唯一**失败通道 ⇒ 会被后人当死代码删掉。

⇒ 已登记在 ADR-X.10 Status Update **G 段**（architect 域内可登记的事实）;**代码修正交回 builder**（`src/**` 不在 architect 可写范围）。建议措辞：「⚠️ `validateConfig` **不做** param 校验（`validate_module_params` 为零调用方死代码，登记于 `.rddf/improvements/cpptlm-validate-params-dead-code.md`）⇒ 本分支是 `buffer_size` 非法值的**唯一**失败通道 ⇒ ⛔ 不可删」。

---

## 闸（返工后复跑）

| 闸 | 结果 |
|---|---|
| `scripts/test/cep_docs_sync_check.sh` | ✅ 全通过 |
| `scripts/test/cep_doc_ref_check.sh` | ✅ 全通过 |
| `scripts/test/agents_md_lint.py` | ✅ 结构 / 体量 / 行长全部合规 |
| `openspec validate --changes --strict` | ✅（本次未改 change） |

---

## 结论

**APPROVE_WITH_CONDITIONS 已闭合** —— critic 的 0 BLOCKER 无返工需求;7 条 SHOULD_FIX **全部闭合**;4 条 NITS 中 3 条闭合、N1 已改、N4 记理由不适用;1 项 scope_exceeded 记 backlog 不返工。**T0.4 与 T6.1 两项裁决均已落盘**（Status Update 开头的「本段收口的两项裁决」块）。
