# AMD GPU 前端（CP/IB/doorbell/HQD/SPI）专利研究综述

> 领域：GPU架构 | 子领域：AMD 前端全链路（命令处理器 CP/间接缓冲 IB/PM4 包处理 + doorbell/HQD 队列提交 + SPI/SQ wave 分发）| 文献数量：10 篇本地专利（8 篇逐篇解析）+ 40 余件网络候选 | 更新日期：2026-08-19

---

## 一、领域概述

本专题是 [14_Front_End_and_Task_Graphs](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/研究综述.md)（NVIDIA TMU 上游）的 AMD 对照篇，研究"命令如何从 CPU 进入 AMD GPU 并变成 wavefront"的整条前端链路的专利布局。综合各专利的样板架构段落，AMD GPU 的标准前端链路为：

```
CPU（用户态/内核态）──写入──> ring buffer（命令环，存 PM4 包）/ AQL queue（HSA 队列）
        ──doorbell 写（MMIO doorbell 页）──>
        ─────────> Command Processor（CP）
                    ├─ ME（Micro Engine）/ PFP（Prefetch Parser）/ CE（Constant Engine）：图形管线
                    └─ MEC / ACE（Compute Engine）：计算队列，含 Hardware Scheduler（HWS）
        ─────────> Indirect Buffer（IB）：PM4 包间接跳转的命令片段，可链式/迭代
        ─────────> SPI（Shader Processor Input）→ SQ（Sequencer）：wave 分发
        ─────────> CU（Compute Unit）wavefront 执行
```

与 NVIDIA 前端链路的总体对应关系：

| 前端环节 | NVIDIA（本库 12/13/14 专题） | AMD（本专题） |
| --- | --- | --- |
| 命令流容器 | pushbuffer（存指向命令数据结构的指针） | ring buffer + Indirect Buffer（IB），内容为 PM4 包 |
| 主机接口/命令读取 | Host Interface Unit 206/210/232 + Front End 212/215 | Command Processor（PFP 预取解析、ME 微引擎执行、CE 常量引擎） |
| 任务就绪/调度 | TMU 234（Scheduler Unit） | CP 内的 Hardware Scheduler（HWS）+ 计算引擎队列（MEC/ACE） |
| 工作分发 | WDU 236 + Work Distribution Crossbar 330 | SPI → SQ → CU（wave 分发与资源分配） |
| 用户态提交触发 | put pointer + notifier（US20230153146A1） | doorbell 写 + HQD（US8310492B2、US9176795B2、US20210191730A1） |
| 描述符/命令预取 | task descriptor 预取（US11182207B2） | IB prefetch packet（US20220091847A1） |
| 任务图/动态工作 | task graph + TMU 依赖计数；work graphs | dynamic work creation 协处理器（US12131186B2）、task graph scheduling（US11481256B2）、迭代 IB（US20210304349A1） |

**关键差异**：NVIDIA 专利文本从不使用 "doorbell/HQD/CP/IB" 等 AMD 术语（见 14 号专题第七节负结果），AMD 专利同样从不使用 "pushbuffer/TMU/WDU" 等 NVIDIA 术语——两家前端专利各自形成完全自洽的术语体系，功能对照只能靠机制而非名词建立。

## 二、本地全文专利与逐篇解析（8 篇）

