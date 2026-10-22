---
id: phase-10
kind: phase
status: planning
phase_refs:
  - phase-7
  - phase-8
  - phase-9
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
---

## phase-10: Coherence Exploration Platform(CEP)

> **真源 1 (基础)**: [`docs/architecture/06-inter-module-network.md`](../../../docs/architecture/06-inter-module-network.md) v0.4(仿真层 L1-L6 正交,非 CEP 6 层)
> **真源 1 (基础)**: [`docs/architecture/06-bis-coherence-as-network-extension.md`](../../../docs/architecture/06-bis-coherence-as-network-extension.md) v0.3(将被 supersede by CEP,顶部加 Status Update 段)
> **真源 1 (新,✅ 已拍板 α + 已创建)**: [`docs/architecture/07-coherence-exploration-platform.md`](../../../docs/architecture/07-coherence-exploration-platform.md) v0.1(657 行,2026-10-20~21)
> **真源 3**: **21 份** CEP ADR(per `ADR-CEP-NUMBER-MAP §2 Decision 1`;2026-10-21 architect 阶段两批新增 10 份):
> - 治理:`ADR-CEP-00-platform-charter-and-governance.md` v0.6
> - 真源映射:`ADR-CEP-00A-real-source-anchor-map.md` v0.6
> - 架构基座:6 层 + 旁路平面 + canonical 10 流 + 内部接口 + Stub Cache + 实验 + 仿真 seam + 编号预分配 = `ADR-CEP-01~07` + `NUMBER-MAP`
> - **排序与组合契约(2026-10-21 第一批)**:`ADR-CEP-08` Ordering Point 可迁移性 / `ADR-CEP-09` 多 VC 仲裁 + 死锁自由性论证 / `ADR-CEP-12` 能力映射表 schema / `ADR-CEP-13` 一致性域归属
> - **协议族与路线**:`ADR-CEP-17` 协议族接入 + 组合承诺边界 / `ADR-CEP-18` 协议生成路线范围
> - **对齐与治理(2026-10-21 第二批)**:`ADR-CEP-11` 协议版本锁定 / `ADR-CEP-14` Stub 合同测试 / `ADR-CEP-15` CacheTLM 对齐 / `ADR-CEP-16` `06 §15.4` 演进映射 / `ADR-CEP-07A` 06-bis 产物处置

### 当前活跃 (Active)— Stage 0 Umbrella

| 子任务 | 状态 | 范围 | 估时 | 关联 ADR | KPI slot |
|--------|------|------|------|----------|----------|
| **10.A 真源 ADR 起草与评审** | ✅ 完成(2026-10-19~21) | **21 份 ADR**(原 10 份 + 2026-10-21 两批新增 08/09/12/13/18 + 11/14/15/16/07A,修订 02/04/05/06/07/17) | 12 人日(architect) | ADR-CEP-00~18 全族 + NUM | 0(doc-only 起草)|
| **10.B OpenSpec `cpptlm-cep-umbrella` 起草** | 🟡 **Proposed(2026-10-21 已创建,`openspec validate --strict` PASS)** | proposal + design + specs + tasks **4 件套已落**(95 个 task) | 5 人日(builder) | improvement `cpptlm-cep-umbrella.md` | **1**(Stage 0 计 KPI,per oracle v0.4 B2-1 修订)|
| **10.C 实验输出 4 件套契约测试** | 📋 Planned | `test_cep_canonical_invariants.cc` + `test_cep_dummy_signals.cc` + `test_cep_module_factory_seam.cc` + `test_cep_simulation_mode_lock.cc` | 2 周(builder) | ADR-CEP-06 §2 + ADR-CEP-07 §2 | 0(沿用 10.B)|
| **10.D `experimental/` 目录 + tools** | 📋 Planned | `tools/render_report.py` + `run_sweep.py` + `ab_compare.py` + `metrics.json` schema | 1 周(builder) | ADR-CEP-06 §4 | 0(沿用 10.B)|
| **10.E `07-coherence-exploration-platform.md` 顶层真源** | ✅ **完成(2026-10-20~21)** | 整合 **21 份** ADR 为可读真源 1(657 行) | 5 人日(architect,已消耗) | ADR-CEP-00 §5.2 | 0 |
| **10.F `scripts/test/cep_docs_sync_check.sh` ADR 锚点 lint** | 📋 Planned | 真源章节号机器检查(覆盖 **16 份** ADR) | 6 人日(原 5,随 ADR 数量上调) | ADR-CEP-00A §5.1 | 0 |
| **10.G capability schema 定义 + 契约** | 📋 Planned | `include/tlm/cep/protocol_instance.schema.json` + `INV-CEP-12-IC1~3` 断言 | 3 人日 | ADR-CEP-12 §2 + §3 | 0(沿用 10.B)|

