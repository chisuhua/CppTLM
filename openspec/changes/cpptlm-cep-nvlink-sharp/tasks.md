# Tasks — cpptlm-cep-nvlink-sharp v1.0(Stage 2 · 第 1 个 change · **实现层** · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 第 1 个 change(**实现层**)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-nvlink-sharp/spec.md`](specs/cep-nvlink-sharp/spec.md) · [`specs/cep-collective-plane-contract/spec.md`](specs/cep-collective-plane-contract/spec.md)
> **口径**: 台账 **Entry #186**(规约层归档 + 移交)· **#185 / #184 / #183 / #182** · **#175**(规约层建档)· **#179 / #178**(归因 id 唯一化)· **#171**(Decision D)· **#169 / #170**(三则 ADR 定稿)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(basis 唯一人工审计面)
> **估时**: **8 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 change = `cpptlm-cep-nvlink-sharp-state-spec`(规约层,✅ 已归档)· **本 change = 实现层**

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ⬜ 待实施 | 转正登记 + 冻结面实测 + KPI 回填 + 失指 change 名订正 |
| 1 载体形态定稿 | ⬜ 待实施 | `nvlink_adapter.hh` + `nvlink_broadcast_null_tracker.hh` + 编译期结构断言 |
| 2 声明面 | ⬜ 待实施 | `nvlink_4` 实例 + `collective_planes.json` 三块 + **V6-c** 载体 + **CP-05** 扩展 + 装配期校验 |
| 3 判据收口 | ⬜ 待实施 | 正向测试 + **9 组**反向 fixture + probe/脚本 + ctest 注册 + 文档同步 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + `validate` + KPI **1/3 → 0/3** + **冻结面两步授权** + 归档 + 07 号同步 |

---

## Phase 0 — 治理前置

