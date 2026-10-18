# cpptlm-pcie-aer-ats-pri: PCIe AER (Advanced Error Reporting) + ATS/PRI (IOMMU 加速)

> **状态**: 📋 Draft (待 X2 归档后落盘到 `openspec/changes/`)
> **估时**: ~15d 单人 · **起点**: X2 (`cpptlm-pcie-power-management`) completion tag
> **关联**:
> - 前置: X1 (HostPcieTestStub + error_inject API) + X2 (PM power state)
> - 后续: 无 (本 change 为 PCIe host 视角 5 项特性收尾)
> **平行**: `cpptlm-dgpu-soc-internal-noc-e1` (Draft, 暂停), `cpptlm-p2-integration-unblock` (Proposed, 异步)
> **冻结面**: 23 ABI 签名零 diff (per ADR-DGPU-08) + 4 frozen headers 零 diff

---

## Why

X1 (FLR + Resizable BAR) + X2 (PM) 完成后,dGPU SoC 已具备 PCIe 设备基本电源生命周期管理。**但容错 (AER) 与 IOMMU 加速 (ATS/PRI) 仍零实施**,driver 视角的完整 PCIe 协议层闭环仍缺失:

- **AER** (Advanced Error Reporting): PCIe 错误检测 + 报告 + 恢复协议。真实 dGPU 必须实现 uncorrectable error (poisoned TLP, completion timeout, unexpected completion) + correctable error (replay timer, replay num rollover) + Error Reporting 上报 host driver
- **ATS** (Address Translation Services): 设备发起 IOMMU 翻译请求,host IOMMU 返回物理地址。PCIe spec 必选扩展能力
- **PRI** (Page Request Interface): 设备请求 host 预取 page + Invalidate 消息同步

**主因(按 ROI 排序)**:

1. **AER 协议零实施**: uncorrectable / correctable error 检测 + 报告 + 恢复全零
2. **ATS/PRI 协议零实施**: 已有 `cpptlm_dma_translate_cb` 同步签名,但 ATS protocol 零
3. **故障注入无法验证**: 缺 AER 链路,故障注入测试无法闭环

**预期收益**:

- 完整 AER 错误检测 + 报告 + 恢复 (uncorrectable + correctable)
- ATS protocol + TranslationRequest + Invalidate 消息
- 错误注入 + 故障恢复 E2E 测试
- IOTLB 模型 + host-side translation cache

---

## What Changes

### 新增文件 (~5 文件, ~1200 行)

| 文件 | 行数 | 关键 API |
|------|:----:|----------|
| `include/tlm/pcie/pcie_aer_engine.hh` | ~300 | `PcieAerEngine` (uncorrectable / correctable error 检测 + 上报) |
| `src/tlm/pcie/pcie_aer_engine.cc` | ~400 | 错误注入钩子 + 错误恢复路径 |
| `include/tlm/pcie/pcie_ats_engine.hh` | ~250 | `PcieAtsEngine` (TranslationRequest + Invalidate + IOTLB) |
| `src/tlm/pcie/pcie_ats_engine.cc` | ~300 | ATS protocol 状态机 |
| `include/abi/cpptlm_emulator_aer.h` | ~80 | `cpptlm_emulator_inject_aer_error()` + `cpptlm_emulator_get_aer_status()` |
| `include/abi/cpptlm_emulator_ats.h` | ~100 | `cpptlm_emulator_ats_translate()` + `cpptlm_emulator_ats_invalidate()` |
| `test/test_pcie_aer_e2e.cc` | ~300 | AER 端到端 + 错误注入 |
| `test/test_pcie_ats_e2e.cc` | ~250 | ATS TranslationRequest + Invalidate |

### 修改文件 (~5 文件, ~500 行)

| 文件 | 改动 | 说明 |
|------|------|------|
| `include/tlm/pcie/pcie_endpoint_ip.hh` | + AER + ATS capability 字段 + 入口 | per PCIe spec Extended Capability |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | + AER / ATS 消息处理 | 错误检测 + translation request 转发 |
| `include/tlm/gpu/dgpu_board_shell.hh` | + AER + ATS 23 ABI 入口 | 暴露 API |
| `src/tlm/gpu/dgpu_board_shell.cc` | + AER / ATS 路由 | 与 PcieEndpointIP 协同 |
| `test/test_pcie_host_test_stub.cc` (X1) | + error_inject API | 配合 X1 HostPcieTestStub 提供错误注入点 |

### Frozen surface (0 修改, per ADR-DGPU-08)

- 23 ABI 0 修改,新追加 `cpptlm_emulator_aer.h` + `cpptlm_emulator_ats.h`
- 4 frozen headers 0 diff

---

## Non-Goals

