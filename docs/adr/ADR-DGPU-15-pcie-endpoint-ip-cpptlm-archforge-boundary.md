# ADR-DGPU-15: PcieEndpointIP 的 CppTLM / ArchForge 职责边界 —— 键面契约升格 + 已消费/零消费二分

> **状态**: ✅ **Proposed**（2026-11-19 签发；oracle 2 轮 + critic 4 轮，末轮 APPROVE_WITH_CONDITIONS 无 BLOCKER）
> **日期**: 2026-11-19
> **作者**: architect（build agent 代为落盘）
> **Parent**: [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md)（23 ABI 冻结，**沿用不变**）· [ADR-DGPU-07](ADR-DGPU-07-minimal-soc-evolution-seam.md)（D3 演进 seam，沿用）· ArchForge `docs/adr/ADR-SOC-07-dgpu-board-soc-layering.md`（board/SOC 两层分离）· ArchForge `docs/adr/ADR-SOC-11-pcie-endpoint-ip.md`（`PcieEndpointIP` 决策，**状态仍为 Proposed**）
> **关联 change**: [cpptlm-module-param-migrate-batch2](../../openspec/changes/archive/2026-11-19-cpptlm-module-param-migrate-batch2/)（本 ADR 的**直接影响面** = 其 AC-01 准入表全量化 / AC-08 板级直读键三态）
> **backlog**: `cpptlm-ep-key-surface-reconciliation`（ArchForge 侧 EP 键面契约 + 跨仓对账闸，**待 planner 立项**）
> **AC 映射**: V-1 ~ V-4 对应 C2 的 AC-01 / AC-08 / AC-10;V-5 / V-6 属 backlog change（AC 编号待 planner 补）
> **审稿报告**: [../../.rddf/reviews/ADR-DGPU-15.md](../../.rddf/reviews/ADR-DGPU-15.md)

---

## 1. 背景与问题

### 1.1 触发事件

change `cpptlm-module-param-migrate-batch2`（下称 **C2**）把 `PcieEndpointIP` 纳入「声明即读取」的参数声明面时，verifier 独立验收（`.rddf/reviews/cpptlm-module-param-migrate-batch2-verify.md`，2026-11-19）发现该类型的 **3 个 JSON 顶层键无任何消费面**：

| 键 | 实测事实 |
|---|---|
| `resizable_bars` | `attach_composition` **不读**（`warn_unconsumed` 报 `no consumer`）;ArchForge 19 号文档只把 `resizable_bars_` 列为**值成员**，未定义 JSON 键 |
| `num_msix_vectors` | 旧 EP 模型遗留名;本类的 MSI-X vector 数由**嵌套键**（`sr_iov.vf_msix_vectors` / `transaction_layer.msix_num_vectors`）承载;PF 表 resize 入口 `sync_msix_cap_table_size(uint16_t)` 是**冻结头里的 no-op**（`include/tlm/pcie/pcie_endpoint_ip.hh:156-158`「no-op (A-2b option 2)」，该头属 `INV-CEP-07-IC5` 的 4 个 frozen headers）⇒ **本类不可能消费** |
| `msix_num_vectors` | 同上（顶层误用） |

⇒ 处置（态② 显式拒绝 + 配置与测试副本清理）已由 C2 落地并通过复验。**但这只是症状处置**。

### 1.2 症状背后的结构性问题（事实 + 证据位置）

