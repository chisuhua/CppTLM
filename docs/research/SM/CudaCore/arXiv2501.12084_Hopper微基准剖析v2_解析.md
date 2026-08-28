# arXiv 2501.12084 — Dissecting the NVIDIA Hopper Architecture through Microbenchmarking and Multiple Level Analysis（Hopper SM 多层级微基准剖析）解析

> 分析日期：2026-08-20
> 本地 PDF：file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2501.12084_Hopper微基准剖析v2.pdf （文本层完整）
> 在线版：https://arxiv.org/abs/2501.12084
> 文本依据：本地 PDF 全文提取（33 页通读）

## 1. 文献元信息

| 项目 | 内容 |
| --- | --- |
| 标题 | Dissecting the NVIDIA Hopper Architecture through Microbenchmarking and Multiple Level Analysis |
| 作者 | Weile Luo、Ruibo Fan、Zeyu Li、Dayou Du、Hongyuan Liu（均 HKUST-GZ）；Qiang Wang（哈尔滨工业大学深圳，共同通讯）；Xiaowen Chu（HKUST-GZ，共同通讯） |
| 单位 | 香港科技大学（广州）＋ 哈尔滨工业大学（深圳） |
| arXiv 号与版本 | arXiv:2501.12084v2 [cs.DC]，v2 修订水印日期 2025-09-04（提取文本行 "arXiv:2501.12084v2 [cs.DC] 4 Sep 2025"） |
| 期刊去向 | 正文含 ACM 版权页且页脚反复出现 "Manuscript submitted to ACM"，ACM Reference Format 为占位模板（J. ACM 37, 4, Article 111、"Received 20 February 2007; revised 12 March 2009; accepted 5 June 2009" 均为 ACM 模板样例文本而非真实录用信息）——即投稿中、尚未见刊 |
| 前作 | "Benchmarking and Dissecting the Nvidia Hopper GPU Architecture"，IPDPS 2024（arXiv 2402.13499，pp. 656–667，doi:10.1109/IPDPS57955.2024.00064） |
| 实测 GPU | A100 PCIe（Ampere，CC 8.0，108 SM×64 core，最高 1410 MHz，40MB L2，HBM2e 1555 GB/s）、RTX 4090（Ada，CC 8.9，128 SM×128 core，最高 2520 MHz，72MB L2，GDDR6X 1008 GB/s）、H800 PCIe（Hopper，CC 9.0，114 SM×128 core，最高 1755 MHz，50MB L2，HBM2e 2039 GB/s）（Table 2） |
| 软件栈 | RTX 4090：driver 530.30.02 + CUDA 12.1；A100 与 H800：driver 560.35.03 + CUDA 12.6 |
| 工件 | https://github.com/HPMLL/NVIDIA-Hopper-Benchmark |

## 2. 研究动机与问题

作者从三处缺口立题（§1）：

1. NVIDIA 两年一代但微架构规格披露极少，性能/功耗建模、GPU 模拟器与应用优化都缺乏可靠参数，"开发者无法充分利用现代 GPU 的潜力"；
2. 张量核心自 Volta 起逐代增强，既有研究多停留在 Ampere/Turing 的汇编分析，Hopper 新增 wgmma 与扩展精度集后"需要 Hopper 专属的 TC 研究"；
3. Hopper 新特性 DPX、分布式共享内存（DSM）TMA 中，"TMA 的实现细节与性能特征在既有文献中基本未被探索"。

"multiple level analysis" 的定位是把每个关键部件沿**指令级 → 库级 → 应用级**三层打通：张量核心从 PTX 指令延迟/吞吐测到 Transformer Engine（te.Linear / te.TransformerLayer）再测到 Llama 真实推理；DSM 从接口开销、访问模式测到 histogram 应用；DPX 从函数级测到 Smith-Waterman 生物序列比对；TMA 从单指令吞吐测到 GEMM 流水线。自陈三项贡献：系统性横评 Ampere/Ada/Hopper 的延迟与吞吐；关键部件多层级分析（含 LLM 生成应用与 HPC 访问模式）；全面探索 L2 分区缓存、wgmma、DPX、TMA、DSM 等 Hopper 独有特性。Table 1 相对前作 [25] 的增量标注为：L2 分区缓存、wgmma 能耗、DSM/TMA/DPX 的应用级评测；作者称这是"对 Hopper 架构最全面深入的基准测试"。

## 3. 方法学

