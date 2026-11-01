# Tasks — cpptlm-cep-tilelink-state-spec v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#11** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-tilelink-state-spec/spec.md`](specs/cep-tilelink-state-spec/spec.md)
> **口径**: 台账 **Entry #134** / **#135** / **#138**
> **普查**: `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(Task 0 ⇒ 硬门**通过**)
> **估时**: 4 周(规约层)· **KPI**: Proposed 并发 **0/3 → 1/3**

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-01) | 版本锁定(Entry #135)· 三项 architect 裁决(Entry #138)· Task 0 普查(硬门**通过**)· improvement 草稿(HARD-GATE 通过)· change 目录 4 件套已建 |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-01)· ⚠️ **二审已更正** | 3 稳定态 + 4 事件 + **`kind` + 3 位目标掩码** ⇒ 分桶 `single 5 / multi 5 / empty 2 / unknown 0`;与 #10 的单值矩阵**不同型**,见 design D-β / D-γ |
| 2 规约载体 | ⬜ 待实施 | `tilelink_state_spec.hh`:3 态 / 4 事件 / 12 格 / 3→3 投影 / 4 类结构断言 / `basis` / 编译期锁 |
| 3 判据收口 | ⬜ 待实施 | 契约测试 + 反向 fixture + 锚点核对表生成器 + `--check` 交叉核对 + ctest 注册 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + 零冻结面 diff + KPI 1/3 → 0/3 + 7 处 baseline 同步 + 归档 |

---

## Phase 0 — 治理前置 ✅

- [x] 0.1 **版本锁定**:`ADR-CEP-11 §3` 锁表 `tilelink-c-1.3` → **`tilelink-c-1.7.1`**(SiFive)
      ⇒ 台账 **Entry #135**(冻结面授权记录)+ `ADR-CEP-11` 的 `## Status Update(2026-11-01,per 台账 Entry #135)`
      ⇒ **原地更新 3 处 + 增补块 + SU**(该 ADR 决策 2 注释要求「变更必须同步两表」)⇒ **R-T1 闭合**
- [x] 0.2 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`
      ⇒ **🔵 硬门通过**(3 稳定态 + 4 事件 + 12 格**零 `Unknown`** + `perm` 3→3 + 非对称形态)
- [x] 0.3 **三项 architect 裁决一次性签发**(per 台账 **Entry #138**):
      **R-T2**(5 通道 → 4 VC,合并 D∪E)· **R-T3**(6 类归属表)· **R-T6**(非对称定义形态)
      ⇒ 三份 ADR **纯追加** `## Status Update(2026-11-01,per 台账 Entry #138)`
- [x] 0.4 **improvement 草稿** ⇒ `.rddf/improvements/cpptlm-cep-tilelink-tree-manager.md`
      ⇒ `pre_create_brainstorm_check.sh` **rc=0**(HARD-GATE 满足)
