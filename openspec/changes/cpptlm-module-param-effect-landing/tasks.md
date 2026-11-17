# tasks: cpptlm-module-param-effect-landing

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`;实施形态见本 change 的 `design.md`（D-1~D-5）。
> 共 **T0~T7**;标 ⚠️ 的条目为审查/盘点新增。

## T0 — 前置与盘点

- [ ] T0.1 ⚠️ 写**盘点脚本**（可重跑）：遍历 `configs/**/*.json`（排除 `configs/param_rules/**`）⇒ 输出「类型 × 参数键 × 实例数 × 是否有读取面（`on_config_loaded` / `set_config`）」;判据 = 脚本输出的类型数 / 键×类型对数与 `design.md` D-1 一致
- [ ] T0.2 ⚠️ **逐键裁决表**回填 `design.md` D-1：每个键一个态（①/②/③）+ 证据 + 态① 的观测量选型;⛔ 不得留「待定」

## T1 — B1 批次（`ADR-X.17` F-14 五类 + 扩展键）

- [ ] T1.1 `CacheTLM` 六键（`size` / `level` / `associativity` / `protocol` / `assoc` / `line_size`）裁决 + 落地;⚠️ 该类**无容量/组相联模型** ⇒ 优先评估态②
- [ ] T1.2 `TrafficGenTLM` 七键（`pattern` / `num_requests` / `start_addr` / `end_addr` / `hotspot_addrs` / `hotspot_weights` / `stride`）裁决 + 落地（`seed` 已消费，不动）
- [ ] T1.3 `GmmuTLM.page_size_bytes`（读而丢弃）裁决 + 落地
- [ ] T1.4 `LwcBusHub.max_ip_ports` 裁决 + 落地
- [ ] T1.5 `MemoryTLM` 五键（`size` / `type` / `mem_type` / `read_latency` / `write_latency`）裁决 + 落地;⚠️ `size` 与 `capacity_gb` 语义重叠 ⇒ 评估态②
- [ ] T1.6 B1 回归：`[router]` / `[memory]` / `[gmmu]` / `[trafficgen]` / `[lwc]` 全绿

## T2 — B2 批次（零配置读取面家族 · 14 类）

- [ ] T2.1 `CPUTLM` 三键（`pattern` / `request_interval` / `start_addr`）裁决 + 落地（**95 个配置实例** ⇒ 影响面最大）
- [ ] T2.2 `CrossbarTLM.port_count` 裁决 + 落地
- [ ] T2.3 `ComputeUnitTLM`（`kernel_id` / `workgroup_size`）裁决 + 落地
- [ ] T2.4 `GPUTLM` 四键（`kernel_id` / `workgroup_size` / `start_addr` / `end_addr`）裁决 + 落地
- [ ] T2.5 `PcieControllerIP` 三键（`bar_bases` / `bar_sizes` / `link_speed_gts`）裁决 + 落地;⚠️ `bar_sizes` 另有**板级消费方**（`dgpu_board_shell.cc:78-79`）⇒ 按 `ADR-X.17` D-3 三态条款分开处置
- [ ] T2.6 `PcieMemoryDevice.capacity_gb` 裁决 + 落地（⚠️ backing 归 `DGpuBoard` —— per `ADR-DGPU-05`;该键与注入的 backing 语义重叠 ⇒ 评估态②）
- [ ] T2.7 `StreamingMultiprocessorTLM` 两键（`execution_latency` / `warp_size`）裁决 + 落地
- [ ] T2.8 `ArbiterTLM.n_ports` 裁决 + 落地
- [ ] T2.9 `MemoryClusterTLM` 两键（`capacity_gb` / `channels`）裁决 + 落地（⚠️ 与 `MemoryCluster` 是两个类）
- [ ] T2.10 `SharedMemoryTLM` 两键（`banks` / `size_kb`）裁决 + 落地
- [ ] T2.11 `SocInternalNoc` 两键（`data_flow` / `vc_counts_enabled`）裁决 + 落地（该类已 archived ⇒ 态② 首选）
- [ ] T2.12 `TCCTLM` 两键（`coalescing` / `size`）裁决 + 落地
- [ ] T2.13 B2 回归：全量套件绿 + `configs/` 存量写法逐处登记（态② 键的命中清单）

## T3 — B3 批次（`RouterTLM.routing`）

- [ ] T3.1 `routing` 裁决 + 落地（态②「生成方停止产出」或态③「显式拒绝」）;⛔ **不得**停留在 `ignore_param` 登记（`ADR-X.17` D-3b）
- [ ] T3.2 `GpuNoC::simulate_instantiate` 的内部配置生成与 T3.1 的处置同步（`src/tlm/cluster/gpu_noc_cluster.cc:26-31`）
- [ ] T3.3 `[simmodule][infra]` 的 GpuNoC 用例绿（`test/test_simmodule_infra_clusters.cc:57/80`）

## T4 — 测试（逐键判别）

- [ ] T4.1 态① 的每个键一条**行为判别测试**（两个不同合法值 ⇒ 可观测指标不同 —— `ADR-X.17` D-6）;⛔ 不得为通过判别而新增「只为测试存在」的观测面
- [ ] T4.2 态② 的每个键一条**装配失败**测试（配置含该键 ⇒ `instantiateAll` 返回 false 且错误含键名）
- [ ] T4.3 态③ 的每个键一条**非法值拒绝**测试（错误含键名 + 合法值集合）
- [ ] T4.4 ⚠️ **存量配置清单**：每个态② 键列出 `grep -rn '"<key>"' configs/` 的全部命中，逐处判定（改键名 / 删键 / 保留并接受失败 ⇒ 后者须显式登记）

## T5 — 台账

- [ ] T5.1 `docs/implementation/module-param-baseline.md` **表 B 逐条更新**（`待修` ⇒ `已消费` / `已停止产出` / `已显式拒绝`），逐条附实现位置
- [ ] T5.2 ⚠️ AC-07：`grep -rn 'ignore_param(' src/ include/` 的非注释调用点与表 B 条目**一一对应**（承接 `cpptlm-module-param-read-declares` 的 AC-14）

## T6 — 验证

- [ ] T6.1 `cpptlm_tests` 全量绿（assertions / cases ≥ 基线 **883191 / 2371**）
- [ ] T6.2 `python3 -m pytest test/python/` 全绿（基线 232 passed）
- [ ] T6.3 `ctest -R cep` 全绿（基线 100/100）
- [ ] T6.4 `openspec validate cpptlm-module-param-effect-landing --strict` + `cep_docs_sync_check` + `cep_doc_ref_check` + `agents_md_lint` 全绿
- [ ] T6.5 ⚠️ AC-06：`git diff <base> -- configs/` 的每处变更都对应一个态② 任务（⛔ 不得顺手删配置）

## T7 — 收口

- [ ] T7.1 台账 Entry（实施记录 + 实测 + 改动面 + 遗留）
- [ ] T7.2 KPI 回填（活口径 6 处）
- [ ] T7.3 spec delta 合并（`openspec archive`，⛔ 不用 `--skip-specs`）+ 新 main spec 的 Purpose 手写
- [ ] T7.4 独立验收（切 verifier;AC-01~AC-09 + G-1/G-2 逐条取证）
- [ ] T7.5 ⚠️ **残留清点**：本 change 范围内不得留 TODO / 半截实现 / 临时日志 / 被 skip 的测试 / 未替换的 mock
