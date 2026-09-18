# cpptlm-p2-integration-unblock: Tasks (9 步对应 P0.5-1..P0.5-9)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md)
> **关联阶段**: `docs/soc_arch/roadmap/phase9-p2-unblock.md` (P0.5-1 创建)
> **P2 关联**: 本 change 是 P2-1 写完后启动的硬前置 (unblock P2-2/3/4/5)

---

## 文件清单 (P0.5)

| 文件 | 变化 | 步骤 |
|------|------|:----:|
| `include/framework/axi4_cache_adapter.hh` | **新** | P0.5-2 |
| `src/framework/axi4_cache_adapter.cc` | **新** | P0.5-2 |
| `include/chstream_register.hh` | 改 | P0.5-2 |
| `include/framework/axi4_stream_adapter.hh` | 改 (暴露桥接点) | P0.5-4 |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 改 (D3/D4 + MSI-X) | P0.5-5 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 改 (tick() 扩展) | P0.5-5 |
| `include/tlm/pcie/host_bypass_tlm.hh` | 改 (msix_delivery_in) | P0.5-6 |
| `src/tlm/pcie/host_bypass_tlm.cc` | 改 (tick MSI-X) | P0.5-6 |
| `include/tlm/gpu/sdma_engine_tlm.hh` | 改 (端口扩展) | P0.5-7 |
| `src/tlm/gpu/sdma_engine_tlm.cc` | 改 (doorbell 触发) | P0.5-7 |
| `examples/dgpu_soc_with_pcie_ip.json` | 改 (sdma + msix) | P0.5-7 |
| `include/tlm/gpu/completion_ring_mvp.hh` | 改 (irq_out) | P0.5-7 |
| `src/tlm/gpu/completion_ring_mvp.cc` | 改 (tick 投递) | P0.5-7 |
| `test/test_axi4_cache_adapter.cc` | **新** (TDD 红→绿) | P0.5-1..3 |
| `test/test_pcie_endpoint_ip_msix_path.cc` | **新** | P0.5-8 |
| `test/test_pcie_endpoint_ip_sdma_wiring.cc` | **新** | P0.5-8 |
| `test/test_pcie_endpoint_ip_completion_ring_wiring.cc` | **新** | P0.5-8 |
| `docs/soc_arch/roadmap/phase9-p2-unblock.md` | **新** | P0.5-1 |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | 改 (§9.4 追加) | P0.5-9 |

---

## 1. P0.5-1: 阶段文件 + 桥接测试骨架 (TDD 红)

- [ ] 1.1 创建 `docs/soc_arch/roadmap/phase9-p2-unblock.md` (阶段文件, 含 9 步路径)
- [ ] 1.2 创建 `test/test_axi4_cache_adapter.cc` 测试文件 (TDD 红, 编译失败预期, **双标签** `[pcie][pcie-ep-soc-bridge]` per G9)
- [ ] 1.3 写测试: `cmake --build build --target cpptlm_tests -j$(nproc)` 应找不到 `Axi4CacheAdapter` 类
- [ ] 1.4 跑 → **FAIL** (确认编译错误)
- [ ] 1.5 推迟 commit

**验收**:
- 阶段文件含 9 步 P0.5-1..P0.5-9 详细路径
- 桥接测试文件含 7+ test cases (AW/W/R/B 转换, OOO, error, 8-bit 冲突)

## 2. P0.5-2: Axi4CacheAdapter 实现 (TDD 绿核心)

- [ ] 2.1 创建 `include/framework/axi4_cache_adapter.hh` (类声明, 2 方向状态机)
- [ ] 2.2 创建 `src/framework/axi4_cache_adapter.cc` (实现: AW→req, AR→req, R→rresp, B→bresp)
- [ ] 2.3 维护 `awid_to_src_id_` / `arid_to_src_id_` / `src_id_to_awid_` / `src_id_to_arid_` 双向映射表
- [ ] 2.4 8-bit 冲突检测 (D2 决策): 同一 src_id 已占用时拒绝新事务
- [ ] 2.5 在 `chstream_register.hh` 添加 `REGISTER_CHSTREAM(Axi4CacheAdapter)` 注册
- [ ] 2.6 跑 → **PASS** (`cmake --build build --target cpptlm_tests -j$(nproc)` + `[pcie-ep-soc-bridge]` 全绿)
- [ ] 2.7 推迟 commit