**总 tasks 待 execute**: Stage 0 共 **41 人日**(builder + architect 混合;**已消耗 13 人日**:10.A 8 + 10.E 5;**剩余 28 人日**:10.B 5 + 10.C 10 + 10.D 5 + 10.F 6 + 10.G 3 —— per oracle S5 人日口径 + 2026-10-21 architect 阶段修订)

**KPI 预算**(per `ADR-CEP-00 §2 Decision 3` + oracle v0.4 B8 三处口径对齐):

- Stage 0 = 1 slot(`cpptlm-cep-umbrella`,计 KPI,**不符 doc-only 豁免先例**)
- Stage 1 = 7 串行 × 1 slot = 7 季度(含 `cpptlm-cep-poc-spike`,**2026-10-22 P-1 增补**)
- Stage 2 = 2 串行 × 1 slot = 2 季度
- Stage 3 = 1 slot = 1 季度
- **总计**:11 个 OpenSpec change,串行 11 季度,任一时刻 Proposed 并发 ≤ 1

### 子任务依赖图(Stage 0)

```
10.A 真源 ADR 起草 ✅(16 份)──→ 10.B OpenSpec umbrella 起草(同步)
                                    ↓
        ┌───────────────────────────┼───────────────────────────┐
        ↓                           ↓                           ↓
10.C 契约测试                    10.D tools + experimental    10.E 7 文档 ✅
         │                           │                           │
         └─────── 10.G capability schema 契约 ────┘
        ↓                           ↓                           ↓
        └─────────────────────── 10.F 注入 lint ─────────────┘
                                    ↓
                          (Stage 0 验收 → Stage 1 启动)
```

### 子任务依赖图(Stage 1~3 远期)

```
Stage 0 完成 ──→ Stage 1 (7 change 串行 7 季度,**KPI 总数 = 11**,2026-10-22 P-1 增补):
                 ├─ cpptlm-cep-poc-spike              ← 2026-10-22 新增,置首位
                 │    (可丢弃 spike:解阻 A6 → 解阻全部协议插件 change)
                 ├─ cpptlm-cep-stub-cache-core
                 ├─ cpptlm-cep-directory-ordering-plane   ← 2026-10-21 重排
                 │    (原 sharer-tracker-base 扩容:Address Resolver +
                 │     Sharer Tracker 5 实现 + Ordering Point 三判据 +
                 │     装配期通道依赖图无环检查)
                 ├─ cpptlm-cep-collective-plane-base
                 ├─ cpptlm-cep-chi-rnf-subset
                 ├─ cpptlm-cep-cxl-nonsymmetric
                 └─ cpptlm-cep-tilelink-tree-manager
                                    ↓
                          Stage 2 (2 change 串行 2 季度):
                          ├─ cpptlm-cep-cxl-dcoh-symmetric
                          └─ cpptlm-cep-nvlink-sharp
                                    ↓
                          Stage 3 (1 change 串行 1 季度):
                          └─ cpptlm-cep-ualink-inc
                                    ↓
                          Phase 4+ 远期(per ADR-CEP-NUMBER-MAP §2):
                          ├─ CEP-08/09/12/13 **已起草**(v0.1 2026-10-21),实现按上表分 Stage 落地
                          ├─ CEP-11 协议版本锁定(待 architect 起草)
                          ├─ CEP-14 Stub 可验证性合同测试(待 architect 起草)
                          ├─ CEP-15 CacheTLM 对齐(待 architect 起草)
                          ├─ CEP-13 IoT 域归属
                          ├─ CEP-16 06 §15.4 演进对应(待 architect 起草)
                          └─ CEP-07A 06-bis 既有产物处置(待 architect 起草)
```

### 关键依赖

- **前序依赖**: phase-7(D-AXI Driver-Visible 完成)+ phase-8(timing-mode 完成)+ phase-9(dGPU Phase 2/3 active,无依赖)
- **后续被依赖**: 无(CEP 是探索平台,不接管 dGPU 业务或 PCIe EP 业务)
- **平行 phase**: phase-9(dGPU Phase 2/3)继续 active,CEP 不抢 KPI
- **跨仓依赖**: ArchForge coherence 接口草案(per `06-bis §5.2` SnoopProbe / `coherence-protocol.md` 等)

### 治理与文档约束(per AGENTS.md §DOC HYGIENE)

