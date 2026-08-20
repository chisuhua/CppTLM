US9928109B2 — “Method and system for processing nested stream events”（NVIDIA，授权 2018-03-27，优先 2012-05-09）。专利提出一种在并行处理子系统（GPU）内，由GPU 侧调度内核（scheduler kernel）和事件项（wait/signal events）驱动的跨流（cross-stream）依赖与嵌套任务（nested kernel）执行机制：用事件队列+依赖计数把“CPU 参与的顺序控制”下沉到 GPU，避免 CPU 介入造成的串行瓶颈，保证依赖有序且高吞吐推进。以下解读全部据原文与图示展开，并给出“专利原文 vs 工程推导”标注。(专利图像)

专利解读文档
US9928109B2 — Method and system for processing nested stream events (专利图像)
1) 背景与目标
● 传统：GPU 执行每一批任务后需通知 CPU，CPU 再发下一批，或靠 pushbuffer 预排队；但遇到数据相关/跨流依赖时仍会出现“GPU → CPU → GPU”串行等待。(谷歌专利)
● 目标：在 GPU 内部以事件模型（等待/信号）和调度内核完成跨流依赖管理与嵌套执行，实现无需 CPU 介入的正确执行次序与高效推进。(专利图像)

2) 顶层与部件（按原文图示）
● 系统/PPU 顶层（Fig.1–3A）：CPU↔并行处理子系统（PPU）。PPU 内部含 Front End 212、Task/Work Unit 207、Task Management Unit (TMU) 300、Scheduler Table 321、TMD Cache 322、Work Distribution Unit (WDU) 340、Task Table 345 等。它们与 GPC/SM、L2/DRAM 通过 Crossbar 互连。(专利图像)
● GPC/SM 细节（Fig.3B/3C）：Pipeline Manager 305、SM 310、Warp Scheduler/Instruction Unit 312、LSU 303、Shared Memory 306、L1/L1.5、MMU 328 等（框图用于上下文，不涉及本专利新增机制的微实现）。(专利图像)

3) 机制总览：嵌套执行与跨流依赖
3.1 嵌套执行（Nested Kernel）
● 父线程/父网格在 GPU 上直接触发子网格（child grid），并把**继续执行所需的“延续状态（Continuation State 642）”**保存到内存，随后让出；当子网格完成，调度器通知并恢复父 CTA，直至完成（Fig.5 时序）。(专利图像)
● 系统要素（Fig.6）：Application Work 612 → GTMD/QTMD（grid/queue task metadata）→ Scheduler 610 & Distributor 620 & SM 630，配合 Memory 640 和 Continuation State 642 实现嵌套执行整链路。(专利图像)
3.2 跨流依赖（Cross-stream Dependencies）
● 将依赖转化为事件项加入各自队列（TMD Queues）：
    ○ WE（Wait Event）：携带依赖计数（Dep.Count）；
    ○ SE（Signal Event）：代表“某一任务完成”的通知；
    ○ Task：实际计算工作项。
● 调度内核遍历队列：识别队头项是 Task/WE/SE → 更新计数 → 当 WE 的所有依赖满足（计数归零）时，释放对应 Task 进入可执行态（Fig.7–9A/9B、方法流程 1200）。这样即可在 不同流的任务间建立有序关系，而不需要 CPU 调度。(专利图像)

4) 关键算法（方法 1200 摘要化）
以每条流（stream）的队列为单位，调度内核循环处理队头项：(专利图像)
取队头项 → 判断类型：
  - 若为 Task：若其前置依赖满足 → 进入执行；否则留队（由事件驱动）
  - 若为 WE：检查/更新其依赖计数（来自相关 SE）；当计数==0 → 允许关联任务执行，弹出该 WE
  - 若为 SE：触发匹配的事件ID，递减对应 WE/Task 的依赖计数；弹出该 SE
遇到 StreamNext 链接 → 递归遍历并在每个节点更新状态（跨队列/跨层级）

（对应 Fig.12 的步骤：区分 Task/WE/SE、递减计数、触发 SE、递归遍历 streamNext 等。）(专利图像)

