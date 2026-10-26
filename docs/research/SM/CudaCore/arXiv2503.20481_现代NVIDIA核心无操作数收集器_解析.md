# arXiv 2503.20481《Analyzing Modern NVIDIA GPU cores》— 现代 NVIDIA 核心**没有**操作数收集器 解析

> **arXiv 号与版本**: arXiv:2503.20481v1 [cs.AR]（2025-03-26）
> **标题**: Analyzing Modern NVIDIA GPU cores
> **作者**: Rodrigo Huerta、Mojtaba Abaie Shoushtary、José-Lorenzo Cruz、Antonio González（Universitat Politècnica de Catalunya, Barcelona）
> **PDF**: https://arxiv.org/pdf/2503.20481 （2026-10-27 下载，15 页）
> **文本层**: 完整（"operand collector" 全文 **3 次**命中，均已逐字核对）
> **在线版**: https://arxiv.org/abs/2503.20481
> **分析日期**: 2026-10-27

---

## 1. 为什么这篇论文是本专题的"分水岭"

本专题前 5 篇材料（3 件专利 + CORF + Volta 微基准）都在**默认 operand collector 存在**。这篇论文是**唯一一篇用实测证明现代 NVIDIA GPU 没有 OC**的文献，并且给出了**为什么没有**的理论论证。

它的论证链（作者自述）：

```
① NVIDIA ISA 的指令延迟必须在编译期已知（否则编译器无法正确排依赖）
        ↓
② OC 会让 issue→writeback 的时间随 bank 冲突情况变化（可变延迟）
        ↓
③ 可变延迟 ⇒ 延迟不可预知 ⇒ 与 ① 矛盾
        ↓
④ 故现代 NVIDIA GPU 必须放弃 OC，改用「定长延迟 + 冲突表现为 issue 侧气泡」
```

它同时给出了一个**可运行的替代模型**，并证明该模型在 RTX A6000 上把执行周期 MAPE 从 Accel-Sim 的 18.24% 降到 **13.98%**（**相对改善 18.24%**）。

---

## 2. 关键论断（逐字引用）

### 2.1 "现代 NVIDIA GPU 不使用 OC"（§5.3）

> "Unlike previous works [1, 18, 49] that assume the presence of an operand collector to deal with conflicts in the register file ports, **modern NVIDIA GPUs do not make use of it**. Operand collector units would introduce variability in the elapsed time between issue and write-back, making it impossible to have fixed-latency instructions for the NVIDIA ISA, whose latency must be known at compile time to handle dependencies correctly as explained section 4. We have confirmed the absence of operand collectors by checking the correctness of specific producer-consumer sequences of instructions when varying the number of operands that go to the same bank. We observed that **regardless of the number of register file port conflicts, the value required in the Stall counter field of instructions to avoid data hazards and the elapsed time to execute the instruction remains constant**."

**实验方法**：改变同一 bank 内的操作数数量，验证 producer-consumer 序列的正确性——**无论冲突多少，Stall counter 需要的值与指令执行耗时都不变**。这是"没有可变收集延迟"的直接实验证据。

### 2.2 定长延迟 + 端口预约窗口（§5.3）

> "Regarding reads, we have observed a **bandwidth of 1024 bits per bank**. ... **FMUL instructions with both source operands in the same bank create a 1-cycle bubble**, whereas if the two operands are in different banks, there is no bubble. **FFMA instructions with all three source operands in the same bank generate a 2-cycle bubble.**"

> "Our experiments showed that **all the fixed-latency instructions spend three cycles for reading source operands**, even if some of these cycles the instruction is idle (for instance when there are only two source operands) because FADD and FMUL have the same latency as the FFMA despite having one operand less, and **FFMA always has the same latency regardless of whether its three operands are in the same bank or not**. If the instruction in the allocate stage realizes that it cannot read all its operands in the next three cycles, it is **held in this stage (stalling the pipeline upwards) and generates bubbles** until it can reserve all ports needed to read the source operands in the next three cycles."

**这段是整篇论文最关键的建模描述**，必须完整理解：

| 概念 | 含义 |
|------|------|
| **3 周期** | 所有定长延迟指令读源操作数的**固定**窗口（FFMA 3 源、FADD/FMUL 2 源，都是 3 周期） |
| **Allocate stage** | 位于 issue 与 RF read 之间，负责**预约未来 3 个周期内的读端口** |
| **预约失败** | 指令**滞留在 Allocate**，**向上游（issue 方向）压气泡** |
| **冲突表现** | **不是**指令延迟变长，而是 **issue 侧多出气泡**（1 周期 / 2 周期） |

