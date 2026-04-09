# GemSc + CppHDL 混合建模 - 快速参考卡

> **用途**: 快速查阅关键信息、决策表、命令行  
> **格式**: 一页纸快速参考

---

## 📊 架构简图

```
GemSc (TLM)              TLMToStreamAdapter<T>           CppHDL (RTL)
┌─────────────────────┐  ┌────────────────────┐  ┌─────────────────────┐
│ Packet*             │  │ 1. 流控转换        │  │ ch_stream<T>        │
│ send(pkt)           │  │ 2. 时空映射        │  │ valid + ready       │
│ recv(pkt)           │  │ 3. 零拷贝内存      │  │ tick() 驱动         │
│ 反压 (bool)         │──│ 4. 事务追踪        │──│ 周期精确            │
│ EventQueue @1GHz    │  │ 5. 配置发现        │  │ 可 @ 多频率         │
└─────────────────────┘  └────────────────────┘  └─────────────────────┘
        TLM 发起          ✨ 五维适配层           RTL 响应
```

---

## 🎯 关键决策快速查表

| 问题 | 答案 | 为什么 |
|------|------|--------|
| 用什么Port类型? | `Port<T>` 泛型模板 | 支持任意类型，向后兼容 |
| 小数据<256B传递方式? | 值语义（栈） | 零分配，高效 |
| 大数据>256B传递方式? | `MemoryProxy`（直接指针） | 无复制，性能 10-100x |
| 如何转换Packet→Bundle? | 通过 `TLMToStreamAdapter<T>` | 1️⃣-5️⃣维适配 |
| 如何处理时钟差异? | `CDCAsyncFIFO` + `TemporalMapper` | Gray code同步 + beat展开 |
| 如何追踪事务? | `TransactionID` 透传 + VCD输出 | 端到端可观测性 |
| 性能开销多少? | < 10% | 零拷贝设计 + 缓冲批量处理 |
| 向后兼容吗? | 100% | PacketPort 包装原有SimplePort |

---

## 📈 实施路线图（一表）

| Phase | 周数 | 主任务 | 输出 | 可用性 |
|-------|------|--------|------|--------|
| **A** | 2 | Port<T>, SimObject扩展 | `generic_port.hh` | 框架级 |
| **B** | 2 | TLMToStreamAdapter核心 | `tlm_stream_adapter.hh` | 可混合仿真 |
| **B** | 1 | MemoryProxy实现 | 零拷贝内存 | 大数据优化 |
| **C** | 1-2 | TemporalMapper完善 | 变延迟支持 | 精确建模 |
| **C** | 1 | TransactionTracker | VCD追踪 | 调试可视化 |
| **C** | 1 | 自动配置 | JSON驱动 | 易用性 |
| **持续** | ∞ | 优化、文档、案例 | 生产就绪 | 可工业应用 |

**总计**: ~10周核心，16周完整生产级

---

## 🔧 快速启动命令

### 创建项目分支

```bash
cd /workspaces/GemSc
git checkout -b feature/hybrid-modeling
git pull origin main
```

### 创建基础文件结构

```bash
mkdir -p include/core include/adapters include/debug
touch include/core/port.hh
touch include/core/fifo_port.hh
touch include/adapters/adapter.hh
touch include/adapters/tlm_stream_adapter.hh
```

### 编写 Phase A.1 代码

```cpp
// include/core/port.hh - 参考 PORT_AND_ADAPTER_DESIGN.md
template <typename T>
class Port {
    virtual bool trySend(const T& data) = 0;
    virtual bool tryRecv(T& data_out) = 0;
    // ... 完整API见设计文档
};
```

### 编译与测试

```bash
cd build
cmake .. -DENABLE_HYBRID_MODELING=ON
make -j8
ctest --verbose
```

---

## 📋 5维适配层的5个关键特性

### 1️⃣ 流控协议转换

```cpp
// TLM 侧：simple bool 返回
bool success = port->trySend(req);  // true/false

// HW 侧：握手信号
adapter->setHWReady(true);
bool hw_valid = adapter->getHWValid();

// 适配层：状态机驱动握手
adapter->tick(cycle);  // 推进状态机
```

### 2️⃣ 时空映射

```cpp
// TLM 侧：一个 100-cycle 的读事务
Packet tlm_req(addr=0x1000, size=64, delay=100);

// HW 侧：自动展开为 4 个 beat
//  beat 0 @ cycle 100, data[0:15]
//  beat 1 @ cycle 125, data[16:31]
//  beat 2 @ cycle 150, data[32:47]
//  beat 3 @ cycle 175, data[48:63]

TemporalMapper mapper;
auto beats = mapper.expandTransaction(tlm_req, 16, 100);
```

### 3️⃣ 零拷贝内存

```cpp
// 分配 64MB 的物理内存
PhysicalMemory memory(0x80000000, 64*1024*1024);

// CppHDL DMA 模块需要读取内存
MemoryProxy proxy(&memory, 0x80000000);

// 直接访问：无复制！
uint8_t* data = proxy.directRead(0x80100000, 4096);
memcpy(local_buffer, data, 4096);  // HW 侧的操作
```

### 4️⃣ 调试与追踪

