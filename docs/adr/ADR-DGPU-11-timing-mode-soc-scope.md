# ADR-DGPU-11: Timing-Mode (AT, cycle-approximate) SoC 与 functional-mode minimal_v1 并列共存

> **状态**: ✅ **已签发** (T0 阶段, 2027-02-09, per Oracle 八轮复评 PASS)
> **日期**: 2027-02-09
> **关联 OpenSpec**: [cpptlm-dgpu-soc-timing-mvp](../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) (v0.2 终稿, 18 ADDED Requirements + 52 scenarios, **PASS**)
> **关联架构文档**: [../designs/dgpu-soc/architecture.md](../designs/dgpu-soc/architecture.md) (functional-mode minimal_v1) + [../designs/dgpu-soc/timing-mode.md](../designs/dgpu-soc/timing-mode.md) (timing-mode 业务架构视图, v0.2 终稿)
> **配套 ADR**: [ADR-DGPU-05](ADR-DGPU-05-vram-storage-ownership.md) (单一 VRAM 所有权, **沿用**) + [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) (AxiMemBundle 边界, 沿用模式) + [ADR-DGPU-07](ADR-DGPU-07-minimal-soc-evolution-seam.md) (演进 seam, VramControllerTLM 是 D3 占用) + [ADR-DGPU-10](ADR-DGPU-10-backing-naming-convention.md) (backing 命名, 沿用)

---

## 1. 背景

### 1.1 问题

D-AXI v1.4 阶段 (commit `429327d`)，functional-mode minimal_v1 SoC 已交付，提供 driver 正确性闭环 (23 ABI + 41 [minimal_dgpu_soc] assertions + 24 [pcie-memory] cases 全绿)。但 **缺乏 cycle-approximate 性能建模能力**——driver 端无法测量 TLB miss / row hit / doorbell 延迟累加，无法做 workload profiling。

### 1.2 Oracle 评审结论

经过 **8 轮 Oracle 评审**（一轮 15 must-fix + 3 should-fix，二轮 H1-H6 v0.2 简化，三/四/五/六/七轮 21 项硬伤逐步落盘），最终在第七轮达到 **✅ PASS**：
- **openspec validate --strict PASS** (18 ADDED Requirements + 52 scenarios)
- **docs_sync_check --strict PASS** (370 路径, 0 缺失)
- **0 残留 grep**：无 std::deque/FIFO/push_back/SDMA 自加 cycle/v0.1 sub-req/8 模块矛盾 等 v0.1 语义
- **v0.2 简化骨架成立**：priority_queue (非 FIFO) + board 集中 cycle 推进 + SDMA 不自加 current_cycle_ + VramControllerTLM 同位置替换 (非 cascade)

### 1.3 现状约束

- **23 ABI 签名字节级冻结** (per ADR-DGPU-08 §D5)：timing-mode 是仿真模式切换，**不**改变 ABI 含义
- **D-AXI v1.4 已交付功能不变**：`simulation_mode` 默认 `"functional"`，无 breaking change
- **CrossbarTLM 现有架构**（NUM_PORTS=4 loopback，无 req_out/resp_in 下游转发）：timing-mode **不引入新连接**，沿用 minimal_v1 拓扑
- **CacheReqBundle 实际字段**（仅 transaction_id/parent_id/fragment_id/fragment_total/address/size/is_write/data）：**无** rid/user 字段，**无** AXI master port 概念
- **9.5d 工程量**（v0.2 终稿）：T0=1, T1=1, T2=1, T3=0.5, T4=1.5, T5=2, T6=1, T7=2, T8=0.5

---

## 2. 决策 (v0.2 终稿)

### 2.1 决策 1: 并列共存 (方案 B)

timing-mode SoC **不替换** functional-mode minimal_v1，二者通过 JSON `simulation_mode: "functional" | "timing"` 字段切换；同一 `DGpuBoard` 框架、不同 DGpuSoc 实例化分支。

**为什么不是方案 A（改造 minimal_v1）**：
- 会违反 minimal_v1 6 条 invariants（architecture.md §8）
- 破坏 66951 assertions 全绿（含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路）
- driver 视角闭环重测

**为什么不是方案 C（v2.0 重写）**：
- 等价于重新设计 MVP，失去 driver 闭环已交付价值
- 23 ABI 已冻结，破坏 ADR-DGPU-08

### 2.2 决策 2: cycle accounting 在 3 层加，**不**改造 interconnect 拓扑

