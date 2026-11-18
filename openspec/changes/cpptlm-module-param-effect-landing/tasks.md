# tasks: cpptlm-module-param-effect-landing

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`;实施形态见本 change 的 `design.md`（D-1~D-5）。
> 共 **T0~T7**;标 ⚠️ 的条目为审查/盘点新增。
>
> **进度（2026-11-18）**：**T0 + T1 全部 + T4/T5 的 B1 部分 + T6 已完成**（B1 批次 = `TrafficGenTLM` / `MemoryTLM` / `GmmuTLM` / `LwcBusHub` / `CacheTLM`）。
> **B2（T2）/ B3（T3）未开始**;`T1.1`（`CacheTLM`）原阻塞于 R-4（CEP 冻结面）⇒ **2026-11-18 已解冻并落地**。
> 逐键裁决结果与实测口径见 `design.md` **D-1.6**;台账同步见 `docs/implementation/module-param-baseline.md`（表 A/B + §2bis）。

## T0 — 前置与盘点

- [x] T0.1 ⚠️ 写**盘点脚本**（可重跑）：遍历 `configs/**/*.json`（排除 `configs/param_rules/**`）⇒ 输出「类型 × 参数键 × 实例数 × 是否有读取面（`on_config_loaded` / `set_config`）」;判据 = 脚本输出的类型数 / 键×类型对数与 `design.md` D-1 一致
  - 落盘 `scripts/test/param_effect_inventory.py`（`--json` / `--check`）;实测 36 类型 / 107 键×类型对（口径与 D-1 零效果族的差异见 D-1.6.3）
- [x] T0.2 ⚠️ **逐键裁决表**回填 `design.md` D-1：每个键一个态（①/②/③）+ 证据 + 态① 的观测量选型;⛔ 不得留「待定」
  - **B1 部分已回填**（D-1.6.4）;`CacheTLM` 6 键记「待裁决（R-4）」;B2/B3 的键在对应批次落地时回填

## T1 — B1 批次（`ADR-X.17` F-14 五类 + 扩展键）

- [x] T1.1 `CacheTLM` 六键（`size` / `level` / `associativity` / `protocol` / `assoc` / `line_size`）裁决 + 落地
  - ✅ **2026-11-18 解冻并落地**（R-4 已解除）—— 该类原为 CEP 冻结面（`ADR-CEP-15` IC1）⇒ 经**用户裁定解冻**（`ADR-CEP-15` Status Update 2026-11-18 决策 2 改判 + 台账 Entry #478）⇒ `size` / `line_size` / `associativity` / `assoc` / `level` 走**态 ①**（容量模型：槽数 = `size / line_size`,组数 = `max(1, 槽数 / associativity)`,组内 LRU 逐出;`level` ⇒ 命中延迟）;`protocol` 走**态 ②**（`reject_unsupported_key` ⇒ 装配期失败含键名）
  - 连带处置:守门闸 IC1 载体迁移（0 diff 断言 → `protocol` 拒绝的**行为**判据）· `ADR-CEP-07` SU 指针 · `openspec/specs/cep-core-contracts/spec.md` IC1 场景改判 · `docs/architecture/cep/{fabric-transport,parity-baseline}.md` 口径限定 · 对拍常量抽取器 `scripts/cep/parity/cpptlm_params.py` 形态同步
  - 存量配置:`configs/apu_soc_full.json` 删 `protocol` ×3（态②）
- [x] T1.2 `TrafficGenTLM` 七键（`pattern` / `num_requests` / `start_addr` / `end_addr` / `hotspot_addrs` / `hotspot_weights` / `stride`）裁决 + 落地（`seed` 已消费，不动）
- [x] T1.3 `GmmuTLM.page_size_bytes`（读而丢弃）裁决 + 落地
- [x] T1.4 `LwcBusHub.max_ip_ports` 裁决 + 落地
- [x] T1.5 `MemoryTLM` 五键（`size` / `type` / `mem_type` / `read_latency` / `write_latency`）裁决 + 落地;⚠️ `size` 与 `capacity_gb` 语义重叠 ⇒ 评估态②
- [x] T1.6 B1 回归：`[router]` / `[memory]` / `[gmmu]` / `[trafficgen]` / `[lwc]` 全绿
  - 实测：`cpptlm_tests` 全量 **883307 assertions / 2386 cases** 全绿（基线 883191 / 2371 ⇒ +116 / +15 全部来自新判别测试）;`pytest` 232 passed;`ctest -R cep` 100/100

## T2 — B2 批次（零配置读取面家族）

> ⚠️ **清单口径修正（2026-11-18）** —— 立项期的「14 类」是**人工读配置**得出的，实测盘点脚本
> （`scripts/test/param_effect_inventory.py`）复核后**增 4 类 / 删 2 类**（差异逐条见 `design.md` D-1.6.3）:
> **删**的 2 类是 `ComputeUnitTLM` / `TCCTLM` —— **仓内无类定义**（配置引用了不存在的类型 ⇒ 整份配置不可装配）⇒ **不是零效果键**，转 **R-5**（独立 change）;**增**的 4 类是盘点漏检（`CoherentXBarTLM` / `GpuMeshNoC` / `VramControllerTLM` / `MemoryCluster`）⇒ 已补入 T2.13~T2.16。

- [x] T2.1 `CPUTLM` 三键（`pattern` / `request_interval` / `start_addr`）裁决 + 落地（**95 个配置实例** ⇒ 影响面最大） ⇒ **用户拍板 Q-4 ②**（2026-11-18）：`pattern` ⇒ **态③**（合法值唯一 `SEQUENTIAL` —— 本类只有线性扫描，无多模式流量生成;⚠️ 别与 `TrafficGenTLM.pattern` 混，那个有 4 种）;`start_addr` / `request_interval` ⇒ **态①**（两成员**真被**运行期读）。⚠️ 两处关键：① 态③ 落地形态 = **给 CPUTLM 新增读取面**（原无 `on_config_loaded`）+ 校验 + 非法值失败，⛔ 不顺手做模式实现;② `cur_addr_` 是构造期由 `start_addr_` 初始化的 ⇒ 配置改了 `start_addr` 后**必须同步游标**（否则首个请求仍从旧默认地址发 = 配置被静默忽略）—— 已实现。判别测试 3 条（态③ 非法值 / start_addr 改地址 / interval 改请求数）;全量 **883275 / 2378** ⇒ **未打破任何既有测试**（原以为会打破的 3 个 C++ 用例实测零地址断言）。
- [x] T2.2 `CrossbarTLM.port_count` / `ports` 裁决 + 落地（**态②**：端口数是编译期常量 `NUM_PORTS` ⇒ 显式拒绝;LINT007 告警段已退役;`apu_soc_v1.json` / `example_emitter_soc.json` 已清理）
- [ ] T2.3 ~~`ComputeUnitTLM`（`kernel_id` / `workgroup_size`）裁决 + 落地~~ ⇒ **转 R-5**（该类型在 `include/` + `src/` **零类定义**;`configs/apu_soc_full.json:3` 自述「not yet registered」⇒ 处置是「实现类型 / 从配置移除」，**不是**「让键有效果」）
- [x] T2.4 `GPUTLM` 四键（`kernel_id` / `workgroup_size` / `start_addr` / `end_addr`）裁决 + 落地 ⇒ **失效闭环**：该类型已于 2026-11-18 随**设计类型归档**移除（`.hh` 物理删除 + 3 份配置删除，per change `cpptlm-config-type-registration-align` T7.1 · 台账 Entry #481）⇒ 本 task 的对象已不存在，无需裁决。**该 4 键不再出现在盘点中**。
- [x] T2.5 `PcieControllerIP` 三键（`bar_bases` / `bar_sizes` / `link_speed_gts`）裁决 + 落地;⚠️ `bar_sizes` 另有**板级消费方**（`dgpu_board_shell.cc:78-79`）⇒ 按 `ADR-X.17` D-3 三态条款分开处置 ⇒ **态② 已落地**（2026-11-18）：该类是 **stateless facade**，**有意**「Reads sizes dynamically from `PcieEndpointIP::bar_sizes_`（no static snapshot）」⇒ 三个键零消费面 ⇒ `on_config_loaded` 显式拒绝;⚠️ **同名不同语义**：板级读的是 **`pcie_ep` 节点**的 `bar_sizes`（`PcieEndpointIP` 有消费面）⇒ 不得按键名一刀切（per §3 Q-7 的裁决 ①）;3 份配置（`dgpu_soc_minimal_v1` / `dgpu_soc_timing_v1` / `dgpu_soc_v2_realip_p2`）的 `pcie_ctrl.params` 已清空。
- [x] T2.6 `PcieMemoryDevice.capacity_gb` 裁决 + 落地（**态②**：容量由**注入的 backing** 决定（`backing_view_size_`;ADR-DGPU-05 单一 VRAM 真源 + D-AXI v1.4 B8）⇒ 该类**零配置读取面** ⇒ 配置值不可能有效果 ⇒ 显式拒绝;`dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` 已清理;判别测试 = `test_param_effect_landing.cc` 的「AC-04: PcieMemoryDevice.capacity_gb」）
- [x] T2.7 `StreamingMultiprocessorTLM` 两键（`execution_latency` / `warp_size`）裁决 + 落地 ⇒ **态② 已落地**（2026-11-18）：`execution_latency` 全仓零概念（延迟由 12 子模块模型决定）;`warp_size` **不是**该类的成员（只存在于 `GpuTopologyParams` 这一**另一个结构**）⇒ `on_config_loaded` 显式拒绝;配置侧零命中（其唯一配置 `templates/gpu_soc/gpu_soc_gb203_v1.json` 已于类型归档时删除）。
- [x] T2.8 `ArbiterTLM.n_ports` / `ports` 裁决 + 落地（**态②**：端口数由模板参数 `N_PORTS` 决定;`library_memory_demo.json` 已清理;`memory_cluster.cc` 不再生成 `ports`）
- [x] T2.9 `MemoryClusterTLM` 两键（`capacity_gb` / `channels`）裁决 + 落地（⚠️ 与 `MemoryCluster` 是两个类） ⇒ **失效闭环**：同 T2.4（类型已归档，T7.3）
- [x] T2.10 `SharedMemoryTLM` 两键（`banks` / `size_kb`）裁决 + 落地 ⇒ **失效闭环**：同 T2.4（类型已归档，T7.2）
- [x] T2.11 `SocInternalNoc` 两键（`data_flow` / `vc_counts_enabled`）裁决 + 落地（**态②**：该类已 archived,NoC 语义由 `GpuInternalFabric` 承担;`dgpu_soc_minimal_v1.json` 已清理）
- [ ] T2.12 ~~`TCCTLM` 两键（`coalescing` / `size`）裁决 + 落地~~ ⇒ **转 R-5**（同 T2.3：仓内零类定义）
- [x] T2.13 `CoherentXBarTLM` 三键（`domain_protocol` / `snoop_filter` / `ports`）—— **`ports` 已落地**（态②，继承 `CrossbarTLM` 的拒绝）;`domain_protocol` / `snoop_filter` **待后续批次** ⇒ **三键全部落地**（2026-11-18）：`ports`（继承 `CrossbarTLM`）+ `domain_protocol`（无协议状态机 —— `snoop_broadcast()` 是无条件 write-through 透传，per ADR-CEP-15 的协议语义边界冻结）+ `snoop_filter`（无 filter 概念;ArchForge 契约 `snoop_filter.md` 标「🟡 规划中」）⇒ 全部态② 显式拒绝。⚠️ 覆写**必须调基类**（否则继承的 `ports` 拒绝失效）—— 已加判别测试。`configs/apu_soc_full.json` + 生成器 `generate_apu_soc.py` 的 2 键已清。
- [x] T2.14 `GpuMeshNoC` 两键（`dim` / `hops_latency`）裁决 + 落地 ⇒ **态①**（2026-11-18）：两成员**真被**运行期读（`route_latency()` 与 `d2d_forward_payload()` 的延迟模型都用 `hops_latency_`;`get_dim()` / 最坏路径循环用 `dim_`）⇒ `on_config_loaded` 用 `load_param_opt` 接真实消费。判别测试用**既有**观测量 `last_transfer_latency_cycles()`（⛔ 不造新观测面，per D-2.1）：`hops_latency` 1 vs 4 ⇒ 延迟不同。
- [x] T2.15 `VramControllerTLM.operating_frequency_hz` 裁决 + 落地（**态②**：频率是编译期锚点 `kOperatingFrequencyHz`，与 timing 契约双向绑定;运行时可配需「GB/s 带宽输入」这个尚不存在的面 ⇒ 独立 feature;`dgpu_soc_minimal_v1.json` 已清理）
- [x] T2.16 `MemoryCluster.memory_type` 裁决 + 落地（**态②**：无内存类型模型 ⇒ 显式拒绝 + 成员已删;2 个测试数据里的该键已清理）
- [ ] T2.17 B2 回归：全量套件绿 + `configs/` 存量写法逐处登记（态② 键的命中清单）

## T3 — B3 批次（`RouterTLM.routing`）

- [ ] T3.1 `routing` 裁决 + 落地（态②「生成方停止产出」或态③「显式拒绝」）;⛔ **不得**停留在 `ignore_param` 登记（`ADR-X.17` D-3b）
- [ ] T3.2 `GpuNoC::simulate_instantiate` 的内部配置生成与 T3.1 的处置同步（`src/tlm/cluster/gpu_noc_cluster.cc:26-31`）
- [ ] T3.3 `[simmodule][infra]` 的 GpuNoC 用例绿（`test/test_simmodule_infra_clusters.cc:57/80`）

## T4 — 测试（逐键判别）

- [ ] T4.1（**B1 部分已完成**：`test/test_param_effect_landing.cc` 15 cases / 116 assertions，覆盖 `TrafficGenTLM` 8 键 / `MemoryTLM` 4 键 / `GmmuTLM` 1 键 / `LwcBusHub` 1 键）态① 的每个键一条**行为判别测试**（两个不同合法值 ⇒ 可观测指标不同 —— `ADR-X.17` D-6）;⛔ 不得为通过判别而新增「只为测试存在」的观测面
- [ ] T4.2（**B1 部分已完成**：`MemoryTLM` 的 `type` / `mem_type` + `size`↔`capacity_gb` 互斥）态② 的每个键一条**装配失败**测试（配置含该键 ⇒ `instantiateAll` 返回 false 且错误含键名）
- [ ] T4.3 态③ 的每个键一条**非法值拒绝**测试（错误含键名 + 合法值集合）
- [ ] T4.4（**B1 部分已完成**：`configs/apu_soc_full.json` 删 `MemoryTLM.type: "HBM2"` 1 处;`configs/library_memory_demo.json` 删 `mem_type` 4 处;`src/tlm/cluster/memory_cluster.cc` 停止生成 `size`→保留 / `type`→删除）⚠️ **存量配置清单**：每个态② 键列出 `grep -rn '"<key>"' configs/` 的全部命中，逐处判定（改键名 / 删键 / 保留并接受失败 ⇒ 后者须显式登记）

## T5 — 台账

- [ ] T5.1（**B1 部分已完成**：B-2/B-3/B-4 改「已消费」;B-5 拆分「已消费 + 已停止产出」;**B-1 改「已落地」（CacheTLM 解冻，见 T1.1）**;新增 B-7~B-10 + §2bis + 表 A 6 键）`docs/implementation/module-param-baseline.md` **表 B 逐条更新**（`待修` ⇒ `已消费` / `已停止产出` / `已显式拒绝`），逐条附实现位置
- [x] T5.2 ⚠️ AC-07：`grep -rn 'ignore_param(' src/ include/` 的非注释调用点与表 B 条目**一一对应**（承接 `cpptlm-module-param-read-declares` 的 AC-14）

## T6 — 验证

- [x] T6.1 `cpptlm_tests` 全量绿（assertions / cases ≥ 基线 **883191 / 2371**）
- [x] T6.2 `python3 -m pytest test/python/` 全绿（基线 232 passed）
- [x] T6.3 `ctest -R cep` 全绿（基线 100/100）
- [x] T6.4 `openspec validate cpptlm-module-param-effect-landing --strict` + `cep_docs_sync_check` + `cep_doc_ref_check` + `agents_md_lint` 全绿
- [x] T6.5 ⚠️ AC-06：`git diff <base> -- configs/` 的每处变更都对应一个态② 任务（⛔ 不得顺手删配置）

## T7 — 收口

- [ ] T7.1 台账 Entry（实施记录 + 实测 + 改动面 + 遗留）
- [ ] T7.2 KPI 回填（活口径 6 处）
- [ ] T7.3 spec delta 合并（`openspec archive`，⛔ 不用 `--skip-specs`）+ 新 main spec 的 Purpose 手写
- [ ] T7.4 独立验收（切 verifier;AC-01~AC-09 + G-1/G-2 逐条取证）
- [ ] T7.5 ⚠️ **残留清点**：本 change 范围内不得留 TODO / 半截实现 / 临时日志 / 被 skip 的测试 / 未替换的 mock