- **传统 P-chase**（§4.1）：随机步长指针追逐（引 [37,38,47]），64 字节粒度读，逐步加大数组捕捉延迟拐点（每点为百万次以上访问均值）；作者指出随机步长较均匀步长更能暴露双分区 L2 的存在。
- **细粒度 P-chase**（引 Mei & Chu 2017 [29]）：均匀 32 字节步长逐次访问记录每次延迟，配合 K-means 聚类分离延迟组；四条假设——两个 L2 分区容量相等、数组远小于半个 L2 时访问不出分区、SM 与分区物理距离不同导致近/远差异、超出缓存容量可解释范围的延迟归于全局内存 miss。
- **吞吐测试**（§4.2）：L1 用 `ca` 修饰符预热后单块 1024 线程反复访问；共享内存同以 1024 线程块内访问；L2 用 `cg` 预热、块数 = 2×SM 数；全局内存分配远超 L2 容量以绕过硬件预取、float4 向量化读 5 写 1、块数 = 4×SM 数；前三级同时测 float 与 float4 两种访存宽度。
- **TC 微基准**（§6.2）：PTX 级发射并反汇编至 SASS（SASS 对照表在附录 A.2 Table 14）；mma 每 SM 单 warp、wgmma 单 warp group（4 warps），kernel 内重复 1024 次；测"完成延迟"（发射到结果可用）；吞吐按 Total_OPS/Duration 而非总周期数计（规避执行中变频，显式区别于 Sun et al. [44]）；零初始化与随机初始化分别测，以分离功耗墙影响。
- **TMA 测试**（§5）：延迟 = 发射异步 TMA 指令 + 设置 Mbarrier 期望事务 + Mbarrier arrive/wait 的端到端时间；吞吐为 4GB 数据 global→shared 搬运，变量为每指令搬运尺寸（非张量 1/2/4/8/12/16KB、1D Tensor 0.5–2KB、2D 形状 16×16 至 64×64、3D 形状 8×8×4 至 16×16×16）× CTA 数（114/228/342/456，即 1–4×SM 数）。
- **DSM 测试**（§7）：`mov.u32 %0, %%smid` 确认块落在不同 SM；三种访问模式（ring/pair/broadcast，对应 ring-allreduce、butterfly、广播类 HPC 场景）× 三种调度策略（Default/Spread/LoadBalancing）；50160 块、每块 16KB 共享内存；另以 CUDA `cluster.map_shared_rank` / PTX `mapa` 接口量化地址映射开销。
- **对照架构选择**：取三代代表 A100 PCIe / RTX 4090 / H800 PCIe 横评，既隔离代际差异也把 Hopper 硬件加速路径与前代软件模拟路径（DPX）对照。局限是未测全规格 H100 SXM（H800 为 350W 功耗墙的特供变体），且驱动/CUDA 版本跨卡不一致。

## 4. 关键发现

### 4.1 内存层级：双分区 L2 的延迟解剖（§4，Fig. 2、Table 3/4/5）

- 传统 P-chase（Fig. 2、Table 3）：L1 三卡均 32–33 cycles；共享内存 29–31 cycles（与 L1 物理同体）；L2 在 RTX 4090 呈单一延迟（正文约 284.8、Table 3 记 273.0 clocks），而 A100 呈 202.8→408、H800 呈 264.5→502 的**两段延迟**，第三、四拐点分别对应半个 L2 与整个 L2 容量；全局内存延迟 A100 566、H800 656、RTX4090 571 clocks。全层级看，L2 约为 L1 的 6.5×、全局内存约为 L2 的 2.1×。
- 细粒度 P-chase + K-means（Table 4）把双分区结构解成四类：A100 近命中 208.0 / 远命中 356.6 / 近 miss 474.9 / 远 miss 622.7；H800 近命中 258.0 / 远命中 414.1 / 近 miss 555.5 / 远 miss 743.7（单位均 cycles）。
- 交叉验证：传统 P-chase 第一、二拐点间延迟（A100 202.8 / H800 264.5）与近命中值吻合；传统口径的全局内存延迟≈(近 miss+远 miss)/2，与数据在两个分区间概率分布一致——作者据此明言**推翻短篇版基于传统 P-chase 的 L2 数据与结论**。
- 吞吐（Table 5）：H800 L1 约 125.8、共享内存约 127.4 byte/clk/SM，与前代同设计一致；**H800 L2 吞吐 4472.3 byte/clk（FP32），为 RTX4090 的 2.6×、A100 的 2.2×**；全局内存实测达理论峰值 90–92%；L2/全局内存吞吐比三卡分别为 4.67×、2.01×、4.23×。
- 洞察（原文 Insight 框）：Ampere/Hopper 的 L2 与 GPC 均为双分区对齐设计，访问近端分区最优；RTX4090 以 FP32（非 float4）访 L1/SMEM 时吞吐减半，系显存 I/O 节流所致。**论文未做 TLB 测试（全文未载明）**。

