# minimal-dgpu-soc-test-coverage: v1.0 测试覆盖补全

> **所属 change**: [`cpptlm-minimal-dgpu-soc-v1-test-coverage`](../proposal.md)
> **范围**: v1.0 主 change 推迟的测试任务 (C1.1/C1.2/C1.3 + C2.1/C2.3 E2E)
> **关联 spec**: `minimal-dgpu-soc` (v1.0 主 spec,引用其 Scenario 作基线)
> **不变量来源**: 沿用 v1.0 spec `minimal-dgpu-soc` 的 6 条不变量 + §8 Inv-1~6 verbatim

---

## ADDED Requirements

### Requirement: tag-aggregation-summary

聚合 v1.0 主 change 引入的全部标签 SHALL 同时跑通,确认所有模块单元测试齐绿。

#### Scenario: 4 标签组合全 PASS

- **WHEN** 运行 `./build/bin/cpptlm_tests "[memory_backing],[gmmu],[dgpu_framebuffer]"`
- **THEN** 所有 4 标签 case 全部 PASS, 断言数 ≥ 50 (memory_backing 5×4 + gmmu 9 + dgpu_framebuffer 6 + characterization 7)

### Requirement: boundary-edge-cases

framebuffer_ + BAR1 路由 SHALL 锁定边界行为: `framebuffer_size_=0` 全访问 fallback 到 vram_segments_ (返 0, 数据一致); BAR1 非 4/8 字节 len 字节透传一致。

#### Scenario: framebuffer_size_=0 fallback 到 vram_segments_

- **WHEN** `attach_framebuffer_for_testing(nullptr, 0)` 后调用 `backdoor_write/read`
- **THEN** 全部返 `0` (OK), 数据落到 vram_segments_ (per `dgpu_board_shell.cc:536-542` fallback 路径)
- **AND** `REQUIRE(board.backdoor_write(off, data, len) == 0)` + `REQUIRE(board.backdoor_read(off, out, len) == 0)` + `REQUIRE(out == data)` 全部通过 (与既有 PASS 测试 `test_dgpu_board_framebuffer.cc:79-90` 一致锁定)
- **注**: Oracle 2027-02-10 复审决策 — 原"全返 -EINVAL"与既有 PASS 测试锁定的 fallback 行为直接矛盾, 改为 fallback 锁定 (符合本 change "纯测试代码 0 实现改动" 自我约束)

#### Scenario: BAR1 非 4/8 字节 len 行为锁定

- **WHEN** `mmio_write(1, 0x200, buf, 1)` (1 字节), 后续 `mmio_read(1, 0x200, out, 1)`
- **THEN** `out[0] == buf[0]` (1 字节透传, 不触发零字节或越界错误)

### Requirement: pt-base-lo-hi-race

PT_BASE LO/HI 写 race SHALL 锁定中间态可被 translate 接受的行为 (per Inv-4 verbatim "v1.0 接受此 race")。

#### Scenario: LO 已写 HI 未写时 translate 用 LO-only 中间态

- **WHEN** `set_pt_base_lo(0x10000)` 后立即 `set_enabled(true)` (HI 仍为 0)
- **AND** backing 已注入且 PT_BASE 对应 PTE[0] 已 valid
- **THEN** `translate(0x0, 4096, pa)` 行为可锁定 (LO=0x10000 + HI=0 翻译出 pa 来自 PTE[0] 映射)

### Requirement: minimal-dgpu-soc-e2e-link

端到端 SHALL 验证 host ABI → PCIe BAR → SDMA → GMMU → framebuffer_ 完整链路。

**Oracle 2027-02-10 复审**: T-bs-4 已于 commit `8cd1f033` (D15 true fix - null-guard unregistered module types in instantiateAll) 完成, 996 cases / 32587 assertions PASS; 此 Requirement **无架构依赖**。真实失败根因优先级: (1) test harness framebuffer 显式挂载缺漏, (2) SDMA/GMMU/PCIeEP 数据通路 bug。

#### Scenario: 全链路 H2D + 双读回一致

- **WHEN** 加载 `configs/dgpu_soc_minimal_v1.json` (16MB framebuffer_, 两 flag 启用)
- **AND** **显式 `attach_framebuffer_for_testing(buf, 16MB)`** (关键: `load_soc_config` 不消费 JSON 顶层 `framebuffer_size_bytes`, 必须测试代码显式挂载, 否则 `framebuffer_size_=0` → 全 BAR1/backdoor 返 OUT_OF_RANGE)
- **AND** `init()` (顺序 Inv-2: load → attach → init)
- **AND** host backdoor 注入 16KB pattern 缓冲 (PTE 写 ≥ phys+size)
- **AND** host `mmio_write(0,0,LO,4)` + `mmio_write(0,4,HI,4)` (PT_BASE=0x10000) + `mmio_write(0,8,1,4)` (enable)
- **AND** `mmio_write(1, 0x10008, &pte, 8)` (PTE[1] = `(0x2000 & ~0xFFF) | 1` = 0x2001)
- **AND** `ring_write_entry` 提交 H2D (iova=0x1000, vram_offset=0, size=4096) + doorbell wptr
- **AND** `board.tick()` 推进至完成
- **THEN** `mmio_read(1, 0, out, 4096)` == `host_buf[0x2000..0x3000)` (phys=0x2000 经 PTE 译出)
- **AND** `backdoor_read(0, out2, 4096)` == 同数据 (BAR1 路由与 backdoor 读同一份 framebuffer_)

#### Scenario: fence 完成触发 MSI-X vector 0

- **WHEN** 上一 Scenario 的描述符链含 Fence 描述符
- **AND** MSI-X coalescing 显式 disable (`set_msix_coalesce_enabled(false)`)
- **THEN** host `irq_cb_` 收到 vector == `SdmaEngineTLM::kSdmaFenceVector` (== 0)

---

## MODIFIED Requirements

> **MODIFIED 语义**: 本 change 仅扩展测试覆盖, 不修改 v1.0 spec 既有 behavior. 引用而非复制 v1.0 spec 的 6 个 Requirement (`gmmu-single-level-translation`, `gmmu-mmio-register-interface`, `memory-tlm-backing-store`, `framebuffer-single-backing`, `bar1-storage-routing`, `sdma-gmmu-translate-injection`) 作为基线, 通过新增 requirement 增补其边界覆盖.

---

## 兼容性约束 (引用, 非 Requirement)

- **15 ABI 字节级兼容** — `git diff HEAD -- include/abi/cpptlm_emulator.h` 必须为空
- **既有 66805 assertions 零回归** (per v1.0 主 change AC9)
- C2.1-C2.4 E2E 测试失败走 cpptlm-debug SKILL 6 步独立定位 (**Oracle 2027-02-10**: T-bs-4 已完成, 不再预设 SIGSEGV 前提; 真实根因优先级 (1) framebuffer 挂载, (2) SDMA/GMMU/PCIeEP 数据通路)
- v1.0 主 change 的 tasks.md 推迟项 (C1.x/C2.x) 在本 change 完成并 archive 后标 `won't do` 并补一行理由