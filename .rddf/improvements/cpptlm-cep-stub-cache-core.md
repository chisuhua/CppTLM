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
状态: Proposed
created: 2026-10-25
last_revised: 2026-10-25
---

# cpptlm-cep-stub-cache-core: Stub Cache Core(RN 侧投影)+ M-4 传输层 VC 映射契约闭合

> **状态**: 🟡 **Proposed**(2026-10-25 planner 阶段建目录;双 Phase 形态)
> **队列**: `obj-cep-platform §4.0` 队列 **#3** · 前序 #1 `cpptlm-cep-umbrella` / #2 `cpptlm-cep-poc-spike` 均已 ✅ archived(完成归档语义)→ **KPI 并发 0/3 → 本 change 占 1 slot**
> **载体定位**:本文是 **5 段规划输入**(为什么做);**执行契约**在 `openspec/changes/cpptlm-cep-stub-cache-core/`(做什么/怎么验);**排队与状态**在 `obj-cep-platform §4.0`;**载体分工与状态更新协议**在 `obj-cep-platform §4.7`
> **关联路线图**: [`phase-10`](../roadmap/phases/phase-10.md) · [`objective-cep-platform`](../roadmap/objectives/obj-cep-platform.md)
> **估时 / 季度**: 见 `obj §4.2` / `obj §4.6`(单源纪律 —— 具体数值只在 objective 内)
> **冻结面**: 成员数以 `scripts/test/cep_freeze_check.sh` 清单为准(本 change 的冻结面预算见「Impact → 冻结面」)

---

## Why

本 change 同时承接**两件必须先于实现落地的事**,二者都因「stub cache 是 CEP 第一个实现型 change」而具有**时效性**。

### ① M-4 行为侧决策未闭 —— 且现状语义是**错的**

台账 Entry #11 ④ 登记的遗留项:「`CATEGORY_CREDIT` 按索引落入 `VC_DAT` 桶 → 改它需要**单独决策**(credit 是否独占 VC / 映射是否应为**显式表**而非索引对齐)」。该决策**至今未做**,而现状语义经 2026-10-25 实测被证明是**错的**:

| 实测事实 | 证据 |
|---|---|
| `VC_DAT` 桶在**所有真实路径中恒为 0** | 全仓唯一写 `CATEGORY_CREDIT` 的点是 `include/framework/bidirectional_port_adapter.hh:235`(`send_credit_impl`),该 flit 经 `resp_out_port_[port]->send(pkt)`(ChStream port)发出,**不经** `SocInternalNoc::route_flit()`;`SocInternalNoc` 的全部入向流量只写 `CATEGORY_REQUEST`(`include/tlm/gpu/sdma_noc_adapter.hh:57`)与 `CATEGORY_RESPONSE`(`include/tlm/vram_noc_adapter.hh:96`) |
| 现有注释**描述了一个从未发生的流量** | `include/tlm/cluster/soc_internal_noc.hh:179-181` 自陈「`vc_dat_count()` 实际统计的是 **credit 流量**」—— 实测该 accessor 的**外部消费者 0 命中**(`vc_dat_count` / `vc_snp_count` 在 `test/` `src/` `examples/` 中零引用) |
| 「credit 是否独占 VC」**已被跨仓权威关闭** | ArchForge `32-cep-based-coherence-architecture.md §6.0.1`「明确排除」表:`CATEGORY_CREDIT` = **流控类别**,非一致性消息类,**credit 随所在 VC 的流一起走,不单独占 VC**;本仓镜像 `include/tlm/cep/protocol_instance.schema.json:131` 同义 |

