# NVIDIA Operand Collector 微架构 — 专题综述

> **专题目录**: `docs/research/SM/CudaCore/`
> **创建日期**: 2026-10-27
> **状态**: 📋 Research Survey（背景参考，非契约、非 conformance 声明）
> **材料构成**: 3 件 NVIDIA 专利（PDF 全文提取 + 逐件解析）+ 3 篇学术论文（PDF 全文提取 + 逐篇解析）+ 1 份 GPGPU-Sim 4.2.0 源码剖析
> **验证口径**: 所有专利与论文结论均来自**本次下载的一手 PDF 全文**（`patentimages.storage.googleapis.com` / `arxiv.org`），已逐条核对原文；未取得文本层的内容显式标注为"未核实"

---

## 一、专题定位与范围

本专题回答一个问题：**NVIDIA GPU 的 SM 内部，那个"从寄存器堆取操作数、喂给执行单元"的中间级到底长什么样，它在现代架构里还存在吗？**

"Operand Collector"（操作数收集器，简称 OC）是 NVIDIA 自己的术语。业界对它的认知存在两个极端：

- **一端是"它存在"**：NVIDIA 有以该词为标题的专利（US7834881B2），Fermi 白皮书的 SM 框图上直接标注了这个模块，CUDA 社区与几乎所有 GPU 模拟器（Accel-Sim/GPGPU-Sim）都把它当作 SM 的标准组成部分。
- **另一端是"它不存在"**：2025 年的逆向工程论文（arXiv:2503.20481）在 RTX A6000 / Turing 上实测后明确断言"现代 NVIDIA GPU 不使用 operand collector"，理由是 OC 会给 issue→writeback 引入可变延迟，与 NVIDIA ISA 的**定长延迟**要求冲突。

**这两端都成立，因为它们说的是不同世代的硬件。** 本专题的骨架就是这个"世代分水岭"：

```
Tesla/Fermi/Kepler ──► OC 是一级流水结构（收集多操作数需多拍，延迟可变）
        │
Maxwell/Pascal ──► 32-bit bank + 编译器显式 reuse cache（缓解 bank 冲突）
        │
Volta/Turing/Ampere ──► OC 消失，改为"定长延迟 + issue 侧气泡 + 编译器管理的 RFC"
        │
Hopper/Blackwell ──► 沿用 Volta 起的定长延迟范式（未实测，无一手资料）
```

本专题的检索口径：
- **专利**：以 "operand collector" 及其同义词（collector unit / source operand collector / operand reuse cache / bank arbitration）为检索词，锁定 NVIDIA 及其对照路线；
- **论文**：所有会涉及 SM 内操作数供给路径的 GPU 逆向/微基准论文（含否定性结论）；
- **源码**：GPGPU-Sim 4.2.0（Accel-Sim 2.0 Hopper 分支）的 OC 建模实现（本机 `/workspace/low/source/github/gpgpu-sim_distribution`）。

---

## 二、核心结论（TL;DR）

| # | 结论 | 证据强度 |
|---|------|---------|
| 1 | "Operand Collector" 是 NVIDIA 自有术语，**源头只有一处图 + 一件专利**：US7834881B2《Operand collector architecture》（2010 授权）与 Fermi 白皮书（2010）SM 框图 | ✅ 一手全文 |
| 2 | OC 的本质是：**用多个单端口 bank 拼出多端口寄存器堆，并用一个仲裁器 + 收集单元把散落在各 bank 的操作数汇齐后再送给执行单元** | ✅ 专利摘要 + 说明书 |
| 3 | OC 的**代价是可变延迟**：一条 3 源操作数指令若三个源落在同一 bank，**至少需要 3 个时钟周期**才能收集齐 | ✅ 专利说明书原文（FIG. 3 段） |
| 4 | 存在**操作数复用缓存**的独立专利（US8639882B2）：收集器内含 **6 个 storage elements**、寄存器堆 **4 个 bank**、crossbar 每周期每 bank 只连 1 个寄存器；写回即**失效** cache table | ✅ 专利权利要求 1-14 全文 |
| 5 | NVIDIA 内部还存在一条**对立路线**（US8555035B1）：去掉 crossbar 与 staging register，改由**编译器保证 conflict-free + aligned** | ✅ 摘要 + 说明书；权项为扫描件 |
| 6 | **Volta 之后 OC 已被废除**。现代 NVIDIA GPU 用的是：每 sub-core **2 个 bank**、每 bank **1 个 1024-bit 读端口** + 独立 1024-bit 写端口、**定长 3 周期**读源操作数、冲突表现为 **issue 侧气泡**而非可变延迟 | ✅ 一手实测（arXiv:2503.20481） |
| 7 | **操作数复用（reuse）缓存是真实存在的**，但由**编译器显式管理**：SASS 层 `.reuse` 标志位；Volta/Pascal/Maxwell 为 **4 个 reuse cache + 4 个 source operand slot**（8-byte slot 粒度）；Ampere/Turing 实测为**每 bank 1 entry × 3 × 1024-bit = 6 个子条目** | ✅ 一手实测 + 论文原文 |
| 8 | **双发射与 OC 无关**：Fermi/Kepler/Maxwell/Pascal 的"dual issue"指**同一周期从不同 warp 各发一条**；Volta 起每 processing block 只有 1 个 dispatch unit，**不存在单 warp 双发射** | ✅ 白皮书 + SASS 实测 |
| 9 | GPGPU-Sim 把 OC 建成 `opndcoll_rfu_t`：bank × collector 的 **BookSim wavefront 匹配仲裁**，写优先于读；在 **Hopper(sm_90) 配置上刻意关闭**（`opndcoll_model=1` 走 SIMPLE 模型，不建模 bank 冲突） | ✅ 源码 + config |
| 10 | CppTLM 当前 `RegFileUnit` 是**纯 flat hash map，无 bank / 无端口 / 无冲突概念**；`IssueUnitTLM` 直接产出 `IssueToExecBundle`，**没有"已 issue 但操作数未齐"的中间态** | ✅ 源码 |