### 2.3 写端口与结果队列（§5.3）

> "Our experiments revealed that **each register file bank has a dedicated write port of 1024 bits**. Besides, when a load instruction and a fixed-latency instruction finish at the same cycle, the one that is delayed one cycle is the load instruction. On the other hand, when there is a conflict between two fixed latency instructions, for instance, a IADD3 followed by an IMAD that uses the same destination bank, **none of them is delayed**. This implies the use of a **result queue** like the one introduced in Fermi [61] for fixed-latency instructions. The consumers of these instructions are not delayed, which implies the use of **bypassing** to forward the results to the consumer before being written in the register file."

**三个硬件机制的实测证据**：

1. **每 bank 独立 1024-bit 写端口**（1024 bit = 32 线程 × 32 bit = 一个 warp 寄存器）；
2. **结果队列（result queue）**——两条定长延迟指令写冲突时都不延后，说明结果进了队列排队；
3. **旁路（bypassing）**——消费者不被延后，说明结果在写回 RF 之前就转发给了消费者。

---

## 3. 寄存器堆结构（§5.3）

原文：

> "**Regular**: Recent NVIDIA architectures have **65536 32-bit registers per SM**, used to store the values operated by by threads. The registers are arranged in **groups of 32**, each group corresponding to the registers of the 32 threads in a warp, resulting in **2048 warp registers**. These registers are **evenly distributed between sub-cores**, and the registers in each sub-core are **organized in two banks**. The number of registers used by a particular warp can vary from **1 to 256**, and it is decided at compile time. The more registers used per warp, the fewer warps can run parallel in the SM."

其它寄存器类别（原文列举）：

| 类别 | 规格 |
|------|------|
| Uniform | 每 warp **64 个 32-bit** 私有寄存器（warp 内所有线程共享值） |
| Predicate | 每 warp **8 个 32-bit**，每 bit 对应一个线程 |
| Uniform Predicate | 每 warp **8 个 1-bit** |
| SB Registers（Dependence counters） | 每 warp **6 个**，用于追踪可变延迟依赖 |
| B Registers | 每 warp **至少 16 个**，用于控制流重汇聚 |
| Special Registers | thread/block ID 等 |

**结构图（论文 Figure 3）**：

```
SM/Core
├── Sub-core 0..3
│   ├── Instruction Buffer → Arbiter (CGGTY) → Issue
│   ├── Control → Allocate
│   ├── Fixed-latency RF reads
│   ├── INT32 / FP32 / Tensor cores / SFU / Memory local unit
│   ├── Result queue → RF write (priority arbiter)
│   └── Bank 0 / Bank 1 + RFC        ← ★ 每 sub-core 2 bank + RFC
├── Register file: Regular / Uniform
├── L0 Icache / Ccache / Stream Buffer
├── L1 data cache / L0 VL constant cache / Texture cache
└── Shared memory / Shared memory unit
```

---

## 4. Register File Cache（RFC）§5.3.1 ★操作数复用缓存的现代形态

### 4.1 结构

> "Regarding the organization of the RFC, our experiments showed that it has **one entry for each of the two register file banks in each sub-core. Each entry stores three 1024-bit values**, each corresponding to one of the three regular register source operands that instructions may have. Overall, the RFC's total capacity is **six 1024-bit operand values (sub-entries)**. Note that there are instructions that have some operands that require two consecutive registers (e.g., tensor core instructions). In this case, each of these two registers come from a different bank, and are cached in their corresponding entries."

**结构**：

```
每个 sub-core:
  Bank 0 ──► RFC entry 0 ──┬─► sub-entry 0 (1024 bit) ← 操作数位置 0
                           ├─► sub-entry 1 (1024 bit) ← 操作数位置 1
                           └─► sub-entry 2 (1024 bit) ← 操作数位置 2
  Bank 1 ──► RFC entry 1 ──┴─► (同上)
  合计 = 2 bank × 3 操作数位置 = 6 个 1024-bit 子条目
```

### 4.2 命中条件与失效语义 ★

> "The compiler manages the allocation policy. When an instruction is issued and reads its operands, **each operand is stored in the RFC if the compiler has set its reuse bit for that operand**. A subsequent instruction will obtain its register source operand from the RFC if:
> - the instruction is from the **same warp**,
> - the **register ID coincides** with the one stored in the RFC, and
> - the **operand position in the instruction is the same** as in the instruction that has triggered the caching.
>
> **A cached value is unavailable after a read request arrives to the same bank and operand position, regardless of whether it hits in the RFC.**"