### 4.2 TMA 与异步执行（§5，Fig. 2/3/4/5）

- **延迟**（Fig. 2）：TMA 访问曲线比普通访存**少一个拐点**——TMA 不经 L1、只受 L2 影响；过第一拐点后趋势与全局内存一致，但整体**高出约 170 cycles**，归因于 TMA 单元开销与同步等待（mbarrier）。这是全文对 mbarrier 等待开销的唯一量化口径：端到端包含值，**未单独隔离 mbarrier arrive/wait 延迟**。
- **吞吐**（Fig. 3）：非张量、2D、3D 场景均可达 1800 GB/s 以上（与 §4.2 全局内存带宽相当）；1D Tensor 受 tensor map 对共享内存 box 每维 ≤256 元素的限制（即使 int64 也仅 2KB 上限），建议一维搬运改用非张量接口；3D 场景下每载 >8KB 时块数越多性能反而越低（box 约束）。
- **形状敏感性**（Fig. 4，16KB 每载、六种 box 形状）：同尺寸下 **x 轴维度越大吞吐越高**且不受块数增多拖累；增大 y/z 轴显著降低吞吐，块数增多时进一步恶化——论文给出"优先拉长 x 维"的 TMA 参数选择准则。
- **应用级 GEMM**（Fig. 5，基于 CUTLASS Hopper 教程改造；CTA tile M/N/K=128/256/64、wgmma tile 64×128×16、流水深度 2）：n<2000 时 TMA 开关差异不大；n≥2000 起 TMA 优势显现；n>7000 后 TMA 版稳定在**接近 600 TFLOPS**（FP16），无 TMA 版约 450 TFLOPS——即摘要所称 TMA 异步编程模型给矩阵乘带来 1.5× 加速。Insight：TMA 须配合 warp specialization 才能充分发挥；load 形状须与流水的内存-计算重叠协同设计。

### 4.3 张量核心与 wgmma（§6，Table 6–11、Fig. 6/7、附录 Table 14/15）

