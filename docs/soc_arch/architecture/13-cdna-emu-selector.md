# 选型建议：CDNA-EMU 后端实现方案（阶段 C 启动前参考）

> **版本**: v1.0-draft (2027-02-09)
> **状态**: 📋 Proposed — 阶段 C 启动前决策依据
> **作者**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../adr/ADR-SOC-15-cdna-real-isa-roadmap.md) §4.3 阶段 C（CDNA 引擎接入）
> **关联设计**: [`architecture/11-cdna-real-isa-integration.md`](../architecture/11-cdna-real-isa-integration.md) §11.3 CDNA 微架构适配
> **关联调研**:
> - [`docs/research/2026-09-04-mgpusim-extraction-feasibility.md`](../../research/2026-09-04-mgpusim-extraction-feasibility.md)
> - [`docs/research/2026-09-04-llvm-mc-gfx942-feasibility.md`](../../research/2026-09-04-llvm-mc-gfx942-feasibility.md)
> - [`docs/research/2026-09-04-self-cdna3-emulator-feasibility.md`](../../research/2026-09-04-self-cdna3-emulator-feasibility.md)
> **目的**: 综合 3 个并行调研结论，给出 CDNA-EMU 后端实现的最终选型建议，供 Oracle 评审与阶段 C 启动决策。

---

## 13.1 选型结论

### 13.1.1 最终推荐：**方案 A 自研最小子集 + LLVM MC 混合方案**

**理由**：

1. **MGPUSim 不可行（Go 异构 + Akita 事件驱动）**：3 个独立调研**一致**否决
2. **LLVM MC 可作为解码引擎，但需自配执行器**：纯 decoder，无 execute 逻辑
3. **自研 155 条核心指令最小子集**：16-26 人天，覆盖 80% HPC/AI 工作负载

**最终架构**（双层）：

```
┌──────────────────────────────────────────┐
│  LLVM MC (Decoder)  ── MCInst 抽象 ──┐   │
└──────────────────────────────────────────┘
                                        │
┌──────────────────────────────────────────┐
│  自研最小 CDNA3 子集 (Executor)     │   │
│  - 155 指令 (SALU/VALU/MFMA/flat)  │   │
│  - 64-bit EXEC mask + VGPR/SGPR   │   │
│  - s_waitcnt (vmcnt/lgkmcnt)       │   │
│  - 机器可读 ISA XML 自动生成表      │   │
└──────────────────────────────────────────┘
                                        ▼
                       ┌────────────────────────────┐
                       │  CppTLM IComputeDevice    │
                       │  + IMemoryPort (阶段B)   │
                       └────────────────────────────┘
```

### 13.1.2 三方案对比矩阵

| 维度 | 自研最小子集 + LLVM MC（推荐） | MGPUSim 提取 | 纯 LLVM MC |
|------|--------------------------------|--------------|-----------|
| **覆盖率** | ~80% (155 指令) | 100% (774 指令) | 100% (decode only) |
| **执行能力** | ✅ 完整 | ✅ 完整 | ❌ 需自行实现执行 |
| **CppTLM 集成模型** | tick-driven 完美匹配 | ❌ Akita event-driven 不兼容 | ✅ 完美匹配 |
| **语言** | C++ (自研) + C++ (LLVM MC) | ❌ Go ↔ C++ FFI | ✅ C++ 原生 |
| **开发时间** | 16-26 人天 + 3-5 人天（LLVM MC 封装） | 5-8 人天理解 + 10-15 人天适配 | 仅 3-5 人天（但执行器需自配） |
| **MFMA 支持** | 20 指令核心子集 | ⚠️ 完整但精度 16.4% WMAPE | ✅ 完整解码表 |
| **s_waitcnt 建模** | 需手写 3-5 人天 | ✅ 已建模 | 需手写 |
| **依赖** | LLVM 15+ 库 (~200-300MB) | Go toolchain | LLVM 15+ 库 |
| **维护负担** | 自主 | ❌ Go↔C++ FFI 持续维护 | LLVM 升级耦合 |
| **总成本** | **19-31 人天** | 15-23 人天 + FFI 长期维护 | 16-26 人天 + 执行器开发 |

**为什么选择混合方案而非纯 MGPUSim**：
- 15-23 人天 MGPUSim 提取 + FFI 长期维护债 vs 19-31 人天混合方案（一次性）
- 混合方案保持 CppTLM Clean Room 原则 + 自主架构演进
- 混合方案不引入 Go runtime（避免 GC + goroutine 与 CppTLM SystemC stub 冲突）

**为什么不选纯 LLVM MC**：
- LLVM MC 只提供 decode，**不提供 execute**
- 等于"用 LLVM MC 解码 + 自研执行器"，与混合方案等价
- 但 LLVM MC 的 decode 表已写好，直接用

### 13.1.3 决策路径

