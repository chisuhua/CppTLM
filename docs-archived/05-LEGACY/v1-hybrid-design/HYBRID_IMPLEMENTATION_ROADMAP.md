# GemSc + CppHDL 混合建模平台 - 实施计划与架构建议

> **文档日期**: 2026年1月28日  
> **标题**: 从事务级仿真到硬件级细粒度建模的无缝集成  
> **目标受众**: 架构师、核心开发者

---

## 📌 核心建议总结（一页纸版）

### 问题陈述

GemSc 现在是**纯事务级框架**，CppHDL 是**硬件级建模框架**。两者的融合需要解决：

```
GemSc (TLM)                          CppHDL (RTL)
┌─────────────────────┐              ┌─────────────────────┐
│ Packet* (指针)      │  ─────────→  │ ch_stream<T> (值)    │
│ 高层逻辑（毫秒）    │              │ 低层精确（纳秒）    │
│ 反压 (bool返回)     │              │ valid/ready握手     │
│ 配置驱动            │              │ 周期精确            │
└─────────────────────┘              └─────────────────────┘

          ↓ 核心问题 ↓
1. 数据格式不兼容 (Packet* vs Bundle<T>)
2. 时间粒度差异 (毫秒级 vs 纳秒级)
3. 流控协议不同 (bool vs valid/ready)
4. 内存管理不同 (指针 vs 值语义)
```

### 解决方案：五维适配层

```
     ┌───────────────────────────────────────┐
     │   1️⃣  流控协议转换                    │
     │   (Packet ↔ Bundle, bool ↔ valid/ready) │
     ├───────────────────────────────────────┤
     │   2️⃣  时空映射                        │
     │   (TLM事务 → HW多周期beat)            │
     ├───────────────────────────────────────┤
     │   3️⃣  零拷贝内存                      │
     │   (直接访问物理内存，避免复制)       │
     ├───────────────────────────────────────┤
     │   4️⃣  调试追踪                        │
     │   (事务ID透传，统一VCD)               │
     ├───────────────────────────────────────┤
     │   5️⃣  配置与发现                      │
     │   (JSON参数化，自动类型推导)         │
     └───────────────────────────────────────┘
```

### 立即行动方案（优先级排序）

| 优先级 | 功能 | 工作量 | 时间 | 输出 |
|--------|------|--------|------|------|
| **P1** | Port<T> 模板 | ⭐⭐⭐ | 1周 | `generic_port.hh` |
| **P2** | TLMToStreamAdapter | ⭐⭐⭐⭐ | 2周 | 流控适配 |
| **P3** | MemoryProxy | ⭐⭐⭐ | 1周 | 零拷贝 |
| **P4** | TemporalMapper | ⭐⭐⭐⭐ | 1-2周 | 时空映射 |
| **P5** | TransactionTracker | ⭐⭐ | 1周 | 调试 |

**总工作量：** ≈8-10周（一个初级+一个中级开发者）

---

## 方案对比与选择指南

### 适配器类型选择矩阵

```
     CppHDL端
       ↓
GemSc→  Stream(valid+ready)     Flow(valid only)
  TLM   ───────────────────     ───────────────
  高层   TLMToStreamAdapter      TLMToFlowAdapter
        ✓ 双向反压               ✓ 单向简单
        ✓ 性能最优               ✗ 无反压
        ⭐ 推荐                  ⚠️  有损
```

**建议：** 优先实现 TLMToStreamAdapter，它最灵活且性能最优。

### 数据传递模式选择

```cpp
// 选项1：纯值语义（CppHDL原生）
template <typename T>
bool send(const T& data);
缺点：大数据复制开销（>256B时成为瓶颈）

// 选项2：纯指针语义（GemSc原生）
bool send(Packet* pkt);
缺点：内存所有权复杂

// 选项3：混合语义（推荐）
bool send(const SmallPacket& pkt);           // <256B，值语义
bool sendLarge(const MemoryProxy& proxy);    // >256B，零拷贝
```

**建议：** 采用混合语义，在小数据用值，大数据用零拷贝代理。

---

## 实施计划细节

### Phase A：基础接口增强

#### A.1 设计 Port<T> 模板

