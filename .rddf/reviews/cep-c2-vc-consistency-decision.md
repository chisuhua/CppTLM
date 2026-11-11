# C2 方案 · 传输层 VC 语义一致性判据落地（W0 产出）

> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md` **§13.6 的 W0**（「architect 出 C2 方案（`T-C2-⓪` + `T-C2-①`）+ 裁 U-3」）
> **作者**: `architect`（2026-11-12）· **落盘**: `builder`（`.rddf/reviews/` 不在 `arch_paths` 内 ⇒ architect 无权写该目录，per 角色矩阵「架构文档范围」）
> **认可**: 用户裁决 **「选 A」** = 认可本方案 + 由 builder 誊进本目录 + 随后 C2 立项
> **边界**: ⛔ **不改任何 ADR 正文** · ⛔ **不改任何声明数据**（`include/tlm/cep/protocol_instances.json` / `include/tlm/cep/collective_planes.json` / `include/tlm/cep/fabric.json`）· ⛔ **无实施代码**（W0 只出方案）
> **上游**: `ADR-CEP-01` §E.3.1 / §E.3.3 · `ADR-CEP-12` 决策 5/6 · `docs/architecture/cep/fabric-transport.md` R1/R5 · 台账 §13.3（C2 定义）· §13.6（串行执行序）· **Entry #344**（W0 回执）

---

## 1. 事实基线（全部实测，可逐条复核）

| # | 事实 | 值 | 复核命令 / 来源 |
|:--:|---|---|---|
| F1 | `certificate_scope` | **1 项** = `{plane: FabricCollectivePlane, network: ualink_fabric}` | `python3 -c "import json;print(json.load(open('include/tlm/cep/collective_planes.json'))['vc_reservation_declaration']['certificate_scope'])"` |
| F2 | `compute_noc` 绑定的实例 | **7 个**（`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c` / `nvlink_4`） | `include/tlm/cep/protocol_instances.json` |
| F3 | `compute_noc` 的一致性 | ❌ **不一致** —— `tilelink_c` 在 vc **1 / 2 / 3** 与其余 6 个不同（`snoop=1` / `snoop_resp=2` / `response=3` vs 反之） | 同上 |
| F4 | `ualink_fabric` 的一致性 | ✅ **(a)(b) 全一致**（3 个实例：`moesi_gpu` / `mesif` / `ualink_4`） | 同上 |
| F5 | `plane_binding` | `moesi_gpu` / `mesif` = `null` · `nvlink_4` = `LocalCollectivePlane` · `ualink_4` = `FabricCollectivePlane` | `include/tlm/cep/collective_planes.json` |
| F6 | plane 注册表 | **3 项**；**仅** `FabricCollectivePlane` 的 `is_local_model == false` | `include/tlm/cep/collective_plane.hh:798-804` |
| F7 | `vc_count` | `compute_noc` = **4** · `ualink_fabric` = **4** | `include/tlm/cep/fabric.json` |
| F8 | **C-1′ 已有实现的判据逻辑** | **已与 network 无关** —— 遍历 `certificate_scope` 的每项、逐 network 比对**全部有 `vc_binding` 的实例**，不一致 ⇒ `FAIL`；无实例 ⇒ `FAIL`（禁 vacuous PASS） | `include/tlm/cep/collective_plane_contract.hh:905-946` |
| F9 | **`ordering_assumption` 的判定** | **全文件 0 处** ⇒ 维度 (b) **完全无判定** | `grep -c ordering_assumption include/tlm/cep/collective_plane_contract.hh` = 0 |
| F10 | **多协议实例同 fabric 的装配场景** | **0 个** —— 8 个 `configs/cep_*` 中**每个恰好 1 个** `protocol_instance` | 逐个 config 提取 `protocol_instance` 取值 |
| F11 | schema 对 (b) 的约束强度 | `ordering_assumption` **必填**，但每个 network 仅 `minProperties: 1` ⇒ **不约束逐 VC 完备性** | `include/tlm/cep/protocol_instance.schema.json:180-197` |
| F12 | C-2（`vc_binding` 覆盖） | 已校验「继承映射覆盖 `vc 0..vc_count-1`」⇒ (b) 的覆盖校验可**与之对称** | `include/tlm/cep/collective_plane_contract.hh`:948-967 |

### 1.1 对台账 §13.3 表述的一处更正

§13.3 写「(a) 已有载体但**适用域受限**（`certificate_scope` 仅 `ualink_fabric`）⇒ `compute_noc` **无判定**」。

- 「**适用域受限**」— 准确。
- 「`compute_noc` **无判定**」— **读法需更正**：不是「判据逻辑不支持」，而是「**`compute_noc` 不在 `certificate_scope` 里**（因为它不用 M-2）」。
- ⇒ **根因不是判据能力，而是作用域是自声明数据、无独立真源校验**（本方案的 C-1′-S 就是补这个洞）。

---

## 2. T-C2-⓪ 裁决 —— 判据作用域 = **条件化口径**（否决全局口径）

### 决策

> 判据 = 「**在 `certificate_scope` 内（该 network 声明采用 M-2 共享 VC 池）的 network**，其全部有 `vc_binding` 的实例必须逐 VC 一致」。
> 不在 `certificate_scope` 内的 network **允许不一致**。

### 为什么（三条独立硬理由，任一条即足以否决全局口径）

**R1 · 既有裁决已经这么定（且 ADR 不可变）**

`ADR-CEP-01` §E.3.3 原文：

> 「**若该 network 上存在实例间映射不一致 ⇒ 该 network 不得使用 M-2**（须回 M-1 或先消除不一致）。」

这是**条件化**表述：不一致 ⇒ **禁止共享池**，**不等于** ⇒ **非法拓扑**。全局口径（「任何混排都不得有不一致」）与已签发 ADR 直接冲突 —— 而 ADR 签发后不可改（只能追加 `## Status Update`），故全局口径**不是本 change 可选项**。

