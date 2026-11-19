---
name: cpptlm-config-type-registration-align
priority: P1
phase: phase-10
category: bugfix
type: bug
状态: "✅ **已完成并归档**（2026-11-18 起草 → 立项 (`cpptlm-module-param-effect-landing` 的 R-5 转派) → 实施 → 归档;per 台账 **Entry #487** 归档回执 —— ✅ **已完成归档语义** · delta 已合并进 `openspec/specs/config-type-registration-align/`;归档件 `openspec/changes/archive/2026-11-18-cpptlm-config-type-registration-align/`，⚠️ 该目录被 `.gitignore` 忽略 ⇒ 持久记录以台账 Entry #487 为准）"
created: 2026-11-18
---

# improvement: cpptlm-config-type-registration-align

**主题**：`configs/**` 里有 **4 个类型**在**运行时注册表里不存在** ⇒ 装配时**该模块被静默丢弃** ⇒ **拓扑缺模块但装配成功** ⇒ 仿真结果**静默失真**。这是 `ADR-X.17`「读取即声明」原则在**类型面**的对应缺口 —— 参数键面已有「读取即声明」机制，**类型面完全没有**。

> **发现路径**：change `cpptlm-module-param-effect-landing` 实施期的**键盘点盘点**（`scripts/test/param_effect_inventory.py`）。盘点时发现 `ComputeUnitTLM` / `TCCTLM` 在 `include/` + `src/` 里**零类定义**，追查时用**运行时注册表**（而非类名）重新盘点 ⇒ 清单从 2 类扩到 **4 类 / 3 份配置**，且**失败形态**比原登记严重得多（见下）。

---

## Why

### 第 1 条 —— 实测清单：**4 个类型 / 3 份配置**引用了运行时未注册的类型

判据 = **运行时枚举 `ModuleFactory::getRegisteredTypes()`（67 个注册类型）** ∩ `configs/**/*.json`（排除 `configs/param_rules/**`）的 `type` 集合 ⇒ 差集：

| 配置里的 type | 实际状态 | 出现处 |
|---|---|---|
| `TmuDispatchProcessorTLM` | 类**存在**（`include/tlm/gpu/tmu_dispatch_processor_tlm.hh` 的 `tlm::gpu::TmuDispatchProcessor`）但**从未注册** ⇒ 且配置里的名字与类名**不一致** | `configs/dgpu_board_v1.json`（**嵌套**在 `soc` 内） |
| `ArbiterTLM` | 只注册了 `ArbiterTLM2` / `ArbiterTLM4`;**模板基类 `ArbiterTLM` 本身未注册** | `configs/library_memory_demo.json` |
| `ComputeUnitTLM` | 类**不存在**（历史 `GpuComputeUnitTLM` 已并入 `StreamingMultiprocessorTLM` —— `include/tlm/gpu/streaming_multiprocessor_tlm.hh:14` 注明 rename 合并） | `configs/apu_soc_full.json`（4 实例） |
| `TCCTLM` | 类**不存在**（全仓 `include/` + `src/` 零命中） | `configs/apu_soc_full.json`（2 实例） |

⚠️ **判据不能用「类名是否存在」**：上表两个最难案例（`TmuDispatchProcessorTLM` 有类但名字不同 / `ArbiterTLM` 是模板基类）都会被「类名存在性」判据**漏掉** ⇒ 判据集合必须**从运行时注册表派生**（零硬编码类型名）。

### 第 2 条 —— 失败形态：**装配成功但模块被静默丢弃**（不是「装不上」）

```cpp
// src/core/module_factory.cc（约 385 行）
} else {
    DPRINTF(MODULE, "[ERROR] Unknown or unregistered type: %s\n", type.c_str());
}
```

`DPRINTF` 之后**没有 `return false`** ⇒ 循环继续 ⇒ 未注册类型的模块**不进 `object_instances`** ⇒ `instantiateAll` **返回 true**。

**实测**：上述 3 份配置跑 `./build/bin/cpptlm_sim <config> --cycles 10` ⇒ **全部 exit 0、无任何报错**。

### 第 3 条 —— 且 Release 构建下**连日志都没有**（完全静默）

`include/core/sim_core.hh:8-16`：`DPRINTF` 仅在 `#ifdef DEBUG_PRINT` 下展开为 `printf`，否则 `do {} while(0)`;而 `DEBUG_PRINT` **只在 Debug 构建**加（`CMakeLists.txt:71-72`）⇒ **Release / CI 默认构建下，那条 `[ERROR]` 根本不产生**。

⇒ **失败是彻底静默的**（不是「有日志但容易忽略」）。

### 第 4 条 —— 后果：拓扑缺模块 ⇒ **仿真结果静默失真**（比「装不上」更坏）

「装不上」是**响亮**的失败（用户立刻知道）；「装上了但少个模块」是**静默**的失败 —— 用户以为拓扑完整，仿真照跑，**结果错了却没人知道**。这正是 `ADR-X.17` 反复强调的那族形态（静默丢弃比失败更坏）。

### 第 5 条 —— `dgpu_board_v1.json` 的派发链**今天就是断的**（不是单纯命名噪音）