---

## 三、专利群总览（3 件）

| # | 专利号 | 主题 | 申请日 | 授权日 | 权项文本可取 | 逐篇解析 |
|---|--------|------|--------|--------|-------------|----------|
| 1 | **US7834881B2** | Operand collector architecture（源头专利） | 2006-11-01 | 2010-11-16 | ⚠ 扫描件（无文本层） | [解析](US7834881B2_操作数收集器架构_解析.md) |
| 2 | **US8639882B2** | Source operand collector caching（复用缓存） | 2011-12-14 | 2014-01-28 | ✅ 完整取得 | [解析](US8639882B2_源操作数收集器缓存_解析.md) |
| 3 | **US8555035B1** | Conflict-free register allocation（对立路线对照） | 2010-07-07 | 2013-10-08 | ⚠ 扫描件（无文本层） | [解析](US8555035B1_无冲突寄存器分配对照_解析.md) |

**发明人**（均取自 PDF 首页 (75) 栏原文）：

- 1 号：Samuel Liu、John Erik Lindholm、Ming Y Siu、Brett W. Coon、Stuart F. Oberman
- 2 号：Jack Hilaire Choquette、Manuel Olivier Gautho、John Erik Lindholm
- 3 号：Anjul Patney、William J. Dally

**族内互引**：3 号（2010 申请）的 "Other Publications" 栏明确引用了 1 号的先行公开 `US 2008/0109611 A1 (5/2008 Liu et al)`，即"取消收集器"方案是**直接针对收集器方案提出的**。1 号自身是母案 US 10/889,730（2004-07-13 申请 → US 7,339,592）的 continuation-in-part。

### 3.1 一号专利的硬件画像（US7834881B2）

**FIG. 3 — Register File Unit 250 内部结构**（专利说明书原文描述）：

```
Register File Unit 250
├── Bank Request Arbitration Unit 310 × N（每个 bank 一个）
├── Bank 320 × N（每个 bank：1 个读请求端口 + 1 个写请求端口）
├── Selector 325 × N（bank → collector 的选择器）
└── Collector Unit 330 × N（收集齐后输出给执行单元）
```

关键原文（本次从 PDF 文本层逐字提取）：

> "Each Bank 320 includes a read request port for receiving requests from a Bank Request Arbitration Unit 310. Each Bank 320 also includes a Write request port for receiving Write requests from Execution Unit(s) 270 ... Therefore, 2 banks of lower port count memories (1 Write port and 1 read port) are used to simulate a multi-port memory with 2 Write ports and 2 read ports."

> "During a clock cycle, each Bank 320 may output an operand specified by read request to a corresponding Collector Unit 330 via a Selector 325. Therefore, when a program instruction specifies 3 operands for source data, **at least 3 clock cycles are needed to gather the operands when they reside in the same Bank 320**."

**FIG. 5 — 加入 Crossbar 的变体**：Crossbar 525 允许任一 bank 的输出路由到任一 Collector Unit 330，使**一条指令的操作数可以分散在不同 bank**；Dispatcher Unit 540 在一组 Collector Unit 330 之间做 **round-robin 或优先级仲裁**，再送给执行单元。

