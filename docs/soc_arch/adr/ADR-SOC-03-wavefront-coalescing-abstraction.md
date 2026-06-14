# ADR-SOC-03: Wavefront Coalescing 抽象（Wavefront Coalescing Abstraction）

> **状态**: ✅ 已确认
> **日期**: 2026-06-14
> **影响**: ComputeUnitTLM 访存合并行为建模
> **类别**: SoC 架构 / GPU 内存模型

---

## 1. 背景

GPU 编程模型中，wavefront/warp 内多个线程（典型 32 或 64 个）的访存可被硬件合并（memory coalescing）以提升带宽利用率。需要决定是否在 CppTLM 中精确模拟该行为：

| 选项 | 描述 | 工作量 |
|------|------|--------|
| **抽象** | 用 `coalescing_factor` 参数代替精确模拟（如 `4` 表示 4 个独立请求在 CU 层面合并为 1 个大请求） | Low |
| 精确 | 模拟同一 warp 的 64 个线程的访存合并（地址合并判断、cache line 边界处理） | Very High |

---

## 2. 决策

✅ **抽象掉 wavefront 内部行为**。CppTLM 作为 TLM 框架，**关注 NoC/Cache/Memory 层级的行为，不是 GPU ISA 行为**。

具体做法：
- ComputeUnitTLM 维护一个 `coalescing_factor` 参数（默认 1，即不合并）
- 当需要模拟合并行为时，ComputeUnitTLM 在 `tick()` 中按 `coalescing_factor` 数量发出"逻辑独立"请求，下游模块（Crossbar/Cache）只看到 `1` 个大请求
- 状态机（参见 [ADR-SOC-01](./ADR-SOC-01-coherence-protocol-strategy.md)）和 NoC 路由不受 wavefront 细节影响

---

## 3. 实施

```cpp
class ComputeUnitTLM {
    // ... 现有字段
    uint32_t coalescing_factor_ = 1;  // 默认不合并

    void emit_coalesced_req(ComputeReqBundle& req) {
        // 应用 coalescing_factor，逻辑上 N 个线程 = 1 个大请求
        req.byte_count = base_byte_count_ * coalescing_factor_;
        // 发出到下游端口
        req_out_->write(req);
    }
};
```

---

## 4. 不做（明确范围外）

- ❌ 线程级（per-thread）地址计算
- ❌ Warp scheduler 状态机
- ❌ Cache line 边界检测
- ❌ 寄存器依赖性追踪

---

## 5. 参考文献

- 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §3 D3（L626-633）
- 复述: [`roadmap.md`](../../../roadmap.md) L65 D3 行
- 微架构: [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md)
- 关联: [ADR-SOC-02 CU 粒度](./ADR-SOC-02-cu-granularity.md)（CU 黑盒与 wavefront 抽象是组合策略）
