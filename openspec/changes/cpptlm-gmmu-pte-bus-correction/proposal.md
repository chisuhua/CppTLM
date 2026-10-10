# cpptlm-gmmu-pte-bus-correction: 修复 GMMU page-walk wire-format + 接入 SocInternalNoc DataNoC (修复 Gap I + G2 + G1)

> **状态**: 🟡 Proposed (Phase 1, Change C, 2026-10-09 lock)
> **作者**: CppTLM Team
> **版本**: v0.5.2 (Tranche 1 修订: 范围缩减 — NOC 路由已实现 (e1 v1.1 commit 13af5d51), 本 change 仅做 GMMU dedicated client 接线 + 删 `translate_cb_` 旧路径; 初版 v1.0 承接 e1 v1.1 + 协同 Change B 的 LWC 设施)
> **关联 OpenSpec**:
>   - ✅ `cpptlm-dgpu-soc-internal-noc-e1` v1.1 archived (SocInternalNoc 4 NODE_ID + 16 字段 + vc_counts_[4] — **代码已落地**（commit 13af5d51，本 change 仅做接线层工作）)
>   - ✅ `cpptlm-lwc-bus-hub-mvp` Phase 0 (上游, 提供 LWC 设施 + 异构 Bundle infrastructure)
>   - ✅ `cpptlm-vram-controller-functional-integration` Phase 0 (上游, VramController functional-mode 数据路径; Change C **依赖** Change A 的 `handle_functional_request` API + GMMU programmatic bridge `set_vram_target()` 注入接口)
>   - **业务定位锁定**: `docs/designs/dgpu-soc/architecture.md` §0.1 业务定位 + §0.1 Gap 矩阵 + §0.2 总体实施计划 (Change C)
>
> **🔗 Cross-Reference (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3, 2026-10-10)**:
> 本 Change C 立项时误诊 Gap I 病因 ("JSON vs 头文件" 假矛盾, per Oracle §4 #3 / Metis M-3)。**真实病因** = ADR-DGPU-06 §2.3 + AGENTS.md KEY INVARIANT line 513 (SDMA 切型决策) 与代码 (`sdma_engine_tlm.hh:82-86` + `gmmu_tlm.hh:50-51` + `pcie_memory_device.hh:94-95` — 全部 PcieTlpBundle) 不一致 (D-AXI v1.5 B25 撤回 v1.3 B2 切型决策, 但 ADR §2.3 + AGENTS.md 未同步, 详见 ADR-DGPU-06 Status Update 2026-10-10)。**v0.5.2 已落地**: 范围缩减 — **NOC 路由已实现** (`include/tlm/cluster/soc_internal_noc.hh:114-168`, e1 v1.1 commit `13af5d51`: `route_flit()` 实际推送 + `vc_counts_[4]` + `send_request/response` 均已落地), 本 change **不重写 NoC**, 仅做 GMMU dedicated client 接线 (绕过 mesh NoC, 直连 VramControllerTLM) + 删 `translate_cb_` 旧路径。与 Gap I 真矛盾诊断对齐。详见 [cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md §O-3](../../archive/2026-10-10-cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md)。
> **修复 Gap**: **I** (HIGH) + **G2** (MED) + **G1** (D3+ 待废弃 lambda), §0.1 Gap 矩阵 16 行中 3 行
> **优先级**: **P2** (Phase 1, 紧随 Phase 0 完成后启动)
> **估时**: **5-7d** (含与 e1 v1.1 设计合并实施)
> **依赖**: Change B (`lwc-bus-hub-mvp`) 已实施 + Change A (`vram-controller-functional-integration`) 已实施 (Phase 0 完成 archive 后)
> **目标 tag**: `cpptlm-gmmu-pte-bus-correction-completion`

---

## Why

§0.1 Gap 矩阵锁定的 16 行中, 3 行由本 change 修复:

### Gap I (HIGH): GMMU page-walk wire-format 语义错配

- **现状 (v0.5.2 修订 per §0.1 Gap I 裁决)**: `include/tlm/gpu/gmmu_tlm.hh:12,86` 注释写 `wire-format: PcieTlpBundle (v1.8 H1)`, 但 `configs/dgpu_soc_minimal_v1.json:60` 配置 `gmmu.0 → pcie_memory.1` 是 **AxiMemBundle 直连**. 头文件与 JSON **自相矛盾**.
- **v0.5.2 修订裁决** (per §0.1 Gap I + 0.5 H100 GMMU dedicated memory client 模式): **GMMU PTE fetch 改走 VramControllerTLM 专用 functional port** (per Change A 新增 `handle_functional_request` + Change C 增 GMMU `set_vram_target()` programmatic bridge); 不再经 PcieMemoryDevice port 1; 不经 NOC mesh. **消除三锁叠加死锁** (A 锁 port 1 + §1.4.6 锁不走 VramController + pcie_memory 统一 PcieTlpBundle).
- **根因**: D-AXI v1.5 B25 决策为避开异构 multi-port adapter 框架限制, 把 GMMU 切回 PcieTlpBundle (PCIe link layer TLP 语义), 但混淆了 PCIe 域与 chip-internal 域 (违反 ADR-DGPU-06).
- **真实物理 IP 边界**:
  - **NVIDIA H100 GMMU**: 专用 memory client, **bypass mesh NoC** (per Oracle RTL 调研)
  - **ARM CMN-700 + SMMUv3 DVM**: 独立通道
  - **ARM CHI**: 4 通道 (REQ/RSP/DAT/SNP) + 4 VC
  - **HBM3 Controller**: AXI4 5 通道 (AW/W/AR/R/B)
  - **PCIe EP**: 硬件 MSI-X Generator (无软件 API)
- **本 change 修复路径**: GMMU PTE fetch 走 **chip-internal AXI** (AxiMemBundle 恢复, 与真实 RTL 模式对齐). GMMU dedicated memory client **不进 mesh NOC** (per Gap G2 边界).

### Gap G2 (MED): chip-internal NOC 2 条 AxiMemBundle 直连

- **现状**: `dgpu_soc_minimal_v1.json:58-61` `sdma.2 → pcie_memory.0` + `gmmu.0 → pcie_memory.1` 两条 **直连** (无 SocInternalNoc mesh).
- **本 change 修复路径**: SDMA→VRAM 路径接入 **SocInternalNoc** mesh NoC (per e1 v1.1 DataNoC 设计). GMMU→VramControllerTLM 路径**不进** mesh NOC (per Gap I: GMMU dedicated memory client bypass mesh).

### Gap G1 (D3+): SDMA↔GMMU lambda 注入

- **现状**: `dgpu_board_shell.cc:714-718` `sdma->set_translate_cb(lambda)`, lambda 内部直调 `gmmu_->translate(iova, size, phys)` — 违反 §0.1 原则 3.1 "无 callback 注入".
- **本 change 修复路径 (v0.5.2)**: SDMA 收到 GMMU 翻译响应经 **GMMU dedicated client 直连 VramControllerTLM** 路径返回 (per Gap I 裁决: GMMU bypass mesh NoC, 不经 SocInternalNoc; NOC 仅承载 SDMA bulk DMA per e1 v1.1).

### 业务定位同步 (per §0.1 §X 强约束)

Gap I + G2 + G1 是 §0.1 原则 3 ("SoC 内部模块接口贴近真实物理 IP 边界") 的 **核心子项**. 没有这 3 个 Gap 修复, 原则 3 部分不可达成 (GAP K 已修得 CR-Bus, 但 GMMU PTE page-walk 仍走 callback 注入).

---

## What Changes

### 新增文件 (1 个, ~50 行)

| 文件 | 内容 |
|------|------|
| `docs/implementation/2026-10-09-gmmu-pte-bus-correction.md` | 实施笔记 (TDD 5-step + commits 索引) |

### 修改文件 (5 个, ~200 行)

| 文件 | 改动 | 备注 |
|------|------|------|
| `include/tlm/gpu/gmmu_tlm.hh` | **恢复 AxiMemBundle wire-format** (per Gap I); 显式 `MasterPort req_out()` + `SlavePort resp_in()`; 移除 PcieTlpBundle (per e1 v1.1 决策反转); 删 `set_translate_cb` 相关 (Gap G1 in 联动) | 同步 e1 v1.1 B22 bound check |
| `include/tlm/cluster/soc_internal_noc.hh` | **NoC 路由已实现** (per e1 v1.1, `include/tlm/cluster/soc_internal_noc.hh:114-168`, commit 13af5d51): `route_flit()` 实际推送 + `vc_counts_[4]` 计数器 + `send_request/response` API 均已落地. 本 change **不重写 NoC**, 仅做 GMMU dedicated client 接线 (绕过 mesh NoC, 直连 VramControllerTLM) + 删 `translate_cb_` 旧路径 (per `include/tlm/gpu/sdma_engine_tlm.hh:233-256`) | 沿用 e1 v1.1 实现 (已落地, 0 新增) |
| `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings` | **删 `set_translate_cb` lambda** (per Gap G1); 加 GMMU→VramControllerTLM dedicated memory client 装配 (per Gap I/G2 区分) | 关键 invasive change (~20 行) |
| `configs/dgpu_soc_minimal_v1.json` | `gmmu.0` connection 改 → **GMMU 直连 VramControllerTLM (per Gap I 裁决, 不经 mesh NoC)**; `sdma.2` connection → `soc_internal_noc` → pcie_memory (DataNoC, per Gap G2) | 一致性 schema |
| `test/test_soc_internal_noc_data_flow.cc` | 新增 `[noc][gmmu-bypass]` 5 cases + `[noc][sdma-bulk-dma]` 5 cases + `[noc][deadlock-fuzz]` 3 cases | TDD RED→GREEN |

### 实施铁律 (per §0.1 + e1 v1.1 + Gap I/G2/G1 修复锁定)

1. **Gap I 修复**: GMMU 用 **AxiMemBundle** (chip-internal AXI), 头文件 wire-format 注释对齐
2. **Gap G2 修复**: SDMA bulk DMA 经 SocInternalNoc mesh; GMMU PTE walk **不进 mesh** (dedicated memory client 直连 VramControllerTLM, per Gap I 裁决)
3. **Gap G1 修复**: 删 `set_translate_cb(lambda)` (per `include/tlm/gpu/sdma_engine_tlm.hh:233-256`), SDMA 经 NOC 触发 translate; GMMU dedicated client 直连 VramControllerTLM (不经 mesh)
4. **不引入新 dual bundle** (per e1 v1.1 决策沿袭 NoCFlitBundle + flit_category)
5. **VramController 范围**: 本 change **不**改 VramController (Phase 0 Change A 责任); 由 Phase 4 VramController timing-mode 集成负责
6. **协同 LWC**: 本 change **不**改 LWC hub (Phase 0 Change B 责任)
7. **Frozen surface 守恒**: 23 ABI + 4 frozen headers 零 diff (per AGENTS.md H3)

### Frozen Surface (0 修改)

- `include/abi/cpptlm_emulator.h` (per ADR-088 §D5)
- `include/tlm/gpu/pcie_endpoint_tlm.h`
- `include/tlm/gpu/pcie_display_device.hh`
- `include/tlm/pcie/pcie_bundles_tlm.hh`
- `include/tlm/pcie/pcie_endpoint_ip.hh` (含 Change B 准 frozen 既有 API)

### 推迟到后续 Change (out of Change C scope)

| 项目 | 推迟到 | 原因 |
|------|--------|------|
| SocInternalNoc E.2 multi-VC NoC + ConfigBus/TranslationBus | Change F (Phase 3) / E.2 (ArchForge) | per e1 v1.1 决策 |
| GMMU dedicated memory client **异构协议** (DVM / CHI REQ channel) | Change F | real RTL 多样性, 本 change 用最简 AxiMemBundle |
| CompletionRing fence→MSI-X 链路 (Gap L) | Change E (Phase 2) | 由 Change E 实施 |
| MSI-X Generator IP | Change E | 不在本 change |
| PcieTlpSerializer (Gap M) | Change D (Phase 1) | 由 Change D 实施 |

---

## Impact

### 影响范围

| 受影响模块/文件 | 影响 | 兼容性 |
|---------------|------|---------|
| `GmmuTLM` wire-format PcieTlpBundle → AxiMemBundle | invasive change | 头文件改; 既有 `[pcie]` 测试可能受影响 (verify in Phase 2 GREEN); `test_pcie_endpoint_ip_basic.cc:102-108` 已覆盖 GMMU 异步 |
| `SDMA::set_translate_cb(lambda)` 删除 | invasive change | 翻译路径改成 NOC 触发 + GMMU dedicated client 直连 VramControllerTLM; 既有 SDMA 测试 `[sdma]` + `[h2d]` 需要切换 SDMA translate 后路径 |
| `SocInternalNoc::route_flit()` 实际推送 (e1 v1.1 已落地) | **0 新增** (per `soc_internal_noc.hh:114-168`, commit 13af5d51) | 0 改动既有 SocInternalNoc (per e1 v1.1 E0 设计) |
| `dgpu_board_shell.cc::bind_memory_backings` | 改 ~20 行 | 0 改动 PcieMemoryDevice / VramController 注入 |
| `dgpu_soc_minimal_v1.json` | GMMU 直连 VramControllerTLM (Gap I); `sdma.2` → `soc_internal_noc` (DataNoC, Gap G2); 改 connection | JSON schema 验证器增 test |

### 受影响测试

| 测试 | 当前 | 实施后 |
|------|------|--------|
| `[noc]` (e1 v1.1 已设计) | 0 cases | ✅ **新增 13 cases** (5 GMMU-bypass + 5 SDMA-bulk + 3 deadlock-fuzz) |
| `[sdma]` (既有) | Pass | ✅ 保持 (SDMA translate 路径改成 NOC 触发 + GMMU direct 直连, 测试需要重连) |
| `[pcie]` (既有 430 cases / 36672 assn) | Pass | ✅ 保持 (PCIe EP 行为不变) |
| `[minimal_dgpu_soc]` | Pass (12 cases / 66 assn) | ✅ 保持 (driver 视角不变) |
| `[dgpu_soc_timing]` | Pass | ✅ 保持 (timing-mode 独立) |

---

## Acceptance (per §0.2 DoD)

- [ ] **Gap I 修复**: `gmmu_tlm.hh:12,86` wire-format 注释与 JSON **GMMU 直连 VramControllerTLM** 配置**对齐** (AxiMemBundle 单 bundle; 不经 mesh NoC, per Gap I 裁决)
- [ ] **Gap G2 修复**: SocInternalNoc `route_flit()` 已实现 (per `soc_internal_noc.hh:114-168`), 本 change 仅完成 SDMA→VRAM 经 NOC 接线, 既有 `[minimal_dgpu_soc]` 0 回归
- [ ] **Gap G1 修复**: `dgpu_board_shell.cc` `set_translate_cb(lambda)` **0 处**, 改成 NOC 触发路径
- [ ] TDD 5-step GREEN (12 commits per tasks.md)
- [ ] `[noc]` 13 新 cases PASS
- [ ] 既有 `[sdma]` / `[pcie]` / `[minimal_dgpu_soc]` 0 回归
- [ ] 23 ABI + **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面)
- [ ] `openspec validate --strict` PASS
- [ ] 文档同步: §0.2 Gap I/G2/G1 行 ✅ 修订 + e1 v1.1 激活注释