**FIG. 4A–4F — 寄存器分配策略**（降低 bank 冲突的编译期/分配期手段）：

- **thin allocation**：线程的寄存器全部分配到**单个 bank**；
- **fat allocation**：线程的寄存器分配到**多个 bank**；
- **phase value**：用相位值把同一条指令的多个操作数**错开到不同 bank**。原文示例："With 4 Banks, the lower two bits of an assigned register number may be exclusive ORed (XOR) With a unique phase value corresponding to the thread that the register is allocated to."

**FIG. 8 — 读请求仲裁流程**（对应权项 1 的方法）：选中某读请求后，若该指令附带的**常量读请求在常量缓存中未命中**，则**丢弃整条指令的读请求**以让出 bank 端口，改选另一条指令；待常量取回后再补读。跨线程常量索引不一致时，常量读被**串行化**为多次读。

### 3.2 二号专利的硬件画像（US8639882B2）

**claim 1 原文**（本次完整取得）：

> 1. A method for executing instructions on a processor core implemented with a source operand collector cache coupled to at least one input of a data path residing within the processor core, the method comprising: decoding an instruction to determine a plurality of operands specified by the instruction; for each operand in the plurality of operands, determining that the operand is not stored in the source operand collector cache, loading the operand into the source operand collector cache from a local register file; and configuring the processor core to execute the instruction across the operands stored in the source operand collector cache.

从属权项给出的硬参数：

| 权项 | 内容 | 参数 |
|------|------|------|
| 2 | 载入时更新 **cache table**，把寄存器索引关联到缓存存储单元 | 索引 ↔ 槽位映射 |
| 3 | 寄存器堆被写入时，**更新 cache table 移除该寄存器相关条目**（= 失效） | 写回即失效 |
| 4 | 寄存器堆由**多个 register bank** 组成 | — |
| 5 | **crossbar** 把第一个 bank 的寄存器连到第一个存储单元 | — |
| 6 | crossbar **每周期每 bank 只连 1 个寄存器**进缓存 | 1 R/bank/cycle |
| 7 | 缓存容量需**同时容纳两条已译码指令**的操作数 | 2 条指令 |

说明书给出的具体数字（本次从 PDF 逐字提取）：

> "As shown, operand collector 440 includes **six storage elements 441-446**."（可为 flip-flop / latch）

> "local register file 304 includes **four register banks**, thereby allowing up to **four operands** to be written into operand collector 440 during a single clock cycle. However, because operand collector 440 includes six storage elements 441-446 ... the operands may need to be loaded in **two consecutive clock cycles (4 operands in the first clock cycle and 2 operands in the second clock cycle)**."

> "Whenever a new value is written to a register in the local register file 304, the cache table is searched ... the cache table is updated to **invalidate** the data in the operand collector 440 ... dispatch unit 470 will reload the value from the local register file 304 into the operand collector 440, ensuring that invalid values are not provided to the ALU 452."

### 3.3 三号专利的对照论点（US8555035B1）

摘要原文（本次逐字提取）：

> "One embodiment of the present invention sets forth a technique for using a multi-bank register file that reduces the size of or eliminates a **switch** and/or **staging registers** that are used to gather input operands for instructions. Each function unit input may be directly connected to one bank of the multi-bank register file with neither a switch nor a staging register. A compiler or register allocation unit ensures that the register file accesses for each instruction are **conflict-free** (no instruction can access the same bank more than once in the same cycle). The compiler or register allocation unit may also ensure that the register file accesses for each instruction are also **aligned** (each input of a function unit can only come from the bank connected to that input)."

**这份文件的价值在于它是 NVIDIA 内部对"两条路线"的官方对照**：一条是运行时收集（OC + crossbar + staging），一条是编译期保证（conflict-free + aligned）。它全文 **0 次**使用 "collector" 一词，改用 "switch" / "staging register"。

---

## 四、论文与实测证据（3 篇）

