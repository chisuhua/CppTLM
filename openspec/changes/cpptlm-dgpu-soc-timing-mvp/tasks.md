# Tasks: cpptlm-dgpu-soc-timing-mvp

> **总工期**: 10.5-12 工作日
> **关联 change**: cpptlm-dgpu-soc-timing-mvp
> **关联 ADR**: ADR-DGPU-11 (✅ 已签发 2027-02-09, Oracle 八轮 PASS) + ADR-DGPU-05/06/07/10 (沿用)
> **关联 spec**: [specs/dgpu-soc-timing/spec.md](./specs/dgpu-soc-timing/spec.md)
> **关联 design**: [design.md](./design.md)
> **关联架构文档**: [docs/designs/dgpu-soc/timing-mode.md](../../docs/designs/dgpu-soc/timing-mode.md) (第 1 层)
> **基线**: minimal_v1 functional-mode v1.0 已交付 (commit `429327d` + 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) 全绿)

## 任务依赖图

```
T0 (OpenSpec 立项 + ADR-DGPU-11 签发) [✅ 已完成 2027-02-09]
   ↓
   ├── T1 (MemoryTLM cycle-approximate) ─────┐
   │                                      ├──→ T3 (SdmaEngineTLM cycle accounting, v0.2 简化) ──┐
   ├── T2 (GmmuTLM TLB + page walk) ──────┘                                         │
   │                                                                                │
   ├── T4 (VramControllerTLM 新建) ─────────────────────────┐                       │
   │                                                        ├──→ T5 (Crossbar C++ + DGpuBoard) ──→ T6 (JSON + minimal test) ──→ T7 (E2E) ──→ T8 (文档)
   │                                                        │
   └── (T1 完成后)                                         ┘
```

---

## T0 [1d] OpenSpec 立项 + ADR-DGPU-11 签发 ✅ [已完成 2027-02-09]

**目标**: 完成 OpenSpec change 立项 + ADR 签发,为后续实施奠定基础。

- [ ] **T0.1** OpenSpec change `cpptlm-dgpu-soc-timing-mvp` 立项(proposal.md + design.md + specs/dgpu-soc-timing/spec.md + tasks.md 已创建)
  - `openspec validate cpptlm-dgpu-soc-timing-mvp --strict` PASS
- [x] ✅ **T0.2** `docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md` 已签发 (2027-02-09, Oracle 八轮 PASS, v0.2 终稿)
  - 内容: timing-mode SoC 范围 + functional/timing 共存边界 + 不破坏 minimal_v1 6 条 Invariants
  - 沿用 ADR-DGPU-05/06/07 引用
  - Status: `## Status` 段引用本 OpenSpec change
- [ ] **T0.3** `AGENTS.md` 状态看板追加 timing-mode 行
  - D-AXI Driver-Visible Minimal SoC 表追加一行: `v0.5 timing-mode (AT) SoC MVP — 实施中 (T0 立项)`
- [ ] **T0.4** `openspec/changes/cpptlm-dgpu-soc-timing-mvp/` 目录结构 verify
  - `proposal.md` / `design.md` / `specs/dgpu-soc-timing/spec.md` / `tasks.md` 全部就位
- [ ] **T0.5** Oracle 评审准备
  - 整理 proposal.md 决策表(§5 关键架构决策 5 项)
  - 准备 Oracle 评审 checklist: 性能数字真实性 / VramControllerTLM 继承策略 / TLB 简化模型 / Crossbar 复用决策 / cycle advance 集中推进

**完工标准**: OpenSpec validate PASS + ADR-DGPU-11 签发 + AGENTS.md 同步

---

## T1 [1d] MemoryTLM cycle-approximate 扩展

**目标**: 实现 `defer_response()` 路径 + 5 个时序参数 setter,**不**破坏 functional-mode 既有 v2.2 行为。

### T1.1 [0.5d] MemoryTLM::tick() defer_response 重构