### Commit 策略 (per e1 v1.1 历史 + D-AXI commit 风格)

预计 **12 commits**, 按 Phase 1 顺序:

```
1.  test(noc): RED 5 cases GMMU bypass (Phase 1.1)
2.  feat(gmmu): wire-format 切回 AxiMemBundle + MasterPort (Phase 1.2)
3.  test(noc): RED 5 cases SDMA bulk DMA (Phase 1.3)
4.  feat(gmmu): GMMU dedicated client 直连 VramControllerTLM 接线 (Phase 1.4; NOC 路由已实现, 0 重写)
5.  feat(board): 删 set_translate_cb lambda (Phase 1.5)
6.  feat(json): GMMU 直连 VramControllerTLM + sdma→soc_internal_noc connections (Phase 1.6)
7.  test(noc): GREEN 10 cases functional (Phase 1.7)
8.  test(regress): 4 套件 [sdma][pcie][minimal_dgpu_soc][dgpu_soc_timing] 0 回归 (Phase 1.8)
9.  test(noc): RED+GREEN 3 cases deadlock-fuzz (Phase 1.9)
10. docs(implementation): 2026-10-09-gmmu-pte-bus-correction 实施笔记 (Phase 1.10)
11. docs(architecture): §0.2 Gap I/G2/G1 ✅ 修订 (Phase 1.11)
12. chore(openspec): archive cpptlm-gmmu-pte-bus-correction (Phase 1.12)
```

