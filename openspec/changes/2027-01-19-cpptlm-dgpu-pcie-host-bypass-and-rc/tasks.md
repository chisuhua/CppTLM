# cpptlm-dgpu-pcie-host-bypass-and-rc: Phase 7 Tasks

> **配套**: [`proposal.md`](../proposal.md) · [`specs/host-bypass-and-rc/spec.md`](../specs/host-bypass-and-rc/spec.md)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置**: Phase 1-6 已完成（PCIe Link Layer / Encoding / PHY / SR-IOV / AXI Stream Adapter / AXI4Mapper）
> **Phase 7 在 Phase 6 Oracle 复评放行后启动**

---

## Phase 7 文件清单

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/host_bypass_tlm.hh` + `src/tlm/pcie/host_bypass_tlm.cc` | **新**: HostBypassTLM（独立组件，桥接 AXI ↔ PcieEndpointIP） |
| `include/tlm/pcie/pcie_root_complex_tlm.hh` + `.cc` | **新**: PcieRootComplexTLM（可选 RC 镜像） |
| `test/test_host_bypass_basic.cc` | **新**: Host Bypass 基础测试 |
| `test/test_host_bypass_software_bringup.cc` | **新**: 软件 bring-up 场景测试 |
| `test/test_pcie_root_complex_enumeration.cc` | **新**: PCIe 枚举测试（若 RC 实现） |
| `examples/demo_pcie_full_e2e.py` | **新**: E2E demo（PcieEndpointIP ↔ Host） |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 显式注册源与测试 |

---

## 严格 TDD 5 步（每个子任务）

### T-P7-1: HostBypassTLM 基础桥接

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_host_bypass_basic.cc`（AXI Bridge 路径基础读写） |
| ⏳ 2 | 跑 → **FAIL** |
| ⏳ 3 | 创建 `include/tlm/pcie/host_bypass_tlm.hh` + `src/tlm/pcie/host_bypass_tlm.cc` |
| ⏳ 4 | 跑 → **PASS** |
| ⏳ 5 | 提交:`feat(pcie-host): HostBypassTLM 基础桥接 (AXI ↔ PcieEndpointIP)` |

**测试场景**：
- ✅ 软件经 HostBypassTLM 发起对 EP 访问，事务经 AXI 送达 PcieEndpointIP
- ✅ 响应正确返回，无丢事务

### T-P7-2: 软件 bring-up 场景

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_host_bypass_software_bringup.cc` |
| ⏳ 2 | 跑 → **FAIL** |
| ⏳ 3 | 扩展 `host_bypass_tlm` — 配置空间写/读回/BAR 访问 |
| ⏳ 4 | 跑 → **PASS** |
| ⏳ 5 | 提交:`feat(pcie-host): 软件 bring-up 场景 (config space + BAR)` |

**测试场景**：
- ✅ 配置空间写 → 读回值正确
- ✅ BAR 访问经 AXI 路由到 EP

### T-P7-3: PcieRootComplexTLM（可选 RC 镜像）

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_root_complex_enumeration.cc` |
| ⏳ 2 | 跑 → **FAIL** |
| ⏳ 3 | 创建 `include/tlm/pcie/pcie_root_complex_tlm.hh` + `.cc`（枚举 + 配置访问） |
| ⏳ 4 | 跑 → **PASS** |
| ⏳ 5 | 提交:`feat(pcie-rc): PcieRootComplexTLM 枚举 + 配置访问` |

**测试场景**：
- ✅ 设备/功能被发现，配置空间可读
- ✅ 分配 BAR，访问路由到 EP

### T-P7-4: E2E demo

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `examples/demo_pcie_full_e2e.py`（PcieEndpointIP ↔ Host 全链路） |
| ⏳ 2 | 跑 → **FAIL**（或无对应 Python 测试则先验证 demo 可执行） |
| ⏳ 3 | 实现 demo + 必要接线 |
| ⏳ 4 | 跑 → **PASS** |
| ⏳ 5 | 提交:`feat(demo): PcieEndpointIP ↔ Host E2E demo` |

**测试场景**：
- ✅ 全链路端到端成功
- ✅ 输出可验证结果（事务计数/状态）

### T-P7-5: CMake + 全量验证

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 修改 `src/CMakeLists.txt` + `test/CMakeLists.txt` — 注册新源与测试 |
| ⏳ 2 | `cmake --build build -j$(nproc)` |
| ⏳ 3 | `ctest -R "host_bypass|root_complex"` + 全量 `[pcie]` 无回归 |
| ⏳ 4 | `openspec validate --changes --strict` PASS |
| ⏳ 5 | 提交:`build(pcie): register Phase 7 sources in CMakeLists` |

---

## Phase 7 Acceptance Gate

| Gate | 验证 |
|---|---|
| **P7-G1** | `test_host_bypass_basic` PASS |
| **P7-G2** | `test_host_bypass_software_bringup` PASS |
| **P7-G3** | `test_pcie_root_complex_enumeration` PASS（若 RC 实现） |
| **P7-G4** | `python examples/demo_pcie_full_e2e.py` 运行成功 |
| **P7-G5** | 全量 `[pcie][axi]` ctest 无回归（Phase 1-6 + 7 全部 PASS） |
| **P7-G6** | `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（23 ABI 保护） |
| **P7-G7** | `openspec validate 2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc --strict` PASS |

---

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: HostBypass AXI 桥接丢事务 | TDD 测试覆盖响应完整返回 |
| R2: RC 枚举与 EP 配置空间不一致 | TDD 测试覆盖枚举结果与 EP 实际能力 |
| R3: E2E demo 依赖复杂拓扑 | demo 最小化：PcieEndpointIP + HostBypass 单链路 |
| R4: 真实 PHY wrap 范围膨胀 | alexforencich/verilog-pcie 为可选 submodule，本 Phase 不强制 |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — Phase 7 准备中（待 Phase 6 修复完成后启动实施）
