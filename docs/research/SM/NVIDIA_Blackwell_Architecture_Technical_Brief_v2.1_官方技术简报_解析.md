# NVIDIA Blackwell Architecture Technical Brief V2.1（官方技术简报）解析

> 分析日期：2026-08-20
> 本地 PDF：file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1.pdf （文本层完整）
> 来源页：https://resources.nvidia.com/en-us-blackwell-architecture（页面内嵌 dam-cdn.nvd.orangelogic.com 资产链接）
> 文本依据：本地 PDF 全文提取（31 页通读）

## 1. 文献元信息

| 项目 | 内容 |
| --- | --- |
| 标题 | NVIDIA Blackwell Architecture Technical Brief（副题 "Built for the Age of AI Reasoning"） |
| 版本 | V2.1；封面脚注：更新至 Blackwell Ultra GB300 Superchip、GB300 NVL72 机架系统与 HGX B300 服务器 |
| 发布方 | NVIDIA Corporation（版权页 © 2025） |
| 页数 | 31 页（封面 + 目录 ii–iv + 正文 p5–30 + Notice p31） |
| 覆盖产品线 | Blackwell GPU 与 Blackwell Ultra GPU（芯片级代号，如 GB100/GB202 未载明）；GB200 / GB300 Grace Blackwell（Ultra）Superchip；GB200 NVL72 / GB300 NVL72 机架系统；HGX B200 / HGX B300 八卡服务器；配套 NVLink Switch、ConnectX-8 SuperNIC、Quantum-X800 InfiniBand / Spectrum-X 以太网 / BlueField-3 DPU |
| 文档性质 | 市场导向的官方技术简报（marketing + specs 混合），非完整架构白皮书；完整 Blackwell Architecture 白皮书在 resources.nvidia.com 需表单获取，本简报是其公开等价物 |

## 2. 文档定位与结构（目录走读）

全文按"叙事 → 架构创新 → 三大系统形态 → 结论 → 附录"展开：

| 页码 | 章节 | 内容性质 | 与 SM 内部的距离 |
| --- | --- | --- | --- |
| p5–6 | GB300 NVL72 Built for the Age of AI Reasoning；Blackwell/Blackwell Ultra Overview | 三条缩放定律叙事（预训练 / 后训练 30× / 测试时缩放 100×），Blackwell 定位"能效 30× Hopper" | 无直接关系 |
| p7–12 | NVIDIA Blackwell Architectural Innovations（8 小节） | 唯一与 SM 内部直接相关的是 Blackwell Tensor Core Architecture（p8）、Second-Generation Transformer Engine（p9）、Attention Layer Acceleration（p9，提到"新指令"）；其余（Confidential Computing、NVLink5、Decompression Engine、RAS）属芯片级/系统级特性 | 张量核心两节：近；其余：中—远 |
| p13–18 | Grace Blackwell Ultra / NVL72 Rack-scale Systems（GB300 NVL72） | Table 2 机架规格；50X/35X/30X/25X 营销主张（DeepSeek-R1、FP4、Dynamo+TRT-LLM 口径）；ConnectX-8/PCIe Gen6；视频生成 30× | 远（机架级） |
| p19–23 | Blackwell GB200 NVL72 | 40X 工厂出；GPT-MoE-1.8T 实时推理 30× H100、FP8 训练 4× Hopper；数据库查询 18× CPU；CFD/EDA 仿真；液冷 | 远（系统级） |
| p24 | AI-Ready Enterprise Platform | NVIDIA AI Enterprise / NIM 软件栈 | 远（软件栈） |
| p25–26 | NVIDIA Blackwell HGX | Table 3：HGX B300/B200 每服务器与**每 GPU**规格——全文最细的规格表 | 中（到 GPU 级为止） |
| p27–28 | Conclusion：三条缩放定律 | 叙事收束 | 无 |
| p29–30 | Appendix A：万亿参数推理并行技术 | DP/TP/PP/EP、inflight batching、chunking（GPT 1.8T MoE，chunk 128–8192、2,700+ 配置组合） | 远（部署层） |
| p31 | Notice | 标准免责声明 | 无 |

