---
name: cpptlm-router-vc-count-param-inert
priority: P2
phase: phase-10
category: bugfix
type: bug
状态: 🔴 **未立项(backlog)· 结论不变(2026-11-19 复核)** —— ⚠️ **复核补充**:本条的 **C++ 声明面已彻底归零** —— `cpptlm-module-param-read-declares` 迁出后,C++ 侧无残留;而 `ParamRules` 机制本身已由 change `cpptlm-module-param-migrate-batch2` 的 **T4** **整体删除**(`get_param_rules()` 的载体 `include/core/param_rules.hh` 不再存在)⇒ 「只登记不修」的结论**更强**:`vc_count` 现在连声明都不存在,只有 **json 侧**条目(`configs/param_rules/router_tlm.json`)未决(归 `ADR-X.10` §F,per 台账 Entry #476)。
created: 2026-11-17
---

# improvement: cpptlm-router-vc-count-param-inert

**主题**：`RouterTLM` 的 `vc_count` 参数被**声明**却**无人消费** —— 声明值（默认 2）与实际值（`NUM_VCS = 4` 编译期常量）不一致，且声明面还带一条**从未被执行**的派生表达式 `(mesh_x >= 4) ? 8 : 4`。本 improvement 记录该缺口的完整事实与**修复成本评估**，并**明确建议「只登记不修」**（理由见 Why 第 4 条）。

> **登记触发**：planner 在起草 `cpptlm-router-buffer-size-param`（`buffer_size` 同族第一例）时做**全参数面排查**时发现。
> **⛔ 不在 `cpptlm-router-buffer-size-param` 范围内** —— 该 change 只做 `buffer_size`（范围收敛见其 `design.md` D-6 第 1 条）。

---

## Why

### 第 1 条 —— 参数被声明但无人消费

```cpp
// src/tlm/router_tlm.cc:573-575（声明存在）
{"vc_count", cpptlm::ParamRule{"vc_count", cpptlm::ParamType::INTEGER, false, 2,
                                std::nullopt, std::nullopt, std::nullopt, 1, 8, std::nullopt}},
// src/tlm/router_tlm.cc:65-84（on_config_loaded 不读它 —— 实测读取集合 = {node_x, node_y, mesh_x, mesh_y, credit_timeout}）
```

⇒ JSON 里写 `"vc_count": 8` ⇒ **装配成功、无告警、行为完全不变**。

### 第 2 条 —— **三重**不一致

| 面 | 值 | 位置 |
|---|---|---|
| C++ 参数声明的默认值 | **2** | `src/tlm/router_tlm.cc:574` |
| config 面声明的默认值 + **派生表达式** | **2** + `derive_expr: "(mesh_x >= 4) ? 8 : 4"` | `configs/param_rules/router_tlm.json:24-31` |
| **实现实际值** | **`NUM_VCS = 4`**（编译期常量） | `include/tlm/router_tlm.hh:138` |

⇒ 声明的派生规则说「`mesh_x >= 4` 时 VC 数应为 **8**」，而实现恒为 **4** ⇒ **大 mesh 场景下声明与实际的偏离更大**。

### 第 3 条 —— 修复成本与 `buffer_size` **量级不同**（关键区分）

`buffer_size` 只需把一个**编译期常量**换成运行时成员（`input_buffer_` 是 `std::queue` **动态**容器，容量靠 `size() < BUFFER_DEPTH` 判定 ⇒ 容器类型不动）。

而 `vc_count` 是 **`NUM_VCS` 本身**，它是**三个容器的数组第二维**：

| 容器 | 声明 | 影响 |
|---|---|---|
| `input_buffer_` | `std::array<std::array<std::queue<RouterFlit>, NUM_VCS>, NUM_PORTS>` | 需换动态容器 |
| `vc_state_` | `std::array<std::array<VCState, NUM_VCS>, NUM_PORTS>` | 需换动态容器 |
| `downstream_credits_` | `std::array<std::array<uint8_t, NUM_VCS>, NUM_PORTS>` | 需换动态容器（且 `uint8_t` 值域 0..255 与 VC 数无关，可保留） |

⇒ **3 个核心数据结构的类型变更** + 全部 `[port][vc]` 索引访问点改写 ⇒ **成本量级与 `buffer_size` 完全不同**。

### 第 4 条 —— 建议「只登记不修」的三条理由

1. **零现有依赖**：`configs/` 下**没有任何 config** 给 router 传 `vc_count`（`grep -rn "\"vc_count\"" configs/` 只命中 `configs/param_rules/router_tlm.json` 的**声明本身**）⇒ 修复的收益 = 0。
2. **修复会改变行为**：让 `vc_count` 生效 ⇒ 大 mesh（`mesh_x >= 4`）的 VC 数从 4 变 8 ⇒ **行为变更** ⇒ 须逐条重推导既有数值（`vc_state_` 的分配/释放、`downstream_credits_` 的二维遍历、`allocate_vc` 的可用 VC 判定）。
3. **收益不匹配成本**：为「让一个无人使用的参数生效」付 3 个核心数据结构的类型变更 + 全量回归，**性价比不成立**。

> **替代路径（更便宜）**：把 `vc_count` 与 `flit_width` 从声明面**删除**（承认「VC 数固定 4、flit 宽固定 64」）⇒ 消除「声明了却不生效」的静默陷阱，成本近乎为零。⚠️ 此路径会改 `get_param_rules()` 与 `configs/param_rules/router_tlm.json` 的**声明面** ⇒ 属契约面变更 ⇒ 需走 OpenSpec 提案（但改动量远小于「让它生效」）。

---

## What Changes（本 improvement **不实施**任何代码改动 —— 只登记）

> ⛔ 本条是 **backlog 登记**，不是实施提案。落地时二选一：

| 路径 | 内容 | 改动量 | 行为变更 |
|---|---|---|---|
| **W1（推荐）** | **删除声明**：`get_param_rules()` 与 `configs/param_rules/router_tlm.json` 中的 `vc_count`（及同族 `flit_width`）条目 ⇒ 消除静默陷阱 | 小（2 个声明面） | ⛔ 无 |
| W2 | 让参数生效：`NUM_VCS` 运行时化 ⇒ 3 个容器类型变更 + 索引访问点改写 | **大** | ✅ 有（VC 数 4→8 等） |

**同族另两例（登记时的连带发现，同属「声明-实际不一致」族）**：

| param | 声明 | 读取 | 状态 |
|---|---|---|---|
| `flit_width` | 默认 **64** / 范围 64–128（`router_tlm.cc:570-572`） | ❌ **全仓零使用**（除声明本身） | 🟡 **幽灵参数** —— 连「读了不用」都不是，是纯声明；仓内 flit 宽度由 `NoCFlitBundle` 的**编译期字段**决定 |
| `credit_timeout` | ❌ **不在** `get_param_rules()`、**不在** `configs/param_rules/router_tlm.json` | ✅ **生效**（`on_config_loaded` 读取 ⇒ `credit_timeout_`） | 🟡 **反向不一致** —— 生效但**声明面缺项** ⇒ 装配期**无参数校验**（无类型/范围检查） |

⇒ 这 3 例 + `buffer_size` 共 4 处，构成 `RouterTLM` 的**参数声明面一致性缺口族**。建议在 W1 中**一次性订正声明面**（`vc_count` + `flit_width` 删除 + `credit_timeout` 补声明）。

---

## Acceptance

> 本条为 backlog ⇒ 下列判据是**落地时**的验收要求（W1 路径）。

| # | 判据（机器可判） |
|:--:|---|
| **AC-01**（W1） | `get_param_rules()` 与 `configs/param_rules/router_tlm.json` 的**参数集合完全一致**（双向相等断言）⇒ 无「只在 C++ 侧声明」或「只在 json 侧声明」的条目 |
| **AC-02**（W1） | 声明面中的每个参数**都有对应的读取点**（静态断言 / 闸脚本：声明集合 ⊆ `on_config_loaded` 读取集合）⇒ **零静默无效参数** |
| **AC-03**（W1） | `credit_timeout` 在声明面补齐（类型 + 范围）⇒ 装配期获得参数校验 |
| **AC-04**（W1） | 删除 `vc_count` / `flit_width` 后 ⇒ **零行为变更**（`NUM_VCS` 仍 4；既有全部测试断言零变化） |
| **AC-05**（通用） | `[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` 全绿；**既有 `REQUIRE` 常量零改动** |

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | `RouterTLM` 参数声明面不再存在「声明了却不生效 / 生效却未声明」的静默陷阱（W1 路径） | ❌ 0 |

---

## Impact

- **改动（W1 路径）**：`src/tlm/router_tlm.cc` 的 `get_param_rules()` + `configs/param_rules/router_tlm.json`（**纯声明面**）· 新增一个「声明集合 ↔ 读取集合」双向相等的闸脚本 + ctest 注册。
- **⛔ 不动**：`NUM_VCS` / `NUM_PORTS` / 任何容器类型 / `input_buffer_` / `vc_state_` / `downstream_credits_` / `RouterPort` / 任何 `REQUIRE` 常量。
- **回归面**：`[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` 全量（预期**零数值变化**）。
- **计数**：`INV-CEP-*` 新增 **0**。

---

## ⚠️ 关联

- `.rddf/improvements/cpptlm-router-buffer-size-param.md`（**同族第一例** · 本 change 的 `design.md` D-6 第 1 条明确把 `vc_count` 排除在外）
- `docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 1 条（原始登记处 —— 该条只点了 `buffer_size`）
- `openspec/changes/cpptlm-router-buffer-size-param/`（同批立项的 change）
- 缺陷代码：`src/tlm/router_tlm.cc:573-575`（`vc_count` 声明）· `:570-572`（`flit_width` 声明）· `:65-84`（`on_config_loaded` · 实测读取集合 = `{node_x, node_y, mesh_x, mesh_y, credit_timeout}`）· `include/tlm/router_tlm.hh:138`（`NUM_VCS = 4`）· `configs/param_rules/router_tlm.json:24-31`（`vc_count` 声明 + `derive_expr`）

---

## 状态沿革

- **2026-11-17** planner 起草并登记（台账 Entry **#467**;原始登记处 = `ADR-X.16` 的 `## 关联` 章「相邻独立缺陷」第 1 条）。
- **2026-11-18** 复核：结论「只登记不修」维持（`ADR-X.17` D-6 把 `vc_count` 排除在声明面之外）。
- **2026-11-19** 复核补充：`ParamRules` 机制已由 change `cpptlm-module-param-migrate-batch2` 的 T4 **整体删除** ⇒ C++ 声明面**连载体都不存在**，结论更强。仅剩 **json 侧**条目（`configs/param_rules/router_tlm.json`）未决 ⇒ `ADR-X.10` §F。
