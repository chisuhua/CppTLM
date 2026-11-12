# cpptlm-cep-transport-vc-consistency: 实施计划（C2 · 传输层 VC 语义一致性判据落地）

> **来源**:`openspec/changes/cpptlm-cep-transport-vc-consistency/`(proposal / design / tasks)+ `.rddf/improvements/cpptlm-cep-transport-vc-consistency.md`(17 AC)
> **上游裁决**:`.rddf/reviews/cep-c2-vc-consistency-decision.md` §1~§12;台账 **Entry #344**(W0 + K-1/K-2)· **#345**(立项)· **#346**(metis gap 分析处置)
> **⚠️ 本 plan 是 `openspec/changes/cpptlm-cep-transport-vc-consistency/tasks.md` 的 TDD 5 步展开**,⛔ **不新增决策** —— 任何新取舍须回 architect。

---

## Goal (目标)

把传输层 VC 语义一致性的两个**当前无机器判定**的维度变成**可拦截的判据**,且**零声明数据改动、零 ADR 正文改动**:

| 判据 | 交付 | 归因 tag |
|---|---|---|
| **(b) / `R1-b`** | 同 network 上混排实例的 `ordering_assumption[network][vc]` 逐 VC **全等** + **每实例 VC 完备** + **key 形态规范** | `[CP-03-VC-CERTIFICATE/R1-b]` |
| **(C-1′-S) / `R1-c`** | 声明了 plane 绑定的实例,其 `vc_binding` 的每个 **`(plane, network)` 对** MUST 在 `certificate_scope` 内 | `[CP-03-VC-CERTIFICATE/R1-c]` |

**完成定义 (DoD)**:
1. **17 条 AC** 全部有工具证据(`bash` rc + 输出片段 / `git diff` 输出)
2. **9 条决定性变异**逐条「破坏 ⇒ 按**正确原因**红 ⇒ 还原 ⇒ 绿」,且**旧实现必绿 / 新实现必红**(AC-3 / AC-12 需**跨实现并排取证**)
3. **既有 6 条反向 fixture 的归因前缀逐一不变**(⛔ 「仍绿」**不等于**「归因未变」)
4. `ctest -j4` 全绿 + `[cep]` 全绿 + **35 个** `cep_*.sh` 闸 **rc + ✗ 计数双判**全绿
5. 冻结面五步授权链走完(**3 处产物**:`docs/architecture/cep/fabric-transport.md` + `ADR-CEP-01` + `ADR-CEP-12`;`artifact_count` 仍 **78**)
6. 声明数据 **4 个文件零 diff**(AC-13 机器守门)
7. 台账 Entry 落盘(含实施回执 + metis 18 条建议的处置核对)

---

## 关键约束(设计要点摘要)