```cpp
// 创建事务时分配 ID
uint64_t tid = TransactionIDGenerator::allocate();
pkt->transaction_id = tid;

// 在关键点记录
tracker->markPhase(tid, "TLM_SEND", cycle=100);
tracker->markPhase(tid, "ADAPTER_FIFO_IN", cycle=100);
tracker->markPhase(tid, "HW_VALID", cycle=102);

// 生成追踪报告
tracker->generateReport("trace.log");

// 输出示例
// tid=0x1001 phase=TLM_SEND cycle=100
// tid=0x1001 phase=HW_VALID cycle=102
// tid=0x1001 phase=ADAPTER_FIFO_OUT cycle=108
```

### 5️⃣ 配置与发现

```json
{
  "adapters": [
    {
      "src": "cpu_core_0",
      "dst": "l1_cache_0",
      "type": "TLMToStream",
      "config": {
        "fifo_depth": 32,
        "delay_cycles": 2,
        "burst_size": 32,
        "frequency_ratio": 1  // 同频
      }
    },
    {
      "src": "l1_cache_0",
      "dst": "noc_router",
      "type": "TLMToStream",
      "config": {
        "fifo_depth": 64,
        "delay_cycles": 5,
        "burst_size": 16,
        "frequency_ratio": 2  // HW 是 GemSc 的 2x 频率
      }
    }
  ]
}
```

---

## 📊 性能指标目标

| 指标 | 目标 | 检验方法 |
|------|------|---------|
| 内存开销 | < 5% 增加 | 对比纯 TLM |
| 执行速度 | < 10% 减速 | 相同工作负载 |
| 精度 | 延迟预测误差 < 15% | vs 硬件仿真 |
| 可靠性 | Valgrind clean | 无内存泄漏 |
| 易用性 | 学习曲线 < 1 天 | 新手编写适配器 |

---

## 🐛 常见问题速查

### Q1: 如何调试背压？
```cpp
if (!adapter->trySend(req)) {
    // 后续的 tick() 会推进状态
    // 在 VCD 中查看 fifo_occupancy 和 hw_ready
    // 通常原因：下游模块慢，或 HW 反应不及时
}
```

### Q2: 如何处理多时钟域？
```cpp
// 在适配器中使用 CDCAsyncFIFO
auto async_fifo = new CDCAsyncFIFO();
// write_domain: GemSc @100MHz
// read_domain:  CppHDL @500MHz
// 自动同步，延迟 2-3 个目标时钟周期
```

### Q3: 大数据如何零拷贝？
```cpp
// ❌ 不要这样（复制！）
uint8_t data[4096] = {...};
adapter->trySend(data);

// ✅ 应该这样（直接指针）
MemoryProxy proxy(&mem, 0x80000000);
uint8_t* ptr = proxy.directRead(addr, 4096);  // 无复制
proxy.directWrite(addr, ptr, 4096);           // 无复制
```

### Q4: 如何验证正确性？
```bash
# 生成 VCD 波形
./gemsc_hybrid simulation.json --vcd=output.vcd

# 用 GTKWave 查看
gtkwave output.vcd

# 追踪报告
./gemsc_hybrid simulation.json --trace=trace.log
```

---

## 🎬 最小可运行示例

### 步骤1：定义数据类型

```cpp
// 简单的内存请求
struct MemRequest {
    uint64_t addr;
    uint32_t size;
    bool is_write;
};
```

### 步骤2：创建适配器

```cpp
auto adapter = new TLMToStreamAdapter<MemRequest>("cpu_to_cache");
adapter->setTLMToHWCapacity(32);
adapter->setDelay(3);
```

### 步骤3：TLM侧发送

```cpp
MemRequest req = {.addr=0x1000, .size=64, .is_write=false};
if (!adapter->trySend(req)) {
    // 背压，缓冲满
}
```

### 步骤4：HW侧处理

```cpp
for (int i = 0; i < 100; ++i) {
    adapter->tick(i);
    
    if (adapter->getHWValid()) {
        auto req = adapter->getHWPayload();
        printf("HW: received request addr=0x%lx\n", req.addr);
        adapter->setHWReady(true);
    }
}
```

### 步骤5：查看统计

```cpp
adapter->printStats();
// Output:
// TLMToStreamAdapter[cpu_to_cache] Statistics:
//   TLM→HW transfers: 100
//   HW→TLM transfers: 100
//   Backpressure events: 5
//   FIFO max occupancy: 28/32
```

---

## 📚 文档导航

| 问题 | 查阅文档 |
|------|---------|
| "我想了解整个架构" | `HYBRID_MODELING_ANALYSIS.md` |
| "我想要实施计划" | `HYBRID_IMPLEMENTATION_ROADMAP.md` |
| "我想看API设计" | `PORT_AND_ADAPTER_DESIGN.md` |
| "我想快速上手" | **本文档** (快速参考卡) |
| "我想了解高层建议" | `HYBRID_PLATFORM_RECOMMENDATIONS.md` |

---

## ✅ 下周任务清单

- [ ] 审查所有5份文档
- [ ] 做出 Phase A 的开始决定
- [ ] 分配 1-2 个核心开发者
- [ ] 创建 `feature/hybrid-modeling` 分支
- [ ] 第一周目标：Port<T> 框架设计完成

**目标**: 在第2-3周看到可编译的代码！

---

## 💬 联系方式

有问题？建议？

1. 查阅上述5份设计文档
2. 在代码审查中讨论具体实现
3. 逐周迭代，持续优化

**记住**: 这是一个可实现的目标。分阶段推进，每个阶段都有明确的交付物。🚀

---

**最后的话：** 你提出的这个问题触及了**硬件仿真的核心问题**——如何既快又精。这份方案就是答案。祝你成功！

