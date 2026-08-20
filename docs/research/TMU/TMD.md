https://patents.google.com/patent/US9286119B2/enhttps://patents.google.com/patent/US9069609B2/enhttps://patents.google.com/patent/US9378139B2/enhttps://patents.google.com/patent/US20210019185A1/en好的！下面是一篇把四篇专利“串成体系”的完整文章，聚焦它们共同奠基的 TMD（Task Metadata）任务元数据 技术能力与整体架构。为便于阅读，我按“概念 → 机制 → 硬件架构 → 运行流程 → 能力矩阵 → 性能/实现要点 → 影响与演进”的顺序展开。

基于 TMD 的 GPU 任务调度与执行体系：从封装到低延迟启动、再到硬件级多任务调度
摘要
这四篇专利构成了一套完整的 GPU 任务级调度执行栈。核心思想是用 TMD（Task Metadata，任务元数据） 将一个计算任务的所有状态与运行参数“自描述化”，并在硬件侧（TMU/WDU）直接感知、缓存、排序、分发和衔接任务，最大限度地减少驱动/内存往返、降低启动延迟、支持任务间依赖、以及实现多任务的优先级/配额/负载均衡/抢占等能力。其技术脉络可概括为四步：
1. TMD 封装（Compute Task State Encapsulation）
2. 低延迟任务加载/启动（Launch Cache + Inline Task Memory Request）
3. 依赖任务的自动衔接（Dependent Task Auto-Launch）
4. 硬件化多任务调度与执行（TMU/WDU + 优先级/时间戳/配额/负载均衡/节流等）

1. TMD 是什么：把“任务”做成可被硬件直接理解的自描述体
TMD 是存放在显存/设备可见内存中的一个结构体，封装了一个任务从“初始化 → 调度 → 执行”的全部必要信息，典型包含：
● 初始化参数：程序入口（kernel 起始地址）、网格/CTA（block）维度、常量/队列指针、缓存失效策略、寄存器/共享内存/本地内存预算等。
● 调度参数：优先级、加入队列的头/尾标志、是否 Grid TMD（固定规模）或 Queue TMD（动态规模/可生产-消费/可嵌套子任务）、每 CTA 入队条目数 N、合并阈值与 coalescing timeout、SM 亲和、是否顺序任务、是否节流（Throttle）模式、依赖任务字段等。
● 执行参数：浮点/舍入/NAN 处理等算术行为、内存屏障与信号量（可选）、串行标志、可并发 CTA 限流（throttle enable）等。
● 动态状态：未完成工作项计数、CTA 进度、队列的 put/get 指针（环形队列）、是否已发射/在飞 CTA 等。
● 硬件私有区：仅 TMU/WDU 可写读的“hardware-only”字段，确保 TMD 在被复用时的连贯性与一致性。
这一步解决了“任务状态如何被标准化、可被硬件直接消费”的问题，是后续所有优化（低延迟加载、自动依赖、硬件调度）的基础。

2. 两类任务形态：Grid TMD vs. Queue TMD
● Grid TMD：固定网格（grid）规模，发射完成即止，适合批量、规模已知的 kernel。
● Queue TMD：内置或指向一个环形队列，每积累 N 条目发射 1 个 CTA；支持 生产-消费、合并（coalescing）、coalescing 超时、以及 子任务（dynamic parallelism） 在线写入队列，天然适配不定长和嵌套工作流。

3. 低延迟启动：让“新到任务”直接常驻片上缓存
传统路径是：CPU/前序任务把 TMD 写入显存 → 调度器再读回 → 解析依赖 → 再读剩余字段 → 才能启动。瓶颈在 “写显存 + 读显存” 的高延迟往返。
专利给出的改进：
● Inline Task Memory Request（内联任务写入请求）：驱动或 SM 把 TMD 以“特殊指令/消息”直接交给 TMU，而不是交给复制引擎。
● Launch Cache（启动缓存）：TMU 里新增一层“完整 TMD 缓存”；TMD 到达即写入该缓存（写通或写回可选），随后再异步复制到显存。
● Scheduler Cache：只放 TMD 的关键调度字段，先做依赖判定与就绪判断，准备就绪后再由 Launch Cache 快速补齐剩余字段并发射。
这一步把“新任务”直接落在 片上缓存，在它被调度-发射时无需回读显存，显著缩短 ready → launch 的延迟。

