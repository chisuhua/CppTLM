# include/tlm/ — TLM 2.0 模块（V2.1 新增）

**域**: Transaction Level Modeling 模块（4 文件）
**基类**: 全部从 `ChStreamModuleBase` 派生，非 `SimObject` 直接派生
**注册**: 通过 `REGISTER_CHSTREAM` 宏注册

## 模块列表

| 文件 | 模块 | 端口 | 作用 |
|------|------|------|------|
| `cache_tlm.hh` | CacheTLM | 1 (单端口) | L1 缓存仿真: hit/miss/替换策略 |
| `memory_tlm.hh` | MemoryTLM | 1 (单端口) | 主存仿真: 读/写/延迟模拟 |
| `crossbar_tlm.hh` | CrossbarTLM | 4 (多端口) | 交叉开关路由: 地址路由/VC 映射 |
| `tlm_stub.hh` | TLM Stub | — | USE_SYSTEMC_STUB 时的 TLM 2.0 头文件桩 |

## 端口架构

**单端口模块** (CacheTLM/MemoryTLM):
- 1 组 ChStreamPort (req_in + resp_out + req_out + resp_in)
- 通过 `set_stream_adapter(StandaloneAdapter*)` 注入

**多端口模块** (CrossbarTLM):
- N 组 ChStreamPort (CrossbarTLM 为 4 端口)
- 通过 `set_stream_adapter(MultiPortAdapter*[])` 注入
- `num_ports() const override { return N; }`

## StreamAdapter 流程

```
JSON连接 → ModuleFactory.instantiateAll() → Step 7 StreamAdapter注入
  → ChStreamAdapterFactory::createAdapter(moduleType)
  → 绑定 MasterPort↔SlavePort 端口对
  → module.set_stream_adapter(adapter)
  → tick() 时 adapter 转发 req/resp
```

## 约定

- Bundle 定义见 `include/bundles/cache_bundles_tlm.hh`（轻量级，非 Ch 原生 Bundle）
- 跨模块延迟通过 JSON 配置的 `latency` 字段注入（StreamAdapter 层面实现）
- `USE_SYSTEMC_STUB=ON` 时 tlm_stub.hh 提供 tlm_generic_payload 桩实现
