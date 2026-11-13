# design —— `cep-evidence-tier2-minimal-slice`

> 依据:`.rddf/improvements/cep-evidence-tier2-minimal-slice.md`(v2)· `.rddf/reviews/cep-tier2-prerequisite-rulings.md`(C′ / C′-bis / C′-bis-ter)
> ⚠️ 本 design 的每条约束都带一手 `file:line` 锚点;⛔ 标「⛔ 不改」的即为**硬边界**。

## D-1 · 观测面形态(per 裁决 R4 / R5)

| 项 | 决策 | 依据 |
|---|---|---|
| 粒度 | **per-event**(⛔ 非 per-txn) | `apply_event` 无 `txn_id`;WRITEBACK 一次 completion = **2 条 transition + 1 条 fill**(`mesif_protocol_engine.hh:439-442`) |
| 落点 ① | `kind = transition` —— `apply_event` **入口** | `apply_event` 是**全部状态迁移的单点入口**(`:325`);读 `Rd`/`RdUnique`(`:439`)、写 `Wr`(`:441`)、失效 `SnpUnique`/`SnpShared`(`:435`/`:458`/`:465`) |
| 落点 ② | `kind = fill` —— `on_completion` 装行**之后** | 行数据**唯一**写入点 = `:442`(`apply_event` 只改 state)⇒ 在迁移点采 `data_after` **恒为旧值** |
| sharer 快照 | `tracker_->view_of(addr)`(**const 纯查询**) | ⛔ **不得**用 `lookup` —— 它有副作用:首次转发时 `dir_.emplace` + 记 `AllocateOnFirstForward` 事件(`sharer_tracker.hh` `FullMapDirectory::lookup`)⇒ 会污染目录事件流 |
| 快照时点 | `transition` = **迁移前**视图 | `sync_tracker` 在 `apply_event` **末尾**(`:344`)才写目录 ⇒ 记录必须在其**之前**取 |
| 字段表 | `{kind, origin, addr, event, from, to, data_before, data_after, sharers_snapshot, self_mask, self_mask_valid, level, node}` | 裁决 R5;⛔ **无 version 字段**(R1) |
| sharer 字段类型 | `std::uint64_t`(**⛔ 不用 `responder_set_t`**) | ⇒ 基类**不新增**对 `sharer_tracker.hh` 的依赖 ⇒ 4 受限族接入时基类**无需改动**(seam 目标) |
| 开关 | **默认关闭**(`observations_enabled_ == false`) | ⇒ 既有行为与性能**零影响**;只有显式开启的用例产生记录 |
| 有界 | 容量上限 4096 + `dropped_observations` 计数 | ⛔ 静默丢弃 ⇒ 计数非零 ⇒ 判**不可判** |

## D-2 · 归因分层(per R1 / R5 §c)

| 判据 | 归因对象 | ⛔ 不得表述为 |
|---|---|---|
| **SWMR** —— 写时该地址仍被他人持有 | **home 侧可见性**(run 内 home 行为) | 「引擎实现错误」 |
| **data-value** —— 读到的值 ≠ home 最近写值 | **run 内 home 行为** | 「引擎未保持一致性」 |
| `DirectInject`(外部直调 `apply_event`) | **测试注入** ⇒ ⛔ **不计入**任何生产流判据 | 「生产流违例」 |

## D-3 · 判定口径(per 裁决 §3)

- **SWMR 判定行** = `kind = transition ∧ event = Wr ∧ to = M` ⇒ **一次写 episode 恰好 1 条**;
- `kind = fill` 行**显式豁免**(它是同一 episode 的第 3 条记录)⇒ 逐行计数会**重复计数**;
- 违例条件 = `sharers_snapshot & ~self_mask != 0`(且 `self_mask_valid == true`);
- **计数按 `(addr, write-episode)` 归并后计** ⇒ ⛔ 不按原始行。

## D-4 · 装配期硬断言(per AC-4)

| # | 断言 | 静默退化形态 |
|:--:|---|---|
| ① | `tracker_ != nullptr` | `sync_tracker` 走 `tracker_missing` 分支(`:600-603`) |
| ② | `responder_mask_of(node) != 0` | `set_sharer` **静默 no-op**(`responder_mask_of` 只映射 4 bit) |
| ③ | `level < HierarchicalDirectory::kLevelCount` | `set_sharer` / `clear_sharer` **静默 no-op** |
| ④ | `caps.completeness == EXACT`(**防御**) | ⚠️ `HierarchicalDirectory` 构造时写死 `EXACT` ⇒ **生产路径恒真** ⇒ 仅防护「实现被换成保守 tracker」⇒ **不要求**反 fixture |

⇒ 失败 ⇒ `observation_invalid` +1 ⇒ 该次观测**判「不可判」** ⇒ ⛔ **不是 0**。

