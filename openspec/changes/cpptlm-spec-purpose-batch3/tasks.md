# tasks: cpptlm-spec-purpose-batch3

> **AC 映射**: proposal AC-01 ~ AC-10 · design D-0 ~ D-5
> **上游**: improvement `cpptlm-spec-purpose-batch3.md` · 台账 **Entry #444**
> **复用**: 批 1 的 D-0 写作规则 + 校验脚本范式 + 归因载体范式 + critic 分组签 + verifier 独立实现

---

## Phase 0 · 立项态（planner）

- [x] **T0.1** 实测 R-1 剩余构成：`--strict` 红 **65** 份 = **59** 份 Purpose 占位 + **69** 条「正文 >500」issue（落 **26** 份）；`cep-*` 43/43 --strict 绿；非 strict 口径 129/129 绿
- [x] **T0.2** 按**能力域**聚类剩余 59 份 ⇒ **4 组**（D1 PCIe 物理链路 5 / D2 框架端口配置 20 / **D3 dGPU 板级 SoC 17** / D4 SM-GPU-命令驱动 17）；**批 3 = D3**
- [x] **T0.2b** ⚠️ **实施状态判定（17 → 14）**：用**实现层行为锚点**（⛔ **不用 tasks 勾选** —— 批 1 F-6：归档后执行未回勾 ⇒ tasks 是记录债）逐份判定 ⇒ **3 份来源 change 未实施** ⇒ 移出 + 登记 **R-16**（`dgpu-board-mvp` / `dgpu-soc-control-hub` / `dgpu-soc-physical-ip-port-boundary`）⇒ **批 3 = 14 份**
- [x] **T0.3** 事实基础提取：14 份的 Requirement 标题 + 来源 proposal「为什么」段 + **逐条可追性核对**（design D-1 末，**行为符号** ⛔ 非头文件存在）
- [x] **T0.4** **风险预扫描**（16 份有标记）⇒ 逐条核实 ⇒ **本批 0 份 spec 级 superseded、0 份单 Requirement、0 份需 waiver**
- [x] **T0.4b** ⚠️ **3 处草稿 vs 事实的主动收窄**（design D-1 末 N-1/N-2/N-3）：#12 `soc-internal-noc` 删「三条路径贯通」（GMMU 不经 mesh NoC）· #6 `gmmu-pte-wf` 改「专用访存客户端绕开 mesh」· #9 `minimal-dgpu-soc-architecture` 删 doorbell 那句（**R-18**）
- [x] **T0.5** **14 份 canonical Purpose 草稿**（design §D-1；写作规则沿用批 1 的 D-0）
- [x] **T0.6** **判据差异登记**：C-6 收紧到 **spec 级** superseded（批 1 口径会假红，8/9 命中是行级历史引用；批 1 已归档**不追溯**）· **C-7 新增 `git-history-proposal` 形态**（2 份来源 change 归档目录被移走，commit `0345d283` / `689752ef` 实测 ⇒ `git show` 可达 + 核心名命中 ⇒ **不需要 metis 指出的弱判据 `repo-doc`**）· **C-8 新增 `implementation_anchors`** · **C-9 新增 AC-02b 闭合载体** · ⛔ **不用 TBD 行做 glob 键**
- [x] **T0.7** improvement + proposal + design + 本 tasks + `.openspec.yaml`（`skip_specs: true`）+ 台账 **Entry #444**
- [x] **T0.8** **metis 预审**（planner 的固定审查者）—— **8 MUST + 8 SHOULD + 5 NIT 全处置**（其中 MUST-1 经实测**扩大**为 3 份移出）；处置对照表见 design §D-7

## Phase 1 · 落地（builder）

