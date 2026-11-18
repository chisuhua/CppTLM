# 模块参数基线台账（表 A / 表 B / 基线豁免）

> 归属：change `cpptlm-module-param-read-declares`（决策源 `docs/adr/ADR-X.17-module-param-read-declares.md`）
> 更新：2026-11-18 —— 由 `cpptlm-module-param-effect-landing` 的 **B1 批次**逐键落地后回填（表 A 新增 6 键 / 表 B 逐条改状态 + 新增 B-7~B-10 / 新增 §2bis 未定义类型）
> 日期：2026-11-17
>
> ⚠️ **本文件是人类可读台账，不是任何闸的输入**（change design D-4）。
> 机制判定由 `test/test_param_alignment.cc`（AC-04 运行时对拍）与
> `cpptlm::is_param_admitted()` / `admitted_param_types()`（唯一 C++ 真源）承担。
> 把本文件喂给脚本会立刻造出「第二份准入事实」—— 正是 ADR-X.17 要消灭的形态。

---

## 1. 表 A —— 已消费但**不经** `load_param` 的键

适用条件：某键由**其它路径**消费（板级直读 / 非标量解析 / 字符串枚举），因此不该由
`load_param` 声明，也不该被判为未知键。

| 键 | 所属类型 | 消费方 | 备注 |
|---|---|---|---|
| `start_addr` / `end_addr` | `TrafficGenTLM` | 模块内 `read_addr_key()`（非 `load_param`） | 配置管线 `VarResolver`（`include/utils/var_resolver.hh` 的 `strtod` 分支）会把**数字形态字符串**（含 `0x1000`）转成 JSON 数字 ⇒ 同一键有两种 JSON 类型 ⇒ `load_param` 的单一类型检查不适用；键已 `ignore_param` 显式登记（`traffic_gen_tlm.hh`） |
| `hotspot_addrs` / `hotspot_weights` | `TrafficGenTLM` | 模块内 `parse_hotspot()`（非 `load_param`） | **非标量**（数组）⇒ 不由 `load_param` 承载（`ADR-X.17` D-2）；键已 `ignore_param` 显式登记 |
| `tlb_size` / `tlb_miss_latency_cycles` | `GmmuTLM` | **板级直读** | `dgpu_board_shell.cc:1056-1057` 直接调 `set_tlb_size` / `set_timing_params`;模块内 `ignore_param` 显式登记（`gmmu_tlm.hh` on_config_loaded） |

**未来准入时必须先登记到本表的键**（来自 ADR-X.17 F-15 的板级直读路径
`src/tlm/gpu/dgpu_board_shell.cc`）：

| 键 | 所属类型 | 消费方 | 位置 |
|---|---|---|---|
| `read_latency_hit_cycles` / `read_latency_miss_cycles` / `write_latency_cycles` / `row_hit_cycles` / `row_miss_cycles` / `bandwidth_gbps` / `use_zero_delay_for_test` | `MemoryTLM` / `VramControllerTLM` | 板级直读 | `dgpu_board_shell.cc:1039-1059` |
| `tlb_miss_latency_cycles` / `tlb_size` | `GmmuTLM` | 板级直读 | `dgpu_board_shell.cc:1056-1057`（⚠️ 已在**上表**先行登记 + 模块内 `ignore_param`，准入时无需重复处置） |
| `device_id` / `quantum_cycles` / `ptx_emu_root` | 板级 | 板级直读 | `dgpu_board_shell.cc:53-59` |
| `bar_sizes` / `visible_vram_size` / `invisible_vram_size` / `va_region_size` / `gpu_id` / `gfx_version` / `bdf` | `PcieEndpointIP` 外观 | 板级直读 | `dgpu_board_shell.cc:75-96` |

⚠️ 上述类型**当前均未准入**（`is_param_admitted()` 为 false）⇒ 本表暂不生效。
⚠️ 按 ADR-X.17 D-3 的板级条款：同一类型内 **不得**同时存在「`load_param` 声明的键」与
「板级直读的键」—— 某类型准入时须二选一（改走已声明入口 / 该键登记进本表）。

---

## 2. 表 B —— 真未消费（配置里有键、实现零效果）的豁免

适用条件：键存在于配置里，但**没有任何代码路径读取它**（或读了即丢）。
它们是**已知缺陷**，不是设计意图 ⇒ 逐条登记，修复属各模块的独立 change。

