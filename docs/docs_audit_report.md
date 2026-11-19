# 文档路径漂移审计（活状态）

> **性质**: **活状态文档** —— 数字由工具产出，本文件只记录**当前值 + 重生成命令**，不手抄快照。
> **重生成**: `bash scripts/test/docs_sync_check.sh --strict`（pre-commit 自动执行，退出码非 0 即阻断提交）
> **最后实测**: 2026-11-19（docs 重组后）

---

## 1. 路径引用闸（`scripts/test/docs_sync_check.sh --strict`）

扫描面 = 4 份核心文档：`AGENTS.md` · `docs/ONBOARDING.md` · `.rddf/roadmap.md` · `scripts/README.md`

| 指标 | 实测值 |
|------|-------|
| 扫描文档数 | 4 |
| 路径引用总数 | **616** |
| **缺失路径数** | **0** ✅ |
| 误报排除（`VIRTUAL_PATHS`） | 27 项（已删除 / 已归档 / 规划中 / 跨仓路径） |

### VIRTUAL_PATHS 的用途

`VIRTUAL_PATHS` 数组登记「**文档里提到、但物理文件已删除或已迁出**」的路径，用于**说明删除原因**时引用而不误报。
新增此类路径时**必须**加进该数组（per `AGENTS.md` §DOC HYGIENE），**不要**直接删掉文档里的说明段落。

---

## 2. AGENTS.md 体量闸（`scripts/test/agents_md_lint.py`）

| 指标 | 实测值 | 上限 |
|------|-------|------|
| 文件体量 | **64433 B** | 65536 B（64 KiB） |
| 单行长度 | 合规 | 1000 字符 |
| STRUCTURE 树路径存在性 | 合规 | — |

> ⚠️ **余量仅 ~1.1 KiB** —— `AGENTS.md` 注入每个 agent 的上下文，体量直接等于每会话 token 成本。
> **过程日志（finding 编号 / 评审轮次 / 归档流水）一律不进 AGENTS.md**（落 ADR `## Status Update` 段或 `openspec/changes/archive/`）。

---

## 3. Markdown 链接闸（自建，见 §5）

`docs_sync_check.sh` 只校验**反引号路径字面量**，**不校验** `[text](target)` 形式的 Markdown 链接。
故额外用全量扫描统计链接可达性（结果见 §4）。

---

## 4. 全仓 Markdown 链接实测（2026-11-19）

| 类别 | 数量 | 处置 |
|------|-----|------|
| 可达链接 | 正常 | — |
| **跨仓路径**（`../../../main/ArchForge/...`） | 若干 | 本仓不可达属正常 —— 目标在 **ArchForge 仓**；按需加 `VIRTUAL_PATHS` |
| **通配写法**（`../adr/ADR-CEP-19-*.md`） | 若干 | 人工可读但工具不可解析 —— 建议改写为具体文件名 |

---

## 5. 维护流程

每次 `src/` `include/` `scripts/` `docs/` 结构变更时：

1. 运行 `./scripts/test/docs_sync_check.sh --strict` —— 零缺失
2. 运行 `python3 scripts/test/agents_md_lint.py` —— 体量 / 行长 / STRUCTURE 树全绿
3. 同步 `AGENTS.md` STRUCTURE 树 + `scripts/README.md` 子目录表 + `docs/ONBOARDING.md` §5.5 脚本表
4. 新增「已删除 / 已归档」路径引用 → 加进 `docs_sync_check.sh` 的 `VIRTUAL_PATHS`
5. 提交 —— pre-commit hook 自动复跑第 1 步

---

**维护**: builder（随结构变更同步）· architect（判据口径）