- [x] **T1.1** 写归因载体**生成器**（独立脚本）：从 `design.md` §D-1 表格**机器提取** 14 条 canonical 草稿（⛔ 不手抄）+ **glob 解析**来源归档目录（⛔ 不依赖 TBD 行）+ 从 spec **自动抽取** `requirement_titles` + 从 design 抽取 `implementation_anchors` + **glob 唯一命中**校验（命中 0 或 ≥2 ⇒ 报错退出）· `git-history-proposal` 的 `ref` 须为**固定 sha**（⛔ 不可写 `HEAD`）⇒ 写 `test/spec_purpose_batch3_attribution.json`（**⛔ 不手填 `checks`**）
- [x] **T1.2** 写 `scripts/test/cep_spec_purpose_batch3_check.py`（D-3 的 **C-1 ~ C-9**；带 `--apply` 落地模式 + `--reconcile` 生成 AC-02b 载体）
- [x] **T1.3** 逐份**只替换 Purpose 段**：正则整段替换 ⇒ **Requirements 段逐字节不动**；**单物理行**、**不新增空行**
- [x] **T1.4** 跑 T1.2 脚本 ⇒ **14/14 全绿**（C-1~C-9）
- [x] **T1.5** **判别力验证**：落地**前**跑校验 ⇒ 须 **0/14 绿**（占位残留被 C-1 抓到）⇒ 判据有牙

## Phase 2 · 复验

- [x] **T2.1** **AC-01** 限段占位计数 = 0（14 份）
- [x] **T2.2** **AC-02** 14/14 份 `--strict --json` 无 Purpose 类 issue
- [x] **T2.3** **AC-02b / AC-02c** 全局闭合（实测）：`pre 65 → post 55 ⇒ k = 10`；**Purpose 占位 59 → 45 = −14 精确闭合**；**「正文 >500」issue 69 → 69 不变**（本批不动正文）⇒ 本批 **10 份完全转绿**、**4 份仍红**（红因**全为「正文 >500」**）。载体 `test/spec_purpose_batch3_reconcile.json` · C-9 机判 **5 条全 True**（`fixed==pre−post` · `union 覆盖 pre` · `不相交` · `k==len(fixed)` · `remaining reasons 是真实 issue code`）
- [x] **T2.4** **AC-03** 四条正交（脚本判定 + **人工抽验 6 份 · 覆盖 5 组**；结论见下）
  - **人工抽验名单与结论（2026-11-16 · 6 份 · 覆盖 5 组 · 含 design §D-6 的组 ⑤ 强制交叉对拍）**：
    ① `dgpu-board-adapter-info`（组①）—— 草稿**不提** `cpptlm_emulator_get_adapter_info`（实测提及 **0** 次）⇒ 与 `cpptlm_emulator.h:108`「已删除 (per ADR-SOC-20 §2.1)」**一致**；草稿只声称「设备句柄的归属与释放路径」⇒ 由 `cpptlm_emulator_open` / `close`（`:106-107` + `src/abi/cpptlm_emulator.cc:340`）承载 ⇒ **一致**
    ② `lwc-bus-hub-mvp`（组②）—— 草稿描述「控制寄存器访问 / 门铃递交 / 消息读取三条路径经统一总线」⇒ 与 `include/tlm/lwc/lwc_bus_hub.hh` 的 `register_doorbell_target` + `doorbell_write` **一致**；⚠️ 实测**实际 handler 名与 `AGENTS.md` 记载的 `handle_cr_transaction` / `handle_doorbell_transaction` 不同** ⇒ 草稿无标识符 ⇒ **不受影响**，但该差异**登记为残余 R-19**
    ③ `minimal-dgpu-soc-architecture`（组③）—— 草稿**不提 doorbell** ⇒ 与该 spec 的 Requirement `doorbell 配置 vs 代码 hardcoded 不一致`（**未收敛的不一致**）**不冲突** ⇒ **一致**（该不一致已登记 **R-18**）
    ④ `driver-visible-minimal-soc`（组④ · 14 条 Requirement 最重）—— 草稿「配置空间所报告的**地址窗口大小**、显存窗口与**真容量**须区分」⇒ 与 `DGpuBoard` 的**双 size 字段**（B11，`dgpu_board_shell.hh`）**一致**；草稿 ⛔ 未声称任何已删除的 ABI ⇒ **一致**
    ⑤ `gmmu-pte-wf`（组⑤ · **metis 指定必含**）—— 草稿「页表取数须由该单元的**专用访存客户端**承接并**绕开 mesh 互联**」⇒ 与实现 `gmmu_tlm.hh:132-138`（`set_vram_target` / `uses_vram_dedicated_client` → `vram_ctrl_` 注入）+ 规范 `gmmu-pte-wf/spec.md:28`「**GMMU 不经 mesh NoC**」**一致** ⇒ **一致**
    ⑥ `soc-internal-noc`（组⑤ · **metis 指定必含** · **强制交叉对拍**）—— 草稿「路由骨架 + 统一请求响应接口 + **各节点是否承接实际流量的显式说明**」⇒ 实测草稿中 `GMMU` / `PCIe` / `页表` / `条带` 提及 **0 次** ⇒ **未声称「GMMU / PCIe 侧流量经内部 NoC 贯通」** ⇒ 与 `soc_internal_noc.hh:61,63`（`NODE_ID_PCIE`「v1.1 **无入向流量**」· `NODE_ID_GMMU`「v1.1 **不接**」）**一致** ⇒ **交叉对拍通过**
  - **结论**：6/6 **一致** ⇒ 无「声称不存在的能力」。⚠️ **机判边界**（design §D-3）：**纯中文的能力声称不在机判射程内** ⇒ 该边界由本次抽验 + critic 的 5 组逐组签署共同承担（批 4~6 **须保留同等人审环节**）。
