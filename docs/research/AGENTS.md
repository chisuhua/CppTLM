# docs/research/ — GPU/PCIe/TMU/WDU 专利研究索引

> **版本**: v0.5 · **日期**: 2026-08-19 · **状态**: 📋 Research Index
> **维护**: CppTLM Team (Sisyphus)
> **关联 OpenSpec**: [`openspec/changes/2026-08-19-cpptlm-v05-redo/`](../../openspec/changes/2026-08-19-cpptlm-v05-redo/)
> **关联 ADR**: [`ADR-X.16-cpptlm-v05-redo.md`](../../adr/ADR-X.16-cpptlm-v05-redo.md)

---

## 1. 范围

`docs/research/` 收纳 GPU/dGPU 仿真相关的**专利解析 + 综述文档**,为 v0.5 redo 设计决策提供理论参考。**不是 openspec spec**,**不进 ABI 契约**,**仅作 background reference**。

**使用原则**(per Oracle + Metis 重评):
- ✅ 引用专利的设计思路作为 v0.5 实现参考
- ❌ **不直接复制专利内容到 CppTLM 源码**(避免版权风险)
- ❌ **不声称与专利实现等价**(仅 reference,非 conformance)

## 2. 目录索引

### 2.1 CP/ — Command Processor (12 专利)

| 文件 | 主题 | 关键概念 |
|------|------|---------|
| `CP/amd/overview.md` | AMD CP 专利综述 | 6 件专利族 |
| `CP/nvidia/overview.md` | NVIDIA CP 专利综述 | 6 件专利族 |
| `CP/amd/US8310492B2_...md` | HQD 硬件队列调度 | Queue Descriptor + ring buffer 模型 |
| `CP/amd/US8675002B1_...md` | 统一命令缓冲 | Unified command buffer |
| `CP/amd/US9176795B2_...md` | 用户态图形处理分发 | User-mode graphics dispatch |
| `CP/amd/US20210191730A1_...md` | 未映射队列聚合门铃 | Unmapped queue aggregated doorbell |
| `CP/amd/US20210304349A1_...md` | 迭代间接命令缓冲 | Iterative IB (Indirect Buffer) |
| `CP/amd/US20220091847A1_...md` | 间接缓冲预取 | IB prefetch |
| `CP/amd/US11822956B2_...md` | 自适应线程组分发 | Adaptive threadgroup dispatch |
| `CP/amd/US12131186B2_...md` | 硬件加速动态工作创建 | Dynamic work creation |
| `CP/nvidia/US10489056B2_...md` | SM 系统队列管理器 | SM system queue manager |
| `CP/nvidia/US20210149719A1_...md` | 可执行图修改 | Executable graph modification |
| `CP/nvidia/US20210248115A1_...md` | 计算图优化 | Compute graph optimization |

**v0.5 应用**: PM4 TYPE3 opcode 解析(per `command-processor-v05.md` §3)借鉴 AMD HQD + NVIDIA SM 队列管理器的设计思路。

### 2.2 WDU/ — Work Distribution Unit (5 专利)

| 文件 | 主题 | 关键概念 |
|------|------|---------|
| `WDU/overview.md` | WDU 专利综述 | Work distribution patterns |
| `WDU/US10332310B2_...md` | 分布式图元批处理 | Distributed primitive batching |
| `WDU/US8941653B2_...md` | 保序分布式光栅化 | Ordered distributed rasterization |
| `WDU/US9594599B1_...md` | 按 SM 数比例分发工作批次 | Proportional SM-based distribution |
| `WDU/US9710306B2_...md` | 封装计算任务自动节流 | Auto-throttling task packaging |

**v0.5 应用**: ComputeUnitTLM v2 的 `dispatch_whitebox` warp 分发策略借鉴 WDU 思想(per `per-warp-instruction.md` §6)。

### 2.3 TMU/ — Task Management Unit (16 文件)

| 文件 | 主题 |
|------|------|
| `TMU/overview.md` | TMU 综述 |
| `TMU/TMD.md` | **Task Management Descriptor 完整字段布局**(Init/Sched/Exec/QueueState/HW-only/Dep/Queue 区) |
| `TMU/TMU专利专题整理.md` | 23 件专利族整理(3 代形态:Kepler → Volta → Hopper WSDU) |
| `TMU/Atomatic_dependent_task_launch.md` | Dependent TMD 自动启动机制 |
| `TMU/Efficiently_Launch_task_on_processor.md` | PREEXIT/ACQBULK 解耦调度/数据依赖 |
| `TMU/Method_and_system_for_processing_nested_stream_events.md` | Nested stream events (WE/SE dep count) |
| `TMU/Signaling_ordering_and_execution_of_dynamically_generated_tasks...md` | 反射通知 + vspan 合并 + 门槛判定 |
| `TMU/US10552202B2_指令级执行抢占_解析.md` | 指令级执行抢占 |
| `TMU/US11182207B2_依赖任务描述符预取_解析.md` | TMD prefetch |
| `TMU/US20130198760A1_依赖任务自动启动_解析.md` | Dependent TMD auto-launch |
| `TMU/US20210304349A1_...md` | 迭代 IB(已列 CP) |
| `TMU/US8572573B2_非抢占式GPU交互调试_解析.md` | Debug 三态线程清单 |
| `TMU/US9535815B2_GPU负载执行统计收集_解析.md` | Trace cell + thread-state table |
| `TMU/US9928109B2_嵌套流事件处理_解析.md` | Wait Event + Signal Event |

