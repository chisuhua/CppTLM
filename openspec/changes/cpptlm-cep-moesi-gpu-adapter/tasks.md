# Tasks — cpptlm-cep-moesi-gpu-adapter v1.0(Stage 1,TDD 5-step)

> **状态**: ✅ **实施完成(2026-10-27)** —— 首个 `implemented` adapter 已交付;**待归档**(KPI slot 释放后)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-moesi-gpu-adapter/spec.md`](specs/cep-moesi-gpu-adapter/spec.md) · [`specs/cep-adapter-schema-contracts/spec.md`](specs/cep-adapter-schema-contracts/spec.md)
> **上游**: `phase-10.md` Stage 1 子任务 · `obj-cep-platform §4.0` 队列 **#6**
> **估时**: **10 周标称**(per `obj-cep-platform` 排队表);**本轮实施 = 1 个 builder 批次**
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 Decision 3` —— 任一时刻 CEP 内部 Proposed 并发 ≤ 1)

---

## 执行进度(2026-10-27,builder 阶段)

| Phase | 状态 | 说明 |
|-------|------|------|
| 1 载体面(`MoesiGpuAdapter`) | ✅ 完成 | 6×6 转换表 + 6→4 投影 + 6→3 映射;`include/tlm/cep/moesi_gpu_adapter.hh` |
| 2 契约面(5 条判据) | ✅ 完成 | `include/tlm/cep/adapter_schema_contracts.hh`(判据实现**单一真源**)+ 装载器 `adapter_contracts_loader.hh` |
| 3 声明源 + schema | ✅ 完成 | `perm_upgrades.json` + `perm_upgrades.schema.json`(**跨 schema `$ref`** 绑定 `perm` 4 值域) |
| 4 `status` 翻转 + ⑥ 收窄 | ✅ 完成 | `moesi_gpu.status` → `implemented`;⑥ 跳过 `status`,其余 **12** 键仍严格 |
| 5 离线 CLI | ✅ 完成 | `examples/cep_adapter_contracts.cc`(与测试**共用**同一份判据实现) |
| 6 测试 | ✅ 完成 | 2 个测试文件 / 13 cases / 170 assertions |
| 7 闸与反向验证 | ✅ 完成 | `cep_decl_alignment_check.sh` ⑥ 收窄 + ⑦ 新增;`cep_decl_reverse_verify.sh` 破坏 14(12 键**穷举**)+ 15(`status` 正控)⇒ **29/29** |
| 8 **S1 有效期重评** | ✅ 完成 | design **D-θ 强制任务** —— 见下方「归档前必须完成」 |

---

## 逐任务清单(TDD 5-step:判据 → 实现 → 夹具 → 验证 → 登记)

### Phase 1 —— 载体面

- [x] 1.1 `MoesiGpuAdapter` 的 6 态 / 6 事件 / 4 值 `perm` 枚举(索引 = 表下标)
- [x] 1.2 `state_transition[6][6]`(逐字抄 ArchForge `09 号` §3.3)
- [x] 1.3 `transition()` / `is_silent_upgrade()` / `snoop_response_carries_data()`
- [x] 1.4 6→4 投影 `perm_of()`(**瞬态 `T` 无投影**)+ `O`/`M` 合并的语义损失登记
- [x] 1.5 6→3 消息类映射表 + `is_identity` / `identity_reason`
- [x] 1.6 `category_of()` 查询 API(未知类**查不到**,不静默返回默认值)

### Phase 2 —— 契约面(5 条判据)

- [x] 2.1 `ContractVerdict` 三态(`SKIP` 与 `PASS` **可区分**;`SKIP` ≠ `PASS`)
- [x] 2.2 `ContractVerdictLine` / `ContractReport` + 逐条可 grep 的 `render()`
- [x] 2.3 `AC-01` `perm` 4 值锁(值域 4 值 / 无 `ATOMIC` / 秩序一致 / 声明值 ∈ 值域)
- [x] 2.4 `AC-02` 引用完整性 + 权限单调(秩不降)
- [x] 2.5 `AC-03` 三态 + 声明源畸形 ⇒ **FAIL**(≠ `SKIP`)+ 无权实例 ⇒ 红
- [x] 2.6 `AC-04` 映射完备 / 值域受限 / 非恒等(或恒等 + 登记)
- [x] 2.7 `AC-05` 投影与状态机自洽(稳定态齐备 / `T` 无投影 / `E+Write→M` / 秩升高)
- [x] 2.8 平台级参考面(`perm_order()` / `category_value_domain()` / `identity_reference_map()`)

### Phase 3 —— 声明源 + schema

- [x] 3.1 `perm_upgrades.json`(独立声明源,**诞生即不冻结**)
- [x] 3.2 `perm_upgrades.schema.json`(`from`/`to` 跨 schema `$ref` → `perm` 枚举)
- [x] 3.3 `$ref` **机制级**验证:`referencing` registry 解析 ⇒ 注入 `ATOMIC` / `SHARED` **必被挡住**
- [x] 3.4 闸接线:`cep_decl_alignment_check.sh` 新增 **⑦**(校验 + 4 条反向 fixture)

