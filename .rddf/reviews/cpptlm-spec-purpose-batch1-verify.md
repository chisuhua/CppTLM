# 验收报告：`cpptlm-spec-purpose-batch1`

> **角色**: verifier（角色内建验收 · **独立于实施者**）
> **被验对象**: change `cpptlm-spec-purpose-batch1`（台账 Entry #441 立项 / #442 收口）
> **归档目录**: `openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/`
> **验收日期**: 2026-11-15 · **轮次**: 第 1 轮 · **产物 commit**: `400aa618`
> **结论**: ✅ **PASS**（10/10 判据通过 · 0 不通过 · **0 未取证**）

---

## 判据来源

| 优先级 | 来源 | 取用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 「按你建议的顺序推进」⇒ 步骤 5 = 批 1 的独立验收 |
| 2 | `proposal.md` 的 `## 验收标准` 段 | **AC-01 ~ AC-10**（本报告主判据） |
| 3 | 本次变更自己声明的行为 | commit `400aa618` + 归档目录内容 |
| 4 | ADR 的 Verification 章 | 本 change 未新增 ADR ⇒ 不适用 |

**分流判定**（是否走 `rdd-verifier` skill）：① change 含 `## 验收标准` 段 ✓ · ② **P3 archive gate 信号缺失**（`.rddf/state/iteration.json` **不存在** —— 本会话从未跑 `rdd-builder` skill ⇒ 无状态机文件）✗ · ③ skill 可加载 ✓
⇒ **条件 ② 不满足 ⇒ 走本地验收**（5 步）。⚠️ 本地验收的额外门（`iteration.json` 的 `tasks_done == tasks_total`）在**该文件不存在时无法适用** ⇒ **本报告的 pass 结论完全由逐条工具证据支撑**，与 iteration 状态无关。

---

## 独立性原则（本轮的核心方法）

被验 change **自带**一个校验脚本 `scripts/test/cep_spec_purpose_batch1_check.py` ⇒ 用它验自己 = 自证。
⇒ 本验收**另写一份独立实现**（`/tmp/opencode/verify_purpose_batch1.py`，不同代码路径、不同判定口径）：
- 长度/单行/LCS/禁式 —— **独立重算**（CJK-only LCS 另写一遍 DP）
- 归因闭合 —— **独立重算**（`isfile` + 核心名命中 + `claim_kind` 互斥）
- 范围零 diff —— **独立 `git diff --numstat` + 逐份 `## Requirements` 段字节比对**
⇒ 两套实现结论一致才算证据。**被验脚本本身的判别力另行验证**：落地**前**跑 ⇒ **0/16 绿**（占位残留被 C-1 抓到）⇒ 有牙。

---

## 逐条判定

