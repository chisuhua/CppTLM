# Fix RouterTLM Pipeline State Clearing (P0.1)

## Why

RouterTLM 在高流量场景下会出现丢包和乱序问题。根因分析发现：在每个周期开始时，`pipe_reg_` 状态被直接覆盖，导致上一周期的等待状态丢失。

## What Changes

### Problem Analysis

- **问题位置**: `include/tlm/router_tlm.hh`
- **症状**: 高流量下丢包/乱序
- **根因**: 每周期开始时 `pipe_reg_` 被直接覆盖，未保存上周期等待状态

### Proposed Fix

在每个周期开始时：
1. 保存上周期的 `pipe_reg_` 状态到临时变量
2. 处理完当前请求后，将等待状态合并回 `pipe_reg_`
3. 避免直接覆盖导致的等待状态丢失

## Impact

- **文件**:
  - `include/tlm/router_tlm.hh` — 修改流水线状态管理逻辑
  - `src/tlm/router_tlm.cc` — 如果有实现的话

- **测试**: 需要添加或更新 RouterTLM 流水线相关测试

## Tasks

见 `tasks.md`
