# arXiv 2402.13499 — Benchmarking and Dissecting the Nvidia Hopper GPU Architecture（Hopper SM 微基准剖析）解析

> 分析日期：2026-08-20
> 本地 PDF：file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2402.13499_Hopper微基准剖析.pdf （文本层完整）
> 在线版：https://arxiv.org/abs/2402.13499
> 文本依据：本地 PDF 全文提取（12 页通读）

## 1. 文献元信息

| 项目 | 内容 |
| --- | --- |
| 标题 | Benchmarking and Dissecting the Nvidia Hopper GPU Architecture |
| 作者 | Weile Luo、Ruibo Fan、Zeyu Li、Dayou Du（均 HKUST(GZ)）；Qiang Wang（哈尔滨工业大学（深圳），通讯）；Xiaowen Chu（HKUST(GZ)/HKUST，通讯） |
| arXiv 号与版本 | arXiv:2402.13499v1 [cs.AR]，2024 年 2 月 21 日 |
| 发表会议 | 本文提取文本未载明；同团队扩展版 arXiv 2501.12084 明确记载其前身 "published in IPDPS 2024" |
| 实测 GPU | A100 PCIe（Ampere，CC 8.0）、RTX4090（Ada Lovelace，CC 8.9）、H800 PCIe（Hopper，CC 9.0）——Hopper 代表机型为 H800 PCIe 而非 H100 SXM |
| 软件栈 | RTX4090：驱动 530.30.02 + CUDA 12.1；A100/H800：驱动 535.104.05 + CUDA 12.2 |
| 关键器件参数（Table III） | H800 PCIe：114 SM × 128 cores、max 1755 MHz、80GB HBM2e、2039 GB/s、456 个第 4 代 TC；对照 A100（108 SM×64、1410 MHz、40GB、1555 GB/s、432 个第 3 代 TC）与 RTX4090（128 SM×128、2520 MHz、24GB GDDR6X、1008 GB/s、512 个第 4 代 TC）；DPX 硬件三机中仅 Hopper 有（No/No/Yes），分布式共享内存行亦为 Hopper 独有（该行数值在文本提取中丢失） |
| 代码 | 正文承诺审稿后开源；扩展版给出 artifact：github.com/HPMLL/NVIDIA-Hopper-Benchmark |

## 2. 研究动机与问题

- Nvidia 每两年一代架构，但微架构细节公开极少，"precise quantification"困难；Hopper 新增第 4 代张量核心（FP8）、DPX 动态规划指令、分布式共享内存（DSM）与 Tensor Memory Accelerator（TMA）异步执行机制，性能与行为"still remain mysterious"。
- 既有张量核心剖析工作止步于旧架构：Jia et al. 测 Volta/Turing，Yan et al.、Raihan et al. 做 SASS 级基准，Fasi et al. 查数值行为，Sun et al. [2] 系统测 mma/ldmatrix/mma.sp——但该文虽讨论 Hopper 可编程性，汇编分析与微基准仍在 Ampere/Turing 上完成。
- Hopper 上 wgmma 与 mma 的 SASS 形态更丰富、支持精度更广，"usage as well as their performance is still uncovered"；wmma API 又无法用上新特性（操作数形状受限、不支持稀疏）。
- DPX、异步拷贝、DSM 的实现与性能细节"在现有文献中未披露"；揭示它们对 AI 应用优化、性能建模与算法设计（动态规划、科学计算）至关重要。作者自称这是首次系统揭秘 Hopper 独有的张量核心性能与编程指令集。
- 相关工作谱系（第 II 节）：GPU 微架构剖析一脉（Wong et al. 的 Fermi 前导、Mei et al. 内存层级、Jia et al. 的 Volta/Turing）服务于性能/功耗建模 [3]–[10]、模拟器 [11]–[13] 与应用优化；张量核心一脉从 wmma API 基准（Markidis、Martineau）到 SASS 级剖析（Yan、Raihan）再到 mma API 系统测试（Sun et al.）；能效一脉（DVFS [26]、多加速器对比 [27]）则为本文 Table XI 能效测试提供语境。

## 3. 方法学

**两条主线**（摘要明示）：其一，常规延迟/吞吐微基准在 Hopper/Ada/Ampere 三代间横向对比；其二，Hopper 新特性专项剖析（DPX、DSM、FP8 张量核心）。

