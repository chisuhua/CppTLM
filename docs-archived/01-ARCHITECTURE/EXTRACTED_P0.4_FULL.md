# P0.4 完整讨论内容

> 从会话文件中提取
> 消息数: 3, 总字符: 8914

---

## 消息 1

HEARTBEAT_OK

---

收到，继续 **P0.4: 时间归一化策略**。

---

## P0.4: 时间归一化策略

### 1. 提案设计回顾（§4.2.2）

提案提出**全局虚拟时间 (GVT)** 概念：

```
问题:
  TLM 事件 = 毫秒级（事务完成时间）
  RTL 周期 = 纳秒级（时钟周期）
  
解决方案:
  全局虚拟时间 (GVT) 统一表示
  - 以皮秒 (ps) 为单位
  - TLM 事件在开始/完成时打时间戳
  - RTL 周期通过时钟频率映射转换为 GVT
  - 按 transaction_id 匹配 TLM 与 RTL 事件
```

---

### 2. 现有代码分析



---

## 消息 2

### 3. 现有时间系统分析

**当前设计**:
```cpp
class EventQueue {
    uint64_t cur_cycle = 0;  // 全局时间，单位：周期
    
    void schedule(Event* ev, uint64_t delay) {
        ev->fire_time = cur_cycle + delay;
    }
};
```

**特征**:
- ✅ 简单：单一 `cur_cycle` 计数器
- ✅ 单位：周期（cycle）
- ⚠️ 无时钟频率概念（假设所有模块同频）

---

### 4. 时间归一化方案设计

基于现有代码 + 提案，我建议**简化版 GVT**：

#### 4.1 核心设计

```
┌─────────────────────────────────────────────────────────────┐
│  全局虚拟时间 (GVT)                                          │
│  单位：周期 (cycle)                                          │
│  分辨率：1 cycle = 可配置的时间单位（默认 1ns）                 │
├─────────────────────────────────────────────────────────────┤
│  TLM 事件时间戳                                              │
│  create_timestamp = EventQueue::getCurrentCycle()           │
│  complete_timestamp = EventQueue::getCurrentCycle()         │
├─────────────────────────────────────────────────────────────┤
│  RTL 周期映射                                                │
│  RTL_cycle → GVT = RTL_cycle × (RTL_freq / GVT_base_freq)   │
│  简化：假设 RTL 与 GVT 同频（1:1 映射）                       │
└─────────────────────────────────────────────────────────────┘
```

---

#### 4.2 时间单位配置

```cpp
// include/core/time_config.hh
#ifndef TIME_CONFIG_HH
#define TIME_CONFIG_HH

namespace gemsc {

// 全局时间配置
struct TimeConfig {
    // GVT 基准频率（MHz）
    // 默认 1000 MHz = 1ns/cycle
    static constexpr uint64_t BASE_FREQ_MHZ = 1000;
    
    // GVT 时间单位（ps）
    // 1000 MHz = 1ns = 1000ps
    static constexpr uint64_t TIME_UNIT_PS = 1000000 / BASE_FREQ_MHZ;
    
    // 将周期转换为皮秒
    static uint64_t cycles_to_ps(uint64_t cycles) {
        return cycles * TIME_UNIT_PS;
    }
    
    // 将皮秒转换为周期
    static uint64_t ps_to_cycles(uint64_t ps) {
        return ps / TIME_UNIT_PS;
    }
};

// 模块时钟配置
struct ClockConfig {
    std::string name;
    uint64_t freq_mhz;           // 模块频率（MHz）
    uint64_t cycles_per_tick;    // 每次 tick 的周期数
    
    // 将模块周期转换为 GVT 周期
    uint64_t to_gvt_cycles(uint64_t module_cycles) const {
        // 简化：假设同频
        return module_cycles;
        
        // 完整实现：
        // return module_cycles * freq_mhz / TimeConfig::BASE_FREQ_MHZ;
    }
};

} // namespace gemsc

#endif // TIME_CONFIG_HH
```

---

#### 4.3 TransactionContext 时间戳

