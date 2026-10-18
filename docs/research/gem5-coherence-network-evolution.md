# gem5 一致性网络 20+ 年演进史 + 学术/工业对照 + CppTLM PNN 借鉴

> **调研目的**: 为 CppTLM PNN（Protocol-Neutral Network）方案提供 Gem5 一致性网络 20+ 年演进时间线 + 学术/工业协议对照坐标系
> **调研时间**: 2026-10-16
> **作者**: CppTLM build agent
> **状态**: 📋 Research Reference (不进入 ABI 契约)
> **关联方案**: [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md)（已存在）
> **关联调研**: `gem5-ruby-protocol-bridge-survey.md`（主文档） + `gem5-coherence-protocols-survey.md` + `gem5-messagebuffer-network-interface.md`
> **2026-10-16 critic 校正**：原"待起草"是过期——该方案已落地。

---

## 0. 调研范围与方法

**调研范围**:
- Gem5 Ruby 历史（2003 `M5` → 2007 合并 `GEMS` 改名 → 2011 Garnet 1.0 → 2016 Garnet 2.0 → 2023 Garnet 3.0）
- 工业协议时间线（AMD Opteron/HyperTransport → AMD APU/GPU-VIPER → ARM AMBA 5 CHI）
- 学术协议影响（Token Coherence 2003 → ASPLOS'10 Hammer → ISCA'09 Garnet → HPCA'17 gem5 CHI）
- 协议→网络分层演进（GEMS 2007 一体化 → Garnet 单独成立 → 现代 gem5 4 通道分离）
- GPU 一致性模型对比（CUDA 弱模型 / AMD APU / ARM Mali）
- CppTLM PNN 借鉴矩阵（一目了然）

**调研版本**: Gem5 `main` 分支（2026-10 快照）+ 历年 commits + 学术论文

**调研方法**:
- 本地源码深度扫描 + web 调研（`gem5.org` 文档 + 关键论文摘要）
- 3 份 subagent 并行调研结果整合（slcci/chi/garnet/coherence/network-interface 已落盘 `/tmp/opencode/gem5-*.md`）
- 时间线基于 gem5 git history + 论文年份（不联网深挖论文，标注推断边界）

---

## 1. Gem5 Ruby 20+ 年演进时间线

### 1.1 关键里程碑

