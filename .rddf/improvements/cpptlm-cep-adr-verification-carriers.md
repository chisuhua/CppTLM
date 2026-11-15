---
name: cpptlm-cep-adr-verification-carriers
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-00
  - ADR-CEP-06
  - ADR-CEP-12
  - ADR-CEP-20
  - ADR-CEP-21
状态: ✅ **已归档**(2026-11-15 · per 台账 Entry #431~#433)
created: 2026-11-15
---

# improvement: cpptlm-cep-adr-verification-carriers

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**:⛔ `include/tlm/cep/**` **零改动**(只加**只读**断言)· ⛔ `scripts/test/cep_decision_block_check.py` **零改动**(只扩快照覆盖集合)· ⛔ `docs/adr/**` **零改动**(只**引用**其 Verification 判据)· ⛔ 不改 `kResponderMappedBits` 的值(那是阶段 2 才动)· ⛔ 新增 `INV-CEP-*` 标识 = **0** · ⛔ 不动 `openspec/specs/cep-core-contracts/spec.md`(避免加剧 F-1,见 §Impact R-1)。
> **估时**:**实施 1.0 人日 + 归档链 0.3 人日**(metis SHOULD-4:原估 0.5 人日低估了 constexpr 探针编译 + 3 份冻结产物 + 2 份 delta spec + 反向 fixture 的成本)
> **KPI**:占 OpenSpec slot **1/3**
> **审查**:metis 第 1 轮(14 MUST + 9 SHOULD + 6 NIT **全处置**,per 本文件末尾「§ metis 处置对照表」)

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-21` **V1-6** | `docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md:150-168`(阶段 1 Verification) | ADR **自认的真空**：「当前**无任何机器闸/测试钉死 `kResponderMappedBits == 4`** ⇒ 阶段 2 的『未执行』状态只能靠 V1-4 的 grep 判据 + 人肉复核」⇒ **诚实登记 ⇒ 本 change 兑现它** |
| `ADR-CEP-21` **V2-6** | 同上 `:167` | 「**运行时 mask 判据**:probe 断言 `kMappedResponderMask == (1<<W)-1` ⇒ 堵住『`cep_tilelink_adapter_reverse_verify.sh:49` 的**文本锚**在 mask 语义漂移时照绿』的盲区」⇒ ⚠️ **V2-6 属阶段 2 组**,本 change **不满足、不替代**它;只新增**同形**的**独立**运行期断言(在当前宽度 4 下成立)⇒ **不构成阶段 2 的执行** |
| `ADR-CEP-21` **§背景 47** | 同上 `:47` | 实证盲区:`scripts/test/cep_tilelink_adapter_reverse_verify.sh:49` 的 `join-no-unmapped-reject` 变异锚 = `if ((extra_responders & ~kMappedResponderMask) != 0)` 的**源码文本** ⇒ 常量 4→8 时**源码文本不变 ⇒ 该闸照绿**(实测:mask 由 `0xF` 变 `0xFF`)⇒ **数值锚**(非恒等式)是唯一堵法 |
| `ADR-CEP-21` **S1-4** | 同上 | 满射判据**已落地**(`test/test_cep_sharer_tracker_base.cc:222-232`)⇒ 本 change **不重复造**,只补**宽度常量 + 掩码数值锚**两格 |
| `ADR-CEP-00` SU 2026-11-11 §⑤ + `ADR-CEP-00` 的「标题口径」 | 已签发 | 决策块计数口径 = 「§2 章下的独立决策载体」;闸的 Python 实现是 **canonical 实现**,C++ 版 `test/cep_adr_parser.hh` 是**受控重复** |
| 决策块闸 = canonical 实现 | `scripts/test/cep_decision_block_check.py:5-40` | 期望值来源 = **闸外快照** `test/cep_decision_block_snapshot.json`;⛔ 期望值**不得**从被检对象自身派生;`authorized_by` = **闸外授权凭据**(形如 `Entry #N`,且闸 `:207-218` 会检查该台账 Entry **真实存在**) |
| `ADR-CEP-20` 决策块实测 | 用**闸自己的** `analyze_section2` 实测(⛔ 非外部 `grep`) | **实测 = `(6, [])`** ⇒ 期望值 **6**;`ADR-CEP-20` 有 `## 2. 决策` 章 + **6 个** `### 决策 N`(决策 1~6) |
| **先例核对** | 同仓既有做法 | ① **ADR 判据落地走 spec delta + 机器载体**(`ADR-CEP-21` S1-4 → `cep-sharer-tracker-base` 的 `responder 位宽扩容的机制与触发判据` Requirement,**已存在**)· ② **闸期望值走闸外快照 + 台账 Entry 授权**(现快照 `authorized_by` = `Entry #363`)· ③ **持久反向 fixture 走影子树**(决策块闸已支持 `--root DIR` 影子树 + 既有变异段;`cep_tilelink_adapter_reverse_verify.sh` 已用「影子树 + 编译期锁毒化副本」形态)⇒ 本 change **不引入新做法** ⇒ **无需 architect 裁决** |
| **constexpr 毒化形态先例** | `scripts/test/cep_tilelink_adapter_reverse_verify.sh`(:49 的编译期锁毒化) | 「**影子树复制头文件 → 改常量 → 编译探针**」形态 ⇒ **2026-11-15 已实测可行**(编译 rc=0 / 运行退出码 1 ⇒ 判红且**非**编译失败冒充) |

## 范围

**In Scope**:

- **W1 · 判据**(改 `test/test_cep_sharer_tracker_base.cc`,标签 `[cep][sharer-tracker]`):新增 **1 个 TEST_CASE**,含两条**只读**断言 —— ① `REQUIRE(kResponderMappedBits == 4u)`(阶段 1「未扩容」状态钉死)② `REQUIRE(kMappedResponderMask == 0xFu)`(**数值锚**,堵住 `ADR-CEP-21:47` 的文本锚盲区)。
  - ⚠️ **⛔ 不用恒等式** `kMappedResponderMask == (1 << kResponderMappedBits) - 1` —— `sharer_tracker.hh:141-142` 的定义**就是**该式 ⇒ 恒真 ⇒ 违反 `test_cep_sharer_tracker_base.cc:12` 自订的「不做恒真的常量存在性检查」纪律。
- **W2 · 快照**(改 `test/cep_decision_block_snapshot.json`):`files` 加 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`;`count` 22 → **23**;`total` 125 → **131**;`authorized_by` → `Entry #431`。
- **W3 · 2 份 delta spec**(落**change 目录内**,⛔ **不直接改** `openspec/specs/`):
  - `openspec/changes/cpptlm-cep-adr-verification-carriers/specs/cep-sharer-tracker-base/spec.md` —— **MODIFIED** Requirement `responder 位宽扩容的机制与触发判据`(加「宽度常量与掩码数值锚**必须被机器钉死**」+ 2 条 Scenario);
  - `openspec/changes/cpptlm-cep-adr-verification-carriers/specs/cep-invariant-gate/spec.md` —— **MODIFIED** Requirement `cep-invariant-gate`(`:40` 的「实测 **125** / **22**」→ **131 / 23**)⇒ ⛔ **不新增 Requirement**(见 §Why 第 4 条)。
- **W4 · 持久反向 fixture**(新增 `scripts/test/cep_adr_verification_carriers_reverse_verify.sh`):**只写 `mktemp -d`**(⛔ 不复用 wrapper 的固定 `/tmp/opencode/cep-decision-block`)⇒ 4 条闭环:
  1. 未毒化 ⇒ 绿;
  2. **宽度毒化**:影子树复制 `include/tlm/cep/sharer_tracker.hh` ⇒ 改 `kResponderMappedBits` 4→8 ⇒ 编译**最小探针 TU**(运行期判定 ⇒ **退出码**即判决)⇒ **非零退出** = 判红;⚠️ **编译失败 ⇒ exit 2 环境错误**(⛔ 不计入「红」,per 仓内「编译失败不得冒充判据失败」纪律);
  3. **覆盖毒化**:影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 的一个 `### 决策 N` 段 ⇒ `python3 scripts/test/cep_decision_block_check.py --root <影子树>` ⇒ 必红且输出含 `决策块计数快照偏离` **与** `ADR-CEP-20`;
  4. 恢复态 ⇒ 绿。
  - ⛔ **不毒化快照文件**(闸的 `load_snapshot()` 固定读真实仓根的 `ROOT/test/...`,`--root` **只**重定向文档语料 ⇒ 快照毒化在不改仓内文件的前提下**不可实现**)。
- **W5 · 台账 Entry #431**(planner 域,append-only):兼作 W2 快照的 `authorized_by` ⇒ **必须先于** W2 落盘 ⇒ 闸的 `:218` 校验才能过。
- **W6 · 文档连带**(metis MUST-5 / MUST-13 / SHOULD-6):
  - `docs/architecture/07-coherence-exploration-platform.md:590` 的「22 份 ADR ... 22 键 + `total`=125」→ **23 份 ... 23 键 + `total`=131`(该文件 ∈ **M1 冻结面** ⇒ 归档时进冻结链 ⇒ **冻结链 = 3 份产物**);
  - `scripts/README.md`:新增脚本行(§CEP 契约守门脚本表 + 目录表脚本清单);
  - `test/CMakeLists.txt`:注册 `cep_adr_verification_carriers_reverse_verify` 进 ctest。

**Out of Scope**:

- ⛔ **不改 `kResponderMappedBits` 的值**(4 ⇒ W 是**阶段 2** 的动作)· ⛔ **不执行** `ADR-CEP-21` 阶段 2 · ⛔ **不满足/不替代** `V2-6`。
- ⛔ **不改 `include/tlm/cep/sharer_tracker.hh`**(常量与掩码的定义**只读**;`static_assert(kResponderMappedBits <= 64)` 已在 `:109`)。
  - ⚠️ metis SHOULD-1 建议把 `kResponderMappedBits == 4` 提升为 header `static_assert`(构建期即红,更强)⇒ ⛔ **本 change 不做**:① 改 `include/tlm/cep/**` 违反本 change 的零 diff 纪律 ⇒ 需 architect 批准;② 把「宽度」写成编译期锁会让**阶段 2 的合法扩容也编译不过** ⇒ 与 ADR 的两阶段设计**直接冲突** ⇒ ⇒ **登记为 backlog**(见 §Impact R-4)。
- ⛔ **不改 `scripts/test/cep_decision_block_check.py`**(闸代码零 diff)⇒ ⚠️ 其 `:11` / `:237` 与 `.sh:10` 注释里的「22 份」将 **stale** ⇒ **登记**(metis SHOULD-8),⛔ 本 change 不改闸代码。
- ⛔ **不改任何已签批 ADR**(含 `ADR-CEP-20` / `ADR-CEP-21`)⇒ ⚠️ `ADR-CEP-21` 的 V1-6 checkbox **保留为 `- [ ]`**(ADR 不可变、不可勾选)⇒ 「载体已存在」的事实**以台账 Entry #431 为准**(metis MUST-6)。
- ⛔ **不把 `ADR-CEP-21` 塞进快照** —— 该 ADR **无 `## 2` 决策章**(章节为 `## 背景与问题` / `## 决策` / `## 选项对比表` / …)⇒ 闸的 `section2()` 返回 `None` ⇒ **结构性不适用**(⛔ 非「无编号决策块」—— metis MUST-15 的订正)⇒ 塞进去只会造出「期望 0 块」的**虚假断言**。
- ⛔ **不新增 spec Requirement**(metis MUST-7):「决策块闸覆盖集合**不得**停在快照签发日」若作为 ADDED Requirement 落地,**无任何机器载体** ⇒ 恰是本仓反复打击的「有登记、无牙齿」⇒ **删除**;「覆盖集合自愈」登记为 backlog(§Impact R-3)。
- ⛔ **不修 F-1**(`openspec validate --strict` 超长 Requirement 群)—— 范围实测为**全仓 87/129 份 spec**(§Impact R-1)⇒ **另立 backlog**。
- ⛔ **不裁决 R-2 / R-5**(镜像 20 值跨仓权威归属 / ArchForge 缺席零保护)—— 属 **architect 判权**。
- ⛔ 不新增 openspec change(那是 planner / builder 域,已建)· ⛔ 不动 M1 冻结面(除 W2/W3/W6 的 3 份产物 ⇒ 归档步走五步链)。

## Why

**第 1 条 —— G-1 的真正风险不是「宽度会变」,而是「文本锚闸在 mask 语义漂移时照绿」(ADR 自认 + 实测)。**

`ADR-CEP-21` 把 responder 位宽扩容设计成**两阶段**:阶段 1 机制化(本 ADR 成立即生效)· 阶段 2 触发式扩容(默认 `W = 8`)。两阶段之间**唯一**的闸门是「阶段 2 **未执行**」这个状态。

实测(2026-11-15,全仓 grep + 影子树编译探针):

| 载体 | 现状 | 缺口 |
|---|---|---|
| `kResponderMappedBits` | `include/tlm/cep/sharer_tracker.hh:92` = `4` | ⚠️ **全仓无任何测试或闸断言它 == 4** |
| `kMappedResponderMask` | `sharer_tracker.hh:141-142` = `(1 << 4) - 1` = `0xF` | ⚠️ **无断言它是 `0xF`** |
| 文本锚 | `scripts/test/cep_tilelink_adapter_reverse_verify.sh:49` 的 `join-no-unmapped-reject` 锚 = 源码**文本** `if ((extra_responders & ~kMappedResponderMask) != 0)` | ⚠️ **ADR-CEP-21:47 自认**:常量 4→8 时**源码文本不变 ⇒ 该闸照绿** |

⚠️ **实测证据**(本轮影子树探针,`kResponderMappedBits` 毒化为 8):编译 rc=0,运行打印 `kResponderMappedBits=8 mask=0xff` ⇒ **mask 由 `0xF` 变 `0xFF`,而源码文本一字未改** ⇒ 文本锚闸**确实**照绿 ⇒ ADR 的登记**成立且可复现**。

⇒ **代价**:任何人把常量从 4 改成 8(哪怕误改、哪怕「提前准备」),`ADR-CEP-21` 的「阶段 2 未执行」前提被**静默破坏** —— 而阶段 2 需要 **architect 裁决 + 台账 Entry**(2b 人裁段)⇒ **一个本该被人裁的事,变成了一条无人察觉的改常量动作**。且 `kResponderBitToNode` 数组尺寸**已是 8** ⇒ 改常量**不会编译失败** ⇒ 连编译器都不报警。

⇒ 本 change 把两格补成**机器判据**:① 宽度常量被钉死为 4;② 掩码被钉死为 `0xF`(数值锚,**非**恒等式)⇒ **文本锚漂移 ⇒ 必红**。

**第 2 条 —— G-2 是治理闸的**覆盖集合**盲区,且机制上**不会自愈**。**

决策块闸 `scripts/test/cep_decision_block_check.py` 的 R-d 段**只遍历闸外快照里的 ADR**(源码 `:247` `for basename in sorted(files):`)⇒ **不在快照里的 ADR 零覆盖**。实测:

| ADR | `## 2` 决策章 | 决策块 | 在快照? | 闸的覆盖 |
|---|:--:|:--:|:--:|:--:|
| `ADR-CEP-00` ~ `-19` + `-00A` + `NUMBER-MAP`(22 份) | ✅ | 3~10 | ✅ | ✅ 逐份比对 + 反向变异 |
| **`ADR-CEP-20`**(2026-11-13 签发,Entry #402) | ✅ | **6** | ❌ | ❌ **零覆盖** |
| `ADR-CEP-21`(2026-11-14 签发,Entry #421) | ❌ **无 `## 2` 章** | — | ❌ | ✅ **结构性不适用**(非缺口) |

⇒ **代价**:`ADR-CEP-20` 的 6 个决策块**不受任何机器判据守** ⇒ 未来若有人把「决策 3」从 `### 决策 3` 降级为 `#### 决策 3`(或增删决策块),**闸不会红** —— 而闸对另外 22 份 ADR **会**红 ⇒ **同一仓内两套覆盖强度**。且快照覆盖集合**无自愈机制**:新签发 ADR 不会自动进快照 ⇒ **盲区单调增长**。

⚠️ **本 change 的诚实边界**(metis SHOULD-5「成功度量」):本 change **只是把一份 ADR 补进覆盖集合**(手工 bump 一次快照)⇒ **不解决「自愈」**。此后:① 谁把 `kResponderMappedBits` 改成 8 ⇒ `./build/bin/cpptlm_tests '[cep][sharer-tracker]'` **红**;② 谁漏登一份含 `### 决策 N` 的 ADR ⇒ ⚠️ **本 change 后仍不会自动红**(无自愈)⇒ 只能靠**季度人工核对**;③ 谁把 `ADR-CEP-20` 的决策块降级/增删 ⇒ `scripts/test/cep_decision_block_check.sh` **红**。

**第 3 条 —— `ADR-CEP-21` 的「零决策块」是**形态差异**,不是缺口(⛔ 不得误报)。**

`ADR-CEP-21` 的章节是 `## 背景与问题` / `## 决策` / `## 选项对比表` / `## 后果 / 代价` / `## Verification` / `## 关联` / `## 审稿报告` ⇒ **没有 `## 2` 章** ⇒ 闸的 `section2()`(`^## 2\b`)返回 `None` ⇒ 即使它有编号决策块也**不会被计数** ⇒ 对该闸**结构性不适用**。⇒ 本 change **不**把它塞进快照(塞进去 = 造出「期望 0 块」的**虚假断言** ⇒ 反而制造噪声)⇒ 只补 `ADR-CEP-20`。

**第 4 条 —— 认识论边界与「有牙」纪律(⛔ 不得过度声称)。**

- **W1 是「当前状态的变更检测器」**:它断言「现在是 4 / 现在是 `0xF`」,**不**论证「4 是正确的宽度」⇒ 正确性来自 `ADR-CEP-21` 的阶段 2 触发判据(2a 机判 / 2b 人裁)⇒ 本 change **不动**该判据,只让「未执行」状态**可机判**。
- **W2 是「覆盖集合的显式登记」**:把 `ADR-CEP-20` 加进快照 = 声明「它纳入治理闸覆盖」⇒ 之后它的决策块**变更必须同步快照**(否则闸红)⇒ 这是**变更检测义务**,**不是**「决策块计数正确」的独立裁决(期望值 6 由本 change **用闸自己的 `analyze_section2` 实测**登记,⛔ 不得从被检对象自身派生 —— 闸文件头 D-4 明文规定)。
- **⛔ 不得引入无载体 Requirement**(metis MUST-7):本仓反复打击「有登记、无牙齿」。故 W3 的第二份 delta 只**修正既有数字**(125/22 → 131/23),**不新增**「覆盖集合不得停在快照签发日」这类无机器载体的要求。

## What Changes

1. **`test/test_cep_sharer_tracker_base.cc`**(改):新增 1 个 TEST_CASE `[cep][sharer-tracker] responder 位宽常量与掩码数值锚`:
   - `REQUIRE(kResponderMappedBits == 4u)` —— 阶段 1「未扩容」状态钉死;
   - `REQUIRE(kMappedResponderMask == 0xFu)` —— 掩码数值锚(堵文本锚盲区);
   - ⛔ **不**重复 `static_assert(kResponderMappedBits <= 64)`(已在 `sharer_tracker.hh:109`)· ⛔ **不**写恒等式(见 §架构依据 W1 注)。
   - ⚠️ **不新增测试文件** —— `test/CMakeLists.txt:6` 的 `file(GLOB TEST_SOURCES "test_*.cc")` 在 **configure 时**展开 ⇒ 新文件须重跑 cmake ⇒ 改现有文件成本最低(metis §5 边界情况)。
2. **`test/cep_decision_block_snapshot.json`**(改):`files` 加 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`;`count` → 23;`total` → 131;`authorized_by` → `Entry #431`。
   - ⚠️ **期望值 6 的独立实测命令**(metis MUST 修正 / SHOULD-7):`python3 -c "import importlib.util as u; s=u.spec_from_file_location('g','scripts/test/cep_decision_block_check.py'); g=u.module_from_spec(s); s.loader.exec_module(g); print(g.analyze_section2(g.section2(g.strip_fences(open('docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md').read()))))"` ⇒ **实测 = `(6, [])`** ⇒ 期望值 6 成立。
   - ⚠️ 若该命令输出 ≠ 6 ⇒ **以闸口径为准**并在本 change 内订正(⛔ 不得用外部 `grep` 结果覆盖闸口径)。
3. **2 份 delta spec**(落 change 目录内):`MODIFIED openspec/specs/cep-sharer-tracker-base/spec.md`(Requirement `responder 位宽扩容的机制与触发判据`)+ `MODIFIED openspec/specs/cep-invariant-gate/spec.md`(Requirement `cep-invariant-gate` 的 `:40` 数字)⇒ 归档时合并进 main spec。
   - ⚠️ **MODIFIED 块必须含整段 Requirement 原文**(metis §5)⇒ 实施时以 main spec 现文为底稿。
4. **`scripts/test/cep_adr_verification_carriers_reverse_verify.sh`**(新增):只写 `mktemp -d`;4 条闭环(见 §范围 W4);编译失败 ⇒ **exit 2**;仓内零改动以 **`sha256sum` 前后比对**自证(metis SHOULD-3 —— `git diff --quiet` 在脏工作区会假红)。
5. **`docs/architecture/07-coherence-exploration-platform.md:590`**(改):`22 份 ADR ... 22 键 + total=125` → `23 份 ... 23 键 + total=131` ⇒ **第 3 份冻结产物**。
6. **`scripts/README.md`**(改):新增脚本行(目录表 + §CEP 契约守门脚本表)。
7. **`test/CMakeLists.txt`**(改):注册 `cep_adr_verification_carriers_reverse_verify` 进 ctest(`WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`)⇒ **ctest 条目数 +1**。
8. **台账 Entry #431**(planner 域):兼作快照 `authorized_by`;**必须先于** W2 落盘。

## Acceptance

| # | 判据 | 可验收点(机器可判) |
|:--:|---|---|
| **AC-01** | 宽度常量被钉死 | `kResponderMappedBits == 4u` 断言在位;**反向**:影子树毒化为 8 ⇒ 探针**非零退出**(= 判红) |
| **AC-02** | 掩码数值锚有效 | `kMappedResponderMask == 0xFu` 断言在位;**反向**:毒化后实测 `mask=0xff` ⇒ 探针非零退出 ⇒ **恒等式做不到这一点**(实证差异) |
| **AC-03** | `ADR-CEP-20` 的覆盖**可证**(⛔ 非「零红」) | 影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 一个 `### 决策 N` 段 ⇒ `python3 scripts/test/cep_decision_block_check.py --root <影子树>` ⇒ **rc≠0** 且输出**同时**含 `决策块计数快照偏离` 与 `ADR-CEP-20` |
| **AC-04** | 快照自洽 + 授权存在 | `count == len(files) == 23`;`total == sum(files.values()) == 131`;`files['ADR-CEP-20-topology-ic3-evidence-strength.md'] == 6`;`authorized_by == 'Entry #431'` 且台账 `^### Entry #431` **命中** |
| **AC-05** | `ADR-CEP-21` **不在**快照(负向断言) | `'ADR-CEP-21-sharer-bitmap-expansion.md' not in files` ⇒ 防未来误加「期望 0 块」 |
| **AC-06** | 持久 fixture 四态闭环 + 仓内零改动 | 脚本 rc=0 且输出「未毒化绿 / 宽度毒化红 / 覆盖毒化红 / 恢复绿」;**仓内零改动**用 **`sha256sum` 前后比对**目标文件(⛔ 不用 `git diff --quiet`) |
| **AC-07** | 范围零改动 | `git diff --name-only` **不含** `include/tlm/cep/` · `docs/adr/` · `scripts/test/cep_decision_block_check.py`;新增 `INV-CEP-*` 标识 = **0**;新增 spec Requirement = **0**(只 MODIFIED) |
| **AC-08** | 0 回归 | `[cep]` 用例数 **+1**;`ctest -R cep` 全绿;ctest 条目数 **+1**;43 个 `cep_*.sh`(**按 ctest 同参数**:docs_sync / doc_ref 带 `--strict`)全 rc=0;`agents_md_lint` 通过;`openspec validate --changes --strict` rc=0 |
| **AC-09** | 归档后逐份 `--strict` 不劣化 | 归档前 `openspec validate cpptlm-cep-adr-verification-carriers --strict` rc=0;归档后 `openspec validate cep-sharer-tracker-base --strict` 与 `openspec validate cep-invariant-gate --strict` **逐份** rc=0(两者现 max 正文长度 = **298 / 348** ⇒ **不逼近 500 阈值**) |
| **AC-10** | 归档五步链 | 顺序固定:**#431 台账先落 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #431"` → 清单单独 commit → 无参复验 rc=0**;清单 diff = **3 份产物**的 hash(`cep-sharer-tracker-base/spec.md` · `cep-invariant-gate/spec.md` · `docs/architecture/07-coherence-exploration-platform.md`)+ `authorized_by` + `frozen_at` + `frozen_at_commit` **元数据**,**无其他 artifact hash 变化**;`artifact_count` 不变(86) |

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|:--:|
| **C-1** | 「responder 位宽**未扩容**」这一状态**可机判** | `test/test_cep_sharer_tracker_base.cc`(W1) | ❌ 0(兑现 `ADR-CEP-21` V1-6,**非新语义**) |
| **C-2** | 「掩码数值 = `0xF`」**可机判**(堵住 `ADR-CEP-21:47` 的文本锚盲区) | 同上 | ❌ 0(与 `V2-6` **同形但独立**;**不满足、不替代** `V2-6`) |
| **C-3** | `ADR-CEP-20` 的决策块**纳入治理闸覆盖** | `test/cep_decision_block_snapshot.json`(W2)+ 闸 | ❌ 0(扩展**覆盖集合**,**非新口径**) |

## Impact

| 面 | 影响 |
|---|---|
| **实现层** | ⛔ **零改动**(`include/tlm/cep/**` 只读) |
| **契约层** | 2 份 main spec **MODIFIED**(数字 + 判据)⇒ M1 冻结面 ⇒ 归档走五步链 |
| **真源 1** | `docs/architecture/07-...:590` 的决策块计数同步 ⇒ **第 3 份冻结产物** |
| **闸** | 决策块闸的**覆盖集合** +1 份;闸**代码零改动**(其注释里的「22 份」stale ⇒ 登记) |
| **测试** | `[cep]` 用例数 +1;ctest 条目数 +1;新增 1 个脚本 |
| **文档** | `scripts/README.md` 脚本表 +1 行 |

**⚠️ 残余登记(5 项 · 全部**未关闭** · ⛔ 本 change 不做)**:

| # | 残余 | 实测状态 | 路由 |
|:--:|---|---|---|
| **R-1** | **F-1 范围被低估一个数量级** —— `openspec validate --strict` **逐 spec** 实测 **87/129 份 rc=1**(cep-* 内 ≥9 个超长 Requirement 分布 7 份:`core-contracts` 3 个 / `channel-dependency-gate` 528 / `collective-plane-base` 541 / `collective-plane-contract` 549 / `refkind-coverage` 1185 / `tilelink-runtime-engine` 591 / `ualink-coherence-engine` 1969)⇒ verifier 报告(`cpptlm-cep-flow-support-assertions-verify.md` §F-1)写「3 个 Requirement」**范围不足** | ⛔ 仓内**任何闸 / CI 都不跑**逐 spec `--strict`(`openspec validate --changes --strict` rc=0 + 43 闸全 rc=0 **已实测**)⇒ 实际影响 = 「手动跑逐 spec `--strict` 会看到红」 | **另立 backlog**;本轮台账 Entry #431 **登记订正**(verifier 报告属 verifier 写集,planner **不回改**) |
| **R-2** | **镜像 20 值**(`moesi_gpu`/`mesif`)的 13 键无仓内 golden 锁(权威在 ArchForge) | 期望表只覆盖 6 个 `repo_owned` 实例 | ⭔ **architect 判权** |
| **R-3** | **决策块闸的覆盖集合无自愈机制** —— 新签发 ADR 仍需**人工** bump 快照 ⇒ 盲区仍会单调增长 | 本 change 只 bump 一次;**无自愈** | **backlog**(需一个「有 `## 2` 决策章且含 `### 决策 N` 的 ADR 必须入快照」的**有牙**载体 ⇒ 属独立 change) |
| **R-4** | `kResponderMappedBits == 4` 的**编译期锁**不可行 | 把宽度写成 `static_assert` 会让**阶段 2 的合法扩容也编译不过** ⇒ 与 ADR 两阶段设计**直接冲突** ⇒ ⛔ **不是**「更强的做法」 | **不追**(ADR 两阶段设计的必然);当前 test-only 判据**足够**(实测有牙齿) |
| **R-5** | **`cep_decl_alignment_check.sh` 在 ArchForge 缺席时整闸 `exit 0`**(`:40-44`)⇒ 7 项子校验全跳过 | 条件性(本机 ArchForge **在位** ⇒ 当前 `[RUN]` 7/7 全绿) | ⭔ **architect 判权**(或 builder 拆闸) |

## Non-Goals

- ⛔ **不**执行 `ADR-CEP-21` 阶段 2(扩容到 `W`)—— 那是**触发后**的动作(2a 机判 / 2b 人裁)本 change **不动**。
- ⛔ **不**满足 / 不替代 `ADR-CEP-21` **V2-6**(阶段 2 组的运行时 mask 判据)—— 本 change 只新增**同形的独立**断言。
- ⛔ **不**把 `ADR-CEP-21` 塞进决策块快照(**无 `## 2` 决策章** ⇒ 结构性不适用)。
- ⛔ **不**改决策块计数口径(§2 章 / 决策块的定义在 `ADR-CEP-00` SU,本 change 只扩**覆盖集合**)。
- ⛔ **不**新增 `INV-CEP-*` 标识(编号位 67 / 实际 66 **不变**)· ⛔ **不**新增 spec Requirement(只 MODIFIED)。
- ⛔ **不**追加 `ADR-CEP-21` 的 `## Status Update` —— ADR 不可变 + 本 change 保持 `docs/adr/**` 零 diff ⇒ 「载体已存在」的事实**以台账 Entry #431 为准**。
- ⛔ **不**裁决 R-2 / R-5(architect 域)· ⛔ **不**修 R-1 / R-3(另立项)。
- ⛔ **不**新增 ADR。

---

## metis 处置对照表(第 1 轮 · 14 MUST + 9 SHOULD + 6 NIT · 全处置)

| # | metis 条目 | 处置 |
|:--:|---|---|
| M-1 | delta 路径 + 补四件套步骤 | ✅ 采纳 —— W3 路径改 `openspec/changes/<name>/specs/**`;⛔ 不直接改 `openspec/specs/`;四件套创建归 builder(§范围 W3) |
| M-2 | V2-5 → **V2-6** + 措辞 | ✅ 采纳 —— §架构依据 / §Why 第 1 条 / §Non-Goals 三处已改 |
| M-3 | W4 第③条改影子树变异 | ✅ 采纳 —— W4 第 3 条 = 影子树删 `ADR-CEP-20` 一个决策块 |
| M-4 | `cep-invariant-gate:40` MODIFIED | ✅ 采纳 —— W3 第二份 delta 改为 MODIFIED 数字修正 |
| M-5 | `07-...:590` 的 22/125 | ✅ 采纳 —— **同步改成 23/131**(不登记为历史快照)⇒ 冻结链 **3 份产物** |
| M-6 | 删「闭环 V1-6 / 可勾选」 | ✅ 采纳 —— 改为「载体已存在;ADR 正文保留历史真空登记,以 Entry #431 为准」;**不追加 SU** |
| M-7 | W3 第二份 ADDED 无载体 | ✅ 采纳 —— **删除该 ADDED Requirement** ⇒ 只 MODIFIED;「覆盖自愈」转 R-3 backlog |
| M-8 | 重写 AC-03 | ✅ 采纳 —— AC-03 = 影子树变异 ⇒ 必红 + 输出含文件名 |
| M-9 | 重写 AC-09 | ✅ 采纳 —— AC-10 = 3 份产物 hash + 元数据 + 无其他 hash 变化 + 顺序固定 |
| M-10 | W5 编号与顺序 | ✅ 采纳(并**订正 metis 自身**:台账最大 Entry = **#430**,非 #429)⇒ W5 = **Entry #431**,**先于** W2 |
| M-11 | W1 第二条改 `0xF` | ✅ 采纳 —— **实测有牙齿**(毒化后 `mask=0xff` ⇒ 红)⇒ 恒等式被替换 |
| M-12 | W4 ctest 注册 + constexpr 毒化策略 | ✅ 采纳 —— **影子树 + 最小探针 TU**(运行期判定 ⇒ 退出码即判决;编译失败 ⇒ exit 2);ctest 注册 +1 |
| M-13 | `scripts/README.md` + `CMakeLists` | ✅ 采纳 —— W6 + What Changes 6/7 |
| M-14 | `ADR-CEP-21` 负向断言 | ✅ 采纳 —— AC-05 |
| M-15 | `ADR-CEP-21` 不适用理由 | ✅ 采纳 —— 改为「**无 `## 2` 决策章**」(实测章节清单) |
| S-1 | 提升为 header `static_assert` | ⛔ **不追** —— 与 ADR 两阶段设计**直接冲突**(合法扩容也会编译不过)⇒ 登记 R-4 |
| S-2 | 统一「invariant」术语 | ✅ 采纳 —— 改为「新增 `INV-CEP-*` 标识 = 0;新增 spec Requirement = 0」 |
| S-3 | AC-05 用 `sha256sum` | ✅ 采纳 —— AC-06 改为 `sha256sum` 前后比对 |
| S-4 | 上调估时 | ✅ 采纳 —— 0.5 → **1.0 人日** + 0.3 归档链 |
| S-5 | Why 增加「成功度量」 | ✅ 采纳 —— §Why 第 2 条末段(三条命令各会红) |
| S-6 | 登记 07 / `ADR-CEP-00` stale | ✅ 采纳 —— W6 + R-1/§Out of Scope(闸注释 stale 登记) |
| S-7 | `6` 的实测命令入台账 | ✅ 采纳 —— What Changes 2 的 ⚠️ 命令(实测 `(6, [])`) |
| S-8 | 闸注释「22 份」stale | ✅ 采纳 —— §Out of Scope 登记(⛔ 不改闸代码) |
| S-9 | `#### 决策 7` 变异 | ✅ 采纳 —— AC-03 追加「另加一条 `#### 决策 7` 变异 ⇒ 输出含 `决策块标题非 ### 级` + 文件名」 |
| N-1..6 | 压缩 ⚠️ 密度 / 统一引用 / 行号区间 / 行号漂移 / #431 授权登记 / 脚本头注 | ✅ 全采纳 —— 本 v2 已压缩 §Out of Scope 的 ⛔ 密度、引用统一为「§Why 第 N 条」、`ADR-CEP-21` 引用区间改为 `:150-168`、台账 Entry #431 正文将写明「授权快照覆盖集合 +1」、脚本头注将写明「只写 `mktemp -d`,不复用 wrapper 固定路径」 |