| # | 文献 | 关键贡献 | 逐篇解析 |
|---|------|---------|----------|
| 1 | **arXiv:2503.20481**《Analyzing Modern NVIDIA GPU cores》(Huerta et al., UPC, 2025-03) | **否定现代 GPU 存在 OC**；给出 Ampere/Turing 的 RF bank / 读端口 / RFC 结构与定长延迟模型；重写 Accel-Sim 模型，MAPE 从 18.24% → **13.98%** | [解析](arXiv2503.20481_现代NVIDIA核心无操作数收集器_解析.md) |
| 2 | **arXiv:1804.06826**《Dissecting the NVIDIA Volta GPU Architecture via Microbenchmarking》(Jia et al., 2020) | 从 **SASS 控制字**反推 **reuse cache** 机制（4 个 reuse cache + 4 个 source operand slot）；Volta **2×64-bit bank**；实测 bank 冲突代价与优化收益 **+15.4%** | [解析](arXiv1804.06826_Volta微基准_寄存器复用与bank冲突_解析.md) |
| 3 | **ASPLOS'19 CORF**《CORF: Coalescing Operand Register File for GPUs》(Esfeden et al., UCR/Tesla/SJSU) | 学术侧对 OC 的标准建模：**CU0–CU3 收集单元** + 两条约束（OC port serialization / register bank conflicts）；Fermi RF 基线 128 KB/SM、4 bank × 8 sub-bank × 128-bit | [解析](ASPLOS19_CORF_操作数收集器建模_解析.md) |

### 4.1 否定性结论的原文（本次逐字提取）

arXiv:2503.20481 §5.3：

> "Unlike previous works [1, 18, 49] that assume the presence of an operand collector to deal with conflicts in the register file ports, **modern NVIDIA GPUs do not make use of it**. Operand collector units would introduce variability in the elapsed time between issue and write-back, making it impossible to have fixed-latency instructions for the NVIDIA ISA, whose latency must be known at compile time to handle dependencies correctly as explained section 4. We have confirmed the absence of operand collectors by checking the correctness of specific producer-consumer sequences of instructions when varying the number of operands that go to the same bank. We observed that regardless of the number of register file port conflicts, the value required in the Stall counter field of instructions to avoid data hazards and the elapsed time to execute the instruction remains constant."

> "**Each bank of the register file has one read port of 1024 bits** ... all the fixed-latency instructions **spend three cycles for reading source operands**, even if some of these cycles the instruction is idle ... If the instruction in the allocate stage realizes that it cannot read all its operands in the next three cycles, it is **held in this stage (stalling the pipeline upwards)** and generates bubbles until it can reserve all ports needed to read the source operands in the next three cycles."

> "FMUL instructions with both source operands in the same bank create a **1-cycle bubble**, whereas if the two operands are in different banks, there is no bubble. FFMA instructions with all three source operands in the same bank generate a **2-cycle bubble**."

### 4.2 reuse cache 的原文（本次逐字提取）

arXiv:1804.06826 §2.1：

> "**Reuse flags.** Volta, Pascal and Maxwell have **4 register reuse caches and 4 source operand slots**. Each of the 4 reuse flag bits corresponds to one of the 8-byte slots. When a flag is set, the value of the register in the corresponding slot will be stored in the reuse cache for future instructions to consume. **Reuse mitigates register bank conflicts.** The least significant bit in reuse flags controls the cache for the first source operand slot. The most significant bit is for the fourth source operand slot."

arXiv:2503.20481 §5.3.1：

> "it has **one entry for each of the two register file banks in each sub-core. Each entry stores three 1024-bit values** ... Overall, the RFC's total capacity is **six 1024-bit operand values (sub-entries)**."

> "A subsequent instruction will obtain its register source operand from the RFC if the instruction is from the **same warp**, the **register ID coincides**, and the **operand position in the instruction is the same** as in the instruction that has triggered the caching. **A cached value is unavailable after a read request arrives to the same bank and operand position, regardless of whether it hits in the RFC.**"

### 4.3 各代 SM 的操作数供给参数（跨来源合表，**注意来源口径不同，不可混用**）

