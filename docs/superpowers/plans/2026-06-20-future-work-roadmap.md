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

---

## 1. 完整待办索引 (P2+)

### 1.1 🔴 高优先级 (建议 1-2 周内启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F1** | **P2 阶段 5 项残留 △ 补测** (TpcCluster cu 计数 / routing="FIFO" / l1_size·l2_size 透传 / channel_size 透传 / 性能 metric) | Handoff PENDING §7 | 0.5-1 天 | 中 | 无 |
| **F2** | **pre-commit 集成 clang-format 钩子** (.pre-commit-config.yaml 加 clang-format pass) | P0+P1 final review 建议 | <15min | 低 | 无 |
| **F3** | **写 ADR: incorporate_parent late-binding semantics** (记录 P1 1A+2A+3A 决策 + 软失败 + 双层幂等) | Handoff PENDING §11 | <30min | 低 | 无 |

### 1.2 🟡 中优先级 (建议 1-2 月内启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F4** | **Phase 7.C: CoherentXBarTLM 6×6 state table 改造** (取代 write-through 透传, 引入 CoherenceState + 状态机) | ADR-SOC-01 §2 | 1-2 周 | 高 | 无 |
| **F5** | **cpptlm.library Python 高级工厂函数** (`cpu_nested_cluster`, `memory_cluster_hierarchical`) | Handoff PENDING §5 | 1-2 天 | 中 | 无 |
| **F6** | **compute_unit_v1.json 蓝图升级** (Phase 7.B 接入 GpuComputeUnitTLM, 扩 ScalarCache + VectorRegFile + Wavefront) | Handoff PENDING §6 | 1-2 天 | 中-高 | F4 部分 |
| **F7** | **`params.ports: 8` schema 校验** (ap u_soc_v1.json 中 xbar 写死 8 端口但类硬编码 4 端口, 加 schema validate) | P0 final review 任务 #13 | 0.5 天 | 中 | 无 |

### 1.3 🟢 低优先级 (按需启动)

