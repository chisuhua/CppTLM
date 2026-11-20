# CEP 缺口计划 · Q-1~Q-11 追认裁决记录（architect · 2026-11-20）

> **性质**: architect 域的**追认裁决**（对 2026-11-20 缺口梳理产出的 11 个待裁问题逐项拍板）。⛔ **不构成新 ADR**、⛔ **不改任何已签批 ADR 正文**（`ADR-CEP-*` 不可变，只能追加 SU）、⛔ **新增 invariant 数 = 0**、⛔ **不重排 `B3b-2` 计数**（仍 `0/4 ⭔`）、⛔ **不新建架构文档**（Q-1 的词表落 `docs/guide/cep-usage.md` —— 该文件**已在** `arch_paths` 内且**不在** M1 冻结集）。
> **上游**: 缺口清单 + 立项计划 v4（`.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6）· 审查链 = **metis 1 轮 + oracle 1 轮 + critic 3 轮**（第 3 轮 **APPROVE_WITH_CONDITIONS**）⇒ 本记录是**收敛后**的产物。
> **授权**: 用户 2026-11-20 指令「对 Q-1~Q-11 切 architect 进行追认讨论和裁决」。

---

## §1 · 裁决总表

| # | 问题 | 裁决 | 落地形态 | 需 user 拍板 |
|:--:|---|---|---|:--:|
| **Q-1** | 「引擎 / 协议族引擎 / adapter 实例」三词边界 + B3b 分母 | ✅ **立三词定义 + 定分母读法**（§2.1） | `docs/guide/cep-usage.md` §12.2 **新增 §12.2.1 词表** | — |
| **Q-2** | Entry #232 已 ratify 的 B2 行措辞**是否需再改** | ✅ **不改**（§2.2） | 无写盘；偏离登记进 `cep-usage.md` §12.7 指针 | — |
| **Q-3** | 「DoD 12/12 ✅ ⇒ 平台完成」读法拦截点落点 | ✅ **落 `cep-usage.md` §12.7**（§2.3） | `cep-usage.md` §12.7 增一条读法限定 | — |
| **Q-4** | 3 条可建模 invariant 的判据定义 | ✅ **逐条给判据形态**（§2.4） | `.rddf/reviews/cep-invariant-batch1-criteria.md` | — |
| **Q-5** | `moesi_gpu` / `mesif` 13 键 golden 的 canonical 来源 | ✅ **仓内 golden 为唯一来源**（§2.5） | `test/cep_instance_golden.json`（新建，由 I-5 批 1 落） | — |
| **Q-6** | R 编号空间冻结 + 登记面收敛 + **P-5 判据形态** + 闸头转指 | ✅ **四项一次裁**（§2.6） | 判据形态 → `cep-stage-gate-p5-hardening`；编号 → 登记册 | — |
| **Q-7** | 「同一事实 5 处」canonical 化 | ✅ **定 canonical = 台账 §11**（§2.7） | 4 处改引用 | — |
| **Q-8** | D-1 验收债（40 份）规则化 + 补哪些 | ✅ **建议：规则化 + 补 0 份**（§2.8） | 规则写入登记册 | ⚠️ **是** |
| **Q-9** | OWNER 批 4 项的执行者 | ✅ **建议：全部改归 builder**（§2.9） | 登记册改分类 | ⚠️ **是** |
| **Q-10** | ArchForge 缺席时闸的处置 | ✅ **采方案甲**（§2.10） | 闸改「显式 SKIP + 告警」 | ⚠️ **是** |
| **Q-11** | E2 季度复审义务的责任方 / 调度落点 | ✅ **登记为季度义务 + 责任方**（§2.11） | 台账登记（planner 落） | — |

> **user 拍板项 = 3 项**（Q-8 / Q-9 / Q-10）—— 三者都是**取舍**（成本 / 责任 / CI 形态），非技术正确性 ⇒ architect 给**建议 + 理由**，最终由 user 定。**其余 8 项由 architect 直接裁决**，理由见 §2。

---

## §2 · 逐项裁决

### 2.1 Q-1 · 三词边界 + B3b 分母（**裁决：立定义**）

**问题**：仓内对「引擎」有两个并存计数 —— `docs/guide/cep-usage.md:42,:50` 写 **6**（engine 文件），`docs/architecture/07-coherence-exploration-platform.md:138` + `docs/architecture/cep/protocol-engine.md:44` 写 **5**（协议族引擎）。两者数的**不是同一物**，但都没定义「引擎」⇒ 读者无从判断哪个对。

**裁决 —— 三词定义**：

| 词 | 定义 | 取值域（2026-11-20 实测） |
|---|---|---|
| **adapter** | 状态**规约**的消费者（转移表 + `perm` 投影），**无运行期** | **8**（`include/tlm/cep/*_adapter.hh` 的 8 个协议 adapter；⛔ 不含 `adapter_contracts_loader.hh` / `adapter_schema_contracts.hh` 两个加载器）|
| **engine 文件** | `include/tlm/cep/*_protocol_engine.hh` 的**文件计数**（⛔ **含** Null 外壳）| **6**：`chi_rnf` · `cohif` · `mesif` · `moesi_gpu` · `tilelink` · `ualink` |
| **协议族引擎** | engine 文件中**承载该协议族状态机**者（⛔ **排除** Null 外壳）| **5**：`chi_rnf` · `mesif` · `moesi_gpu` · `tilelink` · `ualink` |

**裁决 —— Null 外壳的定性**：`include/tlm/cep/cohif_protocol_engine.hh:32-39` 的 `CohIfNullProtocolEngine` 是 **Null 可注册外壳**，**零新语义** ⇒ 它是 **engine 文件**，⛔ **不是协议族引擎**，且 ⛔ **不得**计入任何「协议语义已实现」的宣称（它在本仓的正当用途是**反向 fixture 的对照基线**）。

**裁决 —— B3b 分母读法**：仓内 4 处口径（`B3b-1 = 2/2` · `B3b-2 = 0/4` · 「B3b 达成度 1/6 → 2/6」· 终态「2 达成 / 分母 6」）**不是 4 套口径，而是 2 套口径的 2 种写法**：
- **分面口径（真源）** = `B3b-1`（可读规范族，2/2）与 `B3b-2`（受限族，0/4）**两条独立可判的判定面**（`obj §6.2` 的拆分依据 = 用户裁决 Entry #251 的「选项戊」）。
- **合并写法（派生）** = 「`2/6`」⇒ **6 = 2 + 4**，即「已达成族数 / 全族数」⇒ 它**恒等于** `B3b-1` 达成数 + `B3b-2` 达成数，**分母 6 = 2 + 4**。
⇒ **裁决**：**分面口径为唯一真源**；「2/6」是**允许的派生写法**，但**每次出现必须带分面分解**（`2/6 = B3b-1 2/2 + B3b-2 0/4`）⇒ ⛔ **裸「2/6」不得单独出现**（否则读者会把 `2/4` 与 `2/6` 混读）。**⛔ 不改写任何既有计数**（`B3b-2` 仍 `0/4 ⭔`）。

**落地形态**：`docs/guide/cep-usage.md` §12.2 新增 **§12.2.1「『引擎』三词定义 + B3b 分母读法」**（见 §3）。该文件**不在** M1 冻结集 ⇒ **不触五步链**。
**⛔ 本裁决不改 `07 号:138` 与 `protocol-engine.md:44`** —— 二者**在**冻结集（`docs/architecture/07-*.md` / `docs/architecture/cep/*.md`）⇒ 改它们需「台账 Entry 先落 → 产物 commit → `--update --authorized-by` → 清单单独 commit → 无参复验」五步链。⇒ **登记为待授权项**（§5.2）。

### 2.2 Q-2 · Entry #232 的 B2 行措辞（**裁决：不改**）

**事实**：`obj:529` 的 B2 行标题「66 条守门 invariant 逐条有牙载体 100% 覆盖」是 **Entry #232**（2026-11-05 [scope-change]）**显式授权的改判**（`obj:14978-15004`：「`§6.2 B2` 改为『逐条有牙载体 100% 覆盖』」· 性质「**改判**（⛔ 不是澄清）」· 角色 = architect 裁决 + 双审 / planner 落 entry）。`obj:530`（**同节下一行**）已给出「N 项有牙载体 + M 项授权⭕」登记形态，`obj:531` 已登记 `B2 ✅ 66/66（61 项有牙载体 + 5 项授权⭕）`。

**裁决 —— 三问逐条回答**：

| 问 | 回答 |
|---|---|
| ① Entry #232 已 ratify 的改判措辞**是否需再改**？ | **否** —— ① Entry #232 已完成「改判 + 双审 + 落 entry + 登记达成」的完整流程；② 争议点在**同节下一行**已由 `obj:530` 澄清；③ 闸的机器判据（`cep_invariant_map_check.py:18` 判据 ③「有牙载体 ∪ 授权⭕ == 66 集合」）与登记形态**自洽** ⇒ **措辞层无实质失真**，改动只会制造「已签批内容被反复翻案」|
| ② 改则是否需 user re-ratify？ | **不适用**（不改）。⚠️ 但**若将来**有人再改此措辞 ⇒ **须 user re-ratify**（`obj:988` + Entry #419 已 ratify 口径 ④）⇒ **本次裁决即为该禁令的登记** |
| ③ 回滚路径？ | **不适用**（不改 ⇒ 无回滚面）。⚠️ 若已有人改过 ⇒ 回滚 = 恢复 Entry #232 原文 + 台账追加一条订正 Entry（⛔ **不改历史 Entry 原文**）|

**裁决 —— 偏离登记**：`obj §6.2` 的 B2 行**保持逐字不动**；⛔ **不得**由 planner 原地改写（这是 §6.4 Q-2 的存在理由，而本裁决把它**关闭**为「不改」）。
**落地形态**：⛔ 无写盘（不动台账）；偏离登记进 `cep-usage.md` §12.7 的一条指针（§3.2）。

### 2.3 Q-3 · 「DoD 12/12 ✅ ⇒ 平台完成」读法拦截点（**裁决：落 §12.7**）

**事实**：`obj §0` 的 DoD **12 项全 ✅**（`obj:34-76`，第 7 项含在内）· `obj §6.2` 的 B3b-2 = `0/4 ⭔` + 终态 DoD Go **挂起**（`obj:552-554`）⇒ **B3b 不在 §0 的 12 项 DoD 列表内**（只在 §6.2）⇒ 「§0 全绿 ⇒ 平台完成」的读法路径**无拦截点**。⚠️ **已部分覆盖**：`obj:553`（挂起≠拒绝 + 解除条件）· `obj:18451`（N=0 永不 ✅）· `obj:34`（标题含「DoD, 2030-Q1 **终**」）。

**裁决 —— 落点 = `docs/guide/cep-usage.md` §12.7**（该节已有「DoD 读法（B3b-1 vs B3b-2 + ⭕ 登记语义 + 禁写 `✅(⭕)`）」⇒ **同一读法域** ⇒ 不新建节、不新建文档）。
**裁决 —— 拦截形态**：一条读法限定，明确「**§0 DoD 12 项全 ✅ ≠ 平台完成**」+ 指出终态 Go 挂起的真源（`obj §6.2` B3b-2 + Entry #264）。
**⛔ 不改 `obj:34-76` 的 12 个 ✅ token**、⛔ 不改 `obj §6.2` 的任何计数。

### 2.4 Q-4 · 3 条可建模 invariant 的判据（**裁决：逐条给形态**）

| id | 裁决 | 判据形态（builder 只实现，不得改语义） |
|---|---|---|
| **`CEN-Ib8`** | ✅ **复用档 2 已建的 `EventObservation`**（`include/tlm/cep/protocol_engine_base.hh`），⛔ **不新增基类面** | **可断言对象** = 「一次 `address_resolve` 的终结点**确实在 engine 内**」；**载体形态** = engine 的 egress 观测面 + 一条「终结点标记」字段；**判据写法** = 对每次 `address_resolve` 断言「终结点标记 == engine」且「⭕ 无外部终结残留」；⚠️ **负控**：故意让一个 `address_resolve` 在 engine 外终结 ⇒ 违例数 **精确 == 1** |
| **`INV-CEP-08-IC1`** | ⚠️ **降级为「单点非 vacuous 判据」+ 显式偏离登记**（⛔ **不**追求 J3 通用形式的静态可判定性） | **可断言对象** = 「在**一个已枚举的** Ordering Point 上，序列化**恰好发生一次**」；**载体形态** = `ordering_point_checker.hh` 的既有 checker + **枚举集合显式化**（把「通用形式」替换为**已登记的有限集合**）；**判据写法** = 对集合内每点断言「序列化计数 == 1」且「集合外**不**断言」（⇒ **诚实边界**：集合外的 Ordering Point **不被覆盖** ⇒ 须登记为**偏离**）。⚠️ **理由**：`ordering_point_checker.hh` 自身明写「通用形式不可静态判定」⇒ **通用形式在结构上不可判定** ⇒ 追求它是**推理填空**（与 `ADR-CEP-12` 的 T1–T4「非规范来源不构成 T1」同型纪律）|
| **`INV-CEP-09-IC2`** | ⚠️ **登记为「超出本仓范围」+ 给 Stage 4+ 移交口径**，⛔ **不在本仓引入最小仲裁器** | **可断言对象** = 「**当前仓内不存在**多 VC 仲裁器 ⇒ 该 invariant **无可断言对象**」；**载体形态** = 一条**存在性反断言**（`grep` 断言仓内**无**多 VC 仲裁实现 ⇒ 「未建模」这一事实**被机器钉住**，而非被忽略）；**判据写法** = 「仲裁器不存在」为**真值断言** + 「一旦出现 ⇒ 该断言转红 ⇒ 强制重评」⇒ ⚠️ **这不算达成**（`⭭-unmodelled` 仍登记），但它把「不可判」从**隐式**变成**显式且可机检**。⚠️ **理由**：`ADR-CEP-09` 自记「多 VC 仲裁属 Stage 4+」⇒ 在本仓建模它 = **提前锁定一个尚未裁定的仲裁策略**（策略选择属架构决策，本仓无依据）|

**⛔ 三条共同约束**：① **判据语义以本表为准**，builder **不得**自行发明；② 每条载体须满足 `cep_invariant_map_check.py` 判据 ⑤（邻域 ±8 行含断言宏）⇒ 「恒真占位」必红；③ `INV-CEP-09-IC2` 的形态**不得**被表述为「已达成」。

**落地形态**：本节即 Q-4 的裁决载体（判据文件 = 本记录 §2.4）⇒ `cep-invariant-modeling-batch1` 的前置命中判据（「裁决文件存在且 `grep -F` 命中三条 id」）**已满足**。

### 2.5 Q-5 · 13 键 golden 的 canonical 来源（**裁决：仓内 golden**）

**事实**：`moesi_gpu` / `mesif` **不在** `repo_owned_instance_ids`（6 个）⇒ 按 `ADR-CEP-00` 的仓归属表，二者的实现**在本仓** ⇒ 其 13 键的权威**也在本仓** ⇒ ⛔ **不存在「第二来源」**，只是**没有落盘的可对账锚**。

**裁决 —— canonical = 仓内 golden 文件**：`test/cep_instance_golden.json`（**新建**，由 `cep-invariant-modeling-batch1` 落地）—— 结构 = 实例 id → 13 键 → 权威取值（**从 `protocol_instances.json` 的已声明值派生**，⛔ **不得**另造值）。
**裁决 —— 双源禁令**：仓内 golden 是**派生副本**（真源 = `protocol_instances.json`）⇒ ⇒ **闸必须做双向比对**（沿用 `cep_decl_alignment_check.sh` 的双向对账范式）⇒ **单改一侧即红** ⇒ ⛔ **不得**把 golden 当独立真源。
**裁决 —— 落地归属**：落在 `cep-invariant-modeling-batch1`（该单元本就要改 `protocol_instances.json` 的相关面）⇒ ⛔ **不单独立项**。

### 2.6 Q-6 · 登记面收敛 + P-5 判据形态 + 闸头转指 + 编号空间（**裁决：四项一次定**）

**Q-6a · P-5 判据形态（→ `cep-stage-gate-p5-hardening`）**

| 项 | 裁决 |
|---|---|
| **锚定形态** | ✅ **措辞锚定 + 命中数计数**（⛔ **不用行号** —— 行号会随活口径行被编辑位移） |
| **锚点词表** | 每处活口径行给一个**唯一子串**：`当前真实 baseline = ` · `当前实测 **` · `当前 Proposed 并发 = ` · `实测基线 ` … `→ **` ⇒ 判据 = 「该文件的锚定子串**存在** ∧ 其后紧跟的 `N/3` == 实测」 |
| **期望处数** | **7**（6 文件 / 7 处；`nvlink-sharp.md` **2 处**）⇒ 判据 = 「命中处数 == **7**」⇒ ⛔ **不得**只锚 `:31` 漏 `:211`（critic SHOULD_FIX-1） |
| **豁免机制** | ⛔ **零豁免**（与 `config_type_registration_check.sh` 的「零豁免」先例同型）⇒ 但**允许**在锚点词表里显式登记「某文件**不承载** baseline 活口径」（如未来新增的纯历史文件）⇒ 登记即可见 |
| **`LIVE_FILES` 是否增补** | ✅ **不增补** —— 6 文件即全集；**新增活口径载体 ⇒ 必须同步进 `LIVE_FILES`**（这条义务写进锚点词表的注释） |
| **变异下限** | ✅ **6/6 全红**（采 critic SHOULD_FIX-1 的收紧建议：既锚 7 处，则 6 个文件的活口径行全改 `3/3` ⇒ 6 个文件**全部**须判红；下限写 **≥ 6/6**）|

**Q-6b · 登记面收敛（→ `cep-stage-gate-p5-hardening`）**
✅ **扩展既有** `scripts/test/rddf_ac_carrier.tsv` + `scripts/test/rddf_stage_gate.sh`；⛔ **不新建第三个登记面**（守 Entry #334 ④ 的否决理由 `obj:18622`）；⛔ **不新建闸脚本类型**（反 fixture 走既有 shadow 机制）。

**Q-6c · TSV `TODO` 行升级规则**
✅ 载体表内 **⛔ 零 `TODO` 行**为常态；`TODO` 行**必须**同时携带「升级时限 + 升级顺序」两个字段，且**最长存活 1 个工作日**（超过 ⇒ 判红）。**理由**：闸 `:189-192` 对 `TODO` 行是**跳过不计失败** ⇒ 长期 `TODO` = 机关无牙 ⇒ 与「顺序只由文风保证」是同一种病。

**Q-6d · 闸头引用转指**
✅ `scripts/test/rddf_stage_gate.sh:3` 的「per 册子 `obj-cep-platform-remaining.md` §1.5.6」⇒ 改为 **§6.5**（顺序机关节）⇒ **理由**：§1.5 整节已作废（三个 change 全部归档）⇒ 旧引用悬空。**归属**：`cep-stage-gate-p5-hardening`（该单元本就改这个闸）⇒ **不与 `cep-registry-stale-cleanup` 同改一文件**。

**Q-6e · R 编号空间**
✅ **旧 `R-*` 冻结为历史快照**（⛔ 不重编号 —— 重编号会断掉台账与各 improvement 的既有引用）；**新登记一律用语义前缀**：`SPEC-*`（契约面）· `ADR-*`（ADR 归因）· `PROC-*`（流程/治理）· `INV-*`（invariant）· `E2E-*`（证据/实验）。**本轮 §6 的 `S1-x`/`S2-x`/`S3-x` 编号**属**缺口梳理的临时序号**，⇒ **收口时须转为上述语义前缀**（登记册侧登记，收口时由 planner 执行）。

### 2.7 Q-7 · 「同一事实 5 处」canonical 化（**裁决：台账 §11 为源**）

**裁决 —— canonical = `obj-cep-platform.md` §11 台账**（append-only 的决策流水）⇒ 其余 4 处（structural `decision_adrs` / 快照 keys / `NUMBER-MAP` / `docs_sync ADR_LIST` / `OWNERS+NO_INV`）**全部改为引用**。
**裁决 —— 派生副本纪律**：4 处改为**机械生成**（⛔ 禁手写），生成器落 `scripts/cep/generate_doc_index.py`（**已存在**，per Entry #342）⇒ ⇒ **单改一侧即红**（沿用 `cep_decl_alignment_check.sh` 双向对账范式）。
**裁决 —— 归属**：由 `cep-registry-stale-cleanup` **同批**做（同一登记册/台账改写面 ⇒ 避免两单元同改一文件）；⚠️ 该单元若因此扩大 scope ⇒ 须回本记录追加裁决（**scope 变更须显式**）。

### 2.8 Q-8 · D-1 验收债（40 份）（**裁决建议：规则化 + 补 0 份**）

**事实**：CEP 归档 change **50** 份，其中 **40** 份缺独立验收报告（登记册记 38 ⇒ **口径未定义** —— 本次实测口径 = 「50 CEP 归档 − 10 能按名 join」）；全仓 164 份归档中 **143** 份缺。

**裁决建议 —— 两条**：
1. **规则化（向前）**：新 change **强制**独立验收报告（`.rddf/reviews/<change>-verify.md`）⇒ 写进 `AGENTS.md` 的提案门槛或 `cep-usage.md §12.6` 邻近段 ⇒ **不再追溯**。
2. **不追溯补 40 份**：理由 = ① **验收流程当时未强制**（先例同型的既有判例：`cpptlm-spec-purpose-batch*` 也是补「新近批次」而非全量）；② 全补 ≈ **12 d** 且**不产出任何能力**；③ 40 份中大部分的判据已在 `openspec/specs/` 的 main spec + 台账 Entry 里**留痕** ⇒ 补一份**独立 verify 报告**的边际信息量极低。
3. ⚠️ **例外**：**若**某份 change 的 main spec 存在**与实现不一致**的疑点（抽样发现）⇒ 该份**单独补**（⇒ 转为定向调查，⛔ 不是批量补债）。

**⚠️ 需 user 拍板**（成本取舍）：采此建议 ⇒ 40 份**永久不补**；改采「补高价值子集」⇒ 须先定义「高价值」的判据（⇒ 又一轮 user 裁决）⇒ architect **建议采「规则化 + 补 0 份 + 抽样发现疑点则定向补」**。

### 2.9 Q-9 · OWNER 批 4 项的执行者（**裁决建议：全部改归 builder**）

**事实**：登记册 §1.3 的 OWNER 批 8 项（本次实测 4 项仍有实际缺口：R-12 `abi_guards.h` 悬空引用 · R-13 spec 内旧状态行与新 Purpose 打架 · R-17 2 份来源 change 归档目录缺失 · R-19 `AGENTS.md` LWC handler 名 stale）标为「须各模块 owner 修」—— ⚠️ 但**本仓是单 agent 仓**，「owner」很可能**不存在** ⇒ 该分类 ⇒ **永久挂起**。

**裁决建议 —— 全部改归 builder**（理由：① 仓内无独立 owner 角色 ⇒ 「owner 修」是**不可执行**的分类；② 4 项的共同形态 = **文档/契约面小修**（悬空引用 / 旧状态行 / 路径 / 名字漂移）⇒ **无架构决策含量** ⇒ 不需要 architect 判权；③ 它们与 S3-6 / S3-7 同型 ⇒ **可并入 `cep-doc-hygiene-kpi-and-impl-state`** 一次做完）。
**⚠️ 需 user 拍板**：采此建议 ⇒ 登记册 §1.3 的 OWNER 分类**清空**（4 项转 builder）；不改 ⇒ 该 4 项继续挂起。

### 2.10 Q-10 · ArchForge 缺席时闸的处置（**裁决建议：方案甲**）

**事实**：`scripts/test/cep_archforge_alignment_check.sh` 在 ArchForge 缺席时 `exit 0` ⇒ CI 假绿；**本机 N/A**（`/workspace/main/ArchForge` 在位 ⇒ 真跑 7 项全绿）。

**裁决建议 —— 方案甲（显式 SKIP + 告警 + CI 可区分）**：
| 项 | 裁决 |
|---|---|
| 闸行为 | 缺席 ⇒ 打印**可 grep 的缺席标记**（如 `SKIP: ArchForge absent ⇒ cross-repo alignment NOT verified`）+ **显式告警**；⛔ **不判 fail**（本机与 CI 环境不同 ⇒ 一律判红会让 CI 永久红 ⇒ 噪声 ⇒ 教会团队无视红灯）|
| CI 侧 | **必须区分 SKIP 与 PASS** ⇒ SKIP 在 CI 报告里**单列**（⛔ 不算绿 ⇒ ⛔ 不算红）⇒ ⚠️ 若 CI 侧不做区分 ⇒ 本项**不得合并**（否则只是换一种假绿）|
| 反 fixture | 影子副本把 ArchForge 路径指向不存在目录 ⇒ 须产出**可 grep 的缺席标记**（当前实测：**无任何输出 + exit 0** ⇒ 不可察）|
| 7 项判据本身 | **零改动**（ArchForge 在位时 rc 逐字节相等）|

**⚠️ 需 user 拍板**：方案甲（建议）vs 方案乙（缺席即判红 + CI 须补 ArchForge checkout）⇒ **乙**在 CI 无跨仓 checkout 时会**永久红** ⇒ architect 建议甲。

### 2.11 Q-11 · E2 季度复审义务的责任方 / 调度落点（**裁决：登记为季度义务**）

**事实**：`obj:981` 的 **E2** = 「终态 DoD Go 的**季度复审**」，触发条件「2030-Q1 窗口未到期；**或** E1 命中时」，节奏「季度」；**E1**（D-4 ①/② 触发探测）**已有载体** —— `scripts/test/cep_b3b2_d4_quarterly_review.sh`（已存在、硬闸、**刻意 NOT 注册 ctest**，per Entry #277 §⑥②）⇒ ⇒ **E1 已机器化，E2 无载体**。

**裁决**：
| 项 | 裁决 |
|---|---|
| 义务性质 | ✅ **周期跟踪义务**，⛔ **不是实施轨**（⛔ **不得**进任何实施队列的 AC —— 外部项进实施队列 = 永久红）|
| 责任方 | ✅ **planner**（登记册维护方）+ **architect**（判定复核）—— 二者**均为仓内角色**，⛔ 无需外部 owner |
| 调度落点 | ✅ **台账 `obj §11` 的 Entry**（追加式）—— ⛔ **不新建脚本**（E1 的 cadence 脚本已覆盖「≤100 天未执行」的技术面；E2 是**判定复核**，不是第二个 cadence 探针 ⇒ **再建一个脚本 = 重复机制**）|
| 判据 | 季度复审时 **E2 必须有一条新的台账 Entry**（含：E1 结果 / 四个受限族的 T1–T4 状态 / `B3b-2` 计数是否改判）⇒ ⛔ 无 Entry ⇒ **该季度未复审** |
| 首次触发 | 下一个**季度复审窗口**；或 E1 命中时立即触发 |

**⚠️ 不得**把「脚本已存在」当完成判据（`cep_b3b2_d4_quarterly_review.sh` 覆盖的是 E1，不是 E2）—— 这是 §6.4 Q-11 的原意，**此处予以确认**。

---

## §3 · Q-1 / Q-3 的落地形态（`docs/guide/cep-usage.md`）

> 该文件**已在** `arch-docs.json` 的 `arch_paths` 内（2026-11-10 批 0.7 增补）⇒ architect 可写；且**不在** M1 冻结集（`cep_freeze_check.sh` 的 `FROZEN_GLOBS` 无此路径）⇒ **不触五步链**。
> **落点纪律**：⛔ **不改** `07 号` / `protocol-engine.md`（冻结面）；⛔ **不新建文档**；⛔ **不写「分级清单」**（`cep-usage.md:50` 的既有禁令**继续有效** —— 本裁决给的是**三词定义**（词义），⛔ **不是强度分级表**）。

### §3.1 §12.2 新增 §12.2.1（Q-1）

内容 = §2.1 的三词定义表 + Null 外壳定性 + B3b 分母读法（分面口径为真源 / 「2/6」须带分面分解）。

### §3.2 §12.7 增一条读法限定（Q-2 偏离登记 + Q-3 拦截）

内容 = 两条：
- **Q-3 拦截**：「§0 DoD 12 项全 ✅ **≠** 平台完成」+ 终态 Go 挂起的真源指针（`obj §6.2` B3b-2 · Entry #264）。
- **Q-2 偏离登记**：`obj §6.2` B2 行的「逐条有牙载体 100% 覆盖」措辞是 **Entry #232 授权的改判**（⛔ **不属契约失真**），其「N 有牙 + M 授权⭕」登记形态在**同节下一行** ⇒ ⛔ **不得**据此重开 DoD 7 口径改判。

---

## §4 · 需 user 拍板的 3 项（architect 已给建议）

| # | 选项 | architect 建议 | 理由（可判定） |
|:--:|---|---|---|
| **Q-8** | ① 规则化 + 补 **0** 份（+ 抽样疑点则定向补）② 补高价值子集 ③ 全补 40 份 | **①** | ① 验收流程当时未强制；② 全补 ≈ 12 d **零能力产出**；③ main spec + 台账 Entry 已留痕 ⇒ 独立 verify 报告的边际信息量极低 |
| **Q-9** | ① 全部改归 builder（并入 doc-hygiene 单元）② 保留 owner 分类 ③ 指定具名 owner | **①** | ① 本仓**无独立 owner 角色** ⇒ 「owner 修」**不可执行**；② 4 项共同形态 = 文档/契约面小修，**无架构决策含量**；③ 与 S3-6/S3-7 同型 ⇒ 可合并一次做完 |
| **Q-10** | ① 显式 SKIP + 告警 + CI 可区分（建议）② 缺席即判红 + CI 补 ArchForge checkout | **①** | ① ② 在 CI 无跨仓 checkout 时**永久红** ⇒ CI 噪声 ⇒ 教会团队无视红灯；③ 仓内先例（`rddf_stage_gate.sh` 的「skip + 告警，⛔ 不判 fail」）同型 |

---

## §5 · 边界与待授权项

### §5.1 本裁决**未做**的事

1. ⛔ **不改任何已签批 ADR 正文**（`ADR-CEP-*` 只追加 SU；本裁决**零** ADR 写盘）
2. ⛔ **不动 `07 号` 正文与既有 SU**（冻结面）
3. ⛔ **不改 `obj §6.2` B2 行**（Q-2 裁「不改」）
4. ⛔ **不重排 `B3b-2` 计数**（仍 `0/4 ⭔`）
5. ⛔ **不写「分级清单 / 强度表」**（`cep-usage.md:50` 禁令继续有效）
6. ⛔ **不新建架构文档**、⛔ **不新建登记面**、⛔ **不新建闸脚本类型**
7. ⛔ **不替 user 拍板** Q-8 / Q-9 / Q-10（给建议 + 理由）
8. ⛔ **不创建 OpenSpec change**（那是 builder / rdd-builder 域）

### §5.2 待**冻结面五步链**授权的写盘（本轮**不做**）

| 目标 | 内容 | 授权需求 |
|---|---|---|
| `docs/architecture/07-coherence-exploration-platform.md:138` | 分层图注「CXL 2.0 / CXL 3.0 对称 / NVLink **仅 adapter + state spec,无引擎**」⇒ 加「引擎口径见 `cep-usage.md §12.2.1`」指针 | 台账 Entry 先落 → 产物 commit → `--update --authorized-by` → 清单**单独 commit** → 无参复验 |
| `docs/architecture/cep/protocol-engine.md:44` | 同上（§5.1 的引擎清单注）⇒ 加同一指针 | 同上（**同一次** `--update`，⛔ 不分两次） |
| `docs/adr/ADR-CEP-20`（`:88` / `:126`） | 引擎列举处 ⇒ 加「协议族引擎 vs engine 文件」的区分指针（⛔ **原文不动**，只追加 SU） | 同上 |

⇒ **合并为一次五步链**（三个文件同批）⇒ 归属 `cep-registry-stale-cleanup`？⇒ ⛔ **不** —— 该单元的 AC-6 明写「`git diff --name-only` 不含 `docs/architecture/`」⇒ ⇒ **另立一个单元**（`cep-freeze-lexicon-pointers`，走五步链，⛔ 待 planner 立项）。

---

## §6 · 回滚路径

| 本裁决的产物 | 回滚方式 |
|---|---|
| `docs/guide/cep-usage.md` §12.2.1 / §12.7 两条 | 该文件**不在**冻结集 ⇒ ⛔ **原地回改**（`git revert` 该文件的对应 commit）⇒ 零冻结面影响 |
| Q-4 的三条判据形态 | 判据文件 = 本记录 §2.4 ⇒ 回滚 = 追加一条「判据形态 v2」的台账 Entry（⛔ **不改本记录原文**，append-only）⇒ builder 侧 `cep-invariant-modeling-batch1` 的实现按最新版判据 |
| Q-6 的四项裁决 | 同上（追加式订正 Entry）⇒ ⛔ **不改本记录原文** |
| Q-5 / Q-7 的 canonical 指向 | 同上 |
| **⛔ 未做**的冻结面写盘（§5.2）| 本轮**零写盘** ⇒ 无回滚面 |

---

## §7 · 关联

- 缺口清单 + 立项计划 v4：`.rddf/roadmap/objectives/obj-cep-platform-remaining.md` **§6**（§6.2 缺口清单 · §6.3 归属表 · §6.4 三队列 · §6.5 顺序机关 · §6.6 P-1~P-9）
- 8 份立项草稿：`.rddf/improvements/cep-registry-stale-cleanup.md` · `cep-stage-gate-p5-hardening.md` · `cep-invariant-modeling-batch1.md` · `cep-invariant-modeling-batch2.md` · `cep-tier2-second-family-tilelink.md` · `cep-doc-hygiene-kpi-and-impl-state.md` · `cep-openspec-version-pinning.md` · `cep-archforge-ci-failclosed.md`
- 既有决策面：`ADR-CEP-00`（仓归属 + 单向门）· `ADR-CEP-01`（B3b 分面）· `ADR-CEP-12`（实例声明面）· `ADR-CEP-17`（协议族定义）· `ADR-CEP-19`（CohIf 载体 + 桥）· `ADR-CEP-20`（档位阶梯 + 强度边界）· `ADR-CEP-06` SU#1（未采集契约）
- 闸面：`scripts/test/rddf_stage_gate.sh` · `scripts/test/rddf_ac_carrier.tsv` · `scripts/test/cep_invariant_map_check.py` · `scripts/test/cep_e2e_reconcile_check.sh` · `scripts/test/cep_b3b2_d4_quarterly_review.sh` · `scripts/test/cep_freeze_check.sh`
- 台账：`obj-cep-platform.md`（§0 DoD · §6.2 B2/B3b · §4.0-a 实例状态表 · §11 Entry）· 登记册 `obj-cep-platform-remaining.md`

---

## §8 · 复核订正（oracle 1 轮 · 2026-11-20 · **append-only**）

> **形态纪律**：⛔ **不改本记录 §1~§7 的任何原文**（按 §6 的回滚纪律：裁决变更 = 追加式订正）；本节**取代** §2.4 / §2.6a / §2.10 / §2.9 的对应内容，其余条款逐字有效。
> **触发**：oracle 对抗式复核 1 轮 ⇒ **REJECT**，4 项必修 + 3 项中影响 + 3 项低影响 ⇒ **全部为落盘级**（非方向返工）⇒ **本节逐条闭合**。

### §8.1 4 项必修

**M-1 · Q-4 `INV-CEP-08-IC1` 的「部分覆盖」偏离在现有机制下不可表达** ⇒ **改采方案（b）**

- **事实**：`scripts/test/cep_invariant_map_check.py:135` 的 disposition 词表**只有两态**（`⭭-unmodelled` / `⭭-pending`）；判据 ③（`:18` / `:152-154`）要求「有牙载体 ∩ ⭕ 登记 = ∅」⇒ **一旦加载体就必须移除 deferrals 登记** ⇒ 「集合外不覆盖」这个偏离**无处登记** ⇒ 裁决原形态会令 builder 卡壳。
- **裁决（取代 §2.4 的 IC1 行）**：
  - ✅ **判据形态** = 「在**一个已枚举的** Ordering Point 集合上，序列化**恰好发生一次**」（枚举集合显式化，替代「通用形式」）；载体 = 既有 `ordering_point_checker.hh` checker + 枚举集合；判据 = 集合内每点「序列化计数 == 1」且集合外**不**断言。
  - ✅ **偏离登记落点 = 载体断言的注释 + 台账 Entry 字面量** —— ⛔ **不进 `test/cep_invariant_deferrals.json`**（该文件只登记「**完全**无载体」的 id；加载体后必须移除登记）⇒ ⛔ **不改 `cep_invariant_map_check.py`**（⛔ **不引入 `⭭-partial` 第三态** —— 那会改闸的判据语义，属另一轮裁决）。
  - ⚠️ **诚实边界的可机判载体 = 台账 Entry 正文**（含「集合外不被覆盖」+ 枚举集合的完整清单）⇒ 验收时 `grep -F` 该 Entry 命中即成立 ⇒ ⇒ **偏离不是不可见**，只是**不在 deferrals 里**。
  - ⚠️ **该偏离使 `INV-CEP-08-IC1` 不构成「完全达成」** ⇒ 它**不得**被表述为「J3 通用形式已覆盖」。

**M-2 · Q-6a 锚点词表与磁盘实测不符** ⇒ **改为「文件 → 锚点」映射 + 「行内取值」口径**

- **事实**：原 §2.6a 给的是**全局词表**（「`当前实测 **`」用于 umbrella 面 / 「`实测基线 … → **`」用于 `AGENTS.md`」）—— 与磁盘实测**不符**：`cpptlm-cep-umbrella.md:306` 的实测措辞是「**当前真实 baseline = 0/3**」；`AGENTS.md:313` 的实测措辞是「`**实测基线** (2026-11-19, per …): **0 个 active** → **0/3**」⇒ 「其后**紧跟** `N/3`」在 `AGENTS.md` 上**不成立**（中间隔日期 + per 引用 + 「0 个 active」）⇒ 按错词表实现 ⇒ 闸对 umbrella **恒红** ⇒ I-0 永不能绿。
- **裁决（取代 §2.6a 的词表部分）** —— 锚点改为**逐文件映射**，取值口径改为「**锚点所在行内**的 `N/3`」：

| 文件 | 锚点子串（实测措辞）| 期望处数 |
|---|---|:--:|
| `AGENTS.md` | `实测基线`（行内含 `→ **N/3**`）| 1 |
| `.rddf/roadmap.md` | `当前真实 baseline = ` | 1 |
| `.rddf/roadmap/phases/phase-10.md` | `当前真实 baseline = ` | 1 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | `当前 Proposed 并发 = ` | 1 |
| `.rddf/improvements/cpptlm-cep-umbrella.md` | `当前真实 baseline = ` | 1 |
| `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` | `当前实测 **` | **2** |
| **合计** | | **7** |

  - 判据 = 「每文件命中处数 == 该行期望处数 ∧ **该行内**的 `N/3` == 实测 `active/3`」⇒ ⛔ **不用「紧跟」**（`AGENTS.md` 不成立）；⛔ **不用行号**（行号随活口径行被编辑位移 —— 维持 §2.6a 的原否决理由）。
  - **豁免**：⛔ 零豁免；允许在锚点表里显式登记「某文件**不承载** baseline 活口径」。
  - **`LIVE_FILES` 不增补**（6 文件即全集）；**新增活口径载体 ⇒ 必须同步进 `LIVE_FILES`**。
  - **变异下限 = 6/6 全红**（采 critic SHOULD_FIX-1）。

**M-3 · Q-10 的「假实测」订正**

- **事实**：§2.10 与 `cep-archforge-ci-failclosed.md` AC-1 写的「当前实测：**无任何输出 + exit 0**」是**错的** —— `scripts/test/cep_archforge_alignment_check.sh:30-34` **有输出**（「· 跳过:ArchForge 仓不在本环境($ARCHFORGE)—— 跨仓引用完整性无法在本仓校验」+「(设置 ARCHFORGE_DIR=<路径> 可启用)」+ `exit 0`）。**真实缺陷 = 无统一的 `[SKIP]` token**（对照 `scripts/test/cep_decl_alignment_check.sh:41` 的「· [SKIP] ArchForge 仓不在本环境…」—— 同仓已有 `[SKIP]` / `[RUN]` 的分token 范式，见其文件头 `:26-28`）。
- **裁决（取代 §2.10 的「当前实测」句与反 fixture 形态）**：
  - ✅ **缺陷表述订正为**：「缺席时**有**中文跳过提示，但**无 `[SKIP]` token** ⇒ 与「跑过」**不可区分**（机器侧）⇒ CI 无法分流」。
  - ✅ **反 fixture 判据** = 「输出含**可 grep 的 `[SKIP]`**」（⛔ **不是**「有任何输出」）⇒ 与 `cep_decl_alignment_check.sh:41` 的 token 形态**统一** ⇒ ⇒ 机器侧可分流。
  - ⚠️ **纪律登记**：本仓有「**虚假实测声明**」的 BLOCKER 先例（`obj:2778`）⇒ ⇒ **任何写进裁决/草稿的「实测」陈述，必须能由一条命令复现**；本条即为该纪律的第一次适用。

**M-4 · 裁决 → improvement 判据的**吸收义务**（系统性项）**

- **事实**：§2 的裁决（Q-5 / Q-6e / Q-7 / Q-9）**未回填**进归属 improvement 单元的 Acceptance ⇒ 下一轮 session 按 improvement 开工 ⇒ **裁决落空** ⇒ 这与登记册 D-2（Entry #442 §⑤·3）登记过的同类病（「口径/归属变化后活判据不同步」）同型。
- **裁决（新增义务）**：
  - ✅ **义务内容**：architect 的裁决一经落盘，**planner 必须把每条裁决的可机判判据逐条回填**进归属 improvement 单元的 `## Acceptance` 段（或在登记册 §6.3 归属表新增「**裁决吸收**」列并给每条一个机判命中判据）。
  - ✅ **时序**：**先于队列 2 的任何开工**（P-0 动作）⇒ 否则队列 2 的「前置命中判据」形同虚设。
  - ✅ **归属**：Q-5 → `cep-invariant-modeling-batch1` · Q-6e → `cep-stage-gate-p5-hardening` · Q-7 + Q-9 → `cep-registry-stale-cleanup` + `cep-doc-hygiene-kpi-and-impl-state`（Q-9 的并入面）· M-2 的锚点表 → `cep-stage-gate-p5-hardening` · M-3 → `cep-archforge-ci-failclosed` · M-1 → `cep-invariant-modeling-batch1`。

### §8.2 3 项中影响

**C-1 · Q-4 `CEN-Ib8` 的表达路径** ⇒ **不新增字段**
- **事实**：`include/tlm/cep/protocol_engine_base.hh:166-180` 的 `EventObservation` **字段表已定死**（per 裁决 R5）：`Kind{Transition, Fill}` + `Origin{LocalRequest, CompletionFill, RemoteSnoop, DirectInject}` + `sharers_snapshot` / `self_mask`（`std::uint64_t`）；⛔ **无 version 字段**。
- **裁决（取代 §2.4 的 CEN-Ib8 行的「终结点标记字段」）**：
  - ✅ **⛔ 不新增任何字段**（含「终结点标记字段」）⇒ 表达路径 = **既有 `Kind` / `Origin` 枚举的取值**：一次 `address_resolve` 的终结点若在 engine 内 ⇒ 该事件以 **`Kind = Transition`** + **`Origin = LocalRequest`** 被记录 ⇒ 判据 = 「**每一次 `address_resolve` 都被记录，且其 `Origin` ∈ `{LocalRequest, CompletionFill}`**（= 在 engine 内终结）」+ 负控 = 构造一个**不经 engine** 的 `address_resolve` ⇒ 该事务**在观测面中无对应记录** ⇒ 判红（违例数精确 == 1）。
  - ⚠️ **表达路径须写进判据文件**（本节即判据文件）⇒ ⛔ builder 不得自行 invention。

**C-2 · Q-4 `INV-CEP-09-IC2` 的 grep 模式必须写死**
- **事实**：仓内**确有** `ArbiterTLM2` / `ArbiterTLM4`（`include/chstream_register.hh:118-119`）⇒ 名字含 `Arbiter` 但**不是**多 VC 仲裁器 ⇒ 若判定模式含 `Arbiter` ⇒ **永恒误红**。
- **裁决（取代 §2.4 的 IC2 行的载体形态）**：
  - ✅ **判定模式写死** = 「`grep -rn "多 VC 仲裁\|vc_arbitrat\|VcArbitrat" include/ src/ test/` 命中数 == **0**」⇒ ⛔ **不得**用 `Arbiter` 作为模式词（该词在本仓被 `ArbiterTLM2/4` 占用）；⚠️ 措辞含中文 ⇒ 模式须同时给出**英文标识符面**（`vc_arbitrat*` / `VcArbitrat*`）以避开编码假设。
  - ✅ **反 fixture** = 影子副本中新增一个含 `vc_arbitrat` 的桩实现 ⇒ 该断言**必须转红** ⇒ 证明「一旦出现即强制重评」这条**不是恒真占位**。

**C-3 · Q-7 的否决理由补记**
- ✅ **补记**：曾评估以 `test/cep_adr_structural_deferrals.json` 的 `decision_adrs` 为 canonical —— **否决理由**：该文件**只覆盖 ADR 全集这一个事实**，而 Q-7 要统一的是「同一事实 5 处」中的**清单类**事实（覆盖面不等）⇒ 且它已被结构闸消费 ⇒ 再加一个 canonical 会**制造第二真源** ⇒ 维持「台账 §11 + 机械生成」方案。

### §8.3 3 项低影响

**L-1 · Q-9 的 scope 变更须显式登记** ⇒ ✅ **补记**：「OWNER 4 项并入 `cep-doc-hygiene-kpi-and-impl-state`」= **该单元的 scope 变更** ⇒ 按 §2.7 同款纪律（scope 变更须显式），**落地时须回填该 improvement 草稿的 In-Scope 段**，⛔ 不得只在本记录里裁。

**L-2 · Q-6c 的「TODO 行 1 个工作日」时限** ⇒ ✅ **补记**：该时限是**流程松紧取舍**（严格说属 user 域）⇒ architect 裁为默认值，**可被 user 修订** ⇒ ⛔ 不得表述为不可协商的技术约束。

**L-3 · §5.2 的 `cep-freeze-lexicon-pointers` 排期** ⇒ ✅ **裁决**：**插到队列 0 尾部**（P-1 之后）—— 理由 = 否则 Q-1 只解了**使用手册侧**，**冻结面的 5 vs 6 冲突仍会被新读者读到**（`07:138` + `protocol-engine.md:44` 仍在冻结集里保持旧措辞）⇒ 队列 0 尾部 = 零 architect 决策前置（只走五步链）+ 可与 P-1 同批。

### §8.4 oracle 对 Q-1/Q-2/Q-5/Q-11 的确认（逐字有效）

| 项 | oracle 结论 |
|---|---|
| **Q-1** | 三词定义与 `07:138` / `protocol-engine.md:44` / `cep-usage.md:42,50` 实测**一致** ✓ |
| **Q-2** | 「不改」三件套成立（Entry #232 完整授权链 `obj:14977-15004` + `obj:530` 同节澄清 + 闸判据自洽）✓ **不应当改** |
| **Q-5** | **不违反**「派生副本不得第二真源」（`mirror_note` + 双向比对范式正确复用）✓ |
| **Q-6b/c/d · Q-11** | 均与仓内先例一致 ✓ |
| **Q-8 / Q-10** | 标「需 user 拍板」**恰当** ✓ |
| **Q-9** | 标 user **偏保守** —— 「无 owner 角色」是可判事实、architect 可直判 ⇒ ✅ **据此收窄**：architect **直判**「4 项改归 builder」为技术面；**user 只需拍**「是否接受『OWNER 分类清空 + 历史补 0 份』这一后果」⇒ Q-9 的 user 决策面**收窄为后果确认**，⛔ 不再要求 user 裁技术归属 |

### §8.5 本节订正后的裁决有效性

- **§1 裁决总表的 11 条裁决方向全部有效**；**§2.4（Q-4 三条判据形态）· §2.6a（锚点词表）· §2.10（ArchForge 缺陷表述）· §2.9（Q-9 决策面宽度）** 四处按本节**取代**；其余逐字有效。
- **⛔ 未改任何已签批 ADR 正文** · ⛔ **未改** `07 号` / `protocol-engine.md`（冻结面）· ⛔ **未重排** `B3b-2`（仍 `0/4 ⭔`）· ⛔ **未新建**登记面 / 闸脚本类型 · ⛔ **未创建** OpenSpec change。
- **本节的落地动作**：M-1 / M-2 / M-3 / M-4 / C-1 / C-2 / C-3 / L-1 / L-2 / L-3 ⇒ **由 planner 回填进 8 份 improvement 草稿与登记册 §6.3 / §6.4**（planner 域）⇒ 回填后回本记录**复审**。

---

## §9 · 复审订正（critic 第 1 轮 · 2026-11-20 · **append-only**）

> **触发**：critic 审本裁决记录 + `cep-usage.md` 落盘 + 8 份草稿 ⇒ **REJECT**，**1 项 BLOCKER**（M-2 锚点表对 `obj-cep-platform.md` 的期望处数与磁盘实测不符 ⇒ 按字面实现则真实仓必红 ⇒ builder 被禁自行消歧 ⇒ 死锁）+ 3 项 SHOULD_FIX + 3 项 NITS。
> **形态**：⛔ **不改 §1~§8 的任何原文**（按 §6 的回滚纪律）⇒ 本节**取代** §8.1 M-2 的锚点表与 §8.2 C-1 的一处事实陈述；其余逐字有效。
> ⚠️ **本 BLOCKER 本身就是 M-3 刚登记的「虚假实测声明」同型病** —— M-2 为修「词表与磁盘实测不符」而设，而它自己的期望处数**未跑 `grep -c` 复验** ⇒ ⇒ **M-3 纪律（任何「实测」陈述必须能由一条命令复现）在本节首次适用于本记录自身**。

### §9.1 BLOCKER · M-2 锚点表订正（**取代 §8.1 M-2 的锚点表**）

**事实（复验命令与结果，2026-11-20 实测）**：

```
$ grep -c '当前 Proposed 并发 = ' .rddf/roadmap/objectives/obj-cep-platform.md   → 3
$ grep -c '\*\*当前 Proposed 并发 = ' .rddf/roadmap/objectives/obj-cep-platform.md → 1
```
3 处命中 = `:474`（§5.2 **活口径行**，真锚点）+ `:25815` / `:25967`（**§11 台账 Entry 历史流水**内反引号包裹的 `= 3/3` / `= 2/3`）⇒ 宽子串**不可用作锚点**。

**裁决 —— 锚点订正为逐文件的「精确锚」**（其余 5 文件的实测值**全部不变**）：

| 文件 | 锚点子串（**精确**） | 期望处数 | 复验命令 |
|---|---|:--:|---|
| `AGENTS.md` | `实测基线`（行内含 `→ **N/3**`）| 1 | `grep -c '实测基线' AGENTS.md` → 1 |
| `.rddf/roadmap.md` | `当前真实 baseline = ` | 1 | `grep -c '当前真实 baseline = ' .rddf/roadmap.md` → 1 |
| `.rddf/roadmap/phases/phase-10.md` | `当前真实 baseline = ` | 1 | `grep -c '当前真实 baseline = ' .rddf/roadmap/phases/phase-10.md` → 1 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | **`**当前 Proposed 并发 = `**（**粗体**）| 1 | `grep -c '\*\*当前 Proposed 并发 = ' …obj-cep-platform.md` → **1** |
| `.rddf/improvements/cpptlm-cep-umbrella.md` | `当前真实 baseline = ` | 1 | `grep -c '当前真实 baseline = ' …cpptlm-cep-umbrella.md` → 1 |
| `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` | `当前实测 **` | **2** | `grep -c '当前实测 \*\*' …cpptlm-cep-nvlink-sharp.md` → 2 |
| **合计** | | **7** | |

**裁决 —— 判据语义钉死为「联合命中」**（⛔ **不是**「先按子串计数、再逐命中行取值」两步）：
> **判据** = 对每个文件：**存在至少一处**命中该文件的**精确锚** ∧ **该锚所在行内**的 `N/3` == 实测 `active/3` ∧ **该精确锚的命中处数 == 期望处数**。
> ⇒ 「台账流水里的同形措辞」**既不计入处数、也不参与取值判定**（精确锚已把两者分开）⇒ ⇒ **无歧义** ⇒ builder 无需自行发明读法。

**裁决 —— 保留一条通用纪律**：**任何锚点子串在落盘前必须跑 `grep -c` 复验**；若同一措辞在文件内**多处**出现 ⇒ **必须选一个能唯一定位活口径行的精确形式**（本例 = 粗体标记）⇒ ⛔ 不得用宽子串。

### §9.2 3 项 SHOULD_FIX（逐条闭合）

| # | SHOULD_FIX | 裁决 |
|:--:|---|---|
| **SF-1** | §8.2 C-1 的 `Origin` 枚举列举**漏 `Writeback` / `BackInvalidate`**（实际 **6** 值，`protocol_engine_base.hh:177-184`）| ✅ **补全事实陈述** —— `Origin` = `LocalRequest` / `CompletionFill` / `RemoteSnoop` / **`Writeback`** / **`BackInvalidate`** / `DirectInject`（共 6）；⚠️ **判据本身不变**（CEN-Ib8 的判据仍取 `Origin ∈ {LocalRequest, CompletionFill}`）⇒ 仅**事实陈述**补全（per M-3 纪律） |
| **SF-2** | `cep-registry-stale-cleanup.md` AC-12 含「登记册 §1.3 的 OWNER 分类已清空或已转 builder」，但该单元 In-Scope 未列 §1.3 编辑动作 ⇒ 写入归属未钉死 | ✅ **归属钉死**：**登记册 §1.3 的 OWNER 分类重分类 = `cep-registry-stale-cleanup`**（它已改登记册 living 节，⛔ 避免两单元同改一文件）⇒ **4 项的具体文档修正 = `cep-doc-hygiene-kpi-and-impl-state`**（其 In-Scope 已逐项列出）⇒ 两单元**各改各的**，⛔ 不重复实施 |
| **SF-3** | `cep-doc-hygiene-kpi-and-impl-state.md` AC-2 的「或等价措辞」与 stage-gate AC-11 的锚点措辞**耦合风险** —— I-2 压缩 KPI 行时若改动锚点子串 ⇒ 打断 I-0 刚钉的锚 | ✅ **交叉引用约束**：I-2 压缩 KPI 行时，**6 文件 / 7 处活口径行的锚点措辞必须逐字保留**（以 stage-gate AC-11 的锚点表为准）⇒ ⛔ **只搬历史流水，不动锚点句本身**；⚠️ 该约束**因本裁决 §9.1 而更严**（`obj-cep-platform.md` 的锚点已升级为**粗体精确锚** ⇒ 台账侧动它风险更高 ⇒ 台账**不在** I-2 的 In-Scope 内 ⇒ 天然免疫） |

### §9.3 3 项 NITS（逐条闭合）

| # | NIT | 处置 |
|:--:|---|---|
| **N-1** | §8.1 M-1 引判据 ③ 位置写「`:18` / `:152-154`」，`:18` 未核到 | ✅ 订正为 **`cep_invariant_map_check.py:152-154`**（单处）—— `:18` 是该脚本**文件头注释**里对该判据的自然语言描述，⛔ **不是**可引用的实现行号 |
| **N-2** | §8.4 的 Q-9 行把「历史补 0 份」（属 **Q-8**）捆进 Q-9 的 user 决策面表述 | ✅ **拆分表述**（⛔ 不改 §8.4 原文，本节为准）：**Q-8** = 「40 份历史验收债**补 0 份**」的取舍；**Q-9** = 「OWNER 分类**清空**（4 项转 builder）」的后果确认 ⇒ ⇒ **两个独立的 user 门**，⛔ 不得读成单一拍板项 |
| **N-3** | `cep-archforge-ci-failclosed.md` What Changes #1「非零的**标记位**供 CI 读取」表述含糊 | ✅ **用词统一**为 `[SKIP]`（与 AC-1 的判据字面量一致，也与 `cep_decl_alignment_check.sh:41` 的既有 token 同形）⇒ 措辞统一后 ⛔ 「标记位」与「`[SKIP]`」不得混用 |

### §9.4 本节订正后的有效性

- **§1 裁决总表的 11 条裁决方向全部有效**；**§8.1 M-2 的锚点表**按 §9.1 取代；**§8.2 C-1 的枚举事实陈述**按 §9.2 SF-1 补全；其余逐字有效。
- **⛔ 仍未改任何已签批 ADR 正文** · ⛔ **仍未改** `07 号` / `protocol-engine.md`（冻结面）· ⛔ **仍未重排** `B3b-2`（`0/4 ⭔`）· ⛔ **仍未新建**登记面 / 闸脚本类型 · ⛔ **仍未创建** OpenSpec change。
- **落地动作**：§9.1（同步 `cep-stage-gate-p5-hardening` AC-11 一处）· §9.2 SF-1~SF-3 · §9.3 N-1~N-3 ⇒ **由 planner 回填**（planner 域）⇒ 回填后回本记录**定向复审**（只验：① 订正后 6 文件锚点计数 == 1/1/1/**1**/1/2 = **7** ② AC-11 与 §9.1 措辞一致 ③ 其余锚点计数不变）。

