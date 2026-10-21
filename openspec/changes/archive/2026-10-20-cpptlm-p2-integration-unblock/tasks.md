# cpptlm-p2-integration-unblock: Tasks (Metis + Oracle 2027-09-17 重构: 4 commit 而非 9 步)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md)
> **关联阶段**: ArchForge 仓 `docs/roadmap/phase9-p2-unblock.md` (本 change 异步声明, 创建在 ArchForge)
> **P2 关联**: 本 change 是 P2-1 写完后启动的硬前置 (unblock P2-2/3/4/5)
> **关键修订**:
> - 9 步 P0.5-1..9 → **4 commit 结构** (Metis + Oracle 共识, 同文件多次改不独立回滚)
> - D2 transaction_id 直通, 去 8-bit 压缩 (Oracle 修订)
> - D3 API 修正: `attach_to_endpoint()` 而非误引用的 `set_host_bypass()`
> - 文档仓归属 ArchForge (per AGENTS.md DOC HYGIENE)
> - 测试基线: `[pcie]` 36,598 / 415 cases, `[chstream]` 184 / 46 cases (实测)

---

## 文件清单 (4 commit 结构)

| 文件 | 变化 | Commit |
|------|------|:-----:|
| `include/framework/axi4_cache_adapter.hh` | **新** (~80 行) | Commit 1 |
| `src/framework/axi4_cache_adapter.cc` | **新** (~80 行) | Commit 1 |
| `include/chstream_register.hh` | 改 (注册 Axi4CacheAdapter) | Commit 1 |
| `include/framework/axi4_stream_adapter.hh` | 改 (暴露桥接点) | Commit 1 |
| `test/test_axi4_cache_adapter.cc` | **新** (transaction_id + fragment + OOO + backpressure) | Commit 1 |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 改 (msix 端口 + 3 setter, flag-gated) | Commit 2 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 改 (tick MSI-X 聚合, flag-gated) | Commit 2 |
| `include/tlm/pcie/host_bypass_tlm.hh` | 改 (msix_delivery_in 端口) | Commit 2 |
| `src/tlm/pcie/host_bypass_tlm.cc` | 改 (tick MSI-X 投递) | Commit 2 |
| `test/test_pcie_endpoint_ip_msix_path.cc` | **新** (4 cases, 双标签) | Commit 2 |
| `include/tlm/gpu/sdma_engine_tlm.hh` | 改 (axi_* 端口占位 + trigger_doorbell) | Commit 3 |
| `src/tlm/gpu/sdma_engine_tlm.cc` | 改 (doorbell tick 处理) | Commit 3 |
| `include/tlm/gpu/completion_ring_mvp.hh` | 改 (irq_out[3] header 注释) | Commit 3 |
| `src/tlm/gpu/completion_ring_mvp.cc` | 改 (tick 输出 MsiXDeliveryBundle) | Commit 3 |
| `src/tlm/gpu/dgpu_board_shell.cc` | 改 (init order assert + setter 调用, D8) | Commit 3 |
| `examples/dgpu_soc_with_pcie_ip.json` | 改 (sdma 声明, 无端口接线 per D7) | Commit 3 |
| `test/test_pcie_endpoint_ip_sdma_wiring.cc` | **新** (4 cases, 双标签) | Commit 3 |
| `test/test_pcie_endpoint_ip_completion_ring_wiring.cc` | **新** (3 cases, 双标签) | Commit 3 |
| `ArchForge: docs/roadmap/phase9-p2-unblock.md` | **新** (跨仓 PR) | Commit 4 |
| `ArchForge: docs/microarchitecture/dgpu-soc-pcie-slice.md` §9.4 | 改 (跨仓 PR) | Commit 4 |
| `ArchForge: docs/architecture/19-pcie-ip-microarchitecture.md` 新 §13 | 改 (跨仓 PR) | Commit 4 |
| `AGENTS.md` STRUCTURE 同步 | 改 | Commit 4 |

**总文件数**: 22 (含 4 个 ArchForge 跨仓 PR)

---