- **mma（同步）**（Table 7）：A100 达理论峰值 95% 以上；RTX4090 因实测频率 2710 MHz 高于官方 boost 而"超峰值"；**H800 上 mma 平均仅达理论峰值 62.9%**；典型延迟如 FP16 m16n8k16 在 H800 为 24.1 cycles（A100 24.6）；H800 稀疏 mma 对稠密平均仅 1.42×，而同场景 RTX4090 可达约 2×——说明同步稀疏路径未吃满 Hopper 稀疏张量核心。
- **wgmma（异步）**（Table 8）：零初始化下达理论峰值 95% 以上；m64n256k16（FP16）完成延迟 128 cycles；SS（A/B 均取自共享内存）与 RS（A 取寄存器）模式延迟吞吐几乎一致——异步执行与高计算密度有效隐藏共享内存延迟。随机初始化下 FP16 计算 FP32 累加性能下降最明显，原因是逼近 H800 PCIe 350W 功耗墙后降频。
- **N 维敏感性**（Table 9）：N≥64 时各精度均接近峰值；N<64 时 SS 模式延迟高于 RS、吞吐低于 RS——计算密度不足以隐藏共享内存访问延迟。建议 N≥64（如 N=256 稀疏 RS 可达 1476.2 TFLOPS）。
- **稀疏 wgmma**：SS 模式延迟 144 cycles（RS 128，Table 15）；SS(Rand) 相对稠密约 1.8×、RS(Rand) 超 1.9×；SS 劣于 RS 的机理——SS 从共享内存读 m×k 全量并按 metadata 做 2:4 剪枝，访存需求翻倍、延迟无法被计算掩盖，与稠密 wgmma 的 SS/RS 等效行为形成反差。
- **能效**（Table 10/11，≥20 亿次重复、稳态后采样）：mma 口径 H800 平均能效为 A100 的 1.60×、RTX4090 的 1.69×（稠密）、1.33×/1.39×（稀疏）；wgmma 持续执行时核心频率跌破白皮书 1620 MHz——零输入功耗 <200W，随机输入顶到 350W 阈值；稀疏指令引起大幅降频，故其性能达不到稠密的 2×；**wgmma 稠密/稀疏平均能效仅为 mma 稠密/稀疏的 0.67×/0.78×**，相对 Ampere mma / Ada mma 仍为 1.05×/1.10×。结论：要极限性能选 wgmma，要能效与跨代兼容选 mma。
- **库级/应用级**（Fig. 6/7、Table 12/13、附录 Fig. 16）：te.Linear 小矩阵（N=4096）kernel 仅占执行时间 25.3%，N=16384 时升至 84.7%——FP8 的量化/格式转换开销随规模摊薄，N=16384 时 FP8 吞吐接近 FP16 两倍；te.TransformerLayer 在 hidden_size>4096 后 FP8 优于 FP16 但达不到 2×（Softmax/GeLU 未量化、FlashAttention 不走 FP8 TC）；Llama 推理（decode-only、memory-bound，batch=8、输入/输出各 128 token）中 FP8 相对 BF16 无增益甚至略降（如 H800 llama-2-7B：BF16 432.91 vs FP8 428.15 tokens/s）。
- **SASS 侧写**（附录 Table 14/15、Listing 1）：wgmma 编译为 HGMMA/QGMMA/IGMMA/BGMMA 新指令族（FP8 双变体 E5M2/E4M3 均可编程）；**INT4 mma 在 Hopper 被编译为 IMAD 序列落到 CUDA core**，不达张量核心性能；wgmma 无前瞻兼容包袱故不支持 INT4；完整编程范式为 fence.sync.aligned → mma_async → commit_group → wait_group 的六步结构，从 warp 级（32 线程）升为 warp group 级（128 线程）。

### 4.4 分布式共享内存 / cluster（§7，Fig. 8–11）

- **延迟**：本地共享内存直访 29 cycles；经 DSM 接口访问**本块**共享内存 33 cycles（接口自身开销，即使不跨 SM）；cluster=2 时跨 SM 访问 181 cycles，比 L2 低约 30%；经全局内存中转需 1110 cycles（一 store 一 load），DSM 缩短 **6.13×**，接近官方宣称的 7×；cluster 2→16（Hopper 上限）跨块延迟 184–213 cycles。
- **吞吐**：SM 内共享内存理论 1755 MHz×128B=225 GB/s；DSM 接口访本块仅 205 GB/s（80%），不用 DSM 接口则 >99.8%（对照 Table 5）。调度策略上，LoadBalancing 允许同簇块落同一 SM，多数场景略优于 Default/Spread；broadcast 模式吞吐随簇增大显著下降，ring/pair 差异小。簇大小=2 时峰值约 **3.28 TB/s**，簇=4 降至 2.78 TB/s（SM-to-SM 带宽竞争加剧）；块过小（64 线程）时靠提升 ILP 补救，块足够大时 ILP 无增益。
- **应用级 histogram**（Fig. 11）：cluster 1→2 有增益、继续增大反而下降（广播型争用，呼应 Fig. 9）；增大 block size 显著提升吞吐（弥补低 ILP）；最优簇尺寸随 block size 变化（block=128 时 CS=4，block=512 时 CS=2）；用 DSM 比不用提升 30% 以上（block=512 口径）。

### 4.5 DPX 动态规划指令（§8，Fig. 12/13、附录 Fig. 14）

- 指令级（Fig. 12）：A100/RTX4090 为软件模拟、性能相近；H800 对 16 位操作最高加速 **13×**、relu 类显著更优；但简单操作（如 `__viaddmax_S32`）三卡接近——SASS 显示 Hopper 用新指令 VIMNMX 替代 IMNMX，性能未见显著提升；`__vibmax_S32` 在 A100/4090 上被编译器直接优化为 max 指令而无独立数据。吞吐随块数在 SM 数整数倍处达峰、越界骤降再回升，据此推断 **DPX 加速单元位于 SM 级**。附录 Fig. 14：`__viaddmax_s16x2` 等函数在 H800 上延迟降幅 70–80%。
- 应用级 Smith-Waterman（Fig. 13，沿用 CUDASW++4.0 [39] 配置，20 条长 144–5478 的蛋白序列对百万条长 1024 的模拟库，指标 GCUPS）：不走 DPX 的 half2/float 以 RTX4090 最高（half 精度 4987.4 GCUPS）；S32 三卡排序 4090>H800>A100，同卡 compute_80 与 compute_90 编译无差；**S16 在 compute_90（硬件加速）下显著领先**，相对 compute_80/A100/RTX4090 达 **4.75×**。
- 洞察：更复杂操作（多操作数、带 ReLU 钳位）获益更大；16 位加速明显而部分 32 位无加速；前 Hopper 架构上除非受内存约束应避免 S16、优先浮点。