⇒ **M-4 的真问题不是「credit 放哪」,而是「命名说谎 + 索引对齐的隐式性」**。若不先闭合,stub cache 实现期产生的新断言会**固化**这一错误语义(Entry #55 ③ 已警示:「M-4 改现状语义 → 越早定越好」)。

### ② Stub Cache Core 是 Stage 1 的**首个可实现实体**,但只有纸面契约

`ADR-CEP-05` 已把接口(决策 1:6 接口)、挂载形态(决策 2:三明治)、折叠映射表(决策 3)、bypass 模式(决策 4)与 6 条 `INV-SC*` 全部定义完毕,但**仓内无任何实现**。Stage 1 后续 5 个队列(#4 ~ #8)的准入判据都依赖一个**可运行的 stub** 作为观测对象 —— 没有它,`ADR-CEP-09` 的死锁自由判据、`ADR-CEP-13` 的域可见性判据都只能停留在纸面辩论(这正是队列 #2 用 PoC spike 解决过的问题:「准入判据至今只能靠纸面辩论」)。

### 为什么它们必须在**同一个 change** 里

M-4 是 stub cache 的**准入前置**(stub 的第一行新断言会消费传输层 VC 语义);stub cache 是 M-4 的**观测载体**(只有真实 stub 产生流量,「`VC_DAT` 恒 0」这类断言才有意义)。拆成两个 change 会让 M-4 在「没有真实消费者」的真空里被裁决 —— 那正是队列 #2 之前的失败模式。**双 Phase 单 change** 让 Phase 0 的决策在 Phase 1 的代码落地前**先行固化**,且共享同一个 KPI slot。

---

## What Changes

### Phase 0 —— M-4 传输层 VC 映射契约闭合(先于任何新断言)

| # | 动作 | 触及面 |
|---|------|--------|
| **P0-1** | **协议层裁定**:credit **不独占** VC —— 引用 ArchForge `32 号 §6.0.1` 排除表 + `protocol_instance.schema.json:131` + 回执 §8.4.3(「扩到 6 类」已判不可接受) | 决策登记(文档) |
| **P0-2** | **传输层裁定**:**保持索引对齐**,**不建**显式映射表 —— ① lint 第 10 组①(恰 3 类 + 值域连续 0..2)的机器化**前提**就是索引对齐(`scripts/test/cep_docs_sync_check.sh:573-576`);② 显式映射表的归属已被回执 A14-3 分配给**队列 #6**(S1-5 表体 + contract test),本 change 重造即权责冲突;③ `VC_DAT` 桶零流量,建表是为零流量桶建表 | 决策登记(文档) |
| **P0-3** | **语义正名**:`soc_internal_noc.hh:68-71` 与 `:179-181` 注释改为「`VC_DAT` = **预留 DAT 桶**,当前无 `CATEGORY_*` 数据类;`CATEGORY_CREDIT` 落此桶是索引对齐副产物且**无实际流量**」;`vc_dat_count()` 注释改为「预留桶,当前恒 0」 | `include/tlm/cluster/soc_internal_noc.hh`(注释) |
| **P0-4** | **陈旧注释修正**:`test/test_soc_internal_noc_data_flow.cc:108` 的「但 `vc_counts_[2]` (DAT VC) 仍增」是**陈旧注释** —— 该用例实际写 `CATEGORY_REQUEST`(`:112`)且**只**断言 `flits_dispatched()` / `unknown_dst_count()`(`:116-117`),**不存在** `vc_counts_[2]` 断言 | `test/test_soc_internal_noc_data_flow.cc`(注释) |
| **P0-5** | **冲突输入规则显式化**:`NoCFlitBundle` 同时携带 `vc_id` 与 `flit_category`(两者都写,实测 `test/test_vram_noc_adapter.cc:140-145`)。规则:`vc_counts_` 归 `flit_category`(`route_flit` 现状 `:125-127`),`chip_select` 编码归 `vc_id`(现状),传输层**不仲裁、不报错** —— 仲裁需要一致性词表知识,那是 adapter 层(队列 #6)的职责 | `include/tlm/cluster/soc_internal_noc.hh`(注释)+ 契约 Scenario |
| **P0-6** | **桶名不改**:`VC_DAT` 常量名与值**保留** —— 它已进入 `chip_select = (vc << 4) \| src` 的跨模块 wire 编码契约(`test/test_vram_noc_adapter.cc:140,145` 断言 `0x21`),改名即改编码语义常量;且 credit 本身亦无实际流量,改成 `VC_CREDIT` 同样名不副实 | 决策登记(文档) |

### Phase 1 —— Stub Cache Core 实现(RN 侧协议状态投影)

| # | 动作 | 依据 |
|---|------|------|
| **P1-1** | **6 接口**(`request` / `snoop` / `snoop_resp` / `writeback` / `completion` + core-side 原语 `evict_line`),命名逐字对齐 `ADR-CEP-02 §2` canonical 10 流 | `ADR-CEP-05` 决策 1 |
| **P1-2** | **5 态状态机**(`NONE → I → S → E → M`,协议无关),**禁止**暴露 CHI UC/SC/UD/SD 七态 | `ADR-CEP-05` §4.1 |
| **P1-3** | **双端口形态**:`cpu_side`(功能操作,阻塞式)与 `net_side`(CohIf RN 投影 5 条)分离;hit 路径零网络流量 | `ADR-CEP-05` 决策 2 + `INV-SC6` |
| **P1-4** | **6 条 `INV-SC1` ~ `INV-SC6`** 行为断言 —— 用 **mock ProtocolEngine** 驱动 `net_side`,不引入真实协议引擎 | `ADR-CEP-05` §4.3 |
| **P1-5** | **折叠映射表的单向形态**断言:stub 的接口面**不出现** `forward_data` / `back_invalidate` / `barrier` 字样(静态断言) | `ADR-CEP-05` 决策 3 |

### 已有(本 change **继承**不重做)

| 项 | 状态 |
|---|------|
| `ADR-CEP-05` 的接口 / 状态机 / `INV-SC*` 定义 | ✅ 2026-10-21 签发(架构师阶段)—— 本 change 是其**实现**,不改其定义句 |
| `ADR-CEP-09` 的顶点升级要求 | ✅ 队列 #2 已实跑验证(per `openspec/specs/cep-poc-spike-verification/spec.md`) |
| `ADR-CEP-12` 的 6 类词表与分层纪律 | ✅ 已机器化(lint 第 10 组)—— 本 change **不动** |

### 冻结面

**2 个成员**(均**仅追加** `## Status Update` 段):

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | 声明 §10 的 `-base` / `-impl` 二分已被 `obj §4.0` 重排取代 + M-4 决策结论摘要(见「Acceptance → A-P0-7」) |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | 队列 #3 已启动(KPI 并发 0/3 → 1/3)+ 上一段的「当前态」改为「当时态」(数值保留,仅状态措辞) |

**零改动**:`include/tlm/cep/fabric.schema.json`(其 `vc_count.maximum = 4` **不需要动** —— 权威已排除「扩 VC 数」与「扩 `CATEGORY_*` 到 6 类」两条路,4 桶 / 3 类原样保持)、`include/tlm/cep/fabric.json`、`openspec/specs/cep-core-contracts/spec.md`、`openspec/specs/cep-poc-spike-verification/spec.md`、其余 `ADR-CEP-*` 与 `07 号`。

---

## Non-Goals

1. **不扩 `CATEGORY_*` 到 6 类** —— 回执 §8.4.3 已判**不可接受**(非「两方案待选」):保持 `vc_counts_[4]` 会让 `atomic`/`query` 被**静默折叠**进 `VC_SNP`;扩 `vc_counts_[6]` 与「ARM CHI 4 通道」物理模型矛盾。
2. **不改 `VC_COUNT` / 不动 `fabric.schema.json` 的 `vc_count.maximum` / 不触发该项的冻结授权闸** —— 权威侧 NS-R2 已锁 `compute_noc` / `ualink_fabric` `vc_count=4`,本仓 `fabric.json` 是派生副本。
3. **不交付「6 类一致性消息类 → `CATEGORY_*` 的显式映射表」** —— 归**队列 #6**(`cpptlm-cep-chi-rnf-subset`,S1-5 表体 + contract test)。本 change **不重造**该表。
4. **不改 `chip_select = (vc << 4) | src` 的 wire 编码契约**,不改 `route_flit()` 的路由 / 推送逻辑。
5. **不删不改名 `VC_DAT` 常量与 `vc_dat_count()`** —— `SocInternalNoc` 属**已归档** e1 change 的产物,弃用标注会制造「归档件契约 ↔ 现行行为」漂移。
6. **不实现** `ADR-CEP-05 §4.2` 的「不实现」清单:完整 SRAM 阵列 + 多路组相联细节 + 复杂预取器 + 精确功耗模型 + 完整流水线时序。
7. **不做真实 ProtocolEngine** —— 仓内首个协议实例在队列 #6;本 change 用 **mock** 驱动 `net_side`。
8. **不引入任何 `status != "planned"` 的协议实例声明** —— 会虚假触发 S1-2 / S1-3(per `ADR-CEP-12` `INV-CEP-12-IC3`)。
9. **不写 Traffic Generator / bypass 模式** —— ADR-CEP-05 §5 与决策 4 的归属待定(见 design.md Open Questions);本 change 的 `cpu_side` 由测试夹具驱动。

---

## Acceptance

> **红绿分离**(per 队列 #2 确立的纪律):「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本节条款**不**因结论为红而失败。

### Phase 0 —— M-4 契约

| # | 条款 | 机器可验 |
|---|------|---------|
| **A-P0-1** | M-4 决策**可寻址**:design.md 含逐条编号的裁定(协议层 / 传输层 / 语义正名 / 冲突输入 / 桶名 / 冻结面),每条带 `file:line` 或权威章节号 | 人验(文本检索) |
| **A-P0-2** | `soc_internal_noc.hh` 的 `VC_DAT` 注释**不再**声称「统计 credit 流量」,且**明写**「预留 DAT 桶 / 当前恒 0」 | 机器(新 lint 断言) |
| **A-P0-3** | `test_soc_internal_noc_data_flow.cc` 的陈旧注释(`vc_counts_[2]` 仍增)已修正 —— 该用例**本就不存在**该断言,修正后断言集合不变 | 机器(新 lint 断言 + 断言集合等值) |
| **A-P0-4** | **正控(判据①「输入改变时输出必须改变」)**:注入一个 `CATEGORY_CREDIT` flit → `vc_dat_count()` **必须**从 0 变为 1。**纯负向断言(「恒 0」)没有正控即为无牙** | 机器(ctest 断言) |
| **A-P0-5** | **冲突输入规则**已在 `soc_internal_noc.hh` 注释中显式化(`vc_counts_` 归 `flit_category` / `chip_select` 归 `vc_id` / 传输层不仲裁) | 机器(新 lint 断言) |
| **A-P0-6** | **零改动校验**:冻结面除 `ADR-CEP-05` 外**全部 0 diff**;`fabric.schema.json` 的 `vc_count.maximum` 仍为 4 | 机器(冻结清单比对) |
| **A-P0-7** | `ADR-CEP-05` 追加 `## Status Update`,声明 §10 的 `-base` / `-impl` 二分已被 `obj §4.0` 重排取代(§10 **正文不改**) | 机器(冻结清单比对 + 文本检索) |
| **A-P0-8** | **lint 第 10 组不回退**:①(恰 3 类 + 值域连续 0..2)与 ③(`VC_COUNT=4` + 「4 独立 VC」字样)**仍绿**;②(幽灵类别)仍绿 —— 且新增断言须配**反向 fixture**(变异必红 + GREEN 样本) | 机器(`cep_gate_selftest`) |

### Phase 1 —— Stub Cache Core

| # | 条款 | 机器可验 |
|---|------|---------|
| **A-P1-1** | **6 接口签名**存在且命名逐字对齐 `ADR-CEP-05` 决策 1 的 6 行表 | 机器(ctest / 静态断言) |
| **A-P1-2** | **5 态可达**:从 `NONE` 出发,`I` / `S` / `E` / `M` 四态均可达;**接口面不出现** CHI UC/SC/UD/SD 字样 | 机器(ctest + 静态断言) |
| **A-P1-3** | **`INV-SC1` ~ `INV-SC6`** 六条**各至少有 1 条**可执行断言,且断言的是**行为**不是「常量存在」 | 机器(ctest 覆盖统计) |
| **A-P1-4** | **折叠映射单向形态**:stub 的接口面**不出现** `forward_data` / `back_invalidate` / `barrier` 字样 | 机器(静态断言) |
| **A-P1-5** | **零协议实例声明**:本 change 的产物**不引入**任何 `status != "planned"` 的协议实例声明 | 机器(声明文件扫描) |
| **A-P1-6** | **不可验证条款已降级登记**:ADR-CEP-05 §3 的契约测试 #1(`forward_data` 不可分辨性)与 #4(barrier 不可见)因需真实 ProtocolEngine **不可在本仓验证** → 登记「触发条件 = 队列 #6」,**不进本 change 的 spec** | 人验(登记存在) |
| **A-P1-7** | **Scenario ↔ 锚点等值**:spec 的每个 Scenario 都点名一个 ctest 用例或 lint 断言;对账测试断言「Scenario 数 == 锚点数」 | 机器(对账测试) |

### 收尾

| # | 条款 |
|---|------|
| **A-FIN-1** | `openspec validate cpptlm-cep-stub-cache-core --strict` rc=0 |
| **A-FIN-2** | `cep_docs_sync_check.sh --strict` / `cep_doc_ref_check.sh --strict` / `cep_freeze_check.sh` 全 rc=0;`cep_gate_selftest` 全绿 |
| **A-FIN-3** | `ctest` 全绿;`[cep]` 标签的 case 数与断言数**逐项登记**(迁移前后对比,断言语义变化须**预登记**) |
| **A-FIN-4** | 归档时按 `obj §4.7.3` 的 17 个落点**逐项**回填;delta capability 合并进 main spec 后**按 Entry #56 先例**纳入冻结集(走 `--update --authorized-by`) |

---

## Capabilities

### New Capabilities

#### 1. `cep-vc-mapping-contract` —— 传输层 `CATEGORY_*` → VC 桶映射的可回归契约

传输层 NoC 的 flit 类别与物理 VC 桶的映射关系,及其**已知边界**(零流量桶、冲突输入不仲裁),必须是**可回归的契约**而非仅存于注释 —— 否则 stub cache 实现期的新断言会固化错误语义。

#### 2. `cep-stub-cache-core` —— Stub Cache Core(RN 侧协议状态投影)的接口与行为契约

`ADR-CEP-05` 决策 1 ~ 4 + §4.1 状态机 + §4.3 六条 `INV-SC*` 的**可执行**契约;接口命名逐字对齐 `ADR-CEP-02 §2` canonical 10 流。

### Modified Capabilities

**无** —— 本 change **不修改**任何既有契约(per `ADR-CEP-12` `INV-CEP-12-IC3` 与 `openspec/specs/cep-core-contracts/spec.md` 零改动)。

---

## Impact

### 影响范围

| 层 | 影响 |
---|---|
| **传输层 NoC** | `include/tlm/cluster/soc_internal_noc.hh` —— **仅注释**(桶语义正名 + 冲突输入规则显式化);**零逻辑改动**、**零 API 改动**、**零常量改名** |
| **测试** | `test/test_soc_internal_noc_data_flow.cc` —— **仅注释**修正;新增 Phase 0 正控用例 + Phase 1 契约测试 |
| **新增 stub** | `include/tlm/cep/stub_cache_core.hh`(+ 实现)—— Phase 1 产物;**不注册 23 ABI**、**不进 `ModuleFactory`**(与 stub 的 testbench 定位一致) |
| **守门** | `scripts/test/cep_docs_sync_check.sh` 第 10 组 —— **新增断言**(Phase 0 语义正名 + 冲突输入规则),**既有 3 条不回退** |
| **跨仓 ArchForge** | **零改动** —— 本 change 服从 `32 号 §6.0.1` 排除表与 `22-nsa §0.5.4.1` NS-R2,不要求上游回改 |
| **KPI** | 0/3 → **1/3**(本 change 独占 1 slot) |
| **23 ABI / frozen headers** | **零改动** |

### 风险

| 风险 | 缓解 |
|------|------|
| **Phase 1 的 spec 写出永假验收条款**(声称可自动化却无自动化 —— 本仓已发生过的真实失败模式,per 队列 #2 背景) | A-P1-6 显式降级登记;spec 的 Scenario **只收本仓可执行**判据,每条点名锚点;A-P1-7 断言 Scenario 数 == 锚点数 |
| **Phase 0 的「恒 0」断言无正控 → 无牙**(判据③) | A-P1-4 强制正控:注入 `CATEGORY_CREDIT` flit → 计数必须 0→1 |
| **stub cache 顺手扩 `CATEGORY_*` / 提前建显式映射表** → 越权到队列 #6 | Non-Goals ①③ + A-P1-5;lint 第 10 组① 持续守护 |
| **`ADR-CEP-05 §10` 的两个旧名永久失指** | A-P0-7 追加 Status Update(§10 正文不改,走冻结授权闸) |
| **8 周估时与本 change 的实际范围脱节** | Phase 0 与 Phase 1 的边界在 design.md 逐条划清;Phase 1 不可验证项走 A-P1-6 降级 |

### 复评节奏

- **Phase 0 完成后**即触发一次复评(决策是否需要 `ADR-CEP-12` / `ADR-CEP-09` 追加 Status Update —— 按裁决结论,默认**不需要**)。
- **Phase 1 完成后**按 `ADR-CEP-05 §3` 的契约测试优先级(必做前 2 条 + 高优先级 8 条)逐条核对。

---

**最后**: 2026-10-25 planner 阶段 —— 5 段草稿已写;**change 目录已建**(`openspec/changes/cpptlm-cep-stub-cache-core/`:proposal + design + 2 个 delta capability`cep-vc-mapping-contract` / `cep-stub-cache-core`);**M-4 行为侧决策已落**(design.md D2~D9);`openspec validate --strict` rc=0;KPI 0/3 → **1/3**;per 台账 Entry #58。**下一个 change = 队列 #4 `cpptlm-cep-directory-ordering-plane`**。
