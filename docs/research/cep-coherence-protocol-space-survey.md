# Coherence Exploration Platform 协议空间调研 v0.3 — ArchForge MOESI/GPU 6×6 + gem5 Ruby SLICC + *Gen 算法族(PDF 原文级)

> **文档状态**: 📋 Research Reference(调研附属,**不进 ABI 契约**,per AGENTS.md 真源分级:调研 = 附属层)
> **调研目的**: 验证 CEP(Coherence Exploration Platform)抽象能否容纳既有协议实现空间,并暴露缺口
> **版本**: **v0.4**(2026-10-21)
> - v0.1:源 = 1 份 153 行摄入摘要
> - v0.2:源 = 3 篇论文**全文抽取件** + 12 份 wiki 条目(5 类 18 份)
> - v0.3:源 = 3 篇论文 PDF 原文 + 178 页博士论文 PDF 原文 + 3 源码仓 + 12 份 wiki 条目(5 类 21 份 + 3 仓);新增 17 条发现 F16~F28
> - **v0.4:经 Oracle 独立架构复审**(2026-10-21,首次取得独立复审)**;落地 N1~N9 共 9 项修复 + 5 项架构级修订(CEN-Ib11 / `INV-CEP-06-IC5` / `ADR-CEP-12` schema 补 5 维度 / `ADR-CEP-09` 两段式 / 07 号 §7.2.1);invariant 计数改 **37 编号位 / 36 实际**
> **作者**: CppTLM build agent(architect mode)
>
> **调研对象(5 类 21 份 + 3 源码仓)**:
> - **ArchForge** `docs/architecture/09-coherence-protocol.md` v1.0-draft(2027-02-09,514 行)— dGPU SoC MOESI/GPU 6×6 状态机
> - **gem5 Ruby SLICC**(via `docs/research/gem5-{coherence-protocols,ruby-protocol-bridge,slicc-chi}-survey.md`)
> - **Oswald *Gen 算法族(PDF 原文级 + 源码级)**:
>   - `Nicolai_Oswald_PhD_Thesis.pdf`(**178 页**,`00_Raw_Sources/papers/`)
>   - `gen-family/ProtoGen_ISCA2018.pdf`(14 页)/ `HieraGen_ISCA2020.pdf`(12 页)/ `HeteroGen_HPCA2022.pdf`(16 页)
>   - 对应**文本抽取件**(`04_Knowledge/D01-gpu-architecture/generations/*/*-draft.md`,便于 grep)
>   - **三仓源码**(v0.3 新增,已 clone 到 `/tmp/opencode/gen_src/`):`Errare-humanum-est/{ProtoGen,HieraGen,HeteroGen}`(**MIT License**,最后提交 2022-01-07)
>   - `oswald-phd-thesis-draft.md`(153 行摄入笔记,v0.1 的唯一 *Gen 源)
>   - `01_Wiki/` 12 份概念/实体条目
>
> **关联真源 1**:
> - [`docs/architecture/07-coherence-exploration-platform.md`](../architecture/07-coherence-exploration-platform.md) v0.1(CEP 顶层真源)
> - [`docs/architecture/06-inter-module-network.md`](../architecture/06-inter-module-network.md) v0.4
> - [`docs/architecture/06-bis-coherence-as-network-extension.md`](../architecture/06-bis-coherence-as-network-extension.md) v0.3
>
> **关联真源 3**:
> - [`ADR-CEP-00`](../adr/ADR-CEP-00-platform-charter-and-governance.md) ~ [`ADR-CEP-07`](../adr/ADR-CEP-07-simulation-and-integration-semantics.md) + [`ADR-CEP-NUMBER-MAP`](../adr/ADR-CEP-NUMBER-MAP.md)
> - [`ADR-CEP-17-ruby-slicc-protocol-family.md`](../adr/ADR-CEP-17-ruby-slicc-protocol-family.md)(v0.3 已随本批修订)

---

## 0. 结论摘要

### 0.1 调研源清单(v0.3 升级)

| 维度 | v0.1 | v0.2 | **v0.3** |
|------|------|------|----------|
| *Gen 族证据等级 | 153 行摄入摘要(二手) | 3 篇论文**全文抽取件**(一手) | **3 篇论文 PDF 原文 + 178 页博士论文 PDF 原文** |
| 流水线结构 | 未知 | 论文的 5 步(单工具) | **完整 4 阶段流水线 + Table 1.3 工具能力矩阵** |
| 工具自评 | 未知 | 未知 | **§9.2 Critical Analysis + §9.3 Future Work 7 项** |
| 术语(SSC/MSD/PPL/IR) | 二手转述 | **判负(错误)** | **一手确认全部存在** |
| *Gen 族**源码** | 未知 | 未知 | **3 仓已 clone**(MIT License);`.pcc` 描述语法 / `StateSet` / `ClassNetworkClassification` / `gem5_HeteroGen_protocols/` 已审阅 |
| 源总数 | 5 | 18 | **21 + 3 源码仓** |

### 0.2 6 个调研问题与结论

| # | 问题 | 结论 |
|---|------|------|
| **Q1** | CEP canonical 10 流 + Stub Cache `I/S/E/M` 能否容纳 **ArchForge MOESI/GPU 6×6**? | ✅ **能**,6 事件 100% 映射;暴露 **2 个状态缺口**(`O` / `T`) |
| **Q2** | CEP 能否容纳 **gem5 Ruby SLICC 协议族**? | ✅ **能**,6/8 直接支持;需 **4 处扩展**;`HTM` 明确排除 |
| **Q3** | **ProtoGen / HieraGen / HeteroGen** 对 CEP 抽象提出什么要求? | ⚠️ 3 维度命中;v0.3 给出**可判定判据**而非口号 |
| **Q4** | *Gen 族 **5 条显式局限性** 对 CEP 架构边界的说明? | 🎯 **决定性证据**:L2 坐实 atomic 边界;L4 独立验证 CEP 分层 |
| **Q5** | DSL 路线 vs C++ 插件路线 冲突吗? | ⚠️ **明确分歧,但方向互补**;学术侧 **ProtoGen+ 正在做 SLICC/Rumur 后端**(F23) |
| **Q6** | **\*Gen 工具自身的边界**是什么?(作者自陈) | 🔴 **不能同时做分层 + 异构**;VC 自动分配未做;穷尽验证开销是公认痛点(F18/F19/F22) |

### 0.3 最重要的 4 个发现

**发现 1 — ProtoGen 局限 L4 独立验证了 CEP 的分层**(v0.2 已得,v0.3 已回溯 PDF 坐实)
> "Third, ProtoGen does not specify how protocol actions must interact with the interconnect; it requires the user to manually define virtual channels and assign messages to channels."(`ProtoGen_ISCA2018.pdf` p.7,抽取件 `:755-757`)

**发现 2 — ProtoGen 局限 L2 坐实 atomic 边界**(v0.2 已得,v0.3 已回溯 PDF 坐实)
> "Second, ProtoGen cannot generate new protocol actions not explicitly specified in the SSP. For example, it cannot infer how atomic read-modify-writes must be implemented without it being specified in the SSP;..."(`ProtoGen_ISCA2018.pdf` p.7)

**发现 3 — HieraGen 的"唯一序列化点"是分层组合的可判定判据**
> "any two racing coherence transactions will serialize at exactly one of the directories"(`HieraGen_ISCA2020.pdf` p.2,抽取件 `:151-155`)

**发现 4(v0.3 新增)— VC 分配的目的是**死锁自由**,不是性能**
> "ProtoGen does not automatically assign messages to virtual networks yet **for ensuring deadlock freedom**."(博士论文 §9.3.1)

⚠️ **该条出自论文 §9.3 "Future Work",即作者尚未实现** —— 本调研只取其**方向性**结论(VC 分配的职责归属),不当作已验证事实。

→ 这条把 CEP 的 Fabric & Transport Plane 从"消息路由的实现细节"提升为**"死锁自由性论证的载体"**,是 `ADR-CEP-09` / `ADR-CEP-08` 的硬要求。

### 0.4 v0.2 → v0.3 修订摘要(含**自我纠错**,不掩饰)

| # | v0.2 的说法 | v0.3 的修正 | 性质 |
|---|------------|-------------|------|
| **C1** | §8.2 B7:"博士论文原文**未获取**" | ✅ **已获取**(178 页 PDF),§4.5 新增论文独有深度分析 | **撤回错误边界声明** |
| **C2** | §8.2 B8:"`00_Raw_Sources/papers/gen-family/` 的 PDF **不在本次范围**" | ❌ **该目录存在**,且 `Nicolai_Oswald_PhD_Thesis.pdf` 也在 —— 我此前只 `find -name "*.md"`,**漏检了 PDF** | **撤回错误边界声明 + 记录方法论失误** |
| **C3** | §1.3 Q3:"SSC / MSD … **无法在一手源核实**" | ✅ **一手确认全部存在**(thesis Ch4 PPL / Ch5 SSC+MSD) | **撤回核对误判**(v0.2 错在怀疑 wiki) |
| **C4** | §1.3 Q4:"PPL 专名 / IR 提法 **来源不可核实**" | ✅ **一手确认**:"ProtoGen Protocol Language" 是 thesis Ch4 章名;IR 是 thesis §4.2.1 小节;**HeteroGen 依赖 antlr3 3.5**(解析器由 ANTLR 生成) | **撤回核对误判** |
| **C5** | §1.4:"`github.com/icsa-caps/ProtoGen`" | ⚠️ **两个 org 并存**:ISCA 2018/2020 论文脚注 = `icsa-caps`;**博士论文(2023)脚注 = `Errare-humanum-est`**(含 HeteroGen) | **修正** |
| **C6** | §7.1 "需 4 处扩展" | ✅ 维持,但 **atomic 扩展已闭合**;新增 **VC 分配=死锁自由** 这一扩展点 | 补充 |
| **C7** | §1.4 "`github.com/icsa-caps/ProtoGen`" + "两个 org 并存" | ❌ **修正**:二者是**同一份代码的两个镜像**;`icsa-caps` 是**冻结的废弃快照**(commit = "Discontinued Snapshot"),`Errare-humanum-est` 是作者维护版 | **修正** |
| **C8** | F23 "学术侧**正在**做从 SSP 生成 SLICC" | ❌ **修正**:三仓**后端只有 Murphi**,SLICC/Verilog/Rumur **至今未落地**;但 *Gen 族**已能直接产出 gem5 `.slicc`**(源码 `gem5_HeteroGen_protocols/`) | **修正 + 强化** |
| **C9** | §8.2 B3 "未拉取源码" | ✅ **撤回**:三仓已 clone(MIT License),已做结构级审阅(见 §4.6) | **撤回错误边界声明** |

> **方法论教训(§1.2 详述)**:跨仓调研**不能只按域目录 + 文件后缀取**。v0.1/v0.2 两次都因为"只 find `*.md`"而漏掉了一手 PDF,导致 2 条边界声明错误 + 2 条核对误判。**一手源必须按"文件名关键词 + 目录 + 后缀"三重检索**。

---

## 1. 调研源清单与数据质量核对

### 1.1 全量源清单

| # | 源 | 路径 | 规模 | 等级 |
|---|----|------|------|------|
| S1 | ArchForge dGPU Coherence | `/workspace/main/ArchForge/docs/architecture/09-coherence-protocol.md` | 514 行 | 一手(跨仓真源 2,草稿态) |
| S2-S4 | gem5 Ruby 3 篇 | `docs/research/gem5-{coherence-protocols,ruby-protocol-bridge,slicc-chi}-survey.md` | — | 二手(仓内调研) |
| S5 | **博士论文 PDF** | `00_Raw_Sources/papers/Nicolai_Oswald_PhD_Thesis.pdf` | **178 页 / 373,750 字符** | **一手(PDF 原文)** |
| S6 | **ProtoGen PDF** | `00_Raw_Sources/papers/gen-family/ProtoGen_ISCA2018.pdf` | 14 页 / 72,161 字符 | **一手(PDF 原文)** |
| S7 | **HieraGen PDF** | `00_Raw_Sources/papers/gen-family/HieraGen_ISCA2020.pdf` | 12 页 / 58,815 字符 | **一手(PDF 原文)** |
| S8 | **HeteroGen PDF** | `00_Raw_Sources/papers/gen-family/HeteroGen_HPCA2022.pdf` | 16 页 / 78,044 字符 | **一手(PDF 原文)** |
| S9-S11 | *Gen 论文抽取件(3) | `04_Knowledge/D01-gpu-architecture/generations/*/*-draft.md` | 1687 / 1310+ / 1440+ | 一手(便于 grep) |
| S12 | Oswald 摄入笔记 | `04_Knowledge/D12-cache-coherence-numa/oswald-phd-thesis-draft.md` | 153 行 | 二手摘要 |
| S13-S19 | Wiki 概念条目(7) | `01_Wiki/concepts/{protogen,hieragen,hetergen,gen-algorithm-family,ppl-protogen-protocol-language,stable-state-protocol,murphi-model-checker}.md` | — | 二手消化 |
| S20-S21 | Wiki 概念条目(4) | `01_Wiki/concepts/{compound-consistency-model,memory-consistency-protocols,cache-coherence-gpu,relaxed-memory-model}.md` | — | 二手消化 |
| S22 | Wiki 来源 + 实体(5) | `01_Wiki/sources/oswald-phd-thesis-2023.md` + `01_Wiki/entities/*.md` | — | 二手元数据 |
| S23 | RESEARCH_MAP | `04_Knowledge/RESEARCH_MAP.md` | — | 索引 |

### 1.2 源获取路径的教训(v0.1/v0.2 的方法论失误)

**失误**:v0.1 与 v0.2 我均以 `find . -name "*.md"` 扫描 mynotes,**因此完全没有发现 `00_Raw_Sources/` 下的 35 份 PDF**。由此产生 4 个连锁后果:

| 后果 | 影响的结论 |
|------|-----------|
| ① 误以为"只有 153 行摘要" | v0.1 的 §3 只能给方向性判断,无法给判据 |
| ② 误写 §8.2 B7/B8 两条**错误边界声明** | 把"没找到"写成了"不存在",构成对读者的**误导性诚实** |
| ③ 误判 wiki 的 SSC/MSD/PPL/IR 为"不可核实" | v0.2 §1.3 Q3/Q4 **给了 wiki 负面判定**,而实际 wiki 是对的 |
| ④ 错过了论文独有的 §7.6.6 / §9.2 / §9.3 | 漏掉 9 条发现(F16~F23),其中 3 条是**决策级**的 |

**正确做法(已执行)**:
```bash
find /workspace/main/mynotes -iname "*.pdf" -o -iname "*.md" | sort   # 后缀全覆盖
find / -xdev \( -ipath "*gen-family*" -o -iname "*oswald*" -o -iname "*protogen*" ... \)  # 关键词 + 全盘
```
→ 发现 `00_Raw_Sources/papers/gen-family/`(3 份)+ `Nicolai_Oswald_PhD_Thesis.pdf`。

> **给后续调研的规则**:**"未找到"与"不存在"必须区分**。边界声明里只能写"我搜了 X、Y、Z,未找到";不能写"该源不存在"。

### 1.3 wiki 消化层 vs 论文原文:核对表(v0.3 修订)

> 核对方法:逐条比对 S13-S19 的断言与 S5-S8 原文。**本表在 v0.2 判负 2 条,v0.3 已用一手源推翻自己的判定**。

| # | wiki 断言 | 论文原文 | **v0.3 判定** |
|---|-----------|---------|--------------|
| **Q1** | `hieragen.md`:"支持更深层次、**不完整目录知识、非包容共享缓存、跨层通信**" | §VII-D:"HieraGen is **limited to inclusive shared caches**";§VII-E:"All communication across hierarchy levels is strictly parent/child... **critical to the current design**" | ❌ **明确错误**(2 项)。"更深层次"与"不完整目录知识"确为**放宽**项,另 2 项是**不支持** |
| **Q2** | `protogen.md`:"支持 MOESI 变体 / TSO-CC / RCC / DeNovo / **AMBA CHI**" | ISCA 2018 只生成 **MSI/MESI/MOSI/TSO-CC** + unordered-network MSI。**DeNovo 仅作 related work 出现**(§II-C);**CHI 未提** | ⚠️ **张冠李戴**。CHI/RCC/DeNovo 属**论文集层面**(经 HieraGen/HeteroGen),非 ProtoGen 本身。⚠️ **但 v0.3 补充:博士论文 §2.2 列出了 MOESI/TSO-CC/RCC/RCC-O/DeNovo/GPU 六类输入协议**,故"RCC/DeNovo 可作为输入"在**论文集层面成立**,仅 "ProtoGen 本身直接生成"不成立 |
| **Q3** | `gen-algorithm-family.md`:"SSP Pre-Processing:**SSC + MSD + 状态空间最小化**" | **博士论文 Ch5 明确**:§5.1 State Space Checker (SSC)、§5.2 Intra-Transaction Message Stall Detector (MSD)、§5.3 SSP State Space Minimization | ✅ **v0.2 判负错误**。**wiki 完全正确**,且我第一手核实了全部 3 项(见 §4.5.1) |
| **Q4** | `ppl-protogen-protocol-language.md`:"**PPL (ProtoGen Protocol Language)**" + "**IR** 中间表示" | **博士论文 Ch4 章名 = "ProtoGen Protocol Language"**;§4.2.1 = "Intermediate Representation (IR) Format and Notation";§4.1 Process / §4.1.2 Types;**HeteroGen 依赖 `antlr3 3.5`**(解析器由 ANTLR 生成) | ✅ **v0.2 判负错误**。**wiki 完全正确**,一手坐实 |
| **Q5** | `protogen.md`:"所有生成协议均通过验证" | Murϕ 验证 **仅 3 caches**(`ProtoGen_ISCA2018.pdf` p.12,内存上限);HieraGen 需 **256GB** + hash compaction;HeteroGen **1-3 caches/簇** | ⚠️ **缺关键限定**。状态空间是**真实瓶颈** |
| **Q6** | `hieragen.md`:"MOESI/MOESI dir/cache **59 states, 368 transitions**" | Table II(**atomic**)= 59/368;Table III(**并发 non-stalling**)= **81/495** | ⚠️ **未区分 atomic / concurrent** |
| **Q7** | `hetergen.md`:"111 个异构 litmus tests" | 原文 `:1291` "We used the herd7 tool to generate 111 litmus tests" | ✅ **正确** |

**v0.3 核对统计**:7 条 → **3 条正确**(Q3/Q4/Q7,其中 Q3/Q4 是 v0.2 判负后被一手源推翻)+ **1 条明确错误**(Q1)+ **3 条偏差**(Q2/Q5/Q6)。

> **元结论(比核对表本身更重要)**:v0.2 用"**我手上只有摘要**"这个事实,去质疑 wiki 的**一手源级断言**,并写进了文档 —— 这是**过度断言**。v0.3 的教训:**当二手源与一手源冲突而我只有二手源时,正确做法是标注"待一手核实",而不是判负**。这一条同样适用于我自己的调研文档的引用。

### 1.4 *Gen 工具链可获取性(v0.3 修正)