4. 任务依赖的零内存衔接：Dependent TMD Auto-Launch
用信号量（semaphore）做任务依赖，需要一次写加若干次读，带来额外延迟与带宽消耗。为此提出 “依赖任务字段”：
● 在 上游 TMD 的元数据中，写入 dependent enable 与 dependent TMD pointer（以及是否复制到硬件私有区的标志）。
● 当上游任务完成时，WDU/TMU 被通知，若依赖标志为真，直接安排依赖任务入调度表，无需任何显存信号量往返。
● 可链式：依赖任务本身还能再挂一个依赖，形成连续衔接。
这一步让“任务收尾 → 依赖任务起步”之间的延迟近似归零，比信号量方案更高效、更省带宽。

5. 硬件化的多任务调度执行：TMU/WDU 的职责分离
Task/Work Unit（TWU） 是硬件调度核心，分为：
● TMU（Task Management Unit）
    ○ 维护按优先级分组的 Scheduler Table（链表/表头表尾），可接受多个 pushbuffer 或来自 SM 的子任务。
    ○ 负责接纳、缓存（TMD Cache）、按优先级与到达次序（时间戳/分配序）排序，并将可运行任务放入工作池。
● WDU（Work Distribution Unit）
    ○ 维护当前在跑任务的 Task Table（槽位）。
    ○ 具备 抢占/恢复：高优先级任务可驱逐低优先级任务，未完成的 TMD 回 scheduler 等待重启。
    ○ 负担 SM 选择与 CTA 发射：
        ■ Load-Balancing 模式：依据各 SM 上报的 CTA availability（可再容纳 CTA 的实时估计）选择最空闲者；若并列，用固定优先表打破平局。
        ■ Round-Robin 模式：按序循环发射，保障均匀利用。
    ○ 支持 Sequential Task（每次仅 1 CTA in-flight）、Launch Quota（每任务 CTA 额度）、Throttle Mode（共享内存独占/分割），以及 SM 亲和性 等规则。
这一步把“多任务调度”的复杂性放进了硬件微架构，摆脱驱动在执行期的频繁介入，同时也比纯软件调度更细粒度、更低开销。

6. 端到端执行流程（简化）
CPU / 前序任务
   └─ 生成 TMD → Inline Task Memory Request
        └─ TMU.Launch Cache ←（写通/写回）→ 显存
             └─ TMU.Scheduler Cache（读取关键字段）
                  └─ 依赖/合并/配额/顺序/节流 等判定（就绪？）
                       └─ WDU 选择 SM（负载均衡 或 轮转）
                            └─ 发射 CTA（Grid 固定 / Queue 按 N 条目）
                                 └─ SM 执行，更新动态状态
                                      └─ 结束 → 触发 Dependent TMD 自动入队（若有）

Queue TMD 补充：生产者把条目写入环形队列，WDU 按 N 条目合并为 1 CTA；不足 N 时等待 coalescing timeout 触发以避免饿死；子任务执行中可继续向队列写入，形成流水。

