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
> **不新建 header**、**不抽** `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）（假想文件名，本方案不创建）。

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
| B · 抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）（假想文件）供装配闸共用 | ❌ 否决 | 共用消费者**实测 0 个**（F10）⇒ 半死代码 |
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
- ⛔ 不抽 `include/tlm/cep/fabric_vc_consistency.hh`（假想文件，已否决）（假想文件）（T-C2-① = A）。
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

---

## 9. K-1 裁决 —— `R-1` / `R-2` **不进 C-4**，落 `docs/architecture/cep/fabric-transport.md` 的「诚实边界」段

> **触发**: C2 improvement 草案（`.rddf/improvements/cpptlm-cep-transport-vc-consistency.md`）的 `K-1` 待裁项 —— metis gap 分析指出「C-4 的未检验项没有可扩展的落点」。**architect 裁定，用户认可**（2026-11-12）。

### 9.1 决策

> `R-1`（`plane_binding` 漏登记 ⇒ C-1′-S 漏判）与 `R-2`（M-1 不可判）登记进 `docs/architecture/cep/fabric-transport.md` 已有的「**诚实边界**」段（`:106-112`）。
> ⛔ **不扩 `UntestedRegistry`** · ⛔ **不新建独立登记面**。

### 9.2 核实的决定性事实

| # | 事实 | 来源 |
|:--:|---|---|
| A1 | `UntestedRegistry` **本来就是可注入参数**，注释逐字写「由调用方注入 ⇒ 本仓当前**唯一** M-2 使用方默认取 `default_untested_registry()` … **将来出现第二个 M-2 使用方时不必改契约头**」 | `include/tlm/cep/collective_plane_contract.hh:1374-1378` |

⇒ **「扩展 registry 来源」是伪需求** —— 注入点早已存在。真问题是「**该不该**往里塞」，不是「能不能」。

### 9.3 为什么（语义类别不同 —— 决定性）

`ADR-CEP-01` §E.3.4 的 U-1 / U-2 属同一类：

| 项 | 内容 | 类别 |
|---|---|---|
| U-1 | 「VC 隔离确实避免协议级死锁」(H6) —— M-2 **主动放弃**隔离 ⇒ 永远无法检验 | **证书机制本身不可检验** |
| U-2 | 同 VC 内 coherence 与 collective 的相互阻塞 —— VC 级依赖图**不表达** | **机制表达力不足** |
| **R-1 / R-2** | 自声明数据可能漏 / M-1 无载体 | **判据适用范围边界**（≠ 机制不可检验） |

⇒ 混在一起会让 **C-4 的语义从「证书机制的诚实登记」漂移成「一切已知缺陷登记」** ⇒ 此后任何残余都会往 C-4 塞 ⇒ C-4 失去鉴别力。

**边际成本为 0**：`docs/architecture/cep/fabric-transport.md` **本来就在 C2 的修改清单内**（判据③ R5 表同步 + R1 段订正 + `R1-c` 处置）⇒ 五步链成本已计入。

### 9.4 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **A · 进 `docs/architecture/cep/fabric-transport.md` 的诚实边界段** | ✅ **采纳** | 语义对位；文档已在 C2 清单内 ⇒ 边际成本 0；与既有 R1 边界登记同处 |
| B · 扩 `UntestedRegistry` 使其含 R-1/R-2 | ❌ 否决 | 注入点已存在（A1 ⇒ 无技术障碍）⇒ 反对理由纯语义：会把「判据边界」混进「机制不可检验」，稀释 C-4 |
| C · 新建独立登记面 | ❌ 否决 | **无消费者**（只有 2 条且不会再增长）+ 新文件是否入冻结集又需一裁 ⇒ 为不存在的增长预留 |

### 9.5 代价（须在 C2 的文档改动中如实写明）

- `R-1` / `R-2` **不会被任何闸校验**（C-4 的「存在 / 非空 / 未被冒充成已满足」机器检查**不覆盖**它们）⇒ 属**文档级登记**。
- ⚠️ **必须在文档里写明地位差异**：U-1/U-2 有 C-4 机器守门「未被冒充成已满足」;R-1/R-2 **没有**。⛔ 不得让读者以为 R-1/R-2 也有机器守门。

---

## 10. K-2 裁决 —— 锚点**按主题拆两处**，tag 用 `R1-b` / `R1-c`

> **触发**: 同一草案的 `K-2` 待裁项 —— 「(b) / (C-1′-S) 的规约锚点挂哪」。**architect 裁定，用户认可**（2026-11-12）。

### 10.1 决策

| 判据 | tag | 锚点（写进哪份 ADR 的 `## Status Update`） |
|---|---|---|
| (a) 类集合一致性（既有） | `[CP-03-VC-CERTIFICATE/C-1′]` **不动** | `ADR-CEP-01` §E.3.1 的 C-1′（既有） |
| **(b) ordering 一致性** | **`[CP-03-VC-CERTIFICATE/R1-b]`** | **`ADR-CEP-12`** 的 `## Status Update` |
| **(C-1′-S) scope 完备性** | **`[CP-03-VC-CERTIFICATE/R1-c]`** | **`ADR-CEP-01`** 的 `## Status Update` |

