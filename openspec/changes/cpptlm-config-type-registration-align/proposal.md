# cpptlm-config-type-registration-align

## Why

`configs/**` 里有 **4 个类型**在**运行时注册表里不存在**（实测：`TmuDispatchProcessorTLM` · `ArbiterTLM` · `ComputeUnitTLM` · `TCCTLM`，涉及 **3 份配置**）。

`ModuleFactory::instantiateAll` 遇到未注册类型时只打一行 `DPRINTF` 然后**继续**（`src/core/module_factory.cc` 约 385 行）⇒ **装配成功但该模块被静默丢弃** ⇒ **拓扑缺模块** ⇒ **仿真结果静默失真**。而 `DPRINTF` **只在 Debug 构建生效**（`include/core/sim_core.hh:8-16` + `CMakeLists.txt:71-72`）⇒ **Release / CI 默认构建下连日志都没有**。

实测三份受影响配置跑 `./build/bin/cpptlm_sim <config> --cycles 10` ⇒ **全部 exit 0、零报错**。

这是 `ADR-X.17`「读取即声明」原则在**类型面**的缺口：参数**键**面已有「读取即声明」机制（键必须被消费或显式拒绝），**类型**面至今无对应机制 —— 类型既不被声明也不被检查，只在 Debug 下打一行日志。

**来源**：change `cpptlm-module-param-effect-landing` 的键盘点盘点顺带暴露（其 **R-5**）；metis gap 分析已执行（6 节）。

## What Changes

### ① 机制层 —— 未注册类型 ⇒ 装配失败（顶层）

`ModuleFactory::instantiateAll` 新增 **Step 0.3 预扫描**：在**插件加载之后**（`:328-338`，动态注册路径不受影响）、**实例创建之前**（`:350` 之前）遍历展开后的 `final_config`，逐个 `type` 查运行时注册表 ⇒ 命中未注册 ⇒ **返回 false** 并报出**类型名 + 实例名 + 来源文件**。

- 错误走 **`stderr`**（⛔ 不走 `DPRINTF` —— Release 下无输出等于没报）;
- ⛔ **不加 `--strict` 开关**：开关救不了嵌套路径（`SimModule::simulate_instantiate` 丢弃返回值），且与 `ADR-X.17`「⛔ 禁止用静默豁免长期掩盖」冲突;
- 预扫描在创建之前 ⇒ 失败时**零实例残留**;
- **不重构 `instantiateAll` 其余部分**（528 行，承载 CEP 双闸 / `kCompositeOnly` / StreamAdapter 注入 ⇒ 只加一步）。

### ② 对齐层 —— 4 处漂移逐处处置

| 处 | 处置 |
|---|---|
| `TmuDispatchProcessorTLM`（`configs/dgpu_board_v1.json`，**嵌套**） | ⛔ **不可注册**（`class TmuDispatchProcessor {` **无基类**）⇒ 推荐 **删条目 + `cp.dispatch` 接线改程序化注入 + 登记「该派发链今日本就是断的」**;备选 = 重构为 `ChStreamModuleBase` 真模块（成本数量级更大） |
| `ArbiterTLM`（`configs/library_memory_demo.json`） | 改用 `ArbiterTLM4`（`channels=4`）;其 `params.n_ports` 无消费面 ⇒ 改后成为新零效果键 ⇒ **归 `cpptlm-module-param-effect-landing` 家族**，本 change 只登记;同时修 `cpptlm/library/standard.py:187` 的无条件 emit |
| `ComputeUnitTLM` ×4 / `TCCTLM` ×2（`configs/apu_soc_full.json`） | **移除条目**（⛔ 「实现」路径无规格来源且与 `StreamingMultiprocessorTLM` 已取代的事实冲突 ⇒ 明确排除）;须同改**生成器** `examples/generate_apu_soc.py` + 生成物 + `test/test_apu_soc_from_config.cc:114-164` 的 `modules.size()==16` 断言 |

