# cpptlm-cep-moesi-gpu-adapter: `moesi_gpu` 协议实例(首个 `implemented` 实例)+ `perm` 静默升级表达 + 6→3 消息类映射表

> **状态**: 🟡 **Proposed**(2026-10-26 建目录;planner 阶段产出 proposal + design + 2 个 delta capability)
> **队列**: `obj-cep-platform §4.0` 队列 **#6**(Stage 1 / 2028-Q1)· 依据 `ADR-CEP-12` + `ADR-CEP-05 §13.1` + `ADR-CEP-02` + `ADR-CEP-13` · 前序 #1~#5 均已 ✅ archived(完成归档语义)→ **KPI 并发 0/3 → 本 change 占 1 slot**
> **形态**: **单 change 单 Phase**;估时 10 周标称 → 实际 6-8 周当量(队列 #2/#3/#4/#5 实测先例:标称 8~12 周实际 4-6 周)
> **裁决(2026-10-26,DRI 三问)**: ① `moesi_gpu` / `mesif` **排入 Stage 1 前段**(原 #6 `chi-rnf-subset` 顺延为 #7,`mesif` 新增为 #8;全局 11 → 13 change / 11 → 13 季度)② **S1-7 ①②**(`perm` 形态 schema 演进)**跟 `moesi_gpu` 走** —— 其**唯一真实消费者**是本实例 ③ **S1-5**(6 类 → `CATEGORY_*` 显式映射表)**跟首个 adapter 走**
> **关联 ADR**: `ADR-CEP-12`(schema owner:`perm` 形态演进 + 实例声明)+ `ADR-CEP-05 §13.1`(α 失效判据 2 **已触发** —— `moesi_gpu` 静默 `E→M`)+ `ADR-CEP-02`(canonical 10 流 + `CEN-Ib11` `perm` 4 值锁)+ `ADR-CEP-13`(`INV-CEP-13-IC2` 域可见写判据 (a)(b))+ `ADR-CEP-04`(Sharer Tracker 选型)+ `ADR-CEP-11`(协议版本锁定 `moesi-gpu-1.0`)+ `ADR-CEP-14`(Stub 合同测试)
> **跨仓权威(只读)**: ArchForge `32 号` §6.1(`moesi_gpu` 声明)+ §6.0.1(6 类消息类词表)+ §6.0.2(per-network VC 矩阵)+ §6.4(schema delta)+ `09 号` §3(MOESI/GPU 6×6 状态机)+ `25 号` §4.1(NSA 三级目录)
> **承接**: 队列 #5 归档后重指向本行的余项 —— **S1-7 ①②**(`perm` 形态 schema 演进,本 change 交付)· **S1-5**(6→3 映射表机制,本 change 交付)· **S1-2**(首个 `implemented` 实例装配后复核,**本 change 触发**)· **S1-3**(第 3 个协议实例,触发点前移至 #8)· **S1-1 ②** / **S1-4** / **S1-7 ③ (β)** ✅ 已消费
> **planning 输入**: `.rddf/improvements/cpptlm-cep-moesi-gpu-adapter.md`(5 段草稿)

---

## Why

Stage 1 的第 6 个 change —— 但**排期位置本身就是本 change 的第一产物**。`moesi_gpu` 不是「又一个工业协议」:它是 CEP 平台上**唯一**能让 `perm` 形态演进(S1-7 ①②)有真实消费者的协议实例,也是 `ADR-CEP-05 §7` 判据 2 **唯一**的触发源。

| 缺口 | 证据 |
|---|---|
| **`perm` 形态演进(S1-7 ①②)无归属** | `ADR-CEP-05 §13.1`:判据 2 **已触发** —— `moesi_gpu` 的 `state_transition[2][1]=3`(E+Write→M)是**静默升级**,(a)∧(b) 成立 ⇒ **4 值 `perm` 确无法表达**;而 `ADR-CEP-02 CEN-Ib11` 的 `perm` **4 值锁**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`)仍在 ⇒ 表达缺口**存在**且**无载体** |
| **「演进跟谁走」曾无解** | 原队列 #6 = `chi-rnf-subset`,而 CHI RN-F 是**显式**请求升级(`07 号 §5.1.1`「工业协议族为显式请求升级」)⇒ **不需要**该演进。若由它承载 ⇒ schema 在**无消费者**的真空里演进 = `Entry #58` 明确拒绝的形态(「M-4 前置不能与实现分离 —— 裁决在真空里会被实现目标带偏」) |
| **CEP 至今零个真实 adapter** | `grep -rn "MoesiGpuAdapter\|MesifAdapter" include/ src/ test/ examples/` → **零命中**(仅 `protocol_instances.json` 里的**字符串** `adapter` 名)⇒ **设计口径** 16 个 adapter 实例(`07 号 §5.1`)里 **0 个**有实现(本仓 `protocol_instances.json` 迄今仅含 ArchForge 贡献的 **2** 份实例声明),`status` 全为 `planned` ⇒ `INV-CEP-12-IC3` 的「仅 implemented 计入 adapter 实例数」口径下 **计数 = 0** |
| **S1-2 / S1-3 无触发路径** | S1-2 = 「首个 `implemented` 实例装配后复核 per-network 嵌套形态」;S1-3 = 「第 3 张网 / 第 3 个协议实例」⇒ 二者都以「有真实实例」为前提,而实例**排期 = 未排期** ⇒ 两条准入前置**永久悬空** |
| **6→3 消息类映射表(S1-5)零载体** | `ADR-CEP-12 §8.3` ②:6 类词表落 Coherence Interface 层、`CATEGORY_*` 保持链路层 3 类,**两者关系由 adapter 处的显式映射表承担(非恒等,Stage 1 交付)**;`Entry #58` 裁定传输层**保持索引对齐**、**不建**显式映射表 ⇒ 映射表被**显式推给本 change**,仓内零载体 |
| **`perm_upgrade` 的跨仓镜像冲突(新发现)** | `scripts/test/cep_decl_alignment_check.sh` ⑥ = 本仓 `protocol_instances.json` 与 ArchForge 声明**逐字段严格相等**(含「键只存在于一侧」即判红)⇒ 若把 `perm_upgrade` 加进 `protocol_instances.json` 的实例里 ⇒ **当场红** ⇒ 落点必须重新设计(见 design **D-β**) |

**为什么不能全推给 `#7` `chi-rnf-subset` / `#8` `mesif`**:CHI 不需要 `perm` 演进;`mesif` 被 **R-1**(`HierarchicalDirectory`,本仓尚无 owner)阻塞且 ArchForge 侧**无 `mesif` 状态机规约**。⇒ 唯一能同时满足「`perm` 演进有消费者」「6→3 映射表有真实行」「S1-2 有触发载体」的实例就是 `moesi_gpu`。

**为什么在同一 change**:`perm` 形态表达、6→3 映射表机制、协议实例声明、adapter 实现**互为对方的验证条件** —— 声明里的 `perm_upgrade` 若无 adapter 的静默升级行为作对照,就是**只写不读的死字段**(队列 #5 `Entry #69` U-1 教训);映射表若无真实 6 类消息类作行,「非恒等」根本不可判定。

---

## What Changes

### 新增

| 文件/capability | 作用 |
|---|---|
| `openspec/changes/cpptlm-cep-moesi-gpu-adapter/specs/cep-moesi-gpu-adapter/spec.md`(delta) | **capability `cep-moesi-gpu-adapter`** —— `MoesiGpuAdapter`(6 态 × 6 事件表驱动)+ 6→4 投影 + 6→3 消息类映射 + 实例声明 + **首个 `implemented` 装配** |
| `openspec/changes/cpptlm-cep-moesi-gpu-adapter/specs/cep-adapter-schema-contracts/spec.md`(delta) | **capability `cep-adapter-schema-contracts`** —— **5 条可机器检查**的契约判据:`perm` 4 值锁不破 / `perm_upgrade` 引用完整性 + 权限单调 / 反向 fixture / 6→3 映射表完备且非恒等 / adapter 投影自洽 |
| `include/tlm/cep/moesi_gpu_adapter.hh` | **header-only** `MoesiGpuAdapter`(6 态 × 6 事件 + 6→4 投影 + 6→3 映射表);**testbench 定位**(不进 `ModuleFactory`、不注册 23 ABI、不改 frozen header) |
| `include/tlm/cep/adapter_schema_contracts.hh` | **header-only** 契约判据(5 条)+ 三态 SKIP/PASS/FAIL + verdict 逐条输出 |
| `include/tlm/cep/perm_upgrades.json` | **本仓机器可读**的静默升级声明源(`moesi_gpu`:`UNIQUE → WRITEBACK`);**诞生即不冻结**,per 队列 #4/#5 先例 |
| `include/tlm/cep/perm_upgrades.schema.json` | 配套 schema(**诞生即不冻结**)—— `silent_upgrades[].from/to` **跨 schema `$ref`** 引用 `protocol_instance.schema.json` 的 `perm` 4 值锁 |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/tlm/cep/protocol_instances.json` | `moesi_gpu` 的 `status`:`planned` → `implemented`(**条件达成**,见 design **D-ζ**);**其余 13 字段零改动**(逐字镜像 ArchForge) |
| `scripts/test/cep_decl_alignment_check.sh` | ⑥ 项**排除 `status` 字段**(本仓权威:「本仓是否已实现」,非 ArchForge 镜像面)—— 收窄守卫 ⇒ 必须有**反向 fixture** 证明其余字段仍严格镜像 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 排队表**重排**(11 → 13 change)+ §4.2~§4.4 顺序号 + §4.6 终态 2029-Q2 → **2029-Q4** + §6.2 + §10 + §11 Entry #71 |
| `.rddf/roadmap/phases/phase-10.md` | 依赖图 + KPI 预算行(Stage 1 7 → 9) |
| `.rddf/roadmap.md` · `AGENTS.md` · `.rddf/improvements/cpptlm-cep-umbrella.md` | KPI baseline 0/3 → **1/3** + 季度数 11 → 13 |
| **`docs/adr/ADR-CEP-00-platform-charter-and-governance.md`** | **冻结面** —— §2 决策 3 排队 11 → **13** change + §5 决策汇总同步 + **Status Update 2026-10-26**(队列 #6 已启动 + 排队扩至 13) |
| **`docs/adr/ADR-CEP-12-protocol-engine-capability-map.md`** | **Status Update** —— `perm` 形态演进的**落点裁决**(独立声明源而非 `protocol_instance.schema.json`)+ 6→3 映射表的**首个交付载体** |
| **`docs/adr/ADR-CEP-05-stub-cache-core-interface.md`** | **Status Update** —— §13.1 的「重评估 (β)/`perm` 判别位 → Stage 1(登记 `S1-7`)」**落地载体确认** + 「`protocol_instance.schema.json` 本批不改」的**本批口径澄清** |
| **`docs/adr/ADR-CEP-13-coherence-domain-ownership.md`** | **Status Update** —— (b) 成立后的**首个可执行载体**(本 change 的静默升级声明是 (a)(b) 的**声明侧**投影) |
| `scripts/test/cep_gate_selftest.sh` | 活队列锚点 #6 换名(`cpptlm-cep-chi-rnf-subset` → `cpptlm-cep-moesi-gpu-adapter`) |

> **冻结面改动走两步授权**:先 commit 产物 → `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #71"` → 再 commit 清单(台账 §11 须先有 **Entry #71**)。

### 冻结面

| 成员 | 改什么 | 授权 |
|------|--------|------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | §2 决策 3 排队算术 + §5 汇总 + 追加 Status Update | **Entry #71**(两步 commit 流程) |
| 其余 **38** 个冻结成员 | **0 diff** | — |

### 不改(明确排除)

| 项 | 理由 |
|---|---|
| `include/tlm/cep/protocol_instance.schema.json` | **零改动** —— `perm` 4 值锁(`CEN-Ib11`)+ 现有 13 属性断言零回归;`perm_upgrade` 落**独立声明源**(design **D-β**) |
| `include/tlm/cep/fabric.{schema.json,json}` | 冻结面已冻结(Entry #56);`vc_count` 不变 |
| `include/abi/**` · `include/tlm/gpu/pcie_endpoint_tlm.h` | 23 ABI 冻结面(per `ADR-DGPU-08 §2.3`) |
| `test_cep_canonical_invariants.cc` 的 15 组 66 条编号 | **新增 invariant 数 = 0**(`ADR-CEP-01`/`ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反;本 change 贡献 = **行为化既有条目** + 契约层加固) |
| `mesif` 的实现 | 排 **#8**,被 R-1(`HierarchicalDirectory`)阻塞;本 change **不碰**(design **D-η**) |
| 跨仓 `ArchForge` 任何文件 | **只读**;跨仓同步是**前置**,不是本 change 的动作 |

---

## Capabilities

### 新增 capability:`cep-moesi-gpu-adapter`

**协议实例面** —— `MoesiGpuAdapter` 的行为契约:6 态 × 6 事件表驱动 + 6→4 投影 + 6→3 消息类映射 + 实例声明 + 首个 `implemented` 装配。

### 新增 capability:`cep-adapter-schema-contracts`

**跨实例复用的平台级契约面** —— 5 条可机器检查的判据。**后续 7 个 adapter change**(`chi-rnf` / `mesif` / `cxl-nonsymmetric` / `tilelink` / `cxl-dcoh-symmetric` / `nvlink` / `ualink`)**复用**本 capability 的机制,只填各自的行。

---

## 验收

见 2 份 delta spec 的 Scenario 清单(共 **13** 条 = 7 + 6)。**判据退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**(per 队列 #5 `Entry #68` 的红绿分离纪律)。

**DoD 引用**:`obj-cep-platform §6.2` B1~B8 + §6.3 D1~D3 + `ADR-CEP-00 §2 决策 3`。

---

## 依赖与风险

| 项 | 类型 | 缓解 |
|---|---|---|
| **ArchForge `32 号 §6.1` 的 `moesi_gpu.status`** | **跨仓前置** | ⑥ 项把 `status` 排除出镜像面(本仓权威)⇒ 消除对跨仓同步的**硬依赖**;若跨仓已同步则两侧仍相等 |
| **ArchForge 侧无 `mesif` 状态机规约 / 无 5→4 投影表** | 跨仓债 | 本 change **不碰** `mesif`;#8 行显式登记 R-1 阻塞 |
| **ArchForge `32 号` 是 Draft v0.1 + D-9 批 2(2026-11-05)会改 `09 号` 的 8 处引用** | 锚点漂移 | 本仓引用 `09 号` 的**状态机语义**而非行号;若引用行号,须在批 2 落地后重生成锚点 |
| **C-1 放行判据要求 ArchForge 侧有 commit hash 的接口 PR** | 治理阻塞(既有) | `ADR-SOC-25 D11` 提议改为「接口规约 PR」;本 change 的跨仓前置登记在 design **D-θ** |
| **6→3 映射表的「非恒等」可能对某些实例不成立** | 设计风险 | 判据只要求「**至少一行**非恒等」+「每类有行」;若某实例确实恒等 ⇒ 该实例的映射表**显式声明恒等并给出理由**,不静默放过 |
| **`perm_upgrade` 落独立声明源 ⇒ 与 `protocol_instances.json` 无 schema 级绑定** | 一致性风险 | 由 **AC-02**(引用完整性 + perm 覆盖 + 权限单调)承担;**不**靠 JSON Schema 的跨文件引用 |

---

## 关联

- 台账:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #71**
- 前序:Entry #70(队列 #5 归档)· #67~#69(队列 #5 三段评审)· #66(队列 #5 planner)· #58(M-4 前置裁决先例)
- 变更件:`openspec/changes/cpptlm-cep-moesi-gpu-adapter/`