1. **内存层级**（III-A）：P-chase 类指针追踪法（承自 Saavedra & Smith [28]）。L1 用 `ca` 修饰符预热、单线程测延迟、单 block 1024 线程反复访问测带宽；L2 换 `cg` 修饰符并用大量 block；共享内存直接声明、block 内访问；全局内存分配超 L2 容量避 prefetch，初始化兼作固定步长布点与 TLB 预热，4 个连续线程各读 8B 组成 32B 事务测延迟，吞吐测试用 float4 向量化（读 5 写 1）。
2. **张量核心**（III-B）：PTX 级微基准（粒度与复度的折中）+ 反汇编到 SASS 对照。延迟为 "completion latency"（发射至结果可供后续使用）：同步 mma 用每 SM 一个 warp 发射，异步 wgmma 用 4 warp（一个 warp group），kernel 内重复 1024 次；吞吐 = Total OPS/Duration——刻意**不用**时钟周期折算（与 Sun et al. [2] 不同），因不同 TC 指令执行期间 GPU 频率会漂移。
3. **库与应用层**（III-C）：Transformer Engine（te.Linear、te.LayerNormMLP、te.TransformerLayer）；LLM 层把 Llama 的 nn.Linear/RMSNorm 替换为 te.* 模块，用 ShareGPT 数据集生成合成请求（最大输入/生成各 128 token、batch 8），指标为 (input_len+output_len)/time；hidden size 1024–8192 五档对应 Llama 配置（Table II，4096/5120/8192 对应 7b/13b/70b）。
4. **Hopper 新特性**（III-D）：
   - DPX：CUDA 12 起的 DPX 函数，单线程迭代发指令测平均延迟、整 block 反复发测每 SM 吞吐；变动 launch block 数观察吞吐拐点，以定位加速硬件所在层级。
   - 异步数据搬运：用官方 CUDA sample globalToShmemAsyncCopy（矩阵乘，A 宽 B 高 2048），对照 "SyncShare"（同步拷贝 tiled）与 "AsyncPipe"（异步 + 两级流水、共享内存缓冲翻倍），block size 从 8×8 扫到 32×32，并调 block 数。
   - DSM：编程接口为 `cluster.map_shared_rank(SMEM, DST_BLOCK_RANK)`，编译为 PTX `mapa`。三项基准：(1) 双 block 各一线程、clock() 计时跨 SM 寄存器值相加，用 `mov.u32 %0, %%smid` 确认两 block 落在不同 SM；(2) 环形拷贝 RBC——每 SM 一个 block 组成 cluster，rank R 的块把寄存器值加到 rank (R+1)%CS 的块，用 ILP 压满带宽，调 cluster size/block size/ILP；(3) DSM 版 histogram——bin 分散到 cluster 内各 block，线程定位目标 bin 的 DSM 地址后原子递增，调 cluster size/block size/bin 数。

## 4. 关键发现

### 4.1 内存层级与基础数据（三代对照，Tables IV/V）

- 延迟（clocks，Table IV，顺序 RTX4090/A100/H800）：L1 43.4/37.9/40.7；共享内存 30.1/29.0/29.0；L2 273.0/261.5/263.0；全局 541.5/466.3/478.8。
- 三代架构的各级延迟彼此接近，说明内存层级设计相似；用 HBM2e 的 A100/H800 全局延迟略低于 GDDR6X 的 RTX4090；三机平均 L2≈6.5×L1、全局≈1.9×L2。
- 吞吐（Table V）：共享内存三机几乎同为 ~128 byte/clk/SM；L1（FP32.v4）121.2/106.8/124.1 byte/clk/SM——每 clock 搬运量几乎相同，单位时间吞吐排序（RTX4090>H800>A100）完全由频率决定（原文脚注 4）；L2（FP32.v4）H800 达 3942.4 byte/clk，为 RTX4090 的 2.6 倍、A100（2007.9）的 2.2 倍；全局带宽实测 929.8/1407.2/1861.5 GB/s，各达理论的 92%/90%/91%；L2:全局倍率 4.67/2.01/4.23。
- 方法学注记（原文自陈）：FP64 缓存吞吐读数异常偏低，因访存后须接 FP64 加法防编译器消除指令，测得的 16 byte/clk/SM 是 FP64 计算单元瓶颈，不代表缓存真实吞吐。
- Fig. 1 给出 Hopper SM 配置示意：256KB L1/共享内存、寄存器文件 16384×32bit、FP32×32、FP64×16、INT32×16、LD/ST×8、第 4 代 TC、SFU，以及 SM 级 TMA 与 GPC 内 SM-to-SM 网络。

