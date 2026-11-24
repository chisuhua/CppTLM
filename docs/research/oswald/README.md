# Oswald *Gen 族* 论文资料面（CEP × Gen 族 gap 分析的论文侧溯源单一真源）

> **定位**：本目录是**框架级调研**的一部分（per `docs/research/AGENTS.md` §2.1「CEP 一致性探索平台」）——
> 收纳 **Oswald 博士论文（*ProtoGen* / *HieraGen* / *HeteroGen*，University of Edinburgh 2023）** 的一手材料，
> 供本仓 **CEP 一致性互联平台**的协议空间决策做**可复核的引用**。
> **不是**真源：协议语义设计权威归 ArchForge（per `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` 决策 1）；
> 论文侧**判据**以 `docs/research/cep-coherence-protocol-space-survey.md` 为准，本目录是其**证据锚点层**。

## 1. 为什么有这个目录

本仓的 CEP 决策多次引用论文（`ADR-CEP-08` 决策 2/4 · `ADR-CEP-13` 决策 2/3 · `ADR-CEP-17` 决策 7 · `ADR-CEP-22` 决策 3/4），
但**论文原文存于仓外**（mynotes 仓）⇒ 引用无法在本仓内复核 ⇒ 一度出现
「承重件的锚点指向一张**不含序语义**的协议清单表」这类**溯源缺陷**（见 `paper-claims-index.md` §2）。

本目录把论文材料落进仓内，使「本仓说的这句话，论文原文是不是这么写的」成为**机器可判**的事。

## 2. 内容清单

| 路径 | 角色 | 体量 |
|---|---|---|
| [`source-manifest.json`](./source-manifest.json) | **机读溯源清单**（schema `cep-oswald-source-manifest/v1`）：4 份原文 PDF（sha256 / 字节 / 页数）+ 4 份抽取件（sha256 / 行数）+ **21 条承重论断的行级锚点**（行区间 + 区间 sha256 + 必含关键字） | 11.9 KB |
| [`paper-claims-index.md`](./paper-claims-index.md) | **人读论断索引**：21 条论断 → 行区间 → 锚点 sha256 → 论断；§2 是**三条承重对照**（论文 vs 本仓 ADR） | 7.3 KB |
| [`extracts/`](./extracts/) | **派生抽取件**（逐字节入 git）：论文全文 8542 行（含 `===== PAGE n =====` 页标记）+ 3 份会议论文抽取件 | 588 KB |
| [`materialize_papers.sh`](./materialize_papers.sh) | **原文物化器**：从仓外源路径复制 PDF 到 `.papers/` 并按 sha256 校验 | 3.5 KB |
| `.papers/` | 论文原文 PDF（**gitignored**，见 §3） | 17.4 MB |

## 3. 为什么原文 PDF 不入 git

per `docs/research/AGENTS.md` §5「**第三方原文不存放**（仅分析）— 避免版权 + 体积问题」：

- **版权**：4 份原文分别是 Edinburgh 学位论文与 IEEE/ACM 会议论文（ISCA 2018/2020 · HPCA 2022）；
- **体积**：合计 17.4 MB（本仓 `.git` 已 766 MB；仓内**无** vendored 二进制先例）。

⇒ 处置：**原文以「可寻址 + 可校验」方式存在**（`source-manifest.json` 的 sha256 + `materialize_papers.sh` 物化），
**派生抽取件逐字节入 git**（保住 ADR 引用的行号锚点可解析）。
⇒ **仓内可复核的是「抽取件 + 行级锚点」，不是 PDF 本身**；PDF 同一性由 sha256 保证。

## 4. 怎么用

```bash
# 1) 取论文原文（物化到 .papers/，按 sha256 校验）
bash docs/research/oswald/materialize_papers.sh
bash docs/research/oswald/materialize_papers.sh --list     # 只看清单

# 2) 校验本目录的溯源完整性（抽取件 sha256 + 21 条锚点 + 索引对账）
bash scripts/test/cep_oswald_provenance_check.sh

# 3) 验证闸有牙（3 条反向 fixture，跑在拷贝根 · 零工作树污染）
bash scripts/test/cep_oswald_provenance_check.sh --selftest
```

**引用论文时的写法**（行区间 + 锚点 id）：

```markdown
论文 §8.5.2 只给出 SC 与 x86-TSO 的 ppo 形式化（`T-C13` / `T-C14`；
抽取件 docs/research/oswald/extracts/thesis-text-extract.md:7098-7110）。
```

## 5. 边界（诚实声明）

| # | 边界 |
|---|---|
| B1 | **PDF 原文不在 git** ⇒ 全新 clone 需先跑 `materialize_papers.sh`；仓外源不可达时，P-5 判据为 **SKIP**（⛔ 不当绿） |
| B2 | 抽取件是**派生工件**，**不是**原文；重新抽取可能因工具版本差异产生逐字不同的文本 ⇒ 以**仓内文件 + 其 sha256** 为准 |
| B3 | 论文的**规约文本**（如 PPL 语法、SSP 形式化定义）以论文自身为准；本目录只保证**本仓引用的那 21 条**可复核 |
| B4 | 本目录**不含**本仓对论文的**结论**（结论在 `docs/adr/ADR-CEP-*.md` 与 `docs/research/cep-coherence-protocol-space-survey.md`） |
| B5 | 抽取件中数学符号因 PDF 字体编码呈**逐行分离**（如 `acyclic(` 与 `ppo_com` 分处多行）⇒ 引用须给**行区间**，不给单行 |

## 6. 关联

- 论文侧调研（结论层）：`docs/research/cep-coherence-protocol-space-survey.md`
- 引用本目录的决策：`docs/adr/ADR-CEP-08-ordering-point-portability.md` · `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` ·
  `docs/adr/ADR-CEP-17-ruby-slicc-protocol-family.md` · `docs/adr/ADR-CEP-22-memory-model-source-and-cross-domain-edges.md` ·
  `docs/adr/ADR-CEP-23-incomplete-directory-knowledge-boundary.md`
- gap 总账：`.rddf/roadmap/cep-gen-family-gap-closure.md`
- 本目录的闸：`scripts/test/cep_oswald_provenance_check.sh`
