# include/bundles/ — Bundle 类型定义

**域**: ChStream 内部消息格式（4 文件）
**作用**: 定义 ReqBundle/RespBundle 类型，用于 StreamAdapter 模板参数

## 文件

| 文件 | 作用 |
|------|------|
| `cache_bundles_tlm.hh` | CacheTLM/MemoryTLM 使用的轻量级 Bundle |
| `noc_bundles_tlm.hh` | NoC 相关 Bundle（路由/VC/优先级） |
| `bundle_serialization.hh` | Bundle 序列化工具 |
| `cpphdl_types.hh` | 公共类型定义（地址/数据/VC_ID） |

## 关键类型

```cpp
// Cache Bundle（轻量级，非 Ch 原生 Bundle）
struct CacheReqBundle {
    uint64_t addr;
    uint32_t data;
    uint8_t vc_id;
    // ...
};

struct CacheRespBundle {
    uint64_t addr;
    uint32_t data;
    bool hit;
    // ...
};

// NoC Bundle（含路由信息）
struct NocReqBundle {
    uint64_t addr;
    uint8_t dest_x, dest_y;
    uint8_t vc_id;
    uint8_t priority;
};
```

## Bundle 在 StreamAdapter 中的作用

1. **模板参数**: `MultiPortStreamAdapter<ModuleT, ReqBundleT, RespBundleT, N>`
2. **类型安全**: Bundle 作为 Req/Resp 通道的载体
3. **跨模块传递**: 通过 `consume(req)` / `produce(resp)` 接口

## 约定

- Bundle 定义在 `include/bundles/`，被 `include/framework/` 的 StreamAdapter 引用
- 轻量级设计：与 Ch 原生 Bundle 不同，是 CppTLM 内部格式
- 每个 TLM 模块对应一组 Bundle 类型

## 注意事项

- 添加新 TLM 模块时，需在 `include/bundles/` 定义对应的 ReqBundle/RespBundle
- `noc_bundles_tlm.hh` 定义 NoC 特有的路由信息（dest_x/dest_y）和优先级字段