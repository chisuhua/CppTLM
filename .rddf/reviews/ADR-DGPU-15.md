# 审稿报告 — ADR-DGPU-15（PcieEndpointIP 的 CppTLM / ArchForge 职责边界）

- **对象**：`docs/adr/ADR-DGPU-15-pcie-endpoint-ip-cpptlm-archforge-boundary.md`（新增；最终 212 行；**状态 = ✅ Proposed**）
- **日期**：2026-11-19
- **审查序**：oracle（2 轮）→ critic（4 轮，第 3/4 轮经用户明确批准追加）
- **预算**：oracle 1 轮 + 1 次复审 ✅ 用尽;critic 基础预算 2 轮 ✅ 用尽，第 3 / 4 轮经用户批准追加
- **✅ 最终状态**：critic 第 4 轮（确认性复审）= **APPROVE_WITH_CONDITIONS**，`mergeable: yes`、**无 BLOCKER** ⇒ 第 3 轮 2 项 BLOCKER 逐项闭合确认 ⇒ **ADR 转 `Proposed` 签发**（2026-11-19）。

---

## 判定轨迹

| 轮 | 审查者 | 判定 | 核心 finding |
|:--:|---|---|---|
| 1 | oracle | **BLOCK** | 5 项：① **D2 三层划分（L1/L2/L3）不可机械判定且自反例**（`config_size`/`bar_sizes` 归 L1 而同样决定 driver 可见 config space 的 `transaction_layer` 归 L2;`axi_adapter` 归 L1 的唯一理由是「当前测试消费它」= 以现状反推分类）⇒ 分类器本身失效;② **V-2/V-3 是伪判据**（跨仓内容正确性无法机械判）;③ **选项表漏第三方案**（L2 键移入显式子命名空间）;④ V-1「并集相等」措辞不严谨 + 嵌套键表示法未定义;⑤ 5 项关键事实未独立复核（ArchForge 路径 `/workspace/project/ArchForge` 不存在 ⇒ 实为 `/workspace/main/ArchForge`） |
| 2 | oracle | **PASS** | 5 项**逐项闭合**（①撤销语义分类器;C-1 消费面机械可判 + C-2 语义面外移规格侧;②V-2/V-3 拆机器可判段 + 人工登记段;③方案 E 列入并路由到 D3 契约裁定;④V-1 改 ⊆ + `block.key` 记法;⑤7 项事实全部独立复核）;2 条低危 NOTES（NOTES-1 `param_admission.hh` 注释计数「2 个」实际 3 个 —— **builder 域代码注释**;NOTES-2 D6 取数口径措辞不一） |
| 1 | critic（arch） | **REJECT** | 1 **BLOCKER**：§2.6 方向①（红）与 V-4 变异②（黄）对同一状态给出**互斥 verdict** ⇒ 实施方无法同时满足两条;1 SHOULD_FIX（Status 行承载审稿流水 + §2.2「v1 的教训」段 = 过程日志混入决策文档）;1 NIT（`reject_unsupported_param` 与 `ADR-X.17` D-3b 的并存口径未点明）;另记录 7 项事实抽验为真（含 `INV-CEP-07-IC5` 4 头集合与 `ADR-DGPU-08` 的 4 头集合**不是同一组**） |
| 2（复审） | critic（arch） | **REJECT** | 第 1 轮 BLOCKER **已闭合** ✅（+ 原 SHOULD_FIX / NIT 亦闭合 ✅）;但**本轮新增内容引入 1 项新 BLOCKER**：为修 NIT 而加的「与 `ADR-X.17` D-3b 的并存口径」小节 —— **① 伪引证**（`ADR-X.17` 全文**零命中**「态②/显式拒绝/reject」;该措辞出自 **C2 change design D-2**）;**② 与已签批 `ADR-X.17` D-3b 的处置口径冲突未裁决**（D-3b 明文：配置里有键、实现零效果的**已知缺陷** SHALL `ignore_param` 登记且**不得**使装配失败 ⇒ 「当前无消费方、将来可能有」的第三类在两份文档中均无归属，而 `resizable_bars` 恰属此类）;2 SHOULD_FIX（D6 缺「红覆盖黄」优先级规则;V-4 未覆盖方向②）;2 NITS（§6 残留「三方向」计数;「v2 裁定」是草稿版本痕迹） |

---

## 最终闭合状态（critic 第 2 轮 BLOCKER + SHOULD_FIX + NITS）

