# design: cpptlm-config-type-registration-align

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（「读取即声明」原则;类型面为空白区）;来源 = change `cpptlm-module-param-effect-landing` 的 **R-5**。
> 本文件是**实施形态**的唯一真源 —— 实施期新增事实必须回填 D-1~D-6。

## D-1 — 四处漂移的**性质表**（逐条含证据列）

| # | 配置里的 type | 类的实况 | 注册实况 | 配置位置 | 嵌套? | 处置 |
|:--:|---|---|---|---|:--:|---|
| 1 | `TmuDispatchProcessorTLM` | 类**存在**（`include/tlm/gpu/tmu_dispatch_processor_tlm.hh:19` 的 `class TmuDispatchProcessor {`）⇒ **无基类** ⇒ 进不了 `SimObject` / `SimModule` 两个注册表 | **从未注册**;且配置里的名字与类名**不一致** | `configs/dgpu_board_v1.json:52-54` | **是**（在 `soc` 的内联 `modules` 内）⇒ 顶层 fail-fast **覆盖不到** | **删条目** + `cp.dispatch` 接线改程序化注入 + 登记派发链现状（R-1） |
| 2 | `ArbiterTLM` | 模板类 `class ArbiterTLM : public ChStreamModuleBase`（`include/tlm/arbiter_tlm.hh:64`）⇒ **基类本身不可注册**（只注册实例化规格） | 只注册 `ArbiterTLM2` / `ArbiterTLM4`（`include/chstream_register.hh:125-126`） | `configs/library_memory_demo.json`（`params.n_ports: 4`） | 否 | 改用 `ArbiterTLM4`;`n_ports` 的零效果归 C1 家族（R-2）;同步修 `cpptlm/library/standard.py:187` 的无条件 emit |
| 3 | `ComputeUnitTLM` | 类**不存在**（历史 `GpuComputeUnitTLM` 已并入 `StreamingMultiprocessorTLM` —— `include/tlm/gpu/streaming_multiprocessor_tlm.hh:14` 注明 rename 合并） | — | `configs/apu_soc_full.json`（**4** 实例） | 否 | **移除条目**（⛔ 「实现」路径明确排除 —— 无规格来源） |
| 4 | `TCCTLM` | 类**不存在**（全仓 `include/` + `src/` 零命中） | — | `configs/apu_soc_full.json`（**2** 实例） | 否 | **移除条目** |

> ⚠️ **判据不许用「类名存在性」**：#1（有类但名字不同）与 #2（模板基类）都会被漏判 ⇒ 判据集合**必须从运行时注册表派生**。

## D-2 — 机制落点：`instantiateAll` 的 **Step 0.3 预扫描**

```
instantiateAll(config)
  ├─ Step 1   processExtends / JsonIncluder / VarResolver  （:154-165）⇒ 得到展开后的 final_config
  ├─ Step 2   validateConfig                                （:166）
  ├─ Step 3   PluginLoader 加载插件                          （:328-338）⇒ 动态注册路径的类型此时已进表
  ├─ ★Step 0.3  未注册类型预扫描                             （新增，落在 :339 与 :350 之间）
  │             · 遍历 final_config["modules"]（含嵌套？⛔ 不含 —— 嵌套归 C2）
  │             · 每个 type 查 getRegisteredTypes()
  │             · 命中未注册 ⇒ stderr 报「类型名 + 实例名 + 来源文件」+ return false
  ├─ Step 4   实例创建（:350-453）
  └─ ...
```

| 决策 | 理由 |
|---|---|
| **放在插件加载之后** | 插件类型此时已注册 ⇒ 不会误伤动态注册路径（实测 CEP 协议引擎是**静态**注册，但插件路径存在） |
| **放在实例创建之前** | 失败时**零实例残留**（不产生半构造对象、不污染 `ModuleGroup` 静态表裸指针）⇒ AC-02 的「零残留」与 AC-03 的「可重入」由**位置**保证，而非靠事后清理 |
| **只扫顶层 `modules[]`** | 嵌套传播归 C2（AC-08 blocked-by）⇒ 本 change **不碰** `simulate_instantiate` 签名 |
| **报错走 `stderr`** | `DPRINTF` 仅 Debug 生效 ⇒ Release 下等于没报（实测事实） |
| **不加 `--strict` 开关** | 开关救不了嵌套路径（`include/core/sim_module.hh:95-97` 丢弃返回值）⇒ 给了开关就会留下「非严格模式即可」的叙事出口 ⇒ 与 `ADR-X.17`「⛔ 禁止静默豁免」冲突。先例：`module_factory.cc:364-373` 的 `kCompositeOnly` 就是直接 `return false`，无开关 |
| **一次报全部违规** | 逐个 `return` 会变成「修一个再跑一次」的长循环（`apu_soc_full.json` 有 6 处）⇒ 收集后一次报 |
| **零硬编码类型名** | 预扫描只查注册表 ⇒ 不引入第二份类型白名单 |

