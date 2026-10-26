---
name: cpptlm-cep-moesi-gpu-adapter
priority: P1
phase: phase-10
主题: CEP moesi_gpu 协议实例 —— 首个 implemented adapter + perm 静默升级表达 + 6→3 消息类映射表机制
依赖:
  - ADR-CEP-12
  - ADR-CEP-05
  - ADR-CEP-02
  - ADR-CEP-13
  - ADR-CEP-11
  - ADR-CEP-04
  - ADR-CEP-14
状态: Proposed
created: 2026-10-26
---

> **载体定位**:本文是 **5 段规划输入**;**执行契约**在 `openspec/changes/cpptlm-cep-moesi-gpu-adapter/`
> (proposal + design + 2 个 delta capability);**任务分解**在同目录 `tasks.md`(**builder 阶段产出**)。
> **队列**:`obj-cep-platform §4.0` 队列 **#6**(Stage 1 / 2028-Q1)· 依据 `ADR-CEP-12` + `ADR-CEP-05 §13.1` · 估时 10 周标称。
> **前序**:#1 umbrella / #2 poc-spike / #3 stub-cache-core / #4 directory-ordering-plane / #5 collective-plane-base 均已 ✅ archived(完成归档语义)
> ⇒ **KPI 并发 0/3 → 本 change 占 1 slot**(CEP 系列自身并发 ≤ 1,per `obj §6.3 D1`)。
> **⚠ 本 change 同时承载一次**排期变更**:全局 **13** change / 串行 **13** 季度(新旧对照与终态季度**见 `obj §4.6`**,per DRI 三问裁决 Entry #71)。

---

## Why

`moesi_gpu` 不是「又一个工业协议」—— 它是 CEP 平台上**唯一**能让 `perm` 形态演进(S1-7 ①②)有真实消费者的协议实例,也是 `ADR-CEP-05 §7` α 失效判据 2 **唯一**的触发源。

| 缺口 | 证据 |
|---|---|
| **`perm` 形态演进(S1-7 ①②)无归属** | `ADR-CEP-05 §13.1`:判据 2 **已触发** —— `moesi_gpu` 的 `state_transition[2][1]=3`(E+Write→M)是**静默升级**,(a)∧(b) 成立 ⇒ **4 值 `perm` 确无法表达**;而 `CEN-Ib11` 的 4 值锁仍在 ⇒ 表达缺口**存在**且**无载体** |
| **「演进跟谁走」曾无解** | 原队列 #6 = `chi-rnf-subset`,而 CHI RN-F 是**显式**请求升级(`07 号 §5.1.1`)⇒ **不需要**该演进 ⇒ 由它承载 = schema 在**无消费者**的真空里演进 = `Entry #58` 明确拒绝的形态 |
| **CEP 至今零个真实 adapter** | `grep -rn "MoesiGpuAdapter" include/ src/ test/ examples/` → **零命中** ⇒ **设计口径** 16 个 adapter 实例(`07 号 §5.1`)里 **0 个**有实现(本仓 `protocol_instances.json` 迄今仅含 ArchForge 贡献的 **2** 份实例声明),`status` 全 `planned` ⇒ `INV-CEP-12-IC3` 口径下**计数 = 0** |
| **S1-2 / S1-3 无触发路径** | S1-2 = 「首个 `implemented` 实例装配后复核」;S1-3 = 「第 3 个协议实例」⇒ 二者都以「有真实实例」为前提,而实例**排期 = 未排期** ⇒ 两条准入前置**永久悬空** |
| **6→3 消息类映射表(S1-5)零载体** | `ADR-CEP-12 §8.3` ②:映射表由 **adapter 处的显式映射表**承担(非恒等,Stage 1 交付);`Entry #58` 裁定传输层**不建**映射表 ⇒ 映射表被**显式推给本 change** |
| **`perm_upgrade` 的跨仓镜像冲突** | `cep_decl_alignment_check.sh` ⑥ = 本仓 `protocol_instances.json` 与 ArchForge 声明**逐字段严格相等** ⇒ 直接给实例加字段 ⇒ **当场红** ⇒ 落点必须重新设计 |

## What Changes

### 新增

| 文件/capability | 作用 |
|---|---|
| `openspec/specs/cep-moesi-gpu-adapter/spec.md`(delta) | capability `cep-moesi-gpu-adapter`(7 Scenario)—— `MoesiGpuAdapter` + 6→4 投影 + 6→3 映射 + 实例声明 + 首个 `implemented` 装配 |
| `openspec/specs/cep-adapter-schema-contracts/spec.md`(delta) | capability `cep-adapter-schema-contracts`(6 Scenario)—— 5 条可机器检查的契约判据 |
| `include/tlm/cep/moesi_gpu_adapter.hh` | header-only `MoesiGpuAdapter`(6 态 × 6 事件 + 6→4 投影 + 6→3 映射) |
| `include/tlm/cep/adapter_schema_contracts.hh` | header-only 契约判据(5 条)+ 三态 + verdict 逐条输出 |
| `include/tlm/cep/perm_upgrades.json` + `.schema.json` | 静默升级声明源 + schema(**诞生即不冻结**)—— **不扩** `protocol_instance.schema.json` |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/tlm/cep/protocol_instances.json` | `moesi_gpu.status`:`planned` → `implemented`(**条件达成**) |
| `scripts/test/cep_decl_alignment_check.sh` | ⑥ 项排除 `status` 字段(本仓权威)—— 守卫收窄 ⇒ 须有反向 fixture |
| `.rddf/roadmap/**` + `AGENTS.md` | 排队重排(11 → 13 change)+ KPI 0/3 → 1/3 + 季度 11 → 13 |
| **`ADR-CEP-00` / `ADR-CEP-12` / `ADR-CEP-05` / `ADR-CEP-13` / `ADR-CEP-NUMBER-MAP`** | **冻结面** —— ADR-CEP-00 排队算术 + NUMBER-MAP KPI 预算块(两步授权)+ 其余 3 份 Status Update |

### 冻结面

| 成员 | 改什么 | 授权 |
|------|--------|------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | §2 决策 3 排队 11 → 13 + §5 汇总 + Status Update | **Entry #71** |
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | Status Update §15(S1-7 落地载体确认) | 同上 |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | Status Update §10(`perm_upgrade` 落点裁决) | 同上 |
| `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` | Status Update §9((b) 的首个可执行载体) | 同上 |
| `docs/adr/ADR-CEP-NUMBER-MAP.md` | KPI 预算块 7 → 9 / 11 → 13 + 增补块 | 同上 |
| 其余 34 个冻结成员 | 0 diff | — |

## Non-Goals

| 不做 | 理由 |
|---|---|
| `protocol_instance.schema.json` 改动 | `perm` 4 值锁 + 现有 13 属性断言零回归;`perm_upgrade` 落独立声明源(design D-β) |
| `mesif` 的实现 | 排 #8,被 R-1(`HierarchicalDirectory`)阻塞;本 change 零触碰 |
| `CATEGORY_*` 词表扩容 | `ADR-CEP-12 §8.3` ② 分层纪律 |
| 跨域写传播机制 / proxy-cache | `ADR-CEP-13` 决策 3 的运行期机制,超出「声明 + 契约」层 |
| SharerTracker 第 6 种实现 | R-1 ⇒ #8 的前置 |
| 23 ABI / `fabric.*` / frozen headers | 冻结面 |
| 全表逐格跨仓镜像(6×6) | 过度设计(design D-ε);关键点由 AC-05 断言 |
| 跨仓 `ArchForge` 任何文件 | 只读;跨仓同步是**前置**,不是本 change 的动作 |

## Acceptance

### A. 行为面(`cep-moesi-gpu-adapter`,7 Scenario)

1. 6 态 × 6 事件表驱动 + `E+Write→M` 静默升级 + (a)(b) 两侧语义
2. 6→4 投影完备 + **瞬态 `T` 无投影** + `O/M` 合并登记
3. 6→3 映射表完备 / 值域受限 / **非恒等** / 恒等须带理由
4. 实例声明逐字镜像 ArchForge(除 `status`)+ 10 流全项 + `perm` 4 值
5. per-network 嵌套形态的 **IC2 三条件**装配期校验(含反向 fixture)
6. `status` 翻转 ⇒ 计数 **0 → 1**;跨仓未同步则记「**条件达成**」
7. `mesif` **不在**本 change 交付面内(反向)

### B. 契约判据面(`cep-adapter-schema-contracts`,7 Scenario)

1. **AC-01** `perm` 4 值锁不破(含跨 schema `$ref` 值域绑定)+ 2 条反向 fixture
2. **AC-02** 引用完整性 + `perm` 覆盖 + 权限单调 + 3 条反向 fixture
3. **AC-03** 三态显式化 + verdict 可 grep + 声明源畸形 ⇒ FAIL + `mesif` 注入 ⇒ 红
4. **AC-04** 映射表完备 / 值域 / 非恒等 / 恒等带理由 + 3 条反向 fixture
5. **AC-05** 投影自洽(稳定态齐全 / `T` 无投影 / `E+Write→M` / 投影随转换变化)+ 2 条反向 fixture
6. **⑥ 镜像闸的 `status` 收窄**:正控(仅 `status` 不同 ⇒ 不红)+ 反向 fixture(改任一非 `status` 键 / 增删实例 ⇒ 必红)+ 登记进 `cep_decl_reverse_verify.sh`
7. 判据实现**单一**(禁双实现)+ 声明源被消费 + CLI 与测试 verdict 逐条相同

### C. 流程与治理

- `openspec validate cpptlm-cep-moesi-gpu-adapter --strict` rc=0
- `[cep]` 全绿 + `ctest` 全绿 + 守门 7 项 rc=0 + `cep_gate_selftest` 全绿
- 冻结面:两步授权(`--authorized-by "Entry #71"`)+ manifest 重生成
- 台账 Entry #71;`obj §4.0` 13 行 / 列数一致 / 五处同改到位 / 旧数不复活

## Capabilities

- **新增**:`cep-moesi-gpu-adapter`(协议实例面)· `cep-adapter-schema-contracts`(跨实例复用的契约面)
- **不复用**:`cep-moesi-gpu-adapter` 是**首个** `implemented` 实例 ⇒ `INV-CEP-12-IC3` 的计数口径在本 change 后由 **0** 变 **1**

## Impact

| 面 | 影响 |
|---|---|
| **路线图** | 全局 **13** change / 串行 **13** 季度;终态季度**见 `obj §4.6`**;Stage 1 变 **9** 个 change |
| **后续 change** | `#7 chi-rnf` / `#8 mesif` / `#9 cxl-nonsym` / `#10 tilelink` / `#11~#13` 顺序号与季度顺延;`chi-rnf` 范围收窄(复用本 change 的机制) |
| **准入前置** | S1-2 由 ⛔ 未触发 → **🟡 本 change 触发**;S1-5 / S1-7 ①② **本 change 交付**;S1-3 触发点前移至 #8 |
| **孤儿登记** | 「`moesi_gpu` / `mesif` 未排期」**消解** |
| **冻结面** | **5** 个成员改动(ADR-CEP-00 排队算术 + NUMBER-MAP KPI 预算块 + ADR-CEP-12 / 05 / 13 各 1 个 Status Update) |
| **跨仓** | 只读;`status` 翻转的跨仓同步**不作为前置**(design D-ζ);R-1 / C-1 / D-9 批 2 三项登记在案 |
| **下游 adapter** | 后续 7 个 adapter change **复用** `cep-adapter-schema-contracts` 的判据实现,各自**填行** |
