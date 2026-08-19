# ADR-X.16: cpptlm-v05-redo (dGPU Board v0.5 — Submodule + Per-Warp Instruction Precision)

> **状态**: ✅ Accepted — 2026-08-19
> **日期**: 2026-08-19 · **Owner**: CppTLM Team (Sisyphus)
> **关联 OpenSpec**: [`openspec/changes/2026-08-19-cpptlm-v05-redo/`](../../../openspec/changes/2026-08-19-cpptlm-v05-redo/) (新建,取代 v3.0-extract)
> **撤销**: [`ADR-X.15-cpptlm-v3-dgpu-extract.md`](../../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) (v3.0-extract 决策,被本 ADR 反转)

---

## 1. Context (背景)

### 1.1 v3.0-extract 决策回顾(被反转)

[`ADR-X.15-cpptlm-v3-dgpu-extract.md`](../../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) 锁定 5 项 v3.0 决策:

| # | v3.0 决策 | 来源 |
|---|---------|------|
| 1 | `libptxemu_device.so::image_execute` 黑盒集成(dlopen + 8 ABI) | ADR-X.15 D1 + HSK-6 |
| 2 | PM4 解析委托 host (UsrLinuxEmu/TaskRunner) | v0.4.1 §1.2 非目标 |
| 3 | ComputeUnit 不仿真,12 SM 模块 → Legacy | tasks.md T-P3-5 |
| 4 | 周期精度仅 PCIe (250-700ns) | v0.4.1 §3.2.5 |
| 5 | `CPPTLMBRIDGE_VERSION = 2` 永久冻结(HSK-6) | ADR-X.15 D3 |

### 1.2 v0.5 反转触发

User 调研 Oracle + Metis 后决策:
- **per-instruction precision 是 dGPU 仿真核心需求**(对标 nvprof / nsight warp state)
- **黑盒 `image_execute` 无法暴露 per-warp PC/progress** — 必须 submodule + modify
- **PM4 应由硬件 CP 模块解析**(符合真实 PCIe 设备语义)
- **PTX-EMU 内部已具备指令级骨架** — `WarpContext::execute_warp_instruction` 已存在,只需暴露

### 1.3 Oracle + Metis Phase B 重评

| 来源 | 关键事实 |
|------|---------|
| Oracle `ses_fe691c532ffeF4dACcAd1dNwwY` | PTX-EMU 指令级骨架已存在,工作量比预期小 1 个数量级 |
| Metis `ses_fe6915688ffelUG9G8HB1msoaY` | 需 5 项关键决策 + 新 openspec change + HSK-7 (或源码契约) |

---

## 2. Decision (决策)

### D1. 方向反转 v3.0-extract (Net-new)

✅ **v0.5 redo = Net-new,完全取代 v3.0-extract**:
- v3.0-extract 的 9 周 P0-P4 时间线**全部撤销**
- 11 项删除清单**全部撤销**
- v3.0.0 BREAKING bump**撤销**(v3.0-extract 不再 ship)

### D2. PTX-EMU 修改范围 = Option A(仅加 API)

✅ **仅在 PTX-EMU 加新公共 API(`SMContext::stepOneInstruction()` 等),不改既有 `image_execute`**:
- 跨仓侵入最小(只 additivity)
- 风险最低(失败时 v3.0 黑盒路径仍可用)
- 与 HSK-7 additivity 兼容

### D3. per-instruction 颗粒度 = per warp instruction

✅ **per-warp instruction,不是 per-PTX-instruction**:
- 与 nvprof / nsight 默认颗粒度一致
- 不需重写 PTX-EMU 解释器循环
- 仿真速度损失 10x-50x (gpgpu-sim 同量级)

### D4. ComputeUnit 架构 = Adapter pattern

✅ **ComputeUnitTLM 通过 adapter 调用 PTX-EMU 内部类**:
- PTX-EMU 头不暴露在 CppTLM 接口(除 `PtxEmuSubmoduleV05.cc` 一处)
- 测试隔离最强(adapter 可 mock)
- 与 v3.0 `PtxEmuSubmodule` 模式一致

### D5. PM4 解析 = CppTLM CP 模块解析

✅ **CppTLM 端 CommandProcessor 模块解析 PM4**(反转 v0.4.1 决策):
- 符合真实 PCIe 设备语义(driver 写入 ring buffer,硬件 CP 解析)
- 替代 v0.4.1 删除的 CommandProcessor + Pm4Decoder 设计
- Mesa-style TYPE3 decoder(Mesa bit field convention)

