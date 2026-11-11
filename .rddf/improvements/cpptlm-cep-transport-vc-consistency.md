---
name: cpptlm-cep-transport-vc-consistency
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— 传输层 VC 语义一致性判据落地
依赖:
  - ADR-CEP-01
  - ADR-CEP-12
状态: 🟡 **待实施**（2026-11-12 起草;per 台账 **Entry #344** 的 W0 方案 + 用户「选 A」认可;metis gap 分析 1 轮已过（P0 6 / P1 5 / P2 3 **全部落盘**）;K-1 / K-2 两个阻塞前置**已由 architect 裁定、用户认可**（per `.rddf/reviews/cep-c2-vc-consistency-decision.md` §9 / §10））
created: 2026-11-12
---

# cpptlm-cep-transport-vc-consistency: 传输层 VC 语义一致性判据落地

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— 传输层 VC 语义一致性判据落地

> **状态**: 🟡 **待实施** —— 两个阻塞前置 `K-1` / `K-2` **已裁**（见下「已裁项」）;metis 的 14 条建议已全量落盘。**可立项**。
> **关联路线图**: [`phase-10`](../roadmap/phases/phase-10.md)(in-progress)+ [`objective-cep-platform`](../roadmap/objectives/obj-cep-platform.md)(**§13.3** C2 定义 · **§13.6** W2 · **Entry #340** 排序裁决 · **Entry #344** W0 + ⑨ K-1/K-2 裁决)
> **上游决策（已裁决，本草案⛔ 不重新讨论）**: `T-C2-⓪` = **条件化口径** · `T-C2-①` = **扩展 C-1′ 适用域** · `U-3` = `compute_noc` 上 `tilelink_c` 的差异是**合法协议差异**、⛔ 不改声明（per Entry #344）· **K-1** = `R-1`/`R-2` 落 `docs/architecture/cep/fabric-transport.md` 诚实边界段 · **K-2** = tag `R1-b`/`R1-c` + 锚点拆两处
> **metis 审查**: 1 轮，6 节全出;本草案已按其 **P0（6）/ P1（5）/ P2（3）** 逐条修订（见文末「处置对照」）
> **统一判据**: 每个新判据必须有一条**决定性变异**，且**旧实现必绿 / 新实现必红**（方法同 C1 的 AC-9，per 台账 Entry #342）;⛔ 不得靠「文件消失」才红;⛔ 不得靠改声明数据变绿

---

## 已裁项（planner 不代裁，已由 architect 裁定并落盘）

裁决全文：`.rddf/reviews/cep-c2-vc-consistency-decision.md` **§9 / §10 / §11**;台账 **Entry #344 ⑨**。

| # | 已裁内容 | 落点 |
|:--:|---|---|
| **K-1** | `R-1` / `R-2` **不进 C-4**，登记进 `docs/architecture/cep/fabric-transport.md` 已有的**「诚实边界」段**（`:106-112`）。⛔ 不扩 `UntestedRegistry`（注入点已存在 ⇒ 「扩展来源」是伪需求）· ⛔ 不新建登记面 | In-Scope #4 |
| **K-2** | 锚点**按主题拆两处**：(b) → **`ADR-CEP-12`** 的 `## Status Update`，tag = **`[CP-03-VC-CERTIFICATE/R1-b]`**;(C-1′-S) → **`ADR-CEP-01`** 的 `## Status Update`，tag = **`[CP-03-VC-CERTIFICATE/R1-c]`**。⚠️ (a) 保留既有 `/C-1′` tag **不动**，文档须写明「**`R1-a` ≡ `C-1′`**」 | In-Scope #1/#2/#3 |

> **K-2 的强依据（不是新增决策，是兑现既有承诺）**：`include/tlm/cep/protocol_instance.schema.json:180` 的 description 早写「…一致性由**装配期校验**承担（per `INV-CEP-12-IC2`）」，而 **`INV-CEP-12-IC2` 全文只保证 key 可解析**（`docs/adr/ADR-CEP-12-protocol-engine-capability-map.md:160`）⇒ **该承诺至今无承担者** ⇒ 这是**已存在的决策-实现缺口**。

---

## 范围

**In-Scope（2 项新判据 + 1 项文档订正 + 1 项登记）**:

1. **(b) 新增 ordering 一致性判据** —— 同 scope（`certificate_scope` 内的 network），逐 VC 比对 `ordering_assumption[vc]` **全等**;并校验 VC 完备性（`vc 0..vc_count-1`）。tag = **`[CP-03-VC-CERTIFICATE/R1-b]`**。⚠️ **这是本 change 的核心增量**。
2. **(C-1′-S) 新增 scope 完备性判据** —— 对每个 `decl.plane_binding` 指向 `is_local_model == false` 的 plane 的实例，**`(plane, network)` 对**必须出现在 `certificate_scope` 内（⛔ 不是只比 network）。tag = **`[CP-03-VC-CERTIFICATE/R1-c]`**。
3. **(文档订正)** `docs/architecture/cep/fabric-transport.md` 的 **R5 表**「当前机器判定」列更新 + R1 段 scope 表述订正 + **`:145` 的 `R1-c` 悬空引用**（⚠️ 它**已由 In-Scope #2 定义**，不必"定义或删除"二选一）+ **写明 `R1-a ≡ C-1′` 映射**。
   > ⚠️ **本项 0 行代码**。(a) 的**判据逻辑本无需改动**（`include/tlm/cep/collective_plane_contract.hh:905-946` 早已与 network 无关）⇒ ⛔ 本 change **不碰** `certificate_scope` 的驱动逻辑，也不把 `compute_noc` 加进 scope。
4. **(登记)** `R-1` / `R-2` 登记进 **`docs/architecture/cep/fabric-transport.md` 的「诚实边界」段**（`:106-112`，per K-1），并**写明地位差异**：U-1/U-2 有 C-4 机器守门「未被冒充成已满足」;**R-1/R-2 没有**（纯文档级）。

**Out-of-Scope（明确不做）**:

- ⛔ **不改任何 ADR 正文**（`ADR-CEP-01` / `ADR-CEP-12` 不动;K-2 的锚点 ⇒ 只在**末尾追加 `## Status Update`**）
- ⛔ **不改任何声明数据**：`compute_noc` / `tilelink_c` 的 `vc_binding` 与 `ordering_assumption` **一字不改**（per `U-3`;AC-13 机器守门）
- ⛔ **不把 `compute_noc` 加进 `certificate_scope`** —— 它是 M-1 网络，不是"漏登"
- ⛔ **不抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）**（选项 B 已否决：共用消费者实测 0 个）
- ⛔ **不给 `VcBindingSnapshot` 加 `plane_binding`** —— `decl.plane_binding` 已存在且已被 V6-c 消费（metis P0-3）
- ⛔ **不扩 `UntestedRegistry`**（K-1;注入点已存在）
- ⛔ **不改 `include/tlm/cep/protocol_instance.schema.json` 的 description**（K-2：它是决策 5 的转录;在 ADR SU 里澄清即可）
- ⛔ **不实施 M-1 回退机制**
- ⛔ **不含 C3 / C4**（D1「Proposed 并发 ≤ 1」串行约束）

---

## Why

**(b) 与 scope 完备性当前完全无机器判定**，且既有文档把 (a) 的域受限读成了「判据能力不足」（不准确）。实测证据：

| 缺口 | 实测（可复核） | 不修的后果 |
|---|---|---|
| **(b) ordering 无判定** | `grep -c ordering_assumption include/tlm/cep/collective_plane_contract.hh` = **0** | `p2p_ordered` 与 `unordered` 混用同一 VC ⇒ **保序语义被稀释** ⇒ 跨实例排序假设不一致**不可发现** |
| **(a) 的适用域是自声明** | `certificate_scope` = **1 项**（`FabricCollectivePlane` / `ualink_fabric`），**人手写的清单**，无独立真源校验 | 写漏一个 network ⇒ 判据**静默不覆盖**该网，且**没有任何闸会抱怨** |
| **schema 挡不住 (b) 的不完备** | `include/tlm/cep/protocol_instance.schema.json:180` 的 `ordering_assumption` 每 network 仅 `minProperties: 1` | 某实例只登记 vc0、漏 vc1-3 ⇒ **schema 合法** ⇒ 只能靠判据挡 |
| **决策文本的承诺无承担者** | `protocol_instance.schema.json:180` 说「一致性由装配期校验承担（per `INV-CEP-12-IC2`）」，而 IC2 只保证 key 可解析 | 读者以为已有校验 ⇒ 实际**没有** |
| **文档表述误导 + 悬空引用** | R5 表写「(b) 无载体」;`:145` 出现 `R1-b / R1-c` 而 **`R1-c` 全文从未定义** | 读者会以为「判不了」是**能力缺失**;`R1-c` 无定义 ⇒ 不知指什么 |

**⚠️ 对台账 §13.3 的一处理解更正（采用 Entry #344 的结论）**：`compute_noc`「无判定」的根因**不是判据能力** —— C-1′ 逻辑**早已与 network 无关**（`include/tlm/cep/collective_plane_contract.hh:905-946`）。根因是**作用域是自声明数据**。⇒ In-Scope #2（C-1′-S）才是 (a) 的真实增量。

**残余不可判项（per K-1 登记进「诚实边界」段;⛔ 不得宣称已全覆盖）**:

| # | 残余 | 为什么不可判 |
|:--:|---|---|
| **R-1** | `plane_binding` 自身漏登记（实例实际用 fabric 承载 plane 但 `plane_binding` 为 null）⇒ C-1′-S **漏判** | `plane_binding` 是自声明数据，无独立真源 |
| **R-2** | 「不一致的 network **确实**走了 M-1」不可判 | 本仓无 M-1 配置载体 |

**已知的表征局限（须写入文档/注释，⛔ 不得当作缺陷）**:

- **(L-1) 单实例 network 上空转**：scope 内 network 只有 1 个实例时，(a)/(b) 的「全等」天然成立 ⇒ **恒绿、无鉴别力**。今天 `ualink_fabric` 有 **3** 个实例（不空转），但该局限应登记。
- **(L-2) (b) 只证「跨实例一致」，不证「逐类正确」**：`ualink_fabric` 的 **vc 3 同时承载 `response` 与 `snoop_resp`**（3 实例皆如此）⇒ 而 `ordering_assumption[vc]` 是**每 VC 一个值**，无法表达「`response` 需 ordered 而 `snoop_resp` 需 unordered」。这是 schema 的既有语义（「排序是 VC 的静态属性」），**不是本 change 引入的缺陷**，但 ⛔ 不得把 (b) 的绿当作「排序正确性」证明。

---

## What Changes

| # | 变更 | 文件 | 形态 |
|:--:|---|---|---|
| 1 | 快照**新增** `per_network_orderings`（`network → [vc(string) → ordering]`） | `include/tlm/cep/collective_plane_contract.hh` | `VcBindingSnapshot` 加**一个**字段 + 收集逻辑。⚠️ 收集口径**必须与 (b) 的实例集合一致**（AC-9） |
| 2 | **新增 (b) 判据**：逐 VC 全等 + VC 完备性 + 越界/非规范 key 处置 | 同上（V6-d 段内） | tag = `[CP-03-VC-CERTIFICATE/R1-b] `（K-2） |
| 3 | **新增 (C-1′-S) 判据**：`(plane, network)` pair ⊆ `certificate_scope` | 同上（V6-d 段内） | tag = `[CP-03-VC-CERTIFICATE/R1-c] `;读 **`decl.plane_binding`**（已解析于 `:181`、`:226-243`，V6-c 在 `:830` 已消费）⇒ ⛔ **不新增快照字段** |
| 4 | `R-1` / `R-2` 登记 + 地位差异说明 | `docs/architecture/cep/fabric-transport.md` 的「诚实边界」段（`:106-112`） | 纯文档;⛔ 不得含「已验证 / 已满足」字样（AC-16） |
| 5 | R5 表同步 + R1 段 scope 表述 + `R1-a ≡ C-1′` 映射 + `R1-c` 处置 | 同上 | ⚠️ **冻结产物** ⇒ 五步授权链 |
| 6 | **两份 ADR 各追加 1 则 `## Status Update`**（K-2 锚点） | `docs/adr/ADR-CEP-01-*.md` + `docs/adr/ADR-CEP-12-*.md` | ⚠️ **均在冻结集** ⇒ 五步授权链 |
| 7 | 新建 OpenSpec change | `openspec/changes/<name>/` | 改**公共契约头** ⇒ 属「公共 API」⇒ 按 `AGENTS.md` 必须走 OpenSpec |

---

## Acceptance

> ✅ K-2 已裁 ⇒ AC-3 / 4 / 5 / 10 / 11 / 12 的**红因 tag 已钉死**，从「只验红」升级为「**验红 + 验红因**」。

- [ ] **AC-1（决定性变异 · (a) 消息类）**：在 `certificate_scope` **内**的 network 上，把某实例的某 VC 改成另一消息类 ⇒ **必红**，红因归因 `[CP-03-VC-CERTIFICATE/C-1′]`。
  > ⚠️ **反向判据（metis P1-8）**：做反向变异时**只改 `compute_noc` 子映射**，且**必须用同时拥有双网 `vc_binding` 的实例**（`moesi_gpu` / `mesif`）—— `ualink_4` **只有** `ualink_fabric`，用它做不了这个反向。若在 `compute_noc`（scope 外）上改**也红** ⇒ **判据实现错了**（scope 泄漏）⇒ 回到 `T-C2-⓪`，⛔ 不得改声明来"修"。
- [ ] **AC-2（基线仍绿）**：`ualink_fabric` 侧未破坏时 **rc=0**（实测：3 实例的 (a)(b) 全一致）。
- [ ] **AC-3（决定性变异 · (b) ordering）**：把某实例某 VC 的 `ordering` 从 `p2p_ordered` 改成 `unordered`（或反向）⇒ **必红**，红因**必须**归因 `[CP-03-VC-CERTIFICATE/R1-b]`。
  > ⭐ **本 AC 是 (b) 的唯一决定性变异**：同一变异在**旧实现上必绿**（`ordering_assumption` 在旧实现 0 处）⇒ 按「旧绿 / 新红」**并排取证**。
- [ ] **AC-4（决定性变异 · (b) VC 不完备）**：删某实例某 VC 的 `ordering` 条目 ⇒ **必红**（schema 仍合法，仅 `minProperties: 1`），红因归因 `R1-b`。
- [ ] **AC-5（决定性变异 · C-1′-S scope 完备性）**：把 `compute_noc` 上的某实例的 `plane_binding` **新增**为 `FabricCollectivePlane`（⚠️ 措辞：当前该实例**不在** `plane_binding` 里 ⇒ 是**插键**不是改值）⇒ **必红**，红因**必须是**「`(plane, network)` 不在 `certificate_scope`」并归因 `[CP-03-VC-CERTIFICATE/R1-c]`（⛔ **不是**「plane 未登记」）。
  > ⚠️ **判定顺序必须固定**（metis 边界）：`find_collective_plane` 未登记 ⇒ FAIL（该情形**已由 V6-c `:830` 覆盖**）→ 已登记但 `(plane,network)` 不在 scope ⇒ FAIL（本判据）。否则归因不稳定。
- [ ] **AC-6（反 vacuous · 既有基线）**：`certificate_scope` 清空 ⇒ **必红**。⚠️ 本 AC 触发的是**既有** `:901-904` 的空早退，**不是** C-1′-S 的证词（metis slop #5）⇒ 只作回归基线，⛔ 不得用它证明新判据有牙。
- [ ] **AC-7（文档同步）**：R5 表「当前机器判定」列逐条更新（R1 不再写「(b) 无载体」/「`compute_noc` 无判定」）;R1 段订正为「作用域 = 声明 M-2 的 network（`compute_noc` **按裁决不属 M-2**，非"未覆盖"）」;**写明 `R1-a ≡ C-1′` 映射**;**`:145` 的 `R1-c` 表述改为「已由 `R1-c` 判据覆盖」**（per K-2：它已由 In-Scope #2 定义）。
- [ ] **AC-8（0 回归 + 冻结链 · 3 处）**：`ctest --test-dir build -j4` 全绿 + `[cep]` 全绿 + **35 个** `cep_*.sh` 闸 rc + ✗ 双判全绿 + `docs_sync_check.sh --strict` rc=0 + **冻结面五步授权链**走完（⚠️ **3 处产物**：`docs/architecture/cep/fabric-transport.md` + `ADR-CEP-01` + `ADR-CEP-12`）。判据 = `bash scripts/test/cep_freeze_check.sh` **无参 rc=0** 且 `artifact_count` **不新增**。⚠️ 三处可在**同一次** `--update --authorized-by` 内重算，但**产物 commit 必须先于 `--update`**（C1 已验证的顺序）。
- [ ] **AC-9（(b) 的实例集合与缺失行为，metis P0-4）**：显式规定「集合 = 该 network 上**所有有 `vc_binding` 的实例**」;任一此类实例**缺** `ordering_assumption[network]` ⇒ **FAIL**（⛔ 不得以 `ordering_assumption` 为驱动 —— 那会静默把缺失实例当子集排除）。
  > 决定性变异：构造「有 `vc_binding[network]` 但无 `ordering_assumption[network]`」的实例 ⇒ **必红**。
- [ ] **AC-10（越界/非规范 VC key，metis P1-7）**：`ordering_assumption` 的 key 是**字符串数字**（`"0"`），而 `vc_count` 是整数 ⇒ 须显式 `stoul` 并明确三类策略：① 非数字 ⇒ **FAIL**;② 前导零（`"04"`）⇒ **FAIL**（规范化拒绝，避免与 `"4"` 语义分裂）;③ `>= vc_count` ⇒ **FAIL**。三类各一条决定性变异，红因归因 `R1-b`。
- [ ] **AC-11（C-1′-S 的 vacuous 探测，metis P1-7）**：绑 `FabricCollectivePlane` 的实例其 `vc_binding` 为**空** ⇒ 必须给出明确结论（**建议 FAIL**，与 (a) 的「无实例 ⇒ FAIL」对称），⛔ 不得 vacuous green，红因归因 `R1-c`。
- [ ] **AC-12（C-1′-S 的「旧绿 / 新红」并排，metis P1-7）**：AC-5 的同一变异在**旧实现上必绿**（旧实现无此判据）⇒ 并排取证，对称 AC-3。
- [ ] **AC-13（声明数据零改动的机器证据，metis P1-10）**：`git diff --exit-code -- include/tlm/cep/protocol_instances.json include/tlm/cep/collective_planes.json include/tlm/cep/fabric.json` 为**空**（防实施中顺手「修」`tilelink_c`）。
- [ ] **AC-14（归因 tag 精确前缀不冲突，metis P0-2 / K-2）**：新 tag `[CP-03-VC-CERTIFICATE/R1-b]` / `[CP-03-VC-CERTIFICATE/R1-c]` 不得被 `[CP-03-VC-CERTIFICATE/C-1′]`（带 `′] ` 终结）或 `[CP-03-VC-CERTIFICATE]` 的**精确前缀**误命中 ⇒ 加一条回归钉住（⚠️ 既有 `test/test_cep_ualink_inc.cc:465` 用**带 `]`** 的 `rfind` ⇒ 理论上安全，但需回归）。
- [ ] **AC-15（真实 gate 路径可达，metis slop #6）**：新判据必须在**真实 gate**（`include/tlm/cep/collective_plane_contract.hh` 的 `run_collective_plane_contract` 调用链，`:1447` 附近）可触达，⛔ 不得只在测试里手搓 `decl` 直调 `evaluate_shared_mode_certificate`。
- [ ] **AC-16（K-1 · `R-1`/`R-2` 登记的文字守门）**：`R-1` / `R-2` 在 `docs/architecture/cep/fabric-transport.md` 的「诚实边界」段**可 grep 命中**，且该段 **⛔ 不含「已验证 / 已满足」字样**（与 C-4 的 `untested_is_not_satisfied` 同型）。决定性变异：把 `R-1` 的表述改成「已验证」⇒ 必红。
- [ ] **AC-17（K-1/K-2 · 文档地位与映射）**：文档显式写明 ①「**`R1-a` ≡ `C-1′`**」映射 ②「**U-1/U-2 有 C-4 机器守门 / `R-1`/`R-2` 没有**（纯文档级）」的地位差异。逐字可核。

---

## Capabilities

- **排序假设不再静默漂移**：(b) 落地后，同一 network 上混排实例的 VC 排序语义不一致 ⇒ 提交前即红。
- **作用域不再靠自觉**：C-1′-S 让 `certificate_scope` 从「声明」变为「声明 + 受校验」⇒ 「写漏一个 network 就静默不覆盖」的洞被关闭。
- **兑现既有承诺**：`protocol_instance.schema.json:180` 宣称的「装配期一致性校验」自此有真实承担者（此前是无承担者的文本）。
- **判据语义与文档一致**：R5 表不再把「不属 M-2 的 network 不判」误述为「无判定 / 能力缺失」;`R1-c` 悬空引用收口。
- **残余诚实**：`R-1` / `R-2` 显式登记并**标明「无机器守门」**;`L-1` / `L-2` 两类表征局限写入文档 ⇒ 不把「不可判 / 不空转」伪装成「已覆盖 / 已证明」。

---

## Impact

| 面 | 影响 |
|---|---|
| **源码** | 1 个 header（`include/tlm/cep/collective_plane_contract.hh`，**CP-03 共享头**）⇒ ⚠️ 触契约面 ⇒ 必须走 OpenSpec |
| **冻结面（3 处）** | ⚠️ `docs/architecture/cep/fabric-transport.md`（`docs/architecture/cep/*.md` glob）+ ⚠️ `docs/adr/ADR-CEP-01-*.md` + ⚠️ `docs/adr/ADR-CEP-12-*.md`（`docs/adr/ADR-CEP-*.md` glob）⇒ **全在冻结集** ⇒ AC-8 的**五步链是 3 处**;✅ `include/tlm/cep/collective_plane_contract.hh` **不在**冻结集（已核） |
| **ADR** | **零正文改动**;K-2 的锚点 = 两份 ADR 各**末尾追加 1 则 `## Status Update`**（触冻结 hash ⇒ 见上） |
| **声明数据** | **零改动**（`include/tlm/cep/protocol_instances.json` / `include/tlm/cep/collective_planes.json` / `include/tlm/cep/fabric.json` 一字不改，AC-13 机器守门）;✅ `R-1`/`R-2` **不进** `UntestedRegistry`（K-1） |
| **测试** | 需覆盖 AC-3 / AC-4 / AC-5 / AC-9 / AC-10 / AC-11 / AC-12 / AC-14 / AC-15 / AC-16 的反向 fixture;不新增磁盘 `.sh` 闸（若新增 ⇒ 磁盘数 35 → 36 ⇒ 须同步 `scripts/README.md` / `AGENTS.md` 计数口径） |
| **D1 槽位** | **占 1 个 Proposed**（当前 CEP 系列 0 个 active ⇒ 可占）;⛔ 串行：C3/C4 须待本 change 归档后才可立项 |
| **下游** | R2/R3/R4 不受影响（R2 对 `vc_binding` 语义恒真、R3/R4 已有载体） |

---

## 处置对照（metis 14 条 + K-1/K-2）

| 来源 | 编号 | 建议 | 处置 |
|---|---|---|---|
| metis | **P0-1** | K-1 先裁再立项;状态改「阻塞」 | ✅ 已裁（§9）⇒ 状态现为 🟡 待实施/可立项 |
| metis | **P0-2** | 补规约锚点，禁借 `C-1′` 之名;tag 改 `R1-b`/`R1-c` + 冲突回归 | ✅ K-2 已裁（§10）⇒ tag 钉死;AC-14 加冲突回归 |
| metis | **P0-3** | 修正 What Changes 第 3 行（`plane_binding` 已在 `decl`） | ✅ 已改;Out-of-Scope 明列「⛔ 不加 `plane_binding` 进快照」 |
| metis | **P0-4** | 钉死 (b) 实例集合与缺失行为 | ✅ **AC-9** |
| metis | **P0-5** | C-1′-S 改 `(plane, network)` pair + 固定判定顺序 | ✅ In-Scope #2 + AC-5 |
| metis | **P0-6** | In-Scope #1 降级为文档订正 | ✅ (a) 明标「0 行代码」（In-Scope #3） |
| metis | **P1-7** | 补 4 条 AC | ✅ AC-9 / AC-10 / AC-11 / AC-12 |
| metis | **P1-8** | AC-1 反向变异限定 + 用双网实例 | ✅ AC-1 |
| metis | **P1-9** | AC-7 扩到 `R1-c` 悬空引用 | ✅ AC-7（且 K-2 使其变为「已由本 change 定义」） |
| metis | **P1-10** | 机器证据 AC（声明数据零改动） | ✅ **AC-13** |
| metis | **P1-11** | AC-8 冻结链补具体命令 | ✅ AC-8（且从 1 处扩为 **3 处**，per K-2） |
| metis | **P2-12** | 记录 vc3 多类 ⇒ (b) 只证跨实例一致 | ✅ Why 的 **L-2** |
| metis | **P2-13** | 记录单实例 network 空转 | ✅ Why 的 **L-1** |
| metis | **P2-14** | 订正 F10：`configs/cep_chi_rnf_e2e_a.json` 无 `protocol_instance` 值 | ✅ 原「8 个 config 每个恰好 1 个」**错误**，已订正为：`configs/cep_*.json` 共 **9** 份，**8 份恰 1 个** + `configs/cep_chi_rnf_e2e_a.json` **0 个**（只有复数键 `protocol_instances` = 文件路径）⇒「多实例共享消费者 = 0」结论不变 |
| **K-1** | — | `R-1`/`R-2` 落点 | ✅ In-Scope #4 + **AC-16** + **AC-17** |
| **K-2** | — | 锚点拆两处 + tag 钉死 | ✅ In-Scope #1/#2 + What Changes #6 + AC-8（3 处） |
