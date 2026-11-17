# change: cpptlm-router-buffer-size-param

**主题**：让 `RouterTLM` 的 `buffer_size` 配置项**真正生效** —— 当前它被 `get_param_rules()` 声明（默认 16 · 范围 1–64）却**无人消费**，`on_config_loaded()` 不读它，实际生效的是编译期常量 `BUFFER_DEPTH = 8` ⇒ 用户写 `buffer_size` **静默无效**（无报错、无告警），且声明值与实际值不一致。

> **上游**：`.rddf/improvements/cpptlm-router-buffer-size-param.md`（本 change 的 improvement 草稿 · 5 段 + AC-01~AC-05）· 原始登记处 = `docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 1 条
> **决策源**：本 change 的 `design.md` **D-1 默认值裁决**（取 **8**，让声明反映实际 ⇒ 零行为变更）
> **同族但独立**：`vc_count` param 同样声明-实际不一致（声明 2 / 实际 `NUM_VCS = 4`），但修它需动**数组维度** ⇒ 已单独登记为 `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`，⛔ **不在本 change 范围内**

---

## Why

### 1. 参数被声明但无人消费 ⇒ 写了等于没写

```cpp
// src/tlm/router_tlm.cc:576-578 —— 声明存在
{"buffer_size", cpptlm::ParamRule{"buffer_size", cpptlm::ParamType::INTEGER, false, 16,
                                  std::nullopt, std::nullopt, std::nullopt, 1, 64, std::nullopt}},
