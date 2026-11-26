# design: cep-ssp-declaration（批 1）

> **目标**：SSP 声明面 + loader + **V-24-2 逐格对账**。⛔ 不含解释器 / 装配 / 掩码格 / 实验 4 件套（批 2）。

## D-1 · 事件字母表的两层结构

`event_alphabet.hh` 只有 **14 核心锚**（封闭）+ `flow` 映射。**私有事件不在本头**（`kPrivateEventsNotModelledHere`）⇒ 私有事件由 loader / schema 校验（须 `flow` + `architect_approval`）。理由：先例库（已批准私有事件复用免批）是**运行时数据**，⛔ 不是编译期契约。

## D-2 · ⚠️ `flow` 映射是**部分**的（本设计的关键发现）

`Cap` / `Grow` / `Prune` / `Report` 是**本地权限转移**语义（TileLink-C 风格），⛔ 不对应任何 canonical 流 ⇒ `flow = nullptr`。

⇒ 若强行给它们编 flow（例如都算 `request`），会**误标语义**；若把 AC 写成「所有事件 flow ∈ 10 流」，则这 4 个锚**必然不合规**。

⇒ **裁定**：AC 的正确表述 = 「**非空** flow ∈ 10 流」；并在 `event_alphabet.hh` 用 `kFlowMappingIsPartial = true` + 注释 + spec 的诚实边界 #2 **三处**登记。

## D-3 · ⚠️ V-24-2 「逐格相等」的**精确口径**（对 ADR 措辞的收窄，须评审确认）

`ADR-CEP-24` 的 V-24-2 原文：「机判其 transition 表与 `mesif_state_spec.hh` **逐格相等**（含 `0xFF` 空洞与逐格 `Basis`）」。

**本 change 比较的字段**（`test/test_cep_ssp_declaration.cc`）：

| 比较 | 内容 |
|---|---|
| ✅ `states` / `events` 数与顺序 | 逐项 |
| ✅ `transition[i][j]` 目标态 | 逐格（`0xFF` 空洞 ⇒ `null`）|
| ✅ `perm[i]` | 逐格（4 值锁）|
| ✅ `basis_kind[i][j]` / `basis_pending[i][j]` | 逐格（**语义**字段）|
| ✅ `basis_projection_kind[i]` / `basis_projection_pending[i]` | 逐格 |
| ⛔ **不比较** `basis.ref` / `basis.note` | **引用元数据**（非语义）|

**不比较 `ref` / `note` 的理由**（⚠️ 这是对 V-24-2「逐格 Basis」的**口径收窄** ⇒ 须评审确认）：
1. `ref` / `note` 是**引用字符串**（本仓 MESIF 的 35 格全部指向 ArchForge 42 号文档）⇒ 比较它们等于测「引用转写」，⛔ **不测表达力**；
2. **用户自创协议没有外部引用**（`ADR-CEP-24` 决策 4 的 `UserDefined` 档明确允许 `ref` 为空）⇒ 若要求 `ref` 逐字相等，则 SSP 面**无法表达用户协议** ⇒ 与 S1 的目标直接冲突；
3. 语义等价 = `transition` + `perm` + `basis.kind` + `basis.pending`（「这格的依据是哪种强度、是否待追认」）⇒ 这是可跨协议比较的部分。

⇒ ⚠️ **若评审认为必须比较 `ref` / `note`**，则需：① 把 MESIF 的 35 条 `ref`/`note` 全量转写进 `ssps.json`（体量 ~7 KB）② 并在 spec 里声明「用户协议的 `ref` 允许为空」的例外 —— 这会引入「同一字段两套口径」。

## D-4 · 计数同步面（本 change 的 6 处）

闸 55→56 · `test_cep_*.cc` 80→81 · spec `cep-*` 50→51 · `scripts/README.md` · 登记册 §0.1 · `include/tlm/cep/AGENTS.md` 文件索引（新增 4 个声明源/头）。

## D-5 · 五步链（新主 spec 入冻结集）

主 spec `openspec/specs/cep-ssp-declaration/spec.md` ⇒ 冻结面新增 1（96 → 97）⇒ 五步链 1 次（`--authorized-by "Entry #553"`）。

## D-6 · delta 由主 spec **提取生成**（零漂移）

`openspec/changes/cep-ssp-declaration/specs/cep-ssp-declaration/spec.md` = 主 spec 的 `## Requirements` 段 + 标题换成 `## ADDED Requirements`（脚本提取，⛔ 不手写两份）。
