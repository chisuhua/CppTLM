# src/tlm/ — TLM 模块实现

**域**: RouterTLM + NICTLM + LinkTLM（3 文件，797 行）
**作用**: TLM 2.0 周期精确路由器、NIC、链路模块的具体实现

## 文件

| 文件 | 行数 | 模块 | 作用 |
|------|------|------|------|
| `router_tlm.cc` | 482 | RouterTLM | 路由算法（XY/先进先出）、VC 仲裁、流水线 |
| `nic_tlm.cc` | 191 | NICTLM | 网卡仿真：包组装/拆解、流量控制 |
| `link_tlm.cc` | 124 | LinkTLM | 物理链路：延迟建模、Credit-based Flow Control |

## RouterTLM 架构

- **端口**: N×(req_in/resp_out/req_out/resp_in) 多组 ChStreamPort
- **路由算法**: `route_XY()` (X first) 或 `route_fIFO()` (先进先出)
- **VC 映射**: `vc_map_[dest_port][vc_id]` 表
- **流水线**: `pipeline_stages_` 计数器延迟

## NICTLM 架构

- **包组装**: `packetizer_` 将大包拆分为 flits
- **流量控制**: 发送/接收窗口，跟踪 in-flight 包

## LinkTLM 架构

- **延迟队列**: `delay_queue_` 按周期延迟 flit
- **Credit 返回**: `credit_queue_` 反向传播
- **`latency_` 参数**: 构造时注入链路延迟周期数

## 约定

- **tick() 模式**: 每个模块实现自己的 tick()，在 ModuleFactory::startAllTicks() 中被调用
- **端口绑定**: 通过 StreamAdapter 的 `bind()` 连接 ChStreamPort 对
- **Bundle 类型**: 使用 `include/bundles/cache_bundles_tlm.hh` 或 `noc_bundles_tlm.hh`

## 注意事项

- Phase 7 新增（RouterTLM/NICTLM/LinkTLM）
- 头文件在 `include/tlm/`（router_tlm.hh, nic_tlm.hh, link_tlm.hh）
- 与 SystemC TLM 2.0 无关，是内部 TLM（Transaction Level Modeling）实现