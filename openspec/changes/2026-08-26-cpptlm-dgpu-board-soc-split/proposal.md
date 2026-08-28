# cpptlm-dgpu-board-soc-split: DGpuBoard C++ shell + DGpuSoc JSON 拓扑 + s2 helper 迁移

> **状态**: 📋 Proposed — 2026-08-26 · **日期**: 2026-08-26 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D4/D5/D6
> **依赖**:
> - [`2026-08-26-cpptlm-dgpu-pcie-endpoint`](../2026-08-26-cpptlm-dgpu-pcie-endpoint/)（PcieEndpointTLM 端口存在, **未 archive, tasks 25/25 complete** per Metis 审查 2026-08-28）
> - [`2026-08-26-cpptlm-dgpu-sdma-engine`](../2026-08-26-cpptlm-dgpu-sdma-engine/)（SdmaEngineTLM 端口存在, **已 archive 2026-08-28**）
> - [`2026-08-21-cpptlm-v05-mvp-s3-command-pipeline`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/) **三个 commit 落地即可**（T-s3-1/T-s3-2/T-s3-3 头文件接口冻结），不等 archive（per design §6 Q5 判据 1-3）
> - [`2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites`](../2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites/) (change E, **未 archive**) — 本 change 依赖其:
>   - T-prereq-4 (Doorbell 排队测试) 锁定 `doorbell_mvp.*` 当前行为,C 的 T-bs-2 Doorbell 迁移需在 E 测试后或同步修订 E 用例
>   - C 的 T-bs-2 是 **PBA 语义修改后续影响**: 若 E 的 T-prereq-3 先修订 MSI-X 测试断言,C 实施时 Doorbell + MSI-X 集成需引用 PBA 语义
> **依赖方向说明**: 本 change 是 pcie-endpoint + sdma-engine 的下游消费者（依赖其组件端口就位），是 abi-export 的上游生产者（为其提供 shell 与冻结 ABI 头）。完整 DAG: `pcie-endpoint → {sdma-engine → board-soc-split → abi-export}`，precondition chain `pcie-slice-prerequisites → board-soc-split → abi-export`。

---

## Why

ADR-SOC-07 D1 决策的落地：s2 `DGpuBoardTLM` 单体（ChStreamModuleBase 外壳 + `Impl` 8 个普通 C++ 值成员 + `tick()` 手动串联 + `set_stream_adapter()` no-op）必须重构为两层：

- `DGpuBoard`（C++，非数据面组件）：仅承担 ADR-088 23 ABI 的 shell 职责（ABI 翻译 / 设备枚举 / SOC 装配 / 回调接线 / 生命周期）；
- `DGpuSoc`（SimModule 容器）：JSON + ModuleFactory 构建全部内部组件，连接走 JSON `connections` + StreamAdapter。

这是 CppTLM 组件化模型（JSON + 注册表 + StreamAdapter）在 dGPU 路径上的归位，也是 ADR-088"仿真拓扑与真硬件一致"承诺的 CppTLM 侧落地。

**触发事件**:
- 2026-08-26 ADR-SOC-07 创建（Board/SOC 分层修正）
- s2 单体被识别为过渡形态（ADR-SOC-06 Status Update 2026-08-26）

---

## What Changes

### 1. 新建文件

| 文件 | 用途 |
|------|------|
| `include/tlm/gpu/dgpu_soc.hh` + `src/tlm/gpu/dgpu_soc.cc` | **🆕 DGpuSoc**（SimModule 容器，嵌套 JSON 实例化 SOC 内部组件） |
| `include/tlm/gpu/dgpu_board_shell.hh` + `src/tlm/gpu/dgpu_board_shell.cc` | **🆕 DGpuBoard**（C++ ABI shell，**不继承** ChStreamModuleBase/SimModule；5 项职责 per ADR-SOC-07 D1） |
| `configs/dgpu_board_v1.json`（替代 `.json.in`） | board 级 JSON 顶层容器（pcie_ep + sdma + cp + tmu + sq + cq + gpu_cluster + memory + connections） |
| `test/test_dgpu_soc_from_config.cc` | SOC JSON 实例化 + 内部 connections 解析 PASS |
| `test/test_dgpu_board_shell_abi.cc` | shell 5 职责：ABI 翻译注入 / 枚举 / 回调接线 / 生命周期 PASS |
| `test/test_dgpu_pcie_device_perspective.cc` | **适配重写**：PCIe 驱动视角 6 测试改为经 shell→SOC 端口注入路径（语义保留） |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `include/modules_cluster.hh` | 加 `REGISTER_MODULE(DGpuSoc)`（SimModule 注册表） |
| `include/chstream_register.hh` | s2 `DGpuBoardTLM` 注册降级为 legacy 别名（过渡期）或移除（视迁移完成度，见 design §5） |

### 3. 迁移映射（per ADR-SOC-07 D6）