> ⛔ **不顺手重构 `instantiateAll`**：该函数 528 行，承载 CEP 双闸 / `kCompositeOnly` / StreamAdapter 注入 ⇒ 本 change 只**加一步**，其余零 diff。

## D-3 — 判据工具形态：「运行时导出 + 集合差」

| 决策 | 理由 |
|---|---|
| **导出口 = `cpptlm_sim --list-types --json`**（复用 `ModuleFactory::getRegisteredTypes()`，`include/core/module_factory.hh:215-220`） | 判据集合**从运行时注册表派生** ⇒ 零源码解析、零类型硬编码、不会与实现漂移 |
| ⛔ **不扩展 `scripts/test/param_effect_inventory.py` 的 `undefined-type` 判据** | 它是**名字匹配**（`find_headers()` 按类名找定义;`_iter_with_base()` 对 `ArbiterTLM4` 退到基类）⇒ **恰好漏掉本 change 两个最难案例**（#1 有 `using` 别名 / #2 模板定义）⇒ 接进 ctest 只是形式主义 |
| **闸的扫描域** = `configs/**/*.json` + `configs/templates/*.json`，排除 `configs/param_rules/**`，作用于 `extends`/`include`/`${var}` 展开**之后** | 展开前的静态扫描会漏掉 `${var}` 注入的类型 |
| **闸必须带反向自测** | 只验「绿」= 没验（CEP 家族 `*_reverse_verify.sh` 先例） |
| **闸按构建运行** | 类型集合**构建相关**（`HybridCacheWrapper` 仅 `BUILD_RTL` 注册，`include/chstream_register.hh:298-306`）⇒ 静态白名单方案在此直接失效 |
| **默认零豁免** | 闸上线第一次跑必须零豁免通过；豁免集合要么为空，要么每条含 `path + reason` 且与台账 §2bis 一一对应 |

## D-4 — 与 `cpptlm-module-param-migrate-batch2`（C2）的关系

| 面 | C2 | 本 change | 处置 |
|---|---|---|---|
| **嵌套失败传播** | T0.2 / AC-05 独占 | ⛔ 不实现 | AC-08 标 `blocked-by`;若 C2 未落地 ⇒ 本 change 的嵌套段**交付为空**并显式登记，⛔ 不悄悄缩范围 |
| **文件冲突面** | `src/core/module_factory.cc`（删默认值注入段 `:263-291`）+ `include/core/sim_module.hh`（改基类调用序列 + 签名） | `src/core/module_factory.cc`（**加**预扫描段 `:339`-`:350` 之间） | **不同位置** ⇒ 可并行;若 C2 先合并 ⇒ 本 change rebase 行号（R-4） |
| **判据域** | 参数**键**面（未知键检出） | 类型**面**（未注册类型） | 两面互补，不重叠 |
| **执行顺序** | — | — | 本 change **T1（对齐）必须先于 T2（上闸）** —— 否则 `test/test_param_alignment.cc`（会实例化含已准入类型的配置）与 `test/test_dgpu_pcie_device_perspective.cc` 会同时红，无法区分是本 change 的回归还是既有债 |

## D-5 — 「愿景配置」的两态处置

`configs/apu_soc_full.json:3` 的 `description` 自述「ComputeUnitTLM / CoherentXBarTLM / TCCTLM not yet registered (planned Phase 7.B/C/D)」⇒ 它是**愿景配置**（声明要装但还没装）。

| 选项 | 评价 |
|---|---|
| **A. 移除未实现类型的条目**（本 change 采用） | ① 配置变成**可装配**且**诚实**（不再声明做不到的事）;② ⛔ 不删整份文件（会连带 `test_apu_soc_from_config.cc:114-164` 的 `modules.size()==16` 断言 + `test_cep_contracts_crosscheck.cc:334-351` 的「4 份 config 中 ≥3 存在」两条**无关**断言）;③ 「Phase 7.B/C/D 的承诺」落在**roadmap / ADR**，不落在配置里 |
| B. 保留条目 + 加机器可读的「不可装配」标记 | 保留价值更高，但需要**新造一个标记机制**（目录 / 字段）⇒ 属独立 change（R-3）;⛔ 本 change **不得**引入万能豁免（否则闸会变成「已知红的才绿」） |
| C. 删整份文件 | ⛔ 破两条无关断言 + 丢失 Phase 7 拓扑的意图记录 ⇒ 否决 |

⇒ 采用 **A**；B 的机制需求登记为 R-3。

## D-6 — 存量配置的**三向联动**（改 JSON 会回漂）

