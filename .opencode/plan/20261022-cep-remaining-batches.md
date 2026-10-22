# 执行计划：CEP 剩余任务批次（对外动作 · 批 2 · 批 3 · 收尾）

> **日期**: 2026-10-22 · **作者**: CppTLM CEP DRI（build agent, plan 模式）
> **状态**: 📋 **待 DRI 拍板** · **前置**: 批 1（契约收敛）已执行完毕并全绿,见台账 Entry #9/#10
> **上游计划**: [`20261022-cep-contract-convergence-batch1.md`](./20261022-cep-contract-convergence-batch1.md)（批 1,v2 + §9 实施记录）
> **回执**: `docs/implementation/2026-10-21-cep-archforge-response-receipt.md`

---

## 0. 一句话

批 1 之后剩 **6 组任务**:其中 **1 组可立即做且零决策成本**（对外答复 + 催 Q-3）、
**1 组是本季度第二次 adr-amendment**（A4 锁表 + 计数口径,**带 1 条 CI 锚点冲击 + 1 处 A6 耦合**）、
**1 组被跨仓阻塞**（A6 等 Q-3）、
**1 组被 ArchForge 阻塞**（A5 锚点重生成等 D-9 批 2）、
**1 组是流程动作**（批 1 的 commit）、
**1 组本仓无法闭合**（R10 真人外部评审）。
**关键结论**:对外动作（T1）能**顺带解锁**批 2 与批 3 —— 一次发函同时问 Q-3 与 A4 的排期口径。

---

## 1. 现状与依赖图

```
        ┌──────────────────────────────────────────────────────┐
        │  ArchForge 侧(外部)                                   │
        │  · Q-3: R-2 判据 (b) 评估   ← 2 轮未交付 ⚠️            │
        │  · D-9 批 2: 含全部 20 个锚点 ← 未合入                 │
        └───────────────┬──────────────────────┬───────────────┘
                        │ Q-3 答复              │ D-9 批 2 合入
                        ▼                      ▼
   ┌────────────────────────┐   ┌──────────────────────────┐
   │ T1 对外动作(可立即)     │   │ T4 A5 锚点重生成         │
   │  · A14 答复 R-6' 附带项 │   │  · 21 锚点重生成 + 复跑  │
   │  · A15 催 Q-3 + 问排期  │   │  · 必须与 D-9 批 2 同批   │
   └───────────┬────────────┘   └──────────────────────────┘
               │ Q-3 答复到达
               ▼
   ┌────────────────────────────────────────────────────────┐
   │ T2+T3 批 2/批 3(**合并**,因 ADR-CEP-05:227 耦合)          │
   │  T2 A4: ADR-CEP-11 §3 锁表 +2 行                        │
   │         + 07 号 §5.1.1 +2 行 + 计数 14→16              │
   │         + 8 处文本同步 + 1 条 CI 锚点更新                │
   │  T3 A6: ADR-CEP-05 §10 判据 2 状态收敛(需 Q-3 的 (b))    │
   └───────────────┬────────────────────────────────────────┘
                   │
                   ▼
   ┌────────────────────────────────────────────────────────┐
   │ T5 批 1 commit(可独立于 T1~T4 先行)                       │
   │ T6 本仓无法闭合项:R10 真人外部评审 / Stage 0 archive      │
   └────────────────────────────────────────────────────────┘
```

**任务清单**

| # | 任务 | 阻塞于 | 现在能做? |
|---|------|-------|:---------:|
| **T1** | 对外动作:A14 答复 R-6' 附带项 + A15 催 Q-3 | — | ✅ **可立即** |
| **T2** | A4 批 2:锁表 + §5.1 + 计数口径(**14 落点**,见 §3.1) | DRI 拍板(Q-3 到达与否) | 🟡 需决策 |
| **T3** | A6 批 3:`ADR-CEP-05 §10` 判据 2 状态收敛 | **Q-3** | 🏳 阻塞 |
| **T4** | A5:21 锚点重生成 | **ArchForge D-9 批 2** | 🏳 阻塞 |
| **T5** | 批 1 commit(A13) | 你确认 | ⏸ 待确认 |
| **T6** | R10 真人外部评审 → Stage 0 archive | 外部人 | 🔴 本仓不可闭合 |

---

## 2. T1 · 对外动作（零决策成本,立即可做）

### 2.1 A14 —— 正式答复 ArchForge R-6' 的 CppTLM 附带项

