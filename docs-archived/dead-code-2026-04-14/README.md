# 死代码归档 — 2026-04-14

**归档原因**: 全量代码审查发现 26 个死文件（无引用 / ODR 冲突 / CMake 未纳入构建）

## 死头文件 (19 个)

| 原位置 | 原因 |
|--------|------|
| `bundles/cache_bundles.hh` | 旧版，被 `cache_bundles_tlm.hh` (v2.1) 替代 |
| `bundles/fragment_bundles.hh` | 从未被引用（Phase 8+ 规划） |
| `bundles/noc_bundles.hh` | 从未被引用 |
| `ext/coherence_extension.hh` | 从未被引用 |
| `ext/performance_extension.hh` | 从未被引用 |
| `ext/prefetch_extension.hh` | 从未被引用 |
| `ext/qos_extension.hh` | 从未被引用 |
| `framework/error_category.hh` | ODR 冲突：与 `core/error_category.hh` 相同 guard 但内容不同 |
| `modules/legacy/crossbar_rr.hh` | 无引用 |
| `modules/legacy/example_tlm_module.hh` | 无引用 |
| `modules/legacy/router_hash.hh` | 无引用 |
| `modules/legacy/stream_consumer.hh` | 无引用 |
| `modules/legacy/stream_traffic_gen.hh` | 无引用 |
| `sc_core/sc_*.hh` (6 个) | 全部死代码，SystemC 包装器已废弃 |
| `src/noc/*.hh` (5 个) | 旧 NoC 实现，GemSc 历史遗留 |

## 死源文件 (12 个)

| 原位置 | 原因 |
|--------|------|
| `src/noc/*.cc` (6 个) | CMakeLists.txt 已注释掉不编译 |
| `src/sc_core/sc_wrapper.cc` | 不在 CMake 构建中 |

## 验证

- ✅ 编译通过: `cmake --build build` — 无错误
- ✅ 测试通过: 201 用例, 10,879 断言全部通过
- ✅ 零回归: 所有活跃文件引用检查通过
