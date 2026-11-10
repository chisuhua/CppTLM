# tasks —— `cpptlm-cep-bridge-reattach-guard`

> 顺序:**T1(helper)→ T2(装配闸改调)→ T3(装配路径关口)→ T4(判据 + 变异)→ T5(delta spec)→ T6(`07 号` SU 十七)→ T7(冻结面五步链)→ T8(回归 + 归档)**
> 门:**G1 = T4**(判据有牙)· **G2 = T7**(冻结面链闭合)· **G3 = T8**(无回归)

## T1 · 共享 helper(唯一实现)

- [x] T1.1 新增 `include/tlm/cep/bridge_module_membership.hh`(`tlm::cep` **公开**命名空间,⛔ 不留在 `detail`)
- [x] T1.2 `count_bridge_module_membership(bridges)` ⇒ `map<string,int>`(**无 entry 内去重** ⇒ 单 entry 内重复亦计数)
- [x] T1.3 `find_bridge_module_conflicts(bridges)` ⇒ `vector<pair<string,int>>`(按模块名升序 ⇒ 诊断稳定)
- [x] T1.4 `bridge_module_conflict_message(name, n)` ⇒ **逐字**沿用 `[BSS-01] module in multiple bridge entries: <name> (<n>)`
- [x] T1.5 头注释写明判据**出处**(SU · 十七 + Entry #319)+「**唯一实现,禁复制**」+ 两个子情形

## T2 · 装配闸改调 helper(行为严格等价)

- [x] T2.1 `instance_assembly_gate.hh` ⑥ 段改调 `find_bridge_module_conflicts` + `bridge_module_conflict_message`
- [x] T2.2 ⛔ 不改 `run_instance_assembly_gate` 签名 / `InstanceAssemblyReport` / verdict 枚举
- [x] T2.3 ⛔ 同批改写 `:210-212` 的「只测不拦 / 另立 change」注释(critic N-1 —— 就在被改文件里)
- [x] T2.4 ✅ SCE-12 既有正/反控**照过**(行为等价的回归证据)

## T3 · 装配路径结构性关口

- [x] T3.1 `module_factory.cc` 新增 `#include "tlm/cep/bridge_module_membership.hh"`
- [x] T3.2 在 Step 7.5 挂载循环(`:811`)**之前**调 `find_bridge_module_conflicts(bsec)`
- [x] T3.3 命中 ⇒ **直接 `return false`**(⛔ **不是**只置 `bridge_failed` —— 该 flag 是**累积标志** ⇒ 置位后循环照跑 ⇒ 半挂载)
- [x] T3.4 诊断沿用 `bridge_module_conflict_message`(逐字一致)+ `[ModuleFactory] …` 前缀风格
- [x] T3.5 同批补 Step 7.5 注释的第 6 条红条件(critic N-2)
- [x] T3.6 注释**只引用 helper**(⛔ 不引 ADR / 不写 `§` 节号)· 但**写明失败原子性边界**(限桥层、⛔ 非「零副作用」)与**诊断优先级**(结构性关口先于逐条桥校验)

## T4 · 判据 + 决定性变异

- [x] T4.1 新增 `test/test_cep_bridge_reattach_guard.cc`(3 cases)
- [x] T4.2 **子情形 ①**:跨 entry 重叠 ⇒ `instantiateAll == false`(+ 正控:原 cfg 装配成功)
- [x] T4.3 **子情形 ②**:单 entry 内重复(`["w","chi","w"]`)⇒ `instantiateAll == false`
- [x] T4.4 **两侧一致**:同一 config ⇒ 闸判红 **且** 装配失败;正控 ⇒ 闸判绿 **且** 装配成功
- [x] T4.5 载体形态:**真 `instantiateAll`**(⛔ 不用「直接调 helper」冒充)
- [x] T4.6 **决定性变异 ①**:helper 判定改成恒不命中 ⇒ 用例必红
- [x] T4.7 **决定性变异 ②**:helper 加 entry 内去重(口径收窄)⇒ 子情形②必红
- [ ] T4.8 登记 ctest 项(标签不含 `[cep]` 之外的覆盖问题 ⇒ `[cep][assembly]` 已被 `cep_all` 覆盖 ⇒ **核对**是否需独立注册)

## T5 · delta spec

- [x] T5.1 `specs/cep-instance-assembly/spec.md` = MODIFIES `openspec/specs/cep-instance-assembly/spec.md`
- [x] T5.2 判据集合加入 `[BSS-01]`(装配闸 SHALL 判红)
- [x] T5.3 归因 tag 范围改为 `[IAG-01]`~`[IAG-07]` **与 `[BSS-01]`**;⛔ **不得**复用 `IAG-08`
- [x] T5.4 **两子情形显式枚举**(跨 entry 重叠 · 单 entry 内重复)⇒ 口径**不得收窄**
- [x] T5.5 「装配路径 SHALL 执行同一判据」段(含共用实现 / 循环前 / 零桥挂载 / 原子性边界 / 诊断优先级)

## T6 · `07 号` SU · 十七(冻结面)

- [x] T6.1 追加 **SU · 十七**(append-only;⛔ **不回改** SU 十五 ②④ 原文)
- [x] T6.2 登记:结构性关口已落地 · 「只测不拦 / 登记为 backlog」陈述**已闭合** · 原子性口径(**限桥层**)· 诊断优先级 · CLI 不对称登记
- [x] T6.3 索引 A / B 各加一行 + 头部自述 **48 → 49 个一级节 / 20 → 21 则 SU**
- [x] T6.4 ⛔ 索引**仍是文末最后一个 `## ` 段**(`cep_index_position_check.sh` 复跑 rc=0)

## T7 · 冻结面五步链

- [x] T7.1 台账 **Entry #319** 授权(已落)
- [ ] T7.2 产物 commit(SU 十七 + 索引 + 计数)
- [ ] T7.3 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #319"`
- [ ] T7.4 **清单单独 commit**(⛔ 不与产物同 commit)
- [ ] T7.5 无参复验 rc=0 + provenance 通过

## T8 · 回归 + 归档

- [ ] T8.1 `ctest --test-dir build -j4` 全绿
- [ ] T8.2 `./test.sh --mode off` 21/21
- [ ] T8.3 `bash scripts/test/cep_freeze_check.sh` 无参 rc=0
- [ ] T8.4 `bash scripts/test/cep_docs_sync_check.sh --strict` ✅
- [ ] T8.5 `scripts/test/cep_*.sh` **35/35** 全绿
- [ ] T8.6 `bash scripts/test/cep_adr_structural_check.sh` 4/4
- [ ] T8.7 零 diff 边界核验(`include/tlm/cep/**` 除本 change 2 头外 · B3b-2 三件 · 冻结清单除授权更新外)
- [x] T8.8 `openspec validate cpptlm-cep-bridge-reattach-guard --strict` rc=0
- [ ] T8.9 **归档**:`openspec archive cpptlm-cep-bridge-reattach-guard --yes`(带 delta ⇒ 合并进 `cep-instance-assembly` main spec)⇒ KPI slot 释放 + 6 处 KPI 活口径同步回 0/3
- [ ] T8.10 台账追加**实施回执** Entry(含 T4 的两条变异结果 + T5/T6 的落点 + T7 的链证据)
- [ ] T8.11 ⛔ **不主动 push**(用户决策)

**关联**:台账 Entry **#319** / **#317** / **#310** / **#305 ③ R-3** · oracle(1 轮)· critic(1 轮,REJECT ⇒ 本 change 为闭合动作)· `07 号` SU · 十五 ②④
