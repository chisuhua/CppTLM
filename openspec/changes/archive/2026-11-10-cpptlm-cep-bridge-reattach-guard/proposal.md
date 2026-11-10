# cpptlm-cep-bridge-reattach-guard: 装配路径的桥重挂检测 ⇒ `[BSS-01]` 执行臂落地

> **一句话问题**:`[BSS-01]`（同一模块属多个 bridge entry ⇒ 模块的 `bridge_` 单槽被**后挂的桥静默覆盖**）早已存在，但**只被测试侧调用** ⇒ 「只测不拦」⇒ 装配路径**不拦** ⇒ R-3 残余。
> **性质**:补齐**已签发判据的执行臂**;⛔ **不新增任何判据语义**;带 **delta spec**(MODIFIES `cep-instance-assembly`)。

## Why

- **三处 `attach_protocol_bridge` 全是覆盖赋值**（`protocol_engine_base.hh:190` / `cohif_responder.hh:98` / `stub_cache_endpoint.hh:96`）⇒ 同一模块若出现在两条 `cep.bridges[]` entry，后挂的桥**静默覆盖**先挂的 `bridge_` ⇒ 模块**绑到了错误的桥**，而 `instantiateAll` **返回成功**。
- **契约侧判据已存在**：`instance_assembly_gate.hh:212-229` 的 `[BSS-01]`（判定面 = 模块名在**全部** entry 的 `modules` 展开序列计数 > 1，**无 entry 内去重**）+ 反向 fixture SCE-12。
- **但闸只测不拦**：`grep "instance_assembly" src/core/module_factory.cc` ⇒ **零命中** ⇒ 装配路径**不执行**该判据。
- **契约侧已把「跨 entry 模块互斥」定为硬纪律**（台账 `obj:16671`）⇒ 本 change **不新增决策**，只补执行臂。
- ⚠️ **合法配置空间收缩**：重复-entry 拓扑由「可装配 + 静默覆盖」变为「**装配失败**」⇒ 须由 delta spec 登记（critic BLOCKER B2）。⚠️ **实测收缩影响面 = 0 份现存配置**（仓内 8 份含 `cep.bridges` 的 config **全部只有 1 条 entry、零重复模块**）。

## What Changes

1. **抽共享 helper（唯一实现）** —— 新增 `include/tlm/cep/bridge_module_membership.hh`（`tlm::cep` 公开命名空间，⛔ **不留在 `detail`** —— 已升格为跨模块共享契约）：`count_bridge_module_membership` / `find_bridge_module_conflicts` / `bridge_module_conflict_message`（诊断文本与 `[BSS-01]` 现文本**逐字一致**，保留计数 `(n)`）。
2. **装配闸改调 helper** —— `instance_assembly_gate.hh` ⑥ 段改为调 helper ⇒ **行为严格等价**（⛔ 不改 `run_instance_assembly_gate` 签名 / `InstanceAssemblyReport` / verdict 枚举）。
3. **装配路径接入（结构性关口）** —— `src/core/module_factory.cc` Step 7.5：在**挂载循环之前**调同一 helper；命中 ⇒ **直接 `return false`** ⇒ 循环**不进入** ⇒ **零桥挂载**。
4. **判据** —— 新增 `test/test_cep_bridge_reattach_guard.cc`：子情形 ①（跨 entry 重叠）· 子情形 ②（单 entry 内重复）· **装配路径与装配闸结论一致**（正/负控对）。
5. **delta spec** —— MODIFIES `openspec/specs/cep-instance-assembly/spec.md`：判据集合加入 `[BSS-01]` + 归因 tag 范围改为含 `[BSS-01]` + 两子情形显式枚举 + 「装配路径 SHALL 执行同一判据」段。
6. **冻结面同步** —— `docs/architecture/07-coherence-exploration-platform.md` 追加 **SU · 十七**（append-only；⛔ 不回改 SU 十五 ②④ 原文）⇒ 闭合「只测不拦 / 登记为 backlog」的**过时陈述**（critic BLOCKER B1）。

## 验收标准(Acceptance Criteria)

