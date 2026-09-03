# 06. dGPU SoC v1.0 Tensor Core 架构 — wgmma + tcgen05 + MFMA + 5th Tensor Core

> **类别**: SoC Architecture > 子系统架构 (L6 SM/CU 内核 — Tensor Core 子模块)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（§3.6 L6.1 + L6.2 Tensor Core 路径）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/cuda-core-adapter.md`](../modules/cuda-core-adapter.md)（v0.5 MVP Tensor Core 集成）
> - [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md)
> **关联研究综述**:
> - [`docs/research/SM/overview.md`](../../research/SM/overview.md)（wgmma/tcgen05/TMA/mbarrier/DSMEM）
> - [`docs/research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md`](../../research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md)
> - [`docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`](../../research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md)
> **关联 ADR**: 00-overview D7（Tensor Core 渐进）/ ADR-SOC-09（CU 双 vendor 蓝图 Tensor Core 路径）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.6 L6 SM/CU 内核层中**Tensor Core 子模块**的详细化文档。

- 想快速理解 Tensor Core 范围 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 NVIDIA wgmma(Hopper 4 代) → 读 §3
- 想理解 NVIDIA tcgen05(Blackwell 5 代) → 读 §4
- 想理解 NVIDIA TMEM 张量内存 → 读 §5
- 想理解 AMD MFMA → 读 §6
- 想理解 Tensor Core 数据格式 → 读 §7
- 想理解 v1.0/v1.1 战略对齐 → 读 §8
- 想理解 CppTLM TensorCoreTLM 实现 → 读 §9
- 想理解配置 Schema → 读 §10
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §11
- 想评估风险 → 读 §12

---

## 1. 范围与目标

### 1.1 Tensor Core 子模块定位

**Tensor Core** = dGPU SoC v1.0 L6 SM/CU 内核层中的**张量运算加速单元**,负责:

- **NVIDIA 路径**:
  - **Hopper SM_90**:wgmma(4 代,per US20230289398A1)
  - **Blackwell SM_100/120**:tcgen05 + TMEM(5 代,per arXiv 2512.02189)
- **AMD 路径**:MFMA(Matrix Core,CDNA 2/3 主流量)
- **CppTLM TensorCoreTLM**:作为 SM/CU 内部子模块,深度集成 PTX-EMU 与 MFMA

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R05-R06 + D7)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| NV wgmma(Hopper 4 代) | ✅ 4 warp × 32 + FP8/FP16/BF16/TF32 | ✅ 全 FP 格式 | D7(ADR-SOC-09) |
| NV tcgen05(Blackwell 5 代) | ❌ 推迟 | ✅ + TMEM + FP4/FP6/MX | D7 |
| AMD MFMA | ✅ 基础 | + 完整 | D7 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.1 PASS 的 §3.6 L6.1 NVIDIA 路径 + L6.2 AMD 路径 + §4-bis 范围矩阵 R05-R06。

---

## 2. 顶层数据流图

```
                    ┌─────────────────────────────────────────────┐
                    │            L5 WDU → SM/CU 分发             │
                    └───────────────────┬─────────────────────────┘
                                        │
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │      ComputeUnitTLM 内部                 │
                  │  - Warp × 4 / Wavefront                  │
                  │  - Vector RegFile                        │
                  │  - Shared Memory / LDS                    │
                  │  - DSMEM (v1.1)                          │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ NVIDIA 路径        │    │ AMD 路径               │
              │ (nv_mode)          │    │ (amd_mode)             │
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                            │
        ┌───────────────▼────────────┐    ┌──────────▼──────────────┐
        │ wgmma (Hopper 4 代)     │    │ MFMA (CDNA 3)          │
        │ v1.0: FP8/FP16/BF16/TF32│    │ + LDS(64KB per CU)    │
        │ + TMA + mbarrier        │    │ + split workgroup      │
        │ v1.1: tcgen05 + TMEM    │    │                        │
        │       + FP4/FP6/MX      │    │                        │
        └─────────┬─────────────────┘    └──────────┬──────────────┘
                  │                                 │
                  └─────────────┬───────────────────┘
                                │
                                ▼
                  ┌──────────────────────────────────────────┐
                  │        L7 Memory Hierarchy               │
                  │  - HBM3e → L2 → L1 → SMEM/Register       │
                  │  - TMEM (Blackwell v1.1)                  │
                  └──────────────────────────────────────────┘
