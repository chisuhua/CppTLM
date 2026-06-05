# ADR-X.13: tlm_stub 多 Extension 升级

> **状态**: ✅ 已实施  
> **日期**: 2026-06-06  
> **影响**: tlm_stub.hh (94 → 283 行), 8 个 ext/test 文件, +200 行测试代码

## 1. 背景

原 `include/tlm/tlm_stub.hh` (94 行) 用单一 `tlm_extension_base* ext` 指针实现扩展存储，调用 `set_extension<T>()` 时 `delete ext; ext = e;` 静默删除前一个扩展。导致：

- `modules_v2.hh:79` 错误路径 `set_error_code()` 静默删除上游 `TransactionContextExt`
- `add_trace()` / `is_fragmented()` / `get_parent_id()` 在错误后无数据但 `stream_id` 字段 fallback 掩盖
- 与 ADR-X.2 设计意图（"TransactionContextExt 与 ErrorContextExt 共享 payload"）冲突

## 2. 决策

升级 stub 至 Accellera SystemC TLM 2.0 标准实现：

- `tlm_extension<T>::ID` 用 `static const` 类成员（编译期注册，跨 TU 一致）
- `tlm_extension_registry` Meyers-singleton 维护 `type_info → ID` 映射
- `tlm_array<tlm_extension_base*>` 替代单指针（`std::vector` 包装，`expand()` 动态扩容）
- `set/get/clear/release_extension<T>()` API 全部线程安全
- `set_extension<T>()` 返回旧指针（**调用方负责 delete**，匹配 SystemC 2.0 语义）
- `reset()` / `~tlm_generic_payload()` 循环清理所有 extension
- `deep_copy_from()` 用 `std::scoped_lock` 避免死锁

## 3. 实施

| Commit  | Phase | 内容 |
|---------|-------|------|
| 833da21 | 1a    | `tlm_extension_registry` + `tlm_array<T>` |
| c9e9340 | 1b    | CRTP 迁移到 `static const unsigned int ID` |
| adfcc5b | 1c    | Multi-extension API 重构（set/get/clear/release） |
| a15e2fc | 1d    | `deep_copy_from` + `reset` 语义 + 12 个 multi_ext 测试 |
| 93f4a81 | 1.5   | `modules_v2.hh:79` 显式 release 修复（语义清晰化） |

## 4. 兼容性

- **API 表面**：与 SystemC TLM 2.0 完全等价
- **调用方代码**：除 `set_extension<T>(new T())` 旧"静默 delete"语义改为"返回旧指针"外，无变更
- **`USE_SYSTEMC_STUB` CMake option**：保持（默认 ON，定义宏选择 stub）
- **`USE_SYSTEMC` option**：Phase 3a/3b 已删除（仅 stub 路径）

## 5. 参考

- Accellera 官方: https://github.com/accellera-official/systemc/blob/master/src/tlm_core/tlm_2/tlm_generic_payload/
  - `tlm_gp.h:73-85` (CRTP + `static const ID`)
  - `tlm_gp.cpp:35-82` (registry singleton)
  - `tlm_gp.cpp:158-187` (deep_copy_from)
  - `tlm_array.h:51-115` (array class)
- 相关 ADR: X.2 (error handling), X.6 (transaction integration)
- Plan: `.omo/plans/tlm-stub-multi-extension-and-cleanup.md`
