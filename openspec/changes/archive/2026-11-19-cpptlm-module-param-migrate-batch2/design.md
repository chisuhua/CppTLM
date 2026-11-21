# design: cpptlm-module-param-migrate-batch2

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（D-3 / D-3b / D-6 / F-4 / F-15 / F-16）;
> 另参 `docs/adr/ADR-X.10-parameter-framework.md`（§A 决策实施状态 / §C 现役声明面 / §F 未裁决子议题）。
> 本文件只记录**实施层面的可逆选择**;不可逆决策以 ADR 为准。

## D-1 — 迁移判据（准入三态）

一个类型 T **可迁入准入表** ⇔ `ADR-X.17` D-3 的「T 迁移完成」判据三条**同时**成立：

| 条 | 内容 | 判定方式 |
|:--:|---|---|
| (a) | T 的全部参数均由 `load_param` 声明，或已按 D-2 登记为「已消费但不经 `load_param`」（非标量键 / 板级直读键 / 已知零效果键） | 逐键核对：`load_param` 调用点 ∪ `ignore_param` 调用点 ⊇ T 的配置键并集 |
| (b) | `getParamRulesRegistry()` 中无 T 的条目 | 迁完即为真（ParamRules 机制整体退役 ⇒ 该条恒真） |
| (c) | CFG-08 的 T 分支已删除（若 T 曾被 CFG-08 覆盖） | `validateConfig` 函数体非注释行零命中 T |

⚠️ (a) 的前提是**该类型的键都有确定效果** ⇒ 「零效果键未裁决」的类型**不可迁**（见 D-5）。

## D-2 — 迁移清单（20 个类型 · 2026-11-18 实测读取面）

> 清单来源 = `include/**` 中 `on_config_loaded() override` / `void set_config(const json&` 两类覆写点的**静态检索** + `configs/**` 的键并集。
> ⚠️ 实施期每类型的首个动作 = **核对该类型的实际读取集**（清单是检索结果，不等于逐键确认）。

### D-2.1 ChStream 家族（`on_config_loaded` 覆写 · 10 个）

| # | 类型 | 读取集 / 配置键（并集） | 特殊处置 |
|:--:|---|---|---|
| 1 | `NICTLM` | `node_id` / `mesh_x` / `mesh_y` | ⚠️ **CFG-08 分支在此退役**（`node_id` 必填的唯一强制点）⇒ 必须与 `load_param("node_id")` 必填声明**同 commit** 完成 |
| 2 | `MemoryTLM` | 读 `capacity_gb`;配置另有 `mem_type` / `read_latency` / `size` / `type` / `write_latency` | ⚠️ 后 5 键**零效果** ⇒ 归 `cpptlm-module-param-effect-landing`;⚠️ `read_latency_hit_cycles` 等属**板级直读**（D-6） |
| 3 | `VramControllerTLM` | `bandwidth_gbps` / `operating_frequency_hz` / `read_latency_hit_cycles` / `read_latency_miss_cycles` / `row_hit_cycles` / `row_miss_cycles` / `use_zero_delay_for_test` / `write_latency_cycles` | ⚠️ **全部为板级直读键** ⇒ 态②（登记为「已消费但不经 `load_param`」）;⚠️ 它继承 `MemoryTLM` ⇒ 其 `on_config_loaded` 调用链需核实（`include/tlm/memory_tlm.hh:162` 的基类实现是否也执行） |
| 4 | `GmmuTLM` | 读 `page_size_bytes`（**读而丢弃**）/ `tlb_size` / `tlb_miss_latency_cycles` | `page_size_bytes` 归 effect-landing;`tlb_*` 板级直读 ⇒ 态② |
| 5 | `TrafficGenTLM` | 读 `seed`;配置另有 7 个零效果键 | 7 键归 effect-landing;`seed` 是**唯一**可声明键 |
| 6 | `SdmaEngineTLM` | `max_inflight` / `translate_latency` / `vram_size_bytes` | — |
| 7 | `PcieEndpointIP` | `bar0_registers` / `bar_sizes` / `config_size` / `num_msix_vectors` / `resizable_bars` | ⚠️ **二次 `on_config_loaded`**（`src/tlm/pcie/pcie_endpoint_ip.cc:168/172` ⇒ `ADR-X.17` F-11）⇒ 迁移后的加载逻辑**必须幂等**;⚠️ `bar_sizes` 另有板级消费方（D-6） |
| 8 | `CohIfNICTLM` | 读 `node_id` / `mesh_x` / `mesh_y`;配置另有 `address_regions`（**对象数组** ⇒ 非标量） | `address_regions` ⇒ 态②（登记）;非标量键不由 `load_param` 承载（`ADR-X.17` D-2） |
| 9 | `CohIfResponderTLM` | 读 `emit_completions` / `completion_latency_cycles` / `emit_snoops` / `snoop_interval_cycles` | — |
| 10 | `PcieEndpointTLM` | `on_config_loaded` 存在 | ⛔ **显式排除** —— 该类已 `[[deprecated]]` 且属 **23 ABI 冻结面**（`ADR-DGPU-08` §2.3 / `ADR-088` §D5：`include/tlm/gpu/pcie_endpoint_tlm.h` **零修改**）⇒ 不迁、不改;其配置键由既有行为承载 |