## Commit 1: Axi4CacheAdapter (D2 重写 + 单测)

- [ ] 1.1 创建 `include/framework/axi4_cache_adapter.hh` (~80 行, 含 AxiErrorCode enum)
- [ ] 1.2 创建 `src/framework/axi4_cache_adapter.cc` (transaction_id 直通, **不**含 8-bit 压缩, **不**含双向映射表)
- [ ] 1.3 字段映射 (per design §D2):
  - AW: `awid[16] → transaction_id[64]` (直接赋值) + `awaddr/size/is_write/data` 直通 + `awlen+1 → fragment_total` + `awbeat_idx → fragment_id`
  - AR: 同 AW 但 `is_write=false`
  - Resp→R: `transaction_id[15:0] → rid[16]` + `data → rdata` + `error_code → rresp` + `last → rlast`
  - Resp→B: `transaction_id[15:0] → bid[16]` + `error_code → bresp`
- [ ] 1.4 fragment 多拍拆分 (awlen > 0 时生成 fragment_total 个 CacheReq)
- [ ] 1.5 Backpressure (N+1 拒绝语义, 与 Axi4Mapper 对齐, max_outstanding 默认 16)
- [ ] 1.6 AxiErrorCode enum (OK/DECERR/SLVERR/EXOKAY/UNKNOWN 5 值)
- [ ] 1.7 `include/chstream_register.hh` 添加 `REGISTER_CHSTREAM(Axi4CacheAdapter)`
- [ ] 1.8 `include/framework/axi4_stream_adapter.hh` 暴露 `axi_to_cache_out` / `cache_to_axi_in` 端口 (可选 set_cache_adapter setter)
- [ ] 1.9 创建 `test/test_axi4_cache_adapter.cc` (TDD 红→绿, **双标签** `[pcie][pcie-ep-soc-bridge]` per G9)
- [ ] 1.10 测试用例 (覆盖 transaction_id 直通, 不含 8-bit 冲突):
  - AW 写事务转换 (1 拍)
  - AR 读事务转换
  - Cache 响应 → R 通道
  - Cache 响应 → B 通道
  - OOO 响应匹配 (3 outstanding read 乱序)
  - AXI 错误响应 (error_code → rresp/bresp 映射)
  - **多拍 burst 拆分** (awlen=3, 4 拍 fragment)
  - **Backpressure** (容量满拒收)
- [ ] 1.11 跑 `cmake --build build -j$(nproc)` → PASS
- [ ] 1.12 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"` → 8 cases PASS
- [ ] 1.13 跑 `./build/bin/cpptlm_tests "[pcie]"` → 既有 ≥36,598 / 415 cases 0 回归
- [ ] 1.14 commit: `feat(p2-unblock): Axi4CacheAdapter 桥接组件 (transaction_id 直通, 1:1 字段映射)`

**验收 (per design §D2 + §6.5 精简版)**:
- Axi4CacheAdapter 类完整实现, 2 方向 round-trip 测试 PASS
- OOO 响应匹配 (3 outstanding read 乱序) PASS
- 多拍 burst 拆分 (awlen=3) PASS
- AxiErrorCode enum 5 值 PASS
- Backpressure 拒收 PASS
- `[pcie]` 既有 ≥36,598 / 415 cases 0 回归
- 不含 8-bit 冲突逻辑 (旧 design 已废)
- **不**依赖 PCIe 符号 (header git grep 验证)
- 测试文件**双标签** `[pcie][pcie-ep-soc-bridge]`

---

## Commit 2: EP/HB tick MSI-X 扩展 (flag-gated)

