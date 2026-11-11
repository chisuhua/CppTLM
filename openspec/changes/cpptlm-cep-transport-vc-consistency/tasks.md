# tasks —— `cpptlm-cep-transport-vc-consistency`

> 顺序 = **A0(授权门) → A1(快照字段) → A2(判据 (b)) → A3(判据 C-1′-S) → A4(并排取证) → A5(文档 3 处) → A6(冻结链) → A7(契约 delta)**;
> **A2 / A3 可并行**(改同一文件的两个不同区段 ⇒ 串行更省冲突,按 A2 → A3 走)。
> 门:**G1 = A0** · **G2 = A2 + A3**(两条判据落地 + 变异全红)· **G3 = A4**(并排取证)· **G4 = A5 + A6**(文档 + 冻结链)· **G5 = A7**(契约 delta)。
> 里程碑:**M1 快照就位 → M2 两条判据有牙 → M3 并排取证 → M4 文档同步 + 冻结链 → M5 契约合并 → M6 归档**。

> ⚠️ **TDD 5 步纪律**(per `rdd-workflow-writing-plans`):每条判据 = **红(先写变异) → 绿(实现) → 净(0 回归)**;
> ⛔ **不得**先写实现再补变异(那样变异是「照着实现写的」⇒ 无法证明判据有牙)。

---

## A0 · 授权门(G1)

- [ ] A0.1 立项 Entry 授权本 change(占 **D1 的 1 个 Proposed 槽位** —— 当前 CEP 系列 **0 active** ⇒ 可占);⚠️ **C3 / C4 须待本 change 归档后才可立项**(串行约束)
- [ ] A0.2 登记 §13 实施路径推进:Entry #344 的 W2 波次 → 本 change 实施中
- [ ] A0.3 确认**声明数据零改动基线**:`git diff --exit-code -- include/tlm/cep/protocol_instances.json include/tlm/cep/collective_planes.json include/tlm/cep/fabric.json include/tlm/cep/protocol_instance.schema.json` 为空(AC-13 的**起点**)
- [ ] A0.4 确认既有 V6-d 四证书基线绿:`ctest --test-dir build -R cep_ualink_inc` 全绿(改动前的对照)

---

## A1 · 快照字段(契约头 · `include/tlm/cep/collective_plane_contract.hh`)

- [ ] A1.1 `VcBindingSnapshot` **新增一个**字段 `per_network_orderings`(`network → vector<pair<instance_id, map<vc_string, ordering>>>`)—— per design **D-2**
      > ⚠️ ⛔ **不加** `plane_binding` 进快照(`decl.plane_binding` 已是唯一源,`:181` / `:226-243`)⇒ 加了 = 双源死字段
- [ ] A1.2 `load_vc_binding_snapshot` 的收集逻辑:遍历每个 instance 的 `vc_binding`,**对每个 network** 产出一条 entry;`ordering_assumption[network]` 缺失时其 ordering 映射为**空**(表示「未声明」)
      > ⚠️ **收集口径 = 「有 `vc_binding` 的实例」** —— ⛔ **不得**以 `ordering_assumption` 为驱动(否则缺声明的实例被静默排除 ⇒ per **AC-9**)
- [ ] A1.3 ⛔ 快照层**不判**任何一致性、**不吞**异常(`ordering_assumption` 缺失 = 数据缺失,由判据判红,⛔ 不是 `source_error`)
- [ ] A1.4 ⛔ `ordering` 的 key **保持字符串**形态(⛔ 不在快照层规范化 —— 否则 AC-10② 的「前导零 ⇒ FAIL」将无法检出)
- [ ] A1.5 既有 `per_network_mappings` 的消费方(`:910` / `:923` / `test/test_cep_ualink_inc.cc:470`)**零改动** ⇒ 既有 6 条反向 fixture 仍绿

---

## A2 · 判据 (b) —— 排序假设逐 VC 一致(tag `R1-b`)

> **先写变异(红)再写实现(绿)** —— per TDD 纪律。