| # | 键 | 所属类型 | 证据 | 状态 |
|:--:|---|---|---|---|
| B-1 | `size` / `level` / `associativity` / `protocol` / `assoc` / `line_size` | `CacheTLM` | 改动前该类无 `on_config_loaded` / 无 `set_config` 覆写；`size`/`level` 由 `cache_cluster.cc:25/28-29` 生成 | ✅ **已落地**（2026-11-18）—— 该类原为 CEP 冻结面（`ADR-CEP-15` IC1）⇒ 经**用户裁定解冻**（`ADR-CEP-15` Status Update 2026-11-18 决策 2 改判 + 台账 Entry #478）⇒ **`size` / `line_size` / `associativity` / `assoc` / `level` 已消费**（容量模型：槽数 = `size / line_size`、组数 = `max(1, 槽数 / associativity)`、组内 LRU 逐出;`level` ⇒ 命中延迟 1→5 / 2→10 / ≥3→20 cycle）;**`protocol` 已显式拒绝**（态②:协议语义不入本类 ⇒ 装配期失败含键名） |
| B-2 | `pattern` / `num_requests` / `start_addr` / `end_addr` | `TrafficGenTLM` | 改动前 `on_config_loaded` 只读 `seed` | **已消费**（`traffic_gen_tlm.hh` 的 `on_config_loaded` + `apply_pattern` / `read_addr_key` / `set_addr_range`;判别测试 `test/test_param_effect_landing.cc` `[effect]`） |
| B-3 | `page_size_bytes` | `GmmuTLM` | 改动前 `contains` 分支体为空,实际恒用 `constexpr 4096` | **已消费**（`gmmu_tlm.hh` 的 `set_page_size_bytes` 替换 `constexpr`;2 的幂且 ≥ 4096 否则装配失败） |
| B-4 | `max_ip_ports` | `LwcBusHub` | 改动前该类零配置读取面,上限恒为编译期 `kMaxIpPorts = 32` | **已消费**（`lwc_bus_hub.hh` 的 `max_ip_ports_`,作用于 `register_cr_target` / `register_doorbell_target` 上限判断） |
| B-5 | `size` / `type` / `read_latency` / `write_latency` / `mem_type` | `MemoryTLM` | 改动前只读 `capacity_gb`；`size`/`type` 由 `memory_cluster.cc:28` 生成 | **`size` / `read_latency` / `write_latency` 已消费**（带单位解析 + hit/miss 同值语义数量化）;**`type` / `mem_type` 已停止产出**（态②：`reject_unsupported_key` ⇒ 配置写了即装配失败;`memory_cluster.cc` 不再生成 `type`） |
| **B-6** | **`routing`** | **`RouterTLM`** | **本 change 的未知键检出机制暴露**：由 `GpuNoC::simulate_instantiate`（`src/tlm/cluster/gpu_noc_cluster.cc:26-31`）为每个内部 router 生成，但 `RouterTLM` 只有 `XYRouting` 一种算法 ⇒ 该键从未被消费（取值恒 `"XY"`） | **本 change 以 `ignore_param("routing")` 显式登记**（`src/tlm/router_tlm.cc` 的 `on_config_loaded`）；把 `routing` 接入算法选择属独立 change |
| B-7 | `domain_protocol` / `snoop_filter` / `ports` | `CoherentXBarTLM` | 该类无配置读取面;`ports` 此前只被 `module_factory_validate.cc` 的 LINT007 读去做**越界告警**（值从不生效） | **`ports` 已显式拒绝**（2026-11-18 态②：端口数是编译期常量 `NUM_PORTS`）;`domain_protocol` / `snoop_filter` **仍待修**（B2 批次后续） |
| B-8 | `dim` / `hops_latency` | `GpuMeshNoC` | 该类无配置读取面（只有 `set_dim` / `set_hops_latency` setter,无生产调用方传配置值） | 待修（B2 批次后续） |
| B-9 | `operating_frequency_hz` | `VramControllerTLM` | 全仓零命中（其余 7 键由板级直读消费,见上表 A 的前瞻段） | **已显式拒绝**（2026-11-18 态②：频率是编译期锚点 `kOperatingFrequencyHz`，与 timing 契约双向绑定;做成运行时可配需「GB/s 带宽输入」这个尚不存在的面） |
| B-10 | `memory_type` | `MemoryCluster` | 读入成员但**从不被读**;原唯一下游是生成 `MemoryTLM.type`（同样零效果）⇒ B1 停止生成后彻底悬空 | **已显式拒绝 + 成员已删**（2026-11-18 态②：无内存类型模型） |
| B-11 | `data_flow` / `vc_counts_enabled` | `SocInternalNoc` | 该类已随 `cpptlm-dgpu-soc-internal-noc-e1` **archived**;实际 NoC 语义由 `GpuInternalFabric` 承担 ⇒ 两键无消费面 | **已显式拒绝**（2026-11-18 态②）;`configs/dgpu_soc_minimal_v1.json` 的 2 键已清理 |
| B-12 | `n_ports` / `ports` | `ArbiterTLM` | 端口数由**模板参数 `N_PORTS`** 决定（编译期）⇒ 两键均无消费面 | **已显式拒绝**（2026-11-18 态②）;`configs/library_memory_demo.json` 的 `n_ports` 已清理 |
| B-13 | `port_count` / `ports` | `CrossbarTLM` | 端口数是编译期常量 `NUM_PORTS`;`ports` 此前只被 LINT007 读去做**告警** | **已显式拒绝**（2026-11-18 态②）;LINT007 告警段已**退役**（否则是死代码）;`configs/apu_soc_v1.json` / `example_emitter_soc.json` 的键已清理 |
| B-14 | `capacity_gb` | `PcieMemoryDevice` | 该类**零配置读取面**（无 `on_config_loaded` / `set_config`）;容量由**注入的 backing** 决定（`backing_view_size_`,per ADR-DGPU-05 单一 VRAM 真源 + D-AXI v1.4 B8）⇒ 配置值**不可能有效果** | **已显式拒绝**（2026-11-18 态②）;`dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` 的该键已清理 |
| B-15 | `bar_bases` / `bar_sizes` / `link_speed_gts` | `PcieControllerIP` | 该类是 **stateless facade**，**有意**从 `PcieEndpointIP::bar_sizes_` 动态读（no static snapshot）⇒ 三键零消费面;⚠️ **同名不同语义**：板级 `dgpu_board_shell.cc:76-79` 读的是 **`pcie_ep` 节点**的 `bar_sizes`（`PcieEndpointIP` **有**消费面） | **已显式拒绝**（2026-11-18 态②）;3 份配置的 `pcie_ctrl.params` 已清空 |
| B-16 | `execution_latency` / `warp_size` | `StreamingMultiprocessorTLM` | `execution_latency` 全仓零概念（延迟由 12 子模块决定）;`warp_size` **不是**该类成员（只在 `GpuTopologyParams` 这一另一结构里） | **已显式拒绝**（2026-11-18 态②）;配置侧零命中（唯一配置模板已随类型归档删除） |
| B-17 | `domain_protocol` / `snoop_filter` | `CoherentXBarTLM` | 无协议状态机（`snoop_broadcast()` 无条件 write-through）;无 filter 概念（ArchForge `snoop_filter.md` 标「🟡 规划中」） | **已显式拒绝**（2026-11-18 态②）;`apu_soc_full.json` + 生成器已清 |
| B-18 | `dim` / `hops_latency` | `GpuMeshNoC` | 两成员**真被**运行期读（`route_latency()` + `d2d_forward_payload()` 的延迟模型）⇒ 不是零效果，只是**无配置读取面** | **已接真实消费**（2026-11-18 态①）;判别测试用既有 `last_transfer_latency_cycles()` |
| B-19 | `pattern` / `start_addr` / `request_interval` | `CPUTLM` | 该类原**无** `on_config_loaded`;`start_addr_` / `request_interval_` 两成员**真被**运行期读（`:87` 窗口边界 · `:90` timer 取模）;`pattern` 无对应概念（只有线性扫描） | `start_addr`/`request_interval` **已接真实消费**（态①）;`pattern` **已显式拒绝**（态③：合法值唯一 `SEQUENTIAL`）—— 2026-11-18 |