```

---

## 3. NVIDIA wgmma(Hopper 4 代张量核心,per US20230289398A1)

### 3.1 核心机制

**wgmma**(Warp Group Matrix Multiply-Accumulate)核心机制(per `docs/research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md`):

- **4 warps × 32 threads** 的 warp group 跨数据通路共享寄存器文件
- B 操作数读一次多播给 4 个 warp
- A 可绕过寄存器文件直接来自**共享内存描述符**
- FIG. 6A 标注 "H100 TC Instruction 64k MACs, 32 cycles"
- **异步执行** 由"worker thread"状态机驱动:
  - **WG.ARRIVE** → 屏障 → **GMMA 发射** → 读操作数/计算/写回流水 → **WG.DEPBAR**
- 与公开 PTX `wgmma.mma_async / commit_group / wait_group` 语义一一对应

### 3.2 GMMA 描述符

- **统一尺寸/格式/布局/swizzle**
- **单指令驱动整块运算**
- wgmma.mma 指令族:
  - `wgmma.mma_async.sync.aligned.m64n*k*.row.col.f32.{fp8,fp16,bf16,tf32}.{f16,f32}`
  - shape 支持 m64n*k* 多种组合(m=64, k=8(tf32)/16(fp16,bf16)/32(fp8), n=8/16/24/32/48/64/80/96/112/128/192/256)

### 3.3 worker thread 状态机

```
WG.ARRIVE
  ↓
屏障等待 group 内所有 warp 就位
  ↓
GMMA 发射(register file + shared memory 读)
  ↓
计算流水(MAC 树)
  ↓
写回流水(register file 累加)
  ↓