**这是与专利口径最重要的差异**：

| | 专利（[US8639882B2](US8639882B2_源操作数收集器缓存_解析.md)） | 实测（现代 Ampere/Turing） |
|---|---|---|
| 谁决定缓存 | 硬件（cache table 自动维护） | **编译器**（`.reuse` 位显式置位） |
| 失效触发 | **寄存器堆被写入** | **有读请求到达同一 bank 的同一操作数位置**（无论命中与否） |
| 命中条件 | 同槽位 + 同寄存器号 | 同 warp + 同寄存器号 + **同操作数位置** |
| 容量 | 6 个 storage elements | 6 个 1024-bit 子条目 |

**"读一次即失效"是一个反直觉但极重要的语义**：它意味着 RFC 天然是"**单次消费**"的，必须靠编译器在**每一条**想复用的指令上重复置 `.reuse` 位。

### 4.3 Listing 2：四个行为示例（原文照录）

```
# Example 1
IADD3 R1, R2.reuse , R3, R4 # Allocates R2
FFMA R5, R2, R7, R8 # R2 hits and becomes unavailable
IADD3 R10 , R2, R12 , R13 # R2 misses

# Example 2
IADD3 R1, R2.reuse , R3, R4 # Allocates R2
FFMA R5, R2.reuse , R7, R8 # R2 hits and is retained
IADD3 R10 , R2, R12 , R13 # R2 hits

# Example 3. R2 misses in the second instruction since it
# is cached in another slot. R2 remains available in the
# first slot since R7 uses a different bank
IADD3 R1, R2.reuse , R3, R4 # Allocates R2
FFMA R5, R7, R2, R8 # R2 misses
IADD3 R10 , R2, R12 , R13 # R2 hits

# Example 4. R2 misses in the third instruction since the
# second instruction uses a different register that goes to
# the same bank in the same operand slot
IADD3 R1, R2.reuse , R3, R4 # Allocates R2
FFMA R5, R4, R7, R8 # R4 misses and R2 becomes unavailable
IADD3 R10 , R2, R12 , R13 # R2 misses
```

> **Example 1 vs 2 的差别只有一处**：第二条指令是否再次置 `.reuse`。这精确印证了"读一次即失效"的语义——要让第三条指令还能命中，**第二条必须重新置位**。

---

## 5. Issue 阶段模型（§5.1）★ 与 OC 的替代关系

### 5.1 Warp 就绪条件（§5.1.1）

> "A warp is considered a candidate for issuing its oldest instruction in a given cycle if some conditions are met. ... Besides, for fixed-latency instructions, **a warp is candidate to issue its oldest instruction in a given cycle only if it can be guaranteed that all needed resources for its execution will be available once issued**."

**关键点**：**issue 阶段只检查"资源保证可用"，不检查 RF 读端口**。

> "As for the availability of read ports in the register file, **the issue scheduler is unaware of whether the evaluated instructions have enough ports for reading without stalls in the following cycles**. We have reached this conclusion after observing that the conflicts in the code shown in Listing 1 do not stall the issue of the second CLOCK if we remove the NOP between the last FFMA and the last CLOCK instruction."

### 5.2 Control / Allocate 两级（§5.1.1）★OC 的现代替代品

> "fixed-latency instructions have **two intermediate stages between the Issue stage and the stage(s) for reading source operands**:
> - The first stage, which we call **Control**, is common for fixed and variable latency instructions, and its duty is to **increase the Dependence counters** or read the value of the clock counter if needed. ... an instruction that increases the Dependence counter and the instruction that waits until that Dependence counter is 0 requires **at least one cycle in between** ...
> - The second stage only exists for fixed-latency instructions. In this stage, **the availability of register file read ports is checked, and the instruction is stalled in this stage until it is guaranteed that it can proceed without any register file port conflict**. We call this stage **Allocate**."

> "Variable-latency instructions (e.g., memory instructions) are **delivered directly to a queue** after going through the Control stage (**without going through the Allocate stage**). Instructions in this queue are allowed to proceed to the register file read pipeline when they are guaranteed not to have any conflict. **Fixed-latency instructions are given priority over variable-latency instructions** to allocate [ports]."

**这就是 OC 的功能替代品**：