- [ ] 2.1 修改 `include/tlm/pcie/pcie_endpoint_ip.hh` (Oracle 修订版, flag-gated 默认关闭):
  - [ ] 2.1.1 新增 `msix_delivery_in` ChPort (ingress, MsiXDeliveryBundle) — 接收 SDMA/CR MSI-X 源
  - [ ] 2.1.2 新增 `msix_delivery_out` ChPort (egress, MsiXDeliveryBundle) — push 到 HB
  - [ ] 2.1.3 新增 `set_host_bypass_(HostBypassTLM*)` setter (实际无 setter, 见 2.1.5)
  - [ ] 2.1.4 新增 `set_xbar_master(Axi4CacheAdapter*)` setter (flag 默认 nullptr, 不破坏 Phase 8 M1)
  - [ ] 2.1.5 新增 `allocate_msix_vector(src_kind)` API (SDMA/CR 经此分配 vector, 避免硬编码冲突)
  - [ ] 2.1.6 `msix_delivery_enabled_` flag (默认 false; setters 调用后才 enable)
- [ ] 2.2 修改 `src/tlm/pcie/pcie_endpoint_ip.cc`:
  - [ ] 2.2.1 `tick()` flag-gated 检查: 仅 `msix_delivery_enabled_` 时聚合 `msix_delivery_in` → `msix_delivery_out`
  - [ ] 2.2.2 `tick()` 调用既有 4 方向程序化闭环 (Phase 8 M1, 不破坏)
  - [ ] 2.2.3 `tick()` 调用既有 attach_to_endpoint 路径 (Phase 8 M1, 不破坏)
  - [ ] 2.2.4 不修改 `axi_master_out` 处理 (保持 Phase 8 M1 既有, D3/D4 由 set_xbar_master 触发)
- [ ] 2.3 修改 `include/tlm/pcie/host_bypass_tlm.hh`:
  - [ ] 2.3.1 新增 `msix_delivery_in` ChPort (ingress, MsiXDeliveryBundle)
  - [ ] 2.3.2 新增 `msix_delivery_count()` 统计 API
  - [ ] 2.3.3 `attach_to_endpoint(EP*)` 既有 API 不动 (Phase 8 M1)
- [ ] 2.4 修改 `src/tlm/pcie/host_bypass_tlm.cc`:
  - [ ] 2.4.1 `tick()` 新增 MSI-X 投递处理: 从 `msix_delivery_in` 接收 → 累加 `msix_delivery_count_`
  - [ ] 2.4.2 经 ABI `cpptlm_emulator_msix_clear_pending()` 清除 pending
  - [ ] 2.4.3 4 方向闭环 (D1/D2 程序化路径) 不破坏
- [ ] 2.5 创建 `test/test_pcie_endpoint_ip_msix_path.cc` (4 cases, 双标签 `[pcie][pcie-ep-soc-bridge]`):
  - [ ] 2.5.1 TEST_CASE: EP→HB MSI-X 单 vector 投递 (vector=N 经 allocate_msix_vector 分配)
  - [ ] 2.5.2 TEST_CASE: 多 vector 优先级 (SDMA + CR 共享 msix_delivery_in, vector 间不冲突)
  - [ ] 2.5.3 TEST_CASE: EP 无 HB 绑定时降级 (`host_bypass_ == nullptr`, 跳过 push)
  - [ ] 2.5.4 TEST_CASE: msix_delivery_in/out 端口命名严格区分
- [ ] 2.6 跑 `cmake --build build -j$(nproc)` → PASS
- [ ] 2.7 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"` → ≥4 cases PASS
- [ ] 2.8 跑 `./build/bin/cpptlm_tests "[pcie]"` → ≥36,598 / 415 cases 0 回归 (Phase 8 M1 4 方向闭环保护)
- [ ] 2.9 跑 `./build/bin/test_pcie_endpoint_ip_full_e2e` (Phase 8 M1 E2E, flag-gated 验证零破坏)
- [ ] 2.10 commit: `feat(p2-unblock): EP/HB MSI-X 双端口 + flag-gated (默认 disable)`

**验收 (per design §D3 修订 + flag-gated 纪律)**:
- EP msix_delivery_in/out 端口就位 (默认 disable)
- HB msix_delivery_in 端口 + MSI-X 投递 tick
- `allocate_msix_vector()` API 避免 vector 命名冲突
- Phase 8 M1 既有 4 方向闭环 + test_pcie_endpoint_ip_full_e2e 0 回归 (flag 默认 disable)
- `[pcie]` ≥36,598 / 415 cases 0 回归

