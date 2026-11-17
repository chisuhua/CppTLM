---
name: cpptlm-router-buffer-size-param
priority: P1
phase: phase-10
category: core-impl
type: feature
状态: 🟡 **已立项**(2026-11-17 · planner 起草并建 change 目录 · 台账 Entry **#467** · KPI **0/3 → 1/3** · 决策源 = 本 improvement 的「默认值裁决」(取 8,让声明反映实际))
created: 2026-11-17
---

# improvement: cpptlm-router-buffer-size-param

**主题**：`RouterTLM::get_param_rules()` 声明了 `buffer_size` 参数（INTEGER · 默认 **16** · 范围 1–64），但**没有任何代码消费它** —— `RouterTLM::on_config_loaded()` 只读 `node_x` / `node_y` / `mesh_x` / `mesh_y` / `credit_timeout`，而实际生效的缓冲深度是**编译期常量** `BUFFER_DEPTH = 8`。⇒ 用户在 JSON 里写 `buffer_size` **静默无效**（无报错、无告警、无任何反馈），且**声明值(16)与实际值(8)不一致**。本 improvement 让该参数**真正生效**。

> **登记处**：`docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 1 条（「`buffer_size` param 不生效」）。
> **同族但独立**：`vc_count` param 同样声明-实际不一致（声明 2 / 实际 `NUM_VCS = 4`），但修它需动**数组维度** ⇒ 已**单独登记**为 `cpptlm-router-vc-count-param-inert`，⛔ **不在本 change 范围内**。

---

## Why

### 第 1 条 —— 参数被声明但**无人消费** ⇒ 写了等于没写

```cpp
// src/tlm/router_tlm.cc:576-578（声明存在）
{"buffer_size", cpptlm::ParamRule{"buffer_size", cpptlm::ParamType::INTEGER, false, 16,
                                  std::nullopt, std::nullopt, std::nullopt, 1, 64, std::nullopt}},
// src/tlm/router_tlm.cc:65-84（on_config_loaded 不读它）
if (cfg.contains("node_x")) { … }  if (cfg.contains("node_y")) { … }
if (cfg.contains("mesh_x")) { … }  if (cfg.contains("mesh_y")) { … }
if (cfg.contains("credit_timeout")) { … }   // ← 没有 buffer_size
```

⇒ JSON 里写 `"buffer_size": 32` ⇒ **装配成功、无告警、行为完全不变**。这是最坏的一类配置面缺陷：**契约声明了能力，实现不存在，且失败是静默的**。

### 第 2 条 —— 声明值(16)与实际值(8)不一致，且 **16 无仓内依据**

`buffer_size` 的声明来自 commit `3c4df028`「feat(param): add ParamRule structure and RouterTLM defaults (T3.1-08)」—— 那是**铺设 ParamRule 机制**时的「默认值清单」，**从未与 `BUFFER_DEPTH` 挂钩**。而实现侧的真实值是 `static constexpr unsigned BUFFER_DEPTH = 8`（`include/tlm/router_tlm.hh:139`）。

⇒ **16 是机制铺设期的清单值，无实现依据**；**8 是实现的真实值**。

### 第 3 条 —— 实际生效的 4 个使用点全部硬编码常量

| 使用点 | 位置 | 作用 |
|---|---|---|
| `downstream_credits_[p][v] = BUFFER_DEPTH` | `src/tlm/router_tlm.cc:59` | credit 预算初值 |
| `input_buffer_[port][vc].size() < BUFFER_DEPTH` | `:154` | **输入缓冲容量判定**（超出即静默丢 flit） |
| `downstream_credits_[in_port][vc] < BUFFER_DEPTH` | `:485` | `receive_credit` 上限 |
| `credit_safety_reset` 的比较与复位 | `:530` / `:535` | 死锁恢复时的缓冲可用量判定 |

⇒ **缓冲深度**同时决定「能缓存多少 flit」与「credit 预算多大」，二者必须同源 ⇒ 不能只改一处。

### 第 4 条 —— 技术上**可行且改动面局部**（关键澄清）

- `input_buffer_` 的类型是 `std::array<std::array<std::queue<RouterFlit>, NUM_VCS>, NUM_PORTS>` ⇒ **`std::queue` 是动态容器**，容量靠 `size() < BUFFER_DEPTH` 判定 ⇒ **改成运行时深度不需要动容器类型**。
- `downstream_credits_` 的类型是 `std::array<std::array<uint8_t, NUM_VCS>, NUM_PORTS>` ⇒ 值域 0..255，参数范围 1–64 ⇒ **够用**，不构成上限。

⇒ 改动面 = `router_tlm.hh`（常量降为默认值 + 1 个运行时成员）+ `router_tlm.cc`（4 个使用点 + `on_config_loaded` 读 1 个参数）+ 两处参数声明的默认值同步。

### 第 5 条 —— 现有 config **零使用** ⇒ 无回归面

`configs/` 下**没有任何 config** 给 router 传 `buffer_size`（`grep -rn "\"buffer_size\"" configs/` 只命中 `configs/param_rules/router_tlm.json` 的**声明本身**）⇒ 本 change **不改变任何现有拓扑的行为**。

---

## What Changes

1. **`include/tlm/router_tlm.hh`**：保留 `BUFFER_DEPTH` 作为**默认深度**（改名语义为「默认值」），新增运行时成员 `unsigned buffer_depth_ = <默认>`；4 个使用点改读该成员。
2. **`src/tlm/router_tlm.cc`**：`on_config_loaded()` 读取 `buffer_size`（若 JSON 中存在）⇒ 校验范围 1–64 ⇒ 写入 `buffer_depth_`；**非法值装配期 fail**（⛔ 不得静默截断/回退）。
3. **参数声明默认值 16 → 8**（`get_param_rules()` + `configs/param_rules/router_tlm.json`）：**让声明反映实际**，而非让所有 router 的行为突变。
4. **`vc_count` 不动**（另立 backlog）。

> **默认值裁决**：取 **8**（对齐实际）而非 16（对齐声明）。理由：① **零行为变更** ⇒ 不触发「数值变化须逐条重推导」纪律；② 16 是机制铺设期的清单值，**仓内无任何依据**、也**从未被任何 config 依赖**；③ 变更方向应是「让声明反映实现」，而不是「让所有 router 的缓冲深度翻倍」。

---

## Acceptance

| # | 判据（机器可判） |
|:--:|---|
| **AC-01** | 设 `buffer_size: N` ⇒ 该 router 的**输入缓冲实际容量 == N**：注入 N 个 flit 全部进入（不丢），第 N+1 个被丢弃（`input_buffer_` 满 ⇒ 静默丢弃路径可观测） |
| **AC-02** | **未设** `buffer_size`（默认）⇒ 行为与改动前**逐项一致**（深度 == 8；`downstream_credits_` 初值 == 8；既有全部测试断言**零变化**） |
| **AC-03** | 设 `buffer_size: N` ⇒ 该 router 的 `downstream_credits_[p][v]` 初值 == N（credit 预算与缓冲深度**同源**） |
| **AC-04** | `buffer_size` 为**非法值**（0 / 65 / 非整数）⇒ **装配期 fail**（硬错误，⛔ 不得静默截断到边界、⛔ 不得回退默认值而静默通过） |
| **AC-05** | `[cep]` / `[e2e]` / `[pcie]` / `[noc]` 全绿；**既有 `REQUIRE` 常量零改动**（本 change 不产生任何数值变化） |

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | `RouterTLM` 的 `buffer_size` 配置项从「声明了但静默无效」变为「生效且可校验」 | ❌ 0 |
| C-2 | 参数声明值与实现值的**一致性**恢复（16 → 8） | ❌ 0 |

---

## Impact

- **改动**：`include/tlm/router_tlm.hh`（1 常量语义 + 1 成员）· `src/tlm/router_tlm.cc`（4 个使用点 + `on_config_loaded` + 参数声明默认值）· `configs/param_rules/router_tlm.json`（默认 16 → 8）· 新增单测。
- **⛔ 不动**：`vc_count`（另立 backlog）· `NUM_VCS` / `NUM_PORTS`（编译期常量）· `RouterPort` 枚举 · `input_buffer_` / `downstream_credits_` 的**容器类型** · `CohIfNICTLM` / `NICTLM` · JSON schema 的其余部分 · 任何既有 `REQUIRE` 常量。
- **回归面**：`RouterTLM` 参与的**全部**拓扑（`[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]`）+ 全部 `RouterTLM` 单测。⚠️ 因默认深度**不变**，预期**零数值变化**（AC-05）；若出现任何变化 ⇒ 须按「旧值 → 新值 → 为何是必然结果」逐条重推导（⛔ 禁止就地改 `REQUIRE` 常量）。
- **计数**：`INV-CEP-*` 新增 **0**。

---

## ⚠️ 关联

- `docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 1 条（该缺口的**原始登记处**）
- `openspec/changes/cpptlm-router-buffer-size-param/`（本 change 四件套）
- 同族独立项：`.rddf/improvements/cpptlm-router-vc-count-param-inert.md`（`vc_count` 声明-实际不一致 · **只登记不修**）
- 声明引入史：commit `3c4df028`「feat(param): add ParamRule structure and RouterTLM defaults (T3.1-08)」
- 相关实现：`include/tlm/router_tlm.hh:138-139`（`NUM_VCS` / `BUFFER_DEPTH`）· `src/tlm/router_tlm.cc:59` / `:154` / `:485` / `:530` / `:535`（4 个使用点）· `:65-84`（`on_config_loaded`）· `:573-578`（参数声明）
