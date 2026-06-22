# 未来工作 Roadmap (P2+) — CppTLM

**Status**: Active Planning · **Date**: 2026-06-20 · **Author**: Sisyphus · **Branch**: main
**Scope**: 完整索引 P0 + P1 + P1.5 之后的所有未完成工作, 按优先级/工期/依赖排序, 给执行者一个清晰的"接下来做什么"清单。

---

## 0. 当前状态摘要

| 阶段 | Commit | 状态 | 测试基线 |
|------|--------|:---:|---------|
| **P0** 全套修复 | `fb56cc3` `5abba12` `4964619` `5a964c5` `9746272` `788b207` | ✅ Done | 684/684 |
| **P1** ApuSoC::incorporate_parent | `04399c8` | ✅ Done | 690/690 (+6) |
| **P1.5** GPU cu_template 完整传播 | `e8c2a97` | ✅ Done | 690/690 (peer_count 3→16+) |
| **format cleanup (P1)** | `c3bdb45` | ✅ Done | — |

**E2E**: `[SUCCESS]` · **Format**: ✅ clean · **docs_sync_check**: ✅ 0 missing · **Origin/main**: up to date

**P0+P1+P1.5 合计**: 9 commits, ~2200 LOC 生产代码 + 测试 + 文档

**测试基线验证** (single source of truth):
```bash
./build/bin/cpptlm_tests --reporter compact
# 期望: "All tests passed (N assertions in 690 test cases)"
# 当前 verified: 15330 assertions in 690 test cases (2026-06-22)
# 注: AGENTS.md 写 "659/659" 是过期基线 (predates format cleanup), 本 roadmap 是最新
```

---

## 0.5 依赖图 + 总工期

### 总工期估算 (F1–F15)

| 等级 | Task | 工期 (工作日) | 累计 |
|:---:|------|:---:|:---:|
| 🟢 | F8 cosmetic cleanup | 0.05 | 0.05 |
| 🟢 | F2 pre-commit hook | 0.05 | 0.1 |
| 🟢 | F3 ADR write | 0.05 | 0.15 |
| 🟡 | F11 E2E 回归守护 | 0.5 | 0.65 |
| 🟡 | F1 P2 补测 (5 TEST_CASEs) | 1 | 1.65 |
| 🟡 | F5 cpptlm.library Python | 1.5 | 3.15 |
| 🟡 | F7 params.ports: 8 schema | 0.5 | 3.65 |
| 🔴 | F4 Phase 7.C 6×6 state table | 10 | 13.65 |
| 🔴 | F12 Phase 7.B 三类实现 | 10 | 23.65 |
| 🟡 | F6 blueprint 升级 | 1.5 | 25.15 |
| 🟡 | F13 Phase 7.D TCC Bridge | 10 | 35.15 |
| 🟢 | F14 Phase 7.E Multi-CU+NoC | 7 | 42.15 |
| 🟢 | F15 Phase 7.F Full APU Demo | 5 | 47.15 |
| 🟢 | F9 多 xbar 实例 | 1.5 | 48.65 |
| 🟢 | F10 metric 收集 | 2.5 | 51.15 |

**总工期估算**: ~51 工作日 (~10 周单人, 或 3-4 周 4 人并行)

### 依赖图

```
                   ┌─ F2 (15min) ─┐
                   ├─ F3 (30min) ─┤
                   └─ F1 (1d) ────┤
                                    │
                                    ↓
                  ┌─ F11 (0.5d) ─┐  (E2E 回归守护, 独立)
                  └─ F5 (1.5d) ──┐ │
                                  │ │
       ┌──── F12 (Phase 7.B 三类, 10d) ────┐
       │          (F6 F13 F14 都依赖)        │
       │                                     ↓
       ├─→ F6 (蓝图升级, 1.5d) ─→ F13 (Phase 7.D TCC, 10d) ─┐
       │                                                      ↓
       └──────────────────→ F4 (Phase 7.C state table, 10d) ─┴─→ F14 (Phase 7.E Multi-CU, 7d) ─┐
                                                                                                  ↓
                                                                                            F15 (Phase 7.F Demo, 5d) ─┐
                                                                                                                       ↓
                                                                                                       F9 (多 xbar, 1.5d)  [独立]
                                                                                                       F10 (metric, 2.5d)   [独立]
                                                                                                       F8 (cosmetic, <30min) [独立]
                                                                                                       F7 (schema, 0.5d)    [独立]
```

