# Tasks — cpptlm-cep-nvlink-sharp-state-spec v1.0(Stage 2 · 第 0 个 change · 规约层 · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-nvlink-sharp-state-spec/spec.md`](specs/cep-nvlink-sharp-state-spec/spec.md)
> **口径**: 台账 **Entry #182**(Phase 2/3 细化 + 3 项口径裁决)· **#181 / #180**(Phase 1 + 最小测试)· **#174**(critic 第 2 轮收口)· **#173**(Task 0 普查)· **#172**(improvement 订正)· **#171**(Decision D)· **#169 / #170**(三则 ADR 定稿)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`
> **估时**: 5 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 本 change = **规约层**;第 1 个 change = `cpptlm-cep-nvlink-sharp`(实现层)

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | 🟡 **进行中**(0.1~0.4 ✅ / 0.5~0.7 待本轮闭环) | Task 0 普查 ✅(Entry #173)· Decision D ✅(#171)· 三则 ADR 定稿 ✅(#169/#170)· improvement 订正 ✅(#172)· critic 第 2 轮收口 ✅(#174)· 4 件套已建 · **待**:转正登记 + 冻结面实测 + KPI 回填 |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-03) | `nvlink_sharp_state_spec.hh`(新 · header-only · **自足**)—— 类型 + 纯函数 + 常量表 + **24 条编译期断言**。⚠️ **2026-11-03 订正**(Entry #182 · `tasks 2.8a`):原写 19 条 ⇒ Phase 1 实测 **24**;Phase 2 后为 **46 条**(全部带归因 id);⛔ 无 opcode / 无区域表 / 无词表型枚举 |
| 2 逐格 basis + 编译期锁 | ✅ **已完成**(2026-11-03) | 12 格逐格 `basis`(两轴)+ 来源许可表(真源在锚点摘要)+ 归因 id 落进断言文案 + 9 项 pending 登记 + **非 pending 格数下限** + 3 处 design 口径订正 + 3 处计数/范围漂移订正 |
| 3 判据收口 | ✅ **已完成**(2026-11-03) | 逐格核对表(含 **heading-aware** 条目收集器,防空转)+ **9 组 / 10 条反向 fixture** + 基线正控 + probe/脚本 + ctest 注册 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + `openspec validate --strict` + KPI 1/3 → 0/3 + 归档 |

---

## Phase 0 — 治理前置

- [x] 0.1 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(Entry #173)
      ⇒ **通过**(3 个公开来源 sha256 钉版 · 5 条锚点 `A-1`~`A-5` · **9 项** `pending` 白名单 **`N-1`~`N-9`**) ⚠️ **2026-11-03 订正**(Entry #182 · `tasks 2.8b`):原写 7 项 ⇒ 与本行的「9 项」自相矛盾
- [x] 0.2 **architect 裁决**:三则 ADR 定稿(Entry #169 / #170)· **VC 空间 = Decision D**(Entry #171)
      ⇒ ✅ **载体形态决定项已全部裁完**(①/⑤ = Entry #176 · ② = 用户裁决 · ③④ = Entry #177)⇒ Phase 1 无未决前置
- [x] 0.3 **improvement 草稿**落 `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` ⇒ `pre_create_brainstorm_check.sh` **rc=0**
      ⇒ **已两次订正**(Entry #172 前置陈述 · Entry #174 的 `In Scope` 与 Decision D 对齐)
- [x] 0.4 **4 件套建档**(proposal + design + delta spec + tasks)
- [x] 0.5 **转正裁决**(per `obj §4.7.2` step 1:DRI 裁决「① 建 change」)⇒ 台账 Entry 登记 ⇒ ✅ 已落(**Entry #175**,2026-11-03)
- [x] 0.6 **冻结面实测确认**:新增载体 `include/tlm/cep/nvlink_sharp_state_spec.hh` 不在 M1 冻结集内 ⇒ **0 件** ⇒ 无需两步授权 ⇒ ✅ 已实测(M1 清单只含 `docs/**` + 台账)⇒ **0 件**
- [x] 0.7 **KPI baseline 回填**:7 处(`obj §5.2` / `§5.3` / `§10` / `phase-10.md` / `roadmap.md` / `AGENTS.md` / umbrella improvement) ⇒ ✅ 已落(**Entry #175**:0/3 → 1/3)

## Phase 1 — 载体形态定稿

- [x] 1.1 **分类判据(R1/R2)**:类型 + 纯函数 `classify_at_fabric_ingress`;源端侧类型**不含**区域表字段 ⇒ ✅ 已落:`IngressDecision{DiverT,Pass}` + `classify_at_fabric_ingress(region_hit, epoch_committed)` —— ⚠️ 签名里**刻意没有** opcode / 区域表 / group 上下文 ⇒ **R1( opcode 非分类器)与 R4(源端无区域表可见性)由**类型**保证** |
- [x] 1.2 **区域/epoch 纪律(R3/R4)**:注册三步原子 · **epoch 未 commit ⇒ 放行** · 注销失效化 ⇒ ✅ 已落:`RegisterResult{Ok,DrainFailed}` + `register_region(drain_succeeded)` + `EpochState{Uncommitted,Committed}` + `epoch_routes_to_collective` + `decision_after_destroy()`(⚠️ Phase 2 已**改名并参数化**为 `classify_after_destroy(bool)`,per `tasks 2.7a`);⚠️ **状态容器不在载体**(区域表归实现层)⇒ 载体只给**纯函数判定**(per SU(#3) K.2) |
- [x] 1.3 **可观测性(R5)**:divert 计数器 + fail-stop 断言 ⇒ ✅ 已落:`DivertCounter`(仅**形状**,状态归实现层)+ `is_fail_stop(region_hit, epoch_committed, coherence_atomic)` |
- [x] 1.4 **ordering A/B/C/D**:四类**可判定**谓词(非散文) ⇒ ✅ 已落:`OrderingRule{A,B,C,D}` + `ordering_a_allows_reduce` / `ordering_b_allows_atomic` / `ordering_c_orders_reduce` / `ordering_d_allows_reduce`;⚠️ **code 相等 ⇒ 顺序未定义** ⇒ 显式登记 `kReduceCodeTieUnspecified` |
- [x] 1.5 **VC 预留单一状态**:载体**仅**表达 `Unavailable` **单一取值**(类型上**无** `vc_id`、**无** `reason` / `owner` / `revisit_trigger`)⇒ **四字段权威恒在** `collective_planes.json` + `CP-03` ⇒ ✅ 已落:`VcReservationState{Unavailable}`(枚举**仅 1 个取值** ⇒ `vc_id` 与四字段**不可表达**) |
- [x] 1.6 **能力域**:`IntraNode` 唯一取值(依据 `A-1` / `A-5`) ⇒ ✅ 已落:`CollectiveScope{IntraNode}`(枚举**仅 1 个取值** ⇒ 跨 fabric 值**不可表达**) |
- [x] 1.8 **Phase 1 的最小测试覆盖**(提前于 Phase 3 的判据收口,per 用户裁决 2026-11-03):`test/test_cep_nvlink_sharp_state_spec.cc` —— **8 cases / 57 assertions**;覆盖 = 载体**红线**(只 include 标准库 · 无词表型枚举 · 无区域表类型)· 分类判据 · **R4 回归锁** · R3/R4/R5 · ordering A/B/C/D · VC 单一状态 · 能力域节点内;⚠️ **只提供正控** + 编译期锁的**运行期见证**;⛔ **不含反向 fixture**(Phase 3 的 tasks 3.2);⚠️ **不新建归因 id**(per tasks 3.6 ②)
- [x] 1.7 ✅ **2 项待裁决定项已裁**(2026-11-03,Entry #176 + `ADR-CEP-03` SU 2026-11-03):
      ① `NvlinkAdapter` = **独立头** `nvlink_adapter.hh` + **委托** state spec(先例 `cxl_dcoh_adapter`),
      **前提**:载体**自足**(只 include 标准库)且**不暴露** `collective_region_table` 类型 ⇒ 区域表运行时状态归实现层;
      ⑤ **reduce 族 = 复用既有 `ReduceOp` 恰 6 值**(不新增枚举)· **epoch 平台侧 = R3/R4**(锁)/ 协议侧 = `N-3` **pending** ·
      **平台原语集维持恰 4** ⇒ 载体 SHALL **NOT** 另立算子族、SHALL **NOT** 断言逐算子支持

## Phase 2 — 逐格 basis + 编译期锁

> **口径裁决(2026-11-03,用户同意 · Entry #182)** —— 三条不裁则 Phase 2 方向必错:
>
> | # | 裁决 | 内容 |
> |---|---|---|
> | **D-1** | **「格」= 12 个枚举值** | `IngressDecision`2 + `RegisterResult`2 + `EpochState`2 + `OrderingRule`4 + `VcReservationState`1 + `CollectiveScope`1(照队列 #12 的 28 格先例);形状型(`GroupId` / `AddressRange` / `DivertCounter`)**不占格** |
> | **D-2** | **self-derived 过许可门** | 新增 `SourceId::AdrContract`(本仓 ADR 契约,许可**自持** = `true`)⇒ `basis_is_valid` 规则**不变**(`ref 非空 && (pending \|\| licenseConfirmed[source])`);⛔ **不**改规则给 self-derived 开豁免 |
> | **D-3** | **`ref` 解析空间** | `A-n`(公开锚点)/ `N-n`(pending 白名单)/ **`C-n`(契约锚点,新增)**;`C-n` 登记在锚点摘要新增的 `§2bis`,逐条指向 `ADR-CEP-0x §y.z Rz` |
>
> ⚠️ **`CollectiveScope` 格判 `self-derived`**:能力域收窄是 ADR 决策(`ADR-CEP-01` SU(#2) G.1 + `ADR-CEP-03 §1`),
> `A-1` / `A-5` 只作**佐证指针** ⇒ 否则该格 `pending=true`,与 `NSS-08` 的**硬锁**直接冲突(两轴规则: `pending=true` ⇒ 不得断言该值)。

- [x] 2.0 **锚点摘要补两张表**(本 Phase 的**硬前置**;`docs/research/` **不在** M1 冻结集 ⇒ 无需两步授权)
      - [x] 2.0a §1 钉版清单加 **`pin` 列** + **引用 / 再分发许可已人工确认** 列
            ⇒ 三个来源均有 `sha256` ⇒ 全 `pin=strong`;许可**保守**取: S1(arXiv 预印本)待确认 · S2(NVIDIA blog ©) · S3(NVIDIA docs ©) ⇒ `false`
      - [x] 2.0b 新增 **`§2bis 契约锚点`**:`C-1` ~ `C-12` 逐条 = 一个 self-derived 格 ↔ 其 ADR 条款(`ADR-CEP-0x §y.z Rz`)
      - [x] 2.0c §6 复核段补一句:`C-n` / `N-n` 的**变更**同样使本摘要在对应格上失效
      ⇒ **判据**:每来源 `pin` + 许可两列齐备;`C-n` 12 条无空洞且每条有 ADR 条款定位

- [x] 2.1 **逐格 `basis`**(两轴):`kind` ∈ `public-direct` / `public-analogical` / `self-derived` · `pending` ∈ `true` / `false`
      - [x] 2.1a 载体加元数据: `#include <array>` + `SourceId{S1,S2,S3,AdrContract,Derived}` + `kSourceCount`
            + `kSourceLicenseConfirmed` + `EvidenceKind{PublicDirect,PublicAnalogical,SelfDerived}`
            + `struct Basis{kind, pending, source, ref}` + `basis_is_valid` + `all_basis_valid<N>`
            ⇒ ⚠️ `<array>` 是**标准库** ⇒ **不破**自足红线(`ADR-CEP-03` SU(#2) F.1-a)
      - [x] 2.1b **12 格逐格表**(6 组):`kBasisIngressDecision[2]` · `kBasisRegisterResult[2]` · `kBasisEpochState[2]` ·
            `kBasisOrderingRule[4]` · `kBasisVcReservationState[1]` · `kBasisCollectiveScope[1]`
      - [x] 2.1c **6 条编译期锁**:`all_basis_valid(kBasisXxx)` + `kBasisXxx.size() == kXxxCount`(逐组,不留空洞)
      ⇒ **判据**:12 格三字段齐全;`pending == false` ⇒ 来源许可已确认(D-2 的 `AdrContract` = `true`)

- [x] 2.2 **`ref` 纪律**:指向锚点摘要条目号(`A-n` / `N-n` / `C-n`)⇒ **SHALL NOT** 指向规范原文
      - [x] 2.2a 逐格 `ref` **不得** `nullptr`、**不得**空串(`basis_is_valid` 覆盖)
      - [x] 2.2b 可解析性核对归 **Phase 3**(3.1),本条只锁**非空**
      ⇒ **判据**:载体零处引用 NVLink 规范正文 / NDA 材料

- [x] 2.3 **来源许可表**:逐来源记录 `pin=strong|weak` 与引用 / 再分发许可的人工确认
      ⇒ **真源在锚点摘要(2.0a)**,载体只**镜像布尔**(`kSourceLicenseConfirmed`)
      - [x] 2.3a `AdrContract` 的许可 = **`true`**(本仓自著 ADR ⇒ 无第三方许可问题)
      - [x] 2.3b `Derived`(派生 / 二手推论)的许可 = **`false`**(照队列 #12 先例)
      ⇒ **判据**:载体布尔与锚点摘要两列**逐来源一致**

- [x] 2.4 **编译期锁**:NSS-01 ~ NSS-09 的 `static_assert`(**每条**另配运行期判据 —— 铁律 ③)
      - [x] 2.4a ⚠️ **归因 id 落进断言文案**(实测当前 24 条**零** id ⇒ Phase 3 的「归因命中」无法成立)
            ⇒ 文案形态 `<唯一归因 id>: <语义>`(id 集**唯一登记处** = 本条 + `tasks 3.2` + `design §3`)
      - [x] 2.4b **新增 4 条缺的锁**:
            ① `classify_at_fabric_ingress` 的**签名锁**
               (`is_same_v<decltype(&classify_at_fabric_ingress), IngressDecision (*)(bool, bool) noexcept>` ⇒ 锁死「无 opcode 参数」)
            ② `NSS-05` 计数器**形状锁** ③ `all_basis_valid` 系列 ④ `all_pending_items_wellformed`
      - [x] 2.4c 逐条对照 `design §3` 的「编译期锁」列 ⇒ 不得缺项(口径冲突见 2.7)
      ⇒ **判据**:去注释后 `static_assert(` 计数 == 「带归因 id 的断言数」(零例外)

- [x] 2.5 **9 项 `pending` 登记**:`N-1` ~ `N-9` 逐条可寻址,且载体**不得**出现其具体取值的断言
      - [x] 2.5a `struct PendingSemanticItem{id, what, ref}` + `kPendingSemanticItemCount = 9` + `kPendingSemanticItems`
      - [x] 2.5b `all_pending_items_wellformed`:id / what / ref 非空 + id 唯一 + 形态 `N-<数字>`
      - [x] 2.5c 「不得出现具体取值」的可判定面:载体零处出现 reduce 算子名 / VC 编号 / 报文格式常量 / 聚合树算法名
      ⇒ **判据**:9 项逐条可寻址 + 反向 fixture(`NSS-09-BASIS-MISSING`)必红

- [x] 2.6 ⚠️ **非 pending 格数下限**:防止「规约层自身成为纸面契约」(design §5.3 风险 5)
      ⇒ 统计 `pending == false` 的格数,**下限**由 Phase 1 的实际产出确定并登记 ⇒ 低于下限即判红
      - [x] 2.6a `kNonPendingCellCount`(constexpr 遍历 12 格)+ `kNonPendingFloor` + `static_assert(count >= floor)`
      - [x] 2.6b **实测登记**:`kNonPendingCellCount` = **12** · `kNonPendingFloor` = **12**(12 格全部出自已签发 ADR ⇒ D-2);任一格改 `pending=true` ⇒ 低于下限 ⇒ **编译红**
      ⇒ **判据**:手改任一格为 `pending=true` ⇒ 计数下降 ⇒ 低于下限即**编译红**

- [x] 2.7 **`design §3` 的口径漂移订正**(使 design ↔ 载体逐条一致;实测 **3 处**)
      - [x] 2.7a `NSS-04`:「`destroy` 后 `lookup_region` 返回空」⇒ 改为「`classify_after_destroy(region_still_registered)` 在**注销成功**时返 `Pass`」
            (`ADR-CEP-03` SU(#3) K.2 的纯函数形态)+ 载体把**恒真**的 `decision_after_destroy()` **参数化**(铁律①)
      - [x] 2.7b `NSS-05`:「`DivertCounters` 字段集合**恰 2**」与载体 `DivertCounter{total}`(**1 字段**)冲突 ⇒ 二选一并登记:
            **(i)** 载体扩为可寻址键形状(`GroupId` + `AddressRange` 键 + `total`)· **(ii)** design 改为「1 字段 + 可寻址性归实现层」
            ⚠️ 须同时对齐 delta spec NSS-05「按 group / 区域可寻址」
      - [x] 2.7c `NSS-07`:「载体解析结果恒为 `Unavailable`」⇒ 改为「`VcReservationState` **枚举仅 1 取值** 且其类型**无** `vc_id` / 四字段」
      ⇒ **判据**:`design §3` 九行逐行对照载体,**零**口径冲突

- [x] 2.8 **计数 / 范围漂移订正**(实测 **3 处**)
      - [x] 2.8a **断言条数失实**(原值偏小)⇒ 订正为 Phase 1 实测 **24**、Phase 2 后 **46**(`Entry #180` + `tasks 1.1`;台账 append-only ⇒ 由新 Entry 订正)
      - [x] 2.8b **pending 白名单的上界写成 7 项**(而实际 9 项)⇒ 订正为 `N-1` ~ `N-9`(`tasks 0.1` + `design` 头注 + `design §4` 表行)
      - [x] 2.8c `design §1.2` 第 2 项「多 group 并存(`A-4`)」与 `A-2` 的 flow_support 粒度在载体**无表达**
            ⇒ 显式登记归属(载体格 or 实现层),**不得静默缺失**
      ⇒ **判据**:change 4 件套内**零**命中「断言条数为 19」与「pending 白名单上界为 7」两种旧表述

- [x] 2.9 Phase 2 验收:门禁 **7** 脚本 rc=0 ✅ + `-Wall -Wextra` **零警告** ✅ + `format.sh --check` rc=0 ✅ + `ctest` **142/142** ✅ + 台账 **Entry #183**

## Phase 3 — 判据收口

- [x] 3.1 **逐格核对表**:`basis` 三字段齐全 + `ref` 可解析
      - [x] 3.1a 运行期复核 **12 格**:三字段齐全 + `basis_is_valid` + `pending == false ⇒ license`
      - [x] 3.1b ⚠️ **新增 heading-aware 的锚点条目收集器**:`A-n` 在 `###` **标题**里、`N-n` / `C-n` 在**表格单元格**里
            ⇒ 照抄 `test/cep_anchor_ids.hh` 的 `collect_ids`(**只扫表格**)会得到**空集** ⇒ 逐格核对**恒真空转**
            (队列 #11 Entry #141 B-7 的**同型陷阱**)
      - [x] 3.1c 断言收集结果**非空**(`A-n` 恰 **5** 条、`C-n` 恰 **9** 条)—— ⚠️ **防空转**;并登记 **`A-2` / `A-4` 无载体格**(见锚点摘要 §2bis.2 口径 3)
      ⇒ **判据**:12 格 `ref` 与锚点摘要条目**一一可解析**;任一侧手改 ⇒ **必红**

- [x] 3.2 **9 组反向 fixture**(每组**必红** + **归因 id** 命中)—— 归因 id 的**唯一登记处 = 本条 + `design §3`**:
      **既有 id(复用,5 条)**:`CP-OPCODE-CLASSIFIER` + `CP-05`(NSS-01)· `CP-EPOCH-SEMANTICS`(NSS-02)·
      `CP-DRAIN-FAIL`(NSS-03)· `CP-DIVERT-STOP`(NSS-05)· `CP-03`(NSS-07);
      **新建 id(本 change 单一登记,4 条)**:`NSS-04-DESTROY-LEAK` · `NSS-06-ORDERING-MIS` ·
      `NSS-08-SCOPE-OVERREACH` · `NSS-09-BASIS-MISSING`
      ⇒ **9 组 / 10 条 fixture**(NSS-01 含 2 条,因其义务有 **2** 个归因 id):
      - [x] 3.2a NSS-01a `CP-OPCODE-CLASSIFIER`:毒化 = 给 `classify_at_fabric_ingress` 加 `std::uint32_t opcode` 参数 ⇒ **签名锁**编译红
      - [x] 3.2b NSS-01b `CP-05`:毒化 = 载体加 `#include "tlm/cep/collective_plane.hh"` 或加区域表成员 ⇒ **运行期源扫描**红
      - [x] 3.2c NSS-02 `CP-EPOCH-SEMANTICS`:毒化 = `epoch_routes_to_collective(Uncommitted)` 返 `true` ⇒ 编译红
      - [x] 3.2d NSS-03 `CP-DRAIN-FAIL`:毒化 = `register_region` 恒返 `Ok` ⇒ 编译红
      - [x] 3.2e NSS-04 `NSS-04-DESTROY-LEAK`:毒化 = `classify_after_destroy` 恒返 `Pass`(抹掉泄露可观测性)⇒ 编译红
      - [x] 3.2f NSS-05 `CP-DIVERT-STOP`:毒化 = `is_fail_stop` 恒返 `false` ⇒ 编译红
      - [x] 3.2g NSS-06 `NSS-06-ORDERING-MIS`:毒化 = `ordering_c_orders_reduce` 反转比较 ⇒ 编译红
      - [x] 3.2h NSS-07 `CP-03`:毒化 = **填入具体 VC 编号**(给载体加 `std::uint8_t vc_id` 成员) ⇒ **运行期源扫描红**。 ⚠️ **2026-11-03 订正**(Entry #184):原计划「加第 2 个枚举值」—— 实测**不会**触发计数断言(`kVcReservationStateCount` 是**手写常量**,与枚举手工同步;C++17 无法反射枚举基数) ⇒ 改采 delta spec 明写的形态(「填入具体 VC 编号 ⇒ 必红」);**结构性**保证在「类型无 `vc_id` 字段」这一面(SFINAE + 源扫描),已在载体 §⑪ 前**如实登记**界限
      - [x] 3.2i NSS-08 `NSS-08-SCOPE-OVERREACH`:毒化 = `CollectiveScope` 加 `CrossFabric` ⇒ 计数锁编译红
      - [x] 3.2j NSS-09 `NSS-09-BASIS-MISSING`:毒化 = 某格 `ref` 置 `nullptr` / `pending` 与许可错配 ⇒ `all_basis_valid` 编译红

- [x] 3.3 **正控**:每条反向 fixture SHALL 有**期望通过**的对照 ⇒ 纯负向断言无对照 = 判据可能没生效
      - [x] 3.3a 脚本**基线正控**:干净载体必须编译通过且探针全绿(否则探针本身有问题 ⇒ 直接 `exit 1`)
      - [x] 3.3b 毒化锚点**未恰好命中 1 次** ⇒ 判 **fixture 失效**(⛔ 不得当作「红」放行)

- [x] 3.4 **「不可表达」型判据**(NSS-07 / NSS-08)的**运行期兜底** ⇒ 不得只以「编译红」充当 ctest 判据
      - [x] 3.4a **SFINAE** 判据从 Phase 1 测试**镜像到** probe(`HasVcId` / `HasReason` / `HasOwner` / `HasRevisitTrigger`)
      - [x] 3.4b 跨 fabric 取值**不可表达**:枚举基数 + 载体源扫描(零 `CrossFabric` / `InterNode` / `GlobalScope` token)

- [x] 3.5 **归因纪律**:反向 fixture SHALL 报出**可归因 id** ⇒ **不得**只以「整体 FAIL」充当判据
      - [x] 3.5a **编译期红**:grep 编译日志的 `<id>:` 前缀命中期望 id
      - [x] 3.5b **运行期红**:probe 输出 `PROBE:FAIL <id>` 逐条匹配

- [x] 3.6 ⚠️ **禁止同一义务出现两个 id**(有范围的禁令,per critic B2 裁决 —— 修复于 Entry #179):
      ① **已给出 id** 的义务 SHALL **复用**该 id,SHALL **NOT** 另起别名;
      ② **新增**义务的 id SHALL 在**本 change 的 `design §3` + `tasks 3.2`** **单一登记**,SHALL **NOT** 在 ADR 与本 change **重复登记**;
      ③ 判据实现与反向 fixture 的**归因** SHALL 一律用上述 id(`NSS-xx` 仅作 Scenario 标题 / 离线标签);
      ④ ⚠️ **既有 id 的来源不限于机器闸的 `CP-01`~`CP-06`** —— ADR 已印出的 `CP-OPCODE-CLASSIFIER` / `CP-EPOCH-SEMANTICS` / `CP-DRAIN-FAIL` / `CP-DIVERT-STOP` / `CP-03-VC-PRECONDITION` 等**同样是既有 id**
      ⇒ **禁止**把「引用机器闸」写成唯一合法来源(否则这 5 条一并违规)
      ⚠️ **实测补注(Entry #182)**:上述 5 条「既有 id」在**代码域零机器落点**(只在 ADR / 台账)
      ⇒ 本 change 的 fixture 是它们的**首个机器落点**;**不得**误解为「扩展已有实现」

- [x] 3.7 **探针 + 驱动脚本**(照队列 #12 `cep_cxl_dcoh_state_spec_{probe.cc,reverse_verify.sh}` 同构)
      - [x] 3.7a `scripts/test/cep_nvlink_sharp_state_spec_probe.cc`(运行期判据 + 源扫描 + `PROBE:FAIL <id>`)
      - [x] 3.7b `scripts/test/cep_nvlink_sharp_state_spec_reverse_verify.sh`
            (影子目录毒化 ⇒ `-I $SHADOW/include` **优先**;仓内文件**零改动**)
      - [x] 3.7c **两类失败形态都支持**:编译期红(`static_assert` 文案)+ 运行期红(`PROBE:FAIL`)

- [x] 3.8 **ctest 注册 + 文档同步**
      - [x] 3.8a `test/CMakeLists.txt` 加 `cep_nvlink_sharp_state_spec_reverse_verify`(照 `cep_cxl_dcoh_state_spec_reverse_verify` 先例)
      - [x] 3.8b `scripts/README.md` 补 3 行(本脚本 + **队列 #12 / #11 的遗漏项** —— 实测未登记)
      - [x] 3.8c `docs/research/AGENTS.md` 索引补 **2 条**(另订正索引计数:逐节合计 + 磁盘实际 = **14**)(#12 CXL-DCOH / #13 NVLink 锚点摘要 —— 实测声明 13 份实际 14 份)

- [x] 3.9 Phase 3 验收:门禁 **7** 脚本 rc=0 ✅ + `ctest` **143/143**(含新注册)✅ + `format.sh --check` rc=0 ✅ + 台账 **Entry #184**

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁 7 脚本全 rc=0 + `format.sh --check` rc=0
- [ ] 4.2 `ctest` 全绿(新增 `[cep][nvlink-sharp][state-spec]` 标签;⚠️ 标签**沿用**队列 #11/#12 的`[cep][<queue>][<face>]` 三段式惯例,非本仓先例的 `[cep][nvlink-sharp-spec]`)
- [ ] 4.3 `openspec validate cpptlm-cep-nvlink-sharp-state-spec --strict` rc=0
- [ ] 4.4 ⚠️ **纸面契约复核**:确认实现层 change(`cpptlm-cep-nvlink-sharp`)已在建或已排期 ⇒ 否则本层**不得**归档
- [ ] 4.5 KPI **1/3 → 0/3** 回填(7 处同 0.7)
- [ ] 4.6 归档:delta spec 合并进 `openspec/specs/cep-nvlink-sharp-state-spec/` + 台账 Entry

---

## ⚠️ 归档前必须闭合的**登记债**(per 队列 #12 先例)

| 项 | 位置 | 处置 |
|---|---|---|
| 主 spec「声明最小集 **SHALL 恰含三组**」未含第四键 `vc_reservation_declaration` | `openspec/specs/cep-collective-plane-contract/spec.md` | ⚠️ 已登记 obj §10 backlog(Entry #174 ③ S1)⇒ **本 change 不承担**;但**实现层**开工前须处理 |
| `ADR-CEP-01` 附录 `V6-a` 行「以 SU D.4 为准」 | `docs/adr/ADR-CEP-01-fabric-collective-plane.md` | ✅ 已闭合(Entry #174 ③ S2) |