- [ ] **AC-1 判据唯一实现**：`instance_assembly_gate.hh` ⑥ 段与 `module_factory.cc` 关口**共用** `tlm/cep/bridge_module_membership.hh` ⇒ ⛔ 无第二份实现（`grep` 可查）。
- [ ] **AC-2 子情形 ①**：跨 entry 重叠 ⇒ `instantiateAll == false`。
- [ ] **AC-3 子情形 ②**：单 entry 内重复（`["w","chi","w"]`）⇒ `instantiateAll == false`（⇒ 口径**未收窄**）。
- [ ] **AC-4 两侧一致**：同一 config ⇒ 装配闸判红 **且** 装配失败；正控 ⇒ 闸判绿 **且** 装配成功。
- [ ] **AC-5 决定性变异（必附）**：① helper 判定改成恒不命中 ⇒ 用例必红；② helper 加 entry 内去重（口径收窄）⇒ 子情形②必红。⛔ 只有「绿」不算闭合。
- [ ] **AC-6 SCE-12 既有正/反控照过**（helper 重构行为等价的回归证据）。
- [ ] **AC-7 delta spec**：MODIFIES `cep-instance-assembly`，含判据集合 + tag 范围 + 两子情形 + 「装配路径 SHALL 执行同一判据」段。
- [ ] **AC-8 `07 号` SU · 十七**：append-only 登记「结构性关口已落地」+ 原子性口径（**限桥层**）+ 诊断优先级 + CLI 不对称登记；⛔ 不回改既有段原文。
- [ ] **AC-9 冻结面五步链**：Entry 授权 → 产物 commit → `--update --authorized-by "Entry #319"` → **清单单独 commit** → 无参复验 rc=0。
- [ ] **AC-10 零 diff 边界**：`include/tlm/cep/**` 除本 change 涉及的 2 个头外零 diff · M1 冻结清单除授权更新外零变化 · `B3b-2` 三件零 diff · ⛔ `B3b-2` 维持 **0/4**。
- [ ] **AC-11 回归**：`ctest` 全绿 · `./test.sh --mode off` 21/21 · `cep_freeze_check` 无参 rc=0 · `cep_docs_sync_check --strict` ✅ · 35 个 `cep_*.sh` 全绿。

## Out of Scope

- ⛔ **不新增**任何判据语义（「多桥挂载不合法」早已裁决）· ⛔ **不引入**「多桥挂载优先级」语义。
- ⛔ **不动**三处 `attach_protocol_bridge` 的实现签名（覆盖赋值是**实现细节**，检测在装配层一处即可）。
- ⛔ **不把** `run_instance_assembly_gate` 拉进装配路径（IAG-02 要求 `protocol_instance` 必填 vs 装配 `:873` 可选；IAG-07 要求 `bridge_id` 必填 vs 装配 `:862`~`:869` 自动分配 ⇒ 整体接入**必误伤**合法配置）。
- ⛔ **不改** `run_instance_assembly_gate` 的公开签名 / `InstanceAssemblyReport` / verdict 枚举。
- ⛔ **不复用** `[IAG-08]`（已被 `test/test_cep_instance_assembly.cc:185` 占用）。
- ⛔ **不修**离线 CLI(`validate_topology`)不挂本判据的既有不对称（**登记**为 backlog，非本轮新增）。
- ⛔ 不改任何**已签发 ADR** 的决策正文 · ⛔ `B3b-2` 维持 0/4 · ⛔ 不宣称「全绿 run」。

## 关联

- 台账 **Entry #319**(裁决 + 冻结面授权)· **#317**(盘点 + P2 轨)· **#310**(R-3 残余登记)· **#305 ③ R-3** · `obj:16671`(跨 entry 模块互斥 = 硬纪律)
- `docs/architecture/07-coherence-exploration-platform.md` SU · 十五 ②④(**只测不拦**表述 · 由 SU · 十七 闭合)
- `openspec/specs/cep-instance-assembly/spec.md`(MODIFIES)· `include/tlm/cep/instance_assembly_gate.hh` · `src/core/module_factory.cc`
- oracle(2026-11-09,1 轮,PASS-WITH-NOTES)· critic(2026-11-09,1 轮,REJECT,2 BLOCKER ⇒ 本 change 为闭合动作)