**关键路径**: F12 → F13 → F14 → F15 (~32 工作日, 阻塞主线 Phase 7 推进)
**快速收益路径**: F2 → F3 → F1 → F11 → F5 (~3.5 工作日, 1 周可完成)

---

## 1. 完整待办索引 (P2+)

> **优先级语义**: 🔴 = 最高风险/最高紧迫性 (与 `roadmap.md` Phase 7 风险分级对齐) · 🟡 = 正常推进 · 🟢 = 按需/cosmetic
> **依赖说明**: 全部依赖均已展开; 隐藏依赖标注在对应 task 详细规范节。

### 1.1 🔴 高优先级 (1-2 周内启动 / 最高风险)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F2** | **pre-commit 集成 clang-format 钩子** (.pre-commit-config.yaml 加 clang-format pass) | P0+P1 final review 建议 | <15min | 低 | 无 |
| **F3** | **写 ADR: incorporate_parent late-binding semantics** (记录 P1 1A+2A+3A 决策 + 软失败 + 双层幂等) | Handoff PENDING §11 | <30min | 低 | 无 |
| **F1** | **P2 阶段 5 项残留 △ 补测** (TpcCluster cu 计数 / routing="FIFO" / l1_size·l2_size 透传 / channel_size 透传 / 性能 metric 断言) | Handoff PENDING §7 | 0.5-1 天 | 中 | 无 |
| **F4** | **Phase 7.C: CoherentXBarTLM 6×6 state table 改造** (取代 write-through 透传, 引入 MOESIF CoherenceState + 状态机) | ADR-SOC-01 §2 | 1-2 周 | **高** | 无 |
| **F11** | **新风险#1: E2E 回归守护** (删 `test_phase6_integration.cc:153-155` 直接驱动 + 新增 "Phase 6: E2E data flow cache→xbar→mem" 测试) | 归档的 p0-alignment-remediation-plan.md | 0.5-1 天 | 中 | 无 |

### 1.2 🟡 中优先级 (1-2 月内启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F5** | **cpptlm.library Python 高级工厂函数** (`cpu_nested_cluster`, `memory_cluster_hierarchical`, `gpu_topology`) | Handoff PENDING §5 | 1-2 天 | 中 | 无 |
| **F7** | **`params.ports: 8` schema 校验** (`apu_soc_v1.json` xbar 写死 8 端口但类硬编码 4 端口, 加 schema validate) | P0 final review 任务 #13 | 0.5 天 | 中 | 无 |
| **F12** | **Phase 7.B 核心: GpuComputeUnitTLM / VectorRegFileTLM / WavefrontTLM 三类实现** (F6 蓝图升级的实际前置) | `roadmap.md` Phase 7.B + ADR-SOC-02/03 | 1-2 周 | 高 | 无 |
| **F6** | **compute_unit_v1.json 蓝图升级** (Phase 7.B 接入 GpuComputeUnitTLM, 扩 ScalarCache + VectorRegFile + Wavefront) | Handoff PENDING §6 | 1-2 天 | 中-高 | **F12** (而非 F4) |
| **F13** | **Phase 7.D: TCC Bridge + 内存层次** (DualPortStreamAdapter write coalescing + MemoryTLM hbm_mode) | `roadmap.md` Phase 7.D + ADR-SOC-04 | 1-2 周 | 高 | F12 |

### 1.3 🟢 低优先级 (按需启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F8** | **文档 cosmetic cleanup** (top_xbar 命名统一 / description v2.2→v2.3 / 命名空间缩进风格 / 红阶段注释清理) | P0 final review 任务 #12, 14, 15, 16 | <30min | 低 | 无 |
| **F14** | **Phase 7.E: Multi-CU + NoC** (ComputeUnitTLM 数组模式 + BidirectionalPortAdapter + apu_soc_v1 完整 GPU 接入) | `roadmap.md` Phase 7.E | 1-2 周 | 中-高 | F12, F4 |
| **F15** | **Phase 7.F: Full APU SoC Demo** (`apu_full_soc.json` + `test_apu_soc.py` + 222/222 Python 端到端) | `roadmap.md` Phase 7.F | 1 周 | 中 | F13, F14 |
| **F9** | **多 xbar 实例支持** (当前单 xbar 设计, Phase 7.C+ 多 xbar 场景) | P1 spec §7 风险表 | 1-2 天 | 中 | F4 |
| **F10** | **CPUTLM / CacheTLM 性能 metric 收集** (Phase 7 末, telemetry framework) | Phase 7 路线图 | 2-3 天 | 中 | 无 |