---

## Commit 3: SDMA/CR/JSON 集成 (D8 framebuffer 契约 + init order assert)

- [ ] 3.1 修改 `include/tlm/gpu/sdma_engine_tlm.hh` (Oracle 修订版):
  - [ ] 3.1.1 新增 `axi_slave_in` ChPort (ingress, Axi4Bundle, 占位供未来 AXI 路径)
  - [ ] 3.1.2 新增 `axi_master_out` ChPort (egress, Axi4Bundle, 占位)
  - [ ] 3.1.3 新增 `trigger_doorbell()` API (EP 程序化调用入口)
  - [ ] 3.1.4 header 注释锁定 5 既有端口 bundle 类型 (全 PcieTlpBundle) + 索引 (host_out=3, done_out=4)
- [ ] 3.2 修改 `src/tlm/gpu/sdma_engine_tlm.cc`:
  - [ ] 3.2.1 `trigger_doorbell()` 实现 (从 EP setter 调, 触发 descriptor ring 处理)
  - [ ] 3.2.2 `tick()` 既有 5 端口逻辑不破坏
- [ ] 3.3 修改 `include/tlm/gpu/completion_ring_mvp.hh`:
  - [ ] 3.3.1 `irq_out[3]` 端口注释改为 "→ pcie_ep.msix_delivery_in (程序化桥接, per 19-pcie-ip-microarchitecture.md §14.2.3 限制)"
  - [ ] 3.3.2 不硬编码 vector=3 (改用 `ep->allocate_msix_vector(COMPLETION_RING_VECTOR)` 经 setter 分配)
- [ ] 3.4 修改 `src/tlm/gpu/completion_ring_mvp.cc`:
  - [ ] 3.4.1 `tick()` 输出 MsiXDeliveryBundle {vector=CR_VECTOR, msg_addr=..., msg_data=..., trans_id=...} 到 `irq_out[3]`
- [ ] 3.5 修改 `src/tlm/gpu/dgpu_board_shell.cc` (D8 不变量 4 + init order):
  - [ ] 3.5.1 `init()` 末尾加 assert: `if (framebuffer_size_ > 0 && framebuffer_ptr_ == nullptr) throw runtime_error`
  - [ ] 3.5.2 setter 调用顺序 (在 init 完成后, sim_thread 启动前):
    - `bind_memory_backings()` (P0.5-landing 既有)
    - `ep->set_sdma_engine(sdma_)` (新增)
    - `ep->set_completion_ring(cr_)` (新增)
    - `ep->set_xbar_master(adapter_)` (新增, 如已实现)
  - [ ] 3.5.3 setter 内部检查: `framebuffer_ptr_ != nullptr` 否则返 -EINVAL (D8 拒收场景)
- [ ] 3.6 修改 `examples/dgpu_soc_with_pcie_ip.json` (Oracle 修订版, D7 程序化):
  - [ ] 3.6.1 modules 列表添加 `sdma` 声明 (仅 name, type, params; 无端口连接):
    `{ "name": "sdma", "type": "SdmaEngineTLM", "params": { "max_outstanding": 16, "fence_size": 4096 } }`
  - [ ] 3.6.2 **不添加**任何 sdma.↔pcie_ep. / sdma.↔xbar. 端口 JSON 连接 (会被 19 §14.2.3 静默丢弃)
  - [ ] 3.6.3 **不添加** completion_ring.↔pcie_ep. 端口 JSON 连接 (走程序化桥接)
  - [ ] 3.6.4 添加 metadata 注释说明程序化桥接路径 (per design D7)
- [ ] 3.7 创建 `test/test_pcie_endpoint_ip_sdma_wiring.cc` (4 cases, 双标签):
  - [ ] 3.7.1 SDMA doorbell 触发 (程序化桥接路径, 经 trigger_doorbell())
  - [ ] 3.7.2 SDMA H2D 数据传输 (mem_out[2] PcieTlpBundle → VRAM backing)
  - [ ] 3.7.3 SDMA 完成中断 (done_out[4] → EP.msix_delivery_in → vector 聚合)
  - [ ] 3.7.4 SDMA 端口索引正确 (host_out=3, done_out=4)
