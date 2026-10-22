# CEP PoC Spike 用例规格（S1 / S2 / S3）

> **日期**: 2026-10-22 · **作者**: CppTLM CEP DRI（build agent）
> **状态**: 📋 **规格草案（不含实现）** · 登记为 Stage 1 change 待 DRI 拍板（P-1）
> **来源**: [`20261022-cep-remaining-batches.md`](../../.opencode/plan/20261022-cep-remaining-batches.md) §10（P-2）
> **上游计划**: [`20261022-cep-contract-convergence-batch1.md`](../../.opencode/plan/20261022-cep-contract-convergence-batch1.md)（批 1）
> **对外载体**: [`2026-10-21-cep-archforge-response-receipt.md`](./2026-10-21-cep-archforge-response-receipt.md) §8.5.1（Q-3e）

---

## 0. 一句话

本规格定义一个**可丢弃的 spike** 的 3 个判定场景 —— 目的是在 Stage 1 动工前
**用可执行证据**裁定两个当前只能靠纸面辩论的问题：
① `ADR-CEP-13` 域可见写判据的 (b) 条件（Q-3，已 2 轮未交付）；
② per-network 嵌套 VC 形态在真实路径上是否**可组合**。

---

## 1. 定位与硬边界

### 1.1 定位

| 项 | 定位 |
|----|------|
| **它是什么** | 一个**单文件、可丢弃**的 spike，验证「关键概念 + 架构 + 设计」的**可组合性** |
| **它不是什么** | ❌ 不是 Stage 1 的实现 ❌ 不是 conformance test ❌ 不是架构变更 |
| **它替代什么** | 替代**纸面论证**——尤其 `INV-CEP-13-IC2` 的 (b) 条件 |

### 1.2 硬边界（4 条，不可省略）

1. **PoC ≠ 提前实现** —— 必须做成**可丢弃的 spike**：
   单文件 / 不进 `ModuleFactory` / 不注册 ABI / 无公共 API。
   否则会把仍标 **provisional** 的 per-network 形态**焊死进框架**（与 D7 provisional 债冲突）。
2. **PoC 不验证「正确性」** —— 只验证「**可组合性 + 契约自洽**」。
   **不得**对外宣称协议合规，**不得**作为 conformance 证据。
3. **实现顺序** —— PoC **规格**（本文件）可现在做；
   **实现**的前置按**子场景**区分（2026-10-22 登记 P-1 时澄清 —— 原表述「实现排在 A4 + A6 之后」存在**循环依赖**）：

   | 子场景 | 前置 | 理由 |
   |--------|------|------|
   | **S2**（跨域可见写） | **A4** 即可（本批已完成） | S2 **不依赖** A6 的结论 —— 它正是**产出**该结论的机制（Q-3e） |
   | **S1**（`perm` 投影不变量） | **A4** + **A6 的 `perm` 表达力结论** | S1 的「期望 `perm`」由 6 态机推导，需先定 4 值表达力边界 |
   | **S3**（死锁自由） | **A4** + `vc_binding` 声明 | 需真实声明才能建图 |

   > ⚠️ **循环依赖澄清**：若把整个 PoC 的实现整体押在 A6 之后，则
   > **A6 需要 (b) 的评估 → (b) 可由 S2 观测 → S2 需要 PoC 实现** → 死锁。
   > → **S2 必须可先行**。这正是该 change 在 Stage 1 **置首位**的理由
   > （per `ADR-CEP-00 §2 决策 3` 的 2026-10-22 增补段）。
   > A4 侧无此问题：`protocol_version` 装配期 fail（`INV-CEP-11-IC1`）是**硬前置**，已在本批完成。
4. **R10 不会被 PoC 关闭** —— PoC 让真人评审**可执行**，
   但**评审本身仍要做**（D5 的最小动作：1 页摘要 + 1 条回执）。

### 1.3 成本量级

Stage 1 的 7 个 change 中,本 PoC 约对应 **3 个**的具体能力:

| 对应的 Stage 1 change | PoC 里对应的最小实现 |
|----------------------|---------------------|
| `cpptlm-cep-stub-cache-core` | Stub Cache Core 层(1 个 L1 + 4 值 `perm` 投影) |
| 一个 protocol subset(如 `cpptlm-cep-chi-rnf-subset`) | Protocol Engine 插件层(`moesi_gpu` 6 态 × 6 事件) |
| `cpptlm-cep-poc-spike` 自身的 fabric 部分 | Fabric & Transport 层(2 网络 × 4 VC,per-network 绑定) |

> PoC **不实现**上述任一 change 的**生产级**内容 —— 只做**可组合性验证**所需的最小切片
> (硬边界 1:单文件 / 不进 `ModuleFactory` / 不注册 ABI / 无公共 API)。

---