**验收**:
- `Axi4CacheAdapter` 类完整实现, 2 方向 round-trip 测试 PASS
- OOO 响应匹配测试 PASS (3 outstanding read 乱序返回)
- 8-bit 冲突测试 PASS (冲突时返回 SLVERR)

## 3. P0.5-3: 桥接单测全部通过

- [ ] 3.1 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"` → 至少 7 test cases PASS
- [ ] 3.2 跑 `./build/bin/cpptlm_tests "[pcie]"` → 既有 ≥36,454 assertions 零回归
- [ ] 3.3 验证 G4: Axi4CacheAdapter 2 方向 round-trip PASS
- [ ] 3.4 推迟 commit

**验收**:
- `[pcie-ep-soc-bridge]` 标签 ≥7 assertions PASS
- `[pcie]` 零回归

## 4. P0.5-4: Axi4StreamAdapter 暴露桥接点

- [ ] 4.1 修改 `include/framework/axi4_stream_adapter.hh` 添加 `axi_to_cache_out` / `cache_to_axi_in` 端口
- [ ] 4.2 添加 `set_cache_adapter(Axi4CacheAdapter*)` setter (可选注入)
- [ ] 4.3 跑 → 既有 `[pcie]` 测试零回归 (Axi4StreamAdapter 既有功能不破坏)
- [ ] 4.4 推迟 commit

**验收**:
- Axi4StreamAdapter 新增 2 端口 (out + in) 不破坏既有
- `[pcie]` ≥36,454 assertions PASS

## 5. P0.5-5: PcieEndpointIP::tick() 扩展 (D3/D4 + MSI-X + SDMA 程序化桥接)

- [ ] 5.1 修改 `include/tlm/pcie/pcie_endpoint_ip.hh` (Oracle 修订版):
  - [ ] 5.1.1 新增 `msix_delivery_in` ChPort (ingress, MsiXDeliveryBundle) — 接收 SDMA done_out / CompletionRing irq_out
  - [ ] 5.1.2 新增 `msix_delivery_out` ChPort (egress, MsiXDeliveryBundle) — 主动 push 到 HostBypassTLM
  - [ ] 5.1.3 新增 `axi_master_resp_from_cache` 内部信号 (从 Axi4CacheAdapter 接收)
  - [ ] 5.1.4 添加 `set_host_bypass(HostBypassTLM*)` setter (MSI-X 投递目标)
  - [ ] 5.1.5 添加 `set_sdma_engine(SdmaEngineTLM*)` setter (SDMA 程序化桥接, per design D4 + D7)
  - [ ] 5.1.6 添加 `set_completion_ring(CompletionRingTLM*)` setter (CompletionRing 程序化桥接)
- [ ] 5.2 修改 `src/tlm/pcie/pcie_endpoint_ip.cc` (Oracle 修订版):
  - [ ] 5.2.1 `tick()` 新增 D3 处理: `axi_master_req()` → Axi4CacheAdapter → xbar
  - [ ] 5.2.2 `tick()` 新增 D4 处理: Axi4CacheAdapter 响应 → `axi_master_resp()` 推送
  - [ ] 5.2.3 `tick()` 新增 MSI-X 投递: 从 `msix_delivery_in` 接收 → 聚合到 `msix_delivery_out` (按 vector 编号排序) → 转发到 `host_bypass_->msix_delivery_in`
  - [ ] 5.2.4 `tick()` 新增 SDMA 程序化桥接: 调用 `sdma_->tick()` (D4 修订, 程序化而非 JSON 接线)
  - [ ] 5.2.5 `tick()` 新增 CompletionRing 程序化桥接: 调用 `completion_ring_->tick()`
- [ ] 5.3 跑 → `[pcie]` 既有 ≥36,454 assertions 零回归 (D1/D2 程序化闭环保留)
- [ ] 5.4 推迟 commit

**验收 (Oracle 修订版)**:
- D1/D2 程序化闭环不破坏 (Phase 8 M1 既有测试)
- D3/D4 新桥接路径就位 (代码层面)
- MSI-X 双端口命名严格 (in/out 不混用, spec msix-delivery Scenario 4 验证)
- SDMA 程序化桥接就位 (set_sdma_engine setter + tick 转发, 替代 JSON 接线)
- CompletionRing 程序化桥接就位 (set_completion_ring setter)

## 6. P0.5-6: HostBypassTLM 新增 msix_delivery_in 端口