// src/tlm/router_tlm.cc:65-84 —— on_config_loaded 不读它
if (cfg.contains("node_x")) { … }   if (cfg.contains("node_y")) { … }
if (cfg.contains("mesh_x")) { … }   if (cfg.contains("mesh_y")) { … }
if (cfg.contains("credit_timeout")) { … }   // ← 没有 buffer_size
```

⇒ JSON 里写 `"buffer_size": 32` ⇒ **装配成功、无告警、行为完全不变**。这是最坏的一类配置面缺陷：**契约声明了能力，实现不存在，且失败是静默的**。

### 2. 声明值(16)与实际值(8)不一致，且 16 无仓内依据

`buffer_size` 的声明来自 commit `3c4df028`「feat(param): add ParamRule structure and RouterTLM defaults (T3.1-08)」—— 那是**铺设 ParamRule 机制**时的「默认值清单」，**从未与 `BUFFER_DEPTH` 挂钩**；实现侧真实值是 `static constexpr unsigned BUFFER_DEPTH = 8`（`include/tlm/router_tlm.hh:139`）。

⇒ **16 是机制铺设期的清单值（无实现依据）**；**8 是实现的真实值**。

### 3. 实际生效的 4 个使用点全部硬编码常量

| 使用点 | 位置 | 作用 |
|---|---|---|
| `downstream_credits_[p][v] = BUFFER_DEPTH` | `src/tlm/router_tlm.cc:59` | credit 预算初值 |
| `input_buffer_[port][vc].size() < BUFFER_DEPTH` | `:154` | **输入缓冲容量判定**（超出即静默丢 flit） |
| `downstream_credits_[in_port][vc] < BUFFER_DEPTH` | `:485` | `receive_credit` 上限 |
| `credit_safety_reset` 的比较与复位 | `:530` / `:535` | 死锁恢复时的缓冲可用量判定 |

⇒ **缓冲深度**同时决定「能缓存多少 flit」与「credit 预算多大」，二者必须**同源** ⇒ 不能只改一处。

### 4. 技术上可行且改动面局部（关键澄清）

- `input_buffer_` = `std::array<std::array<std::queue<RouterFlit>, NUM_VCS>, NUM_PORTS>` ⇒ **`std::queue` 是动态容器**，容量靠 `size() < BUFFER_DEPTH` 判定 ⇒ **改成运行时深度不需要动容器类型**。
- `downstream_credits_` = `std::array<std::array<uint8_t, NUM_VCS>, NUM_PORTS>` ⇒ 值域 0..255，参数范围 1–64 ⇒ **够用**，不构成上限。

### 5. 现有 config 零使用 ⇒ 无回归面

`configs/` 下**没有任何 config** 给 router 传 `buffer_size`（`grep -rn "\"buffer_size\"" configs/` 只命中 `configs/param_rules/router_tlm.json` 的**声明本身**）⇒ 本 change **不改变任何现有拓扑的行为**。

---

## What Changes

1. **`include/tlm/router_tlm.hh`**：保留 `BUFFER_DEPTH` 作为**默认深度**，新增运行时成员 `unsigned buffer_depth_`（初值 = 默认深度）；4 个使用点改读该成员。
2. **`src/tlm/router_tlm.cc`**：`on_config_loaded()` 读取 `buffer_size`（若 JSON 中存在）⇒ 校验范围 1–64 ⇒ 写入 `buffer_depth_`；**非法值装配期 fail**（⛔ 不得静默截断、⛔ 不得回退默认值而静默通过）。
3. **参数声明默认值 16 → 8**（`get_param_rules()`），并在 `configs/param_rules/router_tlm.json` **补** `buffer_size` 条目（`default_int: 8` / `1..64`）：让**双声明面**一致且让声明反映实际。裁决理由见 `design.md` D-1；该 json 文件的必要性见 **D-7.2**（原「可能是 no-op」的判断已作废）。
4. **`vc_count` 不动**（另立 backlog `cpptlm-router-vc-count-param-inert`）。

---

## Acceptance

判据的**默认值裁决**（取 8）已写进 `design.md` **D-1**；AC-02 与 AC-05 的「零变化」要求是 D-1 的直接推论。

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | `buffer_size: N` ⇒ 缓冲深度与容量均生效 | ① `buffer_depth() == N`（只读访问器 · 直接证据）② 每 in_port 注入 `K > N` 个 ⇒ **必丢**（转发数 < 4K）—— 4 个 in_port 汇聚到同一出口，注入 4/cycle 远快于消费 1/cycle。观测面边界见 `design.md` D-3 |
| **AC-02** | 未设 `buffer_size` ⇒ 行为与改动前**逐项一致** | 深度 == 8；`downstream_credits_` 初值 == 8；既有全部测试断言**零变化** |
| **AC-03** | `buffer_size: N` ⇒ credit 预算与深度**同源** | `downstream_credits_[p][v]` 初值 == N |
| **AC-04** | 非法值 ⇒ **装配期 fail** | `buffer_size` = 0 / 65 / 非整数 ⇒ 装配返回 false 且**不静默截断、不静默回退** |
| **AC-05** | 全量回归 + 零数值变化 | `[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` 全绿；**既有 `REQUIRE` 常量零改动** |

**附加检查（guard，不占验收编号）**：G-1 `vc_count` 声明值与实际值**保持现状不变**（本 change 不碰它，且其不一致已有独立 backlog 登记）；G-2 `NUM_VCS` / `NUM_PORTS` 仍为**编译期常量**（本 change 不动数组维度）。

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | `RouterTLM` 的 `buffer_size` 配置项从「声明了但静默无效」变为「生效且可校验」 | ❌ 0 |
| C-2 | 参数声明值与实现值的**一致性**恢复（16 → 8） | ❌ 0 |

---

## Out of Scope

- **`vc_count` param**（声明 2 / 实际 `NUM_VCS = 4`）—— 修它需把 3 个 `std::array` 的第二维改为动态容器 ⇒ 成本量级不同 ⇒ **单独登记** `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`，⛔ 本 change 不修。
- **不改变任何现有拓扑的行为**（默认深度保持 8）。
- **不改** `NUM_VCS` / `NUM_PORTS` / `RouterPort` / 任何容器类型 / JSON schema 的其余部分。
- **不改** `CohIfNICTLM` / `NICTLM`。
- **`module_factory.cc` 仅加 6 行 try/catch**（`set_config`/`on_config_loaded` 调用点 ⇒ 异常转装配失败）—— 必要性见 `design.md` **D-7.1**（该文件的 param 校验器 `validate_module_params` 是**零调用方的死代码**）；⛔ 不改该文件其它任何逻辑。
- **不改**任何既有 `REQUIRE` 常量（AC-05 要求零数值变化）。