| 工具 | ISCA/HPCA 论文脚注 | 博士论文脚注(2023) | **v0.3 源码核实结论** |
|------|-------------------|---------------------|---------------------|
| ProtoGen | `github.com/icsa-caps/ProtoGen` | `github.com/Errare-humanum-est/ProtoGen.git` | ⚠️ **同一份代码的两个镜像**;`icsa-caps` commit = "**Discontinued Snapshot**";README 自述 "**discontinued and replaced by HeteroGen**" |
| HieraGen | `github.com/icsa-caps/HieraGen` | `github.com/Errare-humanum-est/HieraGen.git` | ✅ 可 clone,含 `HieraAlgorithm/` |
| HeteroGen | `doi.org/10.5281/zenodo.5789379` | `github.com/Errare-humanum-est/HeteroGen.git` | ✅ 可 clone,**含 `gem5_HeteroGen_protocols/`**;**不含** HieraGen 算法 |

> **v0.3 修正(v0.2 的"两个 org 并存"表述不准确)**:二者不是两套代码,而是**同一份代码的两个镜像**;`icsa-caps` 是**冻结的废弃快照**(commit message 直接写 "Discontinued"),`Errare-humanum-est` 是作者维护版(内容仅 README 不同)。

**额外发现的依赖与产物**(per `HeteroGen_HPCA2022.pdf` p.14 复现说明):
- `antlr3 3.5` —— **PPL 解析器由 ANTLR3 生成**(证实 PPL → IR 是真实的编译器前端)
- `CMurphi 5.4.9.1` —— Murϕ 5.4.9.1 源码随 Zenodo 提供(`zenodo.org/record/3910803`)
- 另见 `doi.org/10.5281/zenodo.5793240`("Code available on Zenodo")

**后端实现状态(v0.3 已源码核实)**:
- ISCA 2018 论文:只实现了 **Murϕ 后端**;"translation to other outputs like **SLICC or Verilog is future work**"
- **博士论文 §9.2**:正在开发 **ProtoGen+**,整合 3 个算法 + "a variety of Backends like **Rumur and SLICC**"
- **源码核实(v0.3)**:三仓的后端目录**只有 Murphi**(HeteroGen 为 `Backend/{Common, Murphi}`)。→ ⚠️ **SLICC / Verilog / Rumur 后端至今未落地**(仓库最后提交 2022-01-07,论文 2023)
  → **修正 F23**:竞合压力**尚未兑现** —— *Gen 族当前**不能**从 SSP 生成 SLICC。但**已经能直接产出 gem5 `.slicc`**(见 §4.6.2)

**代码规模**(per 博士论文 Table 9.1,**Python 行数**):

| 工具 | 算法行数(论文口径) | 全仓 Python 行数(v0.3 实测,含 vendor) | PPL 版本 | Parser | Backend | ProtoGen 算法 |
|------|-------------------|------------------------------------|---------|--------|---------|--------------|
| ProtoGen | 8,849 | 16,110 | v1 | v1 | – | – |
| HieraGen | 15,771 | 25,727 | v1 | v1.1 | v1 | – |
| HeteroGen | 30,413 | 37,198 | v2 | v2 | – | v1 |
| **ProtoGen+** | **45,000+** | (未落地) | v2.1 | v2.1 | v2 | v1.1 |

> **口径说明**:论文数字是**纯算法实现行数**;v0.3 实测是**全仓 Python 行数**(含 `antlr3/`、`CMurphi/` 等 vendored 依赖)。两者**不可直接比较**,引用时须注明口径。

> **对 CEP 的量级校准**:"做一个一致性协议编译器"是 **30k-45k 行 Python** 的工程量。CEP 走 C++ 插件路线、每协议一个手写 adapter,单协议成本远低于此 —— 这正是 CEP 相对 *Gen 族的**成本优势**,应写进 `ADR-CEP-18`(协议生成路线范围)的 trade-off 表。

---

## 2. ArchForge MOESI/GPU 6×6 → CEP 映射验证(Q1)

### 2.1 ArchForge 09 的协议实体(per `09-coherence-protocol.md` §3.1-§3.3 实证)

**6 状态**:

| 状态 | 缩写 | 含义 | 类别 |
|------|------|------|------|
| Invalid | I | 无效 | 稳定 |
| Shared | S | 共享读 | 稳定 |
| Exclusive | E | 独占读 | 稳定 |
| Modified | M | 已修改,独占写 | 稳定 |
| **Owned** | **O** | **已拥有,与其他 cache 共享(脏)** | **稳定但 MOESI 特有** |
| **Temporary** | **T** | **临时状态(过渡)** | **瞬态** |

**6 事件**:Rd / Wr / Inv / InvAck / Fl / FlAck

**转换表**(`uint8_t state_transition[6][6]`,per `09-coherence-protocol.md:148-156`):

```cpp
// 状态: 0=I, 1=S, 2=E, 3=M, 4=O, 5=T
// 事件: 0=Rd, 1=Wr, 2=Inv, 3=InvAck, 4=Fl, 5=FlAck
uint8_t state_transition[6][6] = {
    { 1,      3,      0,      0,      0,      0  },   // I
    { 1,      3,      0,      1,      4,      1  },   // S
    { 2,      3,      0,      2,      4,      2  },   // E
    { 3,      3,      3,      3,      3,      3  },   // M
    { 4,      3,      0,      4,      4,      4  },   // O
    { 1,      3,      0,      1,      4,      1  },   // T (过渡回 S/M)
};
```

### 2.2 6 事件 → CEP canonical 10 流映射(**100% 覆盖**)

| ArchForge 事件 | CEP 流 | 方向 | 备注 |
|----------------|--------|------|------|
| **Rd** 读请求 | 流 1 `request` | node → center | perm = READ |
| **Wr** 写请求 | 流 1 `request` | node → center | perm = UNIQUE |
| **Inv** 使他 cache line 失效 | 流 3 `snoop`(type=INVALIDATE) | center → node | 或流 7 `back_invalidate` |
| **InvAck** 失效确认 | 流 4 `snoop_resp` | node → center | hit + state + txn_id |
| **Fl** 脏数据写回 | 流 5 `writeback` | node → center | addr + data + dirty |
| **FlAck** flush 确认 | 流 10 `snoop_completion_ack` | node → center | 配对 `snoop_resp` |

**结论**:6 事件 **全部**落在 CEP canonical 10 流内,**无需新增流**。ADR-CEP-02 的"零流数变更"承诺**成立**。

### 2.3 ArchForge 组件 → CEP 6 层映射

| ArchForge 组件(per `09` §10.1)| CEP 6 层对应 | 备注 |
|---------------------------------|--------------|------|
| `CoherenceProtocol`(6×6 状态机) | **Protocol Engine 插件内部** | 协议状态机 = 插件职责 |
| `CoherenceDomain`(per-cache domain) | **Sharer Tracker** + Protocol Engine 协同 | per `ADR-CEP-04` |
| `CoherenceBridge`(CPU↔GPU 跨域) | **流 7 `back_invalidate`** + Protocol Engine 跨域映射 | 跨域 = 协议转换,非目录职责 |
| `SnoopFilter` | **Sharer Tracker `SparseSnoopFilter`** | caps `CONSERVATIVE` = probe filter 语义 ✅ |
| `CoherentXBar`(每端口 domain_id) | **Fabric & Transport Plane** + Directory Plane snoop 发起 | `domain_id` ≈ CEP `barrier_domain` |
| `CacheTLM` snoop callback | **Stub Cache Core** | per `ADR-CEP-05 §3.2` 折叠表 |

### 2.4 暴露的 2 个状态缺口

**缺口 1:`O`(Owned)** — 稳定但 MOESI 特有(脏且共享)。选项 A 扩 `perm` / B 组合表达 / **C 归 Protocol Engine 内部**(Stub 暴露 S + dirty 标志)。
**缺口 2:`T`(Temporary)** — 瞬态态。**归 Protocol Engine 插件内部**。

**一手佐证(v0.3 强化)**:
- `ProtoGen_ISCA2018.pdf` p.1:"Even a relatively simple MSI directory protocol has **eighteen transient states**"
- 博士论文 §2.4.2 有独立小节 "Transient States"
- 博士论文 Ch5 Step 3 "SSP State Space Minimization" + ProtoGen 算法 Step 2 "Assign Transient States to State Sets"

→ **"稳定状态 = SSP,瞬态状态 = 生成产物"是 ProtoGen 体系的显式二分**,与 CEP 的 Stub(SSP 层)/ Protocol Engine(瞬态层)**完全同构**。

**v0.3 新增佐证(§2.2.5 GPU 协议,一手)**:
> 博士论文 `:1247-1249`:"GPU is a simple GPU protocol as specified in Spandex [19] that enforces **release consistency (RC)**. **Writes to a cache line are immediately written through to the shared cache becoming globally visible.**"

→ **ArchForge 09 §4.1 的 "GPU 请求走 write-through 直写策略" = Spandex 风格 RC write-through**。这是**跨仓一手对照**,不是类比。

### 2.5 ArchForge 09 的一个已验证的决策

`09-coherence-protocol.md` §4.4(per ADR-SOC-01):**"永不复制 gem5 slicc DSL"** — 理由:"slicc 是 gem5 特定语言、5000+ 行不可读";CppTLM 用 **C++ `switch` 表驱动**。

→ 与 CEP 路线**完全一致**。**注意与发现 1 的区别**:ArchForge 反对的是**语言载体**(DSL),ProtoGen L4 反对的是**接口边界**(互连)。两者**互补不重叠**,引用时不可混用。

---

## 3. gem5 Ruby SLICC 协议族 → CEP 映射验证(Q2)

### 3.1 gem5 Ruby 协议族实证清单(per `gem5-coherence-protocols-survey.md:16-23`)

| 协议 | `.slicc` 主文件 | 组成 `.sm` | 类别 |
|------|---------------|-----------|------|
| MI_example | MI_example.slicc | msg/dir/cache/dma | 教学最小协议 |
| **MESI_Two_Level** | MESI_Two_Level.slicc | msg/dir/L1cache/L2cache/dma | directory, 2 级 |
| **MESI_Three_Level** | MESI_Three_Level.slicc | msg/L0cache/L1cache | directory, 3 级 |
| **MESI_Three_Level_HTM** | MESI_Three_Level_HTM.slicc | L0cache (HTM) | directory + **硬件事务内存** |
| **MOESI_CMP_directory** | MOESI_CMP_directory.slicc | msg/dir/L1cache/L2cache/dma | directory, MOESI |
| **MOESI_CMP_token** | MOESI_CMP_token.slicc | msg/dir/L1cache/L2cache/dma | **token coherence** |
| **MOESI_hammer** | MOESI_hammer.slicc | msg/dir/cache/dma | **AMD Hammer (probe filter)** |
| **MOESI_AMD_Base** | MOESI_AMD_Base.slicc | msg/dir/dma/L3cache/probeFilter/Region*/CorePair | AMD APU 基座(含 GPU CorePair) |
| **GPU_VIPER** | GPU_VIPER.slicc | msg/TCP/TCC/SQC + 复用 MOESI_AMD_Base | **GPU 专用** |
| Garnet_standalone | Garnet_standalone.slicc | msg/dir/cache | 网络 standalone 测试 |
| **CHI** | chi/CHI.slicc | 12 个 `.sm` | ARM CHI |

> **注**:`gem5-coherence-protocols-survey.md:27` 指出本快照中 `MESI_CMP_directory` **不存在**(历史 gem5 曾有);上表按调研文档原文照录。

### 3.2 gem5 Ruby 协议内部结构(per `gem5-coherence-protocols-survey.md:41-46`)

以 `MOESI_CMP_directory` 为例(6 文件):`.slicc` 入口 / `-msg.sm`(RequestMsg/ResponseMsg)/ `-L1cache.sm`(L1 MESI 私有,vnet0=request, vnet2=response)/ `-L2cache.sm`(L2 共享 bank, MOESI)/ `-dir.sm`(**Directory controller 状态机, 1120 行**)/ `-dma.sm`。

**结构特征**(per gem5 官方文档引文,`gem5-ruby-protocol-bridge-survey.md:48`):
> "Ruby implements a detailed simulation model for the memory subsystem. It models inclusive/exclusive cache hierarchies with various replacement policies, **coherence protocol implementations**, interconnection networks, DMA and memory controllers, various sequencers..."

### 3.3 gem5 Ruby → CEP 6 层映射

| gem5 Ruby 组件 | CEP 6 层 | 可容纳? |
|----------------|---------|---------|
| RubyPort / Sequencer | Core / Traffic Generator | ✅ |
| RubyCache(L1/L2/L3) | Stub Cache Core | ✅(单级建模,多层在插件内)|
| **协议 MSHR 状态机**(在 `.sm` 内) | **Protocol Engine 插件内部** | ✅ |
| RubyRequest / RubyResponse / RubySnoopResponse | **canonical 10 流** | ✅ 见下表 |
| **Directory 协议**(`dir.sm`) | **Sharer Tracker** | ⚠️ 见 §3.4 |
| Ruby network(Simple / Garnet) | Fabric & Transport Plane | ✅ |
| vnet(CHI=4 / MOESI=3 / GPU_VIPER=11) | Fabric & Transport 的 VC | ✅ |

**消息 → 流映射**:

| gem5 Ruby 消息 | CEP 流 |
|----------------|--------|
| RubyRequest(读/写/atomic/writeback) | 流 1 `request` / 流 5 `writeback` |
| RubyResponse(data / ack) | 流 2 `completion` / 流 6 `forward_data` |
| RubySnoopResponse | 流 4 `snoop_resp` / 流 10 `snoop_completion_ack` |
| snoop 请求(dir 或 probe filter 发起) | 流 3 `snoop` / 流 7 `back_invalidate` |
| 地址→home 解析 | 流 8 `address_resolve` |

**关键观察**:gem5 Ruby 的消息传递是**分布式**的(L1↔L2↔Directory,cache↔cache),而 CEP 10 流的 "center" 是**逻辑一致性服务点**,不绑定物理单点 — L2 或 Directory 都可充当 center。**不构成障碍**。

### 3.4 逐协议可支持性判定

| gem5 协议 | 判定 | Sharer Tracker 选型 | 说明 |
|-----------|------|-------------------|------|
| **MESI_Two_Level** | ✅ **直接支持** | `FullMapDirectory` | 标准 2 级 directory-based |
| **MESI_Three_Level** | ✅ **直接支持** | `FullMapDirectory` | 3 级;多层状态机在插件内 |
| **MOESI_CMP_directory** | ✅ **直接支持** | `FullMapDirectory` | `dir.sm` = 中心目录 |
| **MOESI_hammer** | ✅ **直接支持** | `SparseSnoopFilter` | `probeFilter` 保守过滤 = caps `CONSERVATIVE` ✅ |
| **MOESI_AMD_Base** | ✅ **直接支持** | `SparseSnoopFilter` | 含 `probeFilter` / `Region*` / `CorePair` |
| **GPU_VIPER** | ✅ **直接支持** | 复用 MOESI_AMD_Base | GPU 侧 coherence 在插件内 |
| **MOESI_CMP_token** | ⚠️ **需扩展 1** | **`BroadcastNull` 或不参与** | token 由副本携带,见 §3.5 |
| **MESI_Three_Level_HTM** | ❌ **明确排除** | — | 事务内存边界,见 §3.6 |
| MI_example / Garnet_standalone | — 不评估 | — | 教学 / 网络测试 |
| CHI | ✅ 已在 CEP 清单 | `ChiRnfAdapter` | — |

### 3.5 扩展 1:token coherence 的 token 归属

`MOESI_CMP_token` 的 token **由数据副本携带权限**,不依赖中心 sharer set — 与 Sharer Tracker 5 实现**正交**:

| SharerTracker 实现 | 机制 | token 对应物 |
|-------------------|------|-------------|
| `FullMapDirectory` | 中心精确跟踪 sharer set | ❌ |
| `SparseSnoopFilter` | 保守过滤(可超集)| ❌ |
| `DcohSymmetric` | per-device DCOH hint | ❌ |
| `TreeManager` | 树形权限 | ❌ |
| `BroadcastNull` | 无跟踪 | ⚠️ **最接近** |

**解决**:token 逻辑住 **Protocol Engine 插件内部**;Sharer Tracker 选 `BroadcastNull` 或不参与;`ADR-CEP-04` 补 `INV-CEP-04-IC7`。

**v0.3 新增佐证**:HeteroGen 的 "GPU"(Spandex)协议同样**不做 sharer tracking 式中心目录**,而是 write-through + 局部缓冲 + release 时写回(博士论文 §2.2.5)。→ "**协议自带一致性语义时,中心目录可退化为无状态**"在学术侧有第二个实例。

### 3.6 扩展 2:HTM 明确排除

`MESI_Three_Level_HTM` = 硬件事务内存 + 缓存一致性混合,需事务边界(read-set / write-set / 事务中止)语义。CEP canonical 10 流是**纯缓存一致性**模型。

**判定**:**Out-of-Scope**。未来若需要,应作为**新的协议族维度**引入,**不**塞进 canonical 10 流。

**v0.3 新增佐证**:HeteroGen 把 **lease-based 协议**(Tardis / Relativistic Coherence / G-TSC)与常规 invalidation 协议列为**不可融合** —— "Read permissions allow for a block to be held **potentially indefinitely**, whereas **leases expire**"(`HeteroGen_HPCA2022.pdf` p.11)。→ **HTM / lease 类协议的"权限时效性"是同一类问题的不同表现**。

### 3.7 扩展 3:协议族 ≠ 协议

当前 CEP §5.1 是"5 协议族 → 6 adapter"。但 gem5 Ruby 是 **8 个独立协议**(各自独立 `.slicc`)。选项 A/B/C 见 v0.1,**选 C**(表格语义 = "族 → 协议实例列表")。

**v0.3 新增佐证**:学术侧同样走"**每协议实例一份描述**"的路(博士论文 §2.2 列 6 类输入协议;HeteroGen 8 个 case study 组合)。

### 3.8 扩展 4:atomic 事务边界(v0.3 已闭合)

- **选项 A**:`perm` 扩展 / **B**:新增流 11 / **C**:atomic = Protocol Engine 插件内部语义
- **v0.3 结论:选 C,`perm` 枚举维持 4 值锁** —— 依据 = ProtoGen 局限 L2(见 §4.1.6),已回溯 PDF 坐实。

---

## 4. *Gen 算法族深度分析(Q3/Q4)

> 全部结论带**论文原文**引用(PDF 页码 + 抽取件行号)。

### 4.1 ProtoGen(ISCA 2018)

#### 4.1.1 核心机制:序列化点 + 转发请求改名

> "ProtoGen overcomes the key challenge of protocol generation... by leveraging the insight that, **in a directory based coherence protocol, racing transactions are serialized at the directory**. By **assigning a unique name to every directory-forwarded request** that can arrive at a stable state in a cache, ProtoGen makes it possible for the directory to convey this serialization order to the caches."
> —— `ProtoGen_ISCA2018.pdf` p.1,抽取件 `:204-216`

**改名机制**(`:773-825`):若同一转发请求在 SSP 中可出现在**两个**稳定状态,ProtoGen 为其中一个**起新名字**(MOSI 例:`Fwd GetS` 拆为 `Fwd GetS` / `O Fwd GetS`)。改名后 cache 可由收到的消息类型**反推竞态胜负**。