- [ ] 6.1 修改 `include/tlm/pcie/host_bypass_tlm.hh`:
  - [ ] 6.1.1 新增 `msix_delivery_in` ChPort (ingress, MsiXDeliveryBundle)
  - [ ] 6.1.2 新增 `attach_to_pcie_endpoint(PcieEndpointIP*)` setter (绑定 MSI-X 源)
  - [ ] 6.1.3 新增 `msix_delivery_count()` 统计 API
- [ ] 6.2 修改 `src/tlm/pcie/host_bypass_tlm.cc`:
  - [ ] 6.2.1 `tick()` 新增 MSI-X 投递处理: 从 `msix_delivery_in` 接收 → 累加 `msix_delivery_count_` → 经 ABI `cpptlm_emulator_msix_clear_pending()` 清除 pending
  - [ ] 6.2.2 4 方向闭环 (D1/D2 程序化路径) 不破坏
- [ ] 6.3 跑 → `[pcie]` + `[host-bypass]` 既有测试零回归
- [ ] 6.4 推迟 commit

**验收**:
- HB 新增 1 端口 + tick MSI-X 投递逻辑
- `[pcie]` ≥36,454 assertions PASS
- `[host-bypass]` 既有测试 PASS

## 7. P0.5-7: JSON 配置 + SDMA + CompletionRing 接线 (Oracle 修订版: 程序化桥接)

- [ ] 7.1 修改 `examples/dgpu_soc_with_pcie_ip.json` (Oracle 修订版):
  - [ ] 7.1.1 modules 列表添加 `sdma` 声明 (仅 name, type, params; 无端口连接):
    `{ "name": "sdma", "type": "SdmaEngineTLM", "params": { "max_outstanding": 16, "fence_size": 4096 } }`
  - [ ] 7.1.2 **不添加**任何 sdma.↔pcie_ep. 端口 JSON 连接 (会被 19 §14.2.3 静默丢弃)
  - [ ] 7.1.3 **不添加**任何 completion_ring.↔pcie_ep. 端口 JSON 连接 (走程序化桥接)
  - [ ] 7.1.4 添加 metadata 注释说明程序化桥接路径 (per design D7)
- [ ] 7.2 修改 `include/tlm/gpu/sdma_engine_tlm.hh` (Oracle 修订版):
  - [ ] 7.2.1 新增 `axi_slave_in` / `axi_master_out` ChPort (Axi4Bundle, 供未来 AXI 路径扩展)
  - [ ] 7.2.2 新增 `trigger_doorbell()` API
  - [ ] 7.2.3 header 注释明确 5 端口 bundle 类型 (全 PcieTlpBundle) + 端口索引 (host_out=3, done_out=4)
- [ ] 7.3 修改 `src/tlm/gpu/sdma_engine_tlm.cc`:
  - [ ] 7.3.1 `tick()` 新增 doorbell 处理
  - [ ] 7.3.2 5 端口既有逻辑不破坏
- [ ] 7.4 修改 `include/tlm/gpu/completion_ring_mvp.hh` (Oracle 修订版):
  - [ ] 7.4.1 `irq_out[3]` 端口注释: "→ pcie_ep.msix_delivery_in (程序化桥接, per `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` §14.2.3 限制)"
- [ ] 7.5 修改 `src/tlm/gpu/completion_ring_mvp.cc`:
  - [ ] 7.5.1 `tick()` 输出 MsiXDeliveryBundle {vector=3, ...} 到 `irq_out[3]`
- [ ] 7.6 修改 `include/tlm/pcie/pcie_endpoint_ip.cc`:
  - [ ] 7.6.1 `set_sdma_engine()` setter 实现 (持有 `SdmaEngineTLM*` 指针)
  - [ ] 7.6.2 `set_completion_ring()` setter 实现
  - [ ] 7.6.3 DGpuBoardShell 调用 `ep->set_sdma_engine(sdma)` + `ep->set_completion_ring(cr)` (per Phase 8 setter 模式)
- [ ] 7.7 跑 → `cmake --build build --target validate_topology` PASS (WARN 可接受, 实际数据流由程序化 setter 建立)
- [ ] 7.8 跑 → `[pcie]` + `[sdma]` + `[chstream]` 既有测试零回归
- [ ] 7.9 推迟 commit

**验收 (Oracle 修订版)**:
- `validate_topology` PASS (允许 sdma 端口 WARN, 程序化桥接不依赖 JSON)
- `[sdma]` 既有测试 PASS
- `[pcie]` ≥36,454 assertions PASS
- DGpuBoardShell setter 调用就位 (程序化桥接)

