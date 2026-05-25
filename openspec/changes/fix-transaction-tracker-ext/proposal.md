# Fix TransactionTracker Extension Sync (P3.2)

## Why

`include/framework/transaction_tracker.hh:133` — `(void)event` 注释表明 extension 同步逻辑缺失。

## What Changes

- 补全 `record_hop` 中的 extension 同步逻辑
- 确保 TransactionContextExt 正确传播

## Impact

- 文件: `include/framework/transaction_tracker.hh`

## Tasks

见 `tasks.md`