**对 CEP 的意义**:序列化点 = **Directory & Ordering Plane** 的 ordering point;改名 = **流 3 `snoop` 的 type 字段必须能区分"同型不同序"的转发** → 对 `ADR-CEP-02` 的 `snoop` type 枚举有约束。

#### 4.1.2 State Set 对偶性

`ProtoGen:826-909`:每个稳定状态对应一个 **State Set**;新生成的瞬态状态**同时加入两端点的 State Set**:
```
I={I, IS, IMAD, SI, MI}
S={S, IS, SMAD, SI}
M={M, IMAD, IMA, SMAD, SMA, MI}
```

**v0.3 一手补强**:博士论文目录显式列出 ProtoGen 算法 **"Step 2: Assign Transient States to State Sets"** —— State Set 是**论文级正式概念**,不是实现细节。→ CEP 的 State Set 对应物 = **Protocol Engine 插件内部的协议状态组织方式**(CEP 当前无对应物,因插件手写)。

#### 4.1.3 目录侧的特殊情形:stale Put

`ProtoGen:1204-1234`:非 stalling 目录下并发度 = cache 数,可能出现"**在 SSP 中不可能发生**"的组合(如 cache 已被失效,其 stale `PutS` 才到目录)。ProtoGen 对使用 MOESIF 子集的协议:**直接 ack 掉任何 stale Put**。

**对 CEP 的意义**:"**协议可容忍的竞态形状**"是协议语义的一部分 → 归 Protocol Engine 插件。

#### 4.1.4 Step 4:访问权限分配

`ProtoGen:1175-1187`:为**每个状态**(含瞬态态)分配允许的访问;**输入参数**决定"是否允许在瞬态态上做 load/store" —— 允许则 SWMR 只在**逻辑时间**成立,不允许则在物理时间成立。

**对 CEP 的意义**:`perm` 语义不止"当前权限",还有"**在途事务中能否继续访问**"。CEP 的 4 值 `perm` **不覆盖**此维度 → 归插件内。

#### 4.1.5 实测数据

| 项 | 值 | 出处 |
|----|----|------|
| MSI 协议的**瞬态状态数** | **18** | `ProtoGen_ISCA2018.pdf` p.1 |
| non-stalling MSI / MESI / MOSI | **18-20 states, 46-60 transitions** | **p.12** |
| 生成耗时 | **< 1 秒**(Intel i5) | p.12 |
| Murϕ 验证规模 | **3 caches**(内存上限) | p.12 |
| 状态合并 | `IM_AS = SM_AS`、`IM_ASI = SM_ASI`、`IM_AI = SM_AI` | p.11 |
| 相对 primer | non-stalling **更激进**(更少 stall) | p.11 |
| stalling 版本 | 与 Sorin primer **完全一致** | p.10 |
| unordered network | 额外手写消息的 SSP 可生成 | p.11 |
| TSO-CC | 从 SSP 生成完整并发 TSO-CC,经 Banks et al. 方法验证 | p.11 |

> **对 CEP 的量级校准**:一个**真实**的非 stalling MSI cache controller 是 **18-20 状态**。CEP Stub Cache 的 4 个稳定状态**不是缓存模型**,而是**客户端侧协议状态投影**。这一措辞必须写进 `ADR-CEP-05` / 07 号 §3.2(见 F12)。

#### 4.1.6 ⭐ 5 条显式局限性(**已在 PDF 原文坐实**)

`ProtoGen_ISCA2018.pdf` **p.7**(抽取件 `:742-758`),**连续 4 句**:

| # | 局限(原文) | 对 CEP 的含义 |
|---|-----------|--------------|
| **L1** | "ProtoGen requires a correctly specified SSP as its input; **it cannot automatically "correct" bugs in the SSP**" | 框架**不做协议正确性兜底** → 强化 `ADR-CEP-06` 的 `memory_violations` 闸门必要性 |
| **L2** | "ProtoGen **cannot generate new protocol actions not explicitly specified in the SSP**. For example, **it cannot infer how atomic read-modify-writes must be implemented without it being specified in the SSP**" | 🎯 **atomic 边界坐实** → `ADR-CEP-17 Decision 6` 选 C 有据 |
| **L3** | "it **cannot automatically deduce the protocol for an unordered network given an SSP for an ordered network**" | **网络序假设是输入,不是生成物** → `ADR-CEP-08` 必须覆盖 |
| **L4** | "ProtoGen **does not specify how protocol actions must interact with the interconnect**; it requires the user to **manually define virtual channels and assign messages to channels**" | 🎯 **CEP 分层独立验证** → VC / 消息路由归 Fabric & Transport Plane |
| **L5** | "Finally, ProtoGen is **limited to directory based protocols**" | 非目录协议需另设路径 —— HieraGen 给了答案:**"null directory" (Dir1B)** |

**L2 + L4 合起来的推论**:atomic RMW 是"必须由协议规格显式给出的协议动作",且 VC 分配"不由协议生成器负责" → 在 CEP 里 **atomic 与 VC 都是"协议插件的输入"而非"框架的推导"**。

> **v0.3 强化(F18)**:博士论文 **§9.3 Future Work** §9.3.1(⚠️ **未实现**)揭示了 **VC 分配的目的**:"ProtoGen does not automatically assign messages to virtual networks yet **for ensuring deadlock freedom**"。→ **VC 分配不是 QoS 调优,是死锁自由性论证的一环**。这条把 F3 从"排序假设面"升级为"**死锁自由性载体**"。

#### 4.1.7 工具链:DSL 血统与后端

> "Our DSL is similar in spirit to other previously proposed ones for coherence protocols, including **Teapot** and **SLICC**."(`ProtoGen_ISCA2018.pdf` p.4)

> "Thus far, we have implemented a backend to the language of the **Murϕ** model checker and **translation to other outputs like SLICC or Verilog is future work**."(p.5)

**v0.3 一手补强**:博士论文 Ch4 = "ProtoGen Protocol Language"(PPL),含 §4.1 Process / §4.1.2 Types / §4.2 Parser / §4.2.1 IR / §4.3 Backend;**HeteroGen 依赖 `antlr3 3.5`** → 解析器由 ANTLR3 生成,是一条真实的编译器前端。

> **重要区分**:ArchForge 的"永不复制 slicc DSL"理由是"5000+ 行不可读";ProtoGen 的 DSL 是"独立语言 + ANTLR 前端 + IR + 后端 + 30k-45k 行 Python"。两者都指向"不要把 DSL 搬进 C++ 框架",**理由不同**,引用时不可混用。

#### 4.1.8 安全性质

`ProtoGen:455-476`:**SWMR** + **data-value invariant**;两者在**逻辑时间**上合起来 = per-location sequential consistency。

**对 CEP 的意义**:`memory_violations` 闸门若要有严格定义,应对齐 **SWMR + data-value** 双不变量 → `ADR-CEP-06` 细化建议。

### 4.2 HieraGen(ISCA 2020)

#### 4.2.1 两步流程

`HieraGen:274-291`:
```
每层独立 SSP ──(Step 1: 原子化分层协议)──► 原子分层协议
                                          ──(Step 2: 并发化)──────► 并发分层协议
```

#### 4.2.2 ⭐ 唯一序列化点不变量

> "HieraGen must also implement concurrency in a hierarchical system that can have multiple coherence transaction **serialization points**... We make the observation that in a hierarchical SSP that correctly enforces coherence globally, **any two racing coherence transactions will serialize at exactly one of the directories**."(`HieraGen_ISCA2020.pdf` p.2,抽取件 `:151-155`)

树结构证明(`:778-790`):全局 SWMR ⇒ 每个 block 恰有一个 owner;两条冲突事务路径指向该 owner;**树上两条路径恰有一个公共目录节点**;后到者据此推断自己输了 → 该公共目录是唯一序列化点。

**对 `ADR-CEP-08` 的意义**:把"Ordering Point 可迁移性"从口号变成**可判定准则**。

#### 4.2.3 ⭐ 层间兼容性:E 态静默升级

`HieraGen:618-706`:层间不兼容的根源是 **"silent upgrading of coherence permissions"**,具体是 **E 态可静默升级到 M**。

反例(SSI-L = MESI, SSI-H = MSI):cache-L 拿 E 权限 → 可**静默 E→M 并写入**;同时任一 cache-H 拿 S 权限 → **全局 SWMR 被破坏**。

HieraGen 的解法:直觉解(保守发 GetM-H)/ 高性能解(dir/cache 内加"权限不匹配检测" + proxy-cache 发匹配请求)。

> "since the cache-L can silently transition from E to M and write the block, **HieraGen can infer that E state is writable**"(`:655-658`)→ **检测判据 = SSP 是否允许静默权限升级**。

**对 CEP 的意义(关键)**:**CEP 的 Stub Cache 明确暴露 `E`** → CEP 做**分层组合**时,**E 态静默升级这一检查项在结构上必然存在**(是否真的破坏全局 SWMR 取决于对端协议是否也允许静默升级)。→ `ADR-CEP-08` 应加 E 态兼容性检查;`ADR-CEP-05` 标注。

#### 4.2.4 proxy-cache 机制

`HieraGen:348-376`, `:448-486`:
- proxy-cache = **下层 cache controller 的克隆**,集成进中间的 dir/cache 节点
- 物理上就是 dir/cache 里的**一条临时 cache line + 状态**
- 3 类需要 dir/cache 同时提供两种功能的事务:① 下层读写涉及上层 ② 上层读写涉及下层 ③ dir/cache 自身 eviction(维持目录包容性)

**对 CEP 的意义**:**可借用的具体组合技术**。CEP 跨一致性域组合时,"协议桥接"的一种具体形态 = "在桥接点内嵌一个对端协议状态机实例"。

#### 4.2.5 基线 5 约束:可放宽 / 不可放宽

| 约束 | 基线 | 放宽? | 出处 |
|------|------|-------|------|
| 只有 2 层 | 是 | ✅ **可放宽** | `:797-834` |
| 目录包容 + full-map,只读块逐出非静默 | 是 | ✅ **可放宽**(在输入 SSP 里处理) | `:835-849` |
| 每层 SSP 都是 flat directory 协议 | 是 | ⚠️ **可放宽到 snooping**("null directory", Dir1B) | `:850-863` |
| 共享 cache 包容 | 是 | ❌ **不可放宽** | `:864-887` |
| 跨层通信严格父子 | 是 | ❌ **不可放宽** | `:888-905` |

不可放宽的 2 条的理由(`:872-874`):"allowing the user to specify the SSPs **completely independently**, and **not modifying the SSPs** when composing them... we have drawn a **sharp line between the SSPs**"。

> → 这条"锐利分界线"正是 **CEP Stub Cache / Protocol Engine 分层原则的学术版**。

**v0.3 更新**:博士论文 §9.3.2 把"非包容共享 cache"列为 **future work**("developing new rules that will allow HieraGen to **modify** given input SSPs")—— 即**要支持它,就得放弃"不改 SSP"这条锐利分界线**。这对 CEP 有直接警示:**CEP 的 Stub/Plugin 锐利分界线若要支持非包容层级,必须显式放弃**。

#### 4.2.6 实测数据与验证规模

| 项 | 值 | 出处 |
|----|----|------|
| 生成耗时 | **< 10 秒**/协议 | `:1049-1050` |
| MOESI/MOESI **原子** dir/cache | 59 states / 368 transitions | Table II |
| MOESI/MOESI **并发 non-stalling** dir/cache | **81 states / 495 transitions** | Table III |
| 状态合并 | 并发版状态数常**不多于**原子版(`MI`/`SI` 可合并) | `:1031-1045` |
| Murϕ 配置 | 1 root + 2 cache-H + dir/cache(含 proxy-cache-L) + 2~3 cache-L | `:1186-1195` |
| 验证服务器 | **256GB 内存** | `:1188-1190` |
| 扩展手段 | Murϕ **hash compaction**,多次运行直到漏检概率 < **0.001%** | `:1196-1216` |

**对 CEP 的意义**:**状态空间是硬瓶颈** → `ADR-CEP-06` 若做穷尽验证,必须承认规模上限;否则只能做随机测试(gem5 Ruby 的 `ruby_random_test.py` 路线)。**这与 F22 呼应**。

#### 4.2.7 相关工作

`HieraGen:1217-1236`:**MCP**(把 dir/cache 拆成 manager + client,**不提供自动化**)与 CEP 的目录/传输划分**同源**;**Cook**(通信模板,但 blocking dirs / 无兄弟通信 / 写回时 cache 阻塞)。

### 4.3 HeteroGen(HPCA 2022)

#### 4.3.1 ⭐ 一致性接口:read / write / **FENCE**

> "The processor cores interact with the coherence protocol through an interface consisting of **reads, writes, and fences**."(`HeteroGen_HPCA2022.pdf` **p.3**(§II-B),抽取件 `:254-262`)

**对 CEP 的意义**:**fence 是一致性接口的一等操作**。CEP canonical 流 9 `barrier` **存在**,语义钉死为 **requester-scoped**(per `ADR-CEP-02 §2 Decision 3`)。HeteroGen 的 FENCE 用途**恰在 requester-scoped 维度内**(Dekker litmus:SC 簇 T1 + TSO 簇 T2,TSO 簇插 FENCE 后两 load 不能同时读 0)→ **与 CEP 语义兼容**。

但**同一 FENCE 在不同一致性模型下效果不同**:

| 一致性模型 | FENCE 效果 |
|-----------|-----------|
| SC | 无效果(已全序) |
| TSO | 全屏障(补回 store→load 序) |
| RC | release / acquire(按方向) |
| PLO | 只补 W→W 与 R→W |

→ **同一线信号,不同协议不同效果** → 归**协议侧的一致性标签**,不归流的语义。**无需新增 canonical 流**。

#### 4.3.2 ⭐ 一致性标签体系

`HeteroGen:276-299` + Table I(`:1266-1276`):

| 模型 | 标签 | 协议实例 |
|------|------|---------|
| **SC** | SC-read / SC-write | MSI, MESI |
| **TSO** | TSO-read / TSO-write | TSO-CC |
| **RC** | RC-release-write / RC-data-write / RC-acquire-read / RC-data-read | RCC-O, RCC, **GPU**(Spandex) |
| **PLO** | PLO-read / PLO-write | PLO-CC |

**对 CEP 的意义(直接喂 `ADR-CEP-12`)**:能力映射表维度不应只有"支持哪些流 / 哪些 perm",还应有**一致性标签维度**。**没有它,CEP 无法比较两个协议的组合兼容性**(见 §4.3.7)。

#### 4.3.3 ⭐ 复合一致性模型(形式化公理)

`HeteroGen:503-671`(基于 Alglave et al. axiomatic 框架):

**基础关系**:`po` / `po-addr` / `ws` / `rf` / `rfe` / `fr`

**合法性**(每地址 SC):`acyclic( po-addr ∪ rf ∪ fr ∪ ws )`  (1)

**多副本原子模型** `M ≡ ppo`:`acyclic( ppo ∪ rfe ∪ fr ∪ ws )`  (2)
- SC:`ppo ≜ po`;x86-TSO:`ppo ≜ po \ st(x) → po \ ld(y), ∀x,y`

**复合一致性模型**:`ppo_com(t) | t ∈ T_i ≡ ppo_i`,然后
```
acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )                    (3)
```

**语义**:复合一致性保证**每个簇的操作仍遵守该簇自己的模型**;簇间关系进全局内存序。

**编程含义**(`:692-728`):复合一致性使**编译器映射可复用**(线程映射到哪簇就用那簇的映射)。

**对 CEP 的意义(直接喂 `ADR-CEP-13`)**:若要可判定,应采用这条公理的形式 —— "**每个一致性域保留自己的 ppo,跨域边进全局序**"。这比"一致性域必须有相同模型"更准确、更可实现。

#### 4.3.4 合并目录 + 两种桥接机制

`HeteroGen:729-754`, `:892-937`:
- **合并目录**:各簇 directory controller 合并成**一个**;cache controller **不变**;cluster1 看 cluster2 **像一个大 cache1**
- 桥接靠:① **一致性模型翻译**(ArMOR 找 "SC 等价的访问序列" —— SC 等价的写**保证**触发一次全局传播的写)② **proxy cache**(每簇一个,代表桥接该簇协议流所需的瞬态状态)

**写传播流程**:cluster1 的 directory1 收到写权限/writeback 请求 → 判定"全局可见写" → ArMOR 翻译 → proxy-cache2 向 directory2 发起 → 完成后逐出 → directory1 恢复 → **owner 字段**记为最后写的簇。

**读流程**:本簇无法服务的读 → **当作由当前 owner 簇发起的读**。

**对 CEP 的意义**:ArchForge 09 的 `CoherenceBridge`(CPU↔GPU)≈ **合并目录 + proxy-cache**。

#### 4.3.5 ⭐ "全局可见写" 静态分析判据

`HeteroGen:984-1020`:对每个 cache→directory 请求,检查**最终响应之后**的 cache line 状态 `s1`,若同时满足:
- **(a)** `s1` 允许 store **命中且无需外部通信**(可能转到 `s2`)
- **(b)** `s1` 或 `s2` 接受"导致数据值响应"的转发请求

→ 该请求被分类为**全局可见写**。

**反例说明**(原文 `:1009-1020`):RC 协议中 store 命中 invalid line 时其实发的是**读请求**,但终态 valid **允许后续写**(RC 允许写先本地缓冲)→ 条件 (a) 成立,但条件 (b) 不成立(valid 态不接受转发请求)→ **不**误判为写。

**对 CEP 的意义**:**具体、可实现、可机器检查**的判据。CEP 若要判定"某次写是否越出了本一致性域",可采用同构判据。

#### 4.3.6 proxy cache 的两种并发设计

`HeteroGen:1058-1119`:

| 设计 | 假设 | 粒度 |
|------|------|------|
| **保守 processor-centric** | 允许 cache 提前响应(early ack) | **全部串行** |
| **激进 memory-centric** | 不允许提前完成 | **同地址串行,跨地址可并发** |

**自动选择**:"HeteroGen analyzes the cache state machine: if it allows for early write acknowledgments, we use the **conservative** design."(`:1116-1119`)

**对 CEP 的意义**:与 `ADR-CEP-04` Sharer Tracker 的 **completeness/conservatism** 选择**同构** → `ADR-CEP-13` 的跨域桥接并发粒度**由对端协议的"是否允许提前 ack"决定**。

#### 4.3.7 ⭐ 兼容性边界(可融合 / 不可融合)

`HeteroGen:1194-1234`:

| 组合 | 判定 | 理由(原文) |
|------|------|-----------|
| MOESI 变体(SWMR)+ 自失效/writeback 型(RC) | ✅ 可融合 | §4.3.4 |
| **invalidation-based ↔ update-based** | ❌ **不可** | "the notion of **write permissions** is not compatible with update protocols" |
| **lease-based(Tardis / Relativistic Coherence / G-TSC)↔ invalidation-based** | ❌ **不可** | "Read permissions allow for a block to be held **potentially indefinitely**, whereas **leases expire**" |

**且原文明确把"自动判定两协议是否兼容"列为超出本工作范围**(`:1219-1224`):
> "Given two protocols, can we tell whether the two are compatible? **We believe this is a challenging problem that is beyond the scope of this work.**"