---

## §10 · 用户拍板记录与事实更正（2026-11-20 · **append-only**）

> **触发**：用户 2026-11-20 对 §4 的 3 项（Q-8 / Q-9 / Q-10）作出裁定。
> **形态**：⛔ **不改 §1~§9 任何原文**；本节**取代 §4 的「待拍板」状态**，并**更正 §2.9 / §8.4 的范围口径**。
> **性质**：本节记录的是**用户裁定**，⛔ 不是 architect 的新决策 —— Q-8 / Q-10 的技术内容已由 §8 / §9 审过；本节**唯一的新技术内容 = §10.2 的事实更正**。

### §10.1 三项拍板结果

| # | 用户裁定 | §4 的 architect 建议 | 一致性 |
|:--:|---|---|---|
| **Q-8** | **分层处置**：**乙层 5 份全补** · **甲层 35 份永久封存 + 抽检 5 份** · **废除「0.5 d / 38 份」**改「每份 **≥0.5 d 下限**」· **机关 = 独立闸** `scripts/test/archive_verify_check.sh`（封存边界 = 归档目录日期 **< 2026-11-12**；**先补后上闸**）| 「规则化（向前）+ 补 0 份 + 抽样疑点定向补」 | **方向一致，收紧了** —— 从「补 0」改为「乙层 5 份必补」，因复验发现其中 **3 份带着「未做验收」的 checkbox 就归档** |
| **Q-9** | **废除 OWNER 维度** + **8 项逐项终态**：A 类 4 项 → `cep-doc-hygiene-kpi-and-impl-state` · B 类 3 项 → `cep-spec-stale-refs-r14-r15-r20`（openspec change）· C 类 1 项 → **architect（Q-12）** | 「全部改归 builder」 | **方向一致，范围更正** —— 见 §10.2 |
| **Q-10** | **方案甲**：缺席打 **`[SKIP]`** + CMake `SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"` ⇒ ctest 记 **Skipped 而非 Passed**（⛔ 不 checkout ArchForge）| 同（§4 与 §8.1 M-3） | ✅ **完全一致** |