### 4.2 张量核心：SASS 形态、mma 与 wgmma、FP8 实测（Tables VI–XI）

- **三代 TC 属性对照（Table I）**：Ampere 支持 FP16/BF16/TF32/FP64/INT8/INT4/Binary，仅同步 wmma/mma/mma.sp；Ada 增加 FP8（仍同步）；Hopper 支持 FP16/BF16/FP8/TF32/FP64/INT8/Binary，并新增**异步**的 wgmma/wgmma.sp。原文结论：wmma 与 mma 在 Hopper 上仍可用，但第 4 代 TC 的全部潜力只能通过 wgmma 释放。
- **指令语义（Fig. 2）**：mma 计算 D(m×n)=A(m×k)×B(k×n)+C(m×n)，由单个 warp（32 线程）同步执行，形状 m16n8k16/m16n8k8；wgmma 计算 D=A×B{+D}，由个 warp group（4 warp）异步执行，形状 m64nNk16（N=16/32/64/128/256…），且 A/B 可直接从共享内存加载（mma 则要求全部操作数先入寄存器文件）。
- **SASS 分析（Table VI）**：mma 编译为 HMMA（浮点）/IMMA（整型）/BMMA（二值）；wgmma 编译为新的 GMMA 族——HGMMA.64x256x16（FP16→FP16/FP32）、HGMMA.64x256x8.F32.TF32、QGMMA.64x256x32（FP8 的 E5M2/E4M3 双变体，累加 FP16 或 FP32）、IGMMA.64x256x32（INT8）、BGMMA.64x256x256（Binary）。
- 两个关键偏离：INT4 mma 在 Ampere/Ada 编译为 IMMA.16832.S4.S4，在 Hopper 却退化为 IMAD 指令序列跑 CUDA core，性能达不到 TC 预期；FP8 无 mma 路径（Ada 引入的新类型没有配套 mma）。wgmma 亦不支持 INT4，且为 Hopper 独占——尽管 Nvidia 称 Ada 与 Hopper 同为"第 4 代"张量核心。
- **mma 结果（Table VII）**：形状语义 m16n8k16/m16n8k8（稀疏形状为压缩后表示，实际 k 为表中 2 倍）。同精度下 A100/H800 大形状吞吐更优，RTX4090 无此规律；稀疏与稠密 mma 延迟相同（如 H800 FP16 m16n8k16：24.1/24.0 clocks），稀疏吞吐更高。
- 稀疏加速的代际差异：RTX4090 稀疏可达稠密 2 倍（与厂商宣称一致）；A100 仅大形状稀疏能达理论加速；**H800 稀疏 mma 平均仅 1.42 倍**——Hopper 上 mma 路径未充分用上稀疏张量核心。
- 峰值利用率：A100 mma 达理论峰值 >95%；RTX4090 甚至超过官方峰值（实测频率高于官方 boost）；**H800 mma 平均仅达理论峰值 62.9%**——原文提醒在 Hopper 上写 GEMM/卷积慎用 mma。
- **wgmma 结果（Table VIII，H800）**：零初始化矩阵时吞吐超理论峰值 95%。dense m64n256k16（TF32 为 m64n256k8、FP8/INT8 为 m64n256k32）各数据类型完成延迟均为 128.0 clocks；FP16→FP16 729.3 TFLOPS（峰值 756.5）、FP8→FP16 1448.4、FP8→FP32 1447.5、INT8 1448.7 TOPS、TF32 364.4（峰值 373）。
- "RS"（A 在寄存器）与"SS"（A/B 均在共享内存）两种模式的延迟与吞吐几乎一致——归因于异步执行与大计算量把共享内存访问延迟充分掩盖。
- 随机初始化时吞吐下降（FP16 计算 + FP32 累加最明显：729.3→665.4 TFLOPS），原因是逼近 H800-PCIe 350W 功耗墙触发降频；原文提醒 H800-PCIe 上做 TC 计算必须考虑功耗约束。
- **稀疏 wgmma（Table IX）**："SS"延迟 144.0、"RS"128.0 clocks，且 SS 吞吐低于 RS（如 FP16 sp.m64n256k32：SS 1308 vs RS 1472 TFLOPS）——SS 模式须从共享内存读 m×k 全量数据并依 metadata 现场做 2:4 稀疏剪枝，RS 模式直接读寄存器中已剪枝的 m×k/2；双倍共享内存访问需求无法被计算掩盖，SS 达不到预期峰值。FP8 稀疏 RS 模式达 2945 TOPS（sp.m64n256k64）。
- **N 维扫描（Table X，wgmma.m64nNk16.f32.f16.f16）**：N≥64 时 dense/sparse 各指令均逼近峰值（N=256/128/64 的 dense FP16→FP32：728.5/728.5/719.6 TFLOPS）；N<64 时吞吐下滑且 SS 延迟高于 RS、吞吐低于 RS（N=8：SS 158.2 vs RS 216.7）。解释：N 减小导致计算密度下降，共享内存访问延迟不再能被掩盖。实用建议：尽量取 N≥64。
- **能效（Table XI，最大形状 mma）**：H800 稠密能效平均为 A100 的 1.60 倍、RTX4090 的 1.69 倍；稀疏为 1.33/1.39 倍。示例 FP16→FP16 稠密：H800 2.62 TFLOPS/W（188.6W）vs A100 1.79（173.4W）、4090 1.89（189.1W）。