**背景**:ArchForge `R-6'` 的「CppTLM 附带项」要求本仓把
`noc_bundles_tlm.hh:60-62` 的 `CATEGORY_*` 从 3 类扩到覆盖 **6 类一致性消息类**。
我方裁定 **不扩**（理由见回执 §8.3 M-1:词表混层 + `vc_counts_[vc]` 分桶下标与 VC 0-3 值域冲突）。

**本仓已就位**:回执 §8.3 M-1 + `ADR-CEP-12 §8.3` IC2 ③ 的分层纪律注记 + 台账 Entry #9 ④。

**本批需补**:回执中新增 **§8.4「对 ArchForge 的正式答复」** 段 —— 用**答复/请求**表格把
「我方已定的」与「请对方确认的」分开,避免对方第三次只答一半。

| 我方答复(已定) | 请 ArchForge 确认 |
|----------------|------------------|
| 6 类消息类词表落在 **Coherence Interface 层**（`INV-CEP-12-IC2` 条件 ③),**不**扩 `CATEGORY_*` | 词表从 `32 号 §6.0.1` 引用即可,**不重复定义** —— 请确认无异议 |
| `CATEGORY_*` 保持**传输层 3 类**不变 | 请确认 `32 号` 侧对 `CATEGORY_*` 的引用处也补上「这是链路层类别」注记 |
| 二者关系由 **adapter 处的显式映射表**承担(非恒等) | 映射表归 **Stage 1** 交付 —— 请确认该归属 |
| 6 类词表**不另立 invariant 编号**(并入 IC2 ③) | 请确认不影响 ArchForge 侧对 `ADR-CEP-12` 的引用 |

### 2.2 A15 —— 催 Q-3（一次问完,避免第三轮半答）

**背景**:Q-3 = 「R-2 的判据 (b) 评估」,已 2 轮未交付。它阻塞 **T3(A6)**,
且 T2(A4) 的路线选择也取决于它。

> **Q-3 原文问题**:`ADR-CEP-13` `INV-CEP-12-IC2` 的域可见写判据是 **(a) ∧ (b)** 合取,
> 而 ArchForge 只评估了 (a) 就断言「已命中 `ADR-CEP-05 §10` 触发条件」。

**建议发出的 4 问**(具体到可直接回答):

| # | 问 | 为什么必须问 |
|---|-----|------------|
| **Q-3a** | 对 `moesi_gpu` 的 `E → M`:`E` / `M` 是否**接受**「导致数据值响应的转发请求」?<br>若是 —— 请给出该转发请求在 MOESI 下的**具体形态**;若否 —— 则该转换**不满足** (a)+(b) 合取 | (b) 的评估结论直接决定 `ADR-CEP-05 §10` 判据 2 是「已触发」还是「仍不触发」 |
| **Q-3b** | 若 (b) 不成立,ArchForge 是否**仍**要求 `perm` 加「可静默升级」判别位?<br>(即:需求本体是否**独立于**「已命中」这一论证) | 决定 R-2 是「需求仍成立但论证需改写」,还是「需求与论证一并重估」 |
| **Q-3c** | `ADR-CEP-05 §10` 判据 2 的状态由**哪一侧**标注?(ArchForge 侧写进 `ADR-SOC-25`,还是 CEP 侧写进 `ADR-CEP-05` 的 Status Update) | 避免两侧各写一份 → 又一处双真源 |
| **Q-3d** | ArchForge 贡献的 2 个协议实例(`moesi_gpu` / `mesif`)进入 `ADR-CEP-11 §3` 锁表与 `07 号 §5.1` 的**时点**,是否需要与判据 2 的收敛**同步**? | 直接决定 T2 的路线(见 §3 D1) |

> **Q-3d 是本批的关键一问** —— 它把「催 Q-3」从单纯的催办,升级为**一次能同时解锁 T2 与 T3 的问询**。

---

## 3. T2 · A4 批 2（R-13 锁表 + 族/实例计数口径）

### 3.1 落点清单（**15 项** = 12 处文本落在 7 份冻结产物 + 4 处非冻结,含 1 条 CI 锚点）

> ⚠️ **2026-10-22 复审后修订**:原列 14 项 → 补 3 处(落点 12b / 12c / 15)——
> 复审的 S7「旧口径残留面大于台账声称」暴露了 `ADR-CEP-12:39`、`tasks.md:107`、`research:1394` 三处漏网。