**R2 · 全局口径与平台目的冲突**

CEP（Coherence Exploration Platform）的存在意义 = **跨协议对比**：让 CHI / CXL / TileLink-C / NVLink / UALink 的拓扑在同一 framework 下可比较。各协议的 VC 分配是**协议身份的一部分**，天然不同。若要求「任何混排都不得有不一致」，等于要求不同协议先长得一样才能进同一张网 —— 那就取消了对比的前提。

**R3 · 全局口径落地即红，且唯一出路是造假**

`compute_noc` 现在就不一致（F3）⇒ 若采全局口径，实施当天闸即红，只有两条出路：
- 改 `tilelink_c` 的 `vc_binding` ⇒ **篡改协议映射**（数据造假）；
- 把 `tilelink_c` 踢出 `compute_noc` ⇒ **篡改拓扑**（数据造假）。

⇒ 这两条都触犯「⛔ 不得为了让闸变绿而改数据」这一治理底线。

### 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **A · 条件化口径（`certificate_scope` 内一致）** | ✅ **采纳** | 与 `ADR-CEP-01 E.3.3` 一致；不需改任何声明；能容纳合法的协议差异 |
| B · 全局口径（任何混排都一致） | ❌ 否决 | 与已签发 ADR 冲突；与平台目的冲突；落地即红且只能靠改数据变绿 |
| C · 完全不定判据（维持现状） | ❌ 否决 | 维度 (b) 无判定 + 作用域自声明无校验 = 「登记了但没有牙齿」（C1 刚消灭的缺陷类在此复发） |

### 代价

- 判据**只覆盖声明了 M-2 的 network** ⇒ 未声明 M-2 的 network 的混排语义仍只有**声明正确性**作保（见 §5 残余项）。
- 「不一致 ⇒ 只能回 M-1」这条实际约束仍**靠人工**（本方案不实施 M-1 回退机制）。

---

## 3. U-3 裁决 —— `compute_noc` 上 `tilelink_c` 的差异 **不是缺陷**

> **裁定**：`compute_noc` 上 `tilelink_c` 与其余 6 实例的 VC 映射差异，**是已登记的合法协议差异**，⛔ **不改任何声明**。

**依据**（不伪造协议依据，只用既有裁决 + 平台目的）：

1. `ADR-CEP-01` §E.3.3 **已登记**该事实并只裁决「该网不得用 M-2」，**未**裁决其为非法；
2. `ADR-CEP-12` 决策 6 明确「VC 分配是 **per-network** 的」⇒ 不同实例在同一 network 上的映射由各自协议决定；
3. 本方案 T-C2-⓪ 的条件化口径使该差异**不构成判据失败**（`compute_noc` 不在 scope）。

**表述订正**：该事项应从「**设计风险**」重述为「**已裁决的合法差异 + 该网不得用 M-2**」。⚠️ 本仓**未**为「`tilelink_c` 的 vc1 承载 snoop」提供协议规范引用 ⇒ **不得**在文档中把它写成「协议规定」（那是未经核实的断言）；只写「已按其自身 `vc_binding` 登记，本仓不主张其规范依据」。

---

## 4. T-C2-① 裁决 —— **扩展 C-1′ 适用域**（否决「抽 network-agnostic 判据」）

### 决策