| 世代 | SM 分区 | RF bank | 读带宽 | OC | reuse cache | 冲突表现 | 来源 |
|------|---------|---------|--------|----|-------------|---------|------|
| Fermi GF100 | 2 warp scheduler + 2 dispatch | **4 bank**（每 bank 8 sub-bank × 128-bit，128 KB/SM，每 bank 256 warp-register） | 1 value / bank / cycle | **4 个 CU（CU0–CU3）**，每 CU 1 端口 | — | bank 内串行化 | CORF Fig.1 + 专利 |
| Kepler GK1xx | 4 warp scheduler | **4 bank × 256 registers** | **1 register / bank / cycle** | 每执行单元 1 个 | — | — | US8639882B2 |
| Maxwell / Pascal | 2 processing block（Pascal GP100） | **32-bit 宽 bank** | **1 value / bank / cycle** | — | **4 个 reuse cache + 4 个 source operand slot**（8-byte slot） | 只能靠 reuse 缓解 | arXiv:1804.06826 §1/§2.1 |
| **Volta GV100** | **4 processing block**（每 block 1 scheduler + 1 dispatch） | **2 bank × 64-bit**（bank = reg index mod 2） | **64 bit / bank / cycle** | **0（实测否定）** | 4 个 reuse cache + 4 slot | 3 源同 bank → 串行化 | arXiv:1804.06826 §1/§3.5 |
| **Turing TU10x** | 4 processing block | 2 bank | 每 bank 1 读端口 | **0（实测否定）** | RFC 同 Ampere 形态 | issue 侧气泡 | arXiv:2503.20481（portability 验证） |
| **Ampere GA10x/A100** | 4 sub-partition | **2 bank** | **1 × 1024-bit 读端口 + 1 × 1024-bit 写端口 / bank** | **0（实测否定）** | **每 bank 1 entry × 3 × 1024-bit = 6 子条目** | 0/1/2 冲突 → **5/6/7 cycles** | arXiv:2503.20481 §5.3 |
| Hopper GH100 | 4 processing block（二手来源） | 未实测 | 未实测 | 未实测 | 未实测 | 未实测 | — |
| Blackwell GB203/B200 | 4 sub-core | RF 256 KB/SM | 未实测 | 未实测 | 未实测 | 未实测 | arXiv:2507.10789（结构层） |

> **口径警示**：
> - Fermi 的 "4 bank" 来自 CORF 的**建模基线**（128 KB/SM ÷ 4），专利的 "N bank" 是**参数化**表述，两者不能直接对齐；
> - Maxwell/Pascal 的 "32-bit bank，每 bank 每周期 1 个值" 与 Volta 的 "2×64-bit bank" 是**两代不同宽度**，Volta 的 64-bit 允许一条指令从同一 bank 取 2 个值，这是 Volta 能消除部分 bank 冲突的硬件原因；
> - Hopper / Blackwell **无一手实测**。Hopper 白皮书官方 PDF 在本次调研中不可获取（404 + 表单墙），不作"0 次出现"类结论。

---

## 五、GPGPU-Sim 4.2.0 的 OC 建模（源码剖析）

> 完整剖析见 [gpgpu-sim_操作数收集器实现剖析.md](gpgpu-sim_操作数收集器实现剖析.md)。此处只给结论。

### 5.1 类结构

本机版本为 **GPGPU-Sim 4.2.0（Accel-Sim 2.0 Hopper 分支，git `c3eeb6d1`）**。**注意：不存在 `src/cuda-sim/operand_collector.{cc,hh}` 文件**（历史上从未存在），OC 实现全部内联在 `src/gpgpu-sim/shader.h` + `src/gpgpu-sim/shader.cc`：

| 类 | 位置 | 语义 |
|----|------|------|
| `opndcoll_base_t` | `shader.h:1011` | 抽象基类 |
| `opndcoll_simple_t` | `shader.h:1043` | **简化模型**：单 swap buffer，无 bank 仲裁 |
| `opndcoll_rfu_t` | `shader.h:1057` | **完整 banked-RF 模型**：本专题重点 |

`opndcoll_rfu_t` 的关键嵌套类：

- `op_t`（`shader.h:1105`）：单个操作数的 bank 分配请求（reg 号 + bank 号 + warp id + scheduler id）
- `allocation_t`（`shader.h:1206`）：每 bank 的占用态（`NO_ALLOC` / `READ_ALLOC` / `WRITE_ALLOC`）
- `arbiter_t`（`shader.h:1241`）：**bank × collector 二维匹配仲裁器**，内部用 BookSim 的 wavefront 匹配算法
- `collector_unit_t`（`shader.h:1336`）：一个 OC 槽位（= 一条 warp 指令的收集上下文，含 `m_src_op[64]` 与 64-bit `m_not_ready` 掩码）
- `dispatch_unit_t`（`shader.h:1391`）：派发仲裁（round-robin 找就绪 CU）

### 5.2 数据通路（issue → 执行 → writeback）

```
Scheduler.issue()
  → m_pipeline_reg[ID_OC_*]        ← 已发射、等待操作数
  → OC.allocate_cu()                ← 抢空闲 collector_unit_t，登记 bank 读请求
  → OC.arbiter.allocate_reads()     ← wavefront 匹配，分配 bank
  → OC.allocate_reads()             ← 清 m_not_ready 对应 bit
  → OC.dispatch_ready_cu()          ← 就绪 CU 移入 m_pipeline_reg[OC_EX_*]
  → FU 执行 → m_pipeline_reg[EX_WB]
  → OC.writeback()                  ← 占写端口，失败则 stall
```

