# v2 方案改进清单与代码骨架

**报告日期**: 2026-01-28  
**目的**: 基于代码深潜分析，明确Phase A需要实现的具体内容

---

## 第一部分: 核心代码框架

### 1.1 HybridTimingExtension 完整定义

**文件**: `include/ext/hybrid_timing_extension.hh`

```cpp
#ifndef HYBRID_TIMING_EXTENSION_HH
#define HYBRID_TIMING_EXTENSION_HH

#include "tlm.h"
#include <cstdint>
#include <iostream>

/**
 * HybridTimingExtension
 * 
 * 用于跟踪TLM侧和HW侧的时序信息，支持混合建模中的
 * 精确延迟模型和多事务跟踪。
 * 
 * 使用场景:
 *   - [必需] 需要精确延迟时
 *   - [推荐] 有多个并行事务或多VC时
 *   - [可选] 仅进行功能验证时
 * 
 * 与现有Extension共存: 可与CoherenceExtension、
 * PerformanceExtension等并行使用
 */
struct HybridTimingExtension : public tlm::tlm_extension<HybridTimingExtension> {
    
    // ============================================
    // 时序信息
    // ============================================
    
    /// TLM侧: 事务发起时的周期
    uint64_t tlm_issue_cycle = 0;
    
    /// TLM侧: 事务被接受时的周期
    uint64_t tlm_grant_cycle = 0;
    
    /// HW侧: 事务发送到HW的周期
    uint64_t hw_issue_cycle = 0;
    
    /// HW侧: HW完成事务的周期
    uint64_t hw_complete_cycle = 0;
    
    // ============================================
    // 虚拟通道信息 - VC感知设计
    // ============================================
    
    /// TLM侧的虚拟通道ID（保留原始值供trace）
    int tlm_vc_id = 0;
    
    /// 映射到HW的通道ID（可与tlm_vc_id不同）
    int hw_channel_id = 0;
    
    // ============================================
    // 事务追踪
    // ============================================
    
    /// Adapter分配的事务ID，用于关联TLM请求和HW响应
    uint64_t txn_id = 0;
    
    // ============================================
    // 协议状态
    // ============================================
    
    /// 事务在混合建模中的当前阶段
    enum class AdapterPhase : uint8_t {
        CREATED,           // 0: Extension刚创建
        TLM_RECEIVED,      // 1: Adapter接收到TLM请求
        HW_CONVERTING,     // 2: 转换TLM到HW中
        HW_PROCESSING,     // 3: HW侧处理中
        HW_COMPLETED,      // 4: HW完成，等待转换
        TLM_RESPONSE,      // 5: 转换为TLM响应
        TLM_SENT           // 6: 已发送到上游
    };
    
    AdapterPhase phase = AdapterPhase::CREATED;
    
    // ============================================
    // TLM Extension 接口实现
    // ============================================
    
    /// 克隆此Extension
    tlm_extension* clone() const override {
        return new HybridTimingExtension(*this);
    }
    
    /// 从另一个Extension复制数据
    void copy_from(const tlm_extension& e) override {
        const auto& ext = static_cast<const HybridTimingExtension&>(e);
        tlm_issue_cycle = ext.tlm_issue_cycle;
        tlm_grant_cycle = ext.tlm_grant_cycle;
        hw_issue_cycle = ext.hw_issue_cycle;
        hw_complete_cycle = ext.hw_complete_cycle;
        tlm_vc_id = ext.tlm_vc_id;
        hw_channel_id = ext.hw_channel_id;
        txn_id = ext.txn_id;
        phase = ext.phase;
    }
    
    // ============================================
    // 便利方法
    // ============================================
    
    /// 获取TLM侧的完整延迟（发起到响应）
    uint64_t get_tlm_latency() const {
        return tlm_grant_cycle >= tlm_issue_cycle ? 
               (tlm_grant_cycle - tlm_issue_cycle) : 0;
    }
    
    /// 获取HW侧的处理延迟
    uint64_t get_hw_latency() const {
        return hw_complete_cycle >= hw_issue_cycle ? 
               (hw_complete_cycle - hw_issue_cycle) : 0;
    }
    
    /// 获取转换损耗（TLM接收到HW发送的周期差）
    uint64_t get_conversion_overhead() const {
        return hw_issue_cycle >= tlm_issue_cycle ? 
               (hw_issue_cycle - tlm_issue_cycle) : 0;
    }
    
    /// 获取阶段名称（用于调试）
    static const char* phase_to_string(AdapterPhase p) {
        switch(p) {
            case AdapterPhase::CREATED: return "CREATED";
            case AdapterPhase::TLM_RECEIVED: return "TLM_RECEIVED";
            case AdapterPhase::HW_CONVERTING: return "HW_CONVERTING";
            case AdapterPhase::HW_PROCESSING: return "HW_PROCESSING";
            case AdapterPhase::HW_COMPLETED: return "HW_COMPLETED";
            case AdapterPhase::TLM_RESPONSE: return "TLM_RESPONSE";
            case AdapterPhase::TLM_SENT: return "TLM_SENT";
            default: return "UNKNOWN";
        }
    }
};

// ============================================
// 便利函数
// ============================================

/// 从TLM payload获取HybridTimingExtension
inline HybridTimingExtension* get_hybrid_timing(tlm::tlm_generic_payload* p) {
    HybridTimingExtension* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

/// 设置HybridTimingExtension
inline void set_hybrid_timing(tlm::tlm_generic_payload* p, 
                             const HybridTimingExtension& src) {
    auto* ext = new HybridTimingExtension(src);
    p->set_extension(ext);
}

#endif // HYBRID_TIMING_EXTENSION_HH
```