| # | 问题 | 证据 |
|:--:|---|---|
| **P-1** | **新旧两类的 JSON 键面在规格侧未分列** —— ArchForge 19 号文档 §9.3 的「JSON params 子集(`config_size`, `msix_num_vectors`, `bar0_registers`, `capabilities`)」属**旧类** `PcieEndpointTLM` 的冻结表（`19-pcie-ip-microarchitecture.md:809`）;新类的键面只在示例中零散出现（同文件 `:855-885`）⇒ **两类的键名集合没有一处可对拍** | ArchForge `docs/architecture/19-pcie-ip-microarchitecture.md` |
| **P-2** | **旧键名在迁移中未收敛** —— 旧类的合法顶层键 `num_msix_vectors` 被**原样带进**新类的配置（4 份真实配置 + 12 个测试内联副本）⇒ 配置迁移时**未按规格的迁移路径收敛** | 2026-11-19 实测 + 19 号文档「附录 A: 与现有 `PcieEndpointTLM` 的迁移路径」 |
| **P-3** | **键面无规格对象** —— ArchForge `docs/microarchitecture/` 有 **54** 篇 IP 微架构契约（含 `pcie-root-complex.md`），**EP 命中数 = 0** ⇒ 「每个键由谁消费、什么语义、归哪一层」在规格侧**无处可查** | ArchForge `docs/microarchitecture/` 清单实测 |
| **P-4** | **配置面与实现面无对账机制** —— 键名漂移（键存在于配置、无人消费）只能靠**逐键人工审计**发现;C2 本次是靠「类型准入」这一动作**顺带**暴露的 ⇒ 无机制则必然复发 | C2 的 F1 验收 finding 即为实例 |

### 1.3 用户提出的方向（须先澄清再裁定）

用户提问：「是不是重新审视 `PcieEndpointIP` 设计，**只保留用于框架验证目标**的内容，而真正 `PcieEndpointIP` 的设计可以移入 ArchForge 里」。

**该方向的一半已经是现状**（§3.1），另一半会与既有交付冲突（§3.2）⇒ 不能照字面执行，须先厘清「设计 / 规格 / 实现 / 配置」四者各归谁。

---

## 2. 决策

### 2.1 D1 —— 既有跨仓关系**不变**：规格在 ArchForge，实现在 CppTLM

ArchForge README 已确立关系：「本仓是 CppTLM 的**设计伴侣仓**，存有全部 dGPU SoC 架构设计文档」+「**源代码实现** → CppTLM」;且 19 号文档 §1（`:33`）明确：「`PcieEndpointIP` 是 dGPU SoC 片内 PCIe Endpoint IP 的**完整 CppTLM 模型**，目标行为接近真实硬件数字逻辑」。

⇒ **裁定**：跨仓关系**不变**（不新建代码仓、不迁移实现、不把实现级设计文档搬离 CppTLM）。「移入 ArchForge」在本 ADR 中的**唯一有效含义** = **把「键面契约」升格为 ArchForge 的规格对象**（D3）。

### 2.2 D2 —— 键面**二分**（可机械判定）+ 语义分层**外移到规格侧**

⚠️ **为什么不做「语义分层」**（决策理由，非审稿记录）：把键按「框架验证面 / 硬件建模面 / 已出栈」三层分类**不可机械判定** —— 同一键可同时具备两重属性（`config_size` / `bar_sizes` 既是硬件属性，又决定 23 ABI driver 可见的 config space 与 BAR 语义，而同样决定 driver 可见 config space 的 `transaction_layer` 属另一层）⇒ 分类依据只能是「当前谁在消费」= **以现状反推分类** ⇒ 分类器会随新键面腐化成「归属历史快照」。⇒ 故本 ADR **不在 CppTLM 侧保留任何语义分类器**：语义判定归规格侧（见 C-2）。

⇒ **裁定**（两层职责分离）：

| 面 | 内容 | 判定者 | 机械可判性 |
|---|---|---|---|
| **C-1 消费面**（CppTLM 侧） | 每个 JSON 顶层键**必须**二选一：① 有消费点 ⇒ 登记（`load_param` 或 `ignore_param`）;② 无消费点 ⇒ **显式拒绝**（`reject_unsupported_param`，写了即装配失败并报出键名） | CppTLM | ✅ **可机械判定**（消费点存在与否 + 登记/拒绝名单与配置键集合的 ⊆ 关系） |
| **C-2 语义面**（规格侧） | 每个键的**语义 / 消费点 / 规格出处 / 归属**（框架验证面 or 硬件建模面 or 已出栈） | **ArchForge**（键面契约） | ⚠️ 内容正确性**人工评审**（跨仓无 CI）⇒ 由 D6 的 hash 登记 + 评审登记兜住 |

⇒ **CppTLM 不再维护任何「语义分层」分类器**;「归哪一层」是 **ArchForge 契约的一列**，由**语义所在的地方**维护 ⇒ 消除自反例与分类器腐化。