| # | 专利号 | 标题 | 优先权/申请 | 前端环节 | NVIDIA 对应物 | 解析文档 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | US8310492B2 | Hardware-based Scheduling of GPU Work | 2009-09（ATI ULC） | CP 硬件调度 + ring/IB + CPU 写寄存器通知 | pushbuffer 提交（US9489763B2）；HQD/doorbell 范式源头 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US8310492B2_HQD硬件队列调度_解析.md) |
| 2 | US8675002B1 | Efficient Approach for a Unified Command Buffer | 2010-06（ATI ULC） | 统一命令缓冲（多 GPU 共享 pushbuffer 对应物） | pushbuffer（US9489763B2） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US8675002B1_统一命令缓冲_解析.md) |
| 3 | US9176795B2 | Graphics Processing Dispatch from User Mode | 2010-12 | 用户态直接分发（ring buffer + doorbell + HWS） | 用户态直接工作提交（US20230153146A1，早约 10 年） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US9176795B2_用户态图形处理分发_解析.md) |
| 4 | US20220091847A1 | Prefetching from Indirect Buffers at a Processing Unit | 2020-09 | CP 的 IB 预取（显式 IB prefetch packet） | TMU 任务描述符预取（US11182207B2） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20220091847A1_间接缓冲预取_解析.md) |
| 5 | US20210191730A1 | Aggregated Doorbells for Unmapped Queues in a GPU | 2019-12 | doorbell 虚拟化（多队列共享聚合门铃） | put pointer/notifier；SM 队列管理器（US10489056B2）的资源稀缺问题 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20210191730A1_未映射队列聚合门铃_解析.md) |
| 6 | US20210304349A1 | Iterative Indirect Command Buffers | 2020-03（授权 US11900499B2） | IB 元数据携带迭代次数/谓词，CP 片内循环执行 | Self-Reset Flag（US11182207B2）；可执行图修改（US20210149719A1） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20210304349A1_迭代间接命令缓冲_解析.md) |
| 7 | US12131186B2 | Hardware Accelerated Dynamic Work Creation on a GPU | 2018-09（扫描件视觉阅读） | wavefront 经 fork/spawn 反向注入前端队列 | work graphs / 可执行图（US20210149719A1、US20230093254A1） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US12131186B2_硬件加速动态工作创建_解析.md) |
| 8 | US11822956B2 | Adaptive Thread Group Dispatch | 2020-12（AMD 上海，扫描件视觉阅读） | CP/SPI 按 cache line 格式自适应分发 work items | WDU 按 SM 容量分发/节流（13 号专题 US9594599B1 等） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US11822956B2_自适应线程组分发_解析.md) |

另下载 2 件未逐篇解析、供后续扩展的全文 PDF：

- **US20220058767A1** — Indirect Chaining of Command Buffers（2020-06）：命令缓冲链式组织，属 CP/IB 主题；[本地 PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20220058767A1.pdf)
- **US11481256B2** — Task Graph Scheduling for Workload Processing（2020-05，授权 2022-10）：AMD 任务图调度的授权核心件，与 WO2021242576A1（Task Graph Generation）同族；[本地 PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US11481256B2.pdf)

## 三、术语对照与考证

| AMD 术语（专利原文） | 含义 | NVIDIA 对应物（专利原文） | 备注 |
| --- | --- | --- | --- |
| ring buffer（RPTR/WPTR） | 命令环，CP 读取的主命令流 | pushbuffer | US8310492B2、US8675002B1 建立 RPTR/WPTR 模型 |
| indirect buffer（IB） | PM4 包间接跳转的命令片段 | 命令数据结构/command stream 片段 | IB prefetch（US20220091847A1）、迭代 IB（US20210304349A1）、链式 IB（US20220058767A1） |
| PM4 packet | 命令编码格式 | method / command stream 命令 | US20260086813A1（变宽包取数，无 PDF，仅在线） |
| doorbell | 用户态写 MMIO 触发队列处理 | put pointer + notifier | US8310492B2（寄存器通知原型）→ US20210191730A1（聚合门铃） |
| HQD（Hardware Queue Descriptor） | 硬件队列描述符 | Task Descriptor | 专利正文少用该缩写，机制见 US8310492B2；HQD 精确词命中 5 件 |
| Command Processor（ME/PFP/CE/MEC） | 命令处理器（微引擎族） | Host Interface Unit + Front End | me 查询命中 10 件，如 US12254527B2（可重构虚拟管线） |
| HWS（Hardware Scheduler） | CP 内硬件调度器 | TMU 234 | US9176795B2 首次把 HWS 与用户态分发绑定 |
| SPI / SQ | wave 分发与排序 | WDU 236 | US11822956B2、US20230206382A1 等 |
| AQL queue / dispatch packet | HSA 架构队列与分发包 | pushbuffer + task descriptor | US12131186B2 使用 AQL queue 235 |
| HyperQueue | RDNA2 几何管线商业术语 | （无对应专利术语） | 专利库中无命中，与 NVIDIA 侧负结果一致：商业名称不入专利 |