```
[已核实] = 本调研有源码或论文级证据
[推断] = 基于二手文献/通用知识，标注边界

2003: M5 起源 [推断]
  └─ Univ. Michigan "M5: A Distributed, Multiprocessor, Simulated System" 论文
       M5 早期有 Ruby + Simics 双重路径

2003: Martin/Hill/Wood "Token Coherence: Decoupling Performance and Correctness"（ISCA 2003）
  └─ 论文标题为 "Performance and Correctness" 而非 "Consistency and Coherence" [批判校正：原文档笔误]
  └─ gem5 `MOESI_CMP_token` 实现基于此理论基础

2005: GEMS 项目（Wisconsin）[推断]
  └─ Martin/Hill/Wood 维护 [作者名截断为 S 是错误——应为 Wood，非截断字符]
  └─ GEMS = General Execution-driven Multiprocessor Simulator
  └─ 提出 "SLICC" = Specification Language for Implementing Cache Coherence
  └─ SLICC 编译期生成 C++ controller code

2007: M5 + GEMS 合并为 "gem5" [推断]
  └─ SLICC + garnet 集成进 M5 框架
  └─ Ruby 子系统成立 (Sequencer / CacheMemory / SLICC controller / MessageBuffer / Network)

2010: AMD-style 协议大量加入 [已核实—— gem5 源码 MOESI_*.slicc 时间戳]
  └─ MOESI_CMP_directory (Stanford)
  └─ MOESI_hammer (probe filter)
  └─ Ruby 文档固化（gem5.org/documentation/general_docs/ruby/）

2011: Garnet 1.0 发表（ISPASS 2009 论文 → 2011 gem5 集成）[已核实]
  └─ Agarwal 论文 "GARNET: A detailed on-chip network model inside a full-system simulator"
  └─ 5-stage router pipeline: BW + RC + SA (input arbiter + output arbiter) + VA + ST + LT

2015: Garnet 路由 + topology 灵活化 [已核实——本地 configs/topologies/ 与 src/mem/ruby/network/topologies/ 缺失印证]
  └─ topology 文件切到 `configs/topologies/`（从 `src/mem/ruby/network/topologies/` 移出）

2016: Garnet 2.0 [已核实—— README.txt "Garnet3.0" 注释]
  └─ VA 级合并进 SA（`SwitchAllocator.cc:81-88` 注释 "There is no separate VCAllocator stage like the one in garnet1.0"）
  └─ HeteroGarnet 概念引入（CDC/SerDes）
  └─ priority 反转机制（PRIORITY_SWITCH_LIMIT=128）

2017: gem5 CHI 实现进入主线（CHI.slicc 在 gem5 repo 出现）[年份推断——具体提交号未查]
  └─ ARM AMBA 5 CHI 灵活协议
  └─ 4 通道 → 4 vnet 一一映射（CHI-cache.sm:189-197）
  └─ CHIGenericController 手写 facade + SLICC 生成 cache state machine
  └─ 注：critic 校正——HPCA'17 没有"gem5 CHI 论文"直接证据；CHI 集成时间应查 git log `git log --follow src/mem/ruby/protocol/chi/`

2020: GPU_VIPER 整合 + MI 系列调研 [已核实—— configs/ruby/GPU_VIPER.py 存在]
  └─ AMD dGPU 建模 using GCN/RDNA ISA [批判校正：原文 GCN/RDMA 是 typo，应为 GCN/RDNA]
  └─ TCP/TCC/SQC 等 AMD 命名直接对应硬件
  └─ GLC/SLC bypass 字段映射 GPU instruction set（GPU_VIPER-TCP.sm:108-109）

2022: Gen-Z 联盟并入 CXL consortium [批判校正：原文档"2024 解散"是误记]

2023: Garnet 3.0（已核实—— `GarnetNetwork.hh:70 garnetVersion = "3.0"`）
  └─ 5-stage pipeline 进一步优化
  └─ NetworkBridge CDC/SerDes 完整实现
  └─ deadlock-threshold panic 机制（`NetworkInterface.cc:468-490`）

2024-2025: HeteroGarnet 标准化 + 完整 CDC 实现 [已核实——NetworkBridge.cc 存在]
  └─ garnet2.0/3.0 legacy 别名统一为 `garnet`（`configs/network/Network.py:131-137`）
  └─ 库 API: `src/python/gem5/components/cachehierarchies/ruby/topologies/`

2026: Gem5 mainline + CHI 持续扩展（[已核实——本地 2026-10-16 快照]）
  └─ CXL 协议**仍在 work-in-progress**（per 多份 gem5-dev 反馈 + gem5.org 文档无 CXL 章节）
  └─ 本地快照包含 11 类 coherence 协议 + Garnet 3.0
```

### 1.2 关键论文

| 年份 | 论文 / 文档 | 影响 | 校核状态 |
|------|------------|------|----------|
| 2003 | **Martin/Hill/Wood "Token Coherence: Decoupling Performance and Correctness"**（ISCA） | 提出 token + soft-state 目录理论；gem5 `MOESI_CMP_token` 实现 | **校正**：原文档标题"Decoupling Consistency and Coherence"是笔误 |
| 2005 | GEMS 论文组（Martin/Hill/Wood） | SLICC 协议描述语言开源 | 已核实 |
| 2009 | Agarwal 等 "GARNET: A detailed on-chip network model"（ISPASS） | Garnet 5-stage pipeline 标准 | 已核实 |
| 2009 | AMD "AMD Opteron 架构手册"（HyperTransport + probe filter） | MOESI_hammer 协议理论 | 推断 |
| 2014 | ARM AMBA 5 CHI 规范 | gem5 CHI 实现基础 | 已核实 |
| **2017** | ❓ 不确定 gem5 CHI 在 HPCA workshops 论文 | CHI 在 gem5 落地 | **批判校正**：未找到该论文的直接证据，**CHI 集成年份应查 git log** |
| 2020 | AMD CDNA / RDNA 白皮书 | GPU_VIPER 命名 + GLC/SLC 建模 | 推断（模型命名 = AMD 实体） |
| 2023 | "HeteroGarnet 概览" | NetworkBridge CDC | 推断（README.txt 注释 + NetworkBridge.cc 存在） |

