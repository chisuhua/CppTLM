# 论文承重论断索引（Oswald *Gen 族）

> **定位**：本仓对 *Gen 族*（ProtoGen / HieraGen / HeteroGen）论文的**可机检引用面** ——
> 每条承重论断都锚到**仓内抽取件的行号区间** + **该区间的 sha256** ⇒ 任何人可在仓内复核「本仓说的这句话，论文原文是不是这么写的」。
> **不是**真源：论文原文（PDF）的身份由 [`source-manifest.json`](./source-manifest.json) 的 `papers[].sha256` 锚定。
> **机器载体**：`scripts/test/cep_oswald_provenance_check.sh`（校验本表的每条锚点仍解析到预期内容）。

## 0. 怎么用

| 场景 | 动作 |
|---|---|
| 引用某条论文论断 | 引用形如 `docs/research/oswald/extracts/thesis-text-extract.md:7162-7174`（行区间）+ 锚点 id（`T-C15`） |
| 复核该引用 | 跑 `scripts/test/cep_oswald_provenance_check.sh`（逐条比对锚点区间 sha256） |
| 取论文原文 PDF | `docs/research/oswald/materialize_papers.sh`（物化到 `.papers/`，按 sha256 校验） |
| 论文全文 | `extracts/thesis-text-extract.md`（8542 行，含 `===== PAGE n =====` 页标记） |

> ⚠️ **引用纪律**（承重）：论文原文**存于仓外**（mynotes 仓），本仓按 `docs/research/AGENTS.md` §5「第三方原文不存放」不 vendor PDF 二进制。
> ⇒ **仓内可复核的是「抽取件 + 行号锚点」，不是 PDF 本身**；PDF 同一性由 sha256 保证。

## 1. 论断清单


### `extracts/thesis-text-extract.md`

| id | 行区间 | 锚点 sha256 | 层级 | 论断 |
|---|---|---|---|---|
| `T-C01` | 1104–1104 | `fa8fd09b2534fa3f` | 背景 | 一致性定义 = Write-Serialization + Write-Propagation（另 SWMR / Data-Value） |
| `T-C02` | 1392–1392 | `e4604d381dcac087` | 并发 | 瞬态态代价：MSI = 3 稳定态 + 17 瞬态态（ProtoGen 论文实测 18） |
| `T-C03` | 3964–3964 | `22ccb90802c4b7bc` | 并发 | ProtoGen 局限 L4（VC 须用户手工定义）+ L5（仅目录式协议） |
| `T-C04` | 4007–4007 | `e6e4f822b3185d56` | 并发 | Step 1 转发请求改名（Fwd GetS / O Fwd GetS）⇒ 缓存可反推竞态胜负 |
| `T-C05` | 4121–4121 | `1edfc01ae66ce30b` | 并发 | Step 2 State Set 对偶性（IMAD ∈ I ∩ M） |
| `T-C06` | 5419–5419 | `2949c9421e08a1dc` | 分层 | HieraGen 基线约束：包容式全映射目录（+ 不静默逐出只读块） |
| `T-C07` | 6130–6130 | `40425ddd63469ba4` | 分层 | E 态静默升级 ⇒ 层间兼容性判据（E 可静默升 M ⇒ 破坏全局 SWMR） |
| `T-C08` | 5395–5395 | `ac4c975237cf91cb` | 分层 | 唯一序列化点不变量：任意两条竞争事务恰在一个目录序列化 |
| `T-C09` | 6287–6287 | `355a11ae7e6e35cc` | 分层 | §7.6.2 不完整目录知识 ⇒ 由输入 SSP 处理（算法本身不需 belief） |
| `T-C10` | 6303–6303 | `fe40de6f88626d3a` | 分层 | §7.6.3 null directory（Dir1B）= 广播式 snooping 视角 |
| `T-C11` | 6838–6838 | `b84a59b62e8f696b` | 异构 | 一致性接口 = read / write / **fence** 三元 |
| `T-C12` | 6887–6890 | `2916a21855f12a43` | 异构·NG1承重 | 论文对 RC 的刻画 = **一致性接口请求类型四分**（release-write / data-write / acquire-read / data-read）—— **不含任何序语义** ⇒ RC 的 `po` 规则必为派生 |
| `T-C13` | 7098–7103 | `a811a7625fe0ba30` | 异构·NG1承重 | **SC 的 ppo 形式化**：ppo ≜ po（论文给出的两个 ppo 之一） |
| `T-C14` | 7104–7110 | `3d4fa9e88242f457` | 异构·NG1承重 | **x86-TSO 的 ppo 形式化**：ppo ≜ po \ (st(x);po;ld(y))（论文给出的两个 ppo 之二） |
| `T-C15` | 7162–7174 | `f29685dc367155eb` | 异构·承重 | **复合一致性公理（Eq 8.3）**：acyclic(ppo_com ∪ rfe ∪ fr ∪ ws) |
| `T-C16` | 7123–7123 | `1f840811859f0796` | 异构·承重 | 公理面**不含域内 rf** ⇒ 域内一致性不由复合公理检查（各域自身义务） |
| `T-C17` | 8053–8053 | `5fe24661dfb83493` | 工具链 | §9.2 工具未整合（HeteroGen 不含 HieraGen 阶段） |
| `T-C18` | 8113–8113 | `3235e09c66284876` | 工具链 | §9.3.1 自动 VC 分配未做（为保证死锁自由）⇒ 对应缺口论文侧亦未解决 |
| `T-C19` | 8161–8161 | `e6ccbb704c89036e` | 工具链 | §9.3.7 形式化证明 = future work |

