# cpptlm-cep-tilelink-state-spec: TileLink-C(TileLink Tree Manager)—— **规约层**(3 稳定态 + 4 事件类别 + `perm` 3→3 投影 + 非对称结构断言)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#11** 的**第 0 个 change**(两段式;第 1 个 = 实现层 `cpptlm-cep-tilelink-tree-manager`)
> **口径裁决**: 台账 **Entry #134** / **#135** / **#138** + `ADR-CEP-11` / `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17` 的 `## Status Update(2026-11-01)`
> **硬门产物**: `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(Task 0 公开来源普查 ⇒ **🔵 硬门通过**)
> **规划输入**: `.rddf/improvements/cpptlm-cep-tilelink-tree-manager.md`(2026-11-01,7 段模板,HARD-GATE 通过)
> **估时**: 4 周(规约层)= **1 周普查硬门(已完成)** + 3 周载体 / 锚点核对表 / 生成器 / 编译期锁
> **KPI**: Stage 1 = **1 slot**;Proposed 并发 **0/3 → 1/3**
> **冻结面**: **零正文改动**(M1 40 件零 diff)。本 change **只消费** `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17` 的三项裁决(Entry #138 签发)与 `ADR-CEP-11` 的锁表修订(Entry #135 签发)⇒ **不追加任何 Status Update**

---

## Why

### ① 平台缺的不是「又一个 adapter」,而是**第二种一致性拓扑**

CEP 的差异化命题是**跨协议对比**;已落地的 4 个实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0`)**全部是中心化目录**语义。
TileLink-C 是**树形**语义(权限沿树递归、序列化点在 Tip)⇒ **不立它,平台只覆盖了一半的一致性世界**
⇒ 无法回答「树形 vs 中心化目录」的延迟 / 复杂度 / 可扩展性对比(台账 §0 Goal 的核心命题)。

### ② 现有抽象**已为它留好位置**,但**零使用**

`LookupResult::ESCALATE_UP`(树形向上递归)与 `LookupHazard::CHAIN`(parent chain 加入)是
`ADR-CEP-04` 为 TileLink **专设**的两个枚举值 ⇒ 但 3 个行为实现(`FullMapDirectory` /
`SparseSnoopFilter` / `HierarchicalDirectory`)**一个都没用** ⇒ 平台**声明了**树形语义却**从未执行**过它。
⇒ 这与 R11 的「声明超前于实现」是**同类问题的镜像**(那次「声明有、实现无」;这次「**枚举有、语义无**」)。

### ③ 为什么仍需**规约层**这一层(而不是直接写实现)

`ADR-CEP-04 §5` 已明载:DCOH 与 TileLink 会「**被迫对返回字段善意撒谎**」——
`lookup` 假设单次同步回答全局问题,但树形语义下它**做不到**。
⇒ 实现层若不先有规约,必然**自推**一套「`TreeManager` 该返回什么」的语义
⇒ 落回 #9 的老路(「看似完整实则杜撰」)。
⇒ 本 change 交付的是**载体**(状态集合 + 转移表 + 投影 + 结构断言 + 逐格 `basis`),**不是** adapter。

### ④ Task 0 硬门**已通过**(🔵 —— 比 #10 更强)

| 项 | 队列 #10(CXL) | **队列 #11(TileLink)** |
|---|---|---|
| 规范正文 | ⛔ **全程禁读**(ECA §3 + AI AUP) | ✅ **可读**(SiFive 明示 "free and open standard") |
| basis 来源 | 公开**非规范**来源(论文 / 博客) | **规范正文本身**(D1/D2/D3 三份 `pin=strong`,含 sha256) |
| 逐格覆盖 | 68 格中 **42 格 `Unknown`** | **12 格全 Known、零 `Unknown`** |
| 硬门结论 | 🔵 **有条件通过** | 🔵 **通过** |

⇒ **载体质量显著优于先例** ⇒ 本 change 的**逐格 `basis` 可全部 `pending=false`**。

---

## What Changes

