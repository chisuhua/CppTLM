# GemSc 混合建模 - 改进版快速参考卡（第2版）

> **版本**: v2.0（基于完整文档审视）  
> **日期**: 2026年1月28日  
> **变更**: 简化设计，强调融合而非替换

---

## 🎯 核心洞察（新发现）

```
❌ 错误思路：
   替换 SimplePort → 创建 Port<T>
   添加 Packet 字段 → transaction_id, hw_cycle
   新增生命周期 → hwTick(), syncTick()

✅ 正确思路：
   保护 SimplePort 不变
   使用 Extension 承载所有新信息
   在 pre_tick/tick 中实现适配器逻辑
```

---

## 📊 架构对比表

### 之前的方案（v1）vs 改进后的方案（v2）

| 维度 | v1（过度设计） | v2（融合设计） |
|------|----------|-----------|
| **Packet 修改** | ❌ 添加 3 个字段 | ✅ 零修改（用 Extension） |
| **Port 设计** | ❌ Port<T> 泛型模板 | ✅ 保留 SimplePort |
| **新生命周期** | ❌ hwTick/syncTick | ✅ 复用 pre_tick/tick |
| **核心组件** | Port<T>, FIFOPort<T>, Adapter | Extension, AdapterBase, 具体 Adapter |
| **实施复杂度** | 高（40+ 核心类） | 低（10+ 核心类） |
| **预期工作量** | 16 周 | 10-12 周 |
| **向后兼容性** | 需要 PacketPort 包装 | 100% 无缝 |

---

## 🏗️ 改进后的架构（v2）

```
现有 GemSc 架构（保护不变）
┌─────────────────────────────────────┐
│           SimObject 模块             │
│  (实现 pre_tick() 和 tick())        │
└────────────┬────────────────────────┘
             │
             ↓ 包含
┌─────────────────────────────────────┐
│      Packet 通用通信骨架            │
│  • src/dst_cycle  • stream_id       │
│  • seq_num, credits  • route_path   │
│  • hop_count, priority, flow_id, vc │
└────────────┬────────────────────────┘
             │
             ↓ 包含
┌─────────────────────────────────────┐
│  tlm_generic_payload + Extension    │
│                                     │
│  • 现有 Extension:                  │
│    - CoherenceExtension             │
│    - PerformanceExtension           │
│    - PrefetchExtension              │
│    - QoSExtension                   │
│    - ReadCmdExt/WriteDataExt        │
│                                     │
│  • ✨ 新增 Extension:               │
│    - HybridTimingExtension          │
│      (transaction_id, hw_cycles)    │
└─────────────────────────────────────┘
             △
             │ 转换
             │ (在 Adapter 中)
┌─────────────────────────────────────┐
│    CppHDL Bundle 硬件数据           │
│  • ch::stream<T>  • ch::flow<T>     │
│  • valid/ready 握手                 │
└─────────────────────────────────────┘
```

---

## 🔧 v2 核心组件清单

### 1️⃣ HybridTimingExtension（新增）

```cpp
struct HybridTimingExtension : public tlm::tlm_extension<HybridTimingExtension> {
    // 事务全局ID
    uint64_t transaction_id;
    
    // TLM 侧周期
    uint64_t tlm_injection_cycle;      // 何时进入适配器
    
    // HW 侧周期
    uint64_t hw_injection_cycle;       // 何时进入硬件
    uint64_t hw_completion_cycle;      // 何时完成
    std::vector<uint64_t> hw_beat_cycles; // 每 beat 的时间戳
    
    // 标准 Extension 方法
    tlm::tlm_extension_base* clone() const override { ... }
    void copy_from(tlm::tlm_extension_base const& e) override { ... }
};
```

**何时使用**: 需要追踪事务从 TLM 到 HW 的完整生命周期时

**与现有设计的关系**: 
- 可选（不强制所有事务都有）
- 扩展而非替代 PerformanceExtension

### 2️⃣ TLMToHWAdapterBase（新增）

```cpp
template <typename HWBundleT, typename ExtensionT = ReadCmdExt>
class TLMToHWAdapterBase : public SimObject {
protected:
    Port<Packet*>* m_tlm_in;
    Port<Packet*>* m_tlm_out;
    
    // 子类实现这两个转换方法
    virtual HWBundleT convert_ext_to_hw(const ExtensionT* ext) = 0;
    virtual ExtensionT* convert_hw_to_tlm(const HWBundleT& hw_data) = 0;
    
public:
    // 遵守用户契约：必须调用 port->tick()
    void pre_tick() override {
        // 处理 TLM 输入
        // 转换为 HW Bundle
        // 处理 HW 响应
        // 转换回 TLM
    }
    
    void tick() override {
        // 重试滞留的包
    }
};
```

