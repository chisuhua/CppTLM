# CEP 交接:剩余任务盘点 + 新会话推进说明(2026-11-09)

> **生成时刻**:2026-11-09 · **HEAD** = `96c17efa` · **作者**:planner · **读者**:新会话的 agent / 用户
> **一句话现状**:`cpptlm-cep-stub-cache-chstream-surface` 已**完整闭环并归档**(A0~A9 + W1~W4 + A9.2 三处 SU + KPI 回 0/3)⇒ **轨 A 无遗留**;剩余工作全在**治理轨 / 残余 / 周期项**。

---

## 一、当前状态快照(新会话先读这段)

| 项 | 值 | 证据 |
|---|---|---|
| **active change** | **0 个** | `openspec list` ⇒ No active changes |
| **Proposed 并发(KPI)** | **0/3** | `obj §5.2` + `.rddf/roadmap.md` + `AGENTS.md`(活口径已同步) |
| **已归档 change** | **132** 份 | `openspec/changes/archive/*/` |
| **`cep-*` main spec** | **37** 份 | `openspec/specs/cep-*/` |
| **终态 DoD** | 12 条中 **11 条达成**;唯一缺口 **`B3b-2` = 0/4 ⏭** | `obj §6.2`(**外部阻塞,内部可推进 = 0**) |
| **最近 5 commit** | `96c17efa` KPI 同步 / `ab441d8f` 清单 / `c05b17e2` 归档 / `275458a9` 清单 / `f26812bc` 三处 SU | `git log --oneline -5` |
| **工作区** | 干净(本交接提交后) | `git status --porcelain` |
| **测试** | `ctest` **166/166** · `./test.sh --mode off` **21/21** | 实测 |
| **红线面** | `stub_cache_core.hh` / `include/framework/` / `cohif_noc_bridge.hh` / `protocol_instances.json` / `protocol_instance.schema.json` / `cep_b3b2_deferrals.json` —— **零 diff** | 实测 |
| **角色状态** | `role.json` = **planner**(本会话切至 planner) | `.rddf/state/role.json` |

---

## 二、剩余任务全量盘点(**轨 A 已清空** ⇒ 以下全是新轨)

### 组 1 · 架构治理轨(architect / planner 域 · **不占** KPI slot)

| # | 任务 | 现状/实测 | 估时 | 落点 | 触发条件 / 前置 |
|---|---|---|---|---|---|
| **B2** | **ADR 结构闸** `cep_adr_structural_check.sh` + deferrals 清单 | ⚠️ **口径已漂移**:旧记「13/22 缺 Verification」,**实测 = 20/22**(2026-11-09)⇒ 须按**现场扫描**重算 | 0.5d | `scripts/test/` + `test/cep_adr_*_deferrals.json` | 无(零冻结面)⇒ **建议下一个做** |
| **B1** | 17 份「🟡 起草中」ADR **逐批**核对 ⇒ 达标者转 ✅ 已签发 | 17 份(实测);⛔ **不可批量签发**;每批需**冻结面两步授权** | 1d/批 | `docs/adr/ADR-CEP-*.md` + 冻结清单 | A9 已完成 ⇒ **前置已满足**;建议 B2 之后 |
| **W5** | 签名 typo `DEFERRRAL-TRIGGER-DRIFT`(三 R)统一 | 可选 · 独立 | 0.1d | `scripts/test/` | — |

### 组 2 · 本 change 的**残余**(Entry #305 登记 · 每条都有触发条件)