> 在 `include/tlm/cep/collective_plane_contract.hh` 的 **V6-d** 内实现三项并列判据：**(a) 既有 C-1′** + **(b) 新增 ordering 一致性** + **(C-1′-S) 新增作用域完备性**。
> **不新建 header**、**不抽** `include/tlm/cep/fabric_vc_consistency.hh`（假想文件名，本方案不创建）。

### 为什么（否决 B 的决定性理由 = 实测空场景）

台账 §13.3 说「两条路都合法」，**但实测把天平压到 A**：

| B（抽 network-agnostic 判据）的唯一卖点 | 实测 |
|---|---|
| 「供装配闸 `include/tlm/cep/instance_assembly_gate.hh` 与 CP-03 **共用**」 | **0 个 config 有多协议实例**（F10：8/8 全是单 `protocol_instance`）⇒ **该共用点是空场景** |

⇒ 为不存在的消费者预留实现 = 「**for future use**」死代码 —— 正是 C1 的 **MUST-5**（删 R-f 的「优先探测真实 run」半死分支）刚清掉的那一类。

**其它支持 A 的理由**：

- **禁双实现自动满足**：A 只有**一处实现**，不存在重复（B 也只是把同一实现搬个位置，并不减少实现数）。
- **同域 / 同消费者 / 同数据源**：(a)(b)(C-1′-S) 都在 `certificate_scope` 域内、都归 V6-d、都读 `include/tlm/cep/protocol_instances.json` + `include/tlm/cep/fabric.json` + `include/tlm/cep/collective_planes.json` ⇒ 放一处是**最小改动**。
- **未来若真出现多实例装配**（= 真实需求），再抽 header 不迟 —— 那时抽取有真实消费者，不是投机。

### ⭐ 真正的方法论增量：把作用域从「自声明」改为「自校验」

**C-1′-S（scope 完备性）判据**：

> 对每个 `plane_binding` 指向 **`is_local_model == false`**（= 承载真实 fabric 集合流量）的 plane 的实例，该实例 `vc_binding` 中出现的**每个 network** 必须出现在 `certificate_scope` 内。

| 场景 | 结果 |
|---|---|
| **今天**：`ualink_4 → FabricCollectivePlane` → networks = `[ualink_fabric]` ⊂ scope | ✅ **绿**（无误报） |
| **变异**：把某 `compute_noc` 上的实例绑到 `FabricCollectivePlane` | 🔴 **红**（`compute_noc` 不在 scope） |

⇒ 这关闭了 **(a) 现有实现最大的隐藏缺口**：`certificate_scope` 写漏一个 network ⇒ 判据**静默不覆盖**该网（且**不会有任何闸抱怨**）。

### 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **A · 扩展 C-1′ 适用域（+ 自校验 scope）** | ✅ **采纳** | 单一实现；无空场景预留；改动集中在一个文件 |
| B · 抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件）供装配闸共用 | ❌ 否决 | 共用消费者**实测 0 个**（F10）⇒ 半死代码 |
| C · 只加 (b)、不动作用域 | ❌ 否决 | 作用域自声明的洞不补 ⇒ 「登记了但没有牙齿」在 (a) 侧继续存在 |

### 代价

1. `certificate_scope` 从「声明」变成「**声明 + 受校验**」⇒ 将来新增 fabric 承载 plane 的绑定而忘更新 scope ⇒ **红**（期望行为，但增加维护面 ⇒ 须在 `docs/architecture/cep/fabric-transport.md` 写明）。
2. 需给快照加 `per_network_orderings` 字段（当前 `VcBindingSnapshot` 不收集 `ordering_assumption`，F9）⇒ 同文件内新增字段 + 收集逻辑，改动集中但触及 CP-03 共享头。

---

## 5. (b) 判据的语义钉死（防 vacuous PASS 陷阱）

| 维度 | 决定 | 理由 |
|---|---|---|
| **比较范围** | 与 (a) **同 scope**（`certificate_scope` 内的 network） | 否则 `compute_noc` 立刻红（同 T-C2-⓪ 的 R3） |
| **比较方式** | 逐 VC **全等**（⛔ 不是子集 / 不是求交） | `p2p_ordered` 与 `unordered` 混用同一 VC ⇒ 保序语义被稀释，这正是要防的 |
| **VC 完备性** | 必须覆盖 `vc 0..vc_count-1`；缺失 ⇒ **FAIL** | 与 C-2（`vc_binding` 覆盖，F12）**对称**；schema 只要求 `minProperties: 1`（F11）**挡不住** ⇒ 判据必须自己挡 |
| **无实例** | ⇒ **FAIL**（⛔ 禁 vacuous PASS） | 与 C-1′ 现有「无任何有 `vc_binding` 的既有实例 ⇒ FAIL」同风格 |
| **`certificate_scope` 为空** | ⇒ **FAIL** | 与现有 `:901-904` 同型（本 change 不改） |
| **残余不可判项** | **登记，不静默**：`plane_binding` 自身漏登记（实例实际用 `FabricCollectivePlane` 但 `plane_binding` 为 null）⇒ C-1′-S **漏判** | 与 **C-4** 同模式（「登记 ≠ 达成」）；⛔ 不得宣称「已全覆盖」 |

