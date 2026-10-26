# cpptlm-cep-moesi-gpu-adapter — 设计(Design)

> **阶段**: planner(2026-10-26)· **状态**: 🟡 Proposed · **队列**: `obj-cep-platform §4.0` #6
> **本文件是 planner 阶段的裁决落点** —— 每个 `D-` 段是一次**已拍板**的语义澄清或范围界定;`tasks.md` 属 builder 阶段,**本文件不含任务分解**。

---

## D-α:队列位置 —— 「把 `moesi_gpu` 排进来」本身就是第一产物

**裁决(2026-10-26,DRI 三问)**: ① `moesi_gpu` / `mesif` **排入 Stage 1 前段**,而非追加到全局末尾 ② **S1-7 ①②** 跟 `moesi_gpu` 走 ③ **S1-5** 跟首个 adapter 走。

**新队列**(全局 11 → **13** change / 11 → **13** 季度;终态 2029-Q2 → **2029-Q4**):

| # | Change | Stage | 季度 | 相对原队列 |
|---|--------|-------|------|-----------|
| **6** | `cpptlm-cep-moesi-gpu-adapter` | Stage 1 | 2028-Q1 | **新增**(插在原 #6 之前) |
| **7** | `cpptlm-cep-chi-rnf-subset` | Stage 1 | 2028-Q2 | 原 #6 **顺延** |
| **8** | `cpptlm-cep-mesif-adapter` | Stage 1 | 2028-Q3 | **新增**(排在 #7 之后) |
| **9** | `cpptlm-cep-cxl-nonsymmetric` | Stage 1 | 2028-Q4 | 原 #7 顺延 |
| **10** | `cpptlm-cep-tilelink-tree-manager` | Stage 1 | 2029-Q1 | 原 #8 顺延 |
| #11~#13 | Stage 2 / Stage 3 三项 | — | 2029-Q2 ~ 2029-Q4 | 顺延 |

**为什么不追加到全局末尾**(被否决的选项):那样 `chi-rnf-subset` 仍在 `chi-rnf-subset` 里做 `perm` 演进 ⇒ 消费者与演进分离 ⇒ 违反 `Entry #58` 先例。

**连带的口径变更**(每处都带「命中 1 次」断言):

| 面 | 变更 |
|---|---|
| `obj §4.0` | 13 行;表头 / 口径段 / 终态行 |
| `obj §4.2` | 标题「3 协议 RN-F Subset」→「**5 协议实例**」;顺序号;Stage 1 跨度 **9** 个 change |
| `obj §4.3` / `§4.4` / `§4.5` | 顺序号 + 季度 + 标题 |
| `obj §4.6` | 终态 2029-Q4;「串行 **13** 季度」;保留 Entry #12 的 10 → 11 历史注记 |
| `obj §6.2` | 验收标题季度 |
| `obj §10` | Stage 1~3 远期候选的**顺序指针**(+2 行) |
| `phase-10.md` | 排期真源指针 / KPI 预算(Stage 1 7 → 9)/ 图注 / 依赖图协议实例行 |
| `.rddf/roadmap.md` | 「跨 **13** 季度治理叙事」 |
| **`ADR-CEP-00 §2 决策 3`** | **冻结面** —— Stage 1 名单 7 → 9 + 总计 11 → 13 + §5 决策汇总 + 追加 Status Update(Entry #71) |

> **单源纪律**:具体季度值**只**出现在 objective 内;`phase-10.md` / `.rddf/roadmap.md` / `.rddf/improvements/*.md` 一律只写「见 §4.6」。改完**双边化**自检(新值 + 旧值都查)—— 「旧数复活」检查见 `obj §4.0` 的自检块。

---

## D-β:`perm` 形态演进的落点 —— **独立声明源**,`protocol_instance.schema.json` **零改动**

**裁决**:`perm_upgrade` **不**加进 `protocol_instance.schema.json` 的 13 个顶层属性,而是落**独立声明源** `include/tlm/cep/perm_upgrades.json` + 配套 `perm_upgrades.schema.json`。

**理由(4 条,逐条有据)**:

1. **⑥ 镜像闸会当场红** —— `scripts/test/cep_decl_alignment_check.sh:361` 的 ⑥ 项要求本仓 `protocol_instances.json` 与 ArchForge 声明**逐字段严格相等**(「键只存在于一侧」用 `_MISSING` 哨兵判红)⇒ 给实例加 `perm_upgrade` 键 ⇒ **镜像漂移** ⇒ 红。而 `protocol_instances.json` 的 `mirror_note` 明说它是 **ArchForge 单点权威的派生副本** ⇒ **不得**承载本仓单方新增的字段(否则「派生副本」语义被破坏,⑥ 的守卫对象不再是 ArchForge 声明)。
2. **`perm` 4 值锁的权威在 `ADR-CEP-02`,不在 ArchForge** —— `CEN-Ib11` 说「`perm` 枚举维持 4 值锁」;`ADR-CEP-05 §13.1` 的 `perm` 形态约束(Q-3b 裁定)权威是 **CEP 侧** ⇒ 「协议**是什么**」(ArchForge)与「协议**怎么投影到 stub cache**」(CEP)本就分层 ⇒ 投影声明独立成源**符合既有分层**,不是绕过。
3. **零回归** —— `test/test_cep_schema.cc:234` 断言「schema 必含 **13** 个顶层属性」且 `REQUIRE(s.at("required").size() == groups.size())` ⇒ 加第 14 个属性 ⇒ **两处断言同时红** ⇒ 必然要改断言 ⇒ 改断言 = 改动既有冻结契约的测试面。独立声明源让 `protocol_instance.schema.json` 与其测试**零 diff**。
4. **先例** —— 队列 #5 的 `collective_planes.json` + `collective_planes.schema.json` 就是**独立声明源 + 独立 schema** 先例(且刻意「不碰冻结的 `protocol_instance.schema.json`」)。

**偏离登记**:`ADR-CEP-05 §13.1` 记「`protocol_instance.schema.json` **本批不改**」—— 那「本批」指 2026-10-22 的 A6 批;**本 change 是新批次**,但我们**主动选择仍不改**,理由见上 ⇒ 在 `ADR-CEP-05` / `ADR-CEP-12` 的 Status Update 里**显式登记**该口径(避免后被判为「漏改」)。

---

## D-γ:`perm_upgrade` 的字段形态 —— 顶层**可选**字段 + `silent_upgrades[]` 数组

**裁决**:

```
perm_upgrade: {
  "silent_upgrades": [
    { "from": "UNIQUE", "to": "WRITEBACK" }
  ]
}
```

| 决策点 | 取值 | 理由 |
|---|---|---|
| **加新枚举值?** | ❌ **禁止** | `ADR-CEP-02 CEN-Ib11` 的 `perm` **维持 4 值锁**;加第 5 个值即击穿 ⇒ 本 change **不动** `perm` 字段本体 |
| **改成对象数组**(`perm: [{value, silent_upgrade_to}]`)? | ❌ **不取** | **破坏性**:已有实例声明的 `perm` 是**字符串数组**,改类型 ⇒ 全量重写 + `test_cep_schema.cc` 的 4 值锁断言改写 ⇒ 收益(字段内聚)不抵代价 |
| **顶层字段 vs 字段内嵌?** | **顶层** | ① `perm` 的**语义**是「本 cache 当前持有的权限集合」;`perm_upgrade` 的语义是「**协议能力**(该协议有哪些静默升级路径)」—— 二者是**不同维度**(实例属性 vs 协议属性)⇒ 塞进 `perm` 会把「能力」与「状态」混在一处 |
| **必填 or 可选?** | **可选** | **省略 = 无静默升级能力**,这是**安全默认**(无能力 ⇒ 无域可见写 ⇒ 不会误判)⇒ 现有 2 个实例声明**零改动**;若设为必填 ⇒ `mesif` 也得补 `{"silent_upgrades": []}` ⇒ 又触碰 ⑥ 镜像面 |
| **from/to 的取值域?** | **必须**取自 `protocol_instance.schema.json` 的 `perm` 4 值锁 | 跨 schema `$ref` **声明**该绑定。⚠️ **仓内无 JSON-Schema validator**(实测 `grep -rn jsonschema` 零命中;契约判据是手写 C++)⇒ `$ref` 目前是**声明性**约束,**执行**靠 AC-01 ② 的手写判据 ⇒ 「机制上不可能表达 5 值」的措辞**过强**,实际是「**判据层**不可能」 |
| **泛化布尔 vs 具体 from→to 对?** | **具体 from→to 对** | `ADR-CEP-05 §13.1` 的 Q-3b 裁定:「新字段**必须**表达「`UNIQUE` 可静默升 `WRITEBACK`」这一**具体**语义,**非**泛化布尔(`O→M` 走 `INVALIDATE` 是**显式**升级,触发条件不同)」⇒ 布尔 `silent_upgrade: true` **被明确排除** |
| **权限单调性?** | **要求 `to` 的秩 ≥ `from` 的秩** | 权限秩 `NONE < READ < UNIQUE < WRITEBACK` ⇒ 静默升级**不得**降权限(`UNIQUE → READ` 荒谬)⇒ **可机器判定**(AC-02) |

**`from`/`to` 的秩序权威**:本 change 在 `perm_upgrades.schema.json` 里用 `$ref` 引 `protocol_instance.schema.json` 的 `perm` 枚举 + 在契约判据里**独立**定义秩序(不塞进 schema,因为 JSON Schema 表达不了「秩」)⇒ **两处秩序若不一致,判红**(AC-01 的子项)。

---

## D-δ:6→3 消息类映射表的落点与形态

**裁决**:映射表是**adapter 的属性**,落在 `MoesiGpuAdapter` 内(与「哪个协议发哪类消息」同源),**不**落独立 JSON(因为它**逐 adapter 不同**,不是跨实例共享的声明)。

| 决策点 | 取值 | 理由 |
|---|---|---|
| **映射表放哪?** | adapter 头文件内(静态表) | 逐 adapter 不同 ⇒ 独立 JSON 会变成「一堆只填 1 行的文件」 |
| **映射方向?** | 6 类消息类 → `CATEGORY_*` 3 类 | `ADR-CEP-12 §8.3` ②:6 类在 Coherence Interface 层、`CATEGORY_*` 在链路层、**关系由 adapter 处的显式映射表承担(非恒等)** ⇒ 方向唯一 |
| **行数?** | 每个**该实例实际使用**的 6 类消息类一行 | `32 号 §6.0.2`:某消息类在某网络上缺席是**合法状态**(如 `atomic`/`query` 不在 `compute_noc`)⇒ 映射表按**实例用到的类**填,不强制 6 行 |
| **「非恒等」怎么判?** | **至少一行**的映射目标 **≠** 同名 `CATEGORY_*` | 「非恒等」是**映射表存在的前提**(恒等表 = 无需映射表)⇒ 但**不**要求每行都非恒等(可能恰好某两类同名)⇒ 判据形态:**存在行 ≠ 恒等** |
| **恒等的实例怎么办?** | **显式声明恒等 + 给出理由**,不静默放过 | 「恒等」是**合法状态**,但必须**被登记**(判据层:恒等实例须携带 `identity_reason`)⇒ 否则映射表会退化成「恒等表也判过」 |
| **`CATEGORY_*` 会扩吗?** | ❌ **不扩** | `ADR-CEP-12 §8.3` ②的分层纪律 + `32 号:1055` ArchForge 已撤回扩类请求 ⇒ 本 change **只读** `CATEGORY_*` 的 3 类 |

---

## D-ε:6×6 状态机表的落点 —— adapter 头文件内自含,**不做**跨仓逐格镜像

**裁决**:6 态 × 6 事件表**直接实现**在 `moesi_gpu_adapter.hh` 内(表驱动,per ArchForge `09 号` §3 的 `state_transition[6][6]` 形态);**不**在本仓另存一份「ArchForge 规约副本」并做逐格镜像闸。

**理由**:
1. **跨仓镜像闸的对象是「声明数据」,不是「实现」** —— `cep_decl_alignment_check.sh` 的 ⑤⑥ 两项都对**声明**(网络集 / 实例字段 / 边表)做镜像;**6×6 表在本 change 里是实现产物**(本仓从零实现,ArchForge 是纯设计仓零代码)⇒ 把实现也纳入镜像 = 要求「实现 == 规约」,**任何实现差异**(如本仓的 `T` 态收敛策略)都会被误判为漂移。
2. **过度设计** —— 逐格镜像需要跨仓 markdown 代码块解析器(ArchForge 的 6×6 表是 markdown 代码块,不是 JSON fence —— 与 §6.0.3 边表的处境**完全相同**,而那个已用「散文表格 ⇒ 解析器读不到」的方式处理)⇒ 成本高、收益低。
3. **真正的漂移风险已被现有闸覆盖** —— 关键的**唯一**静默升级转换(`E+Write→M` = `state_transition[2][1] == 3`)由 **AC-05** 断言;若 ArchForge 改了状态机 ⇒ 该断言红 ⇒ 人工介入。**不追求全表逐格自动镜像,只锁住语义关键点**。

> **登记的取舍**:「本仓实现与 ArchForge 规约的全表一致性」**不**由机器保证,只由**代码评审 + 关键点断言**保证 ⇒ 该缺口显式登记(不掩盖)。

---

## D-ζ:`status` 翻转 = **条件达成**;⑥ 项排除 `status` 字段(守卫收窄)

**问题**:S1-2 的触发条件是「首个 `implemented` 实例」⇒ 本 change 交付了可跑的 adapter ⇒ `protocol_instances.json` 的 `moesi_gpu.status` 应改 `implemented`。但 ⑥ 逐字段镜像闸会因该差异判红(ArchForge 侧仍是 `planned`)。

**裁决**:**`status` 是本仓权威字段,不参与镜像比对**。

| 判据 | 取值 | 理由 |
|---|---|---|
| **`status` 的语义** | 「计数口径的唯一依据:**仅 implemented 计入 adapter 实例数**」(`protocol_instance.schema.json` 的字段 description)⇒ 计数的是**本仓**的 adapter 实例数 ⇒ 权威**在本仓** | ArchForge 说「我没实现插件」(`32 号:311`)是真的,但**本仓实现了**也是真的 ⇒ 两者**不是同一个事实** |
| **⑥ 项的处理** | 逐字段比对时**跳过 `status`** 键;其余 **12** 键仍严格相等 | 收窄守卫 ⇒ **必须有反向 fixture**:改 `protocol_version` / `vc_binding` / `perm` 等任一非 `status` 键 ⇒ 仍必红 |
| **跨仓同步** | **不作为前置** | ArchForge 侧 `status` 何时翻转是**它自己的**进度;本仓不因它阻塞。若跨仓已翻转 ⇒ 两侧仍相等 ⇒ 无冲突 |
| **条件达成机制** | 若实施窗口内 ArchForge **未**翻转 ⇒ 本 change 的 S6 降级为「装配校验在 `planned` 下跑通 + 状态翻转**留待**跨仓同步后一步完成」⇒ **S1-2 记为「条件达成」而非「已达成」** | 诚实登记,不假装跨仓已同步 |

---

## D-η:`mesif` 的处理 —— 本 change **不碰**

**裁决**:`mesif` 排 **#8**,本 change **零触碰**(声明源 / adapter / schema 均不动 `mesif`)。

**理由**:
1. **R-1 阻塞**:ArchForge 侧 `mesif` 的 `sharer_tracker.impl=SparseSnoopFilter` 与其**三级目录**语义**不自洽**(`25 号 §4.1` L1/L2/L3)⇒ 需 CEP 侧新增**第 6 种** SharerTracker 实现 `HierarchicalDirectory` —— **本仓尚无 owner**。
2. **无状态机规约**:ArchForge 侧只有 5 态词表 `M/E/S/I/F`(`25 号 §4.1`),**无** 5×N 转换表 ⇒ 实现须本地推导(**待 ArchForge 追认**)。
3. **无 5→4 投影表**:ArchForge 只给了 `F → WRITEBACK` 一行 ⇒ 其余 4 行须本地推导。
4. **成本与 `moesi_gpu` 不同档** ⇒ 不应与 `moesi_gpu` 并列同估。

⇒ **#8 行显式登记 R-1 阻塞 + 「排期 ≠ 就绪」**,不制造「排进去 = 会做」的假象。

---

## D-θ:跨仓前置登记(诚实登记,不掩盖)

| 项 | 状态 | 对本 change 的影响 |
|---|---|---|
| **ArchForge `32 号` 是 Draft v0.1** | 事实 | 本 change 引用其**语义**(6×6 转换 / 投影表 / 6 类词表 / VC 矩阵),不引用其行号 ⇒ 行号漂移不影响本 change |
| **D-9 批 2(计划 2026-11-05)改 `09 号` 的 8 处引用** | 事实 | 同上;本 change **不**在 `09 号` 上建锚点 |
| **C-1 放行判据要求 ArchForge 侧有 commit hash 的接口 PR** | **治理阻塞(既有)** | 纯设计仓结构上不可闭合;`ADR-SOC-25 D11` 提议改为「接口规约 PR」⇒ 本 change 的跨仓前置登记在此,**不阻塞**本仓实施 |
| **ArchForge 侧无 `mesif` 状态机规约 / 投影表** | 债 | 本 change 不碰(见 D-η) |

---

## D-ι:B2 贡献口径 —— **新增 invariant 数 = 0**

`test/test_cep_canonical_invariants.cc::expected_groups()` 把 **15 组 66 条**编号**硬编码**;`ADR-CEP-01` / `ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反(队列 #5 `Entry #66` D-ε 先例)。

⇒ 本 change 的 B2 贡献 = **行为化既有条目** + **契约层加固**:

| 既有条目 | 行为化方式 |
|---|---|
| `INV-CEP-05-IC1` | Stub 只暴露稳定状态 ⇒ **行为化**:`moesi_gpu` 的 6 态 → 4 值投影**不泄漏瞬态 `T`**(AC-05) |
| `INV-CEP-12-IC1` | `protocol_version` 落在锁表内 ⇒ **行为化**:`moesi-gpu-1.0` 的装配期校验 |
| `INV-CEP-12-IC2` | per-network 三条件校验 ⇒ **行为化**:`moesi_gpu` 声明的 `vc_binding` / `ordering_assumption` 逐 network 校验 |
| `INV-CEP-12-IC3` | 仅 `implemented` 计入 adapter 实例数 ⇒ **行为化**:`status` 翻转后计数 0 → 1 |

**新增 invariant 数 = 0** ⇒ `test_cep_canonical_invariants.cc` **零 diff**。

---

## D-κ:判据清单(5 条)与反向 fixture 纪律

| ID | 判据 | 反向 fixture |
|---|---|---|
| **AC-01** | `perm` **4 值锁不破**:① `protocol_instance.schema.json` 的 `perm` 仍是 4 值枚举、无 `ATOMIC` ② `perm_upgrades.schema.json` 的 `from`/`to` **必须**取自该 4 值集合(跨 schema `$ref`) ③ 契约侧的**秩序**与 schema 侧的**枚举顺序**一致 | 注入 `from: "ATOMIC"` / `to: "SHARED"` ⇒ 红 |
| **AC-02** | `perm_upgrade` **引用完整性 + 权限单调**:① 声明的实例 id 必须在 `protocol_instances.json` 里存在 ② 该实例的 `perm` 数组**同时包含** `from` 与 `to` ③ `to` 的秩 ≥ `from` 的秩 | 注入不存在的实例 id / `perm` 缺 `to` / `UNIQUE → READ` ⇒ 红 |
| **AC-03** | **反向 fixture**:① `mesif` **不**出现在静默升级声明源里(无该能力) ② 声明源缺失 / 畸形 ⇒ 判 **FAIL**(与「无可检查对象 ⇒ SKIP」**严格区分**) | 注入 `mesif` 条目 / 删文件 / 改坏 JSON ⇒ 红 |
| **AC-04** | 6→3 映射表**完备且非恒等**:① 每个该实例用到的 6 类消息类**都有**映射行 ② 目标 ∈ `CATEGORY_*` 3 类 ③ **至少一行** ≠ 恒等 ④ 恒等实例须携带 `identity_reason` | 删一行 / 目标写成第 4 类 / 全表恒等且无理由 ⇒ 红 |
| **AC-05** | adapter **投影自洽**:① 6 态中每个**稳定态**都有 4 值投影 ② 瞬态 `T` **无**投影 ③ `E+Write→M` 转换存在(`state_transition[2][1] == 3`)④ 转换后 perm 从 `UNIQUE` 变 `WRITEBACK` | 删 `T` 的无投影约束 / 改 `state_transition[2][1]` / 让 `T` 有投影 ⇒ 红 |

> **纪律**(per 队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**守门/闸的新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

---

## D-λ:范围切小 —— 本 change **不做**什么

| 不做 | 理由 |
|---|---|
| `T` 态的**跨域可见性**机制 | 瞬态必须**留插件内部**(per `32 号 §6.4`);跨域接口只暴露 4 值 ⇒ 本 change 只断言「不泄漏」,不做机制 |
| `atomic` 意图的**语义标签**落地 | `moesi_gpu` 的 `atomic.supported=false` ⇒ 无消费者;`mesif` 才有(但归 #8) |
| 跨域写传播机制(proxy-cache / 模型翻译) | `ADR-CEP-13` 决策 3 的**传播机制**属运行期实现,超出「声明 + 契约」层 |
| SharerTracker 第 6 种实现 `HierarchicalDirectory` | R-1 ⇒ 归 #8 的前置,本 change 不碰 |
| `CATEGORY_*` 词表扩容 | 分层纪律(见 D-δ) |
| `protocol_instance.schema.json` 改动 | 见 D-β |
| 23 ABI / `fabric.*` / frozen headers | 冻结面 |

---

## D-μ:跨实例复用的边界 —— 本 capability 是**机制**,后续 adapter 只**填行**

`cep-adapter-schema-contracts` 提供的 5 条判据是**跨实例复用**的机制;后续 7 个 adapter change(`chi-rnf` / `mesif` / `cxl-nonsymmetric` / `tilelink` / `cxl-dcoh-symmetric` / `nvlink` / `ualink`)**复用**判据实现,**各自填**:
- 静默升级声明(多数为空 ⇒ 省略)
- 6→3 映射行(逐实例用到的类)
- 实例声明(12 键 + `status`)

⇒ **禁止双实现**:判据实现只在 `adapter_schema_contracts.hh` 一份,test 与 CLI 共用(per 队列 #5 `Entry #68` 的「禁双实现」纪律)。