```cpp
// include/core/transaction_context.hh 增强
struct TransactionContext {
    // === 时间戳（GVT 周期）===
    uint64_t create_timestamp = 0;    // 创建时间
    uint64_t complete_timestamp = 0;  // 完成时间
    
    // === 工具函数 ===
    uint64_t get_end_to_end_latency_cycles() const {
        return complete_timestamp - create_timestamp;
    }
    
    uint64_t get_end_to_end_latency_ps() const {
        return gemsc::TimeConfig::cycles_to_ps(
            get_end_to_end_latency_cycles()
        );
    }
    
    // 格式化输出（便于调试）
    std::string latency_string() const {
        uint64_t cycles = get_end_to_end_latency_cycles();
        uint64_t ps = get_end_to_end_latency_ps();
        
        if (ps >= 1000000) {
            return std::to_string(ps / 1000000) + " ms (" + 
                   std::to_string(cycles) + " cycles)";
        } else if (ps >= 1000) {
            return std::to_string(ps / 1000) + " us (" + 
                   std::to_string(cycles) + " cycles)";
        } else {
            return std::to_string(ps) + " ns (" + 
                   std::to_string(cycles) + " cycles)";
        }
    }
};
```

---

#### 4.4 TLM/RTL 事件匹配

```cpp
// include/core/time_normalizer.hh
#ifndef TIME_NORMALIZER_HH
#define TIME_NORMALIZER_HH

#include "transaction_context.hh"
#include <unordered_map>

namespace gemsc {

// TLM/RTL 事件匹配器
class EventMatcher {
private:
    // 按 transaction_id 存储 TLM 和 RTL 事件
    struct MatchedEvents {
        uint64_t tlm_create_cycle = 0;
        uint64_t tlm_complete_cycle = 0;
        uint64_t rtl_create_cycle = 0;
        uint64_t rtl_complete_cycle = 0;
        bool tlm_complete = false;
        bool rtl_complete = false;
    };
    
    std::unordered_map<uint64_t, MatchedEvents> pending_matches;
    
public:
    // 记录 TLM 事件
    void record_tlm_event(uint64_t transaction_id, 
                          uint64_t create_cycle,
                          uint64_t complete_cycle) {
        auto& match = pending_matches[transaction_id];
        match.tlm_create_cycle = create_cycle;
        match.tlm_complete_cycle = complete_cycle;
        match.tlm_complete = true;
        
        // 检查是否可以匹配
        try_match(transaction_id);
    }
    
    // 记录 RTL 事件
    void record_rtl_event(uint64_t transaction_id,
                          uint64_t create_cycle,
                          uint64_t complete_cycle) {
        auto& match = pending_matches[transaction_id];
        match.rtl_create_cycle = create_cycle;
        match.rtl_complete_cycle = complete_cycle;
        match.rtl_complete = true;
        
        // 检查是否可以匹配
        try_match(transaction_id);
    }
    
    // 尝试匹配并生成对比报告
    void try_match(uint64_t transaction_id) {
        auto it = pending_matches.find(transaction_id);
        if (it == pending_matches.end()) return;
        
        auto& match = it->second;
        if (match.tlm_complete && match.rtl_complete) {
            uint64_t tlm_latency = match.tlm_complete_cycle - match.tlm_create_cycle;
            uint64_t rtl_latency = match.rtl_complete_cycle - match.rtl_create_cycle;
            
            // 生成对比报告
            report_match(transaction_id, tlm_latency, rtl_latency);
            
            // 清理
            pending_matches.erase(it);
        }
    }
    
private:
    void report_match(uint64_t tid, uint64_t tlm_lat, uint64_t rtl_lat) {
        int64_t diff = (int64_t)rtl_lat - (int64_t)tlm_lat;
        double diff_pct = (tlm_lat > 0) ? (100.0 * diff / tlm_lat) : 0;
        
        DPRINTF(TIME, "[MATCH] TID=%lu TLM=%lu RTL=%lu diff=%ld (%.1f%%)\n",
                tid, tlm_lat, rtl_lat, diff, diff_pct);
    }
};

} // namespace gemsc

#endif // TIME_NORMALIZER_HH
```

---

### 5. 简化 vs 完整方案对比

| 维度 | 简化方案（推荐） | 完整方案（提案） |
|------|----------------|-----------------|
| **时间单位** | 周期（cycle） | 皮秒（ps） |
| **时钟频率** | 假设同频（1:1） | 支持多频率 |
| **实现复杂度** | 低 | 高 |
| **精度** | 周期级 | 皮秒级 |
| **适用场景** | Phase 1-3 | Phase 4+ |
| **升级路径** | 可扩展到完整方案 | - |

---

### 6. 推荐实施策略