5) 数据结构（原文给出，便于落地）
5.1 Thread Group Context（Fig.10）
包含每个 pushbuffer（PB）最后一个任务指针、工作计数器等，用于按流/组管理推进状态。(专利图像)
5.2 Task / Event Status（Fig.11A–11C）
● Task Status (1120)：TaskID 1142、StreamNext 1144、ThreadGroupContextID 等。
● Signal Event Status (1122)：EventID 1150、StreamNext 1152、EventNext 1154。
● Wait Event Status (1124)：EventID 1160、StreamNext 1162、Dep.Count 1164。这些字段支持链式遍历（StreamNext/EventNext）与依赖计数运算。(专利图像)

6) “硬件实现细节”——按专利语义可落地的最小职责
下列职责直接来自本专利的框图和流程语义（未引入专利外寄存器名）。(专利图像)
6.1 硬件/固件模块与职责表
模块
职责
专利依据
归属
Front End 212
解析/喂入任务与事件项（来自 PB/TMD 队列）
Fig.3A
专利原文
Task/Work Unit 207
承载任务管理/事件调度的入口
Fig.3A
专利原文
TMU 300 + Scheduler Table 321 + TMD Cache 322
维护队列元数据、状态缓存、优先级/就绪度；为事件驱动的推进提供查询
Fig.3A
专利原文
WDU 340 + Task Table 345
将“可执行任务”分派到 GPC/SM，维持运行槽位
Fig.3A
专利原文
GPC/SM（Pipeline Manager/SM/Warp Scheduler）
执行实际 Task；在嵌套场景中按时序保存/恢复“延续状态（Continuation State 642）”
Fig.3B/3C、Fig.6
专利原文
Memory/L2/DRAM
存放 TMD 队列、事件/任务状态结构、Continuation State
Fig.6, Fig.10–11
专利原文
6.2 事件与依赖处理（硬件语义）
元素
关键字段/行为
专利依据
归属
Task
TaskID、链至 StreamNext；被 WE/SE 的依赖计数控制是否可执行
Fig.11A
专利原文
SE（Signal Event）
携带 EventID；到达后递减相应 WE/Task 的依赖计数；支持 EventNext 链
Fig.11B、方法1200
专利原文
WE（Wait Event）
EventID + Dep.Count；当计数降至 0 → 释放关联 Task
Fig.11C、方法1200
专利原文
递归推进
沿 StreamNext 递归更新各节点状态，跨层/跨流传播事件影响
Fig.12（文字描述）
专利原文
嵌套链路
父子网格关系、Continuation State 保存/恢复、调度通知
Fig.5、Fig.6
专利原文
注：专利没有规定具体寄存器名/位宽/中断号；这些均属微实现范畴（见“工程推导”列）。

7) 软件实现细节（运行时/驱动视角）
● 用户态/运行时：编排计算图，生成Task/WE/SE 条目并入队（可由编译器/运行时将跨流依赖转换为事件关系）。(专利图像)
● 驱动/内核态：
    ○ 组织 TMD 队列与状态结构（Fig.7/8/10/11）；
    ○ 启动 scheduler kernel（GPU 上运行的调度程序）扫描更新计数、触发 SE、释放 Task；
    ○ 接收完成通知，继续补充新的任务/事件项。(专利图像)
● CPU 角色变化：从“逐步调度与仲裁”转为“批量提交与监控”，跨流依赖的执行序由 GPU 侧事件系统保障。(专利图像)

8) 运行路径与时序（ASCII）
8.1 跨流依赖（简化自 Fig.7–9 / 方法1200）
[Queues per stream]
S0: push [Task A] in Stream0
S1: push [WE(id=X, dep=2)] into Stream1
S2: push [SE(id=X)] after Task A completes in Stream0
S3: push [SE(id=X)] after Task B completes in Stream2
GPU scheduler kernel loop:
  - sees SE(X) from Stream0 -> dec dep of WE(X) in Stream1
  - sees SE(X) from Stream2 -> dep->0 => release Task-of-Stream1
  - recursively follow StreamNext to update linked nodes
