class MyModule : public SimObject {
public:
    MyModule(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}

    bool handleUpstreamRequest(Packet* pkt, int src_id) { ... }
    bool handleDownstreamResponse(Packet* pkt, int src_id) { ... }

    void tick() override { ... }
};


## 发送流程
```
cpu_port->sendReq(pkt)
    ↓
MasterPort::send(pkt)
    ↓ 统计 req_count, byte_count
    ↓
SimplePort::send() → PortPair → 
    ↓
cache_upstream_port->recv(pkt)
    ↓ SlavePort::recv() → 统计 + 调用 recvReq()
    ↓
cache_upstream_port->recvReq(pkt)
    ↓
CacheSim::handleUpstreamRequest(pkt, id)
```

## 响应流程
```
mem_port->sendResp(pkt)
    ↓
SlavePort::send(pkt)
    ↓
SimplePort::send() → PortPair → 
    ↓
cpu_downstream_port->recv(pkt)
    ↓ MasterPort::recv() → 统计 + 调用 recvResp()
    ↓
cpu_downstream_port->recvResp(pkt)
    ↓
CPUSim::handleDownstreamResponse(pkt, id)
```