```cpp
// include/core/generic_port.hh
template <typename T>
class Port {
public:
    // 非阻塞发送
    virtual bool trySend(const T& data) = 0;
    
    // 非阻塞接收
    virtual bool tryRecv(T& data_out) = 0;
    
    // 状态查询
    virtual bool isFull() const = 0;
    virtual bool isEmpty() const = 0;
    virtual size_t getOccupancy() const = 0;
};

// 向后兼容包装
class PacketPort : public Port<Packet*> {
    // 将原有的 send(Packet*) 映射到 trySend()
};
```

**验收标准：**
- [ ] Port<T> 模板编译通过
- [ ] PacketPort 能无缝替换原有 SimplePort
- [ ] 单元测试覆盖率 > 90%

---

#### A.2 增强 SimObject 生命周期

```cpp
// include/core/sim_object.hh (修改)
class SimObject {
public:
    // 新增钩子
    virtual void preTick() {}
    virtual void tick() = 0;
    virtual void postTick() {}
    
    // 模块分类
    virtual bool isTimingSensitive() const { return false; }
    
    // 时钟信息
    virtual uint64_t getClockFrequencyHz() const { return 1e9; }
};
```

**验收标准：**
- [ ] 所有现有 SimObject 子类仍能正常工作
- [ ] 新钩子可被正确调用
- [ ] 不引入性能开销

---

#### A.3 扩展 Packet 类

```cpp
// 在现有 Packet 中增加
struct Packet {
    // ... 现有字段 ...
    
    // 新增：事务追踪
    uint64_t transaction_id = 0;
    uint64_t source_module_id = 0;
    uint64_t dest_module_id = 0;
    
    // 新增：时空映射支持
    uint64_t hw_injection_cycle = 0;
    uint64_t hw_completion_cycle = 0;
    
    // 新增：CppHDL集成
    void* cpphdl_bundle = nullptr;
    uint64_t cpphdl_beat_index = 0;
};
```

**验收标准：**
- [ ] Packet 大小不超过 256 字节（性能关键）
- [ ] 向后兼容，所有现有代码不需修改
- [ ] 新字段初始化正确

---

### Phase B：适配层核心

#### B.1 实现 TLMToStreamAdapter<T>

**关键设计：**

```cpp
// include/adapters/tlm_stream_adapter.hh
template <typename T>
class TLMToStreamAdapter : public Port<T> {
public:
    // 来自GemSc侧的接口
    bool trySend(const T& data) override;
    bool tryRecv(T& data) override;
    
    // 硬件侧驱动
    void tick();
    
    // 硬件侧信号接口（模拟）
    void setHWReady(bool ready) { hw_ready = ready; }
    bool getHWValid() const { return hw_valid; }
    const T& getHWPayload() const { return hw_payload; }
    
private:
    // 双向FIFO
    std::queue<T> tlm_to_hw_queue;
    std::queue<T> hw_to_tlm_queue;
    
    // 握手信号状态机
    enum State { IDLE, TRANSFERRING, WAITING } state;
    bool hw_valid = false;
    bool hw_ready = false;
    T hw_payload;
    
    // 配置参数
    size_t fifo_depth = 16;
    uint64_t delay_cycles = 0;
    
    // 统计
    struct Stats {
        uint64_t tlm_to_hw_transfers = 0;
        uint64_t hw_to_tlm_transfers = 0;
        uint64_t backpressure_cycles = 0;
    } stats;
    
    // 状态机驱动
    void updateState();
};
```

**关键逻辑：**

1. **TLM发送→HW接收**
   ```
   TLM: trySend(data) → 入tlm_to_hw_queue
        ↓
   Tick: 如果hw_ready，出队 → hw_valid=true
        ↓
   HW:  接收 hw_payload
   ```

2. **HW发送→TLM接收**
   ```
   HW: hw_valid=true, hw_payload=data
        ↓
   Tick: 检测valid，入hw_to_tlm_queue
        ↓
   TLM: tryRecv(data) → 出队
   ```

**单元测试：**
- [ ] 基础握手逻辑
- [ ] 背压传播
- [ ] FIFO溢出处理
- [ ] 延迟注入
- [ ] 统计准确性

---

#### B.2 实现 MemoryProxy