| # | 判据 | 判定 | 证据 |
|:--:|---|:--:|---|
| **AC-01** | 16 份 Purpose 限段零占位 | ✅ | 独立实现逐份正则限段扫 `TBD\|TODO` ⇒ 命中 **0** 份 |
| **AC-02** | 定向：16/16 无 Purpose 类 issue | ✅ | 独立跑 `openspec validate <spec> --type spec --strict --json` ⇒ 16 份中含 `placeholder` 类 issue 的 = **0** 份 |
| **AC-02b** | 全局：`pre − post = k` 且逐份归因闭合 | ✅ | `--strict` 红 **pre 77 → post 65（k=12）**；本批 **12 份完全转绿**、**4 份仍红**（`pcie-ip-integration` / `pcie-ip-microarch` / `pcie-slice-prerequisites` / `pcie-tlp-wire-datapath`，红因**全为「正文 >500」**）；全仓 Purpose 占位残留 **59** = 期望 **75 − 16** ⇒ **精确闭合** |
| **AC-03** | 四条正交（逐份机判） | ✅ | **独立实现**复算 16 份：长度 `50 ≤ n ≤ 160` 全满足 · 全单物理行 · CJK-only LCS 全 < 阈值（`single-requirement` 豁免档 16）· 13 类禁式（过程叙述 / 技术字面量 / change 引用 / 日期）全 0 命中 ⇒ 问题 **0** 项 |
| **AC-04** | 零语义改动 + diff 形状写死 | ✅ | `git diff --numstat 400aa618^ 400aa618 -- openspec/specs/` ⇒ **16 文件 · 每文件 +1/−1**；**16 份的 `## Requirements` 段与 base 逐字节比对 ⇒ 变动 0 份** |
| **AC-05** | 不声称不存在的能力 | ✅ | 独立复算 16 条：证据文件 `isfile` 全 True **且**「来源 change 核心名」命中证据文件前 2000 字符（16/16）· `claim_kind` 与 spec 内 `Superseded\|物理删除` 标记**互斥**校验通过 ⇒ 问题 **0** |
| **AC-06** | `cep-*` 不受影响 | ✅ | 43 份 `cep-*` 逐份 `--strict` ⇒ rc≠0 = **0**；`artifact_count` = **86**（零冻结产物 ⇒ 免五步链） |
| **AC-07** | 范围零 diff | ✅ | `git diff --name-only <base> 400aa618 -- include/ src/ 'test/*.cc' docs/adr/ 'openspec/specs/cep-*/'` ⇒ **0** 个文件 |
| **AC-08** | 闸 0 回归 | ✅ | **45 闸**逐个实跑全 rc=0（`docs_sync` / `doc_ref` 带 `--strict`）· `ctest -R cep` **100/100** · `ctest` 全量 **180/180** · `[cep]` **529 用例 / 12288 断言** |
| **AC-09** | KPI + 体量 | ✅ | `openspec list --json` ⇒ active = **0** · 归档目录 **150**（⚠️ 只数目录，`listdir` 会把 `STATUS_AUDIT_*.md` 也计入）· `docs_sync --strict` rc=0 |
| **AC-10** | 归档语义可判别 | ✅ | 归档 proposal 顶部标记行「✅ **已完成归档 · 主 spec 直改（无 delta）**」在位 · 归因 JSON `archive_semantics == "completed-direct-main-spec"` · 归档目录**无 `specs/`** |

**判定汇总**：**10/10 通过 · 0 不通过 · 0 未取证**。

---

## 验证顺序（早失败早停，未跳步）

| 步 | 内容 | 结果 |
|:--:|---|---|
| **1 · 静态检查** | `bash -n`（改动闸脚本）· `py_compile`（校验脚本）· `agents_md_lint` rc=0 · `openspec validate --changes --strict` rc=0 | ✅ |
| **2 · 单元测试** | `[cep]` **529 用例 / 12288 断言** 全绿 | ✅ |
| **3 · 集成 / 端到端** | `ctest -R cep` **100/100** · 全量 **180/180** · 45 闸逐个实跑 rc=0 · 校验脚本 **16/16** | ✅ |
| **4 · 边界与错误路径** | ① **落地前**跑校验 ⇒ **0/16**（占位被 C-1 抓到 ⇒ 判据有牙）② **判据假阳性演练**：初稿 C-3 因合法产品名 `PcieEndpointIP`（14 字符）触发 ⇒ 度量被拆分修正（CJK-only + 英文词序列）⇒ **两套独立实现复算后一致** ③ 归因载体 `checks` 由脚本重算 ⇒ 不自证 | ✅ |
| **5 · 遗留清扫复核** | 见下 | ✅ |

### 第 5 步 · 遗留清扫（本次范围）

| 类别 | 结果 |
|---|---|
| **TODO / 半截实现** | 本次触碰的 4 个文件（1 闸脚本 + 1 校验脚本 + 16 份 spec）内 `TODO/FIXME/HACK` = **0** |
| **死代码** | ⚠️ **发现并已修**：校验脚本 C-7 原写 `kw = [...][:0]`（恒空）+ `kw_ok = ... or True`（恒真）**且从未参与 checks** ⇒ 与 design §D-2/D-3 两处承诺（isfile **且** 关键字命中）不一致 ⇒ **已删除死代码并落实关键字命中**（critic SHOULD_FIX-3 + verifier 独立复算双向确认） |
| **被 skip 的测试 / `.disabled`** | `test/CMakeLists.txt` 内 `mayfail` / `[!skip]` / `.disabled` = **0** |
| **未替换的 mock** | N/A（本 change 无 mock 面） |
| **临时 fallback** | 校验脚本的 `DOMAIN_TOKENS`（领域缩写白名单，如 `BAR`/`MMIO`/`AXI`/`PCIe`）为 **C-6 兜底** ⇒ 已登记在 design §D-3，并在脚本内注释说明用途 |
| **资源释放** | N/A（无进程/句柄） |