---

### 1.2 Adapter 基类框架

**文件**: `include/adapters/tlm_to_hw_adapter_base.hh`

```cpp
#ifndef TLM_TO_HW_ADAPTER_BASE_HH
#define TLM_TO_HW_ADAPTER_BASE_HH

#include "../core/sim_object.hh"
#include "../core/packet.hh"
#include "../core/packet_pool.hh"
#include "../ext/hybrid_timing_extension.hh"
#include <unordered_map>
#include <queue>

/**
 * TLMToHWAdapterBase
 * 
 * 混合建模中TLM->HW适配器的通用基类。
 * 
 * 使用方式:
 *   template <typename HWTransactionType>
 *   class MyAdapter : public TLMToHWAdapterBase<HWTransactionType> {
 *       HWTransaction convert_tlm_to_hw(Packet* pkt) override { ... }
 *       HWResponse poll_hw_response(uint64_t txn_id) override { ... }
 *   };
 */
template <typename HWTransactionType>
class TLMToHWAdapterBase : public SimObject {
    
protected:
    // ============================================
    // 虚拟通道映射配置
    // ============================================
    
    /// TLM VC ID -> HW Channel ID 的映射
    std::unordered_map<int, int> vc_to_hw_channel;
    
    // ============================================
    // 事务跟踪
    // ============================================
    
    /// 待处理的TLM请求（txn_id -> Packet）
    std::unordered_map<uint64_t, Packet*> pending_tlm_requests;
    
    /// HW响应缓冲（txn_id -> response）
    std::unordered_map<uint64_t, HWTransactionType> hw_responses;
    
    /// 待发送的TLM响应
    std::queue<Packet*> response_queue;
    
    /// 序列号生成器
    uint64_t next_txn_id = 0;
    
    // ============================================
    // 统计信息
    // ============================================
    
    struct AdapterStats {
        uint64_t tlm_requests_received = 0;
        uint64_t tlm_requests_dropped = 0;
        uint64_t tlm_responses_sent = 0;
        uint64_t hw_transactions_issued = 0;
        uint64_t hw_transactions_completed = 0;
        uint64_t total_tlm_latency = 0;
        uint64_t total_hw_latency = 0;
        
        void reset() {
            tlm_requests_received = 0;
            tlm_requests_dropped = 0;
            tlm_responses_sent = 0;
            hw_transactions_issued = 0;
            hw_transactions_completed = 0;
            total_tlm_latency = 0;
            total_hw_latency = 0;
        }
    } stats;
    
public:
    // ============================================
    // 构造和析构
    // ============================================
    
    explicit TLMToHWAdapterBase(const std::string& name, EventQueue* eq)
        : SimObject(name, eq) {
        DPRINTF(ADAPTER, "[%s] Adapter created\n", name.c_str());
    }
    
    virtual ~TLMToHWAdapterBase() {
        // 清理待处理请求
        for (auto& [txn_id, pkt] : pending_tlm_requests) {
            PacketPool::get().release(pkt);
        }
    }
    
    // ============================================
    // 核心生命周期
    // ============================================
    
    /**
     * 处理上游的TLM请求
     * 
     * 工作流程:
     *   1. 验证请求有效性
     *   2. 创建HybridTimingExtension跟踪
     *   3. 应用VC映射
     *   4. 转换TLM到HW事务
     *   5. 发送到HW侧（具体实现由子类提供）
     *   6. 记录待处理
     */
    bool handleUpstreamRequest(Packet* pkt, int src_id, 
                               const std::string& src_label) override {
        
        // 1. 验证
        if (!pkt || !pkt->isRequest() || !pkt->payload) {
            DPRINTF(ADAPTER, "[%s] Invalid TLM request\n", name.c_str());
            if (pkt) PacketPool::get().release(pkt);
            stats.tlm_requests_dropped++;
            return false;
        }
        
        stats.tlm_requests_received++;
        
        // 2. 创建HybridTimingExtension
        auto* hybrid_ext = new HybridTimingExtension();
        hybrid_ext->tlm_issue_cycle = getCurrentCycle();
        hybrid_ext->tlm_vc_id = pkt->vc_id;
        hybrid_ext->txn_id = next_txn_id++;
        hybrid_ext->phase = HybridTimingExtension::AdapterPhase::TLM_RECEIVED;
        pkt->payload->set_extension(hybrid_ext);
        
        // 3. 应用VC映射
        int hw_channel = get_hw_channel_for_vc(pkt->vc_id);
        hybrid_ext->hw_channel_id = hw_channel;
        
        // 4. 转换TLM到HW
        auto hw_txn = convert_tlm_to_hw(pkt);
        
        // 5. 发送到HW
        hybrid_ext->phase = HybridTimingExtension::AdapterPhase::HW_CONVERTING;
        hybrid_ext->hw_issue_cycle = getCurrentCycle();
        
        // 6. 记录
        pending_tlm_requests[hybrid_ext->txn_id] = pkt;
        hw_responses[hybrid_ext->txn_id] = hw_txn;
        stats.hw_transactions_issued++;
        
        DPRINTF(ADAPTER, "[%s] TLM request processed: txn_id=%llu vc=%d hw_ch=%d\n",
                name.c_str(), hybrid_ext->txn_id, pkt->vc_id, hw_channel);
        
        return true;
    }
    
    /**
     * 处理下游的响应（如果有的话）
     */
    bool handleDownstreamResponse(Packet* pkt, int src_id,
                                  const std::string& src_label) override {
        // 大多数情况下，Adapter不从下游接收响应
        // 响应来自HW模拟器（通过poll）
        PacketPool::get().release(pkt);
        return true;
    }
    
    /**
     * 主要处理循环：轮询HW完成，转换为TLM响应
     */
    void tick() override {
        // 1. 检查HW完成的事务
        std::vector<uint64_t> completed_txns;
        for (auto& [txn_id, hw_txn] : hw_responses) {
            if (is_hw_transaction_complete(txn_id)) {
                completed_txns.push_back(txn_id);
            }
        }
        
        // 2. 转换完成的事务为TLM响应
        for (uint64_t txn_id : completed_txns) {
            convert_hw_response_to_tlm(txn_id);
            stats.hw_transactions_completed++;
        }
        
        // 3. 尝试发送缓冲的TLM响应
        flush_response_queue();
    }
    
    // ============================================
    // VC 配置
    // ============================================
    
    /**
     * 设置TLM VC到HW通道的映射
     * 
     * 示例:
     *   adapter.set_vc_mapping(0, 0);  // TLM VC 0 -> HW Channel 0
     *   adapter.set_vc_mapping(1, 1);  // TLM VC 1 -> HW Channel 1
     */
    void set_vc_mapping(int tlm_vc_id, int hw_channel_id) {
        vc_to_hw_channel[tlm_vc_id] = hw_channel_id;
        DPRINTF(ADAPTER, "[%s] VC mapping: TLM VC %d -> HW CH %d\n",
                name.c_str(), tlm_vc_id, hw_channel_id);
    }
    
    /**
     * 获取VC对应的HW通道（默认identity映射）
     */
    int get_hw_channel_for_vc(int tlm_vc_id) const {
        auto it = vc_to_hw_channel.find(tlm_vc_id);
        return it != vc_to_hw_channel.end() ? it->second : tlm_vc_id;
    }
    
    // ============================================
    // 统计接口
    // ============================================
    
    const AdapterStats& getStats() const { return stats; }
    void resetStats() { stats.reset(); }
    
    // ============================================
    // 虚拟接口 - 由子类实现
    // ============================================
    
    /**
     * 将TLM请求转换为HW事务
     * 由子类实现，具体转换逻辑
     */
    virtual HWTransactionType convert_tlm_to_hw(Packet* pkt) = 0;
    
    /**
     * 检查特定事务是否在HW侧完成
     */
    virtual bool is_hw_transaction_complete(uint64_t txn_id) const = 0;
    
    /**
     * 从HW侧获取完成事务的响应
     */
    virtual HWTransactionType get_hw_response(uint64_t txn_id) = 0;
    
    // ============================================
    // 内部方法
    // ============================================
    
protected:
    /**
     * 转换HW响应为TLM响应Packet
     */
    void convert_hw_response_to_tlm(uint64_t txn_id) {
        auto req_it = pending_tlm_requests.find(txn_id);
        if (req_it == pending_tlm_requests.end()) return;
        
        Packet* req_pkt = req_it->second;
        auto& hw_resp = hw_responses[txn_id];
        
        // 1. 创建响应Packet
        Packet* resp_pkt = PacketPool::get().acquire();
        resp_pkt->payload = new tlm::tlm_generic_payload();
        resp_pkt->payload->set_response_status(tlm::TLM_OK_RESPONSE);
        
        // 2. 保持VC ID
        resp_pkt->vc_id = req_pkt->vc_id;
        resp_pkt->type = PKT_RESP;
        resp_pkt->original_req = req_pkt;
        resp_pkt->src_cycle = req_pkt->src_cycle;
        resp_pkt->dst_cycle = getCurrentCycle();
        
        // 3. 更新HybridTimingExtension
        auto* hybrid_ext = get_hybrid_timing(req_pkt->payload);
        if (hybrid_ext) {
            hybrid_ext->hw_complete_cycle = getCurrentCycle();
            hybrid_ext->tlm_grant_cycle = getCurrentCycle();
            hybrid_ext->phase = HybridTimingExtension::AdapterPhase::TLM_RESPONSE;
            resp_pkt->payload->set_extension(hybrid_ext);
            
            stats.total_tlm_latency += hybrid_ext->get_tlm_latency();
            stats.total_hw_latency += hybrid_ext->get_hw_latency();
        }
        
        // 4. 加入响应队列
        response_queue.push(resp_pkt);
        
        // 5. 清理
        pending_tlm_requests.erase(txn_id);
        hw_responses.erase(txn_id);
        
        DPRINTF(ADAPTER, "[%s] HW response converted: txn_id=%llu\n",
                name.c_str(), txn_id);
    }
    
    /**
     * 尝试发送缓冲的响应到上游
     */
    void flush_response_queue() {
        while (!response_queue.empty()) {
            Packet* resp = response_queue.front();
            auto& pm = getPortManager();
            
            if (pm.getDownstreamPorts().empty()) {
                // 没有下游端口，丢弃
                PacketPool::get().release(resp);
                response_queue.pop();
                continue;
            }
            
            // 尝试发送到第一个下游端口
            if (pm.getDownstreamPorts()[0]->sendResp(resp)) {
                response_queue.pop();
                stats.tlm_responses_sent++;
                DPRINTF(ADAPTER, "[%s] Response sent\n", name.c_str());
            } else {
                // 无法发送，留在队列中等待下一个周期
                break;
            }
        }
    }
};

#endif // TLM_TO_HW_ADAPTER_BASE_HH
```

