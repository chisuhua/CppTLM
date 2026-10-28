---
name: cpptlm-cep-stub-cache-core
priority: P1
phase: phase-10
主题: CEP Stub Cache Core —— RN 侧协议状态投影(6 接口 + 5 态)+ M-4 传输层 VC 映射契约闭合
依赖:
  - ADR-CEP-04
  - ADR-CEP-05
  - ADR-CEP-12
  - ADR-CEP-09
  - ADR-CEP-14
  - ADR-CEP-02
状态: Archived
created: 2026-10-25
last_revised: 2026-10-25
---

# cpptlm-cep-stub-cache-core: Stub Cache Core(RN 侧投影)+ M-4 传输层 VC 映射契约闭合

> **状态**: ✅ **已完成并归档**(2026-10-25;**完成归档**语义 —— tasks **36 / 36** 全勾;delta capability `cep-vc-mapping-contract`(8 Scenario)+ `cep-stub-cache-core`(9 Scenario)**已合并**进 `openspec/specs/`;归档路径 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/`;per 台账 Entry #58 / #59 / #60)
> **队列**: `obj-cep-platform §4.0` 队列 **#3**(Stage 1)· 前序 #1 `cpptlm-cep-umbrella` / #2 `cpptlm-cep-poc-spike` 均已 ✅ archived → 归档后 **KPI 并发 1/3 → 0/3**(slot 已释放)
> **载体定位**:本文是 **5 段规划输入**(为什么做);**执行契约**已归档于 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/`(做什么/怎么验);**排队与状态**在 `obj-cep-platform §4.0`;**载体分工与状态更新协议**在 `obj-cep-platform §4.7`
> **关联路线图**: [`phase-10`](../roadmap/phases/phase-10.md) · [`objective-cep-platform`](../roadmap/objectives/obj-cep-platform.md)
> **估时 / 季度**: 见 `obj §4.2` / `obj §4.6`(单源纪律 —— 具体数值只在 objective 内)。**实际交付面显著小于 8 周估时** —— `ADR-CEP-05` 的定义足够收敛,6 接口 + 5 态 + 6 条 `INV-SC*` 可在**单文件头**内实现
> **冻结面**: 成员数以 `scripts/test/cep_freeze_check.sh` 清单为准(本 change 改动 **2** 个成员:`ADR-CEP-05` / `ADR-CEP-00`,均**仅追加** Status Update 段)

---

## Why

本 change 同时承接**两件必须先于实现落地的事**,二者都因「stub cache 是 CEP 第一个实现型 change」而具有**时效性**。

### ① M-4 行为侧决策未闭 —— 且现状语义是**错的**

台账 Entry #11 ④ 登记的遗留项:「`CATEGORY_CREDIT` 按索引落入 `VC_DAT` 桶 → 改它需要**单独决策**」。该决策**至今未做**,而现状语义经 2026-10-25 实测被证明是**错的**:

| 实测事实 | 证据 |
|---|---|
| `VC_DAT` 桶在**所有真实路径中恒为 0** | 全仓唯一写 `CATEGORY_CREDIT` 的点是 `include/framework/bidirectional_port_adapter.hh` 的 `BundleDirectionTraits<NoCFlitBundle>::fill_credit`(2026-10-28 实测行号 **109**;**原为** `:235`(`send_credit_impl` 内)—— 队列 #7 Phase 4 把该写入点**泛化**为 Bundle 专属 trait,位置随之迁移,**语义不变**),该 flit 经 ChStream port 发出,**不经** `SocInternalNoc::route_flit()` |
| 现有注释**描述了一个从未发生的流量** | `include/tlm/cluster/soc_internal_noc.hh` 自陈「`vc_dat_count()` 实际统计的是 **credit 流量**」—— 实测该 accessor 的**外部消费者 0 命中** |
| 「credit 是否独占 VC」**已被跨仓权威关闭** | ArchForge `32 号 §6.0.1` 排除表:credit 是**流控类别**,**不单独占 VC** |

⇒ **M-4 的真问题不是「credit 放哪」,而是「命名说谎 + 索引对齐的隐式性」**。

### ② Stub Cache Core 是 Stage 1 的**首个可实现实体**,但只有纸面契约

`ADR-CEP-05` 已把接口(决策 1:6 接口)、挂载形态(决策 2:三明治)、折叠映射表(决策 3)、bypass 模式(决策 4)与 6 条 `INV-SC*` 全部定义完毕,但**仓内无任何实现**。

### 为什么它们在**同一个 change** 里

M-4 是 stub cache 的**准入前置**;stub cache 是 M-4 的**观测载体**。**双 Phase 单 change** 让 Phase 0 的决策在 Phase 1 的代码落地前**先行固化**,且共享同一个 KPI slot。

---

## What Changes

### Phase 0 —— M-4 传输层 VC 映射契约闭合 ✅

| # | 动作 | 状态 |
|---|------|------|
| **P0-1** | **协议层裁定**:credit **不独占** VC(跨仓权威已闭,本仓**不重新裁决**) | ✅ design.md D2 |
| **P0-2** | **传输层裁定**:**保持索引对齐**,**不建**显式映射表(显式表归**队列 #6**) | ✅ design.md D3 |
| **P0-3** | **语义正名**:`VC_DAT` = **预留 DAT 桶**(当前恒 0) | ✅ design.md D4 + lint 断言 ④ |
| **P0-4** | **陈旧注释修正**:`test_soc_internal_noc_data_flow.cc:108` | ✅ design.md D9 |
| **P0-5** | **冲突输入规则显式化**:计数归 `flit_category` / 编码归 `vc_id` / 传输层**不仲裁** | ✅ design.md D5 + lint 断言 ⑤(含**代码等价**核对) |
| **P0-6** | **桶名不改**:`VC_DAT` 与 `vc_dat_count()` 不改名不弃用 | ✅ design.md D6 |
| **P0-7** | **`fabric.schema.json` 零改动** —— 立项时的「须同步 `vc_count.maximum=4`」前提经实测**不成立** | ✅ design.md D7 |

### Phase 1 —— Stub Cache Core 实现 ✅

| # | 动作 | 状态 |
|---|------|------|
| **P1-1** | **6 接口**(net-side 5 条 CohIf RN 投影 + 1 条 core-side 原语 `evict_line`) | ✅ `include/tlm/cep/stub_cache_core.hh` |
| **P1-2** | **5 态状态机**(`NONE → I → S → E → M`),**不**暴露 CHI RN-F 细节态 | ✅ 同上 + `test_cep_stub_cache_core.cc` |
| **P1-3** | **6 条 `INV-SC1` ~ `INV-SC6`** 行为断言 | ✅ 14 cases / 204 assertions |
| **P1-4** | **折叠映射单向形态**(5 隐藏流名**不作为接口标识符**) | ✅ 静态扫描(去注释后) |
| **P1-5** | **零协议实例声明** / **不进 `ModuleFactory`** / **不注册 23 ABI** | ✅ 3 条集成面断言(各带正控) |
| **P1-6** | **不可验证条款降级**:`ADR-CEP-05 §3` #1 / #4 登记「触发条件 = 队列 #6」 | ✅ design.md D10 + Q2 |

### 已有(本 change **继承**不重做)

| 项 | 状态 |
|---|---|
| `ADR-CEP-05` 的接口 / 状态机 / `INV-SC*` 定义 | ✅ 2026-10-21 签发 —— 本 change 是其**实现**,未改其定义句 |
| `ADR-CEP-09` 的顶点升级要求 | ✅ 队列 #2 已实跑验证 |
| `ADR-CEP-12` 的 6 类词表与分层纪律 | ✅ 已机器化(lint 第 10 组)—— 本 change **不动** |

### 冻结面

**2 个成员改动**(均**仅追加** `## Status Update` 段,走 `cep_freeze_check.sh --update --authorized-by "Entry #58"`):

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | §14 —— 声明 §10 的 `-base`/`-impl` 二分已被 `obj §4.0` 重排取代 + M-4 决策结论摘要 |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | Status Update 2026-10-25 —— 队列 #3 已启动 + 上一段的「当前态」改「当时态」 |

**零改动**:`include/tlm/cep/fabric.schema.json`(其 `vc_count.maximum = 4` **不需要动**)、`include/tlm/cep/fabric.json`、2 份既有 main spec、其余 `ADR-CEP-*` 与 `07 号`。

---

## Non-Goals

1. **不扩 `CATEGORY_*` 到 6 类** —— 回执 §8.4.3 已判**不可接受**。
2. **不改 `VC_COUNT` / 不动 `fabric.schema.json` / 不要求 ArchForge 上游回改**。
3. **不交付**「6 类一致性消息类 → `CATEGORY_*` 的显式映射表」 —— 归**队列 #6**。
4. **不改 `chip_select = (vc << 4) | src` 的 wire 编码契约**;不改 `route_flit()` 路由 / 推送逻辑。
5. **不删不改名 `VC_DAT` 常量与 `vc_dat_count()`**。
6. **不实现** `ADR-CEP-05 §4.2` 的「不实现」清单(SRAM 阵列 / 多路组相联 / 预取器 / 功耗模型 / 流水线时序)。
7. **不做真实 ProtocolEngine** —— 用 mock 驱动 `net_side`。
8. **不引入任何 `status != "planned"` 的协议实例声明**。
9. **不写 Traffic Generator / bypass 模式**(归属待定,见 design.md Open Questions Q1)。

---

## Acceptance

> **红绿分离**(per 队列 #2 确立):「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本节条款**不**因结论为红而失败。

### Phase 0 —— M-4 契约 ✅

| # | 条款 | 状态 |
|---|------|------|
| **A-P0-1** | M-4 决策**可寻址**(design.md D2~D9,每条带 `file:line` 或权威章节号) | ✅ |
| **A-P0-2** | `VC_DAT` 注释**不再**声称「统计 credit 流量」,**明写**「预留 DAT 桶 / 恒 0」 | ✅ lint 断言 ④ |
| **A-P0-3** | 陈旧注释已修正,**断言集合不变** | ✅ lint 断言 ⑥ |
| **A-P0-4** | **正控**:注入 `CATEGORY_CREDIT` → `vc_dat_count()` **0 → 1** | ✅ `test_cep_vc_mapping_contract.cc` |
| **A-P0-5** | 冲突输入规则已显式化 | ✅ lint 断言 ⑤ |
| **A-P0-6** | **零改动校验**:冻结面 32 成员 0 diff;`vc_count.maximum` 仍为 4 | ✅ |
| **A-P0-7** | `ADR-CEP-05` 追加 `## Status Update`(§10 正文不改) | ✅ |
| **A-P0-8** | lint 第 10 组 ①②③ **不回退**;新增 ④⑤⑥ 配**反向 fixture** | ✅ `cep_gate_selftest` 31/31 |

### Phase 1 —— Stub Cache Core ✅

| # | 条款 | 状态 |
|---|------|------|
| **A-P1-1** | **6 接口**存在,命名逐字对齐 `ADR-CEP-05` 决策 1 的 6 行表 | ✅ |
| **A-P1-2** | **5 态可达**;接口面**不**出现 CHI RN-F 细节态 | ✅ |
| **A-P1-3** | `INV-SC1` ~ `INV-SC6` **各**至少 1 条**行为**断言 | ✅ 6 组 |
| **A-P1-4** | **折叠映射单向形态** | ✅ 静态扫描 |
| **A-P1-5** | **零协议实例声明** | ✅ |
| **A-P1-6** | **不可验证条款已降级登记**(#1 / #4 → 队列 #6) | ✅ |
| **A-P1-7** | **Scenario ↔ 锚点等值** | ✅ `test_cep_scenario_anchor_reconciliation.cc` |

### 收尾 ✅

| # | 条款 | 状态 |
|---|------|------|
| **A-FIN-1** | `openspec validate --strict` rc=0 | ✅ |
| **A-FIN-2** | 守门全 rc=0;`cep_gate_selftest` 全绿 | ✅ 31/31 |
| **A-FIN-3** | `ctest` 全绿;case 数与断言数**逐项登记** | ✅ 104/104 |
| **A-FIN-4** | 归档回填(17 落点)+ delta capability 合并 + 纳入冻结集 | ✅ 2026-10-25 |

---

## Capabilities

### New Capabilities

- **`cep-vc-mapping-contract`** —— 传输层 `CATEGORY_*` → VC 桶映射的**可回归契约**(8 Scenario):映射关系与每个桶的**当前流量实况**可被断言;零流量桶的「恒 0」**有正控**;`vc_id` 与 `flit_category` 冲突时的归属规则**显式且可断言**;传输层**不仲裁**该冲突。已合并进 `openspec/specs/cep-vc-mapping-contract/spec.md`。
- **`cep-stub-cache-core`** —— Stub Cache Core(RN 侧协议状态投影)的**可执行契约**(9 Scenario):6 接口签名与命名逐字对齐 canonical 10 流;5 态状态机可达且**不**暴露 CHI 细节态;`INV-SC1` ~ `INV-SC6` **各**至少 1 条行为断言;折叠映射表的**单向形态**可静态断言。已合并进 `openspec/specs/cep-stub-cache-core/spec.md`。

### Modified Capabilities

**无** —— 本 change **不修改**任何既有契约。

---

## Impact

### 影响范围

| 层 | 影响 |
|---|---|
| **传输层 NoC** | `soc_internal_noc.hh` —— **仅注释**;零逻辑 / 零 API / 零常量改名 |
| **新增 stub** | `include/tlm/cep/stub_cache_core.hh` —— header-only;不进 `ModuleFactory`、不注册 ABI |
| **新增测试** | `test/test_cep_vc_mapping_contract.cc`(5 cases / 34 assertions)· `test/test_cep_stub_cache_core.cc`(14 / 204)· `test/test_cep_scenario_anchor_reconciliation.cc`(4 / 25) |
| **守门** | `cep_docs_sync_check.sh` 第 10 组新增断言 ④⑤⑥;`cep_gate_harness.py` 支持 `--group 10/11`;`cep_gate_selftest.sh` 新增 7 条 fixture |
| **跨仓 ArchForge** | **零改动** —— 服从 `32 号 §6.0.1` 排除表与 `22-nsa §0.5.4.1` NS-R2 |
| **KPI** | 0/3 → 1/3 →(归档)**0/3** |
| **23 ABI / frozen headers** | **零改动** |

### 风险

| 风险 | 缓解 |
|------|------|
| spec 写出**永假验收条款** | spec 只收本仓可执行判据,每条点名锚点;对账测试断言 Scenario 数 == 锚点数 |
| 「恒 0」断言无正控 → 无牙 | A-P0-4 强制正控:注入 `CATEGORY_CREDIT` → 计数必须 0→1 |
| stub 顺手扩 `CATEGORY_*` / 提前建显式映射表 → 越权到队列 #6 | Non-Goals + lint 第 10 组① 持续守护 |
| `ADR-CEP-05 §10` 的两个旧名永久失指 | A-P0-7 追加 Status Update(§10 正文不改) |
| **单开发者仓无外部真人评审**(独立性问题) | 本仓的「评审」由 oracle / metis / critic subagent 承担 —— **不等价于**真人外部评审,独立性缺口已在 `obj §8.1` R10 登记 |

### 复评节奏

- **Phase 0 完成后**的复评:决策**服从**既有权威,`ADR-CEP-12` / `ADR-CEP-09` **未**产生新决策 → **不需要**追加 Status Update。
- **Phase 1 完成后**按 `ADR-CEP-05 §3` 的契约测试优先级逐条核对:必做前 2 条 + 高优先级 8 条中,#1 / #4 **已降级**(触发条件 = 队列 #6),其余逐条落到 `test_cep_stub_cache_core.cc`。

---

**最后**: 2026-10-25 —— **✅ 已完成并归档**。tasks **36 / 36** 全勾;2 个 delta capability 已合并进 `openspec/specs/`;归档路径 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/`;KPI **0/3**(slot 已释放);**下一个可启动的 change = 队列 #4 `cpptlm-cep-directory-ordering-plane`**(per `obj §4.0`)。per 台账 Entry #58 / #59 / #60。
