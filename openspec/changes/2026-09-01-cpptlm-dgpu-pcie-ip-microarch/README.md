# 2026-09-01-cpptlm-dgpu-pcie-ip-microarch

> **dGPU PCIe IP 微架构 + 7 阶段实施路线图** — 伞形 OpenSpec change
> **状态**: 📋 Proposed — 2026-09-01
> **Owner**: CppTLM Team (Sisyphus)

## 快速导航

| 你想了解... | 看哪个文档 |
|---|---|
| **Why**: 为什么需要这个 change? | [`proposal.md`](./proposal.md) |
| **架构设计 + 数据流图** | [`design.md`](./design.md)(含 4 张 ASCII 数据流图) |
| **关键决策点**(Q1-Q17,9 原 + 8 Oracle 新增) | [`decisions.md`](./decisions.md) |
| **实施路线图 + 子 change 描述** | [`roadmap.md`](./roadmap.md) |
| **能力规格(ADDED Requirements)** | [`specs/pcie-ip-microarch/spec.md`](./specs/pcie-ip-microarch/spec.md) |
| **TDD 5 步任务清单** | [`tasks.md`](./tasks.md) |
| **Phase 1 启动指南** | [本文 §Phase 1 快速启动](#phase-1-快速启动) |

## 一句话总结

把当前只覆盖事务层的 `PcieEndpointTLM` 扩展为**完整 PCIe IP 数字模型**(`PcieEndpointIP`),包括链路层 / PHY 数字控制 / SR-IOV(8-16 VFs)/ 热插拔 / AXI Stream Adapter / 3 态 Bypass Mux,支持 Gen5 32 GT/s,**保持 23 ABI 兼容性**,分 **7 个 phase、24 周**交付。

## 文件清单

```
openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/
├── README.md                              # ← 你在这里
├── proposal.md                            # Why / What / Acceptance Gate / Cross-Repo
├── design.md                              # 微架构 + 4 张 ASCII 数据流图
├── decisions.md                           # Q1-Q17 决策点(9 原 + 8 Oracle 新增)
├── roadmap.md                             # 7 阶段详细路线图 + 每阶段的 OpenSpec Change 描述
├── tasks.md                               # TDD 5 步任务清单
└── specs/
    └── pcie-ip-microarch/
        └── spec.md                        # 9 个 ADDED Requirements
```

## 关键决策一览(详细见 [`decisions.md`](./decisions.md))

| # | 决策点 | 推荐 | Phase |
|---|---|---|---|
| Q1 | Gen5 128b/130b 编码抽象 | **C 透明 + 延迟**(Gen5 ≠ FLIT!) | P2 |
| Q2 | Flow Control 实现 | **B Token Bucket** | P1 |
| Q3 | PIPE 接口抽象 | **C 4-signal 轻量** | P3 |
| Q4 | 开源 PHY BFM | **B 混合**: Xilinx pcie-model + alexforencich | P1+P7 |
| Q5 | 商业验证策略 | **B reference**(不嵌入) | 全程 |
| Q6 | SR-IOV 端口架构 | **B VF Pool** | P4 |
| Q7 | Bypass Mux | **A 3 态 + HostBypass 独立** | P3+P7 |
| Q8 | 23 ABI 兼容(修订) | **A: 23 C 符号冻结 + 类方法 noexcept/[[nodiscard]]**(对象明确,per Oracle Top-1) | P1+全程 |
| Q9 | AXI4Mapper 范围 | **A 独立 + JSON 可选** | P5+P6 |

**关键修正**(Librarian 揭示):**Gen5 ≠ FLIT**(FLIT 是 PCIe 6.0 特性)。Oracle 初次咨询误判,已在 [`decisions.md`](./decisions.md) 修订。

## 7 阶段实施甘特图(Oracle 修订 #6, 2026-09-01)

```
Phase 1: Link Layer + FC Token Bucket      ████████░░░░░░░░  W1-W4   ← 🔴 P0(立即启动)
Phase 5: AXI Stream Adapter                ███░░░░░░░░░░░░░  W1-W3   (与 P1 并行,依赖独立)
Phase 2: 128b/130b Encoding 延迟           ░░░░████░░░░░░░░  W5-W6
Phase 3: PHY Digital Ctrl + Bypass Mux     ░░░░░░████████░  W5-W8   (与 P2 并行)
Phase 4: SR-IOV VF Pool                    ░░░░░░░░░░░████░  W9-W12
Phase 6: AXI4Mapper 独立模块               ░░░░░░░░░░░░░██░  W13-W16
Phase 7: Host Bypass + RC                  ░░░░░░░░░░░██░██  W17-W19(3 周工作量)
                                              ░░░░░░░░░██░  W20-W22 集成测试
                                              ░░░░░░░██░░  W23-W24 缓冲/Bug 修复
═══════════════════════════════════════════════════════════════════════════
总工期: 24 周日历 = ~6 个月

⚠️ 单人全职无并行(并行 = 依赖独立,非实际并发):
- Phase 工作量加总 = 4+2+4+4+3+4+3 = 24 人周 = 24 周日历
- 实际顺序(依赖图): P1 → P5 → P2 → P3 → P4 → P6 → P7
```

## Phase 1 快速启动

### 1. 创建 Phase 1 OpenSpec Change folder

```bash
# 由本 umbrella change 评审通过后执行
mkdir -p openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/specs/link-layer-and-fc
```

### 2. 拷贝模板

Phase 1 子 change 的 `proposal.md` / `design.md` / `specs/link-layer-and-fc/spec.md` 模板可参考:
- **proposal**: 拷贝 [`proposal.md`](./proposal.md),改标题 + 引用 [`roadmap.md` §Phase 1](./roadmap.md)
- **design**: 拷贝 [`design.md` §4](./design.md)(链路层数据流),扩展为完整 Phase 1 design
- **spec**: 拷贝 [`specs/pcie-ip-microarch/spec.md` §LINK_LAYER + §FLOW_CONTROL_TOKEN_BUCKET](./specs/pcie-ip-microarch/spec.md),重组为 Phase 1 spec

### 3. 启动 Phase 1 TDD 5 步(完整任务见 [`tasks.md`](./tasks.md))

```bash
# Step 1: Write failing test (PcieDllpBundle)
$EDITOR test/test_pcie_dllp_bundle.cc  # 写测试
ctest -R test_pcie_dllp_bundle         # → FAIL

# Step 2: Verify fail
# 确认测试因"头文件不存在"而 FAIL,而非其他原因

# Step 3: Implement(最小化代码)
$EDITOR include/bundles/pcie_dllp_bundles_tlm.hh  # 最小化实现

# Step 4: Verify pass
ctest -R test_pcie_dllp_bundle         # → PASS

# Step 5: Commit
git add . && git commit -m "feat(pcie-dllp): add PcieDllpBundle with type+vc+credit fields"
```

### 4. Phase 1 Acceptance Gate(必须全部通过才能 archive)

- [x] P1-G1: `cmake --build build` 通过 + **7 个新测试 PASS**(Oracle C7 修订: 4 → 7,含 Q15 error injector + Q17 下行 Rx)
- [x] P1-G2: DLLP gen/parse 单元测试 PASS
- [x] P1-G3: FC Token Bucket 单元测试 PASS
- [x] P1-G4: ACK/NAK 重传测试 PASS
- [x] P1-G5: PcieEndpointTLM + LL 集成测试 PASS(无回归)
- [x] P1-G6: 23 ABI 边界(修订,per Oracle Top-1/Top-9): `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏 + 类方法 `noexcept`/`[[nodiscard]]` 纪律;**不**在旧类加 `[[deprecated]]`(推迟到 PcieEndpointIP 可用)
- [x] P1-G7: 全量 `ctest -j$(nproc)` 无回归

## 关联文档

| 文档 | 路径 |
|---|---|
| 当前 PcieEndpointTLM(基线) | `include/tlm/gpu/pcie_endpoint_tlm.h` |
| PCIe Bundle(已存在) | `include/bundles/pcie_bundles_tlm.hh` |
| PCIe Endpoint 旧 change(2026-08-26 archive) | `openspec/changes/archive/2026-08-29-2026-08-26-cpptlm-dgpu-pcie-endpoint/` |
| ADR-SOC-07(PCIe slave 归属 SOC) | `docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md` |
| ADR-088(23 ABI 32-C) | `docs/superpowers/specs/` |
| Oracle 咨询记录 | `bg_661f4ee7` (2026-09-01) |
| Librarian 调研记录 | `bg_07392e0c` (2026-09-01) |

## 风险与缓解(摘要,完整见 [`roadmap.md`](./roadmap.md) §风险总览)

| ID | 风险 | 缓解 |
|---|---|---|
| R1 | 23 ABI 破坏 | `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` + 严格 noexcept |
| R2 | SR-IOV 端口爆炸 | VF Pool 共享 StreamAdapter |
| R3 | FC 反压死锁 | Token Bucket 容量上限 + 测试覆盖 |
| R4 | Bypass 状态泄漏 | `apply_mode()` 强制 flush pending 状态 |
| R5 | AXI4Mapper 耦合 | AXI4Mapper 独立模块 |
| R6 | LTSSM 异步 vs TLM 同步 | 内部状态机 + EventQueue 建模异步 |
| R8 | 单工程师 24 周超期 | 单人顺序 P1→P5→P2→P3→P4→P6→P7;P7 日历 W23-W24 是 Bug 修复期(无 24 周外的额外缓冲) |

## 评审清单(reviewer 用)

评审本 umbrella change 时,请检查:

- [ ] **架构合理性**([`design.md`](./design.md)):7 个内部组件边界清晰,数据流图准确
- [ ] **决策完备性**([`decisions.md`](./decisions.md)):Q1-Q17 17 项决策都有 spec 引用 + 推荐 + 复杂度
- [ ] **路线图可执行性**([`roadmap.md`](./roadmap.md)):7 阶段工期合理(总 24 周),依赖图无循环
- [ ] **Phase 1 完整性**([`tasks.md`](./tasks.md) §Phase 1):TDD 5 步任务详细到"1 天内可完成"
- [ ] **spec 一致性**([`specs/pcie-ip-microarch/spec.md`](./specs/pcie-ip-microarch/spec.md)):9 个 ADDED Requirements 与 `pcie-endpoint` archive spec 无冲突
- [ ] **23 ABI 兼容**([`decisions.md` §Q8](./decisions.md)):v2 锁定边界 + noexcept + [[nodiscard]] 措施到位
- [ ] **跨仓协调**([`proposal.md` §Cross-Repo](./proposal.md)):不破坏 PTX-EMU / UsrLinuxEmu / 开源 BFM license

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Proposed — 等待评审后 archive(不进入实施,实施由 Phase 1 子 change 承担)