WG.DEPBAR(屏障,等待完成)
```

### 3.4 CppTLM 实施

- **v1.0 MVP**:`TensorCoreTLM::wgmma_mma_64x*` 基础 API
- **FP 格式**:FP8/FP16/BF16/TF32
- **shape 支持**:核心 m64n16k16, m64n128k16 等

---

## 4. NVIDIA tcgen05(Blackwell 5 代张量核心,per arXiv 2512.02189)

### 4.1 关键演进

**tcgen05 核心特征**(per `docs/research/SM/overview.md` §四 + arXiv 2512.02189):

| 维度 | wgmma(Hopper) | tcgen05(Blackwell) |
|------|---------------|-------------------|
| 发射方式 | warp group 协作 | **单线程发射** |
| 操作数来源 | 共享内存 + 寄存器 | **共享内存 + TMEM** |
| warp 同步 | 需要 | **取消** |
| 张量操作调度 | warp 级别 | **每线程** |
| CTA pair | 不支持 | **支持(2 CTA 共享)** |
| 延迟特性 | 随尺寸 32-128 cycles | **恒定 ≈11 cycles** |

### 4.2 与 wgmma 的关键差异

1. **单线程发射**:不再需要 warp group 协作,简化调度
2. **TMEM 引入**:每 SM 256KB 张量内存,显式管理(`tcgen05.ld/st/cp`)
3. **每线程调度**:张量操作获得真正的 per-thread 调度
4. **CTA pair 协作**:相邻 rank 的两个 CTA 共享操作数,映射到同一 TPC
5. **weight-stationary**:卷积数据流硬化,collector buffer 缓存 B 操作数

### 4.3 低精度格式扩张(per Blackwell 简报 Table 1/3)

| 格式 | NVIDIA Hopper | NVIDIA Blackwell SM_100 | NVIDIA Blackwell Ultra SM_120 |
|------|---------------|------------------------|------------------------------|
| FP4 | ❌ | ✅(B200) | ✅(Blackwell Ultra) |
| FP6 | ❌ | ✅(B200) | ✅(Blackwell Ultra) |
| FP8 | ✅ | ✅ | ✅(FP4 为 FP8 的 2× throughput) |
| INT8 | ✅ | ✅ | ✅ |
| FP16/BF16 | ✅ | ✅ | ✅ |
| TF32 | ✅ | ✅ | ✅ |
| FP64 | ✅ | ✅ Tensor Core | ✅ Tensor Core |

### 4.4 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R05)
- **v1.1 完整版**:`TensorCoreTLM::tcgen05_mma_*` + TMEM 256KB/SM + CTA pair

---

## 5. NVIDIA TMEM 张量内存(per arXiv 2512.02189)

### 5.1 关键参数(B200 实测)

**TMEM(Tensor Memory)** 是 Blackwell SM 新设的张量内存:

| 参数 | 值 |
|------|-----|
| 容量 | **256KB/SM** |
| 组织 | **512 列 × 128 lane** |
| 读带宽 | **~16 TB/s** |
| 最优 tile | 64×64 |
| 操作码 | `tcgen05.ld/st/cp` 显式管理 |

### 5.2 TMEM 角色

- **张量中间结果存储**:MMA 计算中间结果无需写回通用寄存器
- **CTA pair 共享**:相邻 rank CTA 通过 TMEM 共享操作数
- **warp 取消**:per-thread 调度不需要 warp 级协调

### 5.3 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R22)
- **v1.1 完整版**:`TMEM` 模块 + 256KB/SM + 16 TB/s 读带宽 + tcgen05.ld/st/cp API

---

## 6. AMD MFMA(Matrix Core,CDNA 3)

### 6.1 关键机制

**MFMA**(Matrix Fused Multiply-Add)是 AMD Matrix Core 的张量运算指令:

- **SIMD32/64 wavefront** 作为 MFMA 基础单元
- 支持 FP16/BF16/INT8/FP32 等格式
- 通过 **LDS**(Local Data Share,64KB per CU)共享输入数据
- **HW collector buffer** 缓存 B 操作数(weight-stationary,类似 Blackwell)

### 6.2 MI300X MFMA 配置

- **MI300X**:304 CU total(8 XCD × 38 CU),wave64 主流量
- **MFMA shapes**:16×16×16, 32×32×8 等
- **指令**:`v_mfma_*_*` (CDNA 2/3 命名约定)

### 6.3 split workgroup 与 MFMA

- **split workgroup**(per EP3785113B1):workgroup 拆分到多个 CU,每个 CU 处理部分 wavefront
- 每个 CU 内部 MFMA 计算使用 LDS 共享数据
- **load-rating 反馈**:动态调整 workgroup 拆分比例

### 6.4 CppTLM 实施

- **v1.0 MVP**:`TensorCoreTLM::mfma_*` 基础 API
- **FP 格式**:FP16/BF16/INT8
- **shape 支持**:16×16×16, 32×32×8

---

## 7. Tensor Core 数据格式

### 7.1 NVIDIA 格式支持矩阵

| 格式 | Hopper SM_90 wgmma | Blackwell SM_100/120 tcgen05 | 备注 |
|------|---------------------|------------------------------|------|
| FP64 | ✅ Tensor Core | ✅ Tensor Core | HPC 精度 |
| TF32 | ✅ | ✅ | 训练精度 |
| FP32 | ✅(基础 ALU)| ✅(基础 ALU)| 一般计算 |
| FP16 | ✅ | ✅ | 训练/推理 |
| BF16 | ✅ | ✅ | 训练(降低精度) |
| FP8(E4M3 + E5M2) | ✅ | ✅ | 推理 |
| FP6 | ❌ | ✅(Ultra)| 推理 |
| FP4(MX 格式) | ❌ | ✅(Ultra)| Blackwell Ultra 推理 |
| INT8 | ✅ | ✅ | 推理 |

### 7.2 AMD MFMA 格式支持

| 格式 | CDNA 2 MI200 | CDNA 3 MI300X |
|------|---------------|---------------|
| FP64 | ❌ | ❌(MI300X/MI325X 的 FP64 能力需按具体 SKU 核实；MI250X 保留 FP64 MFMA)|
| BF16 | ✅ | ✅ |
| FP16 | ✅ | ✅ |
| INT8 | ✅ | ✅ |
| FP8 | ✅(MI300X)| ✅ |

### 7.3 第二代 Transformer Engine(per Blackwell 简报)

**micro-tensor scaling**(per `docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md` §3.3):

- "advanced dynamic range management algorithms and fine-grain scaling techniques"
- 使能 FP4 AI
- 配套 Dynamo + TensorRT-LLM + Nemo Framework / Megatron-Core
- "doubles the performance of Blackwell's FP4 Tensor Core, doubles the parameter bandwidth to the HBM memory, and doubles the size of models supported per GPU"

### 7.4 Blackwell Ultra 注意力层加速

- **2× speedup** over Blackwell GPUs for attention layer compute
- "**new instructions** to improve the performance of long input sequences"(per Blackwell 简报 §3.4)
- **指令名称/语义未公开**(per 简报明确"未提及清单")

---

## 8. v1.0 战略对齐

### 8.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| NV wgmma v1.0 | ✅ FP8/FP16/BF16/TF32 | ✅ §3 |
| NV tcgen05 v1.1 | + TMEM + FP4/FP6/MX | ✅ §4 + §5 |
| AMD MFMA | ✅ 基础 + LDS | ✅ §6 |
| FP 格式矩阵 | 双 vendor 对齐 | ✅ §7 |

### 8.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| NV wgmma(Hopper) | ✅ FP8/FP16/BF16/TF32 | + 全 shape |
| NV tcgen05(Blackwell) | ❌ | ✅ + TMEM 256KB/SM |
| NV TMEM | ❌ | ✅ 16 TB/s 读 |
| NV FP4/FP6/MX | ❌ | ✅(OCP microscaling)|
| NV Transformer Engine | ❌ | ✅ micro-tensor scaling |
| AMD MFMA(CDNA 3) | ✅ FP16/BF16/INT8 | + FP8 |
| AMD split workgroup | ✅(per EP3785113B1) | + load-rating |
| AMD wavefront 重组 | ✅(per US9135077B2) | + 多内核调度器 |

### 8.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| 00-overview D7 | Tensor Core 渐进实施(wgmma v1.0 → tcgen05 v1.1) |
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 蓝图 Tensor Core 路径 |

---

## 9. CppTLM TensorCoreTLM 实现

### 9.1 模块定位

`TensorCoreTLM` 作为 ComputeUnitTLM 内部子模块,封装:

- NVIDIA wgmma / tcgen05 API
- AMD MFMA API
- Tensor Core 性能模型(per arXiv 微基准)

### 9.2 关键方法

```cpp
class TensorCoreTLM {
public:
    // NVIDIA wgmma (Hopper)
    void wgmma_mma_64x*_k*(
        uint64_t a_desc, uint64_t b_desc, uint64_t c_desc,
        FPType a_type, FPType b_type, FPType c_type
    );
    