### 4.6 关键实测数字速览

| 指标 | 数值 | 出处 |
| --- | --- | --- |
| H800 L2 近命中 / 远命中 / 近 miss / 远 miss | 258.0 / 414.1 / 555.5 / 743.7 cycles | Table 4 |
| H800 全局内存延迟 | 656 cycles | Table 3 |
| H800 L2 吞吐（FP32） | 4472.3 byte/clk（A100 的 2.2×） | Table 5 |
| TMA 端到端附加延迟（含 mbarrier 同步） | ≈170 cycles | Fig. 2 |
| TMA 峰值吞吐（非张量/2D/3D） | >1800 GB/s | Fig. 3 |
| GEMM FP16（n>7000）有/无 TMA | ≈600 / ≈450 TFLOPS | Fig. 5 |
| wgmma m64n256k16 完成延迟（SS/RS） | 128 cycles | Table 8 |
| 稀疏 wgmma SS / RS 延迟 | 144 / 128 cycles | Table 15 |
| H800 mma 平均达峰比 | 62.9% | §6.2 |
| wgmma 能效 vs mma（稠密/稀疏） | 0.67× / 0.78× | Table 10/11 |
| DSM 跨 SM 延迟（cluster=2） | 181 cycles | §7.1 |
| DSM vs 全局内存中转 | 6.13×（1110→181 cycles） | §7.1 |
| DSM 峰值吞吐（cluster=2 / 4） | 3.28 / 2.78 TB/s | Fig. 10 |
| DPX 16 位最大指令级加速 | 13× | Fig. 12 |
| Smith-Waterman S16（compute_90）加速 | 4.75× | Fig. 13、§9 |

## 5. 与专题主题的关系

本论文是目录内 10 件 Hopper/Blackwell SM 专利集群**最系统的独立行为学印证**——专利给出机制设计，本文给出真实硅片上的可测行为：

1. **TMA（对应 US20230289304A1 TMAU 核心案、US12020035B2 多播案、US20240176663A1 描述符缓存案）**：专利的"tensor descriptor + 单指令异步搬运"在 §5 被逐字实测——Copy Descriptor/Tensor Map 即 304 案三层参数模型的软件面；"TMA 不经 L1"（Fig. 2 少一个拐点）印证 TMAU 作为每 SM 紧耦合单元走独立数据通路、绕过常规 load 路径的设计；x 轴维度越大吞吐越高的形状敏感性（Fig. 4）与 304 案按连续行/swizzle 布局拆子请求的搬运逻辑一致；约 170 cycles 的端到端附加延迟与 GEMM 1.5× 收益则是专利不载、只能实测获得的量化补充。多播（US12020035B2）与描述符缓存（US20240176663A1）未被单独隔离测试，属未覆盖项；白皮书宣称的"TMA 地址翻译效率"仅间接观测，未直接量化。
2. **mbarrier（对应 US20230289242A1 / 授权 US12536056B2）**：242 案的事务字节计数（expect_tx + arrive-on）在本文即 TMA 延迟测试中"初始化 Mbarrier 期望事务 + arrive/wait"流程；实测口径为端到端包含 mbarrier 同步的 ~170 cycles 附加开销，未单独隔离屏障等待延迟——专利的 SYNCS 加速单元（barrier cache / try-wait buffer）行为仍是黑盒。
3. **wgmma（对应 US20230289398A1）**：吻合度最高的一组。专利"单指令驱动 warp group 异步执行、B 读一次多播、A 可绕过寄存器文件直取共享内存"在实测中逐项对应：m64n256 单指令形态、128 cycles 完成延迟（专利 FIG. 6A "H100 TC Instruction 64k MACs, 32 cycles" 的吞吐面印证）、SS/RS 等效（SMEM 直供操作数的延迟被异步流水隐藏）、fence/commit/wait 序列与 Listing 1 完全同构。实测还给出专利不会披露的负面边界：mma 兼容路径仅 62.9% 峰值、稀疏 SS 劣于 RS、350W 功耗墙降频。
4. **DSMEM/cluster（对应 US20250173152A1 及其母案）**：专利的"GPC 内专用 SM2SM 网络"实测为 cluster=2 时 181 cycles 跨 SM 延迟与 3.28 TB/s 峰值吞吐；"接口本身有开销"（本地 DSM 访问 29→33 cycles、吞吐降至 80%）对应专利中地址映射（mapa/rank 翻译）与路由查询路径；LoadBalancing 策略下同簇块可落同一 SM 而提速，则从侧面印证 cluster 调度与 GPC 内 SM 拓扑的耦合（与 US20230289211A1 负载均衡案的 CGA 准入语境相接）。
5. **官方宣称校验**：DSM 相对全局内存中转 6.13× ≈ 官方 7×；DPX 16 位 13×、S16 Smith-Waterman 4.75×、FP8≈2×FP16 均为首见系统量化。