7. 能力矩阵一览（来自四篇专利的拼合）
能力
归属
说明
任务自描述（TMD）
封装专利
初始化/调度/执行/动态状态/硬件区
Grid / Queue 两态
封装专利
固定 vs. 动态规模、支持生产-消费与子任务
依赖自动衔接
依赖专利
Dependent enable + pointer；免信号量
低延迟启动
低延迟专利
Inline 写入 TMU、Launch Cache + Scheduler Cache
调度接纳与排序
封装/硬件调度
按优先级 + 到达顺序（时间戳/分配序）
发射与负载均衡
硬件调度
Load-Balancing / Round-Robin、CTA availability
抢占/恢复
硬件调度
高优先级可抢占，TMD 状态用于恢复
节流（Throttle）
硬件调度
独占/分段共享内存，切换需清空在飞任务
顺序/配额/亲和
硬件调度
Sequential、Launch Quota、SM Affinity
合并与超时
封装/硬件调度
N 条目合并为 CTA，超时保障进度

8. 性能与实现要点（工程视角）
性能收益点
● 减少 ready→launch 延迟：新任务可直接命中 Launch Cache，无需“写显存→读显存→再读剩余”。
● 带宽友好：依赖自动衔接免去 Semaphore 频繁读写；Queue 合并减少小任务发射开销。
● 并行效率：硬件调度（优先级+负载均衡） + 抢占/恢复，提升吞吐与公平性。
● 实时性：Sequential/配额/节流/亲和，确保关键任务的确定性与隔离性。
实现关注点
● 缓存一致性：Launch Cache 写回时机与 SM 可见性必须严格受控，避免读到“半新不旧”。
● TMD 复用安全：硬件私有区 + 首次加载复制策略，避免旧任务残留字段污染新任务。
● 驱逐策略：Launch Cache 何时驱逐、驱逐谁（LRU/优先级/就绪态/最近预empted）需精心取舍。
● 调度状态机复杂度：Throttle/Sequential/Quota/Coalescing/Dependency 等规则交织，需清晰的优先关系与可验证性。

9. 与现代 GPU 软件抽象的契合
● CUDA Streams/Graphs 的落地支撑：
    ○ Graph 节点依赖 ↔ Dependent TMD
    ○ Dynamic Parallelism ↔ Queue TMD 子任务
    ○ 图的快速反复启动 ↔ Launch Cache + 硬件调度
● 多进程/多上下文：时间片组（TSG）与 TMU/WDU 状态保存/恢复，使多租户并发具备硬件底座。

10. 关于launch cache和schedule cache的两级设置
项
Launch Cache
Scheduler Cache
时机
Host 提交即刻使用（短驻留）
等待依赖、资源、优先级（长驻留）
数据内容
完整 TMD（上百字节）
元信息（几十字节）
访问模式
一次命中即消费
多次查询、排队
生存期
几个 GPU 周期到几百周期
可能数千周期以上
容量需求
少（命中率关键）
大（队列深度关键）
功耗敏感性
高频访问区，需保持低功耗
可低频扫描
阶段
模块
事件
①
Host → TMU
提交 inline task memory request
②
Launch Cache
暂存完整 TMD，write-through 到显存
③
Scheduler Cache
后续当依赖满足、任务 ready 时登记 entry（仅引用 TMD header/pointer）
④
SL → WDU
若 Launch Cache 命中 → 直接取完整 TMD；否则从显存拉取
⑤
Launch Cache Evict
发射后 / 空间不足时回收
⑥
Scheduler Cache Evict
任务执行完或被抢占时释放 entry

11. 小结：一条清晰的技术演进主线
1. 把任务“物化”为 TMD（可被硬件直接消费）；
2. 让新任务先进入片上缓存（低延迟启动路径）；
3. 用元数据直接描述依赖（免信号量的自动衔接）；
4. 在硬件端建立可扩展的调度-分发体系（TMU/WDU + 多规则混合）。
这条路线把“任务级”的编排与执行从软件路径搬到了更靠近执行单元的硬件路径上：更快的启动、更低的开销、更强的并发控制与实时性保障。而 TMD 既是这套体系的数据锚点，也是工程上连接“编程模型—驱动—硬件调度—执行单元”的关键接口。

下面是两幅图来更细致的表达这个基础能力：
1. 架构速查图（Cheatsheet） —— 把 TMD 调度执行体系的核心能力压缩成一页表格/图块。
2. 全景图（Pipeline/State 流程图） —— 展示 TMD 在硬件路径中的流转，从生成、缓存、调度到 SM 执行。

11. 后记
在专利文本里，Task Type 明确分为 Grid TMD 和 Queue TMD，它们的区别关系到 任务的生命周期 和 数据驱动 vs 配置驱动 的执行模式。

🔹 Grid TMD
● 定义Grid TMD 表示一个静态定义好的计算任务，执行时会启动一个固定数量的 CTA（Cooperative Thread Array，即 thread blocks）。
● 特点：
    a. 任务大小固定：GridDim × BlockDim 已经确定。
    b. 一次性任务：执行完全部 CTA → 任务完成。
    c. 常见场景：普通 CUDA kernel launch（例如矩阵乘、卷积），输入数据量已知。
● 字段：
    ○ GridDim, BlockDim
    ○ Kernel start PC
    ○ Data buffer pointer（输入/输出数据）
● 示意：
Grid TMD
 ├── GridDim = (X, Y, Z)
 ├── BlockDim = (Bx, By, Bz)
 └── Launch fixed #CTA = X*Y*Z


🔹 Queue TMD
● 定义：Queue TMD 表示一个动态任务队列，不是固定 grid，而是由数据/事件驱动，每次从队列中取一批工作项生成 CTA。
● 特点：
    a. 任务大小不固定：随着运行，任务不断被“推入队列”。
    b. 支持 Child Task：SM 执行过程中可以往队列写入新的工作项。
    c. 支持嵌套与递归：父任务可以生成子任务，子任务继续排队。
    d. 常见场景：
        ■ 动态并行（CUDA Dynamic Parallelism）
        ■ 图遍历、递归算法
        ■ AI 中的动态 workload（例如稀疏 MoE routing）
● 字段：
    ○ QueuePointer（循环队列指针）
    ○ ContinuationPointer（挂起任务的恢复点）
    ○ CoalescingRule（队列合并规则，N 个 item 组成 1 个 CTA）
    ○ Timeout（避免小任务无限等待）
● 示意：
Queue TMD
 ├── QueuePointer -> [work item list]
 ├── Each CTA = N work items (coalescing rule)
 ├── Timeout -> force launch CTA
 └── Child Task -> enqueue new item


🔹 直观对比
属性
Grid TMD（静态网格）
Queue TMD（动态队列）
任务大小
固定（GridDim × BlockDim）
不固定（依赖队列长度）
定义时机
提交时已知
运行时不断生成/追加
是否支持子任务
不支持
支持（Child Task → 新队列项）
典型应用
矩阵运算、卷积、渲染 Pipeline
图遍历、动态并行、稀疏计算
完成条件
所有 CTA 执行完
队列为空且无新任务生成

🔹Summary
● Grid TMD = 硬件层面的“静态 kernel launch”类似于你写 <<<Grid, Block>>> 启动一次 kernel。
● Queue TMD = 硬件层面的“动态任务队列”类似操作系统里的 工作队列 (Work Queue)，但实现搬到 GPU 硬件层，支持子任务直接插队。
👉 可以理解为：
● Grid TMD 面向批处理计算（batch，预定义规模）。
● Queue TMD 面向事件驱动/递归计算（dynamic，数据/任务驱动）。

🔹关于SIZE的意义
🔹 1. Circular buffer 的 容量 size
● 这是 QMD 里定义的 size，表示这个环形队列容纳多少个 work item slot。
● 类似我们常说的 ring buffer 长度 N。
● 管理字段：
    ○ base_addr
    ○ size (capacity)
    ○ rptr (read pointer)
    ○ wptr (write pointer)

🔹 2. 每次 push 的 item 大小
● 每个 work item 的结构是固定的（类似固定长度的 struct）。
    ○ 例如：一个 item = 16B/32B/64B（取决于硬件定义）。
    ○ item 里可能包含：指针、参数、任务 ID、child task continuation 信息。
● 所以 push 的时候，只能按 item 粒度写入，而不是可变长度。
● 这保证了 rptr/wptr 可以通过简单的 +1 mod size 来推进，而不需要复杂的变长管理。

🔹 3. Coalescing 规则和 push 的关系
● 虽然 单个 item 长度固定，但调度器在发射 CTA 时，可能要求 一次取 N 个 item 合并成一个 CTA：
    ○ CoalescingRule = N → 每次发射 CTA 要消费 N 个 item。
    ○ 如果队列里 item 数不足 N，则根据 timeout 决定是否提前发射。
● 所以：
    ○ push = 固定 item size
    ○ 发射 CTA = 可能一次性消费多个 item

🔹 直观图解
Queue Buffer (size = 8 items, each item = 32B)
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ item0│ item1 item2│ item3│ item4│ item5│ item6│ item7│
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
   ^rptr                          ^wptr

- 每次 push: 写入 1 个 item（固定长度）
- 每次发射: 取 N 个 item（由 coalescing rule 决定）


🔹 4. 总结
● “size = 每次 push 的内容长度”对应的是 item size，这是 硬件规定好的固定长度。
● 专利里 “size” 一般指 circular buffer 的容量（能放多少个 item）。
● push 时只能按固定长度 item 写入，发射 CTA 时才会用 N 个 item 合并。
🔹 专利中的原文线索
1. Queue 大小 (Circular Buffer Size)
在专利中描述 Queue TMD 的时候，有这段话：
“The queue may be implemented as a circular queue so that the total amount of data is not limited to the size of the queue.”
👉 这里的 size 指的是 环形队列容量，也就是 buffer 能容纳多少个 entry。专利强调用循环的方式避免“写满一次就结束”。

2. 每个 work item 的固定性
在 Queue TMD 的 CTA 生成部分，专利写到：
“…when a TMD specifies M work items per CTA, then N items are read by each CTA. … This ‘coalesces’ the N separate work items into one CTA. However, the plurality of TMDs might not generate a number of work items that is divisible by N, which results in a partial set of work items that are left outstanding.”
👉 这里的关键词是 “work items”：
● 表明 队列里的单位是 work item。
● 每个 work item 是最小粒度，长度固定（否则没法用 N items 来整除/合并）。
● CTA 消费时，可以一次拿 N 个 item 来拼成一个 CTA → 这对应“Coalescing Rule”。

3. Push/Pop 指针的语义
专利描述队列操作时提到：
“…the queue entries may also represent a child task that is generated by another TMD 322 during execution of a thread, thereby providing nested parallelism.”
👉 说明 每个 entry（即 item）是一个任务/子任务的描述符单元，而不是变长 blob。这样才能用简单的 rptr/wptr 来管理。

🔹 和问题对应关系
1. 环形队列大小
    ○ 专利里的 “size of the queue” 指的是 整个 circular buffer 的容量，由创建 Queue TMD/QMD 时指定。
    ○ 对应解读：这是 QMD.size。
2. 每个 push 的内容长度
    ○ 专利里的 “work item” 是固定长度的 entry。
    ○ 因为 CTA 的 coalescing 规则是基于 “N items per CTA”，这必须要求每个 item 大小固定，才能按 slot 来算。
    ○ 对应解读：push 的粒度就是 1 个固定 item，而不是可变长。
3. rptr/wptr 的含义
    ○ 专利没明确用 rptr/wptr 这些词，但说了 “circular queue + queue entries”，说明队列必须有读写指针来管理。
    ○ 在 Nvidia 硬件文档里（QMD 部分）才会直接出现 rptr/wptr 的名字。