### 4.3 DPX 动态规划指令实测（Figs. 6/7）

- RTX4090 与 A100 无 DPX 硬件（软件模拟），两者性能几乎相同；H800 硬件加速显著：relu 类指令明显更快，16 位运算最高加速 **13 倍**。
- 并非所有 DPX 函数都受益：简单如 `__viaddmax_s32`（返回 max(s1+s2, s3)）三机性能接近——SASS 显示 Hopper 用新指令 VIMNMX 替换 IMNMX，但性能未见明显提升；`__vibmax_s32` 在 RTX4090/A100 上被编译器优化为单条 max 指令，无法测量。
- **定位实验**：block 数小于 SM 数时 DPX 吞吐与 block 数成正比；刚超过 SM 数的整数倍时吞吐骤降、随 block 数增加逐渐回升，峰值恰出现在整数倍处——据此推断 **DPX 加速单元位于 SM 级**。

### 4.4 分布式共享内存（DSMEM）与线程块集群观测

- **延迟**：SM-to-SM 网络延迟 **180 clocks，较 L2（263 clocks）降低 32%**，验证了 SM 间直连网络便于生产者-消费者高效交换数据的优势；官方文档宣称该网络最多可减少 7 倍块间数据传输开销（本文方法节引述）。
- **吞吐（RBC，Fig. 8）**：峰值约 **3.27 TB/s（cluster size=2）**；cluster size=4 时降至 2.65 TB/s——cluster 内块越多、对 SM-to-SM 带宽竞争越激烈，总吞吐越低；更大 block size 与更高 ILP（更多并行搬运指令）提升吞吐。
- **应用（histogram，Fig. 9）**：最优 cluster size 随 block size 变化（block 128 → CS=4，block 512 → CS=2）；CS=1 时 Nbins 从 1024 增至 2048 出现明显性能跌落——bin 多了吃共享内存、限制每 SM 活跃 block 数，把 bins 切分到 cluster 内各块可提高并发性、缓解该问题；即便共享内存不构成瓶颈（block 512 场景），合适的 cluster size 也能借 SM-to-SM 网络分担片上共享内存流量、提升整体性能。
- **集群观测结论**：大 cluster 降低更多块的数据搬运延迟，但加剧吞吐竞争；块尺寸/簇尺寸的权衡是重要调优方向——这也是目录内 CGA/cluster 专利（US20230289211A1 负载均衡、US20250173152A1 DSMEM）的行为学注脚。

### 4.5 异步拷贝、Transformer Engine 与 LLM 层