**结论**：31 页中与 SM 内部直接相关的正文约 2 页（p8–9）；本简报是规格基准与术语锚点，不是微架构文档。

## 3. 与 SM 内部相关的官方表述

### 3.1 第五代张量核心（p8，Blackwell Tensor Core Architecture）

- 张量核心定义为"specialized high-performance compute cores for matrix multiply and accumulate (MMA) math operations"，首见于 Tesla V100，逐代增强；Blackwell 为**第五代**。
- "SM" 一词在全文正文仅出现一次，即此处："Tensor Cores operating in parallel across SMs in one NVIDIA GPU"——未展开 streaming multiprocessor 的任何内部结构。
- 新增数字格式：**FP4**，"including community-defined microscaling (OCP) formats"（即 MX 系列；简报用 OCP microscaling 之名）。Table 1 列出支持数据类型 9 种：FP64、FP32、TF32、FP16、BF16、FP8、INT8、FP6、FP4。注意 Table 1 将 FP6 单列，而 Table 2/3 的算力行将 FP8/FP6 合并给数。

### 3.2 规格表摘录（Table 3 每 GPU，HGX 口径，p25–26）

| 指标（Dense/Sparse） | HGX B300（Blackwell Ultra） | HGX B200（Blackwell） |
| --- | --- | --- |
| FP4 Tensor Core | 14 / 18 petaFLOPS | 9 / 18 petaFLOPS |
| FP8/FP6 Tensor Core | 4.5 / 9 petaFLOPS | 4.5 / 9 petaFLOPS |
| INT8 Tensor Core | 0.15 / 0.30 petaOPS（存疑，见 §7） | 4.5 / 9 petaOPS |
| FP16/BF16 Tensor Core | 2.2 / 4.5 petaFLOPS | 2.2 / 4.5 petaFLOPS |
| TF32 Tensor Core | 1.1 / 2.2 petaFLOPS | 1.1 / 2.2 petaFLOPS |
| FP32 | 75 teraFLOPS | 75 teraFLOPS |
| FP64 Tensor Core / FP64 | 1.2 teraFLOPS | 37 teraFLOPS |
| 显存 | 270 GB HBM3e @ 7.7 TB/s | 最高 192 GB HBM3e @ 7.7 TB/s |
| 最大 TDP / 互连 | 1100 W；NVLink 5 + PCIe Gen6 | 1000 W；NVLink 5 + PCIe Gen5 |

机架口径（Table 2，p13）：GB300 NVL72 FP4 1,080/1,440 PF、GB200 NVL72 720/1,440 PF（dense/sparse）；FP8/FP6 均 360/720 PF；FP16/BF16 均 180/360 PF；HBM 20 TB vs 13.5 TB、576 TB/s；每 GPU 最高 279 GB HBM3e（p17 正文，与 Table 3 的 270 GB 存在 279/270 两处口径）。HGX B300 每服务器"16 Blackwell Ultra Die for 8 GPUs"，HGX B200 "16 Blackwell Die for 8 GPUs"——双 die 构成一 GPU 的官方确认。

### 3.3 第二代 Transformer Engine（p9）

- "advanced dynamic range management algorithms and fine-grain scaling techniques, called **micro-tensor scaling**"，使能 FP4 AI；配套 Dynamo、TensorRT-LLM、Nemo Framework / Megatron-Core。
- 原文效果句："doubles the performance of Blackwell's FP4 Tensor Core, doubles the parameter bandwidth to the HBM memory, and doubles the size of models supported per GPU"——比较基准**未载明**（从上下文推断是相对 FP8 路径，简报未明说）。
- 训练侧：新 expert parallelism 技术 + 第五代 NVLink；"lower precision formats open possibilities for further acceleration of large-scale training"。

### 3.4 注意力层加速（p9，Blackwell Ultra 独有）

- "Blackwell Ultra GPU provides a **2X speedup** over Blackwell GPUs for attention layer compute with **new instructions** to improve the performance of long input sequences"——全简报唯一明示的 ISA 层改动（新指令），但指令名称、语义均未载明。

### 3.5 芯片级官方事实（p7–8）