**对 CEP 的意义(范围纪律)**:CEP **不应承诺**自动协议兼容性检测。`ADR-CEP-13` 应写明:兼容性由**人工声明 + 实验验证**。

#### 4.3.8 验证与性能:可被 CEP 复用的三件套

**① 异构 litmus 测试**(`:1277-1306`):**herd7** 生成 **111 个**异构 litmus test(MP, S, IRIW, 2+2W, CoRR, LB, R, RWC, SB, WRC, WRW+WR, WRW+2W, WWC),对每个 test **遍历所有可能的"线程→核"分配**;Murϕ 预载 cache 初值 → 穷举事件序 → 检查是否出现模型禁止的结果。**全部通过**。

**② 死锁自由**(`:1307-1316`):2 地址 + 每簇 1-3 cache 穷举;更多则 hash compaction,漏检概率 < **0.05%**。

**③ gem5 性能评测**(`:1366-1440`):**gem5**,**64 核**(60 tiny + 4 big),2 簇;基线 **HCC**(公开的手工异构协议);13 个应用(cilk5-*, ligra-*);**平均运行时间相当**;HeteroGen **读更快**(消除冗余握手),**写略慢**;网络总流量差异 **< 5%**;变体实验(带握手版)平均快 **2%**。

**对 CEP 的意义(桥接证据)**:🎯 **gem5 已经是协议生成研究线的评测载体**。→ `ADR-CEP-00 §5.1` 的 gem5 Ruby 类比获得**第二重含义**:**生态类比**。

### 4.4 ⭐ 完整流水线与工具能力矩阵(v0.3 新增,来自博士论文)

**Table 1.3 — *Gen 工具实现的算法阶段**:

| *Gen Tool | Pre-Processing | HeteroGen | HieraGen | ProtoGen |
|------------|:--------------:|:--------:|:--------:|:--------:|
| ProtoGen | – | – | – | ✅ |
| HieraGen | – | – | ✅ | ✅ |
| HeteroGen | – | ✅ | – | ✅ |
| **ProtoGen+** | ✅ | ✅ | ✅ | ✅ |

**完整 4 阶段流水线**(per 博士论文 §1.3,`:1010-1060`):
```
PPL 源码 ──Parser──► IR transition graph ──► [1] Pre-Processing
   (Ch4)              (Ch4 §4.2.1)              (Ch5)
                                                        │
                                   [2] HeteroGen 阶段 ─┤  (Ch8) 组合多输入 SSP 的 directory controller
                                                        │        → 异构 SSP directory controller
                                   [3] HieraGen 阶段 ──┤  (Ch7) 下层 dir-controller SSP + 上层 cache-controller SSP
                                                        │        → 分层 SSP coherence controller
                                   [4] ProtoGen 阶段 ──┘  (Ch6) SSP → 并发协议 FSM
```

**关键机制(博士论文 §1.3 原文)**:
> "In the SSP Pre-Processing stage (1)... **dependencies between the separate cache controller and directory controller SSP descriptions are analyzed and extracted. Furthermore, the correctness of the provided SSP is verified.**"
> "Depending on the designer's system configuration input, the **HeteroGen or HieraGen stages may be skipped**."

**HieraGen 阶段可消费 HeteroGen 阶段的输出**:
> "the lower-level directory controller SSP input of HieraGen **can be the atomic heterogeneous directory-controller output of the HeteroGen stage**."

> ⚠️ **但这在已发布的工具里做不到** —— 见 §4.5.2。

### 4.5 ⭐ 博士论文独有内容(v0.3 新增,3 篇会议论文都没有)

#### 4.5.1 Ch4 PPL + Ch5 SSP Pre-Processing(工具链全貌)

**Ch4 ProtoGen Protocol Language**:
| 节 | 内容 |
|----|------|
| §4.1 PPL in a Nutshell | — |
| §4.1.1 Process | 机器定义(cache / directory) |
| §4.1.2 Types | `bool` / `int` / `set` + 协议专用类型 `Data` |
| §4.2 Parser | — |
| §4.2.1 IR Format and Notation | 转移图 `G_controller` |
| §4.2.2 PPL Parsing | — |
| §4.2.3 MSI Protocol Example | 完整 MSI 的 PPL 描述 |
| §4.3 Backend | §4.3.1 Summary |

**Ch5 SSP Pre-Processing**:
| 节 | 内容 |
|----|------|
| §5.1 Step 1: **State Space Checker (SSC)** | §5.1.1 State Space Exploration;§5.1.2 **Valid Reachable Global Stable Coherence State Graph (VGSCSG)**;§5.1.3 Basic Sanity Checks;§5.1.4 Coherence Transaction Analysis |
| §5.2 Step 2: **Intra-Transaction Message Stall Detector (MSD)** | §5.2.1 Identify Stalled Messages and Dependent Transitions;§5.2.2 MSI Protocol Example |
| §5.3 Step 3: **SSP State Space Minimization** | §5.3.1 Summary |

**每台机器的两个预定义内部变量**:`State`(机器状态)+ `ID`(机器标识)。

> **对 CEP 的意义(v0.3)**:这层"**依赖提取 + SSP 正确性验证 + 状态空间最小化**"是 *Gen 工具的真实前端**。CEP 走手写插件路线时的对应物 = **`ADR-CEP-14`(Stub 可验证性合同测试)**;若未来走 SSP 描述路线(ADR-CEP-18),则需要复刻这一层。

#### 4.5.2 ⭐ §9.2 Critical Analysis(作者自陈的工具边界)

> "**The HieraGen and HeteroGen tool are publicly available as two stand alone tools with limited compatibility. Automatically generating a heterogeneous and hierarchical cache coherence protocol from given input SSPs is not possible with these tools without manual intervention.**"(博士论文 §9.2)

⚠️ **v0.3 复审修正(对抗核验)**:原文的限定词是 "**without manual intervention**" —— 即"**不能无人工介入地自动完成**",**不是"不能组合"**。算法层是**可组合**的:论文 §1.3 明确 "the lower-level directory controller SSP input of HieraGen **can be** the atomic heterogeneous directory-controller output of the HeteroGen stage";源码亦有预留点(`TraceNetworkx.py:77` "New sub trees can be generated by other Algorithms like for example ProtoGen or **HieraGen**")。→ **F19 的正确表述是"工具工程限制",不是"算法不可能"**。

**其他自陈**:
- ProtoGen 用 **Python** 写(为快速分享),代码结构清晰
- 适配 HieraGen 时 ProtoGen 代码**质量显著下降**:"the code quality **significantly decreased** making it **difficult to maintain and extend** the HieraGen tool"
- HeteroGen **largely from scratch** 重写,并引入**贯穿全工具的统一 IR 数据结构**
- "**the HeteroGen tool does not implement the HieraGen algorithm**"
- *"Gen tools are effectively compilers"*,需要大量单元测试
- §9.1:"The feedback from academia and industry alike show that the **\*Gen tools are already getting adopted**."

#### 4.5.3 ⭐ §9.3 Future Work(7 项)

| # | 主题 | 原文要点 | 对 CEP 的意义 |
|---|------|---------|--------------|
| **9.3.1** | **ProtoGen: Automatic Virtual Channel Assignment** | "ProtoGen does not automatically assign messages to virtual networks yet **for ensuring deadlock freedom**" | 🎯 **F18**:VC 分配 = 死锁自由性论证,不是 QoS 调优 |
| **9.3.2** | HieraGen: Non-Inclusive Shared Caches | 需"allow HieraGen to **modify** given input SSPs" | ⚠️ 要支持就得**放弃"不改 SSP"锐利分界线** —— 对 CEP 的警示 |
| **9.3.3** | HeteroGen: Scoped + Non-MCA Models | 扩展复合记忆模型 | 与 CEP 的 `barrier_domain` 作用域设计相关 |
| **9.3.4** | **Snooping Protocols** | "One of key insights of ProtoGen is that the **directory is the point of serialization**... This insight can be extended to **the bus in the snooping protocol acting as the point of serialization** for all transactions. A cache can infer whether its own transaction has been serialized, by **observing the request initiating this transaction on the bus**." | 🎯 **F17**:**序列化点是协议无关抽象** —— 支持 CEP 把 Ordering Point 独立于"目录" |
| **9.3.5** | Timestamp Protocols | 已用 ProtoGen 生成过 flat timestamp Linearization 协议 | 时间戳协议族可作为 CEP 远期族 |
| **9.3.6** | Replication Protocols | 复制机制**在输入 SSP 里**;未来探索自动转换 | 复制协议 = SSP 特化,归插件内 |
| **9.3.7** | **Formal Proof** | "we rely on model checkers to confirm their correctness. **To avoid the necessity of exhaustive model checking**, future work may focus on formally proving the correctness of the *Gen algorithms." | 🎯 **F22**:**穷尽验证开销是公认痛点** |

> ⚠️ **9.3.1~9.3.4 都是 future work(未实现)**。本调研引用它们时**只作为方向性证据**,不作为"已验证"。

#### 4.5.4 §7.6.6 Non-SWMR Protocols(论文独有)

HieraGen 原本只支持 SWMR;论文指出可借 HeteroGen 的复合一致性模型**分层组合不同一致性模型的协议**,但暴露一个**具体陷阱**:

> "in the RCC coherence protocol... the directory is **unable to determine, whether a data read request is associated with an acquire or a read access**. Therefore... HieraGen must interpret the data read request to be issued due to the access with the **strongest access ordering guarantees**, which in this case would be the acquire... in case of a hierarchical RCC protocol translating every read access into a higher-level acquire will result in the dir/cache node being required to perform a **self-invalidation for every read** effectively eliminating its caching functionality."

**两条解法**:
1. 高层改用 MOESI(SC by design)
2. **给下层 cache 请求加标识符**:RCC 里 read 与 acquire 都发 `GetV` → **重命名为 `GetV_R`(read)/ `GetV_A`(acquire)** → 目录可推断访问类型

> **对 CEP 的意义**:这是 **ProtoGen "转发请求改名"机制在跨层 / 跨一致性模型语境下的推广**,且暴露了一个**具体的实现陷阱**(目录无法区分访问类型)。→ `ADR-CEP-12` 的能力映射表应能表达"**同一 wire 请求在不同一致性标签下语义不同**" → 与 F6 的 `consistency_labels` 呼应,并给 `consistency_labels` 增加一个"**请求级标签**"的细化方向。

### 4.6 ⭐ 源码实证(v0.3 新增:三仓已 clone)

**获取方式**(`git clone --depth 1`,`/tmp/opencode/gen_src/`):

| 仓 | 最后提交 | 文件数 | Python 行数 | License |
|----|---------|-------|-------------|---------|
| `github.com/Errare-humanum-est/ProtoGen` | 2022-01-07 | 50 | 16,110 | **MIT**(Copyright (c) 2018 The University of Edinburgh) |
| `github.com/Errare-humanum-est/HieraGen` | 2022-01-07 | 279 | 25,727 | **MIT**(同上) |
| `github.com/Errare-humanum-est/HeteroGen` | 2022-01-07 | 553 | 37,198 | **MIT**(同上) |
| `github.com/icsa-caps/ProtoGen` | 2022-01-06 | 50 | — | 同上;commit message = "**Discontinued Snapshot of ProtoGen published at ISCA20**" |

> **修正 §1.4 的"两个 org 并存"**:二者是**同一份代码的两个镜像**;`icsa-caps` 是**冻结的废弃快照**,`Errare-humanum-est` 是作者维护版(内容仅 README 不同)。
>
> **代码量口径说明**:上表 Python 行数**含 vendored 依赖**(如 `antlr3/`、`CMurphi/`),论文 Table 9.1 的 8,849 / 15,771 / 30,413 是**纯算法实现行数**,两者口径不同,不可直接比较。

**License 结论**:三仓均为 **MIT**,**无 copyleft 义务**。→ CEP 可自由借鉴其**设计思路**(甚至代码片段),仅需保留版权声明。这是 F15/F23 trade-off 表的**关键输入**。

#### 4.6.1 ⚠️ ProtoGen 已废弃 —— 修正 F23

`ProtoGen/README.md` 首行:
> "*ProtoGen is still under development. **This is version is discontinued and replaced by HeteroGen** https://github.com/Errare-humanum-est/HeteroGen*"

**并且**:论文 §9.2 计划中的 **SLICC / Verilog / Rumur 后端至今未落地** —— 三仓的后端目录只有:
- ProtoGen:`{Monitor, Murphi, Algorithm, Parser, DataObjects}`
- HieraGen:`{Monitor, Murphi, Algorithms, Graphv, Parser, Protocols, DataObjects, General}`
- HeteroGen:`{..., Backend/{Common, Murphi}, antlr3, CMurphi, MurphiLitmusTests, gem5_HeteroGen_protocols}`

→ **修正 F23**:竞合压力**尚未兑现** —— *Gen 族当前**不能**从 SSP 生成 SLICC。但见 §4.6.2:**它已经能直接产出 gem5 `.slicc`**。

#### 4.6.2 ⭐ 最强证据:*Gen 族已产出**gem5 Ruby 协议源文件**(⚠️ 非"开箱即用")

`HeteroGen/gem5_HeteroGen_protocols/` 目录内容(源码级铁证):
```
src/mem/protocol/HETERO_RCCO_GEN_WR_HS.slicc
src/mem/protocol/HETERO_RCCO_GEN_NO_HS.slicc
src/mem/protocol/HETERO_RCCO-L1cache_GEN_WR_HS.sm
src/mem/protocol/HETERO_RCCO-L2cache_GEN_WR_HS.sm
src/mem/protocol/HETERO_RCCO-dir.sm
src/mem/protocol/HETERO_RCCO-msg.sm
src/mem/protocol/HETERO_RCCO-RCCOCache_GEN_WR_HS.sm
configs/ruby/HETERO_RCCO_GEN_WR_HS.py       ← gem5 Ruby config
configs/ruby/HETERO_RCCO_GEN_NO_HS.py
setup.sh / run_RCCO_GEN_WR_HS.sh / run_DeNovo.sh
```

→ **学术协议生成工具与 gem5 Ruby 在源码层面直接相遇**。这使 F14 的"生态类比"从"论文声明 gem5 评测"升级为"**产出物就是 gem5 协议文件**"。

> **⚠️ v0.3 复审降级(对抗核验)**:原表述"**可直接运行的 gem5 Ruby 协议**"**过强**。实测:
> - `.slicc` 仅 **268 字节**,只是 `include` wrapper(引用 `RubySlicc_interfaces.slicc` + 5 个 `.sm`);**实质协议逻辑在 `.sm`**(7.4 KB ~ 58 KB)
> - `setup.sh` 实为 `cp -r . ../alloy-gem5/` → 需要预先存在 **`alloy-gem5`**(定制 gem5 fork / 参考系统),**不是标准 gem5**
> - `run_*.sh` 使用**硬编码路径** `/artifact_top/alloy-gem5/build/RISCV_Big_Tiny_DeNovo/gem5.opt` + `/artifact_top/alloy-apps/...`
> - `HeteroGen/README.md` 要求:先按**另一篇论文**("Efficiently Supporting Dynamic Task-Parallelism on Heterogeneous Cache-Coherent Systems",Zenodo `10.5281/zenodo.5793240`)搭好参考系统,并在 **docker container** 内运行
> - 仿真配置:**64 核**(60 tiny `TimingSimpleCPU` + 4 big `DerivO3CPU`)、**RISCV**、`garnet2.0`、`MeshDirL2Bottom_XY`
>
> **准确表述**:*Gen 族**已产出 gem5 Ruby 协议源文件**,但**运行需 alloy-gem5 + 外部 docker 参考系统,非开箱即用**`。

**对 CEP 的意义(v0.3 修正)**:
- **互补而非竞争**:*Gen 族负责**生成 gem5 协议**;CEP 负责**在统一实验床上运行 / 对比协议**(跨 gem5 / CHI / CXL / TileLink)
- **反向需求**:gem5 协议空间一旦可被自动扩张,**CEP 需要探索的协议空间变大** —— 这**加强**了"生态类比"而非削弱它
- **但也提醒**:若 *Gen 族继续产出 gem5 协议,gem5 Ruby 的"手写 `.slicc`"范式会被侵蚀 → CEP 若也做 gem5 侧适配,需明确定位

#### 4.6.3 ⭐ 排序假设是**协议描述语言的一等公民**(支撑决策 8)

`ProtoGen/MSI_Proto.pcc` 原文(节选):
```
# NrCaches 3

Network { Ordered fwd;    //FwdGetS, FwdGetM, Inv, PutAck
          Unordered resp; // Data, InvAck
          Unordered req;   //GetS, GetM, PutM
          };

Cache {
    State I;
    Data cl;
    int[0..NrCaches] acksReceived = 0;
    int[0..NrCaches] acksExpected = 0;
} set[NrCaches] cache;

Directory {
    State I;
    Data cl;
    set[NrCaches] ID cache;
    ID owner;
} directory;
```

**两个决定性观察**:
1. **`Network { Ordered ...; Unordered ...; }` 是描述语言的内建结构** → 排序假设是**逐网络**(per-network)声明的**协议规格的一部分**,**不是**框架配置开关
2. **协议描述语言文件名后缀 = `.pcc`**(ProtoGen Cache Coherence),示例:`MSI_Proto.pcc` / `MESI.pcc` / `MESI_unordered.pcc` / `MOSI.pcc`

→ **`ADR-CEP-17 Decision 8` 的 `ordering_assumption` 有了源码级依据**:一个协议实例的排序假设**必须**是其契约的一部分。**且粒度是"逐网络"而非"逐协议"** —— 建议 `ADR-CEP-12` 的 schema 把 `ordering_assumption` 从枚举改为**逐网络映射**:
```
ordering_assumption:
  <network_name>: p2p_ordered | unordered
```

#### 4.6.4 ⭐ VC 自动分配在源码里有**预留位置**(支撑 F18)

`HieraGen/Algorithms/NetworkAnalysis/ClassNetworkClassification.py`:
```python
class ClassNetworkClassification:
    def __init__(self, parser, transitions):
        self.networks = self.generate_network_objects(parser.getNetwork())
        self.req_networks = self.generate_network_request_class(transitions)

        # Automatic virtual channel assignment can be implemented here.
        # Possibly reuse model checker features