## 8. P0.5-8: 3 个 E2E 测试文件 (Oracle 修订版: 双标签强制)

- [ ] 8.1 创建 `test/test_pcie_endpoint_ip_msix_path.cc`:
  - [ ] 8.1.1 TEST_CASE: EP→HB MSI-X 投递 (vector 0)
  - [ ] 8.1.2 TEST_CASE: 多 vector 优先级 (0,1,2)
  - [ ] 8.1.3 TEST_CASE: EP 无 HB 绑定时降级 (记录错误日志)
  - [ ] 8.1.4 TEST_CASE: msix_delivery_in / out 端口命名严格区分 (spec Scenario 4 验证)
  - [ ] 8.1.5 **双标签** `[pcie][pcie-ep-soc-bridge]` (Oracle G9 强制)
- [ ] 8.2 创建 `test/test_pcie_endpoint_ip_sdma_wiring.cc`:
  - [ ] 8.2.1 TEST_CASE: SDMA doorbell 触发 (程序化桥接路径)
  - [ ] 8.2.2 TEST_CASE: SDMA H2D 数据传输 (mem_out[2] PcieTlpBundle → VRAM)
  - [ ] 8.2.3 TEST_CASE: SDMA 完成中断 (done_out[4] → EP.msix_delivery_in)
  - [ ] 8.2.4 TEST_CASE: SDMA 端口索引正确 (host_out=3, done_out=4)
  - [ ] 8.2.5 **双标签** `[pcie][pcie-ep-soc-bridge]`
- [ ] 8.3 创建 `test/test_pcie_endpoint_ip_completion_ring_wiring.cc`:
  - [ ] 8.3.1 TEST_CASE: CompletionRing IRQ 投递 (程序化桥接, vector=3 → EP.msix_delivery_in)
  - [ ] 8.3.2 TEST_CASE: SDMA done_out[4] + CompletionRing irq_out[3] 共享 EP.msix_delivery_in
  - [ ] 8.3.3 TEST_CASE: header 注释同步 (irq_out[3] → pcie_ep.msix_delivery_in)
  - [ ] 8.3.4 **双标签** `[pcie][pcie-ep-soc-bridge]`
- [ ] 8.4 跑 → 4 文件全部 PASS (含 test_axi4_cache_adapter.cc)
- [ ] 8.5 跑 → `[pcie-ep-soc-bridge]` 总标签 ≥30 assertions PASS (G2)
- [ ] 8.6 跑 → `[pcie]` 总标签 ≥36,700 assertions PASS (G6, 双标签保证包含新测试)
- [ ] 8.7 推迟 commit

**验收 (Oracle 修订版)**:
- 4 文件 ~250 assertions PASS
- `[pcie-ep-soc-bridge]` 总断言 ≥30 (G2)
- `[pcie]` ≥36,700 (G6 双标签数学成立)
- 4 文件**全部双标签** `[pcie][pcie-ep-soc-bridge]` (G9 强制)

## 9. P0.5-9: 全量回归 + Archive + 文档同步

- [ ] 9.1 跑 `cmake --build build -j$(nproc)` → 退出码 0
- [ ] 9.2 跑 `ctest --test-dir build --output-on-failure -j4` → 全 PASS
- [ ] 9.3 跑 `./build/bin/cpptlm_tests "[pcie]"` → ≥36,700 assertions PASS (G6 零回归)
- [ ] 9.4 跑 `./build/bin/cpptlm_tests "[chstream]"` → 155 assertions PASS (G7 零回归)
- [ ] 9.5 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"` → ≥30 assertions PASS (G2)
- [ ] 9.6 跑 `cmake --build build --target validate_topology` → PASS (G5)
- [ ] 9.7 跑 `openspec validate cpptlm-p2-integration-unblock --strict` → PASS (G1)
- [ ] 9.8 修改 `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` §9.4 追加 P2 unblock 状态 (G8)
- [ ] 9.9 修改 AGENTS.md STRUCTURE 节同步 (G8)
- [ ] 9.9b 修改 `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` 新增 §13 EP↔SoC 桥接章节 (G8 Oracle 修订, 含 Axi4CacheAdapter + MSI-X 双端口 + SDMA 程序化桥接 + CompletionRing setter 引用)
- [ ] 9.10 跑 `openspec archive cpptlm-p2-integration-unblock --yes`
- [ ] 9.11 聚合 commit: `feat(p2-unblock): 4 EP↔SoC 集成断点修复 (Axi4CacheAdapter + MSI-X + SDMA + CompletionRing)`
- [ ] 9.12 同步主 spec: `openspec/specs/pcie-ep-soc-noc-axi-bridge/spec.md` 永久落地

