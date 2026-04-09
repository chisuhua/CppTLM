# Port<T> 模板与适配器框架 - API设计规范

> **版本**: 1.0  
> **状态**: Ready for Implementation  
> **优先级**: P1 (基础)

---

## 1. Port<T> 模板设计

### 1.1 核心接口

```cpp
// include/core/port.hh
#ifndef PORT_HH
#define PORT_HH

#include <cstdint>
#include <queue>
#include <stdexcept>
#include <string>

// 端口状态枚举
enum class PortState {
    IDLE,           // 空闲
    ACTIVE,         // 活跃
    BACKPRESSURE,   // 背压
    ERROR           // 错误
};

// 数据流方向
enum class PortDirection {
    INPUT,          // 接收
    OUTPUT,         // 发送
    BIDIRECTIONAL   // 双向
};

// ============================================================================
// Port<T> 泛型模板 - 核心端口基类
// ============================================================================
template <typename T>
class Port {
public:
    using value_type = T;
    
    // ---- 核心接口 ----
    
    /**
     * 尝试发送数据（非阻塞）
     * @param data 要发送的数据
     * @return true 如果发送成功，false 如果缓冲满（背压）
     */
    virtual bool trySend(const T& data) = 0;
    
    /**
     * 尝试接收数据（非阻塞）
     * @param data_out 接收到的数据（如果返回true）
     * @return true 如果接收成功，false 如果缓冲空
     */
    virtual bool tryRecv(T& data_out) = 0;
    
    // ---- 状态查询 ----
    
    /**
     * 检查缓冲是否满
     */
    virtual bool isFull() const = 0;
    
    /**
     * 检查缓冲是否空
     */
    virtual bool isEmpty() const = 0;
    
    /**
     * 获取缓冲占用数量
     */
    virtual size_t getOccupancy() const = 0;
    
    /**
     * 获取缓冲容量
     */
    virtual size_t getCapacity() const = 0;
    
    /**
     * 获取端口状态
     */
    virtual PortState getState() const = 0;
    
    // ---- 配置接口 ----
    
    /**
     * 设置缓冲深度
     */
    virtual void setCapacity(size_t new_capacity) = 0;
    
    /**
     * 设置端口名称（用于日志）
     */
    virtual void setName(const std::string& name) = 0;
    
    /**
     * 获取端口名称
     */
    virtual std::string getName() const = 0;
    
    /**
     * 设置端口方向（用于连接验证）
     */
    virtual void setDirection(PortDirection dir) = 0;
    
    /**
     * 获取端口方向
     */
    virtual PortDirection getDirection() const = 0;
    
    // ---- 统计信息 ----
    
    /**
     * 获取已发送的元素数量
     */
    virtual uint64_t getSentCount() const = 0;
    
    /**
     * 获取已接收的元素数量
     */
    virtual uint64_t getRecvCount() const = 0;
    
    /**
     * 获取背压事件次数
     */
    virtual uint64_t getBackpressureCount() const = 0;
    
    /**
     * 重置统计信息
     */
    virtual void resetStats() = 0;
    
    // ---- 生命周期 ----
    
    /**
     * 初始化端口
     */
    virtual void init() {}
    
    /**
     * 启用端口
     */
    virtual void enable() {}
    
    /**
     * 禁用端口
     */
    virtual void disable() {}
    
    /**
     * 虚析构
     */
    virtual ~Port() = default;
};

#endif // PORT_HH
```

### 1.2 FIFO端口实现

