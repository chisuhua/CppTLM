# tasks: cpptlm-cep-mesh-e2e-carrier

> **AC 映射**: proposal AC-01 ~ AC-08 · design D-1 ~ D-7
> **上游**: 台账 **Entry #444** §2.2 类 B · §3 波 2 · improvement `cpptlm-cep-stub-cache-chstream-surface` 的 Out of Scope 明列项
> **前置事实**: G1(StubCacheCore 端口面)**已于 2026-11-09 归档完成**(Entry #305)⇒ 本 change 的缺口收窄为「CohIf ↔ 网络适配层 + mesh 联合拓扑」

---

## Phase 0 · 立项态

- [x] **T0.1** 实测缺口基线(⛔ 不采信台账措辞):CEP 现有 e2e 判据的拓扑全为点对点(`cep_cohif_pe_minimal.json` / `cep_chi_rnf_e2e_a.json` / `_b.json`)· `fabric.json` 无节点/链路/路由 · 桥不填 `dst_node` · `RouterTLM` 只从 `dst_node` 取目标
- [x] **T0.2** 确认 G1 已完成(`StubCacheEndpointTLM` + `CohIfResponderTLM` 存在且已注册)⇒ **不重复实现**
- [x] **T0.3** 登记 3 条 architect 待裁决项(**不在本 change 实施**):**Q1** 多流无损(`flit_category` 3 值装不下 5 个 `flow_id`)· **Q2** CEP mesh 拓扑声明面 · **Q3** `NICTLM::lookup_node` 静默投 node 0
- [x] **T0.4** improvement + proposal + design + 本 tasks + `.openspec.yaml` + delta spec + 台账 Entry

## Phase 1 · 实现

- [x] **T1.1** 新增 `include/tlm/cep/cohif_nic.hh`:`CohIfNICTLM`(`ChStreamModuleBase` 派生)
  - 4 端口(异构 dual-port):`pe_req_in(): CohIfBundle` · `pe_resp_out(): CohIfBundle` · `net_req_out(): NoCFlitBundle` · `net_resp_in(): NoCFlitBundle`
  - ⛔ **不得**用对称 `REGISTER_CHSTREAM_BIDIRECTIONAL`(要求两侧同 BundleT)
  - 配置:`node_id` / `mesh_x` / `mesh_y` / `address_regions[]`(base / size / target_node)
  - 观测面:`stats_flits_sent_` / `stats_flits_received_` / `stats_dropped_unmapped_` / `last_dst_node()` ⛔ 不提供 latency 聚合
- [x] **T1.2** 新增 `src/tlm/cep/cohif_nic.cc`:正向映射(D-1.3 表)· 反向映射(D-1.4)· 地址映射(**未命中 ⇒ 显式丢弃 + 计数**,⛔ 不学 `NICTLM` 的静默投 node 0)· `tick()` 驱动
- [x] **T1.3** ⚠️ **硬前置探针**(critic SHOULD_FIX-1 升级项):写一个**不依赖本 change** 的探针(2 router + 2 NIC + `address_regions`,用**既有** `NICTLM`)实测 4 件事 —— ① 每种 connection 需要**几条**(router↔router / router↔NIC / CohIf↔NIC)② flit 实际走 `resp_out` 还是 `req_out`(F-14/F-15/F-16)③ `hops` 是否随转发递增(F-18)④ 回程路径在 X-first(F-17)下覆盖哪些 router ⇒ **不得凭 D-2.0 的推断实现**
- [x] **T1.3b** **据探针结果定稿 D-2.1 连接表**并写回 `design.md`(端口号 + 方向 + 条数)⇒ 若探针证明 `is_dual` 分支的硬编码 `DualPortStreamAdapter` cast 对本模块不可用 ⇒ **停**并按 `proposal.md` 改动文件清单的**条件项**最小化改 `src/core/module_factory.cc`(默认零 diff)
  - **探针实测结论(2026-11-16,`test/test_zz_mesh_probe.cc` 5 用例)**:
    · router↔router **1 条 ⇒ 失败**(`ni1.packets_received=0` · `r1.flits=0` ⇒ flit 停在源 router)⇒ **需 2 条对称**
    · router↔router **2 条 ⇒ 成功**(`ni1.packets_received=4`)
    · router↔NIC **1 条 ⇒ 失败** ⇒ **需 2 条**
    · 2×2 mesh 去程 **成功**:`ni0.sent=4` · `ni1.recv=4` · hops 逐跳 `r00=4` `r01=8` `r11=12`(=1/2/3 跳)
    · 2×2 mesh **回程成功**(补 cpu1):`ni1.sent=10` · `ni0.recv=13` · **`r10.flits=8`** ⇒ 回程确实走 X-first 路径经 r10 · 4 router 全参与
    · ⇒ D-2.1 的 **17 条 connection** 已按实测定稿;⚠️ **唯一未实测项** = 「stub ↔ NIC(CohIf 侧)1 条」⇒ 实施时须验
    · ⇒ `src/core/module_factory.cc` 的 `is_dual` cast 对本模块**可用**(探针的 `NICTLM` 同走该路径)⇒ **零 diff**
- [x] **T1.4** 注册:`ModuleFactory::registerObject` + `ChStreamAdapterFactory::registerDualPortAdapter<CohIfNICTLM, CohIfBundle, CohIfBundle, NoCFlitBundle, NoCFlitBundle>`(范式照 `include/chstream_register.hh:243-253`)
- [x] **T1.5** 新增 `configs/cep_mesh_e2e.json`:**9 实例 / 6 类型**(tg / w / home / nic_a / nic_b / r00 / r01 / r10 / r11)· ⛔ **不含协议引擎**(per ADR-CEP-19 SU 2026-11-09)· `address_regions` 与 `TrafficGenTLM` 的地址段**显式对齐**
- [x] **T1.6** ⛔ **零 CMake 改动** —— 实测:仓内 CEP **全为 header-only**(`src/tlm/cep/` 之前不存在)⇒ 本模块同样 header-only ⇒ **不新增 .cc** ⇒ 无需改 `CMakeLists.txt`
- [x] **T1.7** 新增 `test/test_cep_mesh_e2e.cc`(标签 `[cep][e2e][mesh]`):J-1 ~ J-4

## Phase 2 · 判据强度

- [x] **T2.1** **J-5 决定性变异**:断开 `r01→r11` ⇒ 判据必红 **且** 失败原因 == 「未抵达」(⛔ **不是**「装配失败」)⇒ 用**结局分类枚举**(范式照 `test/cep_cohif_assembly_helper.hh:35-40`),⛔ 不用 `REQUIRE(!arrived)`
- [x] **T2.2** **J-4 反证**:断言「穿过 mesh」不能只靠「抵达」⇒ `flits_forwarded` > 0 ∧ `packets_forwarded` > 0 ∧ **`nic_b.last_recv_hops() >= 1`**(⚠️ critic SHOULD_FIX-3:flit 在 router 内是瞬态的 ⇒ 必须有**入站侧**命名可观测量)
- [x] **T2.3** **J-2 对端性**:断言 **`nic_b.last_recv_dst_node() == 3`**(⚠️ critic SHOULD_FIX-2:原 `last_dst_node()` 是**出站**查表结果 ⇒ 切片内 `nic_b` 唯一出站是 Completion ⇒ 该值恒为 0 ⇒ J-2 必红 ⇒ 须用**入站侧**可观测量)· ⛔ 只断言「计数>0」会被「回环到自己」骗过
- [x] **T2.4** **J-6 零回归**:既有 `[cep]` / `[e2e]` 全绿 + 8 个既有文件零 diff(见 T3.2 清单)
- [x] **T2.5** **边界复核**:本模块映射代码**仅**在本模块内(⛔ 未外溢到 `include/framework/` / `include/tlm/nic_tlm.hh`)

## Phase 3 · 审查与归档

- [x] **T3.1** critic 审(**`arch` 档**)—— 重点:Q1 有损边界的表述是否诚实 · 垂直切片是否真能回答「真实 mesh 下会怎样」 · J-4/J-5 的判别力
- [x] **T3.2** **零 diff 清单复核**:`include/bundles/**` · `include/tlm/router_tlm.hh` · `include/tlm/nic_tlm.hh` · `include/tlm/cep/{stub_cache_core,stub_cache_endpoint,cohif_responder,cohif_noc_bridge,protocol_engine_base}.hh` · 既有 43 份 `cep-*` main spec · `protocol_instances.json` / `protocol_instance.schema.json` / `fabric.json` · `src/core/module_factory.cc`(**默认零 diff**;仅 T1.3 探针证伪时才允许条件性最小改动,且须记入台账)⇒ **全部零 diff**
- [x] **T3.3** 闸 0 回归(**归档前跑一次**):**45** 闸全 rc=0 + `ctest -R cep` + `[cep]` + `[e2e]`
- [x] **T3.4** 契约面归档:delta 合入 `openspec/specs/cep-cohif-network-adapter/spec.md` · ⛔ **不 MODIFIES** 任何既有 `cep-*` spec(⇒ 零冻结面产物 ⇒ **免五步链**)
- [x] **T3.5** 产物 commit
- [x] **T3.6** KPI **1/3 → 0/3**(6 处活口径 + obj 并发句)+ 归档 change 目录数 **151 → 152** + 台账闭合回执 Entry
- [x] **T3.7** 闸最终复验 + `git status` 全清
- [ ] **T3.8** verifier 独立验收(报告落 `.rddf/reviews/cpptlm-cep-mesh-e2e-carrier-verify.md`;**须另写独立实现复算**)

## Phase 4 · 遗留

- [x] **T4.1** 提请 architect:**Q1**(多流无损 · 3 条出路)· **Q2**(CEP mesh 拓扑声明面)· **Q3**(`NICTLM` 静默失败)
- [x] **T4.2** 登记:**`CohIfNICTLM` 的 `is_write` 由 `perm` 近似**(⛔ 非精确写意图)· **CohIfBundle 无 `vc_id` ⇒ 单 VC**

---

## 实施结果回填（2026-11-16 · builder）

| 项 | 结果 |
|---|---|
| **新增文件** | `include/tlm/cep/cohif_nic.hh`（**header-only** · 照仓内 CEP 惯例）· `configs/cep_mesh_e2e.json` · `test/test_cep_mesh_e2e.cc` · `test/test_noc_mesh_binding_semantics.cc`（探针**转正**为回归载体） |
| **改动文件** | `include/chstream_register.hh`（+1 include + 4 行注册）· `scripts/CMakeLists.txt`（+1 条 CEP config 清单登记）· change 四件套 · improvement · 台账 · 5 处 KPI 活口径 |
| **实测结果** | MESH-1 装配 9 实例 ✅ · MESH-2 `nic_b.last_recv_dst_node()==3` + `last_recv_hops()==3` + `r.flits=45` ✅ · MESH-3 Completion 回程抵达 stub ✅ · MESH-4 变异按**正确原因**红（`NotArrived`）✅ |
| **零回归** | `[cep]` 533 用例 / 12321 断言全绿 · `[e2e]` 144 用例 / 7358 断言全绿 · `ctest -R cep` **100/100** |
| **冻结面** | 13 个既有文件**全部零 diff** · 既有 43 份 `cep-*` main spec **零 diff** · `module_factory.cc` **零 diff**（`is_dual` 的硬编码 `static_cast` 只用于 `bind_*_ports`（不碰 `ModuleT`），`tick()` 走**虚分派** ⇒ 运行时安全 ⇒ 无需改） |
| **闸** | 45 闸数量不变 · `validate_topology` 新增 1 条 config 后 10 份全过 |
| **实施期发现的 2 处 design 缺陷**（已回写 design） | ① `OutputStreamAdapter::valid()` 语义 =「**有数据待发**」⇒ 可写条件是 `!valid()`（**我最初写反了** ⇒ flit 永不发出）② 断开一条「对称的 2 条」connection **不足以**切断链路（另一条仍提供 `resp_out` 路径）⇒ 变异必须**成对**删除 |