---

### 1.3 ReadCmdAdapter 参考实现

**文件**: `include/adapters/read_cmd_adapter.hh`

```cpp
#ifndef READ_CMD_ADAPTER_HH
#define READ_CMD_ADAPTER_HH

#include "tlm_to_hw_adapter_base.hh"
#include <queue>

/**
 * 简单的HW事务类型
 */
struct SimpleHWReadTransaction {
    uint64_t addr = 0;
    uint32_t size = 0;
    bool is_complete = false;
    std::vector<uint8_t> response_data;
};

/**
 * ReadCmdAdapter - 参考实现
 * 
 * 将TLM Read命令转换为HW事务。这是一个功能演示Adapter。
 * 
 * 使用方式:
 *   ReadCmdAdapter adapter("read_adapter", &event_queue);
 *   adapter.set_vc_mapping(0, 0);
 *   // ... 注册端口 ...
 */
class ReadCmdAdapter : public TLMToHWAdapterBase<SimpleHWReadTransaction> {
    
private:
    // 简单的HW模拟：使用固定延迟
    static const int HW_READ_LATENCY = 3;
    
public:
    explicit ReadCmdAdapter(const std::string& name, EventQueue* eq)
        : TLMToHWAdapterBase(name, eq) {}
    
    // ============================================
    // 虚拟接口实现
    // ============================================
    
    /**
     * 将TLM Read请求转换为HW事务
     */
    SimpleHWReadTransaction convert_tlm_to_hw(Packet* pkt) override {
        SimpleHWReadTransaction txn;
        
        if (!pkt || !pkt->payload) {
            return txn;
        }
        
        // 验证是Read命令
        if (pkt->payload->get_command() != tlm::TLM_READ_COMMAND) {
            DPRINTF(ADAPTER, "[%s] Non-read command ignored\n", name.c_str());
            return txn;
        }
        
        // 提取地址和大小
        txn.addr = pkt->payload->get_address();
        txn.size = pkt->payload->get_data_length();
        txn.is_complete = false;
        
        DPRINTF(ADAPTER, "[%s] TLM Read: addr=0x%llx size=%u\n",
                name.c_str(), txn.addr, txn.size);
        
        return txn;
    }
    
    /**
     * 检查HW事务是否完成
     * 这里用简单的周期计数来模拟
     */
    bool is_hw_transaction_complete(uint64_t txn_id) const override {
        auto it = hw_responses.find(txn_id);
        if (it == hw_responses.end()) return false;
        
        // 在实际实现中，这里会检查HW模拟器的状态
        // 这里演示：如果HW响应已标记完成则返回true
        return it->second.is_complete;
    }
    
    /**
     * 获取HW响应
     */
    SimpleHWReadTransaction get_hw_response(uint64_t txn_id) override {
        auto it = hw_responses.find(txn_id);
        if (it == hw_responses.end()) {
            return SimpleHWReadTransaction();
        }
        return it->second;
    }
    
    /**
     * 重写tick以模拟HW完成
     */
    void tick() override {
        // 模拟HW延迟：循环检查是否应该完成
        uint64_t current_cycle = getCurrentCycle();
        for (auto& [txn_id, txn] : hw_responses) {
            if (!txn.is_complete) {
                // 简单延迟模型：发送后3个周期完成
                auto req_it = pending_tlm_requests.find(txn_id);
                if (req_it != pending_tlm_requests.end()) {
                    auto* req_pkt = req_it->second;
                    auto* hybrid = get_hybrid_timing(req_pkt->payload);
                    if (hybrid) {
                        if (current_cycle >= hybrid->hw_issue_cycle + HW_READ_LATENCY) {
                            txn.is_complete = true;
                            txn.response_data.resize(txn.size, 0xAB);  // 模拟数据
                        }
                    }
                }
            }
        }
        
        // 调用基类的处理
        TLMToHWAdapterBase::tick();
    }
};

#endif // READ_CMD_ADAPTER_HH
```

