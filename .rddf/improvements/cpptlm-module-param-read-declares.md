---
name: cpptlm-module-param-read-declares
priority: P1
phase: phase-10
category: refactor
type: feature
状态: 🟡 **已立项**（2026-11-17 · 台账待记 · 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（Proposed））
created: 2026-11-17
---

# improvement: cpptlm-module-param-read-declares

**主题**：模块内可配置参数（如 `RouterTLM` 的 `buffer_size`）的**声明与读取同源** —— 每次读取即一次声明，一次调用完成「类型校验 + 范围校验 + 默认值 + 赋值 + 已消费键登记」，从而消除「同一参数在两处各写一遍」的漂移面，并使 `params` 中的**未知键**可被检出。

## Why

### 第 1 条 —— 一个真实缺陷及其根因

`RouterTLM` 的 `buffer_size` 曾被 `get_param_rules()` 声明（默认 16、范围 1–64），但实现用编译期常量 `BUFFER_DEPTH = 8` ⇒ **配置写了不生效**。该缺陷已于 2026-11-17 由 change `cpptlm-router-buffer-size-param` 闭合，但**根因未除**：参数声明散在**四处**，且**几乎没有强制力**：

| # | 声明面 | 强制力 |
|:--:|---|---|
| ① | `configs/param_rules/*.json` | **无**（C++ 侧零加载;仅被已 Deprecated 的 `cpptlm_config/validator.py` 消费） |
| ② | C++ `get_param_rules()`（`router_tlm.cc:584-611`） | 仅 `required` + `default_int`（当 `params` 存在时）;`type`/`min`/`max` **死**（其校验器 `validate_module_params` 是零调用方死代码） |
| ③ | 模块内手写范围（`router_tlm.cc:98-99` 的 1..64） | 逐模块各自为政，是**第三遍**写同一件事 |
| ④ | CFG-08 的 per-type 硬编码键表（`module_factory_validate.cc:311-330`） | 有（但仅 RouterTLM/NICTLM、仅顶层实例） |

### 第 2 条 —— 更大的一族缺陷

「**配置里有键、实现零效果**」共 **5 类 / 14 键**（`CacheTLM.{size,level,associativi,protocol}`、`TrafficGenTLM.{pattern,num_requests,start_addr,end_addr}`、`GmmuTLM.page_size_bytes`、`LwcBusHub.max_ip_ports`、`MemoryTLM.{size,type,read_latency,write_latency}`）。

**它们为什么能存活至今**：配置值恰好等于硬编码默认值 ⇒ **零可观测差异** ⇒ 任何测试都抓不到（例：`MemoryTLM.write_latency` 配置 `120` ↔ 硬编码 `120`）。

⚠️ 本 change **不修复**这一族（改配置行为属各模块的独立工作），只为它提供**唯一可用的检出机制**（未知键检出）。

### 第 3 条 —— 为什么不沿用现状

现状下「声明」与「赋值」分离 ⇒ 任一处改动都可能让另一处失效，且无机制发现。把二者收敛到同一行代码，即可让「声明面与读取面分离」这一形态的缺陷**在表达上不可出现**。

## What Changes

1. 新增 `include/core/param_loader.hh`：`load_param(cfg, key, target, domain, range, default)` + 已消费键登记 + 值域/存储类型解耦（收窄转换 ⇒ 失败）。
2. 模块侧接入：`on_config_loaded` **或** `set_config` 覆写内逐参数调用;加载**幂等**且**不清除非本机制写入的状态**。
3. `module_factory` 侧：`set_config` 调用链为唯一装配入口;无 `params` 键的实例也加载一次（否则默认值永远不落成员）。
4. **RouterTLM 试点**（6 个参数，**不含 `vc_count`** —— 其值不影响运行时行为）+ 同 change 删除其 `registerParamRules` 注册、`get_param_rules()` 本体、**CFG-08 的 RouterTLM 分支**。
5. 未知键检出：**逐类型准入** + **基线二表**（表 A 已消费但不经 `load_param` / 表 B 真未消费豁免）。
6. 仓库级对拍闸（`configs/**/*.json` 的键集 ⊆ 源码声明键集 ∪ 表 A 键集）。

