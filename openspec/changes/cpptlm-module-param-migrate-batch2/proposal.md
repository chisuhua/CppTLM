# proposal: cpptlm-module-param-migrate-batch2

## Why

`ADR-X.17`（模块参数「读取即声明」）的**机制**已落地，但**覆盖面**几乎没动：20 处参数读取点里只迁了 1 个类型（`RouterTLM`）。三个后果：

1. **双真值仍在** —— 必填强制点有两个（`ADR-X.17` F-4）：CFG-08 的 per-type 硬编码分支（`NICTLM`）与模块内 `load_param` 必填声明并存 ⇒ 同一个「必填」概念有两套写法。
2. **C++ 侧 `ParamRules` 机制已成死代码** —— `RouterTLM` 迁移后 `registerParamRules` **零生产调用方**、`validate_module_params` **零调用方**、注册表只剩一张空表（`ADR-X.10` A 表：决策 4.5 / 5 / 6 / 7 Stage 2 **全部未实施**）⇒ 留着会让后续读者以为「规则表仍是活的声明面」。
3. **嵌套装配失败不上传** —— `SimModule::simulate_instantiate` 调 `internal_factory->instantiateAll(config)` 后**丢弃返回值**（`include/core/sim_module.hh:95-98`）⇒ 嵌套实例的必填声明即使执行了，失败也不会让外层 `instantiateAll` 返回 false ⇒ **迁移对嵌套实例形同虚设**。

⇒ 而 `cpptlm-module-param-effect-landing`（并行 change）正在给零效果键补效果 —— 一旦那些类型迁移，**零效果键会从「静默丢弃」突变为「未知键 ⇒ 存量配置装不上」** ⇒ 机制侧必须先把这批类型迁完（或明确不迁）。

## What Changes

1. **逐类型迁移**（design D-2 的清单）：把**已有配置读取面**的 **21 个类型**迁到 `load_param`（每类型：声明化 + 行为判别测试 + 准入表加项 + 删旧声明面）。
2. **嵌套失败传播**（前置）：`SimModule::simulate_instantiate` 的内部装配失败 SHALL 让外层 `instantiateAll` 返回 false。
3. **CFG-08 的 `NICTLM` 分支退役**（与 `NICTLM` 的 `load_param("node_id")` 必填声明**同批**完成 ⇒ 必填真值唯一）。
4. **C++ 侧 `ParamRules` 机制退役**：删 `include/core/param_rules.hh` / `registerParamRules` / `getParamRulesRegistry` / `validate_module_params` / 工厂侧的默认值注入循环;改写 `test/test_d3_param_resolver_registry.cc`（其 RouterTLM 用例已在上一 change 删除 ⇒ 剩下的通用机制用例随机制一并退役）。
5. **spec delta**：`module-param-load-declares`（全量准入后的表述）· `param-derivation`（`ParamRule structure` 退役 + 声明面表述）· `param-validation`（范围校验执行者 + 必填真值唯一 + 「mesh 范围 1-16」条款订正）。

## Out of Scope

| 项 | 归属 |
|---|---|
| `configs/param_rules/*.json` 的存废定位（保留为生成期规则面 / 降级为派生副本 / 废弃） | ⚠️ **需独立 ADR 级裁决**（`ADR-X.10` §F）⇒ architect 域;**本 change 不裁决、也不删除该文件**（它是 `cpptlm_config/validator.py` PARAM-01/02 的活数据源，删除会静默关闭生成期校验并打挂 `scripts/test/test_off.sh:84` 的 pytest） |
| Python 生成期校验能力的迁往（`cpptlm_config` 已 Legacy / Deprecated，继任者 `cpptlm/` 零 `param_rules` 引用） | 同上裁决的一部分 |
| `derive_expr` 派生求值 / 跨参数约束的实现（`ADR-X.10` 决策 5 / 决策 4.5 后半 / 决策 7 Stage 2） | 独立 change（**当前无活用户** —— 唯一声明过 `derive_expr` 的 `RouterTLM` 已退役该声明面） |
| 零效果键的语义落地 | change `cpptlm-module-param-effect-landing` |
| `vc_count` 的容器级改造 | 沿用 `cpptlm-router-vc-count-param-inert` 的「只登记不修」结论 |