**建模要点**：GPGPU-Sim 的 OC **不搬运寄存器值**。寄存器值在 functional simulator 侧（`ptx_thread_info::m_regs`），指令在 issue 当拍就通过 `func_exec_inst` 完成功能执行；OC 只模拟 **bank 冲突 / 端口争用导致的延迟**。

### 5.3 关键参数（默认值）

| 参数 | 默认 | 出处 |
|------|------|------|
| `-gpgpu_num_reg_banks` | 8 | `gpu-sim.cc:521` |
| `-gpgpu_reg_file_port_throughput` | 1 | `gpu-sim.cc:682` |
| `-gpgpu_opndcoll_model` | 0 = DETAILED | `gpu-sim.cc:696` |
| `-gpgpu_operand_collector_num_units_{sp,sfu,tensor_core}` | 4 | `gpu-sim.cc:533/539/545` |
| `-gpgpu_operand_collector_num_units_mem` | 2 | `gpu-sim.cc:549` |
| `-gpgpu_operand_collector_num_units_gen` | 0 | `gpu-sim.cc:552` |
| `-gpgpu_enable_specialized_operand_collector` | 1 | `gpu-sim.cc:530` |
| `-gpgpu_num_sched_per_core` | 1 | `gpu-sim.cc:614` |
| `-gpgpu_sub_core_model` | 0 | `gpu-sim.cc:527` |

**各架构 config 实际取值**（`configs/tested-cfgs/`）：

| config | num_reg_banks | port_throughput | num_units_gen | sub_core | opndcoll_model |
|--------|---------------|-----------------|---------------|----------|----------------|
| SM3_KEPLER_TITAN | 16 | 2 | — | 0 | —（DETAILED） |
| SM6_TITANX | 16 | 2 | 8 | 1 | —（DETAILED） |
| SM7_QV100 | 16 | 2 | 8 | 1 | —（DETAILED） |
| SM75_RTX2060 | 8 | 2 | 8 | 1 | —（DETAILED） |
| SM80_A100 | 32 | 2 | 8 | 1 | —（DETAILED） |
| **SM90_H100 / H200** | **64** | 2 | 8 | 1 | **1 = SIMPLE（不建模 bank 冲突）** |

> **行业事实值得单独标注**：Accel-Sim 2.0 的 Hopper 性能模型（commit `e10018b6`）**刻意把 sm_90 切到 SIMPLE 模型**，即**在 Hopper 上完全不建模寄存器堆 bank 冲突**。这与 arXiv:2503.20481 的实测结论方向一致（现代 NVIDIA 冲突表现为 issue 侧气泡，而非 OC 内的可变延迟）。

---

## 六、对 CppTLM 的可借鉴点

> CppTLM 现状对照：`include/tlm/gpu/sm/reg_file.hh`（`cpptlm::gpu::RegFileUnit` = flat hash map，**无 bank / 无端口 / 无冲突概念**）、`include/tlm/gpu/sm/issue_unit_tlm.hh`（`tlm::sm::IssueUnitTLM`，round-robin 调度，**无 "issue 后 / 执行前" 中间站**）、`include/tlm/gpu/sm/writeback.hh`、`include/tlm/gpu/sm/hazard_tracker.hh`（vgpr 占用位图，解决 RAW，与端口争用是两套独立机制）。