- **异步拷贝（Tables XIII/XIV）**：小块时 AsyncPipe 优势明显（8×8 block：H800 平均 +39.5%、A100 +19.6%），因 warp 数不足时无法掩盖同步共享内存拷贝延迟，两级流水使搬运与计算重叠；16×16 收窄至 +9.7%/+4.9%；32×32 时 H800 上 AsyncPipe 反而 −1.8%（A100 +1.7%）——大 block 的高 warp 并发已自然掩盖拷贝延迟。注意：该测试走 compute capability 8.0+ 的 cp.async 路径，未触及 TMA 张量映射路径。
- **te.Linear（Figs. 3/4）**：FP8 计算前有格式转换 + 量化开销（按输入最大绝对值定 scale）——N=4096 时 GEMM kernel 仅占执行时间 25.3%（Memcpy 0.4%、CPU Exec 29.4%、Other 44.9%），N=16384 时 kernel 占 84.7%；小矩阵 FP8 吞吐反低于 FP16/FP32，N=8192 起优势显现，N=16384 时 H800/4090 的 FP8 吞吐约为 FP16 的 2 倍——FP8 高吞吐需要足够条件才能达到最优计算密度。
- **te.TransformerLayer（Fig. 5）**：计算密度增大后 H800 优势显现；FP16 约为 FP32 的 2 倍快；FP8 仅在 hidden size>4096 时超过 FP16 且达不到 FP16 的 2 倍——Softmax/GeLU 未被 TE 量化、DotProductAttention 用 flash-attention 而非 FP8 张量核心，格式转换开销仍在。
- **LLM 推理（Table XII）**：decode-only 模型推理访存受限，FP8 算力优势不显著；且 TE 支持不完善，模块间仍以 FP16/FP32 传数、无算子融合。H800 上 llama-3B：FP32 679.45 / BF16 624.10 / FP8 537.92 tokens/s，llama-2-7B：568.91/502.65/474.42——FP8 反而最低；RTX4090 上 llama-2-7B 的 FP32 与 FP8 均 OOM。作者预期模型规模与输入长度增大、算子融合完善后 FP8 或有改善。

## 5. 与专题主题的关系：Hopper 专利集群的独立行为学印证

本文为目录内 2022-03-10 Hopper 专利申请集群提供了第三方独立实测对照：

| 专利（本目录） | 本文对应实测 | 吻合点 |
| --- | --- | --- |
| US20230289398A1（wgmma） | Tables VI–X：wgmma 独占 Hopper、4-warp 异步、128 clocks、SS/RS 等价 | 专利 FIG. 6A "H100 TC Instruction" 的行为学现身；"A 绕过寄存器文件直自共享内存"（claim 15）落地为 SS 模式且无性能损失；mma 仅 62.9% 峰值 vs wgmma >95%，正面验证专利动机章"前代 MMA 受寄存器文件供数所限" |
| US20250173152A1（DSMEM） | 4.4 节：SM-to-SM 180 clocks、RBC 3.27 TB/s、histogram 原子操作 | SM-to-SM 延迟低于 L2（263 clocks）即该专利 claim 10/22"低于 global/L2 访问延迟"的实测值；load/store/atomic 全覆盖对应 claim 7/19；`mapa` 地址映射即专利地址窗口的编程面 |
| US20230289304A1 / US12020035B2（TMA/多播） | 4.5 节异步拷贝（cp.async 路径） | 仅间接印证：本版本未测 TMA 描述符/多播路径，直接实测由扩展版补齐 |
| US20230289242A1 / US20230315655A1（mbarrier/快速同步） | — | 本文未直接测量（文内未载明相关基准） |

补充两点：

- **稀疏 wgmma 的 metadata 剪枝观测**（SS 模式读 m×k 全量后按 metadata 做 2:4 剪枝、RS 直读 m×k/2）是张量核心稀疏通路（mma.sp/wgmma.sp 专利族语境）少有的行为学数据，解释了稀疏加速"标称 2×、实测打折"的来源。
- **对 Blackwell 的意义**：本文确立的 Hopper 基线（wgmma 完成延迟 128 clocks、DSMEM 180 clocks、FP8≈2×FP16 的条件性、DPX 的 SM 级定位、N≥64 的效率门槛）是解读 Blackwell tcgen05/TMEM/FP4 演进的锚点——目录内两篇 Blackwell 论文沿用同类微基准法对照 H100/H200，才能区分"代际改进"与"性能回退"。

## 6. 与本专题其他材料的关系