- [ ] `include/tlm/memory_tlm.hh`:
  - 新增 `void advance_cycle() noexcept { ++current_cycle_; }` setter
  - 新增 `uint64_t current_cycle() const noexcept { return current_cycle_; }` getter
  - 新增 `void set_timing_params(uint64_t read_hit, uint64_t read_miss, uint64_t write, bool use_zero) noexcept;`
  - 新增 `uint64_t inflight_resp_count() const noexcept { return pending_resps_.size(); }`
  - 新增私有成员:
    - `std::priority_queue<PendingResp, std::vector<PendingResp>, std::greater<PendingResp>> pending_resps_;` (PendingResp = {resp, ready_cycle}, 按 ready_cycle 升序, per M1 + 七轮 P0-3 落盘)
    - `uint64_t current_cycle_ = 0;`
    - `uint64_t read_latency_hit_cycles_ = 100;`
    - `uint64_t read_latency_miss_cycles_ = 200;`
    - `uint64_t write_latency_cycles_ = 120;`
    - `bool use_zero_delay_for_test_ = true;` // 默认 true 保 functional 零回归 (per M5 Oracle 反馈)
  - 新增 `bool row_hit(uint64_t addr) const noexcept;` (4K row 简化)
- [ ] `MemoryTLM::tick()` 重构:
  - 阶段 1: 处理新 req (按 use_zero_delay_for_test 路径分支)
  - 阶段 2: drain pending_resps_ 队列(ready_cycle ≤ current_cycle_ 的)
  - sample stats_latency_read_/write_ (always 启用)
- [ ] `MemoryTLM::do_reset()` 重置 pending_resps_ + current_cycle_

### T1.2 [0.5d] 单元测试

- [ ] `test/test_memory_tlm_timing.cc` 新建,6 cases:
  - read hit → stats_latency_read_.sample(100) + pending_resps_ queue order
  - read miss → 200 cyc latency
  - write → 120 cyc latency
  - out-of-range → 1 cyc immediate error
  - pending_resps_ order strict (FIFO by ready_cycle)
  - use_zero_delay_for_test=true 走 v2.2 既有路径(零 diff)
- [ ] Catch2 注册 `[dgpu_soc_timing]` 标签
- [ ] `cmake --build build` 编译通过
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` 6 cases PASS
- [ ] `./build/bin/cpptlm_tests "[memory_tlm]"` 既有套件零回归

**完工标准**: 6 unit cases PASS + 既有 `[memory_tlm]` 零回归

---

## T2 [1d] GmmuTLM TLB + page walk

**目标**: 实现 `translate_timing()` API + 32-entry TLB,**不**破坏既有 `translate()` API。

### T2.1 [0.5d] GmmuTLM TLB + translate_timing

- [ ] `include/tlm/gpu/gmmu_tlm.hh`:
  - 新增 `int translate_timing(uint64_t iova, uint32_t size, uint64_t& out_paddr, uint64_t& out_latency_cycles);`
  - 新增 `struct TranslateStats { uint64_t hits; misses; total_walk_cycles; };`
  - 新增 `const TranslateStats& stats() const noexcept;`
  - 新增 `void reset_stats() noexcept;`
  - 新增 `void set_timing_params(uint64_t tlb_miss_latency) noexcept;`
  - 新增 `void set_tlb_size(size_t entries) noexcept;`
  - 新增私有成员:
    - `std::vector<TlbEntry> tlb_entries_;` (32-entry default,直接映射)
    - `TranslateStats translate_stats_;`
    - `uint64_t tlb_miss_latency_cycles_ = 50;`
- [ ] 既有 `translate()` 内部转 `translate_timing()` + 丢弃 latency
- [ ] `do_reset()` 清 TLB + reset stats

### T2.2 [0.5d] 单元测试

- [ ] `test/test_gmmu_tlm_timing.cc` 新建,6 cases:
  - TLB hit → translate_timing lat=0, translate_stats.hits++
  - TLB miss → lat=50, TLB fill
  - 128KB 工作集 (32 pages) 重复访问 → hit rate ≈ 96.875% (32-entry TLB initial fill + 992 repeats)
  - 4MB 顺序流 → hit rate 0% (无 LRU thrashing, 直映射模型)
  - 32-entry TLB 全满 → 第 33 次访问仍 fill (无 LRU)
  - 跨页 → -EIO
  - 既有 `translate()` API 兼容(零回归)
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` 6 cases PASS
- [ ] `./build/bin/cpptlm_tests "[gmmu]"` 既有套件零回归