**§4 / §8.4 的「需 user 拍板」状态至此解除**（Q-8 / Q-9 / Q-10 三项均已裁定）。

### §10.2 事实更正：Q-9 的「4 项」是**窄检口径**，实测 8 项**全部仍 live**

⚠️ **这是本节唯一需要复审的技术内容** —— §2.9 与 §8.4 写「本次实测 **4 项**仍有实际缺口（R-12 / R-13 / R-17 / R-19）」**不完整**：复验显示 §1.3 登记的 **8 项全部仍成立**。

**复验命令与结果**（2026-11-20 逐条执行）：

```
R-12  ls include/cudart/abi_guards.h            → 不存在
      grep -n abi_guards.h openspec/specs/pcie-slice-prerequisites/spec.md → :14 命中
R-14  grep -n 19-pcie-ip-microarchitecture openspec/specs/pcie-ip-integration/spec.md → :13,:19 命中
R-15  grep -n 00-overview openspec/specs/v1-architecture/spec.md → :8,:20 命中
R-20  grep -n cpptlm_emulator_get_adapter_info openspec/specs/dgpu-board-adapter-info/spec.md → :10,:21 命中
R-18  grep -n doorbell openspec/specs/minimal-dgpu-soc-architecture/spec.md → :101「Requirement: doorbell 配置 vs 代码 hardcoded 不一致」
R-19  grep -n handle_cr_transaction AGENTS.md → :423 命中；实现为 register_doorbell_target/doorbell_write
```

