---
name: cpptlm-spec-purpose-batch1
priority: P1
phase: phase-10
主题: CppTLM 规范面 —— 非 CEP 主 spec 的 Purpose 占位修复（批 1：16 份）
依赖:
  - ADR-CEP-00
状态: 🟡 **已立项**(2026-11-15 · planner 起草 · metis 预审 7 MUST + 8 SHOULD + 5 NIT **全处置** · 台账 Entry #441 · KPI **0/3 → 1/3**)
created: 2026-11-15
---

# improvement: cpptlm-spec-purpose-batch1

**主题**: 闭合 **R-1 批 1** —— **16 份**非 `cep-*` 主 spec 的 `## Purpose` 仍是 `TBD - created by archiving change …` 占位。

> **范围纪律**: ⛔ 零实现层 · ⛔ **只改 `## Purpose` 段**（Requirement / Scenario 正文**零改动**）· ⛔ 不动 43 份 `cep-*`（已 --strict 全绿）· ⛔ 不裁正文（属 R-1 批 2）· ⛔ 不新增闸。
> **估时**: **1.2 人日**（16 份 Purpose 落地 + 归因载体 + 校验脚本 + 预演 + 逐份复验）。
> **KPI**: 占 OpenSpec slot **1/3**（**归属已裁决**：改的是 `openspec/specs/` **契约面**，**不适用** `AGENTS.md` 的 doc-only 豁免 ⇒ **计入**）。

---

## 架构依据

| 依据 | 来源（一手） | 要点 |
|---|---|---|
| **Purpose 只能直改主 spec** | `openspec validate` 的 WARNING 原文 + **本轮影子根实验** | 「Replace it … **editing the main spec directly**」；实验实测归档器原话 `delta Purpose ignored; <spec> already has one. Edit … spec.md directly to change it.` ⇒ **0 份 delta 是唯一路径** |
| **上一 change 已验证的 Purpose 四条正交判据** | `openspec/changes/archive/2026-11-15-cpptlm-cep-spec-strict-compliance/`（W4 = 8 份 Purpose 直改） | ① `openspec --json` 判 valid ② ≥50 字符 ③ 不复制 Requirement 首句 ④ 不含 `本 change`/日期 ⇒ **本 change 把 ③ ④ 定死为机判形式**（metis P0-4） |
| **归因载体必须由脚本生成** | metis S-5 | 人手填 `checks` 四布尔 = **自证** ⇒ 判别力为零 ⇒ 该文件由校验脚本生成 |
| **R-1 的真实构成（实测）** | 2026-11-15 逐份 `openspec validate --strict --json` | 非 `cep-*` **86** 份中：**75** 份 Purpose 为 `TBD/TODO` 占位（全为同一形态，**无分叉**）· **70** 份 Requirement 正文 >500 字符（重叠）⇒ 本 change 只做 Purpose |
| **superseded spec 不能写现状** | `cpptlm-d1-p1-pipeline-scoreboard/spec.md:54` + 实测 | `Status: SUPERSEDED` · `include/cudart/` **整个目录不存在** · `include/tlm/gpu/{scoreboard,pipeline,tensor_core}_tlm.hh` **全不存在** ⇒ **移出批 1**（R-10） |
| **16 份结构完全统一** | 实测 `## Purpose` 都在 line 3、占位 line 4、`## Requirements` line 5 | ⇒ **预期 diff 形状可写死**：每份恰 1 hunk、删 1 增 1、**不新增空行** |
| **CEP 冻结面不受影响** | `cep_freeze_check.sh` 的 `FROZEN_GLOBS` | 冻结面 = `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `openspec/specs/cep-*/spec.md` ⇒ 这 16 份**均不在** ⇒ **零冻结产物 ⇒ 免五步链** |

## 范围

**In Scope —— 批 1 的 16 份**（`pcie-*` 10 + `cpptlm-*` 6）：

`pcie-axi-datapath-hardening` · `pcie-cfg-encoding-minor` · `pcie-flr` · `pcie-host-test-flr-rebar` · `pcie-host-test-infrastructure` · `pcie-ip-integration` · `pcie-ip-microarch` · `pcie-resizable-bar-runtime` · `pcie-slice-prerequisites` · `pcie-tlp-wire-datapath` · `cpptlm-pcie-endpoint-ip-json-config` · `cpptlm-pcie-ep-foundation` · `cpptlm-stage-1-1-fixes` · `cpptlm-stage-1-2-msix` · `cpptlm-stage-1-4-2-1` · `cpptlm-stage-1-4-2-1-ue-extensions-unblock`

（`cpptlm-emulator-abi` / `cpptlm-emulator-asan-cleanup` 已有实质 Purpose ⇒ 排除；`cpptlm-d1-p1-pipeline-scoreboard` **superseded** ⇒ 移出 ⇒ R-10）

**改法**：仅替换 `## Purpose` 段（逐份 canonical 草稿 + **写作规则 D-0** 见 `design.md`）。
**载体**：
- **0 份 delta**（⛔ Purpose 不能走 delta —— 工具明文 + 实验实测）
- **1 份归因载体** `test/spec_purpose_batch1_attribution.json`（**由脚本生成**）
- **1 个校验脚本** `scripts/test/cep_spec_purpose_batch1_check.py`（实现 D-3 的 C-1~C-7 机判项）

**Out of Scope**:
- ⛔ **不裁 Requirement 正文**（>500 字符的 70 份 ⇒ R-1 批 2，**另一档风险**）
- ⛔ 不动 43 份 `cep-*` · ⛔ 不动实现层 · ⛔ 不新增闸 · ⛔ 不改 `## Requirements` 段的任何字节
- ⛔ 不动其余 59 份非 `cep-*` 的 Purpose（批 3~5）· ⛔ 不处理 R-10~R-14（登记，不在本 change 修）

## Why

**第 1 条 —— 占位 Purpose 使「这份规范管什么」在机器与在人两侧都不可读。**

现状：75 份主 spec 的 Purpose 是 `TBD - created by archiving change <name>. Update Purpose after archive.` —— 这是 `openspec archive` 自动写的占位，**归档后从未被人改写**（实测：非 `cep-*` 86 份里 75 份占位、11 份有实质 Purpose）。

后果：① **人**：读规范不知道该能力的边界与用途 ⇒ 只能翻 change 的 proposal。② **机器**：`--strict` 把占位判为 WARNING ⇒ 全仓 `--strict` 红的 77 份里 75 份因此红。③ **工具**：`openspec` 对新建能力的 Purpose 有明确质量要求（≥50 字符、非占位），而这批能力归档时**没走 delta** ⇒ **工具自己留了坑，人没填**。

**第 2 条 —— 写 Purpose 是「授权契约内容」，口径必须收窄为「忠实记录既有实现」。**

Purpose 描述的是**能力是什么**，属契约面 ⇒ 授权行为。但本 change **不新增任何能力** ⇒ 口径收窄：
- **事实基础** = 来源 change 的 proposal「为什么」段（实施期写下的意图陈述）+ 该 spec 的 Requirement 标题（能力面）⇒ 二者都是**实施时既有事实**。
- ⇒ 追加**反向存在性**判据（C-6）：Purpose 里的标识符必须在 Requirement 标题或仓内可 grep 的符号里能找到；找不到 ⇒ 该条须显式声明 `claim_kind != current`。
- ⇒ **superseded 一律移出**（R-10）—— 写现状即立假账。

**第 3 条 —— 只做 Purpose、不动 Requirements 正文 ⇒ 风险可控。**

Purpose 段与 Requirement 段是**两个独立段** ⇒ 改动**不触及任何可验证行为** ⇒ 本 change **不改变任何判据的判别力**（与 R-7 同性质：表述/结构修正）。
⚠️ **正文 >500 字符的裁剪（批 2）会动 Requirement 段** ⇒ **另一档**风险，**必须另立 change**。

**第 4 条 —— 口径澄清：R-1 的「77 份」是 `--strict` 口径。**

承接 Entry #438 §3：非 strict 口径下 R-7 修完后 **rc≠0 = 0** ⇒ R-1 剩余全部是 **WARNING 级**（Purpose 占位 + 正文超长）⇒ **不阻塞非 strict 使用**；但它是 `openspec validate --strict` / CI 严格模式的阻塞项 ⇒ 是「规范面可信」的最后一公里。

**第 5 条 —— 批大小：保持 16 份一批，拆的是「评审」不是「change」。**

拆批会把固定成本（立项 / AC 复验 / 归档 / 台账 2 条）乘 3（KPI 上限 3 ⇒ 拆 3 批只能串行）。真正的风险是**评审注意力**（16 段文案一次过）⇒ critic 按 **4 组**签（`pcie-*` 数据通路 / `pcie-*` 集成与微架构 / `cpptlm-stage-*` / `cpptlm-*` 其他），一组不过只回退该组。

## What Changes

1. **16 份主 spec 的 `## Purpose` 段**：占位 → 实质 Purpose（canonical 草稿见 `design.md` §D-1；**规则**见 §D-0）。
2. **1 个校验脚本** `scripts/test/cep_spec_purpose_batch1_check.py`（D-3 的 C-1~C-7）。
3. **1 份归因载体** `test/spec_purpose_batch1_attribution.json`（**由脚本生成**，不手填）。
4. **1 份 improvement** + **1 份 proposal / design / tasks** + 台账 Entry **#441**（立项）/**#442**（收口）。

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | 16 份 Purpose **零占位** | **限段** `sed -n '/## Purpose/,/## Requirements/p'` 后 `TBD`/`TODO` 计数 = **0** |
| **AC-02** | **定向**：16/16 不再出现 Purpose 类条目 | 逐份 `openspec validate --strict --json` 输出中**无** Purpose 相关 issue ⇒ **16/16**（永远可达） |
| **AC-02b** | **全局**：红数下降**且闭合** | `pre − post = k`，`k` = 「修完整份转绿」的份数；**k 与剩余红数必须逐份归因到具体 spec + 具体原因**（Purpose-only 转绿 16−k 份 / 仍因正文超长 n 份）⇒ 数字**不写死**，但**必须闭合** |
| **AC-03** | **Purpose 四条正交（逐份机判）** | ① `--json` 判 valid ② `50 ≤ len ≤ 160` **且单物理行** ③ 规范化后与该 spec 全部 Requirement 正文的 **LCS < 12**（`waiver: single-requirement` ⇒ < 16）④ ⛔ 含过程叙述 / 技术字面量 / change 引用 / 日期 ⇒ 红 |
| **AC-04** | **零语义改动 + diff 形状写死** | **16 文件 · 每文件恰 1 hunk · 删 1 行增 1 行 · 不新增空行**；`## Requirements` 段**逐字节不变** |
| **AC-05** | **不得声称不存在的能力** | `claim_kind` ∈ {`current`,`historical`,`superseded`} 且与 spec 内 `Superseded`/`物理删除` 标记**互斥**；Purpose 标识符 ⊆ Requirement 标题 ∪ 仓内可 grep 符号；`source_evidence` **全部 `isfile` == True** 且关键字命中 |
| **AC-06** | `cep-*` 不受影响 | 43 份逐份 `--strict` ⇒ rc≠0 = **0**；`artifact_count` **86 不变**（零冻结产物 ⇒ 免五步链） |
| **AC-07** | 范围零 diff | `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` / `openspec/specs/cep-*/spec.md` 全 0 |
| **AC-08** | 闸 0 回归（**归档前跑一次**即可） | **45 闸**全 rc=0（ctest 同参数）· `ctest -R cep` / `[cep]` == T0 |
| **AC-09** | KPI + 体量 | **0/3 → 1/3**（立项）→ **0/3**（归档）+ 归档 change 数 **149 → 150** |
| **AC-10** | **归档语义硬门** | T3.0 由 architect 裁决并落 **出口 A 或 B（二选一）**；归档后目录**可判别**（A：proposal 顶部标记 + JSON `archive_semantics`；B：台账显式记「未走归档器」+ 理由）⇒ ⛔ 不得与 ⏸ 暂存归档**同形无标记** |

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|:--:|
| **C-1** | 16 份主 spec 的 Purpose 从占位变**可读且真实** | AC-01/02/03/05 | ❌ 0 |
| **C-2** | 改动**零语义**（Requirements 段逐字节不变） | AC-04/06/07 | ❌ 0 |

## Impact

| 面 | 影响 |
|---|---|
| **实现层** | ⛔ 零改动 |
| **契约** | 16 份**非 CEP** 主 spec 的 **Purpose 段**；**0 份 delta** ⇒ **零冻结产物** ⇒ 免五步链 |
| **闸** | 新增 **1 个校验脚本**（`cep_spec_purpose_batch1_check.py`，属**本 change 的自测工具**，⛔ 不改变既有 **45** 个 `cep_*` 闸的数）· ctest 条目数**不变** |
| **文档** | 归因载体 1 份 + 台账 2 条 Entry |

**⚠️ 残余（本 change 登记，不修）**：

| # | 残余 | 实测 | 路由 |
|:--:|---|---|---|
| **R-10** | `cpptlm-d1-p1-pipeline-scoreboard` 的**存在性裁定**（spec 标 SUPERSEDED + 实现物理删除 ⇒ 孤儿 spec） | `include/cudart/` 目录不存在 | **architect** |
| **R-11** | `pcie-host-test-flr-rebar` 与 3 份兄弟 spec **内容重复**（其 3 条 Requirement 是兄弟 spec 的同名复制 ⇒ 无独占内容） | Requirement 标题逐条对应 | **architect** |
| **R-12** | `pcie-slice-prerequisites/spec.md:14` 引用**已删除**的 `include/cudart/abi_guards.h` | 目录不存在 | 各 owner |
| **R-13** | spec 内部**旧状态行**与新 Purpose 打架（`pcie-ip-microarch:296`「等待评审 + Phase 1 启动实施」· `pcie-ip-integration` 末行「Phase 8 最终整合交付中」） | 实测 | 批 2（动 Requirements 段时一并处理） |
| **R-14** | `pcie-ip-integration` 要求交付的 `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` **已迁 ArchForge**（仓内仅占位 README） | 文件不存在 | 各 owner |
| **R-1 批 2** | **70** 份 Requirement 正文 >500 字符（裁剪**会动 Requirements 段** ⇒ 另一档风险） | 实测 | 另立 change |
| **R-1 批 3~5** | 其余 **59** 份 Purpose 占位 | 实测 | 后续批次 |
| **R-9 / R-2** | 清单增殖 / `moesi_gpu`·`mesif` 13 键 golden | — | **architect**（与本 change 无依赖） |

## Non-Goals

- ⛔ 不裁正文 · ⛔ 不动 Requirement / Scenario 的任何字节 · ⛔ 不动 `cep-*` · ⛔ 不动实现层 · ⛔ 不改既有 45 闸。
- ⛔ 不发明实现中不存在的能力（AC-05）· ⛔ 不处理 R-10~R-14 · ⛔ 不裁决 R-2 / R-9。
- ⛔ 不改 `AGENTS.md`（归档语义补全属治理面 ⇒ architect 流程）。

---

## metis 预审处置对照表（第 1 轮 · 7 MUST + 8 SHOULD + 5 NIT）

| # | metis 条目 | 处置 |
|:--:|---|---|
| **MUST-1** | `cpptlm-d1-p1-pipeline-scoreboard` 已 superseded + 实现物理删除 ⇒ 写现状即**立假账** | ✅ **采纳** —— **移出批 1**（17→**16** 份）+ 登记 **R-10**；并加写作规则 **R-3**（spec 含 `Superseded` ⇒ 一律移出） |
| **MUST-2** | AC-05「文件存在 + 关键字命中」是**文本形式检查**，非事实检查 | ✅ **采纳** —— 换 `claim_kind` 枚举 + 与 `Superseded` 标记**互斥校验** + **标识符 ⊆ Requirement 标题 ∪ 仓内可 grep 符号**（D-3 C-6） |
| **MUST-3** | 引用了**不存在的** `Entry #441`，且 T0.6 已勾 `[x]` | ✅ **采纳** —— **Entry #441 已真实落盘**（本 improvement 的立项 Entry） |
| **MUST-4** | AC-03③「不复制 Requirement 首句」无边界（例：`pcie-cfg-encoding-minor` 语义复述可逐字节放行） | ✅ **采纳** —— 定死为「规范化 + **LCS < 12**（CJK 计 1）+ 硬禁技术字面量 + 单 Requirement 豁免（LCS<16 + 强制起句 + `waiver`）」 |
| **MUST-5** | AC-02 的 `≤62` 算术错 + **方向可能不可达**（6 份含长正文 ⇒ post ≥ 61） | ✅ **采纳** —— 换**定向**（16/16 不再有 Purpose 条目，永远可达）+ **全局**（`pre−post=k` 且 k 与剩余红数**逐份闭合**，数字不写死） |
| **MUST-6** | `skip_specs` 与 ⏸ 暂存归档**结构同形** ⇒ 可判别性不成立 | ✅ **采纳（部分）** —— 升级为**归档前硬门 T3.0** + 两条互斥出口（A：标记行 + JSON `archive_semantics`；B：不走归档器 + 台账显式记理由）+ **失败代价写进 proposal**（KPI 卡 1/3 ⇒ 后续提案全被 3/3 阻塞）。⚠️ **最终口径仍需 architect 裁决**（设计侧只能把两条路摆清楚） |
| **MUST-7** | KPI 归属未裁（doc-only 豁免是否适用）悬置 | ✅ **采纳** —— **裁决为「计入」**：本 change 改 `openspec/specs/`（**契约面**），而 `AGENTS.md` 的豁免限定「改 docs + 注释 + 注解」且 0 函数/逻辑/构建改动 ⇒ **不适用** ⇒ 计入 1/3 |
| **SHOULD-8** | 归因载体 3 处来源 change 路径错 | ✅ **采纳** —— 逐处订正为**实测路径**（`2026-10-09-…` / `2026-09-10-2026-09-10-…` / `2026-09-03-2027-02-09-…`） |
| **SHOULD-9** | Purpose #2「非法取值必须被显式拒绝」越界（实为 warning + 回退） | ✅ **采纳** —— 改写为「以告警形式暴露并在回退到缺省取值后继续运行」 |
| **SHOULD-10** | Purpose #6 把 ACS 说成能效（错） | ✅ **采纳** —— 改定性为「电源管理（能效）+ 拓扑与**隔离**增强」 |
| **SHOULD-11** | Purpose #11 含过程叙述 + 未说明与兄弟 spec 的关系 | ✅ **采纳** —— 去掉过程叙述 + 明写「归档载体 / 汇总三项契约 / 独立判据见各自规范」+ 登记 **R-11** |
| **SHOULD-12** | 17 条 Purpose 向「Requirement 摘要」漂移 | ✅ **采纳** —— 写作规则 **R-1**（≤2 句 + ≤160 字符 + ⛔ 不复述清单）；16 份草稿已全部压缩 |
| **SHOULD-13** | AC-04 的「允许 diff 形状」没定死（3 行空行纠缠史） | ✅ **采纳** —— 写死「16 文件 · 每份 1 hunk · 删 1 增 1 · 不新增空行 · 单物理行」 |
| **SHOULD-14** | tasks 缺 T1.0 / T2.8 / T3.0 | ✅ **采纳** —— 全部补入 |
| **NIT-15** | 批大小 | ✅ **采纳（反向）** —— **保持 16 份一批**（拆批把固定成本乘 3）；拆的是**评审**（critic 4 组签） |
| **NIT-16** | AC-08 对纯文本改动过度验证 | ✅ **采纳** —— 闸族只跑**归档前**一次；增量阶段用 `validate --changes --strict` + `git diff --stat` 白名单 |
| **NIT-17** | 归因载体 `checks` 四布尔自证 | ✅ **采纳** —— 改为**脚本生成**，人只填原始输入 |
| **NIT-18** | Capabilities 未映射到可反查位置 | ✅ **采纳** —— 归因 JSON 增 `result_commit` 字段 |
| **NIT-19** | 暴露的 5 处既有缺口 | ✅ **采纳** —— 登记 **R-10 ~ R-14**（本 change 不修） |