### B-1（critic 第 2 轮 BLOCKER）—— **已按其可套用措辞闭合**

| 要求 | 落地 |
|---|---|
| **改引真实权威** | §2.2 的四态表把「态② 显式拒绝」的出处改标为 **C2 change design D-2** + `include/core/sim_object.hh::reject_unsupported_param` + 台账表 B 状态列，并明写「`ADR-X.17` **全文无「态② / 显式拒绝」概念** ⇒ 本 ADR 不向其挂不存在的锚点」 |
| **裁决与 D-3b 的边界**（三态 → 四态） | §2.2 四态表：① 有消费方且在本模块 ⇒ `load_param`;② 有消费方但不在本模块 ⇒ `ignore_param`（登记 ≠ 豁免）;③ **当前**无消费方但**对应已定义的实现契约 / 规格语义**（将来可能有）⇒ `ignore_param` + **表 B 登记「待修」**（依 `ADR-X.17` D-3b 明文）;④ **永不可能**有消费方 ⇒ 态② 显式拒绝 ⇒ **唯一判据 = 一条二分**（「是否永不可能有消费方」） |
| **逐键落到本 ADR 的 3 个键** | `resizable_bars` ⇒ **③**（规格侧有该语义:19 号文档把 `resizable_bars_` 列为 EP 值成员 + 运行期 API `enable_resizable_bar()` 已存在;缺的只是 JSON 绑定）⇒ **C2 落地的态②拒绝与 D-3b 冲突 ⇒ 登记为待 builder 返工项**（§6）;`num_msix_vectors` / `msix_num_vectors`（顶层）⇒ **④**（旧类的键，规格上**不属于**新类 + PF resize 入口是冻结头里的 no-op）⇒ 态②拒绝成立 |

⚠️ **该闭合连带暴露一处实施缺口**（architect 域外，已登记）：C2 的 `resizable_bars` 处置需由**态②拒绝**改为 **`ignore_param` 登记 + 表 B「待修」**（连带：台账 B-21 状态 + 验收报告 F1 描述）⇒ 须在 C2 归档前由 builder 完成，并由 verifier 在复验轮确认。

### SHOULD_FIX-1 —— D6 补「红覆盖黄」优先级规则

§2.6 增补：方向①与方向③可对同一键共燃（契约删 K 而本仓配置仍含 K 且登记仍在）⇒ **方向①优先判红**（红 = 必修，黄 = 待办）;并注明方向②与③对同一键**互斥**（②要求 K ∈ `S_contract`，③要求 K ∉ `S_contract`）⇒ 不会同时命中。

### SHOULD_FIX-2 —— V-4 补方向②的变异

§5 V-4 由 4 条变异扩为 **6 条**：⑤ 契约侧**加**键 K 而本仓配置与登记均不含 ⇒ 方向② **黄**（待实现 / 待出栈）;⑥ 方向①与③共燃 ⇒ **红**（红覆盖黄）。

### NITS

| NIT | 落地 |
|---|---|
| §6 残留「三方向变异」计数 | 改为「**六条变异 / 四方向**」 |
| 「⇒ **v2 裁定**」是草稿版本痕迹 | 改为「⇒ **裁定**」 |

### oracle NOTES-1 —— **不属本 ADR 判据面**（builder 域）

`include/core/param_admission.hh` 的准入表注释写「2 个零消费键」，实为 **3** 个（含 `msix_num_vectors`）⇒ 归入 C2 归档前的 builder 尾项（与上面的 `resizable_bars` 返工一并处理）。

---

## 事实复核留档（critic 第 2 轮独立复核为真）

1. ArchForge `docs/architecture/19-pcie-ip-microarchitecture.md:33` —「`PcieEndpointIP` 是 dGPU SoC 片内 PCIe Endpoint IP 的**完整 CppTLM 模型**」;
2. 同文件 `:809` —「JSON params 子集(`config_size`, `msix_num_vectors`, `bar0_registers`, `capabilities`)」属**旧类** `PcieEndpointTLM` 冻结表;`:855-885` 为新类示例键面 ⇒ **P-1 成立**;
3. ArchForge `docs/microarchitecture/` **54** 篇 `.md`，含 `endpoint` 者 **0** 篇 ⇒ **P-3 成立**;
4. ArchForge `README.md:3-4` —「设计伴侣仓 / 源代码实现 → CppTLM」;
5. CppTLM `include/tlm/pcie/pcie_endpoint_ip.hh:156-158` — `sync_msix_cap_table_size` 为 no-op;该头在 `INV-CEP-07-IC5` 的 4 个 frozen headers 内;
6. 准入表 **20** == design D-2 期望集 **20**（逐项相等，差集空）;
7. `examples/dgpu_soc_with_pcie_ip.json` 的 EP 顶层 params = `axi_adapter` / `bypass_mode` / `link_layer` / `phy_digital` / `sr_iov` / `transaction_layer`。