### ③ 判据层 —— 「配置 type ⊆ 运行时注册表」闸

- 工具形态固定为「**运行时导出 + 集合差**」：给 `cpptlm_sim` 加 `--list-types --json`（复用 `ModuleFactory::getRegisteredTypes()`）⇒ 脚本只读这一个源，**零源码解析、零类型硬编码**;
- 闸注册为 **ctest**，扫描域 = `configs/**/*.json` + `configs/templates/*.json`（排除 `configs/param_rules/**`），作用于 `extends` / `include` / `${var}` 展开**之后**的结果;
- 闸**必须带反向自测**（注入假 type ⇒ 必红）;
- ⚠️ 类型集合**构建相关**（`HybridCacheWrapper` 仅 `BUILD_RTL` 下注册）⇒ 闸按构建运行，**禁静态白名单**。

## Out of Scope

| # | 排除项 | 理由 |
|:--:|---|---|
| 1 | **嵌套**（SimModule 内联 `modules`）的失败传播 | **blocked-by** `cpptlm-module-param-migrate-batch2` 的 T0.2（已在其 tasks 立为 AC-05）⇒ 本 change 重复实现 = 同文件二次改动;本 change 只在 AC-8 显式标注依赖 |
| 2 | 实现 `ComputeUnitTLM` / `TCCTLM` | 无规格来源 + `ComputeUnitTLM` 已被 `StreamingMultiprocessorTLM` 取代（`include/tlm/gpu/streaming_multiprocessor_tlm.hh:14`）⇒ 从零做架构 |
| 3 | 实现 `TmuDispatchProcessor` 真模块 | 本 change 只删条目 + 登记派发链现状;真模块化属独立 feature |
| 4 | `ArbiterTLM.n_ports` 的零效果处置 | 属 `cpptlm-module-param-effect-landing` 的 F-14 家族（键面）⇒ 本 change 只登记 |
| 5 | `configs/param_rules/**` | `ADR-X.10` §F 未裁决 ⇒ 不动 |
| 6 | `instantiateAll` 的其余重构 | 该函数承载 CEP 双闸 / `kCompositeOnly` / StreamAdapter 注入 ⇒ 只加一步预扫描 |
| 7 | 删 `configs/apu_soc_full.json` 整文件 | 会连带 `test_apu_soc_from_config.cc` 与 `test_cep_contracts_crosscheck.cc:334-351`（要求 4 份 config 中 ≥3 存在）两条**无关**断言 ⇒ 只改内容 |

## 已知残留（本 change 不闭合 · 须具名 owner）

