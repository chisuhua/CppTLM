# I-7b 交办包 · `cep-tilelink-home-side-state-driver`（登记册队列 2 · I-7 的**第二个子单元**）

- **日期**：2026-11-21 · **提出方**：builder（I-7a 交付后的路线测绘）
- **对象单元**：`.rddf/improvements/cep-tilelink-home-side-state-driver.md`（状态「🟡 待立项」· planner 起草 · architect 裁决 §四.4 已定**形态**）
- **前置门（AC-13）**：✅ **已满足** —— `include/tlm/cep/tilelink_tree_manager.hh` 已有 `LocalView view_of(std::uint64_t) const`（I-7a · commit `a2a4c7ae`）且与 `FullMapDirectory::view_of` 同签名
- ⚠️ **本文件不是裁决** —— §三 的逐消息映射是**草案**（待裁），⛔ 不得当作已定形态实施

## 一、结论摘要

| 项 | 结论 |
|---|---|
| **技术可行性** | ✅ **无硬阻塞** —— 5 项关键技术探测全部通过（§二）⇒ **可以开工** |
| **工作量** | ≈ **I-7a 的 2.6 倍**（~700 行新代码 + 冻结面 delta spec + 架构文档 SU + 五步链 + 验收 + 归档）|
| **需裁决项** | ⚠️ **2 个**（§四）—— ① **逐消息 → 规约事件的映射**（含 2 处**仓内不可判定**）② **冻结面 spec 主体措辞** |
| **不动引擎** | ✅ 与裁决一致 —— TLE-05 禁引擎持状态 ⇒ 驱动**不进** `tilelink_protocol_engine.hh` |

## 二、已完成的 5 项技术探测（全部可复现）

| # | 探测 | 手段 | 结果 |
|:--:|---|---|---|
| **D-1** | **delta spec 的 MODIFIED 主体改写是否可行** | 起草 delta（主体首句改写 + 追加 1 个 tracker 侧 Scenario）⇒ `openspec validate --strict` ⇒ **归档合并** ⇒ 看 main spec diff | ✅ `validate` 绿；合并 diff = **+8 / −1**（**1 行主体改写 + 7 行新 Scenario**）⇒ 族 1 的 **7 个 Scenario 逐字保留** ⇒ 冻结面 **只 1 个 hash 变**（`cep-evidence-tier2-observation/spec.md`）⇒ **五步链可控**。⚠️ 探测已**完全回滚**（`git checkout` + 归档目录移走 ⇒ `_active` 归 0 ⇒ 仓库绿）|
| **D-2** | **reconcile 闸的判据骨架** | 读 `scripts/test/cep_e2e_reconcile_check.sh`（650 行）| 第一块 heredoc（`:83-367`）含 **T4 / T5a-c / T6 / T7**；解析结果在 `doc`（`:127`）/ `entries`（`:135`）；失败统一 `print(f"✗ …", file=sys.stderr)` + `problems += 1`；`DOC_UNITS`（`:221-228`）= 3 元组 `(路径, 段标题前缀 或 None, 标签)`；变异基座 `mutate()`（`:547-631`）**9 条**（`:632-640` 调用）|
| **D-3** | **档 2 条目 ↔ `coverage_map` 有无跨表判据** | 同上 | ⚠️ **无** —— 现存只有 T5c（`evidence_tiers` × `run_tiers`）与 T7（同文件 `coverage_map` × `protocol_instances` × 代码 `kTrackerTraits`）⇒ ⇒ **T8 是真缺口**（AC-2 的「新条目 `family` 改成 `moesi_gpu` ⇒ T8 红」当前**无处可红**）|
| **D-4** | **族 1 判据范式** | 读 `test/test_cep_tier2_observation.cc`（553 行，**11 个 TEST_CASE**）| 三段分离：引擎只落原始事实（`record_observation`）⇒ **判定全在 test 侧** `detect()`（`:76-121`）+ `Tier2Verdict`（`:50-61`）；正控/负控/反 fixture 形态 = 「同流 + 单差异」+ **精确 `== 1`**；`observation_invalid` 的 3 条前提在 `mesif_protocol_engine.hh:345-350`（tracker 非空 / node 已映射 / level 合法）|
| **D-5** | **TileLink 消息面是否存在** | grep + 读 `tilelink_state_spec.hh` / `tilelink_c_adapter.hh` | ✅ 存在但**薄**：`tilelink_c_adapter.hh:196-203` 有 **10 条消息 → 6 类角色**映射（`kProtocolMessageMap`）· `tilelink_state_spec.hh:245-265` 有 **5 条消息 → 通道/方向**（`kAsymmetryMessages`）· `tilelink_state_spec.hh:171+` 有 **12 格规约转移表**（`kTransitions`）· ⚠️ **无结构化消息类型**（无 addr/node/param 字段载体）|