⇒ **8 项全部 live**（R-13 / R-17 由前轮复验确认，本轮未重测）。

**这个更正改变了 Q-9 的落点形状**：8 项**不是同一类**——

| 类 | 项 | 落点 | 为什么不能合并 |
|:--:|---|---|---|
| **A 类 · 文档/路径** | R-12 · R-13 · R-17 · R-19 | `cep-doc-hygiene-kpi-and-impl-state` | 改 `.md` / 改 AGENTS.md 即可 |
| **B 类 · spec 契约面失效引用** | R-14 · R-15 · R-20 | `cep-spec-stale-refs-r14-r15-r20`（**openspec change**）| 落点在 `openspec/specs/*/spec.md` ⇒ **改契约**，须走 delta spec + 归档合并；⛔ 直接改文本会绕过契约变更流程 |
| **C 类 · 架构取舍** | **R-18** | **architect（Q-12）** | spec 里**明文写着**「配置 vs hardcoded 不一致」⇒ 修它要么改代码、要么改契约 ⇒ **取舍**，⛔ builder 不能单方面定 |

⇒ **§2.9 的「4 项改归 builder」在 B 类与 C 类上不成立**（前者是契约面、后者是取舍）⇒ 以本节三分裁定为准。

### §10.3 落盘形态（planner 已执行，2026-11-20）

