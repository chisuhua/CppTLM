# proposal: cpptlm-module-param-read-declares

## Why

`RouterTLM` 的 `buffer_size` 曾「声明了却无人消费」（声明默认 16、范围 1–64，实现用编译期常量 `BUFFER_DEPTH = 8`）⇒ 配置写了不生效。该缺陷已由 change `cpptlm-router-buffer-size-param`（2026-11-17 归档）闭合，但**根因未除**：模块参数的声明散在**四处**且**几乎没有强制力** ——

| # | 声明面 | 强制力（实测） |
|:--:|---|---|
| ① | `configs/param_rules/*.json` | **无**（C++ 侧零加载;仅被已 Deprecated 的 `cpptlm_config/validator.py` 消费） |
| ② | C++ `get_param_rules()`（`router_tlm.cc:584-611`） | 仅 `required` + `default_int`;`type`/`min`/`max` **死**（校验器 `validate_module_params` 是零调用方死代码） |
| ③ | 模块内手写范围（`router_tlm.cc:98-99` 的 1..64） | 逐模块各自为政 —— 同一件事的**第三遍**写法 |
| ④ | CFG-08 的 per-type 硬编码键表（`module_factory_validate.cc:311-330`） | 有，但仅 RouterTLM/NICTLM、仅顶层实例 |

⇒ 「声明」与「赋值」分离 ⇒ 任一处改动都可能让另一处失效，且**无机制能发现**。

更有一族更大的缺陷：「**配置里有键、实现零效果**」共 5 类 / 14 键（`CacheTLM.{size,level,associativity,protocol}`、`TrafficGenTLM.{pattern,num_requests,start_addr,end_addr}`、`GmmuTLM.page_size_bytes`、`LwcBusHub.max_ip_ports`、`MemoryTLM.{size,type,read_latency,write_latency}`）。它们存活至今是因为**配置值恰好等于硬编码默认值** ⇒ 零可观测差异 ⇒ 测试抓不到。

## What Changes

1. 新增 `include/core/param_loader.hh` —— **单个自由函数模板**（⛔ 不引入 `ParamSpec` 容器、不引入 `ValueDomain` 枚举、不引入 `ParamRange` 结构）：
   ```cpp
   template <class T>
   void load_param(const json& cfg, std::string_view key, T& target,
                   std::optional<T> lo = std::nullopt,
                   std::optional<T> hi = std::nullopt,
                   std::optional<T> def = std::nullopt);   // def 缺省 ⇒ 必填
   ```
   一次调用 = **声明 + 类型校验 + 范围校验 + 默认值 + 赋值 + 已消费键登记**。值域由 `T` 决定（本 change 的六个参数没有一个需要「域 ≠ 存储类型」）。
2. **未知键由基类执行**（`ADR-X.17` D-3a）：`ParamLoadContext` 由 `SimObject` 按实例持有;`set_config` 在 `on_config_loaded()` 返回后执行「准入判定 + 未知键失败」，抛既有 `cpptlm::ParamValidationError`（`include/core/param_errors.hh`）—— ⛔ 不依赖模块自己调用、⛔ 不新造异常类型。
3. **准入表 = 唯一 C++ 真源**（`is_param_admitted(std::string_view type)`），运行时与仓库级测试/闸**从该表派生**（⛔ 不在脚本里硬编码类型名）。
4. **RouterTLM 试点**（`node_x` / `node_y` / `mesh_x` / `mesh_y` / `buffer_size` / `credit_timeout`;**不含 `vc_count`** —— 其值不影响运行时行为）;同 commit 删除其 `registerParamRules` 注册、`get_param_rules()` 本体、**CFG-08 的全部 RouterTLM 引用**（含 `:327` 的 `type == "RouterTLM"` 析取 → 改为 `else if (type == "NICTLM")`）。
5. **`buffer_size` 的 credit 同源副作用显式保留**：`downstream_credits_` 重算**仅在深度实际变化时**发生（保证流量已发生后的二次加载不刷新已消耗额度）。
6. **spec 修订走 delta 机制**（`## REMOVED` / `## MODIFIED Requirements`），⛔ 不直接编辑 main spec。
7. 仓库级**运行时**对拍测试（⛔ 不用 grep 提取键名）：遍历 `configs/**/*.json`，对含已准入类型实例的配置调用 `instantiateAll`，断言成功。