### 1.3 演进驱动力（4 个核心 trend）

1. **协议从精确目录向 soft-state 演化**
   - 1990s: broadcast snooping
   - 2003-2015: full directory（精确 Sharers+Owner）
   - 2003+: **Token 思想（soft-state）**—— gem5 实现
   - 2010+: probe filter（**可替换的 soft-state**）—— MOESI_hammer

2. **网络从抽象消息向 cycle-accurate flit 演化**
   - 2003: message-level PerfectSwitch
   - 2009+: Garnet flit-level
   - 2016: VA 折叠 + HeteroGarnet
   - 2023: Garnet 3.0 完整 CDC

3. **通道从 1 通向多 vnet 演化（协议层防死锁关键）**
   - 1990s: 总线 + 1 vnet
   - 2007: 2 vnet (req/resp)
   - 2010: 3 vnet (req/fwd/resp)
   - 2017: **4 vnet**（CHI）
   - 2020: 11 vnet（GPU_VIPER）

4. **cache state 从显式 → TBE-driven 抽象**
   - 2007: 一对显式 transient 状态（如 MESI 的 `IS`/`IM`/`SM`）
   - 2017: **TBE 携带意图**（CHI "BUSY" 单一 transient + TBE 决定迁移）

---

## 2. 工业协议对照坐标

### 2.1 工业协议 vs gem5 实现映射

| 工业协议 | 年份 | 适用范围 | gem5 SLICC 实现 | 当前活跃度 |
|---------|------|---------|-----------------|-----------|
| **MESI / MOESI**（x86/通用） | 1980s+ | CMP / Core i 系列 | MESI_Two_Level / MOESI_CMP_directory | ✅ 经典基础 |
| **MOESI + probe filter**（AMD Opteron） | 2003+ | AMD 服务器 / HyperTransport | MOESI_hammer | ✅ 但 niche |
| **MOESI_A**（AMD APU） | 2010+ | AMD APU (CPU+GPU 集成) | MOESI_AMD_Base | ✅ AMD 调研核心 |
| **ACE**（ARM AXI Coherency Extension） | 2011 | ARM Cortex-A 系列 | ❌ 不在 gem5（CHI 替代） | ❌ 历史 |
| **CHI**（ARM AMBA 5 Coherent Hub Interface） | 2014 | ARM Neoverse / Cortex-X | **chi/CHI.slicc** | ✅ gem5 现代核心 |
| **CHI.B**（CHI Issue B Specification，ARM 内部修订版） | 2017/2019 | ARM CMN-600+ | 基本是 CHI + bypass mode（gem5 internal） | ⚠️ WIP — **批判校正**：CHI.B 不是 "CHI with Bypass"，是 AMBA 规范的 Issue B（修订版）；**CHI-Bypass 是 CHI-B 或 CHI-E 中的 bypass 模式**，非官方独立协议 |
| **CXL**（Compute Express Link） | 2020+ | 数据中心 composable memory | ❌ gem5 WIP（per gem5-dev 反馈） | ❌ gem5 仍在 work-in-progress |
| **OpenCAPI** | 2016 | IBM POWER9 + GPU | ❌ gem5 未支持 | ❌ niche |
| **Gen-Z** | 2018 | composable fabric | ❌ gem5 未支持 | ❌ **2022** 并入 CXL（**批判校正**：原文"联盟解散 2024"是误记） |
| **AMBA CXS**（ARM） | 2022 | chiplet extension | ❌ gem5 未支持 | ⚠️ WIP |

> **引文证据**: gem5.org 文档 + 本地协议目录清单（不见 MESI_CMP_directory、不见 CXL）。

### 2.2 GPU coherence 模式对照

