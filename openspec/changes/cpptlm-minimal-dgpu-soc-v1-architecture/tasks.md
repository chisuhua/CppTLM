# Tasks: cpptlm-minimal-dgpu-soc-v1-architecture

> **总工期**: 1.5-2.5 工作日
> **关联 change**: cpptlm-minimal-dgpu-soc-v1-architecture
> **关联 ADR**: ADR-DGPU-05/06/07/10
> **关联 spec**: specs/minimal-dgpu-soc-architecture/spec.md

## T0 [0.5h] 基线构建 + 测试快照

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` 配置
- [ ] `cmake --build build -j$(nproc)` 全量编译
- [ ] `./build/bin/cpptlm_tests "[pcie-memory]"` baseline (24 cases PASS)
- [ ] `./build/bin/cpptlm_tests "[minimal_dgpu_soc]"` baseline (41 assertions PASS)
- [ ] `./build/bin/cpptlm_tests "[sdma]"` baseline (既有 [sdma] 套件全绿)
- [ ] `./build/bin/cpptlm_tests "[abi][minimal_dgpu_soc]"` baseline (28 assertions PASS)
- [ ] 记录 baseline commit hash + 0 regressions 状态

## T1 [0.5d] MemoryTLM setter rename

- [ ] `include/tlm/memory_tlm.hh`:
  - 新增 `void set_backing_view(uint8_t* ptr, uint64_t size) noexcept`
  - 字段 `backing_ptr_` → `backing_view_`(raw ptr)
  - 字段 `backing_size_` → `backing_view_size_`
  - 旧 `set_backing_store()` 标 `[[deprecated("use set_backing_view")]]`,内部转调新 setter
- [ ] `src/tlm/gpu/dgpu_board_shell.cc:616` 调用 `set_backing_store` → `set_backing_view`
- [ ] `cmake --build build` 编译通过
- [ ] `./build/bin/cpttlm_tests "[pcie-memory]"` 24 cases PASS(无 deprecation 警告)
- [ ] `grep -rn 'set_backing_store\|backing_ptr_\b' src/tlm/ test/` 必须 0 匹配(除 deprecation wrapper)

## T2 [0.5d] GmmuTLM 字段+setter rename

- [ ] `include/tlm/gpu/gmmu_tlm.hh`:
  - 新增 `void set_mem_view(uint8_t* ptr, uint64_t sz) noexcept`
  - 字段 `backing_` → `mem_view_`
  - 字段 `backing_size_` → `mem_view_size_`
  - 旧 `set_backing()` 标 `[[deprecated("use set_mem_view")]]`,内部转调新 setter
  - `translate()` 函数引用 `backing_` → `mem_view_`
- [ ] `src/tlm/gpu/dgpu_board_shell.cc:635` 调用 `set_backing` → `set_mem_view`
- [ ] `cmake --build build` 编译通过
- [ ] `./build/bin/cpttlm_tests "[sdma]"` 既有套件全绿
- [ ] `./build/bin/cpttlm_tests "[minimal_dgpu_soc]"` 41 assertions PASS
- [ ] `grep -rn 'set_backing\b\|backing_\b' include/tlm/gpu/gmmu_tlm.hh` 必须 0 匹配(除 deprecation wrapper)

## T3 [0.5d] PcieMemoryDevice 删除 `memory_backing_` (per ADR-DGPU-05 v1.4 B7)

- [ ] `include/tlm/gpu/pcie_memory_device.hh`:
  - 删除 `std::vector<uint8_t> memory_backing_;` 字段
  - 新增 `uint8_t* backing_view_ = nullptr;` + `uint64_t backing_view_size_ = 0;`
  - 新增 `void set_backing_view(uint8_t* ptr, uint64_t size) noexcept`
  - `has_memory_backing()` → `has_backing_view()` 返回 `backing_view_ != nullptr`
  - `memory_read/write` 函数使用 `backing_view_` 替代 `memory_backing_`
- [ ] `include/tlm/gpu/pcie_memory_device.cc`:
  - `memory_backing_.resize/empty/size` → `backing_view_` 或 `backing_view_size_`
  - `memory_read/write` 函数 body 同步
  - `ensure_memory_backing_allocated` 删除(改为 `set_backing_view` 注入)
- [ ] `src/tlm/gpu/dgpu_board_shell.cc:507,508,563,564` 调用 `ep->memory_device().memory_read/write(...)` 已存在,无需改动(只改 backing 字段)
- [ ] `cmake --build build` 编译通过
- [ ] `./build/bin/cpttlm_tests "[pcie-memory]"` 24 cases PASS(机械迁移)
- [ ] `grep -rn 'memory_backing_' include/tlm/gpu/pcie_memory_device.{hh,cc}` 必须 0 匹配

## T4 [1d] DGpuBoard 字段 rename (framebuffer_storage_ → vram_storage_)

- [ ] `include/tlm/gpu/dgpu_board_shell.hh`:
  - 新增 `std::unique_ptr<uint8_t[]> vram_storage_;` (8GB default-init, per D-AXI v1.4 B7)
  - 新增 `uint64_t vram_size_ = 0;`
  - 字段 `framebuffer_storage_` → 改为 `[[deprecated("use vram_storage_")]] std::vector<uint8_t>` (兼容)
  - 字段 `framebuffer_ptr_` 提取为方法 `vram_storage_ptr()`(但保留旧字段兼容)
  - 字段 `framebuffer_size_` 保留(语义是 framebuffer 总大小,不是 BAR1 窗口)
- [ ] `src/tlm/gpu/dgpu_board_shell.cc`:
  - `framebuffer_storage_.resize(...)` → `vram_storage_ = std::make_unique<uint8_t[]>(size)` (改用 unique_ptr)
  - `framebuffer_storage_.data()` → `vram_storage_.get()`
  - `framebuffer_ptr_ = framebuffer_storage_.data();` → `framebuffer_ptr_ = vram_storage_.get();` (保留旧字段但指向新 owner)
  - `framebuffer_size_` 赋值 `final_size` (语义不变)
  - `attach_framebuffer_for_testing` 标 `[[deprecated]]`,内部转 `attach_vram_for_testing`
  - 添加 `attach_vram_for_testing` 方法
- [ ] `cmake --build build` 编译通过
- [ ] `./build/bin/cpttlm_tests "[minimal_dgpu_soc][driver_visible][e2e]"` PASS
- [ ] `grep -rn 'framebuffer_storage_' include/ src/ test/` 仅匹配 `dgpu_board_shell.{hh,cc}` (兼容层)

## T5 [1d] 测试套件迁移 + grep audit

- [ ] `test/pcie/*.cc` (24 cases):
  - `memory_backing_` 引用 → `backing_view_`
  - `set_backing_store` 调用 → `set_backing_view`
  - `framebuffer_storage_` 引用 → `vram_storage_`
- [ ] `test/test_minimal_dgpu_soc*.cc`:
  - `framebuffer_storage_` 引用 → `vram_storage_`
  - `set_backing_store` → `set_backing_view`
  - `set_backing` → `set_mem_view`
- [ ] `grep audit` (每个命令 0 匹配旧名,除 deprecation wrapper):
  - `grep -rn 'set_backing_store\b' include/ src/ test/ | grep -v deprecated`
  - `grep -rn 'memory_backing_' include/tlm/gpu/pcie_memory_device.{hh,cc}`
  - `grep -rn '\.backing_' include/tlm/gpu/gmmu_tlm.{hh,cc} | grep -v mem_view_`
  - `grep -rn 'framebuffer_storage_' include/ src/ test/ | grep -v 'dgpu_board_shell\|deprecated\|vram_storage_'`
- [ ] `./build/bin/cpttlm_tests` 全量测试 0 regression
- [ ] ctest 回归全绿 (`ctest --test-dir build --output-on-failure -j4`)

## T6 [0.5d] Oracle 评审 + Status Update 追加

- [ ] Oracle 一轮评审 (per OpenSpec 工作流):
  - 验证 ADR-DGPU-10 §4 Migration 8 步全部落地
  - 验证 5 消费者共享 vram_storage_ (per ADR-DGPU-05 v1.4 B7)
  - 验证 [pcie-memory] 24 cases 机械迁移 + 0 逻辑改动
  - 验证 [minimal_dgpu_soc] 41 assertions PASS
- [ ] ADR-DGPU-05 Status Update 追加 实施 commit hash(es)
- [ ] ADR-DGPU-10 Status Update 追加 实施 commit hash(es),状态从 📋 → ✅
- [ ] architecture.md §1.4 仿真模式声明 + §4.1 BAR 布局 + §5 GMMU 段落同步最新命名
- [ ] `openspec validate cpptlm-minimal-dgpu-soc-v1-architecture --strict` PASS
- [ ] `openspec archive cpptlm-minimal-dgpu-soc-v1-architecture` 归档

## Definition of Done (DoD)

- [ ] T0-T6 全部任务 completed
- [ ] grep audit 全部通过 (旧名 0 匹配,除 deprecation wrapper)
- [ ] 全量 build 0 编译错 + 0 deprecation 警告(除 deprecation wrapper 自身)
- [ ] 全量 ctest PASS (66951 + 24 + 41 + 28 = 67044 assertions)
- [ ] Oracle 一轮评审 PASS
- [ ] ADR-DGPU-05/10 Status Update 追加
- [ ] architecture.md §1.4 + §3 + §4 + §5 + §7 同步最新代码
- [ ] `openspec archive` 成功
- [ ] 归档后 `openspec/changes/cpptlm-minimal-dgpu-soc-v1-architecture/` 移至 `archive/`
- [ ] 同步归档: `docs/soc_arch/changes/cpptlm-minimal-dgpu-soc-v1-architecture/` (ArchForge mirror)

## 关键风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| 测试迁移漏改 | 🟡 M | `grep -rn 'set_backing_store\|memory_backing_'` 0 匹配(除 deprecation wrapper) |
| 旧 setter deprecation 警告刷屏 | 🟢 L | T0 baseline 后,新代码用新 setter;旧调用方不需改(deprecation 仅警告) |
| PcieMemoryDevice::memory_backing_ 删除破坏 lazy alloc 行为 | 🔴 H | 改用 `set_backing_view` 注入 (per D-AXI v1.4 B7),无 lazy alloc |
| DGpuBoard 字段双轨 (framebuffer_storage_ + vram_storage_) 长期共存 | 🟡 M | v2.0 删除旧字段;v1.0.x 提供 v1.5+ 兼容性 |
| Oracle 评审发现 must-fix | 🟢 L | 实施 T1-T3 完成后再启动 T4 (增量),must-fix 范围可控 |

## Oracle 评审检查清单

- [ ] ADR-DGPU-10 §3 Inv-1 验证: `grep -rn '_storage_' include/ src/ test/ | grep -v 'dgpu_board_shell\|vram_storage_' | grep -v deprecated` 必须 0 匹配
- [ ] ADR-DGPU-10 §3 Inv-2 验证: 全部 `_backing_view_` 字段类型是 `uint8_t*`(非 std::vector)
- [ ] ADR-DGPU-10 §3 Inv-3 验证: SdmaEngineTLM `_backdoor_` 字段类型是 `void*`(非 uint8_t*)
- [ ] ADR-DGPU-10 §3 Inv-4 验证: GmmuTLM `_mem_view_` 字段类型是 `uint8_t*`
- [ ] ADR-DGPU-05 §3 Inv-3 验证: PcieMemoryDevice 无 `memory_backing_` 字段
- [ ] ADR-DGPU-05 §3 Inv-5 验证: 所有消费者 `bound = injected backing_size_`
- [ ] `dgpu_soc_minimal_v1.json` BAR1 ≥ 256MB+64KB + doorbell offset 0x10010000 可命中
