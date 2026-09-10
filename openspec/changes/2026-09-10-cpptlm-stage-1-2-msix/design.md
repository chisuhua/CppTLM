# Design: cpptlm-stage-1-2-msix — Oracle O5 修订 2026-09-10

> **关联**: [proposal.md](../proposal.md) + [tasks.md](../tasks.md) + [specs/cpptlm-stage-1-2-msix/spec.md](../specs/cpptlm-stage-1-2-msix/spec.md)
>
> **⚠️ Oracle 修订**：原 design 误认为 `PcieEndpointTLM::trigger_irq_async` 是 stub，但该函数实际**已存在**（`dgpu_board_shell.cc:396-410`，detached std::thread 调 `irq_cb_`）。真正的修复 #4 断链点是 **`msix_update_pending`（`cpptlm_emulator.cc:241-247`）从不调 `board->trigger_irq_async`**。本 change 修复点已重写。

## 设计概述

修复 #4 中断链断裂（在 `msix_update_pending` 接线 `board->trigger_irq_async(vector)`）。设计原则：
1. **TDD 5 步**：先写失败测试，再实施
2. **路径独立**：修复点仅在 cpptlm_emulator.cc 一处
3. **架构约束**：不改 23 ABI + 5 ports
4. **payload 不在 23 ABI 范围内**（头文件 L96 `cpptlm_emulator_msix_update_pending(emu, vector)` 无 payload 参数；intr_cb typedef L56 为 `(user_ctx, vector, trans_id)`）

## 当前代码（断链点）

```cpp
// src/abi/cpptlm_emulator.cc:241-247 (msix_update_pending)
int cpptlm_emulator_msix_update_pending(cpptlm_emulator_t* emu, uint32_t vector) {
    if (vector >= emu->msix_table_size) return -EINVAL;
    std::atomic_fetch_add(&emu->msix_pending[vector], 1);  // ← 只 set pending bit
    return 0;  // ← 从未触发 trigger_irq_async → intr_cb 永不被调
}
// DGpuBoard::trigger_irq_async(vector) 已实现（dgpu_board_shell.cc:396-410），但未被调用
```

## 目标代码（真实接线）

```cpp
// src/abi/cpptlm_emulator.cc:241-247 (msix_update_pending)
int cpptlm_emulator_msix_update_pending(cpptlm_emulator_t* emu, uint32_t vector) {
    if (vector >= emu->msix_table_size) return -EINVAL;
    std::atomic_fetch_add(&emu->msix_pending[vector], 1);
    // ← 新增：触发 DGpuBoard::trigger_irq_async（已存在，无需重写）
    if (emu->board) {
        emu->board->trigger_irq_async(vector);  // detached std::thread → irq_cb_(vector)
    }
    return 0;
}
// msix_init 路径补全：msix_init 后确保 board->register_irq_callback(intr_cb) 已绑定
```

## 关键变更

- **新增接线点**：`msix_update_pending` 末尾调 `board->trigger_irq_async(vector)`（detached thread）
- **msix_init 路径补全**：确保 `register_callbacks` 已设置 `intr_cb_`（per `cpptlm_emulator_register_callbacks` L103-106，4 cb 捆绑）
- **vector 由 driver 指定**：参数化 vector（per design §9.1）
- **payload 不可用**：23 ABI 冻结面无 payload 参数；spec Scenario 断言 intr_cb 的 trans_id 而非 payload
- **线程模型**：detached std::thread（非 inject_q_/sim_loop），需注意测试稳定性（entry §9 retry 1 + CI 容忍度）

## 实施顺序

TDD 5 步（1 commit）：
1. 写 `test_dgpu_msix.cc::test_msix_intr_cb_called_within_200ms` 失败测试
2. 验证失败（当前 msix_update_pending 只 set pending，从不调 trigger_irq_async，cb_called = 0）
3. 实施 `msix_update_pending` 末尾添加 `board->trigger_irq_async(vector)` 调用
4. 验证通过（200ms 内 intr_cb ≥1 次触发，captured_vector == 测试 vector）
5. commit

## 风险评估

| 风险 | 缓解 |
|------|------|
| detached thread 完成时机不可控 | entry §9 retry 1 + CI 容忍度放大（200ms → 2s 标称） |
| `board` 指针为空 | `msix_update_pending` null check |
| 多次 `msix_update_pending` 同 vector 触发多次 thread | 由 `DGpuBoard::trigger_irq_async` 内部 mutex 保护 |
| 测试 flaky（CI 慢机器） | 200ms 是验证上限，非性能断言；retry 1 缓解 |

## 不在设计范围

- MSI-X Capability + 向量表扩展（父 change `cpptlm-pcie-ep-foundation` 任务 1.2.2 残余，P1）
- 中断节流（父 change 任务 1.2.3 残余，P1）
- 阶段 1.3a SDMA Ring Buffer（独立 change `stage-1-3-sdma`）