| # | 文件:行 | 改什么 | 冻结 |
|---|---------|--------|:----:|
| 1 | `docs/adr/ADR-CEP-11-protocol-version-locking.md` §3 表 | **+2 行**:`moesi_gpu` → `moesi-gpu-1.0`;`mesif` → `mesif-1.0` | 🔴 |
| 2 | `docs/architecture/07-coherence-exploration-platform.md` §5.1.1 表 | **+2 行**(`moesi_gpu`/`MoesiGpuAdapter`、`mesif`/`MesifAdapter`,状态 = 未排期) | 🔴 |
| 3 | 同上 §5.1.1 标题 | 「(5 族 / 6 实例)」→「(**6 族 / 8 实例**)」 | 🔴 |
| 4 | 同上 `:310` | 「2 族 = **14** 个 adapter 实例」→「**16** 个」 | 🔴 |
| 5 | 同上 `:74` | 头部对照表 14 → 16 | 🔴 |
| 6 | 同上 `:659` | 「**14** adapter 实例(工业 6 + Ruby 8;…落地后为 18)」→「**16**(工业 8 + Ruby 8;…**20**)」 | 🔴 |
| 7 | `docs/adr/ADR-CEP-00-...md:153` | 「2 来源族 / **14** adapter 实例(工业 **5 族 6 实例** + Ruby 1 族 8 实例)」→「16 / 工业 **6 族 8 实例**」 | 🔴 |
| 8 | `docs/adr/ADR-CEP-00A-...md:169` | 「**14** adapter 实例映射(工业 6 + Ruby 8;…18)」→「16(工业 8 + Ruby 8;…20)」 | 🔴 |
| 9 | `docs/adr/ADR-CEP-12-...md:147` | 决策 9 理由句「`07 号 §5.1` 的"**14** adapter 实例"是**下界**…落地后变 **18**」→「16」/「20」 | 🔴 |
| 10 | `docs/adr/ADR-CEP-18-...md:68` | 「§5.1 现为 **2 来源族 / 14 adapter 实例**(…18)」→「16」(…20) | 🔴 |
| 11 | `docs/adr/ADR-CEP-05-...md:227` | **见 §3.3 耦合** —— 必须加限定注记,否则当场变假 | 🔴 |
| 12 | `docs/architecture/README.md:37` | 索引行 14 → 16 | 🟢 |
| 12b | `docs/adr/ADR-CEP-12-...md:39` | 「计数会漂移」风险行内的「14 adapter 实例」→ 16 | 🔴 |
| 12c | `openspec/changes/cpptlm-cep-umbrella/tasks.md:107` | 「复核 §5.1 adapter 计数口径(2 来源族 / 14 实例…)」→ 16 | 🟢 |
| 13 | `test/test_cep_schema.cc:417` | 硬编码的 6 个锁表取值 → **补 2 个** | 🟢 |
| 14 | `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md` 锚点块 | `07 号:310` 的 symbol `'14 个 adapter 实例'` → `'16 个 adapter 实例'` | 🟢 |
| 15 | `docs/research/cep-coherence-protocol-space-survey.md:1394`(N3 条) | 附属/历史层 → 加「计数已于 2026-10-22 改为 16」的注记,**不改写历史结论** | 🟢 |

> **🔴 CI 锚点冲击(本轮新发现)**:第 14 项 —— `cep_archforge_alignment_check.sh` 的
> **CppTLM 侧 12 个锚点**里有一条指向 `07 号:310`、symbol = `14 个 adapter 实例`。
> 改计数 → 该锚点**当场失效** → `cep_archforge_alignment_check` 红 → lint 第 8 组红。
> **与 D-9 的 20 条 ArchForge 锚点同类,但发生在本仓侧** —— A4 落地**必须**同步改锚点块。
> （ArchForge 侧 21 个锚点不受影响 —— A4 全程不动 ArchForge 仓。）

**机器强制**:`test/test_cep_schema.cc:564` 会数 §5.1.1/§5.1.2 的**表格行数**与声明数比对
→ 改了表格忘改计数就红;`test_cep_schema.cc:417` 则是**硬编码** 6 个锁表取值
→ 忘了补 `moesi-gpu-1.0` / `mesif-1.0` **不会红,但会静默失去覆盖**(silent gap)→ 必须手工补。

### 3.2 连带影响：KPI 排期口径？

`ADR-CEP-00 §2 决策 3` 的 **10-change 串行队列**里排了 6 个协议实例
(`chi-rnf-subset` / `cxl-nonsymmetric` / `tilelink-tree-manager` / `cxl-dcoh-symmetric` / `nvlink-sharp` / `ualink-inc`)
—— 即 **14 个注册实例中只排了 6 个**。