🔹 结论（有专利支撑的解读）
● size（专利原文 “size of the queue”）指的是环形队列容量。
● 每个 work item 固定大小（专利原文 “M work items per CTA … N items are read … coalesce” 支撑），push 就是往环形队列写一个 entry。
● push/get 管理：虽然专利没点名 rptr/wptr，但 “circular queue” 的定义就隐含了这一点。

1. 架构速查图（Cheatsheet）
┌─────────────────────────────────────────────────────┐
│                 GPU Task Metadata (TMD)             │
├─────────────────────────────────────────────────────┤
│ 【任务形态】 Grid TMD | Queue TMD                   │
│ - Grid: 固定规模 Grid/CTA                          │
│ - Queue: 动态队列，支持生产-消费 & 子任务嵌套       │
├─────────────────────────────────────────────────────┤
│ 【调度字段】                                        │
│ - Priority (1~10)                                   │
│ - Timestamp / Allocation Order                      │
│ - Sequential Flag (单 CTA in-flight)                │
│ - Launch Quota (额度控制)                           │
│ - SM Affinity (亲和性)                              │
│ - Throttle Mode (共享内存独占/分割)                 │
│ - Coalescing Rules & Timeout (合并+超时)            │
│ - Dependent Enable + Pointer (依赖任务)             │
├─────────────────────────────────────────────────────┤
│ 【硬件路径】                                        │
│ - TMU (Task Management Unit): 接纳+排序+缓存        │
│   · Scheduler Table (优先级链表)                     │
│   · Launch Cache (完整 TMD)                         │
│   · Scheduler Cache (关键字段，快速判定就绪)         │
│                                                     │
│ - WDU (Work Distribution Unit): 分配+发射           │
│   · Task Table (运行中任务槽位)                     │
│   · SM State (CTA availability, 就绪标志)            │
│   · 策略：Load-Balancing / Round-Robin              │
│   · 抢占/恢复，高优先级驱逐低优先级                 │
├─────────────────────────────────────────────────────┤
│ 【执行流程】                                        │
│ 1. CPU/前序任务生成 TMD → Inline 写入 TMU           │
│ 2. TMU Launch Cache 命中 → Scheduler Cache 判定就绪  │
│ 3. WDU 选择合适 SM（平衡/轮转/优先级）              │
│ 4. 发射 CTA → SM 执行 → 更新状态/触发子任务/依赖     │
│ 5. 完成后依赖任务直接入队（免信号量）               │
└─────────────────────────────────────────────────────┘


2. 全景图（Pipeline/State 流程图）
 [CPU / Parent Task]
        │
        │  Inline Task Memory Request
        ▼
 ┌───────────────┐
 │  TMU (Task Mgmt) │
 │ ─────────────── │
 │ Scheduler Table │  ◄── Priority / Timestamp 排序
 │ Launch Cache    │  ◄── 完整 TMD 缓存 (写通/写回)
 │ Scheduler Cache │  ◄── 关键字段缓存 (快速判定)
 └───────────────┘
        │
        │ 就绪 TMD
        ▼
 ┌───────────────┐
 │  WDU (Work Dist) │
 │ ─────────────── │
 │ Task Table      │  ◄── 在跑任务槽位
 │ SM State        │  ◄── CTA availability, Ready
 │ Sched. Policy   │  ◄── Load-Balance / Round-Robin
 │ Preemption      │  ◄── 高优先级驱逐低优先级
 └───────────────┘
        │
        │  Assign CTA
        ▼
 ┌───────────────┐
 │  SM Cluster(s) │
 │ ─────────────── │
 │ CTA Execution   │  ◄── Warp/Thread Array
 │ Local Reg File  │
 │ Shared Mem / L1 │
 │ Global/L2/DRAM  │
 └───────────────┘
        │
        │  CTA 完成 / 状态更新
        ▼
 ┌─────────────────────┐
 │ Dependent Task Trigger │
 │ - Dependent Enable+Ptr │
 │ - 自动推送到 Scheduler │
 └─────────────────────┘