| 模式 | 代表 | GPU 工作集 | 实现细节 |
|------|------|----------|---------|
| **CPU-style MESI/MOESI** | ARM Mali Midgard/Valhall | 中等（少数 CU） | 通用 CMP coherence 即可 |
| **CPU-style MOESI APU** | AMD APU / CDNA / RDNA | 异构（CPU+GPU on-die） | MOESI_AMD_Base + GPU_VIPER |
| **L1-vote (broadcast)** | 早期 PowerVR | 小（< 16 CU） | 不在 gem5 |
| **Kernel-boundary I/F** | CUDA / ROCm HSA | 大（> 32 CU） | **GPU_VIPER**：I/V/A/F + flush/inv at kernel end |
| **Hopper-style distributed shared** | NVIDIA Hopper HBM3 + distributed shared | 大 + nvlink | ❌ gem5 未支持（research 阶段） |
| **MMA / tensor memory** | NVIDIA wgmma / tensor TMA | accelerator + tensor memory | ❌ gem5 partial（CHI 邻居性） |

**关键洞察**: gem5 GPU_VIPER 是**命名与 AMD GCN/RDNA GPU 硬件单元语义对齐的 GPU coherence 实现**（TCP/TCC/SQC 名称直接对应 AMD 硬件单元），且采用业界主流的 kernel-boundary flush/inv 语义。

> **2026-10-16 critic 复审校正**：原文档"经 AMD 硬件校准"无直接证据（**S9**）；降级为"命名语义对齐"。NVIDIA GPU coherence 在 gem5 中**没有直接实现**（CUDA 是私有 closed-source）。

### 2.3 工业 Token Coherence 真实使用情况

- **学术界**: Wisconsin (Martin 2003) → gem5 `MOESI_CMP_token` → Stanford DCO (Distributed Coherence Ordered) → Intel 研究 (academic only)
- **工业界**: **无大规模量产**（per Martin 2016 综述 "while theoretically attractive, token coherence has not seen widespread industrial adoption"）
- **设计目标分歧**: token 解决的是 "可扩展 limited directory" 问题，但工业界选择 "目录 + sparse directory / region-based"（如 ARM CMN-700）—— **不引入广播通道依赖环**

---

## 3. PNN 借鉴矩阵（CppTLM 视角）

### 3.1 Gem5 Ruby 演化对 PNN 设计的 6 个启示

| 启示 | Gem5 体现 | CppTLM PNN 借鉴 |
|------|-----------|----------------|
| **协议无关组件最大化** | 4+1 类（Seq/Cache/Repl/MemCtrl + NI） + SLICC generated controller | `ProtocolBridge<InB, OutB>` 抽象出 4 类 protocol-independent 组件 |
| **协议层编译期绑定** | SLICC 编译期烧死，无运行时反射 | C++ 模板编译期绑定（同思路，丢 DSL 换直接） |
| **vnet 是保序正确性单元** | ordered/unordered vnet + data vs ctrl 信用容量差异 | `vc_counts_[4]` 按 flit_category 分桶（已 e1 v1.1；**2026-10-16 critic 校正**：原文档"escape VC"为误读，gem5 不存在 escape VC 概念） |
| **多通道分离** | CHI 4 vnet (REQ/SNP/RSP/DAT) + GPU_VIPER 11 vnet | e1 v1.1 4 通道对位 CHI |
| **kernel-boundary flush/inv** | GPU_VIPER I/V/A/F + Flush/Evict/InvL2 事件 | P3 候选：GLC/SLC bypass + flush/inv 事件 |
| **死锁恐慌兜底** | `calculateVC()` panic + `send_allowed()` ordered 门控 | P3 候选：`RouterTLM::run()` + panic on stall count |

### 3.2 CppTLM PNN 不应直接采纳的 4 类 gem5 决策

| Gem5 决策 | CppTLM 不采纳原因 | 替代方案 |
|-----------|------------------|---------|
| **SLICC DSL 编译链** | 工程太重（PLI/Python parser + AST + SConscript；**第三轮 critic 校正**：原文"13 个 CI 测试"无来源，src/mem/slicc/ glob 无 test 目录，应删除该数字） | 直接 C++ 模板 + 编译期 `if constexpr` |
| **`mandatoryQueue` / `optionalQueue` 拆开** | C++ 模板组合表达力差 | 用模板参数 `Optional = bool_constant<>` 统一一个 buffer 类 |
| **`triggerQueue ordered=True` 显式类** | 现代 C++ 模板元编程可以直接表达 | `StreamBuffer<T, Ordered = true>` 模板参数 |
| **`configs/topologies/*.py` 拓扑 Python 化** | CppTLM 已有 JSON 拓扑 + `ModuleFactory` 装配 | 沿用 JSON `connections[]` + `port_index` |

