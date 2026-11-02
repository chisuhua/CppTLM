# CXL 3.0 对称(DCOH)—— 公开来源锚点摘要(Task 0 普查产物)

> **日期**:2026-11-02 · **状态**:🟡 **普查完成(有条件通过)** —— 5 项载体形态决定项中 **4 项可结构钉死**;
> **1 项(`aggregation_at` 二元)出现公开来源反证** ⇒ **需 architect 复核**(阻塞项);
> **10 项**细粒度语义只能靠规范正文 ⇒ 登记 `pending`(走「人类成员条款摘要」流程,per 台账 Entry #122 ①-d)
> · **用途**:队列 #12 `cpptlm-cep-cxl-dcoh-symmetric` 的 **basis 唯一人工审计面**
> (per `ADR-CEP-11` Status Update ①-c / ④;照队列 #10 `2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md` 范式)
> · **上位裁决**:台账 `obj-cep-platform.md` **Entry #122** · `docs/adr/ADR-CEP-11-protocol-version-locking.md` 的 `## Status Update(2026-11-01)`
> · **⚠️ 本轮附带发现**:仓内 **4 处口径错误** + **1 处 ADR 决策被公开来源反证** ⇒ 见 **§6**(architect 裁决面)

---

## §0 范围与合规声明(⚠️ 必读)

| 项 | 内容 |
|---|---|
| **⛔ 合规禁令** | CXL 规范正文受 **Evaluation Copy Agreement §3**「USE OF ARTIFICIAL INTELLIGENCE SYSTEMS」+ **AI Acceptable Use Policy** 约束 ⇒ **禁止**下载 / 打开 / 引用 / 摘抄任何 CXL **规范 PDF**(1.0 / 1.1 / 2.0 / 3.0 / 3.1 / 4.0)⇒ **禁止**把规范正文投喂任何 AI(含 AI agent 自身) |
| **未做** | 未打开 / 未下载 / 未引用任何 CXL 规范 PDF ⇒ **零规范正文暴露** |
| **⚠️ 检索中遇到的规范来源(已排除)** | ① 搜索结果返回 `computeexpresslink.org/wp-content/uploads/2024/02/CXL-3.0-Specification.pdf` 链接 + 一段含章节号的规范正文片段 ⇒ **立即停止采用**,未打开该 PDF,未把片段用于任何断言;② `SNIA-SDC23-CXL-Enabling-an-interoperable-ecosystem.pdf` 对 curl / webfetch 均 **403** ⇒ 登记为排除;③ Cadence 原博客 **403**(Cloudflare)⇒ 未绕过,改用**公开转载摘要**(强度降级) |
| **允许来源** | ① CXL Consortium **公开白皮书** ② **公开**会议 tutorial / deck(FMS / MSST / Hot Chips)③ **同行评审**论文(arXiv / ACM)④ 厂商**公开**博客 ⑤ 二手技术笔记(仅弱来源) |
| **强度三档** | `direct`(来源明确这样写)· `analogical`(跨代次 / 跨协议类比)· `inferred`(推理)⇒ **禁止升格** |
| **钉版三档** | `pin=strong`(本环境取到字节 + sha256)· `pin=weak`(仅 URL + 抓取日)· `pin=transcribed`(原文不可得,仅二手转述) |
| **两轴规则**(照 #10 §8.2) | 证据强度轴(`kind`)与钉版强度轴(`pending`)**必须拆开**;`pending=false` SHALL **同时**满足 ① `pin=strong` **且** ② **引用 / 再分发许可已人工确认** ⇒ 仅 **S5** 与 **S7**(均 CC-BY 4.0)可支撑 `pending=false`;© CXL Consortium 的 **S1 / S2 / S3 / S6** 即使 `pin=strong` 本轮**一律标 `pending=true`**(引用范围待人工确认) |
| **不整段复制** | 只记**文献级 / 条款级定位 + 断言摘要**,不复制原文段落(IP 风险) |
| **上游无实现** | CXL 3.0 的 HDM-DB / BISnp 机制在本仓**无现成实现可抄** ⇒ 载体须**自建**(但语义有 basis) |

---

## §1 设备侧稳定态集合(**决定载体形态**)

**结论(分层)**:
1. **CXL.cache 缓存代理(设备自己的 Dev $)稳定态 = MESI 4 态(M/E/S/I)**,与 CXL 2.0 **相同**,CXL 3.0 未改;
2. **DCOH 不引入新的缓存稳定态**;它引入一套 **3 态 coherence metadata**(`I / S / A`,A = host 可能处于 M/E/S/I 的**集合**);
3. 多 host 共享内存的 **home 目录**另有一套 **3 态(`I / S / E`)+ sharing list**;
4. ⚠️ **DCOH ≠ "Device Cache Ownership History"** —— 公开来源的展开是 **Device Coherence (agent)** / **Device Coherence Engine**;它记录**当前** host 缓存状态,**不记录历史**。

| 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|
| CXL.cache 设备缓存侧有 **4 个 stable state**(modified / exclusive / shared / invalid);CXL.cache 是 **asymmetric** 协议 | **direct** | **S5** p.3 | ✅ `pin=strong` + CC-BY ⇒ **`pending=false`** |
| 设备 caching agent 执行 **MESI**;CXL 采用 **asymmetric** coherency approach | **direct** | **S4** p.4–5 | `pending=true`(许可 absent) |
| CXL 3.0 的「symmetric coherency」作用域 = **HDM(host-managed device-attached memory)**,**不是** CXL.cache 全协议 | **direct** | **S1** p.3 · **S2** p.91 · **S3** p.18 | `pending=true` |
| **DCOH = Device Coherence (agent)**,位于设备内,管理 / 跟踪设备与 host 之间的一致性 | **direct** | **S4** p.12 | `pending=true` |
| **DCOH = Device Coherence Engine**,是 HDM-D* 内存上 host 与 device 访问的**最终冲突仲裁者** | **direct** | **S6** p.30 | `pending=true` |
| HDM-D 通过 **MetaValue** 向设备暴露 host 缓存状态;取值 = **Any(M/E/S/I) / Shared(S 或 I) / Invalid(I)** | **direct** | **S6** p.30 | `pending=true` |
| HDM-DB(CXL 3.0 新增)在 CXL.mem 增 **BISnp(S2M)+ BIRsp(M2S)**;用 **inclusive snoop filter** 跟踪 host 缓存 | **direct** | **S6** p.25 / p.39–40 · **S4** p.20–22 | `pending=true` |
| HDM-DB 的 bias / SF 状态仍是 **I / S / A**(A = host 可为 M/E/S/I) | **direct** | **S6** p.39 | `pending=true` |
| 多 host 共享内存的 home 目录用 **2 bit 的 Invalid / Shared / Exclusive + sharing list** | **direct** | **S4** p.21 | `pending=true` |
| 公开来源**未见** CXL 3.0 新增第 5 个缓存稳定态 / pseudo-shared / 厂商扩展态 | **证据不足** | — | ⚠️「搜不到 ≠ 不存在」⇒ **R-S9** |

### ⇒ 载体侧结构断言(V-1)

```
DevCacheState = { M, E, S, I }     // 4 —— 喂 perm 投影
DcohViewState = { I, S, A }        // 3 —— host 缓存状态摘要;A 是**集合**,不是单态
HomeDirState  = { I, S, E }        // 3 —— 多 host home 目录 + sharing list
// 三个状态空间 SHALL NOT 合并;只有 DevCacheState 进入 perm
```

---

## §2 事件词表与 local / remote 划分(**决定编译期断言**)

**结论**:
- **CXL.cache 通道不变**:双向各 3(Req / Rsp / Data);`H2D Req = host 发起的 snoop`,`D2H Req = 设备发起`;
- **CXL.mem 通道 = 4 基础 + 2 新增**:`M2S Req`、`M2S RwD`、`S2M NDR`、`S2M DRS` + **`S2M BISnp`** + **`M2S BIRsp`**;
- **local(设备发起)**:`D2H Req/Rsp/Data` + `S2M NDR/DRS/BISnp`;
- **remote(host 发起)**:`H2D Req/Rsp/Data` + `M2S Req/RwD/BIRsp`;
- ⚠️ **通道级不相交成立**(方向性通道);**事件种类级不相交不成立** —— host 与 device **都可以**发起 snoop 种类(`H2D Req` vs `S2M BISnp`);
- ⭐ **唯一机械判别式**:`device_initiated_snoop_set = { BISnp }`(非空,恰 1 类)—— 正是 #10 `device_snoop_initiators = ∅` 的**否定**。

| 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|
| CXL.cache 每方向 3 通道;H2D Req 是 host 的 snoop;D2H Req 是设备请求 | **direct** | **S5** p.3 · **S4** p.9 | ✅ S5 `pending=false` |
| CXL.mem 基础通道 4 条:`M2S Req`、`M2S RwD`、`S2M NDR`、`S2M DRS` | **direct** | **S4** p.11 · **S6** p.25 | `pending=true` |
| CXL 3.0 新增 `S2M BISnp` + `M2S BIRsp`;BIRsp 预分配(deadlock freedom) | **direct** | **S4** p.22 · **S6** p.25 | `pending=true` |
| **BISnp 的发起方 = host HDM-DB 内存的设备(Type-2/3)**,目标是 host | **direct** | **S2** p.16 · **S3** p.18 · **S6** p.39 · **S4** p.21 | `pending=true` |
| 该设备「monitor memory accesses;若有一致性冲突,则产生 BI 到 host」 | **direct** | **S3** p.18 · **S2** p.16 | `pending=true` |
| ⇒ CXL 3.0 对称下**存在设备发起的 snoop**(与 CXL 2.0 的「设备侧无 snoop 发起」相反) | **direct** | 上两条 | `pending=true` |
| 设备**不直接**与 peer cache 交互(CXL.cache 语义,3.0 未改) | **direct** | **S4** p.9 | `pending=true` |
| 通道级 local ∩ remote = ∅ | **direct**(方向性通道的结构事实) | 通道定义 | — |
| 事件种类级 local ∩ remote ≠ ∅(snoop 两侧都有) | **inferred** | 由上述两条推出 | `self-derived` ⇒ `pending=true` |
| 多 host 规模:GFD / GFAM 目录;`CacheID` 为 4-bit,单 root port 最多 16 个 CXL.cache 设备 | **direct** | **S4** p.21 / p.23 · **S2** p.20 | `pending=true` |
| GFD 规模口径 **4095(S2/S3)vs 4096(S4)** —— 两来源不一致 | **direct**(分歧本身) | **S2** p.20 · **S3** p.21 · **S4** p.21 | 登记 **R-S8** |

### ⇒ 载体侧编译期结构断言(V-2)

```
D2HReqOp ≠ ∅ ∧ H2DReqOp ≠ ∅
H2DReqOp ⊆ { SnpInv, SnpData, SnpCurr }        // CXL.cache,沿用 #10
device_initiated_snoop_set == { BISnp }        // ⭐ 对称判别式(非空,size 1)
channel_direction(local) ∩ channel_direction(remote) = ∅
// ⚠️ 禁止沿用 #10 的「设备侧不存在 snoop 发起型事件」断言
```

---

## §3 `perm` 投影形状

**结论**:**不是单一 4→4,也不是 3→3,而是「按域分离」**:

| 域 | 状态空间 | 投影 | 说明 |
|---|---|---|---|
| **CXL.cache 缓存代理(Dev $)** | `{M,E,S,I}` = 4 | **4→4**(`I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK`) | **沿用 #10 §3 投影表**;`F` 行仍不存在 |
| **DCOH bias / SF 视图** | `{I,S,A}` = 3 | **不可单值投影**:`A → {READ, UNIQUE, WRITEBACK}` | A 是**集合** ⇒ **不得**塞进 `perm` 4 值 |
| **多 host home 目录** | `{I,S,E}` = 3 | **3→3**(`NONE` / `READ` / `UNIQUE`) | `E` 是否含 dirty(M)**未知** ⇒ **R-S5** |

⇒ **`perm` 枚举维持 4 值锁**(per `ADR-CEP-02` CEN-Ib11),**不新增第 5 值**;
⇒ DCOH / home 目录的 3 态属 **tracker view / coherence metadata**,**不得**折叠进 `perm`。

| 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|
| CXL.cache 设备侧 4 态集合 | **direct** | **S5** p.3 · **S4** p.4–5 | ✅ S5 `pending=false` |
| `perm` 4→4 映射本身 | **analogical**(语义对齐;无公开文档逐字给出该表) | #10 §3 | `pending=true` |
| DCOH bias 三态 `I/S/A`;`A` = host 可处于 M/E/S/I | **direct** | **S6** p.30 / p.39 | `pending=true` |
| `A` 无法投影为单一 `perm` 值 | **inferred** | 由「A 是集合」推出 | `self-derived` ⇒ `pending=true` |
| home 目录三态 `I/S/E` + sharing list | **direct** | **S4** p.21 | `pending=true` |
| home 目录的 `E` 是否区分 dirty / M | **证据不足** | — | **R-S5** |
| 若把 home 目录当状态载体 ⇒ 实例 `perm` 可能填 **3 值** | **analogical** | **S4** p.21(同 TileLink 形态) | `pending=true`,**需 architect 裁决** |

---

## §4 「对称」的**可判定义**

> ⚠️ **前置警告**:CXL 3.0 的 "symmetric" **不是**经典意义的 symmetric
> (经典定义见 **S4** p.4:「Every node … would implement the same coherency algorithms and track required state」)。
> CXL 的 "symmetric coherency" 是**窄义特性名**:**设备可作为 HDM 的一致性 home,并反向 snoop host**。
> ⇒ 载体**不得**把 "symmetric" 写成 "peer-to-peer 全对称"。

```
cxl3_symmetric(p) :=
      P1  ∃ device-originated back-invalidate snoop channel (S2M BISnp), target = host
    ∧ P2  the HDM-DB hosting device implements a snoop filter / directory over its HDM range
    ∧ P3  hardware coherence spans ≥ 2 independent hosts on the same physical line
    ∧ P4  P2P access to HDM-DB resolves coherence without going through the host CPU
    ∧ P5  the snooped agent is a caching host, not a peer device cache
```

| 谓词 | 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|---|
| **P1** | 设备(Type-2/3)可发起 `S2M BISnp` 到 host | **direct** | **S2** p.16 · **S3** p.18 · **S6** p.39 | `pending=true` |
| **P2** | host HDM-DB 的设备实现 SF / 目录,覆盖其 HDM 地址范围 | **direct** | **S1** p.3 · **S4** p.20 · **S7** p.1 | S7 CC-BY ⇒ 该子句可 `pending=false` |
| **P3** | 硬件一致性跨多个独立 host;同一物理行可被多 host 同时缓存 | **direct** | **S1** p.3 · **S4** p.20–21 · **S7** p.1 | 同上 |
| **P4** | P2P 到 HDM-DB 不经 host CPU;由内存设备解析一致性 | **direct** | **S4** p.20 · **S6** p.43 · **S2** p.16 | `pending=true` |
| **P5** | snoop 目标是 host(memory controller back-snoops the host processor),**不是** peer device cache | **direct** | **S4** p.21 · **S6** p.39 | `pending=true` |
| **P6(否定)** | 所有节点实现同一套一致性算法 —— CXL 3.0 **不满足** | **direct**(否定) | **S4** p.4 | `pending=true` |
| **P7(反证)** | 「归属基于 DPA 而非 PA」—— **反证**:CXL.mem 用 **HPA**,设备内部翻译到 media address | **direct**(反证) | **S4** p.11 | `pending=true` |
| **P8(未支持)** | 设备可缓存 peer device 内存并**直接 snoop peer** | **证据不足 / 未见** | — | **R-S8** |
| **P9(命名)** | CXL 3.0 白皮书 / FMS / MSST 均称该特性为 "symmetric coherency" | **direct** | **S1** p.3 · **S2** p.91 · **S3** p.18 | `pending=true` |

### ⇒ 判定流程(建议写入载体)

1. `device_initiated_snoop_set == ∅` ⇒ **asymmetric**(CXL 2.0 形态)
2. `device_initiated_snoop_set == {BISnp}` **且** P2 ∧ P3 ∧ P4 ∧ P5 ⇒ **cxl3_symmetric**
3. 存在 P8(peer-device snoop)⇒ **超出 CXL 3.0 公开语义** ⇒ 需另行裁决(当前公开来源**不支持**)

---

## §5 编译期断言可锁的结构

| 项 | 能否静态锁死 | 锁什么 | 依据 / 备注 |
|---|---|---|---|
| 缓存稳定态数 | ✅ **能** | `kDevCacheStates == 4`(M/E/S/I) | S5 CC-BY ⇒ `pending=false` |
| DCOH 视图态数 | ✅ **能** | `kDcohViewStates == 3`(I/S/A) | S6 `direct`,`pending=true` |
| home 目录态数 | ✅ **能** | `kHomeDirStates == 3`(I/S/E) | S4 `direct`,`pending=true` |
| CXL.mem 通道数 | ✅ **能** | `kCxlMemChannels == 6`(4 基础 + BISnp + BIRsp),以**枚举名**表达(**不写死 opcode**) | S4 p.22 / S6 p.25 |
| CXL.cache 通道数 | ✅ **能** | 双向各 3(Req/Rsp/Data) | 沿用 #10 |
| 设备发起 snoop 集合 | ✅ **能** | `device_initiated_snoop_set == {BISnp}`(非空,size 1) | ⭐ **对称判别式**;反向 fixture:置空 ⇒ 必须红 |
| local/remote 通道不相交 | ✅ **能** | `local_channels ∩ remote_channels == ∅`(按方向) | 结构事实 |
| `perm` 行数(缓存代理域) | ✅ **能** | `perm_rows == 4`,且 `F` 行不存在 | 沿用 #10 |
| **`aggregation_at` 二元** | ❌ **不能照原样锁** | 公开来源只支持 `HOME`;`P2P ⇒ REQUESTER` **未被印证且出现反证** | ⚠️ **需 architect 复核**(见 §6) |
| DCOH 的 `Authority` / `Completeness` / `broadcast_fallback` | 🟡 **部分能** | `STALL` 可锁(S6 p.39–40:「may block M2S Request waiting for BISnp」);`CONSERVATIVE` / `LOCAL_HINT` 只能 `analogical`;`broadcast_fallback=true` **未见依据** | 见 §6 |
| opcode 编码 / 字段宽度 / credit / 排序 | ❌ **不能** | — | **R-S1 / R-S2** |
| BISnp block 编码(128B / 256B) | ❌ **不能** | 公开只给「2×128B 或 4×256B」 | **R-S2** |
| SF 精度策略(exact vs imprecise / 4KB 粒度) | ❌ **不能** | 公开只说「imprecise SF 会发多余 BI」 | **R-S5** |
| transient 态 / 错误语义 / poison / viral | ❌ **不能** | — | **R-S6 / R-S7** |

---

## §6 ⚠️ 本轮附带发现:仓内 **4 处口径错误** + **1 处 ADR 决策被反证**(architect 裁决面)

> **性质**:本节是**只读的仓内交叉核对**结果(非公开来源)⇒ 全部登记为**待裁决**,**本轮不改任何仓内文件**。
>
> ✅ **2026-11-02 后更新**:**5 项已全部裁决并落地** —— ①~④ 由 architect 裁决后**正文订正**(07 号 `Status Update(2026-11-02)`,per 台账 **Entry #154**);⑤(`ADR-CEP-04 §3.5` 的 `P2P ⇒ REQUESTER`)⇒ 追加 **ADR 第 3 则 Status Update** + 07 号 / `sharer_tracker.hh` 的 `aggregation_at` 收敛为 **HOME**。队列 #12 归档后的**状态追平**见台账 **Entry #167**(07 号 4 处状态失实 + 「残留 A」错指订正)。
> ⇒ 本节**不再是待裁决清单**,保留为**核对过程记录**(审计面)。

| # | 仓内位置 | 原文断言 | 公开来源核对结果 | 处置 |
|---|---|---|---|---|
| **1** | `docs/architecture/07-coherence-exploration-platform.md:318` | 「DCOH(**Device Cache Ownership History**);设备可主动反向 snoop;**基于 DPA 而非 PA** 归属」 | ❌ **三处均不成立 / 未印证**:① DCOH 公开展开 = **Device Coherence (agent / engine)**,**不是** Ownership History;② 「反向 snoop」只对 **host**,不对 peer device;③ CXL.mem 用 **HPA**,设备内部翻译到 media ⇒ **不是 DPA 归属**(S4 p.11 反证) | 冻结面 ⇒ 需 **architect 裁决 + Status Update**(不改写正文) |
| **2** | `docs/architecture/07-...:177` | 「CXL 3.0 BISnp 走 `back_invalidate` 流,**host→device**」 | ❌ **方向反了**:BISnp 是 **S2M(device → host)**;`M2S BIRsp` 才是 host → device(S4 p.22 · S2 p.16) | 同上 |
| **3** | `docs/adr/ADR-CEP-04-...` §3.5 | 「HDM P2P ⇒ `REQUESTER`;HDM home ⇒ `HOME`」 | 🟡 **home ⇒ HOME 印证**;**P2P ⇒ REQUESTER 反证** —— S4 p.21 明写 **memory device** 发 BISnp、**等两个 host 的 BIRsp**、再更新目录 ⇒ 聚合点在 **HOME**,不在 requester | ⚠️ **ADR 决策被反证** ⇒ 需 **architect 复核**(ADR 不可变 ⇒ 只能追加 SU) |
| **4** | `include/tlm/cep/sharer_tracker.hh:207-210` | `DcohSymmetric` = `LOCAL_MEMORY_REGION` / `CONSERVATIVE` / `LOCAL_HINT` / `AggregationAt::REQUESTER` + `STALL` + `broadcast_fallback=true` | 🟡 **部分印证**:`STALL` ✅(S6 p.39–40);`LOCAL_MEMORY_REGION` ✅(HDM 区域);`CONSERVATIVE` 🟡(imprecise SF 会 over-snoop,S7 p.1);`LOCAL_HINT` 🟡(DCOH 亦被称为「最终仲裁者」,S6 p.30);`AggregationAt::REQUESTER` ❌(同 #3);`broadcast_fallback=true` ❌ **未见依据** | 需 architect 复核(前 4 项是 traits 声明,**未落地行为实现** ⇒ 可随规约层一并订正) |

---

## §7 10 项残余风险(**只能靠规范正文确定** ⇒ 禁止推理填空)

| # | 项 | 处置 |
|---|---|---|
| **R-S1** | `BISnp` / `BIRsp` 的 **opcode 编码**、字段宽度、credit 规则、channel 级排序 | 载体用**枚举名** + `pending` |
| **R-S2** | **BISnp 消息变体**(block BISnp 是否独立 opcode)与 **BIRsp payload** 语义;block 粒度 128B / 256B 的编码 | 登记 + 人类摘要 |
| **R-S3** | DCOH bias 表的**精确编码**(1 bit 还是 2 bit;第 4 个 code 是否保留);`A` 是否严格等于 `{M,E,S,I}` | 登记(2-bit 说法仅来自二手) |
| **R-S4** | HDM-DB 是「**替换** bias-based」还是「**复用** bias table 状态」—— S1 p.3 与 S6 p.39 措辞张力 | 登记;载体**分别**建模两个 3 态空间,**不合并** |
| **R-S5** | home 目录 `I/S/E` 是否区分 **dirty / M**;sharing list 编码;SF 精度(exact vs imprecise)的规范性要求 | 登记 |
| **R-S6** | **transient state** 的官方命名 / 编码;BI 与 M2S Req 的排序规则细节 | 载体**不建模** transient ⇒ 只登记稳定态 |
| **R-S7** | **错误语义**(BI timeout / poison / viral / isolation) | 登记;实现层**不预支** |
| **R-S8** | **peer 集语义**:home 是否可能 snoop **peer device cache**(公开来源只说 host);GFD 4095 vs 4096;MLD 32 | 登记 + 人类摘要 |
| **R-S9** | 是否存在 **P / pseudo / 厂商扩展态**;CXL 3.1 / 4.0 是否新增状态 | ⚠️「搜不到 ≠ 不存在」 |
| **R-S10** | **`aggregation_at` 二元**(`HDM P2P ⇒ REQUESTER`)的规范性依据 | ⚠️ **architect 复核**(公开来源反证指向 `HOME`) |

⇒ **处置流程**:**人类成员提供条款摘要**(AI 不接触正文)⇒ 摘要入本文件 ⇒ 载体 `pending` 翻转 + 二方复核。

---

## §8 来源清单(钉版信息)

| ID | 标题 | URL | 类型 | 抓取日 | 钉版 | sha256 / 许可 |
|---|---|---|---|---|---|---|
| **S1** | *Compute Express Link 3.0*(CXL Consortium 公开白皮书,4 页) | `computeexpresslink.org/wp-content/uploads/2023/12/CXL_3.0_white-paper_FINAL.pdf` | 白皮书 | 2026-11-02 | ✅ `pin=strong` | `d8d8ba0e7cb27833ef0e718d8628ccd2693ffda2d44884da24e4fe25a560a2b6` · © Consortium ⇒ `pending=true` |
| **S2** | *CXL 3.0: An Open Industry Standard for Composable Computing*(FMS 2023 Tutorial,92 页) | `computeexpresslink.org/wp-content/uploads/2023/12/CXL_FMS-2023-Tutorial_FINAL.pdf` | 会议 tutorial | 2026-11-02 | ✅ `pin=strong` | `0275ae8ed6ecb1b25c2a162593245c0b838c7392871e890c1a13f0b8fc60de7e` · © Consortium ⇒ `pending=true` |
| **S3** | *Compute Express Link (CXL): A Coherent Interface for Ultra-High-Speed Transfers*(MSST 2023,43 页) | `msstconference.org/MSST-history/2023/TavallaeiPresentation.pdf` | 会议演讲 | 2026-11-02 | ✅ `pin=strong` | `e8a704e5650b72a96c0b2838630be467ddcd861d29003db176f872e3a229562e` · © Consortium ⇒ `pending=true` |
| **S4** | Sharma / Blankenship / Berger,*An Introduction to the CXL Interconnect*,ACM CSUR 2024 | arXiv `2306.11227v3` · DOI `10.1145/3669900` | 同行评审综述 | 2026-11-02(沿用 #10 字节) | ✅ `pin=strong` | `30f5baac34bb23e478834536db334c45338ec77324f25db799ef6e0deb3075bf` · 许可 **absent** ⇒ `pending=true` |
| **S5** | Tan / Donaldson / Wickerson,*Formalising CXL Cache Coherence*,ASPLOS'25 | arXiv `2410.15908v2` · DOI `10.1145/3676641.3715999` | 同行评审论文 | 2026-11-02(沿用 #10 字节) | ✅ `pin=strong` | `a1c740bb9359d7dece4886804b12007b643b4238139d05762c8beaf13e283303` · **CC-BY 4.0** ⇒ 可 `pending=false` |
| **S6** | *CXL 3.0 Coherence Deep Dive*,Hot Chips 2022(48 页) | `hc34.hotchips.org/.../Hot Chips 2022 CXL3 Coherence Deep Dive.pdf` | 公开 tutorial | 2026-11-02(沿用 #10 字节) | ✅ `pin=strong` | `8de5e47a0d23c8869dd1dbbafd56b24f03eb4672ee29ee676abb5605a3561a54` · © Consortium ⇒ `pending=true` |
| **S7** | Jain / Yeleswarapu / Al Maruf / Gupta(AMD),*Memory Sharing with CXL*,HCDS 2024 | arXiv `2404.03245v1` | 同行评审 workshop | 2026-11-02(沿用 #10 字节) | ✅ `pin=strong` | `6513d4f4e23461be47fca04737065b3679cf2b144f1833bf540bb397e1c7598d` · **CC-BY 4.0** ⇒ 可 `pending=false` |
| **S8** | Cadence,*Navigating Cache Coherence: The Back-Invalidate Feature in CXL 3.0* | `community.cadence.com/.../navigating-cache-coherence-the-back-invalidate-feature-in-cxl-3-0` | 厂商博客 | 2026-11-02 | ❌ `pin=transcribed`(**403** Cloudflare,未取字节) | — |
| **S9** | design-reuse 转载摘要(S8 的公开镜像,**非全文**) | `design-reuse.com/blog/55812-...` | 厂商博客转载 | 2026-11-02 | ✅ `pin=strong`(HTML) | `a6450d1629f096f0511223084b998900fa6e83336ef2365c3ca576dce1ecc1dd` · 仅摘要 ⇒ 强度 `analogical` |
| **S10** | *How to achieve cross-host cache coherence using CXL 3.0 HDM-DB* | `0x10.sh/cache-coherence-in-cxl-3-0-hdm-db` | 个人技术笔记 | 2026-11-02 | ✅ `pin=strong`(HTML) | `225c7c17ae9f016a3c331fda6d856675bc34876da919bd93f51092b345117b4b` · 二手转述规范 §3.3.2 ⇒ `analogical` |
| **S11** | *CXL: Enabling an interoperable ecosystem*,SNIA SDC23 | `snia.org/sites/default/files/2025-05/SNIA-SDC23-...pdf` | 会议 deck | 2026-11-02 | ❌ `pin=transcribed`(curl / webfetch 均 **403**) | — |
| **S12** | CXL 3.0 Specification(规范正文) | `computeexpresslink.org/.../CXL-3.0-Specification.pdf` | **规范 PDF** | **未打开** | ⛔ **合规排除** | — |

---

## §9 与队列 #10 的差异对照

### 9.1 沿用

| 项 | 说明 |
|---|---|
| 合规红线(ECA §3 + AI AUP)与两轴规则 | 完全沿用 #10 §0 / §8.2 |
| `perm` 4 值锁(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`),不新增第 5 值 | 沿用 `ADR-CEP-02` CEN-Ib11 |
| **CXL.cache 缓存代理** 4 态(M/E/S/I)+ **4→4** 投影 | 沿用 #10 §1 / §3 |
| CXL.cache 双向各 3 通道(Req/Rsp/Data);H2D Req = host snoop;D2H Req = device 请求 | 沿用 #10 §2 |
| SF 完备性 `CONSERVATIVE` 的保守处理(宁多探不漏探) | 沿用 #10 §5 的类比口径 |
| **CXL.cache 在 3.0 仍非对称** | 沿用 Entry #122 ①-a;本轮 S5 / S4 **印证** |

### 9.2 改变(与 #10 相反或需改写)

| 项 | #10(CXL 2.0 非对称) | #12(CXL 3.0 对称 / DCOH) |
|---|---|---|
| 设备发起 snoop | **不存在**(`device_snoop_initiators = ∅`) | **存在 1 类**(`S2M BISnp`)⇒ **核心判别式反转** |
| CXL.mem 通道 | 4 条基础 | **6 条**(+ `S2M BISnp` / `M2S BIRsp`) |
| 设备侧状态空间 | 1 套(MESI 4) | **3 套**:`DevCacheState`(4) / `DcohViewState`(3) / `HomeDirState`(3) |
| 「非对称 / 对称」定义 | host-centric / 单一一致性点 | **窄义**:设备可为 HDM home + 反向 snoop host;**不是** peer 全对称 |
| `aggregation_at` | `HOME`(host SF) | ADR 原写**二元**(P2P⇒REQUESTER / home⇒HOME);公开来源**只支持 HOME**,P2P⇒REQUESTER **反证** |
| `DcohSymmetric` 的 `authority` | `LOCAL_HINT`(per ADR) | 公开来源称 DCOH 是其区域内的**最终仲裁者** ⇒ `LOCAL_HINT` 需复核 |

### 9.3 新增(#10 没有)

| 项 | 说明 |
|---|---|
| **HDM-DB**(Device-coherent with Back-invalidate) | CXL 3.0 新内存类型;Type-2/3 均可用 |
| **inclusive snoop filter + SF victim** | 设备侧 SF 满时选 victim,发 BI 失效对应行 |
| **多 host 共享内存(GFD/GFAM)** | home 目录 + sharing list;硬件强制一致 |
| **CacheID 设备扩展** | 4-bit,单 root port 最多 16 个 CXL.cache 设备 |
| **virtual hierarchy** | 设备关联成一个 coherency domain |
| **BISnp block** | 可覆盖 2×128B 或 4×256B |

---

## §10 硬门判定(结论)

**🟡 有条件通过** —— 允许建立队列 #12 的**规约层** change,但**必须**:

1. 把状态空间**拆成 3 套**(`DevCacheState` / `DcohViewState` / `HomeDirState`),**不得合并**;
2. `perm` 只从 `DevCacheState` 投影(4→4),**不新增第 5 值**;DCOH / home 的 3 态**不得**塞进 `perm`;
3. 编译期断言 `device_initiated_snoop_set == {BISnp}`(**不得**沿用 #10 的「空集」断言);
4. 每格 `basis` 标 `pending`,`ref` 指向**本文件条目号**,**不指向规范原文**;
5. ⛔ **不得**把「CXL 3.0 对称」表述为「设备之间可互发反向 snoop / 设备缓存彼此内存」—— 公开来源只支持 **device → host** 的 BI;
6. ⛔ **不得**把「DCOH」展开为 "Device Cache Ownership History";
7. ⛔ **不得**把「归属基于 DPA」写入载体;
8. ⛔ **不得**在 `perm` 中新增 `A` / `S` 等第 5、第 6 值。

**⚠️ 阻塞项(architect 复核)**:§6 的 **#3**(`ADR-CEP-04 §3.5` 的 `P2P ⇒ REQUESTER` 反证)
+ **#4**(`DcohSymmetric` 的 `LOCAL_HINT` / `broadcast_fallback=true` 张力)
+ **#1 / #2**(07 号 4 处口径错误 ⇒ 冻结面 Status Update)。

**🟡 非阻塞(逐格 pending)**:R-S1 ~ R-S9 ⇒ 需**人类成员条款摘要**。

---

## §11 建议载体 `basis` 初始取值(供第 0 个 change 使用)

| 载体格 | `kind` | `pending` | `ref` |
|---|---|---|---|
| `DevCacheState` 4 态 | `public-direct` | `false` | §1 / S5 |
| `perm` 4→4 | `public-analogical` | `true` | §3 / #10 §3 |
| `DcohViewState` 3 态 | `public-direct` | `true` | §1 / S6 |
| `HomeDirState` 3 态 | `public-direct` | `true` | §1 / S4 |
| `CXL.mem` 6 通道 | `public-direct` | `true` | §2 / S4 + S6 |
| `device_initiated_snoop_set = {BISnp}` | `public-direct` | `true` | §2 / S2 + S3 + S6 |
| `aggregation_at` | `archforge-anchor` | `true` | §6 #3(**待 architect 复核**) |
| `CONSERVATIVE` / `LOCAL_HINT` | `public-analogical` | `true` | §6 #4 / S7 + S6 |
| `broadcast_fallback=true` | `self-derived` | `true` | §6 #4(**无公开依据**,建议复核) |

---

## §12 关联

- 台账 `obj-cep-platform.md` **Entry #122**(CXL 口径裁决:「3.0 对称」指 HDM-DB / CXL.mem 的多 host 共享内存)·
  **Entry #153**(本轮普查登记 + architect 裁决面)
- `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` §3.5(聚合点二元 —— **待复核**)·
  `docs/adr/ADR-CEP-02-...`(perm 4 值锁)· `docs/adr/ADR-CEP-11-...`(版本锁定)
- `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`(队列 #10 的同款产物 —— **本文件的直接范式**)
- `include/tlm/cep/sharer_tracker.hh`(`DcohSymmetric` traits 行 —— **待复核**)· `include/tlm/cep/cxl_state_spec.hh`(#10 规约载体)
