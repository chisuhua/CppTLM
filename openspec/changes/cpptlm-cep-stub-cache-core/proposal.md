# cpptlm-cep-stub-cache-core: Stub Cache Core(RN 侧协议状态投影)+ M-4 传输层 VC 映射契约闭合

> **状态**: 🟡 **Proposed**(2026-10-25 建目录;planner 阶段产出 proposal + design + 2 个 delta capability)
> **队列**: `obj-cep-platform §4.0` 队列 **#3**(Stage 1)· 前序 #1 `cpptlm-cep-umbrella` / #2 `cpptlm-cep-poc-spike` 均已 ✅ archived(完成归档语义)→ **KPI 并发 0/3 → 本 change 占 1 slot**
> **形态**: **双 Phase 单 change** —— Phase 0 = M-4 决策落盘(先于任何新断言);Phase 1 = stub cache core 实现。**不拆 change**(裁决 2026-10-25)
> **关联 ADR**: `ADR-CEP-05`(Stub Cache Core 接口 + 状态机 + `INV-SC*`)+ `ADR-CEP-04`(Sharer Tracker 抽象)+ `ADR-CEP-02`(canonical 10 流,命名逐字对齐)+ `ADR-CEP-12`(6 类词表与分层纪律)+ `ADR-CEP-09`(per-network 顶点)+ `ADR-CEP-14`(契约测试)
> **跨仓权威(只读)**: ArchForge `32 号 §6.0.1`(6 类词表 + `CATEGORY_CREDIT` 排除表)/ `32 号 §6.0.2`(per-network VC 分配矩阵)/ `22-nsa §0.5.4.1`(网络清单,NS-R2 单点权威)
> **承接**: 队列 #2 归档时移出的 **M-4 行为侧决策**(孤儿前置,已按台账 Entry #57 的 `⏭(孤儿)` 标记登记在 `obj §4.0` / `§4.2` 两处)
> **planning 输入**: `.rddf/improvements/cpptlm-cep-stub-cache-core.md`(5 段草稿)

---

## Why