### B-6 的处置说明（为什么是显式登记而不是修复）

- **修它**（把 `routing` 接入 `set_routing_algorithm`）会新增能力：目前只有 `XYRouting`，
  「生效」等价于「读值 + 断言 == "XY"」⇒ 只能有一个合法值 ⇒ 不满足 ADR-X.17 D-6 的
  「两个不同合法值 ⇒ 可观测指标不同」⇒ 属于**伪参数**，不该进声明。
- **删它**（`gpu_noc_cluster.cc` 不再透传）会改动 `GpuNoC` 的配置契约，超出本 change 范围。
- ⇒ 采用第三条：`ignore_param("routing")` —— **显式**登记为「已知存在但不消费」，
  使未知键检查不被这条既存缺陷拖红（否则 `GpuNoC` 的内部装配整体失败）。
  ⚠️ 该 API 的调用点可 grep 穷举审计（`grep -rn 'ignore_param(' src/ include/`），
  新增调用必须同步登记本表 —— 否则就是把未知键检查悄悄关掉。

## 2bis. 未注册类型（配置引用了**运行时注册表里没有**的类型）

**不入表 B** —— 表 B 是「键存在但零效果」;这里是「**模块存在但实现里没有该类型**」⇒ 另一族缺陷。

> ⚠️ **2026-11-18 订正（实测推翻原登记）**:本段原写「整份配置**不可装配**」⇒ **与实测不符**。
> 真实失败形态 = **装配成功但该模块被静默丢弃**:`ModuleFactory::instantiateAll`
> (`src/core/module_factory.cc` 约 385 行)遇到未注册类型只打 `DPRINTF` 然后**继续** ⇒
> **拓扑缺模块** ⇒ 仿真结果**静默失真**。且 `DPRINTF` **仅在 Debug 构建生效**
> (`include/core/sim_core.hh:8-16` + `CMakeLists.txt:71-72`)⇒ **Release 构建下连日志都没有**。