- [x] 0.5 change 目录 4 件套已建 + `openspec validate --strict` rc=0
- [x] 0.6 **KPI slot 占用**:**0/3 → 1/3** + **7 处**活文档 baseline 同步
      (obj §5.2 · obj §5.3 · obj §4.0 队列 #11 行 · phase-10 · umbrella improvement · roadmap.md · AGENTS.md)
      ⇒ ⚠️ **第 7 处(obj §5.3)是队列 #10 的前例被门禁抓出来的漏项** ⇒ 勿漏

---

## Phase 1 — 载体形态定稿 ✅

> **状态**:✅ **已完成**(2026-11-01)—— 关键发现:**本载体与 #10 的单值矩阵不同型**。
> ⚠️ **2026-11-01 二审更正**:Phase 1 的产出**存在缺陷**,已就地修正,详见本段各条的「⚠️ 二审」注记:
> ① 1.1 的 `Trunk` 依据由「过渡态」(**推断**)改为**决策口径**(`Table 8.1` 权限等价);
> ② 1.3/1.4 的格语义由「4 值枚举」改为 **`kind` + 3 位掩码**(理由:枚举不阻止非法组合);
> ③ 原稿的 `single 6 / multi 3` **算错**(合计 11 ≠ 12),正确为 **`5 / 5`**。
> ⇒ 标 ✅ **不代表**原产出正确,而代表**已修正并通过复核**。

- [x] 1.1 **稳定态 = 3 值**(`Nothing`/`Branch`/`Tip`);`Trunk` **折叠**为 `Branch`
      ⚠️ **二审更正**:原写「`Trunk` = **过渡态**」是**推断**,且与 D1 §8.1「`four` categories」冲突
      ⇒ 改为**决策口径**:① `Table 8.1` 中 `Trunk`/`Branch` 权限**相同**(均仅 `Get`)
      ② `Table 8.3` 的 `Grow`/`Prune` 必须**单调** ⇒ `T` 唯一自洽读法 = `Tip` ③ 故折叠
      ⇒ ⚠️ **R-T5 已闭合**(3 处内部不一致 + **§8.1.4 末段穷尽性句背书**)
      ⇒ ⚠️ **语义损失已登记**(§1.2bis 三条),**不得**表述为「过渡态」
- [x] 1.2 **事件 = 4 类**(`Cap`/`Grow`/`Prune`/`Report`);`Cap` = **源不定**通配
- [x] 1.3 ⚠️ **`Cap` 的源不定 ⇒ 每格目标是 3 元集合**(非单值)
      ⚠️ **二审**:原写「需 `Known-multi` 格语义」⇒ 形态改为 **`kind` + 3 位掩码**(见 1.4)
- [x] 1.4 ⚠️ **`Grow`(源=T)与 `Prune`(源=N)规范明确为空集**
      ⚠️ **二审**:形态定为 **`kind ∈ {Known, Unknown}` + 3 位目标掩码**(**零非法组合**;
      「规范明确空集」= `Known` ∧ `mask == 0`;「规范未覆盖」= `Unknown` ∧ `mask == 0`)
      ⇒ ⇒ **派生量 5 个**:`kKnown 12` / `kUnknown 0` / `kEmpty 2` / `kSingle 5` / `kMulti 5`
      (⚠️ **原稿 `single 6 / multi 3` 算错,合计 11 ≠ 12**)
      ⇒ ⇒ ⚠️ **必须分别断言**「空集格恰为 2 个」与「`Unknown` 格为 0」—— 合并则**不可分辨**
- [x] 1.5 **`perm` 3→3**;`UNIQUE` **不可达** ⇒ 保守降级方向唯一
- [x] 1.6 **非对称 = TileLink 形态**(权限变更的发起方)⇒ ⚠️ **不得**复用 #10 的 CXL 形态

---

## Phase 2 — 规约载体(TDD 5-step)

### 2.1 载体骨架(`include/tlm/cep/tilelink_state_spec.hh`,新 · header-only)

- [ ] 2.1.1 `State` 枚举(3 值)· `Event` 枚举(4 类)· `Perm` 投影(3 值)
- [ ] 2.1.2 **格形态**:`enum class CellKind { Known, Unknown }` + `std::uint8_t target_mask`
      (位 0/1/2 = `Nothing`/`Branch`/`Tip`)—— **零非法组合**(`Unknown` ⇒ mask MUST 为 0)
- [ ] 2.1.3 **12 格转移表**(`std::array<std::array<Cell, kStateCount>, kEventCount>`)
      —— 行序 SHALL 与 `Event` 枚举值一致 ⇒ `static_assert` 锁
- [ ] 2.1.4 **`perm` 投影表**(3 行)+ `perm_of(state, out)` 辅助
- [ ] 2.1.5 **逐格 `basis{kind, pending, ref, note}`**(全部 `direct` + `pending=false`)
      ⇒ `ref` 只指向锚点摘要的**条目号**
- [ ] 2.1.6 **4 类结构断言**:
      ① 稳定态集合**逐字** = 3 个名字 ② 事件词表**逐字** = 4 个名字
      ③ 非对称断言 = TileLink 形态(`Grant`/`GrantData` 只在 D 通道 · `Probe` 只在 B 通道)
      ④ `Trunk` 不在稳定态集合中
- [ ] 2.1.7 **偏离清单**(**5 条**):
      ① D1 §8.1.4「six subsets」vs 表列 4 类 ② `Table 8.3` 图例行 `Trunk` vs 记号 `T = Tip`
      ③ `UNIQUE` 不可达(保守投影) ④ **`Trunk` 折叠的三条语义损失**(`design.md` §1.2bis)
      ⑤ **`AC-05` 族对 TileLink 恒红**(`design.md` D-η)⇒ ⚠️ **不得**填假值使其变绿
      ⇒ R-T4 只作**指针**(不在本 change 裁决)
- [ ] 2.1.8 **编译期锁**:`kStateCount == 3` · `kEventCount == 4` · `kTransitionCellCount == 12`
      · `kUnknownCellCount == 0` · `kEmptyCellCount == 2`
      · `kSingleCellCount == 5` · `kMultiCellCount == 5`(**5/5/2/0**,逐格见 `design.md` D-γ)
      ⇒ ⚠️ 铁律 ③:**编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 每条须有**运行时**断言

### 2.2 TDD 5-step(每步:红 → 绿 → 重构)

- [ ] 2.2.1 **步 1(红)**:先写 `test/test_cep_tilelink_state_spec.cc` 的 AC-1 ~ AC-6 断言 ⇒ 编译失败
- [ ] 2.2.2 **步 2(绿)**:实现载体使 AC-1 ~ AC-6 全绿
- [ ] 2.2.3 **步 3(重构)**:载体零自有表副本 + 命名与 `cxl_state_spec.hh` 对齐

---

## Phase 3 — 判据收口

- [ ] 3.1 **契约测试** `test/test_cep_tilelink_state_spec.cc`
      —— 覆盖 AC-1 ~ AC-6;⚠️ **纯负向断言 SHALL 有正控**
- [ ] 3.2 **反向 fixture**(每条 SHALL **因正确原因红**):
      ① 加回 `Trunk` ⇒ AC-1 红 ② 换掉某个稳定态名字 ⇒ AC-1 红(逐字断言才有鉴别力)
      ③ 某格改成 `Unknown` ⇒ AC-3 红 ④ 空集格数改成 3 ⇒ AC-3 红(空集与 Unknown **分别**断言)
      ⑤ 非对称断言换成 CXL 形态 ⇒ AC-5 红 ⑥ `perm` 投影加回 `UNIQUE` 行 ⇒ AC-4 红
      ⑦ **`single` ↔ `multi` 互移一格**(如把 `Grow｜N` 建成单值)⇒ AC-3 红(**5/5 分桶**才有鉴别力)
      ⑧ **`Known` 格的掩码置 0 但 `kind` 仍 `Known`** ⇒ AC-3 **GREEN**(空集是合法态)
         —— 这是**正控**:证明命题「空集 ≠ Unknown」可判
      ⑨ **`Unknown` 格携带非零掩码** ⇒ 载体的 `static_assert` **与** 运行时断言**双红**(零非法组合)
- [ ] 3.3 **锚点核对表生成器** `scripts/test/generate_tilelink_anchor_checklist.sh` + `.cc`
      ⚠️ **不得照抄** `generate_cxl_anchor_checklist.cc` 的 ID 文法 —— 其 `is_wellformed_id`
      只认「`prefix` + **全数字**」(`D<digit>` / `R-<digit>`);TileLink 锚点用 `| **D1** |`(**加粗**)
      与 `R-T1`(**非纯数字**)⇒ 合法集为**空** ⇒ 逐格校验**恒真空转**
      ⇒ 必须做 **3 件事**:① ID 文法**容忍 Markdown 加粗** ② **接受 `R-T<n>` 前缀**
      ③ **逐格白名单**比对(**不得**只校验 source id 首部)
      ⇒ 且 SHALL 有**反向 fixture 证明它不是空转**:毒化 ① 加粗形态 ② `R-T` 前缀 ③ 错格号
      ⇒ 三者**各自**必须因正确原因红
- [ ] 3.4 **`--check` 交叉核对**:手改载体或摘要任一侧 ⇒ **必须因正确原因红**
- [ ] 3.5 **ctest 注册** `cep_tilelink_state_spec`

---

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁全绿:docs_sync --strict / doc_ref --strict / cep_docs_sync / gate_selftest /
      freeze / decl_alignment / format --check · `openspec validate --strict` rc=0
- [ ] 4.2 **冻结面零 diff**(`git diff --stat -- docs/adr/ docs/architecture/ openspec/specs/` = 空)
- [ ] 4.3 **KPI 1/3 → 0/3** + **7 处**baseline 同步(⚠️ 同 0.6 的清单)
- [ ] 4.4 **归档语义 = 「✅ 已完成归档」** ⇒ delta spec **合并**进
      `openspec/specs/cep-tilelink-state-spec/spec.md`
- [ ] 4.5 台账 entry(归档记录)+ §4.0 队列 #11 行状态更新

---

## 非目标(Non-Goals)

- ❌ **不**实现 `TileLinkCAdapter` / `TreeManager` tracker 行为类 / 实例声明 / 装配
      ⇒ 归第 1 个 change `cpptlm-cep-tilelink-tree-manager`
- ❌ **不**扩 `perm` 4 值锁 · **不**扩 `CATEGORY_*` 3 类 · **不**改 `CohIfBundle`
- ❌ **不**改冻结 schema · **不**新增 ABI · **不**新增 invariant 编号数
- ❌ **不**追加任何 ADR 的 `## Status Update`(三项裁决 + 锁表修订**已签发**)
- ❌ **不**裁决 **R-T4**(实例 `perm` 数组填 3 值还是 4 值)—— 属第 1 个 change 前的前置
- ❌ **不**单独建模 `Trunk`(**决策口径**:`Table 8.1` 权限等价 ⇒ **折叠**为 `Branch`)
      ⇒ ⚠️ **语义损失已登记**;⚠️ **不得**写成「过渡态」
- ❌ **不**产出任何性能结论