- [ ] A2.1 **红 · AC-3**：毒化 `per_network_orderings["ualink_fabric"]` 某 entry 的某 vc 值(`p2p_ordered` → `unordered`)⇒ 断言**必红** + **精确前缀** `[CP-03-VC-CERTIFICATE/R1-b]`
- [ ] A2.2 **红 · AC-4**：删某 entry 的某 vc ⇒ 必红,前缀 `R1-b`
- [ ] A2.3 **红 · AC-10①**：key = `"a"` ⇒ 必红,前缀 `R1-b`
- [ ] A2.4 **红 · AC-10②**：key = `"04"` ⇒ 必红,前缀 `R1-b`
- [ ] A2.5 **红 · AC-10③**：key 数值 >= `vc_count` ⇒ 必红,前缀 `R1-b`
- [ ] A2.6 **绿 · 实现**：在 `evaluate_shared_mode_certificate` 的 V6-d 段内新增 (b) 子判据 —— 位置 = **C-2 之后、C-3 之前**(per design **D-6**),tag 常量 `kR1b = "[CP-03-VC-CERTIFICATE/R1-b] "`(**尾部空格**,与 `:884-888` 既有 4 个常量同型)
- [ ] A2.7 key 形态校验:非空 ∧ 全 ASCII 数字 ∧ (长度 > 1 ⇒ 首位非 `'0'`)⇒ 再用 `std::from_chars` 转换(⛔ 不用 `stoul`)⇒ 数值 vs `vc_count` 判越界 —— per design **D-4**
- [ ] A2.8 判定序列 = **b-1 集合非空 → b-2 key 形态 → b-3 完备(key 集合 == {0..vc_count-1})→ b-4 跨实例全等**;任一不过 ⇒ 立即 `return FAIL`
- [ ] A2.9 ⛔ **不实现** ordering **值域**检查(既有 `scripts/test/cep_decl_alignment_check.sh` 的 `jsonschema` 已挡)⇒ 登记为 **L-3**
- [ ] A2.10 **净**：既有 V6-d 6 条反向 fixture + 全部 `[cep]` 仍绿

---

## A3 · 判据 (C-1′-S) —— scope 完备性(tag `R1-c`)

- [ ] A3.1 **红 · AC-5**：`decl.plane_binding` **插键**(某 `compute_noc` 上有 `vc_binding` 的实例 → `FabricCollectivePlane`;⚠️ 是插键不是改值)⇒ 必红 + 精确前缀 `[CP-03-VC-CERTIFICATE/R1-c]` + 红因文本**含**「不在 `certificate_scope`」且 **⛔ 不含**「plane 未登记」
- [ ] A3.2 **红 · AC-11**：令绑定 `FabricCollectivePlane` 的实例在 `per_network_orderings` 中**查不到**(即无任何 VC 绑定)⇒ 必红,前缀 `R1-c`
- [ ] A3.3 **绿 · 实现**：新增 (C-1′-S) 子判据 —— 位置 = **scope 项循环之前**(per design **D-5 / D-6**),tag 常量 `kR1c = "[CP-03-VC-CERTIFICATE/R1-c] "`
- [ ] A3.4 判定序列 = **s-1 空绑定跳过 → s-2 plane 未登记 FAIL → s-3 本地模型跳过 → s-4 无 VC 绑定 FAIL → s-5 (plane,network) 不在 scope FAIL**
- [ ] A3.5 ⛔ **不**用 `per_network_mappings` 的实例集合(那是「有 vc_binding 的实例」)⇒ 本判据用 **`decl.plane_binding`** 的实例集合(「声明了 plane 绑定的实例」)
- [ ] A3.6 **净**：既有 V6-d 6 条反向 fixture + 全部 `[cep]` 仍绿

---

## A4 · 并排取证(G3)

> ⛔ **不得**用「同一份 fixture 在新旧两套代码上跑」冒充并排 —— 必须是**跨实现**的证据。

- [ ] A4.1 **AC-3 并排**：用 **git worktree** checkout 改动前的 HEAD ⇒ 编译一个只调 `evaluate_shared_mode_certificate` 的探针 ⇒ 用**同一份**毒化快照:旧实现 **PASS** / 新实现 **FAIL**(`R1-b`)⇒ 证据落 `.rddf/reviews/`
- [ ] A4.2 **AC-12 并排**：AC-5 的同一变异:旧实现 **PASS** / 新实现 **FAIL**(`R1-c`)⇒ 证据落 `.rddf/reviews/`
      > ⚠️ **诚实边界须逐字写明**:旧实现**根本不读** `decl.plane_binding` ⇒ 它的「旧绿」是**平凡**的 ⛔ 不得包装成「旧实现也检查了但更宽松」