## 已知残留（本 change 不闭合 · 须具名 owner）

| # | 残留 | 处置 |
|---|---|---|
| R-1 | `configs/param_rules/*.json` 与 C++ 侧的**双声明面**仍在（json 仍声明 `vc_count` / `derive_expr` / `buffer_size`） | `ADR-X.10` §F 裁决（architect）;本 change 的 C++ 侧退役**收窄**了该问题为「json 是否保留为生成期规则面」 |
| R-2 | `ADR-X.10` 决策 5 / 决策 6（类型感知转换）/ 决策 7 Stage 2 未实施 | 独立 change;⚠️ 决策 6 的 latency / address 转换由 `ModuleFactory` 的**连接**路径承担（不属模块参数面）⇒ 本 change 不动 |
| R-3 | `cpptlm-module-param-effect-landing` 尚未完成 ⇒ 本 change **不迁**那些「零效果键未裁决」的类型 | 依赖顺序见 design D-5 |
| R-4 | `ADR-X.10` A 表「决策 1 选了什么」的标签冲突（选项表 B/C vs 使用方式 A/B） | architect 域（ADR 正文不可变）⇒ 登记待订正 |

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-01** | **准入表全量化** —— `cpptlm::admitted_param_types()` 的条目集合 = design D-2 清单中**已迁类型**的集合（测试侧断言逐项相等，⛔ 不得在测试里硬编码第二份清单 ⇒ 从 `admitted_param_types()` 派生） |
| **AC-02** | **逐类型行为判别** —— 每个迁入类型 SHALL 有覆盖其**全部**声明键的行为判别测试（两个不同合法值 ⇒ 可观测指标不同 —— `ADR-X.17` D-6）;⛔ 不得有「声明了却无效果」的键（承接 `module-param-behavioral-evidence` Requirement） |
| **AC-03** | **必填真值唯一** —— ① `load_param`（无默认值）= 唯一必填来源;② CFG-08 的 per-type 硬编码分支**零残留**（判据 = `validateConfig` 函数体非注释行零命中）;③ `getParamRulesRegistry()` 无任何生产调用方 |
| **AC-04** | **ParamRules 机制退役** —— `include/core/param_rules.hh` / `validate_module_params` / `registerParamRules` / `getParamRulesRegistry` / 工厂侧默认值注入循环**零残留**（`grep` 判据）;⛔ 不得留下「注释掉的死代码」形态的替代品 |
| **AC-05** | **嵌套失败传播** —— 嵌套容器内某模块缺必填参数 ⇒ 外层 `instantiateAll` 返回 **false**;且该失败**不再**被静默吞掉（`simulate_instantiate` 的返回值不得丢弃） |
| **AC-06** | **零回归** —— `cpptlm_tests` 全量绿（assertions / cases ≥ 基线 **883191 / 2371**）;`git diff <base> -- configs/` **不含参数键增删**（本 change 不改参数面）;⚠️ **例外（2026-11-19 实测补记）**：3 份配置**缺顶层 `connections`** ⇒ `validateConfig` 直接拒（结构性缺陷，非参数面）⇒ 各补一行 `"connections": []`（`dgpu_soc_minimal_v1` / `dgpu_soc_timing_v1` / `templates/cpu_cluster_2level`）;这些配置此前因相关类型未准入而被判据跳过，准入后被捕获 |
| **AC-07** | **未知键检出扩面有效** —— 已迁类型上的拼错键 ⇒ 装配失败且错误含该键名（对**至少 3 个不同类型**取证，⛔ 不得只用 `RouterTLM`） |
| **AC-08** | **板级直读键三态合规** —— F-15 的板级直读键（`MemoryTLM` / `VramControllerTLM` / `GmmuTLM` / `PcieEndpointIP`）SHALL 满足 `ADR-X.17` D-3 三态之一（① 改走已声明入口 / ② 登记为「已消费但不经 `load_param`」 / ③ 该类型整体排除）;⛔ 同一类型内不得同时存在「`load_param` 声明的键」与「板级直读的键」 |
| **AC-09** | **台账一致** —— `docs/implementation/module-param-baseline.md` 表 A（板级直读键）逐条对应到实现;表 B 的零效果键状态与 `cpptlm-module-param-effect-landing` 的裁决一致（两 change 不得互相矛盾） |
| **AC-10** | **spec 一致** —— `openspec validate --changes --strict` 通过;`param-validation` 的「mesh 范围 1-16」条款 SHALL 与事实一致（`configs/mesh_4x4.json` 存在 `node_x: 0`）⇒ 该条款被订正或标注为「未实施」 |