Stage 1 的**首个可实现实体**至今只有纸面契约:`ADR-CEP-05` 已把 6 接口 / 5 态状态机 / 折叠映射表 / 6 条 `INV-SC*` 全部定义完毕,但仓内**无任何实现**。后续 5 个队列(#4 ~ #8)的准入判据(`ADR-CEP-09` 死锁自由 / `ADR-CEP-13` 域可见性)都需要一个**可运行的 stub** 作为观测对象 —— 没有它,这些判据只能停留在纸面辩论。

同时,**M-4 行为侧决策**至今未闭,且现状语义经实测被证明是**错的**:

| 实测事实 | 证据 |
|---|---|
| `VC_DAT` 桶在**所有真实路径中恒为 0** | 唯一写 `CATEGORY_CREDIT` 的点 `include/framework/bidirectional_port_adapter.hh:235` 的 flit 经 `resp_out_port_` 发出,**不经** `SocInternalNoc::route_flit()`;`SocInternalNoc` 入向流量只写 `CATEGORY_REQUEST` / `CATEGORY_RESPONSE` |
| 现有注释**描述了一个从未发生的流量** | `include/tlm/cluster/soc_internal_noc.hh:179-181` 自陈「`vc_dat_count()` 实际统计的是 credit 流量」;实测该 accessor 外部消费者 **0 命中** |
| 「credit 是否独占 VC」**已被跨仓权威关闭** | ArchForge `32 号 §6.0.1` 排除表:credit 是**流控类别**,**随所在 VC 的流一起走,不单独占 VC**;本仓镜像 `include/tlm/cep/protocol_instance.schema.json:131` |

⇒ **M-4 的真问题不是「credit 放哪」,而是「命名说谎 + 索引对齐的隐式性」**。若不先闭合,stub cache 实现期产生的新断言会**固化**这一错误语义。

**为什么在同一个 change 里**:M-4 是 stub cache 的准入前置(stub 的第一行新断言会消费传输层 VC 语义);stub cache 是 M-4 的观测载体(只有真实 stub 产生流量,「`VC_DAT` 恒 0」这类断言才有意义)。拆开会让 M-4 在「没有真实消费者」的真空里被裁决。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `openspec/specs/cep-vc-mapping-contract/spec.md`(delta) | **capability `cep-vc-mapping-contract`** —— 传输层 `CATEGORY_*` → VC 桶映射的**可回归契约**(含零流量桶边界 + 冲突输入不仲裁规则) |
| `openspec/specs/cep-stub-cache-core/spec.md`(delta) | **capability `cep-stub-cache-core`** —— Stub Cache Core 的 6 接口 + 5 态状态机 + `INV-SC1` ~ `INV-SC6` 的**可执行**契约 |
| `include/tlm/cep/stub_cache_core.hh`(+ 实现) | **Phase 1 产物** —— RN 侧协议状态投影;**不注册 23 ABI**、**不进 `ModuleFactory`**(testbench 定位) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/tlm/cluster/soc_internal_noc.hh` | **仅注释** —— `VC_DAT` 桶语义正名(预留桶 / 当前恒 0)+ 冲突输入规则显式化。**零逻辑改动、零 API 改动、零常量改名** |
| `test/test_soc_internal_noc_data_flow.cc` | **仅注释** —— 修正 `:108` 陈旧注释(该用例**本就不存在** `vc_counts_[2]` 断言) |
| `scripts/test/cep_docs_sync_check.sh` | 第 10 组**新增**断言(Phase 0 语义正名 + 冲突输入规则);**既有 3 条不回退** |
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | 追加 `## Status Update`(声明 §10 的 `-base`/`-impl` 二分已被 `obj §4.0` 重排取代);**§10 正文不改** |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 状态列 📋 → 🟡 · §4.0/§4.2 承接列修正(S1-4 已消费 → 移除)· §11 追加 Entry |
| `.rddf/roadmap/phases/phase-10.md` | 对应子任务行状态同步 |
| `.rddf/improvements/cpptlm-cep-stub-cache-core.md` | front-matter `状态:` → Proposed(已写)· 「最后」行 |

### 冻结面

**2 个成员改动**(均**仅追加** `## Status Update` 段;走 `cep_freeze_check.sh --update --authorized-by "Entry #58"` 两步授权):

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | §14 —— 声明 §10 的 `-base`/`-impl` 二分已被 `obj §4.0` 重排取代 + M-4 决策结论摘要 |
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | 队列 #3 已启动(KPI 并发 0/3 → 1/3)+ 上一段的「当前态」改为「当时态」(数值保留,仅状态措辞) |

**零改动**:`include/tlm/cep/fabric.schema.json`(其 `vc_count.maximum = 4` **不需要动** —— 权威已排除「扩 VC 数」与「扩 `CATEGORY_*` 到 6 类」两条路,4 桶 / 3 类原样保持)、`include/tlm/cep/fabric.json`、`openspec/specs/cep-core-contracts/spec.md`、`openspec/specs/cep-poc-spike-verification/spec.md`、其余 `ADR-CEP-*` 与 `docs/architecture/07-coherence-exploration-platform.md`。

---

## Capabilities

### New Capabilities

- **`cep-vc-mapping-contract`** —— 传输层 `CATEGORY_*` → VC 桶映射**必须是可回归的契约**:① 映射关系与每个桶的**当前流量实况**可被断言;② 零流量桶的「恒 0」**必须有正控**(注入 → 计数必变);③ `vc_id` 与 `flit_category` 冲突时的归属规则**显式且可断言**;④ 传输层**不仲裁**该冲突(仲裁属 adapter 层)。
- **`cep-stub-cache-core`** —— Stub Cache Core(RN 侧协议状态投影)**必须是可执行的契约**:① 6 接口签名与命名逐字对齐 `ADR-CEP-02 §2` canonical 10 流;② 5 态状态机(`NONE → I → S → E → M`)可达且**不暴露** CHI 七态;③ `INV-SC1` ~ `INV-SC6` **各至少 1 条**行为断言(断言行为,不断言「常量存在」);④ 折叠映射表的**单向形态**可静态断言。

### Modified Capabilities

**无** —— 本 change **不修改**任何既有契约(per `ADR-CEP-12` `INV-CEP-12-IC3` 与 `openspec/specs/cep-core-contracts/spec.md` 零改动)。

### 契约元信息(因 delta spec 只保留 `## ADDED Requirements` 段内内容,以下登记在此)

| 项 | 内容 |
|---|---|
| **红绿语义** | 「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本 change 的条款**不**因结论为红而失败 |
| **消费者** | `cep-vc-mapping-contract` → `test/test_cep_vc_mapping_contract.cc`(ctest)+ `cep_docs_sync_check.sh` 第 10 组断言 ④⑤⑥ · `cep-stub-cache-core` → `test/test_cep_stub_cache_core.cc`(ctest) |
| **不可验证条款的处置** | `ADR-CEP-05 §3` 契约测试 #1(`forward_data` 不可分辨性)与 #4(barrier 不可见)需**真实 ProtocolEngine**(仓内首个协议实例在队列 #6)→ **不进本 change 的 spec**,改以「触发条件 = 队列 #6」登记(per design.md D10 + Open Questions Q2) |

---

## 契约锚点对照表

> A-P1-7 要求:每个 Scenario 点名一个可执行锚点。**锚点已内嵌在各 Scenario 末尾**(归档合并进 main spec 后仍保留,供对账测试读取)。

| capability | Scenario 数 | 锚点数 | 分布 |
|---|---|---|---|
| `cep-vc-mapping-contract` | 8 | 8 | lint 第 10 组断言 ①④⑤⑥ + `test_cep_vc_mapping_contract.cc` |
| `cep-stub-cache-core` | 9 | 9 | `test_cep_stub_cache_core.cc` |

---

## Impact

### 影响范围

| 层 | 影响 |
|---|---|
| **传输层 NoC** | `soc_internal_noc.hh` —— 仅注释;零逻辑 / 零 API / 零常量改名 |
| **新增 stub** | `include/tlm/cep/stub_cache_core.hh` —— Phase 1;不进 `ModuleFactory`、不注册 ABI |
| **跨仓 ArchForge** | **零改动** —— 服从 `32 号 §6.0.1` 排除表与 `22-nsa §0.5.4.1` NS-R2,不要求上游回改 |
| **23 ABI / frozen headers** | **零改动** |
| **KPI** | 0/3 → **1/3** |

### Non-Goals

1. 不扩 `CATEGORY_*` 到 6 类(回执 §8.4.3 已判**不可接受**,非待选)。
2. 不改 `VC_COUNT`、不动 `fabric.schema.json` 的 `vc_count.maximum`、不要求 ArchForge 上游回改。
3. **不交付**「6 类一致性消息类 → `CATEGORY_*` 的显式映射表」—— 归**队列 #6**;本 change 不重造。
4. 不改 `chip_select = (vc << 4) | src` 的 wire 编码契约;不改 `route_flit()` 路由 / 推送逻辑。
5. 不删不改名 `VC_DAT` 常量与 `vc_dat_count()`(已归档 e1 产物的 public surface)。
6. 不实现 `ADR-CEP-05 §4.2` 的「不实现」清单(SRAM 阵列 / 多路组相联 / 预取器 / 功耗模型 / 流水线时序)。
7. 不做真实 ProtocolEngine —— 用 mock 驱动 `net_side`。
8. 不引入任何 `status != "planned"` 的协议实例声明(防虚假触发 S1-2 / S1-3)。
9. 不写 Traffic Generator / bypass 模式(归属待定,见 design.md Open Questions)。

### 风险与缓解

| 风险 | 缓解 |
|------|------|
| spec 写出**永假验收条款**(声称可自动化却无自动化 —— 本仓已发生过的真实失败模式) | spec 的 Scenario **只收本仓可执行**判据,每条点名锚点;对账测试断言「Scenario 数 == 锚点数」 |
| 「恒 0」断言无正控 → 无牙 | 强制正控:注入 `CATEGORY_CREDIT` flit → 计数必须 0→1 |
| stub 顺手扩 `CATEGORY_*` / 提前建显式映射表 → 越权到队列 #6 | Non-Goals + lint 第 10 组① 持续守护 |
| `ADR-CEP-05 §10` 的两个旧名永久失指 | 追加 Status Update(§10 正文不改) |

---

## Acceptance

> **红绿分离**(per 队列 #2 确立):「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本节条款**不**因结论为红而失败。

### Phase 0 —— M-4 契约

- [ ] **A-P0-1** M-4 决策**可寻址**:design.md 含逐条编号裁定(协议层 / 传输层 / 语义正名 / 冲突输入 / 桶名 / 冻结面),每条带 `file:line` 或权威章节号
- [ ] **A-P0-2** `soc_internal_noc.hh` 的 `VC_DAT` 注释**不再**声称「统计 credit 流量」,且**明写**「预留 DAT 桶 / 当前恒 0」
- [ ] **A-P0-3** `test_soc_internal_noc_data_flow.cc` 陈旧注释已修正,且该用例的**断言集合不变**(本就不存在 `vc_counts_[2]` 断言)
- [ ] **A-P0-4** **正控**:注入 `CATEGORY_CREDIT` flit → `vc_dat_count()` 必须 0→1(纯负向断言无正控即为无牙)
- [ ] **A-P0-5** **冲突输入规则**已显式化(`vc_counts_` 归 `flit_category` / `chip_select` 归 `vc_id` / 传输层不仲裁)
- [ ] **A-P0-6** **零改动校验**:冻结面除 `ADR-CEP-05` 外全部 0 diff;`fabric.schema.json` 的 `vc_count.maximum` 仍为 4
- [ ] **A-P0-7** `ADR-CEP-05` 追加 `## Status Update`(§10 正文不改)
- [ ] **A-P0-8** **lint 第 10 组不回退**:①③ 仍绿;② 仍绿;新增断言配**反向 fixture**(变异必红 + GREEN 样本)

### Phase 1 —— Stub Cache Core

- [ ] **A-P1-1** **6 接口签名**存在,命名逐字对齐 `ADR-CEP-05` 决策 1 的 6 行表
- [ ] **A-P1-2** **5 态可达**(`I`/`S`/`E`/`M`);接口面**不出现** CHI UC/SC/UD/SD 字样
- [ ] **A-P1-3** `INV-SC1` ~ `INV-SC6` **各至少 1 条**行为断言(断言行为,不断言「常量存在」)
- [ ] **A-P1-4** **折叠映射单向形态**:接口面**不出现** `forward_data` / `back_invalidate` / `barrier` 字样
- [ ] **A-P1-5** **零协议实例声明**:不引入任何 `status != "planned"` 的声明
- [ ] **A-P1-6** **不可验证条款已降级登记**:`ADR-CEP-05 §3` 契约测试 #1(`forward_data` 不可分辨性)与 #4(barrier 不可见)登记「触发条件 = 队列 #6」,**不进本 change 的 spec**
- [ ] **A-P1-7** **Scenario ↔ 锚点等值**:每个 Scenario 点名一个 ctest 用例或 lint 断言;对账测试断言「Scenario 数 == 锚点数」

### 收尾

- [ ] **A-FIN-1** `openspec validate cpptlm-cep-stub-cache-core --strict` rc=0
- [ ] **A-FIN-2** `cep_docs_sync_check.sh --strict` / `cep_doc_ref_check.sh --strict` / `cep_freeze_check.sh` 全 rc=0;`cep_gate_selftest` 全绿
- [ ] **A-FIN-3** `ctest` 全绿;`[cep]` 的 case 数与断言数**逐项登记**(断言语义变化须**预登记**)
- [ ] **A-FIN-4** 归档时按 `obj §4.7.3` 的 17 个落点逐项回填;delta capability 合并进 main spec 后按 Entry #56 先例纳入冻结集(走 `--update --authorized-by`)