### 3.3 PNN 阶段的演进对标（Gem5 历史）— 月数为量级估算（±2x，WBS 后重估）

> **2026-10-16 critic 复审校正（S7）**：原表格月数为**无依据估算**，**降级为量级参考**（±2x）。

| PNN 阶段 | 对标 Gem5 历史里程碑 | 量级月数估算 |
|---------|---------------------|-------------|
| **P0** (当前既有) | M5 早期（2003 Ruby 起源） | 0（已有） |
| **P0.5**（e1 v1.1） | Garnet 3.0 vnet 分桶（**数量级对齐，语义不等价**） | 0（已有） |
| **P1**（ProtocolBridge 抽象） | SLICC 引入 2005 | **量级 1 月**（无推导依据） |
| **P2**（CHI-like 4 vnet 强化） | Garnet 2.0 通道分离 2016 | **量级 2 月**（无推导依据） |
| **P3**（5-stage cycle-accurate） | Garnet 1.0 5-stage 2009 | **量级 3 月**（无推导依据） |
| **P4**（GPU-VIPER-like kernel-boundary） | GPU_VIPER 2020 | **量级 4-5 月**（无推导依据） |
| **P5**（CXL-like） | ❌ **no-op**（gem5 自身未实现，盲目跟会失真） | **量级 12+ 月**（等 gem5 成熟） |

> **关键校正**：CHI 协议在 gem5 mainline 集成约 9 年（[推断]），远不止 7 年；gem5 的成熟度不能用作 CppTLM 月数估算依据。**月数仅为量级，定 WBS 时必须以 CppTLM 自身 PR 历史数据回归估算**。

### 3.4 风险与陷阱（来自 gem5 历史教训）

**5-年内被废弃的 gem5 决策**（per gem5-soc-survey.md §0）:
1. **Garnet 1.0 → 2.0**（pipeline 重新设计，VA 级消失）
2. **`garnet2.0/` 目录 → `garnet/`**（3.0 合并）
3. **旧 `se.py` / `fs.py` → `gem5_library/`**（v23+ `fatal()` 拒绝运行）
4. **`src/mem/ruby/network/topologies/` 目录删除**（移到 `configs/topologies/`）
5. **`pli/slicc-emitter` 废弃**（Python + PLY 是当前路径）

**对 CppTLM PNN 的启示**:
- `ProtocolBridge<InB, OutB>` 接口**保留稳定**（不轻易改模板签名）
- `network.mode` 字符串枚举保留扩展空间
- 拓扑 JSON schema 演进时优先增字段，不删字段
- 不要把 PNN 与 `simulation_mode` 合并——两者正交

---

## 4. PNN 借鉴决策矩阵（一表）

| 决策项 | gem5 做法 | CppTLM PNN 选择 | 理由 |
|--------|-----------|-----------------|------|
| **协议控制器** | SLICC DSL 编译期 | C++ 模板 + 编译期绑定 | 工程化平衡（DSL 复杂度 vs 可读性） |
| **网络切分粒度** | flit（Garnet）/ message（Simple） | P0: message; P3: flit（按需） | 仿真速度 + 精度双 axis |
| **vnet 数量** | 由协议决定（CHI=4, GPU=11） | 4（对位 e1 v1.1）；预留扩展 | CHI is canonical reference |
| **协议层防死锁** | vnet 分离 + ordered/unordered 门控 | e1 v1.1 已对位 + 加 ordered 显式 flag | 协议层独立保序正确性 |
| **拓扑** | `configs/topologies/*.py` | JSON + `ModuleFactory` | 已落地，无需变 |
| **路径选择** | weight bias routing + 维序（XY） | 沿用既有 `RouterTLM` 加 weight 字段 | 无环路由 |
| **目录协议** | MOESI_CMP_directory（O+S+M） | P3: 显式 Sharers+Owner 位图 | dGPU SoC 可能需要 Snooping 协议 |
| **Token / probe filter** | 学术 + AMD niche | **不直接采纳**（用目录位图代） | token 引入广播环，工程代价 > 收益 |
| **CHI / CHI.B** | mainline gem5 + WIP extensions | P2: 借鉴 4 vnet 思想（已 e1 v1.1） | CHI is canonical reference 2020+ |
| **CXL** | WIP | **不动** | gem5 仍在 WIP，盲目跟会失真 |
| **GPU L1** | GPU_VIPER I/V/A/F + flush/inv | P3: GLC/SLC 字段映射 + kernel-boundary flush | dGPU 必然需要 |
| **DSL 编译链** | SLICC（Python+PLY） | **拒绝 DSL，保留 C++ 直接** | 复杂度 / 收益不匹配 |
| **backpressure** | ordered panic + optional 静默丢 | 模板参数化（已有 Stream Adapter + 补充） | P1 增量 |