| OC（Tesla 口径） | 现代替代（本文口径） |
|-----------------|-------------------|
| collector unit 汇集操作数 | **Allocate stage 预约端口** |
| 收集延迟可变（1~3 拍） | **固定 3 拍** |
| 冲突 → 指令在 OC 内多等 | 冲突 → **指令滞留 Allocate，向上游压气泡** |
| issue 不感知端口 | **issue 不感知端口**，感知在 Allocate |

### 5.3 常量 cache miss 的处理（§5.1.1）★ 与专利 FIG.8 的对照

> "For instructions that have a source operand in the Constant Cache, the tag look-up is performed in the issue stage. When the oldest instruction of the selected warp requires an operand from the constant cache and the operand is not in the cache, **the scheduler does not issue any instruction until the miss is serviced**. However, **if the miss has not been served after four cycles, then the scheduler switches to a different warp** (the youngest with a ready instruction)."

> **对照 [US7834881B2](US7834881B2_操作数收集器架构_解析.md) FIG. 8**：专利的做法是"常量 cache miss → **丢弃整条指令的读请求**、让出 bank 端口、改选另一条"。现代硬件的做法是"**不 issue**，等 miss；超过 4 周期就换 warp**"。
>
> **两者目的相同（不让一条指令卡住整个 bank / 调度器），但机制不同**：专利在 bank 仲裁层丢弃，现代在 issue 层跳过。

### 5.4 调度策略：CGGTY（§5.1.2）

论文测得的现代 NVIDIA 调度策略为 **CGGTY（Compiler Guided Greedy Then Youngest）**——编译器引导 + 贪心 + 最年轻 warp 兜底。

---

## 6. 定量结果：RFC 与读端口数的敏感性（§7.4，Table 6）

**基线 = 1 读端口 + RFC 开启**；实测平台 **NVIDIA RTX A6000（Ampere）**：

| 配置 | MAPE | Speed-up | MaxFlops APE | MaxFlops speed-up | Cutlass APE | Cutlass speed-up |
|------|------|----------|--------------|-------------------|-------------|-------------------|
| **1R + RFC on**（基线） | **13.98%** | 1× | 2.82% | 1× | 9.72% | 1× |
| 1R + RFC **off** | 16.05% | 0.984× | 2.82% | 1× | **39.35%** | **0.79×** |
| **2R**（每 bank 2 读口）+ RFC off | 13.38% | 1.012× | **28.97%** | **1.44×** | 0.97% | **1.11×** |
| Ideal（无限端口） | 13.57% | 1.013× | 28.97% | 1.44× | 2.30% | 1.12× |

**关键解读**（原文）：

> "For MaxFlops, performance is identical regardless of whether the RFC is present, **since only one static instruction makes use of it**. Notably, performance improves dramatically by approximately **44% when two read ports per register file bank** are employed. This improvement is logical given that three operands per instruction are common and **four read ports (two per bank) are sufficient to meet demand**."

> "In the case of Cutlass with sgemm, a single-port configuration without a register file cache leads to a substantial performance degradation (**0.78x**). This performance drop is consistent with the observation that **35.9% of the static instructions** in the program make use of the register file cache in at least one of their operands. However, introducing a register file with two read ports per bank yields a **12% performance improvement**."

> **对建模的三条硬结论**：
> 1. **单端口 + RFC** 在平均表现上接近"无限端口"（1.013×），但个别 benchmark 差距显著；
> 2. **RFC 不是可有可无**：CUTLASS sgemm 关掉 RFC 直接掉到 0.79×；
> 3. **35.9% 的静态指令**至少有一个操作数走 RFC——**这是 RFC 使用率，不是命中率**（命中率无公开数据）。

---

## 7. 依赖管理：软件 vs 硬件（§7.5，Table 7）

| 机制 | Control bits | Speed-up | Area overhead | MAPE |
|------|--------------|----------|---------------|------|
| Scoreboard（无限制消费者数） | 63 | 0.97× | **5.32%** | 14.87% |
| Scoreboard（不同最大消费者数） | — | 1 | **0.09%** | 13.98% |
| **软件依赖处理（本文方案）** | Unlimited | 1 | — | **13.98%** |

原文（面积估算）：

> "A mechanism based on the traditional scoreboard requires as many entries as registers that can be written, that is, **332 entries per warp** (255 for the regular registers, 63 for the uniform registers, 7 for the predicate registers, and 7 for the uniform predicate registers). Additionally, **two scoreboards** are needed: one for WAW/RAW hazards and another for **WAR** hazards ... Assuming support for up to 63 consumers per entry, a single warp would require **2324 bits** ... For the entire SM, this translates to **111,552 bits, which is 5.32%** [of the 256 KB RF area]."

