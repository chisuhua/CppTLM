---
name: cpptlm-ep-key-surface-reconciliation
priority: P2
phase: phase-10
依赖:
  - ADR-DGPU-15
  - ArchForge ADR-SOC-11
  - cpptlm-module-param-migrate-batch2（已归档 2026-11-19）
状态: 📋 **Draft**（2026-11-19 立项；未创建 OpenSpec change，待排期后由 planner 立项）
created: 2026-11-19
---

# improvement: cpptlm-ep-key-surface-reconciliation

**主题**：`PcieEndpointIP` 的 **JSON 键面契约**在 ArchForge 侧的可查性 + 跨仓对账闸（把键面漂移从「逐键人工审计」变成「机制性检出」）

> **决策源**：[`ADR-DGPU-15`](../../docs/adr/ADR-DGPU-15-pcie-endpoint-ip-cpptlm-archforge-boundary.md)（**Proposed** 2026-11-19；oracle 2 轮 + critic 4 轮，末轮 APPROVE_WITH_CONDITIONS 无 BLOCKER）
> **触发**：change `cpptlm-module-param-migrate-batch2`（C2）验收 finding F1 —— 3 个顶层键零消费（`resizable_bars` / `num_msix_vectors` / `msix_num_vectors`），其中 `num_msix_vectors` 是**旧类** `PcieEndpointTLM` 的遗留键名被原样带进新类配置（4 份真实配置 + 12 个测试内联副本）。
> **估时**：**1.5 ~ 2d**（ArchForge 侧文档 ~0.5d + CppTLM 侧对账闸 ~1d + 评审 ~0.5d）

---

## 架构依据

| 依据 | 出处 | 本 improvement 用到的决策要点 |
|---|---|---|
| `ADR-DGPU-15` **D3** | `docs/adr/ADR-DGPU-15-*.md` §2.3 | 键面契约升格为 **ArchForge 的规格对象**：新增 `ArchForge:docs/microarchitecture/pcie-endpoint.md` + 19 号文档 §9.3 **新旧两类键面分列** + `ADR-SOC-11` 状态推进 |
| `ADR-DGPU-15` **D6** | 同上 §2.6 | 跨仓对账闸：三集合 `S_cfg` / `S_contract` / `S_impl` + **四方向**（① 红 ② 黄 ③ 黄 ④ 绿）+ **红覆盖黄** + 契约 **hash 登记** |
| `ADR-DGPU-15` **D2** | 同上 §2.2 | CppTLM 侧只保留**可机械判定**的消费面（C-1）;**语义面（C-2）归 ArchForge 契约** ⇒ 本 improvement 不引入任何语义分类器 |
| `ADR-DGPU-15` **D4** | 同上 §2.4 | 实现面**不收窄** ⇒ 本 improvement **不改** `include/tlm/pcie/**` 的实现能力 |
| `ADR-X.17` **D-3b** | `docs/adr/ADR-X.17-module-param-read-declares.md` | 「配置里有键、实现零效果」的已知缺陷 SHALL `ignore_param` 登记 + 落台账表 B ⇒ 契约里的「待修」键与台账 B 表必须同源 |
| ArchForge `docs/architecture/19-pcie-ip-microarchitecture.md` `:33` | 跨仓 | 「`PcieEndpointIP` 是 dGPU SoC 片内 PCIe Endpoint IP 的**完整 CppTLM 模型**」⇒ 规格在 ArchForge、实现在 CppTLM 的既有关系不变 |

---

## Why

**问题不是「配置没清干净」，而是四个结构性缺口**（C2 的 F1 只是它们的症状）：

| # | 缺口 | 实测证据 |
|:--:|---|---|
| **P-1** | **新旧两类的 JSON 键面在规格侧未分列** | ArchForge `19-pcie-ip-microarchitecture.md:809` 的「JSON params 子集」是**旧类** `PcieEndpointTLM` 的冻结表;新类键面只散落在示例（`:855-885`）⇒ **两类的键名集合无处可对拍** |
| **P-2** | **旧键名在迁移中未收敛** | `num_msix_vectors` 从旧类原样带进新类配置（C2 已清理 4 份配置 + 12 个测试副本，但**无机制防复发**） |
| **P-3** | **键面无规格对象** | ArchForge `ArchForge:docs/microarchitecture/` **54** 篇 IP 微架构契约，含 `endpoint` 者 **0** 篇 ⇒「每个键由谁消费、什么语义」在规格侧无处可查 |
| **P-4** | **配置面与实现面无对账机制** | 键名漂移只能靠**逐键人工审计**发现 —— C2 是靠「类型准入」这一动作**顺带**暴露的 |