- [ ] 3.8 创建 `test/test_pcie_endpoint_ip_completion_ring_wiring.cc` (3 cases, 双标签):
  - [ ] 3.8.1 CompletionRing IRQ 投递 (程序化桥接, vector=CR_VECTOR → EP.msix_delivery_in)
  - [ ] 3.8.2 SDMA done_out[4] + CompletionRing irq_out[3] 共享 EP.msix_delivery_in (vector 命名空间)
  - [ ] 3.8.3 header 注释同步验证 (irq_out[3] → pcie_ep.msix_delivery_in)
- [ ] 3.9 跑 `cmake --build build --target validate_topology` → PASS (允许 sdma 端口 WARN)
- [ ] 3.10 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"` → ≥11 cases PASS (累积)
- [ ] 3.11 跑 `./build/bin/cpptlm_tests "[pcie]"` → ≥36,598 + 新增 ≥250 = ≥36,850 cases 0 回归 (G6 floor)
- [ ] 3.12 跑 `./build/bin/cpptlm_tests "[sdma]"` → 既有测试 0 回归
- [ ] 3.13 跑 `./build/bin/cpptlm_tests "[chstream]"` → ≥184 / 46 cases 0 回归
- [ ] 3.14 commit: `feat(p2-unblock): SDMA/CR/json 集成 + framebuffer init order assert (D8)`

**验收 (per design §D7 + §D8 + flag-gated 纪律)**:
- SDMA 程序化桥接就位 (set_sdma_engine + trigger_doorbell + 5 既有端口不破坏)
- CompletionRing 程序化桥接就位 (set_completion_ring + vector 分配经 allocate_msix_vector)
- framebuffer init order assert 触发 (D8 不变量 4)
- setter 拒收路径 (framebuffer 未分配时返 -EINVAL)
- `[pcie-ep-soc-bridge]` 总标签 ≥11 cases
- `[pcie]` ≥36,850 cases PASS (G6 floor, 实测基线 36,598 + 新增 ≥250)
- `[sdma]` `[chstream]` 既有测试 0 回归
- 4 个新测试文件**全部双标签** `[pcie][pcie-ep-soc-bridge]`

---

## Commit 4: 文档同步 + 跨仓 PR + 验证 + Archive

- [ ] 4.1 ArchForge 仓 PR (跨仓):
  - [ ] 4.1.1 创建 `ArchForge/docs/roadmap/phase9-p2-unblock.md` (阶段文件, 含 9 步路径)
  - [ ] 4.1.2 修改 `ArchForge/docs/microarchitecture/dgpu-soc-pcie-slice.md` §9.4 追加 P2 unblock 状态 + 4 断点修复说明
  - [ ] 4.1.3 修改 `ArchForge/docs/architecture/19-pcie-ip-microarchitecture.md` 新增 §13 EP↔SoC 桥接章节 (含 Axi4CacheAdapter + MSI-X 双端口 + SDMA 程序化桥接 + CompletionRing setter 引用)
- [ ] 4.2 CppTLM 仓:
  - [ ] 4.2.1 修改 `AGENTS.md` STRUCTURE 节同步 (P2 unblock 相关 path)
  - [ ] 4.2.2 运行 `scripts/test/docs_sync_check.sh --strict` → 无新增 VIRTUAL_PATHS 警告