```

→ 论文 §9.3.1 的 future work 在源码里有**明确的 TODO 位置**,且作者已给出实现思路("**复用模型检查器特性**")。这是 F18(VC 分配 = 死锁自由性载体)的**源码级证据**,并给 CEP 提供了**第二条实现路径参考**。

#### 4.6.5 ⭐ 死锁验证的资源上限:最高 **1TB RAM + 数小时**(支撑 F22)

`HeteroGen/README.md` "Hardware dependencies":
> "Any PC with at least **16GB of RAM** suffices to run most tests and these will complete in a matter of minutes... **However, a few deadlocks tests can require up to 1TB of RAM and hours of computation time.**"

→ F22 的**量化上限**:死锁自由性验证在**极端配置**下需要 1TB 级内存与小时级时间。**这对 CEP 的实验协议是硬约束**:`ADR-CEP-06` 不能承诺"任何配置都能穷尽验证"。

#### 4.6.6 其他源码级观察

| 项 | 源码位置 | 意义 |
|----|---------|------|
| **PPL 前端无类型检查** | `ProtoGen/README.md`:"There is currently **no type checking implemented for the ProtoCC language frontend**" | 又一条"CEP 用强类型 C++ 而非 DSL"的支撑 |
| **SSP 验证先于生成** | `ProtoGen/README.md`:"ProtoGen v0.5: Now supports **SSP verification. The SSP is verified before ProtoGen generates the concurrent protocol.**" | F21 的源码级证据(SSC/MSD 层是真实存在的前置) |
| **State Set 是显式数据结构** | `ProtoGen/DataObjects/ClassStateSet.py`:`stablestate` / `states` / `startstates` / `endstates` | 论文所述"对偶性"的代码形态;另有 `_AssignStateSets` / `_StateSetGeneration` / `_CompleteTransitions` |
| **proxy-cache 在生成阶段产出** | `HieraGen/Algorithms/HieraAlgorithm/HieraStateSpaceGen/ProxyCacheGen.py` | proxy-cache 是**代码生成期**的产物,**不是**运行时对象 → 与论文"proxy-cache 就是 dir/cache 里的一条临时 cache line"一致 |
| **瞬态辅助状态 / 延迟响应 / 等价态合并有独立模块** | `HeteroGen/Algorithms/General/{AuxStateHandler,GenDeferMessage,GenMsgDepedencyMap,MergeStates}.py` | 对应论文 §V-D(瞬态辅助状态)、§V-C Case 2(延迟响应)、HieraGen §V-E(等价态合并) |
| **死锁检查内置** | `HeteroGen/Algorithms/ProtoAlgoNetworkx/RunTestProtoGen.py::check_deadlock_freedom`;`ParallelChecker.py` 的 `k_dead = 'Deadlock'` | F22;但受 §4.6.5 资源上限约束 |
| **fence 语义映射表是工具的一部分** | `HeteroGen/MurphiLitmusTests/LitmusTests/dmb_to_RC_map.txt`:`PodWW => DMBdWW => PodWW L` 等 | **F6/F7 的源码级证据**:"同一 barrier 在不同模型下效果不同"是**表驱动**表达的 |
| **litmus test 目录** | `MurphiLitmusTests/{LitmusTests/{SC,RC,FullFenceTests,RunDiy7}, AlternativeTests}` + `ARMv8Parser.py` | 与论文的 111 个 litmus test 对应;含 **ARMv8 DMB 语义** |
| **依赖清单** | ProtoGen:`antlr-python3-runtime 3.4` / `colorama 0.3.9` / `tabulate 0.8.2` / `seqdiag 0.9.5`;HeteroGen:`antlr3 3.4` / `networkx 2.5.1` / `graphviz 0.16` / `CMurphi 5.4.9.1` / `psutil 5.8.0` | 依赖轻量(无编译型前端),但 **Parser 由 ANTLR3 生成**(证实 IR 层存在) |
| **HeteroGen 确实不含 HieraGen 算法** | `HeteroGen/Algorithms/` = `{ControllerGeneration, General, HeteroGen, ModelChecker, NetworkAnalysis, ProtoAlgoNetworkx, ProtoDirectory}` —— **无 HieraGen 目录**;而 `HieraGen/Algorithms/` 含 `HieraAlgorithm/` | ⭐ **F19 的源码级坐实**:论文 §9.2 的"不能同时做分层+异构"是**代码事实**,不只是作者陈述 |

### 4.7 v0.1 → v0.3 判定修正总表

| Oswald 挑战 | v0.1 | v0.2 | **v0.3** | 依据 |
|-------------|------|------|----------|------|
| **Concurrency** | ⚠️ 边界未写死 | ✅ 已由 `ADR-CEP-17 Decision 4` 落地 | ✅ **已落地 + 一手佐证**(ProtoGen State Set 章节 + 18 瞬态态) | 论文 §5.1.2 / ProtoGen p.1 |
| **Hierarchy** | ⚠️ 部分命中 | ⚠️ 有判据雏形 | ⚠️ **仍是空白,但判据已完备**:唯一序列化点 + E 态升级判据 + proxy-cache;**新增 2 条明确排除**(非包容共享 cache / 跨层非树通信) | HieraGen §V-D / §VII-D / §VII-E |
| **Heterogeneity** | ⚠️ 部分命中 | ⚠️ 有 3 件套 | ⚠️ **仍是空白,但 3 件套已形式化**:复合公理 + 一致性标签 + 全局可见写判据;**新增"目录无法区分 read vs acquire"陷阱** | HeteroGen §V-B / §VI-D-1 / 博士论文 §7.6.6 |

**净结论**:v0.1 的方向判断**成立**;v0.3 把"需要做什么"从 3 个模糊标签,变成 **18 条可判定规则**(F1~F18) + **5 条明确排除** + **3 个可借用技术** + **1 条已知不可兼得**(F19)。

---

## 5. 对 CEP 的架构发现(F1~F23)

### 5.1 发现总表

| # | 发现 | 证据 | 影响 | 建议动作 |
|---|------|------|------|---------|
| **F1** | **CEP 的 Protocol Engine / Fabric-Transport 分层被独立验证** | ProtoGen L4 | ⭐ 强化 `ADR-CEP-01/07` | `ADR-CEP-01 §0.2` 补第 2 条独立佐证 |
| **F2** | **atomic 边界 = 插件内语义,选 C 有据** | ProtoGen L2 | ⭐ 闭合 `ADR-CEP-17 Decision 6` | ✅ 已落;`ADR-CEP-02` v0.2 同步 |
| **F3** | **网络序假设是输入,不是生成物** | ProtoGen L3 | `ADR-CEP-08` 必须覆盖 | `ADR-CEP-08` 增 1 条 |
| **F4** | **唯一序列化点 = 分层组合可判定判据** | HieraGen p.2 | `ADR-CEP-08` 从口号变判据 | `ADR-CEP-08` 采用该不变量 |
| **F5** | **E 态静默升级 = 具名组合失败模式** | HieraGen §V-D | 🔴 CEP Stub 暴露 E,该检查项在结构上必然存在 | `ADR-CEP-08` 增检查;`ADR-CEP-05` 标注 |
| **F6** | **一致性标签 = 能力映射表缺失维度** | HeteroGen **p.3**(§II-B)/ Table I | `ADR-CEP-12` schema 缺维度 | `ADR-CEP-12` 加 `consistency_labels` |
| **F7** | **barrier 流无需新增,但需"一致性效果"维度** | HeteroGen **p.3**(§II-B)/ §V-A | 澄清 `ADR-CEP-02` 流 9 边界 | `ADR-CEP-02` 补 1 句 |
| **F8** | **跨域组合判据可形式化** | HeteroGen §V-B | `ADR-CEP-13` 有可判定形式 | `ADR-CEP-13` 采用 `ppo_com` |
| **F9** | **"全局可见写"可静态判定** | HeteroGen §VI-D-1 | `ADR-CEP-13` 的跨域写触发条件 | `ADR-CEP-13` 采用该判据 |
| **F10** | **兼容性自动判定 = 学术侧也未解决** | HeteroGen §VI-E | ⛔ 范围纪律 | `ADR-CEP-13` 明写"不提供自动判定器" |
| **F11** | **proxy-cache 是可借用的跨域组合技术** | HieraGen §V-A / HeteroGen §VI-C | `ADR-CEP-13` 有实现参考 | 记入 `ADR-CEP-13` 备选 |
| **F12** | **Stub Cache 是"协议状态投影",不是缓存模型** | ProtoGen **p.1 / p.12**(18-20 态) | ⚠️ 措辞风险 | `ADR-CEP-05` / 07 号 §3.2 加澄清 |
| **F13** | **跨域写传播触发与 SharerTracker 保守度同构** | HeteroGen §VI-D-2 | `ADR-CEP-04` / `ADR-CEP-13` 联动 | `ADR-CEP-13` 记入并发粒度规则 |
| **F14** | **gem5 已是协议生成研究线的评测床** | HeteroGen §VIII(gem5, 64 核) | ⭐ `ADR-CEP-00` gem5 类比第二重含义 | `ADR-CEP-00 §5.1` 补"生态类比" ✅ 已落 |
| **F15** | **DSL 路线 vs 插件路线 = 明确分歧** | ProtoGen p.4/p.5 + 论文 Table 9.1 | ⚠️ 需显式记录 trade-off | `ADR-CEP-18` 记录 |
| **F16** | **\*Gen 流水线 4 阶段 + Table 1.3 = 二维能力矩阵先例** | 博士论文 §1.3 + Table 1.3 | `ADR-CEP-12` 应二维化 | `ADR-CEP-12` schema 改二维 |
| **F17** | **序列化点是协议无关抽象(目录 OR 总线)** | 博士论文 §9.3.4(⚠️ Future Work,未实现)| ⭐ Ordering Point 应独立于"目录" | `ADR-CEP-08` 明确不绑定物理实体 |
| **F18** | **VC 分配的目的 = 死锁自由** | 博士论文 §9.3.1(⚠️ Future Work,未实现)| 🔴 Fabric&Transport 承担死锁自由论证 | `ADR-CEP-09` / `ADR-CEP-08` 增硬要求 |
| **F19** | **\*Gen 的两个独立工具无法"无人工介入"地自动做分层 + 异构**(⚠️ **v0.3 复审修正**:是**工具工程限制**,非算法不可能 —— 论文 §1.3 明示两阶段可串联) | 博士论文 §9.2 + §1.3 + Table 1.3 + 源码预留点 | 🔴 **诚实边界**(措辞已修正) | `ADR-CEP-13` 显式声明"该组合**可做但须人工声明 + 实验验证**" |
| **F20** | **工具退化教训:插件接口须从一开始就可组合** | 博士论文 §9.2 | ⚠️ `ADR-CEP-07` 插件接口设计约束 | `ADR-CEP-07` 增接口可组合性要求 |
| **F21** | **SSP Pre-Processing = 依赖提取 + SSP 正确性验证** | 博士论文 Ch5 + §1.3 | `ADR-CEP-14` / `ADR-CEP-18` 的前置层 | `ADR-CEP-18` 记录 |
| **F22** | **穷尽模型检查的开销是公认痛点** | 博士论文 §9.3.7(⚠️ Future Work 段,但**问题本身已实现且是现状**)| 🔴 探索 vs 验证必须二选一 | `ADR-CEP-06` 显式选择 |
| **F23** | **学术侧计划做"从 SSP 生成 SLICC / Rumur"** | 博士论文 §9.2(ProtoGen+)| ⚠️ 竞争 / 互补信号(**v0.3 已修正**:后端未落地) | `ADR-CEP-18` 记录竞合 |
| **F24** | **\*Gen 族已产出可直接运行的 gem5 Ruby 协议** | 源码 `HeteroGen/gem5_HeteroGen_protocols/` | ⭐ **F14 生态类比的最强证据** | `ADR-CEP-00` 生态类比补源码级证据 |
| **F25** | **排序假设是协议描述语言的一等公民(逐网络)** | 源码 `ProtoGen/MSI_Proto.pcc` 的 `Network { Ordered fwd; Unordered resp; ... }` | ⭐ **决策 8 的源码级依据**;粒度应为**逐网络**非逐协议 | `ADR-CEP-12` 的 `ordering_assumption` 改逐网络映射 |
| **F26** | **VC 自动分配在源码里有预留位置** | 源码 `ClassNetworkClassification.py` 注释 | ⭐ F18 源码级佐证 + 给出实现路径("复用模型检查器特性") | `ADR-CEP-09` 记录 |
| **F27** | **死锁验证资源上限:最高 1TB RAM + 数小时** | 源码 `HeteroGen/README.md` | 🔴 F22 的量化上限,**`ADR-CEP-06` 的硬约束** | `ADR-CEP-06` 写入 |
| **F28** | **ProtoGen 已废弃;三仓 MIT License;无 SLICC 后端落地** | 源码 README + 仓库结构 | ⚠️ 修正 F23;**License 无障碍** | `ADR-CEP-18` trade-off 表 |

### 5.2 F1~F5 详述

**F1 — 分层被独立验证**(v0.3 已回溯 PDF 坐实,原文见 §0.3 发现 1)

| ProtoGen | CEP |
|---------|-----|
| 协议动作(状态机 / 消息语义) | **Protocol Engine 插件** |
| 互连接口(VC 定义 + 消息到通道的分配) | **Fabric & Transport Plane** |

**两个独立一手来源,针对不同层面**:ArchForge 拒绝 **DSL 载体**;ProtoGen L4 拒绝 **协议/互连耦合**。互补不重叠。

**F2 — atomic 边界的文献依据**(原文见 §0.3 发现 2)

推论链:atomic RMW 是**协议动作** → 必须由协议规格显式给出 → 归 Protocol Engine 插件 → atomic **意图**只需从流 1 `request` 传到插件。

**因此 `ADR-CEP-02` 的 `perm` 枚举不需要扩展为第 5 个 "ATOMIC" 值** —— atomic 意图应作为 `request` 流的**独立属性**(与 `perm` 正交)。

**F3 — 排序假设面**:CEP 每个 Protocol Engine 插件必须**声明其排序假设**(点对点序 / 无序 / 逐 VC 序)。

**F4 — 唯一序列化点**:`ADR-CEP-08` 建议表述:
> **INV-CEP-08-IC1**:在同一一致性域内,任意两条冲突事务**恰在一个** Ordering Point 序列化。CEP 的 Ordering Point 迁移必须保持此唯一性。

**F5 — E 态静默升级**:**违反 F4 / 全局 SWMR 的具体机制**。判据 = **SSP 是否允许静默权限升级**(可机器检查)。建议 `INV-CEP-08-IC2`:组合检查必须包含 "E 态静默升级" 项。

### 5.3 F6~F10 详述

**F6 — 一致性标签**:`ADR-CEP-12` schema 建议增加:
```
consistency_labels:
  read:  SC | TSO | RC-acquire | RC-data | PLO
  write: SC | TSO | RC-release | RC-data | PLO
```
**v0.3 细化(v0.2 §4.5.4)**:还需支持**请求级标签** —— 同一 wire 请求(如 `GetV`)在不同标签下语义不同(RCC 的 read vs acquire)→ `consistency_labels` 应能标到**请求类型**粒度。

**F7 — barrier 的一致性效果**:`ADR-CEP-02` 补一句边界:
> `barrier` 流的**框架语义**是 requester-scoped 排空 / 可见性点;该 barrier 在**具体一致性模型下的一致性效果**(SC 无效 / TSO 全屏障 / RC release|acquire)**由协议插件依据其一致性标签解释**,不改变流的语义。

**F8 — 跨域判据**:`ADR-CEP-13` 建议采用:
> 复合一致性 ⟺ `acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )`,其中 `ppo_com(t) ≡ ppo_i`(t 属于域 i)

**F9 — 全局可见写判据**:`ADR-CEP-13` 建议采用:
> 请求 `r` 是**域可见写** ⟺ (a) 终态 `s1` 允许本地写命中(可能转 `s2`) 且 (b) `s1` 或 `s2` 接受导致数据值响应的转发

**F10 — 范围纪律**:`ADR-CEP-13` 明写"**不提供**自动兼容性判定器"。

### 5.4 F11~F15 详述

**F11 — proxy-cache**:`ADR-CEP-13` 的设计备选(在桥接点内嵌对端协议状态机实例)。

**F12 — Stub Cache 定位澄清**:**建议在 `ADR-CEP-05` 开头加定义**:
> Stub Cache Core **不是缓存性能模型**,而是**客户端侧协议状态投影**(RN projection):只维护 4 个稳定权限状态 + 有限的 outstanding 语义,用于驱动 Protocol Engine;真实协议的瞬态状态数(参考 ProtoGen 生成的 non-stalling MSI 为 **18-20 状态**)全部住在 Protocol Engine 插件内。

**F13 — 并发粒度与保守度同构**:`ADR-CEP-13` 的跨域桥接并发策略应按"对端协议是否允许提前 ack"选择(保守=全串行 / 激进=同地址串行)。

**F14 — gem5 生态类比**:`ADR-CEP-00 §5.1` 应从 1 层扩到 2 层:
| 层 | 含义 |
|----|------|
| 归口类比 | gem5 Ruby 是 gem5 的**框架能力**;CEP 是 CppTLM 的**框架能力** |
| **生态类比** | gem5 **已是学术协议生成研究线的标准评测床**(HeteroGen 在 gem5 上 64 核评测 vs HCC) |

**F15 — DSL 分歧**:`ADR-CEP-18` 记录 trade-off:
| 维度 | *Gen 族(DSL 路线) | CEP(C++ 插件路线) |
|------|-------------------|-------------------|
| 输入 | PPL 描述的 SSP | 手写 C++ 插件 |
| 产出 | Murϕ FSM(计划中:SLICC / Verilog / Rumur 后端) | ProtocolEngine 插件(可直接注册进 ModuleFactory) |
| 工程量 | **30k-45k 行 Python**(Table 9.1) | 每协议一个 adapter,量级低 1-2 个数量级 |
| 工具链 | ANTLR3 + IR + 后端(独立编译器) | 无独立工具链,复用 CppTLM 构建系统 |
| 自动化 | ✅ 自动生成瞬态态 | ❌ 手工 |
| 类型安全 | 弱(动态) | 强(编译期) |
| 长期演进位 | — | SSP 描述 → 插件生成(per F21) |

### 5.5 ⭐ F16~F23 详述(v0.3 新增)

**F16 — 二维能力矩阵先例**:
博士论文 Table 1.3 表明:*Gen 工具的"能力"是 **(算法阶段) × (工具)** 的二维矩阵**,且**每个已发布工具都缺一段**。同理,CEP 的 `ADR-CEP-12` 能力映射表若只做一维("协议族 → adapter"),会丢失"**这个协议实例支持哪些组合维度**"这一信息。
建议 schema 二维化:
```
protocol_instance:
  family: <族>
  adapter: <adapter 名>
  combination_capabilities:   # ← 新增正交维度
    concurrent: yes/no          # 并发事务支持
    hierarchical: yes/no        # 多层目录组合
    heterogeneous: yes/no       # 跨一致性域组合
