# cpptlm-minimal-dgpu-soc-v1-test-coverage: Tasks (TDD)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/minimal-dgpu-soc-test-coverage/spec.md`](specs/minimal-dgpu-soc-test-coverage/spec.md)
> **依赖**: `cpptlm-minimal-dgpu-soc-v1` 主 change 已完成 (v1.0 实现); **Oracle 2027-02-10 复审: 无架构级前置依赖, T-bs-4 已于 commit `8cd1f033` 完成**
> **工期估算**: 1.5 人日 (C1: 4h; C2: 8h; D: 1h)
> **TDD 纪律**: 每任务 5 步 (RED → VALIDATE-FAIL → IMPL → VALIDATE-PASS → REFACTOR); 子任务 1-3h
> **硬门槛**: 既有 66805 assertions 零回归 + 15 ABI 字节级兼容 (`git diff HEAD -- include/abi/cpptlm_emulator.h` 为空)

---

## 文件清单

| 文件 | 变化 | 任务 |
|------|------|:----:|
| `test/test_minimal_dgpu_soc_characterization.cc` | **扩展** | C1.1, C1.2 |
| `test/test_dgpu_board_framebuffer.cc` | **扩展** | C1.2 (framebuffer_size_=0) |
| `test/test_gmmu_tlm.cc` | **扩展** | C1.3 |
| `test/test_minimal_dgpu_soc_e2e.cc` | **新** | C2.1, C2.2, C2.3 |
| `openspec/changes/cpptlm-minimal-dgpu-soc-v1/tasks.md` | **修改** | 标 C1.x/C2.x `won't do` |
| `openspec/changes/cpptlm-minimal-dgpu-soc-v1/design.md` | **修改** | §8 修订注记追加引用 |
| `test/CMakeLists.txt` | 不改 (file GLOB 自动发现) | — |

---

## Phase C1: 防御性 + 边界用例 — 估 4h (本 session 可完成)

### C1.1: 标签汇总 SECTION — 估 30min

- [x] **C1.1.1** RED: 在 `test_minimal_dgpu_soc_characterization.cc` 末尾加 SECTION "tag-aggregation", 跑组合标签 `[memory_backing],[gmmu],[dgpu_framebuffer]`, 断言全部存在且 ≥ 50 assertions — **[15min]**
- [x] **C1.1.2** VALIDATE-PASS: 标签组合全绿 — **[15min]**

**验收**: AC1 达成 (4 标签组合 PASS)。

### C1.2: 边界用例 — 估 2h

> **2027-02-10 修订注记 (per Oracle 复审)**: spec 已从 "framebuffer_size_=0 全返 -EINVAL" 修订为 "fallback 到 vram_segments_ 返 0" (见 `specs/minimal-dgpu-soc-test-coverage/spec.md:25-31`)。本 change 自我约束 "纯测试代码 0 实现改动" — 边界用例锁定既有 PASS 行为 (framebuffer_size_=0 fallback), 不要求生产路径调整。

- [x] **C1.2.1** RED: `test_dgpu_board_framebuffer.cc` 加 SECTION "framebuffer_size_=0" — `attach_framebuffer_for_testing(nullptr, 0)` 后 backdoor 写读 + BAR1 mmio 写读 fallback 到 vram_segments_ 返 0 — **[30min]**
- [x] **C1.2.2** RED: 同文件加 SECTION "BAR1 non-4/8-byte len" — `mmio_write(1, 0x200, &val, 1)` 后 `mmio_read(1, 0x200, &out, 1)` 字节一致 — **[30min]**
- [x] **C1.2.3** VALIDATE-PASS: 两 SECTION 全绿 + 既有 [dgpu_framebuffer] 6/6 零回归 — **[30min]**
- [x] **C1.2.4** REFACTOR: 注释 + D13 DPRINTF 告警路径自检 — **[30min]**

**验收**: AC2 达成。

### C1.3: PT_BASE LO/HI race 锁定 — 估 1h

- [x] **C1.3.1** RED: `test_gmmu_tlm.cc` 加 SECTION "pt_base LO/HI race" — `set_pt_base_lo(0x10000)` 后立即 translate (HI 仍 0), 验证 race 行为可锁定 (LO-only 中间态翻译, per Inv-4 "v1.0 接受 race") — **[30min]**
- [x] **C1.3.2** VALIDATE-PASS: SECTION 全绿 + 既有 [gmmu] 9/9 零回归 — **[30min]**

**验收**: AC3 达成。

---

## Phase C2: E2E 测试 — 估 8h (Oracle 2027-02-10 复审: 无 T-bs-4 依赖)

### C2.1: E2E 全链路 H2D — 估 3h