⚠️ **「登记」与「拒绝」的四态判据**（两种机制不可混用;判据引用的真实出处见括号）：

| 态 | 键的形态 | 处置 | 出处 |
|:--:|---|---|---|
| ① | 有消费方，且消费方**在本模块** | `load_param` 声明 | `ADR-X.17` D-1 |
| ② | 有消费方，但消费方**不在本模块**（板级直读 / 非标量解析） | `ignore_param` 登记（登记后**不**失败;登记 ≠ 豁免） | `ADR-X.17` **D-3 板级路径条款**（`:134`）+ **D-2 消费方扩展**（`:112-114`）/ **F-15**（`:50`）;C2 design D-6 |
| ③ | **当前**无消费方，但**对应已定义的实现契约 / 规格语义**（将来可能有消费方） | `ignore_param` 登记 + **基线台账表 B 登记「待修」**（写了**不**失败） | `ADR-X.17` **D-3b**（明文：「配置里有键、实现零效果」的**已知缺陷** SHALL 以 `ignore_param` 显式登记并落表 B）+ **台账表 B 的 B-21 行** |
| ④ | **永不可能**有消费方（规格上**不属于**本类型 / 冻结面使实现不可能） | **显式拒绝**（`reject_unsupported_param` ⇒ 写了即装配失败并报出键名）—— 本 ADR 称「**态④**」 | `include/core/sim_object.hh` 的 `reject_unsupported_param`（实现）+ **C2 `proposal.md` AC-06 例外段 2** + **C2 `tasks.md` T1.7 与 F1** + **台账表 B 的 B-22 行**（`docs/implementation/module-param-baseline.md`） |

⇒ **唯一判据 = 一条二分**：「该键是否**永不可能**有消费方」⇒ 是 ⇒ ④;否 ⇒ ②③（登记 + 落表 B）。

⚠️ **术语消歧（同名不同义，实施方勿混用）**：**C2 `design.md` D-2 的「态②」= 「已消费但不经 `load_param`」⇒ `ignore_param` 登记**（`design.md:30,31,35,45,47,49`）;而**台账表 B 的「态②」= 「显式拒绝」**。两者**同号不同义** ⇒ 本 ADR 一律用「**态④**」指「显式拒绝」，用「态②/③」指「登记 + 落表 B」两态。
⚠️ `ADR-X.17` **全文无「态② / 显式拒绝」概念**（该措辞出自 C2 `proposal.md` / `tasks.md` 与台账表 B）⇒ 本 ADR 不向 `ADR-X.17` 挂不存在的锚点。
⚠️ `include/core/sim_object.hh:491,497` 的**注释**同样用「态②」指拒绝（注释级、无行为影响）⇒ 与台账同款同号不同义;后续触碰该头时一并改为「显式拒绝」，本 ADR 不为注释单独触发全量重编译（该类头触达约 330 个编译单元）。
⚠️ **本裁定改变了 C2 的一处实施**：`resizable_bars` 属 **③**（规格侧有该语义：19 号文档把 `resizable_bars_` 列为 EP 值成员，且运行期 API `enable_resizable_bar()` 已存在;缺的只是 JSON 绑定）⇒ 处置应为 **`ignore_param` 登记 + 表 B「待修」**，⛔ **不是** C2 当初落地的显式拒绝（当时误称「态②」）⇒ 已返工（见 §6）;配置侧已删除该块**可保留**（键可被登记而配置不写它）。
⚠️ 反之，`num_msix_vectors` / `msix_num_vectors`（顶层）属 **④**：它们是**旧类**的合法键，规格上**不属于新类**（新类的 MSI-X vector 数由 `sr_iov.vf_msix_vectors` / `transaction_layer.msix_num_vectors` 承载，且 PF 表 resize 入口 `sync_msix_cap_table_size` 是冻结头里的 no-op）⇒ **态④** 拒绝成立。

### 2.3 D3 —— 「键面契约」升格为 ArchForge 的规格对象（**P-1/P-2/P-3 的治本动作**）