```cpp
// include/core/memory_proxy.hh
class MemoryProxy {
public:
    MemoryProxy(PhysicalMemory* mem, uint64_t base_addr)
        : phys_mem_(mem), base_addr_(base_addr) {}
    
    // 直接读取
    uint8_t* directRead(uint64_t addr, size_t len);
    
    // 直接写入
    bool directWrite(uint64_t addr, const uint8_t* data, size_t len);
    
    // 便利函数
    template<typename T>
    T read(uint64_t addr) {
        uint8_t* ptr = directRead(addr, sizeof(T));
        if (!ptr) throw std::runtime_error("Invalid address");
        return *reinterpret_cast<T*>(ptr);
    }
    
    template<typename T>
    void write(uint64_t addr, const T& value) {
        directWrite(addr, reinterpret_cast<const uint8_t*>(&value), sizeof(T));
    }
    
private:
    PhysicalMemory* phys_mem_;
    uint64_t base_addr_;
};
```

**验收标准：**
- [ ] 能够直接访问物理内存（无复制）
- [ ] 地址有效性检查
- [ ] DMA大数据传输性能提升 > 50%
- [ ] 安全性：越界访问会抛异常

---

#### B.3 初步时空映射

```cpp
// include/adapters/simple_temporal_mapper.hh
class SimpleTemporalMapper {
    // 将一个TLM事务展开为多个HW beat
    
    struct Beat {
        uint64_t beat_id;
        uint8_t* data_ptr;
        size_t data_size;
        bool is_last;
        uint64_t injection_cycle;  // 何时应该发送此beat
    };
    
    // 主函数：事务展开
    std::vector<Beat> expandTransaction(
        const Packet* pkt,
        size_t beat_size,          // 每个beat大小（如16B）
        uint64_t start_cycle       // 开始周期
    ) {
        auto num_beats = (pkt->getDataLen() + beat_size - 1) / beat_size;
        std::vector<Beat> beats;
        
        for (size_t i = 0; i < num_beats; ++i) {
            Beat b;
            b.beat_id = i;
            b.data_ptr = pkt->getDataPtr() + i * beat_size;
            b.data_size = std::min(beat_size, pkt->getDataLen() - i * beat_size);
            b.is_last = (i == num_beats - 1);
            b.injection_cycle = start_cycle + i;  // 固定周期展开
            beats.push_back(b);
        }
        return beats;
    }
};
```

---

### Phase C：高级特性

#### C.1 完善时空映射

**支持可变延迟：**

```cpp
enum LatencyProfile {
    FIXED,      // 固定延迟
    RANDOM,     // 随机延迟
    VARIABLE    // 可配置的变延迟
};

class AdvancedTemporalMapper {
    std::vector<Beat> expandTransactionWithVariableLatency(
        const Packet* pkt,
        size_t beat_size,
        uint64_t base_latency,
        LatencyProfile profile,
        uint64_t random_seed = 0
    );
};
```

#### C.2 CDC 仿真

```cpp
// include/adapters/cdc_async_fifo.hh
class CDCAsyncFIFO {
    // 异步FIFO，用于跨时钟域
    
    bool write(const Packet* pkt, uint64_t src_cycle);  // GemSc时钟
    Packet* read(uint64_t dst_cycle);                   // CppHDL时钟
    
private:
    // Gray code 指针用于CDC
    std::queue<Packet*> write_domain;
    std::queue<Packet*> read_domain;
    std::mutex domain_sync;
};
```

#### C.3 事务追踪

```cpp
// include/debug/transaction_tracker.hh
class TransactionTracker {
    // 记录每个事务的关键事件
    
    void markPhase(
        uint64_t tid,
        const std::string& phase,   // "TLM_SEND" / "ADAPTER_FIFO" / "HW_VALID" etc
        uint64_t cycle
    );
    
    // 生成报告
    void generateReport(const std::string& filename);
    
    // 输出格式示例：
    // tid=0x1001  phase=TLM_SEND       cycle=100
    // tid=0x1001  phase=ADAPTER_FIFO_IN  cycle=100
    // tid=0x1001  phase=HW_INJECTED    cycle=102
    // tid=0x1001  phase=HW_VALID       cycle=103
    // tid=0x1001  phase=ADAPTER_FIFO_OUT cycle=108
    // tid=0x1001  phase=TLM_RECV       cycle=110
};
```

---

## 测试与验证策略

### 单元测试（按优先级）