### `extracts/HeteroGen_HPCA2022-draft.md`

| id | 行区间 | 锚点 sha256 | 层级 | 论断 |
|---|---|---|---|---|
| `H-C01` | 1275–1276 | `ddadbe4932662fb1` | 异构·NG1承重 | HeteroGen Table I = **case study 协议清单**（SC→MSI/MESI；TSO→TSO-CC；RC→RCC-O/RCC/GPU；PLO→PLO-CC） |
| `H-C02` | 1243–1243 | `90edbf2d030b8917` | 异构·NG1承重 | Table I 列的是**七个同构协议** ⇒ 该表是协议清单，**非**一致性模型序语义表 |
| `H-C03` | 1254–1257 | `6f3e47595c583a87` | 异构·承重 | **PLO 的一手定义**：PLO-CC enforces partial-load-order「enforces the W→W and the R→W orderings but not the other two」⇒ ADR-CEP-22 决策 3 的 PLO 行（写侧 {R→W,W→W} + 读侧 ∅）**逐字吻合** |
| `H-C04` | 297–299 | `2ee39f15cacf8142` | 异构·NG1承重 | HeteroGen §II-B 的**同文平行段**（与 thesis §8.2 的四分 taxonomy 同源）⇒ RC 刻画的**机检锚点有两条**（T-C12 / H-C04） |
| `H-C05` | 584–596 | `76c8d613a44a8542` | 异构·NG1承重 | **会议版**（HPCA 2022 §V-B）的 `ppo` 形式化同样**只有 SC 与 x86-TSO 两个** ⇒ 论文侧**两个版本一致**地未给出 RC 的 `ppo` 关系（闭合「未核对会议版」的边界） |

## 2. ⭐ 四条承重对照（论文 vs 本仓 ADR）

> 这四条是**决定本仓 ADR 取值**的论断，也是本仓**最容易被误引**的地方。

### 2.1 论文**只给两个** `ppo` 形式化 —— SC 与 x86-TSO；**RC 连序语义都没有**

`T-C13` / `T-C14` 锚定**学位论文** §8.5.2 的 "For example:" 段（`:7098` 之后**仅列此两个**）：

```
• SC        ppo ≜ po
• x86-TSO   ppo ≜ po \\ ( st(x) ; po ; ld(y) ), ∀x,y
```

**RC 不在其中。** 论文对 RC 的刻画在 `T-C12`（thesis §8.2，`:6887-6890`）与 `H-C04`（HeteroGen §II-B，`:297-299`）
是**一致性接口的请求类型四分**（release-write / data-write / acquire-read / data-read）
⇒ **该刻画不含任何程序序语义** ⇒ RC 的 `po` 规则**必为派生**。