**Phase 1-2**: 简化方案
```cpp
// 所有时间戳使用 GVT 周期
ctx.create_timestamp = event_queue->getCurrentCycle();
ctx.complete_timestamp = event_queue->getCurrentCycle() + latency;
```

**Phase 3+**: 扩展到完整方案（如需要）
```cpp
// 添加时钟频率支持
class ClockDomain {
    uint64_t freq_mhz;
    uint64_t to_gvt_cycles(uint64_t local_cycles);
};
```

---

### 7. 验收标准

| 标准 | 验证方法 |
|------|---------|
| **时间戳一致性** | 所有 TransactionContext 使用 GVT 周期 |
| **端到端延迟可计算** | complete_timestamp - create_timestamp 正确 |
| **TLM/RTL 事件匹配** | 按 transaction_id 正确匹配 |
| **延迟对比报告** | 生成 TLM vs RTL 延迟差异报告 |

---

### 8. 与提案的一致性

| 提案 §4.2.2 | 当前设计 | 状态 |
|------------|---------|------|
| GVT 统一时间 | ✅ 周期级 GVT | 简化版 |
| TLM 事件时间戳 | ✅ create/complete_timestamp | 完全一致 |
| RTL 周期映射 | ✅ 1:1 映射（简化） | 简化版 |
| transaction_id 匹配 | ✅ EventMatcher | 完全一致 |

---

## P0.4 决策请求

**确认事项**:
- [ ] 同意简化版 GVT（周期为单位，假设同频）
- [ ] 同意 Phase 1-2 使用简化方案
- [ ] 同意保留扩展到完整方案的路径
- [ ] 同意 EventMatcher 设计

**老板，P0.4 是否确认？** 

---

## P0 核心架构决策汇总

| 议题 | 决策 | 状态 |
|------|------|------|
| **P0.1 TransactionContext 设计** | TLM Extension 存储 | ✅ 已确认 |
| **P0.2 传播规则分类** | 透传型/转换型/终止型 | ✅ 已确认 |
| **P0.3 双并行实现模式** | tlm/rtl/compare/shadow | ✅ 已确认 |
| **P0.4 时间归一化策略** | 简化版 GVT（周期级） | ⏳ 待确认 |

**P0 完成后，我们将进入 P1 接口与集成决策。**

---

## 消息 3

## 当前会话记忆/上下文

### 会话目标
**架构讨论（方案 A：一次性完整讨论）** — 确认 CppTLM 混合建模架构的所有关键决策点，然后通过 ACF-Workflow 生成正式文档。

---

### 已完成工作

| 任务 | 状态 |
|------|------|
| 项目代码/文档梳理 | ✅ 完成 |
| 文档重组（新结构） | ✅ 完成 |
| CppHDL 符号链接创建 | ✅ 完成 |
| 架构讨论输入文档 | ✅ 完成 |

---

### P0 核心架构决策进展

| 议题 | 决策 | 状态 |
|------|------|------|
| **P0.1 TransactionContext 设计** | TLM Extension 存储（与现有 CoherenceExtension 等一致） | ✅ 已确认 |
| **P0.2 传播规则分类** | 透传型/转换型/终止型 + SimObject 辅助函数 | ✅ 已确认 |
| **P0.3 双并行实现模式** | tlm/rtl/compare/shadow 四种 impl_type | ✅ 已确认 |
| **P0.4 时间归一化策略** | 简化版 GVT（周期为单位，假设同频） | ⏳ 待确认 |

---

### 待讨论议题

| 优先级 | 议题 | 预计时间 |
|--------|------|---------|
| **P1** | Port<T> 兼容策略 | 10 min |
| **P1** | CppHDL 集成方案 | 5 min |
| **P1** | TLMToStreamAdapter 状态机 | 10 min |
| **P1** | 协议适配器范围 | 5 min |
| **P2** | 置信度评分机制 | 5 min |
| **P2** | 回归测试框架 | 10 min |
| **P2** | 验收标准确认 | 10 min |
| **P3** | Phase 划分与时间估计 | 15 min |
| **P3** | 风险缓解措施 | 10 min |

---

### 下一步

1. 确认 **P0.4 时间归一化策略**
2. 继续 **P1 接口与集成决策**（4 个议题）
3. 继续 **P2 质量与验证决策**（3 个议题）
4. 继续 **P3 实施计划确认**（3 个议题）
5. 通过 ACF-Workflow 生成正式架构文档 + 实施计划

---

**老板，P0.4 是否确认？** 确认后继续 P1 议题。

---

