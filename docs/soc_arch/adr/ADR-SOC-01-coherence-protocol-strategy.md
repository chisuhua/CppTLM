# ADR-SOC-01: 一致性协议分步走策略（Coherence Protocol Stepwise Strategy）

> **状态**: ✅ 已确认
> **日期**: 2026-06-14
> **影响**: `CacheTLM` 演进路径（Phase 7.A → 7.C），`CoherenceDomain` 集成
> **类别**: SoC 架构 / 一致性协议

---

## 1. 背景

Phase 7 APU SoC 需要 CPU 与 GPU 共享 `CoherenceDomain` 与 `CacheTLM` 层次。gem5 参考实现（`src/mem/protocol/MOESI_AMD_Base.slicc` 等）使用 MOESI 全状态 + GPU_VIPER 扩展 + TCC 子状态，状态机规模约 5000+ 行 slicc DSL 代码。

可选策略：

| 策略 | 描述 | 工作量 | 灵活性 |
|------|------|--------|--------|
| (A) 简化状态机 | `I→S→M→I` 三态 + write-through 直写（GPU 不缓存本地副本） | Low | 低 |
| (B) 完整 MOESI | `I/S/E/M/O` 五态 + TCP/TCC/SQC 子状态 + GPU_VIPER | High | 高 |

---

## 2. 决策

✅ **采用 (A)→(B) 分步走策略**。

- **Phase 7.A–7.B**（黑盒 GPU 阶段）：GPU 请求走 `write-through` 直写策略，CacheTLM 不需要 protocol-aware 改造。GPU 请求不缓存在 L1/L2，直接穿透至 MemoryTLM。
- **Phase 7.C**（Coherence 集成阶段）：将 CacheTLM 升级为 protocol-aware，引入 6 状态 × 6 事件转换表（`uint8_t state_transition[6][6]`）。`CoherenceDomain` 与 CacheTLM 通过 snoop callback 集成。
- **永不复制 gem5 slicc DSL**：slicc 是 gem5 特定语言、5000+ 行不可读。CppTLM 用 C++ `switch` 表驱动状态转换。

---

## 3. 实施

| 阶段 | 任务 | 验收 |
|------|------|------|
| 7.A | GPU 请求临时 bypass CacheTLM（write-through） | `cpptlm_tests "[gpu]"` 通过 |
| 7.B | ComputeUnitTLM + CrossbarTLM GPU 路由；CacheTLM 临时 bypass | `configs/apu_demo_v1.json` 端到端 |
| 7.C | CacheTLM 引入 `CoherenceState` 标签 + 6×6 state transition switch 表 | `cpptlm_tests "[coherence][gpu]"` 跨 cache 一致性 |
| 7.C | `CoherenceDomain::lookup_home_node()` 集成 snoop fanout | `configs/apu_demo_v2.json` |

---

## 4. 风险与缓解

- **风险**: Phase 7.C 改造 CacheTLM 导致 5+ 测试回归。
- **缓解**: 改造分两小步——先加 `CoherenceState` 标签（不做 snoop），再加 snoop callback。每步独立单元测试。
- **触发升级条件**: 若 Phase 7.C 失败 → 暂停 coherence 方案，回退到 Phase 7.B 的 write-through bypass 策略。

---

## 5. 参考文献

- 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §3 D1（L602-615）
- 复述: [`roadmap.md`](../../../roadmap.md) L63 D1 行
- 微架构: [`docs/soc_arch/modules/coherence-protocol.md`](../modules/coherence-protocol.md), [`coherence-bridge.md`](../modules/coherence-bridge.md)
- SoC 集成: [`docs/soc_arch/specs/apu-soc-design.md`](../specs/apu-soc-design.md)
- 蓝图参考: gem5 `src/mem/protocol/MOESI_AMD_Base.slicc`, `src/mem/protocol/GPU_VIPER.slicc`
