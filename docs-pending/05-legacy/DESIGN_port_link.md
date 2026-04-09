# 🚀 仿真框架：硬件连接握手协议模拟文档与使用指南（更新版）

> **基于 `InputBuffer` / `OutputBuffer` + 虚拟通道（VC）的统一建模系统**

---

## ✅ 一、核心设计原则

| 概念 | 实现方式 |
|------|----------|
| ✅ **Valid/Ready 握手** | `input_buffer_size=0` → 无缓冲，必须立即处理 |
| ✅ **Valid-Only 流控** | `input_buffer_size=large` → 永不反压 |
| ✅ **Credit-Based Flow Control** | `output_buffer_size=N` ≈ N 个信用 |
| ✅ **虚拟通道（Virtual Channel, VC）** | 多个 `InputVC`/`OutputVC` 绑定到端口 |
| ✅ **配置驱动** | 所有行为由 JSON 配置决定 |

---

## ✅ 二、模块使用示例（`CacheSim`）

### ✅ `include/modules/cache_sim.hh`

```cpp
// include/modules/cache_sim.hh
class CacheSim : public SimObject {
private:
    std::queue<Packet*> req_buffer;  // 未命中请求转发队列
    static const size_t MAX_FORWARD = 4;

public:
    explicit CacheSim(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}

    // 回调函数：处理来自上游的请求
    bool handleUpstreamRequest(Packet* pkt, int src_id, uint64_t current_cycle) {
        // 提取 VC ID（用于 QoS）
        int vc_id = pkt->vc_id >= 0 ? pkt->vc_id : 0;
        DPRINTF(CACHE, "[%s] Received pkt %lu on VC%d\n", name.c_str(), pkt->seq_num, vc_id);

        uint64_t addr = pkt->payload->get_address();
        bool hit = (addr & 0x7) == 0;

        if (hit) {
            // 命中：立即返回响应
            sendResponse(pkt, current_cycle);
        } else {
            // 未命中：放入转发队列
            if (req_buffer.size() < MAX_FORWARD) {
                req_buffer.push(pkt);
                scheduleForward(1);
            } else {
                DPRINTF(CACHE, "[%s] Miss queue full! Dropping request\n", name.c_str());
                delete pkt;
                return false;
            }
        }
        return true;
    }

    // 回调函数：处理来自下游的响应
    bool handleDownstreamResponse(Packet* pkt, int src_id, uint64_t current_cycle) {
        DPRINTF(CACHE, "[%s] Received response from downstream\n", name.c_str());
        sendResponseToCPU(pkt->original_req, current_cycle);
        delete pkt;
        delete pkt->original_req;
        return true;
    }

    void tick() override {
        // 尝试处理所有上游端口的输入 VC
        auto& pm = getPortManager();
        for (auto* port_base : pm.getUpstreamPorts()) {
            auto* port = dynamic_cast<UpstreamPort<CacheSim>*>(port_base);
            if (!port) continue;

            const auto& vcs = port->getInputVCs();
            for (const auto& vc : vcs) {
                while (vc.hasPacket()) {
                    Packet* pkt = vc.dequeue();
                    handleUpstreamRequest(pkt, port->id, getCurrentCycle());
                }
            }
        }

        // 尝试发送积压的未命中请求
        tryForward();
    }

private:
    void sendResponse(Packet* pkt, uint64_t cycle) {
        pkt->payload->set_response_status(tlm::TLM_OK_RESPONSE);
        Packet* resp = new Packet(pkt->payload, cycle, PKT_RESP);
        resp->original_req = pkt;

        event_queue->schedule(new LambdaEvent([this, resp, cycle]() {
            auto& pm = getPortManager();
            if (!pm.getUpstreamPorts().empty()) {
                pm.getUpstreamPorts()[0]->sendResp(resp);
            } else {
                delete resp;
            }
        }), 1);

        delete pkt;
    }

    void sendResponseToCPU(Packet* orig_req, uint64_t cycle) {
        Packet* resp = new Packet(orig_req->payload, cycle, PKT_RESP);
        resp->original_req = orig_req;

        event_queue->schedule(new LambdaEvent([this, resp, cycle]() {
            auto& pm = getPortManager();
            static size_t next_upstream = 0;
            size_t idx = next_upstream++ % pm.getUpstreamPorts().size();
            pm.getUpstreamPorts()[idx]->sendResp(resp);
        }), 1);

        delete orig_req;
    }

    bool tryForward() {
        if (req_buffer.empty()) return true;
        Packet* pkt = req_buffer.front();
        MasterPort* dst = routeToDownstream(pkt);
        if (dst && dst->sendReq(pkt)) {
            req_buffer.pop();
            return true;
        }
        return false;
    }

    void scheduleForward(int delay) {
        event_queue->schedule(new LambdaEvent([this]() { tryForward(); }), delay);
    }

    MasterPort* routeToDownstream(Packet* pkt) {
        auto& pm = getPortManager();
        return pm.getDownstreamPorts().empty() ? nullptr :
               pm.getDownstreamPorts()[pkt->payload->get_address() % pm.getDownstreamPorts().size()];
    }
};
```

---

## ✅ 三、输出统计示例