1. ArchForge 新增 `ArchForge:docs/microarchitecture/pcie-endpoint.md`：**EP 的键面契约** —— 每个顶层键 → 消费点（子组件 / 成员）→ 语义 → 归属（框架验证 / 硬件建模 / 已出栈）→ **新旧两类迁移关系**（旧键 → 新键，或「出栈」）。
2. ArchForge 19 号文档 §9.3：**旧类**（`PcieEndpointTLM`）与**新类**（`PcieEndpointIP`）的 JSON 键面**分列**（当前只有旧类一列 ⇒ P-1）。
3. `ADR-SOC-11`（Proposed）按 D2 的二分复核后推进状态；其「17 端口 / SR-IOV / 23 ABI 边界」的结论**不变**。
4. ⚠️ 契约的**键面形态**（顶层平铺 vs `experimental.*` 子命名空间）由 ArchForge 侧在契约中裁定（见 §3 方案 E）。

### 2.4 D4 —— 实现面**不收窄**

收窄实现面（移除/降级 PCIe 硬件建模块对应的 TLM 子模块）会同时触及：Phase 1-8 已交付能力（7 个已归档 change + Oracle 放行）、23 ABI 冻结（`ADR-DGPU-08` §D5）、CEP 冻结面（`INV-CEP-07-IC5` 的 4 个 header 零 diff）、现有配置（`examples/dgpu_soc_with_pcie_ip.json` 实测使用 `axi_adapter` / `bypass_mode` / `link_layer` / `phy_digital` / `sr_iov` / `transaction_layer`）与大批测试 ⇒ **不是本 ADR 能承载的动作**。

⇒ 裁定：**不收窄**。若将来确需收窄，**必须**独立立项（独立 ADR + 独立 change + 跨仓协同 + 完整回归）。

### 2.5 D5 —— C2 的影响：`PcieEndpointIP` **留在** C2 的参数声明面期望集内

因 D4（实现不收窄）⇒ 该类型在 CppTLM 侧的键**均有消费面**（或已显式拒绝）⇒ 键面全覆盖 ⇒ **准入成立**（C2 已落地的路径 A 维持，**不回退**）。

⚠️ 若将来 D4 的收窄 change 落地 ⇒ 该类型退出 CppTLM 的参数声明面 ⇒ 届时按「显式排除」处置（改 design D-2 #7 + AC-01 期望集 + 台账）。
⚠️ **可回溯条款**：那份未来 ADR **必须包含与本 ADR 的键级 diff**（逐键：继续保留 / 出栈 / 语义变更）⇒ 否则本 ADR 的契约引用会成为 stale 真源，而对账闸（D6）只会把漂移吸收进台账理由。

### 2.6 D6 —— 对账闸（**P-4 的治本动作**）：键名集合级漂移必红

以 ArchForge `ArchForge:docs/microarchitecture/pcie-endpoint.md`（**规格侧唯一真源**，路径**写死**）为基准。

**三个集合（唯一取数口径）**：

| 记号 | 含义 | 取数来源 |
|---|---|---|
| `S_cfg` | CppTLM 侧**被使用**的键 | 配置（`configs/**/*.json` + `examples/*.json`）+ 测试内联副本（`test/**/*.cc`）中 `PcieEndpointIP` 条目的**顶层键**（一级嵌套按 `block.key` 记，不展开更深层） |
| `S_contract` | 规格侧定义的键 | ArchForge 键面契约的顶层键集合（同一记法） |
| `S_impl` | CppTLM 侧**已登记**的键 | 「消费点登记（`load_param` / `ignore_param`）∪ 显式拒绝名单（`reject_unsupported_param`）」的键集合（同一记法） |

**四个方向（每种状态唯一 verdict）**：

| 方向 | 条件 | 判定 | 含义 |
|:--:|---|:--:|---|
| ① | `S_cfg \ S_contract` ≠ ∅ | 🔴 **红** | 本仓配了契约没有的键 ⇒ **配置面漂移**（P-1/P-2 的形态） |
| ② | `S_contract \ (S_cfg ∪ S_impl)` ≠ ∅ | 🟡 **黄 + 告警** | 契约有键，本仓既未使用也未登记 ⇒ 待实现 / 待出栈（须台账登记理由） |
| ③ | `S_impl \ S_contract` ≠ ∅ | 🟡 **黄 + 告警** | 本仓登记了契约没有的键 ⇒ 登记残留 / 出栈未清理 |
| ④ | ①②③ 皆空 | 🟢 绿 | 一致 |

