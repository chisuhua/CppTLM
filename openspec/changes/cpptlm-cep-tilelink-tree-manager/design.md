# Design — cpptlm-cep-tilelink-tree-manager v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#11** 第 1 个 change(实现层)
> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/cep-tilelink-tree-manager/spec.md`](specs/cep-tilelink-tree-manager/spec.md)
> **口径**: 台账 **Entry #134** / **#135** / **#138** / **#141** / **#143**
> **规约载体**: `include/tlm/cep/tilelink_state_spec.hh`(第 0 个 change 已归档产物)
> **审计面**: `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(§2.3bis = 12 格逐格 basis 核对表)
> **估时**: 8 周 · **KPI**: Proposed 并发 **0/3 → 1/3**

---

## D-α adapter 形态 = **零自有表副本**(委托规约载体)

### 依据

`tilelink_state_spec.hh` 的头注释明写「adapter SHALL **零自有表副本**(委托消费本载体)⇒ 规约改一格、adapter 必红」。

仓内两种形态并存:

| 形态 | 实例 | 说明 |
|---|---|---|
| **旧形态** | `MoesiGpuAdapter` · `ChiRnfAdapter` | **自持**二维表 `kStateTransition[6][6]` / `[5][6]` + 自写 `perm_of` switch ⇒ 规约改一格,**adapter 不会红** ⇒ 漂移不可见 |
| **新形态** | `MesifAdapter` · `CxlCacheAdapter` | **委托** `MesifStateSpec::transition` / `CxlStateSpec::transition_kind` ⇒ 规约改一格,**adapter 必红** |

⇒ **采用新形态**。

### ⚠️ 结构性保证(不只是约定)

`cxl_cache_adapter.hh:24-28` 的写法:**去注释后**的 adapter 文本 SHALL **不**含 `State::` / `Event::` / `Perm::` **任何字面量**,SHALL **不**含任何**表名**(`kTransitions` / `kPermProjection` / `kTable`)
⇒ **语法上不可能**携带自己的表 ⇒ 由**测试的静态扫描**兜底(AC-1 的反向 fixture:植入字面量 ⇒ 必红)。

---

## D-β `TreeManager::lookup` 的 3 态语义 —— `ESCALATE_UP` 的**首次真实使用**

### 状态 → 结果映射

| 节点在树中的位置 | 副本权限 | `lookup` 结果 | `view.forward_to` |
|---|---|---|---|
| **Tip**(序列化点) | Read+Write(可能脏) | **`ANSWER_HERE`** | — |
| **Branch**(只读副本) | Read | **`ESCALATE_UP`** | **树父节点** |
| **Nothing**(无副本) | None | **`ESCALATE_UP`** | **树父节点** |

### 依据

- `LookupResult::ESCALATE_UP` 的注释即「树形向上递归(TileLink)」—— **该注释自 2026-10 起就指向队列 #11**
- `ADR-CEP-04 §5` 已明载:DCOH 与 TileLink 会「**被迫对返回字段善意撒谎**」⇒ `lookup` 假设单次同步回答全局问题,但树形语义下**做不到**
  ⇒ ⇒ **`ESCALATE_UP` 正是「不撒谎」的显式出口**:不返回全局答案,而是返回「**向上递归**」+ 递归目标
- 树形**有唯一 Tip** ⇒ **不适用** `BROADCAST`(无目录才广播)⇒ `TreeManager` **永不**返 `BROADCAST`

### ⚠️ `caps` 与语义的一致性

`TreeManager` 的 caps = `Scope::SUBTREE` · `Completeness::EXACT` · `Authority::LOCAL` · `AggregationAt::TREE_PARENT`
⇒ 「作用域 = 子树」「本地权威」「聚合点在树父」⇒ **与「向上递归到父节点」自洽** ⇒ `ANSWER_HERE` 只在 **Tip**(本地即权威)时成立。

---

## D-γ `CHAIN` 的机械 —— 新增 `SnoopMechanics::snoop_join_core`

### 事实

`SnoopMechanics::snoop_begin_core`(`sharer_tracker.hh:336-369`)对**同址已有 in-flight snoop** 的处理:

| 情形 | 现行为 |
|---|---|
| 期望集**相同** | 返**既有 handle**(IC2 幂等,不新建 pending) |
| 期望集**不同** | 返 **0**(无效 handle)并**拒绝** ⇒ **不得**新建第二条 pending |

⇒ **没有任何「把新 responder 并入既有 in-flight snoop」的入口** ⇒ 而 `LookupHazard::CHAIN` 的语义恰是「**加入**同一 in-flight snoop 的 expected_responders」。

### 决策

**在 `SnoopMechanics` 新增 `snoop_join_core(addr, extra_responders, handle)`**(公共机械 ⇒ 禁双实现):

| 步 | 行为 |
|---|---|
| 1 | 该 addr **无** in-flight ⇒ 返 **0**(「无可加入」⇒ 调用方应改走 `snoop_begin`) |
| 2 | `extra_responders` 含**未映射位** ⇒ 返 **0**(与 `snoop_begin_core` 同一拒绝语义) |
| 3 | `extra_responders` 与既有 `expected` **无新增位** ⇒ 返**既有 handle**(幂等) |
| 4 | 否则 ⇒ 把新增位并入 `expected` **与** `remaining` ⇒ 返既有 handle ⇒ `snoop_fold` 的归零逻辑**不变**(⇒ **零重复实现**) |

⇒ **`TreeManager::lookup` 在该 addr 有 in-flight snoop 时返 `LookupHazard::CHAIN`**(而非 `STALL`)⇒ 语义 = 「**别等,加入它**」。

### ⚠️ 与 `STALL` 的分工

`LookupHazard::STALL` = 「等 in-flight snoop 完成后重试」;`CHAIN` = 「加入同一 in-flight snoop」
⇒ 二者是**同一场景的两种处置** ⇒ `TreeManager` 选 `CHAIN`(树形语义:父节点向上递归时,多个子节点可能同时探测同一块 ⇒ 合并成**一条** snoop 链,而不是各自等待)。

---

## D-δ `LocalView` **零改动** —— 复用 `forward_to` 承载「树父节点」

### 事实

`LocalView`(`sharer_tracker.hh:261-266`)只有 **4 字段**:`sharers` / `dirty` / `forward_to` / `valid` ⇒ **无** `tree_parent`。
`LookupResult` 的 `FORWARD_TO`(向指定目标转发取数)与 `ESCALATE_UP`(树形向上递归)**都**需要一个「目标节点」⇒ `forward_to` 是**同一字段的两种用法**。

### 决策

**复用 `forward_to`** ⇒ `LocalView` **零改动** ⇒ **不触及** `cep-sharer-tracker-base` main spec 的 `LocalView` 契约
⇒ 与 improvement 草稿的登记一致(「`LocalView` 是否扩 `tree_parent` ⇒ **须单独登记**,**非**本 change 的默认动作」)。

### ⚠️ 语义代价(**必须登记**)

| 面 | 代价 |
|---|---|
| `forward_to` 的**双重语义** | `FORWARD_TO` 时 = 取数目标节点;`ESCALATE_UP` 时 = **树父节点** ⇒ 调用方**必须**先读 `result` 再解释 `forward_to` |
| 判别方式 | `result` 本身即可区分 ⇒ **无歧义**(不是「同一个值两种含义」,而是「同一字段由 `result` 消歧」) |
| 若将来要扩 `tree_parent` | ⇒ 公开契约变更 ⇒ **另立 change** + 两步授权;**不在本 change** |

---

## D-ε `AC-05` **族无关化** —— 参数化 + SKIP 分支

### 事实(硬编码点)

`include/tlm/cep/adapter_schema_contracts.hh` 的 `check_adapter_self_consistency`:

```cpp
if (e_write_target_state != 3) {          // 硬编码 3 = M 态
    line.verdict = AdapterContractVerdict::FAIL;
    line.detail  = "E + Write 的目标态 = " + ... + "(应为 3 = M)";
    return line;                          // ⚠️ 无 SKIP / UNKNOWN 分支
}
```

