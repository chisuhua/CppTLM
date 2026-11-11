# cpptlm-cep-transport-vc-consistency: 传输层 VC 语义一致性判据落地

> **状态**: 🟡 **Proposed**(2026-11-12 立项 —— per 用户口令「切 builder 执行」;KPI 并发 **0/3 → 1/3**)
> **立项依据**: 台账 **Entry #344**(W0 方案 + K-1 / K-2 裁决)· `.rddf/reviews/cep-c2-vc-consistency-decision.md` **§1~§12** · `.rddf/improvements/cpptlm-cep-transport-vc-consistency.md`(17 条 AC)
> **上游决策(已裁决,本 change ⛔ 不重新讨论)**: `T-C2-⓪` = **条件化口径** · `T-C2-①` = **扩展 C-1′ 适用域** · `U-3` = `compute_noc` 上 `tilelink_c` 的 VC 映射差异是**合法协议差异**、⛔ 不改声明 · `K-1` = `R-1` / `R-2` 落 `docs/architecture/cep/fabric-transport.md` 的**诚实边界段** · `K-2` = 锚点**按主题拆两处**、tag = `R1-b` / `R1-c`
> **估时**: ~2.5d(判据实现 T1~T4 + 文档 T5~T6 + 冻结链 T7)+ 收尾 ~0.5d

## Why

`docs/architecture/cep/fabric-transport.md` R1 要求:同一 network 上混排多协议实例时,**VC → 消息类集合** 与 **VC → 排序假设** 必须逐 VC 一致。当前两个维度的载体状态:

| 维度 | 载体现状 | 缺口 |
|---|---|---|
| **(a) VC → 消息类集合** | ✅ **C-1′ 已实现**(`include/tlm/cep/collective_plane_contract.hh`,归因 `[CP-03-VC-CERTIFICATE/C-1′]`) | ⚠️ **适用域是自声明数据**:`certificate_scope` 是**人手写的清单**(当前 1 项:`FabricCollectivePlane` / `ualink_fabric`)⇒ 写漏一个 network ⇒ 判据**静默不覆盖**该网,且**没有任何闸会抱怨** |
| **(b) VC → 排序假设** | ⛔ **无载体**(`grep -c ordering_assumption include/tlm/cep/collective_plane_contract.hh` = **0**) | 同 network 上混排实例的 `ordering_assumption` 可逐 VC 不一致 ⇒ **保序语义被稀释** ⇒ 跨实例排序假设不一致**不可发现** |

**⚠️ 一处已存在的决策-实现缺口(本 change 兑现它,不是新增决策)**:
`include/tlm/cep/protocol_instance.schema.json:180` 的 description 早宣称「一致性由**装配期校验**承担(per `INV-CEP-12-IC2`)」,而 **`INV-CEP-12-IC2` 全文只保证 key 可解析**(`docs/adr/ADR-CEP-12-protocol-engine-capability-map.md:160`)⇒ **该承诺至今没有承担者** ⇒ (b) 的落点是**兑现已有文本**,不是新造语义。

**⚠️ 对「`compute_noc` 无判定」的表述更正(避免读者误读为能力缺失)**:
`compute_noc` **不是**判据能力不足 —— C-1′ 的逻辑**早已与 network 无关**(`collective_plane_contract.hh:905-946` 遍历 `certificate_scope` 的每项、逐 network 比对全部实例)。真实根因是**作用域是自声明数据** ⇒ 这正是本 change 的 **C-1′-S** 要关掉的洞。