- 2080 亿晶体管（"more than 2.5x" Hopper）；TSMC 4NP 定制工艺；"largest GPU ever built"。
- **双 die**：两片 die 均达光罩（reticle）尺寸上限，经 10 TB/s 片间 **NV-HBI**（NVIDIA High-Bandwidth Interface）连接，"providing one fully coherent chip"。
- 单芯片算力 20 petaFLOPS（原文 "highest compute ever on a single chip, 20 petaFLOPS"，精度口径未载明，按上下文为 FP4 类低精度）。

### 3.6 未提及清单（全文逐词检索确认）

SM 数量、频率、CUDA core 数、L1/L2/共享内存/寄存器文件容量、warp 调度器、**TMA**、**Tensor Memory（TMEM）**、**tcgen05/wgmma**、**thread block cluster/DSMEM**、mbarrier——简报**全部未载明**。"cluster" 一词仅以 server cluster/GPU cluster 的系统义出现。

## 4. 系统级上下文（简述，标注与 SM 的距离）

- **NVLink 5 / NVLink Switch（互连，距 SM 远）**：每链每方向 2 对差分对、有效带宽 50 GB/s（2× Hopper）；每 GPU 18 链、总带宽 1.8 TB/s（每方向 900 GB/s），"over 14X PCIe Gen5"；72-GPU 域聚合 130 TB/s；SHARP FP8 带来 4× 带宽效率；NVLink Switch 可扩展至 576 GPU；多机集群保持 1.8 TB/s，GB300 NVL72 GPU 吞吐为单台八卡的 9×。
- **NVL72 机架（距 SM 远）**：72 GPU + 36 Grace（18 compute tray + 9 switch tray，每节点 4 GPU/2 CPU），液冷，"acts as a single massive GPU"；GB300 NVL72 首创 GPU–ConnectX-8 间 PCIe Gen6 直连（省去独立 PCIe switch），每 GPU 800 Gb/s 网络带宽。
- **Decompression Engine（芯片级非 SM 单元，距 SM 近邻）**：专用解压引擎最高 800 GB/s，支持 LZ4/Snappy/Deflate；配 GB200 单 GPU 的 8 TB/s HBM3e 与 Grace NVLink-C2C，数据库查询 18× CPU、6× H100（TPC-H Q4 衍生负载口径）。
- **Confidential Computing（芯片级安全，距 SM 中）**：业界首款 TEE-I/O GPU；NVLink 在线加密（机密性 + 完整性）；加密模式吞吐"nearly identical"。
- **RAS Engine（芯片级可靠性，距 SM 中）**：专用 RAS 引擎 + AI 预测性维护，故障预判、备件激活、可计划维护窗口。
- **软件栈（距 SM 远）**：Dynamo 实时编排（expert/tensor/pipeline 并行切分，Pareto 前沿运行点滑动）、TensorRT-LLM、Nemo、Magnum IO、CUDA-X、AI Enterprise/NIM。
- **营销主张口径（引用须带脚注）**：GB300 NVL72 vs Hopper：50× AI 工厂产出 / 35× 推理性能 / 30× 能效 / 25× 每 token 成本，且 1.5× GB200 NVL72；GB200 NVL72：40× 工厂产出、GPT-MoE-1.8T 推理 30× H100、FP8 训练 4× Hopper（32,768 GPU 规模）；HGX B300 "7x more AI compute than Hopper"，HGX B200 144 PF。多数图注注明 "Projected performance subject to change"。

## 5. 与专题主题的关系

**作为规格锚点**：简报给出官方口径的第五代张量核心算力，可与目录内 arXiv 2512.02189（B200 对 H200 实测）对照：

| 精度 | 简报 B200 每 GPU（dense） | arXiv 2512.02189 实测 dense GEMM | 实测/简报 |
| --- | --- | --- | --- |
| FP4 | 9 PF | 7,700.2 TFLOPS | ≈86% |
| FP8 | 4.5 PF | 3,850.6 TFLOPS | ≈86% |
| FP6 | 4.5 PF（与 FP8 并栏） | 5,134.4 TFLOPS | 超出并栏值（6-bit 打包收益） |
| FP16 | 2.2 PF | 1,929.6 TFLOPS | ≈88% |
| INT8 | 4.5 POPS | 3,928.5 TOPS | ≈87% |

