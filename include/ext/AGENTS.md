# include/ext/ — TLM 扩展接口

**域**: TLM 扩展插件接口（4 文件）
**作用**: 提供事务上下文、错误处理、内存扩展等扩展能力

## 文件

| 文件 | 作用 |
|------|------|
| `transaction_context_ext.hh` | 事务上下文扩展（trans_id/cycle/模块信息） |
| `error_context_ext.hh` | 错误上下文（错误码/错误类别/堆栈） |
| `mem_exts.hh` | 内存相关扩展（地址映射/访问权限） |
| `credit_stream.hh` | Credit-based 流控扩展 |

## 扩展架构

```cpp
// TLM 扩展基类（类似 SystemC TLM tlm_extension）
class TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    uint64_t trans_id_;
    uint64_t cycle_;
    std::string module_name_;
    // ...
};
```

## 使用场景

- **事务追踪**: 在 req/resp 通过时记录 timestamp、module、trans_id
- **错误传播**: 错误码沿链路传递，用于调试和容错
- **内存映射**: 地址翻译、检查访问权限
- **流量控制**: Credit 计数、窗口管理

## 约定

- 扩展接口遵循 SystemC TLM 2.0 `tlm_extension` 模式
- 通过 `tlm_generic_payload::set_extension()` 附加到事务
- 扩展生命周期由 payload 管理（自动释放）

## 注意事项

- 这些是内部 TLM 扩展（Transaction Level Modeling），不是 SystemC TLM
- 用于 ChStream 的事务追踪和错误处理
- `error_context_ext.hh` 较复杂（5701 行）包含完整错误处理框架