**⚠️ schema 挡不住 (b) 的不完备**:`protocol_instance.schema.json:180` 的 `ordering_assumption` 每 network 仅 `minProperties: 1` ⇒ 某实例只登记 vc0、漏 vc1~vc3 ⇒ **schema 合法** ⇒ 只能靠判据自己挡。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | 快照**新增一个**字段 `per_network_orderings`(network → 逐实例的 `vc(string) → ordering`) | `include/tlm/cep/collective_plane_contract.hh`(`VcBindingSnapshot` + `load_vc_binding_snapshot`) |
| 2 | **新增 (b) 判据**:逐 VC `ordering` 全等 + VC 完备性(`vc 0..vc_count-1`)+ 越界 / 非规范 key 三类处置 | 同上(V6-d 段内)· tag **`[CP-03-VC-CERTIFICATE/R1-b]`** |
| 3 | **新增 (C-1′-S) 判据**:`(plane, network)` pair ⊆ `certificate_scope` | 同上(V6-d 段内)· tag **`[CP-03-VC-CERTIFICATE/R1-c]`** · 读 **`decl.plane_binding`**(已解析于 `:181` / `:226-243`,V6-c 在 `:830` 已消费 ⇒ ⛔ **不新增快照字段**) |
| 4 | **R-1 / R-2** 残余登记 + **地位差异**说明 | `docs/architecture/cep/fabric-transport.md` 的**「诚实边界」段**(`:106-112`) |
| 5 | **R5 表**「当前机器判定」列同步 + **R1 段** scope 表述订正 + **`R1-a ≡ C-1′` 映射** + **`:145` 的 `R1-c` 悬空引用**处置 | 同上 |
| 6 | **两份 ADR 各追加 1 则 `## Status Update`** | `docs/adr/ADR-CEP-01-fabric-collective-plane.md`(C-1′-S 锚点)· `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md`((b) 锚点) |
| 7 | 契约面 delta | 本 change 的 `openspec/changes/cpptlm-cep-transport-vc-consistency/specs/cep-collective-plane-contract/spec.md`(MODIFIED `cep-collective-plane-contract-shared-mode-certificate`) |
| 8 | 反向 fixture + 决定性变异 | `test/test_cep_ualink_inc.cc`(既有 V6-d 四证书的载体面) |

> ⚠️ **冻结面**:第 5 / 6 项**共 3 处产物**在冻结集(`docs/architecture/cep/*.md` glob + `docs/adr/ADR-CEP-*.md` glob)⇒ **走五步授权链**。
> ✅ `include/tlm/cep/collective_plane_contract.hh` **不在**冻结集(已核 manifest globs)。
> ⚠️ 归档时若合并 delta ⇒ `openspec/specs/cep-collective-plane-contract/spec.md` **在**冻结集(`openspec/specs/cep-*/spec.md` glob)⇒ **另行触发**五步链。

## 验收标准(Acceptance Criteria)

> 判据来源 = `.rddf/improvements/cpptlm-cep-transport-vc-consistency.md` 的 **17 条 AC** · 台账 **Entry #344** · `.rddf/reviews/cep-c2-vc-consistency-decision.md` §6 / §11。
> **统一判据纪律**:每个新判据必须有**一条决定性变异** —— 破坏 ⇒ 按**正确原因**红 ⇒ 还原 ⇒ 绿;⛔ **不得**靠「文件消失」才红;⛔ **不得**靠改声明数据变绿。

- [ ] **AC-1(决定性变异 · (a) 消息类)**:在 `certificate_scope` **内**的 network 上,把某实例的某 VC 改成另一消息类 ⇒ **必红**,红因归因 `[CP-03-VC-CERTIFICATE/C-1′]`。
      > ⚠️ **反向判据**:做反向变异时**只改 `compute_noc` 子映射**,且**必须用同时拥有双网 `vc_binding` 的实例**(`moesi_gpu` / `mesif`)—— `ualink_4` **只有** `ualink_fabric`,用它做不了这个反向。若在 `compute_noc`(scope 外)上改**也红** ⇒ **判据实现错了**(scope 泄漏)⇒ 回到 `T-C2-⓪`,⛔ 不得改声明来"修"。

- [ ] **AC-2(基线仍绿)**:`ualink_fabric` 侧未破坏时 **rc=0**(实测:3 实例的 (a)(b) 全一致)。

- [ ] **AC-3(决定性变异 · (b) ordering)**:把某实例某 VC 的 `ordering` 从 `p2p_ordered` 改成 `unordered`(或反向)⇒ **必红**,红因**必须**归因 `[CP-03-VC-CERTIFICATE/R1-b]`。
      > ⭐ **本 AC 是 (b) 的唯一决定性变异**:同一变异在**旧实现上必绿**(`ordering_assumption` 在旧实现 0 处)⇒ 按「旧绿 / 新红」**并排取证**。

- [ ] **AC-4(决定性变异 · (b) VC 不完备)**:删某实例某 VC 的 `ordering` 条目 ⇒ **必红**(schema 仍合法,仅 `minProperties: 1`),红因归因 `R1-b`。