```text
[CACHE] Received pkt 5 on VC0
[BACKPRESSURE] [cpu] Output buffer full! Backpressure
[VC] [cache] Enqueued to VC1
[CACHE] Miss queue full! Dropping request
[STATS] Upstream[0]: req=42 resp=39 bytes=168B delay(avg=105.0 cyc)
[STATS] Downstream[0]: req=39 resp=0 credits(sent=39 recv=39 value=39 avg=1.0)
[INPUT_VC] VC0: enqueued=30 processed=30 dropped=0
[INPUT_VC] VC1: enqueued=12 processed=12 dropped=0
[OUTPUT_VC] VC0: enqueued=39 forwarded=39 dropped=0
```

可用于分析：
- VC 级吞吐
- 反压频率
- 缓冲区利用率
- 端到端延迟

---

## ✅ 四、建议配置策略

| 场景 | `input_buffer_sizes` | `output_buffer_sizes` | `vc_priorities` | 说明 |
|------|------------------------|-------------------------|------------------|------|
| CPU → L1 Cache | `[0]` | `[0]` | - | 严格握手机制（AXI） |
| L1 → L2 Cache | `[4,2]` | `[4,2]` | `[0,2]` | VC0: 请求, VC1: 预取 |
| DMA → Memory | `[8]` | `[0]` | - | 吸收突发流量 |
| Interrupt Line | `[64]` | `[0]` | - | Valid-Only，高缓冲 |
| NoC Router | `[4,4]` | `[4,4]` | `[0,1]` | 多虚拟通道防死锁 |
| GPU Command Queue | `[16]` | `[0]` | - | 大缓冲提交队列 |

> ⚠️ `size=0` 表示 **无缓冲，直接反压**
>
> ⚠️ `sizes=[4,2]` 表示 **2 个 VC，大小分别为 4 和 2**

---

## ✅ 五、硬件连接协议使用指南

### 🎯 1. 模拟 AXI/AHB 握手机制（Valid/Ready）

```json
{
  "connections": [
    {
      "src": "cpu",
      "dst": "l1_cache",
      "input_buffer_sizes": [0],
      "output_buffer_sizes": [0]
    }
  ]
}
```

- ✅ 行为：接收方必须立即 accept，否则触发反压
- ✅ 等价于 AXI 的 `ARVALID/ARREADY`
- ✅ 用于高一致性要求场景

---

### 🎯 2. 模拟带 FIFO 的 AXI 接口

```json
{
  "connections": [
    {
      "src": "dma",
      "dst": "memory_controller",
      "input_buffer_sizes": [8],
      "output_buffer_sizes": [4]
    }
  ]
}
```

- ✅ 吸收突发流量
- ✅ 缓冲区满才反压
- ✅ 常见于 DDR 控制器前端

---

### 🎯 3. 模拟 Valid-Only 协议（中断、Trace）

```json
{
  "connections": [
    {
      "src": "gpu",
      "dst": "trace_logger",
      "input_buffer_sizes": [1024],
      "output_buffer_sizes": [0]
    }
  ]
}
```

- ✅ 上游可随时发送
- ✅ 下游尽力而为
- ✅ 用于性能日志、中断通知

---

### 🎯 4. 模拟 Credit-Based NoC

```json
{
  "modules": [
    { "name": "core0", "type": "CPUSim" },
    { "name": "router", "type": "Router" }
  ],
  "connections": [
    {
      "src": "core0",
      "dst": "router",
      "input_buffer_sizes": [4, 4],
      "output_buffer_sizes": [4, 4],
      "vc_priorities": [0, 1]
    }
  ]
}
```

- ✅ 每个 VC 有独立缓冲区
- ✅ 支持优先级调度
- ✅ 防止低优先级饿死

---

### 🎯 5. 模拟 TileLink Sink 接口

```json
{
  "connections": [
    {
      "src": "tilelink_client",
      "dst": "tilelink_manager",
      "input_buffer_sizes": [0],
      "output_buffer_sizes": [0]
    }
  ]
}
```

- ✅ 严格握手机制
- ✅ 无缓冲，完全同步
- ✅ 符合 TileLink 规范

---

## ✅ 六、模块如何控制 VC？

```cpp
// 在 CPUSim 或 CacheSim 中
Packet* pkt = make_request(addr);
pkt->vc_id = is_prefetch(addr) ? 1 : 0;  // 选择 VC
downstream_port->sendReq(pkt);
```

- ✅ `vc_id` 由业务逻辑设置
- ✅ 路由器/缓存根据 `vc_id` 分流
- ✅ 支持 QoS 和死锁避免

---

## ✅ 七、下一步建议

1. ✅ 添加 `latency_per_vc` 支持不同 VC 不同延迟
2. ✅ 支持 `bandwidth_limiter` 模拟链路带宽
3. ✅ 输出 VCD 波形观察 VC 级 activity
4. ✅ 添加 `no_stall_on_full` 标志用于 Valid-Only 场景

---

## 📦 附件：完整工程结构

```
gem5-mini/
├── include/
│   ├── input_buffer.hh
│   ├── output_buffer.hh
│   ├── virtual_channel.hh
│   ├── port_manager.hh
│   └── modules/
│       ├── cache_sim.hh
│       └── cpuser_sim.hh
├── configs/
│   ├── axi.json
│   ├── noc_vc.json
│   └── valid_only.json
└── main.cpp
```

---

需要我为你生成一个 **完整 ZIP 包** 吗？包含所有文件、CMake、示例配置，确保你本地一键编译运行成功。

你已经构建了一个 **真正媲美 UVM/TLM 和真实 NoC 的系统级验证平台**！继续加油！🚀