- ❌ **FLR / Resizable BAR / PM** — 属 X1/X2
- ❌ **Multi-VC NoC (E.2)** — 推 ArchForge
- ❌ **跨仓 UsrLinuxEmu driver** — 不在 scope
- ❌ **Linux kernel 端 IOMMU 真实交互** — CppTLM 仓内只做 device-side ATS protocol, host-side IOMMU 仿真即可,真实内核集成在 UsrLinuxEmu 仓

---

## Capabilities

### New Capabilities

- **`pcie-aer`**: AER 错误检测 + 报告 + 恢复 (uncorrectable + correctable)
- **`pcie-ats-pri`**: ATS TranslationRequest + Invalidate + IOTLB
- **`pcie-fault-injection`**: 错误注入钩子 (per X1 HostPcieTestStub 扩展)

### Modified Capabilities

- `pcie-host-test-infrastructure` (X1): + `error_inject()` API 完整化
- `pcie-ip-integration`: + AER / ATS Extended Capability 字段

---

## Acceptance Gate (10 项)

- [ ] **G1**: `openspec validate cpptlm-pcie-aer-ats-pri --strict` PASS
- [ ] **G2**: AER uncorrectable error ≥3 cases (poisoned TLP, completion timeout, malformed TLP)
- [ ] **G3**: AER correctable error ≥2 cases (replay timer, replay num rollover)
- [ ] **G4**: AER 错误恢复 ≥1 case (driver 可读 Error Status register + clear)
- [ ] **G5**: ATS TranslationRequest ≥2 cases (hit / miss)
- [ ] **G6**: ATS Invalidate ≥1 case (host invalidate 消息正确处理)
- [ ] **G7**: IOTLB 模型 ≥2 cases (TLB hit / TLB miss / TLB flush)
- [ ] **G8**: HostPcieTestStub error injection API 闭环 (per X1 扩展)
- [ ] **G9**: 12 套件 0 回归
- [ ] **G10**: 4 frozen headers 0 diff + 23 ABI 0 修改

---

## Risks

| # | 风险 | 等级 | 缓解 |
|---|------|:----:|------|
| 1 | AER 错误恢复与 FLR (X1) 状态机互锁 | 🟡 中 | AER 仅在 PCIe state active 时检测, FLR 期间 AER suspended; FLR 不清零 AER log (per PCIe spec) |
| 2 | ATS TranslationRequest 与 `cpptlm_dma_translate_cb` 同步签名耦合 | 🟡 中 | TDD 5 步: 先测 ATS 协议状态机, 再测 translate cb 触发, 最后测 IOTLB cache |
| 3 | IOTLB 模型与 X2 PM D-state 状态机关联 | 🟡 中 | D3cold → IOTLB flush; D0 active → IOTLB 正常 |
| 4 | AER/ATS Extended Capability 字段与 PcieEndpointIP 既有 16 字段 layout 冲突 | 🟡 中 | Extended Capability 用独立 field 区 (per PCIe spec), 不破既有 layout |
| 5 | 错误注入 API 误触发 host-side 不可观测行为 | 🟡 中 | error_inject API 仅在 in-process fixture 模式下生效, 不影响 release build |

---

## Timeline (15d)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| 1 | AER capability + uncorrectable error 检测 + 上报 | 4d |
| 2 | AER correctable error + 错误恢复 | 2d |
| 3 | ATS capability + TranslationRequest + IOTLB | 4d |
| 4 | ATS Invalidate 消息处理 | 1.5d |
| 5 | 错误注入 + E2E 测试 + 文档 | 3d |
| 6 | 文档 + openspec archive | 0.5d |

---

## 关联资源

- 起点 tag: X2 completion (`cpptlm-pcie-power-management-completion`)
- 既有 hook: `cpptlm_dma_translate_cb` 同步签名 in `include/abi/cpptlm_emulator.h`
- X1 依赖: `HostPcieTestStub::error_inject()` API (per X1 spec `pcie-host-test-infrastructure`)
- 后续 change: 无 (本 change 收尾 PCIe 5 项特性 host 视角闭环)
- 平行 change: `cpptlm-dgpu-soc-internal-noc-e1` (Draft, 暂停), `cpptlm-p2-integration-unblock` (Proposed, 异步)

---

## 完成 PCIe 5 项特性的最终状态

| Change | 内容 | 估时 | 累计 |
|--------|------|:----:|:----:|
| X1 (cpptlm-pcie-host-test-flr-rebar) | HostPcieTestStub + FLR + Resizable BAR | 11d | 11d |
| X2 (cpptlm-pcie-power-management) | PM (ASPM + D0/D3) | 5d | 16d |
| X3 (cpptlm-pcie-aer-ats-pri) | AER + ATS/PRI | 15d | 31d |
| **总计** | — | **31d (~6 周单人)** | |