---

## 第二部分: 测试框架

### 2.1 Adapter基本功能测试

**文件**: `test/test_hybrid_adapter_basic.cc`

```cpp
#include "catch_amalgamated.hpp"
#include "adapters/read_cmd_adapter.hh"
#include "core/packet_pool.hh"

class MockMemoryModule : public SimObject {
public:
    std::vector<Packet*> received_requests;
    
    explicit MockMemoryModule(const std::string& name, EventQueue* eq)
        : SimObject(name, eq) {}
    
    bool handleUpstreamRequest(Packet* pkt, int src_id, 
                               const std::string& src_label) override {
        received_requests.push_back(pkt);
        return true;
    }
    
    void tick() override {}
};

TEST_CASE("HybridAdapter Basic Functionality", "[hybrid][adapter]") {
    
    SECTION("Adapter creation and initialization") {
        EventQueue eq;
        ReadCmdAdapter adapter("read_adapter", &eq);
        
        REQUIRE(adapter.getName() == "read_adapter");
        REQUIRE(adapter.getStats().tlm_requests_received == 0);
    }
    
    SECTION("VC mapping configuration") {
        EventQueue eq;
        ReadCmdAdapter adapter("read_adapter", &eq);
        
        // 设置VC映射
        adapter.set_vc_mapping(0, 0);
        adapter.set_vc_mapping(1, 1);
        adapter.set_vc_mapping(2, 0);  // 多个VC映射到同一通道
        
        // 验证映射
        REQUIRE(adapter.get_hw_channel_for_vc(0) == 0);
        REQUIRE(adapter.get_hw_channel_for_vc(1) == 1);
        REQUIRE(adapter.get_hw_channel_for_vc(2) == 0);
        
        // 默认映射（未显式设置）
        REQUIRE(adapter.get_hw_channel_for_vc(3) == 3);
    }
    
    SECTION("HybridTimingExtension attachment") {
        EventQueue eq;
        ReadCmdAdapter adapter("read_adapter", &eq);
        
        // 创建TLM请求
        auto* payload = new tlm::tlm_generic_payload();
        payload->set_command(tlm::TLM_READ_COMMAND);
        payload->set_address(0x1000);
        payload->set_data_length(64);
        payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        Packet* pkt = PacketPool::get().acquire();
        pkt->payload = payload;
        pkt->type = PKT_REQ;
        pkt->vc_id = 0;
        pkt->src_cycle = 0;
        
        // 处理请求
        eq.schedule(new LambdaEvent([&]() {
            adapter.handleUpstreamRequest(pkt, 0, "test");
        }), 0);
        
        eq.run(1);
        
        // 验证Extension被附加
        auto* hybrid = get_hybrid_timing(pkt->payload);
        REQUIRE(hybrid != nullptr);
        REQUIRE(hybrid->tlm_issue_cycle == 0);
        REQUIRE(hybrid->tlm_vc_id == 0);
        REQUIRE(hybrid->phase == HybridTimingExtension::AdapterPhase::TLM_RECEIVED);
    }
}

TEST_CASE("HybridAdapter Statistics", "[hybrid][adapter][stats]") {
    EventQueue eq;
    ReadCmdAdapter adapter("read_adapter", &eq);
    
    SECTION("Request counting") {
        // 创建多个请求
        for (int i = 0; i < 3; ++i) {
            auto* payload = new tlm::tlm_generic_payload();
            payload->set_command(tlm::TLM_READ_COMMAND);
            payload->set_address(0x1000 + i * 64);
            payload->set_data_length(64);
            payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            
            Packet* pkt = PacketPool::get().acquire();
            pkt->payload = payload;
            pkt->type = PKT_REQ;
            pkt->vc_id = i % 2;
            
            adapter.handleUpstreamRequest(pkt, 0, "test");
        }
        
        auto& stats = adapter.getStats();
        REQUIRE(stats.tlm_requests_received == 3);
        REQUIRE(stats.tlm_requests_dropped == 0);
    }
}
```