| 载体 | 内容 |
|---|---|
| `include/tlm/cep/tilelink_state_spec.hh`(新) | TileLink-C **3 稳定态**(`Nothing` / `Branch` / `Tip`;⚠️ `Trunk` **折叠**为 `Branch` —— **决策口径**(`Table 8.1` 权限等价),**非**「过渡态」;代价见 `design.md` §1.2bis)/ **4 事件类别**(`Cap` / `Grow` / `Prune` / `Report`)/ **12 格转移表** / **3→3 `perm` 投影** / 逐格 `basis{kind, pending, ref, note}` / 偏离清单 / **编译期锁** + **非对称结构级断言**(TileLink 形态) |
| `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md` | ⚠️ **已落**(2026-11-01)—— basis 的**唯一人工审计面**;载体 `ref` **只指向本文件的条目号** |
| `scripts/test/generate_tilelink_anchor_checklist.sh` + `.cc`(新) | **锚点摘要的逐格核对表生成器**(+ `--check`,手改必红)—— ⚠️ 照 `generate_cxl_anchor_checklist.cc` 同款机制 |
| `test/test_cep_tilelink_state_spec.cc`(新) | 契约测试 + **反向 fixture**(加回 `Trunk` ⇒ 必红;毒化非对称断言 ⇒ 必红;某格改 `Unknown` ⇒ 必红) |
| `openspec/changes/cpptlm-cep-tilelink-state-spec/`(新) | 本 change 4 件套 + delta spec(capability `cep-tilelink-state-spec`) |

### 明确不做(本 change 内)

- ❌ **不**实现 adapter / `TreeManager` tracker 行为类 / 实例声明 / 装配(⇒ 第 1 个 change)
- ❌ **不**扩 `perm` 4 值锁(值域不变;本协议族只是**用不到** `UNIQUE`)· **不**扩 `CATEGORY_*` 3 类
- ❌ **不**改 `CohIfBundle` · **不**改冻结 schema · **不**新增 ABI · **不**新增 invariant 编号数
- ❌ **不**单独建模 `Trunk`(**决策口径**:`Table 8.1` 中 `Trunk` 与 `Branch` 权限相同 ⇒ **折叠**为 `Branch`)
  ⇒ ⚠️ **语义损失已登记**(副本存在性 / 脏数据 / 树位置,见 `design.md` §1.2bis);**不得**表述为「`Trunk` 是过渡态」