- [ ] 4.3 全量验证:
  - [ ] 4.3.1 `cmake --build build -j$(nproc)` → 退出码 0 (所有 22 文件编译)
  - [ ] 4.3.2 `ctest --test-dir build --output-on-failure -j4` → 全 PASS (含 dlopen_minimal_soc + 76 个 ctest)
  - [ ] 4.3.3 `./build/bin/cpptlm_tests "[pcie]"` → ≥36,850 / 425 cases PASS (G6)
  - [ ] 4.3.4 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"` → ≥11 cases PASS (G2)
  - [ ] 4.3.5 `./build/bin/cpptlm_tests "[chstream]"` → ≥184 / 46 cases PASS (G7)
  - [ ] 4.3.6 `./build/bin/cpptlm_tests "[sdma]"` → 既有测试 0 回归
  - [ ] 4.3.7 `cmake --build build --target validate_topology` → 退出码 0 (G5)
  - [ ] 4.3.8 `openspec validate cpptlm-p2-integration-unblock --strict` → PASS (G1)
  - [ ] 4.3.9 `openspec validate --all` → 全部 spec PASS (pre-existing 2 个失败与本 change 无关, 已 ack)
- [ ] 4.4 commit: `chore(openspec): archive cpptlm-p2-integration-unblock (跨仓 doc 已 PR)`
- [ ] 4.5 commit: `docs: AGENTS.md + 主 spec 永久落地 (pcie-ep-soc-noc-axi-bridge)`
- [ ] 4.6 跑 `openspec archive cpptlm-p2-integration-unblock --yes` (change dir 移至 archive/)
- [ ] 4.7 跑 `openspec validate --all` 复跑 → 主 spec 已永久落地

**验收 (per design §5.1 Commit 4)**:
- ArchForge 仓 PR 已开 (含 19 §13 + dgpu-soc-pcie-slice §9.4 + phase9-p2-unblock.md)
- 8 项 Acceptance Gate 全部 ✅ (G1-G9)
- 主 spec `pcie-ep-soc-noc-axi-bridge` 永久落地 (`openspec/specs/`)
- change dir 已 archive (`openspec/changes/archive/`)
- `[pcie]` ≥36,850 / 425 cases PASS

---

## 9 项 Acceptance Gate (基线重算, Metis + Oracle 2027-09-17)

| Gate | 验证 | 实测基线 |
|------|------|----------|
| **G1** | `openspec validate --strict` PASS | — |
| **G2** | `[pcie-ep-soc-bridge]` 标签 ≥11 cases (4 文件) 双标签 | — |
| **G3** | 4 方向 AXI 流方向图 ✅ (D1/D2 既有 + D3/D4 新增) | Phase 8 M1 + Commit 2 + 3 |
| **G4** | `Axi4CacheAdapter` 2 方向 round-trip PASS (transaction_id 直通, fragment, OOO) | Commit 1 |
| **G5** | `validate_topology` PASS (sdma 声明式, WARN 可接受) | Commit 3 |
| **G6** | `[pcie]` ≥36,850 / 425 cases PASS (基线 36,598 + 新增 ≥250, 双标签数学) | 实测 |
| **G7** | `[chstream]` ≥184 / 46 cases PASS (基线 184) | 实测 |
| **G8** | ArchForge 仓 19 §13 + dgpu-soc-pcie-slice §9.4 + phase9-p2-unblock.md 同步; CppTLM AGENTS.md 同步 | Commit 4 |
| **G9** | 4 个新测试文件**双标签** `[pcie][pcie-ep-soc-bridge]` (Metis 修订: 空格 = AND, 逗号 = OR) | Commit 1/2/3 |

---

## 风险与缓解 (重写版, Metis + Oracle)

