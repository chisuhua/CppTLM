# cpptlm-dgpu-axi4-mapper: Phase 6 Tasks

> **配套**: [`proposal.md`](../proposal.md) · [`specs/axi4-mapper/spec.md`](../specs/axi4-mapper/spec.md)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置**: Phase 1-5 已完成（PCIe Link Layer / Encoding / PHY / SR-IOV / AXI Stream Adapter）
> **Phase 6 在 Phase 5 Oracle 复评放行后启动**

---

## Phase 6 文件清单

| 文件 | 用途 |
|---|---|
| `include/framework/axi4_signal_to_bundle.hh` + `src/framework/axi4_signal_to_bundle.cc` | **新**: AXI 信号 ↔ Axi4Bundle 转换 |
| `include/framework/axi4_bundle_to_signal.hh` + `src/framework/axi4_bundle_to_signal.cc` | **新**: Axi4Bundle ↔ AXI 信号 转换 |
| `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc` | **新**: AXI4 ↔ Bundle Mapper（outstanding 跟踪 + OOO 调度） |
| `test/test_axi4_mapper_basic.cc` | **新**: AXI4 ↔ Bundle 基础测试 |
| `test/test_axi4_mapper_outstanding.cc` | **新**: Outstanding transaction 测试 |
| `test/test_axi4_mapper_out_of_order.cc` | **新**: Out-of-order completion（rid 关联） |
| `test/test_axi4_mapper_integration_with_pcie.cc` | **新**: 与 PcieEndpointIP 集成测试 |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 显式注册源与测试 |

---

## 严格 TDD 5 步（每个子任务）

### T-P6-1: AXI 信号 ↔ Axi4Bundle 转换

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_axi4_mapper_basic.cc`（字段无损往返 + 反向还原） |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 创建 `include/framework/axi4_signal_to_bundle.hh` + `.cc` 和 `axi4_bundle_to_signal.hh` + `.cc` |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(axi): AXI4 signal ↔ Axi4Bundle 无损转换` |

**测试场景**：
- ✅ bundle_to_signal: 所有字段写入信号侧（awaddr/awlen/awsize/awburst/awid/wdata/wstrb/wlast/bid/bresp/araddr/arlen/arsize/arburst/arid/rid/rdata/rresp/rlast）
- ✅ signal_to_bundle 反向还原一致
- ✅ 边界: 空 bundle 转换不崩溃

### T-P6-2: Axi4Mapper outstanding 跟踪

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_axi4_mapper_outstanding.cc` |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 创建 `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc`（outstanding 容量上限，N+1 拒绝） |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(axi): Axi4Mapper outstanding tracking (capacity N, N+1 reject)` |

**测试场景**：
- ✅ 读写独立 ID 空间（awid/arid 各自计数）
- ✅ 填满 N → 第 N+1 个拒绝
- ✅ 完成一个后释放槽位，可再注册

### T-P6-3: OOO completion（rid 关联）

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_axi4_mapper_out_of_order.cc` |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 扩展 `axi4_mapper` — rid 关联乱序 rdata 回原事务 |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(axi): OOO completion via rid association` |

**测试场景**：
- ✅ 多 outstanding 读（arid=A/B/C），下游乱序返回 rdata（rid 乱序）
- ✅ 每个 rdata 经 rid 关联回原事务，正确完成
- ✅ 乱序完成不影响其他 outstanding（不匹配不消耗）

### T-P6-4: 与 PcieEndpointIP 集成

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_axi4_mapper_integration_with_pcie.cc`（JSON `axi4_mapper_inject: true`） |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | wire AXI4Mapper 到 PcieEndpointIP AXI 数据路径（经 Phase 5 Axi4StreamAdapter） |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-axi): Axi4Mapper injectable into PcieEndpointIP data path` |

**测试场景**：
- ✅ JSON `axi4_mapper_inject: true` 时 mapper 注入
- ✅ 端到端读写事务经 mapper 正确路由与完成
- ✅ `axi4_mapper_inject` 缺省/false 时不注入（无回归）

### T-P6-5: CMake + 全量验证

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 修改 `src/CMakeLists.txt` + `test/CMakeLists.txt` — 注册新源与测试 |
| ✅ 2 | `cmake --build build -j$(nproc)` |
| ✅ 3 | `ctest -R "axi4_mapper"` + 全量 `[pcie]` 无回归 |
| ✅ 4 | `openspec validate --changes --strict` PASS |
| ✅ 5 | 提交:`build(axi): register Phase 6 sources in CMakeLists` |

---

## Phase 6 Acceptance Gate

| Gate | 验证 |
|---|---|
| **P6-G1** | `test_axi4_mapper_basic` PASS（AXI4 ↔ Bundle 基础映射） |
| **P6-G2** | `test_axi4_mapper_outstanding` PASS（outstanding 跟踪） |
| **P6-G3** | `test_axi4_mapper_out_of_order` PASS（OOO completion） |
| **P6-G4** | `test_axi4_mapper_integration_with_pcie` PASS（PcieEndpointIP 集成） |
| **P6-G5** | 全量 `[pcie][axi]` ctest 无回归（Phase 1-5 + 6 全部 PASS） |
| **P6-G6** | `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（23 ABI 保护） |
| **P6-G7** | `openspec validate 2026-12-22-cpptlm-dgpu-axi4-mapper --strict` PASS |

---

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: rid 关联错误导致 OOO 完成到错误事务 | TDD 严格断言 per Scenario "Out-of-order completion" |
| R2: outstanding 容量泄漏（完成不释放） | TDD 测试覆盖完成一个释放一个 + N+1 拒绝 |
| R3: 与 PcieEndpointIP 集成耦合 | mapper 独立模块 + JSON 可选注入，无硬编码依赖 |
| R4: 512-bit wdata 名义宽度（Phase 5 M1） | 遵循 Phase 5 spec 已知限制，本 Phase 不假定真实 512-bit 数据宽度 |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: ✅ Tasks — Phase 6 完成（T-P6-1..T-P6-5 全部 PASS，Oracle 复评待启动）
