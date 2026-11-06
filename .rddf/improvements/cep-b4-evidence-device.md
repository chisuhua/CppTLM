---
name: cep-b4-evidence-device
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-06
状态: ✅ **已实施**(2026-11-05 实施 / 2026-11-06 回填 per Entry #250;装置 `test/test_cep_b4_evidence_device.cc` 过 **AC-9** 实现评审 PASS,per Entry #228;**不占 OpenSpec slot**)
created: 2026-11-05
---

# improvement: cep-b4-evidence-device

**主题**: Coherence Exploration Platform(CEP)— B4 达成证据载体(h3 装置升级为「一致性 × 完成域」复合装置)

> **决策源**:`ADR-CEP-06` **SU#4**(Accepted 2026-11-05;oracle 2 轮 + critic 1 轮,审稿报告 `.rddf/reviews/ADR-CEP-06-SU4-b4-evidence-carrier.md`)
> **授权**:台账 **Entry #226**(`scope-change`:H3 预登记收缩 + 冻结授权;**授权先行于任何装置实现/产出**)
> **KPI**:**不占** OpenSpec slot(装置住 `test/`,零契约面改动;先例 = H4 装置 Entry #222/#223)
> **估时**:**2.5d**,分两里程碑 —— **M1**(周期事务引擎 + 双检测器 + 双负控)1.5d · **M2**(观测键仪表化 + 观测键/trace 链路 + 耦合探针 + validation)1d

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-06` **SU#4** | 文末 `## Status Update 2026-11-05(#4)` | 路径 ①(装置补全,不重裁验收口径);§② 六条非空洞性硬约束;§⑤ 验收判据;§① 强制配套(预登记收缩先于产出) |
| `ADR-CEP-06` **SU#1 §④** | B4 读取约束 | 「违例类为 `not-collected` 的 run SHALL NOT 计入 B4 达成证据 ⇒ 违例类必须实测为 0」——**逐字遵守** |
| `ADR-CEP-06` **SU#3** + 勘误 | 报告层三态 | 「✓ 一致性闸门通过」态当前**无真实 run 实例**(仅合成 fixture F1)⇒ 本装置产出**首个真实实例** |
| 台账 **Entry #225 ⑧** | 结构性发现 | 无装置族同时实测两违例类 ⇒ B4 结构性不可达;两装置/口径二选一路由 |
| 台账 **Entry #223 / #222** | H4 装置先例 | `device_qualified()` + 负控是装置合格的前提(emit 层硬约束) |
| `§9.1 ①` H3 **分支 2** | objective 判据表 | `deadlock_violations == 0` **已登记为必纳判据**,但实现从未测量 ⇒ 本条是**补齐已声明判据**,不是新增口径 |

---

## Why

**B4 的达成证据当前结构性不可达**(Entry #225 ⑧ 实测):`objective §6.2 B4` 要求「违例类必须实测为 0」,而
- h2 装置两违例类均无产出者;
- h3 测 `memory_violations`(SWMR + data-value)**不测** `deadlock_violations`;
- h4 / h6 测 `deadlock_violations` **不测** `memory_violations`(模型无内存一致性语义)。

⇒ 没有任何装置族能产出「单 run 两违例类实测 0」⇒ B4 无法达成;同时 `§9.1 ①` **H3 分支 2** 长期处于「未测量 + ⛔ 不继承 pass」;IC3 三态的「✓ 通过」分支**没有真实 run 实例**。

**两条缺口同源**:H3 的判据表本就登记了 `deadlock_violations == 0`,但实现未测 ⇒ 补齐 h3 装置同时解决 B4 与 H3 分支 2。`SU#4` 已裁决走此路径(⛔ 不重裁 B4 口径)。

> ⚠️ **未言明假设的显式化(metis 前置 gap 分析结论)**:h3 现有装置是**无周期、无在途**的同步模型
> (`HomeNodeModel` 逐事务 `service_read/write`);h4 的完成域模型是**纯完成域、无地址/一致性语义**。
>「复合」= **新造一个周期化事务引擎**,⛔ 不是把两件现成模块接线 —— M1 的主体工作量即此。

---

## What Changes

### 0. 提交序(硬,**先于任何装置代码**;per Entry #226「顺序(硬)」)

| 序 | 内容 |
|---|---|
| **commit ①** | `experimental/preregistration.json` H3 行收缩(机器投影;真源 `§9.1 ②` 已于 Entry #226 回合完成)+ 冻结 `--update --authorized-by "Entry #226"` + manifest commit |
| **commit ②** | 装置实现(M1)+ 仪表化/探针(M2)+ 表落(planner,产出后) |

⛔ **commit ① 完成前不得写任何装置代码、不得产出任何 run** —— 该序是**流程闸**(实测 + 未收缩预登记时 drift 恒过,机器闸无兜底)。

### 1. 预登记收缩(commit ①)

| 源 | 变更 |
|---|---|
| 真源 `§9.1 ②` H3 行 | **已收缩**(Entry #226 回合):`deadlock_violations` 移出 out-of-scope |
| `experimental/preregistration.json` H3 行 | **同集收缩** + `reason` 同步;drift 闸两闸互为锁 |

### 2. h3 装置升级(`test/` 内;M1 —— 周期事务引擎 + 双检测器)

| 项 | 内容 |
|---|---|
| 新模型 | `test/cep_consistency_completion_model.hh`:**周期推进**的事务流 —— 真实在途窗口(`in_flight`)、完成域阻塞(coherence 完成依赖 group rendezvous;错标配置可死锁)、完成时点做 **SWMR / data-value** 不变量检查(值可见性语义);每个事务携带 **`(addr, node)` 身份** |
| emit | `run_h3` **原位升级为默认 v2 形态**(`v1` 形态不保留 —— 其 `deadlock=not-collected` 会与新预登记互斥,per SU#4 §① 流程闸;分支 1 数值由 v2 同参数重证) |
| 检测器 | `memory_violations` = `SwmrDataValueDetector`(既有);`deadlock_violations` = 在途窗口 + 无 retire 饥饿判定(**须由真实事务流驱动**;⛔ `ProgressWatchdog` 只有计数入参,**v2 须额外暴露「被阻塞事务键集合」**,见 §3) |
| 负控(硬约束) | ① 错标完成域 ⇒ 死锁=1 ② SWMR 注入(复用 `SwmrDataValueDetector` 的注入面)⇒ `memory_violations > 0`;**两检测器各自红过一次**;不红 ⇒ **拒绝产出**(`EmitResult.fatal`,同 H4 模式) |
| 装置版本标签 | `verification_scope` 带 `device=v2`;`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` **只勘误不重跑** |

### 3. 观测键仪表化 + trace 链路(M2;**AC-5 可判定性的前置**)

| 项 | 内容 |
|---|---|
| 观测键定义 | **事务级 `(addr, node)` 集合**;两组:① 被完成域阻塞的事务键集 ② 被 SWMR/data-value 检查的事务键集 |
| 身份仪表 | 模型/检测器暴露上述两集合(现 `counts()` 不足);⛔ `ProgressWatchdog` 的计数接口**不承担**键语义 —— 键集合由模型侧显式导出 |
| 落盘 | emit 在 `metrics.json` 增 `observation_keys`(schema 顶层 `additionalProperties: true`;⛔ 13 键语义零变);`scripts/test/cep_experiment_run.sh` 的 h5py 镜像将其写入 `trace.h5` 的 `run_summary/observation_keys`;验证 `render_report.py` / gate 容忍度 |
| 断言分层 | **内存层**(ctest 单测直接断言集合交集/覆盖率)与**文件层**(run_sweep 链路读 metrics/trace 校验)**两个断言位置分别落地**,⛔ 不混用(ctest 下 `CEP_EXP_DIR` 未设 ⇒ emit 不产文件) |

### 4. 耦合探针(M2)

| 项 | 内容 |
|---|---|
| 两配置受控 | 阻塞**开/关** = **两份独立 `metrics.json`**,且**除阻塞外 bit 级同分布**(同事务序列/地址集/种子);两配置**各用独立 detector/watchdog 实例**(harness `reset()` 不可替代隔离) |
| 预测锚(独立) | 预测句落 **`experimental/predictions/h3_coupling_<date>.json`**(含写入时间戳 + 当次 git commit hash);`verification_scope` 只存**指针** —— ⛔ 不得把预测写进 metrics.json 自称「先于自己」 |
| 漂移判据 | 显式:阻塞开/关 `memory_violations` 差 **`Δ=0` 或方向与预测一致**;若预测「无关」则判据 = `Δ=0` |
| 灵敏度(防双 0 空过) | SWMR 注入器在**阻塞开/关两配置下均须检出 > 0** —— 证明探针对该维度有灵敏度,⛔ 不接受「双 0 即无漂移」的空过 |

### 5. 表落(planner,产出后)

- `§9.1 ①` H3 分支 2:⏭ 未测量 → 实测结论(0 违例);`§6.2 B4`:验收后回填 ✅(⛔ 不预宣告);
- `docs/validation/2026-11-05-cep-b4-evidence-device.md`:逐字判据 + 口径限定 + 反向 fixture + 4 件套链路 + **分支 1 重证数值**。

---

## Non-Goals

- ⛔ **不改** `§6.2 B4` 验收条款(SU#4 §③ 已否决群集级覆盖等降格表述);
- ⛔ **不新增** `run_hN` 入口、不新增假设预登记行(载体 = h3 原位升级);
- ⛔ **不改** `metrics.schema.json` 的 13 键语义(新增 `observation_keys` = 顶层附加键,不动既有语义);
- ⛔ **不做**协议级证明(模型级结论);
- ⛔ **不动** H2 / H4 / H6 装置与其判定语义。

---

## Acceptance

> 与 `ADR-CEP-06 SU#4 §②/§⑤` 一一对应;实现层验收以本表为准。

| AC | 判据 | 机器锚 |
|---|---|---|
| **AC-1a** | 双源收缩 + 冻结授权链:JSON H3 行与真源同集;`--update --authorized-by "Entry #226"` 完成,hash/provenance 双绿 | commit ① 的 drift 闸 + 冻结核验 |
| **AC-1b** | 产出 run 的 `not-collected ⊆ H3 预登记`(run 级闸) | `cep_prereg_drift_check` / `ab_compare` 判据④ |
| **AC-2** | v2 run 的 `deadlock_violations` 与 `memory_violations` **均在 `_measured`** 且实测 `0`;`_not_collected` 无该两键 | metrics + `[cep][exp]` 断言 |
| **AC-3** | 负控双红:① 错标完成域 ⇒ 死锁=1(**负控 `max_idle_run ≥ threshold`**)② SWMR 注入 ⇒ `memory_violations > 0`;不红 ⇒ emit **拒绝产出**(`REQUIRE_FALSE(fatal)` 仅在**负控不红**分支断言;正控分支断言 `fatal=false` + 产出) | 两分支各自独立断言 |
| **AC-4** | 正控观测面非空:**`in_flight` 峰值 > 1 且 `max_idle_run < threshold`**(配对断言;负控侧由 AC-3 覆盖 `≥ threshold`) | 装置单测直接断言 |
| **AC-5** | **事务级 `(addr,node)` 键集**:阻塞窗口内被阻塞事务键 ∩ SWMR 检查事务键 **≠ ∅** 且**覆盖 ≥ 约定比例**(防单键退化);键集**实际落** `metrics.json` → `trace.h5`;交集为空/拆两段 ⇒ emit **拒绝产出**(负向 fixture);**内存层与文件层分别断言** | ctest(内存)+ run_sweep 链路(文件) |
| **AC-6** | 耦合探针:阻塞开/关 = 两份独立 `metrics.json` + 独立检测器实例 + 除阻塞外同分布;`memory_violations` `Δ=0` 或方向与预测一致;**预测锚在独立文件且时间戳早于 run**;SWMR 注入在**两配置**均检出 > 0 | 两 metrics 比对 + 预测文件时间戳/commit + 注入检出断言 |
| **AC-7** | `render_report --check` 绿 **且** `report.html` 的**闸门表格行**渲染「✓ 一致性闸门通过」(⛔ 全文 grep 不计 —— 防勘误/fixture 文本误命中) | 渲染产物按上下文断言 |
| **AC-8** | 装置版本标签 `device=v2` 落 `verification_scope`;`2026-11-04` 文档只勘误;**v2 run 同参数(1200 事务)重证分支 1:`swmr=0` / `data_value=0` / 漏探=0,数值落 metrics + validation** | 文本 + 数值断言 |
| **AC-9** | **实现评审(architect)**:装置含真实阻塞机制(⛔ 非「watchdog 恒假 + 默认 0」);评审记录落 `.rddf/reviews/`;**B4 回填的前置 gate** | 审稿报告存在 + 结论为 PASS |
| **AC-10** | 停手条款程序化:**错标配置无法构造 stall** 的退化模型变体 ⇒ emit **fatal + 不产出**(证明「无法构造」不会被静默当 0) | 专用 fixture |

---

## Capabilities

**无新增 spec capability**(装置住 `test/`,零契约面):交付物 = ① 复合装置 + 单测(双负控 / 观测窗 / 观测键 / 耦合探针)② `observation_keys`(metrics 附加键 + trace 镜像)③ 预测锚文件 ④ 预登记收缩(JSON 侧)⑤ validation 文档 ⑥ 表落(H3 分支 2 + B4)。若实现中发现需要契约面变更 ⇒ **停手**,回 architect 重裁(SU#4 §② 约束 4 的唯一授权链)。

---

## Impact

- **B4**:获得唯一合规证据形态 ⇒ 达成路径清空(AC-9 gate 通过后可回填 ✅);
- **H3**:分支 2 转实测;分支 1 数值由 v2 重证;
- **IC3 三态**:「✓ 通过」从「仅合成 fixture」升级为「有真实 run 实例」;
- **回归面**:`[cep][h4]` / `[cep][exp]` / `[cep][tools]` / drift 闸 / `cep_experiment_run.sh` 链路;H2/H4/H6 装置零触碰;全量 `ctest` 保持全绿(新增 fixture 用例后计数上升);
- **风险**:空洞达成(SU#4 §② 六约束 + AC-9 人审闸)· 语义耦合(AC-6 探针 + 灵敏度)· 仪表化侵入 metrics(顶层附加键,13 键语义零变;须验证 render/gate 容忍度)。