**对本专题的意义**：现代 NVIDIA 用**编译器在控制位里编码依赖**（Dependence counter + Stall counter + Yield），**替代了硬件 scoreboard**。这与"取消 OC"是同一次架构转向的两面：**把不确定性从硬件挪到编译器**。

---

## 8. 与其它文献的交叉印证

| 来源 | 印证 / 张力 |
|------|-----------|
| **Fermi 白皮书 p.8** | OC 曾是官方 SM 框图的一部分（"Operand Collector" 唯一 1 处标注） |
| [US7834881B2](US7834881B2_操作数收集器架构_解析.md) | OC 的源头专利；其"可变延迟"正是本文论证要避免的 |
| [US8639882B2](US8639882B2_源操作数收集器缓存_解析.md) | 复用缓存专利；现代 RFC 是其"重写版"（编译器管理 + 读一次即失效） |
| [US8555035B1](US8555035B1_无冲突寄存器分配对照_解析.md) | "编译期保证"路线；现代 NVIDIA 保留了这路线的**精神**（`.reuse` 位） |
| **arXiv:1804.06826**（Volta） | 独立实测的 Volta **2×64-bit bank** + **4 个 reuse cache + 4 个 source slot**——与本文的 Ampere "2 bank + 6 子条目"结构同源，跨代一致 |
| **GPGPU-Sim 4.2.0** | sm_90 配置已切到 SIMPLE 模型（不建模 bank 冲突）——**与本文结论方向一致** |
| **CORF (ASPLOS'19)** | 学术侧仍以 OC 为基线；本文正是把这类工作（ref [49] = Accel-Sim）作为"过时假设"的批评对象 |

---

## 9. 对 CppTLM 的可借鉴点

1. **首选口径应为"现代口径"**（Allocate 预约 + 定长 3 周期 + issue 气泡），而非 OC 可变延迟。原因：与真实硬件吻合、且与 CppTLM 现有的"定长延迟"语义一致。
2. **可直接落地的参数表**：

   | 参数 | 值 | 来源 |
   |------|-----|------|
   | 每 SM regular 寄存器 | 65536 × 32-bit = **2048 warp-register** | §5.3 |
   | 每 sub-core bank 数 | **2** | §5.3 |
   | 每 bank 读端口 | **1 × 1024-bit** | §5.3 |
   | 每 bank 写端口 | **1 × 1024-bit** | §5.3 |
   | 定长指令读操作数窗口 | **3 周期** | §5.3 |
   | FMUL 同 bank 冲突 | **1 周期气泡** | §5.3 |
   | FFMA 三源同 bank | **2 周期气泡** | §5.3 |
   | RFC 结构 | 每 bank 1 entry × 3 操作数位置 = **6 子条目** | §5.3.1 |
   | RFC 命中条件 | 同 warp + 同寄存器号 + 同操作数位置 | §5.3.1 |
   | RFC 失效 | 有读请求到达同 bank 同位置即失效 | §5.3.1 |
   | 常量 cache miss | 不 issue；**4 周期**后换 warp | §5.1.1 |

3. **RFC 语义可直接实现**：`RegFileUnit` 上加"每 bank × 每操作数位置 1 个 1024-bit 槽"的表 + 读请求到达即清槽。写入点唯一（`RegFileUnit::read()`），实现成本极低。
4. **调度策略可对标 CGGTY**（编译器引导 + 贪心 + 最年轻兜底），比纯 round-robin 更贴近现代 NVIDIA。
5. **依赖管理分层**：CppTLM 现有 `HazardTracker`（硬件位图）对应 scoreboard；现代 NVIDIA 用编译器控制位替代。若未来对接 PTX-EMU 的控制位信息，可考虑对齐"软件依赖管理"口径。
6. **本篇是本专题中唯一给出"端到端误差指标（MAPE）"的来源**，适合作为 CppTLM SM 模型精度对标的参照系。

---

*本解析基于 2026-10-27 下载的一手 PDF（`arxiv.org/pdf/2503.20481`，v1，15 页）文本层提取。所有引号内英文均为逐字引用；表格数值按原文 Table 6 / Table 7 录入。实测数据以原文为准（平台为 RTX A6000 / Ampere，作者声明模型同时适用于 Turing）。*