| 配置里的 type | 实际状态 | 出现处 |
|---|---|---|
| `TmuDispatchProcessorTLM` | 类**存在**（`include/tlm/gpu/tmu_dispatch_processor_tlm.hh` 的 `tlm::gpu::TmuDispatchProcessor`）但**从未注册** ⇒ 且配置里的名字与类名**不一致** | `configs/dgpu_board_v1.json`（**嵌套**在 `soc` 内 ⇒ 顶层 fail-fast 覆盖不到） |
| `ArbiterTLM` | 只注册了 `ArbiterTLM2` / `ArbiterTLM4`;**模板基类 `ArbiterTLM` 本身未注册** | `configs/library_memory_demo.json` |
| `ComputeUnitTLM` | 类**不存在**（历史 `GpuComputeUnitTLM` 已并入 `StreamingMultiprocessorTLM` —— `include/tlm/gpu/streaming_multiprocessor_tlm.hh:14` 注明 rename 合并） | `configs/apu_soc_full.json`（4 实例） |
| `TCCTLM` | 类**不存在**（全仓 `include/` + `src/` 零命中） | `configs/apu_soc_full.json`（2 实例） |

> **实测方法**:运行时枚举 `ModuleFactory::getRegisteredTypes()`（67 个注册类型）∩
> `configs/**/*.json`（排除 `configs/param_rules/**`）的 `type` 集合 ⇒ 差集 = 上表 **4 个类型 / 3 份配置**。
> ⛔ **不能用「类名是否存在于 include/」判定** —— `TmuDispatchProcessorTLM`（有类、名字不同）与
> `ArbiterTLM`（模板基类）都会被漏判 ⇒ 判据必须**从运行时注册表派生**。
> ⚠️ 原登记「`configs/apu_soc_phase7a.json` 也含 `ComputeUnitTLM`」与实测**不符**
> （该文件仅在 `description` 文本里提到该名,`modules[]` 里**零命中**）⇒ 已订正。
>
> 处置（机制 fail-fast + 4 处对齐）**不属本 change** ⇒ 已登记为 `cpptlm-module-param-effect-landing` R-5
> ⇒ 独立 change `cpptlm-config-type-registration-align` 承接。

---

## 3. 基线豁免的拓扑配置（AC-04）

`test/test_param_alignment.cc` 遍历 `configs/**/*.json`，对含**已准入类型**实例的配置
逐个 `instantiateAll`。以下配置在**本 change 之前**就无法装配 ⇒ 登记为基线豁免。

| 配置 | 豁免原因 | 证据 |
|---|---|---|
| `configs/noc_mesh.json` | 其 4 个 RouterTLM 实例**既无 `params` 子对象、也无扁平 `node_x`/`node_y`** ⇒ 改动前被 CFG-08 的 `else if (type == "RouterTLM" \|\| "NICTLM")` 拦下，改动后被 `load_param` 的必填检查拦下 ⇒ **行为等价**（fail → fail） | 全仓 `grep -rl 'noc_mesh.json' test/ scripts/ examples/` **零引用**；该配置自身缺 RouterTLM 的必填参数 |

⚠️ **不得为了「让闸变绿」往豁免表里加新项**。新增豁免必须同时满足：
① 提供改动前失败的证据；② 在本表写明原因；③ 该失败与本 change 的改动面无关。