- **arXiv 2501.12084（扩展版）**：同团队（新增 Hongyuan Liu）后续加深，原文明示 "based on and extends 'Benchmarking and Dissecting the Nvidia Hopper GPU Architecture' published in IPDPS 2024"。新增内容：L2 partitioned cache 重测（**推翻本版本部分数据与结论**，引用本文 L2 数据时应以扩展版为准）、wgmma 能耗、DSM 全面评估（接口开销、cluster 数扩展的延迟、不同访问模式与块调度策略）、应用级 DPX（计算生物学算法至少加速 4.75×）、TMA 基准（异步编程模型下矩阵乘提速 1.5×、FP8 近 2 倍于 FP16）。
- **arXiv 2507.10789（Blackwell 微基准剖析）**：Delaware 大学 Jarmusch、Graddon、Chandrasekaran，RTX 5080（GB203）对照 H100 PCIe，覆盖内存层级、SM 流水线、子核与第 5 代张量核心（FP4/FP6），方法学直承本文一脉。
- **arXiv 2512.02189（Blackwell 微基准深入）**：同组 Jarmusch & Chandrasekaran，B200 对照 H200，覆盖第 5 代张量核心、TMEM、解压缩引擎、双 die 与 FP4/FP6 精度（ResNet-50 训练 1.85×、GPT-1.3B 1.55×、能效 +32%）。
- 四篇论文加官方 Blackwell 技术简报构成 Hopper→Blackwell 的 SM 内部实测对照链，本文是链的 Hopper 起点，也是目录内 Hopper 专利集群与 Blackwell 实测之间的桥。
- 目录内的 NVIDIA_Blackwell_Architecture_Technical_Brief 作为官方规格侧写，与本文等实测文献互为补充：简报给出 tcgen05/TMEM/CTA pair 等机制的官方叙述，本文一系则给出 clocks 级行为数据。

## 7. 评价与局限

**价值**：首批对 Hopper 新特性（FP8 wgmma、DPX、DSMEM）的系统实测，数据颗粒度到 SASS 指令与 clocks 级延迟；"DPX 单元位于 SM 级""Hopper 上 mma 仅 62.9% 峰值、须用 wgmma 吃满第 4 代 TC""稀疏 wgmma SS 模式因二次取数掉速""FP8 收益需大矩阵/大 hidden size 才显现"等结论具直接工程指导意义，也为 GPU 性能/功耗建模提供稀缺输入。

**局限**：

- IPDPS 篇幅限制（原文自陈"Due to space limitations"只呈现最有意义的结果，完整版见 preprint/开源代码）——多项结论在扩展版被修订或加深（尤其 L2 数据）。
- Hopper 代表机为 **H800 PCIe** 而非 H100 SXM：350W 功耗墙直接造成随机数据下 wgmma 降频，"随机值吞吐下降"结论与特定卡型功耗口径耦合；PCIe 口径亦影响全局带宽读数；正文未讨论 SXM 版差异。
- RTX4090 实测频率高于官方 boost，致其 mma"超官方峰值"，跨代比较需谨慎；消费级 GDDR6X 卡与数据中心 HBM2e 卡混比本身即口径不齐。
- 覆盖空白：TMA 描述符/多播路径、mbarrier、cluster 调度准入细节、DSMEM 内部拓扑（GPCARB/GXBAR）均未触及；tcgen05/TMEM/FP4 等 Blackwell 机制自然不在范围（成文早于 Blackwell）；FP8 数值行为（舍入、非规格化数）未做深入分析；DSM 延迟仅测双块两 SM 场景。
- 时效性：LLM 推理 FP8 反低于 BF16 的结论反映当时 Transformer Engine 支持不完善，随软件栈演进可能失效。

**对专题的价值**：对 17_Blackwell_SM_Internals 而言，本文把目录内专利的机制描述（wgmma 异步状态机、DSMEM 低延迟互连、TMA 异步流水）翻译成可引用的行为学数字——128 clocks 的 wgmma 完成延迟、180 clocks 的 SM-to-SM 延迟、3.27 TB/s 的 DSMEM 峰值吞吐、62.9% 的 mma 峰值利用率——构成 Hopper SM 内部实现的"实测基准面"，后续阅读 Blackwell tcgen05/TMEM 演进与两篇 Blackwell 剖析时，均以此为参照原点。

---
*本文基于本地 PDF 全文提取（12 页通读）撰写，数字以原文为准。*

