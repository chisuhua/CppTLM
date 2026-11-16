---
name: cpptlm-spec-purpose-batch3
priority: P1
phase: phase-10
主题: CppTLM 规范面 —— 非 CEP 主 spec 的 Purpose 占位修复（批 3：dGPU 板级 / SoC 域 17 份）
依赖:
  - ADR-CEP-00
状态: 🟡 **已立项**(2026-11-16 · planner 起草 · 台账 Entry #444 · KPI **0/3 → 1/3**)
created: 2026-11-16
---

# improvement: cpptlm-spec-purpose-batch3

**主题**: 闭合 **R-1 批 3** —— **14 份**非 `cep-*` 主 spec 的 `## Purpose` 仍是 `TBD - created by archiving change …` 占位（**dGPU 板级 / SoC 域**）。

> ⚠️ **初稿 17 份 → 终稿 14 份**（metis MUST-1 复核后**实测扩大**）：3 份的来源 change 是**未实施**草案 ⇒ 移出 + 登记 **R-16**。

> **范围纪律**: ⛔ 零实现层 · ⛔ **只改 `## Purpose` 段**（Requirement / Scenario 正文**零改动**）· ⛔ 不动 43 份 `cep-*`（已 --strict 全绿）· ⛔ 不裁正文（属 R-1 批 2，**另一档风险**）· ⛔ 不新增闸。
> **估时**: **1.2 人日**（14 份 Purpose 起草 + 落地 + 归因载体 + 校验脚本 + 预演 + 逐份复验）。
> **KPI**: 占 OpenSpec slot **1/3**（改的是 `openspec/specs/` **契约面** ⇒ **不适用** doc-only 豁免 ⇒ **计入**）。

---

## 架构依据

| 依据 | 来源（一手） | 要点 |
|---|---|---|
| **Purpose 只能直改主 spec** | 归档器实测原话（批 1 实测） | `delta Purpose ignored; <spec> already has one. Edit … spec.md directly to change it.` ⇒ ⛔ **0 份 delta 是唯一路径** |
| **批 1 已验证的完整范式** | `openspec/changes/archive/2026-11-15-cpptlm-spec-purpose-batch1/` | ① 写作规则 **D-0（7 条）** ② 校验脚本（**C-1~C-7**）③ 归因载体（`checks` **由脚本重算**）④ critic **分 4 组逐组签** ⑤ verifier **另写独立实现**复算 ⇒ **本批直接复用** |
| **批 1 实测的 3 处判据缺陷（必须避免重犯）** | 同上 + Entry #442 §⑤ | ① **C-3 度量假阳性**（`PcieEndpointIP` 同名即触发）⇒ 已拆为 **CJK-only LCS + 英文词序列** ② **TBD 行声明的 change 名 ≠ 归档目录名** ⇒ ⛔ 不可用 TBD 行做 glob 键 ③ **归档目录名带双日期前缀** ⇒ 只能 glob 解析 |
| **R-1 的剩余构成（2026-11-16 实测）** | 逐份 `openspec validate --strict --json` | `--strict` 红 **65** 份（全为非 `cep-*`）= **59** 份 Purpose 占位 + **69** 条「正文 >500」issue（落在 **26** 份）⇒ 本批只做 Purpose |
| **批 3 的域选择** | 实测前缀分布（39 个前缀散落） | 按**能力域**聚类成 4 组 ⇒ 批 3 = **D3 dGPU 板级 / SoC 域（17 份）**（与 AGENTS.md 主线一致，planner 最熟） |
| **CEP 冻结面不受影响** | `cep_freeze_check.sh` 的 `FROZEN_GLOBS` | 冻结面 = `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `openspec/specs/cep-*/spec.md` ⇒ 本批 17 份**均不在** ⇒ **零冻结产物 ⇒ 免五步链** |

## 范围

**In Scope —— 批 3 的 14 份**（D3 域 · dGPU 板级 / SoC）：

`dgpu-4ip-port-contract` · `dgpu-board-abi-hygiene` · `dgpu-board-adapter-info` · `dgpu-soc-timing` · `driver-visible-minimal-soc` · `gmmu-pte-wf` · `lwc-bus-hub-mvp` · `minimal-dgpu-soc-abi-landing` · `minimal-dgpu-soc-architecture` · `minimal-dgpu-soc-test-coverage` · `sdma-engine-tlm` · `soc-internal-noc` · `soc-internal-noc-data-flow` · `vram-controller-functional-mode`

**⛔ 已移出本批（3 份 · 来源 change **未实施** ⇒ 写现状即立假账 ⇒ 登记 R-16）**：`dgpu-board-mvp`（`dgpu_board_mvp.hh/.cc` 与 `dgpu_board_v1_mvp.json.in` 均不存在）· `dgpu-soc-control-hub`（`ControlHub` 0 命中 + `PcieMemoryDevice` 仍在 `configs/dgpu_soc_minimal_v1.json:49`）· `dgpu-soc-physical-ip-port-boundary`（`gmmu_tlm.hh:142` 明写 `wire-format 统一 PcieTlpBundle`，未切 `NoCFlitBundle`）。**依据 = 实现层行为锚点**（⛔ **不用 tasks 勾选** —— 批 1 F-6 教训：归档后执行未回勾 ⇒ tasks 是记录债，非实施证据）。

**载体**：
- **0 份 delta**（⛔ Purpose 不能走 delta）
- **1 个校验脚本** `scripts/test/cep_spec_purpose_batch3_check.py`（D-3 的 C-1~C-7）
- **1 份归因载体** `test/spec_purpose_batch3_attribution.json`（**由脚本生成**）

**Out of Scope**:
- ⛔ **不裁 Requirement 正文**（26 份有 >500 issue ⇒ R-1 批 2，**会动 `## Requirements` 段 ⇒ 另一档风险**）
- ⛔ 不动 43 份 `cep-*` · ⛔ 不动实现层 · ⛔ 不改既有 45 闸 · ⛔ 不改 `## Requirements` 段的任何字节
- ⛔ 不动其余 45 份非 `cep-*` 的 Purpose（批 4~6）· ⛔ 不处理 R-9~R-18（登记，不在本 change 修）· ⛔ **未实施草案 spec 不在本批**（见 R-16）

## Why

**第 1 条 —— 占位 Purpose 使「这份规范管什么」在机器与在人两侧都不可读。**

现状：**59 份**主 spec 的 Purpose 是 `TBD - created by archiving change <name>.` —— 归档器自动写的占位，归档后从未被人改写。后果：① **人**：读 dGPU 板级 / SoC 这类规范不知道能力边界 ⇒ 只能翻 change 的 proposal；② **机器**：`--strict` 把占位判为 WARNING ⇒ **65 份红里 59 份因此红**；③ **工具**：对新能力的 Purpose 有明确质量要求（≥50 字符、非占位），而这批能力归档时**没走 delta** ⇒ **工具自己留了坑、人没填**。

**第 2 条 —— 写 Purpose 是「授权契约内容」，口径必须收窄为「忠实记录既有实现」。**

本 change **不新增任何能力** ⇒ 口径收窄：事实基础 = 来源 change 的 proposal「为什么」段（**实施期**写下的意图陈述）+ 该 spec 的 Requirement 标题（能力面）⇒ 二者都是**实施时既有事实**。追加**反向存在性**判据（C-6）：Purpose 里的标识符必须在 Requirement 标题或仓内可 grep 的符号里找得到（逐条依据见 `design.md` D-1 末「可追性核对」表）。

**第 3 条 —— 只做 Purpose、不动 Requirements 正文 ⇒ 风险可控。**

Purpose 段与 Requirement 段是**两个独立段** ⇒ 改动**不触及任何可验证行为** ⇒ 不改变任何判据的判别力（与 R-7 同性质）。⚠️ **正文 >500 的裁剪（批 2）会动 Requirement 段** ⇒ **另一档**风险，**必须另立 change**。

**第 4 条 —— 批 3 的域选择：dGPU 板级 / SoC 域。**

按**能力域**聚类（而非字符串前缀）把剩余 59 份切成 4 组。批 3 取 **D3（dGPU 板级 / SoC，17 份）**：① 与 `AGENTS.md` 的仓主线一致（dGPU SoC + PCIe EP 是本仓主体）⇒ 事实基础最扎实；② 域内 17 份的来源 change **15 份可 glob 解析** + 2 份有**真实替代证据**（`docs/designs/dgpu-soc/timing-mode.md` / `docs/pcie/driver-visible-minimal-soc.md`）⇒ 归因可追；③ 域内 **0 份 spec 级 superseded**、**0 份单 Requirement** ⇒ **无 waiver、无移出** ⇒ 判据面最简。

## What Changes

1. **17 份主 spec 的 `## Purpose` 段**：占位 → 实质 Purpose（canonical 草稿见 `design.md` §D-1）。
2. **1 个校验脚本** `scripts/test/cep_spec_purpose_batch3_check.py`（C-1~C-7）。
3. **1 份归因载体** `test/spec_purpose_batch3_attribution.json`（**由脚本生成**）。
4. **1 份 improvement** + **1 份 proposal / design / tasks** + 台账 Entry **#444**（立项）/**#445**（收口）。

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | 14 份 Purpose **零占位** | **限段**扫 `TBD`/`TODO` 计数 = **0** |
| **AC-02** | **定向**：14/14 无 Purpose 类 issue | 逐份 `openspec validate --strict --json` 输出中**无** `placeholder` 类 issue ⇒ **17/17**（永远可达） |
| **AC-02b** | **全局**：`pre − post = k` 且逐份归因闭合 | 载体 = `test/spec_purpose_batch3_reconcile.json`（schema `spec-purpose-reconcile/v1`）· 机判：`fixed = pre.red − post.red`；`set(post.red) ∪ set(fixed) == set(pre.red)`；两集**不相交**；`k = len(fixed)`；每个 `remaining[].reasons` 必须是 post 实跑的**真实 issue code**；`fixed` 的 pre issues ⊆ **仅 Purpose 类** |
| **AC-03** | **Purpose 四条正交**（逐份机判） | ① `--json` 判 valid ② `50 ≤ len ≤ 160` **且单物理行** ③ **CJK-only LCS < 12** + **英文词序列公共 < 5 词** ④ ⛔ 过程叙述 / 技术字面量 / change 引用 / 日期 |
| **AC-04** | **零语义改动 + diff 形状** | **14 文件 · 每文件恰 1 hunk · 增 1 行 · 删 1 行（占位后**无**尾随空行）或 2 行（占位后**有**尾随空行 —— 本批 1 份：`vram-controller-functional-mode`）· ⛔ 不新增空行**；`## Requirements` 段**逐字节不变**（实测 14/14 变动 0）。⚠️ **判据订正**：初稿的「删 1 增 1」是**过严代理** ⇒ 实质判据是「Requirements 段逐字节不变」，删行数降为形状描述 |
| **AC-05** | **不得声称不存在的能力** | ① `claim_kind` 与 **spec 级** superseded 标记**互斥**（C-6）② 每份的 `implementation_anchors` **全部命中**（C-8）③ Purpose 标识符 ⊆ Requirement 标题 ∪ 仓内可 grep 符号 ∪ 领域缩写白名单 ④ 证据**可复现**且核心名命中（C-7） |
| **AC-06** | `cep-*` 不受影响 | 43 份逐份 `--strict` ⇒ rc≠0 = **0**；`artifact_count` **86 不变**（零冻结产物 ⇒ 免五步链） |
| **AC-07** | 范围零 diff | `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` / `openspec/specs/cep-*/spec.md` 全 0 |
| **AC-08** | 闸 0 回归（**归档前跑一次**） | **45 闸**全 rc=0（ctest 同参数）· `ctest -R cep` / `[cep]` == T0 |
| **AC-09** | KPI + 体量 | **0/3 → 1/3**（立项）→ **0/3**（归档）+ 归档 change 数 **150 → 151** |
| **AC-10** | **归档语义可判别** | 出口 **A**（`skip_specs: true` + proposal 顶部「✅ 已完成归档 · 主 spec 直改（无 delta）」标记行 + 归因 JSON `archive_semantics` 字段）⇒ 归档后目录**可判别**，⛔ 不得与 ⏸ 暂存归档同形无标记 |

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|:--:|
| **C-1** | 14 份主 spec 的 Purpose 从占位变**可读且真实** | AC-01/02/03/05 | ❌ 0 |
| **C-2** | 改动**零语义**（Requirements 段逐字节不变） | AC-04/06/07 | ❌ 0 |

## Impact

| 面 | 影响 |
|---|---|
| **实现层** | ⛔ 零改动 |
| **契约** | 14 份**非 CEP** 主 spec 的 **Purpose 段**；**0 份 delta** ⇒ **零冻结产物** ⇒ 免五步链 |
| **闸** | 新增 **1 个校验脚本**（属本 change 自测工具，⛔ 不改变既有 **45** 个 `cep_*` 闸的数）· ctest 条目数**不变** |
| **文档** | 归因载体 1 份 + 台账 2 条 Entry |

**⚠️ 残余（本 change 登记，不修）**：

| # | 残余 | 实测 | 路由 |
|:--:|---|---|---|
| **R-15** | `v1-architecture` 引用 `docs/soc_arch/architecture/00-overview.md`（**已迁 ArchForge**，仓内仅占位 README） | 文件不存在 | 各 owner（与 **R-14** 同型） |
| **R-16** | **3 份 spec 的来源 change 未实施 ⇒ 孤儿 spec 存废裁定**：`dgpu-board-mvp`（`dgpu_board_mvp.hh/.cc` 不存在）· `dgpu-soc-control-hub`（`ControlHub` 0 命中 + `PcieMemoryDevice` 仍在 config）· `dgpu-soc-physical-ip-port-boundary`（`gmmu_tlm.hh:142` 未切 `NoCFlitBundle`） | 实现锚点全不命中 | **architect**（与 **R-10** 同型） |
| **R-17** | **2 份 spec 的来源 change 归档目录缺失**（曾存在，commit `0345d283` / `689752ef` 实测）⇒ 归因走 `git-history-proposal` | `git log --all --diff-filter=A` 命中 | 各 owner（目录被移走/改名） |
| **R-18** | `minimal-dgpu-soc-architecture` 的 Requirement `doorbell 配置 vs 代码 hardcoded 不一致` = **未收敛的不一致** ⇒ 草稿**不提**（不写进 Purpose） | spec 明写 | 各 owner |
| **R-1 批 2** | **26** 份有「正文 >500」issue（裁剪**会动 Requirements 段** ⇒ 另一档风险） | 实测 69 条 issue | 另立 change |
| **R-1 批 4~6** | 其余 **45** 份 Purpose 占位（D1 5 / D2 20 / D4 17 + 3 份已移出的未实施 spec） | 实测 | 后续批次 |
| **R-9 / R-2** | 清单增殖 / `moesi_gpu`·`mesif` 13 键 golden | — | **architect** |
| **R-10 / R-11** | superseded spec 存废 / 4 份同源 spec 合并退役 | — | **architect** |
| **B3b-2 = 0/4** | CXL 2.0 / CXL 3.0 / NVLink / UALink **4 族 external 阻塞**（规范不可读） | `test/cep_b3b2_deferrals.json` | **external**（本仓不可推进） |
| **DoD 7 口径** | 「66 条 invariant 机器检查 100% 覆盖」实测 = **61 条有判定级载体 + 5 条 ⏭-unmodelled** | `test/cep_invariant_assertion_map.json`（61）+ `test/cep_invariant_deferrals.json`（5） | **architect**（改 07 号 ⇒ 触冻结面 ⇒ 五步链） |

## Non-Goals

- ⛔ 不裁正文 · ⛔ 不动 Requirement / Scenario 的任何字节 · ⛔ 不动 `cep-*` · ⛔ 不动实现层 · ⛔ 不改既有 45 闸。
- ⛔ 不发明实现中不存在的能力（AC-05）· ⛔ 不处理 R-9~R-15 · ⛔ 不裁决 R-2 / R-9 / B3b-2 / DoD 7 口径。

---

## 批 1 的复用与差异（**显式登记**）

| 面 | 批 1 | 批 3 | 说明 |
|---|---|---|---|
| 写作规则 D-0 | 7 条 | **7 条不变** | ✅ 直接复用 |
| C-1 / C-2 / C-3 / C-4 / C-5 | 同 | **同** | ✅ 直接复用 |
| **C-6** | spec 内含 `Superseded|物理删除` ⇒ 不得 `current` | **spec 级**判据（状态行 / 本 spec 的状态更新） | ⚠️ **放宽到 spec 级**（批 3 实测：9 份命中里 8 份是**行级历史引用**，批 1 口径会假红）⇒ 批 1 已归档，**不追溯** |
| **C-7** | 核心名命中 | + **`evidence_kind`**（`archived-change-proposal` / `repo-doc`） | ⚠️ **扩展**（2 份主 spec 直写、无归档目录） |
| 归因来源解析 | glob（键写死） | glob（键写死）+ **TBD 行禁用** | ⛔ 不用 TBD 行（批 1 实测其名 ≠ 归档目录名） |
| critic 分组 | 4 组 | **4 组**（按 D3 域内子主题） | ✅ 同 |
| verifier 独立性 | 另写独立实现 | **另写独立实现**（不同代码路径） | ✅ 同 |