**不做的后果**：下一个新增/迁移的 EP 键会重复同一形态的漂移，且只有等到某次「顺带」的类型准入才会暴露;ArchForge 侧 EP 的微架构契约继续缺席（与其它 54 篇不齐平）。

---

## What Changes

### A. ArchForge 侧（跨仓，**不在本仓**）

| # | 动作 | 判据形态 |
|:--:|---|---|
| A-1 | 新建 `ArchForge:docs/microarchitecture/pcie-endpoint.md` —— **键面契约**：每个顶层键 → 消费点（子组件 / 成员）→ 语义 → 归属（框架验证 / 硬件建模 / 已出栈）→ **新旧两类迁移关系**（旧键 → 新键，或「出栈」） | 表头含五列（机器可判）;内容经 ArchForge 侧评审（登记评审人 + 日期） |
| A-2 | 19 号文档 §9.3 的 JSON 键面 **旧类 / 新类分列** | 该节出现两列（机器可判） |
| A-3 | `ADR-SOC-11`（当前 Proposed）按 `ADR-DGPU-15` D2 二分复核后推进状态 | 状态不再是 Proposed，或其被取代关系已登记 |

### B. CppTLM 侧（本仓）

| # | 动作 | 判据形态 |
|:--:|---|---|
| B-1 | 新增跨仓对账闸脚本（`scripts/test/cep_ep_key_surface_check.sh` 或等价），接 CTest | 脚本存在 + ctest 注册 |
| B-2 | 闸的**三集合取数**：`S_cfg` = 配置（`configs/**/*.json` + `examples/*.json`）与测试内联副本里 `PcieEndpointIP` 条目的顶层键（一级嵌套按 `block.key`）;`S_contract` = 契约顶层键;`S_impl` = `load_param` / `ignore_param` / `reject_unsupported_param` 的登记 + 拒绝名单 | 取数与 `ADR-DGPU-15` §2.6 逐字一致，**不得双口径** |
| B-3 | **四方向 verdict**：① `S_cfg \ S_contract` ≠ ∅ ⇒ **红**;② `S_contract \ (S_cfg ∪ S_impl)` ≠ ∅ ⇒ **黄 + 告警**;③ `S_impl \ S_contract` ≠ ∅ ⇒ **黄 + 告警**;④ 皆空 ⇒ 绿;**①③ 共燃 ⇒ 红（红覆盖黄）** | 6 条决定性变异全部正确（见 Acceptance） |
| B-4 | 契约 **hash 登记**（台账记录基准文件 hash） | hash 不一致 ⇒ 黄 + 告警（契约被单方面改动可见） |
| B-5 | 台账 §1.1 / 表 B 与契约的「待修」列**同源**（B-21 等条的待修理由以契约语义为准） | 台账条目可回溯到契约行 |

### C. 明确不做（Non-Goals）

- ⛔ **不改实现面**（`include/tlm/pcie/**` 的能力 / 端口 / 语义）—— `ADR-DGPU-15` D4。
- ⛔ **不做语义自动比对** —— ArchForge 无代码仓，闸只能比键名集合;语义靠契约评审 + hash 登记兜住（**闸绿 ≠ 语义未漂移**，须写进脚本注释）。
- ⛔ **不新建代码仓、不迁移实现**（ArchForge 只有 `docs/` + `openspec/`）。
- ⛔ **不在本 improvement 内修订已签批 ADR 的决策**（如需收窄实现 ⇒ 独立 ADR + 独立 change，且须含**键级 diff**，`ADR-DGPU-15` D5）。

---

## Acceptance

> 判据来自 `ADR-DGPU-15` 的 V-2 / V-3 / V-4（前瞻判据），此处落为 change 级 AC。机器可判段 + 人工登记段分列。