## Out of Scope

- ⛔ `configs/param_rules/*.json` 的存废（`ADR-X.10` §F 另案;且「删除 json 不是可选项」—— 它是已 Deprecated 但仍在跑的 Python 生成期校验的活数据源）
- ⛔ **非标量参数**（数组 / 嵌套对象 / 字符串枚举，如 `address_regions` / `axi_adapter` / `target_type`）
- ⛔ **Python 链**、代码生成、schema 导出、宏
- ⛔ **NICTLM 的 CFG-08 分支** —— 必须在 NICTLM 自己的迁移 change 内与 `load_param("node_id", …, def = nullopt)` 一并删除;否则 `node_id` 必填**变成空窗**
- ⛔ **F-14 那一族的修复**（各模块独立 change;本 change 只提供检出机制）
- ⛔ `vc_count` 的运行时可配化（`NUM_VCS` 是编译期 `constexpr`）
- ⛔ **真源 1 文档同步**（`docs/architecture/06-inter-module-network.md` §9.4 · `10-topology-generation-management-system.md` §8.2-8.4）—— 剥离为 doc-only 后续 change（1/16 类型迁移完成时写不准确;写完还得改 15 次）

## 已知残留（本 change 不闭合 · 须具名 owner）

| # | 残留 | 影响 |
|:--:|---|---|
| R-1 | `configs/param_rules/router_tlm.json:33-40` 仍声明 `buffer_size` | 落地后 `buffer_size` **仍有第二个声明面** ⇒ delta spec 的「同一参数 SHALL NOT 在两处各写一遍」在**该文件删除前只能视为渐进达成** |
| R-2 | `configs/param_rules/router_tlm.json:24-32` 的 `vc_count` 仍在，`flit_width` 随 `get_param_rules()` 消失 | 声明面 ① 与 ③ 的漂移**未消除** |
| R-3 | legacy 扁平路径（`module_factory.cc:405-408` 的 `cfg_src = &mod`） | **显式保留**并在未知键检查中豁免（写进 design D-3a）;若改为特判 `params`，则扁平配置永久绕过必填 |
| R-4 | `openspec/specs/param-validation/spec.md:14-24` 的「mesh 范围 1-16」与事实冲突（`configs/mesh_4x4.json` 有 `node_x: 0`） | 本 change 的 T6.2 只改执行者与必填术语;该范围条款的订正另案登记 |

## 验收标准

> 每条 AC 尾部标注其对应的 delta spec Scenario（`S<n>` = `specs/module-param-load-declares/spec.md` 中按出现顺序的第 n 个 Scenario）。**每个 Scenario 至少被一条 AC 覆盖**（覆盖矩阵见 §测试覆盖审查）。