| # | 任务 | 来源 | 工期 | 难度 | 依赖 |
|---|------|------|:---:|:---:|:---:|
| **F8** | **文档 cosmetic cleanup** (top_xbar 命名统一 / description v2.2→v2.3 / 命名空间缩进风格 / 红阶段注释清理) | P0 final review 任务 #12, 14, 15, 16 | <30min | 低 | 无 |
| **F9** | **多 xbar 实例支持** (当前单 xbar 设计, Phase 7.C+ 多 xbar 场景) | P1 spec §7 风险表 | 1-2 天 | 中 | F4 |
| **F10** | **CPUTLM / CacheTLM 性能 metric 收集** (Phase 7 末, telemetry framework) | Phase 7 路线图 | 2-3 天 | 中 | 无 |
| **F11** | **Packet::reset() 显式 release Extension** (P0-#4 + 新风险#1: 修复 Extension 泄漏 + 增 E2E data flow cache→xbar→mem 测试) | 归档的 p0-alignment-remediation-plan.md | 1 天 | 中 | 无 |

---

## 2. 推荐执行顺序 (按 ROI 排序)

### Phase A: 1-2 周 quick wins (commit-based 增量交付)

```
F2 (15min) → F3 (30min) → F1 (0.5-1天) → F8 (30min)
   ↓
[端到端测试覆盖率冲刺 92% → 95%+, 4 commits]
```

| 步骤 | commit | 验收 |
|------|--------|------|
| F2: pre-commit clang-format | `chore: add pre-commit clang-format hook` | `pre-commit run --all-files` 通过 |
| F3: ADR write | `docs(adr): ADR for incorporate_parent late-binding semantics` | `docs/adr/ADR-INC-*.md` 落地 |
| F1: P2 补测 | `test: P2 5 项残留 △ 补测` (5 new TEST_CASEs) | 全测 690 → 695/695+, 覆盖率 92% → 95%+ |
| F8: cosmetic cleanup | `docs+test: cosmetic cleanup (top_xbar naming, v2.3 desc)` | grep 0 stale references |

**Phase A 总工期**: 1-2 天, 4 commits

### Phase B: 2-3 周架构演进 (大块工作)

```
F5 (1-2天) → F7 (0.5天) → F4 (1-2周) → F6 (1-2天) [F4 后期并行]
   ↓
[Python 用户接口 + 架构验证 + Phase 7.C 6×6 state table + 蓝图升级]
```

| 步骤 | 依赖 | commit pattern |
|------|------|----------------|
| F5: cpptlm.library Python | 无 (独立) | `feat(cpptlm): Python 高级工厂 cpu_nested_cluster + memory_cluster_hierarchical` |
| F7: schema validate | 无 (独立) | `feat(json): validate params.ports against class max_ports` |
| F4: Phase 7.C state table | 无 (但需新 brainstorming) | `feat(tlm): CoherentXBarTLM 6×6 state table (per ADR-SOC-01)` |
| F6: blueprint upgrade | 部分依赖 F4 (cache state 与 GpuComputeUnitTLM 集成) | `feat(templates): upgrade compute_unit_v1.json to GpuComputeUnitTLM` |

**Phase B 总工期**: 2-3 周, 4-5 commits, 含 1 个新 brainstorming cycle (F4)

### Phase C: 按需扩展 (Phase 7.C+ 之后)

```
F9 (1-2天) → F10 (2-3天)
   ↓
[多 xbar + telemetry — 长期演进]
```

**Phase C**: 仅在 F4 完成且 SoC 实际有 > 1 xbar 需求时启动

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
| P2.5 | `性能 metric 断言` | 验证 `metrics_reporter.get_summary()` 包含 cache hit rate / avg latency |

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
- `GpuComputeUnitTLM` 类需先实现 (Phase 7.B 独立 workstream)
- `VectorRegFileTLM` 和 `WavefrontTLM` 需先实现

**风险**: 中（需先实现 3 个新 TLM 类）

---

### F7: `params.ports: 8` schema 校验

**来源**: P0 final review 任务 #13

**问题**: `configs/apu_soc_v1.json` 中 `xbar` 写 `"ports": 8`, 但 `CoherentXBarTLM` 硬编码 4 端口 (继承 CrossbarTLM)。`ports: 8` 字段被 silently ignored。

**实现**:
- `ModuleFactory::validateConfig` 加 schema validation: 检查 `params.ports` 与类实际 `n_ports` 一致, 不一致抛 `std::runtime_error`
- 或: 加 warning 提示用户参数被忽略

**风险**: 低（防御性校验, 可能让某些合法配置 fail — 需小心 backward compat）

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

### F11: Packet::reset() 显式 release Extension (P0-#4 + 新风险#1)

**来源**: `docs-archived/plans/p0-alignment-remediation-plan.md` (2026-06-09 状态审计)

**背景**:
- 原 plan 列出 4 项 P0 + 2 项新风险
- P0-#1 (PortRole/BundleType 字符串大小写) ✅ 2026-06 落地
- P0-#2 (v3 hybrid 设计归档) ✅ 2026-06-09 落地
- P0-#3 (CrossbarTLM 单指针化) ✅ 2026-06-19 P0 fix 落地 (multi_adapter_ + set_stream_adapter 单指针)
- P0-#4 (Packet::reset release Extension) ⏳ **仍待实施**
- 新风险#1 (test_phase6_integration.cc 直接驱动) ⏳ **仍待实施**

**P0-#4 实现**:
- `include/core/packet.hh` Packet::reset() 加 `payload->release_extension<>()` 调用
- 验证扩展 release 后内存不增长 (ASan 测试)
- 或：改为 `payload->reset_extensions()` (tlm_generic_payload 提供)

**新风险#1 实现**:
- 删 `test_phase6_integration.cc` 中直接驱动 `xbar.req_in[0]` 的代码
- 新增 E2E 测试 `"Phase 6: E2E data flow cache→xbar→mem"`, 走 StreamAdapter/MasterPort/SlavePort/PortPair 标准通路
- 测试目的: 在 P0-#3 未修时必失败 (因为直接驱动绕过检测), 修复后必通过

**文件**:
- 修改 `include/core/packet.hh` (reset 方法 ~5 行)
- 修改 `test/test_phase6_integration.cc` (删除直接驱动, 新增 E2E 测试 ~30 行)
- 或新建 `test/test_packet_extension_release.cc` (~50 行 ASan 测试)

**风险**: 中（涉及内存管理, 需 ASan 验证）

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
| 7C-01 | Phase 7.C 6×6 state table 决策 | 🟡 待写 (F4) | F4 启动前 |
| LIB-01 | cpptlm.library Python API 决策 | 🟡 待写 (F5) | F5 启动前 |
| GPU-01 | GpuComputeUnitTLM + 蓝图升级决策 | 🟡 待写 (F6) | F6 启动前 |
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