## 三、⚠️ 逐消息 → 树表动作映射（**草案 · 待裁** · 非结论）

**可用的仓内依据（4 组）**：`kProtocolMessageMap`（10 条 → 6 类角色）· `kAsymmetryMessages`（5 条 → 通道 + 方向）· `kTransitions`（`State` × `Event` → 目标掩码，12 格）· `kMessageClassDowngradeNote`（2 处降维的显式登记）。

| 消息 | 角色 | 通道 | 草案动作 | 依据 |
|---|---|---|---|---|
| `Acquire` | request | A | **不动**（请求尚未获批准 ⇒ 无副本）| 角色 = request；规约 `Cap` 的「源不定」语义 |
| `Probe` | snoop | B | **不动**（发起强制撤销 ⇒ 撤销的是**发起方**）| `kAsymmetryMessages`：`Probe` 只在 B 通道、slave→master |
| `ProbeAck` | snoop_resp | — | `set_sharer(Branch)`（客户端**保留**降级副本）| improvement What Changes #1 的明文（`ProbeAck` = 保留）|
| `ProbeAckData` | snoop_resp | — | `clear_sharer`（交回数据**并放弃**）| 同上（**方向相反** ⇒ 正控/负控互为反例）|
| `Release` | snoop_resp | — | `clear_sharer`（自愿放弃）| `kMessageClassDowngradeNote` ①（「语义是**自愿放弃**」）|
| `ReleaseData` | snoop_resp | — | `clear_sharer` | 同上 |
| `Grant` | response | D | **按消息携带的 toT/toB/toN 装表**（`toT`⇒Tip · `toB`⇒Branch · `toN`⇒`clear_sharer`）| `kAsymmetryMessages`（Grant 只在 D 通道 · 权限授予）+ 规约 `Cap` 格 `TLC-01`（记号 toT/toB/toN ⇒ 目标 3 元集合）|
| `GrantData` | response | D | 同 `Grant` | 同上 |
| `ReleaseAck` | response | — | **不动**（ack）| 角色 = response |
| `GrantAck` | response | E | **不动**（ack）| `kAsymmetryMessages`（GrantAck 在 E 通道 · master→slave）+ 降维登记 ② |

### ⚠️ 2 处**仓内不可判定**（⇒ 需裁决，⛔ 不得自行编值）

| # | 缺口 | 实测 | 候选处置 |
|:--:|---|---|---|
| **G-1** | **`Grow` / `Prune` 两个规约事件在消息层无对应** | `kTransitions` 的 4 个事件（`Cap` / `Grow` / `Prune` / `Report`）中，`Grow`（权限升级·长树）/ `Prune`（权限降级·缩树）在 10 条消息上**没有逐消息锚点**（仓内只有 `Cap` 的 `TLC-01` 有 toT/toB/toN 记号）| ① 簿记层**只实现 `Cap` 类**（`Grant`/`GrantData` 携带 toT/toB/toN），`Grow`/`Prune`/`Report` 类事件**显式登记为「不可判定」**并计入 `observation_invalid`（保守）② 授权 builder 按 TileLink 规范常识推导 10 条 → 4 事件的完整映射（⚠️ **仓内无锚点** ⇒ 违反「⛔ 不得编值」纪律）|
| **G-2** | **`Grant` 的目标权限字段无结构化载体** | 规约 `Cap` 的 `toT/toB/toN` 是**消息记号**（`TLC-01`）⇒ 簿记层**必须自建**消息结构携带它 ⇒ 该字段的**承载形态**（枚举？掩码？）无仓内先例 | ① 复用规约载体 `TileLinkStateSpec::State`（3 值：`Nothing`/`Branch`/`Tip`）作载荷（⚠️ **单向**：`State` ⇒ 消息字段 ⇒ 规约 ⇒ **不反向**）② 新建独立枚举（⚠️ 第二套词表 ⇒ 违反单源纪律）|

