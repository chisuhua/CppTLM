# 2026-07-16 RFCs to PTX-EMU（4 跨端契约项）

> **用途**: CppTLM Metis Round 3.5 + 4 审计产出的 **4 项跨端 RFC**，正式发往 PTX-EMU 团队请求决策
> **关联**:
> - Metis Round 4 审计全文: 本仓库 `.opencode/` session `ses_097b54540ffeOJQtALUFt4ff10`
> - CppTLM P0 change: [`openspec/changes/cpptlm-f12b-ld-impl/`](../../openspec/changes/cpptlm-f12b-ld-impl/)
> - CppTLM P1 change: [`openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../../openspec/changes/cpptlm-d1-pipeline-scoreboard/)
> - PTX-EMU P0 姊妹 change: `PTX-EMU/openspec/changes/cpptlm-d1-full/`
> - PTX-EMU P1 姊妹 change: `PTX-EMU/openspec/changes/cpptlm-phase8b-injection-points/`
> **发送时间**: 2026-07-16
> **发送方**: Chi (CppTLM maintainer)
> **接收方**: PTX-EMU 团队（review `cpptlm-d1-full` + `cpptlm-phase8b-injection-points`）
> **回复 SLA**: RFC-001/003/004 ≤ 48h；RFC-002 (Option A path) ≤ 1 周
> **关联 commit**: CppTLM `abd7dd5` (P0 BLOCK fixes) / PTX-EMU `8dc000e` (ABI v1)

---

## 0. 状态追踪

| RFC | 推荐选项 | PTX-EMU 回复 | 状态 |
|:---:|----------|--------------|------|
| RFC-001 HSK-1 #4 timing | A — 移到 P1 后 | ⏳ 待回复 | 🟡 Pending |
| RFC-002 `synchronize_stream` ABI | B — PTX-EMU 端 delegate | ⏳ 待回复 | 🟡 Pending |
| RFC-003 `kernel_args` lifecycle | A — `PendingKernel` 加字段 | ⏳ 待回复 | 🟡 Pending |
| RFC-004 Cross-repo links | A — 各 change proposal.md 加表 | ⏳ 待回复 | 🟡 Pending |

> **更新方式**: PTX-EMU 团队回复后，更新本表"PTX-EMU 回复"列并替换状态 emoji（🟢 Accepted / 🟠 Partial / 🔴 Rejected）。

---

## 1. 正式消息正文（英文，发送用）

**Subject:** 4 cross-project RFCs from CppTLM Metis audit (Rounds 3.5 + 4) — decisions needed before P0

Hi PTX-EMU team,

## Context

Ahead of implementing `cpptlm-f12b-ld-impl` (CppTLM P0), our Metis audit surfaced 4 items that touch your side of the shared ABI (`include/cudart/cpptlm_bridge.h` at `8dc000e`). HSK-1/2/3 are acknowledged on our end (`7ab1ec1`); the items below still need your team's call before we land P0 without rework. Sharing early so cross-repo changes stay in lockstep.

## Summary

| RFC | Problem | Recommendation | Ask |
|-----|---------|----------------|-----|
| 001 | HSK-1 #4 asks for 12-endpoint `static_assert`; CppTLM P0 defers 12-endpoint to P1 | Move #4 to a follow-up HSK after P1 (Option A) | Accept A? |
| 002 | `synchronize_stream` is in ABI but `cudaStreamSynchronize` never calls it (dual sync loop) | Delegate to `bridge->synchronize_stream(target_stream)`, no ABI bump | Pick A or B |
| 003 | CppTLM deep-copies `kernel_args`; `PendingKernel` (design.md:146-154) has no field to store them | Add `std::vector<std::vector<uint8_t>> copied_args` to `PendingKernel` (Option A) | Accept A? |
| 004 | Cross-repo sister-change links are asymmetric (your P1 ↔ our P1) | Add Cross-Project Counterparts sections (snippet below) | Paste snippet into both `proposal.md` |

### RFC-001 — 12-endpoint `static_assert` timing

**Problem.** PTX-EMU HSK-1 #4 asks CppTLM to verify a 12-endpoint (`PipelineId × TcPrecision`) `static_assert`. Our P0 spec defers the full matrix to P1 (`cpptlm-d1-pipeline-scoreboard`) on the grounds that P0 ships only `MemoryBridge` + `KernelLaunchTLM` extension + `query_latency`. Folding the full matrix into P0 would expand P0 scope beyond what P1 is sized for.

**Recommendation (Option A).** Move HSK-1 #4 to a follow-up HSK emitted once `cpptlm-d1-pipeline-scoreboard` lands.

**Ask.** Accept Option A? If yes, please update `cpptlm-d1-full/hsk-1.md:83` flagging #4 as deferred-and-re-emitted. If you prefer Option B (placeholder `static_assert` skeleton in P0), confirm the skeleton-only structure is acceptable.

### RFC-002 — `synchronize_stream` ABI redundancy

**Problem.** `cpptlm_bridge.h` declares `synchronize_stream(uint64_t stream_id)` as pure virtual. `cudaStreamSynchronize` iterates `g_pending_kernels` and calls `bridge->poll_kernel(id)` directly — `synchronize_stream` is never invoked. Two independent sync loops violate ADR-NV-01 ("CppTLM is single clock-of-truth").

**Recommendation (Option B).** Have `cudaStreamSynchronize` delegate to `bridge->synchronize_stream(target_stream)` and forward the bridge's error code. `CPPTLMBRIDGE_VERSION` stays at 1.

**Ask.** Pick A (remove method, bump to 2 — requires re-handshake) or B (delegate, ~1h on your side)? We default to B unless you're mid-bump.

### RFC-003 — `kernel_args` lifecycle gap

**Problem.** ABI docs require CppTLM to deep-copy `kernel_args`; CppTLM implements this. But `PendingKernel` at `cpptlm-d1-full/design.md:146-154` has no field for the copy. With async `exe_once`, the simulator has no args to consume — the contract is half-closed.

**Recommendation (Option A).** Add `std::vector<std::vector<uint8_t>> copied_args` to `PendingKernel`, deep-copy in `register_pending_kernel`. Matches the CppTLM-side copy in `src/tlm/gpu/memory_bridge.cc`. Minimum-surface fix; no ABI change.

**Ask.** Accept Option A? (B = add `get_kernel_args(id)`; C = force synchronous exec — both rejected.)

### RFC-004 — Cross-repo naming links

**Problem.** `cpptlm-d1-full/proposal.md:11` references `cpptlm-phase8b-injection-points` but not CppTLM P1 (`cpptlm-d1-pipeline-scoreboard`); our P1 proposal doesn't reference your `cpptlm-phase8b-injection-points` either. Asymmetric tracking complicates review handoffs.

**Ask.** Add the following section to **both** `cpptlm-d1-full/proposal.md` and `cpptlm-phase8b-injection-points/proposal.md` (before "Out of Scope" or equivalent):

```markdown
## Cross-Project Counterparts

| Repo | Change | Role |
|------|--------|------|
| CppTLM | `openspec/changes/cpptlm-f12b-ld-impl/` | P0 MemoryBridge + KernelLaunchTLM extension |
| CppTLM | `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/` | P1 Scoreboard/Pipeline/TensorCore + 4 Adapters |
| PTX-EMU | `openspec/changes/cpptlm-phase8b-injection-points/` | P1 injection interfaces + `exe_once()` hooks |
```

## Timeline

- **RFC-001/003/004**: decisions within **48h** so the CppTLM P0 PR can be sequenced without rework.
- **RFC-002 (Option A path)**: **1 week** if you choose the ABI bump.

## Next steps

Reply on this thread or in `cpptlm-d1-full/proposal.md` discussion. Once locked, we'll update CppTLM `tasks.md` with the resolved scope, land the matching P1 cross-link on our side, and ping you when CppTLM P0 is merge-ready (~3 days post-resolution). Open to a 30-min sync if any item needs real-time discussion — propose a slot.

---
Chi · CppTLM maintainer · 2026-07-16
Refs: CppTLM `7ab1ec1` (HSK-1/2/3 ack) · PTX-EMU `8dc000e` (ABI v1) · RFCs: `openspec/changes/cpptlm-f12b-ld-impl/specs/rfc-00{1,2,3,4}.md`

---

## 2. CppTLM 端同步动作（已落地 / 待办）

### 2.1 已完成（Day 1，commit `abd7dd5`）

| ID | 修复 | 文件 |
|----|------|------|
| C-1 | `KernelLaunchRequest` 结构体精简（删除冗余字段） | `design.md` |
| C-2 | 删除 `构造函数：保存 EventQueue 指针 + 创建 MemoryBridge 实例` 行 | `design.md` |
| C-3 | `synchronize_stream` 迭代器 UB 修复（snapshot-then-iterate） | `internal-plan.md` |
| C-4 | 移除 `<cuda_runtime.h>` 硬依赖（`kCudaSuccess`/`kCudaErrorInvalidValue` 常量替换） | `internal-plan.md` |
| C-5 | 统一 HSK-4 = `ptx_arg_sizes[]`、HSK-5 = `advance()`（推迟） | `tasks.md` + `design.md` |

### 2.2 待 PTX-EMU 回复后执行（Day 4）

| 触发条件 | 动作 |
|----------|------|
| RFC-001 确认 A | 更新 `cpptlm-f12b-ld-impl/spec.md` HSK-1-follow-up 引用；PTX-EMU 端 `hsk-1.md:83` 修订 |
| RFC-002 确认 B | PTX-EMU 端 `cudart_sim.cpp` `cudaStreamSynchronize` 改为 delegate；CppTLM 端无变更 |
| RFC-002 确认 A | bump `CPPTLMBRIDGE_VERSION` 到 2；重发 HSK-1 |
| RFC-003 确认 A | PTX-EMU 端 `PendingKernel` 加 `copied_args`；CppTLM 端 `spec.md` 加 NOTE |
| RFC-004 确认 A | 在 3 个 change 的 `proposal.md` 添加 Cross-Project Counterparts 表 |

### 2.3 独立推进（Day 3，不阻塞 P0）

- P1 `spec.md` 骨架创建（Metis Round 4 Deliverable 3）
- P1 `.openspec.yaml` + `internal-plan.md` 补齐
- 不需要 PTX-EMU 回复即可进行

---

## 3. 失败模式 / Escalation

### 3.1 24h 内未确认收到

- 升级渠道：email + Slack 双重提醒
- 默认行为：按本文件中的"推荐选项"推进，并在 commit message 中标注 "Pending PTX-EMU confirmation"

### 3.2 48h 内 RFC-001/003/004 未回复

- CppTLM 端可独立推进 P0 实施（这些是 PTX-EMU 端修订，不阻塞 CppTLM 编码）
- 在 PR 中标注 "PTX-EMU side review pending"

### 3.3 1 周内 RFC-002 未回复

- 默认采用 Option B（推荐，无 ABI bump）
- 在 PTX-EMU 端 issue 留 ping，48h 内若无响应则视为同意

### 3.4 PTX-EMU 选 RFC-002 Option A（ABI bump）

- 立即暂停 P0 实施
- 重新触发 HSK-1（版本号变更需重新握手）
- 评估对 CppTLM 端 `MemoryBridge::synchronize_stream` 删除的影响

---

## 4. 元数据

- **生成**: Sisyphus-Junior (`writing` category)，session `ses_097a1d037ffep2VLrfzhSPEK1v`
- **审核**: Metis Round 4 Deliverable 2（RFC-001 ~ RFC-004 全套）
- **关联审计**: Metis session `ses_097b54540ffeOJQtALUFt4ff10`
- **下游消费**: Day 4 reconcile 行动以此文件状态表为准