# CrossbarTLM 设计文档

> **版本**: 1.0  
> **日期**: 2026-04-13  
> **Phase**: 4  
> **前置**: Phase 0-3 完成（CacheTLM, MemoryTLM 已验证）

## 1. 设计目标

实现支持 **4 请求端口 × 4 响应端口**的 Crossbar TLM 模块，使用 ChStream 内部通信。

## 2. 端口拓扑

```
CrossbarTLM (4 端口):

请求方向 (Request Path):
┌─────────────────────────────────────────┐
│  req_in[0] ──┐                          │
│  req_in[1] ──┼─► 路由矩阵 ─► resp_out[0]│
│  req_in[2] ──┤            ─► resp_out[1]│
│  req_in[3] ──┘            ─► resp_out[2]│
│                             ─► resp_out[3]│
└─────────────────────────────────────────┘

响应方向 (Response Path):
┌─────────────────────────────────────────┐
│  resp_in[0] ──┐                         │
│  resp_in[1] ──┼─► 路由矩阵 ─► req_out[0]│
│  resp_in[2] ──┤            ─► req_out[1]│
│  resp_in[3] ──┘            ─► req_out[2]│
│                             ─► req_out[3]│
└─────────────────────────────────────────┘
```

## 3. 路由策略

**Phase 4 采用简单地址位提取路由**：

```cpp
// 目标端口 = (address >> PORT_SHIFT) & PORT_MASK
constexpr unsigned PORT_SHIFT = 12;  // 0x1000 边界
constexpr unsigned PORT_MASK = 0x3;  // 4 端口
int dst_port = (addr >> PORT_SHIFT) & PORT_MASK;
```

**地址空间分配**：
- Port 0: `0x0000 - 0x0FFF`
- Port 1: `0x1000 - 0x1FFF`
- Port 2: `0x2000 - 0x2FFF`
- Port 3: `0x3000 - 0x3FFF`

## 4. 数据结构

```cpp
class CrossbarTLM : public ChStreamModuleBase {
private:
    static constexpr unsigned NUM_PORTS = 4;
    
    // 请求方向: req_in[i] → resp_out[route(addr)]
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>  req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out[NUM_PORTS];
    
    // 响应方向: resp_in[i] → req_out[route(addr)] (简化：直接回环)
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle> resp_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> req_out[NUM_PORTS];
    
    // StreamAdapter 注入
    cpptlm::StreamAdapterBase* adapter[NUM_PORTS] = {nullptr};
    
public:
    void tick() override {
        for (int i = 0; i < NUM_PORTS; i++) {
            if (req_in[i].valid() && req_in[i].ready()) {
                uint64_t addr = req_in[i].data().address.read();
                int dst = route_address(addr);
                // 转发到 resp_out[dst]
                bundles::CacheRespBundle resp = req_in[i].data();
                resp_out[dst].write(resp);
                req_in[i].consume();
            }
        }
    }
    
private:
    int route_address(uint64_t addr) const {
        return (addr >> 12) & 0x3;
    }
};
```

## 5. 接口设计

### 外部可见接口

- `req_in(port_idx)` — 请求输入端口 (0-3)
- `resp_out(port_idx)` — 响应输出端口 (0-3)
- `resp_in(port_idx)` — 响应输入端口 (0-3)
- `req_out(port_idx)` — 请求输出端口 (0-3)

### ModuleFactory 注入接口

- `set_stream_adapter(port_idx, adapter)` — 为指定端口注入适配器

## 6. JSON Schema

```json
{
  "modules": [
    {
      "name": "xbar",
      "type": "CrossbarTLM",
      "num_ports": 4
    }
  ],
  "connections": [
    {"src": "cpu0", "dst": "xbar", "latency": 1},
    {"src": "xbar", "dst": "cache0", "latency": 1}
  ]
}
```

## 7. 测试策略

| 测试类别 | 用例数 | 覆盖内容 |
|---------|:-----:|---------|
| 单端口路由 | 4 | 每端口单独请求，验证路由矩阵 |
| 多端口并发 | 4 | 4 端口同时请求，验证并行处理 |
| 边界地址 | 4 | 0x0FFF→0x1000 边界跨端口 |
| 复位行为 | 1 | 复位后所有输出 invalid |
| 握手协议 | 3 | valid/ready/consume 完整流程 |
| 集成测试 | 3 | CacheTLM → CrossbarTLM → MemoryTLM 端到端 |

**总计**: 15+ 核心用例

## 8. Phase 4 验收标准

- [ ] `include/tlm/crossbar_tlm.hh` 编译通过
- [ ] `include/tlm/crossbar_tlm_adapter.hh` 多端口 StreamAdapter
- [ ] 15+ 测试用例全部通过
- [ ] JSON 配置加载 + 运行
- [ ] 与 CacheTLM/MemoryTLM 混合连接验证

## 9. 后续演进 (Phase 5+)

| Phase | 扩展内容 |
|-------|---------|
| Phase 4b | 可配置路由矩阵（JSON 驱动） |
| Phase 5 | 支持 8 端口 / 16 端口扩展 |
| Phase 5 | 仲裁策略（Round-Robin, Priority） |
| Phase 6 | 与 Legacy Crossbar 行为对标 |