- **AGENTS.md:423 边界**: 第 3 条"SoC 架构组件/拓扑/演进类提案 → ArchForge"。CEP **核心框架层**(5 层骨架 + Stub Cache + Coherence Interface + 插件机制)**归 CppTLM**;**协议语义建模**(5 个具体协议设计文档)**归 ArchForge**(per `ADR-CEP-00 §2 Decision 4` 豁免论证,边界条款对照表)
- **AGENTS.md KPI 政策**: Proposed 并发 ≤ 3(per AGENTS.md §DOC HYGIENE line 420)。**当前真实 baseline = 0/3**(2026-10-20:3 个 active 全部 **⏸ 暂存归档** `--skip-specs`,0% 未实施,per AGENTS.md「两种归档语义必须区分」)。**KPI 空间已腾空** → **创建 `cpptlm-cep-umbrella` 的前置条件已满足**(per AGENTS.md line 422 新提案门槛 + `ADR-CEP-00 §2 Decision 3`)。**单一时钟真源**: `ADR-CEP-00 §2 Decision 3`,本文件不重算(per oracle v0.4 路线图评审 B1 修订)。
- **ADR 不可变 doc-only 类豁免**: `cpptlm-dgpu-soc-arch-truth-cleanup §6` 显式声明,仅 doc-only cleanup 类不计。**umbrella change 含测试契约骨架,不符豁免**(per oracle v0.4 B2-1)
- **真源 1 命名空间**: `docs/architecture/` 一律真源 1,`docs/adr/` 一律真源 3,跨仓 ArchForge 文档另立

### 配套测试标签(规划)

- `[cep]` 新增(Stage 0 起)~30 cases
- `[coherence]` 新增(Stage 1 起)~20 cases
- `[canonical-invariant]` 新增(Stage 0 起)~66 cases(66 条实际守门 invariant / 67 编号位,per oracle v0.5 复审 + 全面架构审查 N1/N2 + architect 阶段 2026-10-21 两批:IC4 **唯一跳空**)
- `[sweep]` + `[ab-compare]` 新增(Stage 0 起,~10 cases)
- baseline 增量:`1241 → ~1300 cases`(Stage 0)+ 后续 Stage 1~3 增量

### 真源现状(per `ADR-CEP-00A` §3.2 实证)

- **06-inter-module-network.md** v0.4 §15.4 P2.0-P5 编号口径(无 P6,per `ADR-CEP-NUMBER-MAP §2 Decision 3` 冻结)
- **06-bis-coherence-as-network-extension.md** v0.3 §7.3-§7.7 阶段范围 + §7.8 KPI 串行约束(per oracle v0.4 B1 实证)
- **01-hybrid-architecture-v2.1.md** v2.1.10 §4.7 = `simulation_mode` 真源之一(per oracle v0.4 B3-5 实证)
- **noc_bundles_tlm.hh** 字段真源:`vc_id` @ :38 / `flit_type` @ :39 / `flit_category` 注释 @ :45 / `CATEGORY_REQUEST/RESPONSE/CREDIT` @ :60-62(per oracle v0.4 B3-4 实证)

### 评审闭环

- **oracle v0.3 REJECT** → 修订 v0.4(per 2026-10-19)
- **oracle v0.4 REJECT**(8 BLOCKERS + 11 SHOULD_FIX + 10 NITS)→ 修订 v0.5(per 2026-10-20)
- **oracle v0.5 窄口 REJECT**(1 BLOCKER B7 + 6 SHOULD_FIX + 8 NITS)→ 修订 v0.6(per 2026-10-20)
- **oracle v0.5 三轮 REJECT 实证**:8/8 + 7/8 + 1/1 BLOCKER 关闭;10 份 ADR 达可签发水准(per oracle v0.5 评级)
- **oracle 全面架构复审(2026-10-21,首次独立复审)**:9 项问题(N1~N9)全部核实并修复 + 5 项架构级修订(CEN-Ib11 / `INV-CEP-06-IC5` / schema 补 5 维度 / `ADR-CEP-09` 两段式 / §7.2.1 验证深度契约);**CEP 骨架判定成立,无需改层**
- **architect 阶段(2026-10-21)**:两批新增 `ADR-CEP-08/09/12/13/18` + `ADR-CEP-11/14/15/16/07A` 共 **10 份** + 修订 8 份 → **21 份**;invariant 编号位 36 → **67**(实际 66);planner 阶段同步 Stage 重排 / DoD 7 分解 / change 清单

### 签发前置决策(per `ADR-CEP-00 §5.1 + §5.2` 待决项,pending 用户拍板)

1. **签发仓**:✅ **已拍板 = CppTLM 仓**(2026-10-20)。定性:"支持做互相协议探索的"归入框架级,类比 gem5 Ruby network(per `ADR-CEP-00 §5.1` gem5 Ruby 类比锚点)。**16 份 ADR** + 07 号真源 1 全留 CppTLM,引用不断裂
2. **真源文档路径**:✅ **已拍板 α** = `docs/architecture/07-coherence-exploration-platform.md`(2026-10-20 已创建 v0.1 草案,已注册 README Doc Map)

---

**版本**: v0.1(2026-10-20)
**最后**: 待 rdd-arch / rdd-builder 阶段推进
**作者**: CppTLM build agent(planner mode)