# 验收报告 · **G-16**（ADR 清单**反向**完整性闸 · 登记册 §6.3 独立批次）

- **角色**：builder（**实施侧自验** —— 本批由同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-23 · **基线 commit**：`186f2423`（W4 批 1 落地）
- **承载缺口**：**G-16**（`ADR_LIST` 缺反向完整性检查 ⇒ 新 ADR 漏登记 ⇒ 静默脱检）
- **归属单元**：`cep-registry-stale-cleanup`（登记册 §6.3 独立批次）

## 判据来源

| 来源 | 位置 |
|---|---|
| **G-16 的完成判据** | 登记册 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6.3 归属表 G-16 行：「`cep_docs_sync_check.sh` 增**反向**完整性检查（磁盘 `ADR-CEP-*.md` 全量 ⇒ 须被 `ADR_LIST` ∪ 显式排除集覆盖）⇒ 新 ADR 漏登记即判红」 |
| **G-16 的缺口陈述** | 同上 §6.2 缺口行（实施前）：「`ADR_LIST` 是**正向**覆盖集…⛔ **无**「磁盘 → `ADR_LIST`」的反向检查 ⇒ 该脚本注释自述「漏登记 = 新 ADR **静默脱检**」；⚠️ 结构闸的分区完整性可兜底**归类**，但归类后**仍可能**漏加 `ADR_LIST` ⇒ 与 G-15 **非同型**」 |
| **「有牙」纪律** | 登记册 §6.6 P-4（判据须可证伪）+ 本会话既定纪律：变异须**按签名红**、断言**点对点**（⛔ 计数 ≥1 无鉴别力） |

## 逐条判定

| # | 判据 | 实测 | 结论 |
|:--:|---|---|:--:|
| **1** | **反向覆盖判据落地**：`cep_docs_sync_check.sh` 增第 **[13/13]** 组「ADR 清单反向完整性（磁盘 ⇒ 覆盖集）」—— 判据 = 磁盘 `docs/adr/ADR-CEP-*.md` **全量** ⇒ 每份必须落在「`ADR_LIST` ∪ 显式排除集」之内 | 组落地于 `scripts/test/cep_docs_sync_check.sh`（**末尾追加**，见下「落点纪律」）；实测输出「✓ 磁盘 **24** 份 CEP ADR 全部被 ADR_LIST ∪ 显式排除集覆盖」；`--strict` rc=**0** | ✅ |
| **2** | **「已登记集」单一真源**（⛔ 不另建第二份清单 ⇒ 不造新漂移源） | 判据**直接复用脚本内既有的 `ADR_LIST` 数组**（`printf '%s\n' "${ADR_LIST[@]}"`）⇒ 无第二份清单可比对漂移 | ✅ |
| **3** | **显式排除集**存在且**受纪律约束**（第二真源 ⇒ 不得成为无声脱检通道） | 排除集落**独立数据文件** `scripts/test/cep_adr_exclude.txt`（每行 `<文件名>\|<理由>`）；**三条自校验**：① 理由**非空** ② 文件**在磁盘上真实存在**（防「预先排除一个未来才出现的文件」⇒ 提前挖洞）③ **不得**同时在 `ADR_LIST` 内（防多余登记）；任一违规 ⇒ 判红并**点名该文件** | ✅ |
| **4** | **新增 ADR 漏登记即判红**（G-16 专属面） | 决定性变异①：影子 ADR 目录 = 真仓 24 份的 symlink + 1 份合成 ADR（`ADR-CEP-98-classified-but-unregistered.md`，即「**已归类**（OWNERS/NO_INV 覆盖）但**未登记进 `ADR_LIST`**」的形态）⇒ **rc=1** 且输出**点名该文件名**（断言 grep 到 `ADR-CEP-98-classified-but-unregistered.md`）⇒ ⛔ 非「计数 ≥1」 | ✅ |
| **5** | **排除通道真实可用**（非死代码 ⇒ 排除项本身也受判据约束） | 决定性变异②：同一影子目录 + 把该 ADR 显式排除（带理由）⇒ **rc=0** 且报「全部被覆盖」⇒ 排除是**有效出口**而非摆设；变异③⑤：缺理由 / 与 `ADR_LIST` 重复 ⇒ 均 rc=1 且**点名该文件** | ✅ |
| **6** | **反向对照（无假红）** | 决定性变异⑥：真实 ADR 目录 + 真实排除集 ⇒ rc=**0** 且报「全部被覆盖」 | ✅ |
| **7** | **影子模式，真实仓零改动** | 自测全程用 `CEP_ADR_DIR` / `CEP_ADR_EXCLUDE_FILE` 两个 env 覆盖（默认行为**完全不变** ⇒ 沿用 `cep_freeze_check.sh` 的 `CEP_FREEZE_MANIFEST` / `cep_e2e_reconcile_check.sh` 的 `CEP_RECONCILE_DOCS_ROOT` 同一先例）；⛔ 真仓 `docs/adr/ADR-CEP-*.md` 属**冻结面** ⇒ **绝不在真仓增删 ADR 文件**（影子目录用 symlink）；收尾**校验真实仓字节指纹前后相等**（`52e44d0a4a8084e7…`）⇒ 零污染 | ✅ |
| **8** | **落点纪律：零行号位移** | 多处历史记录按**行号**引用本脚本：`cpptlm-cep-gate-robustness.md:88` → `:4`（`ADR_LIST` 条数说明行）、`:94` → `:50`（`ADR_LIST` 数组首行）、`cpptlm-cep-invariant-gate-teeth.md:56` → `:156`（正则）、台账 `:23989` → `:50` ⇒ 新组**追加在文件末尾**（`CEP-GATE-11-END` 之后）+ 头部用途清单**同行追加**（不加行）+ 分组计数 `[N/12]→[N/13]` **同行内**替换 ⇒ **`:4` / `:50` / `:156` / `:266` / `:271` / `:281` / `:392` 逐字不变**（实测 7/7 ✓） | ✅ |