**何时使用**: 创建任何 TLM ↔ HW 的双向适配器时

**与现有设计的关系**:
- 是标准的 SimObject 子类
- 通过 ModuleFactory 注册
- 在 JSON 中作为普通模块使用

### 3️⃣ 具体 Adapter 实现（新增）

```cpp
class ReadCmdAdapter : public TLMToHWAdapterBase<ch::stream<ReadCmd>, ReadCmdExt> {
protected:
    ch::stream<ReadCmd> convert_ext_to_hw(const ReadCmdExt* ext) override {
        // ReadCmdExt {addr, size, cache_type} 
        //   → ch::stream<ReadCmd> {address, length, ...}
        return ch::stream<ReadCmd>{ext->data.addr, ext->data.size, ...};
    }
    
    ReadCmdExt* convert_hw_to_tlm(const ch::stream<ReadCmd>& hw_data) override {
        // 反向转换
        return new ReadCmdExt(ReadCmd{hw_data.address, ...});
    }
};
```

---

## 📋 v2 实施路线图（改进版）

### Phase A: 基础框架（1-2 周）

```
Week 1:
  [ ] HybridTimingExtension 实现 + 测试
  [ ] TLMToHWAdapterBase 骨架实现
  [ ] 遵守用户契约的 pre_tick/tick 实现

Week 2:
  [ ] ReadCmdAdapter 完整实现
  [ ] 集成测试（ReadCmdExt → Bundle → 响应 → ReadCmdExt）
  [ ] 验收：所有现有模块无修改，新旧混合工作
```

**交付物**:
- HybridTimingExtension class
- TLMToHWAdapterBase template
- ReadCmdAdapter reference implementation
- 单元测试 (> 90%)
- 集成测试 (端到端流程)

**验收标准**:
- ✅ Extension clone/copy 正确
- ✅ 双向转换无损
- ✅ 性能开销 < 5%
- ✅ 现有模块零修改

---

### Phase B: 其他 Adapter（3 周）

```
Week 3-4: WriteDataExt Adapter
  [ ] 数据打包/拆包
  [ ] 字使能处理
  [ ] 大数据 (>256B) 处理

Week 5: CoherenceExt Adapter
  [ ] 一致性状态转换
  [ ] Snoop 处理
  [ ] Sharer mask 处理

Week 6: 集成与测试
  [ ] 多 Adapter 共存
  [ ] 混合仿真完整测试
```

**交付物**:
- WriteDataAdapter class
- CoherenceAdapter class
- 综合集成测试
- 示例配置文件

---

### Phase C: 高级特性（2-3 周）

```
Week 7: 时空映射精化
  [ ] HW 周期精确计算
  [ ] Beat 展开机制
  [ ] 延迟注入

Week 8: 事务追踪
  [ ] transaction_id 端到端
  [ ] VCD 导出支持
  [ ] 性能分析工具

Week 9 (可选): MemoryProxy
  [ ] 零拷贝大数据传输
  [ ] 性能优化验证
```

---

## 🔄 关键设计决策

### 为什么不用 Port<T> 泛型？

```
❌ Port<T> 方案：
   - 需要重新设计整个端口系统
   - 需要 SimplePort 包装器
   - 破坏现有的 PortManager 逻辑
   - 学习曲线陡峭

✅ Adapter 方案：
   - SimplePort 完全不动
   - Adapter 是普通的 SimObject
   - 在 pre_tick 中进行转换
   - 遵循现有设计模式
```

### 为什么在 Extension 中存储 hw_cycle？

```
❌ Packet 字段方案：
   - 修改 Packet 核心结构
   - 所有事务都增加 8 字节
   - 不符合"按需扩展"的哲学

✅ Extension 方案：
   - HybridTimingExtension 可选
   - 只有混合仿真的事务才有
   - 符合 GemSc "轻量级" 设计
   - 易于后续演进
```

### 为什么不用新的生命周期钩子？

```
❌ 新钩子方案：
   - initiate → pre_tick → tick → hwTick → syncTick
   - 复杂的生命周期管理
   - 需要修改 EventQueue
   - 与现有教学资料冲突

✅ 现有钩子方案：
   - 在 pre_tick 的迭代机制中处理 HW
   - 在 tick 中重试滞留的包
   - GemSc 设计已经支持
   - 代码更简洁
```

---

## 🎓 用户契约（关键）

适配器必须遵守的三条规则：

### 规则 1: pre_tick() 中必须调用 port->tick()

```cpp
void TLMToHWAdapterBase::pre_tick() override {
    // ✅ 正确
    auto* port = getPortManager().getUpstreamPort("tlm_in");
    if (port) port->tick();  // ← 关键！
    
    // 处理端口中的数据包
    Packet* pkt = nullptr;
    if (/* ... 接收 pkt ... */) {
        // 转换
    }
}
```

