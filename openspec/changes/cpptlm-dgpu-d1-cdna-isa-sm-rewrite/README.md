# cpptlm-dgpu-d1-cdna-isa-sm-rewrite

dGPU SoC v1.0 SM 微架构重构 — 12 ChStream 子模块 + 8 Bundle + `IComputeDevice` 15 方法

## Goal

完整重构 GPU 算力侧为 gpgpu-sim 风格 SM 微架构；PTX-EMU 仅通过 `IComputeDevice` 接口步进；supersedes `cpptlm-dgpu-d1-cdna-isa-phase-a`。

## Status Update
- **2027-02-09**: **Superseded** by `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`. 阶段 A 双轨并存决策被反转；CDNA 引擎接入由 SM 重构直接承接。