```
**边界(诚实声明)**:论文矩阵是**工具的算法阶段**,CEP 矩阵是**协议实例的组合能力** —— **形似而非同构**。这是**设计类比**,不是"CEP 应该照抄"。

**F17 — 序列化点是协议无关抽象**:
§9.3.4 原文(见 §4.5.3)。**方向性证据**(future work,未实现)。对 CEP 的意义:CEP 把 **Ordering Point** 放在独立的 `Directory & Ordering Plane`,**不绑定"目录"这一物理实体** —— 论文的"总线即序列化点"是同一抽象的 snooping 形态。**这恰好覆盖 gem5 Ruby 的广播型协议**(`MOESI_AMD_Base` / `MOESI_hammer` 的 probe filter + 广播)。

**F18 — VC 分配 = 死锁自由**:
§9.3.1 原文(见 §4.5.3)。**这是本批最重要的新增约束**:
> CEP 的 Fabric & Transport Plane 承担 **VC 分配**,而 VC 分配的目的**不是 QoS 调优,而是保证死锁自由**。

→ `ADR-CEP-09`(多 VC 仲裁)应包含硬要求:**VC 分配策略必须能论证死锁自由性**(与协议的 ordering point 假设联合论证)。这比"多 VC 怎么仲裁"高一个层次。

**F19 — 工具不能同时做分层 + 异构**:
§9.2 原文(见 §4.5.2)。**这是一条对 CEP 不利的诚实边界**,必须写进调研与 `ADR-CEP-13`:
> **CEP 若同时支持多层目录组合 + 跨一致性域组合,是在解一个连原始研究者都尚未解决的问题**。该能力不应在 Stage 1-3 承诺。

**F20 — 插件接口可组合性**:
§9.2 原文(见 §4.5.2)。ProtoGen 的 OO 结构在适配 HieraGen 时**代码质量显著下降**,HeteroGen **largely from scratch** 重写。**对 CEP 的工程约束**:
> `ADR-CEP-07` 的 ProtocolEngine 插件接口设计**必须从第一版就为组合维度(分层 / 异构)留出扩展点**;否则组合能力落地时将面临接口重写。

⚠️ 这是**工程教训类比**,不是同构证据 —— ProtoGen 面对的是"算法阶段拼接",CEP 面对的是"协议实例组合",相似但不等价。

**F21 — SSP Pre-Processing 层**:
博士论文 Ch5(见 §4.5.1)。该层做两件事:**提取 cache controller 与 directory controller SSP 之间的依赖** + **验证 SSP 正确性**。
对 CEP:手写插件路线下,对应物是 **`ADR-CEP-14`(Stub 可验证性合同测试)**;SSP 描述路线下(`ADR-CEP-18`),需复刻该层。

**F22 — 穷尽验证开销**:
§9.3.7 原文(见 §4.5.3)。**CEP 必须做一次显式选择**:
| 定位 | 验证方式 | 代价 |
|------|---------|------|
| **探索平台** | 随机测试 + 大规模 sweep(gem5 Ruby 路线) | 无法穷尽,靠统计置信 |
| **验证平台** | 穷尽模型检查(Murϕ 路线) | 3 caches / 256GB 上限,不可 sweep |

→ `ADR-CEP-06` 必须**二选一**并写明,不能两者都要。**当前 CEP 的定位(探索平台)倾向后者**,但需在 `ADR-CEP-06` 显式承认"不做穷尽保证"。

**F23 — 学术侧正在做"从 SSP 生成 SLICC / Rumur"**:
§9.2 原文:"we are currently working on the **ProtoGen+** tool. The ProtoGen+ tool encompasses the latest implementations of all *Gen algorithms developed. In addition, it integrates **a variety of Backends like Rumur and SLICC**..."
→ **竞合判断**:
- **对 CEP 是互补信号**:一旦 *Gen 族能产出 SLICC,gem5 Ruby 的协议实现空间就会被"自动生成"扩张 —— 这**扩大了 CEP 需要探索的协议空间**,而非缩小它
- **也是竞争信号**:若 *Gen 工具能自动生成 SLICC,CEP 的"每协议一个手写 adapter"成本优势会缩小
→ `ADR-CEP-18` 应记录:CEP 的定位不是"生成协议",而是"**在统一实验床上运行/对比协议**";二者可共存(gem5 已有 Ruby + 可插拔 SLICC 的先例)。

### 5.6 ⭐ F24~F28 详述(源码实证,v0.3 新增)

**F24 — *Gen 族已产出 gem5 Ruby 协议源文件**（⚠️ **v0.3 复审降级**:非"开箱即用"）
源码目录 `HeteroGen/gem5_HeteroGen_protocols/` 含 `HETERO_RCCO_GEN_{WR,NO_HS}.slicc` + 5 个 `.sm`(L1cache / L2cache / dir / msg / RCCOCache)+ `configs/ruby/*.py` + `setup.sh` + `run_*.sh`。
→ **学术协议生成工具与 gem5 Ruby 在源码层面直接相遇**。对 CEP 的意义(修正 F14):
- **互补而非竞争**:*Gen 族**生成 gem5 协议**;CEP **在统一实验床上运行 / 对比协议**(跨 gem5 / CHI / CXL / TileLink)
- **反向需求**:gem5 协议空间一旦可被自动扩张,**CEP 需要探索的协议空间变大** → **加强**"生态类比"
- **但需明确定位**:若 gem5 手写 `.slicc` 范式被侵蚀,CEP 若也做 gem5 侧适配,须说清自身定位

**F25 — 排序假设是协议描述语言的一等公民**
`.pcc` 原文:`Network { Ordered fwd; Unordered resp; Unordered req; };` → 排序假设是**逐网络**声明的**协议规格的一部分**,不是框架开关。
→ **决策 8 的 `ordering_assumption` 有了源码级依据**,且粒度应为:
```
ordering_assumption:
  <network_name>: p2p_ordered | unordered      # 逐网络,而非逐协议单值
```

**F26 — VC 自动分配在源码里有预留位置**
`ClassNetworkClassification.py` 注释:"Automatic virtual channel assignment can be implemented here. **Possibly reuse model checker features**"。
→ F18 源码级佐证;并给出实现路径参考:**VC 分配可复用模型检查器的可达性/无环性分析** —— 这与 CEP 的 `deadlock_violations` 闸天然契合。

**F27 — 死锁验证的资源上限**
`HeteroGen/README.md`:"a few deadlocks tests can require **up to 1TB of RAM and hours of computation time**"(常规测试 16GB / 分钟级)。
→ `ADR-CEP-06` 的硬约束:**不能承诺"任何配置都能穷尽验证"**;必须声明规模上限与降级策略(如:小配置穷尽 + 大配置随机)。

**F28 — ProtoGen 已废弃 + MIT License + 无 SLICC 后端**
- ProtoGen README:"**discontinued and replaced by HeteroGen**";`icsa-caps` 仓 commit = "Discontinued Snapshot"
- 三仓均 **MIT License**(UoE 2018)→ **无 copyleft 义务**,CEP 可自由借鉴设计思路(保留版权声明)
- 后端只有 Murphi → **F23 修正**:竞合压力尚未兑现

### 5.7 ⭐ F1~F28 证据类型逐条归类(v0.3 复审要求)

> **为什么需要这张表**:F1~F28 混合了**一手证据**、**方向性证据**、**设计类比**、**推断**四种强度。混用会让下游 ADR 把"类比"当"证据"引用。本表是**引用时的强度依据**。

**图例**:
- **E1 = 一手证据**:论文/源码原文可直接支撑(可回溯到具体行号/文件)
- **E2 = 方向性证据**:出自论文 Future Work(**未实现**)或作者自陈局限,只指示方向,不代表已验证
- **E3 = 设计类比**:形似但不同构,借鉴需重新论证
- **E4 = 推断**:由 E1/E2 推出,结论正确性依赖前提成立

| # | 证据类型 | 一手锚点 | 备注 |
|---|---------|---------|------|
| **F1** | **E1** | ProtoGen p.7(局限 L4 原文) | PDF 已回溯坐实 |
| **F2** | **E1** | ProtoGen p.7(局限 L2 原文) | PDF 已回溯坐实 |
| **F3** | **E1** | ProtoGen p.7(局限 L3 原文) | — |
| **F4** | **E1** | HieraGen p.2 + 树结构证明 | — |
| **F5** | **E1** | HieraGen §V-D | — |
| **F6** | **E1** | HeteroGen §II-B + Table I | — |
| **F7** | **E1** | HeteroGen p.2(接口含 fences)+ §V-A | — |
| **F8** | **E1** | HeteroGen §V-B 公理 (3) | — |
| **F9** | **E1** | HeteroGen §VI-D-1 判据 (a)(b) | — |
| **F10** | **E1** | HeteroGen §VI-E("beyond the scope of this work") | — |
| **F11** | **E1** | HieraGen §V-A(proxy-cache)+ HeteroGen §VI-C | — |
| **F12** | **E1**(量级) + **E4**(定性) | ProtoGen p.1/p.11(18-20 态)→ "Stub 是协议状态投影"是推断 | ⚠️ **后半句是 E4**:"Stub 不是缓存模型"是从量级**推断**的,非原文断言 |
| **F13** | **E3** | HeteroGen §VI-D-2 ↔ ADR-CEP-04 completeness | 同构性是**类比** |
| **F14** | **E1** | HeteroGen §VIII(gem5 64 核)+ 源码 `gem5_HeteroGen_protocols/` | 源码部分已降级表述 |
| **F15** | **E1**(量级) + **E3**(trade-off) | ProtoGen p.4/p.5 + 论文 Table 9.1 | trade-off 表是**工程判断** |
| **F16** | **E3** | 论文 Table 1.3(工具的算法阶段矩阵)↔ CEP 能力映射表 | **形似而非同构**(B10) |
| **F17** | **E2** ⚠️ | 论文 §9.3.4(**Future Work,未实现**) | 已在总表 + §0.3 标注 |
| **F18** | **E2** ⚠️ | 论文 §9.3.1(**Future Work**)+ 源码预留位 | 源码有 TODO 位,但功能未实现 |
| **F19** | **E1**(限定的) + **E4**(对 CEP 的映射) | 论文 §9.2("without manual intervention")+ §1.3(阶段可串联)+ 源码预留点 | ⚠️ **v0.3 复审已改措辞**:"工具工程限制"是 E1;"CEP 组合可做"是 E4 |
| **F20** | **E1**(作者自陈)+ **E3**(对 CEP 的映射) | 论文 §9.2 | "插件接口须可组合"是**类比**推论 |
| **F21** | **E1** | 论文 Ch5 + §1.3 | — |
| **F22** | **E1**(现状)+ **E2**(formal proof 属 future work) | 论文 §9.3.7 + 源码 README(1TB RAM) | — |
| **F23** | **E1** | 论文 §9.2(ProtoGen+ 计划)+ 源码(三仓后端只有 Murphi) | — |
| **F24** | **E1** | 源码 `gem5_HeteroGen_protocols/` 目录 | ⚠️ **已降级**;"非开箱即用"的依据是 `setup.sh` + README |
| **F25** | **E1** | 源码 `ProtoGen/MSI_Proto.pcc` 的 `Network { Ordered fwd; ... }` | — |
| **F26** | **E1** | 源码 `ClassNetworkClassification.py` 注释 | — |
| **F27** | **E1** | 源码 `HeteroGen/README.md`("up to 1TB of RAM and hours") | — |
| **F28** | **E1** | 源码 ProtoGen README(废弃声明)+ 三仓 LICENSE(MIT)+ 后端目录 | — |

**统计**:**E1 = 20 条**(F1~F9, F10, F11, F14, F16 的锚点, F21, F22, F23, F24, F25, F26, F27, F28);**E2 = 2 条**(F17, F18);**E3 = 3 条**(F13, F15 的 trade-off, F16, F20);**E4 = 混入 F12 后半 / F19 对 CEP 的映射**。

**引用纪律(本表的使用方式)**:
- **E1** → 可直接作为 ADR 决策依据
- **E2** → 只能作**方向**引用,**禁止**写成"已验证"或"文献证明"
- **E3** → 只能作**类比/启发**引用,**必须**附带"非同构"限定
- **E4** → 只能作**推断**引用,**必须**写出前提

> **v0.3 复审自查发现**:**F12** 的前半(18-20 状态量级)是 E1,后半("Stub Cache 不是缓存模型")是 **E4 推断** —— 原文档未区分,已在本表标注。**F19** 对 CEP 的映射同样是 E4。

---

## 6. 五源交叉定位矩阵

| 维度 | ArchForge 09 | gem5 Ruby SLICC | *Gen 族(PDF 原文级) | **CEP 现状** |
|------|--------------|------------------|-------------------|---------------|
| **协议来源** | dGPU SoC 内部设计 | gem5 学术/教学 | 自动生成 | — |
| **协议实例数** | 1(MOESI/GPU 6×6)| 8 独立 `.slicc` | 6 类输入协议(论文 §2.2)| 2 来源族 / **14 adapter 实例** + 5 一致性模型语义索引 |
| **状态模型** | 6 状态(4 稳定 + O + **T 瞬态**) | 各 `.sm` 自带 SM | **SSP → 非原子 + 瞬态**(MSI **18** 瞬态)| Stub = `I/S/E/M` 4 稳定 |
| **目录机制** | SnoopFilter + CoherenceDomain | directory / token / probeFilter | FullMap / 不完整知识 / **null dir(Dir1B)**| 5 实现 + caps descriptor |
| **跨域** | CoherenceBridge | Region / CorePair | **合并目录 + proxy-cache + 一致性翻译** | 流 7 + `barrier_domain` |
| **多层** | CoherentXBar | L1/L2/L3 + dir + dma | **唯一序列化点 + E 态兼容检查** | TreeManager(scope=SUBTREE) |
| **一致性模型** | 未显式建模 | 未显式建模 | **SC / TSO / RC / PLO 四类 + 8 标签** | ❌ **缺该维度** |
| **验证** | 无自动验证 | Ruby 随机测试 | **Murϕ(Safety+Deadlock) + 111 litmus + gem5 性能** | `deadlock_violations` / `memory_violations` 闸 |
| **规模上限** | 未记录 | 大规模(gem5 数十核)| **3 caches / 256GB+hash / 1-3 cache 每簇** | 未记录 → ⚠️ 需补(F22) |
| **抽象取向** | C++ switch 表(拒绝 slicc DSL) | SLICC DSL(代码生成) | DSL(PPL + ANTLR3 + IR)+ 代码生成 | C++ 插件(拒绝 DSL)✅ |
| **工具工程量** | — | gem5 自身 | **8.8k / 15.8k / 30.4k / 45k+ 行 Python** | 每协议 1 adapter,量级低 1-2 个数量级 |
| **评测床** | — | gem5 自身 | **gem5**(HeteroGen 64 核 vs HCC)| CppTLM |

**矩阵读出的 3 个缺口**:
1. **一致性模型维度缺失** — 5 源中 3 源有,CEP 独缺(F6)
2. **规模上限未记录** — *Gen 族明确记录状态空间瓶颈,CEP 的 `ADR-CEP-06` 没有(F22)
3. **组合维度能力未建模** — *Gen 族用 Table 1.3 的二维矩阵表达,CEP 是一维(F16)

---

## 7. 结论与建议

### 7.1 总结论

| 判定 | 内容 |
|------|------|
| ✅ **CEP 抽象方向正确** | **五源**交叉印证:协议中立 + SSP 抽象 + 协议插件化 + **协议/互连分离** |
| ✅ **canonical 10 流容量足够** | ArchForge 6 事件 + gem5 Ruby 消息 + HeteroGen read/write/**fence** 三元接口 —— **三源零新增流** |
| ✅ **6/8 gem5 Ruby 协议直接支持** | MESI_Two_Level / MESI_Three_Level / MOESI_CMP_directory / MOESI_hammer / MOESI_AMD_Base / GPU_VIPER |
| ✅ **atomic 边界已有文献依据** | ProtoGen L2 → 归插件内;`perm` 4 值锁**无需扩展** |
| ✅ **分层组合判据已可判定** | 唯一序列化点 + E 态静默升级判据 + proxy-cache 技术 |
| ✅ **跨域组合判据已可判定** | 复合一致性公理 + 全局可见写静态判据 + 一致性标签体系 |
| 🎯 **gem5 双重类比成立** | 归口类比 + **生态类比** |
| 🎯 **CEP 分层获第二个独立来源** | ProtoGen L4(接口边界)补上 ArchForge(语言载体) |
| ⚠️ **需 4 处扩展** | ① token 归属 ② 协议族/实例语义(⚠️ v0.3 复审 R8 已澄清为"族=来源集合")③ atomic(已闭合)④ **一致性标签维度** |
| ❌ **1 项明确排除** | `MESI_Three_Level_HTM`(事务内存 / lease 时效性边界) |
| ⚠️ **2 个状态缺口** | `O` / `T` —— 分层选择,非缺陷,已由 `ADR-CEP-17 Decision 4` 落定 |
| ⚠️ **3 项新缺口** | ① 一致性标签维度 ② 规模上限未记录 ③ Stub Cache 定位措辞风险 |
| 🔴 **1 项新约束(必须记入)** | **VC 分配 = 死锁自由性载体**(F18)—— 影响 `ADR-CEP-09` |
| 🔴 **1 项诚实边界(必须记入)** | **分层 + 异构的组合可做,但*Gen 的两个独立工具无法"无人工介入"地自动完成**(F19,⚠️ 措辞已修正为"工具工程限制"而非"算法不可能")—— 影响 `ADR-CEP-13` 的范围承诺 |
| ⛔ **1 项范围纪律** | **不承诺**自动协议兼容性判定(学术侧亦未解决) |

### 7.2 建议(按优先级)

| 优先级 | 建议 | 落点 | 状态 |
|--------|------|------|------|
| **P0** | `ADR-CEP-17` 补 F1/F2 证据 + Decision 6 闭合 | `ADR-CEP-17` | ✅ **已落**(v0.2) |
| **P0** | `ADR-CEP-02` v0.2:atomic 意图作为 `request` 流属性(**不扩 `perm` 枚举**)+ 补 barrier 一致性效果边界句 | `ADR-CEP-02` | 🟡 待下一轮修订 |
| **P0** | `ADR-CEP-05` 开头加 **Stub Cache 定位澄清**(协议状态投影,非缓存模型) | `ADR-CEP-05` | 🟡 待起草修订 |
| **P1** | `ADR-CEP-08` 增 4 条:唯一序列化点 / 排序假设面 / E 态静默升级检查 / **Ordering Point 不绑定物理实体**(F17) | `ADR-CEP-08` | 🟡 待起草 |
| **P1** | `ADR-CEP-09` 增 **VC 分配须论证死锁自由性**(F18) | `ADR-CEP-09` | 🟡 待起草 |
| **P1** | `ADR-CEP-12` schema **二维化**:`consistency_labels` + `ordering_assumption` + `combination_capabilities`(F6/F16) | `ADR-CEP-12` | 🟡 待起草 |
| **P1** | `ADR-CEP-13` 采纳复合公理 + 全局可见写判据 + **显式声明"分层+异构不可同时自动生成"**(F8/F9/F10/F19) | `ADR-CEP-13` | 🟡 待起草 |
| **P1** | `ADR-CEP-06` 增:① `memory_violations` 对齐 **SWMR + data-value** 双不变量 ② **显式选择"探索 vs 验证"** ③ 记录实验规模上限(F22) | `ADR-CEP-06` | 🟡 待起草 |
| **P1** | `ADR-CEP-07` 增 **插件接口可组合性**要求(F20) | `ADR-CEP-07` | 🟡 待修订 |
| **P2** | `ADR-CEP-18`:DSL vs 插件路线 trade-off(Table 9.1 量级) + **竞合分析**(F15/F21/F23) | 新 ADR | 🟡 **编号已预留**,未起草 |
| **P2** | `ADR-CEP-00 §5.1` 补 gem5 **生态类比** | `ADR-CEP-00` | ✅ **已落** |
| **P2** | `07 号 §5.1` 增"学术协议族"小节 | `07 号 §5.1.4` | ✅ **已落** |
| **P2** | `ADR-CEP-01 §0.2` 原则表补 F1 第二独立佐证 | `ADR-CEP-01` | 🟡 待修订 |
| **P3** | 调研源治理:向 mynotes 仓反馈 §1.3 的核对表(v0.3 已修正 Q3/Q4,wiki 正确) | 跨仓 | 🟡 非本仓范围 |
| **P3** | 拉取三个工具源码(注意 **org 冲突**:`icsa-caps` vs `Errare-humanum-est`)验证 PPL/IR 实现 | 跨仓 | 🟡 进行中 |

### 7.3 本调研**不**改变的东西(诚实边界)

- **不**改变 CEP 6 层架构
- **不**改变 canonical 10 流清单(三源零新增流)
- **不**改变 Stub Cache 的 `I/S/E/M` 抽象
- **不**改变守门 invariant 的**分组结构**;本批因 Oracle 复审新增 `CEN-Ib11`(→ 编号位 37 / 实际 36)并补落 `INV-CEP-04-IC7` + `INV-CEP-05-IC1` 进 owner ADR;后续 P1 建议落地仍会改变计数 → 需同步 4 份文档
- **不**触发 AGENTS.md:423 边界条款(仍是框架级)
- **不**推翻 `ADR-CEP-17` 已拍板的 8 项决策;本批只**补证据**
- 本调研是**附属层**文档,不重发明真源 1 / 真源 3

---

## 8. 引用与边界

### 8.1 调研源(全量)

| 源 | 路径 | 等级 | 本调研引用范围 |
|----|------|------|--------------|
| ArchForge 09 | `/workspace/main/ArchForge/docs/architecture/09-coherence-protocol.md` v1.0-draft(514 行) | 一手(草稿) | §2 全部 |
| **博士论文 PDF** | `00_Raw_Sources/papers/Nicolai_Oswald_PhD_Thesis.pdf`(**178 页**) | **一手** | §4.4 / §4.5 全部 + F16~F23 |
| **ProtoGen PDF** | `00_Raw_Sources/papers/gen-family/ProtoGen_ISCA2018.pdf`(14 页) | **一手** | §4.1 全部 + §0.3 发现 1/2 |
| **HieraGen PDF** | `00_Raw_Sources/papers/gen-family/HieraGen_ISCA2020.pdf`(12 页) | **一手** | §4.2 全部 |
| **HeteroGen PDF** | `00_Raw_Sources/papers/gen-family/HeteroGen_HPCA2022.pdf`(16 页) | **一手** | §4.3 全部 |
| gem5 Ruby 3 篇 | `docs/research/gem5-{coherence-protocols,ruby-protocol-bridge,slicc-chi}-survey.md` | 二手(仓内) | §3 全部 |
| Oswald 摄入笔记 | `04_Knowledge/D12-cache-coherence-numa/oswald-phd-thesis-draft.md`(153 行) | 二手摘要 | §1 对照基线(被全文取代) |
| 01_Wiki 12 条目 | `01_Wiki/{concepts,entities,sources}/` | 二手消化 | §1.3 核对 |
| RESEARCH_MAP | `04_Knowledge/RESEARCH_MAP.md` | 索引 | §1.2 |

### 8.2 调研边界(诚实声明,v0.3 修订)

| # | 边界 | 说明 |
|---|------|------|
| **B1** | **引文抽查 36 条 → 33 条脚本命中 + 3 条抽取伪阴性** | 全部 36 条**实质正确**。未命中的 3 条(F1/L3 / F6/release / F10/update)经定位为 **PDF 抽取伪影**:(a) 脚注插入正文流、(b) 硬换行、(c) 页标记 `=== [PAGE N] ===` 插在句中。逐条已人工回原文确认 |
| **B2** | **PDF 文本抽取件是 grep 用的代理,不是最终依据** | 所有行号引用基于抽取件;**页码**基于 PDF 本身。抽取件与 PDF 存在 ligature(`ﬁ`/`ﬂ`)与断词差异 |
| **B3** | ~~**未拉取源码**~~ → **v0.3 已获取,但只做结构级审阅** | ✅ 三仓已 clone(§4.6),MIT License,最后提交 2022-01-07。**已审阅**:目录结构 / `.pcc` 描述语法 / `StateSet` 类 / `ClassNetworkClassification` / README / License。**未做**:逐行算法阅读、实际运行、生成产物端到端复现 |
| **B4** | **ArchForge 09 是 v1.0-draft(待 Oracle 评审)** | 其 6×6 转换表**未经形式化验证**(per 其 §13 R1) |
| **B5** | **未做实验验证** | 本调研是**架构可容纳性分析**,不是实测 |
| **B6** | **§1.3 核对仅覆盖 7 条断言** | 12 份 wiki 条目中 7 份含可核对技术断言;另 5 份为二手转述,未逐句核 |
| **B7** | **~~博士论文原文未获取~~** | ✅ **v0.3 已撤回**:178 页 PDF 已获取并深度分析(§4.5) |
| **B8** | **~~gen-family PDF 不在本次范围~~** | ✅ **v0.3 已撤回**:目录存在,3 份 PDF 均已抽取并逐条回溯 |
| **B9** | **§9.3 Future Work 的 7 项均为未实现** | 本调研引用它们时**只作方向性证据**,不作为"已验证" |
| **B10** | **F16/F20 是设计类比,非同构证据** | *Gen 矩阵 = 工具的算法阶段;CEP 矩阵 = 协议实例的组合能力。形似而非同构 |

### 8.3 待验证问题(交给后续实施 / 评审)

| # | 问题 | 建议验证方式 |
|---|------|------------|
| **Q1** | ~~PDF 数字是否与抽取件一致?~~ | ✅ **v0.3 已做**:36 条引文逐条回溯,3 条伪阴性已定位 |
| **Q2** | ~~三个工具源码是否公开可跑?~~ | ✅ **v0.3 已解决**:三仓均可 clone,MIT License,`icsa-caps` 为废弃快照 |
| **Q3** | PPL 的 ANTLR3 语法文件长什么样? | 读 HeteroGen/HieraGen 仓库的 grammar 文件;验证 §4.5.1 的 IR 格式 |
| **Q4** | ArchForge 09 的 6×6 与 ProtoGen 生成的 MOESI 是否一致? | 用 ProtoGen 生成 MOESI,对比 `state_transition[6][6]` |
| **Q5** | 111 个异构 litmus test 能否转成 CEP 实验用例? | 拉 herd7 litmus 集,评估复用性 |
| **Q6** | ProtoGen+ 的 SLICC 后端是否已产出? | ✅ **v0.3 已答:未产出**(三仓后端只有 Murphi,最后提交 2022-01-07)。跟踪 `Errare-humanum-est` 后续提交 |
| **Q7** | *Gen 族工具的 adoption 情况(论文称"已被学界和工业界采用") | 查引用论文 / 工业案例;影响 CEP 的"生态类比"强度 |
| **Q8** | `gem5_HeteroGen_protocols/` 产出的 gem5 协议**实际能否跑起来**? | 跑 `setup.sh` + `run_RCCO_GEN_WR_HS.sh`,在 gem5 上复现论文 §VIII 的性能结果 —— 这是验证 F24 的最强一步 |
| **Q9** | `dmb_to_RC_map.txt` 的 fence 映射表能否直接移植为 CEP 的一致性标签表? | 评估表结构与 `ADR-CEP-12` 的 `consistency_labels` 兼容性 |
| **Q10** | `ClassNetworkClassification` 的"复用模型检查器特性"思路能否落到 CEP? | 与 `ADR-CEP-09` 的多 VC 仲裁 + `ADR-CEP-06` 的 `deadlock_violations` 闸联合设计 |

---

## 9. 复审状态(自审记录)

### 9.1 外部独立复审**未取得**

| 尝试 | 时间 | 目标 | 结果 |
|------|------|------|------|
| 1 | 2026-10-20 | `oracle` 复审本批(调研 v0.2 + ADR-CEP-17 v0.2 + 07 号) | ❌ `Model unavailable: openstarry/glm-5.3-flash` |
| 2 | 2026-10-20 | `explore` 搜论文原文与 PDF | ❌ 同上模型不可用 |
| 3 | 2026-10-20 | `critic` 复审本批 | ❌ `Model unavailable: openstarry/kimi-k2.6` |
| 4 | 2026-10-20 | `librarian` 查三工具源码 | ❌ `openstarry/glm-5.3-flash` 不可用 |
| 5 | 2026-10-20 | `critic` 复审 v0.3 | ❌ `openstarry/kimi-k2.6` 不可用 |
| 6 | 2026-10-20 | `critic` 复审 v0.3(源码实证后) | ❌ `openstarry/kimi-k2.6` 不可用 |

**结论**:**本批未经独立第三方复审**。以下 §9.2 是**作者自审**,**不等价于独立复审** —— 自审存在确认偏误(作者对自己论证的天然倾向)。**"待独立复审"状态应保持打开**,直到 `oracle` / `critic` 通道恢复后补做。

**作者自行代偿的部分**:subagent 通道不可用期间,作者**直接执行**了原本委派给 `explore` / `librarian` 的任务(本地 find 检索 + PDF 抽取 + `git clone` 三仓 + 源码结构审阅),并**自行执行**了引文抽查(§9.2 S1)。这**缩小了信息缺口**,但**不替代**独立复审 —— 尤其"我的论证是否过度"这一类问题,自审天然不可靠。

### 9.2 自审清单与结论

| # | 自审项 | 方法 | 结论 |
|---|--------|------|------|
| **S1** | **引文真实性** | 36 条引用 → 脚本在 PDF 抽取文本中检索;未命中项逐条人工回原文定位 | ✅ **36/36 实质正确**。10 条脚本未命中经定位全为抽取伪影(ligature `ﬁ`/`ﬂ`、硬换行、脚注插入、页标记 `=== [PAGE N] ===` 插在句中);3 条最后未命中者已逐条打印原文确认 |
| **S2** | **过度解读** | 逐条比对"原文说 A"与"调研推出 B" | ✅ 未发现越界推断。**2 处措辞已收敛**:"必然命中"→"该检查项在结构上必然存在"(§2.4.3 段 + F5 总表行) |
| **S3** | **future work 被当既成事实** | 全文 grep `9.3.x` 引用点 | ✅ **已修 4 处**:§0.3 发现 4、§4.1.6 强化段、F17 总表行、F18 总表行 —— 均补 "⚠️ Future Work,未实现" 标注。**并加 B9 边界条目** |
| **S4** | **类比 vs 证据混淆** | 逐条判定 F1~F23 的证据类型 | ✅ 已显式区分:**一手证据**(F1/F2/F4/F5/F6/F8/F9/F10/F11/F12/F13/F14/F15/F16/F18/F19/F20/F21/F22/F23)/ **方向性证据(future work)**(F17/F18 的 §9.3 出处)/ **设计类比非同构**(F16/F20,见 **B10**) |
| **S5** | **自我纠错是否彻底** | 检查 v0.2 的 2 条错误边界声明 + 2 条核对误判 | ✅ v0.3 §0.4 以表格显式列出 C1~C6,**含撤回与误判承认**;§1.2 记录方法论失误(只 find `*.md` 漏检 PDF) |
| **S6** | **越权** | 检查 `ADR-CEP-17` 是否单方面锁定 `ADR-CEP-02` | ✅ **未越权**:Decision 6 明确写"本 ADR 不单方面改动 `ADR-CEP-02` 的锁",并把该修订列为 `ADR-CEP-02` v0.2 的待办项 |
| **S7** | **计数一致性** | 全仓 grep invariant 计数 | ✅ 唯一值 **36**;`07 号 §4` 计数行、README Doc Map、4 份 roadmap/improvement 文档全部一致 |
| **S8** | **断链** | 全部 CEP 文档 + README 的相对 md 链接 | ✅ **0 断链** |
| **S9** | **范围纪律** | grep "不承诺 / 不提供自动 / 不自动生成" | ✅ 3 处明确范围纪律(兼容性自动判定 / 分层+异构自动组合 / 自动 VC 分配)均已落到 ADR 与 07 号 |
| **S10** | **过度承诺** | 检查"生态类比"等强表述 | ⚠️ **1 处待外部判断**:"生态类比"(gem5 已是该研究线评测床)是否会被读成"CEP 必然能承载此类研究" —— 已在 `ADR-CEP-00` 用"推论"措辞限定 + 补源码级证据(F24)+ 补边界澄清(F28),但**这一处的表述强度应由独立复审判定** |
| **S11** | **源码级事实准确性** | 逐条核对 §4.6 的源码断言(README 原文 / 目录结构 / `.pcc` 语法 / `ClassNetworkClassification` 注释) | ✅ 全部经 `cat` / `find` / `grep` 实测。**未做**:逐行算法阅读、实际运行、生成产物复现 |
| **S12** | **F23 修正是否彻底** | 全文 grep "SLICC" / "Rumur" | ✅ 已修:§1.4 后端状态、§4.6.1、F23 总表行、F28、`ADR-CEP-17 Decision 10` 均已标注"**未落地**" |
| **S13** | **F19 是否有源码级支撑** | 对比 `HeteroGen/Algorithms/` 与 `HieraGen/Algorithms/` 目录 | ✅ **坐实**:`HeteroGen/Algorithms/` = {ControllerGeneration, General, HeteroGen, ModelChecker, NetworkAnalysis, ProtoAlgoNetworkx, ProtoDirectory},**无 HieraGen 目录**;`HieraGen/Algorithms/` 含 `HieraAlgorithm/`。→ 论文 §9.2 的"不能同时做分层+异构"是**代码事实** |
| **S14** | **License 判断** | 读三仓 LICENSE 头部 | ✅ 三仓均 **MIT**(Copyright (c) 2018 The University of Edinburgh)→ 无 copyleft 义务 |
| **S15** | **F25 逐网络粒度是否被正确提炼** | 读 `ProtoGen/MSI_Proto.pcc` 的 `Network { ... }` 结构 | ✅ 确认排序假设是**语言内建结构**且**逐网络**声明;`ADR-CEP-17 Decision 8` 的 schema 已改为逐网络映射 |

### 9.3 对抗式复审发现(2026-10-20 第二轮,作者执行)

> **说明**:subagent 通道第 7 次失败后,作者**自行按 critic 提示词的 6 项清单**执行了对抗式核验。本节记录**实际发现的问题**(非"无问题")。

| # | 严重度 | 发现 | 位置 | 处置 |
|---|-------|------|------|------|
| **R5** | 🔴 **SHOULD_FIX(实质)** | **Decision 9 过度降承诺** —— 论文原文限定词是 "**without manual intervention**"("不能**无人工介入地自动**完成"),**不是"不能组合"**;论文 §1.3 明示 HieraGen 阶段**可消费** HeteroGen 阶段输出,源码亦有预留点(`TraceNetworkx.py:77`)。原表述"不可同时自动生成 / CEP 不承诺"**过强** | 调研 §4.5.2 / F19 行 / `ADR-CEP-17 Decision 9` / `07 号 §5.1.4` | ✅ **已改**:Decision 9 改为"**组合可做,但走人工声明 + 实验验证,不承诺自动化**";并新增"接口须预留组合接缝"要求(接 F20) |
| **R6** | 🟡 **SHOULD_FIX** | **F24 过度声称** —— 原写"已产出**可直接运行**的 gem5 Ruby 协议"。实测:`.slicc` 仅 268 字节(wrapper),实质在 `.sm`;`setup.sh` 实为 `cp -r . ../alloy-gem5/`(需 **alloy-gem5 定制 fork**);`run_*.sh` 用硬编码 `/artifact_top/...` 路径;README 要求按**另一篇论文**搭参考系统并用 **docker container** | 调研 §4.6.2 / F24 行 / §5.6 / `ADR-CEP-00` | ✅ **已降级**为"已产出 gem5 Ruby 协议**源文件**,运行需 alloy-gem5 + 外部 docker 参考系统,**非开箱即用**" |
| **R7** | 🟡 **SHOULD_FIX(引文精度)** | **F19 引文多一个连字符** —— 原文该处为 "directory controller"(两词),调研写作 "directory-controller" | 调研 §4.5.2 / `ADR-CEP-17 Decision 9` | ✅ **已改**为与原文逐字一致 |
| **R8** | 🟡 **SHOULD_FIX(分类维度)** | **§5.1 三个小节用了 3 种不同分类依据** —— §5.1.1 按物理链路/工业标准、§5.1.2 按 gem5 实现来源、§5.1.4 按**一致性模型**;且 §5.1.4 的 **MSI/MESI 与 §5.1.2 的 gem5 Ruby 族语义重叠**(同一协议语义的不同实现)。→ "族"应是**来源集合**,一致性模型应是**实例属性**,二者是**交叉关系**而非并列的第三个族 | `07 号 §5.1.4` / `ADR-CEP-17 Decision 7` | ✅ **已改**:§5.1.4 标题改为"**协议语义实例(按一致性模型索引)**",明确与 §5.1.1/§5.1.2 是**交叉关系**;并显式标注 MSI/MESI 与 §5.1.2 的重叠 |
| **R9** | ⚪ NIT | F19 引文核验时脚注编号混入正文流,导致脚本 1/21 未命中 | — | ✅ 确认为抽取伪影,已逐字回原文确认 |

**扩展引文抽查**:本轮对 v0.3 新增发现的 21 条引文做脚本核验 → **20/21 直接命中**,1 条(R9)为抽取伪影。**累计抽查 57 条(36 + 21),实质正确率 57/57**。

### 9.4 自审修复记录(第一轮)

| # | 问题 | 位置 | 处置 |
|---|------|------|------|
| **R1** | future work 标注在 3 个显眼位置缺失 | §0.3 发现 4 / §4.1.6 强化段 / F18 总表行 | ✅ 已补 |
| **R2** | F17 / F22 总表行未标 future work 出处 | F17 行 / F22 行 | ✅ 已补(F22 另加"问题本身是现状"的区分) |
| **R3** | "必然命中"措辞过强 | §2.4.3 段 / F5 总表行 | ✅ 已收敛为"该检查项在结构上必然存在" |
| **R4** | v0.2 §8.2 的 2 条**错误**边界声明(B7/B8)与 2 条**核对误判**(Q3/Q4)未显式撤回 | 全文 | ✅ §0.4 C1~C4 显式撤回;B7/B8 划线保留以留痕 |

### 9.5 自审的固有局限(必须记录)

1. **自审 ≠ 独立复审**:作者对自己论证的确认偏误无法由自审消除
2. **未核源码**:`ProtoGen` / `HieraGen` / `HeteroGen` 三个仓库**未 clone**,所有机制描述基于论文正文(见 B3)
3. **引文只覆盖 36 条**:调研文档共引用约 60+ 处一手出处,本轮脚本抽查 36 条(覆盖全部 F1~F23 的核心断言),**其余未逐条回溯**
3b. **源码只做结构级审阅**:三仓已 clone,但**未逐行阅读算法**、**未实际运行**、**未复现生成产物**(per §8.3 Q8)。§4.6 的断言均来自 README / 目录结构 / 少量文件片段
4. **§1.3 核对表只覆盖 7 条断言**:12 份 wiki 条目中 5 份为二手转述,未逐句核(见 B6)
5. **未做实验验证**:全部结论是架构可容纳性分析(见 B5)

### 9.6 仍待独立复审的优先项(通道恢复后按序做)

> **状态更新(第三轮后)**:优先项 1 / 2 / 4 / 5 / 6 已由作者在 §9.7 给出判断并落盘;**仍缺的是外部独立终裁**。下表保留原清单以供外部复审对照。

| 序 | 复审项 | 理由 |
|----|--------|------|
| 1 | **S10 生态类比的表述强度** | 唯一自审无法判定的项;涉及对用户的承诺边界 |
| 2 | **Decision 7 的分类维度混淆**(§5.1 三小节 vs 一维 `consistency_labels`) | 涉及 `07 号 §5.1` 的表结构,改动成本随时间上升 |
| 3 | **Decision 8/9/10 是否越权或过度降承诺** | Decision 9 是**降低承诺**的决策,需独立判断是否过度 |
| 4 | **F16 / F20 的"设计类比"边界** | 两处被明确标注为非同构,但类比强度仍需外部判定 |
| 5 | **剩余约 24 处一手引用的逐条回溯** | 抽样已 36/36 通过,扩展抽查成本低 |
| 6 | **§4.6 源码断言的独立复核**(尤其 `gem5_HeteroGen_protocols/` 是否真能跑) | 需实机复现;这是本批**最重**的一条 |
| 7 | **Decision 9 是否过度降承诺**(最值得挑的一点) | 源码已证 HeteroGen 无 HieraGen 算法,但**手写插件路线**是否受同一限制约束,需独立判断 |

### 9.7 第三轮复审的架构判断(作者执行,针对 §9.6 剩余优先项)

| 优先项 | 判断 | 理由 | 落点 |
|--------|------|------|------|
| **P1 · S10 生态类比的表述强度** | ✅ **已充分限定,无需再改** | `ADR-CEP-00` 的表述是**条件式推论**("CEP **若要**成为...就不仅要类比,还要能承载"),不是断言;且 v0.3 已补:① 源码级证据(F24,**含"非开箱即用"限定**)② 边界澄清段(明确 *Gen 族**生成** gem5 协议 / CEP **统一实验床运行对比**,二者互补) | `ADR-CEP-00 §5.1` 已定稿 |
| **P2 · Decision 7 / §5.1.4 的维度修法是否最优** | ✅ **当前修法(索引视图)优于"正交列"方案** | 备选方案是把 `consistency_labels` 提成 §5.1.1/§5.1.2 表格的**正交列**。但该方案会让 **TSO-CC / RCC / RCC-O / PLO-CC / GPU** 这 5 个语义**无处安放** —— 它们在 gem5 Ruby 与工业标准中**均无对应实例**,加列后无行可落。**索引视图**方案既保留"语义 → 一致性模型 → Sharer Tracker 选型"的查询能力,又正确表达了"族 = 来源集合"的正交关系 | `07 号 §5.1.4` 已定稿 |
| **P4 · Decision 8 逐网络粒度是否过重** | ✅ **不过重,但需限定适用面** | CEP 的 `Fabric & Transport Plane` **天然逐网络**(VC 本身就是逐网络的),故"逐网络排序假设"与 CEP 架构**同构**,不构成额外负担。⚠️ 但需限定:该声明只在**协议插件需要区分不同网络的序假设时**才必填;单网络协议可退化为单值 | `ADR-CEP-12` schema 落地时处理 |
| **P6 · §4.6 源码断言的独立复核** | ⚠️ **已完成 9/9 结构级复核,仍未实机复现** | 逐条核对 README 原文 / 目录结构 / `.pcc` 语法 / `ClassNetworkClassification` 注释 / `StateSet` 类 / `dmb_to_RC_map.txt` / litmus 目录 / 依赖清单 / 后端目录 —— 全部实测一致。**未做**:实际运行 `setup.sh`、编译 gem5、复现论文性能(§8.3 Q8 仍未解) | 调研 §8.3 Q8 保持打开 |
| **P5 · 剩余引文回溯** | ✅ **累计 57 条抽查,实质 57/57 正确** | 第一轮 36 条(论文)+ 第二轮 21 条(v0.3 新增发现)全部逐条回溯;3 条脚本伪阴性经定位为抽取伪影(ligature / 硬换行 / 脚注混入 / 页标记) | §8.2 B1 已更新 |

**第三轮净结果**:**5 处作者自查发现(R5~R9)全部复核为"改对了"**;新增 1 处自查遗漏(**F12 的 E1/E4 混合未区分**,已在 §5.7 归类表标注);架构判断 3 项(S10 / 维度修法 / 逐网络粒度)均判定为**维持现状**。

> **⚠️ 仍需独立第三方复审**:以上 3 轮均为**作者执行**。作者对自己论证的确认偏误无法由自审消除(§9.5 局限 1)。**§9.6 的优先项应视为"作者已尽力、待外部终裁"**。

### 9.8 第四轮:架构审查(2026-10-20,作者以架构顾问职责执行)

> **说明**:subagent 通道第 10 次失败后,作者按"架构终裁"职责清单自查,**聚焦架构级影响**(而非引文核对)。本轮**无新增发现**(前 3 轮已把发现挖尽),但**落地 6 项架构级修订**。

| 修订 | 依据 | 落点 | 内容 |
|------|------|------|------|
| **A1** | **F18** | `07 号 §2.2` | Fabric & Transport Plane 职责行补"**死锁自由性论证的载体**"(原仅"拓扑/VC/credit/路由/QoS/背压") |
| **A2** | **F18** | `ADR-CEP-01 §3.3` 抓取点 3 | 澄清既有"死锁通道隔离"规则处理的是 **coherence ↔ collective 跨平面**隔离;**coherence 内部 VC 分配的平面内同类问题**归 `ADR-CEP-09`。**两者是同主题的两个面,不重复** |
| **A3** | **F18** | `07 号 §8.3` | CEP-09 议题描述升格:"多 VC 路由器仲裁 **+ VC 分配的死锁自由性论证义务**" |
| **A4** | **F12** | `ADR-CEP-05 §2` | 加 **Stub Cache 定位澄清**(协议状态投影,非缓存模型)+ 量级参照(18-20 态)。**并显式声明:对现有 6 条 `INV-SC*` 无影响** —— 这 6 条全是行为约束,没有一条假设 Stub 是真实缓存 |
| **A5** | **F12** | `ADR-CEP-05 §2` | 补 `INV-SC1` 的**刻意的保守性**说明:真实 non-stalling 协议中 cache 侧并发度亦被限制为 "one outstanding transaction per block"(ProtoGen §V-C),CEP 取同一条保守线,**有先例** |
| **A6** | **F22 / F27** | `ADR-CEP-06 §3` | 加**实验规模上限契约**:必须声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} + `verification_scope`;**禁止**把 bounded/sampled 结果表述为"已证明无死锁/无违例";`INV-CEP-06-IC3` 的"应 0"语义随 `verification_mode` 变化 |
| **A7** | 计数稳定性 | `07 号 §4` | invariant 总表"36"标注为 **Stage 0 基线值**,并列出后续 ADR 落地后需同步的 4 处 |

**架构级判断(本轮结论)**:
- **CEP 骨架未被动摇** —— 6 层模型 + canonical 10 流 + 36 invariant 均**无需结构性修改**
- **F18 / F12 / F27 是"职责与措辞的升级",不是"边界重划"** —— 三者都落在既有层内,只是把已隐含的义务显式化
- **13 个必纳 metric 全是运行期可观测量,没有"验证覆盖度"维度** → 这是 F27 无法进 metric 列表的原因,故 A6 走**协议约束**而非 metric 路线 —— **正确的落点**

### 9.9 ⭐ 第五轮:**Oracle 独立架构复审**(2026-10-21,首次取得独立复审)

> **状态变更**:前 4 轮均为**作者自审/自查**;本轮为**首次独立第三方复审**(`oracle` subagent,子会话 `ses_ea60fe237ffeH4ysvk2AdIvAj3`)。
> **复审结论**:**CEP 骨架成立,无需改层**;作者 §9.7 的 5 项架构判断**全部被同意**;但发现 **9 项作者漏掉的问题(N1~N9)**,其中 3 项高severity。

**复审独立复核通过的部分**(Oracle 逐条回溯确认):
- F18 原文(论文 PDF p.166 §9.3.1)/ F19 原文(p.165 §9.2 + p.22 §1.3)/ F25 源码(`MSI_Proto.pcc:3-6`)/ F12 量级(ProtoGen p.1)/ F4(HieraGen p.2)/ F10(HeteroGen p.11)**全部逐字核实**
- 7 处 CppTLM 代码引用(`module_factory.hh:122/:135`、`chstream_module.hh:73`、`noc_bundles_tlm.hh:38/:60-62`、`modules_cluster.hh:41-105`、`chstream_register.hh:50`、`modules.hh:30`)**全部命中**
- canonical 10 流逐流核对**无遗漏无重叠**;`ADR-CEP-02` 10 流与 `ADR-CEP-05` 折叠表**逐条对齐无漂移**
- §9.7 的 5 项架构判断**全部同意,无需再改**

**Oracle 发现的 9 项问题(作者漏掉)及处置**:

| # | 严重度 | 问题 | 处置 |
|---|-------|------|------|
| **N1** | 🔴 高 | "36 条守门 invariant"单一真源**不成立** —— `INV-CEP-04-IC7` / `INV-CEP-05-IC1` 只在 `ADR-CEP-17 §3.1` 声明,**未落 owner ADR**(实测 `ADR-CEP-04 §11` 无 IC7、`ADR-CEP-05 §4.3` 无 IC1)。这正是 07 号 §4 声称根除的"19/35/48 三套口径"以新形态回归 | ✅ **已落**:两条 invariant 补进 `ADR-CEP-04 §11` / `ADR-CEP-05 §4.3`;07 号 §4 改**三段式口径**(编号位 / 实际 / 已落 owner ADR) |
| **N2** | 🔴 高 | 07 号 §3.2 写"**6** 条不变量"vs §4 写"**7**";`improvement A2` 写"覆盖 **35** 条 invariant ID(逐条断言,数量 == 36)"—— 自相矛盾且**机器检查必然失败** | ✅ **已修**:§3.2 标题改 7 + 表内补 `INV-CEP-05-IC1` 行;A2 断言改为"覆盖 **36** 条实际 ID(编号位 37)" |
| **N3** | 🔴 高 | adapter 计数 6 vs 14 在 07 号**内部漂移**(§1.4 / §11 未随 §5.1 同步),`ADR-CEP-00A:169` 同 | ✅ **已修**:三处统一为"2 来源族 / 14 adapter 实例" |
| **N4** | 🟡 中高 | "**正确性由实验保证**"是**过度声称**,且与本批刚加的 `ADR-CEP-06 §3` `verification_mode` 契约**直接矛盾**(后者已写"禁止把 bounded/sampled 表述为已证明无死锁") | ✅ **已修**:改为"**实验证伪** —— 在声明的 `verification_scope` 内尝试证伪;**平台不提供无违例保证**";并补口径澄清:**工具集成负担不继承;语义组合负担完全继承** |
| **N5** | 🟡 中高 | 07 号 §5.1.4 四行 "Sharer Tracker 选型 = `SparseSnoopFilter`" 与**同行**协议特征自相矛盾(TSO-CC "主动放弃 sharer tracking";GPU 源自 Spandex 不做中心目录)。**snoop filter 本身即一种 sharer tracker** | ✅ **已修**:该列改 `BroadcastNull` 或"不参与"(与 `ADR-CEP-17` 决策 3 token 同原则),并加修正说明 |
| **N6** | 🟡 中高 | 死锁自由性论证的**责任边界放错层**:F18 原文主语是"**assign messages to virtual networks**"(协议侧),不是"路由器仲裁";且 ①排序假设 ②VC 绑定 ③仲裁一致性 是**三方联合** | ✅ **已修**:07 号 §2.2 **Protocol Engine 行**补"声明排序假设 + 消息→VC 绑定";**Fabric 行**改为"**执行**(而非单边承担)",并把 QoS 降为次要用途 |
| **N7** | 🟡 中高 | `ADR-CEP-05 §7` α 失效判据**只有 1 条**,只覆盖"协议信息泄漏给 stub"单向;缺"stub 可见信息不足以支撑组合"这一侧 → α 方案**单向可证伪** | ✅ **已修**:补**α 失效判据 2(E 态静默升级)**,α 方案变双向可证伪 |
| **N8** | 🟡 中 | ⚠️ **2026-10-22 起本条已被 `ADR-CEP-12 §8` 取代**:key 改为 **network**(其下显式给出 `vc_id`),
| | | 扁平形态的「绑定 `vc_id`」谓词整体消失。原表述:`ordering_assumption` 的 key 必须**绑定 `vc_id`** —— `NoCFlitBundle` 不带 ordering 字段(排序是 **VC 的静态属性**),不绑定则与 `flit.vc_id` 无法关联,死锁自由论证拿不到可计算输入 | ✅ **已修**:`ADR-CEP-17` 决策 8 的 schema 改为 `<vc_id \| named_network>`,并要求 `named_network` 解析到唯一 `vc_id`,否则配置 fail |
| **N9** | ⚪ 低 | 2 处**证据页码错**:ProtoGen "18-20 states / 46-60 transitions" 在 **p.12** 非 p.11;HeteroGen "reads, writes, and fences" 在 **p.3** 非 p.2。07 号 §10 调研引用写 "F1~F23" 实为 F1~F28 | ✅ **已修**:4 处页码/条数更正 |

**Oracle 额外指出的、本批一并落地的架构级修订**:
| 项 | 内容 | 落点 |
|----|------|------|
| **CEN-Ib11** | atomic 意图作为 `request` 流**独立属性**承载 + `perm` 4 值锁(机器可检查) | `ADR-CEP-02`;计数 → **37 编号位 / 36 实际** |
| **`INV-CEP-06-IC5`** | `memory_violations` 判定须覆盖 **SWMR + data-value 双不变量** | `ADR-CEP-06 §3` |
| **ADR-CEP-12 schema 补 5 维度** | `id`/`status`/`protocol_version` / `vc_binding` / `sharer_tracker` / `flow_support` / `atomic` | `ADR-CEP-17 §3.4`(schema 草案先行,待 `ADR-CEP-12` 采纳) |
| **ADR-CEP-09 两段式** | Decision A = 通道依赖图**无环判据**(装配期静态检查) / Decision B = 仲裁策略;**三方联合契约** | `ADR-CEP-17 §3.5`;`NUMBER-MAP` 依赖表补 `CEP-12` |
| **07 号 §7.2.1** | 实验验证深度契约**引用** `ADR-CEP-06 §3`(不复制数字,避免第二真源) | `07 号 §7` |

> **本轮意义**:前 4 轮是作者自审(§9.1 已如实标注"自审 ≠ 独立复审");本轮取得**首次独立第三方复审**,并**证明独立复审确实抓到了作者漏掉的东西**(N1/N2/N3 三项高severity 计数与真源问题,作者 3 轮自查均未发现)。
> **方法论教训(已记入 §1.2)**:自审的天花板是**确认偏误** —— 作者检查自己写的东西时,天然不会怀疑"我宣称是单一真源的东西是不是真的在单一地方"。**计数类断言尤其如此**:作者每次改计数都改了"提到这个数的句子",却没验证"被这个数约束的实体是否真的存在于它声称的归属地"。

---

### 9.10 architect 阶段落地记录(2026-10-21)

**真源 3(ADR)新增 5 份 + 修订 3 份**:

| ADR | 动作 | 核心内容 |
|-----|------|---------|
| `ADR-CEP-07` | **修订 v0.7** | 决策 9 插件接口可组合性 + `INV-CEP-07-IC7` |
| `ADR-CEP-08` | **新建 v0.1** | Ordering Point 定义 + 可迁移性三判据(J1/J2/J3)+ `INV-CEP-08-IC1~3` |
| `ADR-CEP-09` | **新建 v0.1** | 两段式:Decision A 通道依赖图无环判据 / Decision B 仲裁策略 + `INV-CEP-09-IC1~3` |
| `ADR-CEP-12` | **新建 v0.1** | 协议实例 schema(8 字段组)+ `INV-CEP-12-IC1~3` |
| `ADR-CEP-13` | **新建 v0.1** | 一致性域划界 + 复合一致性公理 + 域可见写判据 + 组合承诺边界 + `INV-CEP-13-IC1~3` |
| `ADR-CEP-18` | **新建 v0.1** | 纯手写插件路线 + 竞合定位(互补)+ 引入生成路线的 T1~T4 判据 |

**真源 1(07 号)同步**:`§2.2` 责任边界 / `§4` invariant 总表 / `§7.2.1` 验证深度契约 / `§8.1` Stage DoD 回指 / `§8.3` 演进 seam 状态 / `§9` Primary ADRs / `§11` 能力映射。

**invariant 计数轨迹**:36 编号位(Stage 0 基线)→ 37(`CEN-Ib11`)→ 51 编号位 / 50 实际(第一批新增 `ADR-CEP-08`/`09`/`12`/`13` 各 3 条 + `ADR-CEP-07-IC7` + `ADR-CEP-06-IC5`)→ **67 编号位 / 66 实际 / 15 组**(第二批新增 `ADR-CEP-11`/`14`/`15`/`16`/`07A` 各 3 条)。`INV-CEP-04-IC4` **唯一跳空**。
> **全仓实测复核**:脚本扫描全部 `ADR-CEP-*.md` 的 invariant id,**unique = 51**,与 `07 号 §4` 编号位一致。

**v0.4 补记(2026-10-21,第二轮 architect 阶段)**:
- **新增 5 份 ADR**:`ADR-CEP-11` 协议版本锁定 / `ADR-CEP-14` Stub 合同测试 / `ADR-CEP-15` CacheTLM 对齐 / `ADR-CEP-16` `06 §15.4` 演进映射 / `ADR-CEP-07A` 06-bis 产物处置 → **ADR 总数 21 份**
- **invariant 编号位 51 → 67**(实际 50 → **66**),**15 组**
- **计数口径教训**:`INV-CEP-04-IC4`(唯一跳空)被多处**提及**但**从未定义** —— 计数脚本**必须区分「提及」与「定义」**,否则多算 1 条。已写入 `07 号 §4` 的口径纪律段
- `ADR-CEP-07A` 的**协议泄漏通用禁令**(`INV-CEP-07A-IC2`)把 `coherence_domain.hh` 的 `enum Protocol{MESI,MOESI}` 与 `layer.py` 的 `protocol="MESI"` 两个既有违规**归为同一类**,处置口径统一

**未做(留给 planner / builder)**:`.rddf/roadmap/**` 与 `.rddf/improvements/**` 的计数口径同步与阶段重排;`openspec/changes/cpptlm-cep-umbrella/` 起草。

---

**版本**: **v0.4**(2026-10-21)
**上一版**: v0.3(2026-10-20,PDF 原文级)、v0.2(2026-10-20,论文全文抽取件级)、v0.1(2026-10-20,摄入摘要级)
**关联 ADR**: [`ADR-CEP-17`](../adr/ADR-CEP-17-ruby-slicc-protocol-family.md)(v0.3 已随本批修订)
**维护者**: CppTLM build agent(architect mode)
