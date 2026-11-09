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