### D-2.2 SimModule 家族（`set_config` 覆写读 `params` · 10 个）

⚠️ 这族在 **`set_config` 覆写内**读参数（`ADR-X.17` F-1 ②）⇒ 迁移形态与 ChStream 族不同：覆写内**必须**调用 `SimObject::set_config`（基类负责 `param_ctx_` 的 begin/enforce）⇒ **调用顺序**（先基类后自读，还是先自读后基类）是本族的关键实现细节（design D-2.3）。

| # | 类型 | 配置键（并集） | 备注 |
|:--:|---|---|---|
| 11 | `ApuSoC` | `cpu_topology` / `gpu_topology` | **嵌套块** ⇒ 非标量 ⇒ 态②（登记） |
| 12 | `CacheCluster` | `l1_count` / `l1_size` / `l2_size` | `l1_size` / `l2_size` 是**字符串**（如 `"32KB"`）⇒ 需 `load_param<std::string>` + 解析（⚠️ `load_param` 的 string 分支不做单位解析 ⇒ 解析逻辑归属模块内） |
| 13 | `ComputeCluster` | `cu_count` / `cu_template` | `cu_template` 是**对象**（模板配置）⇒ 非标量 ⇒ 态② |
| 14 | `CpuCluster` | `cluster_id` / `num_cpus` | — |
| 15 | `GpcCluster` | `cu_per_tpc` / `cu_template` / `gpc_id` / `tpc_per_gpc` | `cu_template` 非标量 ⇒ 态② |
| 16 | `GpuCluster` | `cu_per_tpc` / `cu_template` / `gpc_count` / `tpc_per_gpc` | 同上 |
| 17 | `GpuNoC` | `mesh_size` / `routing` | ⚠️ `routing` 归 effect-landing（态②/③ 尚未裁决 ⇒ **本类型暂缓迁移**） |
| 18 | `MemoryCluster` | `channel_count` / `channel_size` / `memory_type` | `channel_size` 字符串（`"1GB"`）⇒ 同 #12 |
| 19 | `TpcCluster` | `cu_per_tpc` / `cu_template` / `tpc_id` | `cu_template` 非标量 ⇒ 态② |
| 20 | `CepMeshInterconnect` | `template` | **嵌套块** ⇒ 非标量 ⇒ 态② |

> ⚠️ **排除项**：`PciePhyDigitalCtrlTLM` 的 `set_config(const PciePhyConfig&)` 是**结构体**签名（非 JSON）⇒ **不是**参数读取面 ⇒ 不在清单内。

### D-2.3 SimModule 家族的 `set_config` 覆写形态（关键实现细节）

`SimModule` 家族的覆写形如 `void set_config(const json& params) { SimModule::set_config(params); if (params.contains("num_cpus")) … }`。

迁移后必须保证：**基类的 `param_ctx_.begin()` / `enforce_param_contract()` 与模块自读之间不产生「键未登记 ⇒ 误判未知键」**。⇒ 实施形态：

- 模块自读的键**改为 `load_param`**（登记进 `param_ctx_`）⇒ 不再需要 `params.contains` 手工分支;
- 非标量键（态②）用 `ignore_param(key)` 登记;
- ⚠️ **`SimObject::set_config` 必须在模块自读之前或之后被调用，二者不可各调一次** ⇒ 实施时定为「**先基类（`begin` + `enforce`），后模块 `load_param`**」还是「**模块 `load_param` 先，基类 `enforce` 后**」——后者需要基类提供「延后 enforce」的钩子。
  ⇒ **本 change 的裁决**：采用**前者**（基类 `set_config` 先跑 ⇒ `enforce` 会在模块 `load_param` 之前执行 ⇒ 会误判！）⇒ **不可行**。
  ⇒ **改为**：基类 `set_config` 提供两个入口 —— `set_config`（`begin` → `on_config_loaded` → `enforce`）与供 SimModule 家族使用的「`begin` → 覆写自读 → `enforce`」路径。⇒ **需要改基类的调用序列**（`include/core/sim_object.hh`）⇒ 该改动是本 change 的**基类改造项**（design D-3 之外单列）。

## D-3 — ParamRules 机制退役清单（C++ 侧）