| 约束 | 来源 | 影响 |
|------|------|------|
| **不新增 `ContractRule`** | `include/tlm/cep/collective_plane_contract.hh:876`(`kContractRuleCount == 6` 是**被测试硬断言**的契约面) | 两条新判据作为 **`CP-03` 行的子判据** ⇒ ⛔ 不得新增第 7 条规则 |
| **归因 id 纪律** | `ADR-CEP-01` §E.3.1 + `:873-888`(4 个 tag 常量**均以 `] ` 尾随空格结尾**) | 新 tag **必须**带尾随空格;归因按**完整前缀精确相等**匹配(⛔ 不用子串前缀) |
| **⛔ 不用 `C-1′-b` / `C-1′-c`** | 台账 Entry #344 ⑨ 的 K-2(选项 C 否决) | `C-1′` 在 `ADR-CEP-01 §E.3.1` 只定义槽位继承 ⇒ 借其名会掩盖这是新决策 |
| **条件化口径** | 台账 Entry #344 的 `T-C2-⓪` | **仅** `certificate_scope` 内的 network 参与逐 VC 一致性;`compute_noc` 按 `ADR-CEP-01 §E.3.3` **不属 M-2** ⇒ 允许不一致 ⇒ ⛔ **不得**把它加进 scope |
| **⛔ 零声明数据改动** | 台账 Entry #344 的 `U-3` | `tilelink_c` 的 `compute_noc` 映射差异是**合法协议差异** ⇒ `include/tlm/cep/protocol_instances.json` / `include/tlm/cep/collective_planes.json` / `include/tlm/cep/fabric.json` / `include/tlm/cep/protocol_instance.schema.json` **一字不改** |
| **⛔ 零 ADR 正文改动** | `AGENTS.md`「ADR 不可变」+ `ADR-CEP-00` SU §⑤ | 只在**末尾追加** `## Status Update` ⇒ 触冻结 hash ⇒ 走五步链 |
| **禁双实现** | `ADR-CEP-01` §E.4(边按 network 声明 ⇒ 诱导子图恒无环 ⇒ 无环闸对 `vc_binding` 语义**结构性恒真**) | 判据只在 `include/tlm/cep/collective_plane_contract.hh`;⛔ 不抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）(共用消费者实测 **0** 个);⛔ **不得**以「重跑 `evaluate_channel_dependency_gate`」当证书 |
| **⛔ 不扩 `UntestedRegistry`** | 台账 Entry #344 ⑨ 的 K-1(注入点已存在 ⇒「扩展来源」是伪需求) | `R-1`/`R-2` 落 `docs/architecture/cep/fabric-transport.md` 的**诚实边界段** ⇒ **纯文档级** ⇒ ⛔ **不得**声称它们有机器守门 |
| **⛔ 产物 commit 先于 `--update`** | C1 已验证的顺序(台账 Entry #342) | 五步链顺序不可颠倒 |
| **`openspec` MODIFIED 场景名守恒** | 实测(`validator.js` 的 `diffScenarioNames`) | delta 的既有 6 个 Scenario **逐字保留** ⇒ 改名会被判「omits scenario」 |
| **⛔ 不改既有 C-3 fixture 的变异形态** | metis P0-1 的实测(Entry #346 ⑨) | `R1-c` 置于 scope 循环**之后** ⇒ 既有 6 条反向 fixture **零改动** |

---

## Work Units

> **TDD 5 步 canonical markers**(`rdd-doctor` 的 `plan-tdd` 检查):
> 1. **Write the failing test** 2. **Run test to verify it fails** 3. **Write minimal implementation** 4. **Run test to verify it passes** 5. **Refactor**(可选)
>
> ⛔ **每条判据 = 先写变异(红) → 实现(绿) → 净(0 回归)**;⛔ **不得**先写实现再补变异(那样变异是「照着实现写的」⇒ 无法证明判据有牙)。

### WU-0 · 授权门(G1 · A0)

- [ ] **A0.1** 台账 Entry **#347** 授权本 change 的实施(⚠️ **#346 已被 metis 处置占用** ⇒ 授权号 = **#347**);该 Entry 同时充当 A6.2 的 `--authorized-by` 授权记录(per C1 先例:Entry #342 = 「C1 开工授权」)
- [ ] **A0.2** 登记 §13 实施路径推进:Entry #344 的 **W2** 波次 → 本 change **实施中**
- [ ] **A0.3** 声明数据零改动基线:`git diff --exit-code -- <4 个文件>` 为空 + 记 4 个 sha256
- [ ] **A0.4** 既有 V6-d 四证书基线绿:`./build/bin/cpptlm_tests "[cep][ualink-inc]"` 全绿(改动前对照)

### WU-1 · 快照字段(G2 前半 · A1)

- [x] **A1.1** `VcBindingSnapshot` 新增**一个**字段 `per_network_orderings`(`network → vector<pair<instance_id, map<vc_string, ordering>>>`)—— per design **D-2**
      > ⛔ **不加** `plane_binding` 进快照(`decl.plane_binding` 已是唯一源 ⇒ 加了 = 双源死字段)
- [x] **A1.2** 收集口径 **逐字复刻** `per_network_mappings` 的 push 条件(`:691-692` 的 `if (!mapping.empty())`)=「该 network 的 `vc_binding` **至少含一个整数值**」⇒ ⛔ **不是**「有该 network 键」(per metis **P0-3**)
- [x] **A1.2b** loader 的缺失 / 畸形口径 6 行表(per metis **P1-10**):`ordering_assumption` 整体缺失 ⇒ 空映射(由 (b) 判红,⛔ **不是** `source_error`)· 非对象 ⇒ 按「缺声明」处理 · 实例缺 `id` / `id` 重复 ⇒ 置 `source_error`(⛔ **不静默排除**)
- [x] **A1.3** ⛔ 快照层**不判**一致性、**不吞**异常
- [x] **A1.4** `ordering` 的 key **保持字符串**形态(⛔ 不在快照层规范化 ⇒ 否则 AC-10② 的「前导零 ⇒ FAIL」无法检出)
- [x] **A1.5** 既有 `per_network_mappings` 的消费方(`:910` / `:923` / `test/test_cep_ualink_inc.cc:470`)**零改动**
- [x] **A1.6** **快照正控断言**:真实数据下 `per_network_orderings["ualink_fabric"].size() == 3` 且每条 key 集合 == `{"0","1","2","3"}`(per metis **P1-11**)⇒ 防 loader 少收集 ⇒ (b) 退化成 L-1 空转而 AC-2 仍绿

### WU-2 · 判据 (b)(G2 后半 · A2)

- [x] **A2.1~A2.5** **红**(5 条变异):AC-3 值不一致 / AC-4 条目缺失 / AC-10① 非数字 / AC-10② 前导零 / AC-10③ 越界 ⇒ 断言**精确前缀** `[CP-03-VC-CERTIFICATE/R1-b]`
- [x] **A2.6** **绿**:V6-d 段内新增 (b) 子判据,位置 = **C-2 之后、C-3 之前**;tag 常量 `kR1b = "[CP-03-VC-CERTIFICATE/R1-b] "`(**尾部空格**)
- [x] **A2.6b** 成功 detail 的「四条证书」⇒「**六条证书**」(`:1006`)(per metis **P1-12**)
- [x] **A2.7** key 形态校验(非空 ∧ 全 ASCII 数字 ∧ 长度 > 1 ⇒ 首位非 `'0'`)⇒ 再 `std::from_chars` 转换(⛔ **不用** `stoul` —— 它接受 `" 4"`、静默接受 `"04"`)
- [x] **A2.8** 判定序列 b-1 → b-2 → **b-2.5 越界** → b-3 完备 → b-4 全等(⚠️ **先越界后缺失**,per metis **P1-9**;b-1 标注**结构性不可达**)
- [x] **A2.8b** 溢出 ⇒ 归「越界 FAIL」(⛔ 不得抛异常)
- [x] **A2.9** ⛔ **不实现** ordering **值域**检查(既有 `scripts/test/cep_decl_alignment_check.sh` 已挡)⇒ 登记 **L-3**
- [x] **A2.10** **净**:既有 6 条反向 fixture 仍绿

### WU-3 · 判据 (C-1′-S)(G2 · A3)

- [x] **A3.1** **红 · AC-5**:`decl.plane_binding` **插键** ⇒ 必红 + 前缀 `R1-c` + 红因含「不在 `certificate_scope`」且 **⛔ 不含**「plane 未登记」
      > ⚠️ 变异实例 MUST 用 **`chi_rnf`**(per metis **P1-8**)—— `moesi_gpu` / `mesif` **已在** `plane_binding` 里 ⇒ 那是「改值」
- [x] **A3.2** **红 · AC-11**:绑 `FabricCollectivePlane` 的实例**无任何** `vc_binding` ⇒ 必红
- [x] **A3.3** **绿**:位置 = **scope 循环之后、C-4 之前**(⚠️ **⛔ 不放循环前** —— 既有 C-3 fixture 的归因会被抢,per metis **P0-1**);tag `kR1c = "[CP-03-VC-CERTIFICATE/R1-c] "`
- [x] **A3.4** 判定序列 s-1 空绑定跳过 → s-2 plane 未登记 FAIL → s-3 本地模型跳过 → s-4 无 VC 绑定 FAIL → s-5 `(plane,network)` 不在 scope FAIL
- [x] **A3.5** ⛔ **不用** `per_network_mappings` 的实例集合 ⇒ 用 **`decl.plane_binding`** 的实例集合
- [x] **A3.3b** 空 `plane_binding` ⇒ **vacuous PASS**(登记 **L-7**)
- [x] **A3.6** **净**:既有 6 条反向 fixture 仍绿 + **归因前缀逐一不变**(A4.4b)

### WU-4 · 并排取证(G3 · A4)

- [x] **A4.1** **AC-3 并排 · 版本无关探针**(per metis **P0-2**):⛔ 探针**不得引用** `per_network_orderings`;旧/新 worktree 各编译一次;**同一份毒化输入文件**(影子 `include/tlm/cep/protocol_instances.json`)⇒ 旧 **PASS** / 新 **FAIL**(`R1-b`)
- [x] **A4.2** **AC-12 并排**:AC-5 的同一影子输入 ⇒ 旧 **PASS** / 新 **FAIL**(`R1-c`);⚠️ **逐字写明**旧实现的「旧绿」是**平凡**的(它**根本不读** `plane_binding`)
- [x] **A4.3** **AC-15 真实 gate 路径**:遍历 `report.lines` ⇒ 找 `line.rule == ContractRule::VcReservation` ⇒ 断言 `detail.find("[CP-03-VC-CERTIFICATE/R1-b] ") != npos`(⛔ **不用** `rfind(...,0)` —— CP-03 的 detail 是**拼接**的)+ 断言 `report.lines.size() == kContractRuleCount`;变异走**临时影子文件**(⛔ 不改仓内 JSON)
- [x] **A4.4** **AC-14 回归**:新 tag 不被既有两条 `rfind` 命中
- [x] **A4.4b** 既有 **6** 条反向 fixture 的**归因前缀逐一不变**
- [x] **A4.5** **AC-1 反向判据**:只改 `compute_noc` 子映射 + **必须用双网实例**(`moesi_gpu` / `mesif`)⇒ **不得红**
- [x] **A4.6** **AC-6 回归**:`certificate_scope` 清空 ⇒ 必红(⚠️ 触发的是**既有** `:901-904` 空早退,⛔ 不得用它证明新判据有牙)
- [x] **A4.7** **AC-2 基线**:`ualink_fabric` 侧未破坏 ⇒ **rc=0**

### WU-5 · 文档同步(G4 前半 · A5)

- [ ] **A5.1~A5.4** `docs/architecture/cep/fabric-transport.md`:R5 表「当前机器判定」列逐条更新 · R1 段 scope 表述订正 · **写明 `R1-a ≡ C-1′`** · `:145` 的 `R1-c` 悬空引用改为「已由 `R1-c` 判据覆盖」
- [x] **A5.5** 「诚实边界」段登记 **L-4 / L-5**(`R-1` / `R-2`)+ 该段 **⛔ 不含**「已验证 / 已满足」字样(per **AC-16** —— ⛔ **措辞核对**,非红/绿闸)
- [x] **A5.6** 写明地位差异:`U-1`/`U-2` **有** C-4 机器守门 / `R-1`/`R-2` **没有**(per **AC-17**)
- [ ] **A5.7 / A5.7b** 登记 **L-1 / L-2 / L-3 / L-6 / L-7** + **`:129` / `:142`** 的「四证书」同步改为六条(per metis **P1-14**)
- [ ] **A5.8 / A5.9** `docs/adr/ADR-CEP-01-*.md` / `ADR-CEP-12-*.md` **末尾追加** `## Status Update`(⛔ **正文逐字不改**)

### WU-6 · 契约 delta(G4 后半 · A7)

- [x] **A7.1** delta:`MODIFIED` `cep-collective-plane-contract-shared-mode-certificate` —— 既有 6 个 Scenario **逐字保留** + 新增 **4** 个(`R1-b` / `R1-c` / **既有归因前缀不变** / 真实 gate 路径)
- [x] **A7.2** ⛔ **不新增 `Requirement`**(与「不新增 `ContractRule`」同型)
- [x] **A7.3** ⛔ 不改 `openspec/specs/cep-collective-plane-contract/spec.md`(归档时才合并 ⇒ **另行触发**五步链)
- [x] **A7.4** `openspec validate cpptlm-cep-transport-vc-consistency --strict` ⇒ rc=0

### WU-7 · 冻结链(G5 · A6)

- [x] **A6.1** 产物 commit(源码 + 测试 + 3 份文档 + delta spec,一次 commit)—— ⚠️ **delta spec 必须在 A7 先写好**
- [x] **A6.2** `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #347"`(**3 处产物同一次重算**)
- [x] **A6.3** 清单 commit(`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`)
- [x] **A6.4** 无参复验 ⇒ **rc=0**;`artifact_count` 仍 **78**
- [x] **A6.5** `git diff --exit-code` 对 4 个声明数据/schema 文件 ⇒ 空(**AC-13** 终态)

### WU-8 · 0 回归与验收(A8)

- [ ] **A8.1~A8.7** 编译 + `ctest -j4` 全绿(**⛔ 断言「不低于改动前的 169/169」**)· `[cep]` 全绿(**⛔ 不低于 508 cases / 11516 assertions**)· **35** 闸 rc + ✗ 双判全绿 · `docs_sync --strict` rc=0 · `agents_md_lint` rc=0 · `cep_freeze_check` 无参 rc=0
- [x] **A8.8** **17 条 AC 逐条判定**(每条带工具证据)⇒ 落 `.rddf/reviews/cpptlm-cep-transport-vc-consistency-verify.md`（待产出）
- [x] **A8.9** **遗留清扫**:本次范围内 0 处 TODO / FIXME / 临时调试日志 / 被 skip 的测试 / 未替换的 mock / 注释掉的代码 / 临时 fallback 分支

### WU-9 · 归档(M6 · A9)

- [ ] **A9.1** `openspec archive cpptlm-cep-transport-vc-consistency` ⇒ delta 合并进 main spec
- [ ] **A9.2** **第二次五步链**(5 步可勾选)⇒ `artifact_count` 仍 **78**
- [ ] **A9.3** KPI 回填(Proposed **1/3 → 0/3**)+ 台账 Entry
- [ ] **A9.4** ⛔ 本 change 归档后才可立项 **C3** / **C4**

---

## 交付物清单

| # | 交付 | 类型 |
|:--:|---|---|
| 1 | `include/tlm/cep/collective_plane_contract.hh` —— `VcBindingSnapshot` +1 字段 + 收集逻辑 + 2 条子判据 + 成功 detail 措辞 | 源码(**CP-03 共享头**) |
| 2 | `test/test_cep_ualink_inc.cc` —— 5 条 (b) 变异 + 2 条 (C-1′-S) 变异 + 快照正控 + 既有归因前缀不变 + 真实 gate 路径 | 测试 |
| 3 | `.rddf/reviews/cpptlm-cep-transport-vc-consistency-oldnew-evidence.md`（待产出） —— 并排取证的**命令 + 输入 + 两端输出** | 证据 |
| 4 | `docs/architecture/cep/fabric-transport.md` —— R5 表 + R1 段 + 诚实边界段 + `:129`/`:142` | 冻结产物 1/3 |
| 5 | `docs/adr/ADR-CEP-01-fabric-collective-plane.md` —— 末尾 `## Status Update` | 冻结产物 2/3 |
| 6 | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` —— 末尾 `## Status Update` | 冻结产物 3/3 |
| 7 | `openspec/changes/cpptlm-cep-transport-vc-consistency/specs/cep-collective-plane-contract/spec.md` —— delta(+4 Scenario) | 契约 |
| 8 | `.rddf/reviews/cpptlm-cep-transport-vc-consistency-verify.md`（待产出） —— 17 AC 逐条判定 | 验收报告 |
| 9 | 台账 Entry —— 实施回执 + 冻结链 provenance + KPI 回填 | 治理 |
| 10 | `openspec/specs/cep-collective-plane-contract/spec.md` —— 归档时合并(**触发第二次五步链**) | 契约(**归档期**) |
