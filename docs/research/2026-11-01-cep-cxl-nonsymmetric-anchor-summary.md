# CXL 2.0 非对称 —— 公开来源锚点摘要(Task 0 普查产物)

> **日期**:2026-11-01 · **状态**:🔵 **普查完成(有条件通过)** —— 决定载体形态的三项(态数 / 事件词表 / 投影形状)**已钉死**;
> **8 项**只能靠规范正文的语义**登记为 `pending`**(走「人类成员条款摘要」流程,per 台账 Entry #122 ①-d)
> · **用途**:队列 #10 第 0 个 change(规约层)的 **basis 唯一人工审计面**(per `ADR-CEP-11` Status Update ①-c / ④)
> · **上位裁决**:台账 `obj-cep-platform.md` **Entry #122** · `docs/adr/ADR-CEP-11-protocol-version-locking.md` 的 `## Status Update(2026-11-01)`

---

## §0 范围与合规声明(⚠️ 必读)

| 项 | 内容 |
|---|---|
| **⛔ 合规禁令** | CXL 规范正文受 **Evaluation Copy Agreement §3**「USE OF ARTIFICIAL INTELLIGENCE SYSTEMS」+ **AI Acceptable Use Policy**(2026-02-12 采纳,覆盖「any **past**, current, or future CXL Specification」)+ **NOTICE TO NON-MEMBERS** 约束 ⇒ **禁止**把规范正文投喂任何 AI ⇒ 本摘要**全部**来自**公开非规范来源** |
| **未做** | 未下载 / 未打开 / 未引用任何 CXL 规范 PDF(1.0 / 2.0 / 3.x / 4.0)⇒ **零合规暴露** |
| **强度三档** | `direct`(该公开来源**明确这样写**)· `analogical`(跨协议/跨代次**类比**)· `inferred`(推理)⇒ **禁止升格**;载体侧 `basis.kind` 与此一一对应 |
| **不整段复制** | 本摘要只记**条款级/文献级定位 + 断言摘要**,**不复制原文段落**(IP 风险) |
| **上游无实现** | Linux `drivers/cxl/` 的 CXL.cache 两项均 `[0]`;QEMU 明写「not emulating a coherency protocol」⇒ **本仓无现成实现可抄** ⇒ 规约层必须自建载体 |

---

## §1 设备侧稳定态集合(**决定载体形态**)

**结论**:CXL.cache 设备侧稳定态 = **MESI 4 态**(`M` / `E` / `S` / `I`)⇒ **投影是 4→4,不是 5→4**。

| 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|
| 「Each cacheline can be in one of four 'stable' states: modified, exclusive, shared, invalid」 | **direct** | D1 ASPLOS'25 形式化论文 | arXiv `2410.15908v2` · DOI `10.1145/3676641.3715999` · CC-BY 4.0 |
| CXL.cache 用 MESI · 64B cache line · 设备缓存容量约 ≤1MB | **direct** | D2 ACM CSUR 2024 · D3 SNIA SDC 2020 | DOI `10.1145/3669900` · arXiv `2306.11227` |
| 「Modern CPU caches and CXL are built on M,E,S,I protocol/states」;「Other extended states and flows are possible but **not covered** in context of CXL」 | **direct** | D3 | PDF 稳定(**CXL 1.1 时代** ⇒ 须标版本) |
| `RdOwn` = 取所有权(grant 为 `GO-E` **或** `GO-M`);`RdOwnNoData` 必须收 `GO-E`;`RdCurr` **不**改 cache state / ownership | **direct** | D1 §2 · D3 · D15 Rambus Glossary | blog ⇒ **须 Wayback 快照 + 抓取日期** |
| 形式化模型内部用 `{M,S,I}` = **证明抽象**(把 E 并入 M),**非**协议只有 3 态 | **direct** | D1 §3.2 原文:「We do not track the distinction between exclusive state and modified state」 | — |
| Host SF 可用压缩编码 `I / S / A`(A = any MESI) | **direct** | D4 Hot Chips 2022 | ⚠️ 那是 **SF 编码**,**非** device cache 状态 |
| 公开来源**不存在** CXL.cache 的 `P` / pseudo-shared 稳定态 | **证据不足** | — | ⚠️ **「搜不到」≠「不存在」** ⇒ 登记为残余风险 R-7 |

---

## §2 事件词表与 local / remote 划分(**决定编译期断言**)

**结论**:**D2H Req = 设备发起(local)**;**H2D Req = 只有 snoop(remote,发起方恒为 Home)** ⇒ 两集合**不相交**。

| 通道 | 方向 | 承载 | 发起方 |
|---|---|---|---|
| **D2H Req** | Device→Host | `RdShared` `RdCurr` `RdOwn` `RdAny` `RdOwnNoData` `CLFlush` `CacheFlushed` `DirtyEvict` `CleanEvict` `CleanEvictNoData` `ItoMWr` `WrCur` `WOWrInv` `WrInv(F)` | **仅设备(local)** |
| **D2H Rsp** | Device→Host | `RspIHitI` `RspIHitSE` `RspSHitSE` `RspIFwdM` `RspSFwdM` `RspVHitV` `RspVFwdV` … | 设备(对 remote snoop 的应答) |
| **D2H Data** | Device→Host | 64B 数据(读响应 / 脏数据转发 / evict 写回) | 设备 |
| **H2D Req** | Host→Device | **只有 snoop**:`SnpInv` `SnpData` `SnpCurr` | **仅 Host / Home(remote)** |
| **H2D Rsp** | Host→Device | `GO`(携带设备应进入的新 `DState`)· `GO_WritePull` · `GO_WritePullDrop` · `WritePull` · `ExtCmp` · `FastGOWritePull` · `GOErrWritePull` | 仅 Host |
| **H2D Data** | Host→Device | 读完成数据 | 仅 Host |

| 断言 | 强度 | 来源 |
|---|---|---|
| 「The interface consists of **three channels in each direction**: Request, Response, and Data」 | **direct** | D2 · D3 · D15 |
| 「**H2D Requests are snoops from the host**」;「D2H Requests from the device」 | **direct** | D3 |
| snoop 三型:`SnpInv`→`I` 且回脏数据;`SnpData`→`S` 且回脏数据;`SnpCurr`→**不改态**只回当前值 + 脏数据 | **direct** | D3 · D4 |
| `GO` 携带新 `DState`;`GO_WritePull` / `GO_WritePullDrop` 用于 evict | **direct** | D1 §3.2 |
| **`Snoop-pushes-GO`**:H2D Req 不得越过同地址的 H2D Rsp(放松即失去 coherence) | **direct** | D1 §2/§3.3 |
| 同一 cacheline 同一设备同时只允许 **1 个** pending snoop(规范原文**有歧义**) | **direct**(含歧义登记) | D1 §4.1 |

### ⇒ 载体侧编译期结构断言(**V-3**,per Entry #122 ⑤)

```
H2DReqOp  ⊆ { SnpInv, SnpData, SnpCurr }
D2HReqOp  ∩ H2DReqOp = ∅
设备侧事件集中 **不存在** snoop「发起」型事件
```

### ⇒ 保守化处理(⚠️ 关键)

公开来源**只给语义名,不给编码** ⇒ 载体侧以**枚举名**表达,`kind = public-direct`,`pending = true`
(编码 / 字段宽度 / credit 规则不可得)⇒ 载体**不写死** opcode 数值。

---

## §3 `perm` 投影(**4→4**)

| CXL.cache 态 | `perm` | 对照 MESIF 表 |
|---|---|---|
| `I` | `NONE` | 同 |
| `S` | `READ` | 同 |
| `E` | `UNIQUE` | 同 |
| `M` | `WRITEBACK` | 同 |

⇒ **复用现有 MESIF 投影表前 4 行**;⚠️ **第 5 行 `F → WRITEBACK` 在 CXL.cache 无对应物 ⇒ 整行删除**
(`F` 态是 MESIF 特有;`F→E` 的**秩回落例外**同样**不可移植** ⇒ CXL 侧保留的只有
「`E + Wr → M` 静默升级严格升秩」,且该静默升级须落 `perm_upgrades.json`)。

⚠️ **强度**:4 态集合 = `direct`;**该映射本身是语义对齐(`inferred`)** —— 无任何公开文档逐字给出此表。

---

## §4 逐格覆盖矩阵(粗粒度:4 态 × 4 事件类)

| 事件类 \ 态 | `I` | `S` | `E` | `M` |
|---|---|---|---|---|
| **local read**(`RdShared` / `RdCurr`) | 可进 `S`(共享)· **公开来源 direct** | 保持 `S`(**direct**) | 保持 `E`(**direct**) | 保持 `M`(**direct**) |
| **local write**(`WrInv(F)` / `WOWrInv` / `ItoMWr`) | 可进 `E` / `M`(**direct**:`RdOwn` grant `GO-E`/`GO-M` 存在 ⇒ 写取所有权可行) | 须先降级(`S` + 写 ⇒ `M`,**analogical**:MESI 通用性质) | `E + Wr → M`(**direct**:静默升级,**strict 升秩**) | 保持 `M`(**direct**) |
| **remote snoop**(`SnpInv` / `SnpData` / `SnpCurr`) | 保持 `I`(**direct**) | `SnpInv` → `I`;`SnpData` → `S`(**direct**) | `SnpInv` → `I` + 回脏数据(**direct**) | `SnpInv`/`SnpData` → `I` + **回脏数据**(M→I 转发 / M→S 转发)(**direct**) |
| **evict**(`DirtyEvict` / `CleanEvict` / `CleanEvictNoData`) | — | — | `CleanEvict`(**direct**) | `DirtyEvict`(**direct**) |

⇒ ⚠️ **逐格穷举(4 态 × 全部 opcode)需规范正文** ⇒ 本矩阵是**结构级**的,载体侧仍须逐格标 `pending` + `ref`。
⇒ 「**MESI 通用性质**」类格(S + 写 → M 等)= `analogical`,**不得**标 `direct`。

---

## §5 「非对称」的**可判定义**(三维度)

| 维度 | 定义 | 强度 | 锚点 |
|---|---|---|---|
| **目录侧** | sharer 视图在 **Host 侧 Home / Snoop Filter** 聚合;Host **必须**具备「跟踪设备可能缓存的每个地址」的结构 | **direct** | D2:「the host **must have snoop filter structure** that will track each address that the device may be caching」 |
| **snoop 发起侧** | **仅 Host / Home** 发起 snoop(经 H2D Req);设备侧**无** snoop 发起型事件 | **direct** | D3:「H2D Requests are snoops from the host」· D1:「CXL.cache is an asymmetric protocol, with coherence-related messages between *devices* all going via a central *host*」 |
| **设备态集** | **MESI 4 态**;设备**不感知**其他设备的 sharer | **direct** | D1 · D2:「the host manages all tracking of coherence for peer caches and **the device never directly interacts with any peer cache**」 |

⚠️ **精确表述**(= ADR Status Update ①③):**host-centric / 单一一致性点**;
「设备互不感知」在 **CXL 2.0 平凡成立**(每 root port 只 1 个 cache device)⇒ 不得当作 2.0 的**区分性**语义。

⚠️ **SF 精度**(`CONSERVATIVE` = 宁多探不漏探):公开 CXL 来源**只有「义务性」表述**(必须能跟踪每个地址),
**判定规则无公开原文** ⇒ 只能从 CHI / CCI 的 imprecise-SF 退化策略**类比** ⇒ **不得**标 `direct`。

⚠️ **形式化模型的 caveat**:ASPLOS'25 假设 host **perfect tracking** ⇒ **不得**当作
`SparseSnoopFilter`(`CONSERVATIVE`)的**精度**依据 ⇒ 只可作**协议骨架**(消息 / 状态 / 排序)。

---

## §6 8 项残余风险(**只能靠规范正文确定** ⇒ 禁止推理填空)

| # | 项 | 处置 |
|---|---|---|
| R-1 | opcode **编码** / 字段宽度 / credit 规则 / channel 级排序 | 载体用**枚举名** + `pending` |
| R-2 | 「15 条 request」的**权威枚举**与**逐状态合法性表**(公开 deck 间已有 `MemWr`↔`WrCur` 差异) | 登记 + 人类摘要 |
| R-3 | **transient state** 的官方命名 / 编码(D1 明说「not officially part of the CXL.cache specification」) | 载体**不建模** transient ⇒ 只登记稳定态 |
| R-4 | **错误语义**(`GO-Err` / poison / viral / timeout isolation) | 登记;实现层**不预支** |
| R-5 | **SF completeness** 的**规范性判定**(允许多大 over-snoop) | 只标 `analogical`(CHI / CCI 类比) |
| R-6 | **版本记账**(`CacheID` / 16 device per root port / 68B vs 256B flit 属 2.0 还是 3.0) | 登记;载体只依赖**已钉死**部分 |
| R-7 | 是否存在 **P / pseudo / 厂商扩展态** | ⚠️ **「搜不到」≠「不存在」** ⇒ 登记 |
| R-8 | **HDM-DB** MetaValue 编码 / BISnp / BIRsp opcode | **队列 #12** 范围(本 change 不预支) |

⇒ 处置流程:**人类成员提供条款摘要**(AI 不接触正文)⇒ 摘要入本文件 ⇒ 载体 `pending` 翻转 + 二方复核。

---

## §7 普查结论(硬门判定)

| 判定项 | 结果 |
|---|---|
| 设备侧态数 | ✅ **钉死 = 4**(M/E/S/I)⇒ 投影 **4→4** |
| 事件词表与 local/remote 划分 | ✅ **钉死**(结构级;opcode 编码留 R-1) |
| 投影形状 | ✅ **钉死 = 4→4**(`F` 行不存在) |
| 「非对称」可判定义 | ✅ **钉死**(三维度) |
| 编译期结构断言 | ✅ **可写**(V-3) |
| 8 项细粒度语义 | 🔴 **pending**(R-1~R-8)⇒ **不阻塞**载体结构,但**逐格**必须标 `pending` + `ref` |

⇒ **硬门判定 = 有条件通过** ⇒ **允许**建立 `cpptlm-cep-cxl-nonsymmetric-state-spec` change 目录
⇒ 但载体**逐格**必须:`basis.kind` ∈ {`public-direct`, `public-analogical`, `archforge-anchor`, `self-derived`}
+ `pending` 与 kind 一致 + `ref` 指向**本文件的条目号**(不指向规范原文)。

---

## §8 来源清单(钉版信息)

| # | 来源 | 类型 | 定位 | 强度上限 | 钉版状态 |
|---|---|---|---|---|---|
| D1 | Tan / Donaldson / Wickerson, *Formalising CXL Cache Coherence*, ASPLOS'25 | 同行评审论文(CC-BY 4.0) | arXiv `2410.15908v2` · DOI `10.1145/3676641.3715999` | direct | ⚠️ sha256 **待算**;源码仓 `github.com/ChengsongTan/CXLcacheFormalisation`(**本次 clone 失败** ⇒ 待补 commit SHA) |
| D2 | Sharma / Blankenship / Berger, *An Introduction to the CXL Interconnect*, ACM CSUR 2024 | 同行评审综述 | DOI `10.1145/3669900` · arXiv `2306.11227` | direct | ⚠️ sha256 待算 |
| D3 | Blankenship (Intel), *CXL Cache Mem Protocols*, SNIA SDC 2020 | 公开会议 deck | `snia.org/…/130-Blankenship-CXL-1.1-Protocol-Extensions.pdf` | direct | ⚠️ sha256 待算;⚠️ **CXL 1.1 时代** ⇒ 标版本 |
| D4 | *CXL 3.0 Coherence Deep Dive*, Hot Chips 2022 | 公开 tutorial | `hc34.hotchips.org/…/Hot Chips 2022 CXL3 Coherence Deep Dive.pdf` | direct | ⚠️ sha256 待算;© CXL Consortium ⇒ **引用需节制** |
| D5 | Intel, *Xeon Scalable Uncore Performance Monitoring RM* | vendor-public | `cdrdv2-public.intel.com/671389/336274-…pdf` | analogical | 文档号已钉 |
| D6 | Intel, *DDIO Performance Monitoring* | vendor-public | `intel.com/…/ddio-analysis-performance-monitoring.html` | analogical | ⚠️ 网页可能改版 ⇒ **须存快照** |
| D7 | Intel, *Xeon Scalable Family Technical Overview* | vendor-public | `intel.com/…/xeon-processor-scalable-family-technical-overview.html` | analogical | ⚠️ 同上 |
| D8 | Arm, *CCN-502 TRM — Fully-coherent Home Node (HN-F)* | vendor-public | `support.arm.com/documentation/100052/…` | analogical | 文档 ID + revision 已钉 |
| D9 | Arm, *CCI-500 TRM — Snoop filter* | vendor-public | `support.arm.com/documentation/100023/…` | analogical | 同上 |
| D10 | Arm, *Introducing AMBA CHI* | vendor-public | `documentation-service.arm.com/…/68590853961937560be90eb2` | analogical | 同上 |
| D11 | Linux kernel, *CXL Devices and Protocols* | oss-impl | `kernel.org/doc/html/latest/driver-api/cxl/devices/device-types.html` | direct(负面证据) | kernel version 已钉 |
| D12 | Linux kernel, *CXL Maturity Map* | oss-impl | `docs.kernel.org/driver-api/cxl/maturity-map.html` | direct(负面证据) | ⚠️ 两项均 `[0]` ⇒ 内核**未实现** |
| D13 | QEMU, `docs/system/devices/cxl.rst` | oss-impl | `github.com/qemu/qemu/blob/master/docs/system/devices/cxl.rst` | direct(负面证据) | QEMU tag 已钉 |
| D14 | Isabelle 形式化 artifact | oss-impl | `github.com/ChengsongTan/CXLcacheFormalisation` | direct | ⚠️ **本次 clone 失败** ⇒ commit SHA **待补** |
| D15 | Rambus, *CXL Glossary* | vendor-public | `rambus.com/interface-ip/cxl-glossary` | direct | ⚠️ **blog 易变** ⇒ **须 Wayback 快照 + 抓取日期** |
| D16 | Cadence, *Navigating Cache Coherence: BISnp in CXL 3.0* | vendor-public(blog) | `community.cadence.com/…/navigating-cache-coherence…` | direct | ⚠️ 同上 |
| D17 | *A Novel Extensible Simulation Framework for CXL-Enabled Systems* | 同行评审论文 | arXiv `2411.08312v1` | direct | ⚠️ sha256 待算 |
| D18 | *A Programming Model for Disaggregated Memory over CXL* | 同行评审论文 | arXiv `2407.16300v3` | direct | ⚠️ sha256 待算 |

⇒ ⚠️ **钉版缺口**:**sha256 全部待算** + D14 commit SHA 待补 + 6 个 blog/网页类来源的**快照 URL + 抓取日期**待补
⇒ ⇒ 载体 `--check` 的「摘要 ↔ 载体」比对**可先跑通**(结构层),但**钉版完整性**须在载体编码**之前**补齐
⇒ **责任**:Task 0 的第 2 步(钉版补齐)—— 可与 change 目录建档**并行**,但**载体逐格编码**须等它完成。

---

## §9 关联

- 台账 `.rddf/roadmap/objectives/obj-cep-platform.md` **Entry #122**(口径裁决 + 授权记录 + 8 项残余风险)
- `docs/adr/ADR-CEP-11-protocol-version-locking.md` 的 **`## Status Update(2026-11-01)`**(裁决 7 条)
- `docs/adr/ADR-CEP-04-*.md`(§2 caps · §4 hazard 契约)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值)· `ADR-CEP-05`(投影)
- `docs/architecture/07-coherence-exploration-platform.md` §5.1.1(协议族表)· `14-coherence-parity-baseline.md`(H7 范围限定 + R-B 失效区)
- ArchForge `docs/microarchitecture/snoop_filter.md` §0.2(caps 逐字段一致的**唯一**跨仓锚)· `ADR-SOC-21` D2(不做 CXL 栈)
- 草稿 `.rddf/improvements/cpptlm-cep-cxl-nonsymmetric.md`(2026-11-01 修订版)