---

## 第 3 轮（确认性复审）与第 4 轮闭合记录

| 轮 | 审查者 | 判定 | 结果 |
|:--:|---|---|---|
| 3 | critic（arch，确认性复审） | **REJECT** | 第 2 轮 BLOCKER 部分闭合，但暴露 2 项：**B-1 伪引证未完全闭合**（改挂到「C2 change design D-2」后仍不实 —— `design.md` 全文零命中「显式拒绝 / reject_unsupported」，且其「态②」语义**相反** = `ignore_param` 登记）+ **B-2 文档内互斥**（§6 返工项要求改登记，而 V-6 要求「3 键的拒绝不被回滚」） |
| 4 | critic（arch，确认性复审，用户批准） | **APPROVE_WITH_CONDITIONS**（`mergeable: yes`） | 第 3 轮 2 项 BLOCKER **逐项闭合**（`sim_object.hh:498` / C2 `proposal.md:49` / `tasks.md:23,103` / 台账 B-22 全部实测命中;V-6 ↔ 实现 ↔ 台账 ↔ 判据 case 四者一致）;**无新 BLOCKER**;2 SHOULD_FIX + 2 NITS |

### 第 4 轮 SHOULD_FIX / NITS 的处置

| # | 内容 | 处置 |
|:--:|---|---|
| SF-1 | 台账 `B-21` 行仍写「已显式拒绝」，与 `:46` 摘要行、实现（`ignore_param`）、V-6② 互斥 | ✅ **已改**为「已登记 + 待修（态③）」，写明曾误落拒绝及改正经过 |
| SF-2 | ADR 态④ 行出处混挂 B-21（其内容正是被推翻的处置） | ✅ **已改**为只挂 **B-22**;态③ 行补挂 **B-21** |
| NIT-1 | §2.2 末「⇒ 态②拒绝成立」在术语消歧段之后又用旧号 | ✅ **已改**为「⇒ **态④** 拒绝成立」;另清掉 §2.2/§4.1 其余 2 处旧号（「3 键态②拒绝」一并按现状改为「2 键拒绝 + 1 键登记」） |
| NIT-2 | `include/core/sim_object.hh:491,497` 注释也用「态②」指拒绝 | 📌 **不本次修**（注释级、无行为影响;该头触达约 330 TU，为注释触发全量重编译不成比例）⇒ 已在 ADR 术语消歧段登记「后续触碰该头时一并改」，并记入 C2 归档后的 backlog 说明 |

---

## 未闭合 / 移交

| 项 | 归属 | 状态 |
|---|---|---|
| critic 第 3 / 4 轮（确认性复审） | critic | ✅ **已完成**（用户批准追加）⇒ 第 4 轮 APPROVE_WITH_CONDITIONS 无 BLOCKER，ADR 转 `Proposed` |
| `resizable_bars` 处置返工（reject → `ignore_param` + 表 B「待修」） | **builder**（C2 尾项） | ✅ **已完成**（实现 + 判据 case + 台账 B-21） |
| `param_admission.hh` 注释计数与三键处置描述 | **builder**（C2 尾项） | ✅ **已完成** |
| 验收报告 `.rddf/reviews/cpptlm-module-param-migrate-batch2-verify.md` 的 F1 描述（「拒绝」→「登记」） | **verifier**（复验轮） | **待更新**（report 归属 verifier，builder 不改） |
| `sim_object.hh` 注释的「态②」措辞 | builder（后续触碰该头时） | 📌 **已登记，本次不做** |
| ArchForge 键面契约（`ArchForge:docs/microarchitecture/pcie-endpoint.md`）+ 19 号文档 §9.3 新旧分列 + `ADR-SOC-11` 状态推进 + 跨仓对账闸 | **跨仓**（backlog `cpptlm-ep-key-surface-reconciliation`） | **待 planner 立项**（本 ADR 不创建 OpenSpec change） |
