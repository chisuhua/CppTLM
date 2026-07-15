# cudart/ Vendored Headers

## cpptlm_bridge.h (ABI 头文件)

- **Source**: `/workspace/project/PTX-EMU` 本地仓库 @ commit `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`
- **Public repo**: github.com/chisuhua/PTX-EMU @ commit `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`
- **Vendor method**: 从 PTX-EMU 本地仓库 commit 显式提取（`git show 8dc000ec:include/cudart/cpptlm_bridge.h`），不复制 working tree
- **ABI 真值源**: PTX-EMU 是 ABI 提供方（CppTLM 通过 HSK-3 选项 1 `ExternalProject_Add` 消费）
- **SHA-256 (commit 8dc000ec)**: `c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe`
- **Last verified**: 2026-07-15（vendor 与 PTX-EMU commit 8dc000ec 字节级一致）
- **Sync policy**: HSK-1 每次重发时手动同步（`git show <new-hash>:include/cudart/cpptlm_bridge.h`）
- **Replaced by**: 未来 HSK-3 选项 1（ExternalProject_Add）实施后，此 vendor 改为动态拉取（无需手动同步）

## 验收检查

- [x] 2026-07-15 — vendor 与 PTX-EMU commit 8dc000ec 字节级一致
