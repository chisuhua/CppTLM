## MODIFIED Requirements

### Requirement: cep-protocol-version-locking-contract

CEP SHALL 以**锁定表**为规范版本的**唯一真源**,并要求每个协议实例声明其规范版本。

#### Scenario: 版本取值必须落在锁定表内(IC1)

- **WHEN** 契约测试校验 `ADR-CEP-11 §3` 的锁定表与 schema 的 `protocol_version`
- **THEN** 锁定表**恰好 8 个协议族**:CHI / CXL 2.0 非对称 / CXL 3.0 对称 / TileLink-C / NVLink / UALink / MOESI/GPU / MESIF
  (2026-10-22 per 台账 Entry #12 —— 增 `moesi_gpu` / `mesif`,二者均为 ArchForge 贡献且 `family` 落 `industrial`)
- **AND** 每个协议实例的 `protocol_version` **必须**满足**两分支之一**:
  - **分支 ①** —— **在** `ADR-CEP-11 §3` 锁定表**内**
  - **分支 ②** —— 匹配 `^user:([a-z][a-z0-9_]*)$` **且** 捕获组 `<ssp-id>` ∈ `include/tlm/cep/ssps.json` 的**已登记** `ssp.id`
- **AND** 两分支皆不满足 ⇒ **装配期 fail**(不得静默降级)
- **AND** ⚠️ **分支 ② 是「表外但合法」** —— ⛔ **不改** `INV-CEP-11-IC1` 的**立法意图**(仍禁「配置层错误静默降级」); ⛔ **不改** `INV-CEP-11-IC2`(§3 锁定表仍是规范版本的**唯一真源**,只允许**表外一种可机判形态**)
  > **来源**:`ADR-CEP-11` SU(2026-11-26)① 决策 3 / `INV-CEP-11-IC1` / 决策 5 三处两分支化(✅ 已 ratify · oracle 2 轮〔首审 `REJECT` → 复审 `APPROVE`〕· critic 1 轮 0 BLOCKER)· `ADR-CEP-24` 决策 3.2 的 Q3 · `CP-Q-1b` 裁决 §5(用户批准 R3)
  > **⛔ 表述纪律**:本 Scenario 两分支化 **≠** 「CEP 已支持用户自定义协议」 —— 那只在 `cep-ssp-batch2` 验收后才成立
- **AND** 两分支**可证不相交**:锁定表内**全部**官方版本**均含** `-` 或 `.`(`chi-r2` / `tilelink-c-1.7.1` / `cxl-2.0` / `nvlink-4.0` / `ualink-2.0` / `moesi-gpu-1.0` / `mesif-1.0` / `cxl-3.0`),而 `ssp.id` 词表 = `^[a-z][a-z0-9_]*$`(`include/tlm/cep/ssp_loader.hh` **E2**)**排除** `-` 与 `.` ⇒ `user:<ssp-id>` **不可能**与任一官方版本字面相同 ⇒ 分支 ② **无法冒充分支 ①**;且拼写错误的官方版本(如 `chi-r3`)**不**匹配 `^user:` ⇒ **仍**落分支 ① 且 **fail** ⇒ **判据未变弱**

#### Scenario: 锁定表是唯一真源(IC2)

- **WHEN** 契约测试扫描 schema / ADR / 代码
- **THEN** 规范版本号**只出现在** `ADR-CEP-11 §3` 的锁定表中
- **AND** 其余位置**只引用不复制**

#### Scenario: 版本变更必须留痕(IC3)

- **WHEN** 契约测试检查变更留痕机制
- **THEN** 规范版本变更**必须**在 `objective-cep-platform §11` 台账留 `scope-change` / `adr-amendment` entry
- **AND** 变更路径**必须**落在三选一内:仅锁表更新 / 语义适配 + adapter 变更 / 并行版本(后者**不支持**)