**完工标准**: 6 unit cases PASS + 既有 `[gmmu]` 零回归

---

## T3 [0.5d] SdmaEngineTLM cycle accounting (v0.2 简化, **不**新增 outstanding/AXI master port)

**目标**: SDMA 沿用既有 5-port + `set_vram_backdoor()` 路径,**不**新增端口、不拆分 descriptor、不接 Crossbar 仲裁。Cycle accounting 仅在 `translate_cb_` 回调中累加 GMMU TLB miss latency + 记录 `last_descriptor_complete_cycle_` (per H2/H3 Oracle 二轮反馈)。

### T3.1 [0.25d] last_descriptor_complete_cycle_ 成员 + setter

- [ ] `include/tlm/gpu/sdma_engine_tlm.hh`:
  - **不**新增 `axi_master_out` / `axi_master_in` 端口 (per H3)
  - **不**新增 `submit_descriptor_timing()` API (沿用既有 `submit_descriptor()`)
  - **不**新增 `outstanding_` map / `next_rid_` (per H2)
  - **新增** getter: `uint64_t last_descriptor_complete_cycle() const noexcept`
  - **新增** setter: `void set_cycle_accounting_enabled(bool en) noexcept`
  - **新增** 私有成员: `uint64_t last_descriptor_complete_cycle_ = 0;` + `uint64_t last_tlb_latency_ = 0;` (由 translate_cb_ 包装层写入, per 七轮终稿) + `bool cycle_accounting_enabled_ = false;`
  - 公共 API (`submit_descriptor` / `set_vram_backdoor` / `set_translate_cb` / `mmio_*`) **零修改**
  - `[sdma]` 既有单测**零回归**

### T3.2 [0.25d] translate_cb_ 内调 translate_timing 累加 lat

- [ ] `SdmaEngineTLM::process_descriptor()` (既有内部方法) 扩展:
  - **v0.2 (per 七轮 P0-12/P0-13)**: SDMA 无 `current_cycle_` 成员;`last_descriptor_complete_cycle_` 由 `DGpuBoard::sdma_fence_complete` 回调注入 (= board.current_cycle_)
  - `translate_cb_` 在 timing-mode 下包装为单次调 `translate_timing` 并通过 `last_tlb_latency_` 成员回传 lat (避免二次调 TLB hit 误读)
  - 当 `cycle_accounting_enabled_=false` (functional-mode) 时,lat_cycles 丢弃 (与 minimal_v1 行为一致)
  - fence 触发时**不**额外调 callback,沿用既有路径
  - **不**新增 `submit_descriptor_timing` API
  - **不**接 Crossbar 仲裁 (`CrossbarTLM` 无 req_out/resp_in 下游端口,per H2)
  - **不**接 VramCtrl cascade (`VramControllerTLM` 同位置替换 MemoryTLM,per H2)

### T3.3 [0d, 含在 T1-T2 总工期] 单元测试

- [ ] `test/test_sdma_engine_timing.cc` 新建,**5 cases** (v0.2 简化, 删 outstanding/rid 场景):
  - 4KB H2D TLB hit (lat=0) → `last_descriptor_complete_cycle_` - submit_cycle ≈ 4 cyc
  - 4KB H2D TLB miss (lat=50) → 差 ≈ 54 cyc
  - 翻译失败 (PTE 无效) → 立即 fence err + lat 丢弃
  - 公共 API 零回归 (`submit_descriptor` 签名兼容 minimal_v1)
  - `set_cycle_accounting_enabled(false)` → lat 丢弃,与 functional-mode 行为一致

### T3 风险与回退 (per §10)

| 风险 | 检测 | 回退 |
|------|------|------|
| T3 cycle accounting 错误 | 测试 5 cases 与 analytical model ±5% 偏差 | 调 `set_cycle_accounting_enabled(false)` 立即退回 functional-mode 行为 |

