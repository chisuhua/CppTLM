# arXiv 2507.10789 — Dissecting the NVIDIA Blackwell Architecture with Microbenchmarks（Blackwell 消费级 SM 微基准剖析）解析

> 分析日期：2026-08-20
> 本地 PDF：file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2507.10789_Blackwell微基准剖析.pdf （文本层完整）
> 在线版：https://arxiv.org/abs/2507.10789
> 文本依据：本地 PDF 全文提取（11 页通读）

## 1. 文献元信息

| 项目 | 内容 |
| --- | --- |
| 标题 | Dissecting the NVIDIA Blackwell Architecture with Microbenchmarks |
| 作者 | Aaron Jarmusch、Nathan Graddon、Sunita Chandrasekaran（schandra@udel.edu） |
| 单位 | University of Delaware, Dept. of Computer Information Sciences, Newark, US |
| arXiv 号与版本 | arXiv:2507.10789v2 [cs.DC]（提取文本所据版本） |
| 提交时间 | v2 日期 2025 年 7 月 21 日（v1 具体日期文本未载明，编号表明 2025 年 7 月） |
| 会议或期刊去向 | 未载明；正文称因 blind-review 政策暂不开源代码，暗示正处同行评审中 |
| 页码 | 11 页（IEEE 双栏会议版式） |
| 实测对象 | Blackwell：GeForce RTX 5080（GB203 芯片，即 SM_120/SM_120A 线）；Hopper 对照：H100 PCIe（GH100 芯片） |
| 驱动 / CUDA 版本 | 驱动版本未载明；软件栈提及 Hopper 的 wgmma/FP8 经 PTX 与 CUDA 11.8 支持，Blackwell 经 CUDA 12.8 + PTX 8.7 引入 tcgen05 与 FP4/FP6 操作数格式，FP4/FP6 mma 实测使用 CUDA 12.9（`.kind::f8f6f4`），引用 PTX ISA Release 8.8 与 CUDA Binary Utilities 12.9 |
| 致谢/资源 | NVIDIA 的 Nikhil Jain 答疑；University of Oregon Frank 集群；DOE 合同 DE-FOA-0003177（S4PST） |

## 2. 研究动机与问题

- **选型问题**：AI/HPC 对算力需求激增，NVIDIA、AMD、Intel、Google 各推专用加速器，"How do we determine which architecture best suits a given workload?" 既有手段（应用 profiling、roofline、解析性能模型、cache stall 预测）各有局限；微架构级剖析又受制于商用 GPU 文档不公开。
- **既有工作缺口**：微基准剖析传统自 Tesla/Fermi（[5][6]）、Kepler/Maxwell/Pascal 延续至 Volta/Turing/Ampere/Hopper（[7]–[9] 等），但"little has been published on the architectural features specific to Blackwell"；Accel-Sim、GCoM 等模拟工具缺乏对 Blackwell 新指令的支持。作者自称这是对 Blackwell 核心子系统（FP64 执行、低精度 MMA 单元、shared/L1/L2 内存吞吐）的**首个**微基准研究。
- **对照设计的内在张力**：GH100 面向大规模 AI 训练与科学计算（高 SM 数、HBM2e、大缓存），GB203 是消费级 Blackwell，面向游戏、渲染与小批量推理，以缓存规模与 FP64 能力换取更高频率与能效。两者"share a similar layout"但在硬件配置与内存层级上显著分歧，构成"同架构两极端"的对比样本。
- **待剖析的已知差异（Table I）**：GH100 每 SM 128 个 FP32 单元、64 个 INT32 单元（分管线），GB203 为统一 INT32/FP32 单元；FP64 每 SM 64 对 2；张量核心第四代（FP8 起）对第五代（增 FP4/FP6）；Transformer Engine 第一代对第二代。这些官方口径的差异哪些属实、哪些有隐藏行为，正是微基准要检验的对象。
- **贡献声明**：（1）一套针对 Blackwell 的新微基准并与 Hopper 对比；（2）深入分析内存层级与 SM 子单元（新一代张量核心、统一 INT32/FP32 核、FP64 核）；（3）给软件/应用开发者性能指南；（4）探究 FP4/FP6 低精度格式在张量核心上的行为与性能影响。

