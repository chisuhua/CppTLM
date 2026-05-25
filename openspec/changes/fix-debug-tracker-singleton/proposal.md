# Fix DebugTracker Singleton Initialization (P3.4)

## Why

`include/framework/debug_tracker.hh:99` — `if (initialized_) return;` 在锁外检查，可能导致竞态。

## What Changes

- 使用 `std::call_once` 确保线程安全初始化
- 或使用 mutex 保护初始化检查

## Impact

- 文件: `include/framework/debug_tracker.hh`

## Tasks

见 `tasks.md`