---

## 2. 推荐执行顺序 (按 ROI 排序)

### Phase A: 1-2 周 quick wins (commit-based 增量交付)

```
F2 (15min) → F3 (30min) → F11 (0.5-1天) → F1 (0.5-1天) → F8 (30min)
   ↓
[5 commits, 覆盖率冲刺, E2E 回归守护建立]
```

| 步骤 | commit | 验收 |
|------|--------|------|
| F2: pre-commit clang-format | `chore: add pre-commit clang-format hook` | `pre-commit run --all-files` 通过 |
| F3: ADR write | `docs(adr): ADR for incorporate_parent late-binding semantics` | `docs/adr/ADR-INC-*.md` 落地 |
| F11: E2E 回归守护 | `test: 新增 Phase 6 E2E data flow cache→xbar→mem 测试 + 删 test_phase6_integration.cc:153-155 直接驱动` | grep 0 直接驱动; E2E 测试覆盖完整路径 |
| F1: P2 补测 | `test: P2 5 项残留 △ 补测` (5 new TEST_CASEs) | 全测 690 → 695/695+ |
| F8: cosmetic cleanup | `docs+test: cosmetic cleanup (top_xbar naming, v2.3 desc)` | grep 0 stale references |

**Phase A 总工期**: 1-2 天, 5 commits

### Phase B: 2-3 周架构演进 + 风险任务 (大块工作, 含 F4 brainstorming)

```
F5 (1.5天) → F7 (0.5天) → [F12 (10d) ∥ F4 (10d)] → F6 (1.5d) [依赖 F12]
   ↓
[Python 用户接口 + schema 校验 + Phase 7.B 核心三类 + Phase 7.C state table + 蓝图升级]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F5: cpptlm.library Python | 无 (独立) | `feat(cpptlm): Python 高级工厂 cpu_nested_cluster + memory_cluster_hierarchical + gpu_topology` |
| F7: schema validate | 无 (独立) | `feat(json): validate params.ports against class max_ports` |
| F12: Phase 7.B 三类 | 无 (但需新 brainstorming) | `feat(tlm): GpuComputeUnitTLM + VectorRegFileTLM + WavefrontTLM (per ADR-SOC-02/03)` |
| F4: Phase 7.C state table | 无 (但需新 brainstorming) | `feat(tlm): CoherentXBarTLM 6×6 state table (per ADR-SOC-01)` |
| F6: blueprint upgrade | F12 (而非 F4) | `feat(templates): upgrade compute_unit_v1.json to GpuComputeUnitTLM (per ADR-SOC-02/03)` |

**Phase B 总工期**: 2-3 周 (含 F12 + F4 并行), 5-6 commits, 含 2 个新 brainstorming cycle (F12 + F4)

### Phase C: 2-3 周 Phase 7.D-E 推进

```
F13 (10d) [依赖 F12] → F14 (7d) [依赖 F12 + F4]
   ↓
[TCC Bridge + 内存层次 + Multi-CU + NoC + BidirectionalPortAdapter]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F13: Phase 7.D TCC | F12 (CU 类需先实现) | `feat(tlm): TCC Bridge + MemoryTLM hbm_mode (per ADR-SOC-04)` |
| F14: Phase 7.E Multi-CU | F12 + F4 | `feat(tlm): ComputeUnitTLM 数组模式 + BidirectionalPortAdapter` |

**Phase C 总工期**: 2-3 周, 2-3 commits

### Phase D: 集成演示 + 按需扩展 (Phase 7.F 收官)