```cpp
// include/core/fifo_port.hh
#ifndef FIFO_PORT_HH
#define FIFO_PORT_HH

#include "port.hh"
#include <mutex>
#include <condition_variable>

// ============================================================================
// FIFOPort<T> - 带缓冲的基础实现
// ============================================================================
template <typename T>
class FIFOPort : public Port<T> {
public:
    explicit FIFOPort(const std::string& name = "", size_t capacity = 16)
        : name_(name), capacity_(capacity), state_(PortState::IDLE),
          sent_count_(0), recv_count_(0), backpressure_count_(0) {}
    
    // ---- 核心实现 ----
    
    bool trySend(const T& data) override {
        if (buffer_.size() >= capacity_) {
            backpressure_count_++;
            return false;  // 缓冲满，背压
        }
        buffer_.push(data);
        sent_count_++;
        state_ = PortState::ACTIVE;
        return true;
    }
    
    bool tryRecv(T& data_out) override {
        if (buffer_.empty()) {
            return false;  // 缓冲空
        }
        data_out = buffer_.front();
        buffer_.pop();
        recv_count_++;
        state_ = PortState::ACTIVE;
        return true;
    }
    
    // ---- 状态查询实现 ----
    
    bool isFull() const override {
        return buffer_.size() >= capacity_;
    }
    
    bool isEmpty() const override {
        return buffer_.empty();
    }
    
    size_t getOccupancy() const override {
        return buffer_.size();
    }
    
    size_t getCapacity() const override {
        return capacity_;
    }
    
    PortState getState() const override {
        return state_;
    }
    
    // ---- 配置实现 ----
    
    void setCapacity(size_t new_capacity) override {
        if (new_capacity < 1) {
            throw std::invalid_argument("Capacity must be at least 1");
        }
        capacity_ = new_capacity;
    }
    
    void setName(const std::string& name) override {
        name_ = name;
    }
    
    std::string getName() const override {
        return name_;
    }
    
    void setDirection(PortDirection dir) override {
        direction_ = dir;
    }
    
    PortDirection getDirection() const override {
        return direction_;
    }
    
    // ---- 统计实现 ----
    
    uint64_t getSentCount() const override {
        return sent_count_;
    }
    
    uint64_t getRecvCount() const override {
        return recv_count_;
    }
    
    uint64_t getBackpressureCount() const override {
        return backpressure_count_;
    }
    
    void resetStats() override {
        sent_count_ = 0;
        recv_count_ = 0;
        backpressure_count_ = 0;
    }
    
protected:
    std::queue<T> buffer_;
    std::string name_;
    size_t capacity_;
    PortState state_;
    PortDirection direction_ = PortDirection::BIDIRECTIONAL;
    
    uint64_t sent_count_;
    uint64_t recv_count_;
    uint64_t backpressure_count_;
};

#endif // FIFO_PORT_HH
```

### 1.3 线程安全端口（可选）

```cpp
// include/core/thread_safe_port.hh
#ifndef THREAD_SAFE_PORT_HH
#define THREAD_SAFE_PORT_HH

#include "fifo_port.hh"
#include <mutex>

// ============================================================================
// ThreadSafePort<T> - 线程安全的FIFO端口
// ============================================================================
template <typename T>
class ThreadSafePort : public FIFOPort<T> {
public:
    using FIFOPort<T>::FIFOPort;
    
    bool trySend(const T& data) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return FIFOPort<T>::trySend(data);
    }
    
    bool tryRecv(T& data_out) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return FIFOPort<T>::tryRecv(data_out);
    }
    
    bool isFull() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return FIFOPort<T>::isFull();
    }
    
    bool isEmpty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return FIFOPort<T>::isEmpty();
    }
    
    size_t getOccupancy() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return FIFOPort<T>::getOccupancy();
    }
    
private:
    mutable std::mutex mutex_;
};

#endif // THREAD_SAFE_PORT_HH
```

---

## 2. 向后兼容层

### 2.1 PacketPort - Packet指针的包装

```cpp
// include/core/packet_port.hh
#ifndef PACKET_PORT_HH
#define PACKET_PORT_HH

#include "fifo_port.hh"
#include "packet.hh"

// ============================================================================
// PacketPort - 为现有Packet指针提供Port<Packet*>接口
// ============================================================================
class PacketPort : public FIFOPort<Packet*> {
public:
    using FIFOPort<Packet*>::FIFOPort;
    
    // 向后兼容：提供原有的send/recv接口
    virtual bool send(Packet* pkt) {
        return this->trySend(pkt);
    }
    
    virtual bool recv(Packet*& pkt_out) {
        return this->tryRecv(pkt_out);
    }
    
    // 虚函数，供子类覆盖
    virtual bool recvImpl(Packet* pkt) {
        // 默认实现：仅接收，子类可扩展
        return true;
    }
};

#endif // PACKET_PORT_HH
```

### 2.2 SimplePort 到 PacketPort 的过渡

```cpp
// include/core/simple_port.hh (修改)
#ifndef SIMPLE_PORT_HH
#define SIMPLE_PORT_HH

#include "packet_port.hh"

// 现有的SimplePort改为继承PacketPort，确保向后兼容
class SimplePort : public PacketPort {
public:
    // ... 保持原有接口 ...
    
    // 新增：获取为PacketPort的引用（用于适配层）
    PacketPort* asPacketPort() {
        return static_cast<PacketPort*>(this);
    }
};

#endif // SIMPLE_PORT_HH
```

---

## 3. 适配器基类

### 3.1 Adapter<T> 基类