| # | 判据 |
|:--:|---|
| **AC-01** | **夹具模块**（仅测试用，2 个标量参数，其一带默认值）⇒ 越界 / 类型错（`"abc"`）两种配置分别使 `instantiateAll` 返回 **`false`**，且错误信息含**模块名 + 键名 + 期望范围**;异常类型为 `cpptlm::ParamValidationError`（`include/core/param_errors.hh`）↔ S1,S2 |
| **AC-02** | 夹具模块的**两个**参数各有一条「两个不同合法值 ⇒ 可观测指标不同」的测试（行为判别）↔ S11 |
| **AC-03** | **未迁移类型零变化**：① `git diff <base> -- test/ include/ src/` 中，除本 change 新增的测试文件与 `test/test_d3_param_resolver_registry.cc` 外，任何既有 `REQUIRE` / `CHECK` 行的**删除或取值修改**均判红（允许新增断言）;② `git diff <base> -- configs/` **为空**（零配置面变更）;③ 全标签套件全绿;④ `PcieEndpointIP` 的二次 `on_config_loaded` 与 `CohIfNICTLM::regions_.clear()` 等既有副作用行为不变（由 `[pcie]` / `[cep]` 套件承载）↔ S5,S8 |
| **AC-04** | **运行时对拍 + 基线**：① **基线**（改动前先跑并留证据）—— `configs/**/*.json`（排除 `configs/param_rules/**`）中**含已准入类型实例**的配置，逐个 `instantiateAll` 并**记录成功/失败清单**;既存失败者登记为**豁免 + 附证据**（⛔ 不得静默 `try/catch` 吞掉）。② **改动后**：同一清单中基线成功者**仍成功**，且已准入类型实例的 `unknown_keys()` 为空。⛔ 不用 grep 提取键名 ↔ S7 |
| **AC-05** | 边界成对：`buffer_size` = `1` / `64` ⇒ 装配成功;`0` / `65` ⇒ 装配失败（**测试内构造，不改源码**）↔ S1 |
| **AC-06** | **无 `params` 键**的**夹具**实例 ⇒ 声明的默认值仍落到成员（夹具 SHALL 已进准入表，见 design D-2/D-3;RouterTLM 因成员初值恰等于声明默认值而不具判别力）↔ S4 |
| **AC-07** | **未知键在真实模块上生效**：RouterTLM 配 `params.buffer_sizes: 16` ⇒ `instantiateAll` 返回 **`false`**，错误文本含 `buffer_sizes`;⚠️ 且 `grep -nE 'unknown|buffer_sizes' src/tlm/router_tlm.cc` **零命中**（证明失败源是基类的未知键检查，不是模块内手写 throw）↔ S7 |
| **AC-08** | ① **NICTLM 必填零变化**：`test/test_config_schema_validation.cc:122/138/219` 三条断言保持绿;② **RouterTLM 在 CFG-08 零引用**：`grep -n 'RouterTLM' src/core/module_factory_validate.cc` **全部零命中**（文件级，不限段）↔ S8 |
| **AC-09** | **credit 同源 + 二次加载不刷新**：① 存量 `test_router_buffer_size_param.cc` 的 BSP-3（`inject_to_east == depth`）保持绿;② 先注入若干 flit（消耗 credit），再以**相同** `buffer_size` 二次 `set_config` ⇒ 后续 `inject_to_east` 的转发数**不增加** |
| **AC-10** | **默认值不写回 JSON**：声明带默认值的参数、配置中不提供该键 ⇒ `instantiateAll` 后 `cfg["modules"][n]["params"]` **SHALL NOT** 出现该键（夹具 + RouterTLM 各一条）↔ S6 |
| **AC-11** | **数值边界**：`buffer_size: 8.0`（JSON double）/ `true`（bool）/ `1e20` ⇒ 均装配失败;`credit_timeout: -1` ⇒ 装配失败（无符号）；且 `buffer_size: 8`（整数）⇒ 成功（⛔ 不得依赖 `get<int64_t>()` 的隐式转换）↔ S3 |
| **AC-12** | **legacy 扁平路径**：① RouterTLM 扁平形式（参数写在 module 条目上，**无 `params`**）⇒ 装配**成功**且**不**因 `name` / `type` 报未知键 ② ⚠️ **同形式缺 `node_x`** ⇒ 装配**失败**（证明豁免未吞必填）③ NICTLM 扁平形式（顶层 `node_id`）行为不变 ↔ S9 |
| **AC-13** | **无效果参数不得声明**：`grep -nE 'load_param[^;]*"vc_count"' src/tlm/router_tlm.cc` **零命中**（`vc_count` 的 `NUM_VCS` 仍是编译期常量 ⇒ 声明即退化为「声明了却无效」）↔ S11 |

**回归护栏（非变更探测器，改动前后均绿；仍是验收门）**

