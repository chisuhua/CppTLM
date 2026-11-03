# Tasks — cpptlm-cep-nvlink-sharp v1.1(Stage 2 · 第 1 个 change · **实现层** · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 第 1 个 change(**实现层**)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-nvlink-sharp/spec.md`](specs/cep-nvlink-sharp/spec.md) · [`specs/cep-collective-plane-contract/spec.md`](specs/cep-collective-plane-contract/spec.md)
> **口径**: 台账 **Entry #187**(oracle + metis 双审修订)· **#186**(规约层归档 + 移交)· **#185 / #184 / #183 / #182** · **#175**(规约层建档)· **#179 / #178**(归因 id 唯一化)· **#171**(Decision D)· **#169 / #170**(三则 ADR 定稿)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(basis 唯一人工审计面)
> **估时**: **8 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 change = `cpptlm-cep-nvlink-sharp-state-spec`(规约层,✅ 已归档)· **本 change = 实现层**
>
> ⚠️ **v1.1 修订**(2026-11-03,Entry #187):按 **oracle + metis** 双审结论修订 —— 关键 5 项:
> ① **V3 改名 `barrier_key_class` + 落点改协议中立**(`collective_plane.hh`)+ **结构体输入**(禁标签入参);
> ② **V6-c**:只判**非空**绑定值 + 零键 ⇒ **SKIP** + 白名单**注册表派生** + 归因走 `CP-03` 的 **detail 前缀**;
> ③ **F-3 / F-6 恒真** ⇒ 正控改为**毒化真源**(traits 第 4 行 / 源文本枚举项计数);
> ④ **冻结面 = 3 份 + manifest**;**归档顺序**固定为「Entry → archive → `--update` → commit → `--update` → commit」;
> ⑤ **计数同步清单**(6 处硬编码)+ **归因 id 收敛为 2 个新建 `CP-*` + 7 个 `NVA-xx`** + **元测试**。

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-03) | 转正登记 + **冻结面 3 份**实测 + KPI 回填 + 失指 change 名订正 + 台账口径订正 + 计数同步清单 |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-03) | `nvlink_adapter.hh` + `broadcast_null_tracker.hh` + **V3 载体**(`collective_plane.hh`) + 编译期结构断言 + sha256 钉版 |
| 2 声明面 | ✅ **已完成**(2026-11-03) | `nvlink_4` 实例 + `collective_planes.json` 三块 + **V6-c 载体** + **CP-05 双 profile** + **扩展**既有 loader 校验项 |
| 3 判据收口 | ✅ **已完成**(2026-11-03) | 正向测试 + **9 组**反向 fixture + **元测试** + probe/脚本 + ctest 注册 + 文档同步 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + `validate` + KPI **1/3 → 0/3** + **冻结面两步授权** + 归档 + 07 号同步 |

---

## Phase 0 — 治理前置