## 3. 方法学

**微基准实现与防优化**：全部用 PTX + CUDA 编写；PTX 内核写成独立文件、运行时由编译好的 CUDA 文件加载执行，以避免编译期优化；并逐一检查生成的 SASS 确认指令未被优化。

**四类测量手法**：

1. **延迟**：区分 True latency（串行依赖指令链，无重叠）与 Completion latency（独立指令可并行重叠），均以周期/指令计。时钟用只读特殊寄存器 `%clock64` 前后相减；基线开销 GB203 为 1 周期、GH100 为 2 周期（Fig. 1 给出 `mad.lo.s32` 测量代码）。
2. **吞吐**：每 SM 每周期完成指令数，由运行时间与指令数推得；INT32/FP32 单元测试中每个负载执行 1024 次取平均以消除噪声；张量核心部分用式 TFLOPS = 2·M·N·K / runtime。
3. **缓存/内存层级**：pointer-chase 随机串行访问测各级延迟（Fig. 6），延迟尖峰与缓存边界对应、与 Table II 规格一致；shared memory / L1 用可配置 stride 与 warp 数扫描（1–32 warps、stride 1 与 4、32 次访问、每点重复 1024 次取中位数）；L2 用每线程 1024 次全局 load/store 并按 warp 计时；全局内存用持续传输带宽测试；仅测设备侧访存，明确排除受 PCIe/NVLink 影响的 host-device 传输。
4. **调度行为**：依赖链长度 1→1024 扫描（图中示 1–64），通过调整循环次数控制总指令量，观察总周期与吞吐随 ILP 的变化（Fig. 2/3）；张量核心部分扫 ILP × warp 数（Fig. 4/5）。能耗用 `nvidia-smi` 采集。

**对照机选择及其局限**：H100 PCIe 对 RTX 5080 是"数据中心旗舰 vs 消费级次级 die"的不对称对比——SM 数、显存介质（HBM2e 80GB vs GDDR7 16GB）、FP64 配额、L2 分区都不同源。作者以"两者共享微架构相似性、可用同一基准策略"自辩。**关键口径警示：GB203 是消费级 Blackwell（SM_120/SM_120A），并非数据中心 Blackwell（GB100/SM_100，B200 一脉）**；论文自身也承认 tcgen05 在 SM_120A 上尚未获支持（Table IV 注），故其"第五代张量核心"结论实际只覆盖 mma.sync 路径，不能外推到 B200。

## 4. 关键发现

### 4.1 内存层级（L1/L2/global 延迟带宽、缓存结构）

**结构（Table II）**：L0 i-cache GH100 为独立分区、GB203 统一；寄存器文件均 256 KB/SM；L1（统一）GH100 256 KB/SM、GB203 128 KB/SM；shared memory GH100 228 KB（GB203 单元格原文缺失，仅注"unified"，由后文测得可配置上限 ≈99 KB/SM 补全）；L2 GH100 50 MB 双分区、GB203 65 MB 单分区（表头单位写作 MB/SM，照原文录）；显存 80 GB HBM2e vs 16 GB GDDR7。经 `cudaFuncAttributeMaxDynamicSharedMemorySize` 测得可配置共享内存上限 GH100 ≈227 KB/SM、GB203 ≈99 KB/SM；静态默认均为 48 KB/SM。

**延迟分层（Fig. 6，pointer-chase）**：两卡 L1 区命中延迟几乎相同，稳定在 30–40 周期；L2 区从 L1 末端延伸至 ≈30/60 MB；全局内存访问起点 GB203 ≈71 MB、GH100 ≈55 MB，对应延迟 ≈876.7 与 ≈658.7 周期——GH100 归因于 HBM2e 较 GDDR7 带宽更高、延迟更低。