---

## OpenSpec KPI 状态

- **实施前**: 1/3 (`cpptlm-p2-integration-unblock`)  + 1/3 Change A + 1/3 Change B = 3/3
- **本 change 激活条件**: Change A archive 后 (KPI → 1/3 清 1 槽); Proposed Change C → 2/3
- **B archive 后**: Change D Proposed → 3/3 (KPI 满)
- 本 change 期间 2/3 (合规), D 起草**只**可在 B archive 后 activate

---

## 相关变更

| Change | 关系 |
|--------|------|
| `cpptlm-dgpu-soc-internal-noc-e1` v1.1 ✅ archived | **直接前置**: SocInternalNoc 设计已锁 (4 NODE_ID + 16 字段 + vc_counts_[4]); 本 change 实施化到 minimal_v1 |
| `cpptlm-lwc-bus-hub-mvp` ✅ Phase 0 | **直接协作**: GMMU 可以经 LWC (CrBundle) 访问某些 config registers; 但 PTE fetch 不经 LWC (经 AxiMemBundle) |
| `cpptlm-vram-controller-functional-integration` ✅ Phase 0 | **直接协作**: VramController 数据路径接收 SDMA 经 NOC→VRAM 的 DMA; 本 change 保持 VramController 设计 |
| `cpptlm-pcie-tlp-entry-layer` (本计划 Change D) | **并行 (Phase 1)**: D 独立修复 Gap M, 0 直接依赖 |

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-09 (v1.0 起草, Phase 1 Change C 启动)
