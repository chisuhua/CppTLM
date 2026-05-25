# Fix CrossbarTLM Bus Conflict Detection (P3.1)

## Why

`include/tlm/crossbar_tlm.hh:86` — `resp_out[dst].write(resp)` 无冲突检测，可能导致总线冲突。

## What Changes

- 添加 `port_busy[]` 检测数组
- 在写入前检查端口是否忙碌
- 返回错误或等待

## Impact

- 文件: `include/tlm/crossbar_tlm.hh`

## Tasks

见 `tasks.md`
