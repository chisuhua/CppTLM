# cpptlm-cep-cxl-nonsymmetric-state-spec: CXL 2.0 非对称 —— **规约层**(状态机 + `perm` 投影 + 非对称结构断言)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#10** 的**第 0 个 change**(两段式;第 1 个 = 实现层 `cpptlm-cep-cxl-nonsymmetric`)
> **口径裁决**: 台账 **Entry #122** + `ADR-CEP-11` 的 `## Status Update(2026-11-01)`(7 条裁决)
> **硬门产物**: `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`(Task 0 公开来源普查 ⇒ **有条件通过**)
> **规划输入**: `.rddf/improvements/cpptlm-cep-cxl-nonsymmetric.md`(2026-11-01 修订版,10 大项 / 16 编辑点)
> **估时**: 4 周(规约层)= **1 周普查硬门(已完成)** + 3 周载体 / 摘要核对表 / 生成器 / 编译期锁
> **KPI**: Stage 1 = **1 slot**;Proposed 并发 **0/3 → 1/3**
> **冻结面**: **零正文改动**(`07 号` 不碰)。**仅两件 ADR 追加 `## Status Update`(纯追加,正文零改动)**:`ADR-CEP-11`(口径裁决,经台账 **Entry #122** 两步授权)· `ADR-CEP-04`(R11 归属复核,经 **Entry #124** 两步授权)⇒ provenance 已对齐

---

## Why

### ① 口径已裁决(不再有「自推」空间)

| 裁决 | 内容 |
|---|---|
| 语义子集 | 「CXL 2.0 非对称」= **CXL.cache** 子协议(**host-centric / 单一一致性点**)· **不含** CXL.mem 池化 · **不含** CXL 3.0 DCOH/BISnp/BIRsp |
| 态数 | **MESI 4 态**(M/E/S/I)⇒ `perm` 投影 = **4→4** ⇒ ⚠️ **「5→4」作废**(MESIF 的 `F` 态模板形状) |
| 事件划分 | **D2H Req = 设备/local**;**H2D Req = 只有 snoop/remote**(发起方恒为 Home)⇒ 两集合不相交 |
| basis 权威 | **本仓「公开来源锚点摘要」**(既非规范正文、亦非 ArchForge)· 强度三档**不得升格** · 追认 = **仓内**二方复核闭环 |
| 合规 | ⛔ **禁止**把 CXL 规范正文投喂任何 AI ⇒ 8 项细粒度语义走「**人类成员条款摘要**」 |
| 落点 | CXL 实例 = **CEP 探索实例**,显式「**≠ dGPU SoC 实现路径**」;**不镜像**进 ArchForge `32 号 §6.1` |
| 演进轴 | 「CXL 3.0 对称」指 **HDM-DB / CXL.mem 的多 host 共享内存** ⇒ **CXL.cache 全代次都非对称** |

### ② 为什么仍需**规约层**这一层(而不是直接写实现)

#9 的教训:缺了**可执行规约载体**,实现层必然**自推**(「看似完整实则杜撰」)。
⇒ 本 change 交付的是**载体**(转换表 + 投影 + 结构断言 + 逐格 `basis`),**不是** adapter。
⇒ 与 #9 的**关键差异**:CXL 的 basis 来自**公开标准** ⇒ **无跨仓异步往返** ⇒ 周期更短、风险面更小。

### ③ Task 0 硬门已过(有条件)

公开来源普查(`docs/research/2026-11-01-...-anchor-summary.md`)结论:
**态数 / 事件词表 / 投影形状 / 非对称定义 / 编译期断言** 五项**已钉死** ⇒ 允许建档;
**8 项**细粒度语义(`R-1~R-8`)= `pending` ⇒ **不阻塞**载体结构,但**逐格**必须标 `pending` + `ref`。

---

## What Changes

| 载体 | 内容 |
|---|---|
| `include/tlm/cep/cxl_state_spec.hh`(新) | CXL.cache **4 态** / **事件词表**(枚举名,不写死编码)/ **转换表** / **N→4(=4→4) `perm` 投影** / 逐格 `basis{kind, pending, ref, note}` / 偏离清单 / **编译期锁** + **非对称结构级断言** |
| `docs/research/<date>-cep-cxl-nonsymmetric-anchor-summary.md` | ⚠️ **已落**(2026-11-01)—— basis 的**唯一人工审计面**;载体 `ref` **只指向本文件的条目号** |
| `scripts/test/generate_cxl_anchor_checklist.sh` + `.cc`(新) | **锚点摘要的逐格核对表生成器**(+ `--check`,手改必红)—— ⚠️ **对象不是**「发给 ArchForge 的追认请求件」 |
| `test/test_cep_cxl_state_spec.cc`(新) | 契约测试 + **反向 fixture**(把 `F` 行加回 ⇒ 必红;毒化结构断言 ⇒ 必红) |
| `openspec/changes/cpptlm-cep-cxl-nonsymmetric-state-spec/`(新) | 本 change 4 件套 + delta spec(capability `cep-cxl-nonsymmetric-state-spec`) |

### 明确不做(本 change 内)

- ❌ **不**实现 adapter / `SparseSnoopFilter` 行为类 / 实例声明 / 装配(⇒ 第 1 个 change)
- ❌ **不**扩 `perm` 4 值锁 · **不**扩 `CATEGORY_*` 3 类 · **不**改 `CohIfBundle` · **不**改冻结 schema · **不**新增 ABI
- ❌ **不**新增 invariant 编号数(除非 ADR 显式新增并同步编号位)
- ❌ **不**建模 transient state · **不**预支 CXL 3.0 的 BI 通道
- ❌ **不**把规范正文投喂任何 AI · **不**镜像进 ArchForge `32 号 §6.1`
- ❌ **不**产出任何性能结论(CXL 非对称 workload 落在 H7 声明范围**之外** ⇒ R-B 失效区)

---

## Acceptance

| # | 判据 | 机器可判方式 |
|---|---|---|
| **AC-1** | 载体态集合**恰为 4 态**且 `perm` 投影**恰为 4 行**;**`F` 行不存在** | 编译期锁 + 反向 fixture(加回 `F` 行 ⇒ 必红) |
| **AC-2** | **非对称结构断言**成立:`H2DReqOp ⊆ {SnpInv, SnpData, SnpCurr}` ∧ `D2HReqOp ∩ H2DReqOp = ∅` ∧ 设备侧**无** snoop **发起**型事件 | 编译期 `static_assert` + 毒化任一条 ⇒ 必红 |
| **AC-3** | 每格 `basis` **非空**且 `kind`/`pending` **一致**;`ref` 指向**锚点摘要条目号**(不指向规范原文);**每格 ∈ 三态** {`Known`/`Illegal`/`Unknown`},`Unknown` **显式区分**于 `Illegal`;`pending` 格 `note` **带 R-1~R-8 编号** | `--check` 比对载体 ↔ 摘要 ⇒ 手改任一侧必红;**反 vacuity**:全格 `pending` ⇒ 必红 |
| **AC-4** | 强度标注**不得升格**:`analogical` / `inferred` 未被标为 `public-direct` | 生成器校验 + 反向 fixture |
| **AC-5** | 载体**头注释**含「**≠ dGPU SoC 实现路径**」消歧声明 | 文本断言(正向)+ 反向 fixture(删掉 ⇒ 必红) |
| **AC-6** | **8 项残余风险**(R-1~R-8)在载体与摘要中**逐项登记**且状态一致 | 清单比对 ⇒ 缺项/多项必红 |
| **AC-7** | **冻结面零正文改动**:`07 号` / `openspec/specs/*` 零改动;⚠️ **例外(经授权)**:`ADR-CEP-11`(+49)/ `ADR-CEP-04`(+45)**仅追加 Status Update**,分别经台账 **Entry #122 / #124** 两步授权 | `git diff` 断言(仅追加段)+ `cep_freeze_check` provenance 通过 |
| **AC-8** | 门禁全绿:13 项 CEP 脚本 + `docs_sync --strict` + `openspec validate --strict` + `format --check` + `ctest -R cep` | 退出码 |
| **AC-9** | **转换表覆盖率可判且单调**:载体声明 `kLegalTransitionCoverageNum/Den`,测试断言该比值与**逐格实际统计相等**(防虚报);台账每次推进复核**单调不减**(回退 ⇒ 必红 + 须 entry 授权) | 断言 + 台账复核 |
| **AC-10** | **两轴不混读**:`pin=weak` 的格可 `public-direct` 但 `pending=true`;`pin=transcribed` 的格**降** `public-analogical`;`pending=false` 须「`pin=strong` ∧ 许可已人工确认」 | 生成器校验 + 反向 fixture |

⇒ ⚠️ **判据纪律**:每条**纯负向**断言(「`F` 行不存在」「不得升格」)都有**正控**(「`F` 行存在 ⇒ 必红」的反向 fixture)
⇒ **因正确原因红**,不因超时/副作用红。

---

## Capabilities

| # | 能力 | 说明 |
|---|---|---|
| **1** | **CXL 2.0 非对称规约载体** | 4 态 / 事件词表 / 转换表 / 4→4 投影 / 逐格 `basis` / 偏离清单 / 编译期锁 —— **本仓可执行的规约真源** |
| **2** | **非对称的结构级可判性** | local/remote 事件集划分 + 3 条编译期断言 ⇒ 「非对称」从**标签**变成**结构事实** |
| **3** | **basis 审计闭环** | 公开来源锚点摘要(唯一人工审计面)+ 逐格核对表生成器 + 仓内二方复核 ⇒ **不产生跨仓债**,也不产生「结构完备、语义空洞」 |

---

## Impact

| 面 | 影响 |
|---|---|
| **队列 / KPI** | #10 **第 0 个 change 转正**;Proposed 并发 **0/3 → 1/3** |
| **协议实例** | **零改动**(实例声明属第 1 个 change) |
| **SharerTracker** | **零改动**(行为实现属第 1 个 change;`ADR-CEP-04 §2` 的 caps 契约**只消费不重推**) |
| **冻结面(M1, 40 件)** | **零改动** ⇒ **无需两步授权**(口径已由 `ADR-CEP-11` Status Update 承载,per Entry #122) |
| **跨仓债** | **零**(追认 = 仓内二方复核)⇒ **无 #9 式 35 格异步往返** |
| **残余风险** | ① 8 项细粒度语义 = `pending`(R-1~R-8)② 公开来源的强度天花板(SF 判定规则只能类比)③ 形式化模型假设 host perfect tracking ⇒ 不得作 SF 精度依据 ④ **钉版缺口待补**(sha256 / Wayback 快照 / D14 SHA)——**载体逐格编码须等它完成** ⑤ **不得产出性能结论**(R-B 失效区) |