### Phase 4 —— `status` 翻转 + ⑥ 收窄

- [x] 4.1 `moesi_gpu.status`:`planned` → `implemented`(**其余 12 键零改动**)
- [x] 4.2 ⑥ 逐字段比对**跳过** `status`(per design `D-ζ`)
- [x] 4.3 反向 fixture:12 个非 `status` 键**穷举** ⇒ 每次必红(且报出键名)
- [x] 4.4 反向 fixture:`status` 差异 ⇒ **正控绿**(收窄不得退化为「整条不比对」)

### Phase 5 —— 离线 CLI

- [x] 5.1 `examples/cep_adapter_contracts.cc`(逐条 verdict + 退出码 0/1/2)
- [x] 5.2 CMake 注册 + `add_dependencies(cpptlm_tests …)`(⚠️ `test/` 在 `examples/` **之前**)

### Phase 6 —— 测试

- [x] 6.1 `test/test_cep_moesi_gpu_adapter.cc`(7 条 Scenario)
- [x] 6.2 `test/test_cep_adapter_schema_contracts.cc`(AC-01~05 + ⑥ 收窄 + 单一实现)
- [x] 6.3 **CLI 与测试 verdict 逐条相同**(测试实跑 CLI 并逐行比对)

### Phase 7 —— 闸与回归

- [x] 7.1 `cep_decl_alignment_check.sh`:⑥ 收窄 + ⑦ 新增(7 项)
- [x] 7.2 `cep_decl_reverse_verify.sh`:破坏 14(12 键穷举)+ 破坏 15(`status` 正控)
- [x] 7.3 守门 7 项 + `cep_gate_selftest` 34/34 + `ctest` 全绿

### Phase 8 —— **归档前必须完成**(per design `D-θ` 的强制任务)

- [x] 8.1 **S1 有效期重评**:队列 #6 的 `perm` 形态演进落地 ⇒ `S1_VALIDITY_UNTIL=queue6-perm-schema-evolution` **届满**
      → 重评结论 = **S1 仍为 FAIL**(结论未翻转;R-2 发生点不变),**可发现性改善**(静默升级能力现有机器可读声明源)
- [x] 8.2 新有效期登记:`S1_VALIDITY_UNTIL=stub-request-side-perm-extension` + **机器可检测**的届满条件
      (① Stub 请求侧 `perm_upgrade` 扩展;② `perm` 枚举 ≠ 4 值)
- [x] 8.3 重评落在**契约层**(本 change 的 delta spec ⇒ 归档后并入 `openspec/specs/cep-adapter-schema-contracts/spec.md`)
      ⇒ **零冻结面改动**(冻结的 `cep-poc-spike-verification` main spec **逐字仍然成立**)
- [x] 8.4 闸改造:断言 ⑧ 从「届满即永久红」改为「**届满 ⇒ 必须已重评登记**」+ 新锚届满条件检测
- [x] 8.5 闸自检 fixture:重评锚缺失 ⇒ 必红;新锚届满条件命中 ⇒ 必红(旧 fixture 改为正控)

---

## 归档前必须完成(复核)

| # | 项 | 状态 |
|---|---|---|
| 1 | **S1 有效期重评**(design `D-θ` 强制) | ✅ Phase 8 |
| 2 | 判据实现**单一**(test 与 CLI 共用一份) | ✅ Phase 2 + 6.3 |
| 3 | 每条新断言有**反向 fixture** | ✅ Phase 2/6(14 条反向 fixture) |
| 4 | 纯负向断言有**正控** | ✅ Phase 4.4(status 正控)+ Phase 7(旧 fixture 改正控) |
| 5 | 冻结面**零改动** | ✅ 本 change 未改任何冻结成员(hash 全一致) |
| 6 | 新增 invariant 数 = **0** | ✅ `test_cep_canonical_invariants.cc` 零 diff |
| 7 | `protocol_instance.schema.json` **零改动** | ✅ 13 属性不变(perm 演进落独立声明源,per design `D-β`) |
| 8 | `mesif` **零触碰** | ✅ 声明 / adapter / schema 均未动(per design `D-η`) |

---

## 已知限制(诚实登记,不掩盖)

- **S1 仍为 FAIL**:4 值 `perm` 在**请求侧**仍不能表达静默 `E→M` 升级 ⇒ 请求侧 `perm` 的**修正机制**不在本 change 交付面(Stub↔PE 腿归 `cpptlm-cep-stub-cache-chstream-surface`)。
- **6×6 全表与 ArchForge 规约的逐格镜像不在范围**(per design `D-ε`):只锁住语义关键点(`E+Write→M`);全表一致性由**代码评审 + 关键点断言**保证。
- **本实例的 6→3 映射恒等**(等于平台方向默认表)⇒ 按契约**显式登记** `identity_reason`;「非恒等」不是硬要求(恒等 + 登记是合法状态)。
- **S1-2 记「条件达成」而非「已达成」**:ArchForge 侧 `status` 的同步**不**作为前置(per design `D-ζ`)。