- **G-1** 加载幂等：同一实例二次加载 ⇒ 成员值不变（`ADR-X.17` V-7 的定性）↔ S5
- **G-2** 嵌套 RouterTLM 缺 `node_x` ⇒ `instantiateAll` 返回 false（`sim_module.hh:95` 的 `internal_factory->instantiateAll` 已让嵌套走同一必填路径）

## 测试覆盖审查（Scenario → AC 矩阵）

| Scenario | 覆盖 AC | 说明 |
|:--:|---|---|
| S1 参数越界失败 | AC-01, AC-05 | 夹具 + RouterTLM |
| S2 参数类型错误 | AC-01 | 字符串类型错（`"abc"`） |
| S3 数值边界不放宽 / 非整数拒绝 / 无符号拒绝负值 | **AC-11** | 本 AC 为审查新增（此前无判据） |
| S4 无 `params` 键仍得默认值 | AC-06 | ⚠️ 依赖夹具进准入表（design D-2/D-3） |
| S5 加载幂等 + 不清外部状态 | G-1（幂等）+ AC-03④（外部状态） | 同一 Scenario 拆两条承载 |
| S6 默认值 SHALL NOT 写回配置 JSON | AC-10 | 机制来源 = 注销 `ParamRules` 后工厂注入自然失效（design D-8） |
| S7 已准入类型上的拼错参数名 | AC-07, AC-04② | 真实模块 + 运行时对拍 |
| S8 未准入类型维持宽松 | AC-03, AC-08① | 全标签绿 + NICTLM 三条 |
| S9 legacy 扁平路径豁免 | **AC-12** | 本 AC 为审查新增（此前无判据） |
| S10 板级直读键登记 | T4.1（台账） | ⚠️ 无运行时判据 —— 见下 W-1 |
| S11 声明了却无效果的参数不得声明 | AC-02, **AC-13** | 行为判别 + grep 断言 |

## 已知测试薄弱点（本 change 接受）

| # | 薄弱点 | 理由 / 缓解 |
|:--:|---|---|
| W-1 | S9「板级直读键登记为已消费」只有**台账**判据，无运行时断言 | 本 change 只迁移 RouterTLM（其参数无板级直读消费）⇒ 无可测对象;真正需要时在含板级直读键的类型准入时补机制 |
| W-2 | AC-04 的基线可能含**既存装配失败**的配置（如 `configs/apu_soc_full.json` 含 `ComputeUnitTLM` / `TCCTLM` 等类型，是否仍注册需实测） | 基线清单 + 豁免证据（AC-04①），⛔ 不允许静默 catch |
| W-3 | AC-03① 的「既有 `REQUIRE`/`CHECK` 行零删除/改值」靠人工 diff 检查，非自动闸 | 由 verifier 实施期取证（`git diff` 过滤 `^[-+].*(REQUIRE\|CHECK)`）;保留为 AC 而非脚本闸，避免闸与格式耦合 |

## 关联

- **决策源**：`docs/adr/ADR-X.17-module-param-read-declares.md`（Proposed · v1.5;审稿报告 `.rddf/reviews/ADR-X.17.md`）
- **在 `ADR-X.10` 中的定位**：部分取代其决策 1 的「**规则存放位置**」维度;部分闭合其决策 4.5 / 7（未闭合 5 项须各自具名 owner change）
- **同族 backlog**：`.rddf/improvements/cpptlm-validate-params-dead-code.md` · `.rddf/improvements/cpptlm-router-vc-count-param-inert.md`
- **spec 修订义务**（走 delta）：`openspec/specs/param-derivation/`（`## REMOVED` 其 `get_param_rules()` 强制条款与 `flit_width` 派生 → `## MODIFIED` 为「已准入类型用 `load_param`;未准入类型 MAY 保留」）· `param-validation/`（`## MODIFIED` 执行者 + 必填术语）· `param-type-conversion/`（`## MODIFIED` 转换真值点）
