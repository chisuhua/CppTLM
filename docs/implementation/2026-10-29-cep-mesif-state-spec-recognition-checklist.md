# MESIF 规约逐格追认清单(状态机 + 5→4 `perm` 投影表)

> **来源**:CppTLM 仓 change `cpptlm-cep-mesif-state-spec`(队列 #9 `cpptlm-cep-mesif-adapter` 的**规约层**)
> **规范性载体**:`include/tlm/cep/mesif_state_spec.hh`(唯一真源 —— 逐格 `basis` 只在该文件)
> **跨仓承接**:`ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md`(跨仓)—— 债务台账 + 追认面
> **生成方式**:本清单的**三张表**(转换表 / 投影表 / 偏离清单)**由载体直接生成**
> —— 生成器**已入库**:`scripts/test/generate_mesif_recognition_checklist.sh`(+ `.cc`)
> ⇒ **载体变更 ⇒ 跑一次生成器**;一致性由 `test_cep_mesif_state_spec.cc` 的 **MS-06** 逐格核对(机器判据),
> 生成器另有 `--check` 模式(只比对不写)可作提交前自检。
> **日期**:2026-10-29

---

## 0. 一句话

ArchForge 侧**没有** MESIF 的状态转换规约与 5→4 `perm` 投影表(per 立项证据 G1 / G2)⇒ 本仓按
「**本地推导 + 逐格依据 + 待追认标记**」产出规约(per change 的 design `D-α`)⇒ 本清单是**逐格追认的入口**:
ArchForge 侧只需对**第 ②/③ 档**(推导 / 类比)逐格确认或纠正;**第 ① 档**(一手原文)仅需确认未被推翻。

### 依据强度分档(per design `D-ε.2`)

| 档 | `kind` | 含义 | 本清单的追认动作 | 格数 |
|----|--------|------|------------------|------|
| **①** | `Primary` | 一手原文:结论可由 ArchForge 原文定义**直接**推出 | **无需追认** —— 仅需确认该原文未被推翻 | **4** |
| **②** | `Structural` | 结构模板推导:以 MOESI 6×6 为模板按 MESIF 裁剪 | 确认**裁剪**成立 | **11** |
| **③** | `Derived` | 本仓推导:依 MESIF 的目录 / 本地-远端事件语义 | 确认**推导**成立 | **15** |
| **③** | `Analogical` | 类比推导:ArchForge 约束 + CHI 同型先例 | 确认**类比**成立 | **5** |

> **待追认刻度**:**31 / 35** 格待追认(`pending=yes`);该计数随 ArchForge 逐格确认而下降
> (机器可查:`MesifStateSpec::pending_count()`)。

---

## 1. 转换表逐格追认清单(5 态 × 6 事件 = **30 格**)

<!-- MESHIF-TABLE:transition:START -->
| # | 态 | 事件 | 目标态 | kind | pending | ArchForge 需确认什么 |
|---|---|---|---|---|---|---|
| 1 | `I` | `Rd` | `S` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 2 | `I` | `RdUnique` | `E` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 3 | `I` | `Wr` | `I` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 4 | `I` | `Upgrade` | `I` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 5 | `I` | `SnpShared` | `S` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 6 | `I` | `SnpUnique` | `I` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 7 | `S` | `Rd` | `S` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 8 | `S` | `RdUnique` | `E` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 9 | `S` | `Wr` | `S` | `Analogical` | yes | 确认**类比**成立(ArchForge Owner 唯一性约束 + CHI 同型先例) |
| 10 | `S` | `Upgrade` | `E` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 11 | `S` | `SnpShared` | `S` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 12 | `S` | `SnpUnique` | `I` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 13 | `E` | `Rd` | `E` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 14 | `E` | `RdUnique` | `E` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 15 | `E` | `Wr` | `M` | `Analogical` | yes | 确认**类比**成立(ArchForge Owner 唯一性约束 + CHI 同型先例) |
| 16 | `E` | `Upgrade` | `E` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 17 | `E` | `SnpShared` | `F` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 18 | `E` | `SnpUnique` | `I` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 19 | `M` | `Rd` | `M` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 20 | `M` | `RdUnique` | `M` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 21 | `M` | `Wr` | `M` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 22 | `M` | `Upgrade` | `M` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 23 | `M` | `SnpShared` | `S` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 24 | `M` | `SnpUnique` | `I` | `Derived` | yes | 确认**推导**成立(MESIF 目录 / 本地-远端事件语义) |
| 25 | `F` | `Rd` | `F` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 26 | `F` | `RdUnique` | `E` | `Analogical` | yes | 确认**类比**成立(ArchForge Owner 唯一性约束 + CHI 同型先例) |
| 27 | `F` | `Wr` | `F` | `Analogical` | yes | 确认**类比**成立(ArchForge Owner 唯一性约束 + CHI 同型先例) |
| 28 | `F` | `Upgrade` | `E` | `Analogical` | yes | 确认**类比**成立(ArchForge Owner 唯一性约束 + CHI 同型先例) |
| 29 | `F` | `SnpShared` | `F` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
| 30 | `F` | `SnpUnique` | `I` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
<!-- MESHIF-TABLE:transition:END -->

---

## 2. 投影表逐格追认清单(**5 格**)

<!-- MESHIF-TABLE:projection:START -->
| # | 态 | perm | kind | pending | ArchForge 需确认什么 |
|---|---|---|---|---|---|
| 1 | `I` | `NONE` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 2 | `S` | `READ` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 3 | `E` | `UNIQUE` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 4 | `M` | `WRITEBACK` | `Structural` | yes | 确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义) |
| 5 | `F` | `WRITEBACK` | `Primary` | no | **无需追认**(一手原文)—— 仅需确认该原文未被推翻 |
<!-- MESHIF-TABLE:projection:END -->