**回归护栏**（改动前后均绿）

- **G-1** `RouterTLM` 行为零变化 —— `[param]` / `[router]` 全绿
- **G-2** 既有副作用零变化 —— `[pcie]` / `[cep]` / `[config][schema]` 全绿

## 测试覆盖审查（Scenario → AC 矩阵）

| Scenario | 覆盖 AC | 说明 |
|:--:|---|---|
| S1 已准入类型的参数 SHALL 全部经 load_param 声明 | AC-01, AC-02 | 准入表全量化 + 逐键判别 |
| S2 已准入类型上的拼错参数名 SHALL 装配失败 | AC-07 | 扩面取证（≥3 类型） |
| S3 未准入类型 SHALL 维持宽松 | AC-01 | 「未准入」集合收缩为测试夹具 |
| S4 必填 SHALL 只有唯一真值来源 | AC-03 | CFG-08 分支退役 + `load_param` |
| S5 嵌套实例的装配失败 SHALL 上传 | AC-05 | 失败传播 |
| S6 板级直读的键 SHALL 满足三态之一 | AC-08 | `ADR-X.17` D-3 |
| S7 C++ 侧 ParamRules 机制 SHALL 退役 | AC-04 | 死代码清零 |
| S8 参数范围校验 SHALL 在模块内执行 | AC-03, AC-10 | 执行者唯一 + 条款订正 |
| S9 加载 SHALL 幂等 + 默认值不写回 JSON | AC-06 | 零回归 |

## 已知测试薄弱点（本 change 接受）

| # | 薄弱点 | 理由 / 缓解 |
|:--:|---|---|
| W-1 | AC-02 的「全部声明键都有判别测试」靠**逐类型人工核对**（`load_param` 调用点是唯一枚举源，但「键 ⇒ 测试」的映射不可自动判定） | `ADR-X.17` D-6 明示「纪律而非机制」⇒ 保留为 AC + 逐类型 review，不造闸 |
| W-2 | AC-01 的准入表断言**只能覆盖已迁类型**（未迁类型不在表内 ⇒ 不在断言集合内） | 判据 = 「表内条目 ⊇ 已迁类型集合」+ 「未迁类型不在表内」双侧断言（测试内从 design 清单派生） |
| W-3 | AC-05 的嵌套失败传播会**改变外层返回语义**（此前外层返回 true） | 这是**有意的**修正（失败被吞是缺陷）;⚠️ 既有测试若依赖「外层仍 true」须逐条评估并在 tasks 登记 |

## 关联

- 决策源：`docs/adr/ADR-X.17-module-param-read-declares.md`（**D-3** 加载逻辑执行点与按类型退役 / D-3b 显式登记 / **D-6** 行为判别义务 / F-4 双强制点 / F-15 板级直读 / F-16 CFG-08）
- 相关 ADR：`docs/adr/ADR-X.10-parameter-framework.md`（§A 决策实施状态 / **§F `configs/param_rules/*.json` 存废未裁决** / §C 现役声明面）
- 基线台账：`docs/implementation/module-param-baseline.md`（表 A / 表 B）
- 前序 change（已归档 2026-11-17）：`cpptlm-module-param-read-declares`（机制 + `RouterTLM` 试点）· 台账 Entry #472 / #473 / #474
- 并行 change：`cpptlm-module-param-effect-landing`（零效果键的语义落地 ⇒ 依赖顺序见 design D-5）
- 关联 improvement（backlog）：`cpptlm-validate-params-dead-code`（**C++ 侧诉求由本 change 承接**;Python 生成期诉求归 `ADR-X.10` §F）· `cpptlm-router-vc-count-param-inert`（只登记不修）