> **v0.2 vs v0.1 工作量**: 原 T3 估 2d, v0.2 减至 **0.5d** (无新端口 + 无 outstanding + 无 rid + 无跨模块接线),实现复杂度降低约 4×。
  - stale resp 容忍 (race window 残留)
  - 既有 `submit_descriptor()` API 兼容(零回归)
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` 6 cases PASS
- [ ] `./build/bin/cpptlm_tests "[sdma]"` 既有套件零回归

**完工标准**: 6 unit cases PASS + 既有 `[sdma]` 零回归

---

## T4 [1.5d] VramControllerTLM 新建 (含 MemoryTLM protected 化前置)

**目标**: 新建 VramControllerTLM(继承 MemoryTLM + 行缓冲 + bandwidth 上限)。

### T4.0 [0.5d] MemoryTLM protected 化前置 (per M10 Oracle 反馈)

- [ ] `include/tlm/memory_tlm.hh`:
  - 把 `private:` 的 `req_in_` / `resp_out_` / `stats_` / `backing_view_` / `backing_view_size_` / `size_cap_` / `pending_resps_` / `current_cycle_` 改为 `protected:`
  - 添加 `friend class VramControllerTLM;` (允许访问 protected 成员)
  - 新增 public setter: `void set_use_zero_delay_for_test(bool b) noexcept { use_zero_delay_for_test_ = b; }` (per M5)
  - 新增 public setter: `void set_timing_params(uint64_t read_hit, uint64_t read_miss, uint64_t write) noexcept;` (per design §3.3)
  - **保留** v2.2 公共 API 零修改 (`set_backing_view` / `on_config_loaded` / `host_addr` / `backing_view_size` / `has_backing` / `req_in` / `resp_out` 不变)
  - `[memory_tlm]` 既有单测**零回归**验证(改 access 修饰符不影响行为)

### T4.1 [0.5d] 类实现

- [ ] `include/tlm/vram_controller_tlm.hh` 新建:
  - `class VramControllerTLM : public MemoryTLM`
  - `void set_vram_params(uint64_t row_hit_cycles, uint64_t row_miss_cycles, uint64_t bandwidth_gbps);`
  - `void invalidate_row_buffer();`
  - `bool row_hit(uint64_t addr) const noexcept;` (覆盖 MemoryTLM::row_hit,使用 VRAM 行缓冲)
  - `void on_config_loaded() override;` (读 vram_size_bytes)
  - 私有: row_buffer_base_ / row_buffer_valid_ / 3 params
  - 额外 stats: `vram_row_hits_` / `vram_row_misses_` / `bandwidth_limit_waits_`
- [ ] `src/tlm/vram_controller_tlm.cc` 新建:
  - `VramControllerTLM::tick()` override MemoryTLM::tick,加入行缓冲 hit/miss + bandwidth 累加
  - 复用 MemoryTLM::tick() 的 pending_resps_ 处理(继承,priority queue per M1)
- [ ] **`include/chstream_register.hh:46` 宏体内追加** (per H1 + M10):
  ```cpp
  #define REGISTER_CHSTREAM                                                                           \
      ModuleFactory::registerObject<CacheTLM>("CacheTLM");                                            \
      ModuleFactory::registerObject<MemoryTLM>("MemoryTLM");                                          \
      ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM");                                      \
      /* 🆕 v0.2 (per ADR-DGPU-11 + Oracle H1): */                                                        \
      ModuleFactory::registerObject<VramControllerTLM>("VramControllerTLM");                          \
      /* ... 既有其他模块注册保持不变 ... */
  ```
- [ ] **不**使用 `REGISTER_CHSTREAM(VramControllerTLM)` (无参宏,传参会预处理失败)
- [ ] **不**使用 `REGISTER_MODULE(VramControllerTLM)` (`is_base_of<SimModule>` 静态断言失败 — VramControllerTLM 继承 MemoryTLM → ChStreamModuleBase → SimObject)

### T4.2 [0.5d] 单元测试

- [ ] `test/test_vram_controller_tlm.cc` 新建,5 cases:
  - 行缓冲 hit (同 row 连续访问)
  - 行缓冲 miss + row buffer 更新
  - bandwidth 上限 → 128B req 累加 4 cyc wait
  - invalidate_row_buffer() 后下次访问 row miss
  - on_config_loaded() 读 vram_size_bytes
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` 5 cases PASS (v0.2 简化, 与行 159 数字一致)
- [ ] `cmake --build build` 编译通过