## 四、⚠️ 待裁决的 2 个点（**用户口径**）

### Q-1 · 逐消息映射的形态（**建议 architect 裁**）

- **(a) 保守降级**（推荐）：簿记层**只实现 `Cap` 类**（`Grant`/`GrantData` 携带 toT/toB/toN）；`Grow`/`Prune`/`Report` 类**显式登记为不可判定** ⇒ 计入 `observation_invalid`（⛔ 不是「零违例」）。⇒ **零编值**、零无锚点推断；代价 = 覆盖**仅限授予/放弃类**事件。
- **(b) 完整映射**：授权 builder 按 TileLink-C 规范推导 10 条 → 4 事件的完整表 ⇒ 覆盖更全，⚠️ 但 2 处**无仓内锚点** ⇒ 需显式登记「推导来源 = 规范常识（非仓内锚点）」。

### Q-2 · 冻结面 spec 的主体措辞（**建议 architect 裁 / 或用户 ratify**）

- 探测已证：**主体改写可行且极小**（diff **+8/−1** · 族 1 的 7 个 Scenario 逐字保留 · 冻结面只 1 个 hash 变）。
- ⚠️ 但「改写**已签批契约面**的主体措辞」属**架构层动作** ⇒ 按 §审查配对「builder 不得单方面改已签批决策内容」⇒ 需 architect 出措辞或用户 ratify。
- 候选措辞（**草案**）：`CEP 的**协议级观测面** SHALL 暴露 **per-event** 的原始观测事实（**两种载体形态**：协议引擎侧 + tracker 侧）` ⇒ 追加 Scenario「tracker 侧载体 SHALL 暴露驱动边界快照」。

## 五、实施路线（裁决后即可执行 · 阶段划分）

| 阶段 | 内容 | 触冻结面 |
|:--:|---|:--:|
| **S1** | 起 openspec change（proposal / design / tasks + **delta spec**（按 Q-2 措辞））+ 台账 Entry（授权）| ⚠️ 是（delta 合并改 main spec）|
| **S2** | 新建 home 侧消息簿记层（`include/tlm/cep/tilelink_home_state_driver.hh` · header-only · **不碰引擎**）+ 族 2 tracker 侧观测载体（⛔ **不复用** `EventObservation` —— AC-11 禁改族 1 基底面）| 否 |
| **S3** | 族 2 判据载体（test 侧判定器 + 正控/负控/反 fixture + 4 条装配期硬断言 + provenance 反 fixture + 有界性）| 否 |
| **S4** | `test/cep_evidence_tiers.json`：新增 `tilelink_c` 档 2 条目（`carrier_kind = "tracker-observation"`）+ `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」限定；reconcile 闸加 **T8**（两表双向闭合）+ **T9**（`carrier_kind` 封闭词表）+ 变异基座 +2 条 + `DOC_UNITS` 追加新 SU | 否 |
| **S5** | **冻结面五步链**（台账 Entry → 改产物 → commit A → `--update` → 清单单独 commit → 无参复验）· `docs/architecture/07-coherence-explaboration-platform.md` 追加 SU（append-only）| ⚠️ 是 |
| **S6** | 验收报告 + 载体表 `I-7` 行（**父编号** ⇒ I-7a 的 8 行 + I-7b 的 N 行全 done 才算 I-7 完成）+ 登记册回填 + 归档（force-add + 基线 +1）| 否 |

⚠️ **载体表**：I-7b 的行用**父编号 `I-7`** ⇒ I-7 的 **doc 行仍 pending** ⇒ **I-7 本身仍未完成**（I-7b 是第二子单元）⇒ 队列 2 的下一项仍是 **I-7**（P-2a 判据实测 `✓ 队列 2 下一项 (I-7) ⇒ 无跳序`）。

## 六、关联

登记册 §6.4 队列 2 的 **I-7**（= I-7a ✅ + **I-7b** ⏳）· 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四.4（I-7b 的**形态**）· I-7a 验收报告 `.rddf/reviews/cpptlm-cep-tilelink-tree-manager-view-of-verify.md` · 族 1 判据 `test/test_cep_tier2_observation.cc` · reconcile 闸 `scripts/test/cep_e2e_reconcile_check.sh` · 档 2 载体登记面 `test/cep_evidence_tiers.json`