## 四、网络候选专利补充清单（按主题）

以下条目来自 Google Patents 双受让人（Advanced Micro Devices + ATI Technologies ULC）检索，未下载全文；日期为优先权日。

**CP / IB / PM4 主题**：US8743131B2（2009-09，coarse grain command buffer）、WO2013081975A1（2011-11，用 CP 保存/恢复 shader 与非 shader 状态）、US20130235057A1（2012-03，ATI，shader 与 command stream 的区域级依赖链分析）、US9632848B1（2015-12，异步命令提交）、US10579388B2（2011-12，shader 核资源分配策略，HQD 词命中）、US20240192994A1→US12493470B2（2022-12，加速 draw indirect 取数）、US20260086813A1（2024-09，CP 变宽包取数，Greathouse/Mantor 团队）、US20240378790A1（2023-05，流水线化图形状态管理）、US11900123B2（2019-12，marker 指令分组）、EP4268176A1（2020-12，condensed command packet 低开销 kernel launch）、US11809558B2（2020-09，处理器硬件安全加固，CP/微引擎安全）。

**doorbell / HQD / 队列提交主题**：US10545800B2（2017-05，虚拟化设备直接 doorbell ring）、US20220188139A1（2020-12，微引擎访问安全）、CN120883187A（2023-03，未映射队列 doorbell 高级硬件调度）、EP4639344A1（2022-12，多级调度 QoS）、US20260104914A1（2024-10，单线程发命令的控制协议）、US20250307039A1（2024-03，GPU kernel 应用管理优化）、US9009712B2（2012-03，GPU 分布式 work-item 排队）、US9430281B2（2010-12，异构入队/出队任务调度）、US11948000B2（2020-10，gang scheduling 低延迟任务同步）。

**ATI 时代 APD/HSA 专利群（2010-12 优先权日集中提交）**：US20120229481A1（compute resource 可达性，US9176795B2 同族）、JP6086868B2 / EP2652614B1 / JP6381734B2（用户态分发同族）、WO2012082777A1 / US20120194525A1（APD 托管任务调度）、US20120188259A1（任务调度使能机制）、US9122522B2（APD 任务调度软件机制）、US8803891B2（计算任务抢占图形任务）、US8959319B2（SIMD 分歧线程执行）、US8963933B2（基于紧急度的抢占）、US9299121B2 / US10242420B2（抢占式上下文切换）、US20130141447A1（多并发工作输入）、US20130162658A1（多引擎 GPU 信号量同步）。

**SPI/SQ / wave 分发主题**：KR20210002646A（2018-04，反馈式拆分 workgroup 分发）、US20230206382A1（2021-12，动态 workgroup 分发）、KR20240004302A（2021-03，基于参数缓冲的 wave 节流）、KR20230125232A（2020-12，基于资源占用的 shader 节流）、US20220107849A1（2018-12，线程工作负载分布）、US12499604B2（2021-12，运行时更新 shader 调度策略）、US20230195509A1（2021-12，variable dispatch walk）。

**任务图 / 动态工作主题（AMD task graph 族）**：US12131186B2（2018-09，动态工作创建）、US11481256B2 / WO2021242576A1（2020-05，任务图调度/生成）、US20250077307A1（2023-08，任务图队列管理）、US20240303113A1（2023-03，编译器导向的图式命令分发）、US20240385872A1（2023-05，加速器任务聚合调度）、US20250110792A1 / US20250224982A1（2023-09/2024-01，SIOV 设备任务图提交/队列管理）、US20250181384A1（2023-11，任务图控制数据传输）、KR20250121401A（2022-12，图节点纳入专用加速器）。

