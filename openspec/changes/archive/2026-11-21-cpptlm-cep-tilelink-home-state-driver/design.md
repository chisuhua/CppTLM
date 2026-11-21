# design: cpptlm-cep-tilelink-home-state-driver

> **裁决来源**：`.rddf/reviews/cep-i7b-adjudication-2026-11-21.md`（Q-1 逐行 basis 纪律 + Q-2 契约面主体）· `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四.4（I-7b 形态）。
> 本文件承载**裁决留给 change 的行内容**（= 逐消息安装语义表）⇒ 由 **C-1 ~ C-5** 锁死（见 proposal AC-1 ~ AC-5）。

## D-1 · 逐消息安装语义表（**恰 10 行** · 行序 == 消息枚举值）

**推导链（3 段，每段都有仓内锚点）**：

1. **消息 ⇒ canonical 意图** ⇒ T3 表（`tilelink_state_spec.hh:747-777`）的**事件**
2. **事件 × 源态 ⇒ 目标掩码** ⇒ 12 格转移表（`tilelink_state_spec.hh:171-208`）
3. **目标掩码 ∩ 消息载荷** ⇒ 实际安装动作

⚠️ **源态在 home 侧可知**（树位置可查 ⇒ `TreeManager::position_of`）⇒ 允许按 12 格矩阵装表；⛔ **`Cap` 行（源不定，`:172-179`）不可直接安装** ⇒ 任何需要 `Cap` 的输入 ⇒ **不可判定**。

| # | 消息 | 6 类角色 | canonical 意图 | T3 行（事件） | 载荷域 | 动作 | basis | `event_ref` | `payload_ref` |
|:--:|---|---|---|---|---|---|:--:|---|---|
| 1 | `Acquire` | request | `Request` | `TLC-M-01`（**无事件**）| ∅ | `NoOp` | `Direct` | `TLC-M-01` | `TLC-01` |
| 2 | `Grant` | response | `Completion` | `TLC-M-02`（`Grow`）| {Branch, Tip} | `SetState` | `SelfDerived` | `TLC-M-02` | `repo` |
| 3 | `GrantData` | response | `Completion` | `TLC-M-02`（`Grow`）| {Branch, Tip} | `SetState` | `SelfDerived` | `TLC-M-02` | `repo` |
| 4 | `Probe` | snoop | `Snoop` | `TLC-M-03`（`Prune`）| {Nothing, Branch} | `SetState` | `SelfDerived` | `TLC-M-03` | `repo` |
| 5 | `ProbeAck` | snoop_resp | `SnoopResp` | `TLC-M-04`（`Prune`）| {Nothing, Branch} | `SetState` | `SelfDerived` | `TLC-M-04` | `repo` |
| 6 | `ProbeAckData` | snoop_resp | `SnoopResp` | `TLC-M-04`（`Prune`）| {Nothing, Branch} | `SetState` | `SelfDerived` | `TLC-M-04` | `repo` |
| 7 | `Release` | snoop_resp | `Writeback` | `TLC-M-05`（`Prune`）| {Nothing, Branch} | `SetState` | `SelfDerived` | `TLC-M-05` | `repo` |
| 8 | `ReleaseData` | snoop_resp | `Writeback` | `TLC-M-05`（`Prune`）| {Nothing, Branch} | `SetState` | `SelfDerived` | `TLC-M-05` | `repo` |
| 9 | `GrantAck` | response | `SnoopCompletionAck` | `TLC-M-10`（**无事件**）| ∅ | `NoOp` | `Direct` | `TLC-M-10` | `TLC-01` |
| 10 | `ReleaseAck` | response | `SnoopCompletionAck` | `TLC-M-10`（**无事件**）| ∅ | `NoOp` | `Direct` | `TLC-M-10` | `TLC-01` |

### 为什么 7 行是 `SelfDerived`（**诚实登记**，非偷懒）

- **事件层**：T3 表 11 行**全部 `Direct`** ⇒ 事件锚点**无争议**（`event_ref` 恒为 SpecClause）。
- **载荷层**：⚠️ 规约只给了 **`toT`/`toB`/`toN` 记号**（`TLC-01`，`Cap` 行）⇒ 「载荷 = 目标状态」这个**形态**有锚点；但**每类消息的载荷域**（哪几个目标态被允许）**在仓内无逐值锚点** ⇒ ⇒ **排除项是本仓推导** ⇒ `payload_ref = repo`（已登记的 `RepoDerived` id）+ **行内 note 显式登记**（裁决 §2.2 的「`SelfDerived` ⇒ 锚点为 `RepoDerived` **且显式登记**」）。
- ⚠️ **⛔ 不得**把 `ProbeAck` 的「保留降级副本」写成规范事实（**循环自证** —— 交办包 §三 的该行锚 improvement 自身）⇒ 本表**不引用**它；「保留 vs 放弃」由 **`Prune` 的 12 格行 + 载荷**推导（`BtoN` ⇒ `Nothing` · `TtoB` ⇒ `Branch`）。

### 12 格矩阵的装载规则（**算法**，非逐条枚举）

```
resolve_install(msg, payload_mask, src_state) →
  row = kTlHomeInstallRows[msg]
  if row.action == NoOp:
      payload_mask 非 0 ⇒ Undecidable（超域 · fail-closed）; 否则 ⇒ NoOp
  else:
      ev_mask = t3_consumable_event_mask(row.intent)      // 0 ⇒ 无事件 ⇒ Undecidable
      payload_mask 必须是 row.payload_domain 的**非空子集**且**恰 1 位** ⇒ 否则 Undecidable
      cell = transition(src_state, event_of(ev_mask))
      cell.kind != Known                ⇒ Undecidable
      (cell.target_mask & payload_mask) == 0 ⇒ Undecidable（源态下该目标不存在 ⇒ 规范明确空集）
      ⇒ 目标态 = payload_mask 的唯一位 ⇒ Installed
