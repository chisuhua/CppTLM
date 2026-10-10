# dGPU SoC 文档真相清理 — 实施笔记 (cpptlm-dgpu-soc-arch-truth-cleanup v1.3)

> **状态**: ✅ 实施完成 (2026-10-10, per Oracle + Metis 双轮复审 + 4 处文字级润色)
> **作者**: CppTLM Team (Sisyphus 编排 + Metis 审查 + Oracle 审查 + v1.1 复审 + v1.3 修订)
> **优先级**: P0 (Phase 0 启动阻塞项)
> **估时**: 2-3d (实际 ~1 工作日)
> **关联 change**: openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/

---

## 1. 触发

2026-10-10 由 **Metis (Plan Consultant) + Oracle (Architecture Reviewer) 双轨独立审查** 触发。两位顾问独立产出 ~1300 行结论,**~80% 高置信度共识** 于 **9 硬伤 (Oracle §4) + 10 drift (Metis)**。

**v1.0 → v1.1 修订触发**: Oracle + Metis 复审 v1.0 change artifacts, 发现 5 项 P0 blocker (验收 grep AGENTS.md 必败 / design O-2 编造 2 参签名 / O-5 §2.4 过度修复 / 文件清单 4 文档互斥 / KPI 豁免自举)。

**v1.1 → v1.2 修订触发**: Oracle N1-N5 + Metis B1-B3 复审发现 13 处文字级修复。

**v1.2 → v1.3 修订触发**: Oracle + Metis 双轮复审 + 4 处文字级润色: (a) 4 处 N4-residual 英文锚点 grep → 中文锚点 (arch §18 §19); (b) 3 件套版本头同步 v1.3; (c) spec.md:6 修订计数 5 → 6; (d) spec.md:31 ≥6 → ≥8 对齐 design.md:67 + tasks.md:163 增量 T11.4-T11.6。

---

## 2. 修复的 19 项 finding

### Oracle 9 项硬伤 (Spec/Code Truth)

| Finding | 文件 / 行号 | 修复任务 |
|---------|------------|----------|
| **O-1/O-2** 虚构 DGpuBoard::trigger_flr/resize_bar + PcieEndpointIP::handle_resize_bar (全仓零匹配) | arch.md:634-637/:659/:677/:721-723/:738/:740; AGENTS.md:410/:504/:505/:593 | T3.5 + T11.1-T11.7 |
| **O-3** Gap I 病因误诊 (真矛盾 = ADR-DGPU-06 §2.3 + AGENTS.md 与代码漂移) | arch.md:99 + arch.md:208 | T5.1-T5.2 |
| **O-4** AGENTS.md KEY INVARIANT SDMA wire-format 与代码漂移 | AGENTS.md:513 | T3 |
| **O-5** 5-IP vs 6-IP 计数硬伤 | arch.md:69 + README.md:29 | T6.1-T6.3 |
| **O-6** KPI arithmetic 多处不一致 (1/3, 3/3, 4/3, 5/3 混用) | arch.md §0.2 (line 229+) | T8.1 |
| **O-7** §1.4.1 VramControllerTLM 描述用现在时 | arch.md:328 | T7.1 |
| **O-8** PCIe Base Spec §7.5.1.2 章节号错 (应为 §7.5.3.4 / §7.8.4) | arch.md:28 + arch.md:691 + AGENTS.md:503/:569/:588 | T3.6 + T12.1-T12.2 |
| **O-9** arch §18-19 链接已 archive change 且状态漂移 | arch.md:18-19 + arch.md:221-222 + arch.md:466 | T9.1-T9.4 |

### Metis 10 项 Drift