**完工标准**: 5 unit cases PASS + 模块注册成功

---

## T5 [2d] CrossbarTLM C++ 扩展 + DGpuBoard::init_timing_mode

**目标**: DGpuBoard 加 `simulation_mode_` 字段 + `init_timing_mode()` 分支 + cycle advance 统一推进。**CrossbarTLM 复用既有 C++ 代码,仅 JSON 配置**。

### T5.1 [0.5d] DGpuBoard 扩展

- [ ] `include/tlm/gpu/dgpu_board_shell.hh`:
  - 新增 `enum class SimulationMode { Functional, Timing };`
  - 新增 `SimulationMode simulation_mode() const noexcept;`
  - 新增 `uint64_t current_cycle() const noexcept;`
  - 新增私有:
    - `SimulationMode simulation_mode_ = SimulationMode::Functional;`
    - `uint64_t current_cycle_ = 0;`
    - `std::vector<ChStreamModuleBase*> cycle_advance_modules_;` // 仅含 MemoryTLM + VramControllerTLM (剔除 GmmuTLM 同步 per M14 + 剔除 SdmaEngineTLM 无 advance_cycle 方法 per R10)
    - `void init_timing_mode();`
- [ ] `src/tlm/gpu/dgpu_board_shell.cc`:
  - `init()` 内读 `cfg_["simulation_mode"]` 字段,默认 functional
  - `init_timing_mode()` 实现:
    - 找 internal modules (memory / sdma / vram_ctrl,**剔除 gmmu** 同步不需要,per M14)
    - 注入时序参数 (从 JSON 读)
    - 注册 cycle_advance_modules_ `[memory, vram_ctrl]` (**剔除** sdma, 无 advance_cycle 方法 per R10)
  - `tick()` 内 ++current_cycle_ + 广播 advance_cycle()

### T5.2 [0.5d] JSON 配置示例

- [ ] `configs/dgpu_soc_timing_v1.json` 新建(7-8 模块,**0 new connections**, per arch doc §3.6 v0.2 简化)
  - 顶层 `simulation_mode: "timing"`
  - 4-port loopback Crossbar (SDMA / Host egress / CPU/Other / Memory 或 VramCtrl 二选一;GMMU **不**在 Crossbar 数据路径,per M9;**无** 串联链 VramCtrl→Memory cascade,per H2)
  - 各模块时序参数(参考 [design.md §7](../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/design.md))
- [ ] `cmake --build build --target validate_topology` PASS
- [ ] `./build/bin/cpptlm_tests "[dgpu_board]"` 既有套件零回归
- [ ] `./build/bin/cpptlm_tests "[minimal_dgpu_soc]"` minimal_v1 functional-mode 零回归(关键!)

**完工标准**: `dgpu_soc_timing_v1.json` validate PASS + minimal_v1 零回归

---

## T6 [1d] dgpu_soc_timing_v1.json + minimal test 整合

**目标**: 完整 dgpu_soc_timing_v1.json 加载 + 集成测试。

### T6.1 [0.5d] 集成测试

- [ ] `test/test_dgpu_soc_timing_init.cc` 新建:
  - 加载 `dgpu_soc_timing_v1.json` + DGpuBoard::init() 调用
  - 验证 `simulation_mode_ = SimulationMode::Timing`
  - 验证 8 注册类型 / 7 实例化 (VramControllerTLM **或** MemoryTLM 二选一, per H2 互斥)
  - 验证 5 个时序参数 setter 被调用(检查 current_cycle_ 推进)
