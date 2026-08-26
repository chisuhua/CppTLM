# s1 PTX-EMU Integration Specifications

> **状态**: 📋 Proposed — 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D2/D3

> 本目录存放 s1 change 的具体规格说明(接口契约 + 字段定义 + 测试期望)。

---

## Spec 列表(待 s1 实施时输出)

| Spec | 内容 | 状态 |
|------|------|------|
| `ptx-emu-submodule-mvp.md` | PtxEmuSubmoduleMVP **PTX functional facade** 接口契约(per Phase I.1,4 类接口) | 📋 待 T-s1-3 实施时输出 |
| `cuda-core-adapter-mvp.md` | CudaCoreAdapter **SM 微架构探索器** 接口契约(per Phase I.2,4 timing 模块集成 + WarpState 镜像) | 📋 待 T-s1-4 实施时输出 |

> 当前 s1 change 所有接口契约已通过 [`../../../docs/soc_arch/modules/`](../../modules/) 模块设计文档描述。
> 具体 JSON 示例、字段偏移、测试期望将在 s1 W1-W2 实施过程中按需补充到本目录。