| 落点 | 内容 |
|---|---|
| `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §1.1 | D-1 重写为 **40 份分两层** + 估时口径废除 + 封存边界 |
| 同上 §1.3 | **整节标废除** + 8 项终态表（A/B/C 逐项标落点）+ 「`needs: owner` 不得再用」纪律 |
| 同上 §6.2 | S2-8（Q-10 甲）· S3-5（Q-8 落实）· S3-8（Q-9 三分）状态更新 |
| 同上 §6.3.1 | **+3 行**（Q-8 / Q-9 / Q-10 拍板）|
| 同上 §6.4 队列 1 | **+Q-12**（R-18）|
| `.rddf/improvements/cep-d1-archive-verify-backfill.md` | 新建（Q-8，P0，10 条 AC）|
| `.rddf/improvements/cep-spec-stale-refs-r14-r15-r20.md` | 新建（B 类 3 项，P2，8 条 AC）|
| `.rddf/improvements/cep-archforge-ci-failclosed.md` | 待裁 → **已裁甲**；新增 **AC-1b**（ctest 层须 Skipped 而非 Passed）|
| `.rddf/improvements/cep-registry-stale-cleanup.md` AC-12 / `cep-doc-hygiene-kpi-and-impl-state.md` In-Scope | 按三分调整归属 |

### §10.4 衍生新问题：Q-12（R-18）

R-18 在 §2.9 / §8.4 中被归入「文档面小修」是**不对的** —— `minimal-dgpu-soc-architecture/spec.md:101` 的 Requirement **标题本身**就是「doorbell 配置 vs 代码 hardcoded 不一致」⇒ 这是**已知契约缺陷被写进契约**。

**Q-12 待裁二选一**：① 改代码对齐配置（doorbell 地址从配置读，取消 hardcoded）② 改契约承认 hardcoded（spec 改为「当前实现为 hardcoded，配置字段预留」）；并须给「是否动 `configs/dgpu_soc_minimal_v1.json:49`」的明确答复。

### §10.5 本节与 §2.9 / §4 / §8.4 的关系

- **§2.9（Q-9 建议）**：范围口径按 §10.2 更正（4 → 8 项，三类落点）；**「废除 OWNER 维度」方向不变**。
- **§4（3 项待 user 拍板）**：**状态解除** —— 三项均已裁定，见 §10.1。
- **§8.4（Q-9 的 user 决策面收窄为「后果确认」）**：用户已确认后果（含「未来若出现真 owner 需重建该维度」这一不可逆部分）。
- ⛔ **仍未改任何已签批 ADR 正文** · ⛔ **仍未改** `07 号` / `protocol-engine.md`（冻结面）· ⛔ **仍未重排** `B3b-2`（`0/4 ⭔`）· ⛔ **仍未新建**登记面 · ⛔ **仍未创建** OpenSpec change（B 类单元只**登记**，立项在 builder 域）。

### §10.6 新发现：闸的覆盖范围存在歧义，且牵出 3 份同类非 CEP 债务（**Q-13 待裁**）

**触发**：`cep-d1-archive-verify-backfill` 的闸判据初稿写「`openspec/changes/archive/<date>-*` 中日期 ≥ 2026-11-12 **且名字含 `cep`（或全部）**」—— **「或全部」是未消解的歧义**，builder 无权自行择一。

**实测（2026-11-20）**：

```
归档日期 >= 2026-11-12 的 change 总数 = 29
  含 'cep'  = 15  → 有报告 10 · 缺报告 5   （= D-1 乙层，用户已裁：全补）
  不含 'cep' = 14  → 有报告 11 · 缺报告 3   （⛔ 用户未裁）
