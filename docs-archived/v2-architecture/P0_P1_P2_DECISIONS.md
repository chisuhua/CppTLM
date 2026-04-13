# P0/P1/P2 架构决策汇总

> **文档日期**: 2026-04-07  
> **会话**: CppTLM 架构讨论  
> **状态**: ✅ 已全部确认

---

## 完整决策汇总

### P0: 核心架构决策 ✅

| 议题 | 决策 | 依据 |
|------|------|------|
| **P0.1 TransactionContext 设计** | TLM Extension 存储 | 项目重度使用 SystemC TLM extension 机制 |
| **P0.2 传播规则分类** | 透传型/转换型/终止型 | 三种模块行为定义 |
| **P0.3 双并行实现模式** | tlm/rtl/compare/shadow 四种 impl_type | 支持渐进式验证 |
| **P0.4 时间归一化策略** | 简化版 GVT（周期级） | 初期实现简化，避免大数运算 |

---

### P1: 接口与集成决策 ✅

| 议题 | 决策 | 依据 |
|------|------|------|
| **P1.1 Port\<T\> 兼容策略** | 泛型模板 + PacketPort 包装 | 并行共存，现有代码零修改 |
| **P1.2 CppHDL 集成方案** | 开发期符号链接 → 生产期 git submodule | 平衡开发效率与版本控制 |
| **P1.3 适配器库设计** | Bridge 层 + Mapper 层，TLMBundle 嵌入 tid | 分层命名架构 |
| **P1.4 协议适配器范围** | 首期 AXI4 + Native，后续 CHI/TileLink | 优先级排序 |

---

### P2: 质量与验证决策 ✅

| 议题 | 决策 | 依据 |
|------|------|------|
| **P2.1 置信度评分机制** | HIGH/MEDIUM/LOW 三级 | 性能分析可信度保障 |
| **P2.2 回归测试框架** | 功能回归 + 性能回归 + 混合系统回归 + 内存安全 | 四类回归测试 |
| **P2.3 验收标准** | 按 Phase 分级验收 | 提案 §9.6 |

---

## P0 详细决策

### P0.1: TransactionContext 设计

**问题**: 项目重度使用 SystemC TLM extension 机制，TransactionContext 应如何集成？

**方案对比**:

| 方案 | 描述 | 评估 |
|------|------|------|
| **A** | 嵌入 Packet（值语义） | ❌ 与现有机制分离 |
| **B** | 作为 TLM Extension | ✅ 与现有机制统一 |
| **C** | 混合方案 | ⚠️ 复杂度增加 |

**决策**: ✅ **方案 B — TLM Extension**

**实现**:

```cpp
class TransactionContextExtension : public tlm::tlm_extension<TransactionContextExtension> {
public:
    TransactionContext context;
    
    tlm_extension* clone() const override {
        return new TransactionContextExtension(*this);
    }
    
    void copy_from(const tlm_extension_base& ext) override {
        context = static_cast<const TransactionContextExtension&>(ext).context;
    }
};
```

**理由**:
- 与现有 extension 机制统一
- 生命周期自动管理
- 运行时灵活扩展

---

### P0.2: 传播规则分类

**三种模块行为**:

| 类型 | 行为 | 典型模块 |
|------|------|---------|
| **透传型** | 原样转发 Context，仅追加路由延迟到 trace_log | Crossbar、简单互连 |
| **转换型** | 创建子交易，分配 sub_transaction_id，通过 parent_id 关联 | Cache、地址转换单元 |
| **终止型** | 标记交易完成，记录最终延迟和状态 | PhysicalMemory、终端外设 |

---

### P0.3: 双并行实现模式

**impl_type 配置**:

| 配置 | 行为 | 用途 |
|------|------|------|
| `tlm` | 使用 TLM 微模块实现 | 快速仿真，近似时序 |
| `rtl` | 使用 RTL (CppHDL) 实现 | 周期精确，仿真较慢 |
| `compare` | 并行运行两种实现 | 功能等价性验证 |
| `shadow` | RTL 影子模式运行 | 记录 RTL 行为，不影响系统 |