WDU dispatches released Task → SM executes

（与 Fig.9A/9B 的事件图等价：SE/WE 将跨流依赖转成“事件计数门控”。）(专利图像)
8.2 嵌套执行（简化自 Fig.5/6）
Parent CTA runs -> saves Continuation State -> launches child grid
-> parent CTA waits
Child completes -> scheduler notifies -> restore parent Continuation
-> parent CTA resumes -> completes grid

（对应 Fig.5 的时间线与 Fig.6 的“Continuation State 642”。）(专利图像)

9) “保存/恢复/状态字段”清单（带“来源”）
本专利不涉及寄存器级上下文抢占的保存列表，而是给出任务/事件/线程组上下文的数据结构字段；下表仅列原文明确字段，其余标为工程推导。(专利图像)
类别
字段/对象
作用
来源
线程组上下文
Thread Group Context (Fig.10)：PB(i):LastTask、Work Counter…
跟踪每个 PB/流的推进边界
专利原文 (专利图像)
任务状态
Task Status (Fig.11A)：TaskID、StreamNext、ThreadGroupContextID
标识任务、建立链表与归属
专利原文 (专利图像)
信号事件
Signal Status (Fig.11B)：EventID、StreamNext、EventNext
对应任务完成信号、可多对多链接
专利原文 (专利图像)
等待事件
Wait Status (Fig.11C)：EventID、StreamNext、Dep.Count
依赖计数归零即放行任务
专利原文 (专利图像)
延续状态
Continuation State 642（Fig.6）
父 CTA 等待/恢复所需状态块
专利原文 (专利图像)
（工程推导）
队列驻留位置、缓存策略、数据一致性屏障
如何在 L2/DRAM 与缓存间放置/刷写
工程推导
（工程推导）
调度内核触发/优先级、扫描步长、批量阈值
性能调参，不由专利限定
工程推导

10) 工程实现关注点（含“原文 vs 推导”）
主题
专利依据
说明
GPU 侧调度
摘要+方法1200
以 scheduler kernel 在 GPU 内部执行推进、无 CPU 干预。(专利图像)
跨流依赖=事件计数
Fig.7–9
通过 WE(计数)/SE(信号) 显式建模。(专利图像)
嵌套执行的恢复
Fig.5/6
用 Continuation State 恢复父 CTA。(专利图像)
TMU/WDU 协作
Fig.3A
TMU 维护元数据与可调度性，WDU 发射任务。(专利图像)
（推导）一致性
—
事件写入与调度核读之间的可见性需屏障/flush（实现自选）
（推导）容错
—
事件/任务多链路（StreamNext/EventNext）需要去重/健壮性检查
（推导）混部
—
与图形/复制引擎共存时的 QoS/公平由平台策略决定

11) “专利原文 vs 工程推导”对照（统一放在一起，避免误导）
我文中的陈述
归属
采用 事件项（WE/SE）+依赖计数 的跨流依赖模型；scheduler kernel 在 GPU 上遍历/递减/释放任务（方法1200）
专利原文 (专利图像)
Thread Group Context / Task/Signal/Wait Status 的字段与链表/计数关系（Fig.10–11）
专利原文 (专利图像)
嵌套执行的父子网格、延续状态保存/恢复、通知与重入（Fig.5/6）
专利原文 (专利图像)
TMU/WDU/TMD 等模块在事件驱动调度中的分工（Fig.3A）
专利原文 (专利图像)
具体寄存器名/位宽/中断线、缓存一致性细节、扫描策略与批量阈值、异常恢复策略
工程推导（专利未限定）

12) 小结
US9928109B2 把“跨流依赖与嵌套任务”交给 GPU 内部完成：以事件队列（WE/SE）+ 依赖计数的方式，配合 scheduler kernel、TMU/WDU 分工与Continuation State，形成无 CPU 介入的有序推进与高吞吐执行链条。原文清楚给出数据结构字段与流程图/时序图，为工程落地提供了直接的蓝本；而与缓存一致性、优先级调度、异常处理有关的细节留给实现方自行选择与优化。