**v0.5 应用**: TmuDispatchProcessor + Scheduler Cache 关键字段 + dep 链(per `command-processor-v05.md` §4 + design.md §3)。

### 2.4 PCIe/ — PCIe 物理行为 (1 文档)

| 文件 | 主题 | 关键概念 |
|------|------|---------|
| `PCIe/PCIe_上的保序write.md` | **PCIe TLP 保序 write 机制** | MMU ordering pipe + dummy non-posted read flush |

**v0.5 应用**: Doorbell::ring strong-ordered path(per design.md §3.2.5 + `ptx-emu-v05-submodule.md` §3)+ PCIe Gen5 x16 250-700ns latency 区间断言(per `tasks.md` T-P1'-1 验证)。

### 2.5 顶层文件

| 文件 | 主题 |
|------|------|
| `gem5-soc-survey.md` | gem5 标准库 + AMD MI300X dGPU + ComputeUnit patterns 综述 |

**v0.5 应用**: ComputeUnitTLM v2 字段参考(`cu_id` / `WarpContext state machine` / `outstandingReqs`)借鉴 gem5 APU/dGPU 实现。

---

## 3. 命名约定

- **专利解析文件**: `US<专利号>_<中文主题>_解析.md`
  - 例: `US20210304349A1_迭代间接命令缓冲_解析.md`
- **综述文档**: `overview.md`(每子目录顶)+ `SYNTHESIS.md`(综合提取)
- **专利原文不存放**(仅分析) — 避免版权 + 体积问题

## 4. 跟踪状态(per `scripts/test/docs_sync_check.sh`)

| 子目录 | 文件数 | 跟踪 |
|--------|------:|------|
| `CP/` | 12 | ✅ tracked |
| `WDU/` | 5 | ✅ tracked |
| `TMU/` | 16 | ✅ tracked |
| `PCIe/` | 1 | ✅ tracked |
| **顶层** | 1 (gem5-soc-survey.md) | ✅ tracked |

**注**: 当前 `git status` 显示 `docs/research/{CP/,PCIe/,TMU/,WDU/}` 为 **untracked** 状态。**v0.5 redo 实施前**需要 `git add` 入仓跟踪(per `tasks.md` T-P3'-2)。

## 5. 与 v0.5 redo 关联

| v0.5 组件 | 引用专利 |
|----------|---------|
| `Pm4Decoder` (per `command-processor-v05.md`) | `CP/amd/US8310492B2_...md` + `CP/nvidia/US10489056B2_...md` |
| `CommandProcessor` 5-state FSM | `CP/amd/US8675002B1_...md`(Unified command buffer)|
| `ComputeUnitTLM::dispatch_whitebox` | `WDU/US9594599B1_...md` + `WDU/US10332310B2_...md` |
| `ScoreboardTLM` 升级 | `TMU/US11182207B2_...md`(TMD prefetch)|
| `PipelineTLM` 升级 | `TMU/TMD.md`(5+V stage)|
| `Doorbell::ring` strong-order | `PCIe/PCIe_上的保序write.md` |
| `TmuDispatchProcessor::pre_dispatch` | `TMU/US20130198760A1_...md`(Dependent TMD auto-launch)|
| ComputeUnit 字段参考 | `gem5-soc-survey.md`(ComputeUnit 4-stage pipeline)|

---

## 6. 引用方式(代码内)

```cpp
// 引用示例(代码注释中)
class Pm4Decoder {
    // Reference: docs/research/CP/amd/US8310492B2_...md
    // (HQD 硬件队列调度模式)
    Pm4Packet parse_type3(...);
};
```

**规则**: 仅在 header doc-comment 引用,**代码内不 include 任何 `docs/research/*.md`**(保持编译防火墙)。

---

## 7. 跨仓同步

- `git add docs/research/CP/ docs/research/WDU/ docs/research/TMU/ docs/research/PCIe/ docs/research/gem5-soc-survey.md` 入仓跟踪(per `tasks.md` T-P3'-2)
- 与 `docs_sync_check.sh --strict` 同步(per `tasks.md` T-P3'-3 验证)

---

**维护**: CppTLM Team (Sisyphus)
**下次更新**: v0.5 redo W8 (P3' 实施期间引用)
