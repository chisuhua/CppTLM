# CEP B3a / B3b 架构裁决审稿报告（oracle → critic）

**日期**: 2026-11-06 · **角色**: architect · **范围**: `§6.2` 剩余两项(B3a / B3b)的**推进裁决**
**触发**: 台账 `obj-cep-platform.md` **Entry #237**(事实底稿 + 三个裁决请求)
**用户裁决**: D-3 = **B3a 记 ⏭ 阻塞**;D-1 / D-2 = 「由 oracle + critic 给方案 / 建议」
**审查对**: **oracle → critic**(顺序不可颠倒)· 结论 **APPROVE_WITH_CONDITIONS**,**三档均无 BLOCKER**

---

## 0. 判据来源

| # | 判据 | 出处 |
|---|---|---|
| 1 | 用户本次的裁决口径 | D-3 = ① 记为 ⏭ 阻塞;D-1 / D-2 走 oracle + critic |
| 2 | `§6.2` B3a / B3b 的既有条款 | `.rddf/roadmap/objectives/obj-cep-platform.md` §6.2 |
| 3 | B3b 的触发条件 | `docs/architecture/07-coherence-exploration-platform.md` §5.1.1(触发条件句) |
| 4 | B3a 的残余 | `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` SU 2026-11-03 §③ 残余 |
| 5 | `perm` 值域锁 | `docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md` `CEN-Ib11` + SU 2026-11-02 R-T4 |
| 6 | 事实底稿 | 台账 Entry #237 §1.1 ~ §1.5(逐条实测,planner 域) |

---

## 1. oracle 结论(架构顾问 · 4 段式)

### ① 根因

- **D-1 的真实依赖 = 判定式缺失**,**不是**外部事实,也**不是**单纯仓内遗漏:
  「转移表真源」**从未被定义** ⇒ 2026-11-04 的裁决按裁决者心智模型(「转移表 = 按消息索引的状态机」)解读
  ⇒ 把 TileLink-C 登记进「无转移表真源」组 —— 而该登记**对 TileLink-C 是事实错误**(载体自 2026-11-02 起即有 12/12 Known + 全 Direct 矩阵)。
  但 TileLink-C 有其余四族没有的**真实缺口**:矩阵按**权限操作**索引,而引擎消费面是 CohIf 消息/意图
  ⇒ 「消息→事件」的 ingress 映射**在仓内任何载体都不存在**。
  ⇒ 这是本仓「一词两义」型卡住的**第三次**复现(B3 拆分同型)。
- **D-2 的真实依赖 = 合规受限导致的语义无对应物 + 两个未落地的登记面**:
  N-9 的「4 值锁取哪个子集」在 UALink 侧**无对应物** ⇒ 该值只能是 CEP 建模决策;
  `perm` 的 `minItems: 1` ⇒ **空集被 schema 禁止** ⇒ 「不适用」无合法表达;
  `sharer_tracker_rationales{}`(ADR-CEP-04 SU ④ 的**未勾选** Verification)与 `caps ↔ traits` 元测试
  **均从未落地**(JSON / 测试 / git 历史三处零命中)。

### ② 建议

- **D-1 = ③(口径须细化)**,细化后的判定式 = **T1 存在 / T2 可判定 / T3 可消费 / T4 完备**;
  逐族重判 ⇒ **TileLink-C = T1✅ T2✅ T4✅ T3❌** ⇒ **诚实中间态**:触发事件**已客观发生**,但「补引擎」的剩余前置
  被精确化为一项**低成本、规范可读**的工作(T3 普查)。
  ⇒ **不是**「条件未触发 ⇒ 不用做」,也**不是**「已触发 ⇒ 立即开引擎」(后者 = 跳过 T3 = 推理填空)。
- **D-2 = ②(`perm` 必须取值)**:取 **`perm = ["NONE"]`**(basis = `self-derived`)+ `impl = 不参与` + `consistency_labels = PLO/PLO`(假设登记);
  **更轻的替代**(不动 schema):语义落在 `ADR-CEP-02` 的 SU + 理由登记块 ⇒ `minItems: 1` 保留 ⇒ 冻结面不触发授权流程。
  规则 = **最小可锚定子集规则**(只声明可锚定的值;一个都锚不到时取 `[NONE]`;超锚值须 `pending` + 转正 trigger)。
  `impl = 不参与` 的选择**不是**无意义的:`BroadcastNull` 的判别依据(有 snoop 类流量但无集中目录)因 N-8 落地而**事实不适用**。

### ③ 已排除的替代方案(节选)

