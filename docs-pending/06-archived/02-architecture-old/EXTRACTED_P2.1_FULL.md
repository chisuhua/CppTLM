# P2.1 完整讨论内容

> 从会话文件中提取
> 消息数: 1, 总字符: 2739

---

## 消息 1

收到，设计已确认。

---

## P0/P1 完整汇总（最终版）

### P0: 核心架构决策 ✅

| 议题 | 决策 |
|------|------|
| **P0.1 TransactionContext** | TLM Extension 存储 |
| **P0.2 传播规则** | 透传型/转换型/终止型 |
| **P0.3 双并行实现** | tlm/rtl/compare/shadow |
| **P0.4 时间归一化** | 简化版 GVT（周期级） |

### P1: 接口与集成决策 ✅

| 议题 | 决策 |
|------|------|
| **P1.1 Port<T>** | 泛型模板 + PacketPort 包装 |
| **P1.2 CppHDL 集成** | 符号链接 → Submodule |
| **P1.3 适配器库** | Bridge 层 + Mapper 层，TLMBundle 嵌入 tid |
| **P1.4 协议范围** | 首期 AXI4 + Native，后续 CHI/TileLink |

---

## 继续 P2: 质量与验证决策

### P2.1: 置信度评分机制

#### 提案设计回顾（§6.2.4）

| 置信度 | 来源 | 示例 |
|--------|------|------|
| **HIGH** | 校准的 RTL 数据或历史测量 | Cache 命中延迟来自 RTL 仿真 |
| **MEDIUM** | 理论计算或已验证假设 | 内存延迟 = SRAM 访问时间 + 译码器延迟 |
| **LOW** | 默认估算或手动覆盖 | 使用文档默认值 |

#### 设计建议

```cpp
// include/core/confidence.hh
namespace gemsc {

enum class ConfidenceLevel {
    HIGH = 0,    // 基于校准数据
    MEDIUM = 1,  // 基于理论计算
    LOW = 2      // 基于默认估算
};

struct ConfidenceReport {
    ConfidenceLevel level;
    std::string source;       // 数据来源描述
    uint64_t calibration_count; // 校准次数（HIGH 时）
};

} // namespace gemsc
```

**决策请求**:
- [ ] 同意三级置信度（HIGH/MEDIUM/LOW）
- [ ] 同意 Phase 1 实现基础框架
- [ ] 同意置信度随 TransactionContext 传播

**P2.1 是否确认？**

---

### P2.2: 回归测试框架

#### 提案设计回顾（§7.2.3）

| 回归类型 | 测试内容 | 验收标准 |
|---------|---------|---------|
| **功能回归** | 新 RTL 模块通过契约测试 | 功能等价性验证 |
| **性能回归** | RTL 延迟/吞吐量 vs TLM 基线 | 偏差 < 20% |
| **混合系统回归** | RTL + TLM 混合仿真 | 系统级正确性 |
| **内存安全** | AddressSanitizer 运行 | 无泄漏/溢出 |

#### 设计建议

```cpp
// test/regression/contract_test.cc
TEST_CASE("Cache RTL Contract Test") {
    // 1. 功能等价性
    CHECK(rtl_response.data == tlm_response.data);
    
    // 2. 延迟偏差
    double diff = abs(rtl_latency - tlm_latency) / tlm_latency;
    CHECK(diff < 0.20);  // < 20%
    
    // 3. 背压处理
    CHECK(rtl_handles_backpressure == true);
}
```

**决策请求**:
- [ ] 同意四类回归测试
- [ ] 同意性能偏差阈值 20%
- [ ] 同意契约测试框架

**P2.2 是否确认？**

---

### P2.3: 验收标准确认

#### 按 Phase 分级验收

| Phase | 关键交付物 | 验收标准 |
|-------|-----------|---------|
| **Phase 1** | TransactionContext + Port<T> | 100+ 交易完整传播，单元测试 >90% |
| **Phase 2** | StreamBridge + FragmentMapper | TLM→RTL 数据流零损坏，背压正确传播 |
| **Phase 3** | AXI4Mapper + 双并行实现 | compare 模式功能等价，时间归一化对比报告 |
| **Phase 4** | CHI/TileLinkMapper | 多协议拓扑验证通过 |
| **Phase 5** | 产品化 | 回归测试全通过，5 个完整示例 |

**决策请求**:
- [ ] 同意按 Phase 分级验收
- [ ] 同意上述验收标准

**P2.3 是否确认？**

---

## P2 汇总

| 议题 | 决策 | 状态 |
|------|------|------|
| **P2.1 置信度评分** | HIGH/MEDIUM/LOW 三级 | ⏳ 待确认 |
| **P2.2 回归测试** | 功能/性能/混合/内存安全 | ⏳ 待确认 |
| **P2.3 验收标准** | 按 Phase 分级 | ⏳ 待确认 |

**请确认 P2 所有议题。**

---

## P3: 实施计划确认（预览）

| 议题 | 预计时间 |
|------|---------|
| **P3.1 Phase 划分** | 5 个 Phase，20 周 |
| **P3.2 风险缓解** | 6 个风险 + 缓解措施 |
| **P3.3 文档重组** | 已完成 ✅ |

**P2 确认后继续 P3。**

---

