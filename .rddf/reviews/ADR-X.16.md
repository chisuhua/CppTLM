# 审稿报告 — `ADR-X.16-router-local-port-terminal-credit.md`

- **ADR**: `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`
- **Status 转换**: `Proposed` → **`Accepted`**
- **日期**: 2026-11-16
- **审查者**: oracle（先行）→ critic（复审）
- **审查轮次**: oracle 1 轮 + critic 1 轮（**0 BLOCKER**）· **未触发复审**（按评审预算：只有 BLOCKER 触发返工与复审）
- **产物归属**: architect 收口时从两份审稿输出誊写（审查者本身只读）

---

## 〇 · 意图检查点处置（§oracle 调用前）

| 项 | 处置 |
|---|---|
| 意图陈述 | 「修复 `RouterTLM` 的 `LOCAL` 端口在**终端下游**场景下 credit 记账导致**静默停滞**」—— 要解决的问题 + 范围边界（只改 credit 记账语义，不动 JSON schema / 协议面 / `credit_timeout_` 默认值 / `buffer_size` param）均在送入 oracle 前写定 |
| 是否存在 ≥2 种合理解读 | 否。方案 A~E 已由 architect 在 ADR 内枚举并决策（architect 的职责即做该取舍，不需问用户） |
| 是否调用 `question` | 否（意图与范围无反歧义） |
| 是否先行调 metis | 否（意图已定，歧义已在 ADR 的 R-1~R-3 与「不在本 ADR 范围」段枚举） |
| oracle 判定行原文 | `判定: APPROVE` |

---

## 一 · oracle 审查（审查域：其他审查）—— **判定 APPROVE**

### 结论要点（4 节摘录）

**根因**（oracle 复核，与 ADR 的 F-1~F-9 逐条相符）：credit 记账对全部 `out_port` 一视同仁；LOCAL 的下游是终端，结构上无 credit 归还通道（`return_credit_to_upstream(in_port)` → `reverse_port(4)==4` → 经 `resp_out[LOCAL]` 发回终端 ingress，而终端只能丢弃）；`credit_timeout_=0` ⇒ 无超时/计数/日志。

**对三个质疑点的回答**：
1. **上游无副作用 —— 成立**。SA winner 即使因 credit 不足被停进 `pipe_reg_`，也是**先从 `input_buffer` pop、立即 `return_credit_to_upstream`**（`router_tlm.cc:352-360`）；正常路径同理（`:383`）。D1 去掉的只是「停进 `pipe_reg_`」这一步 ⇒ 上游看到的 buffer 归还时机不变或更早。mesh 段 `has_credit/consume_credit/receive_credit/credit_safety_reset` 代码路径零改动 ⇒ 端到端不会出现「流控短路段落」。`pending_link_` 每周期只 drain 1 个（`:400-404`），LOCAL 入队速率被 SA「每 out_port 每周期 1 winner」限在 1/cycle ⇒ 不会堵塞 mesh flit。
2. **双向 LOCAL —— 行为正确**；并指正 ADR 原 F-6 的表述：`apu_soc_full` 的「出方向悬空」在**绑定层**不成立（Step 7b 会为每条 connection 自动补反向 PortPair），且该 config 因 `ComputeUnitTLM` 未注册本就不可装载。
3. **多 VC —— 无影响**（buffer / `vc_state_` / `pipe_reg_` 全部按 `(port,vc)` 隔离）。

**已排除的替代方案复核**：B（含 programmatic setter 变体，同样零消费者）/ C（重置 = 虚增额度，对 mesh 超发）/ D（结论成立但措辞需收敛为「无通往 router credit-receive 端口的通道」）/ E（不可观测缺陷不能只登记）—— 四条否决均成立。

### oracle 的 7 条建议 —— 逐条处置

| # | 建议 | 处置 |
|:--:|---|---|
| 1 | V-3 应钉 **mesh out_port**（全仓 `test/` 无 RouterTLM mesh 反压正向用例；V-4 只是被动等值守卫） | ✅ **已采纳**：V-3 改为「构造 mesh 下游不归还 credit 的场景」，并注明同时补上该验证空洞 |
| 2 | D4 对「router 挂 LOCAL」误用的**可见性主张过度**（D1 后 `has_credit(LOCAL)` 恒真 ⇒ 永不 `waiting_for_credit` ⇒ 不计数；真实失败是下游 router 的 `input_buffer` 满后静默丢 flit） | ✅ **已采纳**：D4 增「定位澄清」段；R-1 的缓解措辞收敛为「唯一防线 = 端口注释（V-6）+ D5 另立决策」 |
| 3 | 「沉默边界迁移」应在 Consequences 登记（D1 后丢包点可能移到终端 ingress：`process_net_response_input` 静默丢包且不 release） | ✅ **已采纳**：Consequences 负面新增该条（含现行终端均 1/cycle 消费 ⇒ 无当前回归） |
| 4 | 三处勘误：V-1~V-5 → V-1~V-6；`stats_credit_stalls_` 计数语义未定死；F-6 的 `apu_soc_full` 表述 | ✅ **全部采纳**：编号对齐；语义定死（后续 critic 指出仍不够精确，见二-1）；F-6 改为「JSON 声明层仅入向、绑定层自动补反向、且当前不可装载」 |
| 5 | `NICTLM` 无 credit flit 过滤（同族缺陷）应另立 backlog | ✅ **已采纳**：登记在「关联 → 相邻独立缺陷」第 2 条（含可复现描述与影响面） |
| 6 | 选项 B 的低成本变体（`RouterTLM::set_credit_aware(port,bool)`）是否值得考虑 | ✅ **确认不采纳**（oracle 自己也判定：零调用方、同样要测试与文档负担，只是把契约成本从 JSON 挪到 C++ API；D5 保留演进 seam） |
| 7 | 实施时兑现：`downstream_credits_[LOCAL][*]` 死状态加注释；`RouterPort::LOCAL` 注释改终端语义 | ✅ **已作为实施落点写入**（Consequences 负面 + V-6） |