- [ ] `test/test_dgpu_soc_timing_inv.cc` 新建,6 cases(6 条新 Invariants):
  - Inv-1: 共享 vram_storage_
  - Inv-2: 同 cycle 内 ordering
  - Inv-3: cycle advance 对齐
  - Inv-4: backdoor 禁用性能测试
  - Inv-5: BAR 路由优先级
  - Inv-6: translate_cb 签名兼容
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` 全部 PASS (验证 8 注册类型 / 7 实例化:VramControllerTLM **或** MemoryTLM 二选一, per H2 互斥)

### T6.2 [0.5d] test 基础设施

- [ ] `test/CMakeLists.txt` 添加 `[dgpu_soc_timing]` 标签 test 源
- [ ] `cmake --build build` 编译通过(完整 build)
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` ≥ 30 cases PASS

**完工标准**: 30+ unit cases 全绿 + 6 Invariants 验证 PASS

---

## T7 [2d] E2E 性能回归测试

**目标**: 5 个 E2E 性能回归测试,验证 throughput / fence cycle accuracy / TLB hit rate / 并发 / 综合。

### T7.1 [1d] H2D/D2H throughput 测试

- [ ] `test/test_dgpu_soc_timing_h2d.cc` 新建,1 E2E case:
  - 4MB 数据 + 1024 个 4KB descriptor
  - 仿真 1M cycles
  - 断言 throughput ≥ 10 GB/s, ≤ 32 GB/s (VramCtrl bandwidth 上限)
  - 验证 stats_latency_read_ 平均 100-200 cyc
- [ ] `test/test_dgpu_soc_timing_d2h.cc` 新建,1 E2E case:
  - 4MB D2H + MSI-X cycle 验证
  - 断言 throughput 期望范围

### T7.2 [0.5d] TLB hit rate + 并发测试

- [ ] `test/test_dgpu_soc_timing_tlb.cc` 新建,1 E2E case:
  - 128KB 工作集 (32 pages) 重复访问 → hit rate ≈ 96.875% (initial fill 32 miss + 992 hit)
  - 4MB 顺序流 → hit rate 0% (32-entry 直接映射, 无 LRU thrashing)
- [ ] `test/test_dgpu_soc_timing_concurrent.cc` 新建,1 E2E case:
  - SDMA + GMMU + Host egress 同时提交
  - 验证 cycle 累加正确 + Crossbar arb stats
  - 验证 fence 顺序正确

### T7.3 [0.5d] 全套回归

- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` 全部 PASS (验证 8 注册类型 / 7 实例化:VramControllerTLM **或** MemoryTLM 二选一, per H2 互斥) (≥ 35 cases)
- [ ] `./build/bin/cpptlm_tests "[minimal_dgpu_soc]"` 零回归 (41 assertions)
- [ ] `./build/bin/cpptlm_tests "[pcie-memory]"` 零回归 (24 cases)
- [ ] `./build/bin/cpptlm_tests "[abi][minimal_dgpu_soc]"` 零回归 (28 assertions)
- [ ] `./build/bin/cpptlm_tests "[dgpu_board]"` 零回归
- [ ] `./build/bin/cpptlm_tests "[sdma]"` 零回归
- [ ] `./build/bin/cpptlm_tests "[gmmu]"` 零回归
- [ ] `./build/bin/cpptlm_tests "[memory_tlm]"` 零回归

**完工标准**: 5 E2E cases PASS + 所有 functional-mode 测试零回归

---

## T8 [0.5d] 文档同步 + OpenSpec archive 准备

**目标**: AGENTS.md 状态看板更新 + ADR-DGPU-11 Status Update + OpenSpec archive 流程。

- [ ] **T8.1** `AGENTS.md` 更新:
  - D-AXI Driver-Visible Minimal SoC 表追加 timing-mode 行:`✅ v0.5 timing-mode (AT) SoC MVP — Oracle 评审通过`
  - 添加 `docs/designs/dgpu-soc/timing-mode.md` 链接到 PCIe EP 微架构章节
- [ ] **T8.2** `docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md` Status Update 段追加:
  - "Status Update 2027-02-XX: 实施完成,tests X cases PASS, 0 regressions"
  - 引用 OpenSpec change commit
- [ ] **T8.3** `docs/designs/dgpu-soc/architecture.md` §14 已知推迟项引用 timing-mode.md:
  - "**Timing-mode SoC**: 见 [timing-mode.md](./timing-mode.md) (v0.5 active,与本功能模式并列)"
- [ ] **T8.4** OpenSpec validate:
  - `openspec validate cpptlm-dgpu-soc-timing-mvp --strict` PASS
  - `openspec validate --changes --strict` PASS
- [ ] **T8.5** `scripts/test/docs_sync_check.sh --strict` PASS
  - 新增 `docs/designs/dgpu-soc/timing-mode.md` 路径加入文件索引
  - 新增 `openspec/changes/cpptlm-dgpu-soc-timing-mvp/` 路径

**完工标准**: 文档全套同步 + openspec validate PASS + docs_sync_check PASS

---

## 总工期汇总

| 任务 | 工作量 | 累计 |
|------|--------|------|
| T0 OpenSpec + ADR | 1d | 1d |
| T1 MemoryTLM | 1d | 2d |
| T2 GmmuTLM | 1d | 3d |
| T3 SdmaEngineTLM cycle accounting (v0.2 简化, **不**新增端口) | **0.5d** | 3.5d |
| T4 VramControllerTLM 新建 (含 T4.0 MemoryTLM protected 化 0.5d + T4.1 类实现 0.5d + T4.2 单元测试 0.5d) | **1.5d** | 5d |
| T5 Crossbar C++ 扩展 + DGpuBoard | **2d** | 7d |
| T6 JSON + minimal test | 1d | 8d |
| T7 E2E 性能回归 | 2d | 10d |
| T8 文档同步 | 0.5d | 10.5d |
| **小计** | **10.5d** | - |
| Buffer | 1.5d | 12d |

---

## 风险与回退

| 风险 | 检测 | 回退 |
|------|------|------|
| T1 defer_response 破坏 functional | `./build/bin/cpptlm_tests "[memory_tlm]"` regression | 立刻 revert T1.1,保持既有 v2.2 行为 |
| T2 translate_timing 影响既有 translate | `./build/bin/cpptlm_tests "[gmmu]"` regression | translate() 内部加 wrapper,丢弃 latency |
| ~~T3 SDMA outstanding race~~ | (v0.2 删除 SDMA outstanding, per H3) | N/A |
| T4 VramCtrl 编译失败(继承 MemoryTLM) | `cmake --build build` 失败 | 改为 composition(非继承),但需复制 pending_resps_ 逻辑 |
| T5 cycle advance 漂移 | 多模块 current_cycle 不一致 | Inv-3 强制 board 集中 advance,模块提供 setter |
| T6/T7 测试失败率 > 30% | 实施回退到 T5 完成态 | 单独推迟 T7 E2E,functional-mode + minimal test 单独交付 |
| 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) 回归 | regression 检测 | 回退单个 commit,定位 minimal_v1 受影响的 commit |

---

## 完成定义 (Final DoD)

- [ ] **T0-T8 全部完成**,所有 task checkboxes 勾选
- [ ] `cmake --build build -j$(nproc)` 全量编译 PASS,无 warning
- [ ] `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` ≥ 35 cases 全绿
- [ ] `./build/bin/cpptlm_tests "[minimal_dgpu_soc][pcie-memory][abi][sdma][gmmu][memory_tlm][dgpu_board]"` 零回归 (≥ 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) 全绿)
- [ ] `openspec validate cpptlm-dgpu-soc-timing-mvp --strict` PASS
- [ ] `openspec validate --changes --strict` PASS
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS
- [ ] `git diff HEAD -- include/abi/cpptlm_emulator.h` 仅含既有内容,**无**signature 改动
- [ ] ADR-DGPU-11 已签发 + Status Update 已追加
- [ ] Oracle 评审通过(proposal §5 关键架构决策 5 项)
- [ ] 文档全套同步(arch doc + README + AGENTS.md)