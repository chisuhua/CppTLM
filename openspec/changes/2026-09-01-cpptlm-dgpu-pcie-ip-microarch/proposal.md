# cpptlm-dgpu-pcie-ip-microarch: PCIe IP 微架构 + 7 阶段路线图(伞形 change)

> **状态**: 📋 Proposed — 2026-09-01 · **日期**: 2026-09-01 · **Owner**: CppTLM Team (Sisyphus)
> **性质**: 伞形 change(umbrella) — 不直接实施,仅产出设计文档 + 决策 + 路线图 + 7 个子 change 描述
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D2
> **Oracle 咨询**: `bg_661f4ee7` (2026-09-01, 失败后 fallback) — 关键修正: Gen5 ≠ FLIT (FLIT 是 Gen6 特性)
> **Librarian 调研**: `bg_07392e0c` (2026-09-01)
> **被依赖(现有)**: [`2026-08-26-cpptlm-dgpu-pcie-endpoint`](../archive/2026-08-29-2026-08-26-cpptlm-dgpu-pcie-endpoint/) (已 archive, 当前 PcieEndpointTLM 事务层基线)
> **被依赖(未来)**: [`2026-08-26-cpptlm-dgpu-board-soc-split`](../archive/) 等依赖 PCIe 端口存在的 change

---

## Why

当前 `PcieEndpointTLM`(2026-08-26 archive, `include/tlm/gpu/pcie_endpoint_tlm.h:147`)仅覆盖 PCIe **事务层**(Config Space / BAR / MSI-X),以下能力全部缺失或外部化:

| 能力 | 当前状态 | 用户需求 |
|---|---|---|
| Link Layer (DLLP / ACK-NAK / FC) | ❌ Board shell 注入 TLP 直接绕过 | 必须仿真(CA 反压) |
| PHY Digital Control (LTSSM / Eq / PIPE) | ❌ "外部 IP 责任" | 必须仿真(链路训练 / 热插拔) |
| Gen5 128b/130b 编码 | ❌ 跳过 | 必须建模延迟(非 bit-level) |
| SR-IOV (1 PF only) | ❌ | 必须扩展(8-16 VFs, GPGPU 标配) |
| Hot-plug (PERST# only) | ⚠️ 部分 | 必须全状态机(PCIe 机箱规范) |
| 链路训练均衡 (Gen3+ Eq) | ❌ | 必须简化为 preset 切换 |
| AXI 接口到 SoC | ❌ 仅 ChStream PcieTlpBundle | 必须 AXI StreamAdapter |
| AXI4Mapper | ❌ 仅 ADR 提及 | 必须可选注入(精确 AXI4 仿真) |
| Host Bypass 模式 | ❌ | 必须独立组件(软件 bring-up) |
| PcieRootComplex 镜像 | ❌ | 可选(若不用外部 RC BFM) |

**Oracle 关键洞察**(bg_661f4ee7, 修订版):
- ❌ **错**: 把 Gen5 当成 FLIT 模式 — FLIT 是 **PCIe 6.0** 特性,Gen5 用 TLP + 128b/130b 编码
- ✅ **对**: SoC 仿真不需要 bit-level 编码;Token Bucket FC > 6-header credit;PIPE 4-signal > 50-signal

**触发事件**:
- 2026-09-01 用户对话: 询问 PCIe EP 仿真范围 + Gen5 + CA + SR-IOV + Hot-plug + AXI 集成
- 2026-09-01 Oracle 咨询(失败 1 次后 fallback 完成): 给出 Q1-Q3 推荐
- 2026-09-01 Librarian 调研: 修正 FLIT 误解 + 列出可用开源 BFM
- 2026-09-01 用户选择 Option A(立即创建 OpenSpec proposal)

---

## What Changes

### 1. 本伞形 change **不直接实施任何代码**

仅产出 7 个文件 + 7 个子 change 的描述:

| 文档 | 内容 |
|---|---|
| `proposal.md`(本文件) | Why / What / Acceptance Gate / Cross-Repo |
| `design.md` | 微架构设计 + 4 张 ASCII 数据流图(顶层 / EP内部 / 链路层 / PHY数字) |
| `decisions.md` | Q1-Q17 决策点完整记录(9 原 + 8 Oracle 新增:Gen5 编码 / FC / PIPE / BFM / 验证 / SR-IOV / Bypass / 23 ABI / AXI4Mapper + FLR / 单 VC / Completion Timeout / AER / Surprise Removal / 错误注入 / ASPM / 下行 Rx) |
| `roadmap.md` | 7 阶段详细路线图 + 每阶段的独立 OpenSpec Change 描述(phase-01 到 phase-07) |
| `specs/pcie-ip-microarch/spec.md` | 顶层能力规格(ADDED Requirements) |
| `tasks.md` | 顶层 umbrella 任务 + Phase 1 详细 TDD 5 步任务 |
| `README.md` | 文档导航 + Phase 1 启动指南 |

### 2. 7 个子 change(描述已就绪,实施时各自独立 folder)

| # | Change 名 | 工期 | 状态 |
|---|---|---|---|
| Phase 1 | `cpptlm-dgpu-pcie-link-layer-and-fc` | 4 周 | 立即可启动 |
| Phase 2 | `cpptlm-dgpu-pcie-130b-encoding` | 2 周 | 依赖 Phase 1 |
| Phase 3 | `cpptlm-dgpu-pcie-phy-digital-ctrl` | 4 周 | 依赖 Phase 1 |
| Phase 4 | `cpptlm-dgpu-pcie-sriov` | 4 周 | 依赖 Phase 3 |
| Phase 5 | `cpptlm-dgpu-axi-stream-adapter` | 3 周 | 独立(可与 Phase 1 并行) |
| Phase 6 | `cpptlm-dgpu-axi4-mapper` | 4 周 | 依赖 Phase 5 |
| Phase 7 | `cpptlm-dgpu-pcie-host-bypass-and-rc` | 3 周 | 依赖 Phase 5 |
| **总计** | | **24 周** | **~6 个月单人** |

### 3. 关键交付物(全 7 阶段完成后)

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/tlm/pcie/pcie_endpoint_ip.cc` | **PcieEndpointIP** 整合模块(替代/扩展当前 `pcie_endpoint_tlm.h`) |
| `include/tlm/pcie/pcie_link_layer_tlm.hh` + `.cc` | 链路层(DLLP + ACK/NAK + Retry buffer) |
| `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh` + `.cc` | PHY 数字控制(LTSSM + Eq + 热插拔) |
| `include/tlm/pcie/pcie_flow_control_token_bucket.hh` + `.cc` | Flow Control Token Bucket 引擎 |
| `include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh` + `.cc` | SR-IOV VF Pool(VF 聚合路由) |
| `include/tlm/pcie/host_bypass_tlm.hh` + `.cc` | Host Bypass 独立组件 |
| `include/bundles/pcie_dllp_bundles_tlm.hh` | **新**: DLLP / FC / PipeSignal bundle |
| `include/framework/axi4_mapper.hh` + `.cc` | **新**: AXI4 ↔ Bundle Mapper |
| `include/bundles/axi4_bundles_tlm.hh` | **新**: AXI4 transaction bundle |
| 23 ABI 兼容修订 | `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 锁定 |

### 4. 明确排除(OUT of scope)

| 功能 | 排除原因 | 后续承接 |
|---|---|---|
| PCIe 6.0 FLIT mode | Oracle 修正: FLIT 是 Gen6 特性;Gen5 用 128b/130b | 后续 v3.x PR |
| PAM4 编码 | Gen6 特性 | 后续 PR |
| PCIe 物理层模拟(SerDes / 通道损伤) | PHY BFM 职责 | 外部 VIP / FPGA |
| 8b/10b 编码(Gen1/2) | Gen5 不需要,Gen5+ 用 128b/130b | N/A |
| Legacy MSI(非 MSI-X) | UsrLinuxEmu 仅 MSI-X(per ADR-088 §D3.5) | 后续 PR |
| ATS / PASID / PRI | ADR-088 v1.0 scope 不含 | 后续 PR |
| 完整 Completion Timeout 错误模型 | UsrLinuxEmu 容错吸收 | 外部责任 |
| RC BFM 商业 VIP | 商业 license 不嵌入 CppTLM | 用户自选 |

---

## 数据流快速视图(完整版见 design.md §2-§5)

```
   ┌─────── Host (RC BFM / Driver / QEMU) ──────────────────────┐
   │  - 真实 RC: Cadence/Synopsys VIP / QEMU vPCI / 自研 RC      │
   │  - Bypass: HostBypassTLM(独立组件,见 design.md §7)         │
   └───────────────────────────┬─────────────────────────────────┘
                              │ PIPE 4-signal (轻量)
                              ▼
   ┌─────────── PCIe PHY BFM (opensource) ─────────────────────┐
   │  - alexforencich/verilog-pcie (MIT, 1621★, Gen3/4)        │
   │  - wyvernSemi/pcievhost (C model, Gen1/2)                 │
   │  - 自建 PIPE 数字 BFM(Gen5 速率切换)                      │
   └───────────────────────────┬───────────────────────────────┘
                              │ PIPE 4-signal
                              ▼
   ┌════════════ PcieEndpointIP CppTLM (核心) ═════════════════┐
   │ §1 PHY Digital Ctrl (LTSSM + Eq + 热插拔)                  │
   │ §2 Link Layer (DLLP + FC Token Bucket + ACK/NAK + Retry)   │
   │ §3 Transaction Layer (TLP gen/parse, 路由, SR-IOV VF Pool) │
   │ §4 Config Space + MSI-X + ARI                              │
   │ §5 Bypass Mux (Full / Bypass / Partial 3 态)                │
   │ §6 AXI StreamAdapter (master/slave/cfg_slave 多端口)        │
   │ §7 SoC 交互端口 (ChStream 上行 / AXI4 下行)                 │
   └═══════════════════════════════════════════════════════════┘
                              │ AXI4 / AXI4Lite
                              ▼
   ┌─────── SoC 内部 (MemoryCluster / CP / SDMA / etc.) ────────┐
   └────────────────────────────────────────────────────────────┘
```

---

## Acceptance Gate(本伞形 change)

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **UM-G1** 7 个文件全部产出(README / proposal / design / decisions / roadmap / tasks / spec) | Sisyphus | ⏳ | `openspec validate 2026-09-01-cpptlm-dgpu-pcie-ip-microarch` PASS |
| **UM-G2** 7 个子 change 描述完整(proposal + design outline + tasks outline) | Sisyphus | ⏳ | roadmap.md §4-§10 全部填写 |
| **UM-G3** 顶层 spec 包含 **9 个** ADDED Requirements(Oracle Top-10 修订)与现有 `pcie-endpoint` spec delta 一致 | Sisyphus | ⏳ | `openspec validate --strict` PASS |
| **UM-G4** 决策矩阵 17 项决策(Q1-Q17)有 spec 引用 + 推荐 + 复杂度估计 | Sisyphus | ⏳ | decisions.md §1-§17 全部填写(9 原 + 8 Oracle 新增) |
| **UM-G5** Phase 1 (`link-layer-and-fc`) 任务细化到 TDD 5 步 | Sisyphus | ⏳ | tasks.md §Phase 1 含 Write failing test → Verify fail → Implement → Verify pass → Commit |
| **UM-G6** 23 ABI 兼容性边界在 design.md 明确标记(`CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2`) | Sisyphus | ⏳ | design.md §6 包含 ABI 边界段 |
| **UM-G7** 数据流图至少 4 张 ASCII art(顶层 / EP内部 / 链路层 / PHY数字) | Sisyphus | ⏳ | design.md §2-§5 |

**最终验收**(本 change 完成时):
- [x] UM-G1 ~ UM-G7 全部 ✅
- [x] `openspec validate 2026-09-01-cpptlm-dgpu-pcie-ip-microarch` PASS
- [x] Phase 1 子 change 可独立 fork 创建(`cpptlm-dgpu-pcie-link-layer-and-fc/`)
- [x] 本 change 可独立 archive(不阻塞 Phase 1 实施)

---

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 状态 |
|---|---|---|
| **UsrLinuxEmu** | ADR-088 §D5 23 ABI — 本 change 锁版本 v2,不破坏 ABI | ✅ 无新 API 需求 |
| **PTX-EMU** | 通过 23 ABI 消费 CppTLM — Phase 1-7 不直接变更 ABI | ✅ N/A |
| **chips4all / chili-chips / wyvernSemi** | 开源 BFM wrap — 各自 license 兼容(MIT / BSD-3) | ⏳ Phase 1 启动时确认 |
| **Xilinx pcie-model** | Xilinx example code 授权 — 参考 fork | ⏳ Phase 5 启动时确认 |
| **Cadence / Synopsys VIP** | 商业 license — 不嵌入 CppTLM,仅 reference 验证策略 | ✅ N/A |

---

## 文件清单(本 change 产出)

```
openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/
├── README.md                       # 导航 + Phase 1 启动指南
├── proposal.md                     # 本文件
├── design.md                       # 微架构 + 4 张 ASCII 数据流图
├── decisions.md                    # Q1-Q17 决策点
├── roadmap.md                      # 7 阶段详细路线图
├── specs/
│   └── pcie-ip-microarch/
│       └── spec.md                 # 顶层能力规格(ADDED Requirements)
└── tasks.md                        # 顶层 umbrella 任务 + Phase 1 TDD 5 步
```

---

**起草**: Sisyphus (2026-09-01, per Oracle + Librarian 联合咨询)
**Owner**: CppTLM Team
**状态**: 📋 Proposed — 待 proposal/design/spec 评审后 archive(不进入实施,实施由 Phase 1 子 change 承担)