---

### 2.2 VC映射测试

**文件**: `test/test_vc_mapping.cc`

```cpp
#include "catch_amalgamated.hpp"
#include "adapters/read_cmd_adapter.hh"
#include "core/packet.hh"

TEST_CASE("VC Mapping Verification", "[vc][mapping]") {
    EventQueue eq;
    ReadCmdAdapter adapter("vc_test", &eq);
    
    SECTION("Multiple VC with different mappings") {
        // 配置映射：VC0->Channel0, VC1->Channel1, VC2->Channel0（复用）
        adapter.set_vc_mapping(0, 0);
        adapter.set_vc_mapping(1, 1);
        adapter.set_vc_mapping(2, 0);
        
        // 发送来自不同VC的请求
        for (int vc = 0; vc < 3; ++vc) {
            auto* payload = new tlm::tlm_generic_payload();
            payload->set_command(tlm::TLM_READ_COMMAND);
            payload->set_address(0x1000 + vc * 64);
            payload->set_data_length(64);
            payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            
            Packet* pkt = PacketPool::get().acquire();
            pkt->payload = payload;
            pkt->type = PKT_REQ;
            pkt->vc_id = vc;
            
            adapter.handleUpstreamRequest(pkt, 0, "test");
            
            // 验证Extension中记录了正确的VC和通道
            auto* hybrid = get_hybrid_timing(pkt->payload);
            REQUIRE(hybrid->tlm_vc_id == vc);
            REQUIRE(hybrid->hw_channel_id == adapter.get_hw_channel_for_vc(vc));
        }
    }
    
    SECTION("VC preservation in responses") {
        adapter.set_vc_mapping(0, 0);
        adapter.set_vc_mapping(1, 1);
        
        // 发送VC1请求
        auto* payload = new tlm::tlm_generic_payload();
        payload->set_command(tlm::TLM_READ_COMMAND);
        payload->set_address(0x2000);
        payload->set_data_length(64);
        payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        Packet* req_pkt = PacketPool::get().acquire();
        req_pkt->payload = payload;
        req_pkt->type = PKT_REQ;
        req_pkt->vc_id = 1;  // 重要：使用VC 1
        
        REQUIRE(adapter.handleUpstreamRequest(req_pkt, 0, "test") == true);
        
        auto* hybrid = get_hybrid_timing(req_pkt->payload);
        REQUIRE(hybrid->tlm_vc_id == 1);
        REQUIRE(hybrid->hw_channel_id == 1);
    }
}
```

