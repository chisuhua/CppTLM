# Phase 1c — Multi-extension API 重构（set/get/clear/release）

**Date**: 2026-06-05
**Plan ref**: `.omo/plans/tlm-stub-multi-extension-and-cleanup.md` 第 425-493 行

## 1. 完成内容

- 替换 `tlm_generic_payload::ext` 单指针为 `tlm_array<tlm_extension_base*> m_extensions`
- 添加 `mutable std::mutex m_extensions_mutex`（线程安全）
- 添加 `private: void resize_for(unsigned int id)` 辅助方法
- 重写 set / get / clear / release API
- 保留 `clear_extensions()` 兼容旧 API
- 析构函数循环 delete 所有 ext
- reset() 临时只调 `m_extensions.clear()`（不 delete）— Phase 1d 会重写
- 添加 Doxygen "CALLER OWNS RETURNED POINTER" 警告
- 把 `tlm_array` 定义移到 `tlm_generic_payload` 之前（按值持有）
- `tlm_array` 新增 `using std::vector<T>::begin/end/clear;`（range-for + clear 支持）

## 2. 调用方审计结果

**结论：所有调用方均安全，无源代码修改。**

### set_extension 调用点（7 个 active 站点）

| 文件 | 行 | 形式 | 风险 |
|------|---|------|------|
| `test/test_tlm_ext.cc:59` | `payload.set_extension(new ReadCmdExt(cmd))` | 忽略返回值 | ✅ 安全 |
| `test/test_tlm_ext.cc:122` | `payload.set_extension(new WriteDataExt(ext))` | 忽略返回值 | ✅ 安全 |
| `test/test_packet_pool.cc:17` | `payload->set_extension(new ReadCmdExt(cmd))` | 忽略返回值 | ✅ 安全 |
| `include/ext/transaction_context_ext.hh:94` | `p->set_extension(ext)` | 忽略返回值 | ✅ 安全 |
| `include/ext/transaction_context_ext.hh:110` | `p->set_extension(ext)` | 忽略返回值 | ✅ 安全 |
| `include/ext/error_context_ext.hh:184` | `p->set_extension(ext)` | 忽略返回值 | ✅ 安全 |
| `include/ext/error_context_ext.hh:202` | `p->set_extension(ext)` | 忽略返回值 | ✅ 安全 |
| `include/core/ext/payload_to_packet.hh:87` | `resp_pkt->set_extension(ext)` | 忽略返回值 | ✅ 安全 |

**API 变化**：`set_extension<T>(T*)` 从 `void` → `T*`（返回旧指针）。
所有调用方都忽略返回值 → C++ 允许忽略非 void 返回值 → 全部编译通过。
**无危险模式**：无 `auto* old = p->set_extension<T>(...)` 然后忘记 delete 的用法。

### get_extension<T>(T*& e) 调用点（9 个 active 站点）

| 文件 | 行 | 形式 | 风险 |
|------|---|------|------|
| `include/core/packet.hh:85,99,116,130,145,158,174,188,202` | `payload->get_extension(ext); if (ext) ...` | 忽略 bool 返回，使用 out-param | ✅ 安全 |
| `include/ext/transaction_context_ext.hh:80,87` | `p->get_extension(ext); return ext;` | 忽略 bool 返回 | ✅ 安全 |
| `include/ext/error_context_ext.hh:174` | `p->get_extension(ext)` | 忽略 bool 返回 | ✅ 安全 |
| `include/framework/debug_tracker.hh:139,146,155` | `payload.get_extension(err_ext); if (err_ext) ...` | 忽略 bool 返回 | ✅ 安全 |
| `examples/example_error_handling.cc:38` | `pkt->payload->get_extension(ext)` | 忽略 bool 返回 | ✅ 安全 |

**API 变化**：`get_extension<T>(T*&)` 从 `bool` → `void`。
所有调用方都使用 out-param 形式，bool 返回值都被忽略 → 全部编译通过。

### get_extension<T>() 单参数形式

完全保持 `T*` 签名 → 零变化。

### 文档（非代码）

- 13 个 docs/ 与 docs-archived/ 目录下的 .md 文件包含 set_extension / get_extension 字符串
- 这些是**纯文档**，不参与编译 → 不影响测试

## 3. 验证结果

| 测试组 | 结果 |
|--------|------|
| `[extension]` | 9/9 ✅ (103 assertions) |
| `[chstream]` | 31/31 ✅ (131 assertions) |
| 全套 | 567/569 ✅ (14586 assertions, 2 已知 NIC 失败未回归) |
| 编译警告 | 0 新增（1 已知 `traffic_gen_tlm.hh:198` unused param，Phase 1c 之前就存在） |

## 4. Phase 1c → 1d 交接清单

- [ ] `reset()` 需要重写为 `for (auto& e : m_extensions) { delete e; e = nullptr; }`
- [ ] `~tlm_generic_payload()` 可以保持当前 loop-delete 实现，但 Phase 1d 应统一 reset/dtor 逻辑
- [ ] 添加 `deep_copy_from(const tlm_generic_payload&)` 方法（1d 任务）
- [ ] 添加 `test_tlm_multi_extension.cc` 验证多 ext 并存（1d 任务）

## 5. 设计决策记录

- **dtor loop-delete**：保持简洁的 for-range delete。Phase 1c reset 不 delete，dtor delete 是为补偿。
- **reset 不 delete**：按 plan spec 允许。Phase 1d 会补齐。
- **clear_extensions 保留**：legacy API，已被 packet_pool 等代码隐式依赖（虽然不直接调用，但有计划提到 PacketPool release 路径）。
- **mutex 粒度**：单 payload 粒度锁。set/get 都加锁。简单且足够。
- **resize_for 私有**：避免外部误用，仅 set_extension 内部调用。
- **tlm_array 移到前部**：因为 tlm_generic_payload 按值持有。避免 forward-decl 复杂度。
