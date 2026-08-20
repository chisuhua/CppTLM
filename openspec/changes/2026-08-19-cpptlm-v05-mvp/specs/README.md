# MVP Slice Specifications — cpptlm-v05-mvp

> **状态**: 📋 Proposed — 2026-08-19 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md)
> **关联 OpenSpec**: [`../../2026-08-19-cpptlm-v05-mvp/`](../../2026-08-19-cpptlm-v05-mvp/)

> 本目录存放 MVP 切片的具体规格说明(接口契约 + 字段定义 + 测试期望)。

---

## Spec 列表

| Spec | 内容 |
|------|------|
| `dgpu-board-mvp.md` | DGpuBoardTLM 接口契约(待 S2 W3-4 实施时输出)|
| `command-processor-mvp.md` | CommandProcessor 5-state FSM 接口契约(待 S3 W5-6 实施时输出)|
| `pm4-decoder-mvp.md` | Pm4Decoder Mesa-style TYPE3 字段定义(待 S3 W5-6 实施时输出)|

> 当前 MVP 切片所有接口契约已通过 [`../../../docs/soc_arch/modules/`](../modules/) 模块设计文档描述。
> 具体 JSON 示例、字段偏移、测试期望将在 S1-S4 实施过程中按需补充到本目录。