### D6. 12 SM Legacy 升级范围 = ScoreboardTLM + PipelineTLM (2 个)

✅ **仅升级 ScoreboardTLM + PipelineTLM 作为 demo**,其他 10 个保留 Legacy:
- ScoreboardTLM:cycle 模型与 per-warp step 最接近
- PipelineTLM:latency issue 模型
- 其他 10 个 SM 模块保留 Legacy 标签 + `[legacy]` 标记

### D7. CPPTLMBRIDGE_VERSION = 2 保持冻结

✅ **`CPPTLMBRIDGE_VERSION = 2` 永久冻结保持**(per Oracle 强推 C++ 源码契约):
- v3.0 黑盒 `libptxemu_device.so` 路径仍可用
- v0.5 新路径走 submodule + 源码级契约,**不需要 C ABI v3**
- HSK-7 公告 = **源码契约公告**(依赖的 PTX-EMU 公开类清单 + semver 承诺),不是 C ABI v3

### D8. 验证基础 = PTX-EMU 自家参考 + 双路径内部一致性

✅ **无独立 nvprof / ncu-sys golden corpus,验证限于**:
- PTX-EMU 自家 test corpus
- 双路径逐字节 diff:`image_execute` 黑盒 vs per-warp step API 白盒,同一 kernel 结果必须 byte-identical
- 文档化 "内部一致性验证,无独立参考" — 不声称真实 GPU 周期对齐

---

## 3. v3.0-extract 决策反转清单(显式撤销)

| v3.0-extract 决策 | v0.5 反转 |
|---|---|
| `CPPTLMBRIDGE_VERSION=2` 永久冻结 | **保持冻结**(D7)|
| `libptxemu_device.so` 黑盒集成 | **双路径共存**:黑盒(快速模式)+ 白盒(精确模式)|
| PM4 解析 host | **CppTLM CP 解析**(D5)|
| ComputeUnit 不仿真,12 SM Legacy | **ScoreboardTLM + PipelineTLM 升级**(D6)|
| 9 周 P0-P4 时间线 | **12 周 P0'-P4'**(submodule 工作)|
| v3.0.0 BREAKING bump + 11 项删除 | **全部撤销**(Net-new)|

---

## 4. v0.5 架构关键决策

### 4.1 submodule 集成

```
CppTLM
├── external/PTX-EMU          (git submodule, pin commit)
├── include/tlm/gpu/
│   └── ptx_emu_submodule_v05.hh  (唯一 include PTX-EMU 头)
├── src/tlm/gpu/
│   └── ptx_emu_submodule_v05.cc  (adapter 实现,编译防火墙)
└── include/tlm/gpu/*.hh        (其他头不引用 PTX-EMU)
```

### 4.2 ComputeUnit v2 架构

```
ComputeUnitTLM (ChStreamModuleBase)
├── PtxEmuSubmoduleV05 (adapter pattern,独占 PTX-EMU 头 include)
│    └── 持有 ptxsim::SMContext 实例
├── ScoreboardViewTlm   (仅 ScoreboardTLM 升级,状态视图)
├── PipelineViewTlm      (仅 PipelineTLM 升级,latency 注入)
├── MemoryBridge (现有,per-instruction 内存访问保留)
└── WarpScheduler       (PTX-EMU::WarpScheduler 注入,策略类)
```

### 4.3 CommandProcessor + Pm4Decoder

```
CommandProcessor (CP, 5-state FSM)
├── Pm4Decoder (Mesa-style TYPE3)
├── SubchannelContext[5] (subchannel 0-4, v0.4.1 限定 5 个有效)
├── CommandDispatcher (PM4 opcode → subchannel handler)
└── 替代 v0.4.1 删除的 CP + Pm4Decoder
```

### 4.4 per-warp step API (PTX-EMU 新增)

```cpp
// PTX-EMU 仓新增(per Oracle Option A):
class SMContext {
public:
    // 既有 image_execute 不变(黑盒兼容)
    int image_execute(ImageHandle, ...);
    
    // v0.5 新增(白盒 per-warp step):
    int stepOneWarpInstruction(uint32_t warp_id,     // 输入: 要执行的 warp
                              uint64_t* out_pc,        // 输出: 当前指令 PC
                              int32_t* out_status,     // 输出: 指令完成状态
                              uint64_t* out_cycle_count); // 输出: cycle 计数
};
```