---

## 第三部分: 改进项代码清单

### 3.1 需要添加到现有代码的宏定义

在 `include/core/sim_core.hh` 中添加:

```cpp
#ifdef DEBUG_PRINT
#define DPRINTF(name, fmt, ...) \
    do { \
        printf("[%s] ", #name); \
        printf(fmt, ##__VA_ARGS__); \
    } while(0)
#else
#define DPRINTF(name, fmt, ...) do {} while(0)
#endif

// 添加ADAPTER分类
#ifdef DEBUG_PRINT
#define ADAPTER_DEBUG 1
#endif
```

### 3.2 ModuleFactory 中的Adapter注册

**建议添加到** `src/core/module_factory.cc`:

```cpp
// 在文件末尾添加默认Adapter注册
void register_default_adapters() {
    ModuleFactory::registerObject<ReadCmdAdapter>("ReadCmdAdapter");
    // 稍后会添加更多Adapter
}

// 在instantiateAll开始调用
void ModuleFactory::instantiateAll(const json& config) {
    register_default_adapters();  // 注册默认adapters
    
    // ... 现有代码 ...
}
```

---

## 第四部分: Phase A 实施检查清单

### 4.1 文件创建清单

- [ ] 创建 `include/ext/hybrid_timing_extension.hh`
- [ ] 创建 `include/adapters/tlm_to_hw_adapter_base.hh`
- [ ] 创建 `include/adapters/read_cmd_adapter.hh`
- [ ] 创建 `test/test_hybrid_adapter_basic.cc`
- [ ] 创建 `test/test_vc_mapping.cc`