- [x] **T2.5** **AC-04** diff 形状：**14 文件 · 每份恰 1 hunk · 增 1 行 · 删 1 或 2 行**（本批 1 份 `vram-controller-functional-mode` 的原占位后带尾随空行 ⇒ 删 2）· ⛔ 不新增空行 · **`## Requirements` 段逐字节不变**（实测 14/14 变动 **0**）
- [x] **T2.6** **AC-05** `claim_kind` 与 spec 级 superseded 互斥（C-6）+ **`implementation_anchors` 全部命中**（C-8）+ 标识符可追 + 证据可复现且核心名命中（C-7）
- [x] **T2.7** **AC-06/07** 43 份 `cep-*` `--strict` rc≠0 = 0 · `artifact_count` 86 不变 · 范围零 diff
- [x] **T2.8** **新残余登记**：**R-15**（`v1-architecture` 引用已迁 ArchForge 的 `00-overview.md`）· **R-16**（3 份未实施 spec 存废 · 路由 architect）· **R-17**（2 份来源归档目录缺失）· **R-18**（doorbell 配置 vs 代码不一致）· **R-19**（`AGENTS.md` 记载的 LWC handler 名 `handle_cr_transaction` / `handle_doorbell_transaction` 与实现实际名 `register_doorbell_target` / `doorbell_write` 不一致 —— 本批 Purpose 无标识符故不受影响，但**登记处 stale**）

## Phase 3 · 审查与归档