- ❌ **不**追加任何 ADR 的 `## Status Update` —— 三项裁决(Entry #138)与锁表修订(Entry #135)**已签发**,本 change 只**引用**
- ❌ **不**裁决 **R-T4**(「`perm` 4 值锁 = 值域锁 还是 每实例用满 4 值」)—— 该口径只影响**实例声明**(第 1 个 change)
- ❌ **不**产出任何性能结论

### ⚠️ 已登记的不兼容与判据风险(**必须**在规约层登记,不得留到实现层)

| # | 项 | 处置 |
|---|---|---|
| 1 | **`AC-05` 判据族对 TileLink 恒红** —— `check_adapter_self_consistency` 硬编码 `e_write_target_state != 3 ⇒ FAIL` 且**无 SKIP 分支**;TileLink 无 `E` 态 ⇒ 实现层**必然红** ⇒ 有「填假值换绿灯」的诱因 | **规约层登记**(本 change);**族无关化**落第 1 个 change;⚠️ **不得**通过填假的 `e_write_target_state` 使其变绿 |
| 2 | **锚点核对生成器不可照抄 CXL 版** —— `is_wellformed_id` 只认「`prefix` + **全数字**」;TileLink 锚点用 `| **D1** |`(加粗)与 `R-T1`(非纯数字)⇒ 合法集为**空** ⇒ 逐格校验**恒真空转** | 生成器须 ① 容忍 Markdown 加粗 ② 接受 `R-T<n>` ③ **逐格**白名单比对;并须有**反向 fixture** 证明它**不是**空转 |
| 3 | **`R-T4`(实例 `perm` 填 3 值还是 4 值)不在本 change** —— 填 4 值会使 `exposes_unique_perm = true` ⇒ `INV-CEP-08-IC2` 走 hierarchical 分支 ⇒ **PASS 但语义为假** | 留第 1 个 change 前由 architect 裁决(**倾向值域锁 ⇒ 填 3 值**) |

---

## Acceptance

- [ ] **AC-1** 规约载体的稳定态集合 SHALL **恰为 3 个**(`Nothing` / `Branch` / `Tip`),且 `Trunk` **SHALL NOT** 出现在稳定态集合中
      (反向 fixture:加回 `Trunk` ⇒ **必须因正确原因红**)
- [ ] **AC-2** 事件词表 SHALL **恰为 4 类**(`Cap` / `Grow` / `Prune` / `Report`),且每类 SHALL 有 D1 §8.1.4 `Table 8.3` 的**枚举转移**支撑
- [ ] **AC-3** 转移矩阵 SHALL 为 **3 × 4 = 12 格**,且 **`Unknown` 格数 SHALL 为 0**
      (编译期锁 + 运行时断言**双证**;反向 fixture:把某格改成 `Unknown` ⇒ **必须因正确原因红**)
- [ ] **AC-4** `perm` 投影 SHALL **恰为 3 行**且逐行为 `Nothing→NONE` / `Branch→READ` / `Tip→WRITEBACK`;`UNIQUE` **SHALL NOT** 出现在投影结果中
- [ ] **AC-5** 非对称断言 SHALL 用 **TileLink 形态**(「`Grant`/`GrantData` **只**在 D 通道、`Probe` **只**在 B 通道」);⚠️ **SHALL NOT** 复用队列 #10 的 CXL 形态模板
- [ ] **AC-6** 逐格 `basis` SHALL 全部指向锚点摘要的**真实条目号**;载体与摘要任一侧被改 ⇒ **必须因正确原因红**(交叉核对生成器)
- [ ] **AC-7** `openspec validate cpptlm-cep-tilelink-state-spec --strict` rc=0
- [ ] **AC-8** **冻结面零 diff**(`git diff --stat -- docs/adr/ docs/architecture/ openspec/specs/` = 空)
- [ ] **AC-9** 每处活文档的 KPI baseline 表述 SHALL 与 `openspec list --json` 实测**一致**(建档时 **0/3 → 1/3**;归档时 **1/3 → 0/3**;⚠️ **7 处**同步,含 obj §5.3)
- [ ] **AC-10** 判据 SHALL 遵守 4 铁律(正控 / 反向 fixture 因正确原因红 / 不用编译红充当 ctest 判据 / 状态变迁后重新基线化)

---

## Capabilities

| Capability | 类型 | 状态 |
|---|---|---|
| `cep-tilelink-state-spec` | 新增 | ⬜ 本 change 交付(归档后并入 `openspec/specs/cep-tilelink-state-spec/spec.md`) |

**新增 invariant 数 = 0** —— 本 change 只**消费**既有 66 条。

---

## Impact

| 面 | 影响 |
|---|---|
| **`ADR-CEP-11` §3 锁表** | **零改动**(Entry #135 已签发;本 change 只**引用** `tilelink-c-1.7.1`) |
| **`ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17`** | **零改动**(Entry #138 已签发;三项裁决只**被引用**) |
| **`ADR-CEP-02` / `ADR-CEP-04` / `ADR-CEP-05` / `ADR-CEP-08`** | **零改动** —— 只**消费**其冻结契约 |
| **`perm` 4 值锁** | **零改动** —— 值域不变;TileLink-C 只是**用不到** `UNIQUE` |
| **`CATEGORY_*` 3 类** | **零改动** |
| **冻结面(M1,40 件)** | **零 diff** ⇒ provenance 维持通过 |
| **KPI 并发** | **0/3 → 1/3**(本 change 占 1 个)⇒ 归档释放(→ 0/3)⇒ 第 1 个 change 再占 |
| **跨仓** | **零**(不新增 network、不改 ArchForge 声明) |