⚠️ **优先级规则（红覆盖黄）**：方向①与方向③可对**同一键**同时命中（契约删键 K 而本仓配置仍含 K 且登记仍在）⇒ **方向①优先判红**（红 = 必修，黄 = 待办/待登记）⇒ 复合态的 verdict = **红**。
⚠️ 方向②与方向③对同一键**互斥**（②要求 K ∈ `S_contract`，③要求 K ∉ `S_contract`）⇒ 不会同时命中。

⚠️ **边界**（显式声明）：本闸只比对**键名集合**，**不比对语义**（ArchForge 无代码仓 ⇒ 无 CI 可挂语义检查）⇒ **语义漂移仍需人工评审** ⇒ 闸绿 **不等于**语义未漂移。
⚠️ **契约基准缺失 / 被移动 / hash 不一致** ⇒ 判 🟡 黄 + 告警（**不判红** —— 跨仓不可达不应拖垮本仓 CI），并在告警里提示「契约路径不可达」。
⚠️ **契约文件 hash 登记**：CppTLM 侧台账记录当前基准文件的 hash ⇒ 契约被**单方面**修改时（键集合未变、语义已变）从静默变为**可见**（hash 不一致 ⇒ 黄）。

---

## 3. 选项对比表

| # | 方案 | 结论 | 理由 |
|:--:|---|:--:|---|
| **A** | **键面二分（已消费 / 零消费，机械可判）+ 语义分层外移到 ArchForge 契约 + 实现不收窄 + 对账闸**（= 本 ADR 的 D1~D6） | ✅ **采纳** | ① 直接治本（P-1~P-4）;② CppTLM 侧只保留**可机械判定**的部分（oracle BLOCK 要点①的正面回应）;③ 不动已交付能力（D4）;④ 与 ArchForge README 的既有跨仓关系一致（D1）;⑤ C2 的 F1 处置**无需回滚**（D5） |
| **B** | **实现面收窄**（CppTLM 只留框架验证目标的内容） | ❌ **否决** | ① 与 ArchForge 19 号文档 §1「`PcieEndpointIP` = 完整 CppTLM 模型」的规格**直接冲突**（须先改规格）;② 破坏 Phase 1-8 已交付且 Oracle 放行的能力 + 23 ABI 冻结 + CEP 冻结面（4 header 零 diff）;③ ArchForge **无代码仓** ⇒ 实现无处可去;④ 收益与成本严重不匹配（配置面漂移可由 A 治） |
| **C** | **配置面出栈**（把硬件建模键整体移出 CppTLM 配置面） | ❌ **否决** | ① 与已交付配置（`examples/dgpu_soc_with_pcie_ip.json` 实测用 6 个硬件建模键）与大批测试**冲突**;② 「规格在 ArchForge」**不蕴含**「CppTLM 不能配置这些键」—— 规格约束语义，不取消可配性;③ 与 B 同性质（破坏交付） |
| **D** | **只修 C2 的 AC-01 口径**（维持现状，仅把 `PcieEndpointIP` 显式排除） | ❌ **否决** | ① 不治本（P-1~P-4 全留）⇒ 键名漂移必然复发;② 放弃「准入」带来的未知键检出能力;③ 把**架构问题**降级为**账目问题** |
| **E** | **硬件建模键移入显式子命名空间**（如 `experimental.*` / `l2_modeling.*`），使 L1/L2 边界在 JSON 结构上**机械可分** | ⏸ **暂缓（交 D3 契约裁定）** | ① 优点真实：保留可配性（不像 C）、且让「归哪一层」由**JSON 结构**表达 ⇒ 比 v1 的语义分类器更硬;② 但它是**配置面破坏性**改动（4 份配置 + 大量测试的键路径迁移），且**键面形态本就属于规格侧裁定范围**（D3.4）⇒ 在 ArchForge 契约落地前**无法**在 CppTLM 单方面执行 ⇒ **不否决其价值，而是把形态裁定权交给 D3**;③ 若 D3 的契约选定子命名空间形态，本 ADR 的 D2/C-2 随之修订（契约形态 = 规格侧决定，CppTLM 侧只跟随） |

