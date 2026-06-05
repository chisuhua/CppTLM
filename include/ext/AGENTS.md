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
- **多 extension 并存**（Phase 1c）：单个 payload 可同时附加多个不同类型的 extension，由 `tlm_extension_registry` 统一分配 ID，由 `tlm_array<tlm_extension_base*>` 索引存储
- **`set_extension<T>()` 返回旧指针**（SystemC 2.0 语义）：调用方负责 `delete` 旧 extension，避免静默销毁
- **`release_extension<T>()`**（Phase 1c 新增）：删除并清空槽位，匹配 `tlm::tlm_extension<T>` 标准 API
- **`get_extension<T>()` 非空 + `set_extension<T>()` 返回非空**：先 delete 旧值，再写入新值（不依赖 RAII）

## 注意事项

- 这些是内部 TLM 扩展（Transaction Level Modeling），不是 SystemC TLM
- 用于 ChStream 的事务追踪和错误处理
- `error_context_ext.hh` 较复杂（5701 行）包含完整错误处理框架