- [ ] 0.1 **转正裁决**(per `obj §4.7.2` step 1:DRI 裁决「① 建 change」)⇒ 台账 Entry 登记 ⇒ ✅ 已落(Entry #187)
- [x] 0.2 **冻结面实测确认**:本层归档将改 **3 份** M1 冻结产物
      ① `docs/architecture/07-coherence-exploration-platform.md`(`cep_freeze_check.sh:32` 在 `FROZEN_GLOBS` 内)
      ② `openspec/specs/cep-collective-plane-contract/spec.md`(delta 合并)
      ③ `openspec/specs/cep-collective-plane-base/spec.md` —— ⚠️ **单独立项**(本层**不并入**)
      ⇒ 另:`--update` 会**重算全部 40 件** hash ⇒ 授权 Entry 必须**逐件**列名
      ⇒ ⛔ **不得**在无授权下改冻结产物
- [x] 0.3 **KPI baseline 回填**:**0/3 → 1/3**,**8 处**
      (`obj §5.2` / `§5.3` / `§4.0` #13 行 / `phase-10.md` / `roadmap.md` / `AGENTS.md` / umbrella improvement / 队列 #13 improvement)
- [x] 0.4 **失指 change 名订正**(台账 §10 登记项):`include/tlm/cep/collective_plane.hh:206`
      ⇒ change 名 ⇒ 本 change;⚠️ **保留**「接线侧属 P3」的承诺(本层**不**接线)⇒ 附注「判读面由本 change 交付;**接线侧 P3 未排期**」
      ⇒ 其余两处(`cep-collective-plane-base/spec.md` = 冻结面 / `ADR-CEP-00` = architect 域)⇒ **不在本层**
- [x] 0.5 **台账口径订正**(append-only):`Entry #186 ⑥` 的「SHARP 归约**行为实现**」措辞 ⇒ 追加订正句
      「本层交付 = 载体 + 声明 + 判据;**不含**归约**数据路径**」⇒ **不改**原文
- [x] 0.6 **计数同步清单登记**(加实例必转红处,per `design §9.3`):逐条登记
      `test_cep_adapter_schema_contracts.cc:369/383/at(0)/at(1)` · `test_cep_chi_rnf_subset.cc:1144` ·
      `test_cep_mesif_instance_decl.cc:105` · `test_cep_moesi_gpu_adapter.cc:397` ·
      `test_cep_tilelink_instance_decl.cc:199/205` · `test_cep_collective_plane_contract.cc:100/102-103`
- [x] 0.7 **Phase 0 实测基线**:`bash scripts/test/cep_decl_alignment_check.sh` rc=0 ·
      `test_cep_schema.cc:795` 的 `rows511 == 8` 仍绿(§5.1.1 **不新增行** ⇒ 只改状态格)

## Phase 1 — 载体形态定稿

- [x] 1.1 **`nvlink_adapter.hh`**(新 · header-only · 独立头)
      ⇒ ① **零自有表副本**(禁词表 14 项,per `design §2.2`)② **无 `transition_*`**
      ③ **不暴露** `collective_region_table` ④ **委托** `nvlink_sharp_state_spec.hh`
      ⑤ **类型名别名允许**(不含 `::`)⑥ ⚠️ **数值镜像表不在文本扫描面内** ⇒ 人工核对兜底
- [x] 1.2 **`broadcast_null_tracker.hh`**(新 · header-only · **协议无关** · **不叫 `nvlink_*`**)
      ⇒ ① caps / behavior **逐字段取自** traits 的 `BroadcastNull` 行(**查找函数**,⛔ 不用硬编码下标)
      ② `scope=NONE` ⇒ `lookup` **恒** `BROADCAST` + `view.valid == false` + `hazard == NONE`
      ③ 安装面**显式 no-op** ④ snoop 三方法走 `SnoopMechanics` 公共机械
      ⑤ **不持有任何** `std::map` / `std::vector` 状态容器(源扫描 ⇒「零目录边」可证伪)
- [x] 1.3 **V3 载体**:`include/tlm/cep/collective_plane.hh` 新增
      ⇒ ① `enum class BarrierKeyClass : uint8_t { Ordering, Collective }`
      ② `struct BarrierKey { enum class Kind { Address, Group }; Kind kind; uint64_t addr; group_id_t group; }`
      ③ `barrier_key_class(const BarrierKey&)` —— **结构体输入**(⛔ 禁「仅标签」入参 ⇒ 否则恒等映射)
      ④ **双重 key / 空 key ⇒ 判红** ⑤ **不**复用 `fabric_ingress_classify` 名(已被 `INV-CEP-04-IC5` 占用)
      ⑥ ⚠️ 登记「**判读面,无生产消费者**(接线侧 P3 未排期)」
- [x] 1.4 **编译期结构断言**(带**归因 id** 文案):traits 行存在性 + 禁词表的编译期可判部分 + 接口面不变
- [x] 1.5 **「零改动」的机械定义**(`design §2.6`):接口面 = ① `SharerTrackerIf` 的 4 个纯虚签名
      ② `kTrackerTraits` 6 行值 ③ traits 查找函数 ⇒ 代码行 diff == 0(注释行可改)
- [x] 1.6 **sha256 钉版断言**:`nvlink_sharp_state_spec.hh`(**不在** `FROZEN_GLOBS` ⇒ 用钉版补冻结面缺位)
      + `sharer_tracker.hh` 接口面 sha256 ⇒ 「只读消费」从纪律变判据
- [x] 1.7 **运行期兜底判据**(铁律 ③:编译期锁**不得**只以「编译红」充当 ctest 判据)

## Phase 2 — 声明面

- [x] 2.1 **`nvlink_4` 实例声明**(`include/tlm/cep/protocol_instances.json`):**13 键齐备**(schema `required` **实读**)
      · `protocol_version = nvlink-4.0` · `sharer_tracker.impl = BroadcastNull`(caps 与 traits **逐字段相等**)
      · `atomic = {"supported": true}`(**省略** `intent_field`)· `perm` = 4 值**值域**(子集 pending `N-8`)
      · `vc_binding` / `ordering_assumption` 绑 `compute_noc`(coherence 流量 ⇒ 不受 Decision D 约束)
      · `repo_owned_instance_ids` **+1**
- [x] 2.2 **`collective_planes.json`**:`plane_binding` 加 `nvlink_4` → `LocalCollectivePlane`
      · `registered_regions` 增 1 条(`mode = nc_drop_cacheable`,per R3)⇒ 集体面**首个消费者**
      · `collective_vc_reservation` 保持 `[]` · `vc_reservation_declaration` 四字段**只消费不改**
      · ⚠️ **同批改** `derived_from.current_state`(现写「全为 null / 为空」⇒ 变假陈述)
      · ⚠️ 写进声明源:「多实例归属不可表达 ⇒ 冻结前置 = 加 `owner_instance`」
- [x] 2.3 **`collective_plane_contract.hh` —— V6-c 载体**
      ⇒ ① `evaluate_vc_precondition(decl, detail)` 独立纯函数 ② **只判非空**绑定值 ③ 零键 ⇒ **SKIP**
      ④ 白名单**从 plane 注册表派生**(定义点声明局部性;未注册名 ⇒ FAIL)
      ⑤ `ContractPaths.local_plane_whitelist` 可注入(便于 fixture 毒化)
      ⑥ 归因走 `CP-03` 行的 **detail 前缀** `[CP-03-VC-PRECONDITION]`;⛔ **不新增** `ContractRule`(族数仍 6)
- [x] 2.4 **`collective_plane_contract.hh` —— CP-05 双 profile**
      ⇒ ① `kGroupScoped[]` **补** `collective_region_table` ② 扫描面扩展经 `ContractPaths` **可注入**
      ③ ⛔ **不**把 collective 侧 adapter 头塞进 coherence 侧扫描 ④ 既有**单路径**签名行为**保持不变**
- [x] 2.5 **判据落测试面 · 装配期靠既有 loader 自然覆盖**(per `design §4.2`)
      ⇒ 判据(13 键 / caps ↔ traits / `repo_owned` / `plane_binding` 键 ⊆ 实例)落 `NVA-03` / `NVA-07` / `NVA-08`
      ⇒ ⛔ **不新增第二套装载器 / 校验器**(禁双实现;既有读者 ≥ 6)
      ⇒ 覆盖证据 = 既有 **6 处实例计数断言**因 `nvlink_4` 而转红(⇒ 该实例**确实**进入装配闸输入)
- [x] 2.6 **计数同步落地**(per `design §9.3`):逐条改 **6 处**硬编码 + **保留来源注释**

## Phase 3 — 判据收口

- [x] 3.1 **正向测试**(`test/test_cep_nvlink_sharp.cc`):每条**负向断言配正控**(铁律 ①)
      ⇒ ⚠️ 出参**全断言**(`result` / `view.valid` / `view.sharers` / `view.dirty` / `view.forward_to` / `hazard`)
- [x] 3.2 **9 组反向 fixture**(每组**必红** + **归因 id** **精确相等**命中 · 铁律 ②)
      F-1 成员字面量 · F-2 `transition_*` · F-3 **毒化 traits 行** · F-4 实例声明 · F-5 V3 两类同判 ·
      F-6 **追加 `Prod = 6`** · F-7 移除 Fault 产出 · F-8 删绑定 / 改 `mode` · F-9 绑未注册 plane 名
- [x] 3.3 **基线正控**(脚本自检:干净载体必绿 ⇒ 否则 fixture 组失效)· **两类红形态**都支持
      ⇒ 编译期红的 `static_assert` 文案须以 `<归因 id>:` **开头**;运行期红的 id **精确相等**(⛔ 不用前缀匹配)
- [x] 3.4 **归因 id 元测试**(per `design §6.3`):`design §7` 的 id 集合 ⊇ fixture 的 id 集合,且每个
      **本层新建** id 至少出现一次;**反向**也查 ⇒ 照 `cep_doc_ref_registry.tsv` 的 TSV + 校验脚本先例
- [x] 3.5 **探针 + 驱动脚本**(`scripts/test/cep_nvlink_sharp_probe.cc` / `..._reverse_verify.sh`)+ **ctest 注册**
      ⇒ 探针 SHALL 以 **argv 接收 shadow root**(读盘扫描类 fixture 的毒化副本**必须**生效)
- [x] 3.6 **文档同步**:
      ① ⛔ **07 号的同步属冻结面 ⇒ 移至 Phase 4.6**(与两步授权同批;不在本 Phase 动)
      ② `scripts/README.md`:表内计数订正(16 行 vs 磁盘 19 个 · 差异说明)+ 守门表新增行 + **反向 fixture 登记**新增节
      ③ `sharer_tracker.hh:13-24` 头注释订正(`BroadcastNull` 行为面已落地)
      ④ `collective_plane.hh:206` 失指名订正(只订名,保留「接线侧 P3」承诺)
      ⑤ 队列 #13 improvement:反向 fixture 组数 **5 → 9**
      ⑥ 台账 §10 backlog 更新 + `Entry #186 ⑥` 的**追加**订正句

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁 **7** 脚本全 rc=0 + `format.sh --check` rc=0
- [ ] 4.2 `ctest` 全绿 + **专项** `ctest -R cep_nvlink_sharp` + 记录 **case / assertion 数**(本仓惯例)
- [ ] 4.3 `openspec validate cpptlm-cep-nvlink-sharp --strict` rc=0
      + 合并后不变式:main spec Requirement 数 **1 → 2** · Scenario 数 **8 → 10** · `kContractRuleCount` 仍 **6**
      + `openspec validate cep-collective-plane-contract --strict` rc=0
- [ ] 4.4 ⚠️ **纸面契约复核**:确认**下一队列**(队列 #14 `cpptlm-cep-ualink-inc`)已排期 ⇒ 否则本层**不得**归档
- [ ] 4.5 KPI **1/3 → 0/3** 回填(**8 处**,同 0.3)
- [ ] 4.6 ⚠️ **冻结面两步授权 + 归档**(**顺序不可颠倒**):
      ① 台账追加 **Entry(授权记录,逐件列名 3 份冻结产物 + manifest)**
      ⇒ ② `openspec archive cpptlm-cep-nvlink-sharp -y`(合并 2 份 delta)
      ⇒ ③ `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ **commit**
      ⇒ ④ `bash scripts/test/cep_freeze_check.sh --update` ⇒ **commit**
      ⇒ ⑤ `VIRTUAL_PATHS` 登记(**指名**:`scripts/test/cep_doc_ref_check.sh` 的 `VIRTUAL_PATHS` 增
      `openspec/changes/cpptlm-cep-nvlink-sharp/` ⇒ 长期地址 = `openspec/specs/cep-nvlink-sharp/`)
      ⇒ ⑥ 07 号 §5.1.1 / §11 同步(与 ④ 同批 commit)· 队列 #13 improvement 状态 ⇒ 已归档 · 台账归档 Entry

---

## ⚠️ 归档前必须闭合的**登记债**

| 项 | 位置 | 处置 |
|---|---|---|
| `openspec/specs/cep-collective-plane-base/spec.md` 的失指 change 名残余 | 该 main spec | ⚠️ **M1 冻结面** ⇒ **单独立项**(本层**不并入**) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` 的失指 change 名残余 | 治理 ADR(真源 3) | ⚠️ **architect 域** ⇒ **不在本层** |
| `ADR-CEP-01` 附录 V3 行的 `fabric_ingress_classify` **描述性别名** + 与 `INV-CEP-04-IC5` 撞名 | 治理 ADR(冻结件) | ⚠️ **architect 域** ⇒ 本层**只提 SU 申请**(登记 `CP-BARRIER-CLASS` 为唯一归因) |
| `ADR-CEP-03` SU F.4「载体归属」表缺 tracker 行 | 治理 ADR(冻结件) | ⚠️ **architect 域** ⇒ 本层**只提 SU 申请** |
| `collective_planes.schema.json` 的 `registered_regions` 项**无实例归属键** | schema | ⚠️ 本层**不改** schema ⇒ 写进声明源 + backlog |
| `protocol_instance.schema.json` 的 `perm` 描述歧义(「4 值锁」vs「逐实例子集」) | schema(**M1 冻结件**) | ⚠️ 本层**不改** ⇒ backlog |
| 声明层的**建模假设**(`SC/SC` · `writeback=true` · `combination_capabilities` 三项同形) | `protocol_instances.json` | ⚠️ 无 basis 纪律 ⇒ backlog |
| `ADR-CEP-01` 附录 V2(10 流四方 diff 的 07 号 / ADR-CEP-05 侧未实现) | 判据 | ⚠️ 由队列 #13 / #14 补齐 ⇒ backlog |
