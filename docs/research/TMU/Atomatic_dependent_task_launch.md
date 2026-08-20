TWI619075B — Automatic dependent task launch申请人：NVIDIA｜授权：2018-03-21｜优先权：2012-01-27

一、背景与现有问题
1.1 传统信号量模式
● GPU 任务间依赖通常靠 semaphore（释放/获取） 实现：
    ○ 第一任务完成 → 写内存信号量 → CPU 或 GPU 轮询 → 第二任务才能启动。
● 缺陷：
    a. 多次内存交易（写 + 读）。
    b. 带宽浪费（频繁访问全局内存）。
    c. 高延迟（CPU/GPU 间切换导致的等待）。
1.2 专利目标
● 在 第一任务的 TMD (Task MetaData) 里直接写入 依附任务信息（使能标志 + 指针）。
● 当第一任务完成时，任务调度单元 (TMU) 读取 TMD，立即调度依附任务：
    ○ 无需 semaphore。
    ○ 延迟更低。
    ○ 支持 链式依附（多个任务自动衔接）。

二、系统部件（架构概览）
● Front End (212)：接收 CPU/驱动提交的任务，生成 TMD。
● Task/Work Unit (207)：管理任务表，与调度单元交互。
● TMU (Task Management / Scheduling Unit)：
    ○ 监控任务完成信号。
    ○ 检查 TMD 的依附字段 (424)。
    ○ 自动启动依附任务。
● WDU (Work Distribution Unit)：把就绪任务分配到 SM。
● GPC/SM：执行任务。
● L2/DRAM：存放 TMD 和数据。
● TMD Cache (350)：加速 TMD 访问，管理写回/回收。

三、TMD 结构（Fig.4A）
+------------------------------------------------------+
|                  Task Metadata (TMD)                 |
+----------------+----------------+--------------------+
| Init Params    | Sched Params   | Exec Params        |
| (406)          | (410)          | (416)              |
+------------------------------------------------------+
| Cooperative Execution Queue State (420)              |
+------------------------------------------------------+
| Hardware-only Field (422)                            |
+------------------------------------------------------+
| Dependent Task Metadata Field (424):                 |
|   - Enable Flag (bit)                                |
|   - Pointer/Index to Dependent Task TMD              |
|   - (可选) Type/Format 标识                          |
+------------------------------------------------------+
| Queue Field (426)                                    |
+------------------------------------------------------+

字段说明
● Init Params (406)：初始化参数，由驱动写入。
● Sched Params (410)：调度参数，如优先级、流 ID。
● Exec Params (416)：执行相关参数（kernel 入口、寄存器配置）。
● Queue State (420)：协同执行状态。
● Hardware-only (422)：仅硬件可写的字段，用于执行状态/保护。
● Dependent Task Field (424)：
    ○ Enable Flag：是否启用依附任务。
    ○ Pointer：指向依附任务的 TMD。
● Queue Field (426)：任务链和队列管理。

四、运行流程（Fig.5–7）
4.1 单依附任务（Fig.5）
[Host] 构造 First.TMD {dep.enable=1, dep.ptr=Dep.TMD}
   ↓
[GPU/SM] 执行 First
   ↓ 完成
[TMU] 收到完成信号 → 检查 First.TMD.dep
   ↓ dep.enable=1 → 读取 dep.ptr
   ↓ 调度 Dep.TMD
   ↓
[SM] 执行 Dep
   ↓
[TMD Cache] 回收 First.TMD 存储

4.2 父子依附（Fig.6）
First.TMD --dep.ptr--> Dep1.TMD
Dep1.TMD --dep.ptr--> Dep2.TMD
Dep2.TMD --dep.ptr--> ...

4.3 链式依附（Fig.7）
● 每个任务完成 → TMU 检查 TMD(424)。
● 若有依附 → 自动调度下一个。
● 链式执行持续到 dep.enable=0。

五、硬件实现细节表
模块
职责
来源
Front End (212)
解析 CPU 提交的任务，生成 TMD
专利原文
Task/Work Unit (207)
管理任务表，调度入口
专利原文
TMU
检查完成信号，读取 TMD(424)，自动启动依附任务
专利原文
WDU
分发就绪任务至 SM
专利原文
GPC/SM
执行任务，发出完成信号
专利原文
TMD Cache (350)
提供缓存，任务完成后清除/回收
专利原文
通知信号线、中断号
专利未说明，需实现方定义
工程推导
环路检测机制
专利未说明，需额外实现
工程推导