- [ ] **T3.0** **归档语义备案**：出口 A（实测收敛为唯一路径）⇒ 落 **标记行 + 归因 JSON `archive_semantics`**（⛔ 本 change 不改 `AGENTS.md`）
- [x] **T3.1** critic 审（`arch`+`plan`+`build` 三档；**按 5 组签**）—— ✅ **mergeable: yes / 0 BLOCKER / 4 SHOULD_FIX / 5 NIT / 5 组全签**（无一组回退）
  - **4 条 SHOULD_FIX 全修**：
    ① **C-6 大小写漏洞**（`re.search` 大小写敏感 ⇒ 状态位写小写 `deprecated` 会**漏判**）⇒ 加 `re.IGNORECASE`；⚠️ 词表**故意不含** `SUPERSEDE`（不带 d）—— `| **supersede** | 原 X |` 的语义是「**本 spec 取代了 X**」⇒ 命中它 = 假红（critic 亦指出 `driver-visible-minimal-soc/spec.md:400` 这处「判对但部分靠运气」）。加 IGNORECASE 后实测 **14 份状态位置仍无命中**
    ② **C-9 与 design D-3/AC-02c 不一致** ⇒ ③ `fixed_pure` 原算了却**未纳入 mc**（⇒ AC-02c「fixed 的 pre issues ⊆ 仅 Purpose 类」**无机器强制**）④ 原查的是 fixed 规格的 **post** issues 而文档要求 **pre** ⇒ 改为在**影子根**里查 pre issues ⑤ `k_equals_len_fixed` 原**硬编码 True**（恒真无判别力）⇒ 改为真判据 `len(fixed) == len(pre.red) - len(post.red)`。⇒ C-9 机判由 5 条增为 **6 条，全 True**
    ③ **#13 `soc-internal-noc-data-flow` 的「各目标端」超出实现**（v1.1 实际只装 **2** 个 adapter；该 spec 自身 `e1-dgpu-board-adapters-wiring` 明写「不装配 PcieControllerIP / GMMU adapter」）⇒ **收窄**为「**数据流所及的**目标端」⇒ 已重新落地
    ④ **残余登记漏项**（`dgpu-board-adapter-info/spec.md:10,21` 的 Scenario 仍引用**已删除**的 `cpptlm_emulator_get_adapter_info`）⇒ ⚠️ critic `scope_exceeded: true`（属 Requirements 正文 staleness，**超本 change 范围**）⇒ 登记 **R-20**，**不回改**
  - **5 条 NIT 的处置**：① #12 的「统一的请求响应接口」实现在**同源姊妹 spec**（`e1-send-request-response-api`）⇒ 系统层面为真，文档层面略有溢出 ⇒ **NIT，不返工**（⛔ 避免范围膨胀）② `repo_has_symbol` 递归扫 `test/` 且含 `*.py`，比 D-3 文档的 `test/*.cc` **更宽** ⇒ **收窄到与文档一致** ③ D-2 称 `purpose_text`「从 spec 自动抽取」，实为从 design.md 提取 ⇒ **新增 C-10**（载体 `purpose_text` == spec 现行 Purpose 的机器比对，防「apply 后人为改 spec ⇒ 载体 stale」）④ C-6 区域 2 要求 `>` + 粗体 ⇒ 非粗体/中文标题会绕过 ⇒ 已接受的残余面（T0.4 预扫描兜底）⑤ AC-04 的 diff 形状不在 C-1~C-9 机判射程内 ⇒ 仅 T2.5 人工验证（可复验，接受）
  - **critic 的验证建议已采纳**：⚠️ `--reconcile` 的基线**不可取 HEAD**（落地已 commit ⇒ HEAD 即 post 态 ⇒ 重跑得 pre=55/k=0 = 假失效）⇒ **基线显式化**（`--reconcile --base <sha>` > 载体 `base_commit` > HEAD）⇒ 本批用 `--base f1cb3cb8`（落地前 commit）实跑 ⇒ **pre 65 → post 55 / k=10 / 6 条机判全 True**
- [ ] **T3.2** 影子根**预演**归档 ⇒ main spec 零改动 + 归档无警告
- [ ] **T3.3** 真仓归档（出口 A）+ 复验 14 份 `--strict`
- [ ] **T3.4** **AC-08** 闸 0 回归（**归档前跑一次**：45 闸 + `ctest -R cep` + `[cep]` == T0）
- [ ] **T3.5** 产物 commit
- [ ] **T3.6** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 归档 **change 目录数** **150 → 151** + 台账闭合回执 Entry **#445**
- [ ] **T3.7** 闸最终复验 + `git status` 全清
- [ ] **T3.8** verifier 独立验收（报告落 `.rddf/reviews/cpptlm-spec-purpose-batch3-verify.md`；**须另写独立实现复算**）

## Phase 4 · 遗留

- [ ] **T4.1** 提请 architect：R-9（清单增殖 canonical）· R-2（13 键 golden）· R-10（superseded spec 存废）· R-11（4 份同源 spec 合并退役）· **DoD 7 口径订正**（66 条 invariant 实为 61 条有载体 + 5 条 ⏭）· `AGENTS.md` 归档分类表补记「✅ 主 spec 直改（无 delta）」变体
- [ ] **T4.2** 提请 architect：**R-1 批 2**（26 份正文 >500 裁剪 —— 会动 `## Requirements` 段 ⇒ 另一档风险，须另立 change）