| 产物 | 生成方 | 断言方 | 必改 |
|---|---|---|---|
| `configs/apu_soc_full.json` | `examples/generate_apu_soc.py:120-186`（`description` 文本在 `:185-186` 拼出） | `test/test_apu_soc_from_config.cc:114-164`（`modules.size()==16` + description 含 `ComputeUnitTLM`） | 生成器 + 生成物 + **断言** |
| `configs/library_memory_demo.json` | `examples/generate_library_configs.py:41-44` | — | 生成器 + 生成物 |
| `cpptlm/library/standard.py:187`（无条件 emit `"ArbiterTLM"`，`channels` 任取） | 自身 | — | 改为按 `channels` emit `ArbiterTLM2` / `ArbiterTLM4` |

⇒ **AC-05**（重跑生成器 ⇒ `git diff --exit-code configs/` 为空）是防回漂的**唯一机械手段** ⇒ 必须进闸或独立脚本。

## D-7 — 失败形态的**可观测性**判据（禁断言日志）

| 判据 | 断言对象 | ⛔ 禁 |
|---|---|---|
| AC-01 / AC-02 | `instantiateAll` 返回值 + `getAllInstances()` / `getInstance(name)` | 断言日志文本存在 |
| AC-02 的报错内容 | `stderr` 捕获（**不是** `DPRINTF`） | 断言 `DPRINTF` 输出 |
| AC-03 | 失败后再跑合法 config 的返回值 + 实例表 | 断言进程退出码（退出码由 `src/main.cpp:101-104` 提供，**不是装配层的性质**） |

> 动机（metis）：`DPRINTF` 在非 `-DDEBUG_PRINT` 构建下展开为 `do {} while(0)` ⇒ 任何以「日志存在」为判据的 AC **恒绿** ⇒ 等于没验。

## D-8 — 设计类型归档（用户 2026-11-18 裁定）

### D-8.1 判据

**ArchForge 有计划做的设计 IP ⇒ CppTLM 不保留其行为实现**，只保留「影响框架层」的示例；多余示例归档。

两步判定（实测执行）：
1. **ArchForge 有无计划** —— 查 `docs/microarchitecture/<name>.md` 的状态行（✅ 已实施 / 🟡 规划中 / 📋 v1.0 扩展）+ `docs/roadmap/*.md` 的归属声明；
2. **CppTLM 实现有无框架层价值** —— 判据 = **删除后框架会失去哪项能力的验证？** 若无 ⇒ 属多余示例。

### D-8.2 逐类型裁决（含证据）

| 类型 | ArchForge 计划 | 框架层价值实测 | 裁决 |
|---|---|:--:|:--:|
| `GPUTLM` | `gpu-gputlm.md`「✅ 已实施（Phase 7.A v0）」+ §7.1「**Phase 7.B 共享基类（已规划）**」+ R2「`on_config_loaded` 不读 ⇒ Phase 7.B 统一修复」 | 3 个测试；`ComputeReqBundle` 发起侧 **SM 已双向覆盖** | **归档** |
| `SharedMemoryTLM` | `gpu-shared-memory.md`「✅ Phase 8.A Task 1」+ 📋 v1.0 SMEM/LDS | 1 个测试；`bank_conflict_cycles()` 是 stub（`return 1 + (num_threads-1)`，不用 `banks_`） | **归档** |
| `MemoryClusterTLM` | `gpu-memory-cluster.md`「✅ Phase 8.A Task 2」+ v1.0 HBM3e | 2 个测试；`tick()` 只 `++requests_completed_` | **归档** |
| `GpuSocTLM` | 无微架构契约（组合容器）；`modules_cluster.hh:59` 注册 | 1 个测试；`gpu_soc_tlm.hh:26/42/48/56` **持有 `MemoryClusterTLM`** | **归档（连带）** |

**保留（ArchForge 有 A 计划 ≠ CppTLM 可删）**：

| 类型 | 为什么保留 |
|---|---|
| `CPUTLM` | 95 实例，所有 cache/xbar/memory 链路测试的驱动 |
| `StreamingMultiprocessorTLM` | **22 个测试**；ArchForge `phase9-p1-sm-gate-verification.md:12` 明写「SM 重构收尾**全部在 CppTLM 仓内**，不涉及跨仓协调」 |
| `GpuMeshNoC` | `gpu-noc-mesh.md` ✅ Phase 8.A Task 3；mesh 拓扑 + D2D 延迟模型（3 个测试） |
| `CoherentXBarTLM` | `coherent_xbar.md` 🟡 规划中，但 CppTLM 已实现（⚠️ **契约的 Header 路径与实际不符**：契约写 `include/tlm/interconnect/coherent_xbar_tlm.hh`，实际 `include/tlm/coherent_xbar_tlm.hh`）⇒ 跨仓登记，本 change 不改 |
| `PcieControllerIP` / `PcieMemoryDevice` | 无契约；D-AXI 主路径的 4-IP facade / BAR2 外观层，大量测试依赖 |

