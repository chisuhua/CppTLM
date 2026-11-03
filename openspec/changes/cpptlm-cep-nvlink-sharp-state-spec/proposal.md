# cpptlm-cep-nvlink-sharp-state-spec: NVLink SHARP —— **规约层**(分类判据 + 区域/epoch 纪律 + 可观测性 + ordering + 逐格 `basis`)

> **状态**: 🟡 **Proposed**(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 的**第 0 个 change**(规约层)
> **口径**: 台账 **Entry #174**(critic 第 2 轮收口)· **#173**(Task 0 普查完成)· **#172**(improvement 订正)· **#171**(VC 空间裁决 **Decision D**)· **#169 / #170**(三则 ADR 定稿 + critic/oracle 修复)
> **规划输入**: `.rddf/improvements/cpptlm-cep-nvlink-sharp.md`(2026-11-03,HARD-GATE rc=0)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(**basis 唯一人工审计面**)
> **估时**: 5 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 = **本 change**(规约层)· 第 1 个 = `cpptlm-cep-nvlink-sharp`(实现层)
> **冻结面**: 预期 **0 件**(载体是**新增**头文件;`collective_planes.json` / `collective_planes.schema.json` 的 Decision D 改动**已在 `2ea09607` 落地**)—— **须 Phase 0 实测确认**

---

## Why

### ① 平台缺的是**网内归约这一档语义**,不是「又一个 adapter」

已落地的 6 个实例全部是 **host-centric / 中心化目录 / 树形**语义 ⇒ 覆盖不到
**「归约发生在网络交换机里」**这一形态 —— 而这正是 `ADR-CEP-01` 把 collective 提为**旁路平面**的动机
(§0 Goal 的核心命题之一:同一地址在 coherence 与 collective 之间如何分域)。

### ② `BroadcastNull` 的 traits **已承诺、实现面零兑现**

`ADR-CEP-04 §2` 的 caps 表已登记 `BroadcastNull`(第 4 个 tracker 实现)· `ADR-CEP-04 §13` 明禁「克隆 + 抄一份 caps」;
而 `BroadcastNull` **至今无行为类** ⇒ **声明面已承诺、实现面零兑现** ⇒ 与 R11 同型
(队列 #10 的 `SparseSnoopFilter`、队列 #12 的 `DcohSymmetric` 均同款先例)⇒ 本队列**顺带闭合**。

### ③ 但 SHARP 的语义**必须钉窄**,否则必然写成杜撰

公开来源明写 NVLink SHARP(`NVLS`)是**节点内**归约(跨节点才转 CollNet)⇒ 若载体写成
「NVSwitch 可为整个 fabric 归约」⇒ 与来源冲突 ⇒ **能力域必须钉为 intra-node**。

### ④ 公开来源**不支持**的部分必须显式留空,不能推理填空

Task 0 普查(Entry #173)把 **7 项**细粒度语义判为**无公开来源支撑**(reduce 算子穷举集 / 逐操作支持矩阵 /
epoch 状态机 / VC 编号 / 聚合树构建算法 / 报文格式 / 数值精度)⇒ 载体若把它们填成具体值 ⇒ **杜撰**。

### ⑤ 而 VC 这一项**本仓当前无解** ⇒ 只能以「显式声明」表达,不能以「预留表」表达

两个网络的 `vc_id` 0-3 **全被 coherence 占满**;扩 VC 是**跨仓债**(`ADR-CEP-09` 已判)⇒ Decision D(Entry #171)
把 CP-03 从「空预留 ⇒ 默认 PASS(恒真)」改成「**空预留 ⇒ 必须显式声明 unavailable**」⇒
本队列的载体必须登记**预留状态**,而不是**预留表**。

---

## What Changes

| 面 | 改动 |
|---|---|
| **载体(新增)** | `include/tlm/cep/nvlink_sharp_state_spec.hh`(header-only):**R1/R2 分类判据**(opcode 永不作分类器 · 唯一分类点 = fabric ingress)+ **R3/R4 区域互斥与 epoch 纪律**(注册三步原子 · epoch 未 commit 前**放行** · 注销失效化)+ **R5 可观测性**(divert 计数器 + fail-stop 断言)+ **ordering A/B/C/D** 的可判定谓词 + **集合 VC 预留状态**(Decision D 的 `unavailable` 四字段声明)+ 逐格 `basis{kind, pending, ref→锚点摘要条目号}` |
| **判据** | 编译期结构断言(分类点唯一性 · epoch 双向 · 三步原子 · 四类 ordering · 预留状态)+ 逐格 `basis` 核对表 + **反向 fixture**(毒化 ⇒ 必红 + **归因命中**) |
| **零改动** | `ADR-CEP-*` 正文 · `protocol_instance.schema.json` · `collective_planes.json` / `collective_planes.schema.json`(**Decision D 已落**)· canonical 10 流清单 · **0 新增** ABI · **0 新增** invariant 编号数 |
| **登记** | **7 项 `pending`**(N-1 ~ N-7)逐条入载体 `basis` ⇒ **禁止推理填空** |
| **同步** | `ADR-CEP-01` / `02` / `03` **零改动**(契约面已于 Entry #169 / #170 / #171 交割) |

> ⚠️ **反向 fixture 形态**:与既有 `*_reverse_verify.sh` **同型**(「毒化 ⇒ 判据**必红**」= 鉴别力)
> ⇒ 与「毒化 ⇒ 仍绿」那一组**形态相反** ⇒ **两者都要登记**,避免后人混用。
> ⚠️ **归因纪律**(per critic 第 2 轮 S7 的同款教训):每条反向 fixture SHALL **报出可归因的 id**
> ⇒ **不得**只以「整体 FAIL」充当判据。

---

## Acceptance Criteria

| # | 判据 | 形式 |
|---|---|---|
| **NSS-01** | **R1/R2 分类判据** SHALL 钉住「opcode 永不作分类器」+「唯一分类点 = fabric ingress」 | 载体提供纯函数 `classify_at_fabric_ingress`;**源端侧 SHALL NOT** 持有区域表可见性(静态扫描)· **反向 fixture**:让源端按「是否 SHARP 区」改 opcode ⇒ 必红 |
| **NSS-02** | **R4 epoch 语义 SHALL 双向成立** | 未 commit ⇒ **放行**;已 commit ⇒ **divert** · **反向 fixture**:把「未 commit ⇒ 拒绝」写回 ⇒ 必红(⚠️ **本仓已错过一次**,improvement 草稿写反 ⇒ 本条是回归锁) |
| **NSS-03** | **R3 注册 SHALL 为三步原子** | 登记 + drain + 建 epoch;**drain 失败 ⇒ 注册失败且不建 epoch** · **反向 fixture**:drain 失败仍建 epoch ⇒ 必红 |
| **NSS-04** | **R4 注销 SHALL 失效化并回归 coherence 域** | 区域退出表 ⇒ 后续 coherence 请求**正常服务** · **反向 fixture**:注销后仍 divert ⇒ 必红 |
| **NSS-05** | **R5 可观测性 SHALL 落地** | divert 计数器 + **fail-stop 断言**(epoch commit 后仍有 coherence atomic 命中已注册区域 = error) · **反向 fixture**:该情形不报错 ⇒ 必红 |
| **NSS-06** | **跨平面 ordering A/B/C/D** SHALL **可判定** | 四类可见性次序各一例 + **反向 fixture**:把某类判反 ⇒ 必红 |
| **NSS-07** | **集合 VC 预留 SHALL 登记为「状态」而非「表」** | 载体 SHALL 断言 `status == "unavailable"` + 四字段齐全;**SHALL NOT** 断言任何独立 VC 编号 · **反向 fixture**:填入具体 VC 编号 ⇒ 必红 |
| **NSS-08** | **能力域 SHALL 钉为节点内** | 载体 SHALL 断言 collective 能力域 = intra-node(依据 A-1 / A-5);**SHALL NOT** 声明跨 fabric 归约 · **反向 fixture**:把能力域改为跨 fabric ⇒ 必红 |
| **NSS-09** | **逐格 `basis` SHALL 带两轴 + 7 项 `pending` 逐条登记** | 每格带 `kind` / `pending` / `ref→锚点摘要条目号`;**7 项** pending 逐条可寻址且 **SHALL NOT** 出现其具体取值的断言 · **反向 fixture**:去掉 `pending` 标记 ⇒ 必红 |

⇒ `ctest` 全绿 · 门禁全 rc=0 · `openspec validate --strict` rc=0 · `format.sh --check` rc=0。

---

## Impact

| 面 | 影响 |
|---|---|
| **代码(新增)** | `include/tlm/cep/nvlink_sharp_state_spec.hh`(header-only)· `test/test_cep_nvlink_sharp_state_spec.cc` |
| **代码(既有)** | **零改动** |
| **冻结面** | 预期 **0 件**(Phase 0 实测确认) |
| **契约** | **0 新增** ABI · **0 新增** invariant 编号数 · canonical 10 流清单**零 diff** |
| **KPI** | 1 slot(串行 ⇒ **规约层归档后**才建实现层) |
| **风险 ①** | ⚠️ **能力域写成跨 fabric** ⇒ NSS-08 锁节点内(A-1 / A-5) |
| **风险 ②** | ⚠️ **epoch 语义写反** ⇒ NSS-02 双向锁(本仓已错过一次) |
| **风险 ③** | ⚠️ **7 项 pending 被推理填空** ⇒ 逐格 `basis` 必带 `pending` + `ref→锚点摘要` |
| **风险 ④** | ⚠️ **VC 被写成「预留独立 VC」** ⇒ NSS-07 锁「状态」+ Decision D |
| **风险 ⑤** | ⚠️ **合规** —— **任何时候不得**把 NVLink SHARP 正式规范正文投喂 AI;pending 项走「人类成员条款摘要」流程 |
| **风险 ⑥** | ⚠️ **纸面契约** —— 规约层若每格 `basis` 都是 `pending` ⇒ 本 change 自身即纸面契约 ⇒ Phase 2 SHALL 统计**非 pending 格数 ≥ 下限**(见 tasks 2.6) |
