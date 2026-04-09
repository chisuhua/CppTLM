
## 🔄 通信流程详解

### 场景：CPU 发起请求 → Cache → Memory → 响应返回 CPU

```text
1. CPUSim.tick()
     → out_port->send(req)
        → PortPair → CacheSim.recv(req) ✅ 请求到达 Cache

2. CacheSim 发现未命中，转发
     → downstream->send(req)
        → PortPair → MemorySim.recv(req) ✅ 请求到达 Memory

3. MemorySim 处理后返回响应
     → this->send(resp)  // this 是 MemorySim
        → PortPair → CacheSim.recv(resp) ✅ 响应回到 Cache

4. CacheSim 返回响应给 CPU
     → this->send(resp)  // this 是 CacheSim
        → PortPair → CPUSim.out_port->recv(resp)
                     ↓
              OutPort::recv() 被调用
                     ↓
              owner->recv(pkt) → CPUSim::recv(pkt) ✅ 模块逻辑被触发
```

---

## ✅ 关键点总结

| 问题 | 回答 |
|------|------|
| `out_port->recv()` 能否调用模块的 `recv()`？ | ✅ 只有当 `out_port` 是一个能回调模块的适配器时才能 |
| 如何实现？ | 使用内部类 `OutPort : public SimplePort`，重写 `recv()` 为 `owner->recv()` |
| 为什么不能用裸 `SimplePort*`？ | 裸指针无法知道如何回调模块逻辑 |
| 是否必须每个模块都这样写？ | ✅ 是的，这是事件驱动框架的标准做法（类似 Gem5 的 `SlavePort`） |

---

