# cpptlm-v05-redo: dGPU Board v0.5 (Submodule + Per-Warp Instruction Precision)

> **状态**: � Design — 等 user review
> **日期**: 2026-08-19 · **Owner**: CppTLM Team (Sisyphus)
> **关联**: 取代 [`openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/`](../../2026-08-18-cpptlm-v3-dgpu-extract/) (v0.4.1, archived)
> **新 ADR**: [`docs/adr/ADR-X.16-cpptlm-v05-redo.md`](../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md)
> **撤销**: v3.0-extract 的 9 周 P0-P4 时间线 + 11 项删除 + v3.0.0 BREAKING bump

---

## Why

### v3.0-extract 决策回顾(被反转)

[`openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/proposal.md`](../../2026-08-18-cpptlm-v3-dgpu-extract/proposal.md) 的 5 项核心决策:

| # | v3.0 决策 | v0.5 反转 |
|---|---------|-----------|
| 1 | `libptxemu_device.so::image_execute` 黑盒集成 | **per-warp instruction step API**(submodule) |
| 2 | PM4 解析委托 host (UsrLinuxEmu/TaskRunner) | **CppTLM CP 模块解析**(符合 PCIe 设备语义) |
| 3 | ComputeUnit 不仿真,12 SM 模块 → Legacy | **ScoreboardTLM + PipelineTLM** 升级 production |
| 4 | 周期精度仅 PCIe (250-700ns) | **per-warp instruction cycle count** |
| 5 | `CPPTLMBRIDGE_VERSION = 2` 永久冻结 | **保持冻结**,新路径走 C++ 源码级契约(per Oracle 强推) |

### 决策理由

**Oracle + Metis 重评(Phase B)** 揭示:

1. **PTX-EMU 已具备指令级骨架** — `WarpContext::execute_warp_instruction(StatementContext&, int)` 已存在,无需重写解释器
2. **v0.5 工作量比预期小一个数量级** — "暴露 + 适配" 不是 "打开 + 重写"
3. **C ABI dlsym 是纯负担** — submodule 静态链接后,直接 C++ 调 SMContext 更干净
4. **PM4 在 driver 解析是 PCI 误区** — 真实硬件 CP 解析 PM4,host driver 仅写入 ring buffer

## What Changes

### 1. 新建文件(Per Phase)

| Phase | 文件 | 用途 |
|-------|------|------|
| **P0'** | `external/PTX-EMU` (git submodule) | PTX-EMU 源码依赖 |
| **P0'** | `docs/adr/ADR-X.16-cpptlm-v05-redo.md` | v0.5 决策锁定 ADR |
| **P0'** | `openspec/changes/2026-08-19-cpptlm-v05-redo/` | 本 change |
| **P1'** | `include/tlm/gpu/ptx_emu_submodule_v05.hh` | Adapter pattern(唯一 include PTX-EMU 头) |
| **P1'** | `src/tlm/gpu/ptx_emu_submodule_v05.cc` | submodule 实现 |
| **P1'** | `include/tlm/gpu/command_processor_v05.hh` | CP 模块(PM4 解析,反 v0.4.1) |
| **P1'** | `src/tlm/gpu/command_processor_v05.cc` | CP 实现 |
| **P1'** | `include/tlm/gpu/pm4_decoder_v05.hh` | Mesa-style TYPE3 PM4 decoder |
| **P1'** | `src/tlm/gpu/pm4_decoder_v05.cc` | decoder 实现 |
| **P2'** | `include/tlm/gpu/compute_unit_v05.hh` | ComputeUnit (adapter 模式) |
| **P2'** | `src/tlm/gpu/compute_unit_v05.cc` | ComputeUnit 实现 |
| **P2'** | `include/tlm/gpu/scoreboard_tlm_v05.hh` | 升级的 ScoreboardTLM |
| **P2'** | `include/tlm/gpu/pipeline_tlm_v05.hh` | 升级的 PipelineTLM |
| **P3'** | `tests/test_v05_dual_path.cc` | 双路径内部一致性(per-warp step vs image_execute) |
| **P3'** | `tests/test_v05_warp_step.cc` | per-warp 精度单元测试 |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `include/cudart/abi_guards.h` | 追加 PTX-EMU v0.5 submodule 断言(per Oracle 强推 C++ 源码契约) |
| `include/tlm/AGENTS.md` | 更新 v0.5 模块注册宏说明 |
| `CMakeLists.txt` | `add_subdirectory(external/PTX-EMU)` 静态链接 |
| `tests/CMakeLists.txt` | 新增 v0.5 测试目标 |
| `docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md` | 追加 `## Status Update (2026-XX-XX)`: v0.5 redo 反转 |

### 3. 删除清单(v3.0-extract 11 项 → 撤销)

| # | 原 v3.0 删除项 | v0.5 状态 |
|---|---------------|-----------|
| 1 | `MemoryBridge` 删除 | **保留**(per-instruction memory access 必需) |
| 2 | `IPtxEmuDriver` 接口 删除 | **保留 + 改写** |
| 3 | `DriverWrapper` 类 删除 | **保留** |
| 4 | `g_ptx_emu_driver` 全局 删除 | **保留** |
| 5 | `cpptlm_set_driver` ABI 入口 删除 | **保留** |
| 6 | `ptx_emu_driver_shim.cc` 删除 | **保留** |
| 7 | vendored `cpptlm_bridge.h` 删除 | **保留**(ABI v2 仍冻结) |
| 8 | vendored `pipeline_interface.h` 删除 | **保留** |
| 9 | vendored `scoreboard_interface.h` 删除 | **保留** |
| 10 | vendored `tensor_core_interface.h` 删除 | **保留** |
| 11 | `PtxEmuDriverApi` 布局锁 删除 | **保留** |