### 决策

| 改法 | 内容 |
|---|---|
| **参数化** | 三元组「**唯一干净态** + **写事件** → **目标态**」由调用方传入(**不再硬编码 3**)⇒ 4 个既有族按各自语义传参 ⇒ 判定**逐条不变** |
| **SKIP 分支** | 该族**没有** unique-clean 态(`unique_clean_state` 为「无」)⇒ 判据 **SKIP**(**不是** FAIL)⇒ 理由 = 「该协议族不暴露干净独占态 ⇒ `E + Write` 语义不适用」 |
| **秩单调** | 保留既有「`M` 态秩 > `E` 态秩」的判据,同样按传入秩比较 |
| ⚠️ **回归锁** | 族无关化**放宽**了既有族 ⇒ SHALL 有**回归锁**:既有 4 族的 verdict **逐条不变**(既有测试已覆盖 ⇒ 直接作为回归锁) |

⇒ ⇒ **本 change 不靠「填假的 `e_write_target_state = 3`」让 TileLink 变绿**。

---

## D-ζ 实例声明 `tilelink_c` —— 13 键 + `perm` **3 值** + `vc_binding` 按 R-T2

| 键 | 值 | 依据 |
|---|---|---|
| `id` | `tilelink_c` | schema pattern `^[a-z0-9][a-z0-9_-]*$` |
| `family` | `academic` | TileLink 是**开源学术标准**(非工业 / 非 ruby-slicc) |
| `adapter` | `TileLinkCAdapter` | 对齐 adapter 的 `kAdapterName` |
| `status` | `implemented` | 本仓权威域 ⇒ **不受** ArchForge 镜像约束 |
| `protocol_version` | `tilelink-c-1.7.1` | per `ADR-CEP-11 §3` 锁表(Entry #135)⇒ schema pattern 已容纳 |
| `flow_support` | **5 true / 5 false** | 锚点摘要 §4(canonical 10 流映射) |
| `perm` | **`["NONE","READ","WRITEBACK"]`** | **R-T4 裁决** = 值域锁 ⇒ 3 值合法(`items.enum` 是值级枚举 + `minItems:1`)⇒ **schema 零改动** |
| `atomic` | `{ supported: false }` | TileLink 无 atomic 语义 |
| `consistency_labels` | `{ read: "SC", write: "SC" }` | TileLink 是**强一致**(序列化点)⇒ SC |
| `ordering_assumption` | `compute_noc`: 响应向 VC 须 `p2p_ordered` | **J1 判据**:绑定 `response` / `snoop_resp` 的 VC 必须在 `ordering_assumption` 声明 `p2p_ordered` ⇒ `vc2` / `vc3` 必声明 |
| `vc_binding` | `compute_noc`: `request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3 | **R-T2 裁决**(Entry #138 / #141)· ⚠️ **不得**写 `writeback`(流名 ≠ 消息类名;schema enum 也不含) |
| `sharer_tracker` | `impl: "TreeManager"` + caps 4 属性逐字段等于 `kTrackerTraits[3]` | schema 的 `impl` enum **已含** `TreeManager` |
| `combination_capabilities` | 3 个 bool | TileLink **非**并发多副本族 ⇒ 取值须在实现时按语义定,并登记 |

### ⚠️ `repo_owned_instance_ids`

ArchForge **未**声明 TileLink ⇒ `tilelink_c` 属**本仓域** ⇒ **必须**登记进 `repo_owned_instance_ids`
⇒ ⑥ 镜像比对**跳过**、①~⑤ 校验**覆盖**(`cep_decl_alignment_check.sh` 的既定口径)。

---

## D-η **2 件冻结面**同步(⇒ 两步授权)

| # | 冻结面成员 | 改动 |
|---|---|---|
| 1 | `openspec/specs/cep-sharer-tracker-base/spec.md` | 「其余 **3** 种实现(`DcohSymmetric` / `TreeManager` / `BroadcastNull`)的**行为实现**尚未落地」⇒ 落 `TreeManager` 后改「其余 **2** 种」;并登记 `TreeManager` 的落位 |
| 2 | `docs/architecture/07-coherence-exploration-platform.md` §11 | 「SharerTracker **6** 实现」行的 `🟡 **3 / 6**` ⇒ `🟡 **4 / 6**`;「其余 3 个……仅声明层 traits」⇒ 「其余 **2** 个」 |

⇒ **两步授权**:台账 Entry → `--update --authorized-by "Entry #N"` → commit 产物 → `--update`(provenance 对齐)→ commit。
⇒ ⚠️ `protocol_instance.schema.json`(★ 冻结)**本 change 零改动** ⇒ 不进本清单。

---

## D-θ `perm_upgrades.json` —— **不加行** + `current_state` 自述**失实**

| 面 | 处置 |
|---|---|
| 加 `tilelink_c` 行? | ❌ **不加** —— TileLink **不暴露 `UNIQUE`** ⇒ **无**「`UNIQUE` → `WRITEBACK`」静默升级 ⇒ 加了就是**假声明** |
| 派生集后果 | `derive_no_upgrade_instances` = 实例集 − 已声明集 ⇒ `tilelink_c` **首次**进入 ⇒ 该派生集**首次非空** ⇒ 既有断言 `no_upgrade_instances.empty()` **必红** ⇒ 改为「**恰含** `tilelink_c`」 |
| ⚠️ `current_state` 自述 | 现写「4 个实例全部具备该能力 ⇒ 派生量恒为空」⇒ **失实** ⇒ 须同步(该文件**非**冻结面) |
| ⚠️ 派生量的**判别力** | 该派生集从「恒空」变「非空」⇒ 派生量**从无判别力变为有判别力** ⇒ 是**判据变强**而非变弱 |

---

## D-⑨ 已排除的替代方案

| 方案 | 排除理由 |
|---|---|
| **adapter 自持表副本**(照 `moesi_gpu` / `chi_rnf` 旧形态) | 规约改一格、adapter **不红** ⇒ 漂移不可见 ⇒ 违背第 0 个 change 的「零自有表副本」纪律 |
| **扩 `LocalView` 加 `tree_parent`** | 公开契约变更 ⇒ 须另立 change + 两步授权;**非**本 change 的默认动作(improvement 草稿已登记)⇒ 本 change 复用 `forward_to`(D-δ) |
| **`TreeManager` 自持 CHAIN 机械**(不入 `SnoopMechanics`) | 触发「禁双实现」审查 ⇒ CHAIN 的归零逻辑会与 `snoop_fold_core` **双份** ⇒ 归零语义漂移风险 |
| **实例 `perm` 填 4 值**(含 `UNIQUE`) | `exposes_unique_perm = true` ⇒ `INV-CEP-08-IC2` 走 hierarchical 分支 ⇒ **PASS 但 detail 语义为假**(TileLink 无 `E` 态)⇒ **语义为假的绿灯** ⇒ 违反 R-T4 裁决 |
| **改 `protocol_instance.schema.json` 放宽 `perm`** | **零改动即合法**(`items.enum` 是值级枚举)⇒ 动**冻结面**属**无谓的破坏** |
| **加 `tilelink_c` 的 `perm_upgrades` 行** | 无 `UNIQUE` ⇒ 无静默升级 ⇒ 加了是**假声明** |
| **在 `channel_dependency_edges` 里加 TileLink 专用边** | 边表是**顶层**跨实例共享 ⇒ 加了会污染 mesif / moesi_gpu 的判定 ⇒ 保守近似**已登记**(Entry #141) |
| **本 change 一并做 protocol engine** | 引擎与 adapter **分属不同 change**(照 mesif / chi_rnf 形态)⇒ 拆开 |

---

## 已排除的替代方案(范围层)

| 方案 | 排除理由 |
|---|---|
| **规约层 + 实现层合并为一个 change** | 规约层**已归档** ⇒ 事实上的两段式 ⇒ 合并不可行 |
| **只做实现层、跳过判据** | 实现层必然**自推** `TreeManager` 语义 ⇒ 重蹈 #9 覆辙 ⇒ 判据与实现**同批** |
