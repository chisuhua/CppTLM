# Fix RouterTLM VC Allocation Failure Handling (P3.5)

## Why

`src/tlm/router_tlm.cc:364` — `allocate_vc()` 返回 `NUM_VCS` 表示分配失败，但调用处未检查。

## What Changes

- 在调用 `allocate_vc()` 处检查返回值
- 当返回 `NUM_VCS` 时，正确处理失败情况

## Impact

- 文件: `src/tlm/router_tlm.cc`

## Tasks

见 `tasks.md`
