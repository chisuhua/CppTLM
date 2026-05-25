# Fix PayloadToPacket Hash Function (P3.3)

## Why

`include/core/ext/payload_to_packet.hh:31` — `std::hash<uint64_t>` 用于 pair key，导致哈希函数错误。

## What Changes

- 实现自定义 `PairHash` 结构
- 使用 `std::hash_combine` 组合哈希

## Impact

- 文件: `include/core/ext/payload_to_packet.hh`

## Tasks

见 `tasks.md`