| 符号 / 文件 | 位置 | 处置 |
|---|---|---|
| `ParamRule` / `ParamRules` / `ParamType` | `include/core/param_rules.hh` | **删文件** |
| `ModuleFactory::registerParamRules` / `getParamRulesRegistry` | `include/core/module_factory.hh:87/94` | **删** |
| 工厂侧默认值注入 + `required` 检查循环 | `src/core/module_factory.cc:263-291` | **删**（默认值已由 `load_param` 落成员，不写回 JSON —— `ADR-X.17` D-5） |
| `validate_module_params`（`static`，零调用方） | `src/core/module_factory_validate.cc:466-499` | **删** |
| `ParamParser`（`src/core/param_parser.cc`） | 其 `validate`（min/max）唯一调用点在死函数内 ⇒ `validate` 随死函数删;`parse` 的其他调用方**须逐一核实** | 部分删 / 部分留（实施期核实，⛔ 不得连带删掉活代码） |
| `test/test_d3_param_resolver_registry.cc` | 现有 3 个用例全部测的是**注册表机制本身** | 随机制退役 ⇒ **删文件**（⚠️ 其上一 change 新增的「`RouterTLM` 已迁移」正向契约断言须**迁移**到新文件，不得丢失） |
| `configs/param_rules/*.json` | `configs/param_rules/{router_tlm,nic_tlm}.json` | ⛔ **不删** —— `ADR-X.10` §F 未裁决（且它是 Python 生成期校验的活数据源）⇒ **本 change 不动** |

## D-4 — 嵌套失败传播设计

现状：`SimModule::simulate_instantiate` 调 `internal_factory->instantiateAll(config)` 后**丢弃返回值**（`include/core/sim_module.hh:95-98`）⇒ 内部失败只打日志。

⇒ 目标形态：**内部装配失败 ⇒ 外层 `instantiateAll` 返回 false**。

实现要点：

1. `simulate_instantiate` 返回 `bool`（或新增 `bool instantiate_ok_` 成员）—— ⚠️ **签名变更影响面**：调用点 = `src/core/module_factory.cc` 的 Step 4.5 + `src/tlm/cluster/gpu_noc_cluster.cc` 等容器自己的 `simulate_instantiate` 覆写 ⇒ **逐个调用点核实**;
2. `ModuleFactory::instantiateAll` 的 Step 4.5 在内部失败时 `return false`;
3. ⚠️ **既有行为变化**：此前「内层失败但外层 true」的场景会变成 false ⇒ 既有测试若依赖旧语义须逐条评估（proposal W-3）;
4. ⚠️ **幂等守卫交互**：`simulate_instantiate` 的幂等守卫（已 populate 则直接 return）⇒ 失败路径下 `internal_factory` 可能**部分填充** ⇒ 重试语义须明确（失败后是否可再次调用）⇒ **裁决：失败路径不做回滚**（保持现状的「部分填充」语义），但**不得**因幂等守卫而把「失败」误判为「已 populate」⇒ 守卫判定改为「已 populate **且**上次成功」。

## D-5 — 依赖顺序（与 `cpptlm-module-param-effect-landing`）

| 阶段 | 内容 | 依赖 |
|---|---|---|
| **P0** | 嵌套失败传播（D-4）+ 基类调用序列改造（D-2.3） | 无 |
| **P1a** | 迁移**已有读取面且键已确定效果**的类型（D-2 的 20 个中，剔除 `GpuNoC`（`routing` 未裁决）、`MemoryTLM` / `GmmuTLM` / `TrafficGenTLM` 的零效果键部分） | 无（可与 effect-landing 并行） |
| **P1b** | 迁移 `GpuNoC` + `MemoryTLM` / `GmmuTLM` / `TrafficGenTLM`（零效果键裁决后） | **依赖 effect-landing 的 B1/B3** |
| **P2** | ParamRules 机制退役（D-3） | 依赖 P1a+P1b（否则注册表仍有条目） |
| **P3** | spec delta + 归档 | — |

⇒ **并行安全的部分**：P0 与 P1a。⇒ 两个 change 可同时推进，但 **P2 的退役动作必须等 effect-landing 完成**。

## D-6 — 板级直读键的三态处置（`ADR-X.17` D-3）

| 类型 | 板级直读键（`dgpu_board_shell.cc`） | 处置 |
|---|---|---|
| `MemoryTLM` | `read_latency_hit_cycles` / `read_latency_miss_cycles` / `write_latency_cycles` / `row_hit_cycles` / `row_miss_cycles` / `bandwidth_gbps` / `use_zero_delay_for_test` | **态②**：登记为「已消费但不经 `load_param`」⇒ `ignore_param` + 基线台账表 A;⛔ 不得同时由 `load_param` 声明同名键 |
| `VramControllerTLM` | 同上（timing-mode 分支） | 态② |
| `GmmuTLM` | `tlb_miss_latency_cycles` / `tlb_size` | 态② |
| `PcieEndpointIP` | `bar_sizes` / `visible_vram_size` / `invisible_vram_size` / `va_region_size` / `gpu_id` / `gfx_version` / `bdf` | 态②;⚠️ `bar_sizes` 同时出现在 `PcieEndpointIP` 自身的 `params` ⇒ **同名键两侧都有** ⇒ 须按 D-3「同一类型内不得同时存在」裁决为单一消费方（建议：板级保留，模块侧登记为已消费） |
| 板级自身 | `device_id` / `quantum_cycles` / `ptx_emu_root` | 不属模块类型 ⇒ 不在本 change |

⚠️ `ignore_param` 的调用点 SHALL 可 grep 穷举审计，且与 `docs/implementation/module-param-baseline.md` **表 A** 一一对应（承接 `cpptlm-module-param-read-declares` 的 AC-14）。
