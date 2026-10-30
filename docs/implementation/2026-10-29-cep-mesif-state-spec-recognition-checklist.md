# MESIF 规约逐格追认清单(状态机 + 5→4 `perm` 投影表)

> **来源**:CppTLM 仓 change `cpptlm-cep-mesif-state-spec`(队列 #9 `cpptlm-cep-mesif-adapter` 的**规约层**)
> **规范性载体**:`include/tlm/cep/mesif_state_spec.hh`(唯一真源 —— 逐格 `basis` 只在该文件)
> **跨仓承接**:`ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md`(跨仓)—— 债务台账 + 追认面
> **生成方式**:本清单的**三张表**(转换表 / 投影表 / 偏离清单)**由载体直接生成**
> —— 生成器**已入库**:`scripts/test/generate_mesif_recognition_checklist.sh`(+ `.cc`)
> ⇒ **载体变更 ⇒ 跑一次生成器**;一致性由 `test_cep_mesif_state_spec.cc` 的 **MS-06** 逐格核对(机器判据),
> 生成器另有 `--check` 模式(只比对不写)可作提交前自检。
> **日期**:2026-10-29 · **追认闭合**:2026-10-30(ArchForge `42 号` 35 格逐格追认闭合 ⇒ 本清单的「待追认」状态**已归零**)
> **跨仓追认结论载体**:`ArchForge:docs/architecture/42-mesif-state-ratification.md`(跨仓,只读引用)

---

## 0. 一句话

ArchForge 侧原先**没有** MESIF 的状态转换规约与 5→4 `perm` 投影表(per 立项证据 G1 / G2)⇒ 本仓按
「**本地推导 + 逐格依据 + 待追认标记**」产出规约(per change 的 design `D-α`)⇒ 本清单是**逐格追认的入口**。
**2026-10-30:追认已闭合** —— ArchForge `42 号` 给出 **35 格逐格结论**(路径 A 混合变体:
20 格逐格裁决 + 15 格按档批量,豁免 1)⇒ 载体 35 格**全部**转为**一手原文**(`kind = Primary` · `pending = false`)
⇒ **待追认刻度归零**。

### 依据强度分档(per design `D-ε.2`;**追认闭合后**的实测分布)

| 档 | 档位名 | 含义 | 追认动作 | **追认前**格数 | **追认后**格数 |
|----|--------|------|----------|----------------|----------------|
| **①** | `Primary` | 一手原文 | 确认未被推翻 / 逐格结论 | **4** | **35** |
| **②** | `Structural` | 结构模板推导:以 MOESI 6×6 为模板按 MESIF 裁剪 | 确认裁剪成立 | **10** | **0**(痕迹在 `note`) |
| **③** | `Derived` | 本仓推导:依 MESIF 的目录 / 本地-远端事件语义 | 确认推导成立 | **15** | **0**(痕迹在 `note`) |
| **③** | `Analogical` | 类比推导:ArchForge 约束 + CHI 同型先例 | 确认类比成立 | **6** | **0**(痕迹在 `note`) |

> **追认前的原档位刻度 = 4 / 10 / 15 / 6**(合计 35),与 `ArchForge 42 号` §0 的「原档位」分组
> **逐数一致**;该刻度由载体的**结构化字段** `Basis::origin_kind` 承载 + `origin_kind_count()` +
> `kExpectedOrigin*` 常量 + 静态锁 ⑪ **机器锁定**(不再靠散文同步 ⇒ 双侧刻度不会漂移)。
>
> **`I + RdUnique` 的原档位由 `Structural` 改判为 `Analogical`**(per `ArchForge` 回信 §2:MOESI 6×6
> 事件集**无** `RdUnique` 列,实据为 CHI `I+RdUnique→UC` 同构)⇒ 故 `Structural` 11 → **10**、
> `Analogical` 5 → **6**;该格当前 `kind` 仍为 `Primary`(追认闭合后一律如此)。

> ⚠️ **原档位不丢**:每格 `note` 的「〔原档位 …;原依据 …〕」段保留了追认**前**的档位与依据,
> 且 `origin_kind` 字段是**机器可读**的那一份 ⇒ **审计链完整**,而「待追认」计数归零
> ⇒ 跨仓债的**性质**从「待追认」变为「已追认 + 残余风险」(见 §5-残余风险)。

> **待追认刻度**:**0 / 35** 格待追认(`pending=no`;机器可查:`MesifStateSpec::pending_count()`);
> 一手原文 **35 / 35**(`MesifStateSpec::primary_count()`)。

---

## 1. 转换表逐格追认清单(5 态 × 6 事件 = **30 格**)

<!-- MESIF-TABLE:transition:START -->
| # | 态 | 事件 | 目标态 | kind | 原档位 | pending | ArchForge 需确认什么 |
|---|---|---|---|---|---|---|---|
| 1 | `I` | `Rd` | `S` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 2 | `I` | `RdUnique` | `E` | `Primary` | `Analogical` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 3 | `I` | `Wr` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 4 | `I` | `Upgrade` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 5 | `I` | `SnpShared` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 6 | `I` | `SnpUnique` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 7 | `S` | `Rd` | `S` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 8 | `S` | `RdUnique` | `E` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 9 | `S` | `Wr` | `S` | `Primary` | `Analogical` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 10 | `S` | `Upgrade` | `E` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 11 | `S` | `SnpShared` | `S` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 12 | `S` | `SnpUnique` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 13 | `E` | `Rd` | `E` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 14 | `E` | `RdUnique` | `E` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 15 | `E` | `Wr` | `M` | `Primary` | `Analogical` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 16 | `E` | `Upgrade` | `E` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 17 | `E` | `SnpShared` | `F` | `Primary` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 18 | `E` | `SnpUnique` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 19 | `M` | `Rd` | `M` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 20 | `M` | `RdUnique` | `M` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 21 | `M` | `Wr` | `M` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 22 | `M` | `Upgrade` | `M` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 23 | `M` | `SnpShared` | `F` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 24 | `M` | `SnpUnique` | `I` | `Primary` | `Derived` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 25 | `F` | `Rd` | `F` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 26 | `F` | `RdUnique` | `E` | `Primary` | `Analogical` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 27 | `F` | `Wr` | `F` | `Primary` | `Analogical` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 28 | `F` | `Upgrade` | `E` | `Primary` | `Analogical` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 29 | `F` | `SnpShared` | `F` | `Primary` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 30 | `F` | `SnpUnique` | `I` | `Primary` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
<!-- MESIF-TABLE:transition:END -->

---

## 2. 投影表逐格追认清单(**5 格**)

<!-- MESIF-TABLE:projection:START -->
| # | 态 | perm | kind | 原档位 | pending | ArchForge 需确认什么 |
|---|---|---|---|---|---|---|
| 1 | `I` | `NONE` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 2 | `S` | `READ` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 3 | `E` | `UNIQUE` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 4 | `M` | `WRITEBACK` | `Primary` | `Structural` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 5 | `F` | `WRITEBACK` | `Primary` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
<!-- MESIF-TABLE:projection:END -->

---

## 3. 偏离清单(**9 条** —— 追认优先级最高)

以下 9 格**偏离** CHI / MOESI 协议模板 ⇒ 它们是 ArchForge 侧最需要表态的部分
(per change 的 design `D-ε.8`;逐条数据由载体的 `kDeviations[]` 承载 ⇒ 下表**由载体生成**)。
**D5** 为 2026-10-30 追认**纠正**时新增(`M + SnpShared`:原 `S` → 纠正为 `F`);D1 ~ D4e **不重编号**。

<!-- MESIF-TABLE:deviations:START -->
| # | 态 | 事件 | 模板取值 | 本表取值 | 偏离理由 |
|---|---|---|---|---|---|
| 1 | `I` | `SnpShared` | `SC` | `I` | D1:CHI 该格把请求者拿到共享副本写进了响应者的行;本表按语义正确性取无副本不因被 snoop 而获得副本 |
| 2 | `E` | `SnpShared` | `SC` | `F` | D2:MESIF 的 Forward 语义 —— 干净 Owner 保留 Owner 身份成为转发点(CHI 的 RN 侧无转发点概念) |
| 3 | `S` | `RdUnique` | `SC` | `E` | D3:MESIF 的目录语义 —— 取独占 ⇒ sharer 全失效 ⇒ 本节点成唯一持有者 |
| 4 | `F` | `RdUnique` | `—` | `E` | D4a:MESIF 特有(F = 转发点)—— 取独占 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占 |
| 5 | `F` | `Wr` | `—` | `F` | D4b:MESIF 特有 —— F 有 sharer ⇒ 非独占 ⇒ 写非法 ⇒ 状态不变(须先 Upgrade 到 E) |
| 6 | `F` | `Upgrade` | `—` | `E` | D4c:MESIF 特有 —— 升级 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占 |
| 7 | `F` | `SnpShared` | `—` | `F` | D4d:MESIF 特有 —— 已是单 Owner + 有 sharer ⇒ 再来 sharer ⇒ 仍满足 F 的定义 |
| 8 | `F` | `SnpUnique` | `—` | `I` | D4e:MESIF 特有 —— 独占 snoop ⇒ Owner 转移 ⇒ Forward 清空 ⇒ 无副本 |
| 9 | `M` | `SnpShared` | `SC` | `F` | D5:MESIF 特有 —— 脏 Owner 写回后成干净 Owner,保留 Owner 身份成为转发点(与 D2 同型;CHI 无转发点概念,UD+SnpShared→SC 不适用) |
<!-- MESIF-TABLE:deviations:END -->

> **风险分档**(人工评估,per design `D-ε.8`):**高** = D2(`E+SnpShared` → `F`,MESIF Forward 语义)
> 与 D4a~D4e(`F` 行 5 格,CHI **无对应态**)**+ D5**(`M+SnpShared` → `F`,脏 Owner 写回后成转发点,
> 与 D2 同型);**中** = D3(`S+RdUnique` → `E`,MESIF 目录语义);
> **低** = D1(`I+SnpShared`,CHI 该格把「请求者拿到共享副本」写进了**响应者**的行)。

---

## 4. 跨仓动作登记(本仓侧待办)

| # | 动作 | 触发条件 | 执行主体 | 状态 |
|---|------|----------|----------|------|
| **X-1** | `protocol_instances.json` 的 `authority_migrations[mesif]` **降级 / 删除** | ArchForge 侧完成 `32 号 §6.2` 的追认(方案 A)⇒ 两端口径一致 | **本仓**(需人工裁决 + 台账 entry) | ✅ **已触发并执行**(2026-10-30 —— ArchForge `32 号 §6.2:519-523` 已改为 `HierarchicalDirectory` + `SYSTEM/EXACT/GLOBAL/HIERARCHICAL`;两端声明**逐字段实测全等**(12 个非 `status` 键)⇒ 迁移条目**已删除**,⑥ 镜像闸对 `mesif` **恢复逐字段校验**;per 台账 Entry #102) |
| **X-2** | 追认完成后把 35 格回填进 ArchForge `32 号`(§7 之后新增小节)+ `25 号 §4.2` 的目录条目状态编码依据 | ArchForge 侧追认完成 | ArchForge | ✅ **已完成**(2026-10-30,ArchForge commit `b3062f1`)—— 落点:`32 号 §7.4`(:634,30 格转换表)+ `25 号 §4.2` + `32 号 §7.2.1`(投影 5 格);**无需后续 change**(per ArchForge 回信 §1) |

---

## 5. 追认进度刻度(机器可查)

| 刻度 | 值 | 查询方式 |
|------|-----|----------|
| 待追认格数 | **0 / 35**(追认已闭合) | `MesifStateSpec::pending_count(MesifStateSpec::kTable)` |
| 一手原文格数 | **35 / 35** | `MesifStateSpec::primary_count(...)` |
| 转换表一手原文 | **30**(= 全部转换格) | `MesifStateSpec::transition_primary_count(...)` |
| 投影表一手原文 | **5**(= 全部投影格) | `MesifStateSpec::projection_primary_count(...)` |

### 5-残余风险(per ArchForge `42 号` §4.2;**显式,不隐藏**)

| # | 残余风险 | 本仓的处置 |
|---|----------|------------|
| **R-1** | **按档批量的 14 格证据强度弱于逐格**(档位理由是归纳,逐格是枚举)⇒ 若某格的**语义判断**(而非推导方法)有误,档位理由覆盖不到 | 本仓侧**已把该风险显式登记**在此;缓解 = ArchForge 的「独立复核 gate 制度化」(非填表角色抽 5 格、必须含全部 `Analogical` 档,独立重推比对)—— 该纠正已在 `M+SnpShared` 上**实证生效**(oracle 独立复核抓出) |
| **R-2** | **纠正落地前的差异窗口**:纠正(`M+SnpShared`:`S`→`F`)在 ArchForge 侧先落地,本仓载体后落地 ⇒ 窗口内两端取值不一致 | ✅ **已闭合**(2026-10-30 本仓载体已同步 `F`)⇒ 载体为最终真源 |
| **R-3** | **纠正带来的新语义后果**:`M → F`(脏 Owner 写回后成**干净**转发点)与 `M` 同投影 `WRITEBACK` ⇒ 「已写回(干净)」与「仍脏」在 4 值接口上**同值不可分辨** ⇒ **写回事实在 perm 接口上不可见** | ✅ **已登记**为语义损失 ③(载体 `kPermMergeWritebackPathNote` + main spec MS-04 子条件 ⑥);⚠️ 该后果**不可消除**(消除需扩 `perm` 值域 ⇒ 违反 4 值锁)⇒ 实现层须依赖 **Sharer Tracker** 判别「谁持有」 |

> ⚠️ **判据口径边界(追认闭合后)**:本仓的判据锁**完备性 / 可审计性 / 待追认标记 / 内部自洽**,
> **不**锁推导值的**语义正确性** —— 追认**前**靠 `pending` 标记表达「未确认」;
> 追认**后**(本态)该标记归零 ⇒ 「未确认」的表达责任**移交**给 §5 的**残余风险**登记
> ⇒ 实现层(队列 #9)可启动,但 SHALL 以 §5 的残余风险为已知输入。

---

## 6. 维护记录

| 日期 | 版本 | 修订 |
|------|------|------|
| 2026-10-29 | v0.1 | 初版:30 格转换 + 5 格投影 + 8 条偏离 + 2 项跨仓待办(清单由载体生成) |
| 2026-10-30 | **v0.3** | 收讫 ArchForge 回信(`cross-repo-ratification-apply-ack.md`):**原档位刻度结构化** —— `Basis::origin_kind` + `origin_kind_count()` + `kExpectedOrigin*`(4/10/15/6)+ 静态锁 ⑪ ⇒ 双侧刻度机器可核对;`I+RdUnique` 原档位 `Structural` → `Analogical`(per 回信 §2);清单三表增「原档位」列;**X-2 ✅ 已完成**(落点 `32 号 §7.4` / `25 号 §4.2` / `32 号 §7.2.1`) |
| 2026-10-30 | v0.2 | **追认闭合落地**(per ArchForge `42 号`):35 格全部转一手原文(`pending` 31 → **0**)· **纠正 1 格**(`M+SnpShared`:`S`→`F`,新增 **D5** ⇒ 偏离 8 → **9**)· 统计按**实表**重算(转换 **30** / 投影 **5**)· 刻度与残余风险(§5)重写 · X-1 已触发并执行 |
