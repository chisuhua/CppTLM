# OpenSpec Change: gpu_soc Phase 8.C — 高级特性

> **Change ID**: `2026-06-24-gpu-soc-phase8c-advanced`
> **Status**: 🔄 Draft（待 Phase 8.B 完成后启动）
> **Date**: 2026-06-24
> **Author**: Sisyphus
> **Target Release**: Phase 8.C end (~3 weeks)
> **Depends on**: `2026-06-24-gpu-soc-phase8b-core` (M2)

---

## 1. Why（动机）

**Phase 8.B 完成后**：
- ✅ 6 个核心模块（SubCore / WarpScheduler / Scoreboard / TensorCore / Pipeline / L2Partition）落地
- ✅ 5 类 microbenchmark 跑通，gpgpu-sim 带宽 ±15%
- ❌ **但**：缺 TCC 写合并 / TMA 异步拷贝 / DSM inter-SM shmem / PowerModel
- ❌ **但**：缺 Python 拓扑生成器 + 报告生成
- ❌ **但**：与 apu_soc 共享子模块的端到端集成未验证

**目标（Phase 8.C 完成后）**：
- ✅ 4 个高级模块（Tcc / Tma / Dsm / PowerModel）落地
- ✅ 3 个 Python 子包（cpptlm.nvidia / cpptlm.gpu_workload / cpptlm.gpu_soc）
- ✅ 与 apu_soc Phase 7.F 共享 GpuCluster 端到端集成
- ✅ 完整 5 类场景验证报告
- ✅ M3 验收：5 类场景 gpgpu-sim 数值对照（带宽 ±15%, 延迟 ±20%）

---

## 2. What Changes（变更内容）

### 新增文件（4 个 .hh + 4 个 .cc + 4 个 test + 3 个 Python 子包 + 4 个微架构 doc）

| 类型 | 路径 |
|------|------|
| Header | `include/tlm/gpu/tcc_tlm.hh` |
| Header | `include/tlm/gpu/tma_tlm.hh` |
| Header | `include/tlm/gpu/dsm_tlm.hh` |
| Header | `include/tlm/gpu/power_model_tlm.hh` |
| Impl | `src/tlm/gpu/{tcc,tma,dsm,power_model}_tlm.cc` |
| Test | `test/test_{tcc,tma,dsm,power_model}_tlm.cc` |
| Test | `test/test_gpu_soc_phase8c.cc`（apu_soc 集成） |
| Python | `cpptlm/cpptlm/nvidia/{__init__,topology,blueprint,export,sku_library}.py` |
| Python | `cpptlm/cpptlm/gpu_workload/{__init__,generator,replay,patterns}.py` |
| Python | `cpptlm/cpptlm/gpu_soc/{__init__,simulate,report}.py` |
| Python Test | `test/python/test_cpptlm_{nvidia,gpu_workload,gpu_soc}.py` |
| Config | `configs/templates/gpu_soc/gpu_soc_phase8c.json` |
| Doc | `docs/soc_arch/modules/gpu-tcc.md` |
| Doc | `docs/soc_arch/modules/gpu-tma.md` |
| Doc | `docs/soc_arch/modules/gpu-dsm.md` |
| Doc | `docs/soc_arch/modules/gpu-power-model.md` |
| Report | `docs/validation/phase8c_verification_report.md` |
| Report | `docs/validation/gpgpu_sim_baseline.csv` |

### 修改文件

| 文件 | 修改 |
|------|------|
| `include/chstream_register.hh` | +4 行注册 |
| `include/tlm/cluster/gpu_cluster.hh` | 集成 TccTLM + TmaTLM + PowerModelTLM |
| `AGENTS.md` | STRUCTURE 节加 `cpptlm/cpptlm/{nvidia,gpu_workload,gpu_soc}/` |
| `docs/adr/README.md` | (ADR-NV-01 已在 commit 3aa810b) |
| `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` | 追加 Phase 8 节 (M3 完整状态) |

---

## 3. Goals

| 编号 | 标准 |
|------|------|
| **G1** | 4 个高级模块单元测试全 pass `[gpu][tcc][tma][dsm][power]` |
| **G2** | 3 个 Python 子包 pytest 全 pass (15+ 用例) |
| **G3** | apu_soc 集成测试 pass: 两者引用同一 GpuCluster 实例 |
| **G4** | 完整 5 类场景验证报告生成 (带宽 ±15%, 延迟 ±20%) |
| **G5** | 性能 M3: 完整仿真可跑 (无具体性能目标) |
| **G6** | docs_sync 0 missing + format clean |

---

## 4. Non-Goals

- ❌ APU coherence 完整实现（apu_soc Phase 7.C 范畴）
- ❌ dGPU + PCIe（Phase 9 备选）
- ❌ 真实 kernel 编译/执行

---

## 5. Risks

| # | 风险 | P | I | 缓解 |
|---|------|:-:|:-:|------|
| **R1** | Python 子包与 C++ 仿真器接口不稳定 | 中 | 高 | Pybind11 stub 先实现，8.C 末切换 |
| **R2** | apu_soc 共享 GpuCluster 接口冲突 | 中 | 高 | Phase 8.A 已引入 GpuClusterSharedInterface 缓解 |
| **R3** | TCC 写合并粒度选择 | 中 | 中 | 默认 64B cache line (按 SOC-09) |

---

## 6. 关联文档

- **Spec**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` §5.3
- **Plan**: `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.C
- **ADR**: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`
- **依赖**: `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` (M2)
- **Sibling**: apu_soc Phase 7.F (`openspec/changes/2026-06-12-phase7-apu-fused-soc-design.md`)

---

## 7. 验收节点

| 节点 | 时机 | 标准 |
|------|------|------|
| M3 单元测试 | Task 17-20 完成 | G1 |
| M3 Python 包 | Task 21-23 完成 | G2 |
| M3 集成测试 | Task 24 完成 | G3+G4 |
| M3 文档 | Task 25 完成 | G6 |
| Oracle 审查 | 全部 Task 完成 | — |
| OpenSpec 归档 | Oracle 批准 | → archive/ |