---

### P0.4: 时间归一化策略

**全局虚拟时间 (GVT)**:
- 以**周期**为单位（简化实现）
- TLM 事件在 RTL 周期边界触发
- 按 transaction_id 匹配 TLM 与 RTL 事件

**注意**: 暂不实现频率映射和多时钟域，Phase 3 再考虑。

---

## P1 详细决策

### P1.1: Port\<T\> 兼容策略

**方案**: 并行共存

```
现有代码                    新代码
    │                          │
    ▼                          ▼
SimplePort ──包装──► PacketPort<T> ◄── Port<T> 模板
    │                          │
    └──────────────────────────┘
              现有代码零修改
```

**向后兼容**: `PacketPort<T>` 包装 `SimplePort`，现有代码无需修改。

---

### P1.2: CppHDL 集成方案

**短期（开发阶段）**: 符号链接

```bash
ln -s /workspace/CppHDL /workspace/CppTLM/external/CppHDL
```

**长期（生产阶段）**: Git Submodule

```bash
git submodule add /workspace/CppHDL external/CppHDL
```

---

### P1.3: 适配器库设计（Bridge + Mapper 分层）

**分层架构**:

```
┌─────────────────────────────────────────────────────────────┐
│  Bridge 层（协议无关）— TLM ↔ CppHDL Stream/Flow        │
│  职责：valid/ready 握手，FIFO 缓冲，不分片               │
├─────────────────────────────────────────────────────────────┤
│  Mapper 层（映射层）— Bundle ↔ Fragment/Protocol           │
│  职责：分片/重组，协议特定映射                           │
└─────────────────────────────────────────────────────────────┘
```

**适配器命名**:

| 适配器类型 | 名称 | 职责 |
|-----------|------|------|
| TLM ↔ ch_stream<Bundle> | **StreamBridge** | valid/ready + FIFO |
| TLM ↔ ch_flow<Bundle> | **FlowBridge** | 单向传输 |
| Bundle ↔ ch_fragment | **FragmentMapper** | 分片/重组 |
| Bundle ↔ AXI4 | **AXI4Mapper** | AXI4 协议映射 + 分片 |
| Bundle ↔ CHI | **CHIMapper** | CHI 协议映射 + 分片 |
| Bundle ↔ TileLink | **TileLinkMapper** | TileLink 协议映射 + 分片 |

**TLMBundle 设计**:

```cpp
struct TLMBundle {
    uint64_t transaction_id;  // 交易 ID
    uint64_t flow_id;        // 流 ID
    uint64_t address;       // 地址
    uint64_t length;        // 数据长度
    uint8_t  data[64];     // 数据负载
    uint8_t  strb;          // 字节使能
    bool     write;         // 写使能
    uint8_t  priority;      // 优先级
};
```

**Transaction ID 传递**: 每个 fragment 携带完整 TLMBundle（含 transaction_id）。

---

### P1.4: 协议适配器优先级

| 优先级 | 协议 | 说明 |
|--------|------|------|
| **首期** | AXI4 + Native | 覆盖主流场景 |
| **后续** | CHI | 缓存一致性协议 |
| **后续** | TileLink | 开源处理器常用 |

---

## P2 详细决策

### P2.1: 置信度评分机制

**三级评分**:

| 置信度 | 来源 | 示例 |
|--------|------|------|
| **HIGH** | 校准的 RTL 数据或历史测量 | Cache 命中延迟来自 RTL 仿真 |
| **MEDIUM** | 理论计算或已验证假设 | 内存延迟 = SRAM 访问时间 + 译码器延迟 |
| **LOW** | 默认估算或手动覆盖 | 使用文档默认值 |

**实现**:

```cpp
enum class ConfidenceLevel {
    HIGH = 0,    // 基于校准数据
    MEDIUM = 1,  // 基于理论计算
    LOW = 2      // 基于默认估算
};
```

---

### P2.2: 回归测试框架

**四类回归测试**:

| 类型 | 测试内容 | 验收标准 |
|------|---------|---------|
| **功能回归** | 新 RTL 模块通过契约测试 | 功能等价性验证 |
| **性能回归** | RTL 延迟/吞吐量 vs TLM 基线 | 偏差 < 20% |
| **混合系统回归** | RTL + TLM 混合仿真 | 系统级正确性 |
| **内存安全** | AddressSanitizer 运行 | 无泄漏/溢出 |

---

### P2.3: 验收标准（按 Phase 分级）

| Phase | 关键交付物 | 验收标准 |
|-------|-----------|---------|
| **Phase 1** | TransactionContext + Port\<T\> | 100+ 交易完整传播，单元测试 >90% |
| **Phase 2** | StreamBridge + FragmentMapper | TLM→RTL 数据流零损坏，背压正确传播 |
| **Phase 3** | AXI4Mapper + 双并行实现 | compare 模式功能等价，时间归一化对比报告 |
| **Phase 4** | CHI/TileLinkMapper | 多协议拓扑验证通过 |
| **Phase 5** | 产品化 | 回归测试全通过，5 个完整示例 |

---

## 完整适配器库结构

```
gemsc::adapter::
├── Bridge 层（协议无关）
│   ├── StreamBridge<T>          — TLM ↔ ch_stream<T>
│   ├── FlowBridge<T>            — TLM ↔ ch_flow<T>
│   ├── StreamToTLMBridge<T>     — ch_stream<T> → TLM
│   └── FlowToTLMBridge<T>       — ch_flow<T> → TLM
│
├── Mapper 层（分片/协议）
│   ├── FragmentMapper<T>        — Bundle ↔ ch_fragment<Bundle>
│   ├── AXI4Mapper<T>            — Bundle ↔ AXI4 信号
│   ├── CHIMapper<T>             — Bundle ↔ CHI 信号
│   └── TileLinkMapper<T>        — Bundle ↔ TileLink 信号
│
└── Bundle 类型
    ├── TLMBundle                — 嵌入 transaction_id
    └── TLMBundleFragment        — ch_fragment<TLMBundle>
```

---

## 风险登记册

| # | 风险 | 概率 | 影响 | 缓解措施 |
|---|------|------|------|---------|
| R1 | TransactionContext 性能开销 >5% | 中 | 中 | 基准测试验证，优化内存布局 |
| R2 | CppHDL 接口变更 | 中 | 高 | 锁定 commit hash，编写接口测试 |
| R3 | 命名空间冲突 | 低 | 中 | 明确命名空间封装，代码审查 |
| R4 | 时间归一化复杂度高 | 中 | 中 | Phase 1 简化方案，Phase 3 扩展 |
| R5 | 分片重组逻辑缺陷 | 中 | 高 | 契约测试 + 边界条件测试 |
| R6 | 仿真速度过慢 (<100 KIPS) | 中 | 中 | 非关键路径 TLM Bypass 模式 |

---

## 实施时间线

| Phase | 周数 | 主要交付物 |
|-------|------|-----------|
| Phase 0 | 1 周 | 环境准备、CppHDL 链接 |
| Phase 1 | 3 周 | TransactionContext + Port\<T\> |
| Phase 2 | 4 周 | StreamBridge + FragmentMapper + AXI4Mapper |
| Phase 3 | 4 周 | 双并行实现框架 |
| Phase 4 | 4 周 | CHI/TileLinkMapper |
| Phase 5 | 4 周 | 产品化 |
| **总计** | **20 周** | **800 小时** |

---

## 相关文档

| 文档 | 位置 |
|------|------|
| FragmentMapper 决议 | `FRAGMENT_MAPPER_DECISIONS.md` |
| 交易上下文设计 | `transaction-context.md` |
| 适配器 API 设计 | `adapter-design.md`（待创建） |
| 协议适配器规范 | `protocol-adapters.md`（待创建） |
| 历史文档 | `docs/05-LEGACY/` |

---

**状态**: ✅ P0/P1/P2 全部确认  
**后续**: 等待 P3 确认后生成正式 ADR 文档