**验收**:
- 8 项 Acceptance Gate 全部 ✅
- 既有 `[pcie]` 36,454 → 36,700+ (新增 ≥250 assertions)
- `[chstream]` 155 PASS
- OpenSpec change 已 archive + 主 spec 永久落地

---

## Acceptance Gate (9 项, Oracle 修订版)

| Gate | 验证 | 步骤 |
|------|------|:----:|
| **G1** | `openspec validate cpptlm-p2-integration-unblock --strict` PASS | P0.5-9 |
| **G2** | `[pcie-ep-soc-bridge]` 标签测试 PASS (4 个新测试文件, 双标签 `[pcie][pcie-ep-soc-bridge]`) | P0.5-3, P0.5-8 |
| **G3** | 4 方向 AXI 流方向图全部 ✅ (D1/D2/D3/D4 + MSI-X 双端口 + SDMA 程序化 + CompletionRing) | P0.5-5..8 |
| **G4** | `Axi4CacheAdapter` 桥接 2 方向 round-trip PASS | P0.5-2..3 |
| **G5** | `dgpu_soc_with_pcie_ip.json` `validate_topology` PASS (sdma 声明式 + 程序化桥接, WARN 可接受) | P0.5-7 |
| **G6** | 既有 `[pcie]` 测试零回归 (实测基线 36,454, 双标签保证 G6 数学 ≥36,700) | P0.5-9 |
| **G7** | 既有 `[chstream]` 测试零回归 (实测基线 155) | P0.5-9 |
| **G8** | AGENTS.md + `dgpu-soc-pcie-slice.md` §9.4 + `19-pcie-ip-microarchitecture.md` 新 §13 EP↔SoC 桥接 同步 | P0.5-9 |
| **G9** (Oracle 新增) | 4 个新测试文件**双标签** `[pcie][pcie-ep-soc-bridge]` 验证 (Catch2 标签不嵌套防 G6 失效) | P0.5-8 |

---

## 风险与缓解

| Risk | 等级 | 缓解 |
|------|:----:|------|
| `Axi4CacheAdapter` 状态机设计缺陷 | 🟡 中 | TDD 5 步先写测试 (P0.5-1), 包含 OOO/error/冲突场景 |
| AXI 16-bit ID → 8-bit 压缩冲突 | 🟡 中 | D2 决策: 双向映射表 + 冲突检测 (P0.5-2 step 4) |
| EP tick() 扩展破坏 Phase 8 M1 既有 4 方向闭环 | 🟡 中 | `[pcie]` 回归测试覆盖 (G6), 每步推进后必跑 |
| SDMA 模块加入 JSON 影响 `validate_topology` | 🟡 中 | D4 决策: 3 端口最小化接线, G5 验证 (P0.5-7) |
| MSI-X 单向 push 与既有中断机制冲突 | 🟢 低 | D3 决策: 新端口独立, 不与 AXI 混淆 (P0.5-6) |
| CompletionRing irq_out[3] 含义模糊 | 🟢 低 | D5 决策: 显式连接到 EP msix_delivery_in (P0.5-7) |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — P2 集成 4 断点修复 (Axi4CacheAdapter + MSI-X + SDMA + CompletionRing)
**关键路径**:
1. D0+ 创建本 change (P0.5-1) + 桥接测试骨架 (TDD 红)
2. D1-3 实现 Axi4CacheAdapter (TDD 绿) + 单测全绿
3. D4-6 扩展 EP/HB tick() (D3/D4 + MSI-X)
4. D7 JSON + SDMA + CompletionRing 接线
5. D8 3 个 E2E 测试文件
6. D9+ 全量回归 + archive + 文档同步

---

## 关联

- **proposal**: [`proposal.md`](proposal.md)
- **design**: [`design.md`](design.md)
- **spec**: [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md)
- **P2 主计划**: `docs/soc_arch/roadmap/phase9-p2-cp-attach-via-axi.md`
- **P0.5 阶段文件**: `docs/soc_arch/roadmap/phase9-p2-unblock.md` (P0.5-1 创建)
- **架构文档**: `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`
- **相关 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`