---

## 4. 后果 / 代价

### 4.1 正面

- **键名集合级**配置面漂移有**机制**检出（P-1/P-2 的复发被挡），语义漂移由**契约评审 + hash 登记**兜住（D3/D6）⇒ 不再完全依赖逐键人工审计（⚠️ 仅键名集合级，见 §4.2）。
- CppTLM 侧**不维护语义分类器**（D2 的 v2 修正）⇒ 无自反例、无分类器腐化。
- C2 的 F1 处置**保留**（D5）⇒ 已落地的处置（`num_msix_vectors` / `msix_num_vectors` = 态④ 显式拒绝;`resizable_bars` = 态③ 登记 + 表 B「待修」）+ 配置清理**不作废**。
- ArchForge 侧补上 EP 微架构契约（P-3）⇒ 与其它 54 篇 IP 微架构**齐平**。
- 「哪些键能改、谁说了算」**可判定**：消费面由 CppTLM 判（C-1），语义由 ArchForge 判（C-2）⇒ 减少「两仓各自改一半」。

### 4.2 负面 / 代价

- **跨仓工作量**：ArchForge 侧需 1 份 `pcie-endpoint.md` + 19 号文档 §9.3 分列 + `ADR-SOC-11` 状态推进 ⇒ **不在本仓**，本 ADR 只能**登记**（backlog）⇒ 落地依赖跨仓协同。
- **对账闸的语义盲区**：只比对键名集合 ⇒ 语义漂移仍需人工评审（D6 ⚠️）⇒ **闸绿 ≠ 语义未漂移**。
- **契约基准缺失时闸判黄** ⇒ 跨仓不可达期间，本仓 CI 无法给出「配置面已对账」的强保证。
- 本 ADR **不减少** CppTLM 的实现面 ⇒ 「CppTLM 太大」的诉求**未被本 ADR 解决**（那是 B/D4 的事，属独立 change）。

### 4.3 风险

| 风险 | 缓解 |
|---|---|
| ArchForge 侧迟迟不落地键面契约 ⇒ 对账闸无基准 | 闸的「基准缺失」分支判**黄 + 告警**（不判红）⇒ 可见但可先行;契约落地后自动转绿 |
| 契约被**单方面**修改（键集合未变、语义已变） | 契约文件 **hash 登记**（D6）⇒ hash 不一致即黄 |
| 未来收窄实现（D4）时与本 ADR 冲突 | D4 明写「独立 ADR + 独立 change」;**D5 的可回溯条款**要求那份 ADR 含**键级 diff** |
| 「键面契约」变成又一个**无人消费的文档** | 契约是 D6 闸的**唯一基准** ⇒ 契约缺失 = 闸判黄 + 告警 ⇒ 契约有**机制性**存在理由 |

---

## 5. Verification（可勾选验收判据 · 前瞻）

> 判据来源：C2 的 AC-01 / AC-08 / AC-10 + 本 ADR 的 D2 / D3 / D6。
> ⚠️ 每条标注**机器可判段**与**人工登记段**（oracle 要点②）—— 人工段必须登记**评审人与日期**，否则视为未完成。

- [ ] **V-1**（D2/C-1 · **机器可判**）**顶层键集合**满足：`{配置与测试内联副本中出现的顶层键}` ⊆ `{实现消费点登记 ∪ 显式拒绝名单}`。
      **嵌套键表示法**：一级嵌套块的键按 `block.key` 记（如 `transaction_layer.msix_num_vectors`），不展开更深层。
      **比较对象**：CppTLM 侧 = `configs/**/*.json` + `examples/*.json` + `test/**/*.cc` 内联副本中 `PcieEndpointIP` 条目的**顶层键**集合。
- [ ] **V-2**（D3 · **机器可判段**）ArchForge `ArchForge:docs/microarchitecture/pcie-endpoint.md` 存在，且含键面契约表（表头含「键 / 消费点 / 语义 / 归属 / 新旧迁移」五列）。
      **人工登记段**：契约表内容经 ArchForge 侧评审 —— 登记**评审人 + 日期**。
