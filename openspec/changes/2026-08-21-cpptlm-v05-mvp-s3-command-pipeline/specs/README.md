# s3 Command Pipeline Specifications

> **状态**: 📋 Proposed — 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5

> 本目录存放 s3 change 的具体规格说明。

---

## Spec 列表(待 s3 实施时输出)

| Spec | 内容 | 状态 |
|------|------|------|
| `command-processor-mvp.md` | CommandProcessor 5-state FSM + **NVIDIA method packet** 接口契约(per Phase F-H.3) | ❌ 未产出(module 级 spec 由 fill-mvp spec 替代,fill-mvp spec 是权威) |
| `pm4-decoder-mvp.md` | Pm4Decoder **NVIDIA method packet** 字段定义(per Phase F-H.3 路径 3) | ❌ 未产出(module 级 spec 由 fill-mvp spec 替代,fill-mvp spec 是权威) |
| `tmu-dispatch-processor-mvp.md` | TmuDispatchProcessor **反压停 fetch** 接口契约(per Phase F-D.2 H5) | ❌ 未产出(module 级 spec 由 fill-mvp spec 替代,fill-mvp spec 是权威) |

> 当前 s3 change 所有接口契约已通过 [`../../../docs/soc_arch/modules/`](../../modules/) 模块设计文档描述，但 module 级 spec 由 fill-mvp spec 替代，fill-mvp spec 是权威。
> 具体 JSON 示例、字段偏移、测试期望将在 s3 W5-W6 实施过程中按需补充到本目录。