### 规则 2: tick() 中应该调用 output_port->tick()

```cpp
void TLMToHWAdapterBase::tick() override {
    // ✅ 推荐
    auto* out_port = getPortManager().getDownstreamPort("tlm_out");
    if (out_port) {
        out_port->tick();  // ← 重试滞留的包
    }
}
```

### 规则 3: 零延迟连接会导致迭代执行

```
如果 Adapter → HW Module 的连接延迟为 0：
  CPUSim.tick() 
    → send(req) 
      → Adapter.pre_tick() 被触发（迭代）
        → send(hw_bundle) 
          → HWModule.pre_tick() 被触发
            → 可能产生新的零延迟事件
              → 再次触发 Adapter.pre_tick()
                
直到系统稳定（无新事件产生）
```

**含义**: Adapter 的实现必须考虑多次迭代执行的可能性。

---

## 💡 v2 方案的优势

| 方面 | 优势 |
|------|------|
| **学习曲线** | ✅ 开发者不需要学习 Port<T>，只需理解 Adapter |
| **代码复用** | ✅ 100% 复用现有的 SimplePort、Packet、Extension |
| **向后兼容** | ✅ 零破坏性，现有代码照常工作 |
| **演进灵活** | ✅ 新 Extension 类型无需修改框架 |
| **测试成本** | ✅ 现有的 Port、Packet 测试都通过 |
| **性能** | ✅ 无额外开销（与 v1 相同，但实现更简洁） |
| **文档** | ✅ 复用现有文档，额外说明只需 Adapter 部分 |

---

## 📊 v2 工作量估计（改进）

```
总工作量：~ 320-400 人·小时（相比 v1 的 400-500 减少 20-30%）

分布：
  Phase A: 60 人·小时 (1-2 周)    ← 减少 20%
  Phase B: 120 人·小时 (2-3 周)   ← 减少 20%
  Phase C: 80 人·小时 (1-2 周)    ← 减少 20%
  集成/文档: 80 人·小时 (1-2 周)   ← 新增

关键改进：
  ❌ 不需要设计/实现 Port<T> 系统 (-60 小时)
  ❌ 不需要改造 PortManager (-30 小时)
  ✅ 只需实现 Adapter 子类 (+40 小时)
```

---

## 🚀 立即可采取的行动

### 第 1 周

- [ ] 阅读 DESIGN_REVIEW_AND_ADJUSTMENTS.md
- [ ] 确认 v2 方案是否可接受
- [ ] 开始 HybridTimingExtension 开发

### 第 2 周

- [ ] 完成 TLMToHWAdapterBase
- [ ] 完成 ReadCmdAdapter
- [ ] 第一个集成测试通过

### 第 3 周

- [ ] Phase B 开始
- [ ] WriteDataAdapter 开发
- [ ] 多 Adapter 共存验证

---

## 📚 文档导航（v2）

| 文档 | 关键内容 |
|------|--------|
| DESIGN_REVIEW_AND_ADJUSTMENTS.md | 🔍 深度分析：为什么调整，调整什么 |
| HYBRID_TLM_EXTENSION_INTEGRATION.md | 📖 Extension 机制深入解析 |
| 本文件 | 🎯 v2 快速参考 |
| HYBRID_MODELING_ANALYSIS.md | (保留，但部分需要更新) |
| HYBRID_IMPLEMENTATION_ROADMAP.md | (需要基于 v2 重写) |

---

## ✅ v2 验收检查清单

### Phase A 完成标准

- [ ] HybridTimingExtension 编译通过，测试通过
- [ ] TLMToHWAdapterBase 实现，遵守用户契约
- [ ] ReadCmdAdapter 可工作
- [ ] 单元测试 > 90% 覆盖
- [ ] 现有模块无任何修改
- [ ] 现有 JSON 配置无任何修改
- [ ] 新模块可通过 ModuleFactory 注册
- [ ] 性能开销 < 5%

### Phase B 完成标准

- [ ] WriteDataAdapter 可工作
- [ ] CoherenceAdapter 可工作
- [ ] 三个 Adapter 可同时使用
- [ ] 集成测试覆盖所有 Adapter
- [ ] VCD 输出正确（包含 transaction_id）

### Phase C 完成标准

- [ ] 时空映射精确度 < 5%
- [ ] transaction_id 端到端正确
- [ ] MemoryProxy (如实现) 性能提升 > 50%

---

**准备好用 v2 方案启动了吗？** 🚀

相比 v1，v2 方案更符合 GemSc 的设计哲学，工作量更少，风险更低。

