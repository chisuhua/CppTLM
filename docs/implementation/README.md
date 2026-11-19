# docs/implementation/ — 实施笔记与契约载体（附属 2）

> **定位**: 附属 2 · 业务实施笔记。**引用真源 + OpenSpec，不重发明架构决策**。
> **与真源的关系**: 架构决策在 [`../adr/`](../adr/)；本目录记录「**怎么落的**」「**落在哪**」「**判据是什么**」。
> **归档规则**: 实施完结 ⇒ 迁 [`../archive/implementation/`](../archive/implementation/)。

> ⚠️ **机器消费目录**：本目录的部分文件被 **CEP 契约闸与 C++ 测试硬编码路径引用** ⇒
> **文件名与路径不可改动**（改动会静默破坏 `scripts/test/cep_*.sh` 与 `test/test_cep_*.cc`）。
> 受影响文件：`2026-10-21-cep-archforge-interface-alignment.md` · `2026-10-21-cep-archforge-response-receipt.md` ·
> `2026-10-21-cep-stage0-freeze-manifest.json` · `2026-10-22-cep-archforge-response-checklist.md` ·
> `2026-10-22-cep-stage0-external-review-summary.md` · `2026-10-22-cep-poc-spike-spec.md` ·
> `2026-10-29-cep-mesif-state-spec-recognition-checklist.md` · `2026-10-30-cep-mesif-adapter-ratification-request.md` ·
> `2026-11-01-cep-cxl-state-spec-anchor-checklist.md` · `2026-11-02-cep-tilelink-state-spec-anchor-checklist.md` ·
> `module-param-baseline.md`。

---

## 目录

### CEP ↔ ArchForge 对齐（跨仓契约）

| 文档 | 说明 | 状态 |
|------|------|------|
| [2026-10-21-cep-archforge-interface-alignment.md](./2026-10-21-cep-archforge-interface-alignment.md) | **ArchForge 侧接口对齐清单**（M2 准入门可执行前置 + 33 锚点；§6 C-4 / §7 漂移检测） | 📋 交付中 |
| [2026-10-21-cep-archforge-response-receipt.md](./2026-10-21-cep-archforge-response-receipt.md) | **对齐答复回执**（R-1~R-13 + P0-1~P0-5 逐条） | 📋 交付中 |
| [2026-10-22-cep-archforge-response-checklist.md](./2026-10-22-cep-archforge-response-checklist.md) | 对齐答复核对表 | 📋 交付中 |
| [2026-10-22-cep-stage0-external-review-summary.md](./2026-10-22-cep-stage0-external-review-summary.md) | Stage 0 外部评审摘要（1 页） | 📋 待发出 |

### CEP Stage 0 契约层

| 文档 | 说明 | 状态 |
|------|------|------|
| [2026-10-21-cep-stage0-contracts.md](./2026-10-21-cep-stage0-contracts.md) | **Stage 0 契约层**（21 份 ADR + 冻结清单；§7 处置台账） | ✅ 完成 |
| [2026-10-21-cep-stage0-freeze-manifest.json](./2026-10-21-cep-stage0-freeze-manifest.json) | **冻结面清单**（89 件产物的 sha256；机器可读载体） | ✅ 冻结 |
| [2026-10-23-cep-stage0-self-review-receipt.md](./2026-10-23-cep-stage0-self-review-receipt.md) | Stage 0 自审回执（变体 B；**独立性未满足**） | ✅ 出具 |
| [2026-10-22-cep-poc-spike-spec.md](./2026-10-22-cep-poc-spike-spec.md) | **PoC Spike 用例规格**（S1/S2/S3 输入 / 期望 / 判定） | 📋 规格 |

### 协议族状态规格核对表

| 文档 | 说明 |
|------|------|
| [2026-10-29-cep-mesif-state-spec-recognition-checklist.md](./2026-10-29-cep-mesif-state-spec-recognition-checklist.md) | MESIF 状态规格识别核对表（状态机 + 5→4 `perm` 投影） |
| [2026-10-30-cep-mesif-adapter-ratification-request.md](./2026-10-30-cep-mesif-adapter-ratification-request.md) | MESIF adapter 批准请求（由 `scripts/test/generate_mesif_ratification_request.sh` 生成） |
| [2026-11-01-cep-cxl-state-spec-anchor-checklist.md](./2026-11-01-cep-cxl-state-spec-anchor-checklist.md) | CXL 状态规格锚点核对表（残余风险 R-2） |
| [2026-11-02-cep-tilelink-state-spec-anchor-checklist.md](./2026-11-02-cep-tilelink-state-spec-anchor-checklist.md) | TileLink 状态规格锚点核对表 |

### 其他

| 文档 | 说明 | 状态 |
|------|------|------|
| [module-param-baseline.md](./module-param-baseline.md) | **模块参数基线**（参数「读取即声明」机制的判据面；ADR-X.17 §3 留证据） | 🟡 活基线 |
| [2026-10-09-vram-controller-functional-integration.md](./2026-10-09-vram-controller-functional-integration.md) | VRAM 控制器 functional 模式集成实施笔记 | ✅ 完成 |
| [11-tgms-v4-implementation-plan.md](./11-tgms-v4-implementation-plan.md) | TGMS v4 实施计划 | 📋 草案 |

---

## 已归档（移出本目录）

实施完结的笔记与计划已迁 [`../archive/implementation/`](../archive/implementation/)（6 份）：
TGMS v4 / Dashboard / ArchForge 拆仓 / GMMU PTE 总线修正 / LWC 总线 hub MVP / dGPU SoC 架构真相清理。
第 4 层实施跟踪的历史产物在 [`../archive/tracking/`](../archive/tracking/)（15 份）。

---

## 相关契约闸

| 闸 | 用途 |
|----|------|
| [`../../scripts/test/cep_archforge_alignment_check.sh`](../../scripts/test/cep_archforge_alignment_check.sh) | 读对齐清单做跨仓漂移检测 |
| [`../../scripts/test/cep_freeze_check.sh`](../../scripts/test/cep_freeze_check.sh) | 校验冻结面 sha256 |
| [`../../scripts/test/cep_doc_ref_check.sh`](../../scripts/test/cep_doc_ref_check.sh) | 对外文档节号引用校验 |

---

**维护**: builder（产出）· architect（契约判据）
