# MVP Slice Specifications — cpptlm-v05-mvp

> **状态**: 📋 Proposed — 2026-08-19(per Phase J 2026-08-20 对齐)· **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) (per Phase I.4 ADR 移动)
> **关联 OpenSpec**: [`../../2026-08-19-cpptlm-v05-mvp/`](../../2026-08-19-cpptlm-v05-mvp/)

> 本目录存放 MVP 切片的具体规格说明(接口契约 + 字段定义 + 测试期望)。

---

## Spec 列表

| Spec | 内容 |
|------|------|
| `dgpu-board-mvp.md` | DGpuBoardTLM **6 组件** 接口契约(待 S2 W3-4 实施时输出,per Phase F-H.2)|
| `command-processor-mvp.md` | CommandProcessor 5-state FSM + **NVIDIA method packet** 接口契约(待 S3 W5-6 实施时输出,per Phase F-H.3)|
| `pm4-decoder-mvp.md` | Pm4Decoder **NVIDIA method packet** 字段定义(待 S3 W5-6 实施时输出,per Phase F-H.3 路径 3)|
| `submit-queue-mvp.md` | **🆕** SubmitQueue **WDU 分发网络** 接口契约(待 S3 W5-6 实施时输出,per Phase F-H.5)|
| `cuda-core-adapter-mvp.md` | **CudaCoreAdapter SM 微架构探索器** 接口契约(待 S1 W1-2 实施时输出,per Phase I.2)|
| `ptx-emu-submodule-mvp.md` | **PtxEmuSubmoduleMVP PTX functional facade** 接口契约(待 S1 W1-2 实施时输出,per Phase I.1)|

> 当前 MVP 切片所有接口契约已通过 [`../../../docs/soc_arch/modules/`](../modules/) 模块设计文档描述(per Phase F-H.2/I.1/I.2 重构)。
> 具体 JSON 示例、字段偏移、测试期望将在 S1-S4 实施过程中按需补充到本目录。