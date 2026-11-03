# Tasks — cpptlm-cep-nvlink-sharp-state-spec v1.0(Stage 2 · 第 0 个 change · 规约层 · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-nvlink-sharp-state-spec/spec.md`](specs/cep-nvlink-sharp-state-spec/spec.md)
> **口径**: 台账 **Entry #174**(critic 第 2 轮收口)· **#173**(Task 0 普查)· **#172**(improvement 订正)· **#171**(Decision D)· **#169 / #170**(三则 ADR 定稿)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`
> **估时**: 5 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 本 change = **规约层**;第 1 个 change = `cpptlm-cep-nvlink-sharp`(实现层)

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | 🟡 **进行中**(0.1~0.4 ✅ / 0.5~0.7 待本轮闭环) | Task 0 普查 ✅(Entry #173)· Decision D ✅(#171)· 三则 ADR 定稿 ✅(#169/#170)· improvement 订正 ✅(#172)· critic 第 2 轮收口 ✅(#174)· 4 件套已建 · **待**:转正登记 + 冻结面实测 + KPI 回填 |
| 1 载体形态定稿 | ⬜ 待实施 | `nvlink_sharp_state_spec.hh`(新)—— 类型 + 纯函数 + 常量表 |
| 2 逐格 basis + 编译期锁 | ⬜ 待实施 | 逐格 `basis`(两轴)+ 来源许可表 + 编译期锁 + **运行期兜底判据** |
| 3 判据收口 | ⬜ 待实施 | 逐格核对表 + **9 组反向 fixture**(各带可归因 id)+ **9** 项 `pending` 登记 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + `openspec validate --strict` + KPI 1/3 → 0/3 + 归档 |

---

## Phase 0 — 治理前置

- [x] 0.1 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(Entry #173)
      ⇒ **通过**(3 个公开来源 sha256 钉版 · 5 条锚点 `A-1`~`A-5` · **9 项** `pending` 白名单 `N-1`~`N-7`)
- [x] 0.2 **architect 裁决**:三则 ADR 定稿(Entry #169 / #170)· **VC 空间 = Decision D**(Entry #171)
      ⇒ ✅ **载体形态决定项已全部裁完**(①/⑤ = Entry #176 · ② = 用户裁决 · ③④ = Entry #177)⇒ Phase 1 无未决前置
- [x] 0.3 **improvement 草稿**落 `.rddf/improvements/cpptlm-cep-nvlink-sharp.md` ⇒ `pre_create_brainstorm_check.sh` **rc=0**
      ⇒ **已两次订正**(Entry #172 前置陈述 · Entry #174 的 `In Scope` 与 Decision D 对齐)
- [x] 0.4 **4 件套建档**(proposal + design + delta spec + tasks)
- [ ] 0.5 **转正裁决**(per `obj §4.7.2` step 1:DRI 裁决「① 建 change」)⇒ 台账 Entry 登记
- [ ] 0.6 **冻结面实测确认**:新增载体 `include/tlm/cep/nvlink_sharp_state_spec.hh` 不在 M1 冻结集内 ⇒ **0 件** ⇒ 无需两步授权
- [ ] 0.7 **KPI baseline 回填**:7 处(`obj §5.2` / `§5.3` / `§10` / `phase-10.md` / `roadmap.md` / `AGENTS.md` / umbrella improvement)

## Phase 1 — 载体形态定稿

- [ ] 1.1 **分类判据(R1/R2)**:类型 + 纯函数 `classify_at_fabric_ingress`;源端侧类型**不含**区域表字段
- [ ] 1.2 **区域/epoch 纪律(R3/R4)**:注册三步原子 · **epoch 未 commit ⇒ 放行** · 注销失效化
- [ ] 1.3 **可观测性(R5)**:divert 计数器 + fail-stop 断言
- [ ] 1.4 **ordering A/B/C/D**:四类**可判定**谓词(非散文)
- [ ] 1.5 **VC 预留单一状态**:载体**仅**表达 `Unavailable` **单一取值**(类型上**无** `vc_id`、**无** `reason` / `owner` / `revisit_trigger`)⇒ **四字段权威恒在** `collective_planes.json` + `CP-03`
- [ ] 1.6 **能力域**:`IntraNode` 唯一取值(依据 `A-1` / `A-5`)
- [x] 1.7 ✅ **2 项待裁决定项已裁**(2026-11-03,Entry #176 + `ADR-CEP-03` SU 2026-11-03):
      ① `NvlinkAdapter` = **独立头** `nvlink_adapter.hh` + **委托** state spec(先例 `cxl_dcoh_adapter`),
      **前提**:载体**自足**(只 include 标准库)且**不暴露** `collective_region_table` 类型 ⇒ 区域表运行时状态归实现层;
      ⑤ **reduce 族 = 复用既有 `ReduceOp` 恰 6 值**(不新增枚举)· **epoch 平台侧 = R3/R4**(锁)/ 协议侧 = `N-3` **pending** ·
      **平台原语集维持恰 4** ⇒ 载体 SHALL **NOT** 另立算子族、SHALL **NOT** 断言逐算子支持

## Phase 2 — 逐格 basis + 编译期锁

- [ ] 2.1 **逐格 `basis`**(两轴):`kind` ∈ `public-direct` / `public-analogical` / `self-derived` · `pending` ∈ `true` / `false`
- [ ] 2.2 **`ref` 纪律**:指向锚点摘要条目号(`A-n` / `N-n`)⇒ **SHALL NOT** 指向规范原文
- [ ] 2.3 **来源许可表**:逐来源记录 `pin=strong|weak` 与引用 / 再分发许可的人工确认
- [ ] 2.4 **编译期锁**:NSS-01 ~ NSS-09 的 `static_assert`(**每条**另配运行期判据 —— 铁律 ③)
- [ ] 2.5 **9 项 `pending` 登记**:`N-1` ~ `N-9` 逐条可寻址,且载体**不得**出现其具体取值的断言
- [ ] 2.6 ⚠️ **非 pending 格数下限**:防止「规约层自身成为纸面契约」(design §5.3 风险 5)
      ⇒ 统计 `pending == false` 的格数,**下限**由 Phase 1 的实际产出确定并登记 ⇒ 低于下限即判红

## Phase 3 — 判据收口

- [ ] 3.1 **逐格核对表**:`basis` 三字段齐全 + `ref` 可解析
- [ ] 3.2 **9 组反向 fixture**(每组**必红** + **归因 id** 命中)—— 归因 id 的**唯一登记处 = 本条 + `design §3`**:
      **既有 id(复用,5 条)**:`CP-OPCODE-CLASSIFIER` + `CP-05`(NSS-01)· `CP-EPOCH-SEMANTICS`(NSS-02)·
      `CP-DRAIN-FAIL`(NSS-03)· `CP-DIVERT-STOP`(NSS-05)· `CP-03`(NSS-07);
      **新建 id(本 change 单一登记,4 条)**:`NSS-04-DESTROY-LEAK` · `NSS-06-ORDERING-MIS` ·
      `NSS-08-SCOPE-OVERREACH` · `NSS-09-BASIS-MISSING`
- [ ] 3.3 **正控**:每条反向 fixture SHALL 有**期望通过**的对照 ⇒ 纯负向断言无对照 = 判据可能没生效
- [ ] 3.4 **「不可表达」型判据**(NSS-07 / NSS-08)的**运行期兜底** ⇒ 不得只以「编译红」充当 ctest 判据
- [ ] 3.5 **归因纪律**:反向 fixture SHALL 报出**可归因 id** ⇒ **不得**只以「整体 FAIL」充当判据
- [ ] 3.6 ⚠️ **禁止同一义务出现两个 id**(有范围的禁令,per critic B2 裁决 —— 修复于 Entry #179):
      ① **已给出 id** 的义务 SHALL **复用**该 id,SHALL **NOT** 另起别名;
      ② **新增**义务的 id SHALL 在**本 change 的 `design §3` + `tasks 3.2`** **单一登记**,SHALL **NOT** 在 ADR 与本 change **重复登记**;
      ③ 判据实现与反向 fixture 的**归因** SHALL 一律用上述 id(`NSS-xx` 仅作 Scenario 标题 / 离线标签);
      ④ ⚠️ **既有 id 的来源不限于机器闸的 `CP-01`~`CP-06`** —— ADR 已印出的 `CP-OPCODE-CLASSIFIER` / `CP-EPOCH-SEMANTICS` / `CP-DRAIN-FAIL` / `CP-DIVERT-STOP` / `CP-03-VC-PRECONDITION` 等**同样是既有 id**
      ⇒ **禁止**把「引用机器闸」写成唯一合法来源(否则这 5 条一并违规)

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁 7 脚本全 rc=0 + `format.sh --check` rc=0
- [ ] 4.2 `ctest` 全绿(含新增 `[cep][nvlink-sharp-spec]` 标签)
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