- [ ] **V-3**（D3 · **机器可判段**）19 号文档 §9.3 出现**旧类 / 新类两列**的 JSON 键面分列。
      **人工登记段**：`ADR-SOC-11` 状态推进的结论与理由（登记**评审人 + 日期**）。
- [ ] **V-4**（D6 · **机器可判**）跨仓对账闸存在并接入 CTest，且**每个变异状态在 D6 的四方向表中有唯一 verdict**：
      ① 往 CppTLM **配置**加一个契约外的顶层键 ⇒ `S_cfg \ S_contract` ≠ ∅ ⇒ **红**（方向①）;
      ② 契约侧删键 K 而 CppTLM **配置仍含** K ⇒ 同方向① ⇒ **红**（**不是黄** —— 这正是 P-2「旧键名残留」的形态）;
      ③ 契约侧删键 K 而 CppTLM **配置已删、登记仍在** ⇒ `S_impl \ S_contract` ≠ ∅ ⇒ **黄**（方向③）;
      ④ 契约文件缺失 / 被移动 / hash 不一致 ⇒ **黄**（跨仓不可达不判红）;
      ⑤ 契约侧**加**键 K 而本仓配置与登记均不含 K ⇒ `S_contract \ (S_cfg ∪ S_impl)` ≠ ∅ ⇒ **黄**（方向②: 待实现 / 待出栈）;
      ⑥ 方向①与方向③对同一键共燃 ⇒ **红**（红覆盖黄，优先规则见 D6）。
- [ ] **V-5**（D5 · **机器可判**）C2 的 AC-01 判据：准入表条目集合 == design D-2 期望集（**当前 20 == 20**，由 `[batch2][admission]` 承载）。
- [ ] **V-6**（D2/C-1 · **机器可判**）键面处置**与四态表一致**：
      ① 顶层 `num_msix_vectors` / `msix_num_vectors` = **态④ 显式拒绝**，4 份配置与 12 个测试内联副本的清理**不被回滚**;
      ② `resizable_bars` = **态③ 登记**（`ignore_param` + 台账表 B「待修」）⇒ `[batch2][pcie-ep]` 中该键的 case 由「拒绝」**改写为「登记 + 不失败」**;
      ③ 全部由 `cpptlm_tests` 与 `ctest` 全绿承载。
- [ ] **V-7**（D5 · **机器可判**）若 D4 的收窄 ADR 将来落地，其**键级 diff**与本 ADR 的键面契约一致（逐键：保留 / 出栈 / 语义变更）—— **未发生收窄时本条不适用**（记 N/A）。

---

## 6. 关联与后续

| 后续动作 | 归属 | 状态 |
|---|---|---|
| ArchForge `ArchForge:docs/microarchitecture/pcie-endpoint.md`（键面契约） | **ArchForge**（跨仓） | backlog `cpptlm-ep-key-surface-reconciliation`（**待 planner 立项**） |
| 19 号文档 §9.3 新旧键面分列 + `ADR-SOC-11` 状态推进 | **ArchForge**（跨仓） | 同上 |
| 跨仓键面对账闸 + CTest 注册（含六条变异 / 四方向） | **CppTLM** | 同上（本 ADR 的 D6 预留了判据形态） |
| 键面契约的**形态裁定**（顶层平铺 vs 子命名空间） | **ArchForge**（D3.4 / 方案 E） | 同上 |
| C2 的 AC-01 / AC-08 判据与台账（B-21 ~ B-23） | CppTLM | **已落地**（Entry #496 / #489） |
| ⚠️ **`resizable_bars` 的处置返工**：由显式拒绝改为 `ignore_param` 登记 + 表 B「待修」（本 ADR §2.2 态③ 裁定）;连带更新台账 B-21 状态与验收报告的 F1 描述 | **CppTLM**（builder 域） | **已返工**（实现 + 判据 case + 台账 B-21）;验收报告的 F1 描述待 verifier 复验轮更新 |

⚠️ 本 ADR **不创建** OpenSpec change（per architect 域边界）⇒ 上述 backlog 的立项与 AC 编号由 planner 阶段补。
