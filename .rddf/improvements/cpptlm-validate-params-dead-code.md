---
name: cpptlm-validate-params-dead-code
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: 🟢 **C++ 侧已闭合(2026-11-19)** —— change `cpptlm-module-param-migrate-batch2` 的 **T4.4/T4.5** 已删 `validate_module_params`(死代码) + `ParamParser::validate`;同批 **T4.1~T4.3** 更进一步把 `ParamRules` 机制**整体退役**(`include/core/param_rules.hh` / `registerParamRules` / `getParamRulesRegistry` / 工厂侧默认值注入循环)⇒ 本 improvement 的 W2 路径落地且超出预期:参数声明面统一到 `load_param` / `load_param_opt`(判据 `test/test_param_migrate_batch2.cc` + 台账 Entry #492)。⛔ **Python 生成期诉求仍未闭合**(`cpptlm_config/validator.py` 的 PARAM-01/02 依赖 `configs/param_rules/*.json`;该目录按 T4.7 **未触碰**)⇒ 转 `ADR-X.10` **§F** 独立裁决(architect 域)。以下原状态行保留作沿革。
created: 2026-11-17
---

# improvement: cpptlm-validate-params-dead-code

**主题**：`ModuleFactory` 的 **param 校验器 `validate_module_params` 是零调用方的死代码** ⇒ `ParamRules` 声明的 `type` / `min_value` / `max_value` 约束在 `instantiateAll` 装配路径上**完全不起作用**。⇒ 任何模块的参数**类型错误或越界**都不会被前置拦截，只能靠模块自己在 `on_config_loaded()` 里主动校验（而那是**逐模块各自为政**，且多数模块根本没写）。

> **发现路径**：change `cpptlm-router-buffer-size-param` 实施期（`design.md` D-7.1）。当时为实现「非法 `buffer_size` 装配期 fail」，先假设 `validateConfig` 会按 `ParamRules` 拦下 —— 实测发现它**不会**，才去查它的调用链，发现校验器是死代码。
> **同族关联**：`cpptlm-router-vc-count-param-inert`（声明-实际不一致 · 只登记不修）· `cpptlm-router-buffer-size-param`（首个发现者 · 其 AC-04 只能靠 `on_config_loaded` 抛异常 + 装配点 catch 临时补通道）。

---

## Why

### 第 1 条 —— 校验器存在但**零调用方**（实测）

```cpp
// src/core/module_factory_validate.cc:474-499
static bool validate_module_params(const std::string& module_type, const json& params,
                                   const cpptlm::ParamRules& rules) {
    for (const auto& [param_name, rule] : rules) {
        if (rule.required && !params.contains(param_name)) { … return false; }
        if (params.contains(param_name)) {
            auto result = cpptlm::ParamParser::parse(…, rule.type);
            if (!result.success) { … return false; }
            if (!cpptlm::ParamParser::validate(result, rule)) { … return false; }   // ← min/max 在这里
        }
    }
    return true;
}
```

`grep -rn "validate_module_params" src/ include/` ⇒ **仅定义处一处，零调用方** ⇒ `static` 死代码（编译器若无 `-Wunused-function` 对 `static` 的告警则静默留存）。

而 `ParamParser::validate`（`src/core/param_parser.cc:54-67`，实现 min/max 比较）**唯一**的调用点就在这个死函数里 ⇒ **整套 min/max 校验能力实际未被使用**。

### 第 2 条 —— `instantiateAll` 路径只做了 **required** 检查，没做 type / 范围检查

`module_factory.cc:265-287`（Step 1.2）确实遍历了 `ParamRules` 注册表，但**只做两件事**：

| 行为 | 是否生效 |
|---|---|
| `rule.required && !params.contains(name)` ⇒ `return false` | ✅ **生效** |
| `rule.default_int` 缺失时注入默认值 | ✅ **生效** |
| `rule.type` 解析校验 | ❌ **不生效**（只在死代码里） |
| `rule.min_value` / `max_value` 范围校验 | ❌ **不生效**（只在死代码里） |

⇒ **净效果**：「必填」有保护，「类型正确」与「取值在范围内」**没有保护**。

### 第 3 条 —— 具体后果（已实际发生）

`RouterTLM` 声明了 `buffer_size`（`1..64`）。在 change `cpptlm-router-buffer-size-param` 之前，写 `"buffer_size": 9999` ⇒ **装配成功、无告警**；写 `"buffer_size": "abc"` ⇒ 同样静默通过（除非模块自己 parse 时抛异常）。

该 change 的 AC-04（非法值 ⇒ 装配期 fail）**本应由 `validateConfig` 免费提供**，实测却必须额外做两件事才成立：
1. `on_config_loaded` 里**手动**校验 + 抛异常；
2. `module_factory.cc` 的 `set_config` 调用点**新增 6 行 try/catch**（异常 ⇒ `return false`）。

⇒ 第 2 步的 try/catch 是**通用基础设施**，但它现在只为 RouterTLM 服务 —— 其它模块若也在 `on_config_loaded` 里抛异常，**同样能受益**（这是 6 行改动的正向外溢），但**类型/范围校验仍然缺席**。

### 第 4 条 —— 声明面与约束面**三重割裂**

| 面 | 位置 | 内容 |
|---|---|---|
| C++ 声明 | `get_param_rules()`（各模块静态自注册） | `type` / `required` / `default` / `min` / `max` |
| JSON 声明 | `configs/param_rules/<Type>.json` | 同一组字段（**可能不一致** —— 例如 `buffer_size` 原先只在 C++ 侧有） |
| **实际约束** | 无 | ⟵ **本缺口** |

⇒ 前两者是「文档」，只有第三者是「约束」；而第三者为空 ⇒ **`ParamRules` 目前等价于注释**。

---

## What Changes

> 本条是 backlog 登记。落地时二选一：

### W1（推荐）：**接回** `validate_module_params`，让 `validateConfig` 真正校验

1. `validateConfig`（`module_factory_validate.cc:199`）对每个含 `params` 的模块，从 **`ModuleFactory::getParamRulesRegistry()`**（C++ 单一真源）取 rules ⇒ 调 `validate_module_params` ⇒ 失败 ⇒ `return false`。
2. ⚠️ **rules 必须取自 C++ registry，而非 `configs/param_rules/*.json`** —— 否则又变成一个双真源（见第 4 条）。
3. `configs/param_rules/*.json` 的定位需**明确**：要么废弃（唯一真源 = C++），要么加「与 C++ registry 双向相等」的闸（与 `ADR-CEP-11` 的 IC2 同款范式）。

### W2（替代）：**明确废弃**死代码，统一到「`on_config_loaded` 抛异常 + 装配点 catch」范式

1. 删除 `validate_module_params`（死代码）+ `ParamParser::validate` 若无其它调用方一并处置。
2. 在 `SimObject::on_config_loaded()` 的契约文档里写明：**「非法参数 ⇒ 抛异常」是唯一范式**（change `cpptlm-router-buffer-size-param` 已建立该先例 + 装配点 catch）。
3. 代价：每个模块要**自行**校验（重复劳动），且没有统一的「声明即约束」保证。

> **对比**：W1 让 `ParamRules` 从「文档」变成「约束」（治本）；W2 承认它只是文档（治标但便宜）。⚠️ W1 的**风险**：接回后，**此前静默通过的非法 param 会开始 fail** ⇒ 必须**先排查 `configs/` 下所有 config 的 param 是否都在声明范围内**（含各模块 `derive_expr` 的派生值）。

---

## Acceptance

> 本条为 backlog ⇒ 下列判据是**落地时**的验收要求（以 W1 为准；W2 只需 AC-04/AC-05）。

| # | 判据（机器可判） |
|:--:|---|
| **AC-01**（W1） | 某模块的 param **越界**（`min` / `max` 之外）⇒ `instantiateAll` **返回 false**（⛔ 不需要该模块在自己的 `on_config_loaded` 里手动校验） |
| **AC-02**（W1） | param **类型错误**（如 INTEGER 位置传字符串）⇒ `instantiateAll` **返回 false** |
| **AC-03**（W1） | `required` 缺失 ⇒ 返回 false（**与 Step 1.2 的既有检查不重复/不矛盾** —— 需显式说明由哪一处负责） |
| **AC-04** | **合法配置零行为变化**：`configs/` 下全部 config 装配成功；`[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` 全绿；**既有 `REQUIRE` 常量零改动** |
| **AC-05** | **声明面一致性**：`configs/param_rules/*.json` 与 `getParamRulesRegistry()` 的 param 集合**双向相等**（W1 若选「C++ 单一真源」则 AC-05 改为「json 文件已废弃且无残留引用」） |
| **AC-06** | 变异验证：对每条新判据各构造一个**决定性变异**（越界 / 类型错 / 缺失），确认对应断言**必红** |

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | `ParamRules` 从「文档」变为「装配期约束」（W1） | ❌ 0 |
| C-2 | 「非法参数 ⇒ 装配失败」的范式**唯一化**（W2） | ❌ 0 |

---

## Impact

- **改动（W1）**：`src/core/module_factory_validate.cc`（`validateConfig` 接回校验 + rules 取自 C++ registry）· 可能触及 `configs/param_rules/*.json` 的定位（废弃或加一致性闸）· 新增单测。
- **改动（W2）**：`src/core/module_factory_validate.cc`（删死代码）+ `include/core/sim_object.hh` 的契约注释。
- **⛔ 不动**：各模块的 `on_config_loaded()` 实现（W1 路径下无需逐模块改）· `ParamParser` 的解析/校验实现本身（它是对的，只是没人调）· 任何既有 `REQUIRE` 常量。
- **回归面**：**全部**含 param 的 config（`configs/**`）—— ⚠️ W1 接回后「此前静默通过的非法 param」会开始 fail ⇒ 须先做**全仓 config 排查**（含 `derive_expr` 派生值是否落在 `min..max`）。
- **计数**：`INV-CEP-*` 新增 **0**。

---

## ⚠️ 关联

- `openspec/changes/cpptlm-router-buffer-size-param/` 的 `design.md` **D-7.1**（本缺口的**首发记录** · 当时用 6 行 try/catch 临时补通道）
- `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`（同族：声明-实际不一致 · 只登记不修）
- `.rddf/improvements/cpptlm-router-buffer-size-param.md`（首个发现者）
- 缺陷代码：`src/core/module_factory_validate.cc:474-499`（`validate_module_params` · **零调用方**）· `src/core/module_factory.cc:265-287`（Step 1.2 · **只做 required + default 注入**）· `src/core/param_parser.cc:54-67`（`ParamParser::validate` · 唯一调用点在死代码里）· `src/core/module_factory_validate.cc:199`（`validateConfig` · **不调**该校验器）
- 相关范式先例：`src/core/module_factory.cc:487-493`（Step 4.5 `simulate_instantiate` 的 `try{...}catch{return false}`）

---

## 状态沿革

- **2026-11-17** planner 起草并登记（台账 Entry **#470**;发现于 change `cpptlm-router-buffer-size-param` 的 `design.md` **D-7.1**）。
- **2026-11-18** 部分转派：C++ 侧诉求转给 change `cpptlm-module-param-migrate-batch2`（台账 Entry **#476**）。
- **2026-11-19** **C++ 侧闭合**：该 change 的 T4 删除 `validate_module_params` / `ParamParser::validate`，并把 ParamRules 机制整体退役（台账 Entry #492）。⚠️ 落地路径比本 improvement 预设的 W2 更彻底 —— 不是「承认 ParamRules 只是文档」，而是**机制消失**，约束统一由 `load_param` 的 `lo`/`hi` 与模块内校验承担。
- **仍未闭合**：Python 生成期（`cpptlm_config/validator.py` 的 PARAM-01/02 ← `configs/param_rules/*.json`）⇒ `ADR-X.10` §F。