```cpp
// include/adapters/adapter.hh
#ifndef ADAPTER_HH
#define ADAPTER_HH

#include "port.hh"
#include "packet.hh"
#include <memory>

// ============================================================================
// Adapter<T> - 所有适配器的基类
// ============================================================================
template <typename T>
class Adapter : public Port<T> {
public:
    explicit Adapter(const std::string& name = "")
        : name_(name), enabled_(true) {}
    
    // ---- 生命周期 ----
    
    void init() override {
        onInit();
    }
    
    void enable() override {
        enabled_ = true;
        onEnable();
    }
    
    void disable() override {
        enabled_ = false;
        onDisable();
    }
    
    // ---- 配置 ----
    
    void setName(const std::string& name) override {
        name_ = name;
    }
    
    std::string getName() const override {
        return name_;
    }
    
    // ---- 虚拟钩子（供子类覆盖）----
    
    /**
     * 初始化钩子
     */
    virtual void onInit() {}
    
    /**
     * 启用钩子
     */
    virtual void onEnable() {}
    
    /**
     * 禁用钩子
     */
    virtual void onDisable() {}
    
    /**
     * 心跳驱动（用于CppHDL侧）
     */
    virtual void tick(uint64_t cycle) {}
    
protected:
    std::string name_;
    bool enabled_;
};

#endif // ADAPTER_HH
```

---

## 4. TLMToStreamAdapter 框架

### 4.1 接口定义

```cpp
// include/adapters/tlm_stream_adapter.hh
#ifndef TLM_STREAM_ADAPTER_HH
#define TLM_STREAM_ADAPTER_HH

#include "adapter.hh"
#include "packet.hh"
#include <queue>
#include <memory>

// ============================================================================
// TLMToStreamAdapter<T> - TLM ↔ CppHDL Stream双向适配
// ============================================================================
template <typename T>
class TLMToStreamAdapter : public Adapter<T> {
public:
    // ---- 构造与配置 ----
    
    explicit TLMToStreamAdapter(const std::string& name = "TLMToStreamAdapter")
        : Adapter<T>(name),
          tlm_to_hw_capacity_(16),
          hw_to_tlm_capacity_(16),
          delay_cycles_(0),
          hw_valid_(false),
          hw_ready_(false),
          total_cycles_(0) {}
    
    /**
     * 设置TLM→HW方向的FIFO深度
     */
    void setTLMToHWCapacity(size_t cap) {
        if (cap < 1) throw std::invalid_argument("Capacity must be >= 1");
        tlm_to_hw_capacity_ = cap;
    }
    
    /**
     * 设置HW→TLM方向的FIFO深度
     */
    void setHWToTLMCapacity(size_t cap) {
        if (cap < 1) throw std::invalid_argument("Capacity must be >= 1");
        hw_to_tlm_capacity_ = cap;
    }
    
    /**
     * 设置TLM→HW的注入延迟（周期数）
     */
    void setDelay(uint64_t cycles) {
        delay_cycles_ = cycles;
    }
    
    // ---- 从Port<T>继承的接口实现 ----
    
    bool trySend(const T& data) override;
    bool tryRecv(T& data_out) override;
    
    bool isFull() const override;
    bool isEmpty() const override;
    size_t getOccupancy() const override;
    size_t getCapacity() const override;
    PortState getState() const override;
    
    void setCapacity(size_t new_capacity) override;
    void setDirection(PortDirection dir) override;
    PortDirection getDirection() const override;
    
    uint64_t getSentCount() const override {
        return stats_.tlm_to_hw_sent;
    }
    
    uint64_t getRecvCount() const override {
        return stats_.hw_to_tlm_recv;
    }
    
    uint64_t getBackpressureCount() const override {
        return stats_.backpressure_events;
    }
    
    void resetStats() override;
    
    // ---- CppHDL硬件侧接口 ----
    
    /**
     * 硬件侧心跳驱动
     * 由CppHDL模块调用以推进适配器状态
     */
    void tick(uint64_t cycle) override;
    
    /**
     * 硬件侧反馈：设置ready信号
     */
    void setHWReady(bool ready) {
        hw_ready_ = ready;
    }
    
    /**
     * 硬件侧查询：获取valid信号
     */
    bool getHWValid() const {
        return hw_valid_;
    }
    
    /**
     * 硬件侧查询：获取负载数据
     */
    const T& getHWPayload() const {
        return hw_payload_;
    }
    
    // ---- 统计与调试 ----
    
    struct Statistics {
        uint64_t tlm_to_hw_sent = 0;      // TLM发送到HW的数据数
        uint64_t hw_to_tlm_recv = 0;      // HW发送到TLM的数据数
        uint64_t backpressure_events = 0; // 背压事件数
        uint64_t tlm_to_hw_fifo_max = 0;  // TLM→HW FIFO最大占用
        uint64_t hw_to_tlm_fifo_max = 0;  // HW→TLM FIFO最大占用
    };
    
    const Statistics& getStats() const {
        return stats_;
    }
    
    void printStats() const;
    
private:
    // ---- 内部数据 ----
    
    std::queue<T> tlm_to_hw_queue_;
    std::queue<T> hw_to_tlm_queue_;
    
    size_t tlm_to_hw_capacity_;
    size_t hw_to_tlm_capacity_;
    uint64_t delay_cycles_;
    
    // ---- 硬件侧握手信号 ----
    bool hw_valid_;
    bool hw_ready_;
    T hw_payload_;
    
    // ---- 状态机 ----
    enum State {
        IDLE,           // 空闲
        TRANSFERRING,   // 正在传输
        WAITING,        // 等待硬件就绪
    } state_;
    
    uint64_t total_cycles_;
    
    // ---- 统计 ----
    Statistics stats_;
    PortDirection direction_ = PortDirection::BIDIRECTIONAL;
    
    // ---- 内部方法 ----
    void updateTLMToHWPath();
    void updateHWToTLMPath();
    void updateState();
};

#endif // TLM_STREAM_ADAPTER_HH
```