```

⚠️ 事件位掩码 = `1 << Event 枚举值`（`kT3EventCap=0b0001` ↔ `Event::Cap=0` · `kT3EventGrow=0b0010` ↔ `1` · `kT3EventPrune=0b0100` ↔ `2` · `kT3EventReport=0b1000` ↔ `3`）⇒ 可直接解位。

⚠️ **本表不做的事**（⛔ 禁）：不复制 T3 表 / 12 格表的**字面量**（⇒ **C-5** 静态扫描判红）⇒ 一律经规约层访问器（`t3_consumable_event_mask` / `transition` / `mask_of`）**只读消费**。

## D-2 · 观测载体（**tracker 侧** · ⛔ 不复用族 1 的 `EventObservation`）

族 1 的 `EventObservation` 在 `protocol_engine_base.hh:169-199`（**族 1 基底面** · AC-19 禁改）且字段表定死（`event`/`from`/`to`/`data_before`/`data_after` 是**引擎态语义**）⇒ tracker 侧**无引擎态迁移** ⇒ 硬套 = **假字段** ⇒ ⇒ **自建** tracker 侧载体：

| 字段 | 类型 | 语义 |
|---|---|---|
| `addr` | `uint64_t` | 地址 |
| `message` | `TlHomeMessage` | **消息 id**（裁决 §3.2 的「消息 id」）|
| `node` | `uint32_t` | 节点（安装方）|
| `action` | `TlHomeAction` | 实际动作（`NoOp` / `SetState`）|
| `target` | `TileLinkStateSpec::State` | `SetState` 时的目标态 |
| `sharers_before` | `uint64_t` | **安装前**副本分布快照（`view_of`）|
| `self_mask` | `uint64_t` | 自身位掩码（归因来源）|
| `self_mask_valid` | `bool` | 节点未映射 ⇒ false ⇒ 判不可判 |
| `episode_id` | `uint64_t` | 写 episode 归并键 |

⚠️ ⛔ **不含** `event`/`from`/`to`/`data_before`/`data_after`（引擎态语义不适用）· ⛔ **不含**版本号字段。

## D-3 · 驱动类（`TileLinkHomeStateDriver`）

| 面 | 内容 |
|---|---|
| **构造** | 持 `TreeManager&`（**具体类型**，per mesif 先例「按具体实现接线是诚实的」）+ 观测开关（**默认关闭**）+ 缓冲上限（**有界**）|
| **主入口** | `apply(message, addr, node, payload_state)` → 返回 `TlHomeApplyResult{outcome, installed_state}`；⚠️ 步骤序 = **① 装配前提检查 ② 取安装前快照（`view_of`）③ 解析安装 ④ 装表（`set_sharer`）⑤ 记观测** |
| **观测记录** | 每次 `apply` 在**驱动边界**产生**恰好 1 条**（`NoOp` 也产生 ⇒ 否则「无观测」与「无事件」不可区分）；⚠️ 快照时点 = **安装前** |
| **安装台账（溯源）** | `addr → (位掩码 → 消息 id)` ⇒ 「他人 sharer」的每一位可溯源到输入流消息；⚠️ **绕过驱动**直接 `set_sharer` 的位**不在**台账 ⇒ `provenance_broken` |
| **不可判（4 条）** | ① `node` 未映射（`responder_mask_of == 0`）② `tips` 已含他人（多 Tip）③ 观测面关闭 / 空流 ④ 丢弃计数 > 0 ⇒ 递增**观测面失效计数** ⇒ 判**不可判**（⛔ **不得**记 0）|
| **有界性** | 缓冲上限 ⇒ 溢出递增**丢弃计数**（⛔ 不静默丢弃）⇒ 丢弃非零 ⇒ 该次观测**不可判** |
| **⛔ 不做** | 不自主发 `Probe`/`Grant` · 不做超时与 transient 处理 · 不进引擎文件 · 不持有协议状态机 |

## D-4 · 档 2 条目与 `coverage_map` 限定（`test/cep_evidence_tiers.json`）

- 新条目 `tilelink_c-observation`：`tier = 2` · `family = "tilelink_c"` · `carrier_kind = "tracker-observation"` · `run_produced = false` · `has_verdict = false` · `coverage_scope.families = ["tilelink_c"]` · `topologies = ["single-topology"]` · `instances = 1` · `carrier_files` = 驱动头 + 新测试 ⇒ ⚠️ T4 要求 `carrier_files` 每项**实际存在** ✓
- `can_claim`（**3 条**）· `cannot_claim`（**必含**「不同型」+「不覆盖 data-value 维」+「不覆盖跨子树」）· `evidence_scope` 含档位词（`档 2` + `协议级`）⇒ T4 绿
- `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」限定（⛔ 既有文字**逐字保留**，只追加）

## D-5 · reconcile 闸的 T8 / T9

| # | 判据 | 红条件 |
|:--:|---|---|
| **T8** | `evidence_tiers` ↔ `coverage_map` **双向**跨表一致 | ① 某档 2 条目的 `family` 不在 `coverage_map.families` ⇒ 红 ② 某档 2 条目的 `family` 的 `tier2_applicable != true` ⇒ 红 ③ `coverage_map` 中 `tier2_applicable == true` 的族**无**任何档 2 条目覆盖 ⇒ 红 |
| **T9** | `carrier_kind` **封闭词表** + 两值互异 | 出现词表外取值 ⇒ 红；两个条目的 `carrier_kind` 相同 ⇒ 红 |

⚠️ **变异基座 +2 条**（`mutate "<签名>"`）：① 新条目 `family` 改成 `moesi_gpu` ⇒ **T8 红** ② `carrier_kind` 撞值成 `protocol-engine-observation` ⇒ **T9 红**；⚠️ 签名串**必须**与失败消息中的子串**完全一致**（闸内 `grep -qF`）。

⚠️ **`DOC_UNITS` 追加**新 SU（`docs/architecture/07-coherence-exploration-platform.md` 的新 SU 段标题前缀）⇒ T6 变异的定位随之更新 ⇒ 否则新 SU **完全不受 T6 保护**。

## D-6 · delta spec（**逐字取裁决 §3.1 / §3.2**）

- **Requirement 主体首句** = 裁决 §3.1 的文本（**逐字**）
- **追加 Scenario**「tracker 侧载体 SHALL 在驱动边界产生 per-installation 观测」= 裁决 §3.2 的文本（**逐字** · 含族 1 的四条 disciplines）
- **族 1 的 7 个 Scenario 逐字保留**（⚠️ MODIFIED 内**禁止重命名 Scenario**）· **spec `Purpose` 段不动**（裁决 §3.3）

## D-7 · 五步链（冻结面）

台账 Entry → 改产物 → **commit A** → `--update --authorized-by "Entry #N"`（第 2 次 ⇒ `frozen_at_commit` = commit A）→ **清单单独 commit** → 无参复验 rc=0。
⚠️ `artifact_count` **逐字节不变**（本单元**不新增**冻结成员：delta 改的是**既有** `cep-evidence-tier2-observation/spec.md`；归档目录只匹配 `*cpptlm-cep-umbrella` 形态 ⇒ 不入集）⇒ 冻结清单的 hash 变更**只 1 个**。

## D-8 · 载体表（`scripts/test/rddf_ac_carrier.tsv`）

**AC 编号用父编号 `I-7`** ⇒ I-7a 的 8 行 + I-7b 的 N 行**全 done** 才算 I-7 完成（⛔ 阻止 I-7b 单独宣告 I-7 完成）⇒ I-7 的 **doc 行仍 `pending`** ⇒ 队列 2 的下一项**仍是 I-7**。

⚠️ **⛔ 不新增** `I-7b` 独立队列槽位（否则须改 `scripts/test/rddf_stage_gate.sh` 的 3 处队列数组 + 位置硬约束）。
