# cpptlm-dgpu-pcie-ip-integration: Phase 8 Tasks

> **配套**: [`proposal.md`](../proposal.md) · [`specs/pcie-ip-integration/spec.md`](../specs/pcie-ip-integration/spec.md)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置**: Phase 1-7 全部完成（含 Phase 7 Oracle PASS 含 M1/M2 条件）
> **Phase 8 在 Phase 7 Oracle 复评放行后启动**

---

## Phase 8 文件清单

| 文件 | 用途 |
|---|---|
| `docs/architecture/14-pcie-ip-microarchitecture.md` | **新**: 完整架构文档（由 umbrella design.md 931 行迁移） |
| `examples/dgpu_soc_with_pcie_ip.json` | **新**: 完整 dGPU SOC + PCIe EP 示例配置 |
| `test/test_pcie_endpoint_ip_full_e2e.cc` | **新**: 全链路 E2E 测试（解决 Phase 7 Oracle M1） |
| `include/chstream_register.hh` | **改**: 移除 `PcieEndpointTLM` 注册（保留 `PcieEndpointIP`） |
| `include/tlm/gpu/pcie_endpoint_tlm.h` | **改**: 加 `[[deprecated("use PcieEndpointIP")]]` 标注 |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 注册新 E2E 测试 |

---

## 严格 TDD 5 步（每个子任务）

### T-P8-1: 架构文档迁移

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `docs/architecture/14-pcie-ip-microarchitecture.md` 索引（指向 umbrella design.md 历史位置） |
| ⏳ 2 | N/A（文档无需 FAIL/PASS 验证） |
| ⏳ 3 | 从 `openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md` 迁移内容（931 行），按 Phase 1-7 章节组织，加入 Phase 7 Oracle M2 标注（RC 枚举 PF0-only） |
| ⏳ 4 | 文档链接验证（Phase 7 评审提到 "见 design.md §6.4" 等可跳转） |
| ⏳ 5 | 提交:`docs(arch): migrate 14-pcie-ip-microarchitecture from umbrella design.md` |

**验收**：
- ✅ 内容覆盖 Phase 1-7 全部
- ✅ 标注 Phase 7 Oracle M2：RC 枚举为 PF0-only 简化模型
- ✅ 链接到 umbrella design.md 历史位置
- ✅ `openspec validate` PASS

### T-P8-2: dGPU SOC + PCIe EP 示例配置

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `examples/dgpu_soc_with_pcie_ip.json`（PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM + AXI4Mapper 可选注入 + 连接） |
| ⏳ 2 | `cmake --build build --target validate_topology` → **FAIL**（文件缺失） |
| ⏳ 3 | 补充 modules/connections 配置（含 `axi4_mapper_inject: true`） |
| ⏳ 4 | `validate_topology` → **PASS** |
| ⏳ 5 | 提交:`feat(demo): dgpu_soc_with_pcie_ip.json 完整示例配置` |

**验收**：
- ✅ `validate_topology` PASS
- ✅ 包含 PcieEndpointIP（17 端口）、HostBypassTLM、PcieRootComplexTLM
- ✅ AXI4Mapper 可选注入

### T-P8-3: 全链路 E2E 测试（解决 Phase 7 Oracle M1）

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_endpoint_ip_full_e2e.cc`（HostBypass → PcieEndpointIP 真实数据路径） |
| ⏳ 2 | 跑 → **FAIL** |
| ⏳ 3 | 实现 HostBypass/RC ↔ PcieAxiAdapter AXI 真实接线（master_out ↔ slave_in 双向 + ep.tick() 驱动） |
| ⏳ 4 | 跑 → **PASS**（EP 真实消费 AXI 请求并返回真实响应，不是测试手写注入） |
| ⏳ 5 | 提交:`feat(pcie-ep): full E2E test with real AXI data path closure (resolves Phase 7 M1)` |

**验收**：
- ✅ 测试断言 EP 真实处理请求（不是手动注入响应）
- ✅ 配置写经 AXI 到达 EP 配置空间真实源
- ✅ BAR 写经 AXI 到达 EP 内部处理后返回真实响应

### T-P8-4: 旧代码迁移

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `[[deprecated("use PcieEndpointIP")]]` 标注到 `PcieEndpointTLM` |
| ⏳ 2 | 从 `chstream_register.hh` 移除 `registerObject<PcieEndpointTLM>` 行 |
| ⏳ 3 | 实现迁移 |
| ⏳ 4 | 编译验证（应产生 deprecated 警告但不失败）+ 全量测试无回归 |
| ⏳ 5 | 提交:`refactor(pcie): deprecate PcieEndpointTLM, migrate to PcieEndpointIP` |

**验收**：
- ✅ `PcieEndpointTLM` 加 `[[deprecated]]` 警告
- ✅ `chstream_register.hh` 不再注册 `PcieEndpointTLM`
- ✅ 全量测试无回归

### T-P8-5: CMake + 全量验证

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 修改 `src/CMakeLists.txt` + `test/CMakeLists.txt` 注册 `test_pcie_endpoint_ip_full_e2e` |
| ⏳ 2 | `cmake --build build -j$(nproc)` |
| ⏳ 3 | `ctest -R pcie_endpoint_ip_full_e2e` + 全量 `[pcie][axi]` 无回归 |
| ⏳ 4 | `openspec validate --changes --strict` PASS |
| ⏳ 5 | 提交:`build(integration): register Phase 8 full E2E test` |

---

## Phase 8 Acceptance Gate

| Gate | 验证 |
|---|---|
| **P8-G1** | `test_pcie_endpoint_ip_full_e2e` PASS（EP 真实消费 AXI 请求） |
| **P8-G2** | `examples/dgpu_soc_with_pcie_ip.json` 通过 `validate_topology` |
| **P8-G3** | 架构文档含 Phase 7 Oracle M2 标注（RC 枚举 PF0-only） |
| **P8-G4** | `PcieEndpointTLM` 加 `[[deprecated]]` 标注 + `chstream_register.hh` 移除注册 |
| **P8-G5** | 全量 `[pcie][axi]` ctest 无回归（Phase 1-7 + 8 全部 PASS） |
| **P8-G6** | `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（23 ABI 冻结：仅加 deprecated 标注，不修改布局） |
| **P8-G7** | `openspec validate 2027-02-09-cpptlm-dgpu-pcie-ip-integration --strict` PASS |

---

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: 旧代码迁移破坏 ABI（必须加 deprecated 但不改布局） | 仅加 `[[deprecated]]` 属性 + chstream_register.hh 移除 registerObject 行，不修改任何虚拟方法或成员 |
| R2: AXI 真实数据路径接线复杂（M1） | TDD 严格：测试断言 EP 真实处理，否则视为未闭环 |
| R3: 架构文档与代码实现偏差 | 文档迁移后由 Oracle 复评验证 |
| R4: validate_topology 对新 JSON schema 严格 | 参考 configs/apu_soc_v1.json 等现有完整示例 |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — Phase 8 最终整合交付中
