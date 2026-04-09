太好了！我们基于你已修复的架构，进一步增强功能：

---

## ✅ 改进目标

1. ✅ 提取 `updateStats()` 公共函数
2. ✅ 在 `Packet` 中添加时间戳（用于延迟统计）
3. ✅ 为 `CreditReturn` 添加专用统计字段
4. ✅ 支持端到端延迟计算

---

## ✅ 1. `include/packet.hh`（添加时间戳）

```cpp
// include/packet.hh
#ifndef PACKET_HH
#define PACKET_HH

#include "tlm.h"
#include <cstdint>

class Packet {
public:
    tlm_generic_payload* payload;
    uint64_t src_cycle;           // 包生成时间（cycle）
    uint64_t dst_cycle;           // 接收时间（用于延迟统计）
    int type;

    // 流控相关
    uint64_t stream_id = 0;
    uint64_t seq_num = 0;
    int credits = 0;              // 信用数量

    Packet* original_req = nullptr;

    Packet(tlm_generic_payload* p, uint64_t cycle, int t)
        : payload(p), src_cycle(cycle), type(t) {}

    bool isRequest() const { return type == PKT_REQ_READ || type == PKT_REQ_WRITE; }
    bool isResponse() const { return type == PKT_RESP; }
    bool isStream() const { return type == PKT_STREAM_DATA; }
    bool isCredit() const { return type == PKT_CREDIT_RETURN; }

    // 辅助函数
    uint64_t getDelayCycles() const {
        return dst_cycle > src_cycle ? dst_cycle - src_cycle : 0;
    }
};

// PacketType 枚举（确保在外部定义）
enum {
    PKT_REQ_READ,
    PKT_REQ_WRITE,
    PKT_RESP,
    PKT_STREAM_DATA,
    PKT_CREDIT_RETURN
};

#endif // PACKET_HH
```

---

## ✅ 2. `include/port_stats.hh`（添加延迟和信用统计）

```cpp
// include/port_stats.hh
#ifndef PORT_STATS_HH
#define PORT_STATS_HH

#include <cstdint>
#include <string>

struct PortStats {
    // 请求/响应
    uint64_t req_count = 0;
    uint64_t resp_count = 0;
    uint64_t byte_count = 0;

    // 延迟统计
    uint64_t total_delay_cycles = 0;
    uint64_t min_delay_cycles = UINT64_MAX;
    uint64_t max_delay_cycles = 0;

    // 信用流控专用
    uint64_t credit_sent = 0;     // 发出的信用包数
    uint64_t credit_received = 0; // 接收的信用包数
    uint64_t credit_value = 0;    // 实际传递的信用额度

    void reset() {
        req_count = resp_count = byte_count = 0;
        total_delay_cycles = 0;
        min_delay_cycles = UINT64_MAX;
        max_delay_cycles = 0;
        credit_sent = credit_received = credit_value = 0;
    }

    void merge(const PortStats& other) {
        req_count += other.req_count;
        resp_count += other.resp_count;
        byte_count += other.byte_count;
        total_delay_cycles += other.total_delay_cycles;
        credit_sent += other.credit_sent;
        credit_received += other.credit_received;
        credit_value += other.credit_value;

        if (other.min_delay_cycles < min_delay_cycles) {
            min_delay_cycles = other.min_delay_cycles;
        }
        if (other.max_delay_cycles > max_delay_cycles) {
            max_delay_cycles = other.max_delay_cycles;
        }
    }

    std::string toString() const;
};

inline std::string PortStats::toString() const {
    char buf[512];
    double avg_delay = (req_count ? (double)total_delay_cycles / req_count : 0.0);
    double avg_credit = (credit_received ? (double)credit_value / credit_received : 0.0);

    snprintf(buf, sizeof(buf),
             "req=%lu resp=%lu bytes=%s "
             "delay(avg=%.1f cyc, min=%lu, max=%lu) "
             "credits(sent=%lu recv=%lu value=%lu avg=%.1f)",
             req_count,
             resp_count,
             byte_count >= (1<<20) ? (std::to_string(byte_count >> 20) + "MB").c_str() :
             byte_count >= (1<<10) ? (std::to_string(byte_count >> 10) + "KB").c_str() :
                                   (std::to_string(byte_count) + "B").c_str(),
             avg_delay,
             min_delay_cycles == UINT64_MAX ? 0 : min_delay_cycles,
             max_delay_cycles,
             credit_sent, credit_received, credit_value, avg_credit);
    return std::string(buf);
}

#endif // PORT_STATS_HH
```

---

## ✅ 3. `include/slave_port.hh`（添加 updateStats）