---

## 二 · critic 审查（profile: arch）—— **APPROVE_WITH_CONDITIONS**

**`mergeable: yes` · `scope_exceeded: false` · 0 BLOCKER**

### BLOCKER（四类判据逐一核过 → 无）

- **冲突 ADR**：`ADR-X.15` 的 Status Update 把 LOCAL credit 回流显式定性为「既有路由器行为」、修复「另立 backlog 追踪」⇒ X.16 正是**接这条被显式推迟的决策路径**，不推翻 X.15 任何决策条目，无需取代流程；`ADR-DGPU-06` 管辖 board-level TLP 边界，与 chip-internal `NoCFlitBundle` 的 router 流控**正交**。
- **决策完备性**：选项 A–E 覆盖真实备选 + D5 演进 seam；零配置面 + 改动局限 credit 记账三分支 ⇒ `git revert` 即完整回滚，不构成「缺回滚路径」（但建议显式写出，见 NITS-1）。
- **自相矛盾 / 伪决策**：D1–D5 / R-1~R-3 / V-1~V-6 交叉引用未见二义（1 处统计语义歧义见 SHOULD_FIX-1，两种读法均满足 V-3 与 improvement AC-03，不阻断）。
- **focus 2 的契约面核对**：`RouterPort::LOCAL` 注释的订正已被 D5 + V-6 指名为实施落点；`topology_generator.py:76,150-155` 与 `docs/architecture/10-topology-generation-management-system.md` 的 NI↔Router LOCAL 映射**不含 credit 语义断言**（生成器只对 Router↔Router 链路挂 `credit_flow` 配置块；schema 描述明写「用于 Router 间链路」）⇒ 这些契约面 **already 与 D1 一致，无需订正**，ADR 未遗漏落点。
- **focus 3 的隐藏依赖核查**：credit 归还发生在 `input_buffer` pop 时、与出向端口无关 ⇒ `NICTLM` credit 过滤缺失**不改变** credit 归还体量，不构成本 ADR 结论的隐藏依赖；`buffer_size` 不生效只影响阈值公式锚点，不影响 D1 成立。

### SHOULD_FIX —— 1 条（已修，不触发复审）

| # | 内容 | 处置 |
|:--:|---|---|
| 1 | **`stats_credit_stalls_` 语义在同一句内自相矛盾**（「stall 周期数」vs「每累计 ≥16 cycles 记一次」，第三种读法「阈值后每周期 +1」也通）⇒ 实施方无法从文本判定 | ✅ **已修**：D4 改为**分两件陈述** —— 「计数器 = stall 周期累计（该 `(port,vc)` 处于 `waiting_for_credit` 的**每个周期 +1**）」+「日志 = 每 `(port,vc)` 每 **16 cycles** 打印一次」，两者分开 |

### NITS —— 2 条（已修）

| # | 内容 | 处置 |
|:--:|---|---|
| 1 | 无显式回滚陈述 | ✅ Consequences 负面补「回滚路径」一条（零配置面 ⇒ `git revert` 完整回滚，无迁移成本） |
| 2 | 阈值公式 `2 × BUFFER_DEPTH` 锚定了「不生效的」`buffer_size` 语境，未来若生效会漂移 | ✅ D4 注明「阈值绝对值以 **V-3 的 16 cycles** 为准，公式仅为取值依据，不随公式漂移」 |

### critic 的验证建议（转交实施轮，非本轮范围）

- 实施轮由 build/verifier 执行：`grep -n "LOCAL" include/tlm/router_tlm.hh` 确认 V-6 落点；`[cep]` / `[e2e]` / `[mesh]` 全量回归验证 D3/V-4。
- V-1 测试的下游终端选型：**避免用 `NICTLM`** 作 LOCAL 下游的计数基准（其 credit 过滤未修，见「关联 → 相邻独立缺陷 2」），选 `CohIfNICTLM` 或纯 sink。
- 立项后回填 AC 编号时，核对 improvement `AC-03`（较宽松，允许 `credit_timeout_` 告警）与 ADR `V-3`（较严格，钉死 mesh 场景 + timeout 仍为 0）的口径差，**以 ADR 为准**。

---

## 三 · 收口检查（ADR 完成的标志 5 条）