`tmu` 在 `configs/dgpu_board_v1.json:52-54`，而 `DGpuSoc::simulate_instantiate`（`src/tlm/gpu/dgpu_soc.cc:10-13`）只走默认递归，全仓 `src/tlm/gpu/` 无任何 `TmuDispatchProcessor` 实例化;`cp.dispatch → tmu.dispatch_in`、`tmu.cta_out → sq.cta_in`、`cq.done_out → tmu.done_in` 三条连接（`:83` / `:84` / `:88`）**全部悬空** ⇒ **默认 23 ABI 入口配置的 CP→TMU→SQ 派发路径从未存在**，而 `test_dgpu_pcie_device_perspective.cc` 照样 `REQUIRE(board.load_soc_config(cfg))` 通过。

⇒ 处置必须**显式裁决**「这是 ABI 缺陷还是配置噪音」⇒ **不得**以「删条目让闸变绿」收场。

### 第 6 条 —— 两处配置是**生成物** ⇒ 只改 JSON 必然回漂

- `configs/apu_soc_full.json` 由 `examples/generate_apu_soc.py:120-186` 生成（`description` 文本也在 `:185-186` 拼出来）⇒ 改 JSON 而不改生成器 ⇒ 下次生成即回漂;
- `configs/library_memory_demo.json` 由 `examples/generate_library_configs.py:41-44` 生成，且 `cpptlm/library/standard.py:187` **无条件 emit** `"ArbiterTLM"` ⇒ 改 JSON 而不改 Python 库 ⇒ 回漂。

⇒ 对齐必须**改生成器 + 生成物 + 断言测试**三处同时。

### 第 7 条 —— 与 `ADR-X.17` 的关系

`ADR-X.17` D-1 立的是**参数键面**的「读取即声明」（键必须被消费或显式拒绝）;**类型面**至今无对应机制 —— 类型既不被声明也不被检查，只在 Debug 下打一行日志。⇒ 本 improvement 是把同一条原则**平移到类型面**，不是新造机制。

---

## What Changes

### ① 机制层 —— 未注册类型 ⇒ **装配失败**（顶层）

`ModuleFactory::instantiateAll` 新增 **Step 0.3 预扫描**：在**插件加载之后**（`module_factory.cc:328-338`，动态注册路径不受影响）、**实例创建之前**（`:350` 之前）遍历展开后的 `final_config`，对每个 `type` 查运行时注册表 ⇒ 命中未注册 ⇒ **返回 false** 并报出**类型名 + 实例名 + 来源文件**。

- 错误输出走 **`stderr`**（⛔ **不走 `DPRINTF`** —— Release 下无输出，等于没报）;
- ⛔ **不加 `--strict` 开关**：开关救不了嵌套路径（`SimModule::simulate_instantiate` 丢弃返回值），且与 `ADR-X.17` R-2「⛔ 禁止用静默豁免长期掩盖」冲突;
- 预扫描**在创建之前** ⇒ 失败时**零实例残留**（不产生半构造对象 / 不污染 `ModuleGroup` 静态表）;
- **不重构 `instantiateAll` 的其余部分**（该函数 528 行，承载 CEP 双闸 / `kCompositeOnly` / StreamAdapter 注入 ⇒ 只加一步）。

### ② 对齐层 —— 4 处漂移逐处处置

| 处 | 处置 | 判据 |
|---|---|---|
| `TmuDispatchProcessorTLM` | ⛔ **不可注册**（`class TmuDispatchProcessor {` **无基类** ⇒ 进不了两个注册表）⇒ 二选一：**(a)** 删条目 + 把 `cp.dispatch` 接线改为程序化注入 + 登记「该派发链今日本就是断的」;**(b)** 把它重构为 `ChStreamModuleBase` 派生真模块 | (a) 成本 1 天 / (b) 成本数量级更大 ⇒ **推荐 (a)**，但**必须**在台账写明默认 ABI 路径的派发能力现状 |
| `ArbiterTLM` | 改用 `ArbiterTLM2`（`library_memory_demo.json` 的 `channels=4` ⇒ 对应 `ArbiterTLM4`）;⚠️ 其 `params.n_ports` 在 `ArbiterTLM` 类里**无消费面** ⇒ 改后会变成新的零效果键 ⇒ **归 `cpptlm-module-param-effect-landing` 家族**，本 change 只登记不修 | 改配置 + 改 `cpptlm/library/standard.py:187` 的无条件 emit |
| `ComputeUnitTLM` ×4 | **移除条目**（⛔ 「实现」路径与 `StreamingMultiprocessorTLM` 已取代的事实冲突，且无规格来源 ⇒ 明确排除） | 改 `examples/generate_apu_soc.py` + 生成物 + `test/test_apu_soc_from_config.cc:114-164` 的 `modules.size()==16` 断言 |
| `TCCTLM` ×2 | 同上 | 同上 |

### ③ 判据层 —— 「配置 type ⊆ 运行时注册表」闸 + 可重跑工具

