# cep-mesif-state-spec Specification

## Purpose
定义 **MESIF**(`mesif` 协议实例,per `ADR-CEP-12` 决策 1~10 + `ADR-CEP-11 §3` 锁表 `mesif-1.0`)的
**规约层**契约:**5 态状态机** + **5→4 `perm` 投影表** + **逐格推导依据** + **待追认标记**。

**背景**:ArchForge 全仓检索(2026-10-29)确认 MESIF **只有 5 态词表**,**无**状态转换规约、**无** 5→4 投影表
⇒ 队列 #9 `cpptlm-cep-mesif-adapter` 的**实现层**缺少可判据化的语义来源。本能力交付**规约层**,
实现层由队列 #9 承担。

> **边界**:不改 `protocol_instance.schema.json`(冻结面)· 不改 `adapter_schema_contracts.hh` ·
> 不改 `CohIfBundle` · 不扩 `perm` 4 值 · 不引入新的 SharerTracker 实现 ·
> **不**实现 `MesifAdapter`(队列 #9)。

## ADDED Requirements

### Requirement: cep-mesif-state-spec

CEP SHALL 为 **MESIF** 提供:5 态状态机规约 + 5→4 `perm` 投影表 + **逐格推导依据** + **待追认标记**。

> **语义分层**:规约 = **协议语义**;`perm` 投影 = **跨域接口表达**;SharerTracker = **目录形态**
> (三者**正交**,per `ADR-CEP-02` 的分层纪律)。本能力只交付前两者。

#### Scenario: MS-01 —— MESIF 的 5 态 SHALL 全部有 4 值 `perm` 投影

- **WHEN** 核对 MESIF 规约的状态 → `perm` 投影
- **THEN** 子条件 ① SHALL 成立:`M` / `E` / `S` / `I` / `F` **每个**态 SHALL 有 4 值投影
  (缺一即红)—— 4 值锁不破(per `ADR-CEP-02` CEN-Ib11)
- **AND** 子条件 ② SHALL 成立:`I` 的投影 SHALL 为 `NONE`;`F` 的投影 SHALL 为 `WRITEBACK`
  (per ArchForge `32 号:529` 的一手原文)
- **AND** 子条件 ③ SHALL 成立:**瞬态 SHALL 无投影**(per `INV-CEP-05-IC1`)—— 本规约当前**不声明瞬态**
  ⇒ 该子条件平凡成立,但载体 SHALL 保留瞬态列表以支持未来追认后的扩展
- **AND** **反向 fixture**:让某个态**无投影** ⇒ 必红;给瞬态加投影 ⇒ 必红
- **AND** **正控**:`chi_rnf` / `moesi_gpu` 的既有投影 SHALL **零回归**

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-01 断言 + 2 条反向 fixture + 正控断言

#### Scenario: MS-02 —— MESIF 的状态转换表 SHALL 完备,且**写只在独占态合法**

- **WHEN** 核对 MESIF 的 (状态 × 事件) 转换表
- **THEN** 子条件 ① SHALL 成立:每个 (态 × 事件) 组合 SHALL 有定义(无空洞)—— 5 态 × 6 事件 = **30** 格
- **AND** 子条件 ② SHALL 成立:`E + Wr → M` 的转换 SHALL 存在(目标态 = `M`)
- **AND** 子条件 ③ SHALL 成立:**写只在独占态合法** ⇒ `S` 上的写 SHALL **状态不变**(须先 `Upgrade`)
  (per ArchForge `25-nsa:552-554` 的 Owner 唯一性 + 同型先例 IHI0050 §B4.1)
- **AND** 子条件 ④ SHALL 成立:`perm` 秩 SHALL **单调**(升级路径上不降;`E`→`M` 严格升高)
- **AND** **反向 fixture**:挖掉转换表的一格 ⇒ 必红;让 `S` 上的写改变状态 ⇒ 必红;秩下降 ⇒ 必红
- **AND** **正控**:`chi_rnf` 的同类断言 SHALL 仍 PASS

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-02 断言 + 3 条反向 fixture + 正控断言

#### Scenario: MS-03 —— 规约的每一格 SHALL 带**推导依据**与**待追认状态**

- **WHEN** 核对规约的推导可审计性
- **THEN** 子条件 ① SHALL 成立:转换表的**每一格** SHALL 带**非空** `basis`(依据来源锚点)
- **AND** 子条件 ② SHALL 成立:依据为「本仓推导 / 类比」而非 ArchForge 一手原文的格 SHALL 带
  **待追认**标记 ⇒ 跨仓债**可见**(不静默放过)
- **AND** 子条件 ③ SHALL 成立:**待追认格数 SHALL 可查**(供「追认进度」观测)
- **AND** 子条件 ④ SHALL 成立:一手原文格 SHALL 引用 **ArchForge 的具体位置**(文件 + 行/条款)
- **AND** **反向 fixture**:清空某一格的 `basis` ⇒ 必红;把一手原文格的依据改成推导 ⇒ 必红(标记与依据不一致)
- **AND** **正控**:`F` 态投影的 `basis` SHALL 指向 `32 号:529`(一手原文)⇒ 待追认计数**不**含它

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-03 断言 + 2 条反向 fixture + 正控断言

#### Scenario: MS-04 —— `F` 与 `M` 的投影合并 SHALL 被**显式登记**语义损失

- **WHEN** 核对 `F`(Forward:单 Owner + 多 Sharer)与 `M` 同投影 `WRITEBACK` 的后果
- **THEN** 子条件 ① SHALL 成立:该合并 SHALL 被**显式登记**(不得静默)
- **AND** 子条件 ② SHALL 成立:登记 SHALL 注明「是否还有 sharer」在 4 值下**不可分辨**
- **AND** 子条件 ③ SHALL 成立:登记 SHALL 注明该损失**可由 Sharer Tracker 补偿** ——
  `mesif` 的 `sharer_tracker.impl = HierarchicalDirectory`(caps `SYSTEM`/`EXACT`/`GLOBAL`/`HIERARCHICAL`,
  per `ADR-CEP-04 §13`)⇒ EXACT 目录**能**回答「谁持有」
  ⇒ ⚠️ 与 `chi_rnf` 的 `SD`/`UD` 合并损失(**不可补偿**,RN 侧无 sharer set)**措辞必须不同**
- **AND** **反向 fixture**:登记缺「可由 Sharer Tracker 补偿」的限定 ⇒ 必红(误用 `chi_rnf` 的措辞)

> **锚点**:`test_cep_mesif_state_spec.cc` 的 MS-04 断言 + 1 条反向 fixture