| Risk | 等级 | 缓解 |
|------|:----:|------|
| ~~Axi4CacheAdapter 状态机设计缺陷~~ | ~~🟡 中~~ | **已废 (Oracle 重写)**: transaction_id 直通消除 OOO/conflict/SLVERR 复杂度 |
| ~~AXI 16→8 压缩冲突~~ | ~~🟡 中~~ | **已废**: `CacheReqBundle.transaction_id` 是 64-bit, 直通无冲突 |
| Axi4CacheAdapter 实施错误 (transaction_id 错位 / fragment 拆分 bug) | 🟡 中 | TDD 5 步先写测试, fragment 多拍 + 多 outstanding 并发 |
| EP tick() 扩展破坏 Phase 8 M1 | 🟡 中 | flag-gated 默认 disable (Commit 2), `[pcie]` 36,598 基线 + test_pcie_endpoint_ip_full_e2e 显式 gate |
| `set_host_bypass()` API 误引用 (Metis) | 🟡 中 | **已修**: 用既有 `HostBypassTLM::attach_to_endpoint()` API |
| **D2 旧前提 8-bit 压缩** (Metis + Oracle) | 🔴 **高 (历史)** | **已废**: D2 Oracle 重写, 实施前 P0.5-2 code review 必须确认无压缩逻辑 |
| JSON 接线被 19 §14.2.3 静默丢弃 | 🔴 **高** | D7 程序化桥接强制, 范围扩展含 xbar 下游 |
| SDMA bundle 类型误标 | 🔴 **高** | D4 锁定 5 端口全 PcieTlpBundle, header 注释明确 |
| **framebuffer 单一真源破坏** (Oracle 新增) | 🔴 **高** | **D8 新增**: Axi4CacheAdapter 出站写 = framebuffer_, DGpuBoardShell init order assert + setter 拒收 |
| MSI-X vector 命名空间冲突 (Oracle) | 🟡 中 | D3 修订: vector 由 EP `allocate_msix_vector()` 统一分配 |
| Catch2 标签语义误解 (Metis) | 🟢 低 | spec Scenario 修订: 空格 = AND, 逗号 = OR |
| 测试基线陈旧 (Metis) | 🟢 低 | 重算: `[pcie]` 36,598 / 415 cases, `[chstream]` 184 / 46 cases (实测) |

---

## 时间线重估 (Metis 修订)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| 1 | Axi4CacheAdapter (~80 行) + chstream_register + axi4_stream_adapter + 单测 | 2-3 天 |
| 2 | EP/HB tick MSI-X (flag-gated) + msix 端口 + 单测 | 3-4 天 |
| 3 | SDMA/CR 集成 + json + init order assert (D8) + 2 个 E2E 单测 | 3-5 天 |
| 4 | 跨仓 doc PR + AGENTS 同步 + archive + 验证 | 1 天 (跨仓异步) |
| **总计** | — | **9-13 天 ≈ 2-3 周单人** (含 [pcie] regression 实跑) |

**注意**: 旧估时 "1.5-2 周" 过度乐观. 当前估时**含 Oracle/Metis 修订 + flag-gated 默认 disable + init order assert + ArchForge 跨仓 PR**, 更现实.

---

## 维护

**Owner**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — P2 集成 4 断点修复 (Axi4CacheAdapter + MSI-X + SDMA + CompletionRing)
**关键路径**:
1. Commit 1: 桥接组件 + 单测 (TDD)
2. Commit 2: EP/HB tick 扩展 (flag-gated 默认 disable, 不破坏 Phase 8 M1)
3. Commit 3: SDMA/CR/json 集成 (D8 framebuffer 契约)
4. Commit 4: 跨仓 doc PR + archive + 验证

---

## 关联

- **proposal**: [`proposal.md`](proposal.md)
- **design**: [`design.md`](design.md) (D1-D8 决策, 含 Oracle 2027-09-17 修订)
- **spec**: [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md) (8 个 MODIFIED Requirements, 含 D2/D3/D8 修订)
- **P2 主计划** (ArchForge): `docs/roadmap/phase9-p2-cp-attach-via-axi.md`
- **P2 unblock 阶段文件** (ArchForge, Commit 4 创建): `docs/roadmap/phase9-p2-unblock.md`
- **架构文档** (ArchForge, Commit 4 新 §13): `docs/architecture/19-pcie-ip-microarchitecture.md`
- **dgpu-soc-pcie-slice** (ArchForge, Commit 4 §9.4): `docs/microarchitecture/dgpu-soc-pcie-slice.md`
- **P0.5-landing 前置** (archived): `openspec/changes/archive/2026-09-25-2027-09-17-cpptlm-minimal-dgpu-soc-v1-landing/`
- **相关 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`
- **AGENTS.md**: KEY INVARIANTS (framebuffer 自动分配 + OpenSpec Proposed ≤3 KPI) + DOC HYGIENE (ArchForge 跨仓约束)