| # | 残留 | 处置 |
|:--:|---|---|
| **R-1** | **默认 ABI 入口配置的 CP→TMU→SQ 派发路径今天就是断的**（`dgpu_board_v1.json` 的三条连接全部悬空;全仓 `src/tlm/gpu/` 无 `TmuDispatchProcessor` 实例化）⇒ 本 change 删条目后该事实**依然成立**且**变得更显眼** | 台账显式登记「派发能力现状」;补齐该路径属独立 feature |
| **R-2** | `ArbiterTLM.n_ports` 在改为 `ArbiterTLM4` 后成为新的零效果键 | 归 `cpptlm-module-param-effect-landing`（键面家族） |
| **R-3** | 「愿景配置」（声明要装但还没装的类型）**没有机器可读标记** —— 目前只有 free-text `description` ⇒ 本 change 的闸只能「零豁免」通过 | 若将来需要保留未实现类型的配置 ⇒ 引入机器可读标记（如 `"assembly": "unsupported"`）属独立 change;⛔ 本 change **不得**引入万能豁免 |
| **R-4** | 与 `cpptlm-module-param-migrate-batch2` 的文件冲突面 = `src/core/module_factory.cc`（C2 删默认值注入段 / 本 change 加预扫描段 ⇒ **不同位置**） | 若 C2 先合并 ⇒ 本 change 须 rebase 到 C2 的行号;登记于 design D-4 |

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | **正向不误伤** —— 只含**已注册**类型的最小 config ⇒ `instantiateAll == true` 且实例存在（断言**返回值 + 实例表**,⛔ 不断言日志） |
| **AC-02** | **负向 fail-fast** —— 同一 config 的 `type` 改成未注册名 ⇒ `instantiateAll == false`;**零实例残留**（`getAllInstances().empty()`）;`stderr` 同时含**类型名 + 实例名 + 来源文件**（⛔ 走 `stderr`,⛔ 不走 `DPRINTF` —— Release 下后者无输出） |
| **AC-03** | **失败后可重入** —— 同进程内失败一次后再跑合法 config ⇒ 仍 `true` 且实例完好（动机：`ModuleGroup::registerInstance` 是**静态表裸指针**,失败路径有悬挂风险） |
| **AC-04** | **4 处漂移清零** —— 运行时注册表 ∩ 配置 `type` 集合的**差集为空**;3 份受影响配置逐个可装配 |
| **AC-05** | **生成器一致** —— 重跑 `examples/generate_apu_soc.py` 与 `examples/generate_library_configs.py` ⇒ `git diff --exit-code configs/` 为空 |
| **AC-06** | **闸有判别力** —— 向 fixture config 注入假 type ⇒ 闸**红**;移除 ⇒ 闸**绿**（只验绿 = 没验） |
| **AC-07** | **零回归** —— `cpptlm_tests` 全量绿（≥ **883336 / 2393**）;`pytest test/python/` **232 passed**;`ctest` 全量绿 |
| **AC-08** | **嵌套传播** —— 外层 SimModule 内联 `modules` 含未知类型 ⇒ 外层 `instantiateAll == false`;⚠️ **blocked-by** `cpptlm-module-param-migrate-batch2` T0.2 ⇒ 未落地则标 `blocked-by` 并显式登记,⛔ 不许静默跳过 |
| **AC-09** | **文档与台账同步** —— `docs/implementation/module-param-baseline.md` §2bis 改「已闭合」;`scripts/README.md` 新脚本行;`AGENTS.md`（新增 CLI flag）;台账 Entry |

**回归护栏**（改动前后均绿）

- **G-1** `[param]` / `[config][schema]` / `[cep]` / `[e2e]` 全绿
- **G-2** `cpptlm_tests` 全量 + `pytest` + `ctest` 三绿

## 测试覆盖审查（Scenario → AC 矩阵）

| Scenario | 覆盖 AC | 说明 |
|:--:|---|---|
| S1 配置引用的类型必须已注册，否则装配失败且报出类型名 | AC-01, AC-02 | 正负对照 ⇒ 判别力来自**返回值 + 实例表** |
| S2 装配失败不得留下半构造状态 | AC-03 | 可重入 ⇒ 防 `ModuleGroup` 静态表悬挂 |
| S3 配置与运行时注册表必须一致（差集为空） | AC-04, AC-06 | 机械对拍 + 反向 fixture |
| S4 配置生成物与生成器必须一致 | AC-05 | 防回漂 |
| S5 未知类型不得被静默丢弃 | AC-02, AC-07 | ⛔ 判据禁断言日志 |
| S6 本 change 不得改变未涉及的行为面 | AC-07 | 零回归 |

## 关联

- **来源**：change `cpptlm-module-param-effect-landing` 的 **R-5**（其键盘点盘点的副产品）;台账 Entry #480（立项）
- **原则源**：`docs/adr/ADR-X.17-module-param-read-declares.md`（读取即声明;⛔ 禁止静默豁免;类型面为空白区）
- **基线台账**：`docs/implementation/module-param-baseline.md` §2bis（未注册类型登记）
- **依赖**：`cpptlm-module-param-migrate-batch2`（T0.2 嵌套失败传播 ⇒ AC-08 blocked-by）
- **improvement**：`.rddf/improvements/cpptlm-config-type-registration-align.md`