---

## 5. 调研结论

### 5.1 三个核心结论

1. **CppTLM PNN 应"继承 gem5 Ruby 80% 设计 + 拒 20%"**
   - 继承：4+1 类 protocol-independent 组件抽象 + vnet 分桶保序 + 编译期协议绑定
   - 拒：SLICC DSL + Python 拓扑 + 部分学术特性（token/probe filter）

2. **CHI 是 2020 后 canonical reference 协议**
   - 4 通道（REQ/SNP/RSP/DAT）→ 4 vnet 设计对位 e1 v1.1 `vc_counts_[4]`
   - TBE + 单 BUSY transient 是 CHI 最重要的设计指纹
   - **CXL 仍在 WIP**——不要盲跟 gem5，等成熟

3. **PNN 实施阶段顺序的 gem5 历史锚点**
   - P0 → P5 时间线映射到 Garnet 1.0 → 2.0 → 3.0 + CHI 各里程碑
   - P2 阶段不应 ≥3 月（CHI 已约 9 年沉淀，**第三轮 critic 校正**：原文"7 年"是误算，应以 9 年为参照）
   - P5（CXL-like）需要 12-18 月缓冲（gem5 仍在 WIP）

### 5.2 PNN 必看的 3 篇 gem5 论文 / 文档

1. **GEMS SLICC 论文** (Martin 2005): protocol 描述语言理论 + AST
2. **Garnet 论文** (Agarwal 2009 ISPASS): on-chip network 模型标准
3. **gem5 CHI 文档** (gem5.org/documentation/general_docs/ruby/CHI/): 当前最现代协议实现参考

### 5.3 PNN 必看的 3 个 gem5 实施锚点

1. **`src/mem/ruby/network/NetworkInterface.{hh,cc}`** —— NI 切分/重组（CppTLM ProtocolBridge 直接对位）
2. **`src/mem/ruby/protocol/chi/CHI-cache.sm`** —— CHI cache state machine 4 vnet 布局
3. **`configs/ruby/MOESI_CMP_directory.py`** —— directory 协议拓扑样板（含 triggerQueue ordered + 目录↔内存直连）

---

## 6. 关联文档

- **主调研**: [`gem5-ruby-protocol-bridge-survey.md`](gem5-ruby-protocol-bridge-survey.md)（700+ 行，含 4 节原深化 + 11 章关联文档）
- **协议族细节**: [`gem5-coherence-protocols-survey.md`](gem5-coherence-protocols-survey.md)（499 行）
- **SLICC + CHI 细节**: [`gem5-slicc-chi-survey.md`](gem5-slicc-chi-survey.md)（299 行）
- **Garnet 网络细节**: [`gem5-garnet-network-survey.md`](gem5-garnet-network-survey.md)（256 行）
- **NI / vnet 流转细节**: [`gem5-messagebuffer-network-interface.md`](gem5-messagebuffer-network-interface.md)（新, 2026-10）

**架构方案**（已存在，**第三轮 critic 校正**：原"待起草"是过期）:
- [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md)
- [`docs/adr/ADR-DGPU-13-pnn-protocol-neutral-network.md`](../adr/ADR-DGPU-13-pnn-protocol-neutral-network.md)

---

**调研结束时间**: 2026-10-16
**调研者**: CppTLM build agent（基于 3 份 subagent 调研整合）
**下次复审**: PNN P1 实施前（11 月估）
**复审触发**: gem5 下一版本发布 / CXL 协议成熟 / CppTLM e1 v1.1 实施