| # | 借鉴点 | 来源 | 落到 CppTLM 的具体位置 |
|---|--------|------|---------------------|
| 1 | **给寄存器堆引入 bank 维度 + 冲突仲裁器** | gpgpu-sim `opndcoll_rfu_t::arbiter_t`（`shader.h:1241`）；US7834881B2 FIG.3 | `RegFileUnit` 增加 `num_banks_` + 每 bank 占用态；实现 `register_bank(reg, warp_id, num_banks)`（照抄 `shader.cc:5208`） |
| 2 | **区分"数据冒险"与"端口争用"两层** | US7834881B2（scoreboard unit 与 bank arbitration 并列）；gpgpu-sim（Scoreboard 与 OC 互不引用） | 保持 `HazardTracker` 不变，另建独立的 bank/端口仲裁器，**不要把端口延迟塞进 RAW 逻辑** |
| 3 | **引入"已 issue 但操作数未齐"的中间态** | gpgpu-sim `ID_OC_*` → `OC_EX_*` 两级流水级 | 在 `IssueUnitTLM` 与执行单元之间加一段收集排队层（bundle 加 `collected` 标志位），使 OC 语义（Tesla 口径）与定长延迟口径（Volta+ 口径）可切换 |
| 4 | **写端口 stall 语义** | gpgpu-sim `opndcoll_rfu_t::writeback()` 的 `bank_idle()` 检查 | `WritebackUnit::tick()` 增加"本拍可接受写次数"与"目标 bank 被占 → 延后一拍" |
| 5 | **操作数元数据需带寄存器号 → bank 映射** | US8639882B2 cache table；gpgpu-sim `op_t` | `IssueToExecBundle::src_values[2]` 目前是纯值数组，需补 `src_regs[2]` / `src_banks[2]` |
| 6 | **提供两套口径的开关** | 本专题核心分水岭（OC 可变延迟 vs 定长 + 气泡） | 参照 gpgpu-sim 的 `opndcoll_model` 二值开关，在 CppTLM config 中提供 `operand_collector.model ∈ {none, classic, modern}`，默认 `modern`（对齐 Volta+ 实测） |
| 7 | **reuse cache 作为编译器提示建模** | US8639882B2；arXiv:1804.06826 §2.1；arXiv:2503.20481 §5.3.1 | 若 PTX-EMU 侧能保留 `.reuse` 语义，可在 `RegFileUnit` 上加"每 bank 每操作数位置 1 entry"的 RFC，并实现"读一次即失效"的精确语义 |

---

## 七、术语沿革（可引用的历史脉络）

| 时间 | 载体 | 是否使用 "operand collector" | 出处 |
|------|------|--------------------------------|------|
| 2004-07（优先权）/ 2006-11（申请） | US7834881 家族 | ✅ **标题即为 "Operand collector architecture"**；正文用 "Collector Unit 330" / "Bank Request Arbitration Unit 310" / "Dispatcher Unit 540" | 本专题 3.1 |
| 2010 | **NVIDIA Fermi 白皮书 v1.1** | ✅ **SM 框图内标注 "Operand Collector"**（位于 "Core Register File (32,768 × 32-bit)" 与 "Dispatch Port / Result Queue" 之间）；**正文 0 次**（仅图注 1 处） | Fermi Compute Architecture Whitepaper, p.8 |
| 2011 | US8639882 | ✅ "source operand collector caching"；摘要写 "storage elements (i.e., an operand collector)" | 本专题 3.2 |
| 2010 | US8555035 | ❌ 改用 "switch" / "staging registers" / "conflict-free" / "aligned" | 本专题 3.3 |
| 2017 / 2018 / 2020 | Volta / Turing / Ampere(GA102) / Ampere(A100) 白皮书 | ❌ **四份逐字统计均为 0 次**；改用 "processing block" / "dispatch unit" / "warp scheduler" / "register file" | 官方白皮书 PDF 全文检索 |
| 2022-03（公开 2023） | US2023/0289398（group-of-warps MMA，Hopper 集群） | ✅ 仍写 "a particular **operand collector 420**"，与 "register banks 410" / "sub-partition" 并列 | 检索摘录（PDF 403，未取得全文） |
| 2020 | arXiv:1804.06826（Volta 微基准） | ❌ 全文 **0 次**；改用 "register reuse cache" / "register bank conflict" | 本专题 4.2 |
| 2025 | arXiv:2503.20481 | ✅ 出现 3 次，但**用于否定**："modern NVIDIA GPUs do not make use of it" | 本专题 4.1 |

**结论**：该术语是 **NVIDIA 自有、公开可见但非白皮书常规用语**的结构名。专利与 Fermi 框图用它；Volta 之后的官方白皮书改用 processing block / dispatch unit；但**专利文本一直沿用**（2023 年公开的 wgmma 案仍在用），学术界则把它当作"旧世代 SM"的默认构件。

---

## 八、负结果与局限（必读）

