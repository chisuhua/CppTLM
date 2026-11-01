## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ⬜ 待实施 | change 4 件套(本文件)+ KPI slot + **7 处** baseline 同步 |
| 1 `SparseSnoopFilter` 行为实现 | ✅ **已完成**(2026-11-01) | `sharer_tracker.hh` 追加;CONSERVATIVE 语义 + 5 cases / 39 assertions;**2 条毒化实测鉴别力**(无条目→`ANSWER_HERE` ⇒ 3 failed;漏查 sharers 位数 ⇒ 2 failed)|
| 2 `CxlCacheAdapter` | ✅ **已完成**(2026-11-01) | `cxl_cache_adapter.hh`(新);委托规约载体 + 三态查询面 + 零副本扫描器;**3 cases / 610 assertions**;**2 条毒化实测鉴别力**(自带表 ⇒ 1 failed;写死 `State::` 字面量 ⇒ 1 failed)
| 3 实例声明 + 冻结面 | ⬜ 待实施 | `protocol_instances.json` + `perm_upgrades.json` + `07 号` / `ADR-CEP-04` 两步授权 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + 零冻结面 diff + 归档 + KPI 1/3 → 0/3 |

---

## Phase 0 — 治理前置

- [ ] 0.1 change 目录 4 件套(proposal / design / tasks / 2 份 delta spec)+ `openspec validate --strict` rc=0
- [ ] 0.2 KPI slot 占用(**0/3 → 1/3**)+ **7 处**活文档 baseline 同步
      (obj §5.2 · obj §5.3 · obj §4.0 队列表行 · phase-10 · umbrella improvement · roadmap.md · AGENTS.md)
- [ ] 0.3 台账追加 Entry(append-only):change 转正登记

---

## Phase 1 — `SparseSnoopFilter` 行为实现(TDD 5-step)

- [x] 1.1 **先写红**:`test/test_cep_sparse_snoop_filter.cc` 骨架 + 反向 fixture
      (① caps/behavior 与 traits 不一致 ⇒ 必红 ② 「filter 说独占、实际另有 sharer」⇒ 必红
       ③ 无条目 ⇒ 返回 `ANSWER_HERE` ⇒ 必红 ④ contention 不报 `STALL` ⇒ 必红)
- [x] 1.2 `include/tlm/cep/sharer_tracker.hh` 追加 `SparseSnoopFilter`
      (`final : public SharerTrackerIf, public SnoopMechanics`;caps/behavior **取** `kTrackerTraits[1]`)
- [x] 1.3 `lookup` 三档判定(D-β.2)+ 「无条目 ⇒ `BROADCAST`」保守默认
- [x] 1.4 4 个 `override` 薄壳(`snoop_begin`/`snoop_fold`/`observe_snoop` 转调 `SnoopMechanics::*_core`)
      + 观察面 `view_of`
- [x] 1.5 跑绿 + `moesi_gpu` 既有测试 **0 回归**(若 snoop 数变化 ⇒ 显式登记 + 重基线)

---

## Phase 2 — `CxlCacheAdapter`(TDD 5-step)

- [x] 2.1 **先写红**:适配测试骨架 + 反向 fixture(① 注入 `State::` 字面量 ⇒ 扫描器必红
      ② `Unknown` 被折叠成哨兵 ⇒ 必红 ③ 68 格 kind 不一致 ⇒ 必红)
- [x] 2.2 `include/tlm/cep/cxl_cache_adapter.hh`(新):类型别名 + `constexpr` 委托
      (`transition_kind` / `transition_target` / `transition_is_known` / `perm_of` / `is_silent_upgrade`)
- [x] 2.3 **零自有表副本**扫描器 + 扫描器**鉴别力**自测(注入字面量 ⇒ 必红)
- [x] 2.4 68 格逐格 pass-through + `Unknown` 抽样(22 个 `pending=false` 格全抽样)
- [x] 2.5 跑绿 + **零冻结面 diff** 复核

---

## Phase 3 — 实例声明 + 冻结面(两步授权)

- [ ] 3.1 `include/tlm/cep/protocol_instances.json` 新增 `cxl_2_0` 实例块
      (`sharer_tracker.impl = "不参与"`)+ `repo_owned_instance_ids` 追加
- [ ] 3.2 `include/tlm/cep/perm_upgrades.json` 新增 `cxl_2_0` 的 `UNIQUE → WRITEBACK` 一行
      + `pending` 注记
- [ ] 3.3 台账 Entry(授权记录)→ `--update --authorized-by "Entry #N"` → commit 产物
      → `--update`(provenance 对齐)→ commit 清单
      · `docs/architecture/07-coherence-exploration-platform.md`(§5.1.1 CXL 行措辞对齐裁决 ③⑦)
      · `docs/adr/ADR-CEP-04-*.md`(第 3 个行为实现落地 + R11 降级条件评估)
      · `openspec/specs/cep-sharer-tracker-base/spec.md`(⚠️ **第 3 件** —— 「其余 4 实现 deferred」
        已**过期**;走 delta spec `MODIFIED Requirements`;⚠️ **归档 → 重冻**顺序不可颠倒)
- [ ] 3.4 R11 降级评估:V-1 ∧ V-2 ∧ V-3 **全达成**才降 🟢;任一未达成 ⇒ **保持 🟡 中**并登记

---

## Phase 4 — 验收 + 归档

- [ ] 4.1 全量 `ctest --test-dir build -j4` 全绿(**不排除任何测试**)
- [ ] 4.2 **零冻结面 diff** 断言 + `cep_freeze_check` provenance 通过
- [ ] 4.3 `openspec validate --strict` rc=0 + `openspec archive cpptlm-cep-cxl-nonsymmetric`
- [ ] 4.4 台账追加 Entry(完成登记)+ §4.0 队列 #10 行状态更新
      ⇒ KPI 并发 **1/3 → 0/3** + **7 处**活文档 baseline 同步
- [ ] 4.5 `git commit`(**不 push**)

---

## 非目标(Non-Goals)

| # | 不做 | 理由 |
|---|---|---|
| 1 | 不写 CXL protocol engine / JSON 装配 | `moesi_gpu` 先例 = **无 engine** ⇒ adapter-only 是有效形态;引擎属后续 |
| 2 | 不给 CXL 实例挂 `SparseSnoopFilter` | peer 集平凡为空 ⇒ 挂 SF 是**过度声明**(per 裁决 ③) |
| 3 | 不扩 `perm` 4 值锁 / `CATEGORY_*` 3 类 / 不改 `CohIfBundle` / 不改冻结 schema | 平台级不变量 |
| 4 | 不新增 invariant 编号数 | — |
| 5 | 不实现 CXL 3.0 的 BI 通道 / DCOH | 属队列 **#12** |
| 6 | 不产出任何性能结论 | CXL 非对称 workload 在 **H7 声明范围之外** ⇒ R-B 失效区 |
| 7 | 不把 CXL 规范正文投喂任何 AI | ECA §3 + AI AUP |
| 8 | 不镜像实例声明进 ArchForge `32 号 §6.1` | 产品侧注册表 ⇒ 污染 + 与其立法冲突 |
| 9 | 不把 `Unknown` 格补成 `Known` | 只能靠**人类成员**的规范条款摘要定点读 §3.2.5.15 |