## 6. 与本专题其他材料的关系

- **与 arXiv 2402.13499（IPDPS 2024 短篇，目录内论文 3 号）**：本文为其全面深化。增量按作者自陈（首页脚注）：新增 L2 分区缓存测试（并**推翻**短篇版基于传统 P-chase 的旧数据与结论）、wgmma 能耗测试、DSM 全面评测（接口开销、簇规模扩展、访问模式 × 调度策略、histogram 应用）、DPX 应用级测试、TMA 异步编程基准。作者从 6 人增至 7 人（新增 Hongyuan Liu）；短篇版的 DPX/DSM/FP8 初测数据在本文中全部重测并扩展。
- **与两篇 Blackwell 论文的代际衔接**：本文的 H800 基线是 arXiv 2507.10789（RTX 5080/GB203 对 H100 PCIe）与 arXiv 2512.02189（B200 对 H200）的直接前序——三篇构成 Hopper→Blackwell 的连续实测序列。本文确立的 wgmma 行为（异步状态机、累加器在 warp group 寄存器文件、SS/RS 操作数来源）正是 Blackwell 两篇刻画 tcgen05/TMEM 断裂时的对照系：发射方从 warp group 收为单线程、累加器迁往 TMEM；本文的 P-chase/TMA/DSM 方法学也被 Blackwell 篇沿用（2512.02189 以指针追逐测 TMEM 延迟即同源手法）。
- **与官方白皮书口径**：Fig. 1 直接引 Hopper 白皮书（GTC22）的 50MB 分区 L2、256KB L1/SMEM、cluster 映射 GPC 等规格作为实测靶标；wgmma 持续执行频率跌破白皮书 1620 MHz 的观测，是民间实测对官方规格的典型修正案例。

## 7. 评价与局限

**价值**：33 页篇幅在 GPU 微基准文献中属深档——同一部件从指令到应用三层打通（wgmma 能效分解与 TMA 形状敏感性两项为首见）；方法学上对分区 L2 的传统 P-chase 失效分析（细粒度 P-chase + K-means 四类分解）可复用于后续分区缓存架构；工件全部开源。对本专题而言，它是把 10 件专利的机制叙事"落到数字"的唯一系统性独立来源。

**局限**：

1. 单卡、且为 H800 PCIe 而非 H100 SXM——350W 功耗墙使 wgmma 随机数据下的性能与能效数据带强烈散热/功耗口径，不可直接外推 SXM 版；
2. 驱动/CUDA 栈跨卡不一致（530/CUDA12.1 vs 560/CUDA12.6），横评公平性存疑；
3. 多处机制仍是黑盒：mbarrier 等待延迟未隔离、TMA 地址翻译效率仅间接观测、多播与描述符缓存未测、L2 分区到地址的映射规则靠假设反推、DPX 硬件单元仅有 SM 级定位推断；
4. 论文自承认部分传统基准不适配 Ampere/Hopper 而需重做（L2 结论即被自我推翻），提示同类测量对驱动与硬件配置高度敏感；
5. 排版遗留 ACM 模板占位文本（假 DOI、2007–2009 样例日期），期刊录用状态以页面 "submitted" 为准，引用时应注意其尚非正式期刊版本。

---
*本文基于本地 PDF 全文提取（33 页通读）撰写，数字以原文为准。*