**虚拟化 / 多租户前端**：US12254527B2（2016-10，可重构虚拟图形与计算管线）、US20240211290A1（2022-12，world switch 时作业提交对齐）、US20240394829A1→US12354183B2（2019-12，跨 GPU 依赖进程调度）、EP3646177B1（2017-06，虚拟化 APD 提前上下文切换）、US20240419482A1（2023-06，上下文解映射时 GPU 自保存）。

## 五、AMD 与 NVIDIA 前端架构对比要点

1. **提交模型**：两家都在 2009–2012 年完成"软件排队 → 硬件调度"的转型——NVIDIA 侧是 pushbuffer + 前端硬件（US9489763B2，2012 前后样板段落），AMD 侧是 US8310492B2（2009-09，RLC + CP 基于 workload profile 选择 ring/IB 与优先级）。AMD 的用户态直接分发（US9176795B2，优先权 2010-12）比 NVIDIA 同类专利（US20230153146A1，2021 申请）早约十年，但 NVIDIA 的公开文本对 doorbell 类机制的寄存器级细节（put pointer/notifier）描述更具体。
2. **队列虚拟化**：面对"队列数 ≫ 硬件提交槽位"，AMD 走聚合 doorbell + 未映射队列中断轮询路线（US20210191730A1），NVIDIA 走 SM 内 queue manager 标量化重组路线（US10489056B2）——前者虚拟化提交入口，后者虚拟化执行入口。
3. **循环/重复执行**：AMD 把迭代控制下沉到 IB 元数据（iterative flag + iteration count + predication，US20210304349A1），NVIDIA 把循环复位下沉到任务描述符（Self-Reset Flag，US11182207B2）；粒度上前者是命令流级，后者是任务级。
4. **动态工作生成**：AMD US12131186B2 用片上 coprocessor 让执行中的 wavefront 经 fork/spawn 直接写回 AQL 队列 + doorbell，与 NVIDIA work graphs（TMU 依赖计数 + 描述符预取）构成两条"GPU 自生成工作"路线：AMD 复用既有队列提交通路，NVIDIA 新建任务描述符硬件通路。
5. **预取策略**：AMD IB prefetch 是软件显式插入的 PM4 包（驱动可控），NVIDIA 描述符预取由任务描述符中的 flag 位触发（TMU 自治）；两者同属"用当前工作执行窗口掩盖下一工作取数延迟"。
6. **时间线特征**：AMD 前端专利在 2010-12（APD/HSA 群）与 2018–2024（doorbell 虚拟化、IB 增强、任务图、wave 分发自适应）两个时段密集；NVIDIA 对应主题的专利集中在 2017–2023（queue manager、用户态提交、任务图族）。

## 六、检索记录（2026-08-19）

检索入口为 Google Patents XHR API，受让人分别限定 "Advanced Micro Devices" 与 "ATI Technologies ULC"（2011 年前图形 IP 在 ATI 名下，双受让人检索必要）。短语精确命中数（snippet 过滤后）：

| 查询短语 | 总结果 | 精确命中 | 代表性命中 |
| --- | --- | --- | --- |
| "indirect buffer" | 17 | 13 | US8310492B2、US20220091847A1、US20220058767A1、CN120883187A |
| "hardware queue descriptor" | 5 | 3 | US10579388B2、US20250307039A1、WO2026006158A1 |
| "micro engine" | 10 | 9 | US12254527B2、US20240378790A1、US12493470B2 |
| "command processor" AND "indirect buffer" | 14 | 9 | US8310492B2、US20220091847A1、WO2021061777A1 |
| doorbell | 49 | 38 | US20210191730A1、US12131186B2、CN120883187A、US20260086813A1 |
| "task graph" | 25 | 13 | US11481256B2、WO2021242576A1、US20250077307A1 |
| "shader processor input" | 16 | 16 | US11822956B2、US20230206382A1、KR20210002646A |
| "command stream" | 155 | 83 | US9176795B2、US9632848B1、WO2013081975A1 |
| ATI 受让人宽查询 | 153 | — | 2010-12 APD/HSA 专利群 |