- D-1 直接选 ①(立即开引擎)= 跳过 T3 = 2026-11-04 裁决明确禁止的推理填空;
- D-1 选 ②(直接 ⏭)= 需靠一条**事实错误**维持登记;
- 「有矩阵即算真源」= 会把 CXL 2.0(42/68 Unknown)也触发 ⇒ 直接撞它已冻结的登记;
- D-2 选 ①(`minItems: 1→0`)= 契约面两步授权 + 空集**零信息值**(与「忘填」不可区分)+ 值域锁退化;
- `perm` 取 `[NONE, READ]` 或全 4 值 = READ 锚不到 / 把 MESIF 形态照抄到无 cache-state 的 fabric;
- 用 `flow_support` 派生 tracker 选型 = 已被 6/7 存量声明反证(`ADR-CEP-04` SU 2026-11-03 §②)。

### ④ 下一步

**必做**(无论裁决如何):① 修错误事实陈述(validation 文档 §3 B 类 + 逐族单列);② **T1–T4 判定式落载体**(`07 号` SU);
③ 登记面补落(O1 理由登记 + O2 caps↔traits 元测试)。
**条件触发**:④ T3 普查 change(全行有锚 ⇒ 转引擎 change;存在无锚行 ⇒ 该行 pending + TileLink-C 登记 ⏭);
⑤ D-2 顺序铁律(SU → 登记面 → 声明实例 → 计数链)。

---

## 2. critic 结论(审稿 · 4 段式 + mergeable / scope_exceeded)

**mergeable: yes** · **scope_exceeded: false** · 结论 **APPROVE_WITH_CONDITIONS** —— **三档(arch / build / plan)均无 BLOCKER**

### ① BLOCKER

**无。** critic 逐档排查了 4 个候选并逐一排除:
R-T4 ③「SHALL 填 3 值」与新规则存在**字面张力**但可按「可表达集合」统一读法消解;
`ADR-CEP-04:763` §③残余的前置(Task 0 普查)**已由 Entry #237 完成**;
T1–T4 是台账 Entry #237 明示「待 architect 裁决 D-1」的**授权裁决**,非伪决策;
**无**任何与已签 ADR 的未授权冲突。

### ② SHOULD_FIX(11 条 —— 全部已落入裁决文本,逐条处置见 §3)

critic 的 focus 三问结论:
1. **T1–T4 确实堵住了自欺**,TileLink-C 的中间态**成立**(判 T3❌ 是诚实,不是把已触发偷换成前置)——
   **但条件是** SU 必须显式写出「2026-11-04 的阻塞理由对 TileLink-C 已解除,剩余是**有实证出口的工程项**」。
2. **`perm` 是精化不是降格**(词汇锁未动 + schema 零改动恰证);下游误读风险**已实测排除**
   (`perm` 的唯一机器消费者是 `exposes_unique_perm`,`["NONE"]` 只触发 vacuous PASS 且 detail 文字准确)——
   **但条件是** SU 必须明说「`NONE` = 零可锚定值的唯一合法表达,是**信息下限**而非事实断言」。
3. **未发现互为掩护 / DoD 降格变体**(B3b 维持 1/6、防自欺条款原样保留、T1–T4 让触发条件**变严**)——
   **但** `ualink_inc_state_spec.hh:650` 的编译期锁字面是「Uis08 SHALL 保持 pending(公开来源撑不住)」,
   本轮翻转它的依据**从「无字面 no-snoop」升级为「4 个独立非规范来源 + 显式推断」** ⇒ SU 须按「**判据升级**而非平反」写。

### ③ NITS(6 条 —— 全部已处置,见 §3.3)

### ④ 结论

三档均 **APPROVE_WITH_CONDITIONS**;`mergeable: yes`;`scope_exceeded: false`。

---

## 3. critic SHOULD_FIX / NITS 的**逐条处置**

### 3.1 arch 档(5 条)