- [x] **C2.1.1** RED: 创建 `test/test_minimal_dgpu_soc_e2e.cc` 双标签 `[minimal_dgpu_soc][e2e]` — Scenario "全链路 H2D + 双读回一致" (spec `minimal-dgpu-soc-test-coverage::minimal-dgpu-soc-e2e-link`)
  - 加载 `dgpu_soc_minimal_v1.json` (16MB framebuffer_, 两 flag 启用) → init
  - **Oracle 2027-02-10 重要提示**: `load_soc_config` 不消费 JSON 顶层 `framebuffer_size_bytes` 字段, 测试必须显式调 `attach_framebuffer_for_testing(buf, 16MB)`, 否则 `framebuffer_size_=0` → 全 BAR1/backdoor 返 OUT_OF_RANGE
  - host backdoor 注入 16KB pattern
  - PT_BASE=0x10000 (LO+HI) + enable
  - `mmio_write(1, 0x10008, &pte, 8)` 写 PTE[1]=0x2001
  - `ring_write_entry` H2D (iova=0x1000, vram=0, size=4096) + doorbell wptr
  - `board.tick()` 推进
  - 断言 `mmio_read(1, 0, out, 4096)` == `host_buf[0x2000..0x3000)` + `backdoor_read(0, ...)` 一致 — **[1.5h]**
- [x] **C2.1.2** VALIDATE-PASS (首轮即绿): 显式 framebuffer 挂载规避根因 #1; 无数据通路 bug (GMMU PTE 编码 + SDMA ring + doorbell sync 路径均正确)。测试链路支撑改动: `DGpuBoard::sdma_engine()` accessor + `modules_cluster.hh` GmmuTLM 短名注册 — **[1h]**
- [x] **C2.1.3** 路径 + 期望数据校验 (offset 计算 + host_buf 边界) — **[30min]**

**验收**: AC4 达成。

### C2.3: E2E fence MSI-X — 估 1h (依赖 C2.1 PASS)

- [x] **C2.3.1** RED: `test_minimal_dgpu_soc_e2e.cc` 加 Scenario "fence → MSI-X vector 0" — 提交 Fence 描述符 + `set_msix_coalesce_enabled(false)` + 断言 `irq_cb_` 收到 vector==0 — **[30min]**
- [x] **C2.3.2** VALIDATE-PASS: 断言 vector==`kSdmaFenceVector` (==0) — **[30min]**

**验收**: AC5 达成。

### C2.4: 诊断清零复查 — 估 15min

- [x] **C2.4.1** `grep -l "static FILE\* diag\|fopen(\"/tmp/" include/ src/ test/ -r` 为空 — **[15min]**

**验收**: AC6 达成。

---

## Phase D: 归档前修订 — 估 1h

### D1: 修主 change artifacts — 估 30min

- [x] **D1.1** `openspec/changes/archive/2026-09-24-cpptlm-minimal-dgpu-soc-v1/tasks.md` — **Oracle 2027-02-10 复审: 主 change 已归档, 归档版 tasks.md L117-138 已将 C1.x/C2.x 标 `won't do (移交)`, 仅 L120/L135 的 "依赖 T-bs-4" 陈腐理由需追加修订注记 (per OpenSpec 惯例归档文件以追加修订注记为主, 不修改既有文字)** — **[15min]**
- [x] **D1.2** `openspec/changes/archive/2026-09-24-cpptlm-minimal-dgpu-soc-v1/design.md` §8 修订注记追加一行 "v1.0.1 test-coverage follow-up created, see openspec/changes/cpptlm-minimal-dgpu-soc-v1-test-coverage/" — **[15min]**

### D2: 验证 + 归档 — 估 30min

- [x] **D2.1** `openspec validate cpptlm-minimal-dgpu-soc-v1 --strict` PASS (主 change archive 后仍 PASS) — **[15min]**
- [x] **D2.2** `openspec validate cpptlm-minimal-dgpu-soc-v1-test-coverage --strict` PASS — **[15min]**

### D3: 回归 — 估 30min (C1 完成时即可跑, 不需等 C2)

- [x] **D3.1** 全量回归: `./build/bin/cpptlm_tests` 既有 66805 + 新增 C1 assertions 全 PASS, 0 新失败 — **[15min]**
- [x] **D3.2** ABI 校验: `git diff HEAD -- include/abi/cpptlm_emulator.h` 为空 — **[15min]**

**验收**: AC7 + AC8 + AC9 达成。

---

## 依赖图

```
C1.1 ─┐
C1.2 ─┼─ D3 (本 session 内)
C1.3 ─┘
C2.1 ─┐
C2.3 ─┼─ D2 (本 session 或跨 session)
C2.4 ─┘
      │
      ▼
D1 (主 change tasks/design 修订) → openspec archive
```

**关键路径 (本 session)**: C1.1 + C1.2 + C1.3 + D3 (≈ 4h)
**关键路径 (后续)**: C2.1 → C2.2 修复 (按根因) → C2.3 → D2 → archive (≈ 10h)
**Oracle 2027-02-10 注**: T-bs-4 已于 commit `8cd1f033` 完成, 不在依赖图中。