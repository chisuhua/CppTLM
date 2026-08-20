https://patents.google.com/patent/US8984183B2/en1. 总览：这篇专利在解决什么问题
在 GPU 等多处理器系统里，父任务运行过程中会产生子任务数据并写入队列；传统做法需要CPU 参与（写 pushbuffer 或发通知）才能进入调度管线，导致额外往返延迟。这篇专利给出的方案是：
● 在写入队列的同一路径由硬件自动产生反射通知（reflected notification），直接发给任务管理单元，无需 CPU 介入；通知负载携带offset 与 delta，表示“从 offset 起，写好了 delta 长度的有效跨度 vspan”【图 3D、图 4、摘要】。(谷歌专利)
● 任务管理单元合并多条 vspan、只要队列头形成连续前缀就判定“可调度”，把子任务插入调度表，随后分发到 GPC 或 SM 执行【图 4、图 5、图 6】。(谷歌专利)
● 队列条目允许乱序写，但按前缀有序进入执行，降低延迟同时保持处理顺序【说明书明确提到 out-of-order 写入与 front-prefix 就绪】。(谷歌专利)

2. 关键对象、数据结构与信号
2.1 概念与对象对照表
对象/术语
作用
关键字段/要点
备注
TMD Task Metadata
描述任务的初始化、调度与执行参数；挂接工作队列
分发参数、门槛、队列描述等
图 3C 列举了 TMD 内容【图 3C】。(谷歌专利)
动态任务队列
存放子任务数据的线性或环形队列
head 前缀指针、容量 Q，可配“四阶段”推进（Outer/Inner）
支持乱序写，前缀就绪后触发调度【图 5】。(谷歌专利)
vspan 有效跨度
由通知带来的“已写好区间”
offset, delta
多条 vspan 在任务管理侧合并与前缀推进【图 4、图 6】。(谷歌专利)
反射通知
写入时自动发送的小报文
offset, delta, queue_id, tmd_id（实现可扩展）
绕过 CPU，直达任务管理单元【摘要】。(谷歌专利)
任务管理单元（Task/Work Unit 前端）
收通知、合并 vspan、判定就绪、入调度表
vspan 表、调度表、门槛器
直接与处理集群通信，无需 pushbuffer 指令【图 3A、摘要】。(谷歌专利)
分发单元
根据 TMD 分发参数把 CTA 发往 GPC/SM
min_ready_entries, num_cta, step_size 等
分发参数的细节也见相关专利 US9921873B2。(谷歌专利)
相关：错误检查/撤销机制等“发派前窗口”的做法与同族文献一致（如 out-of-order 调度错误检查）。(谷歌专利)

3. 任务生成 → 反射 → vspan 合并 → 前缀就绪 → 入调度 → 发派
3.1 写入与反射通知（高兼容流程图）

要点：通知与写同源，无需 CPU；只要形成从队列头起的连续前缀才可进入调度，从而兼顾乱序生产与有序处理（出自图 3D、图 4、图 6 的描述）。(谷歌专利)
3.2 分发与执行（与分发参数解耦）

TMD 的分发参数决定一次发多少 CTA、最小就绪量、step 指针步进等；该参数化分发参见 US9921873B2。(谷歌专利)

4. “schedule kernel 如何通知 CP”——从信号到发派的超细时序
这里把 schedule kernel 视为任务管理前端做“可调度化”的那段逻辑，CP对应前端的任务管理与分发总称（Task/Work Unit + Distributor）。这与专利“处理单元直接与任务管理单元通信”的结构一致。(谷歌专利)

这一链路完整复现了“无需 CPU 介入、由反射→合并→就绪→入表→发派”的连续动作【图 3A、3D、4、6、7 与摘要】。(谷歌专利)

5. vspan 与环形队列：图例与边界处理
5.1 vspan 合并示意（与环回）

● 通知顺序可能是 64..95 先到，再到 0..31，最后 32..63；合并后形成0..95 的连续前缀，此时 head 可推进到 96。
● 环形需要模长运算；必要时把区间拆成两段（尾段与头段）后再做合并与前缀推进。
● 对小碎 vspan，任务管理侧要快速合并，避免“通知风暴”。
5.2 门槛与分发参数
● 可调度门槛：可按条目数（如≥N）、字节数、或业务单元（帧/块）定义；门槛太低会碎片化，太高会拉长首包等待。
● 分发参数（在 TMD 中）：一次发多少 CTA、并发桶数、step_size 如何更新队列指针等（对应 US9921873B2 的参数化分发）。(谷歌专利)

6. 工程实现要点与取舍
1. 写通路旁挂“轻报文”：在写 DMA 或存储前端增加反射报文发射器，负载含 offset, delta, queue_id, tmd_id，直达任务管理单元（对应专利中“由写请求产生并直接发送的通知”）。(谷歌专利)
2. 任务管理的 vspan 结构：有序链表/平衡树/跳表均可；每次插入 O(logN)（或摊销近似 O(1) 合并）并尝试前缀推进。
3. 前缀就绪→写可调度标志→入调度表：保证“发派前窗口”内的一致性检查与可撤销处理（同族文献常见做法）。(谷歌专利)
4. 调参分离：就绪触发与分发参数解耦，便于按负载调优吞吐与延迟（US9921873B2）。(谷歌专利)
5. 可观测与诊断：保留“最近 N 条通知”“vspan 表摘要”“当前前缀长度”等寄存器或只读窗口，快速定位阻塞点。

7. 伪代码：环形队列 vspan 合并与调度触发（可落地）
目标：支持乱序写、合并 vspan、前缀推进、门槛就绪、入调度表、按 TMD 分发参数发派。以下写成接近工程可用的伪代码（无特定语言依赖）。
// 环形队列基本参数
struct Queue {
  u32 cap;          // 容量 Q
  u32 head;         // 前缀起点, 指向下一个待消费位置
  // 也可增设 Outer/Inner 四阶段指针, 此处从略
}

// 有效跨度 vspan 区间, 坐标使用 [start, end] 半开区间
struct Vspan { u32 start; u32 end; } // end = start + len; 均为相对 0..cap-1

// 任务元数据 TMD 关键字段
struct TMD {
  Queue q;
  u32   min_ready_entries;  // 可调度门槛
  u32   step_size;          // 分发步进
  u32   ctas_per_issue;     // 每次发派 CTA 数
  // 其他初始化与执行参数从略
}

// 任务管理侧的状态
struct SchedState {
  TMD* tmd;
  OrderedSet<Vspan> spans;   // 有序不重叠集合, 支持合并
  bool sched_flag;           // 可调度标志
  PriorityKey prio;          // 调度优先级键
}

// 将写入时的反射通知送入任务管理, offset,len 在 0..cap-1 范围
function on_reflect_notify(SchedState* st, u32 offset, u32 len):
  cap = st->tmd->q.cap
  // 1) 处理环回: 将 [offset, offset+len) 映射为 1 或 2 段线性区间插入
  segments = split_ring_segment(offset, len, cap)
  for seg in segments:
    insert_and_coalesce(st->spans, seg)    // O(logN) 插入并与相邻合并

  // 2) 前缀推进: 仅当从 head 起连续覆盖时推进
  advanced = try_prefix_advance(st)
  if advanced > 0:
    // 3) 门槛检查
    if prefix_ready_count(st) >= st->tmd->min_ready_entries:
       st->sched_flag = true
       insert_into_runqueue(st)            // 入调度表

// 将环形区间拆分为线性 1~2 段
function split_ring_segment(off, len, cap) -> list<Vspan>:
  end = off + len
  if end <= cap:
     return [ Vspan{off, end} ]
  else:
     // 跨环回: [off, cap) U [0, end-cap)
     return [ Vspan{off, cap}, Vspan{0, end - cap} ]

// 插入并合并: 保持 spans 有序且不重叠
function insert_and_coalesce(OrderedSet<Vspan>& S, Vspan s):
  // 找到第一个与 s.end 相邻或重叠的左侧区间 prev
  // 与第一个与 s.start 相邻或重叠的右侧区间 next
  // 根据是否相邻(相差 0)或重叠来合并为更大区间
  // 伪实现略, 核心是把 [s.start, s.end) 与左右连通的区间合并后写回

// 前缀推进: 尝试从 head 起尽量吃掉 spans 覆盖的连续区间
function try_prefix_advance(SchedState* st) -> u32 advanced:
  q    = &st->tmd->q
  cap  = q->cap
  cur  = q->head
  loop:
    seg = find_span_covering_point(st->spans, cur)
    if seg == null: break
    // seg 覆盖 cur, 则可推进到 seg.end
    remove_span(st->spans, seg)
    // 若 seg.end 超过 cap, 正常不应发生, 因为我们已线性化
    // 推进 head
    next = seg.end % cap
    advanced += distance_ring(cur, next, cap)
    cur = next
  q->head = cur
  return advanced

// 由前缀长度估算“可调度量”, 可按条目或字节计
function prefix_ready_count(SchedState* st) -> u32:
  // 也可用 last_advanced 做增量统计
  // 或维护 head_shadow 与生产指针的差值
  // 这里简单化: 返回最近一次 try_prefix_advance 的 advanced

// 入调度表
function insert_into_runqueue(SchedState* st):
  runqueue_push(st->prio, st)

// 调度循环中的发派逻辑
function scheduler_dispatch_loop():
  while true:
    st = runqueue_pop_highest_prio()
    if st == null: continue
    // 二次校验: 防止虚假可调度
    ready = prefix_ready_count(st)
    if ready < st->tmd->min_ready_entries:
       st->sched_flag = false
       continue
    issue_ctas(st, min(st->tmd->ctas_per_issue, ready / st->tmd->step_size))

// 发派 CTA 并更新队列指针
function issue_ctas(SchedState* st, u32 n):
  for i in 0..n-1:
    desc = make_work_descriptor(st->tmd, st->tmd->q.head)
    send_to_gpc(desc)                 // 下发到 GPC 或 SM
    st->tmd->q.head = (st->tmd->q.head + st->tmd->step_size) % st->tmd->q.cap
  // 发派后根据剩余 ready 决定是否保留在 runqueue
  if prefix_ready_count(st) >= st->tmd->min_ready_entries:
     runqueue_push(st->prio, st)
  else:
     st->sched_flag = false

实现说明
● split_ring_segment 解决环形跨界；insert_and_coalesce 用有序集合合并相邻或重叠区间；try_prefix_advance 只吃掉“覆盖 head 的区间”，确保仅以队列头为锚推进，维持处理顺序。
● min_ready_entries 是可调度门槛；ctas_per_issue、step_size 源自 TMD 分发参数，与就绪触发解耦，有助于在不同业务下调平延迟/吞吐（US9921873B2）。(谷歌专利)
● 真机上可把 prefix_ready_count 做成寄存器/计数器，由 try_prefix_advance 增量维护，避免昂贵扫描。
● 入表前可插入一致性检查/撤销窗口，与相关专利保持一致的安全性处理。(谷歌专利)

8. 与原文的关键对应
● 无需 CPU、反射通知 offset+delta、直接通知调度单元、降低延迟：摘要与定义段清晰描述。(谷歌专利)
● 队列 out-of-order 写、跟踪已写连续前缀、前缀就绪即调度：正文明确阐述，图 3D、图 4、图 5、图 6 给出流程与示意。(谷歌专利)
● TMD 内容、Task/Work Unit、GPC、图 3A–3C、3D、4、5、6、7：结构、数据与流程出现在这些图与对应段落。(谷歌专利)
● 分发参数化控制：US9921873B2 对“读 TMD 分发参数、CTA 粒度、步进与并发策略”有系统描述，可与本专利的就绪触发无缝衔接。(谷歌专利)
● 发派前窗口的检查/撤销：相关同族文献给出 out-of-order 调度的错误检查与边界处理参考。(谷歌专利)
9. 扩展与思考
如果考虑专利和DeviceGraph之间的关系：
● US8984183B2（专利）：讲是底层调度前端如何在“设备内部”低延迟地把运行中内核产生的子任务推入可执行队列：写队列→反射通知(offset, delta)→vspan 合并→“前缀就绪”→入调度表→分发到 GPC/SM。它解决的是就绪性判定与排程触发的硬件/固件路径，核心在绕开 CPU的同时保证有序与高吞吐（乱序生产、前缀消费）。
● CUDA Device Graph（设备侧图启动）：是上层 API/运行时能力：在设备端（内核里）直接 cudaGraphLaunch() 一个预实例化的 GraphExec，不用回主机；结合Tail / Fire-and-Forget / Sibling等模式与条件节点/循环，实现设备侧的控制流与批量启动，从而消除频繁的 host-launch 开销。(NVIDIA Developer)
换句话说：Device Graph 是“设备侧发起更多工作”的高层机制；而这篇专利提供的则是底层调度/就绪判定的硬件化思路。两者目标一致（减少 CPU 参与、降低延迟、提高吞吐），但抽象层次与实现颗粒度不同。

一图看关系（简化时序）
父内核(设备)  ──写子任务/或决定下一步────────┐
   │                                         │
   ├─(专利路径) 写队列→反射(offset,delta)→TM→入表→分发→执行   │  ← 底层：就绪判定/排程触发
   │                                         │
   └─(Graphs路径) 设备内核直接 cudaGraphLaunch(GraphExec) → 调度执行  ← 上层：已编排好的工作批

● 设备图启动常见两类：
    ○ Tail launch / Fire-and-Forget / Sibling：设备线程把图作为后续/并行工作排入图专用流；必须从图内启动（即“图内再起”）。(NVIDIA Docs)
    ○ 条件节点与循环节点（CUDA 12.8/Blackwell）：让部分子图在设备侧分支/重复，继续避免回主机。(NVIDIA Developer)

二者的“同与不同”
维度
US8984183B2（反射+vspan）
CUDA Device Graph
抽象层
硬件/固件调度前端、就绪性检测、队列与窗口
运行时/编程模型（GraphExec 设备侧启动）
触发机制
写入即反射：offset+delta→vspan 合并→前缀就绪→入表
设备侧 cudaGraphLaunch()；图已由主机预构建/实例化
顺序保证
乱序生产、前缀消费（head 连续才可调度）
依图内依赖边与图流模式（tail/faf/sibling）
适用形态
内核运行中不断“产数+产活”的流式/队列型
已知结构的批操作/控制流片段复用、内核内分支/循环
CPU 参与
旁路 CPU；写通路上自动通知调度前端
构图与实例化多由主机完成；启动可在设备侧进行
延迟/吞吐
通过就绪判定硬件化降低首包/批间延迟
通过批量启动与设备侧控制流降低 launch 开销与往返
文档/产品
专利思路（结构图 3/4/5/6/7）
CUDA 10 起支持 graphs；CUDA 12 起支持设备侧启动；后续加入条件节点、常数时延直线图优化等。(NVIDIA Developer)
结论：从体系结构看，Device Graph 更像是把“在设备端继续发起工作”的入口标准化；而专利给出了当设备端产生工作后，调度前端如何“快速判定可执行与分发”的底层机制。两条线在大型系统里是可互补/可叠加的：Device Graph 负责“怎么在设备端表达/发起工作批”，专利式反射+vspan 负责“这些工作批/子任务怎么被快速感知与发派”。

典型组合场景（工程建议）
● 图内设备侧分支 → 产出动态任务：用 条件节点/循环节点 表达控制流；当分支生成数据块时，用队列+反射+vspan（或等价的设备侧就绪门槛）驱动后续子任务的细粒度排程。这样粗粒度靠 Graph 批启动，细粒度靠反射门槛触发，既省 CPU，又保证数据就绪即开工。(NVIDIA Developer)
● 超低延迟管线：对于“到齐即干”的边缘拼装/解码/压缩流水，Graph 负责稳定骨架（预取+计算+后处理），反射+vspan 让数据齐段立刻进入执行，而不是等下一轮 host-launch。

参考（官方资料）
● CUDA 12 引入设备侧图启动；两种模式示例与应用动机。(NVIDIA Developer)
● CUDA Graphs 基础与动机、常数时延直线图优化（2024）。(NVIDIA Developer)
● 设备侧启动细节：必须从图内发起，并使用专用“设备-only 图流”：cudaStreamGraphTailLaunch、cudaStreamGraphFireAndForget、cudaStreamGraphFireAndForgetAsSibling。(NVIDIA Docs)

工程蓝图：Device Graph + 反射 vspan 队列
1) 架构与职责

● 反射 vspan：写队列时自动发通知到任务管理；合并 vspan并以队列头连续前缀为就绪条件→入调度与发派（降延迟，保障顺序）。
● Device Graph：在设备内核里用fire-and-forget（立即独立执行）或tail（按序、可自重启形成循环）启动已实例化图，避免回主机。(NVIDIA Developer)

2) 关键参数选型表
类别
参数
说明
经验值/建议
队列
cap
环形队列容量
取 2^k，至少覆盖 1–2 个批次；配合 L2/TMA 预取粒度
队列
head
前缀指针
仅由前缀连续 vspan推进
vspan
offset, delta
反射报文负载
写路径硬件产生，送任务管理
门槛
min_ready_entries
前缀就绪门槛
小 kernel 用较小门槛（快启动），大 kernel 与拷贝粒度对齐
分发
ctas_per_issue
每轮发派 CTA 数
与 SM 并发、寄存器占用、共享内存约束协同调优
分发
step_size
队列步进
与任务粒度相等；若有聚合可>1
Graph
cudaGraphInstantiateFlagDeviceLaunch
设备侧可启动
必须在实例化时设置；并upload到设备侧才能从设备启动。(NVIDIA Developer)
Graph
cudaStreamGraphFireAndForget
立即执行流
设备侧立即派发，与父图解耦，适合“准备好就开工”。(NVIDIA Developer)
Graph
cudaStreamGraphTailLaunch
尾随执行流
保序，且会等待所有 faf 启动完成；适合循环调度。(NVIDIA Developer)
设备侧图启动的约束：设备端必须从图内部再起图（不能从裸 <<<>>> 内核直接起图）；presentation 与社区答复均强调了这一点。(hihat.opencommons.org)

3) 设备侧 Graph 调度时序

● faf图用于立刻启动后续算子；tail让调度器自重启形成循环，并按序与前次 faf 完成同步。(NVIDIA Developer)

最小可运行样例（PoC骨架）
目标：Host 端构建三个子图与一个设备侧调度器图；调度器内核根据当前数据选择 zip/lzw/deflate 图，在设备侧用fire-and-forget启动；然后用tail自我重启，继续处理下一项。示例与官方样例同构，只是注释更工程化。(NVIDIA Developer)
Host 端（关键片段，C++/CUDA Runtime）：
// 1) 构建子图 并实例化为可设备侧启动的 GraphExec
cudaGraph_t zipG, lzwG, defG, schedG;
cudaGraphExec_t zipEx, lzwEx, defEx, schedEx;
cudaStream_t stream; cudaStreamCreate(&stream);

// 假设 create_*_graph 会往图里放一条 kernel 节点，并把数据指针参数接到 device 可见内存
create_zip_graph(&zipG, currentFileDataDevPtr);
create_lzw_graph(&lzwG, currentFileDataDevPtr);
create_deflate_graph(&defG, currentFileDataDevPtr);

// 实例化时一定要带 Device Launch 标志; 并尽早 upload，避免首发隐式上传的额外延迟
cudaGraphInstantiate(&zipEx, zipG, cudaGraphInstantiateFlagDeviceLaunch);
cudaGraphUpload(zipEx, stream);
cudaGraphInstantiate(&lzwEx, lzwG, cudaGraphInstantiateFlagDeviceLaunch);
cudaGraphUpload(lzwEx, stream);
cudaGraphInstantiate(&defEx, defG, cudaGraphInstantiateFlagDeviceLaunch);
cudaGraphUpload(defEx, stream);

// 2) 捕获包含 schedulerKernel 的“调度器图”
cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);
schedulerKernel<<<1, 1, 0, stream>>>(filesDev, numFiles, currentIdxDev, currentFileDataDevPtr,
                                      zipEx, lzwEx, defEx);
cudaStreamEndCapture(stream, &schedG);
cudaGraphInstantiate(&schedEx, schedG, cudaGraphInstantiateFlagDeviceLaunch);

// 3) Host 首次启动调度器图（会隐式或显式完成 upload）
cudaGraphLaunch(schedEx, stream);

设备侧调度器内核（选图 + faf + tail 自重启）：
enum compressionType { zip = 1, lzw = 2, deflate = 3 };

struct FileRec {
  compressionType comp;
  void* data;
};

__global__ void schedulerKernel(FileRec* files, int numFiles, int* cur,
                                void** curData,
                                cudaGraphExec_t zipExec,
                                cudaGraphExec_t lzwExec,
                                cudaGraphExec_t defExec)
{
    // 1) 绑定当前任务数据
    *curData = files[*cur].data;

    // 2) 根据业务类型在设备侧 fire-and-forget 启动对应图
    switch (files[*cur].comp) {
      case zip:     cudaGraphLaunch(zipExec, cudaStreamGraphFireAndForget);     break;
      case lzw:     cudaGraphLaunch(lzwExec, cudaStreamGraphFireAndForget);     break;
      case deflate: cudaGraphLaunch(defExec, cudaStreamGraphFireAndForget);     break;
      default: break;
    }

    // 3) 递增任务索引 并 tail 自重启
    int next = atomicAdd(cur, 1) + 1;
    if (next < numFiles) {
        // 设备端获取自身图句柄并尾随重启
        cudaGraphExec_t self = cudaGetCurrentGraphExec();
        // tail 会在本次图+其派生的所有 faf 执行完之后再启动下一轮
        cudaGraphLaunch(self, cudaStreamGraphTailLaunch);
    }
}

要点：
● 子图与调度器图都实例化为 device-launchable，并提前 upload；设备内核里用 cudaGraphLaunch(exec, cudaStreamGraphFireAndForget) 或 cudaStreamGraphTailLaunch 自重启循环。(NVIDIA Developer)
● 设备侧必须从图内再起图（而不是随便一个 <<<>>> 内核）；这是 12.x 的限制与最佳实践。(hihat.opencommons.org)

反射 vspan 调度：加强图例与伪代码
vspan 合并与前缀推进（流程图）

伪代码（可落地，环形队列 + 合并 + 门槛 + 发派）
// 环形队列
struct Queue { u32 cap; u32 head; }

// vspan 区间 [start, end) (半开区间), 坐标按 0..cap-1
struct Vspan { u32 start; u32 end; }

struct TMD {
  Queue q;
  u32   min_ready_entries;
  u32   step_size;
  u32   ctas_per_issue;
}

struct SchedState {
  TMD* tmd;
  OrderedSet<Vspan> spans;  // 有序不重叠集合
  bool sched_flag;
  PriorityKey prio;
}

// 写时反射通知入口
function on_reflect_notify(st, offset, len):
  cap = st.tmd.q.cap
  segs = split_ring_segment(offset, len, cap)   // 处理跨环回
  for seg in segs:
    insert_and_coalesce(st.spans, seg)          // O(logN)
  advanced = try_prefix_advance(st)             // 从 head 起吃掉覆盖区间
  if advanced > 0 and prefix_ready(st) >= st.tmd.min_ready_entries:
     st.sched_flag = true
     runqueue_push(st.prio, st)

// 将可能跨环回的 [off, off+len) 拆成 1~2 个线性段
function split_ring_segment(off, len, cap):
  end = off + len
  if end <= cap: return [Vspan{off, end}]
  else:          return [Vspan{off, cap}, Vspan{0, end - cap}]

// 插入合并：与左右相邻或重叠的区间合并成更大段
function insert_and_coalesce(S, s):
  // 省略实现细节：按 start 查找相邻区间并合并，保持不重叠有序

// 仅以 head 为锚推进前缀
function try_prefix_advance(st):
  cap = st.tmd.q.cap
  cur = st.tmd.q.head
  advanced = 0
  loop:
    seg = find_span_covering_point(st.spans, cur)
    if seg == null: break
    remove(st.spans, seg)
    next = seg.end % cap
    advanced += distance_ring(cur, next, cap)
    cur = next
  st.tmd.q.head = cur
  add_to_prefix_counter(st, advanced)  // 用寄存器记录累计前缀
  return advanced

function prefix_ready(st): return read_prefix_counter(st)

// 调度循环：发派 CTA 并按 step_size 推进
function scheduler_dispatch_loop():
  while true:
    st = runqueue_pop_highest_prio()
    if st == null: continue
    if prefix_ready(st) < st.tmd.min_ready_entries:
        st.sched_flag = false
        continue
    n = min(st.tmd.ctas_per_issue, prefix_ready(st) / st.tmd.step_size)
    issue_ctas(st, n)
    if prefix_ready(st) >= st.tmd.min_ready_entries:
        runqueue_push(st.prio, st)
    else:
        st.sched_flag = false

function issue_ctas(st, n):
  for i in 0..n-1:
    desc = make_descriptor(st.tmd, st.tmd.q.head)
    send_to_gpc(desc)
    st.tmd.q.head = (st.tmd.q.head + st.tmd.step_size) % st.tmd.q.cap

实现要点
● 反射报文在写路径生成（offset, delta），任务管理侧做“有序集合合并+前缀推进”，仅在从 head 起连续时推进，确保处理有序、生产可乱序。
● 门槛到达→写可调度标志并入排程；分发参数（ctas_per_issue, step_size）与就绪触发解耦。
● 设备侧 Graph 用 faf 形成“准备好就开工”的并发分支，用 tail 让调度器自重启形成循环与自然同步。(NVIDIA Developer)

上线建议与检查清单
● Graph 准备：所有拟在设备侧启动的 cudaGraphExec_t 必须以 cudaGraphInstantiateFlagDeviceLaunch 实例化并 cudaGraphUpload 至设备（可以首发隐式完成，但建议显式）。(NVIDIA Developer)
● 设备侧限制：设备侧图必须从图内再起（不要从普通内核直接起）。(hihat.opencommons.org)
● 队列尺：与 L2/TMA 传输粒度、CTA 粒度匹配；避免碎片化通知风暴（可在写侧聚合或在任务管理侧快速合并）。
● 可观测性：暴露“最近 N 条通知”“vspan 表摘要”“前缀长度计数器”等只读窗口，便于线上定位“前缀卡住”的根因。

