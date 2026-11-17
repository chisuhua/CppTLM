# 模块参数基线台账（表 A / 表 B / 基线豁免）

> 归属：change `cpptlm-module-param-read-declares`（决策源 `docs/adr/ADR-X.17-module-param-read-declares.md`）
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
| （当前为空） | — | — | 本 change 只迁移 `RouterTLM`，其全部参数均经 `load_param` 声明，无表 A 键 |

**未来准入时必须先登记到本表的键**（来自 ADR-X.17 F-15 的板级直读路径
`src/tlm/gpu/dgpu_board_shell.cc`）：

| 键 | 所属类型 | 消费方 | 位置 |
|---|---|---|---|
| `read_latency_hit_cycles` / `read_latency_miss_cycles` / `write_latency_cycles` / `row_hit_cycles` / `row_miss_cycles` / `bandwidth_gbps` / `use_zero_delay_for_test` | `MemoryTLM` / `VramControllerTLM` | 板级直读 | `dgpu_board_shell.cc:1039-1059` |
| `tlb_miss_latency_cycles` / `tlb_size` | `GmmuTLM` | 板级直读 | `dgpu_board_shell.cc:1056-1057` |
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
| B-1 | `size` / `level` / `associativity` / `protocol` | `CacheTLM` | 该类无 `on_config_loaded` / 无 `set_config` 覆写；`size`/`level` 由 `cache_cluster.cc:25/28-29` 生成 | 待修 |
| B-2 | `pattern` / `num_requests` / `start_addr` / `end_addr` | `TrafficGenTLM` | `traffic_gen_tlm.hh:102-110` 注释自述「其余 params 未被解析」；`set_num_requests` / `set_addr_range` 无生产调用方 | 待修 |
| B-3 | `page_size_bytes` | `GmmuTLM` | `gmmu_tlm.hh:307-309` 的 `contains` 分支体为空；实际翻译用 `constexpr page_size = 4096` | 待修 |
| B-4 | `max_ip_ports` | `LwcBusHub` | 全仓仅 `configs/dgpu_soc_minimal_v1.json:69` 命中；该类零配置读取面 | 待修 |
| B-5 | `size` / `type` / `read_latency` / `write_latency` | `MemoryTLM` | `memory_tlm.hh:162-168` 只读 `capacity_gb`；`size`/`type` 由 `memory_cluster.cc:28` 生成 | 待修 |
| **B-6** | **`routing`** | **`RouterTLM`** | **本 change 的未知键检出机制暴露**：由 `GpuNoC::simulate_instantiate`（`src/tlm/cluster/gpu_noc_cluster.cc:26-31`）为每个内部 router 生成，但 `RouterTLM` 只有 `XYRouting` 一种算法 ⇒ 该键从未被消费（取值恒 `"XY"`） | **本 change 以 `ignore_param("routing")` 显式登记**（`src/tlm/router_tlm.cc` 的 `on_config_loaded`）；把 `routing` 接入算法选择属独立 change |

### B-6 的处置说明（为什么是显式登记而不是修复）

- **修它**（把 `routing` 接入 `set_routing_algorithm`）会新增能力：目前只有 `XYRouting`，
  「生效」等价于「读值 + 断言 == "XY"」⇒ 只能有一个合法值 ⇒ 不满足 ADR-X.17 D-6 的
  「两个不同合法值 ⇒ 可观测指标不同」⇒ 属于**伪参数**，不该进声明。
- **删它**（`gpu_noc_cluster.cc` 不再透传）会改动 `GpuNoC` 的配置契约，超出本 change 范围。
- ⇒ 采用第三条：`ignore_param("routing")` —— **显式**登记为「已知存在但不消费」，
  使未知键检查不被这条既存缺陷拖红（否则 `GpuNoC` 的内部装配整体失败）。
  ⚠️ 该 API 的调用点可 grep 穷举审计（`grep -rn 'ignore_param(' src/ include/`），
  新增调用必须同步登记本表 —— 否则就是把未知键检查悄悄关掉。

---

## 3. 基线豁免的拓扑配置（AC-04）

`test/test_param_alignment.cc` 遍历 `configs/**/*.json`，对含**已准入类型**实例的配置
逐个 `instantiateAll`。以下配置在**本 change 之前**就无法装配 ⇒ 登记为基线豁免。

| 配置 | 豁免原因 | 证据 |
|---|---|---|
| `configs/noc_mesh.json` | 其 4 个 RouterTLM 实例**既无 `params` 子对象、也无扁平 `node_x`/`node_y`** ⇒ 改动前被 CFG-08 的 `else if (type == "RouterTLM" \|\| "NICTLM")` 拦下，改动后被 `load_param` 的必填检查拦下 ⇒ **行为等价**（fail → fail） | 全仓 `grep -rl 'noc_mesh.json' test/ scripts/ examples/` **零引用**；该配置自身缺 RouterTLM 的必填参数 |

⚠️ **不得为了「让闸变绿」往豁免表里加新项**。新增豁免必须同时满足：
① 提供改动前失败的证据；② 在本表写明原因；③ 该失败与本 change 的改动面无关。