| 层级 | 现有 (functional) | timing-mode (v0.2) |
|------|-------------------|-------------------|
| `GmmuTLM::translate_timing()` | 同步 translate，丢弃 latency | 加 32-entry TLB + 累加 `page_walk_cycles_` + `tlb_miss_latency_cycles_` |
| `MemoryTLM::tick()` | backing 零时 memcpy | 加 priority_queue `defer_response()` + stats_latency_read_/write_ 启用 + `use_zero_delay_for_test_` 默认 `true` 保 functional 零回归 |
| `CrossbarTLM::tick()` | NUM_PORTS=4 loopback | 加 `set_arb_latency()` + `stats_arb_cycles_` + `stats_port_utilization_` (4 端口 × 1 指标) |

**关键不变量 (TInv-1 ~ TInv-6)**：per timing-mode.md §8，与 functional-mode 6 条 invariants 兼容无冲突。

### 2.3 决策 3: SDMA 沿用既有 5-port + backdoor path，**不**新增端口

SDMA **不**新增 `axi_master_out` / `axi_master_in` 端口、**不**拆分 descriptor 为 sub-req、**不**接 Crossbar 仲裁、**不**接 VramController cascade。cycle accounting 仅在 `translate_cb_` 回调中累加 GMMU TLB miss latency → `last_tlb_latency_` 成员 → 由 `DGpuBoard::sdma_fence_complete` 回调注入 `last_descriptor_complete_cycle_` (= board.current_cycle_, **不** SDMA 自加 current_cycle_)。

**为什么不是 v0.1 方案（AXI master + outstanding + rid 关联）**：
- `CacheReqBundle` 无 `rid`/`user` 字段 (`cache_bundles_tlm.hh:38-69` 实际只有 transaction_id/parent_id/fragment_id/fragment_total/address/size/is_write/data)
- `size` 是 `ch_uint<8>` 上限 255 字节，4KB sub-req 溢出
- "Phase 5 PcieAxiAdapter 已实现 4KB chunk 模式" 是伪引用（`pcie_axi_adapter_tlm.cc` 无 chunk/split 逻辑）
- `CrossbarTLM` 无 `req_out`/`resp_in` 下游转发，无 multi-hop cascade 拓扑
- `PcieEndpointIP` 无 `host_egress`/`host_ingress` 端口

### 2.4 决策 4: VramControllerTLM 同位置替换 MemoryTLM，**不**是 cascade

`VramControllerTLM` **不**是 `SDMA → Crossbar → VramController → Memory` cascade 链路上的独立节点，而是 **`DGpuSoc` 配置中可与 `MemoryTLM` 二选一**的 seam 占用（per ADR-DGPU-07 D3 seam）。

JSON 配置二选一：
```jsonc
// 模式 A: MemoryTLM（functional-mode 默认）
{ "modules": [{ "name": "memory", "type": "MemoryTLM", "params": {...} }] }

// 模式 B: VramControllerTLM（timing-mode 性能回归）
{ "modules": [{ "name": "vram_ctrl", "type": "VramControllerTLM", "params": {...} }] }
```

**实现路径**：
- `VramControllerTLM : public MemoryTLM`（继承行缓冲 hit/miss + bandwidth 上限）
- `MemoryTLM` 需 protected 化 (`req_in_`/`resp_out_`/`stats_`/`backing_ptr_`/`pending_resps_`/`current_cycle_`/`last_tlb_latency_` 等成员从 `private:` 改 `protected:` + `friend class VramControllerTLM;`)
- 模块注册：`include/chstream_register.hh:46` 宏体内追加 `ModuleFactory::registerObject<VramControllerTLM>("VramControllerTLM");`（**不**用 `REGISTER_CHSTREAM(VramControllerTLM)` 宏传参，`REGISTER_CHSTREAM` 是无参硬编码宏；也**不**用 `REGISTER_MODULE`，`is_base_of<SimModule>` 静态断言失败）

### 2.5 决策 5: priority_queue (非 FIFO) for pending_resps_

`MemoryTLM::pending_resps_` 用 `std::priority_queue<PendingResp, std::vector<PendingResp>, std::greater<PendingResp>>` 按 `ready_cycle` 升序，**不**用 `std::deque` + `front()/pop_front()` FIFO。