### D-8.3 完整影响面（实施清单，逐项实测）

**A. `GPUTLM`（header-only，无 `.cc`）**

| 文件 | 动作 |
|---|---|
| `include/tlm/gpu/gpu_tlm.hh` | 删 |
| `include/chstream_register.hh:130` / `:266-267` | 删 `registerObject` + `registerAdapter` |
| `configs/apu_soc_phase7a.json` | **整份删**（只剩 GPUTLM + MemoryTLM） |
| `configs/gpu_standalone.json` | **整份删**（同上） |
| `configs/apu_soc_phase7b.json` | 删 `compute_unit_0` + 其连接 + hierarchy 成员 + coherence domain 成员 |
| `examples/generate_apu_soc.py` | 删 `build_apu_phase7a()` + `main()` 的 7a target；`build_apu_phase7b()` 去 `compute_unit_0`；顶部 docstring 同步 |
| `test/test_gpu_standalone.cc` | 删（整文件） |
| `test/test_apu_soc_from_config.cc` | 删 phase7a TEST_CASE（`:28-54`）；phase7b 改断言（8→7 模块、去 `compute_unit_0`）；bulk 去 phase7a |
| `test/test_sm_streaming_multiprocessor_port.cc` | 改注释 + adapter 注册的 sanity 对照（改用仍存在的类型） |
| `test/python/test_topo_{cli,e2e,layer,patch}.py` | 5 处 `add_module('gpu0','GPUTLM')` 改用仍存在的类型 |
| `test/test_cep_contracts_crosscheck.cc:334-335` | cfgs 列表去 `apu_soc_phase7a.json`（`REQUIRE(existing >= 3)` 仍成立：剩 full / phase7b / hierarchy_tree_3level） |

**B. `SharedMemoryTLM`**

| 文件 | 动作 |
|---|---|
| `include/tlm/gpu/shared_memory_tlm.hh` + `src/tlm/gpu/shared_memory_tlm.cc` | 删 |
| `src/CMakeLists.txt:43` | 删源登记 |
| `include/chstream_register.hh:190` | 删注册 |
| `test/test_shared_memory_tlm.cc` | 删（整文件） |

**C. `MemoryClusterTLM` + `GpuSocTLM`（连带）**

| 文件 | 动作 |
|---|---|
| `include/tlm/gpu/memory_cluster_tlm.hh` + `src/tlm/gpu/memory_cluster_tlm.cc` | 删 |
| `include/tlm/gpu/gpu_soc_tlm.hh` + `src/tlm/gpu/gpu_soc_tlm.cc` | 删（持有 `MemoryClusterTLM`） |
| `src/CMakeLists.txt:44` / `:50` | 删源登记 |
| `include/chstream_register.hh:191` | 删 `MemoryClusterTLM` 注册 |
| `include/modules_cluster.hh:30` / `:59` | 删 include + `REGISTER_MODULE(GpuSocTLM)` |
| `test/test_memory_cluster_tlm.cc` + `test/test_gpu_soc_tlm.cc` | 删（整文件） |

**D. 孤儿模板**

| 文件 | 动作 |
|---|---|
| `configs/templates/gpu_soc/gpu_soc_gb203_v1.json` | **整份删**（实测全仓零引用；删 SM + ClusterTLM 后只剩 2 模块） |

### D-8.4 约束

- ⛔ **不得**在 CppTLM 重新长出同类实现 —— 设计真源在 ArchForge，将来需要时**按 ArchForge 契约重新立项**；
- ⛔ **不得**删 `CPUTLM` / `StreamingMultiprocessorTLM` / `GpuMeshNoC`（理由见 D-8.2）；
- ⚠️ **跨仓动作不在本 change**：ArchForge 侧 `gpu-gputlm.md` / `gpu-shared-memory.md` / `gpu-memory-cluster.md` 的「✅ 已实施」状态需在其仓内改为「⏸ 已归档」⇒ 登记为 **R-5**（跨仓 backlog）。

### D-8.5 与 C1 的关系

归档使 `cpptlm-module-param-effect-landing` 的 B2 批次**从 21 键缩到 13 键**：

| 消失的键 | 数量 |
|---|:--:|
| `GPUTLM.{kernel_id, workgroup_size, start_addr, end_addr}` | 4 |
| `SharedMemoryTLM.{banks, size_kb}` | 2 |
| `MemoryClusterTLM.{capacity_gb, channels}` | 2 |
| **小计** | **8** |

剩余 13 键：`CoherentXBarTLM`(2) · `PcieControllerIP`(3) · `PcieMemoryDevice`(1) · `StreamingMultiprocessorTLM`(2) · `GpuMeshNoC`(2) · `CPUTLM`(3) —— C1 的 design/tasks 须同步缩减。