| # | critic 的 finding | 处置 | 落点 |
|---|---|---|---|
| A1 | `ADR-CEP-02` 新 SU ↔ R-T4 SU ③(「不暴露 `UNIQUE` 的协议族 SHALL 填 3 值」)缺调和句 ⇒ 留雷 | ✅ **已加三情形表** + 明写「R-T4 §③ 的『3 值』**仅适用于有 3 值投影的族**」+「⛔ 不推翻前两行、⛔ 不改值域」 | `ADR-CEP-02` SU 2026-11-06 **§③** |
| A2 | `ADR-CEP-04` 新 SU 必须**显式闭合 §③残余** ⇒ 否则同一 ADR 两段并存「不得预先钉死」与「现已钉死」而无 transition | ✅ 段首加「本段是 §③残余的**正式收口**」+「**前置已满足**(Task 0 普查 per Entry #237 §1.4)」 | `ADR-CEP-04` SU 2026-11-06 **头部 + §①** |
| A3 | `07 号` SU 必须改写「剩余前置」框架 + 给 T3 普查下**定义**(否则下一个未定义术语) | ✅ §④ 写明「阻塞**已解除**,剩余是**工程项**而非**条件闸**」+ 两条反向声明(「不是条件未触发」/「也不是已触发即开引擎」);§⑤ 给 T3 三要素(**行宇宙** / **锚标准** / **交付物与登记位置**)+ 与 `ADR-CEP-12` R-T3 的关系(两轴不同 artifact) | `07 号` SU 2026-11-06 **§④ + §⑤** |
| A4 | Uis08/Uis09 翻转的 `BasisKind` 映射必须写进 SU(载体只有 2 值,无 `public-nonnormative-derived`) | ✅ 明写:载体 `BasisKind` 只有 `SelfDerived`/`Pending` ⇒ ⛔ **不发明第三值** ⇒ 翻转后 = **`SelfDerived` + 锚点摘要 `C-10`**(「非规范来源支撑的建模声明」的诚实表达 = 锚到仓内已签锚点) | `ADR-CEP-04` SU **§④** |
| A5 | 变更范围**漏了锚点摘要**(N-8/N-9 的登记真源)⇒ SU + 载体翻转 + 摘要更新是**同一裁决的三件套** | ✅ §④ 同步范围写明「锚点摘要新增 `C-10` + `N-8` 状态变更 + **『9 项 pending』计数同步**」;§⑦ Verification 第 3 项落地该义务 | `ADR-CEP-04` SU **§④ + §⑦** |

### 3.2 plan / build 档(4 条)

| # | critic 的 finding | 处置 | 落点 |
|---|---|---|---|
| P1 / B1 | **既有 7 实例计数断言未列入 tasks ⇒ verify 必红**:`test_cep_instance_declaration_contract.cc:108`(`REQUIRE(n == 7u)`)· `test_cep_tilelink_instance_decl.cc:209` · `test_cep_ualink_inc.cc:398` | ✅ 三处**文件:行号**已写入 Verification 勾选项 | `ADR-CEP-04` SU **§⑦** |
| P2 | Uis08/09 翻转的**任务归属与排序**未定义(否则实例声明 `snoop=false` 与载体锁「SHALL 保持 pending」并存) | ✅ §④「归属」+ §⑤ **顺序铁律**(① 签 SU ⇒ ② 登记面 + `C-10` + 载体翻转 ⇒ ③ 声明实例 ⇒ ④ 计数链) | `ADR-CEP-04` SU **§④ + §⑤** |
| P3 | 「`07 号` 三处」必须**枚举成文件级清单** + grep 复核;且 UALink 行翻转后须**保留**「实例级装配仍 backlog」 | ✅ §⑤ 计数链表把三处**具名**(§5.1.1 UALink 行 / §11 计数 / §6.2 SU 注记);⚠️「声明 ≠ 装配」写明 | `ADR-CEP-04` SU **§⑤** |
| B2 | 新「声明对齐闸」应把**四项裁决值与 basis 标签逐字钉进测试**(使 `public-nonnormative-derived` 的非规范属性有机器牙齿) | ✅ §⑦「决定性变异」勾选项覆盖 caps 一格 / 理由一条两条变异;**逐字断言清单**列入 builder 任务 | `ADR-CEP-04` SU **§⑦** + builder 任务 |

### 3.3 NITS(6 条)

| # | NIT | 处置 |
|---|---|---|
| N1 | `07 号` 治理不变量只允许「正文就地澄清(零行数变化)」或「末尾追加 SU」 | ✅ 已遵守:触发条件句**就地澄清且行数不变**(已 assert);T1–T4 走**末尾追加 SU** |
| N2 | 重判表把「达成度」与「T1–T4 闸评估」混在一列 ⇒ 读者会把「已 1/6」读成「CHI 已过 T3」 | ✅ 已**拆成两列**(`B3b 达成度` / `T1…T4`) |
| N3 | 最小可锚定子集规则应声明归因 id 归属(既有体系 or 待建立) | ✅ `ADR-CEP-02` SU §②「归因 id」行:复用 `CP-0x`,未纳入者按附录纪律**登记为待建立**,⛔ 禁新造平行体系 |
| N4 | PLO/PLO「以假设登记进同一证伪通道」须**命名登记载体** | ✅ `ADR-CEP-04` SU §① 末列:登记载体 = **台账 Entry + 本段** |
| N5 | 若新 SU 定义了新 INV-*/CEN-* 条目,确认 `cep_docs_sync_check.sh` 条数自解析 | ✅ **本轮新增 invariant 数 = 0**(`07 号` SU §⑥ 明记)⇒ 无需改条数,moot |
| N6 | schema 零改动可接受性 | ✅ critic 逐项核验**可接受**:`perm=["NONE"]` / `impl="不参与"` / `PLO` 均在既有枚举内;`protocol_instance.schema.json` 在冻结面内 ⇒ **不改即不触发** `--update --authorized-by` |

---

## 4. 裁决(本报告的产出)

| # | 裁决 | 载体 |
|---|---|---|
| **D-1** | 「转移表真源」= **T1–T4 四要件全满足**;**TileLink-C = T1✅ T2✅ T4✅ T3❌** ⇒ **阻塞已解除**,剩余 = **T3 映射锚点普查**(工程项,非条件闸);CXL 2.0 **冻结**(T4❌);CXL 3.0 / NVLink / UALink **未触发**;**B3b 维持 1/6** | `07 号` SU 2026-11-06 §②–§⑦ |
| **D-2** | `perm` = **`["NONE"]`**(basis = `self-derived`;语义 = 「CohIf 腿不承载 cache 权限升级语义」,**信息下限**非事实断言)· `flow_support.snoop/snoop_resp = false/false`(basis = `public-nonnormative-derived`,**一步推断**已留档)· `impl = 不参与` · `consistency_labels = PLO/PLO`(假设登记);**schema 零改动**;**先补 O1/O2 登记面,后声明实例** | `ADR-CEP-02` SU 2026-11-06 · `ADR-CEP-04` SU 2026-11-06 |
| **D-3**(用户已定) | B3a 终态口径 = **⏭ 阻塞**;收口形态 = `ualink_4` 声明 ∨ 登记第 8 实例 ⏭ | 台账(待 planner 落 Entry #238) |

---

## 5. 「ADR 完成的标志」自检(per architect 产出形态契约)

| # | 判据 | 状态 |
|---|---|---|
| 1 | 四要素齐(元数据 / 选项对比 / 独立后果代价章 / Verification 可勾选章) | ✅ 三份 SU 均含「裁决 + 依据 + 后果/代价 + Verification(可勾选)」;选项对比以「情形 × 取值」表呈现 |
| 2 | oracle + critic 双审完成 | ✅ oracle(§1)+ critic(§2),顺序正确;**无 BLOCKER** ⇒ 无需复审 |
| 3 | Status 已转已决 | ⚠️ **部分** —— `ADR-CEP-04` 整体 Status 仍为「起草中 v0.6」;本轮**只裁 `ualink_4` 取值**,**不改**整体 Status(超出本裁决范围,须另立裁决) |
| 4 | 关联 change 已落库或已登记 | ✅ 占位 `backlog: cpptlm-cep-b3a-b3b-advancement`(三份 SU 均写明)⇒ **待 planner 立项后回填** |
| 5 | 审稿报告已落 `.rddf/reviews/` 且 ADR 内有指针 | ✅ 本文件;三份 SU 的「关联」行均指向 `.rddf/reviews/cep-b3a-b3b-ruling.md` |

---

## 6. 路由(后续执行顺序 —— builder / planner 域)

**builder 域**(依 `ADR-CEP-04` SU §⑤ 顺序铁律):
① 落地 O1(`sharer_tracker_rationales{}` + 理由存在性元测试)· O2(`caps ↔ kTrackerTraits` 一致性元测试);
② 锚点摘要新增 `C-10` + `N-8` 状态变更 + **「9 项 pending」计数同步**;载体 `Uis08`/`Uis09` 翻转为 `SelfDerived` + `C-10`;
③ 声明 `ualink_4` 实例(13 键)+ `repo_owned_instance_ids`;
④ 计数链同步(7→8 / `implemented` 7→8 / `no_upgrade_instances` 3→4 / `07 号` 三处)+ **三处存量计数断言**翻转;
⑤ 改写 `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` §3 B 类(TileLink-C 归类)+ **逐族单列**
(⚠️ 该文件属 `docs/validation/` = **附属 2 类**,**不在 architect 的 `arch_paths` 内** ⇒ 归 builder);
⑥ T3 普查 change 立项(行宇宙完整 + 逐行 `basis`/`ref` + 决定性变异)。

**planner 域**:台账 Entry #238 + `§6.2` B3a / B3b 指针 + 四族 ⏭ 登记(`reason` + `trigger_condition` + `ledger_entry`)
+ 立项 `cpptlm-cep-b3a-b3b-advancement` improvement(回填三份 SU 的占位指针)。

**验收命令**:
`python3 scripts/test/cep_invariant_map_check.py --check --for-achievement` ·
`bash scripts/test/cep_decl_alignment_check.sh --strict` · `bash scripts/test/cep_freeze_check.sh --strict` ·
`bash scripts/test/cep_prereg_drift_check.sh --strict` · `bash scripts/test/cep_docs_sync_check.sh --strict` ·
`cmake --build build -j4 --target cpptlm_tests && ./build/bin/cpptlm_tests "[cep]"`