```
阶段 C 启动时（待阶段 B Gate 通过）：
├─ 阶段 C.1（3-5 人天）:
│   └─ 封装 LLVM MCDisassembler 为 CppTLM 的 CdnaInstDecoder
│       - 输入: GFX942 ELF / 裸字节流
│       - 输出: 标准化 MCInst → CppTLM InstrDescriptor
│
├─ 阶段 C.2（10-15 人天）:
│   └─ 自研 155 条最小子集执行器（CdnaEmuExecutor）
│       - 输入: InstrDescriptor
│       - 输出: register file + s_waitcnt counter update
│       - 测试: 与 tinygrad AMD emulator 对拍（±0.1% 精度）
│
├─ 阶段 C.3（3-5 人天）:
│   └─ 集成 IMemoryPort（来自阶段 B）
│       - flat_load/store → IMemoryPort::send_request
│       - 响应回调 → s_waitcnt decrement
│
├─ 阶段 C.4（3-5 人天）:
│   └─ 集成 IComputeDevice 接口
│       - CdnaEmuDevice : IComputeDevice
│       - exe_once() 驱动 1 cycle 推进
│
└─ 阶段 C Gate（Oracle 评审 + 全量回归）:
    - [pcie]/[axi]/[e2e]/[wave2] 100% 保持基线
    - 新增 [cdna-phase-c] 测试 50+ 用例
    - SGEMM kernel 通过 CppTLM tick 仿真
```

---

## 13.2 关键决策点

### 13.2.1 为什么不直接用 LLVM MC decode + 完全自研执行

**自研执行**已经包含**完整指令实现**（含 155 条最小子集）。

如果用 LLVM MC 做 decode，每条指令的解码表都已存在。但**解码 ≠ 执行**：
- LLVM MC 不知道 v_add_f32 如何操作 VGPR
- LLVM MC 不知道 v_mfma_f32_16x16x16_fp16 如何写 ACCVGPR
- LLVM MC 不知道 s_waitcnt 如何跟踪 vmcnt 计数器

**结论**：使用 LLVM MC 做 decode 可以**节省 3-5 人天**（避免手写 7 个解码表的位域逻辑），但**执行必须自研**。

### 13.2.2 与 PTX-EMU 的对称性

当前 PTX-EMU 也是"ANTLR4 decode + 自研执行"。混合方案延续此对称性：

| ISA | Decode | Execute | Timing |
|-----|--------|---------|--------|
| PTX (当前) | ANTLR4 | PTX-EMU SMContext::exe_once() | CppTLM 注入 + 系统级 |
| CDNA3 (阶段 C) | LLVM MCDisassembler | 自研 CdnaEmuExecutor | CppTLM 注入 + 系统级 |

**架构一致性**：两端都是 `IComputeDevice` 派生类，CppTLM `tick()` 通过同一接口驱动。

### 13.2.3 自研子集的具体范围

**必须实现（155 条核心指令）**：

| 类别 | 数量 | 关键指令 |
|------|------|----------|
| **核心 SALU** | 40 | s_mov_b32, s_add_u32, s_sub_u32, s_mul_i32, s_and_b32, s_or_b32, s_cmpk_eq_u32, s_waitcnt, s_branch, s_cbranch_* |
| **核心 VALU** | 80 | v_add_f32, v_mul_f32, v_fma_f32, v_sub_f32, v_max_f32, v_min_f32, v_cmp_*, v_cvt_*, v_mov_b32, v_add_u32 |
| **核心 MFMA** | 20 | v_mfma_f32_16x16x16_fp16, v_mfma_f32_32x32x8_fp16, v_mfma_f32_16x16x32_bf16, v_mfma_i32_16x16x32_i8 |
| **核心 flat** | 15 | flat_load_dword, flat_store_dword, flat_load_b32, flat_store_b32, flat_atomic_* |

**推迟到阶段 D+**（不在阶段 C 范围）：
- 完整 SALU/VALU (~400 条)
- 完整 MFMA (64+ 变体)
- DS、LDS、SMEM 全部变体
- FMA/F16 全部变体
- 特化指令 (image、atomic 等)

### 13.2.4 测试策略

**3 层验证**（per ADR-SOC-15 §4.3 Gate C）：

1. **单指令精度对拍**（每条指令 vs tinygrad AMD emulator）：
   - 50 条 MFMA + 100 条 VALU + 5 条 SALU 关键指令
   - 容差：±0.1%（FP 算术）+ 完全一致（整数）
2. **microbenchmark vs MGPUSim**（per §12 校准基线）：
   - M1 VALU, M2 MFMA, M4 waitcnt 3 类基准
   - 容差：±15%（per ADR-SOC-15 Gate D）
3. **端到端 SGEMM kernel**：
   - SGEMM kernel 通过 CppTLM tick 仿真
   - 输出矩阵数值 vs 真机或 MGPUSim ±0.1%

---