| # | 残余 | 归属 | 触发条件 / 做法 |
|---|---|---|---|
| **R-1** | 新 main spec `openspec/specs/cep-stub-cache-chstream-surface/spec.md` **未纳入 M1 冻结集** ⇒ 其契约面可被**无授权**改动(与既有 5 份 `cep-*` main spec 治理不对等) | **architect 裁决** | 纳入 ⇒ 另立台账 Entry + 两步授权链(`cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单单独 commit → 无参复验);⛔ **不得**擅自扩 `FROZEN_GLOBS` |
| **R-2** | **回程腿在拓扑级不可达** ⇒「全绿 run」目标**未达成**(IC3 = ⚪ 不适用 + 延迟族 `not-collected`) | builder(另立 change) | **双条件**:① 修 `include/framework/dual_port_stream_adapter.hh` 的**入向按端口路由**(红线面 ⇒ 另立 change + 冻结面授权)② 拓扑 config 改 `emit_completions: true` 并重跑(详见 `07 号` SU · 十五 ② ①) |
| **R-3** | `ModuleFactory` Step 7.5 的**重挂检测**未做(本轮只落地测试级 `[BSS-01]` 闸 ⇒ **只测不拦**) | builder(另立 change) | 动 `src/core/module_factory.cc`(红线面)⇒ 另立 change |
| **R-4** | `cohif_responder.hh` 注释文件名张冠李戴 | ✅ **已修**(builder 同批) | — |

### 组 3 · 周期项(**保不漏检**,⛔ 不得造内部替代品)

| # | 项 | 现状 | 做法 |
|---|---|---|---|
| **E1** | `B3b-2` 的 **D-4 触发探测**季度检索 | ⚠️ `scripts/test/cep_b3b2_d4_quarterly_review.sh` 报 **`CADENCE-NEVER-RUN`** ⇒ **从未执行过一次真实探测** | 先跑 `--validate-only` 看现状;**真实探测**须查 4 族 × 2 路径的公开来源(CXL / UALink 规范可读化 · NVIDIA 条款级规范 · 人类成员条款摘要)⇒ `--record` 落一条 run-record ⇒ ⛔ **不得**凭推理填 outcome |
| **E2** | 终态 DoD Go 季度复审 | `B3b-2` 仍 **0/4** ⇒ 挂起 | 与 E1 同批做;窗口 2030-Q1 未到期 ⇒ **挂起 ≠ 拒绝** |

### 组 4 · 可选改进 / 状态漂移

| # | 项 | 说明 |
|---|---|---|
| **O-1** | `07 号` **索引生成器**入仓 | 现为**手工同步**(生成器不在仓内,已登记 backlog)⇒ 已有 `scripts/test/cep_index_position_check.sh` 兜底(位置 + 计数自洽)⇒ 优先级**低** |
| **O-2** | improvement `cpptlm-cep-stub-cache-chstream-surface` 的**状态**仍是 `pending` | ⛔ 已归档 ⇒ 状态漂移,本交接提交同批修正为「已归档」(planner 域) |
| **O-3** | improvement `cpptlm-cep-b3b2-d4-trigger-probe`(🟡 提案) | 与 E1 同源 ⇒ 建议随 E1 一起处理(要么实施、要么合并进 E1 的手工节奏) |
| **O-4** | `.rddf/state/.arch-handoff.json`(2026-08-15)**陈旧** | ⇒ `rdd-planner` / `rdd-builder` 的 handoff 前置**不满足** ⇒ 继续走**内置流程**(已登记,⛔ 不得伪造前置) |
| **O-5** | `.rddf/skill-profiles/session-bootstrap.toml` | 本交接**新建**(skill 的零配置路径)⇒ 新会话可用 `rdd-session-bootstrap` 一键出 Layer 3 状态 |

---

## 三、新会话怎么推进(两种方式)

### 方式 A(推荐)· 用 skill 生成 paste-ready prompt

```bash
cd /workspace/project/CppTLM
bash ~/.agents/skills/rdd-workflow/skills/rdd-session-bootstrap/rdd-session-bootstrap.sh
```

⇒ 输出 **Layer 3 原始状态**(HEAD / active changes / recent commits / workspace health)⇒ 再由 LLM 合成 **Layer 4 paste-ready prompt**(⛔ 不直接拿 Layer 3 当 prompt 用;⛔ 复用旧 prompt 前须核对 timestamp + HEAD)。

### 方式 B · 手动启动清单(6 步,约 2 分钟)

1. `cat .rddf/state/role.json | head -20` ⇒ 确认当前角色与**授权窗口**(本仓 `auto_switch_window = null` ⇒ **切换角色必须等用户口令**)
2. `git log --oneline -12 && git status --short` ⇒ 确认 HEAD / 工作区
3. `openspec list --json` ⇒ 确认 **0 active**
4. 读本文件 **§二**(剩余任务)与 `obj-cep-platform.md` 的 **§10 路径图修订 ④** + **§11 Entry #305**
5. `ctest --test-dir build -j4` ⇒ 确认 **166/166**;`bash scripts/test/cep_freeze_check.sh` ⇒ 确认冻结链绿
6. 明确**本轮要做的 1 件事** + 角色口令(见 §四)⇒ 开工

### 新会话的第一句话(可直接粘贴)

```text
读 .rddf/reviews/cep-next-session-handover-2026-11-09.md(CEP 交接:剩余任务盘点 + 推进说明)。
当前轨 A 已闭环归档;请先复述 §二 的剩余任务分组与建议顺序,然后按我给的角色口令开工。
本会话的默认纪律:冻结面改动走「台账 Entry 授权 → 产物 commit → cep_freeze_check.sh --update
--authorized-by "Entry #N" → 清单单独 commit → 无参复验」;B3b-2 维持 0/4;不主动 commit/push。
```

---

## 四、建议的推进顺序(按「解锁最多 / 风险最低」排序)

| 序 | 任务 | 角色 | 为什么排这里 |
|---|---|---|---|
| **1** | **B2**(ADR 结构闸 + deferrals,0.5d) | builder | 零冻结面;把「ADR 结构缺口」从**暗**变**可见**(实测口径已漂移到 20/22);且它是 B1 的前置准备 |
| **2** | **R-1** 裁决(新 main spec 是否入冻结集) | architect | 小决策,但**治理不对等**已存在;裁决后需一次两步授权链 |
| **3** | **B1** 第 1 批(逐份核对签发,1d/批) | architect + builder | A9 已完成 ⇒ 前置已满足;⛔ 每批都要冻结面授权 ⇒ 慢,分批推 |
| **4** | **E1**(D-4 季度探测,含 `--record`) | planner + 外部检索 | **从未执行过** ⇒ 漏检风险实在;⛔ outcome 必须来自真实检索 |
| **5** | **R-2 / R-3**(框架与 core 的红线面修复) | planner → builder | 各自**另立 change**;R-2 是「全绿 run」的前置 ⇒ 价值最高但成本最大 |

---

## 五、硬纪律(新会话必读,⛔ 违反即返工)

1. **冻结面两步授权链**:`docs/adr/ADR-CEP-*` / `docs/architecture/07-*` / `openspec/specs/*` ⇒ 台账 Entry 授权 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → **清单单独 commit** → **无参复验**。
2. **计数器纪律**:`B3b-2` **必须维持 0/4**;`test/cep_b3b2_deferrals.json` 零 diff;⛔ **不得**用「框架跑通」改判。
3. **红线零 diff**:`include/framework/` · `stub_cache_core.hh` · `cohif_noc_bridge.hh` · `protocol_instances.json` · `protocol_instance.schema.json`。
4. **不主动 commit / push**(等用户确认);⛔ **不得**用「框架跑通」冒充「协议语义达成」(这是 B3b 计数虚增的根因)。
5. **角色切换**只认用户口令(`切换到 X`);`auto_switch_window = null` ⇒ ⛔ 不得自推断切换。
6. **新判据必须「有牙」**:带**决定性变异体**或反向 fixture;⛔ 不得用「断言存在」冒充「行为断言」。
7. **`B3b-2` 的外部阻塞**:⛔ 不得为它造内部替代品(4 受限族的 D-4 路径已耗尽)。

---

## 六、关键文件索引(新会话按需取用)

| 用途 | 路径 |
|---|---|
| 交接(本文件) | `.rddf/reviews/cep-next-session-handover-2026-11-09.md` |
| 目标 + 路径图 + 台账 | `.rddf/roadmap/objectives/obj-cep-platform.md`(**§10 修订 ④** = 开工基线;**§11 Entry #297~#305** = 实施流水) |
| 本 change 归档件 | `openspec/changes/archive/2026-11-09-cpptlm-cep-stub-cache-chstream-surface/`(tasks **52/52**) |
| 架构真源(实施后状态) | `docs/architecture/07-coherence-exploration-platform.md`(**SU · 十五** @ L2290) |
| ADR 实施后状态 | `docs/adr/ADR-CEP-19-*.md`(SU 2026-11-09)· `docs/adr/ADR-CEP-06-*.md`(SU 2026-11-09) |
| 目标一致性审查 | `.rddf/reviews/cep-goal-alignment-review-2026-11-08.md` |
| 双审发现(14 条) | `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md` |
| 判据载体 | `test/test_cep_stub_cache_e2e.cc`(SCE-01~21)· `test/test_cep_stub_cache_emit.cc`(E2E-01)· `test/test_cep_reproducibility.cc`(RPR-01~05) |
| 台账机器可读 | `test/cep_e2e_runs.jsonl` + 闸 `scripts/test/cep_e2e_reconcile_check.sh` |
| 冻结清单 | `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(41 产物) |

---

## 七、勘误与裁决(2026-11-09 追加,append 式 —— ⛔ 不改写上方原文)

> **追加者**:planner · **追加时刻**:2026-11-09(同日晚于本文件)
> **触发**:用户口令「请 oracle 对上述裁决的 4 点给出建议,然后按建议顺序用对应的角色实施任务」
> **性质**:本段**勘误上方 §一 / §二 / §四 / §五 的实测与措辞错误**,并登记 4 项架构裁决。**上方原文一律保留**(append-only)⇒ 引用上方任何数字前**先读本段**。
> **裁决来源**:oracle 咨询(1 轮,2026-11-09)· 台账 **Entry #307**。

### 7.1 实测订正(8 处)

| # | 上方写的 | 实测 | 处置 |
|---|---|---|---|
| **E-1** | §一 HEAD = `96c17efa` | 实际 `03732b74`(**本文件自身的 commit**)⇒ §一 快照测于**本文件提交之前** | 引用 §一 基线时以 `03732b74` 为准;「工作区干净 / ctest 166/166 / 冻结闸绿」三项在提交后复测仍相符 |
| **E-2** | §二 B2「实测 = **20/22**」(台账 Entry #306 ② 同) | **无任何口径可复现**:口径 A(顶层 `##` 章含 Verification)= **19/22 缺** · 口径 B(任意层级)= **13/22 缺**(= 旧记 13/22 的口径)· 口径 C(顶层且在首个 SU 之前)= 19/22 | 订正为 **19/22 缺(含 3 份非决策文档)/ 决策分母 16 缺 + 11·19 豁免 + 14 份待补**(见 7.2 C-1) |
| **E-3** | §二 B2「口径已漂移 ⇒ 须按现场扫描重算」 | `scripts/test/cep_adr_structural_check.sh` **不存在** | B2 = **从零建闸 + 定 schema**,⛔ 不是「修正已存在闸」 |
| **E-4** | §二 R-1「与既有 **5** 份 `cep-*` main spec 治理不对等」 | M1 冻结集只含 **10 条显式** spec 路径,仓内共 **37** 份 ⇒ 未冻结 **27** 份 | R-1 范围**扩大 27 倍** ⇒ 已改写为「manifest 目的裁定 + 27 份显式登记」(见 7.2 C-2) |
| **E-5** | §五.1 授权链覆盖 `openspec/specs/*` | manifest **无此通配**(20 条 globs 逐条列举) | 订正为「9 条显式 spec + 1 条归档 delta」 |
| **E-6** | §五.3「红线零 diff」6 项并列 | 其中**只有 `protocol_instance.schema.json` 在冻结集**;`include/framework/` · `stub_cache_core.hh` · `cohif_noc_bridge.hh` · `protocol_instances.json` · `cep_b3b2_deferrals.json` **都不在** | **两套机制必须分栏**:① **M1 冻结面**(manifest 驱动 ⇒ 改它走五步授权链)② **语义红线**(change scope 约束 ⇒ ⛔ 不是 manifest,不需要 `--update`)。§五.1 与 §五.3 混用会导致新会话误判「要不要跑授权链」 |
| **E-7** | §二 R-2「修 `dual_port_stream_adapter.hh` 的**入向按端口路由**」 | ⚠️ **本条订正了前一版勘误** —— 交接件原文**正确**:缺陷是**未按端口组路由**(`DualPortStreamAdapter` 只 override **单参** `process_request_input` ⇒ 框架唯一入口 `ChStreamTargetPort::recvReq`(`include/core/chstream_port.hh:70`)调的**双参**版被基类回退(`include/core/stream_adapter_base.hh:82-84`)⇒ 端口下标丢失 ⇒ **恒落 PE 侧**)。⛔ **不是**「不按包类型分流」—— 按 type 分流**反而更错**:`req_in[i]` 与 `resp_in[i]` **共享同一 `port_idx_`**,且 CohIf 的 egress 走 `req_out` ⇒ 回程 `Completion` 以 `PKT_RESP` 打在**对端的 `req_in`** 上 ⇒ 正确改法 = **按端口组**(组 0 = PE ⇒ `pe_req_in()` · 组 1 = Net ⇒ `net_resp_in()`) | **实施证据**:per change `cpptlm-cep-dual-port-inbound-dispatch` 的 `design.md` §1~§3 + `test/test_dual_port_inbound_dispatch.cc` |
| **E-8b** | §二 R-2 的「双条件」(① 修适配器 + ② 改 config `emit_completions: true` 并重跑) | ⚠️ **双条件不完整** —— 实测还需**第三条**:CohIf 的 egress 走 `req_out` ⇒ `home` 的 `req_out[0]` 在原拓扑里**未连接** ⇒ 回程 `Completion` **根本发不出去** ⇒ 须在拓扑里补 **`home.0 → w.1`** 的回程连接(每个方向一条显式连接 = CohIf「断链即断达」模型的直接推论) | 三条条件缺一不可;**实施证据**:仅做 ①+② 时 `home.completions_sent > 0` 但 `w.net_completions_in == 0` ⇒ 回程仍不可达 |
| **E-8** | §二 R-2 验收 =「**全绿 run**」 | `07 号` SU 十五 ① 写明 IC3 = ⚪ 不适用(违例类 `memory_violations` 无样本)⇒ **R-2 修完最多让延迟族 5 键出数值,IC3 仍 ⚪** | 目标改写为「延迟族 5 键出数值 + IC3 仍 ⚪ 并给理由 + 回程包落 `net_resp_in` 的反向 fixture」⇒ 同时守住 §五.2「不得用框架跑通改判」 |

**另 3 处(非数字但影响执行)**:
- **E-9** §二 O-2 / O-5 列为剩余任务 ⇒ **两者已完成**(O-2 已改成「✅ 已归档」· O-5 已入库 `03732b74`)⇒ 标记为**非任务**。
- **E-10** §三 E1「4 族 × 2 路径的**公开来源**(…人类成员条款摘要)」⇒ 探测协议(`test/cep_b3b2_d4_probe.json`)规定第 2 路径 = `check_type: internal_artifact`(扫 `docs/research/*cep-*-clause-excerpt*.md`,**今日 0 命中 ⇒ 必 miss**),⛔ **不是外部检索**。照字面执行会诱导去外部搜内部路径 ⇒ 编造命中。
- **E-11** §二 W5「typo 统一 · 可选 · 独立」⇒ `DEFERRRAL-TRIGGER-DRIFT` 同时出现在**产出侧** `scripts/test/cep_b3b2_d4_probe_validate.py:143` 与**消费侧** `scripts/test/cep_b3b2_d4_probe_check.sh:128`(**决定性变异⑤的断言**)⇒ 只改一处 ⇒ 变异⑤**永久不红** ⇒ 必须**原子改 4 处 + 跑变异验证**。

### 7.2 4 项裁决(oracle 2026-11-09,1 轮)

| # | 裁决 |
|---|---|
| **C-1** | **ADR 结构 schema** = 口径 A(顶层 `##` 章含 Verification **且在首个 `## Status Update` 之前**)· 分母 = **19 份决策 ADR**(排除 NUMBER-MAP / 00 宪章 / 00A 锚点表)· 01/02/03 已合规 · **11/19 永久豁免**(带 `evidence_anchor` 行号锚点,闸校验锚点存活)· **04/05/06/07/07A/08/09/12/13/14/15/16/17/18 共 14 份 = 真实缺口**(⛔ 不许进 deferrals)· `ADR-CEP-10` 文件不存在 ⇒ 占位登记不占分母 |
| **C-2** | **M1 冻结集目的 = 「已签发能力的契约面基线」**(枚举式;依据 = manifest 自身 glob-vs-显式路径的结构差异)⇒ 新建 `test/cep_freeze_scope_registry.json` 登记 **27 份未冻结 spec**(`unfrozen-active` + reason + ledger_entry;frozen 名单不复制)· 配套**反向校验**(`ls openspec/specs/cep-*/spec.md` == manifest 内 ∪ registry)· **`cep-stub-cache-chstream-surface` 纳入冻结集**(走五步授权链) |
| **C-3** | **可签署集 = 14 份**(⚠️ 订正先前「≈13」—— 多减了不在 22 份清单内的 `ADR-CEP-10`)· **「签发」= 两步**:追加 SU 签发段 + **原地更新 header `> **状态**:` 元数据行**(仅此一行豁免,5/5 先例一致)· 决策正文一字不动仍是红线 · **生命周期状态真源 = ADR 文件自身**;NUMBER-MAP 降级为「编号分配真源 + 状态镜像」(其 ✅ 图例已三义漂移)· 三源一致性检查并入 B2 闸 |
| **C-4** | **本轮走内置手工流程**;⛔ **不重生 `.arch-handoff.json`**(重生 = 伪造 arch stage 退出信号,比陈旧更糟)· 产物形态 = **台账 Entry + commit + 闸无参 exit 0 证据 + `ctest -R cep` 无回归**;顺序 = **先立闸让它红,再补数据让它绿** · **arch-handoff 废弃**;恢复条件 = 下一个 Stage 4+ 协议能力立项时由 rdd-arch 重建 |

### 7.3 角色死锁解除(§四「建议顺序」表首行要求 builder vs §五.5「切换只认用户口令」)

**已由用户口令解除**:用户于 2026-11-09 明令「按建议顺序**用对应的角色**实施任务」⇒ 本轮**授权在建议顺序内按需切换角色**(每次切换写 `role.json`,`updated_by: "user"`,并在响应正文标注)。`auto_switch_window` 仍为 `null` ⇒ **窗口外**的角色切换仍需单独口令。

### 7.4 实施顺序(替代 §四 —— 合并 oracle 建议序 + 能力轨)

**T0**(本段 + 台账 Entry #307)→ **R-2**(builder · 建 openspec change · 框架改动 + 反向 fixture)→ **E1**(planner · 8 槽位首次真实探测)→ **T1**(builder · ADR 结构闸 + deferrals + 三源检查)→ **T2**(architect 授权 + builder 机制 · 新 spec 入冻结集)→ **T3**(builder · freeze scope registry + 反向校验)→ **T4**(architect + builder · 签发 14 份,分 4 批,每批 oracle→critic 双审)→ **R-3**(planner 举证 → builder)。

⚠️ **oracle 4.3「本轮全部任务不建 openspec change」仅适用于治理轨**(T1~T5 = 工具/文档,无 delta spec)⇒ **R-2 建 change**(capability 变更:影响框架层全部双端口用户 ≥2 模块 ⇒ per AGENTS.md 反模式 + per Entry #305 ③ R-2「另立 change」)。

### 7.5 未列入本段但实测存在的状态漂移(登记,⛔ 不静默)

- `.rddf/improvements/` 共 **22** 份:**21** 份已标 ✅/已归档,仅 **1** 份 🟡 提案(`cpptlm-cep-b3b2-d4-trigger-probe` = O-3)⇒ **improvements 目录 ≠ 待办池**,新会话若据此推断剩余工作会得出 22 条假任务。
- `cpptlm-cep-cxl-dcoh-symmetric`(状态串「✅ 已完成」,但 `openspec/changes/archive/2026-11-02-cpptlm-cep-cxl-dcoh-symmetric` **存在**)· `cpptlm-cep-tools-test-isolation`(状态串「✅ 已完成」,**无对应归档件**)⇒ 低优核对项,不阻塞本轮。
- `ADR-CEP-NUMBER-MAP.md` 状态列 ✅ **三义漂移**(已签发 / 版本号 / 起草)⇒ C-3 已裁定降级为镜像;**本轮不改**(改动须冻结面授权,且属 C-3 后续批次)。