---

## 6. C2 验收判据（台账原 4 条 + 本方案新增 4 条）

| # | 判据 | 决定性变异（**必附**） | 来源 |
|:--:|---|---|---|
| ① | 改某实例的某 VC **消息类** ⇒ **必红** | 在 `certificate_scope` 内的 network 上做（今天只有 `ualink_fabric`）；⚠️ 若跨 scope 做也红 ⇒ **说明口径错了**（反证 T-C2-⓪） | 台账原判据 |
| ② | `ualink_fabric` 侧仍 **绿** | 基线（F4） | 台账原判据 |
| ③ | `docs/architecture/cep/fabric-transport.md` R5 表的「当前机器判定」列**同步更新**（⛔ 不留「无载体」的过时表述） | 逐字可核 | 台账原判据 |
| ④ | `ADR-CEP-01` 的 C-1′ **正文不改**；若需扩适用域 ⇒ **末尾追加 `## Status Update`** | 逐字可核（ADR 不可变） | 台账原判据 |
| **⑤（新）** | **(b) 改某实例某 VC 的 `ordering`（`p2p_ordered` ↔ `unordered`）⇒ 必红** | **(b) 的唯一决定性变异** —— 旧实现**必绿**（F9：根本没这判据）⇒ 能证明新判据有牙 | 本方案 |
| **⑥（新）** | **C-1′-S：把 `compute_noc` 上某实例绑定到 `FabricCollectivePlane` ⇒ 必红** | 红因须是「`compute_noc` **不在 certificate_scope**」，⛔ 不是「plane 未登记」 | 本方案 |
| **⑦（新）** | **(b) VC 不完备：删某实例某 VC 的 `ordering` ⇒ 必红** | schema 允许该形态（F11）⇒ 只能靠判据挡 | 本方案 |
| **⑧（新）** | **⛔ 反 vacuous**：`certificate_scope` 清空 ⇒ **必红**（不是「无判定 ⇒ PASS」） | 与现有 `:901-904` 同型，作回归基线 | 本方案 |

> **⑤ 与 ⑥ 是本 change 证明「新判据有牙」的唯一手段** —— 必须按「**旧实现必绿 / 新实现必红**」并排取证（方法同 C1 的 AC-9）。
> 判据 ① 若在 `compute_noc` 上做而红，则**判据实现错了**（scope 泄漏），须回到 T-C2-⓪。

---

## 7. 残余与不做的边界

**明确不做**：

- ⛔ 不实施 M-1 回退机制（「不一致 ⇒ 回 M-1」仍靠人工）—— 不在 C2 范围。
- ⛔ 不改 `compute_noc` / `tilelink_c` 的任何声明（U-3）。
- ⛔ 不改 `ADR-CEP-01` / `ADR-CEP-12` 正文。
- ⛔ 不抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件）（T-C2-① = A）。
- ⛔ 不把 `compute_noc` 加进 `certificate_scope`（它是 M-1 网络，不是"漏登"）。

**残余不可判项（须在 C2 的 C-4 登记面体现）**：

| # | 残余 | 为什么不可判 |
|:--:|---|---|
| **R-1** | `plane_binding` 自身漏登记 ⇒ C-1′-S 漏判 | `plane_binding` 是自声明数据，无独立真源能证明「某实例实际在用 fabric 承载 plane」 |
| **R-2** | 「不一致的 network **确实**走了 M-1」不可判 | 本仓无 M-1 配置载体 ⇒ 只能登记 |

---

## 8. W0 退出判据对照（台账 §13.6）

| 判据 | 状态 |
|---|---|
| `T-C2-⓪`（判据作用域）已裁 | ✅ §2 = 条件化口径 |
| `T-C2-①`（实现路径）已裁 | ✅ §4 = 扩展 C-1′ 适用域（+ 自校验 scope） |
| `U-3` 已裁 | ✅ §3 = 合法协议差异，⛔ 不改声明 |
| 方案落 `.rddf/reviews/` | ✅ 本文件 |
| ⛔ 不改 ADR | ✅ 0 处 ADR 改动 |
| ⛔ 不占 D1 槽位 | ✅ 本文件不是 OpenSpec change |