## 决定性变异明细（**按签名红**，点对点）

自测载体：`scripts/test/cep_adr_reverse_check_selftest.sh` ⇒ 实测 **7/7 通过**（含指纹核对）。

| # | 变异 | 期望 | 实测 |
|:--:|---|---|---|
| ① | 影子 ADR 目录多一份未登记 ADR | rc≠0 且**点名 `ADR-CEP-98-classified-but-unregistered.md`** | ✅ |
| ② | 同一目录 + 合法显式排除（带理由） | rc=0 且报「全部被覆盖」 | ✅ |
| ③ | 排除项**缺理由** | rc≠0 且**点名该文件** | ✅ |
| ④ | 排除项指向**磁盘不存在**的文件 | rc≠0 且**点名该文件** | ✅ |
| ⑤ | 排除项与 `ADR_LIST` **重复** | rc≠0 且**点名该文件** | ✅ |
| ⑥ | 真实仓基线态（反向对照） | rc=0 且报「全部被覆盖」（无假红） | ✅ |
| ⑦ | 真实仓字节指纹前后 | 相等 ⇒ 变异零污染 | ✅ |

> ⚠️ **本自测刻意不注册 ctest**（同 `scripts/test/rddf_stage_gate.sh --self-test` 的纪律：「开发期/评审期证据，不是回归闸」）；**回归闸本体 = `cep_docs_sync_check`**（已注册 ctest，`--strict` ⇒ 反向判据不绿即 CI 红）。

## 一处**实施期实测发现**（原计划的事实错误，已订正）

| # | 原述 / 预期 | 实测 | 处置 |
|:--:|---|---|---|
| **E-1** | 「新增未登记 ADR ⇒ 本组判红」可由**直接往真仓 `docs/adr/` 加一份文件**验证 | **不可行**：① 真仓 `docs/adr/ADR-CEP-*.md` 属**冻结面**（`cep_freeze_check.sh` 的 `FROZEN_GLOBS`）⇒ 增删即触五步链；② 且 `[2/13]` 组（invariant 覆盖检查）会**先**判红并因 `set -e` **中止整个脚本** ⇒ 本组根本不输出 ⇒ 拿不到本组签名 | 改用**影子 ADR 目录 + env seam**（`CEP_ADR_DIR`）⇒ ① 真仓零改动 ② 影子目录只多 1 份**已归类**的 ADR ⇒ 绕过 `[2/13]` 的先判 ⇒ 精确命中本组签名。**顺带发现**：G-16 的**专属面**确实是「**已归类但漏加 `ADR_LIST`**」—— 这与结构闸的分区完整性**非同型**（结构闸兜底归类、不兜底 `ADR_LIST` 登记），与缺口陈述一致 |

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| — | **无失败项** —— 6 条决定性变异全按签名红，真实仓基线态绿 | — |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次改动 | `scripts/test/cep_docs_sync_check.sh`（新增 [13/13] 组 + 头部用途同行追加 + 分组计数同行替换）· `scripts/test/cep_adr_exclude.txt`（新数据文件）· `scripts/test/cep_adr_reverse_check_selftest.sh`（新自测脚本）· `test/CMakeLists.txt`（未改 —— 本自测刻意不注册 ctest）· `scripts/README.md`（守门表 +1 行 + `cep_docs_sync_check.sh` 描述改「13 组」）· 登记册 §0.1 / §6.2 / §6.3 · 载体表 4 行 · 本报告 |
| 临时诊断代码 / 临时文件 | ✅ **无仓内残留**（自测的临时目录全部在 `${TMPDIR:-/tmp/opencode}` 且 `trap` 清理） |
| TODO / 半截实现 / 被 skip 的测试 | ✅ 无新增 |
| 未替换的 mock / 临时 fallback | ✅ 无 |
| 范围纪律 | ✅ 零 C++ / spec / ADR 正文改动 · 零 `openspec/changes/**` 改动 · **零 force-add** · ⛔ 未动任何已归档件 · ⛔ 未动任何**冻结面成员**（`docs/adr/ADR-CEP-*.md` 全程零改动 ⇒ **不触五步链**） |
| 零位移核验 | ✅ `:4` / `:50` / `:156` / `:266` / `:271` / `:281` / `:392` **逐字不变**（7/7） |
| 台账 append-only | ✅ 本批**未改台账任何历史正文**（回执 Entry **#528** 追加；授权 Entry **#527** 同批追加） |

## 回归记录

| 检查 | 命令 | 结果 |
|---|---|---|
| CEP 契约同步闸 | `bash scripts/test/cep_docs_sync_check.sh --strict` | ✅ rc=0（13 组全绿） |
| G-16 决定性变异自测 | `bash scripts/test/cep_adr_reverse_check_selftest.sh` | ✅ **7/7 通过** |
| 顺序机关 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0 |
| 活基线闸 | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0（5 项文档取值 == 实测） |
| 旧口径残留闸 | `bash scripts/test/cep_registry_stale_refs_check.sh` | ✅ rc=0 |
| 文档路径同步闸 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| 冻结面校验 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0（90 产物 hash 不变） |

## 已用轮次

**1/3**（G-16 实施侧自验第 1 轮 —— 6 条决定性变异 + 1 条反向对照一次到位；实施期发现 E-1 并当场订正；无返工轮）。