**Shared memory / L1 行为（Fig. 7/8）**：低 warp 压力（1–4 warps）下 GB203 延迟更低，显示轻载路径更优化；6–32 warps 下 GH100 反超（更大容量或更稳健的 bank 冲突缓解）。stride 4 时 GB203 延迟陡增（bank 争用 + 小分区易饱和），GH100 平滑。L1 对 warp 数的延迟曲线比 shared memory 平；32 warps 时 shared memory 与 L1 延迟趋同。作者推测 GB203 的小 warp 数优势或来自 multiported banks 或 warp-aware 调度（原文为推测语气）。

**L2（Fig. 9）**：标准命中延迟 GB203 固定 ≈358 周期、GH100 ≈273 周期（双分区降低争用）；GH100 双分区饱和后（31–45 MB）升至 ≈508 周期，GB203 因 65 MB 大容量可将基线延迟维持到更深位置。warp 扩展：1–4 warps 时 GH100 每 warp 平均 ≈43.5k 周期优于 GB203 的 49k；GB203 单 L2 接口在 16 warps 处饱和（≈66k）；但 20 warps 处 GB203 追平并略超，32 warps 时 GB203 ≈128.4k 对 GH100 ≈128.9k——极端满载下 GB203 聚合 L2 带宽占优，GH100 受分区仲裁约束先到吞吐上限。

**全局带宽（Fig. 10）**：峰值读带宽 GH100 15.8 TB/s、GB203 8.2 TB/s；写带宽 2.2 对 1.6 TB/s（均偏向读密集设计）。**异常注记**：此二数值远超两卡公开规格的显存带宽量级，原文未交代测量口径，引用时需存疑（见第 7 节）。

### 4.2 计算管线与子核（指令吞吐、warp 调度器行为、分歧处理）

**统一 INT32/FP32（Table III）**：GB203 采用统一执行单元，可按指令混成动态调度，但同一周期只能作 INT32 或 FP32 之一。纯 INT32 与纯 FP32 的 true latency 两卡均为 4 周期（completion：GB203 16.97/7.97，GH100 16.69/7.86，Hopper 略优）；混合负载反转——GB203 mixed 1/2 true latency 15.96/26.28 对 GH100 31.62/43.54，说明统一核带来更高效的混合管线。另据 `%clock64` 测量，包裹混合指令序列时的基线开销取决于指令组合，作者认为这可反推指令工作流特征。注：GB203 首跑存在缓存未预热的高延迟异常，已从结果中剔除（Hopper 无此现象）。

**FP64**：GB203 每 SM 仅 2 个 FP64 单元，GH100 为 64 个（Table I）。1024 条依赖 FP64 指令 GH100 延迟远低于 GB203（8.04 对 63.57，Table III）；但只跑 2 条依赖指令时 GB203 降至 37.5 周期。作者推断这两个单元"仅用于类型与指令支持"，实际计算或由其他精度模拟（原文推测语气）。

**warp 调度器（Fig. 2/3）**：吞吐在前 1–9 条依赖指令内稳定爬升，其后两卡分化；短链时 GH100 总周期更低（更深的指令缓冲与更激进的调度器容忍依赖），8 条后两者总周期骤降（调度预热/管线效应）；GH100 曲线毛刺多、GB203 平稳——"GH100 tolerates short latency bound instruction sequences, whereas GB203 is optimized for more regular high-ILP kernels"。**分歧处理**：概览节称 Blackwell 改进了 warp 调度、降低 divergent 负载的 dispatch 延迟（Section III-1），但正文未见针对分支分歧的专项微基准，该说法未被直接实测。

### 4.3 张量核心（FP4/FP6 第五代、tcgen05 与 wgmma 的不兼容）