## D-5 · 已知边界(随实现落盘 · ⛔ 不得当判据用)

### 假阴(最危险 —— 产出「0」)

| # | 形态 | 处置 |
|:--:|---|---|
| 1 | `node_id ∉ {0,1,2,3}` ⇒ `set_sharer` 静默 no-op(而 `tracker_feeds` 照涨) | AC-4 ② 硬断言 ⇒ 判不可判 |
| 2 | `level ≥ 3` ⇒ 目录写入静默 no-op | AC-4 ③ |
| 3 | 两引擎共用同一 `node_id` ⇒ 互为「自己」⇒ 恒 0 | design 硬约束:正控**两个不同节点** |
| 4 | `set_line` 播种**旁路** `sync_tracker`(`:302-308`)⇒ 目录与本地态脱钩 | ⛔ 正控**不得**用 `set_line` 播种 sharer(provenance 判据) |
| 5 | `to == I` ⇒ `lines_.erase` ⇒ 行数据随行消失 | 观测记录在 erase **之前**取 ⇒ 不受影响 |
| 6 | `on_snoop_resp` 丢弃 `data`(决策 E-5)⇒ 转发路径的 data-value 不可见 | 登记 |
| 7 | `Wr` 在 `S`/`F`/`I` 上「状态不变」⇒ 返回值不可见 | 判定行取 `(event, to)` 而非返回值 |
| 8 | WRITEBACK 一 txn 两事件 ⇒ 观测条数 ≠ txn 数 | 按 episode 归并(D-3) |
| 9 | `submit_local_request` 先登记 `txn_addr_` 再写 egress ⇒ egress 全忙时**映射残留** ⇒ 后续 completion 会作用在「从未发出的 txn」上 | 本档位**不走**拓扑级 egress ⇒ 正控不触发;登记为已知形态 |

### 假阳(把 harness 的问题记成协议违例)

| # | 形态 | 处置 |
|:--:|---|---|
| 1 | home 授予 WRITEBACK 后未向对端发 snoop ⇒ 目录里对端位不被清 ⇒ 报违例 | 归因声明 = **run 内 home 行为** |
| 2 | 保守 tracker ⇒ sharer 是**上界** ⇒ 恒假阳 | AC-4 ④ `completeness == EXACT` |
| 3 | `forward_to` 有双重语义 ⇒ 用它猜「有人」会假阳/假阴 | ⛔ 只用 `sharers` / `dirty` |

### 时序 / 配置

| # | 形态 | 处置 |
|:--:|---|---|
| 1 | 同一地址多写者交错 ⇒ per-engine 视角不可比 | 本档位**只**判「写时他人 sharer」⇒ 不做跨引擎数据比对 |
| 2 | 从未写过的地址被读 ⇒ 无样本 | 判「无样本」而非 0 |
| 3 | 长 run ⇒ `lines_` / 观测缓冲增长 | 观测缓冲**有界**(D-1);`lines_` 的增长策略登记 |
| 4 | 多线程驱动引擎 ⇒ `lines_` / `ctr_` / 观测缓冲无锁 | design 硬约束:**单线程** |

### 其他已知缺陷(⛔ 本次**只登记、不修**)

| # | 形态 | 处置 |
|:--:|---|---|
| 1 | `apply_event` 的注释称「false = 出向流不支持(拒绝闸)」,但实现**恒返回 true** | 过期注释 —— 改注释会牵动语义判断 ⇒ **只登记**,不改返回值 |
| 2 | `responder_mask_of` 只有 4 bit ⇒ 多节点族 sharer 维度**不可承载** | **能力缺口**(非工时问题)⇒ 全族推广的单独立项 |

## D-6 · 停手条件(per AC-1)

**AC-1 的三条任一不成立 ⇒ 停手回报**(⛔ 不以交付数量为目标):

1. 正控未观测到**完整写 episode**(transition(Wr→M) + fill);
2. 断线三任一未**硬失败**(被读成 0);
3. **provenance** 不成立 —— sharer bit 无法溯源到 peer 引擎自身的 `apply_event → sync_tracker`(⇒ 「用 fixture 喂饱判据」)。

⚠️ **M3 动机预警**:「R-4 从排期债降级」这一动机**可绕过** AC-1(交一份 SU + spec 即算达成)⇒ ⇒ AC-1 ③ 的 provenance 判据是**唯一**兜底 ⇒ 不得省。

## D-7 · 未实施项(本 change 内明确不做)

4 件套 run · 版本维 · home 侧共享写序 · 权限维 · 标签维 · 检测器搬进 `include/` · `kResponderMappedBits` 扩容 · `B3b-2` 的 4 族 · 任何已签批 ADR 正文改动。