**为什么**：
- `CacheReqBundle.data` 8 字节 payload + 64-bit ch_uint 限制
- 若 req A (lat=100) 先入队、req B (lat=50) 后入队，FIFO 会让 B 等到 cycle N+100（head-of-line blocking）
- priority_queue 顶部必为 `ready_cycle` 最小者，B 在 cycle N+55 立即发出
- TInv-2 强 ordering 保证：同一 cycle 内 port A 写 + port B 读同地址时，B 必读到 A 写入后数据

### 2.6 决策 6: cycle 统一由 `DGpuBoard::tick()` 集中推进

`cycle_advance_modules_` 类型 `std::vector<ChStreamModuleBase*>`，注册 `[MemoryTLM, VramControllerTLM]`（**剔除** GmmuTLM 同步 + **剔除** SdmaEngineTLM 无 advance_cycle 方法）。

**为什么**：
- `ChStreamModuleBase`/`SimObject` 全局**无** `advance_cycle()` 虚函数，T0 实施时须添加 `virtual void advance_cycle() noexcept {}` 默认空实现
- TInv-3 强制：任何模块不可自行 `++cycle`，由 `DGpuBoard::tick()` 统一推进
- SDMA **不**注册到 `cycle_advance_modules_`：SDMA 无 `current_cycle_` 成员，`last_descriptor_complete_cycle_` 由 `DGpuBoard::sdma_fence_complete` 回调注入 = `board.current_cycle_`

---

## 3. 实施范围

### 3.1 新建模块（1 个）

| 模块 | 来源 | 注册 |
|------|------|------|
| `VramControllerTLM` | 🆕 新建 (D3 seam 占用, per ADR-DGPU-07) | `chstream_register.hh:46` 宏体内追加 `registerObject<VramControllerTLM>` |

### 3.2 扩展模块（4 个）

| 模块 | 改动 |
|------|------|
| `MemoryTLM` | protected 化 + `defer_response()` priority_queue + `set_timing_params()` + `use_zero_delay_for_test_` (默认 `true` 保 functional 零回归) + `last_tlb_latency_` 成员 |
| `GmmuTLM` | 32-entry 直接映射 TLB + `translate_timing()` 累加 `page_walk_cycles_` + `TranslateStats` |
| `CrossbarTLM` | `set_arb_latency()` + `stats_arb_cycles_` + `stats_port_utilization_[4]` |
| `SdmaEngineTLM` | 仅 `last_descriptor_complete_cycle_` getter + `set_cycle_accounting_enabled()` + `last_tlb_latency_` 私有成员（**不**新增端口、**不** outstanding、**不** rid） |

### 3.3 不在范围（明确不做）

- **不做 multi-hop cascade 拓扑**（`CrossbarTLM` 无 `req_out`/`resp_in` 下游转发）
- **不做 AXI master port + outstanding 拆分**（`CacheReqBundle` 无 `rid`/`user` 字段）
- **不做 LRU TLB**（v0.2 仅 fill，不 evict；D4 后续）
- **不做真实硬件校准**（cycles 是仿真建模值，**不**与具体 GPU SKU 对齐）
- **不做 Compute Pipeline**（GPU kernel launch 不模拟，仅 DMA/内存子系统）
- **不修改 23 ABI 签名**（per ADR-DGPU-08 §D5）
- **不破坏 minimal_v1 functional-mode 6 条 invariants**（架构文档 §8）

---

## 4. 验证 (v0.2 终稿)

### 4.1 测试策略

- **单元测试** (5 cases × 6 模块 = 30 cases)：
  - `test_memory_tlm_timing.cc` (6)：priority_queue + drain 顺序 + 错误立即 + use_zero_delay
  - `test_gmmu_tlm_timing.cc` (6)：TLB hit/miss + 128KB 工作集 + 4MB 顺序 thrashing + 跨页
  - `test_sdma_engine_timing.cc` (5)：4KB TLB hit/miss + 翻译失败 + 公共 API 零回归 + last_descriptor_complete_cycle_ 记录
  - `test_vram_controller_tlm.cc` (5)：行缓冲 hit/miss + bandwidth 上限 + invalidate
  - `test_crossbar_timing.cc` (3)：4 routes 仲裁 + 忙端口 wait + utilization
- **E2E 性能回归** (5 cases)：H2D throughput (4MB/1M cycles ≥ 10 GB/s, ≤ 32 GB/s) + D2H + TLB hit rate (128KB 工作集 ≥ 95%) + 并发 (SDMA+GMMU+Host egress) + 6 条新 invariants 全验证