### 10.2 核实的决定性事实

| # | 事实 | 来源 |
|:--:|---|---|
| A2 | `INV-CEP-12-IC2` 全文 = 「`ordering_assumption` 的 `named_network` key **必须**解析到唯一 `vc_id`，否则配置加载 fail」 ⇒ **只**保证 key 可解析，**完全不涉及跨实例一致性** | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md:160` |
| A3 | 但 `include/tlm/cep/protocol_instance.schema.json` 的 description 写「…一致性由**装配期校验**承担（per `INV-CEP-12-IC2`）」 | `include/tlm/cep/protocol_instance.schema.json:180` |
| A4 | 现有 tag 纪律：四条证书**共用** `CP-03-VC-CERTIFICATE`，靠 `detail` **子标记**区分，形态 = `[CP-03-VC-CERTIFICATE/<tag>]`;测试用 `rfind("[CP-03-VC-CERTIFICATE]", 0) == 0`（**带 `]`**） | `collective_plane_contract.hh:873-888` + `ualink_inc_state_spec.hh:597` + `test/test_cep_ualink_inc.cc:465-536` |
| A5 | `fabric-transport.md:145` 写「R1-a 有载体但域未覆盖;**R1-b / R1-c** 无载体」，而 **`R1-c` 全文从未定义**（仅此一处出现） | `docs/architecture/cep/fabric-transport.md:140,145` |

🔴 **A3 + A2 合起来是一个已存在的决策-实现缺口**：决策文本宣称「一致性由装配期校验承担（per IC2）」，而 **IC2 并不承担该职责** ⇒ 该承诺**至今没有承担者**。⇒ (b) 的 SU **不是新增决策**，而是**兑现已有文本的承诺**。

⇒ A4 保证 `[CP-03-VC-CERTIFICATE/R1-b]` **不会**被 `[CP-03-VC-CERTIFICATE]` 或 `/C-1′]` 误命中（`]` 位置不同）。
⇒ A5 保证 `R1-c` 的语义空缺**本来就在等这个判据**。

### 10.3 为什么这样拆

**(b) → `ADR-CEP-12`**：决策 5 就是 `ordering_assumption` 的定义。SU 内容 = 「决策 5 所称的『装配期校验』由 `R1-b` 承担;其作用域 = 声明 M-2 的 network（per `ADR-CEP-01 §E.3.3` 的条件化口径）;⚠️ `INV-CEP-12-IC2` 只保证 key 可解析，不承担跨实例一致性」。
**⛔ 不改 `include/tlm/cep/protocol_instance.schema.json` 的 description** —— 它是决策 5 的转录;在 SU 里澄清即可，符合「ADR 不可变 + 追加 SU」。

**(C-1′-S) → `ADR-CEP-01`**：§E.3.3 定义了「参照集 = 两域并集」+「不一致 ⇒ 该网不得用 M-2」，但**从未规定「哪些 network 需要被检查」**。SU 内容 = 「`E.3.3` 的适用域由 `certificate_scope` 声明;新增 `R1-c` 校验其完备性：对每个 `plane_binding` 指向 `is_local_model == false` 的 plane 的实例，其 `vc_binding` 中的每个 **`(plane, network)` 对**必须在 `certificate_scope` 内」。

**tag 形态**：符合既有纪律（A4）—— 共用 `CP-03-VC-CERTIFICATE` 前缀 + `detail` 子标记，**⛔ 不新造顶层归因 id**，**⛔ 不用 `C-1′-b`**（`ADR-CEP-01` 的 `C-1′` 只定义槽位继承 = (a)，借它的名字会掩盖这是新决策）。

**(a) 保留 `/C-1′` 不动**：它是 §E.3.1 四条证书体系的既有术语，改它会破坏 `test/test_cep_ualink_inc.cc:477/489` 的既有 fixture 且**无收益**。⇒ 只在 `docs/architecture/cep/fabric-transport.md` 写明映射「**`R1-a` ≡ `C-1′`**」。

**`R1-c` 悬空引用自然收口**（A5）：`R1-c` 就是 C-1′-S ⇒ C2 的 AC-7 不再是「定义它还是删除」，而是「**它已由本 change 定义**」。

### 10.4 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **A · 按主题拆两处（`ADR-CEP-12` + `ADR-CEP-01`），tag = `R1-b`/`R1-c`** | ✅ **采纳** | 主题归属准确；(b) 能兑现 A3 的既有承诺;C-1′-S 是 `E.3.3` 的域判据化;`R1-c` 悬空引用同时收口 |
| B · 两条都塞进同一份 ADR 的 SU | ❌ 否决 | 会让一份 ADR 承载另一个主题的决策（`ADR-CEP-12` 不定义 `certificate_scope` 的语义，`ADR-CEP-01` 不定义 ordering 语义） |
| C · tag 用 `C-1′-b` / `C-1′-c` | ❌ 否决 | 借身份：`C-1′` 在 `E.3.1` 里只指槽位继承;且与 `docs/architecture/cep/fabric-transport.md` 的 `R1-b/R1-c` 编号**两套并存** |
| D · 新造顶层归因 id（如 `CP-13`） | ❌ 否决 | 违反既有纪律（`collective_plane_contract.hh:873-877`「四条共用 `CP-03-VC-CERTIFICATE`」+ `:876`「⛔ 不新增 `ContractRule`」） |

### 10.5 代价（本裁决的主要成本）

1. **两份 ADR 各追加 1 则 SU** ⇒ `docs/adr/ADR-CEP-01-*.md` 与 `docs/adr/ADR-CEP-12-*.md` **均在冻结集**（glob `docs/adr/ADR-CEP-*.md`）⇒ **C2 的五步链从 1 处（`docs/architecture/cep/fabric-transport.md`）增至 3 处**。⚠️ 须在 C2 的 `openspec/changes/<name>/tasks.md` 显式列出。
   - 缓解：三处可在**同一次** `cep_freeze_check.sh --update --authorized-by` 内一并重算;但**产物 commit 必须先于 `--update`**（C1 已验证的顺序，per 台账 Entry #342）。
2. **`R1-a` / `C-1′` 双命名共存** ⇒ 文档须显式写映射，否则读者会以为 `R1-a` 无载体。
3. 判据 ①（既有 (a)）的 tag 不变 ⇒ 既有 fixture 零改动 ✓;但也意味着 **4 条既有证书 + 2 条新判据 = 6 个 tag 形态** ⇒ C2 的 `AC-14`（tag 冲突回归）必要性上升。

---

## 11. 对 C2 improvement 草案的连带修订清单（planner 回填时须逐条落）

| 草案位置 | 修订 | 依据 |
|---|---|---|
| `状态` | ⛔ 阻塞 → **可立项**（K-1/K-2 已裁） | §9 / §10 |
| 待裁项 K-1 / K-2 | 改为「已裁（per 本文件 §9 / §10）」 | §9 / §10 |
| What Changes #2 / #3 的 tag | 钉死 `R1-b` / `R1-c` | §10.1 |
| What Changes #4 | 落点 = `docs/architecture/cep/fabric-transport.md` 的**诚实边界段**（⛔ 不是 C-4 / 不是新文件） | §9.1 |
| AC-3 / 4 / 5 / 10 / 11 / 12 | 红因 tag 钉死 ⇒ 从「只验红」升级为「**验红 + 验红因**」 | §10.1 |
| AC-7 | `R1-c` 不再是「定义或删除」，而是「**已由本 change 定义**」 | §10.3 |
| **AC-8** | 五步链从 **1 处 → 3 处**（`docs/architecture/cep/fabric-transport.md` + `ADR-CEP-01` + `ADR-CEP-12`） | §10.5 |
| **新增 AC** | `R-1`/`R-2` 在「诚实边界」段**可 grep 命中**且 **⛔ 不含「已验证 / 已满足」字样**（与 C-4 同型的文字守门） | §9.5 |
| **新增 AC** | 文档显式写「`R1-a` ≡ `C-1′`」映射 + 「U-1/U-2 有机器守门 / R-1/R-2 **没有**」的地位差异 | §9.5 / §10.5 |

---

## 12. 边界说明

- 本文件 §1~§8 = **W0** 产出（per 台账 Entry #344）;§9~§12 = **K-1/K-2 裁决**（architect 产出、用户认可后由 builder 落盘，per 本仓 `.rddf/reviews/` 不在 `arch_paths` 的分工）。
- ⛔ **本文件不改任何 ADR / 声明数据 / 实施代码**;§10 的 SU 内容将在 **C2 实施期**写入 ADR（届时触发冻结面五步链）。
- ⚠️ **本文件不是 OpenSpec change** ⇒ 不占 D1 槽位。