| s2 `Impl` 值成员 | 迁移目标 | 方式 |
|------------------|---------|------|
| `DGpuBar` | 拆分：BAR0 寄存器表 → `PcieEndpointTLM`；VRAM → `MemoryCluster`/MemoryTLM | 删除 `dgpu_bar.hh` 独存语义，常量化参数进 JSON |
| `Doorbell` | `PcieEndpointTLM` 门铃 register block | strong-order 语义保留，迁移测试 |
| `SubmitQueue` | `SubmitQueueTLM`（注册组件） | 提升 + 端口化 |
| `CompletionRing` | `CompletionRingTLM`（注册组件） | 提升 + `done_out` 接线 |
| `CommandProcessor` | `CommandProcessorTLM`（注册组件） | s3 填充后提升 |
| `TmuDispatchProcessor` | `TmuDispatchProcessorTLM`（注册组件） | s3 填充后提升 |
| `CudaCoreAdapterMVP` / `PtxEmuSubmoduleMVP` | 保持 s1 边界，JSON 参数装配 | 不改接口 |
| `UsrLinuxEmuIoctlStub` | **板外测试工具**（不进 SOC） | 从 board JSON 移出 |

### 4. 删除/退役

| 文件 | 处置 |
|------|------|
| `include/tlm/gpu/dgpu_board_mvp.hh` + `.cc`（s2 单体） | 迁移完成后删除（两阶段：先 deprecated 别名，E2E 全绿后物理删除） |

---

## Acceptance Gate（本 change）

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **BS-G1** DGpuSoc 从 JSON 完整实例化（全部内部组件 + connections 非空） | CppTLM | ⏳ | `ctest -R "test_dgpu_soc_from_config"` PASS |
| **BS-G2** DGpuBoard shell 5 职责测试 PASS | CppTLM | ⏳ | `ctest -R "test_dgpu_board_shell_abi"` PASS |
| **BS-G3** PCIe 驱动视角 6 测试经新路径 PASS（语义等价） | CppTLM | ⏳ | `ctest -R "test_dgpu_pcie_device_perspective"` PASS |
| **BS-G4** s2 单体删除 + 注册表清理 + 全量无回归 | CppTLM | ⏳ | `build/bin/cpptlm_tests` 全 PASS + `validate_topology` PASS |
| **BS-G5** 23 ABI 语义不变验证（shell 层契约冻结） | CppTLM | ⏳ | (per Metis 审查可执行化) **两步机械化 + 一步人工核对**:<br>①. 自动化: `scripts/diff_abi_vs_adr088.sh` 从 `docs/superpowers/plans/.../adr-088-dgpu-complete-simulation.md` §D5 提取 23 项 ABI 名称+签名,与 `include/abi/cpptlm_emulator.h` grep diff 输出比对,差异 > 0 报错<br>②. 自动化: `gcc -c -std=c11 -I include/abi include/abi/cpptlm_emulator.h -fsyntax-only` 纯 C 编译 smoke PASS<br>③. 人工: 在 change PR description 中列出 23 项 ABI 逐项状态(全部 frozen),由 owner 签收 |

**最终验收**: BS-G1~G5 全部 ✅ + s2 `Impl` 值成员零残留 + 本 change 可独立 archive。

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **UsrLinuxEmu** | ADR-088 §D5 23 ABI（shell 层接口签名不变，shell 实现由本 change 完成）+ 2026-08-26 修订注记已写入 | ✅ 无新 API 需求 |
| **sdma-engine (CompletionBundle 复用)** | 本 change 的 `dgpu_bundles_tlm.hh` 不定义 `CompletionBundle`；复用 `cpptlm-dgpu-sdma-engine` change 交付的 `bundles::CompletionBundle`（sdma 是该类型唯一所有者） | ✅ 已对齐 |
| **UsrLinuxEmu `cpptlm-dgpu-abi-export`（后续 change D）** | 23 ABI C 函数体实现 + `cpptlm_emulator` SHARED 库暴露给 UsrLinuxEmu 端 dlopen/链接 | ⏳ 待起 — 依赖本 change archive |
| **`2026-08-28-cpptlm-dgpu-pcie-slice-prerequisites` (change E)** | **时序声明 (per Metis 审查 2026-08-28)**: E 的 T-prereq-4 (Doorbell 排队测试) 锁定 `doorbell_mvp.*` 当前行为,C 的 T-bs-2 Doorbell 迁移需按以下顺序之一: ① E 测试先行 → C 修订 E 用例; ② C 先行 → E 测试跟随修订 | ⏳ **未 archive** — owner 决策时序 |
| **PTX-EMU** | 无（s1 边界不变） | ✅ N/A |

---

**起草**: Sisyphus (2026-08-26, per ADR-SOC-07 D1/D6)
**Owner**: CppTLM Team
**状态**: 📋 Proposed — 依赖 pcie-endpoint + sdma-engine + s3 三 commit 落地（per design §6 Q5 判据 1-3；不等 archive）
