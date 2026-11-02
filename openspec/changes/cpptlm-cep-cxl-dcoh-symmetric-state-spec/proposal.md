# cpptlm-cep-cxl-dcoh-symmetric-state-spec: CXL 3.0 对称(DCOH)—— **规约层**(3 套状态空间 + 6 通道 + `{BISnp}` 判别式)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#12** 的**第 0 个 change**(规约层)
> **口径**: 台账 **Entry #154**(4 项 architect 裁决 + 冻结面授权)· **#153**(Task 0 普查登记)· **#122**(CXL 口径裁决)
> **规划输入**: `.rddf/improvements/cpptlm-cep-cxl-dcoh-symmetric.md`(2026-11-02,HARD-GATE rc=0)
> **审计面**: `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(**basis 唯一人工审计面**)
> **估时**: 5 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 = **本 change**(规约层)· 第 1 个 = `cpptlm-cep-cxl-dcoh-symmetric`(实现层)
> **冻结面**: 预期 **0 件**(载体是**新增**头文件;`protocol_instance.schema.json` 零改动)—— **须 Phase 0 实测确认**

---

## Why

### ① 平台缺的是**第四种一致性拓扑**,不是「又一个 adapter」

已落地的 5 个实例全部是 **host-centric 或中心化目录**语义(仅 `tilelink_c` 是树形)
⇒ CXL 3.0 对称(DCOH)是**设备侧自带一致性代理 + SF/目录、可对 host 发起反向 snoop**的第四种
⇒ 不覆盖它,就答不了「中心化目录 vs 设备侧代理」这一档取舍(§0 Goal 的核心命题)。

### ② `DcohSymmetric` 的 traits **已承诺、实现面零兑现**

`ADR-CEP-04 §13` 已登记第 6 种 tracker;`AggregationAt` 枚举已含 `REQUESTER`;
而 `scope=LOCAL_MEMORY_REGION` / `authority=LOCAL_HINT` **只有 CXL 3.0 的设备侧代理用得上**
⇒ **声明面已承诺、实现面零兑现** ⇒ 与 R11 同型(队列 #10 的 `SparseSnoopFilter` 已同款先例)。

### ③ Task 0 已过 ⇒ 语义**不必自推**

普查(Entry #153)把 5 项载体形态决定项中的 **4 项**钉死 ⇒ 规约层**只消费**证据
⇒ 避免 #9 式「看似完整实则杜撰」。

### ④ 但公开来源**反证了一处既有 ADR 决策** ⇒ 必须先裁决、不能带着错值上路

`ADR-CEP-04 §3.5` 的 `REQUESTER` 分支被一手复核反证(per Entry #154 ③)⇒ 裁决已落(聚合点恒为 `HOME`)
⇒ 载体**可锁** `aggregation_at == HOME` ⇒ 若无裁决,载体会锁错值。

### ⑤ 「对称」极易被写错 ⇒ 必须钉成**可机械判定**的谓词

公开来源明写 CXL 3.0 **不满足**经典 symmetric ⇒ 载体若写成「设备之间可互发反向 snoop / 设备缓存彼此内存」
⇒ **杜撰** ⇒ 必须钉窄义 **5 谓词合取**。

---

## What Changes

| 面 | 改动 |
|---|---|
| **载体(新增)** | `include/tlm/cep/cxl_dcoh_state_spec.hh`(header-only):**3 套状态空间**(`DevCacheState` 4 / `DcohViewState` 3 / `HomeDirState` 3,**不得合并**)+ CXL.mem **6 通道**(枚举名,**不写死 opcode**)+ CXL.cache 双向各 3 + ⭐ `device_initiated_snoop_set == {BISnp}` + `perm` **仅**从缓存代理域 **4→4** 投影 + 窄义 **5 谓词**「对称」定义 + 逐格 `basis{kind, pending, ref→锚点摘要条目号}` |
| **判据** | 编译期结构断言(4/3/3 态 · 6 通道 · `{BISnp}` · `perm_rows == 4`)+ 逐格 basis 核对表 + **反向 fixture**(毒化 ⇒ 必红 + 归因命中) |
| **零改动** | `ADR-CEP-*` 正文 · `protocol_instance.schema.json`(值级枚举已含 `REQUESTER` ⇒ 零改动)· `CxlCacheAdapter` · `cxl_state_spec.hh`(队列 #10 载体)· canonical 10 流清单 · **0 新增** ABI · **0 新增** invariant 编号数 |
| **登记** | 10 项 `pending`(R-S1 ~ R-S9)逐条入载体 `basis` ⇒ **禁止推理填空** |

> ⚠️ **反向 fixture 形态**:与既有 `*_reverse_verify.sh` **同型**(「毒化 ⇒ 判据**必红**」= 鉴别力)
> ⇒ 与队列 #11 修**误报**的那一组(「毒化 ⇒ 仍绿」)**形态相反** ⇒ **两者都要登记**,避免后人混用。

---

## Acceptance

| # | 判据 | 形式 |
|---|---|---|
| **CDS-01** | **3 套状态空间** SHALL **不合并** | 编译期断言:三套的**元素集合互不相交**且各自基数 == 4 / 3 / 3 |
| **CDS-02** | CXL.mem 通道 SHALL 恰 **6**、CXL.cache SHALL 双向各 **3** | 编译期断言(以**枚举名**表达,**不写死 opcode**) |
| **CDS-03** | `device_initiated_snoop_set` SHALL == **`{BISnp}`** | 编译期断言(非空 + size == 1)· **反向 fixture**:置空 ⇒ 必红 |
| **CDS-04** | `perm` 投影 SHALL **仅**来自 `DevCacheState` 且为 **4→4** | 编译期断言:`perm_rows == 4` · `F` 行不存在 · DCOH/home 的 3 态**未**进入 `perm` |
| **CDS-05** | 「对称」SHALL 钉为窄义 **5 谓词**合取 | 载体侧谓词常量 + 反向 fixture(把某谓词置假 ⇒ 判别式不成立 ⇒ 必红) |
| **CDS-06** | 逐格 `basis` SHALL 带 `kind` / `pending` / `ref` | 逐格核对表 + **反向 fixture**(去掉 `pending` 标记 ⇒ 必红) |
| **CDS-07** | **10 项 `pending`** SHALL 逐条登记 | 载体 `basis` 中逐条可寻址 · `ref` 指向**锚点摘要条目号**(**不**指向规范原文) |

⇒ `ctest` 全绿 · 门禁全 rc=0 · `openspec validate --strict` rc=0 · `format.sh --check` rc=0。

---

## Impact

| 面 | 影响 |
|---|---|
| **代码(新增)** | `include/tlm/cep/cxl_dcoh_state_spec.hh`(header-only)· `test/test_cep_cxl_dcoh_state_spec.cc` |
| **代码(既有)** | **零改动** |
| **冻结面** | 预期 **0 件**(Phase 0 实测确认) |
| **契约** | **0 新增** ABI · **0 新增** invariant 编号数 · `perm` **4 值锁不变** |
| **KPI** | 1 slot(串行 ⇒ 规约层归档后才建实现层) |
| **风险 ①** | ⚠️ **`perm` 折叠** —— DCOH 的 `A` 是**集合**(host 可为 M/E/S/I)⇒ 须**显式**表达「不可单值投影」 |
| **风险 ②** | ⚠️ **「对称」写成 peer 全对称** ⇒ 须钉 5 谓词 + 反向 fixture |
| **风险 ③** | ⚠️ **10 项 pending 被推理填空** ⇒ 逐格 `basis` 必带 `pending` + `ref→锚点摘要` |
| **风险 ④** | ⚠️ **合规** —— **任何时候不得**把 CXL 规范正文投喂 AI;pending 项走「人类成员条款摘要」 |