→ A4 若**只改注册表**(`07 号 §5.1` + 锁表),则 16 个实例里 8 个未排期,现状语义不变(可接受);
→ A4 若**同时排期**,则要新增 2 个 change → **10 → 12 个 change / 12 季度**,
连带 `ADR-CEP-00` 决策 3 + objective DoD + `phase-10` 交付预期。

→ **这是必须由 DRI 拍板的范围问题**(见 §4 D2),**不**在本批默认范围内。

### 3.3 🔴 A4 ↔ A6 耦合：ADR-CEP-05:227 会当场变假

`ADR-CEP-05:227` 现行文本:

> **当前不触发**:§5.1.1 / §5.1.2 的 **14 个** adapter 实例**均无**「静默 E→M」需求(工业协议族为显式请求升级)

而 `moesi_gpu` 恰恰**就是**那个有静默 E→M 的实例(`state_transition[2][1]=3`,E+Write→M,ArchForge 自认)。

→ **A4 一旦把 `moesi_gpu` 登记进 §5.1.1,这句话当场变假** —— A4 单独做会
**在 ADR-CEP-05 里制造一个新的活矛盾**(正是批 1 刚清掉的那类问题)。
→ 而收敛这条判据正是 **T3(A6)** 的动作,但 T3 被 Q-3 阻塞。

**三种处置**(见 §4 D1):
- **路线 A**:等 Q-3 → A4 + A6 同批,彻底自洽;代价 = ArchForge 声明在此期间仍**无法通过装配期校验**
  (`INV-CEP-11-IC1`:表外取值 = fail)
- **路线 B(推荐)**:**A4 + ADR-CEP-05:227 加限定注记**（「当前 14 个实例均无…」→「除 `moesi_gpu` 外,当前已登记实例均无…」)
  + 把 A6 登记为 **Stage 1 前置** → 立刻解锁装配期校验,债**显式化**而非消除
- **路线 C**:A4 只改锁表、不改 §5.1.1 表格 → ❌ 不可行:违反 `ADR-CEP-11 §3` 使用规则 2
  (「表中不含任何未列入 §5.1 的协议族」),且 §5.1.1 标题与计数会自相矛盾

---

## 4. 决策点（需 DRI 拍板）

| # | 决策 | 选项 | 我的推荐 |
|---|------|------|---------|
| **D1** | **A4 的路线** | A:等 Q-3 再做(A4+A6 同批)<br>**B:A4 + `ADR-CEP-05:227` 限定注记 + A6 登记为 Stage 1 前置**<br>C:只改锁表(不可行) | **B** —— 立刻解锁 ArchForge 声明的装配期校验;债以「限定注记 + Stage 1 前置」**显式**留在契约里,而不是靠「不登记」来回避 |
| **D2** | **A4 是否同时排期** | 仅改注册表(14→16,排期不变,10 change / 10 季度)<br>或同时排期(→ 12 change / 12 季度,连带 3 处治理文件) | **仅改注册表** —— 排期是 DRI 的独立决策,不应被一次 adr-amendment 顺带改掉;且 §5.1 已有 8 个实例未排期的先例 |
| **D3** | **A4 与 A6 是否合并批** | 合并 / 分开 | **合并**(因 §3.3 耦合);但若走路线 B,则 A6 降级为「Stage 1 前置」而非本批执行 |
| **D4** | **批 1 的 commit 拆分** | 1 个 / **2 个**(契约面 + 治理面)/ 3 个 | **2 个** —— ①契约面(schema + 3 ADR + spec + fabric + 检查器 + 测试,可独立回滚)②治理面(台账 + 回执 + 计划 + 冻结清单) |
| **D5** | **R10 的最小可执行外部评审** | 维持不 archive / 放宽为 Stage 1 前置 / **定义一个可执行的最小动作** | **第 3 条**:冻结清单 + 回执 + 3 份 ADR 摘要 → 1 页 → 外部 reviewer 只需回 1 条「无阻断项」。把 R10 从「空喊」变成**一个动作** |
| **D6** | **A5 的触发机制** | 人工触发 / 在 lint 里加 ArchForge 变更检测 | **人工触发** —— 在台账登记触发条件(「ArchForge 批 2 commit 合入」),不做自动化探测(过度设计) |

---

## 5. 实施步骤