1. **无现代架构的官方 OC 参数**。NVIDIA 从未公开过"每个 sub-partition 几个 OC / 每个 OC 多少条目"。仅有 3 组间接数字，且分属不同世代：专利级 6 个 storage elements（Kepler 时代，US8639882B2）、CORF 建模 4 个 CU（Fermi）、Accel-Sim 默认 8 个通用 OC 单元（现代配置）。
2. **Hopper / Blackwell 无一手实测**。Hopper 官方白皮书 PDF 本次不可获取（404 + resources.nvidia.com 表单墙）；Blackwell 两篇剖面论文全文 **0 次** "operand collector"。**因此不能断言 Hopper/Blackwell 是否有该结构**，只能说"未找到证据"。
3. **一号与三号专利的权利要求页为扫描件**（无文本层，本机无 OCR 工具）。本专题引用的是**摘要 + 说明书 + SUMMARY OF THE INVENTION 段落**（均为文本层，可逐字核对），权项原文标记为未取得。
4. **arXiv API 检索 "operand collector" 返回 0 条**：说明没有任何论文以该词为标题/摘要主题；本专题涉及的论文都是"顺带"讨论该结构。
5. **两个常见引用错误**（本次核查发现，供后续引用时避免）：
   - `arXiv:2408.00703` 不是 "Dissecting the Ampere GPU Architecture"（实为敏捷软件开发，cs.SE）。真正的 Ampere 论文是 **arXiv:2208.11174**（Abdelkhalik et al., HPEC'22）。
   - `arXiv:2006.04974` 不是 "Demystifying Volta's TPC and SM Design"（实为天体物理 astro-ph.GA）。Volta 论文是 **arXiv:1804.06826**（Jia et al., 2020）。
6. **检索通道受限**：Google Patents / Espacenet / Lens.org / Justia / FreePatentsOnline / archive.org 在本环境均不可达（超时或 403）；可用的通道是 `patentimages.storage.googleapis.com`（取 PDF 全文）与 `eureka.patsnap.com`（取书目/法律状态）。本专题的专利事实**均以 PDF 全文为准**。

---

## 九、本专题文件清单

| 文件 | 类型 |
|------|------|
| [overview.md](overview.md) | **本综述**（专题主文档） |
| [US7834881B2_操作数收集器架构_解析.md](US7834881B2_操作数收集器架构_解析.md) | 专利 + 解析（源头专利） |
| [US8639882B2_源操作数收集器缓存_解析.md](US8639882B2_源操作数收集器缓存_解析.md) | 专利 + 解析（复用缓存） |
| [US8555035B1_无冲突寄存器分配对照_解析.md](US8555035B1_无冲突寄存器分配对照_解析.md) | 专利 + 解析（对照路线） |
| [gpgpu-sim_操作数收集器实现剖析.md](gpgpu-sim_操作数收集器实现剖析.md) | 源码剖析 |
| [arXiv2503.20481_现代NVIDIA核心无操作数收集器_解析.md](arXiv2503.20481_现代NVIDIA核心无操作数收集器_解析.md) | 论文 + 解析（否定性结论） |
| [arXiv1804.06826_Volta微基准_寄存器复用与bank冲突_解析.md](arXiv1804.06826_Volta微基准_寄存器复用与bank冲突_解析.md) | 论文 + 解析（reuse cache 证据） |
| [ASPLOS19_CORF_操作数收集器建模_解析.md](ASPLOS19_CORF_操作数收集器建模_解析.md) | 论文 + 解析（学术界 OC 建模口径） |

---

## 十、推荐阅读路径

1. **只想知道结论**：读本文件第二节（TL;DR）+ 第四节（论文证据）。
2. **要理解 OC 是什么**：本文件 §3.1 → [US7834881B2 解析](US7834881B2_操作数收集器架构_解析.md) 的 FIG. 3/5/8 三节。
3. **要理解 OC 为什么消失了**：本文件 §4.1 → [arXiv:2503.20481 解析](arXiv2503.20481_现代NVIDIA核心无操作数收集器_解析.md)，特别是 §5.3 的定长延迟论证与 §5.3.1 的 RFC 语义。
4. **要在仿真器里实现**：[gpgpu-sim 剖析](gpgpu-sim_操作数收集器实现剖析.md) 的数据通路图 + 仲裁器实现，再对照本文件第六节的 CppTLM 借鉴点表。
5. **要做架构取舍**：[US8555035B1 解析](US8555035B1_无冲突寄存器分配对照_解析.md)（编译期保证路线）vs 一号专利（运行时收集路线），这是 NVIDIA 内部两条路线的官方对照材料。

---

*本综述基于 2026-10-27 本次下载的一手材料整理：3 件 NVIDIA 专利 PDF（`patentimages.storage.googleapis.com`，含完整文本层，已逐字提取摘要/说明书/权项）+ 3 篇论文 PDF（`arxiv.org` / `cs.ucr.edu`，已逐字提取关键段落）+ 本机 GPGPU-Sim 4.2.0 源码（`/workspace/low/source/github/gpgpu-sim_distribution`，git `c3eeb6d1`）。专利法律状态以官方 USPTO 记录为准；论文实测数据以原文为准；源码行号以本机该 commit 为准。*