⛔ **不做**：`configs/param_rules/*.json` 的存废（`ADR-X.10` §F 另案）· 非标量参数（数组/嵌套对象/字符串枚举）· Python 链 · 代码生成 · 宏 · **NICTLM 的 CFG-08 分支**（须在 NICTLM 自己的迁移 change 内删，否则 `node_id` 必填空窗）。

## Acceptance

| # | 判据（对应 `ADR-X.17` 的 V-1~V-7） |
|:--:|---|
| **AC-01** | **测试夹具模块**（仅测试用，2 个标量参数，其一带默认值）⇒ 越界 / 类型错 / 未知键 三种配置分别使 `instantiateAll` 返回 `false`，错误信息含**模块名 + 键名 + 期望范围** |
| **AC-02** | 夹具模块的两个参数各有一条「两个不同合法值 ⇒ 可观测指标不同」的测试 |
| **AC-03** | ① **未**迁移的类型（含全部 SimModule 家族）行为与改动前**逐项一致**（全标签套件全绿 + 既有 `REQUIRE` 常量零变化）② **已迁移类型**的嵌套实例必填行为变更（静默取默认 → 装配失败）**逐类型登记**于行为面评估 |
| **AC-04** | 仓库级对拍（**限定已迁移且已开启未知键检查的类型**）：配置键集 ⊆ **源码声明键集 ∪ 表 A 键集**;未迁移类型不在判定范围;基线二表已落盘且逐键标注消费方 |
| **AC-05** | 声明的 `min`/`max` 与失败行为一致：测试内构造越界值 ⇒ 失败;边界值 ⇒ 通过（不改源码） |
| **AC-06** | 无 `params` 键的实例 ⇒ 声明的默认值仍落到成员（夹具模块验证;RouterTLM 因成员初值恰等于默认值而不具判别力） |
| **AC-07** | 加载幂等：同一实例二次加载 ⇒ 成员值不变 |
| **AC-08** | **NICTLM 的 `node_id` 必填保护零变化**（CFG-08 分支未删 ⇒ `test_config_schema_validation.cc:122/138/219` 三条断言保持绿） |

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 「声明面与读取面分离」这一形态的缺陷消除（读取行即声明行） | ❌ 0 |
| C-2 | 模块参数的 type / min / max 在装配期生效（**仅已迁移类型**） | ❌ 0 |
| C-3 | `params` 未知键可被检出（**仅已准入类型**） | ❌ 0 |

## Impact

- **改动**：`include/core/param_loader.hh`（新）· `src/tlm/router_tlm.cc` + `include/tlm/router_tlm.hh`（试点）· `src/core/module_factory.cc`（加载路径 + 无 `params` 也加载 + 删 CFG-08 的 RouterTLM 分支）· `src/core/module_factory_validate.cc`（删 RouterTLM 分支）· `test/test_d3_param_resolver_registry.cc`（删除或改写）· 新增测试（夹具 + 对拍闸）
- **⛔ 不动**：`configs/**` 任一配置（零配置面变更）· ABI · 冻结面 · `NICTLM` 的任何分支 · 其余 15 个模块（渐进迁移）
- **回归面**：全标签套件;重点 `[router]` / `[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` / `[config][schema]`
- **计数**：`INV-CEP-*` 新增 **0**
- **风险**：NICTLM 分支误删 ⇒ `node_id` 必填空窗（AC-08 钉死）;基线二表漏键 ⇒ 对拍闸首日红

---

## ⚠️ 关联

- **决策源**：`docs/adr/ADR-X.17-module-param-read-declares.md`（Proposed;审稿报告 `.rddf/reviews/ADR-X.17.md`）
- **ADR-X.17 的待复核项**：critic 复审预算用尽 ⇒ B-1（NICTLM 分支删除时机）须在实施 change 的 design 中确认
- **同族 backlog**：`.rddf/improvements/cpptlm-validate-params-dead-code.md`（`validate_module_params` 死代码）· `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`（`vc_count` 不生效）
- **F-14 那一族**（5 类 / 14 键）的**修复**属各模块独立 change，不在本 change