- **工具形态固定为「运行时导出 + 集合差」**：给 `cpptlm_sim` 加 `--list-types --json`（复用 `ModuleFactory::getRegisteredTypes()`）⇒ 脚本只读这一个源，**零源码解析、零类型硬编码**;
- **闸注册为 ctest**，扫描域 = `configs/**/*.json` + `configs/templates/*.json`（排除 `configs/param_rules/**`），作用于 `extends` / `include` / `${var}` 展开**之后**的结果;
- **闸必须带反向自测**（注入假 type ⇒ 必红）—— 只验证「绿」等于没验证;
- ⚠️ **类型集合是构建相关的**（`HybridCacheWrapper` 仅 `BUILD_RTL` 下注册）⇒ 闸必须**按构建运行**，不能静态白名单。

---

## Acceptance

| # | 判据 | 机械判定 |
|:--:|---|---|
| **AC-1** | **正向不误伤** —— 一份只含**已注册**类型的最小 config ⇒ `instantiateAll == true` 且实例存在 | 断言返回值 + 实例表 |
| **AC-2** | **负向 fail-fast** —— 同一 config 把 `type` 改成未注册名 ⇒ `instantiateAll == false`;**零实例残留**;`stderr` 同时含**类型名 + 实例名 + 来源文件** | 断言返回值 + `getAllInstances().empty()` + stderr 文本 |
| **AC-3** | **失败后可重入** —— 同进程内失败一次后再跑合法 config ⇒ 仍 `true` 且实例完好 | 断言返回值 + 实例表（动机：`ModuleGroup::registerInstance` 是**静态表裸指针**，失败路径有悬挂风险） |
| **AC-4** | **4 处漂移清零** —— 运行时注册表 ∩ 配置 `type` 集合的**差集为空**（3 份受影响配置逐个可装配） | 脚本输出差集为空 + 逐份 `instantiateAll` |
| **AC-5** | **生成器一致** —— 重跑 `examples/generate_apu_soc.py` 与 `examples/generate_library_configs.py` ⇒ `git diff --exit-code configs/` 为空 | 命令 + 退出码 |
| **AC-6** | **闸有判别力** —— 向 fixture config 注入假 type ⇒ 闸**红**;移除 ⇒ 闸**绿** | 反向 fixture（ctest 内） |
| **AC-7** | **零回归** —— `cpptlm_tests` 全量绿（≥ 基线 **883336 / 2393**）;`pytest` **232 passed**;`ctest` 全量绿 | 三条命令 |
| **AC-8** | **嵌套传播** —— 外层 SimModule 内联 `modules` 含未知类型 ⇒ 外层 `instantiateAll == false` | ⚠️ **依赖 `cpptlm-module-param-migrate-batch2` 的 T0.2**（嵌套失败传播）⇒ 未落地则本 AC 标 `blocked-by` 并**显式登记**，⛔ 不许静默跳过 |
| **AC-9** | **文档与台账同步** —— `docs/implementation/module-param-baseline.md` §2bis 改成「已闭合」;`scripts/README.md` 新脚本行;`AGENTS.md`（若新增 CLI flag） | 人工复核 + `docs_sync_check --strict` |

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|---|
| C-1 | **类型面**的「读取即声明」 —— 配置引用的每个类型必须已注册，否则装配失败并报出类型名 | ❌ 0（本仓非 CEP 域;CEP 侧不新增不变量） |
| C-2 | 「配置 type ⊆ 运行时注册表」成为**机器可判**的常态（不再靠人读 description 判断「愿景配置」） | ❌ 0 |

---

## Impact

| 面 | 改动 |
|---|---|
| **代码** | `src/core/module_factory.cc`（**+1 步预扫描**;其余零 diff）· `src/main.cpp`（`--list-types --json`） |
| **配置** | `configs/dgpu_board_v1.json` · `configs/library_memory_demo.json` · `configs/apu_soc_full.json`（3 份） |
| **生成器** | `examples/generate_apu_soc.py` · `examples/generate_library_configs.py` · `cpptlm/library/standard.py`（Python 侧无条件 emit） |
| **测试** | 新增 `test/test_config_type_registration.cc`（+1 文件）· 改 `test/test_apu_soc_from_config.cc`（`modules.size()` 断言）· `test/CMakeLists.txt`（登记新测试 / ctest 条目） |
| **工具** | 新增 `scripts/test/config_type_registration_check.sh`（差集闸 + 反向自测） |
| **文档** | `docs/implementation/module-param-baseline.md` §2bis · `scripts/README.md` · `AGENTS.md`（CLI flag 段）· 台账 Entry |
| **依赖** | 嵌套传播段 **blocked-by** `cpptlm-module-param-migrate-batch2` T0.2;两者的文件冲突面 = `src/core/module_factory.cc`（C2 删默认值注入段 / R-5 加预扫描段 ⇒ **不同位置**，须 rebase 协调） |

**KPI 现状**：本 change 立项 ⇒ **2/3 → 3/3**（占满）⇒ **C1 归档前不得再开新提案** ⇒ 本 change 必须能在 C1 归档后**独立收口**，不依赖「等下一个 change」。