### 4.2 零回归基线

- **66951 assertions**（per AGENTS.md baseline；含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路）
- **8 注册类型 / 7 实例化**（VramControllerTLM 或 MemoryTLM 二选一，per H2 互斥）

### 4.3 验证命令

```bash
# 静态验证
openspec validate cpptlm-dgpu-soc-timing-mvp --strict   # PASS (v0.2 终稿)
./scripts/test/docs_sync_check.sh --strict                # PASS (370 路径, 0 缺失)

# 回归测试
./build/bin/cpptlm_tests "[dgpu_soc_timing]"              # ≥ 30 cases (新)
./build/bin/cpptlm_tests "[minimal_dgpu_soc]"            # 41 (零回归)
./build/bin/cpptlm_tests "[pcie-memory]"                 # 24 (零回归)
./build/bin/cpptlm_tests "[abi][minimal_dgpu_soc]"       # 28 (零回归)
```

---

## 5. 工作量与时间表

| 任务 | 工作量 | 依赖 | 关键产出 |
|------|--------|------|----------|
| T0 OpenSpec 立项 + ADR-DGPU-11 签发 | 1d | - | proposal.md + ADR |
| T1 MemoryTLM protected 化 + `defer_response()` + 5 时序参数 | 1d | T0 | `include/tlm/memory_tlm.hh` + `test_memory_tlm_timing.cc` (6 cases) |
| T2 GmmuTLM TLB + translate_timing | 1d | T0 | `include/tlm/gpu/gmmu_tlm.hh` + `test_gmmu_tlm_timing.cc` (6 cases) |
| T3 SDMA cycle accounting (v0.2 简化) | **0.5d** | T1+T2 | `last_tlb_latency_` 成员 + `last_descriptor_complete_cycle_` 记录 (无新端口) |
| T4 VramControllerTLM 新建 (含 T4.0 MemoryTLM protected 化) | **1.5d** | T1 | `include/tlm/vram_controller_tlm.hh` + `test_vram_controller_tlm.cc` (5 cases) |
| T5 CrossbarTLM C++ 扩展 + DGpuBoard::init_timing_mode | **2d** | T1-T4 | 4-port loopback + arb_latency + stats + simulation_mode 切换 |
| T6 dgpu_soc_timing_v1.json + minimal test 整合 | 1d | T5 | `configs/dgpu_soc_timing_v1.json` + 7-8 模块 (0 new connections) |
| T7 E2E 性能回归 (H2D/D2H/TLB hit rate/concurrent/Invariants) | 2d | T6 | 5 E2E cases |
| T8 文档同步 (arch doc §14 + README 维护表 + AGENTS.md 看板) | 0.5d | T7 | 文档全套同步 |

**小计**: 10.5d (T0=1 + T1=1 + T2=1 + T3=0.5 + T4=1.5 + T5=2 + T6=1 + T7=2 + T8=0.5) + buffer 1.5d = **11-12d**。

---

