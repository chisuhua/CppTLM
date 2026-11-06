# design —— 契约对齐 + 第四变异态

## D-1 为什么是 ADDED requirement 而非 MODIFIED

`openspec/specs/cep-ualink-inc-state-spec/spec.md` 的 UIS-10 场景只讲**逐格 basis 完整性**与 **`pending` 白名单相等**,**根本没有**「载体只 include 标准库」这条约束(该约束只活在测试与 change 设计里)⇒

- ⛔ **不是契约漂移**(契约没写过 ⇒ 无从漂移);是**契约缺项**;
- ✅ 故用 **ADDED requirement** `cep-ualink-inc-state-spec-self-containment` 独立承载(自足性是**独立约束**,不该塞进 UIS-10 的 basis 完整性场景)⇒ delta 体积小、语义清晰。

## D-2 第四变异态的形态

| 态 | 影子改动 | 期望签名 |
|---|---|---|
| ④ misclassified | `cxl_cache_adapter.hh`:`transition_is_free` 的判据由 `kind == CellKind::Free` 改为 `kind == CellKind::Unknown` | `FREE-KIND mismatch s=<n> e=<n>`(rc=1) |

- 影子树改为**复制整个 `include/tlm/cep/` 目录**(原先只复制单个头)⇒ 后续加头无需改脚本;
- probe 扩展为三段判据:tileink basis↔ref_kind(T3 + 主矩阵)· cxl `Free` ⇔ `transition_is_free` · (注册表一致性由脚本的 JSON↔头文件比对承担,不需编译);
- ⛔ 变异只发生在**影子副本**;收尾断言真源 3 个文件 sha256 未变。

## D-3 白名单的可审计性

白名单是**唯一 1 条**且写明理由(纯 `constexpr` + 只依赖标准库 + 不引入平台/区域/转移判定)⇒ 不是「消音器」;契约里同时要求扫描器**具备鉴别力**(毒化文本必须命中)⇒ 白名单本身不能掩盖扫描器失效。