| # | 判据 | 形态 |
|:--:|---|:--:|
| **AC-1** | `ArchForge:docs/microarchitecture/pcie-endpoint.md` 存在，表头含「键 / 消费点 / 语义 / 归属 / 新旧迁移」五列 | 机器可判 |
| **AC-2** | 该契约内容经 ArchForge 侧评审 —— 报告登记**评审人 + 日期**（缺登记视为未完成） | 人工登记 |
| **AC-3** | 19 号文档 §9.3 出现**旧类 / 新类两列**的 JSON 键面分列 | 机器可判 |
| **AC-4** | `ADR-SOC-11` 状态推进的结论与理由已登记（评审人 + 日期），且其「17 端口 / SR-IOV / 23 ABI 边界」结论未被推翻 | 人工登记 |
| **AC-5** | 对账闸存在并接入 CTest，**6 条决定性变异**全部正确：① 配置加契约外键 ⇒ 红;② 契约删键而配置仍含 ⇒ 红（方向①）;③ 契约删键而配置已删、登记仍在 ⇒ 黄（方向③）;④ 契约加键而本仓未用未登记 ⇒ 黄（方向②）;⑤ 契约缺失 / hash 不一致 ⇒ 黄;⑥ ①③ 共燃 ⇒ 红 | 机器可判 |
| **AC-6** | 闸的三集合取数与 `ADR-DGPU-15` §2.6 **逐字一致**（同一套取数，无双口径） | 机器可判（脚本内唯一常量/函数） |
| **AC-7** | 回归：实现面**零改动** —— `cpptlm_tests` 全绿、CEP 冻结面 89 产物 hash 一致、`include/tlm/pcie/**` 零 diff（`git diff <base> -- include/tlm/pcie/` 为空） | 机器可判 |
| **AC-8** | 台账 §1.1 / 表 B 的「待修」条目与契约「归属=硬件建模且未实现」列**一一对应**（差集 ∅） | 机器可判（脚本对拍） |

**前置阻塞（须先解）**：A-1 ~ A-3 在 **ArchForge 仓**，该仓**无 CI** ⇒ AC-2 / AC-4 只能人工登记;若 ArchForge 侧迟迟不落地，本改善在 CppTLM 侧的 B-1 ~ B-5 只能先以「基准缺失 ⇒ 黄 + 告警」形态在位（`ADR-DGPU-15` D6 ⚠️ 已裁决**不判红**）。

---

## Capabilities

| 能力 | 归属 | 状态 |
|---|---|---|
| `pcie-endpoint-key-surface`（EP 键面契约：键 / 消费点 / 语义 / 归属 / 新旧迁移） | **ArchForge**（规格对象） | 待建（A-1） |
| `ep-key-surface-reconciliation-gate`（三集合 + 四方向 + 红覆盖黄 + hash 登记 的跨仓对账闸） | **CppTLM**（`scripts/test/` + CTest） | 待建（B-1 ~ B-4） |
| 既有：`module-param-load-declares` / `param-derivation` / `param-validation` | CppTLM（main specs） | ✅ 已在契约内（C2 归档合并） |

⚠️ 本改善**不新增**参数声明面语义（那是 `ADR-X.17` 与 C2 的既有契约）;它只建立「键面**可对拍**」这一层。

---

## Impact

| 面 | 影响 |
|---|---|
| **跨仓** | A-1 ~ A-3 必须在 ArchForge 仓做（CppTLM 无法代写）;B-1 ~ B-5 在本仓;两仓须协同提交，**单仓回滚不影响另一仓**。 |
| **OpenSpec KPI** | 立项时占 1 个 Proposed slot（当前 baseline **0/3**，空间充足）。 |
| **构建/测试** | 新增 1 个 shell 闸 + 1 条 CTest;不改任何 C++ 源码 ⇒ 不触发重编译。 |
| **文档** | `ArchForge:docs/microarchitecture/` 补齐 EP（与其它 54 篇齐平）;19 号文档 §9.3 由「旧类一列」改为两列。 |
| **风险** | ① **语义盲区**：闸只比键名集合，语义漂移仍需人工评审（契约 hash 登记使其**可见**）;② **跨仓不可达**：契约被移动/删除 ⇒ 闸判黄而非红（避免拖垮本仓 CI）;③ **契约形态未定**：顶层平铺 vs 子命名空间（如 `l2_modeling.*`）属**契约侧裁定**（`ADR-DGPU-15` D3.4 / 方案 E）⇒ 若选子命名空间，配置面会有一次破坏性迁移，须另立 change。 |
| **依赖** | `ADR-DGPU-15`（Proposed，判据真源）;ArchForge `ADR-SOC-11`（状态待推进）;C2（已归档，键面现状的基线）。 |

---

## 后续步骤（planner 阶段）

1. **排期后立项**：`openspec/changes/<name>/proposal.md`（AC 取本文件 Acceptance 的 AC-1 ~ AC-8）+ `design.md`（闸的取数与变异设计）+ `tasks.md`。
2. **跨仓协调单**：ArchForge 侧的 A-1 ~ A-3 需单独在其仓提 PR（本仓 change 只引用，不代写）。
3. **与 `ADR-DGPU-15` 的状态联动**：`ADR-SOC-11` 推进完成后，回填 `ADR-DGPU-15` §6 的「状态」列与 `V-3` 勾选。