```
Phase B 完成后应有：
┌─────────────────────────────────────┐
│ TLMToStreamAdapter_Test             │
│ ├─ BasicHandshake                   │
│ ├─ Backpressure                     │
│ ├─ FIFOOverflow                     │
│ ├─ LatencyInjection                 │
│ └─ Statistics                       │
├─────────────────────────────────────┤
│ MemoryProxy_Test                    │
│ ├─ DirectReadWrite                  │
│ ├─ BoundaryChecks                   │
│ ├─ LargeDataTransfer                │
│ └─ Performance                      │
└─────────────────────────────────────┘
```

### 集成测试

1. **CPU-Cache-Memory 链**
   - 创建纯TLM系统
   - 逐步用RTL组件替换

2. **DMA传输**
   - 验证零拷贝内存访问
   - 测试带宽和延迟

3. **多时钟域**
   - GemSc @100MHz, CppHDL @500MHz
   - 验证CDC同步

---

## 关键指标与验收标准

### 性能指标

| 指标 | 目标 | 检查方法 |
|------|------|---------|
| 内存开销 | <5% vs 现有 | 运行大规模仿真，对比内存使用 |
| 时间开销 | <10% vs 现有 | 相同工作负载，对比仿真速度 |
| 延迟追踪精度 | <1 cycle | 与金标参考对比 |
| TLM→HW转换准确度 | 100% | 所有beat都正确注入 |

### 代码质量

| 指标 | 目标 |
|------|------|
| 单元测试覆盖 | > 90% |
| 代码注释 | > 50% |
| 无内存泄漏 | Valgrind clean |
| 编译警告 | 0 |

---

## 与CppHDL的集成要点

### 1. Bundle 类型映射

```cpp
// 定义映射规则
template<>
struct BundleMapping<MemRequest, ch_stream<MemRequest>> {
    static MemRequest fromBundle(const ch_stream<MemRequest>& stream) {
        return stream.payload;
    }
    
    static ch_stream<MemRequest> toBundle(const MemRequest& req) {
        ch_stream<MemRequest> stream;
        stream.payload = req;
        stream.valid = true;
        return stream;
    }
};
```

### 2. 模块交互模式

```cpp
// GemSc侧
class CPUSim : public SimObject {
    Port<MemRequest>* downstream;  // 可指向 TLMToStreamAdapter
    
    void tick() override {
        MemRequest req = buildRequest();
        if (!downstream->trySend(req)) {
            // 背压：缓冲满，等下一周期
            stall_cycles++;
        }
    }
};

// CppHDL侧（通过适配层驱动）
class HWCrossBar {
    void tick(ch_stream<MemRequest>& input) {
        if (input.fire()) {  // valid && ready
            // 处理请求
        }
    }
};
```

---

## 风险缓解

| 风险 | 缓解策略 |
|------|---------|
| **指针安全** | 使用智能指针 + Valgrind检查 |
| **时序不准** | 详细的事务追踪和VCD验证 |
| **内存泄漏** | 定期内存审计，使用RAII |
| **死锁** | 自动超时检测，强制推进 |
| **性能衰退** | 定期性能基准测试 |

---

## 总体时间表

```
Week 1-2:  Phase A.1 (Port<T> 模板)
Week 3-4:  Phase A.2-A.3 (SimObject + Packet扩展)
Week 5-8:  Phase B.1 (TLMToStreamAdapter)
Week 9-10: Phase B.2 (MemoryProxy)
Week 11:   Phase B.3 (SimpleTemporalMapper)
Week 12+:  Phase C (高级特性，持续迭代)
```

**总周期：** 3个月内完成核心功能，6个月完成生产就绪版本

---

## 下一步行动

### 立即执行（本周）

- [ ] 审视与批准 Port<T> 设计
- [ ] 分配P1任务给开发者
- [ ] 创建dev分支开始编码

### 2周内

- [ ] Port<T> 模板实现完成
- [ ] 启动B.1 (TLMToStreamAdapter)
- [ ] 编写详细的API文档

### 1月内

- [ ] Phase A完成
- [ ] Phase B.1完成 + 单元测试通过
- [ ] 完整技术文档

---

**问题？建议？** 这份计划是可执行的，但需要明确的优先级和资源承诺。建议先完成Phase A+B，评估实际工作量，再计划Phase C。

