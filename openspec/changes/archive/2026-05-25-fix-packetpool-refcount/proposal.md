# Fix PacketPool Reference Counting (P0.2)

## Why

PacketPool 当前引用计数使用手动 `ref_count++/--` 操作，在多线程共享 PacketPool 时存在竞态条件风险。

## What Changes

### Problem Analysis

- **问题位置**: `include/core/ext/packet_pool.hh`
- **症状**: 多线程共享 PacketPool 时可能出现竞态
- **根因**:
  1. `add_ref()` 为私有死代码（从未被调用）
  2. `remove_ref()` 仅在持有锁的 `release()` 内调用
  3. `ref_count` 使用手动整数而非 atomic

### Proposed Fix

1. 将 `ref_count` 改为 `std::atomic<uint32_t>`
2. 清理死代码 `add_ref()`（私有方法，无调用方）
3. 确保 `remove_ref()` 逻辑正确（已有锁保护）

## Impact

- **文件**: `include/core/ext/packet_pool.hh`
- **测试**: 需要添加多线程安全测试

## Tasks

见 `tasks.md`