- [ ] **AC-5(决定性变异 · C-1′-S scope 完备性)**:把 `compute_noc` 上的某实例的 `plane_binding` **新增**为 `FabricCollectivePlane`(⚠️ 当前该实例**不在** `plane_binding` 里 ⇒ 是**插键**不是改值)⇒ **必红**,红因**必须是**「`(plane, network)` 不在 `certificate_scope`」并归因 `[CP-03-VC-CERTIFICATE/R1-c]`(⛔ **不是**「plane 未登记」)。
      > ⚠️ **判定顺序必须固定**:`find_collective_plane` 未登记 ⇒ FAIL(该情形**已由 V6-c `:830` 覆盖**)→ 已登记但 `(plane,network)` 不在 scope ⇒ FAIL(本判据)。否则归因不稳定。

- [ ] **AC-6(反 vacuous · 既有基线)**:`certificate_scope` 清空 ⇒ **必红**。⚠️ 本 AC 触发的是**既有** `:901-904` 的空早退,**不是** C-1′-S 的证词 ⇒ 只作回归基线,⛔ 不得用它证明新判据有牙。

- [ ] **AC-7(文档同步)**:`docs/architecture/cep/fabric-transport.md` 的 R5 表「当前机器判定」列**逐条更新**(R1 不再写「(b) 无载体」/「`compute_noc` 无判定」);R1 段订正为「作用域 = 声明 M-2 的 network(`compute_noc` **按裁决不属 M-2**,非"未覆盖")」;**写明 `R1-a ≡ C-1′` 映射**;**`:145` 的 `R1-c` 表述改为「已由 `R1-c` 判据覆盖」**(per K-2:它已由本 change 定义)。

- [ ] **AC-8(0 回归 + 冻结链 · 3 处)**:`ctest --test-dir build -j4` 全绿 + `[cep]` 全绿 + **35 个** `cep_*.sh` 闸 rc + ✗ 双判全绿 + `docs_sync_check.sh --strict` rc=0 + **冻结面五步授权链**走完(⚠️ **3 处产物**:`docs/architecture/cep/fabric-transport.md` + `ADR-CEP-01` + `ADR-CEP-12`)。判据 = `bash scripts/test/cep_freeze_check.sh` **无参 rc=0** 且 `artifact_count` **不新增**。⚠️ 三处可在**同一次** `--update --authorized-by` 内重算,但**产物 commit 必须先于 `--update`**(C1 已验证的顺序)。

- [ ] **AC-9((b) 的实例集合与缺失行为)**:显式规定「集合 = 该 network 上**所有有 `vc_binding` 的实例**」;任一此类实例**缺** `ordering_assumption[network]` ⇒ **FAIL**(⛔ 不得以 `ordering_assumption` 为驱动 —— 那会静默把缺失实例当子集排除)。
      > 决定性变异:构造「有 `vc_binding[network]` 但无 `ordering_assumption[network]`」的实例 ⇒ **必红**。

- [ ] **AC-10(越界 / 非规范 VC key)**:`ordering_assumption` 的 key 是**字符串数字**(`"0"`),而 `vc_count` 是整数 ⇒ 须显式 `stoul` 并明确三类策略:① 非数字 ⇒ **FAIL**;② 前导零(`"04"`)⇒ **FAIL**(规范化拒绝,避免与 `"4"` 语义分裂);③ `>= vc_count` ⇒ **FAIL**。三类各一条决定性变异,红因归因 `R1-b`。

- [ ] **AC-11(C-1′-S 的 vacuous 探测)**:绑 `FabricCollectivePlane` 的实例其 `vc_binding` 为**空** ⇒ 必须给出明确结论(**建议 FAIL**,与 (a) 的「无实例 ⇒ FAIL」对称),⛔ 不得 vacuous green,红因归因 `R1-c`。

- [ ] **AC-12(C-1′-S 的「旧绿 / 新红」并排)**:AC-5 的同一变异在**旧实现上必绿**(旧实现无此判据)⇒ 并排取证,对称 AC-3。

- [ ] **AC-13(声明数据零改动的机器证据)**:`git diff --exit-code -- include/tlm/cep/protocol_instances.json include/tlm/cep/collective_planes.json include/tlm/cep/fabric.json` 为**空**(防实施中顺手「修」`tilelink_c`)。