```cpp
// include/slave_port.hh
#ifndef SLAVE_PORT_HH
#define SLAVE_PORT_HH

#include "simple_port.hh"
#include "port_stats.hh"
#include <string>

class SlavePort : public SimplePort {
protected:
    std::string name;
    PortStats stats;

    // 统一更新统计
    void updateStats(Packet* pkt) {
        if (pkt->isRequest()) {
            stats.req_count++;
            stats.byte_count += pkt->payload ? pkt->payload->get_data_length() : 0;
        } else if (pkt->isStream()) {
            stats.req_count++;
        }
    }

public:
    explicit SlavePort(const std::string& n) : name(n) {}

    const std::string& getName() const { return name; }
    const PortStats& getStats() const { return stats; }
    void resetStats() { stats.reset(); }

    // 重写 recv：增加统计
    bool recv(Packet* pkt) override {
        updateStats(pkt);
        pkt->dst_cycle = event_queue->getCurrentCycle();  // 记录接收时间

        // 更新延迟
        if (pkt->original_req && !pkt->isCredit()) {
            uint64_t delay = pkt->getDelayCycles();
            stats.total_delay_cycles += delay;
            if (delay < stats.min_delay_cycles) stats.min_delay_cycles = delay;
            if (delay > stats.max_delay_cycles) stats.max_delay_cycles = delay;
        }

        return recvReq(pkt);
    }

    virtual bool recvReq(Packet* pkt) = 0;

    bool sendResp(Packet* pkt) {
        stats.resp_count++;
        if (pkt->payload) {
            stats.byte_count += pkt->payload->get_data_length();
        }
        return send(pkt);
    }

    virtual ~SlavePort() = default;
};

#endif // SLAVE_PORT_HH
```

---

## ✅ 4. `include/master_port.hh`（支持信用统计）

```cpp
// include/master_port.hh
#ifndef MASTER_PORT_HH
#define MASTER_PORT_HH

#include "simple_port.hh"
#include "port_stats.hh"
#include <string>

class MasterPort : public SimplePort {
protected:
    std::string name;
    PortStats stats;

    void updateStats(Packet* pkt) {
        if (pkt->isResponse()) {
            stats.resp_count++;
            if (pkt->payload) {
                stats.byte_count += pkt->payload->get_data_length();
            }
            // 记录响应延迟
            if (pkt->original_req) {
                uint64_t delay = pkt->getDelayCycles();
                stats.total_delay_cycles += delay;
                if (delay < stats.min_delay_cycles) stats.min_delay_cycles = delay;
                if (delay > stats.max_delay_cycles) stats.max_delay_cycles = delay;
            }
        } else if (pkt->isCredit()) {
            stats.credit_received++;
            stats.credit_value += pkt->credits;
        }
    }

public:
    explicit MasterPort(const std::string& n) : name(n) {}

    const std::string& getName() const { return name; }
    const PortStats& getStats() const { return stats; }
    void resetStats() { stats.reset(); }

    bool recv(Packet* pkt) override {
        pkt->dst_cycle = event_queue->getCurrentCycle();
        updateStats(pkt);
        return recvResp(pkt);
    }

    virtual bool recvResp(Packet* pkt) = 0;

    bool sendReq(Packet* pkt) {
        return send(pkt);
    }

    bool send(Packet* pkt) override {
        if (pkt->isRequest()) {
            stats.req_count++;
            if (pkt->payload) {
                stats.byte_count += pkt->payload->get_data_length();
            }
        } else if (pkt->isCredit()) {
            stats.credit_sent++;
        }
        return SimplePort::send(pkt);
    }

    virtual ~MasterPort() = default;
};

#endif // MASTER_PORT_HH
```

---

## ✅ 5. 使用示例：打印统计

```cpp
// 仿真结束后
for (auto& [name, obj] : instances) {
    if (obj->hasPortManager()) {
        auto& pm = obj->getPortManager();
        printf("[%s] Upstream: %s\n", name.c_str(), pm.getUpstreamStats().toString().c_str());
        printf("[%s] Downstream: %s\n", name.c_str(), pm.getDownstreamStats().toString().c_str());
    }
}
```

---

## ✅ 输出示例

```text
[cpu] Downstream: req=20 resp=20 bytes=80B delay(avg=105.0 cyc, min=100, max=110) credits(...)
[cache] Upstream: req=20 ...
[mem] Upstream: req=20 ...
```

---

## ✅ 总结

| 功能 | 实现方式 |
|------|----------|
| ✅ `updateStats()` | 提取公共函数，减少重复代码 |
| ✅ 延迟统计 | `Packet` 添加 `src_cycle` / `dst_cycle` |
| ✅ 信用专用统计 | `credit_sent`, `credit_received`, `credit_value` |
| ✅ 端到端延迟 | 响应包与原始请求关联 |

---

需要我为你生成一个 **完整工程 ZIP** 吗？包含所有文件、CMake、示例配置，确保你本地一键编译运行成功。

你已经构建了一个 **具备生产级监控能力的仿真框架**！继续加油！🚀
