# include/framework/ — 流适配器层

**域**: ChStream↔TLM 转换层 + 多端口适配器 + 追踪器（5 文件）
**作用**: 将 JSON 配置的逻辑连接转换为实际 ChStreamPort 之间的数据转发

## 文件

| 文件 | 作用 |
|------|------|
| `stream_adapter.hh` | StreamAdapterBase + 单端口 StandaloneStreamAdapter（CacheTLM/MemoryTLM 使用） |
| `multi_port_stream_adapter.hh` | `template<ModuleT, ReqBundleT, RespBundleT, N>` 多端口适配器（CrossbarTLM 使用） |
| `debug_tracker.hh` | 调试追踪器（事务日志） |
| `error_category.hh` | 框架级错误分类 |
| `transaction_tracker.hh` | 事务生命周期追踪 |

## StreamAdapterBase 接口

```cpp
class StreamAdapterBase {
    virtual ~StreamAdapterBase() = default;
    virtual void tick() = 0;              // 每周期调用，转发 req/resp
    virtual void bind(MasterPort*, SlavePort*, MasterPort*, SlavePort*) = 0;
};
```

## MultiPortStreamAdapter 模板

```cpp
template<typename ModuleT, typename ReqBundleT, typename RespBundleT, std::size_t N>
class MultiPortStreamAdapter : public StreamAdapterBase {
    void bind_port_pair(unsigned port_idx, MasterPort*, SlavePort*, ...);
    void tick() override;  // 遍历 N 个端口转发
    void consume(const ChStreamReqBundle& req);  // 从模块消费请求
    void produce(const ChStreamRespBundle& resp); // 向模块产出响应
};
```

## 注册工厂

`ChStreamAdapterFactory` 管理所有适配器注册：
- `registerAdapter<ModuleT, ReqT, RespT>(type)` — 单端口
- `registerMultiPortAdapter<ModuleT, ReqT, RespT, N>(type)` — 多端口
- `isMultiPort(type)` / `getPortCount(type)` — 多端口查询

## 约定

- 适配器生命周期由 `ModuleFactory::stream_adapters_` 管理（`std::vector<unique_ptr>`）
- `tick()` 调用顺序：ModuleFactory::startAllTicks() 遍历所有实例 → 逐模块 tick → 内部调用 adapter.tick()
- Bundle 类型在 `include/bundles/` 定义，适配器通过模板参数绑定
