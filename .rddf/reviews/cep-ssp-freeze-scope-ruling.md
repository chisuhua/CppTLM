# 审稿报告：`ADR-CEP-24` SU 第四则（指针回填）+ 第五则（SSP 声明面载体纳入 M1 冻结集）

> **审稿对象**：`docs/adr/ADR-CEP-24-user-defined-ssp-declaration.md` 末尾两则 append-only SU（本文正文 §1~§9 与前 3 则 SU **逐字未变**）
> **审稿时间**：2026-11-27
> **审稿域**：项目内 `docs/adr/**`（⛔ 非 `~/.config/opencode/**` 治理面）
> **顺序**：oracle → critic（⛔ 不可颠倒）
> **预算**：本 ADR 用 oracle **1** 轮 + critic **1** 轮；⛔ **0 BLOCKER** ⇒ 无需复审
> **上游**：台账 `Entry #557 ⑥`（architect 域待裁项）

---

## §1 oracle（第 1 轮 · 判定 `APPROVE`）

判定行原文：`判定： APPROVE`

**核实结论**：裁决成立；事实底稿逐条属实，且**两处比底稿更强**。三个真正的缺口不在「纳不纳入」，而在 SU 文本的三处未闭合 —— 均可 append 修补，不推翻决策。

### findings 与处置

| # | finding | 处置 |
|:--:|---|---|
| 1 | **签批人能力**：schema 只表达 14 条 E 规则的一部分，却课以「必须人工核对」之责而不给映射表 ⇒ 「未工具化的流程」 | ✅ **已落附录 A**（`E1`~`E14` ↔ schema 21 条子规则覆盖表 · 12 ✅ / 9 ❌）。⛔ 未直接采信 oracle 给的锚点，**逐行自行核实并纠正 3 处**（`events` 空 `:85`→`:86` · `transition` 维度 `:88-96`→`:89-96` · 汇总计数由「约 7.5 / 14」改为精确的「21 条中 12 ✅ · 9 ❌」）|
| 2 | **边际条款缺失**：同目录 `protocol_instances.json` / `collective_planes.json` / `perm_upgrades.json` / `sources.json` 同样被闸消费却未冻结 ⇒ 本 SU 的原则可被引用到过强结论（「全冻」爆炸） | ✅ **已落 ④ ⓪ 边际条款**：区分「协议面登记（罕见 · 语义扩展）⇒ 冻」vs「实例面装配（例行实验）⇒ 不冻」，并把这四件 + 两 schema 显式登记为**后续裁决项**（③ 后续项清单 ②）|
| 3 | **残余绕行**：`kDefaultSspsPath` 住在未冻结的 `instance_assembly_loader.hh:38` ⇒ 冻结管文件不管**角色** | ✅ **已落 ④ 风险③** + ⑥ 增一条「⛔ 不宣称唯一真源性质已受保护」|
| 4 | **摩擦形态未写明**：`cep_freeze_check` 已注册 ctest ⇒ SSP 迭代中途 ctest 必红，第一个撞上的人会当 bug 报 | ✅ **已落 ④ 负面第 3 条**（已核实 `test/CMakeLists.txt:884-886`）|
| 5 | `cep-usage.md` 新文本仍硬编码「`exclusions` **0** 条」 | ❌ **不采纳**：这是 C-2′ 的 standing 口径（排除表当前为空是**设计态**，不是会腐化的计数）。改动会使表述偏离「glob 全量 + 显式排除表」的原话 |
| 6 | V-24-15 的 `len(artifacts)` 表述 | ✅ 无需改（后由 critic NIT④ 改为数据格式精确表述）|

### oracle 的 Q1~Q3 结论（存档）

- **Q1（会怎么坏）**：三个场景均不致命 —— ① 签批人能力「未工具化」而非「不存在」（已由附录 A 缓解）；② 原则证明力过强（已由边际条款缓解）；③ 角色与文件脱钩（已由 ④ 风险③ 点名）。loader fail-closed 使漂移的危害**有界**（用户按漂移 schema 写声明 ⇒ 装配期被 E 码拒，是用户可见的拒，不是静默腐坏）。
- **Q2（边界诚实性）**：SU ④ **显式承认**冻结 `ssps.json` 等价于给新增协议加审阅门（「摩擦即目的」），未掩饰。混合载体非对称（`mesif_ssp` 转写自未冻结的 `.hh`）已承认且方向论证成立：`.hh` 单改 ⇒ V-24-2 红；`ssps.json` 单改 ⇒ 冻结红；一致双改 ⇒ 已授权 ⇒ **无静默路径**。「只冻 schema + 数据走流程闸」不成立（流程闸无机器见证 —— 正是 Entry #56 B1' 要消灭的形态）。
- **Q3（是否为未来付费）**：**不是**。四条理由：① schema 件的暴露与 SSP 数量无关（今天就是用户可读契约）；② 晚冻 = 把**未经审阅的累积内容**铸成基线（provenance 已失）；③ 现存未闸面非零（事件名盲区）；④ 边际成本≈0（批 2 的 architect 注意已在发生，冻结只是**机器化**）。

---

## §2 critic（第 1 轮 · 判定 `APPROVE_WITH_CONDITIONS`）

