# 归档的 .disabled 测试

> **归档日期**: 2026-04-13
> **零债务原则**: 所有无法启用的测试必须正式归档并说明原因

## 归档清单

| 原文件 | 原因 | 状态 |
|--------|------|------|
| `test_config_loader.cc.disabled` | JSON 字段 `input_buffer_sizes` 与当前 `buffer_sizes` API 不匹配；`DownstreamPort<MockSim>` 模板参数缺失 | 待 Phase 7.3 修复 |
| `test_latency_injection.cc.disabled` | MockConsumer 通过 `handleUpstreamRequest` 回调接收包，但 PortPair 路径不经过此回调 | 待 Phase 7.3 修复 |
| `test_end_to_end_delay.cc.disabled` | 依赖 `LambdaEvent` 类不存在于当前代码库 | 废弃 |
| `test_layout_styles.cc.disabled` | 依赖 `TopologyDumper` 类不存在于当前代码库 | 废弃 |

## 修复路径

Phase 7.3 统一修复前 2 个（API 适配），后 2 个永久废弃。