- [ ] A4.3 **AC-15 真实 gate 路径**：新增一条经 `run_collective_plane_contract` 调用链触发的用例(⛔ 不得只在测试里手搓 `decl` 直调)⇒ 断言新判据的结论**折进 `CP-03` 行**且 `rule=` 字段**仍为** `CP-03`(⛔ 未新增第 7 条 `ContractRule`)
- [ ] A4.4 **AC-14 回归**：新 tag 不被 `[CP-03-VC-CERTIFICATE]` 与 `[CP-03-VC-CERTIFICATE/C-1′]` 的**精确前缀** `rfind` 命中 + 既有 fixture 仍绿
- [ ] A4.5 **AC-1 反向判据**：只改 `compute_noc` 子映射、且**必须用同时拥有双网 `vc_binding` 的实例**(`moesi_gpu` / `mesif` —— `ualink_4` 只有 `ualink_fabric`,做不了这个反向)⇒ **不得红**;若红 ⇒ **判据实现错了**(scope 泄漏)⇒ 回到 `T-C2-⓪`,⛔ 不得改声明来"修"
- [ ] A4.6 **AC-6 回归**：`certificate_scope` 清空 ⇒ 必红(⚠️ 触发的是**既有** `:901-904` 空早退,⛔ **不得**用它证明新判据有牙)
- [ ] A4.7 **AC-2 基线**：`ualink_fabric` 侧未破坏 ⇒ **rc=0**

---

## A5 · 文档同步(3 处冻结产物 · G4 前半)

- [ ] A5.1 `docs/architecture/cep/fabric-transport.md` · **R5 表**「当前机器判定」列逐条更新(R1 行**不再**写「(b) 无载体」/「`compute_noc` 无判定」)
- [ ] A5.2 同上 · **R1 段** scope 表述订正:「作用域 = 声明 M-2 的 network(`compute_noc` **按裁决不属 M-2**,非"未覆盖")」
- [ ] A5.3 同上 · **写明 `R1-a ≡ C-1′` 映射**(否则读者以为 R1-a 无载体)
- [ ] A5.4 同上 · **`:145` 的 `R1-c` 悬空引用处置** ⇒ 改为「已由 `R1-c` 判据覆盖」(per K-2:它**已由本 change 定义**,⛔ 不是「定义它还是删除它」二选一)
- [ ] A5.5 同上 · **「诚实边界」段**登记 **L-4 / L-5**(`R-1` / `R-2`),且该段 **⛔ 不含**「已验证 / 已满足」字样(per **AC-16**;决定性变异:把 `R-1` 表述改成「已验证」⇒ 必红)
- [ ] A5.6 同上 · **写明地位差异**:`U-1`/`U-2` **有** C-4 机器守门 / `R-1`/`R-2` **没有**(纯文档级)(per **AC-17**)
- [ ] A5.7 同上 · 登记 **L-1**(单实例 network 空转)/ **L-2**((b) 只证跨实例一致)/ **L-3**(值域由既有 schema 闸挡)
- [ ] A5.8 `docs/adr/ADR-CEP-01-fabric-collective-plane.md` · **末尾追加** `## Status Update`(C-1′-S 锚点)—— ⛔ **正文逐字不改**
- [ ] A5.9 `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` · **末尾追加** `## Status Update`((b) 锚点 + IC2 职责边界澄清)—— ⛔ **正文逐字不改**

---

## A6 · 冻结面五步链(G4 后半)

