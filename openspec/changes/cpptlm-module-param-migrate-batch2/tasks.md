# tasks: cpptlm-module-param-migrate-batch2

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`;实施形态见本 change 的 `design.md`（D-1~D-6）。
> 共 **T0~T8**;标 ⚠️ 的条目为审查新增。阶段划分见 design D-5（P0 / P1a / P1b / P2 / P3）。

## T0 — 前置（机制侧改造）

- [ ] T0.1 ⚠️ **基类调用序列改造**（design D-2.3）：`SimObject` 提供「`begin` → 模块自读 → `enforce`」路径，供 SimModule 家族的 `set_config` 覆写使用;⛔ 不得出现「基类 `enforce` 先于模块 `load_param`」的顺序（会把已声明键误判为未知键）
- [ ] T0.2 ⚠️ **嵌套失败传播**（design D-4）：`SimModule::simulate_instantiate` 的内部装配失败 ⇒ 外层 `instantiateAll` 返回 false;⚠️ 幂等守卫须改为「已 populate **且**上次成功」才短路
- [ ] T0.3 ⚠️ `simulate_instantiate` 签名/返回值变更的**调用点全量核实**（`src/core/module_factory.cc` Step 4.5 + 各容器覆写）
- [ ] T0.4 ⚠️ 既有测试中依赖「内层失败但外层返回 true」的用例**逐条评估**并登记（proposal W-3）

## T1 — P1a：迁移（ChStream 家族 · 10 个类型）

> 每类型四件套：**声明化**（`load_param`）· **行为判别测试**（D-6）· **准入表加项** · **旧声明面清理**。

- [ ] T1.1 `NICTLM`：`node_id` / `mesh_x` / `mesh_y` 声明化 + **CFG-08 分支退役**（同 commit!）+ 准入表加项
- [ ] T1.2 `MemoryTLM`：`capacity_gb` 声明化 + 板级直读键态②登记（D-6）;⚠️ 零效果键（`mem_type` / `read_latency` / `size` / `type` / `write_latency`）**不在本任务范围** ⇒ 依赖 effect-landing ⇒ 暂缓准入
- [ ] T1.3 `VramControllerTLM`：全部板级直读键 ⇒ 态②登记 + 准入表加项;⚠️ 核实其与基类 `MemoryTLM::on_config_loaded` 的调用链
- [ ] T1.4 `GmmuTLM`：`tlb_*` 态②登记;⚠️ `page_size_bytes` 依赖 effect-landing ⇒ 暂缓准入
- [ ] T1.5 `TrafficGenTLM`：`seed` 声明化 + 准入表加项;⚠️ 其余 7 键依赖 effect-landing
- [ ] T1.6 `SdmaEngineTLM`：`max_inflight` / `translate_latency` / `vram_size_bytes` 声明化 + 判别测试 + 准入
- [ ] T1.7 `PcieEndpointIP`：声明化 + 判别测试 + 准入;⚠️ **二次 `on_config_loaded` 的幂等性**必须保持（F-11）;⚠️ `bar_sizes` 与板级同名 ⇒ 按 D-3 裁决单一消费方
- [ ] T1.8 `CohIfNICTLM`：`node_id` / `mesh_x` / `mesh_y` 声明化 + `address_regions`（非标量）态②登记 + 准入
- [ ] T1.9 `CohIfResponderTLM`：`emit_completions` / `completion_latency_cycles` / `emit_snoops` / `snoop_interval_cycles` 声明化 + 判别测试 + 准入
- [ ] T1.10 `PcieEndpointTLM`：**显式排除**（`[[deprecated]]` + 23 ABI 冻结面 ⇒ 零修改）⇒ 在准入表与台账中登记「不迁」及理由

## T2 — P1a：迁移（SimModule 家族 · 10 个类型）

- [ ] T2.1 `CpuCluster`：`cluster_id` / `num_cpus` 声明化 + 判别测试 + 准入
- [ ] T2.2 `CacheCluster`：`l1_count` 声明化 + `l1_size` / `l2_size`（**字符串** + 单位解析，解析逻辑归模块内）+ 判别测试 + 准入
- [ ] T2.3 `MemoryCluster`：`channel_count` / `memory_type` 声明化 + `channel_size`（字符串）+ 判别测试 + 准入
- [ ] T2.4 `ComputeCluster`：`cu_count` 声明化 + `cu_template`（**对象**）态②登记 + 判别测试 + 准入
- [ ] T2.5 `TpcCluster`：`tpc_id` / `cu_per_tpc` 声明化 + `cu_template` 态②登记 + 判别测试 + 准入
- [ ] T2.6 `GpcCluster`：`gpc_id` / `tpc_per_gpc` / `cu_per_tpc` 声明化 + `cu_template` 态②登记 + 判别测试 + 准入
- [ ] T2.7 `GpuCluster`：`gpc_count` / `tpc_per_gpc` / `cu_per_tpc` 声明化 + `cu_template` 态②登记 + 判别测试 + 准入
- [ ] T2.8 `ApuSoC`：`cpu_topology` / `gpu_topology`（**嵌套块**）态②登记 + 准入
- [ ] T2.9 `CepMeshInterconnect`：`template`（嵌套块）态②登记 + 准入
- [ ] T2.10 ⚠️ `GpuNoC`：`mesh_size` 声明化 + 准入;⚠️ `routing` 依赖 effect-landing（态②/③ 未裁决）⇒ **本任务暂缓准入**

## T3 — P1b：依赖 effect-landing 的部分

- [ ] T3.1 ⛔ **前置**：`cpptlm-module-param-effect-landing` 的 B1（`TrafficGenTLM` / `MemoryTLM` / `GmmuTLM`）与 B3（`routing`）完成
      - ⚠️ **机械前置命令**（critic 2026-11-18 要求 —— 否则「前置」只靠人读）：`bash scripts/test/rddf_stage_gate.sh --phase C` ⇒ 断言 C1 的 `tasks.md` 无未勾 task 且 C2 目录未提前开工
- [ ] T3.2 `TrafficGenTLM` / `MemoryTLM` / `GmmuTLM` / `GpuNoC` 的剩余键按裁决结果声明化 + 准入
- [ ] T3.3 ⚠️ 零效果键的**存量配置清单**复核（承接 effect-landing T4.4）

## T4 — P2：ParamRules 机制退役

- [ ] T4.1 删 `include/core/param_rules.hh`（`ParamRule` / `ParamRules` / `ParamType`）
- [ ] T4.2 删 `ModuleFactory::registerParamRules` / `getParamRulesRegistry`（`include/core/module_factory.hh:87/94`）
- [ ] T4.3 删工厂侧默认值注入 + `required` 检查循环（`src/core/module_factory.cc:263-291`）;⚠️ 默认值已由 `load_param` 落成员、**不写回 JSON**（`ADR-X.17` D-5）
- [ ] T4.4 删 `validate_module_params`（`src/core/module_factory_validate.cc:466-499`，零调用方死代码）
- [ ] T4.5 ⚠️ `ParamParser` 的 `validate`（min/max）随死函数删;`parse` 的其他调用方**逐一核实** ⇒ 活代码不得连带删
- [ ] T4.6 ⚠️ `test/test_d3_param_resolver_registry.cc` 随机制退役（删文件）⇒ 但其上一 change 新增的「`RouterTLM` 已迁移」**正向契约断言 SHALL 迁移**到新文件（⛔ 不得丢失）
- [ ] T4.7 ⛔ **不动** `configs/param_rules/*.json`（`ADR-X.10` §F 未裁决）⇒ 显式登记「本 change 不裁决」

## T5 — 测试与验收支撑

- [ ] T5.1 ⚠️ AC-01：准入表断言（从 `admitted_param_types()` 派生，⛔ 测试内不硬编码第二份清单）
- [ ] T5.2 ⚠️ AC-07：**≥3 个不同类型**的拼错键 ⇒ 装配失败且错误含键名
- [ ] T5.3 ⚠️ AC-05：嵌套容器内缺必填 ⇒ 外层 `instantiateAll` 返回 false
- [ ] T5.4 ⚠️ AC-08：板级直读键三态合规核对（`ignore_param` 调用点 ↔ 台账表 A）
- [ ] T5.5 ⚠️ AC-02：逐类型行为判别测试（每个声明键两条不同合法值 ⇒ 可观测指标不同）

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