---

## 5. Consequences (后果)

### 5.1 正面

- **指令级精度**: per-warp instruction 暴露 PC + cycle + status,可对标 nvprof
- **双路径共存**: 快速模式(`image_execute`)+ 精确模式(stepOneWarpInstruction)互为对照测试
- **零侵入扩展**: submodule 仅加新 API,既有 PTX-EMU 兼容性零破坏
- **测试基础设施**: 双路径 byte-identical 比较零额外成本

### 5.2 负面

- **永久 fork 风险**: submodule + modify 意味着 PTX-EMU 升级需同步
- **仿真速度损失**: 10x-50x slowdown (per-warp step vs 黑盒)
- **验证无独立参考**: 仅 PTX-EMU 自家 + 双路径内部一致性,不声称真实 GPU 周期对齐
- **构建复杂度**: CppTLM CI 从此继承 PTX-EMU 完整构建链(可能含 ANTLR4 4.13.2)

### 5.3 风险登记

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | PTX-EMU maintainer 拒收新 API | 中 | 高 | 上游 PR 先行,本地 fork 兜底 |
| R3 | fork 长期 merge 冲突 | 中 | 中 | 监控 diff 量,>1000 LOC 评估独立 release |
| R4 | UsrLinuxEmu 需同步调整 | 中 | 中 | HSK-7 cc UsrLinuxEmu |
| R5 | submodule 编译依赖扩散 | 中 | 低 | 符号可见性 -fvisibility=hidden |
| R6 | 验证无独立 golden | 已确认 | 中 | 文档化 "内部一致性验证,无独立参考" |
| R7 | 12 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ P1 推迟其余 |

---

## 6. Acceptance Gates (per proposal.md)

| # | Gate | 状态 |
|---|------|:---:|
| 1 | HSK-7 协议发出 + ack | ⏳ W1 |
| 2 | git submodule add PTX-EMU | ⏳ W1 |
| 3 | ADR-X.16 + openspec change 创建 | ⏳ W1 |
| 4 | CommandProcessor + Pm4Decoder (TYPE3) | ⏳ W2-4 |
| 5 | PtxEmuSubmoduleV05 adapter | ⏳ W2-4 |
| 6 | ComputeUnit v2 adapter pattern | ⏳ W5-7 |
| 7 | ScoreboardTLM + PipelineTLM 升级 production | ⏳ W5-7 |
| 8 | 双路径逐字节 diff 测试 PASS | ⏳ W8-10 |
| 9 | 全部 ≥850 测试通过(v0.5 baseline) | � W11-12 |

---

## 7. References

### 上游决策
- Oracle 重评 `ses_fe691c532ffeF4dACcAd1dNwwY`
- Metis 重评 `ses_fe6915688ffelUG9G8HB1msoaY`

### 仓 HEAD 锚点
```
PTX-EMU    @87820951 (per Oracle 调研,含 HSK-4 + step machinery)
CppTLM     @74c2fd1 (v0.5 proposal commit,v0.5 起点)
UsrLinuxEmu@37a91b6 (ADR-090 v2)
```

### 撤销 ADR
- [`ADR-X.15-cpptlm-v3-dgpu-extract.md`](../../adr/ADR-X.15-cpptlm-v3-dgpu-extract.md) — v3.0-extract 决策(被本 ADR 反转)

### 研究文档(本 ADR 实施参考)
- [`docs/research/CP/`](../../research/CP/) — 12 专利(AMD + NVIDIA Command Processor)
- [`docs/research/WDU/`](../../research/WDU/) — 5 专利(Work Distribution Unit)
- [`docs/research/TMU/`](../../research/TMU/) — 16 文件
- [`docs/research/PCIe/`](../../research/PCIe/) — PCIe 保序 write

### 跨仓 commit 顺序(per ADR-035 §R5.1)
```
PTX-EMU 新 API PR → PTX-EMU submodule pin → CppTLM HSK-7 ack → submodule 添加 → adapter 实施
```

---

**维护**: CppTLM Team (Sisyphus)
**下次 review**: W1 启动时(submodule pin + HSK-7 公告确认后)
**Status Update 触发条件**: PTX-EMU API PR 被拒收(回退 Option A → B)