### 4.2 代码完整性检查

- [ ] HybridTimingExtension编译无错误和警告
- [ ] Adapter基类所有虚拟函数有实现或被标记为=0
- [ ] ReadCmdAdapter完全实现TLMToHWAdapterBase接口
- [ ] VC映射逻辑在Adapter和Extension中一致
- [ ] 所有Extension clone和copy_from方法正确实现

### 4.3 测试覆盖

- [ ] 基本功能测试通过（创建、初始化、处理请求）
- [ ] VC映射测试通过
- [ ] 统计信息收集正确
- [ ] Extension附加和克隆正确
- [ ] 向后兼容性测试（现有模块不受影响）

### 4.4 文档和注释

- [ ] 所有公共方法有Doxygen注释
- [ ] VC映射说明文档清晰
- [ ] HybridTimingExtension使用指南完整
- [ ] 示例代码注释清晰

---

## 预期成果

### Phase A 结束时

✅ **可交付物**:
1. HybridTimingExtension（生产级质量）
2. TLMToHWAdapterBase框架（生产级质量）
3. ReadCmdAdapter参考实现（演示级质量）
4. 完整的单元测试套件
5. 详细的API文档

✅ **验证条件**:
- 所有代码通过编译和Lint检查
- 单元测试覆盖率 >90%
- 端到端测试通过
- 100%向后兼容

✅ **性能目标**:
- Adapter开销 <2个周期
- VC处理无额外延迟
- Extension克隆 <1μs

---