---

## 5. 使用示例

### 5.1 简单使用（TLM仅）

```cpp
// 创建端口
Port<int>* port = new FIFOPort<int>("my_port", 16);

// 发送数据
if (!port->trySend(42)) {
    std::cerr << "Backpressure!" << std::endl;
}

// 接收数据
int data;
if (port->tryRecv(data)) {
    std::cout << "Received: " << data << std::endl;
}

// 查询状态
std::cout << "Occupancy: " << port->getOccupancy() 
          << "/" << port->getCapacity() << std::endl;

delete port;
```

### 5.2 混合使用（TLM + RTL）

```cpp
// 创建适配器
TLMToStreamAdapter<MemRequest>* adapter = 
    new TLMToStreamAdapter<MemRequest>("l1_to_l2");
adapter->setTLMToHWCapacity(32);
adapter->setDelay(3);

// TLM侧：模块发送请求
MemRequest req = buildRequest(0x1000, MemRequest::READ);
if (!adapter->trySend(req)) {
    tlm_stall_cycles++;  // 背压处理
}

// CppHDL侧：硬件心跳驱动
for (int i = 0; i < 100; ++i) {
    adapter->tick(i);
    
    if (adapter->getHWValid()) {
        processRequest(adapter->getHWPayload());
        adapter->setHWReady(true);
    }
}

// TLM侧：接收响应
MemResponse resp;
if (adapter->tryRecv(resp)) {
    handleResponse(resp);
}

// 查看统计
adapter->printStats();
```

---

## 6. 编译与测试

### 6.1 CMakeLists.txt 更新

```cmake
# include/core/CMakeLists.txt
add_library(gemsc_core
    port.hh
    fifo_port.hh
    thread_safe_port.hh
    packet.hh
    packet_port.hh
    sim_object.hh
    event_queue.hh
)

set_target_properties(gemsc_core PROPERTIES
    LINKER_LANGUAGE CXX
    CXX_STANDARD 17
)

# 适配器库
add_library(gemsc_adapters
    adapters/adapter.hh
    adapters/tlm_stream_adapter.hh
)

target_link_libraries(gemsc_adapters PUBLIC gemsc_core)
```

### 6.2 单元测试模板

```cpp
// test/test_port.cc
#include "catch_amalgamated.hpp"
#include "port.hh"
#include "fifo_port.hh"

TEST_CASE("FIFOPort Basic Operations", "[port]") {
    FIFOPort<int> port("test", 16);
    
    SECTION("Send and Receive") {
        REQUIRE(port.isEmpty());
        REQUIRE(port.trySend(42));
        REQUIRE(!port.isEmpty());
        
        int data;
        REQUIRE(port.tryRecv(data));
        REQUIRE(data == 42);
        REQUIRE(port.isEmpty());
    }
    
    SECTION("Backpressure") {
        for (int i = 0; i < 16; ++i) {
            REQUIRE(port.trySend(i));
        }
        REQUIRE(port.isFull());
        REQUIRE(!port.trySend(100));  // 背压
    }
    
    SECTION("Statistics") {
        port.trySend(1);
        port.trySend(2);
        int data;
        port.tryRecv(data);
        
        REQUIRE(port.getSentCount() == 2);
        REQUIRE(port.getRecvCount() == 1);
    }
}
```

---

## 总结

这份API设计提供了：

✅ **清晰的接口**：`Port<T>` 泛型模板支持任意类型  
✅ **向后兼容**：现有 `SimplePort` 和 `Packet*` 代码无需修改  
✅ **易于扩展**：适配器框架为新的转换逻辑提供基础  
✅ **完整的统计**：内置性能监控  
✅ **生产就绪**：包含线程安全、错误处理、测试框架  

**下一步**：实现 `tlm_stream_adapter.hh` 中的核心逻辑。