**会议版同样如此**：`H-C05`（HeteroGen §V-B，`:584-596`）的 `ppo` 形式化**也只有 SC 与 x86-TSO 两个**
⇒ **论文的两个版本一致地未给出 RC 的 `ppo` 关系**。

⇒ **影响**：`docs/adr/ADR-CEP-22-memory-model-source-and-cross-domain-edges.md` 决策 3 的
`RC-acquire` / `RC-release` / `RC-data` 三条 `po` 授权表是**本仓的自主建模**，**论文无对应形式化**。
⇒ 该 ADR 的溯源标注已由其 `## Status Update(2026-11-25)`（台账 Entry #546 · 用户 ratify = 选项 C）更正为
「**论文未给出可援引的形式化**」+ 显式定性「**本仓解释性建模**」。

> ⚠️ **勘误（2026-11-25 · oracle 复审 R1）**：本节早期版本曾把「`ADR-CEP-22` 决策 3 的 RC 行**旧锚点**」误记为
> HeteroGen 论文 **Table I**（`:1266-1276`）。**实测不成立**：决策 3 的 RC 三行溯源列（`ADR-CEP-22:66-68`）写的是
> 「锚点 = `ADR-CEP-12` 决策 3（RC 分 release-write/data-write/acquire-read/data-read，**per HeteroGen §II-B**」），
> 而 `ADR-CEP-12:99` 的依据正是 **HeteroGen p.3 §II-B** ⇒ **旧锚点 = §II-B = 本节新登记的 `H-C04` 同一材料**。
> **Table I 指针不在 RC 序断言的引用链上**（它见于 `ADR-CEP-22:11` 元数据，归 `ADR-CEP-17` · `protocol-engine.md:90` ·
> `ADR-CEP-12:15`）⇒ 本 SU 的真实 delta 是「**转引链机检锚点化 + 补『论文未给出 RC ppo』举证锚点 + 定性更正**」，
> **不是**「替换错误锚点」。

### 2.2 Table I 是**协议清单表**，不是序语义表（**旁证** · 不在 RC 引用链上）

`H-C01` / `H-C02` 锚定 HeteroGen 论文 Table I：

```
In Table I, we list the seven homogeneous protocols that …
Protocol | SC: MSI, MESI | TSO: TSO-CC[16] | RC: RCC-O[10],[27], RCC[27], GPU[11] | PLO: PLO-CC
Table I — Protocols used in the case studies
```

⇒ 该表列的是 case study 用的七个同构协议，**不含任何程序序 / 一致性序语义**。
⇒ **用途**：它是「论文里**哪些**协议被用于 case study」的清单 —— 可用于**佐证 RC 族有哪些协议实例**，
但**不能**用作 RC 的 `po` 规则的语义来源（见 §2.1）。

### 2.3 PLO 行的锚点是**一手**（对照项）

`H-C03` 锚定论文原文：PLO-CC enforces partial-load-order
「**enforces the W→W and the R→W** orderings **but not the other two**」
⇒ `ADR-CEP-22` 决策 3 的 PLO 行（写侧 `{R→W,W→W}` + 读侧 `∅`）**逐字吻合** ⇒ **PLO 行无缺陷**。

### 2.4 复合一致性公理**不含域内 `rf`**（⇒ 域内一致性不由它检查）

`T-C15` / `T-C16` 锚定论文 Eq. 8.3 与其紧邻说明：

```
acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )          (8.3)
… we treat intra-cluster and inter-cluster communication relations identically.
```

⇒ 公理并集里只有 `rfe`（跨域读自），**没有域内 `rf`**。
⇒ **影响**：ADR-CEP-22 决策 6 的「双图折衷」与 §4.2「缺口①：域内一致性不由本公理检查」
**是对论文的忠实复刻**，不是本仓的弱项 —— 论文里域内一致性同样是各簇自身 `Mi` 的义务。

### 2.5 附：两条「抄不到」的未来工作

| 论断 | 锚点 | 含义 |
|---|---|---|
| `T-C18` | §9.3.1 | ProtoGen **未**自动分配消息到虚拟网络（为保证死锁自由）⇒ 本仓**协议级死锁自由论证**在论文侧亦无现成方案 |
| `T-C19` | §9.3.7 | *Gen 算法的**正确性目前靠模型检查器**，形式化证明 = future work |