**理由**: v0.5 submodule 模式 = 源码级契约,v3.0 黑盒 dlopen 假设不再适用。**v3.0-extract 的 11 项删除清单全部撤销**。

### 4. 9 周双轨时间线(已修订)

```
P0' (W1)  v0.5 redo 启动
  ├─ HSK-7 协议发出(PTX-EMU 仓) — Oracle 说不必需 C ABI v3,但需源码契约公告
  ├─ CppTLM HSK-7 ack 响应
  ├─ git submodule add external/PTX-EMU
  └─ ADR-X.16 + 本 openspec change 创建

P1' (W2-4)  Adapter pattern 落地
  ├─ CommandProcessor (PM4 解析, 反 v0.4.1)
  ├─ Pm4Decoder (Mesa-style TYPE3)
  ├─ PtxEmuSubmoduleV05 (adapter 模式)
  └─ 双路径 image_execute 与 per-warp step API 共存

P2' (W5-7)  ComputeUnit v2 + SM 升级
  ├─ ScoreboardTLM 升级 production(cycle 模型)
  ├─ PipelineTLM 升级 production(latency issue)
  ├─ 验证 per-warp step API 与 image_execute 功能等价
  └─ 单 kernel E2E 测试

P3' (W8-10) 验证 + 文档
  ├─ 双路径逐字节 diff 测试套件
  ├─ docs/research/ 跟踪
  └─ 同步 AGENTS.md + ADR-X.15 Status Update

P4' (W11-12)  收尾
  ├─ 全部 ≥764 测试通过
  └─ v0.5.0 tag + CHANGELOG
```

## Acceptance Gate

| Gate | Owner | 当前状态 |
|------|-------|:---:|
| #1 HSK-7 协议发出 + ack | PTX-EMU + CppTLM | ⏳ W1 |
| #2 git submodule add PTX-EMU | CppTLM | ⏳ W1 |
| #3 ADR-X.16 + openspec change 创建 | CppTLM | ⏳ W1 |
| #4 CommandProcessor + Pm4Decoder (TYPE3) | CppTLM | ⏳ W2-4 |
| #5 PtxEmuSubmoduleV05 adapter | CppTLM | ⏳ W2-4 |
| #6 ComputeUnit v2 adapter pattern | CppTLM | ⏳ W5-7 |
| #7 ScoreboardTLM + PipelineTLM 升级 production | CppTLM | � W5-7 |
| #8 双路径逐字节 diff 测试 PASS(image_execute vs per-warp step API) | CppTLM | ⏳ W8-10 |
| #9 全部 ≥764 测试通过(v0.5 baseline) | CppTLM | � W11-12 |

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 当前状态 |
|----|---------|:---:|
| **PTX-EMU** | 新 API PR + 源码契约公告 (submodule pin) | ⏳ W1-W2 |
| **CppTLM** | 本 openspec change + ADR-X.16 | 🔄 实施中 |
| **UsrLinuxEmu** | 通知 PM4 解析反转(driver 仍写 ring buffer,CP 端解析) | 🟡 待通知 |

**跨仓 commit 顺序**(per ADR-035 §R5.1):
```
PTX-EMU 新 API 公告 → CppTLM HSK-7 ack → submodule 添加 → adapter 实现 → 双路径验证
```

## Migration (Phase 化详细)

完整 P0'-P4' 操作步骤 + commit 模板,见:
- [`tasks.md`](tasks.md) — 12 周任务清单
- 实施指南待写(`/docs/05-advanced/cpptlm-v05-redo-action-plan.md`)

## References

### 上游决策
- Oracle 重评 `ses_fe691c532ffeF4dACcAd1dNwwY`
- Metis 重评 `ses_fe6915688ffelUG9G8HB1msoaY`
- [ADR-X.15-cpptlm-v3-dgpu-extract.md §Status Update (2026-XX-XX)](../../../docs/adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) — 反转决策追加

### 跨仓研究
- [`docs/research/CP/`](../../../docs/research/CP/) — 12 专利(AMD + NVIDIA Command Processor)
- [`docs/research/WDU/`](../../../docs/research/WDU/) — 5 专利(Work Distribution Unit)
- [`docs/research/TMU/`](../../../docs/research/TMU/) — 16 文件(v0.4.1 已用)
- [`docs/research/PCIe/`](../../../docs/research/PCIe/) — PCIe 保序 write

### 仓 HEAD 锚点
```
PTX-EMU  @87820951 (per Oracle 调研,含 HSK-4 接口 + step machinery)
CppTLM  @923e372 (v0.4.1 DGpuBar RAII 修复 commit,v0.5 起点)
UsrLinuxEmu@37a91b6 (ADR-090 v2,v3.0 架构)
```

---

**起草**: Sisyphus (2026-08-19,基于 Oracle + Metis Phase B 重评 + 5 项决策)
**Owner**: CppTLM Team
**状态**: 📐 Design — 等 user review + v0.5 实施 plan
