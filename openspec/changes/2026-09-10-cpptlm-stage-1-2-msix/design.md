# Design: cpptlm-stage-1-2-msix

> **关联**: [proposal.md](../proposal.md) + [tasks.md](../tasks.md) + [specs/cpptlm-stage-1-2-msix/spec.md](../specs/cpptlm-stage-1-2-msix/spec.md)

## 设计概述

修复 #4 中断链断裂（`trigger_irq_async` 真实接线）。设计原则：
1. **TDD 5 步**：先写失败测试，再实施
2. **路径独立**：trigger_irq_async 路径不依赖其他 ABI
3. **架构约束**：不改 23 ABI + 5 ports

## 当前代码（stub）

```cpp
// src/tlm/pcie/pcie_endpoint_ip.cc
int PcieEndpointTLM::trigger_irq_async(uint32_t vector, uint64_t payload) {
    (void)vector;
    (void)payload;
    return -ENOSYS;  // ← stub：导致 intr_cb 永远不被调用
}
```

## 目标代码（真实接线）

```cpp
int PcieEndpointTLM::trigger_irq_async(uint32_t vector, uint64_t payload) {
    if (vector >= msix_table_size_) {
        return -EINVAL;  // vector 越界
    }
    if (msix_pending_[vector]) {
        return -EAGAIN;  // 已有 pending，避免重入
    }
    // 1. 标记 pending
    msix_pending_[vector] = true;
    msix_payload_[vector] = payload;

    // 2. 构造 MSI-X TLP 推入 inject_q_
    PcieTlpBundle tlp;
    tlp.type = PcieTlpType::MSI_X;
    tlp.vector = vector;
    tlp.payload = payload;
    tlp.completion_cb = [this, vector]() {
        // 3. sim_loop drain 后调 intr_cb
        if (intr_cb_) {
            intr_cb_(vector, payload);
        }
        msix_pending_[vector] = false;
    };
    {
        std::lock_guard<std::mutex> lock(inject_mu_);
        inject_q_.push_back(std::move(tlp));
    }
    return 0;  // async fire-and-forget
}
```

## 关键变更

- **新增数据结构**：`msix_pending_[vector]` + `msix_payload_[vector]` 跟踪 pending 状态
- **msix_init 后立即可用**：`msix_table_size_` 在 init 时设置，trigger_irq_async 即可用
- **intr_cb 真实触发**：sim_loop drain 后通过 completion_cb 调用
- **vector 由 driver 指定**：参数化 vector（per design §9.1 `trigger_msix(vector)` 透传）

## 实施顺序

TDD 5 步（1 commit）：
1. 写 `test_dgpu_msix.cc::test_msix_intr_cb_called_within_200ms` 失败测试
2. 验证失败（当前 -ENOSYS）
3. 实施 trigger_irq_async 真实接线
4. 验证通过（200ms 内 intr_cb ≥1 次触发）
5. commit

## 风险评估

| 风险 | 缓解 |
|------|------|
| msix_table_size_ 未初始化 | msix_init 时强制设置；trigger_irq_async 越界返 -EINVAL |
| completion_cb 重入 | msix_pending_[vector] 标记避免 |
| intr_cb 线程安全 | completion_cb 在 sim_loop 单线程执行 |

## 不在设计范围

- MSI-X Capability + 向量表扩展（父 change `cpptlm-pcie-ep-foundation` 任务 1.2.2 残余，P1）
- 中断节流（父 change 任务 1.2.3 残余，P1）
- 阶段 1.3a SDMA Ring Buffer（独立 change `stage-1-3-sdma`）