### Step 1 — T1 对外动作（无编译依赖,立即可做）

1. 回执新增 **§8.4「对 ArchForge 的正式答复」**（A14 的 4 条已定 + 4 条待确认,见 §2.1）
2. 回执新增 **§8.5「催问 Q-3」**（Q-3a~Q-3d 四问,见 §2.2）
3. 随 **T5** 的 commit 一并交付（`回执` 本身就是对外载体 → A13 闭合 ArchForge `V-8`）
4. 台账 Entry #9 ④ 追加一行:「A14 答复与 A15 催问已随本轮回执交付,待 ArchForge 回复」

**验收**:`grep -c "Q-3a\|Q-3b\|Q-3c\|Q-3d" 回执` == 4;回执 lint 绿。

### Step 2 — T5 批 1 commit（D2 决策后执行）

| commit | 内容 |
|--------|------|
| **① 契约面** | `protocol_instance.schema.json` · `fabric.{schema.json,json}` · `ADR-CEP-{08,09,12}-*.md` · umbrella `spec.md` · `cep_decl_alignment_check.sh` · `cep_docs_sync_check.sh` · `test/CMakeLists.txt` · `test/test_cep_schema.cc` |
| **② 治理面** | `obj-cep-platform.md`(Entry #9/#10) · 回执 · 计划 v2 · 冻结清单 · `ADR-CEP-00`(§8 Status Update) · `interface-alignment.md`(§6 判据状态) |

> ⚠️ commit 后**必须再跑一次** `cep_freeze_check.sh --update` —— 清单的 `frozen_at_commit` 取运行时 HEAD
> (批 1 的清单当前指向**变更前** HEAD `b8088819`)。
> ⚠️ `.opencode/plan/` **不在任何 .gitignore 里** → 需决定:随仓入库(决策记录)还是加进 `.opencode/.gitignore`(本地私有)。

### Step 3 — T2 A4 批 2（D1/D2 拍板后执行）

1. `ADR-CEP-11 §3` 锁表 +2 行
2. `07 号 §5.1.1` 表格 +2 行 + 标题「6 族 / 8 实例」+ `:310`/`:74`/`:659` 三处计数
3. `ADR-CEP-00:153` · `ADR-CEP-00A:169` · `ADR-CEP-12:147` · `ADR-CEP-18:68` 四处计数同步
4. `docs/architecture/README.md:37` 索引行
5. **`ADR-CEP-05:227` 限定注记**（路线 B）—— 正文不改,追加 `## Status Update` 或在同段加限定
6. `test/test_cep_schema.cc:417` 硬编码取值 +2
7. **锚点块**:`07 号:310` 的 symbol `'14 个 adapter 实例'` → `'16 个 adapter 实例'`
8. 台账 **Entry #11 [adr-amendment]** + `cep_freeze_check.sh --update`

**验收**:
- `bash scripts/test/cep_archforge_alignment_check.sh` → 33/33 绿（锚点已同步）
- `ctest --test-dir build -R cep` → 16/16 绿（`test_cep_schema.cc:564` 的表行数 ⇔ 计数一致性绿）
- `bash scripts/test/cep_docs_sync_check.sh --strict` → 9/9 绿
- `grep -rn "14 个 adapter 实例\|14 adapter 实例" docs/ .rddf/` → **仅**历史/Status 引述
- `openspec validate cpptlm-cep-umbrella --strict` → PASS

### Step 4 — T3 A6 批 3（Q-3 到达后执行）

1. 依 Q-3a 的 (b) 结论,重写 `ADR-CEP-05 §10` 判据 2 的「当前不触发」陈述
2. 依 Q-3b 决定 R-2 的落点(`perm_upgrade` 顶层字段 vs 改 `perm` 结构 —— 后者**已被证否**:
   会击穿 `test_cep_schema.cc:190-198` 的 4 值锁结构断言)
3. 依 Q-3c 决定标注侧(ArchForge 侧 vs 本仓侧)
4. 若 Q-3a 判定 (b) **不成立** → R-2 降级为 P1,`ADR-CEP-12 §8` 的 provisional 标注范围同步收窄

**验收**:`ADR-CEP-05` 与 `ADR-CEP-12` 的 IC 口径一致;lint 绿。

### Step 5 — T4 A5（ArchForge D-9 批 2 合入后执行）

1. 重新生成 **21 个 ArchForge 锚点**(按对方返工后的新行号)
2. 重跑 `cep_archforge_alignment_check.sh` → 33/33
3. **必须与 D-9 批 2 同批** —— 对方已把 D-9 拆成「不含锚点的批 1」+「含全部 20 锚点的批 2」,节奏已对齐

**验收**:33/33 绿;且**能指出**哪些锚点因 D-9 失效(而非「碰巧没红」)。

### Step 6 — T6 R10 处置（D5 决策后执行）

若采 D5-第 3 条:产出一页「外部评审摘要」→ 外部 reviewer 回一条「无阻断项」→ 回填台账 → Stage 0 可 archive。

---

## 6. 验收标准

- [ ] **AC-1** T1 交付:回执含 §8.4(A14 答复 4+4 条)+ §8.5(Q-3a~d 四问);随 commit ② 交付
- [ ] **AC-2** T2 落点 14 处**逐条**落地(表 §3.1 可逐行勾)
- [ ] **AC-3** T2 后 `cep_archforge_alignment_check.sh` 33/33 绿(**锚点 symbol 已同步**)
- [ ] **AC-4** T2 后 `ctest -R cep` 16/16 绿(表行数 ⇔ 计数一致性由 `:564` 强制)
- [ ] **AC-4b** T2 后 `cep_decl_alignment_check.sh` 5 项子校验全绿 + `cep_freeze_check.sh` **29/29** + provenance 自检通过
- [ ] **AC-5** T2 后 `grep -rn "14 个 adapter 实例\|14 adapter 实例" docs/ .rddf/ openspec/` 仅剩历史/Status 引述
      (覆盖面含 `ADR-CEP-12:39` 与 `tasks.md:107` —— 2026-10-22 复审 S7 的漏网处)
- [ ] **AC-6** T2 后 `ADR-CEP-05` 的「当前不触发」陈述**与 §5.1.1 登记表一致**(路线 B 的限定注记在位)
- [ ] **AC-7** T2 后 `openspec validate cpptlm-cep-umbrella --strict` PASS
- [ ] **AC-8** T2 台账 Entry #11 + 冻结清单重生成(`--authorized-by "Entry #11"`);`frozen_at_commit` 须指向**产物所在**的 commit
      —— **两步 commit 流程**:先 commit 产物 → `--update` → 再 commit 清单(2026-10-22 复审 B4 的教训:
      清单的 `frozen_at_commit` 若指向**变更前** HEAD,provenance 即为假,自检会报出)
- [ ] **AC-9** T3 后 `ADR-CEP-05` §10 与 `ADR-CEP-12` §8 的 R-2 口径**一致**(无「已触发/未触发」矛盾)
- [ ] **AC-10** T4 后 33/33 绿,且失效锚点可追溯到 D-9 返工
- [ ] **AC-11** T5:两个 commit 均**不**含冻结清单与产物 hash 不一致的中间态(commit 后重跑 `--update`)
- [ ] **AC-12** 全量回归 `ctest --test-dir build` **94/94**(每批结束时);声明对齐检查的**反向验证 12/12** 仍全绿
      (`bash scripts/test/cep_decl_reverse_verify.sh`)

---

## 7. 风险与回滚

| # | 风险 | 等级 | 缓解 / 回滚 |
|---|------|:----:|------------|
| **RK-1** | **A4 改计数却漏改锚点 symbol** → `cep_archforge_alignment_check` 红 | 🔴 | AC-3 + 锚点同步列入落点第 14 项;**A4 与锚点更新必须同批** |
| **RK-2** | **A4 制造 `ADR-CEP-05:227` 新活矛盾** | 🔴 | 路线 B 的限定注记是**必做项**(落点第 11 项),非可选 |
| **RK-3** | `test_cep_schema.cc:417` 忘补 2 个锁表取值 → **不红但静默失去覆盖** | 🔴 | 落点第 13 项;验收用 `grep` 核对(AC-5 同源) |
| **RK-4** | A4 顺带改了 KPI 排期口径 → 10 季度变 12 季度,治理文件 3 处失配 | 🟡 | D2 默认**仅改注册表**;排期另起决策 |
| **RK-5** | Q-3 第三轮仍只答一半 → T3 再次悬空 | 🟡 | A15 把 4 问**一次问完**且**具体化**;并把 Q-3d 与 T2 路线绑定,提高对方答的动力 |
| **RK-6** | ArchForge D-9 批 2 与 T4 时序错位 → 本仓 CI 中途红 | 🟡 | 对方已按「不含锚点的批 1 / 含 20 锚点的批 2」拆分,节奏已对齐;台账登记触发条件 |
| **RK-7** | 一次 adr-amendment 改 8 个冻结产物 → 评审负担重 | 🟡 | 批 2 只做 A4(计数),批 3 只做 A6(判据),**不混**;T2/T3 因耦合而合并的那部分仅限 `ADR-CEP-05` 一处限定注记 |
| **RK-8** | commit 后忘跑 `--update` → 冻结清单指向旧 commit | 🟡 | Step 2 的 ⚠️ + AC-11 |
| **RK-9** | R10 长期不闭合 → Stage 0 永远不 archive | 🔴 | D5 第 3 条把 R10 变成**一个动作**(1 页 + 1 条回执),而非无限期挂起 |

**回滚**:批 2 = 8 个冻结产物 + 2 个非冻结 + 锚点块 + 台账 → `git revert <commit>` + `cep_freeze_check.sh --update`;
批 3 同理。跨仓侧无改动(纯本仓 adr-amendment),无跨仓回滚成本。

---

## 8. 引用

**本仓**: `docs/implementation/2026-10-21-cep-archforge-response-receipt.md`(§8.2 N-1~N-4 / §8.3 M-1~M-3 / §10 执行记录) ·
`docs/adr/ADR-CEP-11-...md` §3 锁表 + 使用规则 · `docs/adr/ADR-CEP-12-...md` 决策 9 / §8 ·
`docs/adr/ADR-CEP-05-...md:227` α 失效判据 2 · `docs/adr/ADR-CEP-00-...md` §2 决策 3(KPI 排队)/ `:153` ·
`docs/adr/ADR-CEP-00A-...md:169` · `docs/adr/ADR-CEP-18-...md:68` ·
`docs/architecture/07-coherence-exploration-platform.md` `:74` `:310` `:312` `:659` ·
`test/test_cep_schema.cc` `:417` `:564` · `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md` 锚点块(末条) ·
`.rddf/roadmap/objectives/obj-cep-platform.md` §5.2 / §11 Entry #9/#10

**跨仓(只读)**: ArchForge `8290b4c` / `ac956b8` / `32 号 §6.0.1` / `32 号 §6.0.2` / `32 号 §6.4` ·
`22-nsa §0.5.4.1` · `ADR-SOC-25` D9 / D11

**机制**: `.rddf/roadmap/objectives/obj-cep-platform.md` §9.2(M1/M2/M4)· §11 台账 · `obj-cep-platform` DoD 7 · `improvement A2` · `phase-10`

---

## 10. 建议新增:从 ArchForge 使用场景抽象 PoC 用例(验证关键概念/架构/设计)

> **来源**:DRI 2026-10-22 提议 —— 「是否根据 ArchForge 端的使用场景抽象出 PoC 的用例,用于验证关键概念,架构或设计」。
> **本节结论**:✅ **强烈支持** —— 且它恰好能**解开当前两个死结**(Q-3 的 (b) 争议、R10 的独立验收缺口)。
> 但须加一条硬边界:**PoC ≠ 提前实现**,否则会把仍标 provisional 的形态焊死。

### 10.1 为什么 PoC 的价值高于它看起来

| 现在的死结 | 纸面状态 | PoC 能做什么 |
|-----------|---------|------------|
| **Q-3**:R-2 的 (b) 争议(E→M 静默升级是否构成跨域可见写) | 2 轮未答;ArchForge 双方各执一词 | **跑一遍就知道**:D-GPU 的 E 态静默升级后,D-FABRIC 读同一行 → 读到旧值 = (b) 成立 → R-2 命中;读到新值 = (b) 不成立 → R-2 降级。**这是 `ADR-CEP-06 §3` 的 `verification_mode` 提供的「声明 + 实验证伪」路径**(RK-2 的降级方案) |
| **R10**:独立验收缺口(真人外部评审缺) | 21 份 ADR + 66 invariant 纸面自洽,但**没有任何可执行物** | 外部 reviewer 面对「一个可跑的 slice + 契约差异」,而非 21 份 ADR → 评审**成本**与**门槛**同时下降 |
| **R-3 / R-1 / R-6**:scope 分级 / 分级目录 / 消息类映射 | 纸面都合理,**组合起来才会崩** | PoC 是「组合」的最小载体 —— 单独看每条都过,跑起来才暴露表达力缺口 |
| **D7 provisional 债** | 嵌套形态是**跨仓单方提案**,Stage 1 零实例 | PoC 若实现它,**就是第一次真正执行**该形态 → provisional 有了执行证据 |

### 10.2 从 ArchForge 使用场景抽的最小切片

> ArchForge 侧的三类场景(09 号 SoC 内 snoop / 25 号 NSA 跨卡目录 / 30 号 按 fabric scope 分级的跨节点模型)
> + 我方 3 域划分(D-CPU / D-GPU / D-FABRIC,per `ADR-SOC-25` D6)。

| 层 | PoC 最小实现 | 要验证的关键概念 |
|----|--------------|----------------|
| Stub Cache Core | 1 个 L1(4 值 `perm` 投影) | `INV-CEP-05-IC1`:6 态 → 4 值投影;瞬态态 `T` 不泄漏 |
| Protocol Engine | `moesi_gpu` 插件(6 态 × 6 事件) | 协议逻辑住插件;**无 `translate`**;请求携带 consistency label(D3-a/b/c) |
| Coherence Interface | canonical 10 流的最小子集(request / completion / snoop / snoop_resp / writeback) | 流口径与配对(CEN-Ib1 / Ib3 / Ib4 / Ib10) |
| Directory & Ordering | 1 个 SharerTracker(`SparseSnoopFilter`) | `ADR-CEP-04` 的 caps 语义 |
| Fabric & Transport | **2 网络 × 4 VC**(`compute_noc` + `ualink_fabric`),**per-network** 绑定 | **本次契约演进的核心** —— per-network VC 绑定在真实路径上跑不跑得通 |
| 跨域桥接 | D-GPU ↔ D-FABRIC 一条边 | `ADR-CEP-13` 域可见写判据 (a)(b) —— **Q-3 的答案** |

### 10.3 三个判定场景

| # | 场景 | 判定 |
|---|------|------|
| **S1** | 单域基线:GPU L1 读(命中/缺失)→ 写回 | `perm` 投影与 6 态状态机的不变量成立 |
| **S2** | 跨域可见写:D-GPU E 态静默升级 → D-FABRIC 读同一行 | **观测读到旧值还是新值** → 直接给出 (b) 的答案 → 解锁 A6 |
| **S3** | 死锁自由:通道依赖图在 `(协议实例, network, vc_id)` 顶点上无环 | 验证 `ADR-CEP-09` 顶点升级后判据仍成立(而非退化为全局图) |

### 10.4 硬边界(不可省略)

1. **PoC ≠ 提前实现** —— 必须做成**可丢弃的 spike**(单文件 / 不进 `ModuleFactory` / 不注册 ABI / 无公共 API),
   否则会把仍标 **provisional** 的 per-network 形态**焊死进框架**(与 D7 冲突);
2. **PoC 不验证「正确性」** —— 只验证「**可组合性 + 契约自洽**」;**不是** conformance test,不得对外宣称协议合规;
3. **实现顺序**:PoC **设计**(用例抽取 + 骨架)可现在做(不依赖未闭合项);
   **实现**排在 **A4(锁表,否则 `protocol_version` 装配期 fail)+ A6(R-2 判据,否则 `perm` 表达力未定)** 之后;
4. **成本**:Stage 1 的 6 个 change 中,本 PoC 约对应 3 个(`stub-cache-core` / 一个 protocol subset / fabric per-network);
5. **R10 不会被 PoC 关闭** —— PoC 让真人评审**可执行**,但**评审本身仍要做**(D5 的最小动作:1 页摘要 + 1 条回执)。

### 10.5 建议的动作

| # | 动作 | 时机 |
|---|------|------|
| **P-1** | 把本节作为 **Stage 1 的第 7 个 change**(`cpptlm-cep-poc-spike`)登记进 `ADR-CEP-00 §2 决策 3` 的 KPI 排队(10 → 11 change) | ⏸ 需 DRI 拍板(排队口径变更,与 A4 的 D2 同类) |
| **P-2** | 起草 PoC 的**用例规格**(S1/S2/S3 的输入 / 期望 / 判定),**不写实现** | ✅ 可立即(不依赖任何未闭合项) |
| **P-3** | 在 **A15 催问 Q-3** 时**加一问**:「贵方是否接受以 PoC 的 S2 场景**实验判定**替代 (b) 的纸面评估?」——若接受,则 Q-3 不再阻塞 A6 | ✅ 可立即(随 T1 一并发出) |
| **P-4** | PoC 实现 | ⏸ 待 A4 + A6 |
