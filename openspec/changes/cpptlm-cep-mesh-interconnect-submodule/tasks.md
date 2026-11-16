# tasks: cpptlm-cep-mesh-interconnect-submodule

> **AC 映射**: proposal AC-01 ~ AC-07 · design D-1 ~ D-6
> **上游**: 用户 2026-11-16 选型 = 路径 B（真子模块）· 前序 change `cpptlm-cep-mesh-e2e-carrier`（已归档）

## Phase 1 · 可行性验证（**硬前置**，⛔ 不得跳过）

- [x] **T1.1** 新增 `include/tlm/cep/cep_mesh_interconnect.hh`（最小版：`set_config` 读 `params.template` + `simulate_instantiate` 照 `ComputeCluster` 顺序 + 蓝图加载 + 缺 `modules` 抛错）
- [x] **T1.2** ⚠️ **探针**（D-6 头号风险）：最小 SimModule 内嵌 1 个 `RouterTLM` + 1 条 exposed 端口（`{"internal": "r0.resp_out[4]", "external": "p0_out"}`）⇒ 实测 `findInternalPath("p0_out")` / `getInternalOutputPort(...)` 是否**真的**能解析
  - ⚠️ 同时实测 D-6 末条：`SimModule::simulate_instantiate` 在 `cfg` **无** `modules` 时，幂等守卫是否会拦住后续蓝图加载
  - ⇒ **不通则停**并回报（方案需改，不得硬推）
- [x] **T1.3** 注册 `modules_cluster.hh` + 编译通过

## Phase 2 · 蓝图与拓扑改造

- [x] **T2.1** 新增 `configs/interconnect/cep_mesh_2x2.json`（4 `RouterTLM` 固定名 `r00`/`r01`/`r10`/`r11` + 8 条环边；**⛔ 不含 `outputs`/`inputs` · ⛔ 不含业务模块**）
- [x] **T2.2** 改造 `configs/cep_mesh_e2e.json`：`fabric` 子模块（`params.template` + 8 条 exposed 端口）+ 业务 5 模块 + **9 条跨层** connection（design D-3 表）
- [x] **T2.3** 复跑 **行为等价**（J-4）：`nic_b.last_recv_dst_node()==3` · `last_recv_hops()==3` · `fabric` 内部 router 转发计数 > 0
- [ ] **T2.4** 新增 `configs/interconnect/cep_mesh_1x2.json` + 验证 **换形态只改蓝图**（AC-07）

## Phase 3 · 判据与回归

- [x] **T3.1** 新增 `test/test_cep_mesh_interconnect.cc`（J-1 ~ J-6）
- [x] **T3.2** 零回归：`[cep]` / `[e2e]` / `ctest -R cep` 全绿
- [x] **T3.3** 零 diff 复核：`include/tlm/cep/cohif_nic.hh` · `include/core/sim_module.hh` · `include/core/module_factory.cc` · `include/utils/json_includer.hh` · **8 份点对点 config** ⇒ 全部零 diff

## Phase 4 · 归档

- [ ] **T4.1** critic 审（arch 档）
- [ ] **T4.2** 归档（delta 合并 + Purpose 手写）· KPI 回填 · 台账
- [ ] **T4.3** verifier 独立验收