## 6. 风险与回退

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| `use_zero_delay_for_test_` 默认 `true` 误改导致 `[memory_tlm]` 既有单测回归 | 低 | [memory_tlm] 24 case 回归 | 既有 v2.2 zero-delay 路径**不**经 defer_response 分支，行为字节级不变 |
| priority_queue 排序错误导致 ready_cycle 失序 | 中 | TInv-2 违反 | 单元测试覆盖 (3 priority_queue scenario)，operator>` 显式重载 |
| SDMA `last_tlb_latency_` 未在 translate_cb_ 包装层写入 | 中 | TLB miss latency 累加恒 0 | `set_translate_cb()` 调用方需保证包装层写回；tasks.md T3.1 + design.md §4.2 双重注册 |
| `ChStreamModuleBase::advance_cycle()` 未在 T0 添加 | 高 | cycle 漂移 / 编译失败 | tasks.md T0.3 显式列为前置任务；spec.md Requirement 2 scenario 独立验证 |
| VramControllerTLM 注册时 `REGISTER_CHSTREAM(VramControllerTLM)` 误用 (无参宏传参) | 中 | 预处理失败 | tasks.md T4.1 / design.md §5.3 明确写宏体内追加 `registerObject` 行 |
| T4 累计列跨文档不一致 | 中 | T4=1d vs 1.5d | T4=1.5d 已统一（tasks/design/proposal/timing-mode.md §9 全对齐） |
| 模块数混淆 (7 vs 8) | 中 | spec.md 内部矛盾 | spec.md:34/541/548 + tasks.md:283/292/331 全部统一 "8 注册类型 / 7 实例化 (VramCtrl 或 Memory 二选一)" |

---

## 7. 状态与签发

| 阶段 | 状态 | 日期 | 评审方 | 结论 |
|------|------|------|--------|------|
| 探索 | ✅ 完成 | 2027-02-09 | 用户 + Oracle 5 轮 + 7 轮 | v0.2 简化骨架成立 |
| ADR 签发 (本 ADR) | ✅ **已签发** | 2027-02-09 | Oracle 八轮复评 ✅ PASS | 21/21 硬伤落盘, 0 残留 |
| 实施 T0-T8 | ⏳ 待启动 | 2027-02-10+ | 用户启动 | 11-12d 工程量 |
| 归档 | ⏳ 待 T8 完成后 | 2027-02-22+ | 用户 | `openspec archive cpptlm-dgpu-soc-timing-mvp` |

---

## 8. 关联文档

### 8.1 第 1 层 (架构视图)

- [`docs/designs/dgpu-soc/architecture.md`](../designs/dgpu-soc/architecture.md) — functional-mode minimal_v1 业务架构 (v1.0, Oracle 二轮 PASS)
- [`docs/designs/dgpu-soc/timing-mode.md`](../designs/dgpu-soc/timing-mode.md) — timing-mode 业务架构 (v0.2 终稿, 八轮 PASS)

### 8.2 第 2 层 (ADR 决策)

- 本 ADR (DGPU-11) — timing-mode 范围 + functional/timing 共存边界
- ADR-DGPU-05 — 单一 VRAM backing 真源 (沿用)
- ADR-DGPU-06 — AxiMemBundle 边界 (沿用模式, v0.2 改为 CacheReqBundle 复用既有 connection)
- ADR-DGPU-07 — Minimal → 完整 SoC 演进 seam (VramControllerTLM 是 D3 占用)
- ADR-DGPU-08 — 23 ABI 冻结 + 0 新增约束 (v0.2 严格遵守)
- ADR-DGPU-10 — backing 字段命名约定 (沿用 `set_backing_store` / `backing_ptr_` / `size_cap_`)

### 8.3 第 3 层 (OpenSpec change)

- [`openspec/changes/cpptlm-dgpu-soc-timing-mvp/`](../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) — v0.2 终稿, 18 ADDED Requirements + 52 scenarios, 八轮 PASS
  - `proposal.md` — Why + What Changes + 5 关键架构决策
  - `design.md` — 实施指导 (11 节, 含 §4.2 包装层 + §6.2 互斥逻辑)
  - `specs/dgpu-soc-timing/spec.md` — 18 ADDED Requirements (SHALL/MUST 规范化)
  - `tasks.md` — T0-T8 实施计划 (9.5d + buffer 1.5d = 11-12d)

### 8.4 第 4 层 (实施跟踪)

- `docs/superpowers/plans/cpptlm-dgpu-soc-timing-mvp.md` (待 T0 启动时创建)
- `openspec/changes/cpptlm-dgpu-soc-timing-mvp/tasks.md` (TDD 5-step 跟踪)

---

## 9. 签发签字

| 角色 | 姓名 | 日期 | 签字 |
|------|------|------|------|
| 提出人 | CppTLM Team | 2027-02-09 | ✅ |
| Oracle 评审 | oracle (kimi-k2.6) | 2027-02-09 | ✅ 八轮 PASS (session: ses_f16ce009effeT2mv3KunL0HZmN) |
| 用户验收 | (待 T0 启动) | — | ⏳ |

**签发后动作**:
1. 同步 `docs/designs/dgpu-soc/README.md` 入口（加 timing-mode.md 链接）
2. 同步 `docs/designs/README.md` 三视角表 (timing-mode 与 functional-mode 并列)
3. 同步 `AGENTS.md` D-AXI 看板（追加 "timing-mode v0.2 完成，待 T0 启动"）
4. 同步 `openspec/changes/cpptlm-dgpu-soc-timing-mvp/proposal.md:9` (从 "待签发" → "已签发", 链 ADR-DGPU-11 路径)
5. 创建 `docs/superpowers/plans/cpptlm-dgpu-soc-timing-mvp.md` (T0 实施跟踪)

---

**Owner**: CppTLM Team
**版本**: v0.2 终稿 (八轮 PASS)
**最后更新**: 2027-02-09