**代际与指令集（Table IV）**：第五代（GB203）支持 FP4、FP6、FP8、INT8、FP16、BF16、TF32、FP64，指令为 mma/wmma/tcgen05；第四代（GH100）支持 FP8 及以下，指令为 mma/wmma/wgmma。表注明"**TCGEN05 IS YET TO BE SUPPORTED FOR THE ARCH. SM 120A**"。原文明确：Hopper 的 wgmma 指令与 Blackwell 不兼容，warp-group 计算改用 tcgen05；但因 GB203 上 wgmma 不受支持、tcgen05 又尚未落地，**本研究的张量核心实测只能走两卡共有分母 mma.sync**。

**低精度 mma 实测（Table V）**：CUDA 12.9 中须显式 `mma.sync.aligned.kind::f8f6f4` 前缀加 `.m16n8k32.row.col.f32.<e2m1|e3m2|e2m3|e4m3|e5m2>...` 后缀；在 GH100 上使用或缺 kind 限定符均报 PTX 错。e8m0 仅作块内缩放指数未测。

**SASS 观察（重要发现）**：GH100 上所有 mma.sync 均译为 HMMA；GB203 上 FP8 两格式与 FP6 两格式均用新指令 **QMMA**；FP4 输入按 CUDA Binary Utilities 12.9 文档本应对应 **OMMA**，实测却是 QMMA；唯有 FP8 + ue8m0 块缩放时见到 OMMA——作者认为当前软件栈中 QMMA 是 FP4 输入的回退路径。

**tile 形状与精度编码**：每条 mma 指令以 M×N×K 指定 tile，如 `mma.sync.aligned.m16n8k32.f32.f16.f16.f32`（式 1）以 16×32 与 32×8 输入计算 16×8 输出 tile，后缀表示 FP16 输入、FP32 累加/输出；另有 m8n8k16、m16n8k64 等形状，提供更细粒度或更大的单指令操作数复用。

**warp 扩展与调度（Fig. 4/5）**：达到持续吞吐的最大 ILP：GH100 为 ILP=5 @ 29 warps，GB203 为 ILP=6 @ 25 warps——Blackwell 每线程可发更多独立 mma。ILP=1、warps=1 时 completion latency GB203 1.21094 周期、GH100 1.65625 周期，且各低精度格式相同，说明同架构内低精度 mma 共用一条执行管线。GB203 吞吐峰值 >11 TFLOP/s（ILP=6、32 warps），各格式吞吐与延迟全面优于 GH100；GH100 随 warp 增加呈阶梯状延迟上升（"更深但欠敏捷的调度队列"），需更多在飞 warp 才能喂饱执行单元。

### 4.4 能耗/能效观测与案例研究

**低精度功耗（Table VI）**：mma 负载下 GB203 功耗随精度降低而降——FP4 e2m1 16.753 W、FP6 e2m3 39.383 W、FP6 e3m2 46.723 W、FP8 e4m3/e5m2 46.661/46.806 W；GH100 不支持 FP4/FP6，两种 FP8 均 ≈55.8 W。"Blackwell's architectural efficiency at low precision" 体现为数值表达力与能耗的权衡。

**FP8 稠密 GEMM（cuBLASLt，D=Aᵀ·B+C，A/B FP8、C bf16、D FP8；Fig. 11/12，Table VII）**：32 MB workspace、每配置 100 次取均值，尺寸扫 1024/2048/4096/8192；另以 tile size 1–64 并追加 512 尺寸做功耗测试（各 tile 功耗相近，故按 tile 平均）。Hopper 全尺寸胜出且差距随规模扩大：8192³ 时 0.887 对 0.233 TFLOP/s（≈4×），Blackwell 该点运行时间 4.710 ms（因过大被从图中省略）；2048³ 0.554/0.191、2048×2048×4096 0.674/0.192、2048×4096×8192 0.759/0.217、1024³ 0.239/0.134 TFLOP/s。功耗上 Hopper 平稳 58–60 W、峰值 68 W；GB203 均值 >80 W、峰值 114.4 W，N=K=8192 组合出现尖峰，仅 512³ 小尺寸显著省电。作者归因于 Blackwell 上 FP8 GEMM 的内核选择/调度尚不稳定，Hopper 编译器启发式更成熟——与微基准层面 Blackwell 张量核心占优形成软件栈反差。