---

## 3. 偏离清单(**8 条** —— 追认优先级最高)

以下 8 格**偏离** CHI / MOESI 协议模板 ⇒ 它们是 ArchForge 侧最需要表态的部分
(per change 的 design `D-ε.8`;逐条数据由载体的 `kDeviations[]` 承载 ⇒ 下表**由载体生成**)。

<!-- MESHIF-TABLE:deviations:START -->
| # | 态 | 事件 | 模板取值 | 本表取值 | 偏离理由 |
|---|---|---|---|---|---|
| 1 | `I` | `SnpShared` | `SC` | `S` | D1:CHI 该格把请求者拿到共享副本写进了响应者的行;本表按语义正确性取无副本不因被 snoop 而获得副本 |
| 2 | `E` | `SnpShared` | `SC` | `F` | D2:MESIF 的 Forward 语义 —— 干净 Owner 保留 Owner 身份成为转发点(CHI 的 RN 侧无转发点概念) |
| 3 | `S` | `RdUnique` | `SC` | `E` | D3:MESIF 的目录语义 —— 取独占 ⇒ sharer 全失效 ⇒ 本节点成唯一持有者 |
| 4 | `F` | `RdUnique` | `—` | `E` | D4a:MESIF 特有(F = 转发点)—— 取独占 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占 |
| 5 | `F` | `Wr` | `—` | `F` | D4b:MESIF 特有 —— F 有 sharer ⇒ 非独占 ⇒ 写非法 ⇒ 状态不变(须先 Upgrade 到 E) |
| 6 | `F` | `Upgrade` | `—` | `E` | D4c:MESIF 特有 —— 升级 ⇒ sharer 全失效 ⇒ F 清空 ⇒ 干净独占 |
| 7 | `F` | `SnpShared` | `—` | `F` | D4d:MESIF 特有 —— 已是单 Owner + 有 sharer ⇒ 再来 sharer ⇒ 仍满足 F 的定义 |
| 8 | `F` | `SnpUnique` | `—` | `I` | D4e:MESIF 特有 —— 独占 snoop ⇒ Owner 转移 ⇒ Forward 清空 ⇒ 无副本 |
<!-- MESHIF-TABLE:deviations:END -->

> **风险分档**(人工评估,per design `D-ε.8`):**高** = D2(`E+SnpShared` → `F`,MESIF Forward 语义)
> 与 D4a~D4e(`F` 行 5 格,CHI **无对应态**);**中** = D3(`S+RdUnique` → `E`,MESIF 目录语义);
> **低** = D1(`I+SnpShared`,CHI 该格把「请求者拿到共享副本」写进了**响应者**的行)。

---

## 4. 跨仓动作登记(本仓侧待办)

| # | 动作 | 触发条件 | 执行主体 | 状态 |
|---|------|----------|----------|------|
| **X-1** | `protocol_instances.json` 的 `authority_migrations[mesif]` **降级 / 删除** | ArchForge 侧完成 `32 号 §6.2` 的追认(方案 A)⇒ 两端口径一致 | **本仓**(需人工裁决 + 台账 entry) | ⏳ **待触发** —— per `ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md` 的 AC-11;**本仓无权单方闭环**(per `ADR-CEP-12 §13` 的跨仓边界) |
| **X-2** | 追认完成后把 35 格回填进 ArchForge `32 号`(§7 之后新增小节)+ `25 号 §4.2` 的目录条目状态编码依据 | ArchForge 侧追认完成 | ArchForge | ⏳ **待 ArchForge 答复** |

---

## 5. 追认进度刻度(机器可查)

| 刻度 | 值 | 查询方式 |
|------|-----|----------|
| 待追认格数 | **31 / 35** | `MesifStateSpec::pending_count(MesifStateSpec::kTable)` |
| 一手原文格数 | **4 / 35** | `MesifStateSpec::primary_count(...)` |
| 转换表一手原文 | **3**(`E+SnpShared` / `F+SnpShared` / `F+SnpUnique`) | `MesifStateSpec::transition_primary_count(...)` |
| 投影表一手原文 | **1**(`F→WRITEBACK`,per `ArchForge 32 号:529`) | `MesifStateSpec::projection_primary_count(...)` |

> ⚠️ **判据口径边界**(per change 的 design `D-ζ`):本仓的判据锁**完备性 / 可审计性 / 待追认标记 /
> 内部自洽**,**不**锁推导值的**语义正确性** ⇒ 未追认的值**带着 `pending` 标记**留在规约里,
> **不**被固化为「既定事实」。⇒ 追认前**不得**据本表写实现层判据(队列 #9 的实现层 change)。

---

## 6. 维护记录

| 日期 | 版本 | 修订 |
|------|------|------|
| 2026-10-29 | v0.1 | 初版:30 格转换 + 5 格投影 + 8 条偏离 + 2 项跨仓待办(清单由载体生成) |