| Finding | 文件 / 行号 | 修复任务 |
|---------|------------|----------|
| **M-1/M-2** Change C 声称新增 SocInternalNoc / NOC adapter 已实现 | arch.md:466 | T10 (并入 T9.2) |
| **M-3** Change C GMMU PTE fetch 5 处矛盾 | arch.md:99 + arch.md:208 | T5.1-T5.2 |
| **M-4** arch §0.1.1 LWC 路径 `cluster/` → `lwc/` | arch.md:129 | T4.1 |
| **M-5/M-6/M-7** | 与 O-9/O-3 重叠 | (随 T9 + T5 完成) |
| **M-8** host_pcie_test_stub 已存在 + Change D serializer 关系未定义 | openspec/changes/cpptlm-pcie-tlp-entry-layer/proposal.md 顶部 | T14.4 |
| **M-9** Change B DoorbellBundle 字段数残留 | openspec/changes/cpptlm-lwc-bus-hub-mvp/design.md 顶部 | T14.2 |
| **M-10** NOC adapter 注释引用已裁定不存在的 dual bundle | include/tlm/vram_noc_adapter.hh:8 + include/tlm/gpu/sdma_noc_adapter.hh:44/:75 | T13.1-T13.2 |

---

## 3. 修改文件清单 (11 个 + 1 新增 + 1 新增)

### 修订文件 (11)

1. `docs/designs/dgpu-soc/architecture.md` — 9 处 § 修订 (T5 + T6 + T7 + T8 + T9 + T10 + T11 + T12 + T15-T16 增量)
2. `docs/designs/dgpu-soc/README.md` — 1 处 (T6.3)
3. `AGENTS.md` — KEY INVARIANT (T3) + flr-rebar 叙事段 (T3.5) + §7.5.1.2 (T3.6) + §DOC HYGIENE doc-only 豁免 (T15) + §PHASE STATE entry (T16)
4. `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md` — 追加 Status Update 2026-10-10 段 (T2)
5. `openspec/changes/cpptlm-vram-controller-functional-integration/design.md` — 顶部 cross-reference 注解 (T14.1)
6. `openspec/changes/cpptlm-lwc-bus-hub-mvp/design.md` — 顶部 cross-reference 注解 (T14.2)
7. `openspec/changes/cpptlm-gmmu-pte-bus-correction/proposal.md` — 顶部 cross-reference 注解 (T14.3)
8. `openspec/changes/cpptlm-pcie-tlp-entry-layer/proposal.md` — 顶部 cross-reference 注解 (T14.4)
9. `include/tlm/vram_noc_adapter.hh` — 仅注释修订 L8 (T13.1)
10. `include/tlm/gpu/sdma_noc_adapter.hh` — 仅注释修订 L44/:75 (T13.2)
11. `scripts/test/docs_sync_check.sh` — VIRTUAL_PATHS +5 (e1 v1.1 + flr-rebar v1.1) + cpptlm_emulator_*.h (T1)

### 新增文件 (2)

1. `docs/implementation/2026-10-10-arch-truth-cleanup.md` — 本实施笔记 (T17.1)
2. `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/README.md` — change 入口

---

## 4. 验证结果 (T18-T19)

### Grep 验证链 (T18.2)

| Grep | 期望 | 实际 |
|------|------|------|
| 虚构 API (`DGpuBoard::trigger_flr` / `DGpuBoard::resize_bar` / `PcieEndpointIP::handle_resize_bar`) | 0 命中 | ✅ 0 |
| `JSON.*AxiMemBundle 直连` / `头文件注释.*PcieTlpBundle.*JSON.*AxiMemBundle` | 0 命中 | ✅ 0 |
| `5-IP\|6-IP` (arch.md:69 + README.md:29 + arch.md:455 双重计数) | arch:69 = "6-IP" + README:29 = "6-IP" + arch:455 = "5-IP" | ✅ |
| `当前 baseline.*3/3\|5/3 (4 Proposed Draft\|原则 1/3` | ≥ 2 命中 | ✅ ≥ 2 |
| `§7.5.1.2.*FLR\|§7.5.1.2.*Device Control` | 0 命中 (5 处全清) | ✅ 0 |
| `🟡 Draft.*e1\|🟡 Draft.*flr-rebar` | 0 命中 | ✅ 0 |
| `mem_in/mem_out 切 AxiMemBundle` | 0 命中 | ✅ 0 |
| `include/tlm/cluster/lwc_bus_hub` | 0 命中 | ✅ 0 |
| `NoCRespFlitBundle\|NoCReqFlitBundle` (adapter 注释) | 0 命中 | ✅ 0 |
| `2026-10-27` (编造 ETA) | 0 命中 | ✅ 0 |
| `on_bar_resize([^)]*,[^)]*)` (2 参虚构调用) | 0 命中 | ✅ 0 |
| `flr_pf\|flr_vf\|enable_resizable_bar\|on_bar_resize` (既有 EP API) | ≥ 8 命中 (arch.md) | ✅ ≥ 12 |
| `archived 2026-10-09` | ≥ 2 命中 | ✅ ≥ 5 |
| `cpptlm-dgpu-soc-arch-truth-cleanup` (4 active change 注解) | ≥ 4 命中 | ✅ 8 |
| `doc-only cleanup 类 change 不计入` (KPI doc-only 豁免) | ≥ 1 命中 | ✅ 1 |