**Transformer 推理（TensorRT + GPTneox；Table VIII）**：平均功耗（W）Hopper/Blackwell：FP32 60.24/58.82、FP16 57.64/47.78、FP8 57.69/45.14、Best 60.15/61.03。Hopper 各精度功耗平稳；Blackwell 随精度下降省电明显，但 "Best" 配置反而升功耗。结论：Hopper 能效更稳，Blackwell 调优后可在推理上获得更好功耗效率。

## 5. 与专题主题的关系

- **对理解 SM_120 内部实现的直接价值**：这是目录内唯一一篇对消费级 Blackwell（SM_120/SM_120A）SM 子系统做逐部件实测的材料——统一 INT32/FP32 核、2 单元 FP64、128 KB 统一 L1、65 MB 单体 L2、30–40 周期 L1 命中、QMMA/OMMA SASS 分工，均是官方简报不披露的微架构细节。
- **与专利机制的印证**：寄存器文件两代均 256 KB/SM（Table II）与 SM 子核/寄存器组织类专利的一般设定一致（见 US20240378089A1 解析）；L1/shared 统一空间、可配置上限（≈227/99 KB）与 48 KB 静态默认的实测，为 SMEM 供数路径类专利（见 US20230289398A1 解析中 operand collectors 自 SMEM 直取的描述）提供了消费级硬件侧的行为参照。
- **与专利机制的断裂**：论文实证 Hopper wgmma 在 GB203 上不可用、tcgen05 为替代但在 SM_120A 尚未落地——wgmma 专利所代表的"warp group 状态机 + RF/SMEM 供数"范式（见 US20230289398A1 解析）在消费级 Blackwell 上发生 ISA 断裂；QMMA/OMMA 等新 SASS 的出现则提示第五代量核心内部数据通路已重组。
- **ISA 演进时间线（软件兼容视角）**：论文给出官方口径——Hopper 的 wgmma 与 FP8 算术经 PTX + CUDA 11.8 引入并保持对传统 mma/CUDA C++ 的向后兼容；Blackwell 由 CUDA 12.8 + PTX 8.7 扩展 tcgen05 与 FP4/FP6 增强操作数类型。这条时间线可与目录内 2022–2024 年 Hopper 专利群的申请/公开节奏互参。
- **SMEM 配置参数的实测补全**：可配置共享内存上限 ≈227 KB/SM（GH100）与 ≈99 KB/SM（GB203）、静态默认 48 KB，是官方白皮书常不写明而内核调优必需的参数，与 SMEM 供数/bank 类专利（见 US20230289398A1 解析）构成"机制—参数"两层互补。
- **缺口**：论文未测量 TMA 异步拷贝、mbarrier 事务屏障、分布式共享内存等 Hopper/Blackwell 异步引擎（未载明任何相关实验）——目录内 US20230289304A1、US20230289242A1、US20250173152A1 等解析所覆盖的机制在本文中无实测对应，属后续工作空间。
- **消费级 vs 数据中心口径警示（再次强调）**：本文一切"Blackwell"结论均限于 GB203/SM_120 消费线；数据中心线（GB100/SM_100）的 tcgen05 主形态、TMEM 与双 die 互联完全不在样本内，迁移结论必须另行验证。

## 6. 与本专题其他材料的关系

