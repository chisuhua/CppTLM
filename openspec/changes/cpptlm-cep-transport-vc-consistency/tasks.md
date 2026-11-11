# tasks —— `cpptlm-cep-transport-vc-consistency`

> 顺序 = **A0(授权门) → A1(快照字段) → A2(判据 (b)) → A3(判据 C-1′-S) → A4(并排取证) → A5(文档 3 处) → A7(契约 delta) → A6(冻结链)**;
> **A2 / A3 可并行**(改同一文件的两个不同区段 ⇒ 串行更省冲突,按 A2 → A3 走)。
> ⚠️ **A7 在 A6 之前** —— A6.1 的产物 commit 含 delta spec ⇒ delta 必须先写好(per metis P0-5)。
> 门:**G1 = A0** · **G2 = A2 + A3**(两条判据落地 + 变异全红)· **G3 = A4**(并排取证)· **G4 = A5 + A7**(文档 + 契约 delta)· **G5 = A6**(冻结链)。
> 里程碑:**M1 快照就位 → M2 两条判据有牙 → M3 并排取证 → M4 文档同步 + 冻结链 → M5 契约合并 → M6 归档**。

> ⚠️ **TDD 5 步纪律**(per `rdd-workflow-writing-plans`):每条判据 = **红(先写变异) → 绿(实现) → 净(0 回归)**;
> ⛔ **不得**先写实现再补变异(那样变异是「照着实现写的」⇒ 无法证明判据有牙)。

---

## A0 · 授权门(G1)