论文自报"96–99% of theoretical peak"，其理论峰值按 #SM×tensor ops/cycle×实测频率计算，与简报规格口径（更高 boost 条件）不同，两套百分比并存不矛盾。FP64 实测 44.8 TF 高于简报 Table 3 的 37 TF，口径参差，引用前建议对 datasheet。

**官方刻意不公开的 SM 微架构细节**（§3.6 清单）——SM 个数、频率、缓存/寄存器容量、TMA/TMEM/tcgen05/cluster 机制——正是本专题 10 件专利与 4 篇微基准论文的价值所在：简报给"是什么规格"，专利与论文回答"内部怎么做、实际怎么跑"。

## 6. 与本专题其他材料的关系

| 材料 | 提供什么 | 与本简报的互补 |
| --- | --- | --- |
| 本简报 | 规格数字（FP4/FP8/… 算力、HBM、NVLink 带宽）、官方术语（第五代张量核心、micro-tensor scaling、OCP microscaling、NV-HBI、TEE-I/O） | 术语与规格的唯一官方锚点 |
| arXiv 2512.02189（B200 深入） | TMEM 延迟/带宽实测、tcgen05 vs wgmma 流水对比、DE 吞吐、96% 峰值利用率 | 把简报"未载明"的 SM 内部补齐为实测行为；FP4/FP6 原生支持被硬件实测证实（OMMA/QMMA 指令） |
| arXiv 2507.10789（RTX 5080/GB203） | 消费级 SM_120 微基准（L1 128KB、寄存器 256KB、wgmma 不兼容） | 简报只覆盖数据中心线，消费级空白由该论文补 |
| arXiv 2402.13499 / 2501.12084（Hopper） | H100 基线（DSMEM/FP8/TMA 行为） | 为简报中所有 "nX vs Hopper" 主张提供独立基线参照 |
| 10 件专利（wgmma/TMA/mbarrier/DSMEM/寄存器配置/负载均衡等） | SM 内部机制的硬件实现与权利要求 | 简报 §3.4"new instructions"、§3.3 micro-tensor scaling 背后的执行单元，在专利族中才有结构描述 |

## 7. 评价与使用建议

**引用注意事项**：

1. **dense/sparse 口径**：表列均含两数；简报正文单引算力时（如"20 petaFLOPS"、HGX B200"144 petaFLOPS"）未标精度与稀疏性，按业界惯例此类营销数字通常取 sparse（较高者），此为推断而非简报明说。引用时须自行补全口径。
2. **FP4 口径**：FP4 峰值含 microscaling（块缩放）格式路径，与裸 FP4 MMA 不完全同义；Table 2/3 的 FP8/FP6 并栏意味着 FP6 没有独立规格数。
3. **内部数字不自洽（照实记录）**：GB300 NVL72 INT8 行作 "12/24 petaOPS"（GB200 对应行为 360/720），B300 每 GPU INT8 作 "0.15/0.30 petaOPS"，简报未解释缘由（或为 Ultra 产品线 INT8 降配，或为文档错误）；GB300 FP4 dense/sparse 作 1,080/1,440 PF，sparse 非 dense 的 2×（GB200 行 720/1,440 则是 2×）；机架口径折算每 GPU（720/72=10 PF）与 HGX 每 GPU（9 PF）不一致；HBM 容量有 279 GB 与 270 GB 两处表述；FP64 37 TF 与论文实测 44.8 TF 参差。以上建议交叉核对产品 datasheet 后再引用。
4. **预测性数字**：50X/35X/30X/25X 等均为 "Projected performance subject to change"，且绑定特定负载（DeepSeek-R1、GPT-MoE-1.8T、FP4、Dynamo+TRT-LLM、32K ISL/8K OSL），不可脱离口径转引。
5. **建议定位**：本简报仅作**术语与规格锚点**使用，不作 SM 机制依据；涉及机制（TMA、TMEM、tcgen05、cluster、同步原语）一律以专利解析与微基准论文为准。简报中可安全引用的硬事实集中在 §3.1/3.2/3.5（格式集、规格表、晶体管数/双 die/NV-HBI）。

---
*本文基于本地 PDF 全文提取（31 页通读）撰写，数字以原文为准。*