六、软件实现细节表
层次
动作
来源
用户态 Runtime
构造任务链，写入 dep.enable/dep.ptr
专利原文
驱动/内核
分配/回收 TMD，管理缓存一致性
专利原文
CPU
由“逐步调度”转为“批量提交+状态观测”
专利原文
健壮性（链深限制、环路检测）
专利未涉及，需实现方补充
工程推导

七、保存/恢复字段清单
字段
功能
来源
dep.enable
指示是否存在依附任务
专利原文
dep.ptr
指向依附任务 TMD
专利原文
hardware-only (422)
硬件独写，避免冲突
专利原文
类型/格式标识
依附任务的类型指示
专利原文
中断/寄存器号
专利未给出
工程推导
异常恢复策略
专利未给出
工程推导

八、专利原文 vs 工程推导
内容
归属
TMD 中依附任务字段 (424) 含 enable + pointer
专利原文
第一任务完成 → TMU 自动调度依附任务，无需 semaphore
专利原文
启动依附任务时回收第一任务 TMD 存储
专利原文
链式依附推进
专利原文
信号线、寄存器位宽、异常检测
工程推导
环路检测、最大链深度
工程推导

九、关键图示（ASCII）
Fig.4A – TMD 字段布局
+------------------------------------------------------+
|                  Task Metadata (TMD)                 |
+----------------+----------------+--------------------+
| Init Params    | Sched Params   | Exec Params        |
| (406)          | (410)          | (416)              |
+------------------------------------------------------+
| Cooperative Execution Queue State (420)              |
+------------------------------------------------------+
| Hardware-only Field (422)                            |
+------------------------------------------------------+
| Dependent Task Metadata Field (424):                 |
|   - Enable Flag                                      |
|   - Pointer to Dependent Task TMD                    |
+------------------------------------------------------+
| Queue Field (426)                                    |
+------------------------------------------------------+

Fig.5 – 自动依附启动流程
[First Task] --执行--> [完成通知] --→ TMU 检查 424
     | dep.enable=1
     v
 [立即调度 Dep Task]
     ↓
 [Dep 执行中]
     ↓
 [回收 First.TMD]

Fig.6 – 父子依附链
First.TMD ──dep.ptr──> Dep1.TMD ──dep.ptr──> Dep2.TMD

Fig.7 – 链式推进
TaskN 完成 → TMU 查 TMD.dep → 启动 TaskN+1
    └──若 TaskN+1 也有 dep → 持续推进


十、Swimlane 时序图
CPU                FE                  TMU                 SM
 |                  |                   |                  |
 | Submit First     |                   |                  |
 |----------------->| Create TMD(First) |                  |
 |                  |------------------>| Queue First      |
 |                  |                   |----------------->|
 |                  |                   |                  | Execute First
 |                  |                   |                  |--------------->
 |                  |                   |                  | First Done
 |                  |                   |<-----------------|
 |                  |                   | Completion Notice
 |                  |                   | Check dep.enable
 |                  |                   | dep.ptr=Dep.TMD
 |                  |                   |----------------->|
 |                  |                   |                  | Execute Dep
 |                  |                   | Mark First done  |
 |                  |                   | Reclaim storage  |


十一、工程落地关注点
1. 性能：绕开 semaphore，减少内存读写延迟。
2. 资源利用率：TMD 存储即时回收。
3. 链式任务流水化：适合大规模任务图。
4. 风险点：需额外实现环路检测、链深度限制。
5. 兼容性：与 semaphore 模可并行存在，根据场景选用。

十二、小结
TWI619075B 通过在 任务元数据 (TMD) 中编码 依附任务字段 (424)，实现了 GPU 内部的 自动依附任务启动机制：
● 无需 semaphore，显著降低延迟与内存带宽占用。
● 依附任务链式衔接，提升吞吐。
● 即时回收 TMD 存储，提高资源利用率。
● 专利明确了字段/流程，但具体寄存器、异常处理等实现细节留给工程方设计。