    // NVIDIA tcgen05 (Blackwell v1.1)
    void tcgen05_mma(
        uint64_t a_smem, uint64_t b_tmem, uint64_t c_tmem,
        FPType type  // FP4/FP6/FP8/FP16/BF16/TF32
    );
    
    // AMD MFMA (CDNA 3)
    void mfma_16x16x16(
        uint64_t a_reg, uint64_t b_lds, uint64_t c_reg,
        FPType type
    );
    
    // 共享 wait_group / barrier
    void wait_group(int n);
    void commit_group();
};
```

### 9.3 与其他模块协作

- **TMA**:wgmma/tcgen05 输入来自 TMA 描述符
- **mbarrier**:wgmma/tcgen05 完成后通过 mbarrier 同步
- **SharedMemory / LDS**:wgmma/tcgen05/MFMA 操作数来源
- **RegFile**:wgmma/MFMA 累加结果写回

---

## 10. 配置 Schema

```json
{
  "name": "tensor_core_0",
  "type": "TensorCoreTLM",
  "params": {
    "vendor_mode": "nv_mode",        // "nv_mode" | "amd_mode"
    
    // NVIDIA 参数
    "nv_tensor_core_gen": 4,          // 4 = wgmma (Hopper); 5 = tcgen05 (Blackwell)
    "nv_supported_fp_types": ["fp8", "fp16", "bf16", "tf32"],
    "nv_wgmma_supported_shapes": ["m64n16k16", "m64n128k16", "m64n256k16"],
    "nv_tcgen05_enabled": false,     // v1.1
    "nv_tmem_kb": 256,                // v1.1
    "nv_transformer_engine": false,  // v1.1
    
    // AMD 参数
    "amd_mfma_supported_shapes": ["16x16x16", "32x32x8"],
    "amd_supported_fp_types": ["fp16", "bf16", "int8"],
    "amd_lds_kb": 64,
    
    // 共享参数
    "tensor_core_utilization_target": 0.85
  }
}
```

---

## 11. ADR/微架构/OpenSpec 引用矩阵

### 11.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| 00-overview D7 | Tensor Core 渐进实施(wgmma v1.0 → tcgen05 v1.1) |
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor Tensor Core 蓝图 |

### 11.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **CudaCoreAdapter** | [`docs/soc_arch/modules/cuda-core-adapter.md`](../modules/cuda-core-adapter.md) |
| **ComputeUnitTLM** | [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md) |
| **MinimalWarpSchedulerTLM** | [`docs/soc_arch/modules/gpu-kernel-launch.md`](../modules/gpu-kernel-launch.md)(关联)|

### 11.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/SM/overview.md`](../../research/SM/overview.md) | NVIDIA Hopper→Blackwell SM 综述(wgmma + tcgen05 + TMEM) |
| [`docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`](../../research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md) | Blackwell 官方简报 + Transformer Engine |
| [`docs/research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md`](../../research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md) | wgmma 4 warps×32 + worker thread |
| [`docs/research/SM/CudaCore/arXiv2512.02189_Blackwell微基准深入_解析.md`](../../research/SM/CudaCore/arXiv2512.02189_Blackwell微基准深入_解析.md) | TMEM + tcgen05 实测 |
| [`docs/research/SM/CudaCore/arXiv2507.10789_Blackwell微基准剖析_解析.md`](../../research/SM/CudaCore/arXiv2507.10789_Blackwell微基准剖析_解析.md) | RTX 5080 消费级 |

---

## 12. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | tcgen05 + TMEM 实施复杂(单线程发射 + 256KB/SM 显式管理) | 🟡 中 | v1.0 MVP 不实施;v1.1 完整版基于 arXiv 2512.02189 实测建模 |
| **R2** | FP4/FP6/MX 格式 OCP microscaling 标准支持 | 🟡 中 | v1.1 完整版追加;v1.0 MVP FP8 已覆盖主流 |
| **R3** | AMD MI300X 移除 FP64 路径(per MI250X → MI300X 演进) | 🟢 低 | MFMA v1.0 MVP 不含 FP64;如需 HPC,推迟 |
| **R4** | 双 vendor Tensor Core 蓝图共享的微差异(per-SM vs per-CU) | 🟢 低 | per-shape 行为由 nv_mode/amd_mode 分支 |
| **R5** | Transformer Engine micro-tensor scaling 软硬件协同 | 🟡 中 | v1.1 完整版;v1.0 MVP 仅硬件层 |

---

## 13. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L6 Tensor Core 子模块架构,基于 NVIDIA wgmma/tcgen05 + AMD MFMA + FP 格式矩阵 + CppTLM TensorCoreTLM) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS