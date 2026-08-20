https://patents.google.com/patent/US9286119B2/en下面这篇专利讲的是：把“任务依赖管理”从软件内核搬到硬件（TMU：Task Management Unit）里，用“动态任务依赖图 + 引用计数（refcount）+ 占位节点（placeholder）+ 流与事件（streams/events）”来做硬件级调度与同步，从而减少软件参与带来的往返与内存访问开销、降低调度/同步延迟。
我把要点、机制、与上一篇（PREEXIT/ACQBULK）专利的关系理一遍，顺带给出两幅简化 ASCII 图和一点伪代码，方便你和团队讨论/落地。

一、核心观点（Executive summary）
● 动态任务依赖图（dynamic task-dependency graph）：每个任务对应一个任务数据结构（scheduler object），含
    ○ ReferenceCnt（引用计数，表示尚未满足的依赖数）
    ○ OutDependence[i]（输出依赖指针，指向后继任务的任务数据结构）
    ○ TMDPointer（指向任务元数据 TMD：kernel 入口、参数、数据位置、优先级等）
● 占位节点（placeholder）：在软件真正填好 TMD 之前，先把“位置”接到流（stream）的链表上，后面填好 TMD 再把 ReferenceCnt--。这让任务图的结构可先行固化，而“任务内容”可后补。
● 硬件 TMU 组件：
    ○ TDR（Task-Dependence Resolution）：维护/解析依赖，做 refcount 递减、触发就绪
    ○ SL（Schedule & Launch）：根据优先级等把“就绪任务”调度/下发到 SMs
    ○ Task Cache：在任务就绪时预取任务状态（TMD 等），缩短 launch 前准备时间
● 触发规则：ReferenceCnt == 0 → 任务就绪 → 进入 SL 调度/launch
● 多流与事件同步：
    ○ Record Event：在流 A 放一个“记录事件”任务（可是真任务，也可仅信号，不下发到 SM）
    ○ Event Stream：为事件创建专门的“事件流”，其中挂多个 Wait Event 占位
    ○ 其他流的依赖任务把自身 ReferenceCnt++，并在事件触发时被 TDR 递减 refcount
结果：依赖解析、就绪判定、launch 前预取都在硬件里快速完成，减少 OS/driver 周期性扫描、优先级重算、信号量管理等软件路径上的往返与内存瓶颈。

二、数据结构与执行生命周期
1) 任务数据结构（专利 Fig.5）
TaskStruct {
  int ReferenceCnt;              // 未满足依赖计数
  ptr OutDependence[0..K-1];     // 指向后继任务的指针数组
  ptr TMDPointer;                // 任务元数据(内核入口/参数/优先级/数据指针…)
  // 可选：优先级等扩展字段
}

2) 任务就绪与传播（TDR 负责）
● 就绪条件：ReferenceCnt == 0 && TMDPointer != NULL
● 完成回写：当任务在 SM 完成，TDR 遍历它的 OutDependence[]：
    ○ 对每个后继 succ：succ.ReferenceCnt--
    ○ 若 succ.ReferenceCnt == 0 → 送入 SL，并预取 succ.TMD 到 Task Cache
● 占位到“真任务”：当软件把 TMDPointer 填好后（比如 device-side enqueue 或 host 端），也会触发一次 ReferenceCnt--
极简伪代码（TDR 部分）
void on_task_complete(TaskStruct* t) {
  for (auto* succ : t->OutDependence) {
    atomic_dec(succ->ReferenceCnt);
    if (succ->ReferenceCnt == 0 && succ->TMDPointer != NULL)
      schedule_ready(succ); // 交给 SL，SL 会预取其 TMD
  }
}

void on_task_populated(TaskStruct* t) { // 软件把 TMD 填好
  t->TMDPointer = ...;
  atomic_dec(t->ReferenceCnt);
  if (t->ReferenceCnt == 0)
    schedule_ready(t);
}


三、Streams & Events：跨流同步机制
● 流（stream）：本质是有序链表（linked list），每个节点是 TaskStruct
● 插入事件：
    ○ 在流 A 的占位节点附上 RecordEvent（可选是否真执行）
    ○ 创建事件流，在其中挂多个 WaitEvent 占位
    ○ 其他流的依赖任务指向对应 WaitEvent，并将自身 ReferenceCnt++
● 事件发生：
    ○ RecordEvent 达到触发 → TDR 递减事件流中相应 WaitEvent 的 ReferenceCnt
    ○ WaitEvent 达到 0 → 对应依赖任务的 ReferenceCnt--（或通过 OutDependence 链接）
简化 ASCII（两个普通流 + 一个事件流）
Stream A:   Task_A0 -> [RecordEvent E0] -> Task_A1 -> ...
                             | (OutDependence)
EventStrm:                [Wait_E0_for_B] -> [Wait_E0_for_C] -> ...
                             |                    |
Stream B:                 Task_Bk (RefCnt+=1)    |
Stream C:                                      Task_Cm (RefCnt+=1)

事件发生路径：
A.RecordEvent 完成 → (TDR) 使 Wait_E0... 就绪 → (TDR) 递减 B/C 对应任务 RefCnt → 
若 RefCnt==0 → 进入 SL → 预取 TMD → dispatch 到 SM


四、和PDL（PREEXIT/ACQBULK）的关系
维度
QMD（硬件任务图+refcount）
PDL（PREEXIT/ACQBULK+预flush+prefetch）
目标
把依赖管理/就绪判定/预取硬件化，减少软件调度/信号量开销
把依赖“在任务内”解耦：调度依赖用 PREEXIT 提前释放，数据依赖用 ACQBULK 等待
粒度
任务图级（跨任务、跨流、跨来源），支持 host/device 共同生成
任务执行级（生产者/消费者之间的 launch latency 缩短、流水化 overlap）
机制
refcount + OutDependence 链 + 占位节点 + 事件流
PREEXIT 触发调度、ACQBULK 等待数据、预flush/预取叠加
互补
谁该被调度 → 硬件更快判定
何时能提前启动/重叠 → 任务内更细粒度地隐藏延迟
结合两者：TMU 让“就绪任务”出现得更快且成本更低；PREEXIT/ACQBULK 让“就绪后的执行阶段”进一步重叠、再压 launch latency 和关键路径。

五、性能动机与收益点
● 减少软件轮询/更新优先级/信号量管理的周期性延迟（特别是多任务、小 kernel 场景）
● 占位节点允许先建图后补内容，避免“等待任务完全生成后再挂接”带来的串行化
● Task Cache 的预取把 TMD 访问延迟前移，与依赖解析重叠
● 支持 device-side 生成子任务（SM 内核生成 TMD/指针），不必回到 host 侧协调

六、实现注意点（工程角度）
1. 可扩展性
    ○ OutDependence[] 固定容量 vs 链接扩展（链表/溢出桶/间接表）；避免热节点溢出
    ○ ReferenceCnt 宽度与饱和；原子递减并发安全
2. 环与死锁
    ○ 需要拒绝环或做环检测（硬件代价大，实际多在软件构图时保证 DAG）
3. 一致性与可见性
    ○ 当 TMDPointer 填好再 RefCnt-- 时，需保证TMD 可见性（写入顺序/屏障）
4. 优先级与公平性
    ○ SL 按优先级/年龄/配额调度，避免某事件扇出巨大导致“雪崩式”独占
5. 事件流的生命周期
    ○ 事件完成后的回收策略；防止事件流无限增长
6. 回退路径
    ○ 极端依赖规模或稀有模式下，允许软件回管控（debug/诊断）

七、和 CUDA 概念的映射（便于你团队对齐）
● TaskStruct ≈ 图节点/Graph Node；OutDependence ≈ 有向边
● ReferenceCnt ≈ 入度计数（所有前置条件满足才“零”）
● TMD ≈ CUDA kernel launch 元数据（函数指针、grid/block 配置、参数包、优先级…）
● Streams/Events ≈ CUDA Streams/Events，但这里强调硬件 TMU 内建的事件流与等待点
● Device-side enqueue：SM 上的 kernel 产生子任务 → 直接把指针给 TMU，无需往返 host
● CUDA Graphs：理念近似，但本专利强调在硬件里常驻、动态增长的 DAG + refcount 触发

八、两幅简图
1) 单流 + 占位 + 传播
[ Task_0 ] --Out--> [ Task_1 (placeholder) ] --Out--> [ Task_2 (placeholder) ] ...
   | RefCnt=1            | RefCnt=2 (SW+T0)                 | RefCnt=2 (SW+T1)
   | TMD ok              | TMD=null                          | TMD=null

流程：
- T0 完成 → TDR 遍历 Out → Task_1.RefCnt-- (from 2->1)
- 软件填 Task_1.TMD → Task_1.RefCnt-- (1->0) → 进入 SL → 预取 TMD → launch
- Task_1 完成 → 推动 Task_2.RefCnt-- …（类推）

2) 事件同步（A 的事件唤醒 B/C）
Stream A: Task_A0 -> [RecordEvent E0] -> Task_A1
                          |                 (Out to A1)
EventStrm:              [Wait_E0_for_B] -> [Wait_E0_for_C]
                          |                    |
Stream B:              Task_Bk (RefCnt+=1)    |
Stream C:                                   Task_Cm (RefCnt+=1)

当 E0 触发：
Wait 节点就绪 → 递减 Bk/Cm 的 RefCnt；变 0 则进入 SL 并 launch


九、你可以怎么用（实践建议）
● 图前置：先构建“占位链”，让 TMU 看到整体依赖；再由 host/device 渐进填充 TMD
● 粗粒度同步→事件化：把跨流的 barrier 拆成“record/wait”事件，避免软同步（DeviceSync/StreamSync）的大停顿
● refcount 亲和性：对高扇出节点，尽量分层（多级触发），减轻一次性风暴
● 与 PREEXIT/ACQBULK 协同：就绪后的 kernel 内部，继续用 PREEXIT/ACQBULK 叠加流水化与预取/预flush 隐藏
