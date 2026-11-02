---
name: cpptlm-cep-cxl-dcoh-symmetric
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-04
  - ADR-CEP-02
  - ADR-CEP-11
  - ADR-CEP-12
  - ADR-CEP-17
状态: 🟡 **进行中**(2026-11-02)—— 队列 #12;**两段式**;Task 0 硬门已过(有条件,per Entry #153)· architect 4 项裁决已落(per Entry #154)· **第 0 个 change(规约层)**`cpptlm-cep-cxl-dcoh-symmetric-state-spec` **已完成归档**(per Entry #159;main spec = `openspec/specs/cep-cxl-dcoh-symmetric-state-spec/spec.md`)· **第 1 个 change(实现层)**`cpptlm-cep-cxl-dcoh-symmetric` **已建档**(per Entry #160)
created: 2026-11-02
---

# improvement: cpptlm-cep-cxl-dcoh-symmetric

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 🟡 进行中(2026-11-02)· **队列 #12**(`obj-cep-platform §4.0` · Stage 2 · **2029-Q3**)
> · **两段式**(照 #9 / #10 / #11 先例):第 0 个 change = `cpptlm-cep-cxl-dcoh-symmetric-state-spec`(**规约层**)
> + 第 1 个 change = `cpptlm-cep-cxl-dcoh-symmetric`(**实现层**)
> · **Task 0 硬门**:✅ **有条件通过**(2026-11-02,per 台账 **Entry #153**)——
> 普查产物 `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(12 条来源 · sha256 钉版 · **零规范正文暴露**)
> · **architect 4 项裁决**:✅ **已落盘**(per 台账 **Entry #154**)——
> `ADR-CEP-04` **第 3 则 SU**(聚合点二元收敛为 `HOME`)+ `07 号` **第 19 则 SU**(6 处口径订正 + adapter 形态裁决)
> · **KPI**: Stage 2 = **1 slot**(并发上限 1,per `obj §5.4`;当前实测 **0/3**)
> · **估时**: 规约层 5 周 + 实现层 8 周(共 13 周;**串行** —— 实现层以规约层为前置)
> · **⚠️ 估时高于队列 #10(12 周)**:本队列的**状态空间是 3 套**(非 1 套)且**多一个 tracker 行为实现**

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-04` §3.5 + **第 3 则 SU**(2026-11-02) | CXL 对称模式的 `aggregation_at` 二元 | ⚠️ 原二元「P2P ⇒ `REQUESTER` / home ⇒ `HOME`」的 **`REQUESTER` 分支被公开来源反证** ⇒ **现行口径 = 聚合点恒为 `HOME`**;`P2P` / `HDM-home` **降级为「场景标签」** ⇒ 载体**可锁** `aggregation_at == HOME` |
| `ADR-CEP-04` §2 caps 表 · §4 hazard 表 · §13 | `DcohSymmetric` 的 caps 6 字段 | `scope=LOCAL_MEMORY_REGION` / `completeness=CONSERVATIVE` / `authority=LOCAL_HINT` / `aggregation_at=HOME` + `hazard=STALL` + `broadcast_fallback`(**待 pending**)⇒ 逐字段的公开来源核对见普查 §6 #4 |
| `ADR-CEP-02` | canonical 10 流 + `perm` 4 值锁(CEN-Ib11) | `perm` **维持 4 值**;**仅**从 CXL.cache 缓存代理域(4 态)投影 ⇒ DCOH 视图域 / home 目录域的 3 态**不得**折进 `perm` |
| `ADR-CEP-11` | §3 锁表 · Status Update ①-c / ④ | **版本锁定**是 basis 的**唯一人工审计面**的前提;本队列的版本 = **CXL 3.0** |
| `ADR-CEP-12` | §3 schema · §11 `perm` 静默升级 · §13 声明权威 | 新实例声明的 13 键 schema;⚠️ 本族**是否**暴露 `UNIQUE` 决定是否需 `perm_upgrades.json` 一行 ⇒ **规约层裁决**(照 #10 / #11 的「不暴露 ⇒ 不声明」先例) |
| `ADR-CEP-17` | §2 Ruby-SLICC 族 · 边界声明 | 「对称」**不得**表述为 peer 全对称 ⇒ 本平台的**窄义**定义(5 谓词合取) |
| 锚点摘要(唯一人工审计面) | `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md` | §1 设备侧稳定态集合(**3 套状态空间**)· §2 事件词表与 local/remote(**6 通道** + ⭐ `{BISnp}` 判别式)· §3 `perm` 投影(**按域分离**)· §4 「对称」的**可判定义**(5 谓词)· §5 编译期断言可锁结构 · §7 10 项 pending · §10 硬门判定 |
| 台账 Entry #153 / #154 | Task 0 普查 + architect 4 项裁决 | 普查**有条件通过**(4/5 项可结构钉死)⇒ 4 项裁决已落 ⇒ **开工阻塞解除** |
| 台账 Entry #122 | CXL 口径裁决 | 「CXL 3.0 对称」指 **HDM-DB / CXL.mem 的多 host 共享内存** ⇒ 本轮公开来源**印证** |
| 队列 #10 先例 | `cpptlm-cep-cxl-nonsymmetric`(规约层 + 实现层) | **两段式** + **同一 adapter 家族的两代** + 「`SparseSnoopFilter` 行为实现归实现层」⇒ 本队列的 `DcohSymmetric` 行为实现**同款归属** |

---

## 范围

### In Scope

- **第 0 个 change(规约层)** `cpptlm-cep-cxl-dcoh-symmetric-state-spec`:
  `include/tlm/cep/cxl_dcoh_state_spec.hh`(新 · header-only)—— **3 套状态空间**(`DevCacheState` 4 / `DcohViewState` 3 / `HomeDirState` 3,**不得合并**)
  + CXL.mem **6 通道**(枚举名,**不写死 opcode`)+ ⭐ `device_initiated_snoop_set == {BISnp}` 判别式
  + `perm` **仅**从缓存代理域 4→4 投影 + 窄义 5 谓词「对称」定义 + 逐格 `basis{kind, pending, ref→锚点摘要条目号}`
- **第 1 个 change(实现层)** `cpptlm-cep-cxl-dcoh-symmetric`:
  `include/tlm/cep/cxl_dcoh_adapter.hh`(新 · **独立 adapter**,per architect 裁决 #4)+ `DcohSymmetric` 行为实现
  + `cxl_3_0_symmetric` 实例声明(13 键)+ 装配期校验
- 判据:正向 + **反向 fixture**;`AC-05` 族无关化复用(本族若无 unique-clean 态 ⇒ **SKIP**)
- 登记:`DcohSymmetric` 的**行为语义基源**缺口(07 号「残留 A」)⇒ 本队列**首次**为该实现提供 basis

### Out of Scope

- ❌ **不改** `ADR-CEP-02` / `ADR-CEP-04` 的**正文**(裁决以 **Status Update 追加**,已在 Entry #154 完成)
- ❌ **不改** `CxlCacheAdapter`(`cxl_2_0` 载体**零改动** —— 对称模式走**独立 adapter**)
- ❌ **不扩** `perm` 4 值 / **不新增** ABI / **不新增** invariant 编号数
- ❌ **不建模** transient 态 / 错误语义 / poison / viral(per 普查 R-S6 / R-S7 ⇒ **只登记**)
- ❌ **不写死** opcode / 字段宽度 / credit / 排序 / block 粒度 / SF 精度(per R-S1 / R-S2 / R-S5)
- ❌ **不**把 DCOH / home 目录的 3 态折进 `perm`(它们是 **tracker view / coherence metadata**)
- ❌ **不**触碰 CXL **规范正文**(ECA §3 + AI AUP)⇒ 10 项 pending 走「**人类成员条款摘要**」
- ❌ **不产出**任何性能结论 · **不**做 protocol engine

---

## Why

### ① 平台缺的不是「又一个 adapter」,而是**第三种一致性拓扑**

已落地的 5 个实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `tilelink_c`)**全部**是
**host-centric 或中心化目录**语义 —— 只有 `tilelink_c` 是**树形**。
CXL 3.0 对称(DCOH)是**第四种**:设备侧**自带**一致性代理 + SF/目录,**可对 host 发起反向 snoop**
⇒ 平台不覆盖它,就答不了「**中心化目录 vs 设备侧代理**」这一档取舍 —— 而这恰是 dGPU SoC 里
「加速器自带 snoop filter」这一现实方案的**直接对应**(per §0 Goal 的核心命题)。

### ② 现有抽象**已为它留好位置**,但**零使用**

`SharerTrackerIf` 的 `DcohSymmetric` traits 行**已登记**(per `ADR-CEP-04 §13`),`AggregationAt` 枚举
**已含** `REQUESTER`,而 `scope=LOCAL_MEMORY_REGION` / `authority=LOCAL_HINT` 这组 caps
**只有 CXL 3.0 的设备侧代理**用得上 ⇒ **声明面已承诺、实现面零兑现**
⇒ 与 R11(「声明超前于实现」)**同型** ⇒ 队列 #10 的 `SparseSnoopFilter` 已是同款先例。

### ③ Task 0 已过 ⇒ 语义**不必自推**

公开来源普查(Entry #153)已把 5 项载体形态决定项中的 **4 项**钉死
(4/3/3 态 · 6 通道 · `{BISnp}` 判别式 · `perm` 按域分离)
⇒ 规约层**只消费**证据、不自推 ⇒ 避免 #9 式「看似完整实则杜撰」。

### ④ 但公开来源**反证了一处既有 ADR 决策** ⇒ 必须先裁决、不能带着错值上路

`ADR-CEP-04 §3.5` 的 `REQUESTER` 分支被一手复核反证(architect 一手复核 + sha256 重算,per Entry #154 ③)
⇒ 若不裁决,规约层会**锁错值** ⇒ 裁决已落(聚合点恒为 `HOME`)⇒ 载体**可锁**。

### ⑤ 「对称」这个词**极易**被写错 ⇒ 必须钉成可机械判定的谓词

公开来源明写 CXL 3.0 **不满足**经典 symmetric(所有节点实现同一套一致性算法)
⇒ 若载体把它写成「设备之间可互发反向 snoop / 设备缓存彼此内存」⇒ **杜撰**
⇒ 必须钉成窄义 **5 谓词合取**(含「snoop 目标是 host 而非 peer device」)。

---

## What Changes

| 面 | 第 0 个 change(规约层) | 第 1 个 change(实现层) |
|---|---|---|
| **载体** | `cxl_dcoh_state_spec.hh`:**3 套状态空间** + 6 通道 + `{BISnp}` 判别式 + 4→4 投影 + 5 谓词定义 + 逐格 `basis` | `cxl_dcoh_adapter.hh`:**独立 adapter**,**零自有表副本**,**委托**规约载体(照 #11 的 `TileLinkCAdapter` 零副本范式) |
| **tracker** | — | `DcohSymmetric` **行为实现**(traits 已登记 ⇒ 本队列**首次**兑现)⇒ 闭合 07 号「残留 A」 |
| **实例** | — | `cxl_3_0_symmetric`(13 键)+ `repo_owned_instance_ids` + `perm_upgrades.json`(若本族**不暴露** `UNIQUE` ⇒ **不声明**,照 #11 先例) |
| **判据** | 编译期结构断言(4/3/3 态 · 6 通道 · `{BISnp}` · `perm_rows == 4`)+ 逐格 basis 核对表 | 正向 + **反向 fixture**(毒化 ⇒ 必红 + 归因命中)· 装配期校验 |
| **零改动** | `ADR-CEP-*` 正文 · `protocol_instance.schema.json`(值级枚举已含 `REQUESTER` ⇒ **零改动**)· `CxlCacheAdapter` · canonical 10 流清单 | 同左 + `CxlCacheAdapter` |

> ⚠️ **反向 fixture 的形态**:与既有 `*_reverse_verify.sh` **同型**(「毒化 ⇒ 判据**必红**」= 鉴别力)
> ⇒ 与队列 #11 修的**误报**场景**形态相反**(那一组是「毒化 ⇒ 仍绿」)⇒ **两者都要登记**,避免后人混用。

---

## Acceptance

- [x] **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`
      ⇒ **有条件通过**(4/5 项可结构钉死;**零规范正文暴露**)
- [x] **architect 4 项裁决**落盘(per Entry #154):聚合点二元 ⇒ `HOME` · `DcohSymmetric` traits ⇒ `HOME` ·
      07 号 **6 处**口径订正 · adapter 形态 ⇒ **独立 `CxlDcohAdapter`**
- [ ] **第 0 个 change(规约层)**:3 套状态空间**不合并** + 6 通道(枚举名)+ `{BISnp}` 判别式 + `perm` 仅 4→4(缓存代理域)+ 5 谓词定义 + 逐格 `basis`(全部 `pending` 标注 + `ref` 指向锚点摘要条目号)
- [ ] **第 0 个 change 的反向 fixture**:毒化任一格 ⇒ 编译期锁**必红** + 归因命中(铁律 ②)
- [ ] **第 1 个 change(实现层)**:独立 adapter **零自有表副本**(静态扫描 + 鉴别力自测)+ `DcohSymmetric` 行为实现 + 实例声明 13 键 + `AC-05` 族无关化(SKIP 或 FAIL,**不得**静默 PASS)
- [ ] **10 项 pending** 逐条登记在载体 `basis` 中,**禁止**推理填空
- [ ] **冻结面**:若规约层触及 `cep-sharer-tracker-base` ⇒ 台账 Entry + **两步授权**;`protocol_instance.schema.json` **零改动**
- [ ] **验收**:`ctest` 全绿 + 门禁全 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0

---

## Capabilities

| capability | 内容 | 状态 |
|---|---|---|
| `cep-cxl-dcoh-symmetric-state-spec` | 对称模式的**规约层**契约:3 套状态空间 + 6 通道 + `{BISnp}` 判别式 + `perm` 按域分离 + 5 谓词「对称」定义 | 🟡 新增(第 0 个 change 的 delta spec) |
| `cep-cxl-dcoh-symmetric` | **实现层**契约:`CxlDcohAdapter`(独立 · 零副本)+ `DcohSymmetric` 行为实现 + `cxl_3_0_symmetric` 实例声明 | 🟡 新增(第 1 个 change 的 delta spec) |

---

## Impact

| 面 | 影响 |
|---|---|
| **代码(新增)** | `include/tlm/cep/cxl_dcoh_state_spec.hh` + `include/tlm/cep/cxl_dcoh_adapter.hh`;`include/tlm/cep/sharer_tracker.hh` 的 `DcohSymmetric` **行为类**(traits 已登记) |
| **代码(既有,零改动)** | `CxlCacheAdapter` · `cxl_state_spec.hh` · `protocol_instance.schema.json` · canonical 10 流清单 |
| **代码(已订正)** | `DcohSymmetric` traits 的 `aggregation_at` ⇒ `HOME`(per Entry #154 ④) |
| **测试** | 新增规约层 + adapter + 实例声明三组用例;**反向 fixture** 同型既有脚本 |
| **冻结面** | `ADR-CEP-04` + `07 号`(已由 Entry #154 授权落盘)· 规约层若触及 `cep-sharer-tracker-base` ⇒ 另需授权 |
| **契约** | **0 新增** ABI · **0 新增** invariant 编号数 · `perm` **4 值锁不变** |
| **KPI** | 1 slot(串行 ⇒ 规约层归档后才建实现层)· 7 处 baseline 同步 |
| **风险 ①** | ⚠️ **`perm` 折叠风险** —— DCOH 的 `A` 是**集合**(host 可为 M/E/S/I)⇒ 若误折成单值 ⇒ 语义丢失 ⇒ 载体须**显式**表达「不可单值投影」 |
| **风险 ②** | ⚠️ **「对称」被写成 peer 全对称** ⇒ 载体须钉 5 谓词 + 反向 fixture |
| **风险 ③** | ⚠️ **10 项 pending** 被推理填空 ⇒ 逐格 `basis` 须带 `pending` 标记 + `ref` 指向锚点摘要(**不**指向规范原文) |
| **风险 ④** | ⚠️ **合规** —— 任何时候**不得**把规范正文投喂 AI;pending 项走「人类成员条款摘要」 |

---

## 遗留(本队列**不**处理,仅登记)

- **G-3**(IC3 变更留痕无自动闸)—— 开放,per §8.1 R2
- **B2** `FullMapDirectory` 反向缺口 —— 登记态 · 不排期(per Entry #139 ③)
- **B3** `LocalView` 双重语义 —— 登记态 · 未落点
- **B4** vc3 内 `E` 不被 `D` 饿死 —— 登记态 · 未建模(多 VC 仲裁属 Stage 4+)
- **B6** 对拍基准 **P-3**(容量对齐档)+ **P-5**(定义待补)—— 开放
- **DcohSymmetric` 的 `broadcast_fallback=true`** —— 公开来源**未见依据** ⇒ **保留 + 标 pending**(「未见 ≠ 不存在」)