## 2. 最小切片

> **场景来源**:ArchForge 侧三类使用场景（`09 号` SoC 内 snoop / `25 号` NSA 跨卡目录 /
> `30 号` 按 fabric scope 分级的跨节点模型）+ 我方 3 域划分
> （D-CPU / D-GPU / D-FABRIC，per `ADR-SOC-25` D6）。

| 层 | PoC 最小实现 | 要验证的关键概念 |
|----|--------------|----------------|
| **Stub Cache Core** | 1 个 L1（4 值 `perm` 投影） | `INV-CEP-05-IC1`：6 态 → 4 值投影；瞬态态 `T` 不泄漏 |
| **Protocol Engine** | `moesi_gpu` 插件（6 态 × 6 事件） | 协议逻辑住插件；**无 `translate`**；请求携带 consistency label（D3-a/b/c） |
| **Coherence Interface** | canonical 10 流的最小子集（request / completion / snoop / snoop_resp / writeback） | 流口径与配对（CEN-Ib1 / Ib3 / Ib4 / Ib10） |
| **Directory & Ordering** | 1 个 SharerTracker（`SparseSnoopFilter`） | `ADR-CEP-04` 的 caps 语义 |
| **Fabric & Transport** | **2 网络 × 4 VC**（`compute_noc` + `ualink_fabric`），**per-network** 绑定 | **本次契约演进的核心** —— per-network VC 绑定在真实路径上跑不跑得通 |
| **跨域桥接** | D-GPU ↔ D-FABRIC 一条边 | `ADR-CEP-13` 域可见写判据 (a)(b) —— **Q-3 的答案** |

> **不实现**:`mesif`（目录型，Stage 1 之外）/ 完整 canonical 10 流 / 真实 snoop 广播拓扑。

---

## 3. 三个判定场景

### 3.1 S1 —— 单域基线（`perm` 投影与状态机不变量）

| 项 | 内容 |
|----|------|
| **输入** | D-GPU 域 1 个 L1（4 值 `perm` 投影）+ `moesi_gpu` 插件（6 态 × 6 事件）。<br>序列：① 读 X（未命中）→ ② 读 X（命中）→ ③ 写 X（store 命中本地行）→ ④ 写回 / 驱逐 X |
| **期望** | ① Stub Cache 对外**只暴露 4 个稳定态**，协议插件私有瞬态态 `T` **不泄漏**（`INV-CEP-05-IC1`）<br>② 每一步的 `perm` 投影与由 6 态状态机推导出的**期望 `perm`** 一致<br>③ 写回路径存在且**被观测到**（不是静默丢弃） |
| **判定** | **逐序列点**比对「实际投影 `perm`」与「由 6 态机推导的期望 `perm`」；任一不一致 → **S1 失败**。<br>⚠️ **不判定协议正确性** —— 只判定「6 态 → 4 值投影」这条**契约**成立 |
| **失败含义** | `perm` 4 值投影的表达力不足 → R-2 的**需求本体**成立（与 (b) 的评估**无关**） |

### 3.2 S2 —— 跨域可见写（**Q-3 的答案**）

| 项 | 内容 |
|----|------|
| **输入** | 同一缓存行 X 由 **D-GPU** 域持有，状态 `E`。<br>D-GPU 侧对 X 发起 **store**（store 命中本地 `E` 行 → MOESI `E → M`）；<br>随后 **D-FABRIC** 域对 X 发起**读** |
| **期望(若 (b) 成立)** | D-FABRIC 的读**必须**看到 D-GPU 已写的新值<br>→ 跨域可见写成立 → `INV-CEP-13-IC2` 触发 → **R-2 命中** `ADR-CEP-05 §10` 判据 2 |
| **期望(若 (b) 不成立)** | D-FABRIC 的读**看不到**该写（或该写在协议侧**根本无传播路径**）<br>→ 不构成域可见写 → **R-2 降级**为「潜在表达力缺口,尚未证实触发」 |
| **判定** | **观测值**（读到**旧值** / **新值**）即为 (b) 的答案 —— **无需纸面论证**。<br>判据是**二值**的，不存在「部分成立」的中间态 |
| **边界** | 本场景**不验证正确性**，只验证「6 态 → 4 值 `perm` 投影 + 跨域桥接 + per-network VC 绑定」在真实路径上**可组合** |
| **失败含义** | 若观测到**旧值** → 需明确：是**协议语义如此**（(b) 不成立），还是**桥接漏了传播路径**（spike 缺陷）。<br>→ 两种情形的**处置不同**，故 S2 必须同时记录**观测值**与**传播路径的可达性** |

> **这是 S2 唯一可能有跨仓裁决后果的场景** —— 其余两个场景只影响本仓 Stage 1 的实现顺序。

### 3.3 S3 —— 死锁自由（通道依赖图无环）