- [ ] **AC-14(归因 tag 精确前缀不冲突)**:新 tag `[CP-03-VC-CERTIFICATE/R1-b]` / `[CP-03-VC-CERTIFICATE/R1-c]` 不得被 `[CP-03-VC-CERTIFICATE/C-1′]`(带 `′] ` 终结)或 `[CP-03-VC-CERTIFICATE]` 的**精确前缀**误命中 ⇒ 加一条回归钉住(⚠️ 既有 `test/test_cep_ualink_inc.cc:465` 用**带 `]`** 的 `rfind` ⇒ 理论上安全,但需回归)。

- [ ] **AC-15(真实 gate 路径可达)**:新判据必须在**真实 gate**(`include/tlm/cep/collective_plane_contract.hh` 的 `run_collective_plane_contract` 调用链,`:1447` 附近)可触达,⛔ 不得只在测试里手搓 `decl` 直调 `evaluate_shared_mode_certificate`。

- [ ] **AC-16(`R-1` / `R-2` 登记的文字守门)**:`R-1` / `R-2` 在 `docs/architecture/cep/fabric-transport.md` 的「诚实边界」段**可 grep 命中**,且该段 **⛔ 不含「已验证 / 已满足」字样**(与 C-4 的 `untested_is_not_satisfied` 同型)。决定性变异:把 `R-1` 的表述改成「已验证」⇒ 必红。

- [ ] **AC-17(文档地位与映射)**:文档显式写明 ①「**`R1-a` ≡ `C-1′`**」映射 ②「**U-1/U-2 有 C-4 机器守门 / `R-1`/`R-2` 没有**(纯文档级)」的地位差异。逐字可核。

## Out of Scope

- ⛔ **不改任何 ADR 正文**(`ADR-CEP-01` / `ADR-CEP-12` 不动;K-2 的锚点 ⇒ 只在**末尾追加 `## Status Update`**)
- ⛔ **不改任何声明数据**:`compute_noc` / `tilelink_c` 的 `vc_binding` 与 `ordering_assumption` **一字不改**(per `U-3`;AC-13 机器守门)
- ⛔ **不把 `compute_noc` 加进 `certificate_scope`** —— 它是 **M-1 网络**(不用共享 VC 池),不是"漏登"
- ⛔ **不抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）**(选项 B 已否决:共用消费者实测 **0** 个 —— `configs/cep_*.json` 共 **9** 份,**8** 份恰 1 个 `protocol_instance` + `configs/cep_chi_rnf_e2e_a.json` **0** 个)
- ⛔ **不给 `VcBindingSnapshot` 加 `plane_binding`** —— `decl.plane_binding` 已存在且已被 V6-c 消费 ⇒ 加了就是**双源死字段**
- ⛔ **不扩 `UntestedRegistry`**(per K-1:注入点已存在 ⇒「扩展来源」是伪需求;且 R-1/R-2 属**判据边界** ≠ **机制不可检验**)
- ⛔ **不改 `include/tlm/cep/protocol_instance.schema.json` 的 description**(per K-2:它是决策 5 的转录;在 ADR SU 里澄清即可)
- ⛔ **不改 `include/tlm/cep/fabric.json`**(只读取 `vc_count`)
- ⛔ **不实施 M-1 回退机制**(「不一致 ⇒ 回 M-1」仍靠人工;`R-2` 已登记为不可判)
- ⛔ **不含 C3 / C4**(D1「Proposed 并发 ≤ 1」串行约束)
- ⛔ **不修 `validate_topology` target**(实测该 target 首条命令引用仓内从未存在的 `build/configs/dgpu_board_v1_mvp.json`（仓内不存在 —— 正是 target 跑不完的根因） ⇒ 跑不完 ⇒ 装配闸 CLI 挂点从未被执行;**独立裁决 + 独立 change**,per 台账 `ADR-CEP-00` SU 的「修复需单独决策」)

## 关联

`ADR-CEP-01` §E.3.1 / §E.3.3 / §E.3.4 · `ADR-CEP-12` 决策 5 / `INV-CEP-12-IC2` · `docs/architecture/cep/fabric-transport.md` R1 / R5 · `docs/guide/cep-usage.md` §12.8 · 台账 **Entry #340** / **#344** · `.rddf/reviews/cep-c2-vc-consistency-decision.md` · `.rddf/improvements/cpptlm-cep-transport-vc-consistency.md`