- **与 arXiv 2512.02189（Blackwell 微基准深入，B200 深入版）互补**：该篇以数据中心级 B200 为对象（见对应解析），本文则以消费级 RTX 5080 为对象；两者合读可区分"Blackwell 架构设计"与"产品线裁剪"各自造成的差异——本文 GB203 上的观察（如 mma 路径限制、FP64 阉割）哪些是架构本质、哪些是消费级裁切，需 B200 数据对照方能定论。
- **与两篇 Hopper 微基准论文（arXiv 2402.13499、arXiv 2501.12084）的关系**：本文将其中后者（Luo et al.）列为参考文献 [9]，方法学承袭"pointer-chase + 依赖链 + 张量核心扫描"一脉；不同在于两篇 Hopper 论文单机深剖，本文以 GH100 为活对照重测，所得 Hopper 数字（如 L2 ≈273 周期）可与两篇 Hopper 剖析互检一致性。
- **与官方技术简报（NVIDIA Blackwell Architecture Technical Brief v2.1，同目录）的规格互证**：定性面一致——第五代张量核心支持 FP4/FP6、第二代 Transformer Engine、L1/shared 统一等与官方定位吻合（论文亦引用 RTX Blackwell 白皮书为 [13]）；定量面则需警惕：本文 Fig. 10 全局读带宽 15.8/8.2 TB/s 与消费级 GDDR7 卡的公开带宽量级明显不符，建议以简报/白皮书规格为准、将本文该数视为待核口径。
- **与研究综述**：本文为综述中"消费级 Blackwell 实测"条目提供一手数字来源（见同目录研究综述.md）。

## 7. 评价与局限

**方法学优点**：PTX 独立内核 + SASS 逐一核验，防编译器干扰的手段明确可复核；true/completion 双延迟口径、ILP×warp 双维扫描、pointer-chase 分层定位，方法谱系完整且承袭 Hopper 剖析传统；案例研究（cuBLASLt GEMM、TensorRT 推理）把微基准结论落到真实软件栈，暴露了微基准优势 ≠ 应用优势的落差（FP8 GEMM 上 Blackwell 反输）。

**方法学缺点与异常**：
- **样本单一**：Blackwell 侧仅一片消费级 RTX 5080，全部"Blackwell"结论不可外推至 SM_100/B200；Hopper 侧仅 PCIe 版 H100（非 SXM），同样带偏对照。
- **张量核心对比不对称**：wgmma 与 tcgen05 都不可测，只能以 mma.sync 公共分母比较，低估了两代张量核心真实形态差异；FP4 走 QMMA 回退也说明软件栈未定型，结论时效性强。
- **存疑数字**：全局带宽 15.8/8.2 TB/s（见第 6 节）；FP64 双单元"仅为类型支持、计算靠模拟"属推测；GB203 低 warp 优势归因 multiported banks 亦属推测。
- **文本瑕疵**：Table III 标题误作"GH100 VS GH203"；Table II 中 GB203 shared memory 数值单元格缺失；Table VI 标题称"POWER USAGE/PERFORMANCE PER WATT"但数值仅为功耗。
- **覆盖缺口**：未测 TMA/mbarrier/异步流水、分支分歧、host-device 传输（作者自陈排除）；功耗仅 nvidia-smi 板级读数，无 SM 级能耗分解。
- **可复现性**：代码因评审政策暂未公开。

**总体定位**：作为首篇 Blackwell 微基准剖析，其价值在于把 SM_120 的内存层级与执行管线参数从"未载明"变为"可引用"，并实证了 wgmma→tcgen05 的 ISA 断裂与 QMMA/OMMA 分工；但单一消费级样本、未定型的软件栈与个别存疑数字决定了它更适合作假设生成器与后续 B200 深剖的对照基线，而非 Blackwell 架构的最终定论。

**作者自评（结论节）**：论文自述完成了对 GB203 内存层级、SM 执行管线与第五代张量核心的系统性实验刻画，强调低精度格式（FP4/FP6）对功耗与性能效率的实际影响，并定位为应用开发者/编译器作者/性能工程师在 Blackwell 平台调优的"可行见解"（actionable insights）；方法学本身也被主张可作为未来架构张量核心评估的参考框架。

---
*本文基于本地 PDF 全文提取（11 页通读）撰写，数字以原文为准。*