- [ ] A6.1 产物 commit(源码 + 测试 + 3 份文档 + delta spec,一次 commit)
- [ ] A6.2 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #XXX"`(**3 处产物同一次重算**)
      > ⚠️ **产物 commit 必须先于 `--update`**(per C1 已验证的顺序,Entry #342)
- [ ] A6.3 清单 commit(`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`)
- [ ] A6.4 无参复验 `bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**;`artifact_count` **不新增**(= 78)
- [ ] A6.5 `git diff --exit-code` 对 4 个声明数据/schema 文件 ⇒ 空(**AC-13** 终态)

---

## A7 · 契约 delta(G5)

- [ ] A7.1 delta spec: `MODIFIED` `cep-collective-plane-contract-shared-mode-certificate` —— 新增 **2 个 Scenario**((b) 排序假设逐 VC 一致 + 完备 / (C-1′-S) `(plane, network)` pair 完备性)+ 更新 V6-d Scenario 的「四条证书」表述为**六条**(C-1′ / C-2 / C-3 / C-4 / R1-b / R1-c)与归因前缀清单
- [ ] A7.2 ⛔ **不新增 `Requirement`** —— 两条判据是既有 `cep-collective-plane-contract-shared-mode-certificate` 的**子判据**(⛔ 与「不新增 `ContractRule`」同型)
- [ ] A7.3 ⛔ 不改 `openspec/specs/cep-collective-plane-contract/spec.md`(归档时才合并 ⇒ **另行触发**五步链)
- [ ] A7.4 `openspec validate cpptlm-cep-transport-vc-consistency --strict` ⇒ rc=0

---

## A8 · 0 回归与验收(AC-8)

- [ ] A8.1 `cmake --build build -j$(nproc)` ⇒ 编译通过
- [ ] A8.2 `ctest --test-dir build -j4` ⇒ **全绿**(基线 169/169)
- [ ] A8.3 `./build/bin/cpptlm_tests "[cep]"` ⇒ 全绿(基线 508 cases / 11516 assertions)
- [ ] A8.4 **35 个** `cep_*.sh` 闸 rc + ✗ 双判全绿
- [ ] A8.5 `bash scripts/test/docs_sync_check.sh --strict` ⇒ rc=0
- [ ] A8.6 `python3 scripts/test/agents_md_lint.py` ⇒ rc=0
- [ ] A8.7 `bash scripts/test/cep_freeze_check.sh` 无参 ⇒ rc=0
- [ ] A8.8 **17 条 AC 逐条判定**(每条带工具证据)⇒ 落 `.rddf/reviews/cpptlm-cep-transport-vc-consistency-verify.md`（待产出）
- [ ] A8.9 **遗留清扫**:本次范围内 0 处 TODO / FIXME / 临时调试日志 / 被 skip 的测试 / 未替换的 mock / 注释掉的代码 / 临时 fallback 分支

---

## A9 · 归档(M6)

- [ ] A9.1 `openspec archive cpptlm-cep-transport-vc-consistency` ⇒ delta 合并进 `openspec/specs/cep-collective-plane-contract/spec.md`
      > ⚠️ 该 spec **在冻结集**(`openspec/specs/cep-*/spec.md` glob)⇒ **另行触发**一次五步链
- [ ] A9.2 归档后 KPI 回填(Proposed **1/3 → 0/3**)+ 台账 Entry
- [ ] A9.3 ⛔ **本 change 归档后**才可立项 **C3**(`R-c` + `R-d`)与 **C4**(MUST-FIX-1/2)

---

## ⛔ 明确不做

- ⛔ 不改任何 ADR **正文**(只在末尾追加 `## Status Update`)
- ⛔ 不改任何声明数据(4 个 JSON/schema 零 diff,AC-13 机器守门)
- ⛔ 不把 `compute_noc` 加进 `certificate_scope`(它是 M-1 网络)
- ⛔ 不抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）(共用消费者实测 0 个)
- ⛔ 不给 `VcBindingSnapshot` 加 `plane_binding`(双源死字段)
- ⛔ 不扩 `UntestedRegistry`(per K-1:注入点已存在 ⇒「扩展来源」是伪需求)
- ⛔ 不改 `include/tlm/cep/protocol_instance.schema.json` 的 description(per K-2)
- ⛔ 不改 `include/tlm/cep/fabric.json`(只读 `vc_count`)
- ⛔ 不实现 M-1 回退机制
- ⛔ 不含 C3 / C4(D1 串行)
- ⛔ 不修 `validate_topology` target(实测首条命令引用仓内从未存在的 `build/configs/dgpu_board_v1_mvp.json`（仓内不存在 —— 正是 target 跑不完的根因） ⇒ 跑不完 ⇒ 装配闸 CLI 挂点从未被执行;**独立裁决 + 独立 change**)
- ⛔ 不新增磁盘 `.sh` 闸(若新增 ⇒ 磁盘数 35 → 36 ⇒ 须同步 `scripts/README.md` / `AGENTS.md` 的计数口径)
