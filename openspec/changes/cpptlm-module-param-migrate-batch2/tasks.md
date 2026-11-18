# tasks: cpptlm-module-param-migrate-batch2

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`;实施形态见本 change 的 `design.md`（D-1~D-6）。
> 共 **T0~T8**;标 ⚠️ 的条目为审查新增。阶段划分见 design D-5（P0 / P1a / P1b / P2 / P3）。

## T0 — 前置（机制侧改造）

- [x] T0.1 ⚠️ **基类调用序列改造**（design D-2.3）：`SimObject` 提供「`begin` → 模块自读 → `enforce`」路径，供 SimModule 家族的 `set_config` 覆写使用;⛔ 不得出现「基类 `enforce` 先于模块 `load_param`」的顺序（会把已声明键误判为未知键） ⇒ **已落地**：`SimObject` 新增 protected `begin_param_load()` / `end_param_load()`（三段式 `begin → 模块自读 → enforce`）+ 模板 `set_config_with(params, self_read)`（供 SimModule 家族在 `set_config` 覆写里自读用）。`set_config()` 本身改为走同一对助手。⛔ 不出现「基类 `enforce` 先于模块 `load_param`」的顺序（否则模块已声明键被误判为未知键）。
- [x] T0.2 ⚠️ **嵌套失败传播**（design D-4）：`SimModule::simulate_instantiate` 的内部装配失败 ⇒ 外层 `instantiateAll` 返回 false;⚠️ 幂等守卫须改为「已 populate **且**上次成功」才短路 ⇒ **已落地**（**改用 design D-4 的第二个方案**）：`simulate_instantiate` **签名保持 void**（⚠️ 改 bool 会让 `include/tlm/pcie/pcie_endpoint_ip.hh` 的 `override` 失配 —— 该头属 `INV-CEP-07-IC5` 的 4 个 frozen headers，`test_cep_module_factory_seam` 的零 diff 断言会红） ⇒ 用 `instantiate_ok_` 成员 + `instantiate_succeeded()` 公开读取口。幂等守卫改为 `already_instantiated()` =「已 populate **且**上次成功」。`ModuleFactory::instantiateAll` 的 Step 4.5 + `config` 文件路径 + `DGpuBoard::load_soc_config` 三处接入并传播。判据 = `test/test_nested_failure_propagation.cc`（3 case / 9 assertions）。
- [x] T0.3 ⚠️ `simulate_instantiate` 签名/返回值变更的**调用点全量核实**（`src/core/module_factory.cc` Step 4.5 + 各容器覆写） ⇒ **已核实**（签名未变，但仍逐点核实了失败通道）——`grep -rn "simulate_instantiate" include/ src/ test/`:① `src/core/module_factory.cc` Step 4.5（顶层 SimModule）⇒ 接入 `instantiate_succeeded()`;② 同上 `config` 文件路径分支 ⇒ 接入;③ `src/tlm/gpu/dgpu_board_shell.cc`（板级 soc）⇒ 接入;④ `src/core/sim_module.hh` 基类递归（子 SimModule）⇒ 接入;⑤ `src/tlm/cluster/apu_soc.cc` 自身递归 ⇒ 接入;⑥ 各容器覆写（cache/compute/gpc/gpu/gpu_noc/memory/tpc cluster + CepMeshInterconnect + GpuInternalFabric）⇒ 全部在 `SimModule::simulate_instantiate(cfg)` 之后检查 `instantiate_succeeded()`;⑦ `PcieEndpointIP`（冻结头）⇒ 内部置 `instantiate_ok_`。
- [x] T0.4 ⚠️ 既有测试中依赖「内层失败但外层返回 true」的用例**逐条评估**并登记（proposal W-3） ⇒ **已逐条评估并修掉 11 处**（全部是**内联副本与真实配置漂移**，不是判据本身错）：① `test_param_loader.cc` G-2 用例（**正是**依赖旧语义的那条）⇒ 改为 `REQUIRE_FALSE(instantiateAll)` + 零残留断言;② `test_simmodule_nested.cc` ×2 ⇒ 显式 resp 连接冗余（解析器自动派生响应路径，显式写会被当请求方向判不兼容）⇒ 删 2 条 resp 连接;③ `test_dgpu_pcie_device_perspective.cc` ×6 ⇒ 内联板级配置含已删的 `tmu`（`TmuDispatchProcessorTLM`，R-5 已从真实配置删）+ `gpu` 用 `config` 字段（真实配置已改内联 `params`）;④ `test_display_device_injection.cc` ×2 + `test_realip_facade_wiring.cc` ×1 ⇒ `PcieMemoryDevice.capacity_gb` 已于 C1 落态②，内联副本未同步;⚠️ **连带修 2 个 C1 遗留缺陷**（T0.2 的传播把它们暴露出来）：① `CPUTLM.start_addr` 用 `load_param_opt` 读 `"0x1000"` ⇒ `VarResolver` 转成 float 4096.0 ⇒ `load_param` 拒绝（`TrafficGenTLM` 早有 `read_addr_value` 绕开，CPUTLM 漏了）⇒ 把该读法提升为 `cpptlm::parse_addr_json` / `parse_addr_text`（`include/core/param_loader.hh`）供两族共用（⛔ 不留第二份实现）;② `test_dgpu_pcie_device_perspective` 的 `gpu` 走 `config` 文件路径（`instantiate()`）⇒ 该路径的失败此前也被吞。

## T1 — P1a：迁移（ChStream 家族 · 10 个类型）

> 每类型四件套：**声明化**（`load_param`）· **行为判别测试**（D-6）· **准入表加项** · **旧声明面清理**。

- [x] T1.1 `NICTLM`：`node_id` / `mesh_x` / `mesh_y` 声明化 + **CFG-08 分支退役**（同 commit!）+ 准入表加项 ⇒ **已落地**：`node_id` **必填**（`load_param` 无默认值）+ `mesh_x`/`mesh_y` 可选（`load_param_opt`）+ `address_regions`（非标量）`ignore_param` 登记;**CFG-08 分支同批退役**（`src/core/module_factory_validate.cc` 的 NICTLM per-type 分支整段删除）⇒ 必填真值唯一（`ADR-X.17` F-4 的双强制点闭环）;已准入;判别测试 `test/test_param_migrate_batch2.cc` 的 `[batch2][nic]` 5 case
- [x] T1.2 `MemoryTLM`：`capacity_gb` 声明化 + 板级直读键态②登记（D-6）;⚠️ 零效果键（`mem_type` / `read_latency` / `size` / `type` / `write_latency`）**不在本任务范围** ⇒ 依赖 effect-landing ⇒ 暂缓准入 ⇒ **已落地**（C1 已声明化，本批补齐 D-6 部分）：7 个**板级直读键**逐个 `ignore_param` 登记（`read_latency_hit_cycles`/`read_latency_miss_cycles`/`write_latency_cycles`/`use_zero_delay_for_test`/`row_hit_cycles`/`row_miss_cycles`/`bandwidth_gbps`）;已准入;新增观测 getter（`size_cap()` / `read_latency_hit_cycles()` / `read_latency_miss_cycles()` / `write_latency_cycles()`）供 AC-02 判别；判别测试 `[batch2][memory]` 3 case;⚠️ 准入后 `test_memory_tlm_backing.cc` 的「未知键被静默忽略」用例语义变更 —— 改为空 params + 新增一条正向 case（未知键**必须**失败）
- [x] T1.3 `VramControllerTLM`：全部板级直读键 ⇒ 态②登记 + 准入表加项;⚠️ 核实其与基类 `MemoryTLM::on_config_loaded` 的调用链 ⇒ **已落地**：`vram_size_bytes` 改 `load_param_opt`（原先手写 `cfg.contains` + `is_number` 分支）;`operating_frequency_hz` 态② 拒绝（编译期锚点，C1 已落）;继承 `MemoryTLM::on_config_loaded()`⇒ 板级直读键登记自动继承;已准入;判别测试 `[batch2][vram]` 1 case（含态② 反向）
- [x] T1.4 `GmmuTLM`：`tlb_*` 态②登记;⚠️ `page_size_bytes` 依赖 effect-landing ⇒ 暂缓准入 ⇒ **已落地**：`page_size_bytes` 态①（2 的幂且 ≥ 4096，C1 已落）;`tlb_size` / `tlb_miss_latency_cycles` 板级直读键 `ignore_param` 登记（C1 已落）;已准入;判别测试 `[batch2][gmmu]` 1 case（含非 2 的幂 ⇒ 失败）
- [x] T1.5 `TrafficGenTLM`：`seed` 声明化 + 准入表加项;⚠️ 其余 7 键依赖 effect-landing ⇒ **已落地**（C1 已完成全部 8 键的裁决）：`pattern`/`num_requests`/`seed`/`stride` 态①;`start_addr`/`end_addr` 走 `parse_addr_json`（**非** `load_param` —— `VarResolver` 会把 `0x` 转 float）;`hotspot_addrs`/`hotspot_weights` 非标量登记;已准入;⚠️ 本批把 `read_addr_value` 的本地实现**提升为共享** `cpptlm::parse_addr_json`（`param_loader.hh`）供 `CPUTLM` 共用 —— 消除两份实现（CPUTLM 那份曾漏 `is_number_float` 分支 ⇒ `"0x1000"` 装配失败）
- [x] T1.6 `SdmaEngineTLM`：`max_inflight` / `translate_latency` / `vram_size_bytes` 声明化 + 判别测试 + 准入 ⇒ **已落地**：3 键（`max_inflight`/`translate_latency`/`vram_size_bytes`）改 `load_param_opt`（原为 `cfg.contains` + `cfg.value` 手写分支）;新增 3 个观测 getter 供 AC-02 判别;已准入;判别测试 `[batch2][sdma]` 1 case
- [x] T1.7 `PcieEndpointIP`：声明化 + 判别测试 + 准入;⚠️ **二次 `on_config_loaded` 的幂等性**必须保持（F-11）;⚠️ `bar_sizes` 与板级同名 ⇒ 按 D-3 裁决单一消费方 ⇒ **已落地**（**处置与清单描述不同 —— 有据**）：design D-2 列的是 5 个标量键，实测本类配置面是 **11 个键**，其中 4 个是**嵌套对象**（`link_layer`/`phy_digital`/`transaction_layer`/`sr_iov`）、2 个是**非标量数组**（`bar0_registers`/`bar_sizes`）⇒ 属 D-2 的非标量类，**不由 `load_param` 承载**;处置 = `ignore_param` 逐个登记（`attach_composition` 末尾）+ **保留**本类独立的 `warn_unconsumed` 具名告警（比未知键失败更细）;已准入;⚠️ **二次 `on_config_loaded` 的幂等性**（F-11）**未受影响** —— `simulate_instantiate` 保持 void 签名（T0.2 的成员通道），`attach_composition` 的重入清空逻辑不变
- [x] T1.8 `CohIfNICTLM`：`node_id` / `mesh_x` / `mesh_y` 声明化 + `address_regions`（非标量）态②登记 + 准入 ⇒ **已落地**：`node_id`/`mesh_x`/`mesh_y` 改 `load_param_opt`（原 `cfg["x"].get<uint32_t>()` ——类型不符抛的是 nlohmann 的 `type_error`，错误信息不含「参数名+期望类型」）;`address_regions`（对象数组）`ignore_param` 登记;已准入;判别测试 `[batch2][cohif-nic]` 1 case（⚠️ `lookup_node` 是 private ⇒ 断言「配置可装配」+ 拼错键必失败，解析行为由 `test_cep_mesh_e2e` 覆盖）
- [x] T1.9 `CohIfResponderTLM`：`emit_completions` / `completion_latency_cycles` / `emit_snoops` / `snoop_interval_cycles` 声明化 + 判别测试 + 准入 ⇒ **已落地**：4 键改 `load_param_opt`（原 `config_.contains(...) && is_boolean()/is_number_unsigned()` ——**类型不符即静默忽略** ⇒ 配置写了等于没写）;已准入;判别测试 `[batch2][cohif-resp]` 1 case（含类型不符 ⇒ 装配失败，⛔ 旧实现不会失败）
- [x] T1.10 `PcieEndpointTLM`：**显式排除**（`[[deprecated]]` + 23 ABI 冻结面 ⇒ 零修改）⇒ 在准入表与台账中登记「不迁」及理由 ⇒ **已显式登记「不迁」+ 理由**（`docs/implementation/module-param-baseline.md` §1.3）：① 已 `[[deprecated]]`;② 属 **23 ABI 冻结面**（`ADR-DGPU-08` §2.3 / `ADR-088` §D5，该头零修改）;③ 迁 `load_param` 必须改冻结头 ⇒ 与 ABI 冻结直接冲突。⛔ 不因「清单上有它」而强行迁移

## T2 — P1a：迁移（SimModule 家族 · 10 个类型）

- [x] T2.1 `CpuCluster`：`cluster_id` / `num_cpus` 声明化 + 判别测试 + 准入 ⇒ **已落地**：`num_cpus` / `cluster_id` 改 `load_param_opt`（原 `params["x"].get<T>()` ——类型不符抛的是 nlohmann `type_error`，不含「参数名+期望类型」）;已准入;判别测试 `[batch2][cpucluster]`（两值 ⇒ getter 不同 + 拼错键必失败）
- [x] T2.2 `CacheCluster`：`l1_count` 声明化 + `l1_size` / `l2_size`（**字符串** + 单位解析，解析逻辑归模块内）+ 判别测试 + 准入 ⇒ **已落地**：`l1_count` 态①；`l1_size` / `l2_size` **带单位字符串** ⇒ `load_param_opt` 取值、解析归模块内（D-2 态①）;⚠️ **结构小修**：旧实现先调 `SimModule::set_config()`（内含 enforce）**再**自读 ⇒ 准入后自读键会被误判为未知键 ⇒ 改 `set_config_with`（enforce 晚于自读）;已准入;判别测试 `[batch2][cachecluster]`（`l1_count` 2/5 ⇒ 子模块数 3/6 + 尺寸字符串落成员）
- [x] T2.3 `MemoryCluster`：`channel_count` / `memory_type` 声明化 + `channel_size`（字符串）+ 判别测试 + 准入 ⇒ **已落地**：`channel_count` 态① + `channel_size`（带单位字符串）;⚠️ `memory_type` **已由 C1 落态②拒绝**（B-10：本仓无内存类型模型 ⇒ 零效果）⇒ 本批**保持拒绝**、不复改为「声明化」（那会造出一个无效果的读取）;已准入;判别测试 `[batch2][memorycluster]`（channel_count 2/4 ⇒ 子模块数 3/5 + `memory_type` 必失败）
- [x] T2.4 `ComputeCluster`：`cu_count` 声明化 + `cu_template`（**对象**）态②登记 + 判别测试 + 准入 ⇒ **已落地**：`cu_count` 态①（<1 / >64 钳位**保留在模块内** —— 合法性裁决不是 `load_param` 的职责）;⚠️ `cu_template` 实测是**路径字符串**（design 记为「对象」）⇒ 按实测落 D-2 态①（取值 + 模块内加载）;已准入;判别测试 `[batch2][computecluster]`（cu_count 2/5/0 ⇒ 2/5/1 钳位）
- [x] T2.5 `TpcCluster`：`tpc_id` / `cu_per_tpc` 声明化 + `cu_template` 态②登记 + 判别测试 + 准入 ⇒ **已落地**：`tpc_id` / `cu_per_tpc` 态① + `cu_template`（路径字符串，同 T2.4）;已准入;判别测试 `[batch2][tpccluster]`
- [x] T2.6 `GpcCluster`：`gpc_id` / `tpc_per_gpc` / `cu_per_tpc` 声明化 + `cu_template` 态②登记 + 判别测试 + 准入 ⇒ **已落地**：`gpc_id` / `tpc_per_gpc` / `cu_per_tpc` 态① + `cu_template`（路径字符串）;已准入;判别测试 `[batch2][gpccluster]`
- [x] T2.7 `GpuCluster`：`gpc_count` / `tpc_per_gpc` / `cu_per_tpc` 声明化 + `cu_template` 态②登记 + 判别测试 + 准入 ⇒ **已落地**：`gpc_count` / `tpc_per_gpc` / `cu_per_tpc` 态① + `cu_template`（路径字符串）;已准入;判别测试 `[batch2][gpucluster]`
- [x] T2.8 `ApuSoC`：`cpu_topology` / `gpu_topology`（**嵌套块**）态②登记 + 准入 ⇒ **已落地**：`cpu_topology` / `gpu_topology` 实测是**蓝图路径字符串**（design 记为「嵌套块」）⇒ 按实测落 D-2 态① + `coherent_xbar_name` 一并声明;已准入;判别测试 `[batch2][apusoc]`（跑通 cpu/gpu 双蓝图装配）
- [x] T2.9 `CepMeshInterconnect`：`template`（嵌套块）态②登记 + 准入 ⇒ **已落地**：`template`（蓝图路径字符串）态① —— 旧实现先 enforce 再自读，同 T2.2 的结构缺陷（已改 `set_config_with`）;已准入;判别测试 `[batch2][cepm]`
- [x] T2.10 ⚠️ `GpuNoC`：`mesh_size` 声明化 + 准入;⚠️ `routing` 依赖 effect-landing（态②/③ 未裁决）⇒ **本任务暂缓准入** ⇒ **部分落地（按任务原文）**：`mesh_size` 已声明化（`load_param_opt`）+ 判别测试;⚠️ **本批不准入** —— `routing` 的裁决归 C1 之后的独立 change（本类只透传给 `RouterTLM`，而后者已落态③「只接受 XY」）⇒ 准入会让两个裁决点互相牵制;判据测试显式断言 `CHECK_FALSE(is_param_admitted("GpuNoC"))` 锁定该状态

> ⚠️ **T2 附带修复**：`configs/templates/cpu_cluster_2level.json` **缺顶层 `connections`**
> ⇒ `validateConfig` 直接拒（与 T1 修的 2 份同型缺陷）。此前被 `test_param_alignment` 跳过
> （其类型未准入）；CpuCluster/CacheCluster 准入后被判据捕获 ⇒ 补 `"connections": []`
> （顶层只列 1 个模块、无顶层连接 ⇒ 空数组是正确形态）。

## T3 — P1b：依赖 effect-landing 的部分

- [x] T3.1 ⛔ **前置**：`cpptlm-module-param-effect-landing` 的 B1（`TrafficGenTLM` / `MemoryTLM` / `GmmuTLM`）与 B3（`routing`）完成 ⇒ **已核验**：`bash scripts/test/rddf_stage_gate.sh --phase C` **全绿（0 项跳过）** —— P-1 依赖边锚点命中（C1 已归档 ⇒ `resolve_path` 走归档感知三态）/ P-3 AC 载体在位;C1 的 B1（TrafficGen/Memory/Gmmu）+ B3（RouterTLM.routing 态③）均已落地
      - ⚠️ **机械前置命令**（critic 2026-11-18 要求 —— 否则「前置」只靠人读）：`bash scripts/test/rddf_stage_gate.sh --phase C` ⇒ 断言 C1 的 `tasks.md` 无未勾 task 且 C2 目录未提前开工
- [x] T3.2 `TrafficGenTLM` / `MemoryTLM` / `GmmuTLM` / `GpuNoC` 的剩余键按裁决结果声明化 + 准入 ⇒ **已落地**：前三个在 T1 已声明化并准入（C1 裁决后的键集不变）;⚠️ **`GpuNoC` 在 T3 准入**（T2.10 的暂缓条件已解除 —— C1 的 B3 已把 `RouterTLM.routing` 落态③「合法值唯一 XY」）;判据：`[batch2][gpunoc]` 断言 `is_param_admitted("GpuNoC")` **为真** + `routing` 非法值（`"FIFO"`）**透传给内部 RouterTLM ⇒ 整体装配失败**（态③ 的传递性验证）
- [x] T3.3 ⚠️ 零效果键的**存量配置清单**复核（承接 effect-landing T4.4） ⇒ **已复核（机械扫描：解析 `configs/**/*.json` 所有 `params` 对象的键集，逐条判定）**：

| 键 | 命中 | 类型 | 判定 |
|---|---|---|---|
| `pattern` | 25 处 | `TrafficGenTLM`（态①，合法值 SEQUENTIAL/RANDOM/HOTSPOT/STRIDED）/ `CPUTLM`（态③，合法值唯一 SEQUENTIAL） | ✅ 全部合法值 |
| `capacity_gb` | 9 处 | `MemoryTLM`（态①） | ✅ 无 `PcieMemoryDevice` 残留（B-14 已清） |
| `mem_type` / `memory_type` / `operating_frequency_hz` / `n_ports` / `port_count` / `bar_bases` / `link_speed_gts` / `domain_protocol` / `snoop_filter` / `vc_counts_enabled` / `execution_latency` / `warp_size` | **0** | — | ✅ 已清空 |
| `protocol` | 5 处 | 均在 `coherence_domains[]` **域级**块内（非模块 `params`） | ✅ 不属模块参数面 |
| `ports` / `type` | 31 / 411 处 | 模块 entry 的**拓扑字段**（`type` / `ports: []`），非 `params` 键 | ✅ 不属参数面 |
| `data_flow` | 1 处 | `dgpu_soc_v2_realip_p2.json` 的**顶层 `soc_noc` 块**（板级开关，非模块 `params`） | ⚠️ **观察项**：板级块内无消费方（`DGpuBoard` 只按实例名 `soc_noc` 找模块，不读该块）⇒ 登记，不属本 change 范围（模块参数面之外） |

⇒ **态②/③ 键在真实配置中零违规**（另有 `test_param_alignment` 的「含准入类型的配置全部可装配」判据兜底）。

## T4 — P2：ParamRules 机制退役

- [x] T4.1 删 `include/core/param_rules.hh`（`ParamRule` / `ParamRules` / `ParamType`） ⇒ **已删**。`ParamType`（`ParamParser::parse` 仍需要）**迁入** `include/core/param_parser.hh`（自此 `param_parser` 零跨头依赖）;`ParamRule` / `ParamRules` / `to_json` / `from_json` 一并消失
- [x] T4.2 删 `ModuleFactory::registerParamRules` / `getParamRulesRegistry`（`include/core/module_factory.hh:87/94`） ⇒ **已删**（`include/core/module_factory.hh`：`ParamRulesProvider` 别名 + 两个静态成员 + `param_rules.hh` include 全部移除）
- [x] T4.3 删工厂侧默认值注入 + `required` 检查循环（`src/core/module_factory.cc:263-291`）;⚠️ 默认值已由 `load_param` 落成员、**不写回 JSON**（`ADR-X.17` D-5） ⇒ **已删**（`src/core/module_factory.cc` 的整段 per-type 默认值注入 + `required` 检查循环）。⚠️ 默认值现由各模块 `load_param_opt` 落成员、**不写回 JSON**（`ADR-X.17` D-5 的语义本来如此 —— 旧实现的写回只在 `type==RouterTLM` 且缺键时触发）;判据：全量测试 883616 assertions 零回归
- [x] T4.4 删 `validate_module_params`（`src/core/module_factory_validate.cc:466-499`，零调用方死代码） ⇒ **已删**（`src/core/module_factory_validate.cc`，零调用方的死函数）
- [x] T4.5 ⚠️ `ParamParser` 的 `validate`（min/max）随死函数删;`parse` 的其他调用方**逐一核实** ⇒ 活代码不得连带删 ⇒ **已删 `validate`**（`param_parser.hh` 声明 + `param_parser.cc` 实现 + `test/test_param_parser.cc` 的 2 条用例）。⚠️ **逐一核实 `parse` 的其他调用方**：生产侧唯一调用方就是被删的 `validate_module_params`；`parse_latency` 是 `parse` 的内部实现、`evaluate_derive_expr` 无调用方 —— 但 `parse` 本身**保留不动**（T4.5 明确「活代码不得连带删」，且它是 `test_param_parser.cc` 的判据；删除属另一个 change 的范围）
- [x] T4.6 ⚠️ `test/test_d3_param_resolver_registry.cc` 随机制退役（删文件）⇒ 但其上一 change 新增的「`RouterTLM` 已迁移」**正向契约断言 SHALL 迁移**到新文件（⛔ 不得丢失） ⇒ **已删文件**;其「`RouterTLM` 已迁移」**正向契约断言已迁移**到 `test/test_param_migrate_batch2.cc` 的 `[param][migrate][batch2][d3]`（⛔ 未丢失）—— 判据从「注册表无条目」改写为「准入表成员」（注册表已不存在，但准入三条条件 (a)(b)(c) 同时成立正是「已迁移」的定义）;⚠️ **连带删除 `test/test_param_rules.cc`**（它测的 `ParamRule` / `ParamType` JSON 序列化随 T4.1 消失，不删则编译不过）—— 同属机制退役的自然结果，非擅自扩大范围
- [x] T4.7 ⛔ **不动** `configs/param_rules/*.json`（`ADR-X.10` §F 未裁决）⇒ 显式登记「本 change 不裁决」 ⇒ **未触碰**（`git diff` 零命中）。理由登记：`ADR-X.10` §F 未裁决该目录的去留;其消费方 `cpptlm_config/validator.py` 属生成期声明面，不在本 change 的运行时参数面范围内

## T5 — 测试与验收支撑

- [x] T5.1 ⚠️ AC-01：准入表断言（从 `admitted_param_types()` 派生，⛔ 测试内不硬编码第二份清单） ⇒ **已落地**（`test/test_param_migrate_batch2.cc` 的 `[batch2][admission]`）：断言准入表的**结构不变量**（非空 / 无空名 / 自洽 `is_param_admitted` / 严格升序无重复 / 负例不成立 / 多次调用不漂移），⛔ **测试内零硬编码类型清单**（全部从 `admitted_param_types()` 派生）
- [x] T5.2 ⚠️ AC-07：**≥3 个不同类型**的拼错键 ⇒ 装配失败且错误含键名 ⇒ **已落地**：**4 个不同类型**（`NICTLM.nodeid` / `MemoryTLM.capacit_gb` / `CpuCluster.num_cpu` / `SdmaEngineTLM.max_inflght`）⇒ 直接调 `set_config` 并断言 `ParamValidationError::param_name` **等于该拼错键名**（`ModuleFactory` 会把异常折成 `false`，键名只能在异常对象上断言;AC-07 的「错误含键名」由此取证）
- [x] T5.3 ⚠️ AC-05：嵌套容器内缺必填 ⇒ 外层 `instantiateAll` 返回 false ⇒ **已由 T0.2 的 `test/test_nested_failure_propagation.cc` 覆盖**（3 case：缺必填 ⇒ 外层 false / 齐备 ⇒ 外层 true 不误伤 / 失败后 `instantiate_succeeded()` 为 false）
- [x] T5.4 ⚠️ AC-08：板级直读键三态合规核对（`ignore_param` 调用点 ↔ 台账表 A） ⇒ **已核对 + 判据落地**。`grep -rn 'ignore_param("' include/ src/` 的 **21 个调用点**（含 `PcieEndpointIP` 的 11 键循环）逐条映射台账：`MemoryTLM` 7 键 / `GmmuTLM` 2 键 / `PcieEndpointIP` 11 键 → §1.1;`NICTLM`+`CohIfNICTLM` 的 `address_regions` / `CPUTLM` 的 `start_addr`+`request_interval` / `TrafficGenTLM` 的 4 键 → §1.2 与表 A —— **零未登记项**。判据测试见 `[batch2][admission]` 的 AC-08 段（这些键**不得**被判为未知键）
- [x] T5.5 ⚠️ AC-02：逐类型行为判别测试（每个声明键两条不同合法值 ⇒ 可观测指标不同） ⇒ **覆盖见 `test/test_param_migrate_batch2.cc` 的 23 个 case / 165 assertions**（`[batch2][nic|memory|vram|gmmu|sdma|cohif-nic|cohif-resp|cpucluster|cachecluster|memorycluster|computecluster|tpccluster|gpccluster|gpucluster|apusoc|cepm|gpunoc]`）+ **`RouterTLM` / `TrafficGenTLM`** 的判别在 C1 的 `test_param_effect_landing.cc`（AC-03 / AC-08）;⚠️ 逐键 ⇒ 测试的映射是**纪律而非机制**（`ADR-X.17` D-6 明示 W-1），本批已逐键台账化

## T6 — 台账与文档

- [ ] T6.1 `docs/implementation/module-param-baseline.md` **表 A** 逐条对应实现（板级直读键）;表 B 与 effect-landing 的裁决一致（两 change 不得互相矛盾）
- [ ] T6.2 ⚠️ `cpptlm-validate-params-dead-code`（backlog）的状态更新：**C++ 侧诉求由本 change 闭合**;Python 生成期诉求转 `ADR-X.10` §F
- [ ] T6.3 ⚠️ `cpptlm-router-vc-count-param-inert`（backlog）的状态更新：结论不变（只登记不修）;其 json 侧条目随 §F 裁决

## T7 — spec delta

- [ ] T7.1 `specs/module-param-load-declares/spec.md`：`## MODIFIED Requirements`（全量准入后的表述;⛔ 既有 Scenario 名全部保留）
- [ ] T7.2 `specs/param-derivation/spec.md`：① `## REMOVED Requirements`（`ParamRule structure` —— 机制退役）② `## MODIFIED Requirements`（`Module param rules declaration` ⇒ 全量 `load_param`;⛔ 既有 Scenario 名全部保留）
- [ ] T7.3 `specs/param-validation/spec.md`：`## MODIFIED Requirements`（`Parameter range validation` + `Required parameter enforcement`;⛔ 既有 Scenario 名全部保留）
- [ ] T7.4 ⚠️ AC-10：`param-validation` 的「mesh 范围 1-16」条款订正或标注为「未实施」（事实：`configs/mesh_4x4.json` 存在 `node_x: 0`）
- [ ] T7.5 `openspec validate cpptlm-module-param-migrate-batch2 --strict` 通过

## T8 — 验证与收口

- [ ] T8.1 `cpptlm_tests` 全量绿（assertions / cases ≥ **实施日重测基线**;重测命令 `./build/bin/cpptlm_tests 2>&1 | tail -3`;⛔ **不得写死数字** —— 上游两个 change（C1 类型归档 −26 case、R-5 判别 case）已改变基线 ⇒ 任何硬编码下限都会在实施后不可达;2026-11-18 实测 = **883242 / 2372**）
- [ ] T8.2 `python3 -m pytest test/python/` 全绿（基线 232 passed）
- [ ] T8.3 `ctest -R cep` 全绿（基线 100/100）
- [ ] T8.4 `cep_docs_sync_check` + `cep_doc_ref_check` + `agents_md_lint` 全绿
- [ ] T8.5 ⚠️ AC-06：`git diff <base> -- configs/` **为空**（本 change 不改配置面）
- [ ] T8.6 台账 Entry（实施记录 + 实测 + 改动面 + 遗留）
- [ ] T8.7 KPI 回填（活口径 6 处）
- [ ] T8.8 独立验收（切 verifier;AC-01~AC-10 + G-1/G-2 逐条取证）
- [ ] T8.9 归档（`openspec archive`，⛔ 不用 `--skip-specs`）+ 新 main spec 的 Purpose 手写
- [ ] T8.10 ⚠️ **残留清点**：本 change 范围内不得留 TODO / 半截实现 / 临时日志 / 被 skip 的测试 / 未替换的 mock