---

## 本次验收的**发现**

| # | 发现 | 性质 | 处置 |
|:--:|---|---|---|
| **F-1** | ⚠️ **我自己的验收脚本有两处口径错**（第一轮跑出 8/10 的**假阴性**）：① 用 `listdir` 数归档目录 ⇒ 把 `STATUS_AUDIT_2026-06-15.md` 也计入（151 vs 实测 150）② 范围零 diff 用了 `test/`（整个目录）而非 `test/*.cc`（实现层测试文件）⇒ 误命中本 change 自己的归因载体 | **验收方法缺陷** | **已修正口径并重跑 ⇒ 10/10**；留痕于此，避免下轮复现 |
| **F-2** | 立项时**未同步 6 处 KPI 活口径** ⇒ change 处于 active 窗口期间 `cep_docs_sync_check` / `cep_gate_selftest` 报红（KPI 口径写 0/3 而实测 1/3）⇒ 归档后自动恢复 | **流程缺口**（非本 change 的 AC 失败） | 已写入 Entry #442 §⑤·3；建议下一轮 planner 把「立项即改 6 处口径」列入动作清单 |
| **F-3** | 台账登记 R-10/R-12/R-14 时引用了 **4 条已删除路径** ⇒ `cep_doc_ref_check` 报红 | 连带缺口 | 已按 `AGENTS.md` 约定补 `VIRTUAL_PATHS` 4 条（各带理由）⇒ 恢复绿；verifier 复验 rc=0 |
| **F-4** | **C-6 的机判边界**：**纯中文的能力声称不在机判射程内**（CJK-LCS < 12 时无法区分「合理概括」与「凭空声称」）⇒ 该边界由**人工抽验 + critic 4 组逐组签**承担 | 已知边界（已声明） | 已写入 design §D-3 C-6；批 2~5 **须保留同等人审环节** |
| **F-5** | `test/spec_purpose_batch1_attribution.json` 的 `requirement_titles` 含过程括注（如 `flr-abi-entry (v1.1 删除)`） | 轻微（机器消费字段，不影响判定） | 建议批 2 起存裸标题 |

---

## 失败项与路由建议

**无失败项** ⇒ 无需路由（`implementation_gap` / `proposal_drift` 分类不适用）。

---

## 已用轮次与后续

- **已用轮次**：**1**（上限 3，与 `RDDF_VERIFIER_MAX_LOOPS=3` 对齐）。
- **结论**：**PASS** —— change `cpptlm-spec-purpose-batch1` 的 **10/10** 判据均取得工具证据并通过。
- **⛔ 本验收不改实现**：全程只读取证（`read` / `grep` / `shell` / `git`），未修改任何实现或规范文件。
- **后续**（见台账 Entry #443 与 Entry #438 §3 的任务清单）：
  ① **R-1 批 2**（70 份正文 >500 字符裁剪 —— **会动 `## Requirements` 段 ⇒ 另一档风险，须另立 change**）
  ② **R-1 批 3~5**（其余 59 份 Purpose —— 复用 design §D-0 的 7 条写作规则 + 本批的校验脚本与归因载体范式）
  ③ **architect 备案**：R-9（清单增殖 canonical）/ R-2（`moesi_gpu`·`mesif` 13 键 golden）/ R-10（superseded spec 存在性裁定）/ R-11（4 份同源 spec 合并退役）/ `AGENTS.md` 归档分类表补记「✅ 已完成归档 · 主 spec 直改（无 delta）」变体
  ④ **R-6** 版本 pinning · **R-3** ArchForge 缺席（本机不成立）

**关联**：台账 Entry **#441**（立项）· **#442**（收口）· **#440**（上一 change 的 verifier 验收）· **#438**（全量梳理）· `openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/proposal.md`（`## 验收标准` 段 = 本报告判据来源）· `scripts/test/cep_spec_purpose_batch1_check.py` · `test/spec_purpose_batch1_attribution.json`