### 23 ABI + 5 frozen headers 零变更验证 (T19.8-T19.9)

- 23 ABI 签名零变更: ✅ (per ADR-DGPU-08 §2.3 0 新增约束)
- 5 frozen headers 零 diff: ✅ (`pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh` + `pcie_endpoint_ip.hh` + `cpptlm_emulator.h`)

### Catch2 测试保持回归 (T19.3-T19.7)

- `[pcie]` 430 cases / 36672 assertions — 不变 ✅
- `[minimal_dgpu_soc][driver_visible]` 12+2 cases — 不变 ✅
- `[realip]` 22 cases / 193 assertions — 不变 ✅
- `[pcie-memory]` 17 cases / 48 assertions — 不变 ✅
- `[dgpu_soc_timing]` 37 cases / 12024 assertions — 不变 ✅

### docs_sync_check (T18.1)

`./scripts/test/docs_sync_check.sh --strict` — PASS (EXIT: 0) ✅

---

## 5. 8 Commits 落地 (T20)

按 D-AXI 历史 commit 风格 (per tasks.md T20.1 + design.md §5):

1. `ci(docs-sync)` — VIRTUAL_PATHS +5 (e1 v1.1 + flr-rebar v1.1) + cpptlm_emulator_*.h
2. `docs(adr-dgpu-06)` — Status Update 2026-10-10 (D-AXI v1.5 B25 撤回 + 代码现状)
3. `docs(arch)` — arch.md 全部 9 处修订合并 (LWC 路径 + Gap I 病因 + 6-IP + §1.4.1 时态 + KPI 双行制 + archived + SocInternalNoc 已实现 + 虚构 API + PCIe spec)
4. `docs(agents)` — AGENTS.md KEY INVARIANT + flr-rebar 叙事段 + §7.5.1.2 + §DOC HYGIENE doc-only 豁免
5. `docs(readme)` — dgpu-soc/README.md 6-IP
6. `docs(changes)` — 4 active change cross-reference 注解
7. `docs(comments)` — NOC adapter 注释 M-10
8. `docs(implementation)` — 实施笔记 + AGENTS.md §PHASE STATE entry

---

## 6. 后续动作

- 用户 review 后 archive (per tasks.md T20.6 仅当用户批准)
- 4 active change author 在自家 v0.5.2 修订时采纳本 change cross-reference 注解
- D-AXI v1.9 修订时另行处理 ADR-DGPU-06 §2.3 文字层 (本 change 不在范围)

---

## 7. KPI 治理

**doc-only cleanup 不计入 Proposed KPI 上限** (per Proposal §6 + AGENTS.md §DOC HYGIENE 治理类豁免):

- 当前 baseline (2026-10-10): **5/3** (4 Proposed Draft v0.5.1 [A/B/C/D] + 1 existing Proposed p2-integration-unblock); 本 change 不消耗 slot
- 修订后 baseline: 仍 5/3 (本 change 已实施完成, 待 archive 释放)

---

## 8. 参考

- change README: `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/README.md`
- proposal: `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md`
- design: `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/design.md`
- spec: `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/specs/dgpu-soc-architecture/spec.md`
- tasks: `openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup/tasks.md`
- ADR Status Update: `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md` (2026-10-10 段)

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-10 (实施完成 + v1.3 cosmetic 同步)