## 13.3 风险与缓解

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | LLVM MC 库体积过大（~200-300MB 静态库） | 🟡 中 | 阶段 C 启动时评估是否仅链接 `libLLVMMCDisassembler.a` + `libLLVMAMDGPUDesc.a`（不链接 Optimizer/Backend），预期体积可降到 ~50MB |
| **R2** | LLVM MC 版本耦合（LLVM 15/19/24 API 差异） | 🟡 中 | 锁定 LLVM minor 版本（如 LLVM 19.x），通过 `find_package(LLVM 19 REQUIRED CONFIG)` 强制 |
| **R3** | 155 条最小子集不足（某些 AI kernel 需要未实现指令） | 🟡 中 | 阶段 D 启动时扩面；阶段 C Gate 时审视所有失败测试用例 |
| **R4** | s_waitcnt 计数器建模复杂度超预期 | 🟡 中 | 参考 MGPUSim 实现 + tinygrad pcode executor；Oracle Gate C 重点审查 |
| **R5** | ACCVGPR 累加器语义错误（MFMA 累加器与 VGPR 分离） | 🟡 中 | 单元测试 50+ MFMA 累加 vs tinygrad 对拍 |
| **R6** | EXEC mask 64-bit 操作错误（CDNA3 特有 vs RDNA3 32-bit） | 🟢 低 | 严格对照 ISA manual；类型系统强制 `uint64_t` |
| **R7** | IMemoryPort 集成与阶段 B 冲突 | 🟢 低 | 阶段 B Gate 通过前不启动阶段 C 实施 |

---

## 13.4 与 ADR-SOC-15 章节对齐

| ADR-SOC-15 章节 | 本选型建议 |
|----------------|------------|
| §3 D3 "CDNA3 (GFX942) 第一目标" | ✅ 维持 |
| §3 D3 "自研最小子集 + AMD 官方 ISA 手册手写 decode" | ✅ 采纳但叠加 LLVM MC decoder 优化 |
| §3 D3 "LLVM MC for AMDGPU / GFX942 decode" | ❌ 部分采纳（decode 部分采纳，execute 不采纳） |
| §4.3 阶段 C "MGPUSim 剥离 / LLVM MC / 自研最小子集" | ✅ 推荐自研+LLVM MC 混合 |
| §4.3 阶段 C "总工作量 15-30 人天" | ✅ 本方案 19-31 人天，符合上限 |

**对 ADR-SOC-15 的唯一修订建议**：在 §3 D3 增加子决策 "推荐方案：自研执行 + LLVM MC decode 混合方案（per `architecture/13-cdna-emu-selector.md`）"。

---

## 13.5 验收门禁（与 ADR-SOC-15 Gate C 对齐）

✅ **阶段 C Gate 必交付物**：
1. LLVM MC 封装模块（`CdnaInstDecoder`，~500 LOC）
2. 自研 CDNA3 最小子集执行器（`CdnaEmuExecutor`，~2000-3000 LOC）
3. `CdnaEmuDevice : IComputeDevice` 工厂类
4. 单元测试 50+ 用例（含 MFMA 累加 vs tinygrad 对拍）
5. SGEMM kernel 端到端测试通过 CppTLM tick 仿真

✅ **Gate C 评审**：
- Oracle 评审应用 `task(subagent_type="oracle", ...)` 验证执行精度
- 用户评审 SGEMM kernel 输出
- 校准报告归档到 `docs/validation/2027-02-XX-cdna-phase-c-validation.md`

---

## 13.6 参考文献

| 调研 | 链接 |
|------|------|
| MGPUSim 提取可行性 | `docs/research/2026-09-04-mgpusim-extraction-feasibility.md` |
| LLVM MC GFX942 可行性 | `docs/research/2026-09-04-llvm-mc-gfx942-feasibility.md` |
| 自研 CDNA3 最小子集可行性 | `docs/research/2026-09-04-self-cdna3-emulator-feasibility.md` |

| 资料 | URL |
|------|-----|
| AMD CDNA3 ISA 手册 (PDF) | `https://www.amd.com/content/dam/amd/en/documents/instinct-tech-docs/instruction-set-architectures/amd-instinct-mi300-cdna3-instruction-set-architecture.pdf` |
| AMD 机器可读 ISA XML | `https://gpuopen.com/machine-readable-isa/` |
| LLVM AMDGPU 后端 | `https://github.com/llvm/llvm-project/tree/main/llvm/lib/Target/AMDGPU` |
| tinygrad AMD emulator (对拍参考) | `https://github.com/tinygrad/tinygrad/blob/master/test/mockgpu/amd/emu.py` |
| MGPUSim v5.0 (校准基线) | `https://github.com/sarchlab/mgpusim` |

| OpenSpec | 状态 |
|---------|------|
| `cpptlm-dgpu-d1-cdna-isa-phase-a` | ✅ Started (e607814) |
| `cpptlm-dgpu-d1-cdna-isa-phase-c` | 📋 待阶段 B Gate 后启动 |
| `cpptlm-dgpu-d1-cdna-isa-phase-d` | 📋 待阶段 C Gate 后启动 |