- [ ] 0.1 **转正裁决**(per `obj §4.7.2` step 1:DRI 裁决「① 建 change」)⇒ 台账 Entry 登记 ⇒ ✅ 已落
- [ ] 0.2 **冻结面实测确认**:本层归档将改 **2 份 M1 冻结产物**
      (`openspec/specs/cep-collective-plane-contract/spec.md` + 视 §9 决定是否含 `cep-collective-plane-base/spec.md`)
      ⇒ **归档期必须走两步授权**(`cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ commit ⇒ `--update` ⇒ commit)
      ⇒ ⛔ **不得**在无授权下改冻结产物
- [ ] 0.3 **KPI baseline 回填**:**0/3 → 1/3**,**8 处**
      (`obj §5.2` / `§5.3` / `§4.0` #13 行 / `phase-10.md` / `roadmap.md` / `AGENTS.md` / umbrella improvement / 队列 #13 improvement)
- [ ] 0.4 **失指 change 名订正**(台账 §10 登记项):`include/tlm/cep/collective_plane.hh` 的
      `cpptlm-cep-nvlink-sharp-adapter` ⇒ 指向**本 change**(代码面)⇒ ⚠️ 其余两处
      (`openspec/specs/cep-collective-plane-base/spec.md` = 冻结面 / `ADR-CEP-00` = architect 域)⇒ **不在本层**,登记 backlog

## Phase 1 — 载体形态定稿

- [ ] 1.1 **`nvlink_adapter.hh`**(新 · header-only · 独立头)
      ⇒ ① **零自有表副本**(禁词表:`IngressDecision::` / `RegisterResult::` / `EpochState::` / `OrderingRule::` /
      `VcReservationState::` / `CollectiveScope::` / `Basis::` / `kBasis*` / `kPendingSemanticItems`)
      ② **无 `transition_*`** ③ **不暴露** `collective_region_table` ④ **委托** `nvlink_sharp_state_spec.hh`
      ⑤ **类型名别名允许**(`using ... = nvlink_sharp::...;`)
- [ ] 1.2 **`nvlink_broadcast_null_tracker.hh`**(新 · header-only · 独立头)
      ⇒ ① caps / behavior **逐字段取自** `kTrackerTraits[4]`(**零** caps 字面量)
      ② `scope=NONE` ⇒ `lookup` **恒** `BROADCAST` + `view.valid == false` + `hazard == NONE`
      ③ 安装面**显式 no-op** ④ snoop 三方法走 `SnoopMechanics` 公共机械 ⑤ **零目录边**
- [ ] 1.3 **编译期结构断言**(带**归因 id** 文案):traits 行锁 + 禁词表的编译期可判部分 + 接口面零改动
- [ ] 1.4 **零副本 / 无转移表**的**运行期兜底判据**(铁律 ③:编译期锁**不得**只以「编译红」充当 ctest 判据)

## Phase 2 — 声明面

- [ ] 2.1 **`nvlink_4` 实例声明**(`include/tlm/cep/protocol_instances.json`):**13 键齐备**(schema `required` **实读**)
      · `protocol_version = nvlink-4.0` · `sharer_tracker.impl = BroadcastNull`(caps 与 traits **逐字段相等**)
      · `atomic = {"supported": true}`(**省略** `intent_field`)· `perm` = 4 值锁全量(具体子集 = pending `N-8`)
      · `vc_binding` / `ordering_assumption` 绑 `compute_noc`(coherence 流量 ⇒ 不受 Decision D 约束)
      · `repo_owned_instance_ids` **+1**
- [ ] 2.2 **`collective_planes.json`**:`plane_binding` 加 `nvlink_4` → `LocalCollectivePlane`
      · `registered_regions` 增 1 条(`mode = nc_drop_cacheable`,per R3)⇒ 集体面**首个消费者**
      · `collective_vc_reservation` 保持 `[]` · `vc_reservation_declaration` 四字段**只消费不改**
- [ ] 2.3 **`collective_plane_contract.hh`**:① **V6-c** 前提约束(归因 `CP-03-VC-PRECONDITION`,**枚举式**白名单)
      ② **CP-05** 标识清单补 `collective_region_table` + coherence 侧扫描集合扩展(**复用** `CP-05`)
- [ ] 2.4 **装配期校验**:实例声明 13 键 / caps 与 traits 一致 / 集体面绑定一致 / VC 预留门 ⇒ **单一**消费点

## Phase 3 — 判据收口

- [ ] 3.1 **正向测试**(`test/test_cep_nvlink_sharp.cc`):每条**负向断言配正控**(铁律 ①)
- [ ] 3.2 **9 组反向 fixture**(每组**必红** + **归因 id** 命中 · 铁律 ②)
      F-1 成员字面量 · F-2 `transition_*` · F-3 `BroadcastNull` caps/行为 · F-4 实例声明 ·
      F-5 V3 barrier 分类 · F-6 算子族 6 值 · F-7 partial failure · F-8 V6-c · F-9 CP-05 清单
- [ ] 3.3 **基线正控**(脚本自检:干净载体必绿 ⇒ 否则 fixture 组失效)· **两类红形态**都支持(编译期 / 运行期)
- [ ] 3.4 **归因 id 唯一性核对**:同一义务**不得**有两个 id(per `design §7` 唯一登记处)
- [ ] 3.5 **探针 + 驱动脚本**(`scripts/test/cep_nvlink_sharp_probe.cc` / `..._reverse_verify.sh`)+ **ctest 注册**
- [ ] 3.6 **文档同步**:07 号 §5.1.1 NVLink 行 + §11 两行计数 · 队列 #13 improvement(反向 fixture 组数 5 → 9)· `collective_plane.hh` 失指名

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁 **7** 脚本全 rc=0 + `format.sh --check` rc=0
- [ ] 4.2 `ctest` 全绿(含新注册的 `[cep][nvlink-sharp]` 标签)
- [ ] 4.3 `openspec validate cpptlm-cep-nvlink-sharp --strict` rc=0
- [ ] 4.4 ⚠️ **纸面契约复核**:确认**下一队列**(队列 #14 `cpptlm-cep-ualink-inc`)已排期 ⇒ 否则本层**不得**归档
- [ ] 4.5 KPI **1/3 → 0/3** 回填(**8 处**,同 0.3)
- [ ] 4.6 **冻结面两步授权** ⇒ **归档**(delta 合并进 `openspec/specs/cep-nvlink-sharp/` + `openspec/specs/cep-collective-plane-contract/`)
      + `VIRTUAL_PATHS` 登记 + 07 号同步 + 台账 Entry

---

## ⚠️ 归档前必须闭合的**登记债**

| 项 | 位置 | 处置 |
|---|---|---|
| `openspec/specs/cep-collective-plane-base/spec.md` 的失指 change 名残余 | 该 main spec | ⚠️ **M1 冻结面** ⇒ 若本层一并修 ⇒ 走**两步授权**;否则**单独立项** |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` 的失指 change 名残余 | 治理 ADR(真源 3) | ⚠️ **architect 域** ⇒ **不在本层** |
| `collective_planes.schema.json` 的 `registered_regions` 项**无实例归属键** | schema | ⚠️ 本层**不改** schema ⇒ 登记 backlog |
| `ADR-CEP-01` 附录 V2(10 流四方 diff 的 07 号 / ADR-CEP-05 侧未实现) | 判据 | ⚠️ 由队列 #13 / #14 补齐 ⇒ 本层**只**承担本 change 相关的部分 ⇒ 登记 backlog |
