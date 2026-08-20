# s2 DGpuBoard Specifications

> **状态**: 📋 Proposed — 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D4/D5

> 本目录存放 s2 change 的具体规格说明。

---

## Spec 列表(待 s2 实施时输出)

| Spec | 内容 | 状态 |
|------|------|------|
| `dgpu-board-mvp.md` | DGpuBoardTLM 6 组件接口契约(per Phase F-H.2) | 📋 待 T-s2-3 实施时输出 |
| `submit-queue-mvp.md` | SubmitQueue WDU 分发网络接口契约(per Phase F-H.5) | 📋 待 T-s2-2 实施时输出 |
| `usrlxemu-ioctl-stub-mvp.md` | 4 IOCTL stub 端到端契约(per Phase F-H.3) | 📋 待 T-s2-4 实施时输出 |

> 当前 s2 change 所有接口契约已通过 [`../../../docs/soc_arch/modules/`](../../modules/) 模块设计文档描述。
> 具体 JSON 示例、字段偏移、测试期望将在 s2 W3-W4 实施过程中按需补充到本目录。