```
F15 (5d) [依赖 F13 + F14] → F9 (1.5d) ∥ F10 (2.5d) ∥ F7 (0.5d) [独立]
   ↓
[Full APU Demo + 多 xbar + metric + schema 校验]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F15: Phase 7.F Demo | F13 + F14 | `feat(demo): apu_full_soc.json + test_apu_soc.py` |
| F9: 多 xbar | F4 (CoherenceDomain) | `feat(simmodule): 多 xbar 实例支持 (P1 spec §7 风险表)` |
| F10: metric 收集 | 无 | `feat(metrics): CPUTLM/CacheTLM/MemoryTLM telemetry` |

**Phase D 总工期**: 1-2 周, 3-4 commits

### 整体规划

**A → B → C → D 全程**: ~10 周 (单人) / 3-4 周 (4 人并行, 关键路径 32 工作日)

---

## 3. 各任务详细规范

### F1: P2 阶段 5 项残留 △ 补测

**来源**: `docs/superpowers/handoffs/2026-06-19-p0-discussion-handoff.md` PENDING TASKS §7

**5 项测试**:

| # | 测试名 | 验证 |
|---|--------|------|
| P2.1 | `TpcCluster internal cu count` | `tpc.getInternalInstance("compute_grp")->getInternalFactory().getAllInstances().size() == cu_per_tpc_` |
| P2.2 | `routing="FIFO" 路径` | 验证 FIFO vs XY 路由选择 |
| P2.3 | `l1_size/l2_size 透传` | 验证 `params.l1_size` 正确传到 l1_0, l1_1; `l2_size` 传到 l2 |
| P2.4 | `channel_size 透传` | 验证 MemoryCluster 通道配置 |
| P2.5 | `性能 metric 断言` | 验证 `metrics_reporter.get_summary()` **结构存在** (顶层字段), 不断言 hit rate 具体值 (F10 负责扩展 metric 类型) |

**文件**: 1 个新 test 文件 + 必要 helper（直接修改现有 test file 也可）

**预期结果**: 690/690 → 695/695+, 覆盖率 92% → 95%+

**风险**: 低（机械补测, 不涉及设计变更）

---

### F2: pre-commit 集成 clang-format 钩子

**来源**: P0 + P1 final review 两次建议 "recommend the contributor integrate format.sh into their pre-commit flow"

**实现**:
- 新建 `.pre-commit-config.yaml`:
  ```yaml
  repos:
    - repo: local
      hooks:
        - id: clang-format
          name: clang-format
          entry: ./scripts/build/format.sh
          language: system
          pass_filenames: false
          always_run: true
  ```
- 文档: `docs/development/CONTRIBUTING.md` 加 "pre-commit setup" 章节（如不存在则建）

**预期结果**: 未来 commit 不会有 format 漂移

**风险**: 极低（一次性配置）

---

### F3: 写 ADR: incorporate_parent late-binding semantics

**来源**: Handoff PENDING §11 "当 incorporate_parent 真实语义确定后, 写 ADR 到 docs/superpowers/specs/"

**实现**: 新建 `docs/adr/ADR-INC-01-incorporate-parent-late-binding.md`, 内容:
- Context: P0 + P1 留下的 wiring pattern, 缺乏 ADR
- Decision: 1A+2A+3A (ModuleFactory Step 9 + 父端全树递归 + GpuCluster 不重写)
- Consequences: 解锁的 wiring path + 已知技术债 + future Phase 7.C+ 扩展点
- Cross-references: `docs-archived/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md`

**风险**: 极低（文档工作, 不涉及代码）

---

### F4: Phase 7.C: CoherentXBarTLM 6×6 state table 改造

**来源**: `docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md` §2 (分步走策略 §2 决定)

**当前状态** (P0): `snoop_broadcast` write-through 透传 (不下场做 coherence 决策)

**目标** (Phase 7.C):
- CacheTLM 引入 `CoherenceState` 标签 (`I` / `S` / `E` / `M` / `O` / `F`)
- CoherentXBarTLM 加 `CoherenceDomain` 集成 + 6×6 state transition switch 表
- snoop 时先读 cache state 再决定是否广播
- 与 gem5 `MOESI_AMD_Base` 协议对齐 (但用 C++ `switch` 而非 slicc DSL)

**预估**:
- 新增 ~500-800 LOC
- 涉及 `CoherenceDomain` 集成 (P0 留 stub), `CacheTLM` 内部状态机, `CoherentXBarTLM` 决策逻辑
- 需新 brainstorming cycle (架构重大变更)
- 测试: 50+ new TEST_CASEs (state transitions 6×6 = 36 cases + snoop scenarios)

**风险**: 中-高（架构变更, 可能影响现有 690 tests）

**前置**: 需先与 ADR-SOC-01 重新对齐确认

---

### F5: cpptlm.library Python 高级工厂函数

**来源**: Handoff PENDING §5

**当前 cpptlm/topo** 已有基础 `TopologyBuilder` API。需扩展:

| # | 函数 | 用途 |
|---|------|------|
| 5.1 | `cpu_nested_cluster(num_cores, cache_config, l2_config)` | 一键生成 2-level CpuCluster JSON |
| 5.2 | `memory_cluster_hierarchical(channels, capacity_per_channel)` | 一键生成多通道 MemoryCluster |
| 5.3 | `gpu_topology(gpc_count, tpc_per_gpc, cu_per_tpc, cu_template)` | 一键生成 4-level GPU (P1.5 后需要支持 cu_template 透传) |

**文件**:
- 新建 `cpptlm/library/` 子包
- `examples/` 加 demo 用法
- 测试: `test/python/test_library_*.py`

**风险**: 低（纯 Python, 不影响 C++ 核心）

---

### F6: compute_unit_v1.json 蓝图升级

**来源**: Handoff PENDING §6

**当前蓝图** (`configs/templates/compute_unit_v1.json`):
- `scalar_cache` (CacheTLM)
- `l1_cache` (CacheTLM)
- 共 2 cache

**升级目标** (Phase 7.B 接入 `GpuComputeUnitTLM`):
- `scalar_cache` → 保留 (CacheTLM 仍可用)
- `vector_regfile` (新 VectorRegFileTLM)
- `wavefront` (新 WavefrontTLM, 含 coalescing)
- `l1_cache` 升级为 `l1_data_cache` + `l1_inst_cache` (Harvard)

**依赖**:
- **F12** (Phase 7.B 三类实现: `GpuComputeUnitTLM` / `VectorRegFileTLM` / `WavefrontTLM`) 是 F6 的实际前置
- ~~`F4` (coherence state table) 不依赖 — F4 是 crossbar 侧, F6 是 CU 蓝图侧, **正交**~~

**风险**: 中（需先实现 3 个新 TLM 类, 见 F12）

---

### F7: `params.ports: 8` schema 校验

**来源**: P0 final review 任务 #13

**问题**: `configs/apu_soc_v1.json` 中 `xbar` 写 `"ports": 8`, 但 `CoherentXBarTLM` 硬编码 4 端口 (继承 CrossbarTLM)。`ports: 8` 字段被 silently ignored。

**实现**:
- **预检步骤** (Day 1): 先 `grep -rn '"ports"\s*:' configs/` 评估 backward compat 影响范围, 列出所有 "ports > n_ports" 的 JSON 文件
- `ModuleFactory::validateConfig` 加 schema validation: 检查 `params.ports` 与类实际 `n_ports` 一致, 不一致抛 `std::runtime_error`
- 或: 加 warning 提示用户参数被忽略 (二选一, 推荐 warning 模式避免 break 现有 config)

**风险**: 低（防御性校验, 但 **必须先 grep configs/** 避免 break 现有 apu_soc_v1.json）

---

### F8: 文档 cosmetic cleanup

**来源**: P0 final review 任务 #12, 14, 15, 16

**子任务**:
- **F8.1**: `top_xbar` vs `xbar` 命名统一 (JSON 是 source of truth — 改 spec/architecture doc 中所有 `top_xbar` 引用)
- **F8.2**: `configs/apu_soc_v1.json:3` description 改 v2.2 → v2.3 (或更新到 v2.4.1)
- **F8.3**: 命名空间缩进风格统一 (`coherent_xbar_tlm.cc:11-12` 特殊缩进)
- **F8.4**: 红阶段注释清理 (test 文件中 "红阶段" 字样 — 可保留作为历史记录)

**风险**: 极低

---

### F9: 多 xbar 实例支持

**来源**: P1 spec §7 风险表

**当前限制**: ApuSoC::incorporate_parent 找单个 xbar（按 `coherent_xbar_name` 配置）

**目标**: 支持 ApuSoC 多个 xbar, 每个 xbar 收集自己的 peer cache 子集

**依赖**:
- F4 (CoherenceDomain 集成) 必须先做
- 需要 new JSON schema: `xbar_instances: [{name, peer_caches: [...]}, ...]`

**风险**: 中-高（架构变更）

---

### F10: CPUTLM / CacheTLM 性能 metric 收集

**来源**: Phase 7 路线图

**当前**: `metrics_reporter` 有 stub 但 CPUTLM / CacheTLM 未实际收集 metric

**目标**:
- CPUTLM: 记录 `transactions_issued`, `avg_response_latency`
- CacheTLM: 记录 `hit_rate`, `miss_rate`, `eviction_count`
- MemoryTLM: 记录 `bandwidth_utilization`
- 整合到 `metrics_reporter::summary()`

**风险**: 中（需 touch 多个 TLM 类）

---

### F11: 新风险#1 E2E 回归守护

**来源**: `docs-archived/plans/p0-alignment-remediation-plan.md` (2026-06-09 状态审计, Oracle 2026-06-22 重新审计)

**背景**:
- 原 plan 列出 4 项 P0 + 2 项新风险
- P0-#1 (PortRole/BundleType 字符串大小写) ✅ 2026-06 落地
- P0-#2 (v3 hybrid 设计归档) ✅ 2026-06-09 落地
- P0-#3 (CrossbarTLM 单指针化) ✅ 2026-06-19 P0 fix 落地 (multi_adapter_ + set_stream_adapter 单指针)
- ~~P0-#4 (Packet::reset release Extension) ✅ **已修复 (Phase 1d) — 见下方 ⚠️ 假阳性说明**~~
- 新风险#1 (test_phase6_integration.cc 直接驱动) ⏳ **仍待实施 (本 F11 范围)**

**⚠️ P0-#4 假阳性说明 (Oracle 2026-06-22 发现)**:
- 原 audit 基于 2026-06-09, 早于 Phase 1d 落地
- `include/tlm/tlm_stub.hh:155-168` 的 `tlm_generic_payload::reset()` **已**在 `Packet::reset()` 调用时, 循环 `delete` + `nullify` 所有 extension
- 文件 L151-154 注释明确: *"Phase 1d: full multi-extension reset semantics. Loop-deletes all owned extensions AND nullifies each slot... After reset(), get_extension<T>() returns nullptr for every T."*
- **roadmap 原提议修复反而会破坏代码**:
  - `payload->release_extension<>()` 会 double-delete (reset 已 delete)
  - `payload->reset_extensions()` API 不存在 (实际是 `clear_extensions()` / `reset()` / `release_extension<T>()`)
- **结论**: P0-#4 无需修复, 已被 Phase 1d 正确实现。已从 F11 移除

**新风险#1 实现**:
- 删 `test/test_phase6_integration.cc:153-155` 中直接驱动 `xbar.req_in[0]` 的代码 (绕过 StreamAdapter 检测)
- 新增 E2E 测试 `"Phase 6: E2E data flow cache→xbar→mem"`, 走 StreamAdapter/MasterPort/SlavePort/PortPair 标准通路
- 测试目的: 在 P0-#3 未修时必失败 (因为直接驱动绕过检测), 修复后必通过

**文件**:
- 修改 `test/test_phase6_integration.cc` (删除 L153-155 直接驱动 ~3 行, 新增 E2E TEST_CASE ~30 行)
- 新建可选 `test/test_packet_extension_release.cc` (~50 行 ASan 验证 Packet::reset 后 extension 已 nullptr)

**验收**:
- `grep "xbar.req_in\[0\]" test/` 返回 0 匹配
- 新 E2E 测试在 P0-#3 未修时**必失败** (证明覆盖有效), 修复后通过
- 690 → 691+ tests pass

**风险**: 中（涉及测试架构改进, 需验证新测试可检测 P0-#3 regression）

---

### F12: Phase 7.B 核心 — GpuComputeUnitTLM / VectorRegFileTLM / WavefrontTLM 三类实现

**来源**: `roadmap.md` Phase 7.B + ADR-SOC-02 (CU 粒度) + ADR-SOC-03 (Wavefront coalescing)

**当前状态** (Phase 7.A):
- `GPUTLM v0` 黑盒发起器 (单端口 Initiator, 周期发出 ComputeReqBundle)
- `ComputeReqBundle` / `ComputeRespBundle` 类型已定义 (4 GPU 维度字段)

**目标** (Phase 7.B):
- `GpuComputeUnitTLM` (新增, ~300 LOC) — 真实 CU 行为: 接收 wavefront → dispatch → SIMT 执行 → 写回
- `VectorRegFileTLM` (新增, ~150 LOC) — vector register file 抽象, 支持 read/write/coalesce
- `WavefrontTLM` (新增, ~200 LOC) — wavefront 调度 + `coalescing_factor` 合并 (per ADR-SOC-03)
- 配套测试: `test/test_gpu_compute_unit.cc` + `test/test_vector_regfile.cc` + `test/test_wavefront.cc` (~30 TEST_CASEs)
- 跨类集成测试: 1 wavefront → 64 lane coalesce → 1 load → CacheTLM (bypass coherence) → MemoryTLM

**依赖**:
- 无外部依赖 (新增类, 不改现有 TLM 接口)
- 需新 brainstorming cycle (3 个新类的设计)
- F6 (compute_unit 蓝图) 依赖此 F12

**预估**:
- 新增 ~650-800 LOC
- 3 个新 .hh + 3 个 .cc + 1 个统一集成测试
- 30+ new TEST_CASEs
- 工期: 1-2 周 (含 brainstorming)

**风险**: 中-高 (3 个新类, 需 careful 接口设计)

**前置**: 需先与 ADR-SOC-02/03 重新对齐确认

---

### F13: Phase 7.D — TCC Bridge + 内存层次

**来源**: `roadmap.md` Phase 7.D + ADR-SOC-04 (HSAPP 极简化)

**目标**:
- `TccTLM` (新增, ~200 LOC) — Translation Cache Coherent Bridge, 真实 GCN 架构参考
- `MemoryTLM::hbm_mode` 扩展 (新增, ~50 LOC 修改) — HBM 带宽模型 + 通道并行
- `DualPortStreamAdapter` 写合并 (write coalescing, ~100 LOC) — 多 request 合并为单 MemoryTLM 写

**依赖**:
- **F12** (CU 类需先实现, F13 是 CU 写回的 downstream)
- 无 brainstorming (架构已在 ADR-SOC-04 中确定)

**预估**:
- 新增 ~350-400 LOC
- 20+ new TEST_CASEs (TCC snoop + write coalescing correctness + HBM 带宽)
- 工期: 1-2 周

**风险**: 中-高 (TCC 状态机 + HBM 模型)

---

### F14: Phase 7.E — Multi-CU + NoC

**来源**: `roadmap.md` Phase 7.E

**目标**:
- `ComputeUnitTLM` 数组模式 (扩展 F12 的 CU) — 1 TPC 容纳 N 个 CU
- `BidirectionalPortAdapter` 真实多 CU 总线 (新增, ~150 LOC)
- `apu_soc_v1.json` 完整 GPU 接入 (4GPC × 2TPC × 2CU = 16 CU, 替换当前的 16 简化 xbar)
- 配套 E2E 测试: GPU 多 CU 并发执行, 验证 NoC 路由 + bandwidth saturation

**依赖**:
- F12 (CU 基础)
- F4 (CoherenceDomain 集成, 多 CU 共享 L2)
- 无 brainstorming (在 F12 基础上扩展)

**预估**:
- 新增 ~400-500 LOC
- 15+ new TEST_CASEs
- 工期: 1-2 周

**风险**: 中 (多 CU 同步 + 真实 NoC 流量)

---

### F15: Phase 7.F — Full APU SoC Demo

**来源**: `roadmap.md` Phase 7.F

**目标**:
- `apu_full_soc.json` (新增) — 完整 APU 拓扑: 2 CPU + 16 CU + 2x2 NoC + DDR4 + 简化 PCI
- `test_apu_soc.py` (新增) — Python E2E 验证脚本: 启动仿真, 注入 CPU + GPU 混合工作负载, 验证 cache coherence + snoop 正确性
- 222 → 230+ Python tests pass
- 演示 demo: `examples/demo_e2e_apu_soc.py` (端到端 CPU+GPU 工作负载)

**依赖**:
- F13 (TCC Bridge)
- F14 (Multi-CU 接入)
- 无 brainstorming (整合已有组件)

**预估**:
- 新增 ~300 LOC (JSON + Python)
- 5+ new Python TEST_CASEs
- 工期: 1 周

**风险**: 低-中 (整合, 主要风险在 F13/F14)

---

---

## 4. 实施模板（每个 task 推荐结构）

每个新 task 推荐按以下 structure (与现有 P0/P1/P1.5 流程一致):

1. **Brainstorming** (新设计 task 必需) — brainstorming skill
2. **Spec** 写 `docs/superpowers/specs/YYYY-MM-DD-<topic>-design.md` — source of truth
3. **Plan** 写 `docs/superpowers/plans/YYYY-MM-DD-<topic>.md` — bite-sized tasks
4. **Implementation** — subagent-driven-development skill
5. **Final review** — verify + clean debug + format + commit + push
6. **Update** 本 roadmap 移除 completed task

---

## 5. ADR 待写清单

| # | ADR | 状态 | 触发条件 |
|---|-----|:---:|---------|
| INC-01 | incorporate_parent late-binding semantics | 🔴 待写 (F3) | 立即 (P1 已落地) |
| 7B-01 | Phase 7.B GpuComputeUnitTLM + VectorRegFileTLM + WavefrontTLM 三类设计 | 🔴 待写 (F12) | F12 启动前 (新 brainstorming 必需) |
| 7C-01 | Phase 7.C 6×6 state table 决策 | 🔴 待写 (F4) | F4 启动前 (新 brainstorming 必需) |
| 7D-01 | Phase 7.D TCC Bridge + HBM 内存模型决策 | 🟡 待写 (F13) | F13 启动前 |
| 7E-01 | Phase 7.E Multi-CU + NoC 架构 | 🟡 待写 (F14) | F14 启动前 |
| 7F-01 | Phase 7.F Full APU Demo 集成策略 | 🟢 待写 (F15) | F15 启动前 |
| LIB-01 | cpptlm.library Python API 决策 | 🟡 待写 (F5) | F5 启动前 |
| GPU-01 | GpuComputeUnitTLM + 蓝图升级决策 | 🟡 待写 (F6) | F6 启动前 |
| METRIC-01 | CPUTLM/CacheTLM/MemoryTLM 性能 metric 收集框架 | 🟢 待写 (F10) | F10 启动前 |
| MULTI-XBAR-01 | 多 xbar 实例架构 | 🟢 待写 (F9) | F9 启动前 |

新 ADR 应在 `docs/adr/ADR-<PREFIX>-<NN>-<topic>.md` 落地, 索引在 `docs/adr/README.md` 更新。

---

## 6. 维护规则

1. **状态更新**: 每完成 1 task, 从本 roadmap 移除 (移至"已完成历史"区)
2. **新增发现**: 在 PENDING 阶段发现的 follow-up task, 加到对应优先级表
3. **优先级回顾**: 每季度 review 一次, 根据业务需要调整优先级
4. **依赖追踪**: 任务之间依赖必须明确标出, 避免后续冲突
5. **历史归档**: 完成 6+ 月的 task 可归档到 `docs-archived/future-work/`

---

## 7. 已完成历史 (从本 roadmap 移除后归档)

| 任务 | Commit | 完成日期 | 备注 |
|------|--------|----------|------|
| P0 全套修复 | `fb56cc3` `5abba12` `4964619` `5a964c5` `9746272` `788b207` | 2026-06-19 | D.1 + CoherentXBarTLM + dead code |
| P1 ApuSoC::incorporate_parent | `04399c8` | 2026-06-19 | 4 phase + 1 bug fix + 6 tests |
| P1.5 GPU cu_template 完整传播 | `e8c2a97` | 2026-06-19 | wrap_template_as_module + TpcCluster |
| format cleanup (P1) | `c3bdb45` | 2026-06-19 | clang-format 4 P1 files |

---

**Roadmap 维护**: CppTLM 开发团队
**下次 review**: 2026-09-20 (Q3 末)
**触发 update 条件**: 任何 task 完成 / 新 task 发现 / 优先级调整