```

**3 份非 CEP 缺报告的 change**（与乙层**同性质** —— 都在规则已形成期归档）：

| change | 归档日期 |
|---|---|
| `cpptlm-spec-scenario-level-fix` | 2026-11-15 |
| `cpptlm-agents-md-slimming` | 2026-11-19 |
| `cpptlm-subdir-agents-governance` | 2026-11-19 |

**两条路都有代价**：

| 闸范围 | 后果 | 代价 |
|---|---|---|
| **统一（全部归档）** | 规则无特判 ⇒ 无新漏洞；但闸上线时需同时补 **8 份**（5 CEP + 3 非 CEP）| 单元扩容 5 → 8 份，≈2~2.5 d → **≈3~4 d**；超出用户已裁范围 |
| **仅 CEP** | 与用户已裁范围一致；5 份补完即绿 | ⛔ **「非 CEP 归档不需要验收报告」成为明文特判** ⇒ 同一种病在非 CEP 面继续静默 |

⇒ **Q-13 待裁**（两项）：① 闸的覆盖范围 = **统一** 还是 **仅 CEP**；② 若选「仅 CEP」⇒ 那 3 份是否**另立**一个登记项（不排期 / 或同样补）。

⚠️ **architect 不建议「按名字白名单豁免」** —— 白名单本质是「把漏洞写进判据」，与 §10.2 查出的「窄检口径」是同一类错误。

⛔ **本节未擅自扩大用户已裁范围**：`cep-d1-archive-verify-backfill` 的闸判据保持「待 Q-13 裁定」，P0 执行（补 5 份）不受影响 —— **补 5 份可以先做，闸的范围等裁**。

### §10.7 Q-13 裁定与闸判据钉死（2026-11-20）

**用户裁定**：**统一覆盖 + 8 份全补**。⇒ 规则**不按 `cep` 前缀特判**（「非 CEP 归档不需要验收报告」这一特判被否决 —— 与 §10.2 查出的「窄检口径」是同一类错误）。

**闸的 join 规则钉死为精确匹配**（消解 §10.6 的歧义）：

```
report_path = ".rddf/reviews/" + strip_date_prefix(archive_dir_name) + "-verify.md"
判定 = os.path.exists(report_path)
```

**该规则的实测覆盖率（2026-11-20）**：归档日期 ≥ 2026-11-12 的 **29** 个 change 中，**21 命中 / 8 缺** —— 精确规则与既有 10 份报告的命名**100% 一致**（`cpptlm-cep-dod-reading-consistency-verify.md` · `cep-evidence-tier2-minimal-slice-verify.md` 等均为 `<name>-verify.md` 形态）。

**8 份待补清单**（闸上线时须全部存在，否则 CI 红）：

| # | change | 期望报告路径 |
|:--:|---|---|
| 1 | `2026-11-13-cep-evidence-tier-declaration` | `.rddf/reviews/cep-evidence-tier-declaration-verify.md` |
| 2 | `2026-11-13-cpptlm-cep-full-green-run-evidence` | `.rddf/reviews/cpptlm-cep-full-green-run-evidence-verify.md` |
| 3 | `2026-11-15-cpptlm-cep-adr-verification-carriers` | `.rddf/reviews/cpptlm-cep-adr-verification-carriers-verify.md` |
| 4 | `2026-11-15-cpptlm-cep-spec-strict-compliance` | `.rddf/reviews/cpptlm-cep-spec-strict-compliance-verify.md` |
| 5 | `2026-11-16-cpptlm-cep-mesh-e2e-carrier` | `.rddf/reviews/cpptlm-cep-mesh-e2e-carrier-verify.md` |
| 6 | `2026-11-15-cpptlm-spec-scenario-level-fix` | `.rddf/reviews/cpptlm-spec-scenario-level-fix-verify.md` |
| 7 | `2026-11-19-cpptlm-agents-md-slimming` | `.rddf/reviews/cpptlm-agents-md-slimming-verify.md` |
| 8 | `2026-11-19-cpptlm-subdir-agents-governance` | `.rddf/reviews/cpptlm-subdir-agents-governance-verify.md` |

**成本修订**：5 份 ≈2~2.5 d → **8 份 ≈3~4 d**（`cpptlm-d1-archive-verify-backfill` 的 scope 相应扩容，⛔ 已按「scope 变更须显式」登记）。

**封存边界不变**：归档日期 **< 2026-11-12** 的 35 份仍在白名单（不扫）；⚠️ 白名单是**一次性历史封存**，⛔ **不得**按名字逐项豁免（那是把漏洞写进判据）。

**命名纪律（新增）**：本闸建立后，**未来的归档 change 必须**产出 `<change-name>-verify.md`（精确名）；⛔ 不得用 `-verify-<date>.md` 等变体（既有 `cep-objective-archive-verify-2026-11-14.md` 这类历史命名仅适用于封存期，⛔ 不作新规范）。

### §10.8 闸强度补强：只查「存在」= 新的假绿口子

**问题**：Q-13 钉死的判据是 `os.path.exists(...)` ⇒ **0 字节文件也能过**。这会让「归档必须有验收报告」退化成「归档必须有一个名叫 `-verify.md` 的文件」—— 与 §10.2 查出的「窄检口径」同型。

**补强（architect 裁定，写入闸判据）**：闸对每份报告**同时**检查：

| 检查 | 判据 | 理由 |
|---|---|---|
| ① 存在 | `os.path.exists(report_path)` | 基础 |
| ② 非空且有结构 | 文件包含 **4 个必备小节标题**：`判据来源` · `逐条判定`（或 `判定`）· `已用轮次` · `遗留清扫`（⛔ 只要标题命中，不做语义判定）| 与「verifier 产出契约」的报告必含项对齐；⛔ 闸不判内容对错（那是 verifier 的职责），只判**报告形态成立** |
| ③ 非模板占位 | 文件**去空白后长度 ≥ 400 字符** | 拦截「复制粘贴空骨架」这种最廉价的假报告 |

**边界声明**：闸**不能**证明报告「判得对」—— 它只证明「报告存在且形态成立」。⚠️ **不得**因此把闸的绿读作「该 change 已验收合格」。这条边界须写进闸的文件头。

**由此导出的纪律**：`cep-d1-archive-verify-backfill` 的 8 份报告必须**先过形态判据再上闸**（顺序不变：先补 → 后上闸），否则闸会在自己的补债产物上转红。

### §10.9 oracle 闭合订正（2026-11-20 · append-only）

> **触发**：oracle 对 §10 增量判 **REJECT**（3 BLOCKER + 3 SHOULD_FIX）。**3 条 BLOCKER 全部为下游传播缺失，非 §10 事实错误**（oracle 独立复验：29/21/8 计数、8 项 live、R-18 spec:101、3 份带未勾 AC 归档 —— 全部属实）。本节逐条闭合；⛔ **不改 §10.1~§10.8 原文**。

#### §10.9.1 BLOCKER-1 · Q-13 未传播

- **事实（复验）**：登记册 `grep -c "Q-13"` = **0**；`cep-d1-archive-verify-backfill.md` 仍含「（或全部）」歧义（`grep -c "或全部"` = **1**）、仍写「先补 **5** 份」、成本未改。
- **裁定**：登记册 §6.3.1 +Q-13 行；该 improvement **全量同步**（scope 5→8 · 删「（或全部）」· 精确 join 规则 · AC 数量 · 成本 3~4 d · Impact 风险表）。**机械判据**：`grep -c "或全部" .rddf/improvements/cep-d1-archive-verify-backfill.md` == **0**。

#### §10.9.2 BLOCKER-2 · §10.8 的 4 标题集不可满足 + 与 AC 打架

- **事实（实测 21 份存量报告）**：

| 候选标题 | 命中率 |
|---|:--:|
| `判据来源` | **21/21** |
| `判定`（含 `逐条判定`）| **21/21** |
| `遗留`（含 `遗留清扫`）| **21/21** |
| `已用轮次`（含 `轮次`）| 20/21（`cpptlm-cep-flow-support-assertions-verify.md` 零命中）|
| `失败项` | 18/21 |

⇒ §10.8 原「4 标题」字面判定会让闸在**存量报告**上转红 ⇒「先补后上闸即绿」为假。

- **裁定（取代 §10.8 的判据②）** —— **闸 = 地板，AC = 交付标准**（两层，⛔ 不是两套并列契约）：

| 层 | 判据 | 依据 |
|---|---|---|
| **闸（地板 · 只证形态成立）** | `判据来源` ∧ （`判定` ∨ `逐条判定`）∧ （`遗留` ∨ `遗留清扫`）∧ 去空白长度 ≥ **400** | 实测 **21/21 全过**（含 8 份补验后的预期产物）⇒ 上闸即绿 |
| **AC（交付标准 · 高于闸）** | 4 段：`判据来源` / `逐条判定 + 工具证据` / `失败项与路由建议` / `已用轮次` | improvement AC-1 原文，⛔ 不改 |

⇒ 两个「四段契约」的张力消解为**层级关系**：闸不要求达到 AC 的强度，AC 也不因闸的宽松而降低。⛔ **不得**把闸绿读作「报告达到 AC 标准」。

- **⚠️ 400 字符阈的实测依据**（反驳「拍脑袋」）：扫面内最短报告 = **4546 B**（`cep-responder-bitmap-expansion-verify.md`）⇒ 400 字符阈有 **11 倍余量**，零误伤面；它是**地板不是目标**。

#### §10.9.3 BLOCKER-3 · R-13 分错类

- **事实（复验）**：
  - `openspec/specs/pcie-ip-microarch/spec.md:296` = 「**状态**: 📋 Spec — 等待评审 + Phase 1 启动实施」✔ 仍 live
  - `openspec/specs/pcie-ip-integration/spec.md:53`（末行）= 「**状态**: 📋 Spec — Phase 8 最终整合交付中」✔ 仍 live
  - 两处**都在 `openspec/specs/` 内** ⇒ 改它就是**改契约文本**
  - 而承接它的 A 类单元 `cep-doc-hygiene-kpi-and-impl-state.md` 的 **AC-6 明文** `git diff` **不含 `openspec/specs/`** ⇒ R-13 按原归属**物理不可执行**
- **裁定（更正 §10.2 的三分表）**：**R-13 由 A 类改归 B 类** ⇒ 随 `cep-spec-stale-refs-r14-r15-r20` 走 delta spec。
  - ⇒ **A 类 = 3 项**（R-12 · R-17 · R-19）· **B 类 = 4 项**（R-13 · R-14 · R-15 · R-20）· **C 类 = 1 项**（R-18）
  - ⚠️ 改正后 A/B 的边界判据 = 「**落点是否在 `openspec/specs/`**」，对全部 7 项成立（R-12 落在 spec 的**注释性引用** ⇒ 改的是引用方文档？❌ 不对 —— R-12 的引用处也在 `pcie-slice-prerequisites/spec.md:14`）

> ⚠️ **§10.9.3 的边界判据需二次修正**：R-12 的引用处同样是 spec（`pcie-slice-prerequisites/spec.md:14`）⇒ 按「落点是否在 `openspec/specs/`」它也应是 B 类。**R-12 与 R-13 同面** ⇒ 一并改归 B 类：
> ⇒ **A 类 = 2 项**（R-17 来源 change 归档目录 · R-19 `AGENTS.md` handler 名）· **B 类 = 5 项**（R-12 · R-13 · R-14 · R-15 · R-20）· **C 类 = 1 项**（R-18）⇒ 合计 **8** ✔

#### §10.9.4 SHOULD_FIX 逐条

| # | 问题 | 裁定 |
|:--:|---|---|
| **SF-1** | 补验报告的**执行角色与防自验**未钉 | ✅ **钉死**：写报告的是 **verifier 角色**（⛔ 不是 builder —— builder 写自己的验收 = 自证）；3 份带未勾 AC 归档的 change（`cpptlm-cep-adr-verification-carriers` / `cpptlm-cep-spec-strict-compliance` / `cpptlm-cep-mesh-e2e-carrier`）**必须非原实现者复验**；报告头强制标注「**原归档未验 · 本报告为 YYYY-MM-DD 追溯补验**」⇒ 该标注可 `grep` 机判 |
| **SF-2** | 闸对**畸形目录名**可能乱 strip | ✅ **fail-closed**：仅对**目录**（`STATUS_AUDIT_2026-06-15.md` 是文件，跳过）校验 `^\d{4}-\d{2}-\d{2}-`；不匹配 ⇒ **判红**（红因签名 = 「归档目录名缺日期前缀」），⛔ 不得静默 strip 11 字符。实测当前归档目录**全部合规**（唯一非目录项是那个 `.md`）|
| **SF-3** | `SKIP_REGULAR_EXPRESSION` 的 CMake 版本前提 | ✅ **补前提**：`CMakeLists.txt:6` 地板 = **3.16**，而 `SKIP_REGULAR_EXPRESSION` **恰为 3.16 引入**（零余量）⇒ 写入 `cep-archforge-ci-failclosed` 判据的前提行：CI 环境 CMake **必须 ≥ 3.16**；反 fixture 须能抓「删掉该 property ⇒ 回退 Passed」（AC-1b 已含后者）|
| **NIT** | 命名纪律未提「单日期前缀」 | ✅ 补：归档目录名**必须单日期前缀** `^\d{4}-\d{2}-\d{2}-`（既有双日期目录名先例 ⇒ 新目录不得沿用）|

#### §10.9.5 本节之后的 §10 有效性

- **§10.1 / §10.3 / §10.4 / §10.5 / §10.6 / §10.7 逐字有效**。
- **§10.2 的三分表**按 §10.9.3（含二次修正）取代：**A 类 2 项 · B 类 5 项 · C 类 1 项**。
- **§10.8 的判据②**按 §10.9.2 取代（3 标题 + 400 字符；⛔ 4 标题集作废）。
- ⛔ **仍未改任何已签批 ADR 正文** · ⛔ **仍未改** `07 号` / `protocol-engine.md` · ⛔ **仍未重排** `B3b-2`。
- **传播义务**（planner 域）：登记册 §1.1 / §1.3 / §6.3.1 · `cep-d1-archive-verify-backfill`（含 SF-1 / SF-2 / 3 标题集）· `cep-spec-stale-refs-r14-r15-r20`（4 → **5** 项，含 R-12 / R-13）· `cep-doc-hygiene-kpi-and-impl-state`（移出 R-12 / R-13，A 类只留 R-17 / R-19）· `cep-archforge-ci-failclosed`（+CMake 前提）。