| 项 | 内容 |
|----|------|
| **输入** | 2 网络 × 4 VC（`compute_noc` + `ualink_fabric`），**per-network** 绑定；<br>通道依赖图的顶点 = **`(协议实例, network, vc_id)`** 三元组 |
| **期望** | 依赖图在该顶点集上**无环** —— 即 `ADR-CEP-09` 的无环判据在顶点升级后**仍成立**（而非退化为全局图） |
| **判定** | ① **正向**:环检测器在**真实声明**上跑 → 无环；<br>② **反向(必须有牙)**:把顶点**降级**回 `(协议实例, vc_id)` 重跑 → **必须报环**。<br>若降级后仍报无环 → 说明该图**根本没有跨网依赖边** → 顶点升级是**形式化**而非实质 → **S3 失败** |
| **失败含义** | ① 报环 → per-network 顶点下**存在**真实的跨网依赖 → 顶点升级是实质需求，但**当前声明触发了死锁** → 需回查 `vc_binding` 声明；<br>② 不报环 → 顶点升级**无实质差异** → `ADR-CEP-09` 的 Status Update 需重述 |

> **② 这一条是本场景的「有牙」保证** —— 与本仓既有的**反向验证**文化一致
> （`scripts/test/cep_decl_reverse_verify.sh` 的 12 条反向断言）。
> **只跑正向的 S3 等于没跑**。

---

## 4. 前置依赖与排队口径

### 4.1 实现前置（两个 adr-amendment 批）

| 前置 | 为什么是前置 | 状态 |
|------|------------|------|
| **A4**（`ADR-CEP-11 §3` 锁表 + `07 号 §5.1` 登记 `moesi_gpu` / `mesif`） | `protocol_version` 校验在**装配期**跑，锁表外取值 = **fail**（`INV-CEP-11-IC1`）→ `moesi_gpu` 未登记则插件**装不上** | ✅ **已完成**（2026-10-22，台账 Entry #12;22 处落点全绿） |
| **A6**（`ADR-CEP-05 §10` 判据 2 状态收敛） | S1 的「期望 `perm`」需先定 4 值表达力边界 | ⏳ 阻塞于 Q-3 —— 但 **S2 正是解阻机制**，见 §1.2 item 3 的循环依赖澄清 |

> ⚠️ **A4 的连带计数变更**:14 → 16 adapter 实例,涉及 **22 处落点**
> (含 8 份冻结产物 + 1 条 CI 锚点)—— 详见
> [`20261022-cep-remaining-batches.md`](../../.opencode/plan/20261022-cep-remaining-batches.md) §3.1。

### 4.2 排队口径（P-1，已拍板 2026-10-22）

已登记为 **Stage 1 的第 7 个 change**（`cpptlm-cep-poc-spike`），
进 `ADR-CEP-00 §2 决策 3` 的 KPI 排队（**10 → 11 change / 11 季度**），**置于 Stage 1 首位**。

> **置首位的理由**:S2 是 Q-3 / A6 的**解阻器**,而 A6 又是全部协议插件 change 的前置
> → 它是唯一能**加速**其余 Stage 1 change 的项;若置于末位则「解阻 Q-3」的用途失效。
>
> **登记 ≠ 立刻实现** —— 规格已先行(本文件),实现按 §1.2 item 3 的分场景前置推进。

---

## 5. 明确不做

- ❌ 不改任何**冻结产物**（`ADR-CEP-*` / `07 号` / schema / spec）
- ❌ 不改 `perm` 的 schema 形态（`perm_upgrade` 顶层字段的落点由 **A6** 决定，不在本规格内）
- ❌ 不实现 `mesif`（目录型）
- ❌ 不做 conformance test，**不**对外宣称协议合规
- ❌ **不**因为 PoC 而宣称 R10（真人外部评审）已闭合

---

## 6. 引用

**本仓**：`docs/adr/ADR-CEP-{04,05,06,09,11,12,13}-*.md` ·
`docs/architecture/07-coherence-exploration-platform.md` §5.1 ·
`include/tlm/cep/{protocol_instance,fabric}.schema.json` ·
`scripts/test/cep_decl_reverse_verify.sh`（反向验证范式）

**跨仓（只读）**：ArchForge `32 号` §6.0.1（6 类消息类词表）/ §6.0.2（per-network VC 矩阵）·
`09 号` §3（MOESI/GPU 6×6）/ `25-nsa`（跨卡目录）/ `30 号`（fabric scope 分级）·
`ADR-SOC-25` D6（3 域划分）/ D7（provisional 债）

**机制**：`ADR-CEP-06 §3` 的 `verification_mode`（声明 + 实验证伪路径）·
`obj-cep-platform` §9.2（M1/M2/M4 机制）· §11 台账