**负结果记录**：

1. 内网 PatentsDB（http://30.21.200.104:9527）核查："Advanced Micro Devices" 0 条、"AMD" 1 条（实为 NVIDIA 误匹配）、"ATI Technologies" 43 条抽样全为 NVIDIA——该库仅收录 NVIDIA 专利，对 AMD 无覆盖，未做进一步检索。
2. 本地语料扫描（717 个 PDF，扫描脚本 /tmp/amd_scan.py）：受让人命中 24 件均为论文/规范/演示文档（ISA 手册、MI300A 论文等），无任何 AMD/ATI 前端专利在库；唯一 "command processor" 高频专利 US8711159 经核实为 Microsoft VGPU 模拟器。
3. "HyperQueue"（RDNA2 商业术语）未见于任何专利文本，与 NVIDIA 侧 "HyperQueue/WST" 负结果对称：商业/驱动层命名不进专利。

## 七、文件清单（本目录）

| 文件 | 类型 |
| --- | --- |
| [US8310492B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US8310492B2.pdf) | 🔧 专利 |
| [US8675002B1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US8675002B1.pdf) | 🔧 专利 |
| [US9176795B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US9176795B2.pdf) | 🔧 专利 |
| [US20220091847A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20220091847A1.pdf) | 🔧 专利 |
| [US20210191730A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20210191730A1.pdf) | 🔧 专利 |
| [US20210304349A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20210304349A1.pdf) | 🔧 专利 |
| [US11900499B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US11900499B2.pdf)（扫描件，US20210304349A1 授权版） | 🔧 专利 |
| [US12131186B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US12131186B2.pdf)（扫描件） | 🔧 专利 |
| [US11822956B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US11822956B2.pdf)（扫描件） | 🔧 专利 |
| [US20220058767A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US20220058767A1.pdf) | 🔧 专利 |
| [US11481256B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/US11481256B2.pdf) | 🔧 专利 |
| 8 篇逐篇解析（见第二节表格链接） | 📝 解析 |

> 共 11 篇专利 PDF + 8 篇逐篇解析 + 1 篇综述 = 20 个文件（另 `_txt/` 为 PDF 提取文本工作目录，不计入）。

## 八、关联资源

| 资源 | 位置 | 关联点 |
| --- | --- | --- |
| 前端与任务图研究综述（NVIDIA TMU 上游） | [14_Front_End_and_Task_Graphs/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/研究综述.md) | 本专题的 NVIDIA 对照篇 |
| TMU 研究综述 | [12_Task_Management_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) | NVIDIA TMU 专利族（本专题多处引用 US11182207B2 对照） |
| WDU 研究综述 | [13_Work_Distribution_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/研究综述.md) | NVIDIA WDU/两级 crossbar，对应 AMD SPI/SQ 分发 |
| NVIDIA TMU 专利专题整理 | [14_Patents/NVIDIA_TMU专利专题整理.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/NVIDIA_TMU专利专题整理.md) | 检索方法论来源 |
| AMD ISA 手册（GCN3/SI/RDNA） | [03_AMD_Architecture/](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/03_AMD_Architecture/) | CP/wavefront 术语的公开规范依据 |

---

*本综述基于 11 份专利 PDF 提取与 8 篇逐篇深度解析生成（9 份经 pypdf/PyMuPDF 文本提取；US12131186B2、US11822956B2 为扫描件，经逐页渲染后视觉阅读提取，US11900499B2 以其文本版公开 US20210304349A1 为准）。专利族与候选清单经 Google Patents XHR API 双受让人检索（2026-08-19）；内网 PatentsDB 与本地语料扫描均为负结果，已记录。最后更新：2026-08-19*

