# Tasks — cpptlm-cep-cxl-dcoh-symmetric v1.0(Stage 2 · 第 1 个 change · 实现层 · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#12** 第 1 个 change(实现层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-cxl-dcoh-symmetric/spec.md`](specs/cep-cxl-dcoh-symmetric/spec.md)
> **口径**: 台账 **Entry #159**(规约层归档)· **#154**(4 项 architect 裁决)· **#153**(Task 0 普查)
> **规约载体**: `include/tlm/cep/cxl_dcoh_state_spec.hh`(**已归档**,只读消费)
> **估时**: 8 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 = 规约层(**已归档**)· 第 1 个 = **本 change**

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-02) | Entry #153 / #154 / #159 前置全闭合 · 4 件套已建 · **转正裁决 Entry #160** · **冻结面实测 = 0 件**(0.6)· **KPI 回填 8 处**(0.7) |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-02) | **6 项形态裁决**落 `design.md` §1.1 ~ §1.6 + **2 组方案对比**(§2 adapter 形态 / §3 tracker 落位)⇒ 随建档一次落盘 |
| 2 adapter + tracker 行为实现 | ✅ **已完成**(2026-11-02) | ✅ `CxlDcohAdapter`(零副本 + 委托 + 无 transition API)· ✅ 零副本扫描器**正控 + 反向 fixture**· ✅ 判据 **3 cases / 26 assertions**;✅ `DcohSymmetric` **行为类**(**独立头** `cxl_dcoh_tracker.hh`,落点裁决 **A ⇒ A′** 见 design §3.1)· ✅ tracker 判据 **7 cases / 57 assertions**(每条行为断言**配正控**) |
| 3 实例声明 + 判据收口 | 🟡 **部分完成**(2026-11-02) | ✅ `cxl_3_0_symmetric`(13 键 · `perm` 4 值 · ★`back_invalidate=true`)+ `repo_owned` **+1** · ✅ `perm_upgrades.json` **零行** + 派生集断言 **5 处**同步 · ✅ `AC-05` **SKIP** + 回归锁 · ✅ 实例声明判据 **3 cases / 80 assertions**;⬜ **5 组反向 fixture**(3.4)+ ⬜ `LOCAL_HINT` 冻结面登记(3.5) |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + KPI 1/3 → 0/3 + 归档 |

---

## Phase 0 — 治理前置

- [x] 0.1 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(per Entry #153)
- [x] 0.2 **architect 4 项裁决**(per Entry #154)
- [x] 0.3 **第 0 个 change(规约层)已归档**(per Entry #159)⇒ main spec = `openspec/specs/cep-cxl-dcoh-symmetric-state-spec/spec.md`
- [x] 0.4 **4 件套建档**(proposal + design + delta spec + tasks)
- [x] 0.5 **转正裁决**(per §4.7.2 step 1)⇒ 台账 Entry 登记
- [x] 0.6 **冻结面实测确认**:`sharer_tracker.hh` / 新增 adapter **不在** M1 冻结集内 ⇒ **0 件**
- [x] 0.7 **KPI baseline 回填**:8 处

---

## Phase 1 — 载体形态定稿

- [x] 1.1 **adapter 形态**:`CxlDcohAdapter`(独立 · header-only · 不进 `ModuleFactory` · 不注册 ABI)
- [x] 1.2 **零副本禁词表**(7 个成员字面量 + 11 个表名)· **类型名**允许 ⇒ 落 `design.md` §1.1(已定稿)
- [x] 1.3 **委托契约**(基数 / `perm` / 判别式 / 通道词表 / 方向表)⇒ 落 `design.md` §1.2(已定稿)
- [x] 1.4 **不暴露 transition API** ⇒ 落 `design.md` §1.3(已定稿)
- [x] 1.5 **`DcohSymmetric` 行为边界**(6 个 caps 字段逐条)⇒ 落 `design.md` §1.4(已定稿)
- [x] 1.6 **`AC-05` 复用** + **`perm_upgrades.json` 不声明** + 实例 `perm` 4 值 + IC-2 措辞 pending ⇒ 落 `design.md` §1.5(已定稿)
- [x] 1.7 **实例 13 键**取值表 ⇒ 落 `design.md` §1.6(已定稿)
- [x] 1.8 **两个方案对比**(adapter 形态 / tracker 行为类落位)⇒ 落 `design.md` §2 / §3(已定稿)

---

## Phase 2 — adapter + tracker 行为实现

- [x] 2.1 **TDD 红**:先写 CDX-01 ~ CDX-05 的用例 ⇒ 在**未实现**载体上必红
- [x] 2.2 `CxlDcohAdapter` 实现(委托面;**零副本**)
- [x] 2.3 `DcohSymmetric` 行为类(保守型 + `STALL` + `LOCAL_HINT`)
- [x] 2.4 **零副本扫描器** + **鉴别力**自测(正控)
- [x] 2.5 **接口面扫描**:确认 adapter **无** `transition_*`

---

## Phase 3 — 实例声明 + 判据收口

- [x] 3.1 `cxl_3_0_symmetric`(13 键)+ `repo_owned_instance_ids` **+1**
- [x] 3.2 `perm_upgrades.json` **零行** + `no_upgrade_instances` 断言同步更新(⚠️ 原估「2 处」**实测 5 处**:3 处派生集断言 + 2 处实例计数断言)
- [x] 3.3 `AC-05` SKIP 断言 + **回归锁**(既有族判定逐条不变)
- [ ] 3.4 **反向 fixture**(形态 = 「毒化 ⇒ 必红」):① 植入成员字面量 ② 植入表名 ③ 暴露 `transition_*` ④ 实例 `perm` 加第 5 值 ⑤ `perm_upgrades` 加假声明行
- [ ] 3.5 **`LOCAL_HINT` 双重语义**登记 ⇒ 冻结面 `cep-sharer-tracker-base` 追加段(**两步授权**)+ 台账 §10 backlog 行

---

## Phase 4 — 验收 + 归档

- [ ] 4.1 `ctest` 全绿(含 `[cep][cxl-dcoh]`)
- [ ] 4.2 `openspec validate cpptlm-cep-cxl-dcoh-symmetric --strict` rc=0
- [ ] 4.3 门禁全 rc=0 + `format.sh --check` rc=0
- [ ] 4.4 KPI **1/3 → 0/3** + 8 处 baseline 同步
- [ ] 4.5 **完成归档** ⇒ delta capability 合并进 `openspec/specs/cep-cxl-dcoh-symmetric/spec.md`
- [ ] 4.6 **收口**:队列 #12 两段式**全部完成** ⇒ §4.0 状态 + 移交链