判定行原文：`APPROVE_WITH_CONDITIONS` · `mergeable: yes` · `scope_exceeded: false`

**BLOCKERS：无**（arch 档四类逐项核对 —— 无与已签批 ADR 冲突 / 决策 6.2 未被改变 / 备选与取舍齐备 / 无伪决策）。

### SHOULD_FIX（2 条 · 均为**事实性错误**，已修）

| # | finding | 处置 |
|:--:|---|---|
| 1 | `事实 6` 的「V-24-2 **从不**比对事件名」对 TileLink 形态**不成立** —— `test_cep_ssp_runtime.cc:126-129` 逐字比对 `kEventNames`。全称命题应改为「盲区 = **索引形态孪生件（MESIF）+ 全部无孪生件 SSP**」 | ✅ **已修**（自行复核 `:126-129` 与 `test_cep_ssp_declaration.cc:98` 后收窄；连带收窄 ② 裁决表同一句）。⚠️ 该 finding 的价值在于：后续项 ③（事件名对照面是否加闸）会以事实 6 为设计输入 ⇒ 表述过宽会把闸的适用范围设计错 |
| 2 | `③ 案 C` 的「合同测试**不读**这两个文件」对 `ssps.json` **不成立** —— `test_cep_ssp_contract.cc:45-52` 确实加载它，C1（`:62-85`）逐事件断言 flow/anchor | ✅ **已修**（改为「读 `ssps.json` 但只断言**属性级**不变量、⛔ 不断言内容同一性；⛔ 不读 `ssp.schema.json`」）。案 C 的否决结论**仍成立**：保持 flow/anchor 合法的事件改名照样绿 |

### NITS（4 条 · 全处理）

| # | finding | 处置 |
|:--:|---|---|
| ① | 先例锚 `ADR-CEP-12:755-775` 指向下一则；指针回填 SU 实际在 `:749-753` | ✅ 已改为 `ADR-CEP-12:749-753`（已复核）|
| ② | V-24-17 缺载体命令 | ✅ 已补：`grep -n "include/tlm/cep/" scripts/test/cep_freeze_check.sh`（命中集 = 5 条纯文件名）|
| ③ | 附录 A 标题「一致性闸的**判据**底表」与表头「⛔ 不是判据」措辞相碰 | ✅ 标题改为「一致性闸的**输入**底表」|
| ④ | V-24-15 的「各有一条 `sha256`」与 manifest 数据格式不符（实为 `path → hex` 字典） | ✅ 已改为字典 membership 表述 + 可执行载体命令 |

### critic 的验证建议（存档）

- **审查记录落盘**：本文件即该要求 —— SU 头部引用的 `.rddf/reviews/cep-ssp-freeze-scope-ruling.md` 现已存在。
- **五步链后复验**（交 builder/verifier）：`cep_freeze_check.sh` rc=0 · `grep -n "ssp" scripts/test/cep_freeze_check.sh` 恰两条新增逐字枚举 · manifest `artifact_count` 98→100 且两件各有哈希条目 · V-24-16 判别力（改一字节未 `--update` ⇒ 必红）。
- critic 已自行复核：`cep_freeze_check.sh:56/57/65` · manifest `artifact_count=98` · `ssp_loader.hh:5-20` · `cep_ssp_check.sh:87/:89-104/:10` · `instance_assembly_gate.hh:238-240` · `ssp_interpreter.hh:61` · `instance_assembly_loader.hh:38` · `test_cep_ssp_declaration.cc:29` · `test/CMakeLists.txt:884-886` · **附录 A 全部 21 行 schema 锚点逐一命中** · 三份归档 change 路径存在 · 台账 `Entry #557 ⑥` 存在。

---

## §3 收口清单

- [x] oracle 1 轮（`APPROVE`）→ critic 1 轮（`APPROVE_WITH_CONDITIONS` · `mergeable: yes` · **0 BLOCKER**）
- [x] 2 条 SHOULD_FIX 全修（均经**自行复核**后修，⛔ 未直接采信审查者锚点）
- [x] 4 条 NIT 全处理；1 条 oracle 建议**明示不采纳**并给出理由
- [x] 附录 A 的 21 行 schema 锚点由 architect **逐行核实**（oracle 给的 3 处行号有误，已纠正）
- [x] 本报告落盘（ADR 元数据引用的指针存在）
- [x] 其余 CEP 闸复验：`cep_docs_sync_check.sh` / `cep_gate_selftest.sh` / `cep_adr_structural_check.py` / `cep_decision_block_check.py` 全 rc=0
- [ ] ⚠️ **`cep_freeze_check.sh` 当前 rc≠0（预期态）**：`ADR-CEP-24` **本身在 M1 冻结集内**（`ADR-CEP-*.md` glob）⇒ append SU 必然改 sha ⇒ 报 `1 被改`。**闭合路径 = 五步链**（planner Entry → 产物 commit → `--update --authorized-by` → 清单单独 commit → 无参复验 rc=0），⛔ 不在 architect 域内执行。

---

## §4 一句话结论

**裁决成立（`mergeable: yes` · 0 BLOCKER）**，两处事实性表述已按 critic 修正；**未实施** —— 冻结集的实际变更待五步链，⛔ 在此之前不得表述为「SSP 声明面已受 M1 保护」。
