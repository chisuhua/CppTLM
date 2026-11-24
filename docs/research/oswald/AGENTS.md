# docs/research/oswald/ — Oswald *Gen 族* 论文资料面

> **域**：CEP 一致性互联平台**协议空间决策**的**论文侧证据锚点层**（Oswald 博士论文，Edinburgh 2023）。
> **不是**真源：协议语义设计权威归 ArchForge（per `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` 决策 1）；
> 论文侧**判据**以 `docs/research/cep-coherence-protocol-space-survey.md` 为准。
> **上位索引**：`docs/research/AGENTS.md` §2.1。

## 文件索引

| 文件 | 角色 |
|---|---|
| `README.md` | 人读入口：为什么有本目录 · 内容清单 · 怎么用 · 边界 |
| `source-manifest.json` | 机读溯源清单（schema `cep-oswald-source-manifest/v1`）：4 原文 PDF + 4 抽取件 + **21 条承重论断的行级锚点** |
| `paper-claims-index.md` | 人读论断索引（21 条 → 行区间 → 锚点 sha256）；§2 = **三条承重对照**（论文 vs 本仓 ADR） |
| `extracts/thesis-text-extract.md` | 论文全文抽取件（8542 行 · 含 `===== PAGE n =====` 页标记） |
| `extracts/ProtoGen_ISCA2018-draft.md` | ISCA 2018 抽取件（逐字节复制，行号与既有引文对齐） |
| `extracts/HieraGen_ISCA2020-draft.md` | ISCA 2020 抽取件（同上） |
| `extracts/HeteroGen_HPCA2022-draft.md` | HPCA 2022 抽取件（同上） |
| `materialize_papers.sh` | 原文物化器（仓外源 → `.papers/` + sha256 校验） |

## 本目录专属纪律

1. **原文 PDF 不入 git** —— per `docs/research/AGENTS.md` §5「第三方原文不存放」（版权 + 体积）。
   `.papers/` 已 gitignored；`materialize_papers.sh` 按 `source-manifest.json` 的 sha256 物化并校验。
2. **派生抽取件逐字节入 git** —— 保住 ADR / 调研文档按**行号**的引文可解析（引文口径不因本目录建立而漂移）。
3. **引文必须给行区间**（不是单行）—— PDF 数学符号因字体编码**逐行分离**（`acyclic(` 与 `ppo_com` 分处多行）。
4. **新增承重论断的流程**：在 `source-manifest.json` 的 `claims[]` 追加一条（`id` / `file` / `lines` / `must_contain` /
   `anchor_sha256` / `claim` / `layer`）⇒ 同步 `paper-claims-index.md` ⇒ 跑 `scripts/test/cep_oswald_provenance_check.sh`
   （⛔ `claims` 条数下界 21 是**防删**护栏；删条目须同步改下界并说明理由）。
5. **新增抽取件** ⇒ 同步 `source-manifest.json` 的 `extracts[]`（sha256 / 字节 / 行数）与 `papers[]` 的对应关系。
6. **⛔ 不写过程日志**（finding 编号 / 评审轮次 / 归档流水）—— 一律落 ADR `## Status Update` 段或 `.rddf/reviews/`。

## 关联

- 上位索引：`docs/research/AGENTS.md` §2.1
- 论文侧结论层：`docs/research/cep-coherence-protocol-space-survey.md`
- 机器闸：`scripts/test/cep_oswald_provenance_check.sh`（含 `--selftest` 反向 fixture）
- gap 总账：`.rddf/roadmap/cep-gen-family-gap-closure.md`
