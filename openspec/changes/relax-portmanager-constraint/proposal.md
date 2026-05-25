# Relax PortManager Template Constraint (P3.6)

## Why

`include/core/port_manager.hh` — `is_base_of_v<SimObject>` 约束过于严格。

## What Changes

- 放宽模板约束
- 允许更多类型使用 PortManager

## Impact

- 文件: `include/core/port_manager.hh`

## Tasks

见 `tasks.md`