| # | 判据 | 状态 |
|:--:|---|:--:|
| 1 | 四要素齐（元数据含 AC 映射 / 选项对比 ≥3 行带结论+理由 / 独立「后果·代价」章 / Verification 前瞻判据） | ✅ |
| 2 | oracle + critic 双审完成；BLOCKER 全部闭合并经复审确认（本 ADR **0 BLOCKER** ⇒ 无需复审轮） | ✅ |
| 3 | `Status` 已从 `Proposed` 转 `Accepted` | ✅ |
| 4 | 关联 change 已落库或已登记（本 ADR 用 `backlog: cpptlm-router-local-port-credit-exhaustion` 占位；立项与 AC 编号由 planner 阶段补，architect 不创建 openspec change） | ✅ |
| 5 | 审稿报告已落 `.rddf/reviews/ADR-X.16.md` 且 ADR 元数据写有该指针 | ✅（本文件） |

**结论**：ADR-X.16 收口完成，`Accepted`。

**未在本次处理（历史/范围外，仅记录）**：`docs/adr` 下既有 ADR 的形态问题不追溯；本 ADR 的相邻独立缺陷（`buffer_size` param 不生效、`NICTLM` 缺 credit 过滤）已在关联段登记，须由 planner 立项。

---

## 四 · 第二轮审查（2026-11-17 · D4 锚点澄清追加段）—— oracle 判定 APPROVE

**改动形态**：末尾追加 `## Status Update`（原文件无此段）；`git diff` = **+21/−0**，未触碰 D1-D5 / 选项对比 / Consequences / V-1~V-6 / 关联章。

**缘起**：change `cpptlm-router-local-port-credit-exhaustion` 的 **T6.1**（跨角色 · 明确列给 architect）。实施期经 metis 提出、builder 实测确认：D4 原文锚点「处于 `waiting_for_credit` 的每周期 +1」**按字面实现恒为 0**。

### oracle 四问逐条（全部 PASS）

| # | 审查点 | 结论 |
|:--:|---|---|
| 1 | 锚点订正的事实是否成立 | **成立**。VA 拒绝分支（`:228-232`：`note_credit_stall` + `release_vc` + `continue`）⇒ `vc_allocated` 保持 false ⇒ SA 跳过（`:260-261`）⇒ 该 flit 永不成为 winner ⇒ `waiting_for_credit` 唯一置位点（`:359`，ST 阶段）不可达 ⇒ 原锚点恒 0 |
| 2 | 「以 Status Update 澄清、不改 D4 正文」是否成立 | **成立**，不构成自相矛盾。D4 的四项决策内容（周期累计语义 / 16-cycle 日志节流 / 16-cycle 阈值 / `credit_timeout_`=0）逐字保留；被订正的只是**实现代理**，且原代理按字面不可实现。规范口径已由 change spec 承接（`specs/router-local-port-flow-control/spec.md:47` 显式写明「锚点口径是 VA/SA 拒绝条件，**不是** `waiting_for_credit`」） |
| 3 | 订正后是否仍达成 D4 原始目的 | **达成**。mesh 死锁可观测（RLC-3：裸 router ⇒ `>=16`；归还 credit 后严格相等不增长，对「无脑 +1」有真实判别力）；不可被重置掩盖（`credit_timeout_` 默认 0 未动）；**LOCAL 误触发不可能**（`has_credit(LOCAL,·)` 恒真 ⇒ 两处 `!has_credit` 分支对 LOCAL 永不成立；RLC-2 亦断言 LOCAL 注入 `stall_cycles == 0`） |
| 4 | 是否超出范围 | **未超**（+21/−0，纯追加） |

### oracle 的两条非阻断项与处置

| # | 内容 | 处置 |
|:--:|---|---|
| 中 | 台账交叉引用待确认（oracle grep `docs/roadmap/README.md` 未命中 #459/#463） | ❌ **不成立** —— **oracle 找错文件**：台账实际落 `.rddf/roadmap/objectives/obj-cep-platform.md`，实测 `:24965` = Entry #459、`:25041` = Entry #463 ✅ 引用正确，无需改动 |
| 低 | 计数器粒度是「(stalled flit, cycle) 事件对」而非「唯一 `(port,vc)` 周期数」（SA 分支 `continue` 不跳出 `in_vc` 循环 ⇒ 多 flit 争用同一 out_vc 时一周期可 +N） | ✅ **备案不改** —— oracle 明确「当前所有判据（V-3 阈值型 `>=16`、RLC-3 严格相等）对此粒度均稳健」，且追加段措辞「存在待发 flit 且 `has_credit == false` 的周期累计」按 per-flit 语义自洽。仅登记以防未来被当作回归借口 |

### 已排除的替代方案（oracle）

新建 ADR-X.17 废止 D4（成本过高，Status Update 对齐机制已足够）/ 直接改 D4 正文（违反不可变）/ 只在 change design.md 记录（真源 3 会与实现漂移）/ 改 `has_credit`·流水线让 flit 进 ST 以挽救原锚点（「改实现凑文档」反模式，破坏 V-4 零 diff 回归）。

**判定行**：`判定: APPROVE`（可收口）
