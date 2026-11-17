# tasks: cpptlm-module-param-effect-landing

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`;实施形态见本 change 的 `design.md`（D-1~D-5）。
> 共 **T0~T7**;标 ⚠️ 的条目为审查/盘点新增。
>
> **进度（2026-11-18）**：**T0 + T1（除 T1.1）+ T4/T5 的 B1 部分 + T6 已完成**（B1 批次 = `TrafficGenTLM` / `MemoryTLM` / `GmmuTLM` / `LwcBusHub`）。
> **B2（T2）/ B3（T3）未开始**;`T1.1`（`CacheTLM`）**阻塞于 R-4**（CEP 冻结面，解冻须 architect 裁决）。
> 逐键裁决结果与实测口径见 `design.md` **D-1.6**;台账同步见 `docs/implementation/module-param-baseline.md`（表 A/B + §2bis）。

## T0 — 前置与盘点

- [x] T0.1 ⚠️ 写**盘点脚本**（可重跑）：遍历 `configs/**/*.json`（排除 `configs/param_rules/**`）⇒ 输出「类型 × 参数键 × 实例数 × 是否有读取面（`on_config_loaded` / `set_config`）」;判据 = 脚本输出的类型数 / 键×类型对数与 `design.md` D-1 一致
  - 落盘 `scripts/test/param_effect_inventory.py`（`--json` / `--check`）;实测 36 类型 / 107 键×类型对（口径与 D-1 零效果族的差异见 D-1.6.3）
- [x] T0.2 ⚠️ **逐键裁决表**回填 `design.md` D-1：每个键一个态（①/②/③）+ 证据 + 态① 的观测量选型;⛔ 不得留「待定」
  - **B1 部分已回填**（D-1.6.4）;`CacheTLM` 6 键记「待裁决（R-4）」;B2/B3 的键在对应批次落地时回填

## T1 — B1 批次（`ADR-X.17` F-14 五类 + 扩展键）

- [ ] T1.1 ⛔ **阻塞（R-4）** `CacheTLM` 六键（`size` / `level` / `associativity` / `protocol` / `assoc` / `line_size`）裁决 + 落地;⚠️ 该类**无容量/组相联模型** ⇒ 优先评估态②。**但该类是 CEP 冻结面**（`ADR-CEP-15` IC1,闸 = `test/test_cep_module_factory_seam.cc:133-134`）⇒ 实测改动即红闸,已回退 ⇒ 解冻 + 建模须 architect 裁决
- [x] T1.2 `TrafficGenTLM` 七键（`pattern` / `num_requests` / `start_addr` / `end_addr` / `hotspot_addrs` / `hotspot_weights` / `stride`）裁决 + 落地（`seed` 已消费，不动）
- [x] T1.3 `GmmuTLM.page_size_bytes`（读而丢弃）裁决 + 落地
- [x] T1.4 `LwcBusHub.max_ip_ports` 裁决 + 落地
- [x] T1.5 `MemoryTLM` 五键（`size` / `type` / `mem_type` / `read_latency` / `write_latency`）裁决 + 落地;⚠️ `size` 与 `capacity_gb` 语义重叠 ⇒ 评估态②
- [x] T1.6 B1 回归：`[router]` / `[memory]` / `[gmmu]` / `[trafficgen]` / `[lwc]` 全绿
  - 实测：`cpptlm_tests` 全量 **883307 assertions / 2386 cases** 全绿（基线 883191 / 2371 ⇒ +116 / +15 全部来自新判别测试）;`pytest` 232 passed;`ctest -R cep` 100/100

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

- [ ] T4.1（**B1 部分已完成**：`test/test_param_effect_landing.cc` 15 cases / 116 assertions，覆盖 `TrafficGenTLM` 8 键 / `MemoryTLM` 4 键 / `GmmuTLM` 1 键 / `LwcBusHub` 1 键）态① 的每个键一条**行为判别测试**（两个不同合法值 ⇒ 可观测指标不同 —— `ADR-X.17` D-6）;⛔ 不得为通过判别而新增「只为测试存在」的观测面
- [ ] T4.2（**B1 部分已完成**：`MemoryTLM` 的 `type` / `mem_type` + `size`↔`capacity_gb` 互斥）态② 的每个键一条**装配失败**测试（配置含该键 ⇒ `instantiateAll` 返回 false 且错误含键名）
- [ ] T4.3 态③ 的每个键一条**非法值拒绝**测试（错误含键名 + 合法值集合）
- [ ] T4.4（**B1 部分已完成**：`configs/apu_soc_full.json` 删 `MemoryTLM.type: "HBM2"` 1 处;`configs/library_memory_demo.json` 删 `mem_type` 4 处;`src/tlm/cluster/memory_cluster.cc` 停止生成 `size`→保留 / `type`→删除）⚠️ **存量配置清单**：每个态② 键列出 `grep -rn '"<key>"' configs/` 的全部命中，逐处判定（改键名 / 删键 / 保留并接受失败 ⇒ 后者须显式登记）

## T5 — 台账

- [ ] T5.1（**B1 部分已完成**：B-2/B-3/B-4 改「已消费」;B-5 拆分「已消费 + 已停止产出」;B-1 改「阻塞（R-4）」;新增 B-7~B-10 + §2bis + 表 A 6 键）`docs/implementation/module-param-baseline.md` **表 B 逐条更新**（`待修` ⇒ `已消费` / `已停止产出` / `已显式拒绝`），逐条附实现位置
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