- [x] A0.1 台账 Entry 授权本 change 的实施(占 **D1 的 1 个 Proposed 槽位** —— 立项时已占,当前 **1/3**);⚠️ **C3 / C4 须待本 change 归档后才可立项**(串行约束)
      > ⚠️ **授权号 = `Entry #347`**(per metis **P0-5** 的实测 + C1 先例:Entry #342 = 「C1 开工授权」且同时充当 `--update` 的授权记录)⇒ **⛔ 不是 `Entry #346`** —— #346 已被 metis gap 分析处置占用(Entry #346 ⑨)
      > ⇒ **本 Entry 同时是 A6.2 / A9.2 的 `--authorized-by` 授权依据**
- [x] A0.2 登记 §13 实施路径推进:Entry #344 的 W2 波次 → 本 change 实施中
- [x] A0.3 确认**声明数据零改动基线**:`git diff --exit-code -- include/tlm/cep/protocol_instances.json include/tlm/cep/collective_planes.json include/tlm/cep/fabric.json include/tlm/cep/protocol_instance.schema.json` 为空(AC-13 的**起点**)
      > ✅ **实测 rc=0**;4 个 sha256 已记入台账 **Entry #347 ④**(内容级守门的起点)
      > 记 4 个文件的 **sha256** 到实施期 Entry ⇒ 实施期终态(A6.5)逐个比对 ⇒ ⛔ 「零 diff」之外再加一道**内容级**守门
- [x] A0.4 确认既有 V6-d 四证书基线绿:`ctest --test-dir build -R cep_ualink_inc` 全绿(改动前的对照)
      > ✅ **实测**:`./build/bin/cpptlm_tests "[cep][ualink-inc]"` ⇒ **All tests passed(294 assertions in 12 test cases)**

---

## A1 · 快照字段(契约头 · `include/tlm/cep/collective_plane_contract.hh`)

- [x] A1.1 `VcBindingSnapshot` **新增一个**字段 `per_network_orderings`(`network → vector<pair<instance_id, map<vc_string, ordering>>>`)—— per design **D-2**
      > ⚠️ ⛔ **不加** `plane_binding` 进快照(`decl.plane_binding` 已是唯一源,`:181` / `:226-243`)⇒ 加了 = 双源死字段
- [x] A1.2 `load_vc_binding_snapshot` 的收集逻辑:遍历每个 instance 的 `vc_binding`,**对每个 network** 产出一条 entry;`ordering_assumption[network]` 缺失时其 ordering 映射为**空**(表示「未声明」)
      > ⚠️ **收集口径 = 「有 `vc_binding` 的实例」** —— ⛔ **不得**以 `ordering_assumption` 为驱动(否则缺声明的实例被静默排除 ⇒ per **AC-9**)
      > ⚠️ **「有」的定义 MUST 逐字复刻** `per_network_mappings` 的 push 条件(`include/tlm/cep/collective_plane_contract.hh:691-692` 的 `if (!mapping.empty())`)⇒ **该 network 的 `vc_binding` 至少含一个整数值**;⛔ **不是**「`vc_binding` 有该 network 键」(值全非整数的条目会被现有 loader 跳过 ⇒ 两个口径分叉 ⇒ per metis **P0-3**)
- [x] A1.2b **loader 的缺失 / 畸形口径**(per metis **P1-10**,逐条钉死 ⛔ 不得静默):`ordering_assumption` 整体缺失 ⇒ 该 network 的 ordering 映射为**空**(由 (b) 判红,⛔ **不是** `source_error`)· `ordering_assumption[network]` **非对象** ⇒ 按「缺声明」处理(红因文本须可区分「缺声明」与「值非法」)· 实例**缺 `id`** / `id` 非字符串 / `id` **重复** ⇒ 置 `source_error`(配置错误;⛔ **不静默排除** ⇒ 否则 `instance_id` 无法定位)
- [x] A1.3 ⛔ 快照层**不判**任何一致性、**不吞**异常(`ordering_assumption` 缺失 = 数据缺失,由判据判红,⛔ 不是 `source_error`)
- [x] A1.4 ⛔ `ordering` 的 key **保持字符串**形态(⛔ 不在快照层规范化 —— 否则 AC-10② 的「前导零 ⇒ FAIL」将无法检出)
- [x] A1.5 既有 `per_network_mappings` 的消费方(`:910` / `:923` / `test/test_cep_ualink_inc.cc:470`)**零改动** ⇒ 既有 6 条反向 fixture 仍绿
      > ✅ **实测**:`[cep][ualink-inc]` ⇒ **All tests passed(402 assertions in 13 test cases)**(较基线 12 cases +294 assertions ⇒ **+1 case / +108 assertions** = 新增快照用例)
- [x] A1.6 **快照正控断言**(per metis **P1-11**):真实 `include/tlm/cep/protocol_instances.json` 下 `per_network_orderings["ualink_fabric"].size() == 3`(`moesi_gpu` / `mesif` / `ualink_4`)且每条 entry 的 key 集合 == `{"0","1","2","3"}` ⇒ ⛔ **防止 loader 少收集一条 ⇒ (b) 退化成 L-1 空转**(而 AC-2 仍可能显示绿)

---

## A2 · 判据 (b) —— 排序假设逐 VC 一致(tag `R1-b`)

> **先写变异(红)再写实现(绿)** —— per TDD 纪律。

- [x] A2.1 **红 · AC-3**：毒化 `per_network_orderings["ualink_fabric"]` 某 entry 的某 vc 值(`p2p_ordered` → `unordered`)⇒ 断言**必红** + **精确前缀** `[CP-03-VC-CERTIFICATE/R1-b]`
- [x] A2.2 **红 · AC-4**：删某 entry 的某 vc ⇒ 必红,前缀 `R1-b`
- [x] A2.3 **红 · AC-10①**：key = `"a"` ⇒ 必红,前缀 `R1-b`
- [x] A2.4 **红 · AC-10②**：key = `"04"` ⇒ 必红,前缀 `R1-b`
- [x] A2.5 **红 · AC-10③**：key 数值 >= `vc_count` ⇒ 必红,前缀 `R1-b`
- [x] A2.6 **绿 · 实现**：在 `evaluate_shared_mode_certificate` 的 V6-d 段内新增 (b) 子判据 —— 位置 = **C-2 之后、C-3 之前**(per design **D-6**),tag 常量 `kR1b = "[CP-03-VC-CERTIFICATE/R1-b] "`(**尾部空格**,与 `:884-888` 既有 4 个常量同型)
- [x] A2.6b **成功 detail 的措辞同步**:`include/tlm/cep/collective_plane_contract.hh:1006` 的「M-2 共享模式证书全绿(**C-1′ / C-2 / C-3 / C-4**)」⇒ 改为「**六条证书**」并补 `R1-b / R1-c`(per metis **P1-12**)⇒ ⛔ 否则 spec 写六条、运行时输出四条 ⇒ 读者会误判
- [x] A2.7 key 形态校验:非空 ∧ 全 ASCII 数字 ∧ (长度 > 1 ⇒ 首位非 `'0'`)⇒ 再用 `std::from_chars` 转换(⛔ 不用 `stoul`)⇒ 数值 vs `vc_count` 判越界 —— per design **D-4**
- [x] A2.8 判定序列 = **b-1 集合非空 → b-2 key 形态 → b-2.5 越界(数值 >= vc_count)→ b-3 完备(key 集合 == {0..vc_count-1})→ b-4 跨实例全等**;任一不过 ⇒ 立即 `return FAIL`
      > ⚠️ **必须先判越界再判缺失**(per metis **P1-9**)⇒ 否则 `"3"` → `"4"` 的变异会误报成「完备性不足」⇒ AC-10③ 的红因不稳
      > ⚠️ **b-1 结构性不可达**(C-1′ `:923-926` 已先判同一情形并 return)⇒ 保留**仅作防御**,⛔ **不得**被读作一条独立判据(per metis **P2-16**)
- [x] A2.8b **溢出处置**:`from_chars` 返回 `result_out_of_range` ⇒ 归「**越界 FAIL**」⇒ ⛔ 不得抛异常、⛔ 不得当作 0(per metis **P1-10**)
- [x] A2.9 ⛔ **不实现** ordering **值域**检查(既有 `scripts/test/cep_decl_alignment_check.sh` 的 `jsonschema` 已挡)⇒ 登记为 **L-3**
- [x] A2.10 **净**：既有 V6-d 6 条反向 fixture + 全部 `[cep]` 仍绿
      > ✅ **实测**:`[cep][ualink-inc]` ⇒ **422 assertions / 14 test cases**(= 基线 402/13 + 新用例 20/1)⇒ **既有 6 条反向 fixture 的断言数未变** ⇒ 归因前缀未被抢
      > ✅ `[cep]` ⇒ **510 cases / 11644 assertions**;`ctest -j4` ⇒ **169/169**;35 闸 ⇒ **35/35 rc=0**;`kContractRuleCount` 仍 **6**

---

## A3 · 判据 (C-1′-S) —— scope 完备性(tag `R1-c`)

- [x] A3.1 **红 · AC-5**：`decl.plane_binding` **插键** ⇒ 必红 + 精确前缀 `[CP-03-VC-CERTIFICATE/R1-c]` + 红因文本**含**「不在 `certificate_scope`」且 **⛔ 不含**「plane 未登记」
      > ⚠️ **变异实例 MUST 用 `chi_rnf`**(per metis **P1-8** 的实测)—— `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c` **不在** `plane_binding` 里 ⇒ 是真「**插键**」;⛔ **不用** `moesi_gpu` / `mesif`(它们**已在** `plane_binding` 里,值为 `null` ⇒ 那是「改值」⇒ 与 AC-5 的描述不符)
- [x] A3.2 **红 · AC-11**：令绑定 `FabricCollectivePlane` 的实例在 `per_network_orderings` 中**查不到**(即无任何 VC 绑定)⇒ 必红,前缀 `R1-c`
- [x] A3.3 **绿 · 实现**：新增 (C-1′-S) 子判据 —— 位置 = **scope 项循环之后、C-4 之前**(per design **D-5 / D-6** 的**修订**),tag 常量 `kR1c = "[CP-03-VC-CERTIFICATE/R1-c] "`
      > ⚠️ **⛔ 不放循环之前**(per metis **P0-1** 的实测):既有 C-3 的两条反向 fixture(`test/test_cep_ualink_inc.cc:501-520`)用「**改写** `certificate_scope[0].plane`」的变异形态 ⇒ 循环前会让 R1-c **抢归因** ⇒ 既有断言 `rfind("[CP-03-VC-CERTIFICATE/C-3]", 0) == 0` 必失败 ⇒ **既有 6 条反向 fixture 零改动**是本位置的**硬约束**
- [x] A3.3b **空 `plane_binding` 的口径**(per metis **P2-17**):零键 ⇒ C-1′-S **无主体** ⇒ **vacuous PASS**(⛔ 与 V6-c 的「零键 ⇒ SKIP」**语义不同**)⇒ 登记进诚实边界段(per **L-7**)
- [x] A3.4 判定序列 = **s-1 空绑定跳过 → s-2 plane 未登记 FAIL → s-3 本地模型跳过 → s-4 无 VC 绑定 FAIL → s-5 (plane,network) 不在 scope FAIL**
- [x] A3.5 ⛔ **不**用 `per_network_mappings` 的实例集合(那是「有 vc_binding 的实例」)⇒ 本判据用 **`decl.plane_binding`** 的实例集合(「声明了 plane 绑定的实例」)
- [x] A3.6 **净**：既有 V6-d 6 条反向 fixture + 全部 `[cep]` 仍绿
      > ✅ **实测**:`[cep][ualink-inc]` ⇒ **433 assertions / 15 test cases**(= A2 后 422/14 + 新用例 11/1)⇒ **既有 C-3 两条 fixture 未被抢归因**(metis **P0-1** 的「循环后」决策**实测成立**)
      > ✅ `[cep]` ⇒ **511 cases / 11655 assertions**;`ctest -j4` ⇒ **169/169**;35 闸 ⇒ **35/35 rc=0**;`kContractRuleCount` 仍 **6**;声明数据零 diff

---

## A4 · 并排取证(G3)

> ⛔ **不得**用「同一份 fixture 在新旧两套代码上跑」冒充并排 —— 必须是**跨实现**的证据。

- [ ] A4.1 **AC-3 并排 · 版本无关探针**(per metis **P0-2** 的实测约束):⛔ **探针源码不得引用 `per_network_orderings`**(旧 HEAD 的 `VcBindingSnapshot` 无此字段 ⇒ 引用则旧 header **编译不过** ⇒ 取证退化为同实现内自证)⇒ 探针**只**调 `load_vc_binding_snapshot(影子 protocol_instances, 真实 fabric)` + `evaluate_shared_mode_certificate`
      > 用 **git worktree** checkout 改动前的 HEAD ⇒ 旧/新分别编译(`g++ -std=c++17 -I<worktree>/include -I<worktree>/external/json/include probe.cc`)⇒ 用**同一份毒化输入文件**(影子 `include/tlm/cep/protocol_instances.json`):旧实现 **PASS** / 新实现 **FAIL**(`R1-b`)
      > **命令 + 输入影子文件 + 两端输出**三者落 `.rddf/reviews/cpptlm-cep-transport-vc-consistency-oldnew-evidence.md`（待产出） ⇒ ⛔ 不得只写一句「旧实现必绿」
- [ ] A4.2 **AC-12 并排**：AC-5 的同一**影子输入文件**:旧实现 **PASS** / 新实现 **FAIL**(`R1-c`)⇒ 证据同上文件
      > ⚠️ **诚实边界须逐字写明**:旧实现**根本不读** `decl.plane_binding` ⇒ 它的「旧绿」是**平凡**的 ⛔ 不得包装成「旧实现也检查了但更宽松」
- [ ] A4.3 **AC-15 真实 gate 路径**：新增一条经 `run_collective_plane_contract` 调用链触发的用例(⛔ 不得只在测试里手搓 `decl` 直调)⇒ 断言新判据的结论**折进 `CP-03` 行**且 `rule=` 字段**仍为** `CP-03`(⛔ 未新增第 7 条 `ContractRule`)
      > ⚠️ **断言形态**(per metis **P0-7** 的实测):CP-03 行的 `detail` 是**拼接**的(`include/tlm/cep/collective_plane_contract.hh:1444-1453` 的 `d += " | " + cert`)⇒ 新前缀**不在位置 0** ⇒ ⛔ **不得**用 `rfind(prefix, 0) == 0`(那是**直调层**写法)⇒ 必须:**遍历 `report.lines`** ⇒ 找 `line.rule == ContractRule::VcReservation` ⇒ 断言 `line.detail.find("[CP-03-VC-CERTIFICATE/R1-b] ") != npos`
      > ⚠️ **影子文件纪律**:real gate 的变异必须通过 `paths.protocol_instances` / `paths.declaration` 指向**临时影子文件** ⇒ ⛔ **不得**修改仓内 JSON(那是「改声明数据变绿」);baseline 正控继续用仓内真实路径
      > ⚠️ **规则数断言**:同时断言 `report.lines.size() == kContractRuleCount`(锁 6 条,per metis **P1-11**)
- [ ] A4.4 **AC-14 回归**：新 tag 不被 `[CP-03-VC-CERTIFICATE]` 与 `[CP-03-VC-CERTIFICATE/C-1′]` 的**精确前缀** `rfind` 命中 + 既有 fixture 仍绿
- [ ] A4.4b **既有 6 条反向 fixture 的归因前缀逐一不变**(per metis 的**隐藏意图** + **P1** 项):C-1′ 2 条 · C-2 1 条 · C-3 2 条 · C-4 1 条 ⇒ 各自的前缀断言**逐字不变** ⇒ ⛔ 「仍绿」不等于「归因未变」(断言写错也可能仍绿)
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
- [ ] A5.7 同上 · 登记 **L-1**(单实例 network 空转)/ **L-2**((b) 只证跨实例一致)/ **L-3**(值域由既有 schema 闸挡)/ **L-6**(R1-c 置于循环后 ⇒ scope 缺口可能被 first-failure-wins 遮蔽)/ **L-7**(空 `plane_binding` ⇒ C-1′-S vacuous PASS)
- [ ] A5.7b 同上 · **`:129`(R3 段「四条证书」)与 `:142`(R5 表「M-2 四证书」)**同步改为六条(per metis **P1-14**)⇒ ⛔ 不留读者可见的过时表述
- [ ] A5.8 `docs/adr/ADR-CEP-01-fabric-collective-plane.md` · **末尾追加** `## Status Update`(C-1′-S 锚点)—— ⛔ **正文逐字不改**
- [ ] A5.9 `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` · **末尾追加** `## Status Update`((b) 锚点 + IC2 职责边界澄清)—— ⛔ **正文逐字不改**

---

## A6 · 冻结面五步链(G4 后半)

- [ ] A6.1 产物 commit(源码 + 测试 + 3 份文档 + delta spec,一次 commit)
      > ⚠️ **delta spec 必须在 A7 先写好**(本 commit 含它 ⇒ per metis **P0-5** 的顺序修正)
- [ ] A6.2 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #347"`(**3 处产物同一次重算**)
      > ⚠️ **授权 Entry 号 MUST 是台账中真实存在的 id**(per `scripts/test/cep_freeze_check.sh:131-139` 的**精确匹配**校验:台账 `### ` 标题里 `— ` 之前的部分必须**完全等于**该参数)⇒ **⛔ 不得**留占位符 `Entry #XXX`(会被闸直接拒绝)⇒ 本 change 的实施期授权 Entry = **Entry #347**(A0.1 落盘时创建;⛔ **不是 #346** —— 后者已被 metis gap 分析处置占用)
      > ⚠️ **产物 commit 必须先于 `--update`**(per C1 已验证的顺序,Entry #342)
- [ ] A6.3 清单 commit(`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`)
- [ ] A6.4 无参复验 `bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**;`artifact_count` **不新增**(= 78)
- [ ] A6.5 `git diff --exit-code` 对 4 个声明数据/schema 文件 ⇒ 空(**AC-13** 终态)

---

## A7 · 契约 delta(G4 后半 · **在 A6 之前**)

- [ ] A7.1 delta spec: `MODIFIED` `cep-collective-plane-contract-shared-mode-certificate` —— 新增 **2 个 Scenario**((b) 排序假设逐 VC 一致 + 完备 / (C-1′-S) `(plane, network)` pair 完备性)+ 更新 V6-d Scenario 的「四条证书」表述为**六条**(C-1′ / C-2 / C-3 / C-4 / R1-b / R1-c)与归因前缀清单
- [ ] A7.2 ⛔ **不新增 `Requirement`** —— 两条判据是既有 `cep-collective-plane-contract-shared-mode-certificate` 的**子判据**(⛔ 与「不新增 `ContractRule`」同型)
- [ ] A7.3 ⛔ 不改 `openspec/specs/cep-collective-plane-contract/spec.md`(归档时才合并 ⇒ **另行触发**五步链)
- [ ] A7.4 `openspec validate cpptlm-cep-transport-vc-consistency --strict` ⇒ rc=0

---

## A8 · 0 回归与验收(AC-8)

- [ ] A8.1 `cmake --build build -j$(nproc)` ⇒ 编译通过
- [ ] A8.2 `ctest --test-dir build -j4` ⇒ **全绿** ⇒ **⛔ 断言「不低于改动前的 169/169」**(per metis **P1-15**:新增测试后 ctest 项数会增加 ⇒ 写死基线数会误判)
- [ ] A8.3 `./build/bin/cpptlm_tests "[cep]"` ⇒ 全绿 ⇒ **⛔ 断言「不低于改动前的 508 cases / 11516 assertions」**(同上;case / assertion 数会因新增反向 fixture 增加)
- [ ] A8.4 **35 个** `cep_*.sh` 闸 rc + ✗ 双判全绿
- [ ] A8.5 `bash scripts/test/docs_sync_check.sh --strict` ⇒ rc=0
- [ ] A8.6 `python3 scripts/test/agents_md_lint.py` ⇒ rc=0
- [ ] A8.7 `bash scripts/test/cep_freeze_check.sh` 无参 ⇒ rc=0
- [ ] A8.8 **17 条 AC 逐条判定**(每条带工具证据)⇒ 落 `.rddf/reviews/cpptlm-cep-transport-vc-consistency-verify.md`（待产出）
- [ ] A8.9 **遗留清扫**:本次范围内 0 处 TODO / FIXME / 临时调试日志 / 被 skip 的测试 / 未替换的 mock / 注释掉的代码 / 临时 fallback 分支

---

## A9 · 归档(M6)

- [ ] A9.1 `openspec archive cpptlm-cep-transport-vc-consistency` ⇒ delta 合并进 `openspec/specs/cep-collective-plane-contract/spec.md`
      > ⚠️ 该 spec **在冻结集**(`openspec/specs/cep-*/spec.md` glob)⇒ **另行触发**第二次五步链(per metis **P0-6** 的展开)
- [ ] A9.2 **第二次五步链**(逐步可勾选):① 归档授权 Entry(台账)→ ② 产物 commit(`openspec/specs/cep-collective-plane-contract/spec.md`)→ ③ `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #<归档授权 Entry>"` → ④ 清单 commit(`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`)→ ⑤ 无参复验 `bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**
      > ⚠️ **产物 commit 必须先于 `--update`**(per C1 已验证的顺序);⚠️ `artifact_count` 仍为 **78**(该 spec **已在**冻结集内 ⇒ 只改 hash)
- [ ] A9.3 归档后 KPI 回填(Proposed **1/3 → 0/3**)+ 台账 Entry
- [ ] A9.4 ⛔ **本 change 归档后**才可立项 **C3**(`R-c` + `R-d`)与 **C4**(MUST-FIX-1/2)

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
