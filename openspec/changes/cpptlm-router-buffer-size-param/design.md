# design: cpptlm-router-buffer-size-param

> 配套 `proposal.md`（Why / What Changes / Acceptance / Capabilities / Out of Scope）。
> 决策源：本文件 **D-1 默认值裁决** + `.rddf/improvements/cpptlm-router-buffer-size-param.md`。
> 原始登记处：`docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 1 条。

---

## D-1 — 默认值裁决（取 **8**，让声明反映实际）

| 选项 | 做法 | 后果 | 评价 |
|---|---|---|---|
| **A（选中）** | 声明默认值 **16 → 8** | 默认行为**零变化**（深度仍 8）⇒ 既有断言零变动 | ✅ **选中** |
| B | 声明默认值保持 16，行为改为 16 | **所有 router 的缓冲深度与 credit 预算翻倍** ⇒ 行为变更 ⇒ 须逐条重推导既有数值 | ❌ |

**裁决理由（三条）**：

1. **零行为变更优先**。本仓对「数值变化」有明确纪律（AC-04b 式「旧值 → 新值 → 为何是必然结果」逐条重推导，⛔ 禁止就地改 `REQUIRE` 常量）。选 A ⇒ **该纪律不需要被触发**；选 B ⇒ 必须触发，且要重推导 `downstream_credits_` 初值、`input_buffer_` 容量判定时机、credit 归还节奏三处数值。
2. **16 无仓内依据**。`buffer_size` 的声明来自 commit `3c4df028`「feat(param): add ParamRule structure and RouterTLM defaults (T3.1-08)」—— 那是**铺设 ParamRule 机制**时的「默认值清单」，**从未与 `BUFFER_DEPTH` 挂钩**；且 `configs/` 下**无任何 config** 使用过该参数（`grep -rn "\"buffer_size\"" configs/` 只命中 `configs/param_rules/router_tlm.json` 的声明本身）⇒ **16 从未被任何实现或拓扑依赖过**。
3. **变更方向**。缺陷是「声明与实现不符」⇒ 修复方向应是**让声明反映实现**，而不是「让实现去追一个无依据的声明」。

> ⚠️ **裁决的连带后果（必须显式承认）**：改完后 `get_param_rules()` 的 `buffer_size` 默认值与 `configs/param_rules/router_tlm.json` 的同名声明**都变成 8** ⇒ 这两处**声明面本身就是本 change 的改动对象**（它们此前与实际不符）。⇒ 属**配置契约面的声明订正**，正因如此本 change 走 OpenSpec 提案而非当 bugfix 直接改。

---

## D-2 — 实现形态

| 落点 | 改动 |
|---|---|
| `include/tlm/router_tlm.hh` | 保留 `static constexpr unsigned BUFFER_DEPTH = 8;` 作为**默认深度**（语义改为「默认值」）；新增 `unsigned buffer_depth_ = BUFFER_DEPTH;` |
| `src/tlm/router_tlm.cc` · 构造 | `buffer_depth_` 初值 = `BUFFER_DEPTH`（⛔ 不改 `downstream_credits_` 的初始化位置与语义，只把常量换成成员） |
| `src/tlm/router_tlm.cc` · `on_config_loaded` | 若 `cfg` 含 `buffer_size` ⇒ 校验类型/范围（1–64）⇒ 写入 `buffer_depth_`；**非法 ⇒ 装配期硬错误** |
| `src/tlm/router_tlm.cc` · 4 个使用点 | `:59`（credit 初值）· `:154`（输入缓冲容量判定）· `:485`（`receive_credit` 上限）· `:530`/`:535`（`credit_safety_reset`）⇒ 全部改读 `buffer_depth_` |
| `src/tlm/router_tlm.cc` · `get_param_rules` | `buffer_size` 的默认值 **16 → 8** |
| `configs/param_rules/router_tlm.json` | 同名声明的默认值 **16 → 8**（若该文件含 `buffer_size`；**实测当前只有 `vc_count`**，则本项为 no-op，见 D-6） |

**零配置面扩张**：不动 JSON schema、不动 `ModuleFactory`、不动任何 `configs/*.json`（拓扑文件）。

**⚠️ 顺序纪律**：`buffer_depth_` 必须在**构造时**就等于默认深度（因为 `downstream_credits_` 的初值在构造里设定）⇒ 参数读取（`on_config_loaded`）发生在其后时，**必须重新按 `buffer_depth_` 同步 `downstream_credits_` 的初值**，否则 credit 预算与缓冲深度**不同源**（违反 D-2 的「同源」要求，也使 AC-03 必红）。⇒ 实现时**先设 `buffer_depth_`，再初始化 `downstream_credits_`**。

---

## D-3 — 测试装置与观测面

| 项 | 规定 | 依据 |
|---|---|---|
| **装置** | 裸 `RouterTLM`（`adapter_ == nullptr`）+ 直接向 `req_in[4]` 注入，`dst_node` = 本节点 ⇒ `computeRoute` 选 `out_port = LOCAL` | 复用 `test/test_router_local_port_credit.cc` 的 `RLC-1` 手法（同一模块、同一注入路径） |
| **注入节拍** | 全局 **1 flit/cycle** | 否则输入 buffer 满 ⇒ `router_tlm.cc:152-155` **静默丢弃** flit ⇒ 得到与 D-2 无关的假失败 |
| **观测（容量）** | 读「**进入 `input_buffer_` 的 flit 数**」与「**被丢弃的 flit 数**」的差 | ⛔ 不可用「`flits_forwarded`」—— 它在**入 `pending_link_` 时** +1，而 `pending_link_` 是**全局单队列、每周期只 drain 1 个** ⇒ 领先真实 egress |
| **观测（credit 初值）** | `downstream_credits_` 的初值 | 该成员是 `private` ⇒ 需一条**只读**访问器或经由「注入后 credit 是否立即耗尽」的行为等价判据（⚠️ 见 D-6「访问器面」裁决） |
| **非法值判据** | `buffer_size` = 0 / 65 / 非整数 ⇒ `instantiateAll` 返回 **false**（硬错误） | ⛔ 不得「静默截断到边界」或「回退默认值而通过」 |
| **零变化判据** | 未设 `buffer_size` 时，既有全部测试断言**零变化** | D-1 的直接推论 |

---

## D-4 — 数值变化的处理纪律（AC-04b 同款）

1. 若出现任何 `flits` / `packets` / `hops` / `recv` / 计数类断言变化 ⇒ 必须在 `design.md` / tasks 里逐条列出「**旧值 → 新值 → 为何是 D-1 的必然结果**」；
2. ⛔ **禁止就地改 `REQUIRE` 常量**；
3. **预期**：D-1 选 A ⇒ **零数值变化** ⇒ 本纪律**不应被触发**；一旦被触发 ⇒ 说明默认深度意外变了 ⇒ **必须回查 D-1**。

---

## D-5 — spec 落点

| 动作 | 内容 |
|---|---|
| **新建** capability | `openspec/specs/router-buffer-size-param/` —— 一条 Requirement `router-buffer-size-param-effective`：3 个 Scenario（生效 / 非法值 fail / 默认值一致性） |
| **对齐** | `configs/param_rules/router_tlm.json` 与 `get_param_rules()` 的默认值**同为 8**（双声明面一致） |
| **不改** | `openspec/specs/port-type-system/spec.md`（`LOCAL` 的 `role = BI_DIRECTIONAL` 不变 —— 本 change 与端口角色无关） |

---

## D-6 — 已知限制（不在本 change 解决）

1. **`vc_count` param 仍不生效**（声明 2 / 实际 `NUM_VCS = 4`）—— 修它需把 `input_buffer_` / `vc_state_` / `downstream_credits_` 的**数组第二维**改为动态容器 ⇒ 成本量级不同 ⇒ 已单独登记 `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`。
2. **`downstream_credits_` 的类型仍是 `uint8_t`**（值域 0..255）—— 参数范围 1–64 ⇒ 够用 ⇒ 本 change 不动该类型；但若将来把范围上探到 >255 ⇒ 须连带改类型。
3. **静默丢 flit 的既有行为不变**：`input_buffer_` 满 ⇒ 静默丢弃（`router_tlm.cc:152-155`）—— 本 change 只让**容量可配**，**不引入**满溢告警（那是独立议题，见 `ADR-X.16` D6 第 1 条「LOCAL↔router 误用不可见」）。
4. **`configs/param_rules/router_tlm.json` 当前只有 `vc_count` 声明**（实测）⇒ 本 change 对该文件的改动**可能是 no-op**；实施时以实测为准，若确无 `buffer_size` 条目则该文件**零 diff**。
